// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_mapper.h"

#include "history/history_item.h"
#include "history/history_item_components.h"
#include "history/history.h"


namespace AyuMapper
{

int mapItemFlagsToMTPFlags(not_null<HistoryItem *> item)
{
	int flags = 0;

	const auto thread = item->topic()
						? (Data::Thread *)item->topic()
						: item->history();
	const auto unseen = item->unread(thread);
	if (unseen) {
		flags |= 1;
	}

	if (item->out()) {
		flags |= 2;
	}

	if (const auto reply = item->Get<HistoryMessageForwarded>()) {
		flags |= 4;
	}

	if (const auto reply = item->Get<HistoryMessageReply>()) {
		flags |= 8;
	}

	if (item->mentionsMe()) {
		flags |= 16;
	}

	if (item->isUnreadMedia()) {
		flags |= 32;
	}

	if (item->isSilent()) {
		flags |= 8192;
	}

	if (item->isPost()) {
		flags |= 16384;
	}

	if (item->isScheduled()) {
		flags |= 262144;
	}

	// todo: legacy
//	if (item->isLegacy()) {
//		flags |= 524288;
//	}

	if (item->hideEditedBadge()) {
		flags |= 2097152;
	}

	if (item->isPinned()) {
		flags |= 16777216;
	}

	if (item->forbidsForward()) {
		flags |= 67108864;
	}

	if (item->topic() && item->topicRootId() == item->id) {
		flags |= 134217728;
	}

	return flags;
}

}
