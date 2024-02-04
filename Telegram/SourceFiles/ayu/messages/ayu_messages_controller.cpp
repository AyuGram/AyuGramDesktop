// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_messages_controller.h"

#include "ayu/ayu_constants.h"
#include "ayu/database/ayu_database.h"
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

std::optional<AyuMessagesController> controller = std::nullopt;

void initialize() {
	if (controller.has_value()) {
		return;
	}

	controller = AyuMessagesController();
}

AyuMessagesController &getInstance() {
	initialize();
	return controller.value();
}

void map(HistoryMessageEdition &edition, not_null<HistoryItem *> item, EditedMessage &message) {
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

	auto serializedText = serializeTextWithEntities(item);
	message.text = serializedText.first;
	//		message.textEntities = serializedText.second;

	// todo:
	message.mediaPath = "/";
	message.documentType = DOCUMENT_TYPE_NONE;

	// message.documentSerialized;
	// message.thumbsSerialized;
	// message.documentAttributesSerialized;
	// message.mimeType;
}

void AyuMessagesController::addEditedMessage(HistoryMessageEdition &edition, not_null<HistoryItem *> item) {
	EditedMessage message;
	map(edition, item, message);

	AyuDatabase::addEditedMessage(message);
}

std::vector<EditedMessage> AyuMessagesController::getEditedMessages(HistoryItem *item) {
	auto userId = item->history()->owner().session().userId().bare;
	auto dialogId = getDialogIdFromPeer(item->history()->peer);
	auto msgId = item->id.bare;

	return AyuDatabase::getEditedMessages(userId, dialogId, msgId);
}

bool AyuMessagesController::hasRevisions(not_null<HistoryItem *> item) {
	auto userId = item->history()->owner().session().userId().bare;
	auto dialogId = getDialogIdFromPeer(item->history()->peer);
	auto msgId = item->id.bare;

	return AyuDatabase::hasRevisions(userId, dialogId, msgId);
}

}
