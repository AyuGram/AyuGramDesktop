// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "ayu/database/entities.h"

#include "history/history_item_edition.h"

namespace AyuMessages
{

class ayu_messages_controller
{
public:
	void addEditedMessage(HistoryMessageEdition &edition, not_null<HistoryItem *> item);
	std::vector<EditedMessage> getEditedMessages(HistoryItem *item);
	bool hasRevisions(not_null<HistoryItem *> item);
};

ayu_messages_controller &getInstance();

}
