// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu_forward.h"
#include "apiwrap.h"
#include "ayu_sync.h"
#include "lang_auto.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "data/data_changes.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "storage/file_download.h"
#include "storage/localimageloader.h"
#include "storage/storage_account.h"
#include "storage/storage_media_prepare.h"
#include "styles/style_boxes.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/text/text_utilities.h"

namespace AyuForward {

std::unordered_map<PeerId, std::shared_ptr<ForwardState>> forwardStates;

bool isForwarding(const PeerId &id) {
	const auto fwState = forwardStates.find(id);
	if (id.value && fwState != forwardStates.end()) {
		const auto state = *fwState->second;

		return state.state != ForwardState::State::Finished
			&& state.currentChunk < state.totalChunks
			&& !state.stopRequested
			&& state.totalChunks
			&& state.totalMessages;
	}
	return false;
}

void cancelForward(const PeerId &id, const Main::Session &session) {
	const auto fwState = forwardStates.find(id);
	if (fwState != forwardStates.end()) {
		fwState->second->stopRequested = true;
		fwState->second->updateBottomBar(session, &id, ForwardState::State::Finished);
	}
}

std::pair<QString, QString> stateName(const PeerId &id) {
	const auto fwState = forwardStates.find(id);


	if (fwState == forwardStates.end()) {
		return std::make_pair(QString(), QString());
	}

	const auto state = fwState->second;

	QString messagesString = tr::ayu_AyuForwardStatusSentCount(tr::now,
															   lt_count1,
															   QString::number(state->sentMessages),
															   lt_count2,
															   QString::number(state->totalMessages)

	);

	QString chunkString = tr::ayu_AyuForwardStatusChunkCount(tr::now,
															 lt_count1,
															 QString::number(state->currentChunk + 1),
															 lt_count2,
															 QString::number(state->totalChunks)

	);

	const auto partString = state->totalChunks <= 1 ? messagesString : (messagesString + " • " + chunkString);

	QString status;

	if (state->state == ForwardState::State::Preparing) {
		status = tr::ayu_AyuForwardStatusPreparing(tr::now);
	} else if (state->state == ForwardState::State::Downloading) {
		status = tr::ayu_AyuForwardStatusLoadingMedia(tr::now);
	} else if (state->state == ForwardState::State::Sending) {
		status = tr::ayu_AyuForwardStatusForwarding(tr::now);
	} else {
		// ForwardState::State::Finished
		status = tr::ayu_AyuForwardStatusFinished(tr::now);
	}


	return std::make_pair(status, partString);
}

void ForwardState::updateBottomBar(const Main::Session &session, const PeerId *peer, const State &st) {
	state = st;

	session.changes().peerUpdated(session.data().peer(*peer), Data::PeerUpdate::Flag::Rights);
}

static Ui::PreparedList prepareMedia(not_null<Main::Session*> session,
									 const std::vector<not_null<HistoryItem*>> &items,
									 int &i,
									 std::vector<not_null<Data::Media*>> &groupMedia) {
	const auto prepare = [&](not_null<Data::Media*> media)
	{
		groupMedia.emplace_back(media);
		auto prepared = Ui::PreparedFile(AyuSync::filePath(session, media));
		Storage::PrepareDetails(prepared, st::sendMediaPreviewSize, PhotoSideLimit());
		return prepared;
	};

	const auto startItem = items[i];
	const auto media = startItem->media();
	const auto groupId = startItem->groupId();

	Ui::PreparedList list;
	list.files.emplace_back(prepare(media));

	if (!groupId.value) {
		return list;
	}

	for (int k = i + 1; k < items.size(); ++k) {
		const auto nextItem = items[k];
		if (nextItem->groupId() != groupId) {
			break;
		}
		if (const auto nextMedia = nextItem->media()) {
			list.files.emplace_back(prepare(nextMedia));
			i = k;
		}
	}
	return list;
}

void sendMedia(
	not_null<Main::Session*> session,
	std::shared_ptr<Ui::PreparedBundle> bundle,
	not_null<Data::Media*> primaryMedia,
	Api::MessageToSend &&message,
	bool sendImagesAsPhotos) {
	if (const auto document = primaryMedia->document(); document && document->sticker()) {
		AyuSync::sendStickerSync(session, message, document);
		return;
	}

	auto mediaType = [&]
	{
		if (const auto document = primaryMedia->document()) {
			if (document->isVoiceMessage()) {
				return SendMediaType::Audio;
			} else if (document->isVideoMessage()) {
				return SendMediaType::Round;
			}
			return SendMediaType::File;
		}
		return SendMediaType::Photo;
	}();

	if (mediaType == SendMediaType::Round || mediaType == SendMediaType::Audio) {
		const auto path = bundle->groups.front().list.files.front().path;

		QFile file(path);
		auto failed = false;
		if (!file.open(QIODevice::ReadOnly)) {
			LOG(("failed to open file for forward with reason: %1").arg(file.errorString()));
			failed = true;
		}
		auto data = file.readAll();

		if (!failed && data.size()) {
			file.close();
			AyuSync::sendVoiceSync(session,
								   data,
								   primaryMedia->document()->duration(),
								   mediaType == SendMediaType::Round,
								   message.action);
			return;
		}
		// at least try to send it as squared-video
	}

	// workaround for media albums consisting of video and photos
	if (sendImagesAsPhotos) {
		mediaType = SendMediaType::Photo;
	}

	for (auto &group : bundle->groups) {
		AyuSync::sendDocumentSync(
			session,
			group,
			mediaType,
			std::move(message.textWithTags),
			message.action);
	}
}

bool isAyuForwardNeeded(const std::vector<not_null<HistoryItem*>> &items) {
	for (const auto &item : items) {
		if (isAyuForwardNeeded(item)) {
			return true;
		}
	}
	return false;
}

bool isAyuForwardNeeded(not_null<HistoryItem*> item) {
	if (item->isDeleted() || item->isAyuNoForwards() || item->ttlDestroyAt()) {
		return true;
	}
	return false;
}

bool isFullAyuForwardNeeded(not_null<HistoryItem*> item) {
	return item->from()->isAyuNoForwards() || item->history()->peer->isAyuNoForwards();
}

struct ForwardChunk
{
	bool isAyuForwardNeeded;
	std::vector<not_null<HistoryItem*>> items;
};

void intelligentForward(
	not_null<Main::Session*> session,
	const Api::SendAction &action,
	Data::ResolvedForwardDraft draft) {
	const auto history = action.history;
	history->setForwardDraft(action.replyTo.topicRootId, {});

	const auto items = draft.items;
	const auto peer = history->peer;

	auto chunks = std::vector<ForwardChunk>();
	auto currentArray = std::vector<not_null<HistoryItem*>>();

	auto currentChunk = ForwardChunk({
		.isAyuForwardNeeded = isAyuForwardNeeded(items[0]),
		.items = currentArray
	});

	for (const auto &item : items) {
		if (isAyuForwardNeeded(item) != currentChunk.isAyuForwardNeeded) {
			currentChunk.items = currentArray;
			chunks.push_back(currentChunk);

			currentArray = std::vector<not_null<HistoryItem*>>();

			currentChunk = ForwardChunk({
				.isAyuForwardNeeded = isAyuForwardNeeded(item),
				.items = currentArray
			});
		}
		currentArray.push_back(item);
	}

	currentChunk.items = currentArray;
	chunks.push_back(currentChunk);

	auto state = std::make_shared<ForwardState>(chunks.size());
	forwardStates[peer->id] = state;


	for (const auto &chunk : chunks) {
		if (chunk.isAyuForwardNeeded) {
			forwardMessages(session, action, true, Data::ResolvedForwardDraft(chunk.items));
		} else {
			state->totalMessages = chunk.items.size();
			state->sentMessages = 0;
			state->updateBottomBar(*session, &peer->id, ForwardState::State::Sending);

			AyuSync::forwardMessagesSync(session, chunk.items, action, draft.options);

			state->sentMessages = state->totalMessages;

			state->updateBottomBar(*session, &peer->id, ForwardState::State::Finished);
		}
		state->currentChunk++;
	}

	state->updateBottomBar(*session, &peer->id, ForwardState::State::Finished);
}

void forwardMessages(
	not_null<Main::Session*> session,
	const Api::SendAction &action,
	bool forwardState,
	Data::ResolvedForwardDraft draft) {
	const auto items = draft.items;
	const auto history = action.history;
	const auto peer = history->peer;

	history->setForwardDraft(action.replyTo.topicRootId, {});

	std::shared_ptr<ForwardState> state;

	if (forwardState) {
		state = std::make_shared<ForwardState>(*forwardStates[peer->id]);
	} else {
		state = std::make_shared<ForwardState>(1);
	}

	forwardStates[peer->id] = state;

	std::unordered_map<uint64, uint64> groupIds;

	std::vector<not_null<HistoryItem*>> toBeDownloaded;


	for (const auto item : items) {
		if (mediaDownloadable(item->media())) {
			toBeDownloaded.push_back(item);
		}

		if (item->groupId()) {
			const auto currentId = groupIds.find(item->groupId().value);

			if (currentId == groupIds.end()) {
				groupIds[item->groupId().value] = base::RandomValue<uint64>();
			}
		}
	}
	state->totalMessages = items.size();
	if (!toBeDownloaded.empty()) {
		state->state = ForwardState::State::Downloading;
		state->updateBottomBar(*session, &peer->id, ForwardState::State::Downloading);
		AyuSync::loadDocuments(session, toBeDownloaded);
	}


	state->sentMessages = 0;
	state->updateBottomBar(*session, &peer->id, ForwardState::State::Sending);

	for (int i = 0; i < items.size(); i++) {
		const auto item = items[i];

		if (state->stopRequested) {
			state->updateBottomBar(*session, &peer->id, ForwardState::State::Finished);
			return;
		}

		auto extractedText = extractText(item);
		if (extractedText.empty() && !mediaDownloadable(item->media())) {
			continue;
		}

		auto message = Api::MessageToSend(Api::SendAction(session->data().history(peer->id)));
		message.action.replyTo = action.replyTo;

		if (draft.options != Data::ForwardOptions::NoNamesAndCaptions) {
			message.textWithTags = extractedText;
		}

		if (!mediaDownloadable(item->media())) {
			AyuSync::sendMessageSync(session, message);
		} else if (const auto media = item->media()) {
			if (media->poll()) {
				AyuSync::sendMessageSync(session, message);
				continue;
			}

			std::vector<not_null<Data::Media*>> groupMedia;
			auto preparedMedia = prepareMedia(session, items, i, groupMedia);

			Ui::SendFilesWay way;
			way.setGroupFiles(true);
			way.setSendImagesAsPhotos(false);
			for (const auto &media2 : groupMedia) {
				if (media2->photo()) {
					way.setSendImagesAsPhotos(true);
					break;
				}
			}

			auto groups = Ui::DivideByGroups(
				std::move(preparedMedia),
				way,
				peer->slowmodeApplied());

			auto bundle = Ui::PrepareFilesBundle(
				std::move(groups),
				way,
				message.textWithTags,
				false);
			sendMedia(session, bundle, media, std::move(message), way.sendImagesAsPhotos());
		}
		// if there are grouped messages
		// "i" is incremented in prepareMedia

		state->sentMessages = i + 1;
		state->updateBottomBar(*session, &peer->id, ForwardState::State::Sending);
	}
	state->updateBottomBar(*session, &peer->id, ForwardState::State::Finished);
}

} // namespace AyuFeatures::AyuForward
