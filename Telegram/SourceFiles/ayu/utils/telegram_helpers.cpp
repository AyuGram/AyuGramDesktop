// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "telegram_helpers.h"
#include <functional>
#include <QTimer>

#include "apiwrap.h"
#include "api/api_text_entities.h"

#include "lang_auto.h"
#include "ayu/ayu_worker.h"
#include "ayu/data/entities.h"
#include "core/mime_type.h"
#include "data/data_channel.h"
#include "data/data_forum.h"
#include "data/data_forum_topic.h"
#include "data/data_histories.h"
#include "data/data_peer_id.h"
#include "data/data_photo.h"
#include "data/data_user.h"
#include "inline_bots/inline_bot_result.h"

#include "data/data_document.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_unread_things.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "ui/text/format_values.h"

// https://github.com/AyuGram/AyuGram4AX/blob/rewrite/TMessagesProj/src/main/java/com/radolyn/ayugram/AyuConstants.java
std::unordered_set<ID> ayugram_channels = {
	1905581924, // @ayugramchat
	1794457129, // @ayugram1338
	1434550607, // @radolyn
	1947958814, // @ayugramfun
	1815864846, // @ayugramfcm
	2130395384, // @ayugram_easter
};

std::unordered_set<ID> ayugram_devs = {
	139303278, // @alexeyzavar
	778327202, // @sharapagorg
	238292700, // @MaxPlays
	1795176335, // @radolyn_services
};

// https://github.com/AyuGram/AyuGram4AX/blob/rewrite/TMessagesProj/src/main/java/com/exteragram/messenger/ExteraConfig.java
std::unordered_set<ID> extera_channels = {
	1233768168,
	1524581881,
	1571726392,
	1632728092,
	1172503281,
	1877362358,
	// custom
	1812843581, // @moeGramX
	1634905346, // @moex_log
	1516526055, // @moexci
	1622008530, // @moe_chat
};

std::unordered_set<ID> extera_devs = {
	963080346,
	1282540315,
	1374434073,
	388099852,
	1972014627,
	168769611,
	480000401,
	639891381,
	1773117711,
	5330087923,
	666154369,
	139303278
};

Main::Session *getSession(ID userId) {
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			if (session->userId().bare == userId) {
				return session;
			}
		}
	}

	return nullptr;
}

bool accountExists(ID userId) {
	return userId == 0 || getSession(userId) != nullptr;
}

void dispatchToMainThread(std::function<void()> callback, int delay) {
	auto timer = new QTimer();
	timer->moveToThread(qApp->thread());
	timer->setSingleShot(true);
	QObject::connect(timer,
					 &QTimer::timeout,
					 [=]()
					 {
						 callback();
						 timer->deleteLater();
					 });
	QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, delay));
}

not_null<History*> getHistoryFromDialogId(ID dialogId, Main::Session *session) {
	if (dialogId > 0) {
		return session->data().history(peerFromUser(dialogId));
	}

	auto history = session->data().history(peerFromChannel(abs(dialogId)));
	if (history->folderKnown()) {
		return history;
	}

	return session->data().history(peerFromChat(abs(dialogId)));
}

ID getDialogIdFromPeer(not_null<PeerData*> peer) {
	auto peerId = peerIsUser(peer->id)
					  ? peerToUser(peer->id).bare
					  : peerIsChat(peer->id)
							? peerToChat(peer->id).bare
							: peerIsChannel(peer->id)
								  ? peerToChannel(peer->id).bare
								  : peer->id.value;

	if (peer->isChannel() || peer->isChat()) {
		peerId = -peerId;
	}

	return peerId;
}

ID getBareID(not_null<PeerData*> peer) {
	return peerIsUser(peer->id)
			   ? peerToUser(peer->id).bare
			   : peerIsChat(peer->id)
					 ? peerToChat(peer->id).bare
					 : peerIsChannel(peer->id)
						   ? peerToChannel(peer->id).bare
						   : peer->id.value;
}

bool isAyuGramRelated(ID peerId) {
	return ayugram_devs.contains(peerId) || ayugram_channels.contains(peerId);
}

bool isExteraRelated(ID peerId) {
	return extera_devs.contains(peerId) || extera_channels.contains(peerId);
}

