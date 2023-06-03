#pragma once

#include "entities.h"
#include "history/history_item.h"
#include "history/history.h"

namespace AyuDatabase {
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

    std::vector<EditedMessage> getEditedMessages(HistoryItem *item);

    bool editedMessagesTableExists();
}
