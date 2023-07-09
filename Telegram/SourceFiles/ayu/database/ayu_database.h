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

namespace AyuDatabase
{
	void addEditedMessage(
		long userId,
		long dialogId,
		long messageId,
		const QString& text,
		bool isDocument,
		QString path,
		long date);

	std::vector<EditedMessage> getEditedMessages(
		long userId,
		long dialogId,
		long messageId
	);

	std::vector<EditedMessage> getEditedMessages(HistoryItem* item);

	bool editedMessagesTableExists();
}
