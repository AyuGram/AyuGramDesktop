#include "ayu/data/deleted_history.h"

#include "ayu/data/ayu_database.h"
#include "ayu/utils/ayu_mapper.h"
#include "ayu/utils/telegram_helpers.h"
#include "ayu/ayu_settings.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

namespace AyuMessages {
namespace {

[[nodiscard]] int HistoryMessageId(const MTPMessage &message) {
	return message.match([](const auto &data) { return data.vid().v; });
}

} // namespace

bool HasDeletedHistory(not_null<PeerData*> peer) {
	return peer->asUser()
		&& AyuSettings::getInstance().saveDeletedMessages()
		&& AyuDatabase::hasDeletedMessages(
			peer->session().userId().bare & PeerId::kChatTypeMask,
			getDialogIdFromPeer(peer),
			0);
}

DeletedHistoryPage RestoreDeletedHistory(
		not_null<PeerData*> peer,
		const QVector<MTPMessage> &messages,
		HistoryPageRequest request,
		bool onlyNewer) {
	auto result = DeletedHistoryPage{ .messages = messages };
	if (!peer->asUser()
		|| !AyuSettings::getInstance().saveDeletedMessages()
		|| request.limit <= 0) {
		return result;
	}
	result.restorationEnabled = true;

	const auto userId = peer->session().userId().bare
		& PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(peer);
	auto deleted = std::vector<DeletedMessage>();
	if (!onlyNewer) {
		deleted = AyuDatabase::getDeletedHistoryMessages(
			userId,
			dialogId,
			0,
			request.offsetId,
			request.limit,
			false);
	}
	if (request.offsetId > 0 && request.addOffset < 0) {
		auto newer = AyuDatabase::getDeletedHistoryMessages(
			userId,
			dialogId,
			request.offsetId - 1,
			0,
			request.limit,
			true);
		deleted.insert(deleted.end(), newer.begin(), newer.end());
	}
	if (deleted.empty()) {
		if (onlyNewer) {
			const auto end = std::remove_if(
				result.messages.begin(),
				result.messages.end(),
				[&](const auto &message) {
					return HistoryMessageId(message) < request.offsetId;
				});
			result.messages.erase(end, result.messages.end());
		}
		return result;
	}

	auto serverEntries = std::vector<HistoryPageEntry>();
	serverEntries.reserve(messages.size());
	for (auto i = 0; i != messages.size(); ++i) {
		const auto &message = messages[i];
		serverEntries.push_back({
			.id = HistoryMessageId(message),
			.index = i,
			.empty = (message.type() == mtpc_messageEmpty),
		});
	}
	auto deletedEntries = std::vector<HistoryPageEntry>();
	deletedEntries.reserve(deleted.size());
	for (auto i = std::size_t(0); i != deleted.size(); ++i) {
		deletedEntries.push_back({
			.id = deleted[i].messageId,
			.index = int(i),
		});
	}
	const auto page = MergeHistoryPage(serverEntries, deletedEntries, request);
	const auto history = peer->owner().history(peer);
	result.messages.clear();
	result.messages.reserve(page.size());
	for (const auto &entry : page) {
		if (onlyNewer && entry.id < request.offsetId) {
			continue;
		}
		if (entry.deleted) {
			const auto message = AyuMapper::DeletedMessageToMTP(
				peer,
				deleted[entry.index]);
			result.messages.push_back(message);
			const auto existing = peer->owner().message(peer, MsgId(entry.id));
			const auto serverEmpty = history->lastServerMessageKnown()
				&& !history->lastServerMessage();
			if (!existing
				|| existing->isDeleted()
				|| existing->isUnavailable()
				|| serverEmpty) {
				const auto item = history->createItem(
					MsgId(entry.id),
					message,
					MessageFlags(),
					false);
				item->setDeleted();
				item->markDeletedAnimated();
			}
		} else {
			result.messages.push_back(messages[entry.index]);
		}
	}
	return result;
}

void ApplyDeletedHistory(
		not_null<History*> history,
		const DeletedHistoryPage &page) {
	if (page.restorationEnabled) {
		const auto newer = AyuDatabase::getDeletedHistoryMessages(
			history->session().userId().bare & PeerId::kChatTypeMask,
			getDialogIdFromPeer(history->peer),
			history->maxMsgId().bare,
			0,
			1,
			true);
		if (!newer.empty()) {
			history->setNotLoadedAtBottom();
		}
	}
}

} // namespace AyuMessages