void MarkAsReadChatList(not_null<Dialogs::MainList*> list) {
	auto mark = std::vector<not_null<History*>>();
	for (const auto &row : list->indexed()->all()) {
		if (const auto history = row->history()) {
			mark.push_back(history);
		}
	}
	ranges::for_each(mark, MarkAsReadThread);
}

void readMentions(base::weak_ptr<Data::Thread> weakThread) {
	const auto thread = weakThread.get();
	if (!thread) {
		return;
	}
	const auto peer = thread->peer();
	const auto topic = thread->asTopic();
	const auto rootId = topic ? topic->rootId() : 0;
	using Flag = MTPmessages_ReadMentions::Flag;
	peer->session().api().request(MTPmessages_ReadMentions(
		MTP_flags(rootId ? Flag::f_top_msg_id : Flag()),
		peer->input,
		MTP_int(rootId)
	)).done([=](const MTPmessages_AffectedHistory &result)
	{
		const auto offset = peer->session().api().applyAffectedHistory(
			peer,
			result);
		if (offset > 0) {
			readMentions(weakThread);
		} else {
			peer->owner().history(peer)->clearUnreadMentionsFor(rootId);
		}
	}).send();
}

void readReactions(base::weak_ptr<Data::Thread> weakThread) {
	const auto thread = weakThread.get();
	if (!thread) {
		return;
	}
	const auto topic = thread->asTopic();
	const auto peer = thread->peer();
	const auto rootId = topic ? topic->rootId() : 0;
	using Flag = MTPmessages_ReadReactions::Flag;
	peer->session().api().request(MTPmessages_ReadReactions(
		MTP_flags(rootId ? Flag::f_top_msg_id : Flag(0)),
		peer->input,
		MTP_int(rootId)
	)).done([=](const MTPmessages_AffectedHistory &result)
	{
		const auto offset = peer->session().api().applyAffectedHistory(
			peer,
			result);
		if (offset > 0) {
			readReactions(weakThread);
		} else {
			peer->owner().history(peer)->clearUnreadReactionsFor(rootId);
		}
	}).send();
}

void MarkAsReadThread(not_null<Data::Thread*> thread) {
	const auto readHistory = [&](not_null<History*> history)
	{
		history->owner().histories().readInbox(history);
	};
	const auto sendReadMentions = [=](
		not_null<Data::Thread*> thread)
	{
		readMentions(base::make_weak(thread));
	};
	const auto sendReadReactions = [=](
		not_null<Data::Thread*> thread)
	{
		readReactions(base::make_weak(thread));
	};

	if (thread->chatListBadgesState().unread) {
		if (const auto forum = thread->asForum()) {
			forum->enumerateTopics([](
				not_null<Data::ForumTopic*> topic)
				{
					MarkAsReadThread(topic);
				});
		} else if (const auto history = thread->asHistory()) {
			readHistory(history);
			if (const auto migrated = history->migrateSibling()) {
				readHistory(migrated);
			}
		} else if (const auto topic = thread->asTopic()) {
			topic->readTillEnd();
		}
	}

	if (thread->unreadMentions().has()) {
		sendReadMentions(thread);
	}

	if (thread->unreadReactions().has()) {
		sendReadReactions(thread);
	}

	AyuWorker::markAsOnline(&thread->session());
}

void readHistory(not_null<HistoryItem*> message) {
	const auto history = message->history();
	const auto tillId = message->id;

	history->session().data().histories()
		.sendRequest(history,
					 Data::Histories::RequestType::ReadInbox,
					 [=](Fn<void()> finish)
					 {
						 if (const auto channel = history->peer->asChannel()) {
							 return history->session().api().request(MTPchannels_ReadHistory(
								 channel->inputChannel,
								 MTP_int(tillId)
							 )).done([=] { AyuWorker::markAsOnline(&history->session()); }).send();
						 }

						 return history->session().api().request(MTPmessages_ReadHistory(
							 history->peer->input,
							 MTP_int(tillId)
						 )).done([=](const MTPmessages_AffectedMessages &result)
						 {
							 history->session().api().applyAffectedMessages(history->peer, result);
							 AyuWorker::markAsOnline(&history->session());
						 }).fail([=]
						 {
						 }).send();
					 });

	if (history->unreadMentions().has()) {
		readMentions(history->asThread());
	}

	if (history->unreadReactions().has()) {
		readReactions(history->asThread());
	}
}

