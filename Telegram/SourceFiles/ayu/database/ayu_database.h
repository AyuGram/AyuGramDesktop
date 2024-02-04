// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "entities.h"
#include "history/history.h"
#include "history/history_item.h"

namespace AyuDatabase {

void initialize();

void addEditedMessage(const EditedMessage &message);
std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId);
bool hasRevisions(ID userId, ID dialogId, ID messageId);

}
