// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu_sync.h"
#include "api/api_sending.h"
#include "apiwrap.h"
#include "ayu/utils/telegram_helpers.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "storage/file_download_mtproto.h"
#include "storage/localimageloader.h"

class TimedCountDownLatch
{
public:
	explicit TimedCountDownLatch(int count)
		: count_(count) {
	}

	void countDown() {
		std::unique_lock lock(mutex_);
		if (count_ > 0) {
			count_--;
		}
		if (count_ == 0) {
			cv_.notify_all();
		}
	}

	bool await(std::chrono::milliseconds timeout) {
		std::unique_lock lock(mutex_);
		if (count_ == 0) {
			return true;
		}
		return cv_.wait_for(lock, timeout, [this] { return count_ == 0; });
	}

private:
	std::mutex mutex_;
	std::condition_variable cv_;
	int count_;
};

namespace AyuSync {

QString pathForSave(not_null<Main::Session*> session) {
	auto path = Core::App().settings().downloadPath();
	if (path.isEmpty()) {
		return File::DefaultDownloadPath(session);
	}
	if (path == FileDialog::Tmp()) {
		return session->local().tempDirectory();
	}
	return path;
}

QString filePath(not_null<Main::Session*> session, const Data::Media *media) {
	if (!media) {
		return {};
	}

	if (const auto document = media->document()) {
		if (!document->filename().isEmpty()) {
			return pathForSave(session) + media->document()->filename();
		}
		if (const auto name = document->filepath(true); !name.isEmpty()) {
			return name;
		}
		if (document->isVoiceMessage()) {
			return pathForSave(session) + "audio_" + QString::number(document->getDC()) + "_" +
				QString::number(document->id) + ".ogg";
		}
		if (document->isVideoMessage()) {
			return pathForSave(session) + "round_" + QString::number(document->getDC()) + "_" +
				QString::number(document->id) + ".mp4";
		}
	} else if (const auto photo = media->photo()) {
		return pathForSave(session) + QString::number(photo->getDC()) + "_" + QString::number(photo->id) + ".jpg";
	}

	return {};
}

qint64 fileSize(not_null<HistoryItem*> item) {
	if (const auto path = filePath(&item->history()->session(), item->media()); !path.isEmpty()) {
		QFile file(path);
		if (file.exists()) {
			auto size = file.size();
			return size;
		}
	}
	return 0;
}

void loadDocuments(not_null<Main::Session*> session, const std::vector<not_null<HistoryItem*>> &items) {
	for (const auto &item : items) {
		if (const auto data = item->media()->document()) {
			const auto size = fileSize(item);

			if (size == data->size) {
				continue;
			}
			if (size && size < data->size) {
				// in case there some unfinished file
				QFile file(filePath(session, item->media()));
				file.remove();
			}

			loadDocumentSync(session, data, item);
		} else if (auto photo = item->media()->photo()) {
			if (fileSize(item) == photo->imageByteSize(Data::PhotoSize::Large)) {
				continue;
			}

			loadPhotoSync(session, std::pair(photo, item->fullId()));
		}
	}
}

void loadDocumentSync(not_null<Main::Session*> session, DocumentData *data, not_null<HistoryItem*> item) {
	auto latch = std::make_shared<TimedCountDownLatch>(1);
	auto lifetime = std::make_shared<rpl::lifetime>();

	auto path = filePath(session, item->media());
	if (path.isEmpty()) {
		return;
	}
	crl::on_main([&]
	{
		data->save(Data::FileOriginMessage(item->fullId()), path);

		rpl::single() | rpl::then(
			session->downloaderTaskFinished()
		) | rpl::filter([&]
		{
			return data->status == FileDownloadFailed || fileSize(item) == data->size;
		}) | rpl::start_with_next([&]() mutable
								  {
									  latch->countDown();
								  },
								  *lifetime);
	});


	constexpr auto overall = std::chrono::minutes(15);
	const auto startTime = std::chrono::steady_clock::now();


	while (std::chrono::steady_clock::now() - startTime < overall) {
		if (latch->await(std::chrono::minutes(5))) {
			break;
		}

		if (!data->loading()) {
			break;
		}
	}

	base::take(lifetime)->destroy();
}

void forwardMessagesSync(not_null<Main::Session*> session,
						 const std::vector<not_null<HistoryItem*>> &items,
						 const ApiWrap::SendAction &action,
						 Data::ForwardOptions options) {
	auto latch = std::make_shared<TimedCountDownLatch>(1);

	crl::on_main([=, &latch]
	{
		session->api().forwardMessages(Data::ResolvedForwardDraft(items, options),
									   action,
									   [&]
									   {
										   latch->countDown();
									   });
	});


	latch->await(std::chrono::minutes(1));
}

void loadPhotoSync(not_null<Main::Session*> session, const std::pair<not_null<PhotoData*>, FullMsgId> &photo) {
	const auto folderPath = pathForSave(session);
	const auto downloadPath = folderPath.isEmpty() ? Core::App().settings().downloadPath() : folderPath;

	const auto path = downloadPath.isEmpty()
						  ? File::DefaultDownloadPath(session)
						  : downloadPath == FileDialog::Tmp()
								? session->local().tempDirectory()
								: downloadPath;
	if (path.isEmpty()) {
		return;
	}
	if (!QDir().mkpath(path)) {
		return;
	}

	const auto view = photo.first->createMediaView();
	if (!view) {
		return;
	}
	view->wanted(Data::PhotoSize::Large, photo.second);

	const auto finalCheck = [=]
	{
		return !photo.first->loading();
	};

	const auto saveToFiles = [=]
	{
		QDir directory(path);
		const auto dir = directory.absolutePath();
		const auto nameBase = dir.endsWith('/') ? dir : dir + '/';
		const auto fullPath = nameBase + QString::number(photo.first->getDC()) + "_" + QString::number(photo.first->id)
			+ ".jpg";
		view->saveToFile(fullPath);
	};

	auto latch = std::make_shared<TimedCountDownLatch>(1);
	auto lifetime = std::make_shared<rpl::lifetime>();

	if (finalCheck()) {
		saveToFiles();
	} else {
		crl::on_main([&]
		{
			session->downloaderTaskFinished() | rpl::filter([&]
			{
				return finalCheck();
			}) | rpl::start_with_next([&]() mutable
									  {
										  saveToFiles();
										  latch->countDown();
									  },
									  *lifetime);
		});
		latch->await(std::chrono::minutes(5));
		base::take(lifetime)->destroy();
	}
}

void sendMessageSync(not_null<Main::Session*> session, Api::MessageToSend &message) {
	crl::on_main([=, &message]
	{
		// we cannot send events to objects
		// owned by a different thread
		// because sendMessage updates UI too

		session->api().sendMessage(std::move(message));
	});


	waitForMsgSync(session, message.action);
}

void waitForMsgSync(not_null<Main::Session*> session, const Api::SendAction &action) {
	auto latch = std::make_shared<TimedCountDownLatch>(1);
	auto lifetime = std::make_shared<rpl::lifetime>();

	crl::on_main([&]
	{
		session->data().itemIdChanged()
			| rpl::filter([&](const Data::Session::IdChange &update)
			{
				return action.history->peer->id == update.newId.peer;
			}) | rpl::start_with_next([&]
									  {
										  latch->countDown();
									  },
									  *lifetime);
	});

	latch->await(std::chrono::minutes(5));
	base::take(lifetime)->destroy();
}

void sendDocumentSync(not_null<Main::Session*> session,
					  Ui::PreparedGroup &group,
					  SendMediaType type,
					  TextWithTags &&caption,
					  const Api::SendAction &action) {

	auto groupId = std::make_shared<SendingAlbum>();
	groupId->groupId = base::RandomValue<uint64>();

	crl::on_main([=, lst = std::move(group.list), caption = std::move(caption)]() mutable
	{
		session->api().sendFiles(std::move(lst), type, std::move(caption), groupId, action);
	});

	waitForMsgSync(session, action);
}

void sendStickerSync(not_null<Main::Session*> session,
					 Api::MessageToSend &message,
					 not_null<DocumentData*> document) {
	auto &action = message.action;
	crl::on_main([&]
	{
		Api::SendExistingDocument(std::move(message), document, std::nullopt);
	});

	waitForMsgSync(session, action);
}

void sendVoiceSync(not_null<Main::Session*> session,
				   const QByteArray &data,
				   int64_t duration,
				   bool video,
				   const Api::SendAction &action) {
	crl::on_main([&]
	{
		session->api().sendVoiceMessage(data,
										QVector<signed char>(),
										duration,
										video,
										action);
	});
	waitForMsgSync(session, action);
}

} // namespace AyuSync
