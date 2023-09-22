// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "telegram_helpers.h"
#include <functional>
#include <QTimer>

#include "api/api_text_entities.h"

#include "apiwrap.h"
#include "data/data_forum.h"
#include "data/data_forum_topic.h"
#include "data/data_histories.h"
#include "data/data_peer_id.h"

#include "ayu/sync/models.h"

#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_unread_things.h"


Main::Session *getSession(ID userId)
{
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			if (session->userId().bare == userId) {
				return session;
			}
		}
	}

	return nullptr;
}

bool accountExists(ID userId)
{
	return userId == 0 || getSession(userId) != nullptr;
}

void dispatchToMainThread(std::function<void()> callback)
{
	auto timer = new QTimer();
	timer->moveToThread(qApp->thread());
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, [=]()
	{
		callback();
		timer->deleteLater();
	});
	QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}

not_null<History *> getHistoryFromDialogId(ID dialogId, Main::Session *session)
{
	if (dialogId > 0) {
		return session->data().history(peerFromUser(dialogId));
	}

	auto history = session->data().history(peerFromChannel(abs(dialogId)));
	if (history->folderKnown()) {
		return history;
	}

	return session->data().history(peerFromChat(abs(dialogId)));
}

ID getDialogIdFromPeer(not_null<PeerData *> peer)
{
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

std::pair<std::string, std::string> serializeTextWithEntities(not_null<HistoryItem *> item)
{
	if (item->emptyText()) {
		return std::make_pair("", "");
	}

	auto textWithEntities = item->originalText();
	auto text = textWithEntities.text.toStdString();
	auto entities = EntitiesToMTP(&item->history()->owner().session(), textWithEntities.entities,
								  Api::ConvertOption::SkipLocal);

	if (entities.v.isEmpty()) {
		return std::make_pair(text, "");
	}

	auto buff = mtpBuffer();
	for (auto entity : entities.v) {
		entity.write(buff);
	}

	return std::make_pair(text, std::string(reinterpret_cast<char *>(buff.data()), buff.size()));
}

void MarkAsReadChatList(not_null<Dialogs::MainList *> list)
{
	auto mark = std::vector<not_null<History *>>();
	for (const auto &row : list->indexed()->all()) {
		if (const auto history = row->history()) {
			mark.push_back(history);
		}
	}
	ranges::for_each(mark, MarkAsReadThread);
}

void MarkAsReadThread(not_null<Data::Thread *> thread)
{
	const auto readHistory = [&](not_null<History *> history)
	{
		history->owner().histories().readInbox(history);
	};
	const auto readMentions = [=](
		base::weak_ptr<Data::Thread> weakThread,
		auto resend) -> void
	{
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
						resend(weakThread, resend);
					}
					else {
						peer->owner().history(peer)->clearUnreadMentionsFor(rootId);
					}
				}).send();
	};
	const auto sendReadMentions = [=](
		not_null<Data::Thread *> thread)
	{
		readMentions(base::make_weak(thread), readMentions);
	};

	const auto readReactions = [=](
		base::weak_ptr<Data::Thread> weakThread,
		auto resend) -> void
	{
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
						resend(weakThread, resend);
					}
					else {
						peer->owner().history(peer)->clearUnreadReactionsFor(rootId);
					}
				}).send();
	};
	const auto sendReadReactions = [=](
		not_null<Data::Thread *> thread)
	{
		readReactions(base::make_weak(thread), readReactions);
	};

	if (thread->chatListBadgesState().unread) {
		if (const auto forum = thread->asForum()) {
			forum->enumerateTopics([](
				not_null<Data::ForumTopic *> topic)
								   {
									   MarkAsReadThread(topic);
								   });
		}
		else if (const auto history = thread->asHistory()) {
			readHistory(history);
			if (const auto migrated = history->migrateSibling()) {
				readHistory(migrated);
			}
		}
		else if (const auto topic = thread->asTopic()) {
			topic->readTillEnd();
		}
	}

	if (thread->unreadMentions().has()) {
		sendReadMentions(thread);
	}

	if (thread->unreadReactions().has()) {
		sendReadReactions(thread);
	}
}