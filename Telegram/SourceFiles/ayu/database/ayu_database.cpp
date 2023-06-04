#include "ayu_database.h"

#include "entities.h"
#include "ayu/sqlite/sqlite_orm.h"
#include "QtCore/QString"
#include "main/main_session.h"

using namespace sqlite_orm;
auto storage = make_storage("ayugram.db",
                            make_table("editedmessage",
                                       make_column("userId", &EditedMessage::userId),
                                       make_column("dialogId", &EditedMessage::dialogId),
                                       make_column("messageId", &EditedMessage::messageId),
                                       make_column("text", &EditedMessage::text),
                                       make_column("isDocument", &EditedMessage::isDocument),
                                       make_column("path", &EditedMessage::path),
                                       make_column("date", &EditedMessage::date)
                            )
);


namespace AyuDatabase {
    void addEditedMessage(
            long userId,
            long dialogId,
            long messageId,
            const QString &text,
            bool isDocument,
            QString path,
            long date) {

        EditedMessage entity;
        entity.userId = userId;
        entity.dialogId = dialogId;
        entity.messageId = messageId;
        entity.text = text.toStdString();
        entity.isDocument = isDocument;
        entity.path = path.toStdString();
        entity.date = date;

        storage.sync_schema();

        storage.begin_transaction();
        storage.insert(entity);
        storage.commit();
    }

    std::vector<EditedMessage> getEditedMessages(
            long userId,
            long dialogId,
            long messageId
    ) {

        return storage.get_all<EditedMessage>(
                where(
                        c(&EditedMessage::userId) == userId and
                        c(&EditedMessage::dialogId) == dialogId and
                        c(&EditedMessage::messageId) == messageId)
        );
    }

    std::vector<EditedMessage> getEditedMessages(HistoryItem *item) {
        auto userId = item->displayFrom()->id.value;
        auto dialogId = item->history()->peer->id.value;
        auto msgId = item->id.bare;
//        auto some = &item->history()->session().account();

        return getEditedMessages((long) userId, (long) dialogId, (long) msgId);
    }

    bool editedMessagesTableExists() {
        return storage.table_exists("editedmessage");
    }
}
