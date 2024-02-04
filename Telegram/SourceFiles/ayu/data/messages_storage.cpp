// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#include "messages_storage.h"

#include "ayu/ayu_constants.h"
#include "ayu/data/ayu_database.h"
#include "ayu/utils/ayu_mapper.h"
#include "ayu/utils/telegram_helpers.h"

#include "base/unixtime.h"

#include "data/data_forum_topic.h"
#include "data/data_session.h"

#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"

#include "main/main_session.h"

namespace AyuMessages {

void map(HistoryMessageEdition &edition, not_null<HistoryItem*> item, EditedMessage &message) {
	message.userId = item->history()->owner().session().userId().bare;
	message.dialogId = getDialogIdFromPeer(item->history()->peer);
	message.groupedId = item->groupId().value;
	message.peerId = item->from()->id.value; // todo: ???
	message.fromId = item->from()->id.value;
	if (auto topic = item->history()->asTopic()) {
		message.topicId = topic->rootId().bare;
	}
	message.messageId = item->id.bare;
	message.date = item->date();
	message.flags = AyuMapper::mapItemFlagsToMTPFlags(item);

	if (auto edited = item->Get<HistoryMessageEdited>()) {
		message.editDate = edited->date;
	} else {
		message.editDate = base::unixtime::now();
	}

	message.views = item->viewsCount();
	message.entityCreateDate = base::unixtime::now(); // todo: rework

	auto serializedText = AyuMapper::serializeTextWithEntities(item);
	message.text = serializedText.first;
	message.textEntities = serializedText.second;

	// todo: implement mapping
	message.mediaPath = "/";
	message.documentType = DOCUMENT_TYPE_NONE;

	// message.documentSerialized;
	// message.thumbsSerialized;
	// message.documentAttributesSerialized;
	// message.mimeType;
}

void addEditedMessage(HistoryMessageEdition &edition, not_null<HistoryItem*> item) {
	EditedMessage message;
	map(edition, item, message);

	AyuDatabase::addEditedMessage(message);
}

std::vector<EditedMessage> getEditedMessages(not_null<HistoryItem*> item) {
	auto userId = item->history()->owner().session().userId().bare;
	auto dialogId = getDialogIdFromPeer(item->history()->peer);
	auto msgId = item->id.bare;

	return AyuDatabase::getEditedMessages(userId, dialogId, msgId);
}

bool hasRevisions(not_null<HistoryItem*> item) {
	auto userId = item->history()->owner().session().userId().bare;
	auto dialogId = getDialogIdFromPeer(item->history()->peer);
	auto msgId = item->id.bare;

	return AyuDatabase::hasRevisions(userId, dialogId, msgId);
}

}