QString formatTTL(int time) {
	if (time == 0x7FFFFFFF) {
		return QString("👀 %1").arg(tr::ayu_OneViewTTL(tr::now));
	}

	return QString("🕓 %1s").arg(time);
}

QString getDCName(int dc) {
	const auto getName = [=](int dc)
	{
		switch (dc) {
			case 1:
			case 3: return "Miami FL, USA";
			case 2:
			case 4: return "Amsterdam, NL";
			case 5: return "Singapore, SG";
			default: return "UNKNOWN";
		}
	};

	if (dc < 1) {
		return {"DC_UNKNOWN"};
	}

	return QString("DC%1, %2").arg(dc).arg(getName(dc));
}

QString getLocalizedAt() {
	static const auto val = tr::lng_mediaview_date_time(
		tr::now,
		lt_date,
		"",
		lt_time,
		"");
	return val;
}

QString formatDateTime(const QDateTime &date) {
	const auto locale = QLocale::system();
	const auto datePart = locale.toString(date.date(), QLocale::ShortFormat);
	const auto timePart = locale.toString(date, "HH:mm:ss");

	return datePart + getLocalizedAt() + timePart;
}

QString getMediaSize(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	int64 size = -1;
	if (document) {
		// any file
		size = document->size;
	} else if (photo && photo->hasVideo()) {
		// video
		size = photo->videoByteSize(Data::PhotoSize::Large);
		if (size == 0) {
			size = photo->videoByteSize(Data::PhotoSize::Small);
		}
		if (size == 0) {
			size = photo->videoByteSize(Data::PhotoSize::Thumbnail);
		}
	} else if (photo && !photo->hasVideo()) {
		// photo
		size = photo->imageByteSize(Data::PhotoSize::Large);
		if (size == 0) {
			size = photo->imageByteSize(Data::PhotoSize::Small);
		}
		if (size == 0) {
			size = photo->imageByteSize(Data::PhotoSize::Thumbnail);
		}
	}

	if (size == -1) {
		return {};
	}

	return Ui::FormatSizeText(size);
}

QString getMediaMime(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	if (document) {
		// any file
		return document->mimeString();
	} else if (photo && photo->hasVideo()) {
		// video
		return "video/mp4";
	} else if (photo && !photo->hasVideo()) {
		// photo
		return "image/jpeg";
	}

	return {};
}

QString getMediaName(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();

	if (document) {
		return document->filename();
	}

	return {};
}

QString getMediaResolution(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	const auto formatQSize = [=](QSize size)
	{
		if (size.isNull() || size.isEmpty() || !size.isValid()) {
			return QString();
		}

		return QString("%1x%2").arg(size.width()).arg(size.height());
	};

	if (document) {
		return formatQSize(document->dimensions);
	} else if (photo) {
		auto result = photo->size(Data::PhotoSize::Large);
		if (!result.has_value()) {
			result = photo->size(Data::PhotoSize::Small);
		}
		if (!result.has_value()) {
			result = photo->size(Data::PhotoSize::Thumbnail);
		}
		return result.has_value() ? formatQSize(result.value()) : QString();
	}

	return {};
}

QString getMediaDC(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	if (document) {
		return getDCName(document->getDC());
	} else if (photo) {
		return getDCName(photo->getDC());
	}

	return {};
}

void resolveUser(ID userId, const QString &username, Main::Session *session, const Callback &callback) {
	auto normalized = username.trimmed().toLower();
	if (normalized.isEmpty()) {
		callback(QString(), nullptr);
		return;
	}
	normalized = normalized.startsWith("@") ? normalized.mid(1) : normalized;

	if (normalized.isEmpty()) {
		callback(QString(), nullptr);
		return;
	}

	session->api().request(MTPcontacts_ResolveUsername(
		MTP_string(normalized)
	)).done([=](const MTPcontacts_ResolvedPeer &result)
	{
		Expects(result.type() == mtpc_contacts_resolvedPeer);

		auto &data = result.c_contacts_resolvedPeer();
		session->data().processUsers(data.vusers());
		session->data().processChats(data.vchats());
		const auto peer = session->data().peerLoaded(
			peerFromMTP(data.vpeer()));
		if (const auto user = peer ? peer->asUser() : nullptr) {
			if (user->id.value == userId) {
				callback(normalized, user);
				return;
			}
		}

		callback(normalized, nullptr);
	}).fail([=]
	{
		callback(QString(), nullptr);
	}).send();
}

