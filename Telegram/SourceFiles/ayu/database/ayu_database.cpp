// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "ayu_database.h"

#include <QString>

#include "entities.h"
#include "ayu/libs/sqlite/sqlite_orm.h"
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


namespace AyuDatabase
{
	void addEditedMessage(
		long userId,
		long dialogId,
		long messageId,
		const QString& text,
		bool isDocument,
		QString path,
		long date)
	{
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
	)
	{
		return storage.get_all<EditedMessage>(
			where(
				c(&EditedMessage::userId) == userId and
				c(&EditedMessage::dialogId) == dialogId and
				c(&EditedMessage::messageId) == messageId)
		);
	}

	std::vector<EditedMessage> getEditedMessages(HistoryItem* item)
	{
		auto userId = item->displayFrom()->id.value;
		auto dialogId = item->history()->peer->id.value;
		auto msgId = item->id.bare;
		//        auto some = &item->history()->session().account();

		return getEditedMessages(static_cast<long>(userId), static_cast<long>(dialogId), static_cast<long>(msgId));
	}

	bool editedMessagesTableExists()
	{
		return storage.table_exists("editedmessage");
	}
}
