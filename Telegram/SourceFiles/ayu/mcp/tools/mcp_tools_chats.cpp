// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/mcp/tools/mcp_tools.h"

#include "ayu/mcp/mcp_registry.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_folder.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "history/history.h"
#include "main/main_session.h"

namespace AyuMcp {
namespace {

constexpr auto kDefaultLimit = 100;

[[nodiscard]] QString peerType(not_null<PeerData*> peer) {
	if (peer->isBroadcast()) {
		return u"channel"_q;
	} else if (peer->isChannel()) {
		return u"supergroup"_q;
	} else if (peer->isChat()) {
		return u"group"_q;
	} else if (peer->isBot()) {
		return u"bot"_q;
	}
	return u"user"_q;
}

[[nodiscard]] bool matches(not_null<PeerData*> peer, const QString &query) {
	return query.isEmpty()
		|| peer->name().contains(query, Qt::CaseInsensitive)
		|| peer->username().contains(query, Qt::CaseInsensitive);
}

void appendChats(
		QJsonArray &chats,
		not_null<const Dialogs::MainList*> list,
		bool archived,
		const QString &query,
		int limit) {
	for (const auto &row : list->indexed()->all()) {
		if (chats.size() >= limit) {
			return;
		}
		const auto history = row->history();
		if (!history || !matches(history->peer, query)) {
			continue;
		}
		const auto peer = history->peer;
		auto chat = QJsonObject{
			{ u"id"_q, qint64(getDialogIdFromPeer(peer)) },
			{ u"title"_q, peer->name() },
			{ u"type"_q, peerType(peer) },
			{ u"unread"_q, history->unreadCount() },
			{ u"archived"_q, archived },
		};
		if (!peer->username().isEmpty()) {
			chat.insert(u"username"_q, peer->username());
		}
		chats.append(chat);
	}
}

[[nodiscard]] QString render(const QJsonArray &chats) {
	if (chats.isEmpty()) {
		return u"No chats matched."_q;
	}
	auto lines = QStringList();
	for (const auto &value : chats) {
		const auto chat = value.toObject();
		auto parts = QStringList{
			QString::number(qint64(chat.value(u"id"_q).toDouble())),
			chat.value(u"title"_q).toString(),
			chat.value(u"type"_q).toString(),
		};
		if (chat.contains(u"username"_q)) {
			parts.append('@' + chat.value(u"username"_q).toString());
		}
		if (const auto unread = chat.value(u"unread"_q).toInt()) {
			parts.append(u"unread %1"_q.arg(unread));
		}
		if (chat.value(u"archived"_q).toBool()) {
			parts.append(u"archived"_q);
		}
		lines.append(parts.join(u" | "_q));
	}
	return lines.join('\n');
}

void listChats(
		const CallContext &context,
		const QJsonObject &arguments,
		Fn<void(ToolResult)> done) {
	const auto query = arguments.value(u"query"_q).toString().trimmed();
	const auto limit = arguments.value(u"limit"_q).toInt(kDefaultLimit);
	auto &owner = context.session->data();
	auto chats = QJsonArray();
	appendChats(chats, owner.chatsList(), false, query, limit);
	if (const auto archive = owner.folderLoaded(Data::Folder::kId)) {
		appendChats(chats, archive->chatsList(), true, query, limit);
	}
	done({
		.text = render(chats),
		.structured = QJsonObject{ { u"chats"_q, chats } },
	});
}

} // namespace

void registerChatTools() {
	registerTool({
		.name = u"list_chats"_q,
		.title = u"List chats"_q,
		.description = u"Lists chats of the active account, most recent "
			"first, archived ones last. The returned id identifies the chat "
			"in other tools."_q,
		.inputSchema = parseSchema(R"({
			"type": "object",
			"properties": {
				"query": {
					"type": "string",
					"description": "Case-insensitive part of a title or username."
				},
				"limit": {
					"type": "integer",
					"minimum": 1,
					"maximum": 500,
					"default": 100
				}
			},
			"additionalProperties": false
		})"),
		.handler = listChats,
	});
}

} // namespace AyuMcp