void searchUser(ID userId, Main::Session *session, bool searchUserFlag, bool cache, const Callback &callback) {
	if (!session) {
		callback(QString(), nullptr);
		return;
	}

	const auto botId = 1696868284;
	const auto bot = session->data().userLoaded(botId);

	if (!bot) {
		if (searchUserFlag) {
			resolveUser(botId,
						"tgdb_bot",
						session,
						[=](const QString &title, UserData *data)
						{
							searchUser(userId, session, false, false, callback);
						});
		} else {
			callback(QString(), nullptr);
		}
		return;
	}

	session->api().request(MTPmessages_GetInlineBotResults(
		MTP_flags(0),
		bot->inputUser,
		MTP_inputPeerEmpty(),
		MTPInputGeoPoint(),
		MTP_string(QString::number(userId)),
		MTP_string("")
	)).done([=](const MTPmessages_BotResults &result)
	{
		if (result.type() != mtpc_messages_botResults) {
			callback(QString(), nullptr);
			return;
		}
		auto &d = result.c_messages_botResults();
		session->data().processUsers(d.vusers());

		auto &v = d.vresults().v;
		auto queryId = d.vquery_id().v;

		auto added = 0;
		for (const auto &res : v) {
			const auto message = res.match(
				[&](const MTPDbotInlineResult &data)
				{
					return &data.vsend_message();
				},
				[&](const MTPDbotInlineMediaResult &data)
				{
					return &data.vsend_message();
				});

			const auto text = message->match(
				[&](const MTPDbotInlineMessageMediaAuto &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageText &data)
				{
					return qs(data.vmessage());
				},
				[&](const MTPDbotInlineMessageMediaGeo &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaVenue &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaContact &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaInvoice &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaWebPage &data)
				{
					return QString();
				});

			if (text.isEmpty()) {
				continue;
			}

			ID id = 0; // 🆔
			QString title; // 🏷
			QString username; // 📧

			for (const auto &line : text.split('\n')) {
				if (line.startsWith("🆔")) {
					id = line.mid(line.indexOf(':') + 1).toLongLong();
				} else if (line.startsWith("🏷")) {
					title = line.mid(line.indexOf(':') + 1);
				} else if (line.startsWith("📧")) {
					username = line.mid(line.indexOf(':') + 1);
				}
			}

			if (id == 0) {
				continue;
			}

			if (id != userId) {
				continue;
			}

			if (!username.isEmpty()) {
				resolveUser(id,
							username,
							session,
							[=](const QString &titleInner, UserData *data)
							{
								if (data) {
									callback(titleInner, data);
								} else {
									callback(title, nullptr);
								}
							});
				return;
			}

			if (!title.isEmpty()) {
				callback(title, nullptr);
			}
		}

		callback(QString(), nullptr);
	}).fail([=]
	{
		callback(QString(), nullptr);
	}).handleAllErrors().send();
}

void searchById(ID userId, Main::Session *session, bool retry, const Callback &callback) {
	if (userId == 0 || !session) {
		callback(QString(), nullptr);
		return;
	}

	const auto dataLoaded = session->data().userLoaded(userId);
	if (dataLoaded) {
		callback(dataLoaded->username(), dataLoaded);
		return;
	}

	searchUser(userId,
			   session,
			   true,
			   true,
			   [=](const QString &title, UserData *data)
			   {
				   if (data && data->accessHash()) {
					   callback(title, data);
				   } else {
					   if (retry) {
						   searchById(0x100000000 + userId, session, false, callback);
					   } else {
						   callback(QString(), nullptr);
					   }
				   }
			   });
}

void searchById(ID userId, Main::Session *session, const Callback &callback) {
	searchById(userId, session, true, callback);
}
