// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#include "ayu/data/messages_storage.h"

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

template<typename DerivedMessage>
std::vector<AyuMessageBase> convertToBase(const std::vector<DerivedMessage> &messages) {
	std::vector<AyuMessageBase> based;
	based.reserve(messages.size());
	for (const auto &msg : messages) {
		based.push_back(static_cast<AyuMessageBase>(msg));
	}
	return based;
}

void map(not_null<HistoryItem*> item, AyuMessageBase &message) {
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
	map(item, message);

	if (message.text.empty()) {
		return;
	}

	AyuDatabase::addEditedMessage(message);
}

std::vector<AyuMessageBase> getEditedMessages(not_null<HistoryItem*> item, ID minId, ID maxId, int totalLimit) {
	auto userId = item->history()->owner().session().userId().bare;
	auto dialogId = getDialogIdFromPeer(item->history()->peer);
	auto msgId = item->id.bare;

	return convertToBase(AyuDatabase::getEditedMessages(userId, dialogId, msgId, minId, maxId, totalLimit));
}

bool hasRevisions(not_null<HistoryItem*> item) {
	auto userId = item->history()->owner().session().userId().bare;
	auto dialogId = getDialogIdFromPeer(item->history()->peer);
	auto msgId = item->id.bare;

	return AyuDatabase::hasRevisions(userId, dialogId, msgId);
}

void addDeletedMessage(not_null<HistoryItem*> item) {
	DeletedMessage message;
	map(item, message);

	if (message.text.empty()) {
		return;
	}

	AyuDatabase::addDeletedMessage(message);
}

std::vector<AyuMessageBase> getDeletedMessages(not_null<PeerData*> peer, ID minId, ID maxId, int totalLimit) {
	return convertToBase(AyuDatabase::getDeletedMessages(getDialogIdFromPeer(peer), minId, maxId, totalLimit));
}

bool hasDeletedMessages(not_null<PeerData*> peer) {
	return AyuDatabase::hasDeletedMessages(getDialogIdFromPeer(peer));
}

}
