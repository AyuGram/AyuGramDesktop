// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_database.h"

#include "entities.h"
#include "ayu/libs/sqlite/sqlite_orm.h"

#include "base/unixtime.h"

using namespace sqlite_orm;

auto storage = make_storage("./tdata/ayudata.db",
							make_table("DeletedMessage",
									   make_column("userId", &DeletedMessage::userId),
									   make_column("dialogId", &DeletedMessage::dialogId),
									   make_column("groupedId", &DeletedMessage::groupedId),
									   make_column("peerId", &DeletedMessage::peerId),
									   make_column("fromId", &DeletedMessage::fromId),
									   make_column("topicId", &DeletedMessage::topicId),
									   make_column("messageId", &DeletedMessage::messageId),
									   make_column("date", &DeletedMessage::date),
									   make_column("flags", &DeletedMessage::flags),
									   make_column("editDate", &DeletedMessage::editDate),
									   make_column("views", &DeletedMessage::views),
									   make_column("fwdFlags", &DeletedMessage::fwdFlags),
									   make_column("fwdFromId", &DeletedMessage::fwdFromId),
									   make_column("fwdName", &DeletedMessage::fwdName),
									   make_column("fwdDate", &DeletedMessage::fwdDate),
									   make_column("fwdPostAuthor", &DeletedMessage::fwdPostAuthor),
									   make_column("replyFlags", &DeletedMessage::replyFlags),
									   make_column("replyMessageId", &DeletedMessage::replyMessageId),
									   make_column("replyPeerId", &DeletedMessage::replyPeerId),
									   make_column("replyTopId", &DeletedMessage::replyTopId),
									   make_column("replyForumTopic", &DeletedMessage::replyForumTopic),
									   make_column("entityCreateDate", &DeletedMessage::entityCreateDate),
									   make_column("text", &DeletedMessage::text),
									   make_column("textEntities", &DeletedMessage::textEntities),
									   make_column("mediaPath", &DeletedMessage::mediaPath),
									   make_column("hqThumbPath", &DeletedMessage::hqThumbPath),
									   make_column("documentType", &DeletedMessage::documentType),
									   make_column("documentSerialized", &DeletedMessage::documentSerialized),
									   make_column("thumbsSerialized", &DeletedMessage::thumbsSerialized),
									   make_column("documentAttributesSerialized",
												   &DeletedMessage::documentAttributesSerialized),
									   make_column("mimeType", &DeletedMessage::mimeType)
							),
							make_table("EditedMessage",
									   make_column("userId", &EditedMessage::userId),
									   make_column("dialogId", &EditedMessage::dialogId),
									   make_column("groupedId", &EditedMessage::groupedId),
									   make_column("peerId", &EditedMessage::peerId),
									   make_column("fromId", &EditedMessage::fromId),
									   make_column("topicId", &EditedMessage::topicId),
									   make_column("messageId", &EditedMessage::messageId),
									   make_column("date", &EditedMessage::date),
									   make_column("flags", &EditedMessage::flags),
									   make_column("editDate", &EditedMessage::editDate),
									   make_column("views", &EditedMessage::views),
									   make_column("fwdFlags", &EditedMessage::fwdFlags),
									   make_column("fwdFromId", &EditedMessage::fwdFromId),
									   make_column("fwdName", &EditedMessage::fwdName),
									   make_column("fwdDate", &EditedMessage::fwdDate),
									   make_column("fwdPostAuthor", &EditedMessage::fwdPostAuthor),
									   make_column("replyFlags", &EditedMessage::replyFlags),
									   make_column("replyMessageId", &EditedMessage::replyMessageId),
									   make_column("replyPeerId", &EditedMessage::replyPeerId),
									   make_column("replyTopId", &EditedMessage::replyTopId),
									   make_column("replyForumTopic", &EditedMessage::replyForumTopic),
									   make_column("entityCreateDate", &EditedMessage::entityCreateDate),
									   make_column("text", &EditedMessage::text),
									   make_column("textEntities", &EditedMessage::textEntities),
									   make_column("mediaPath", &EditedMessage::mediaPath),
									   make_column("hqThumbPath", &EditedMessage::hqThumbPath),
									   make_column("documentType", &EditedMessage::documentType),
									   make_column("documentSerialized", &EditedMessage::documentSerialized),
									   make_column("thumbsSerialized", &EditedMessage::thumbsSerialized),
									   make_column("documentAttributesSerialized",
												   &EditedMessage::documentAttributesSerialized),
									   make_column("mimeType", &EditedMessage::mimeType)
							),
							make_table("DeletedDialog",
									   make_column("userId", &DeletedDialog::userId),
									   make_column("dialogId", &DeletedDialog::dialogId),
									   make_column("peerId", &DeletedDialog::peerId),
									   make_column("topMessage", &DeletedDialog::topMessage),
									   make_column("lastMessageDate", &DeletedDialog::lastMessageDate),
									   make_column("flags", &DeletedDialog::flags),
									   make_column("entityCreateDate", &DeletedDialog::entityCreateDate)
							)
);

namespace AyuDatabase
{

void initialize()
{
	// move to `tdata` from legacy version
	if (std::filesystem::exists("ayugram.db")) {
		try {
			std::filesystem::rename("ayugram.db", "./tdata/ayudata.db");
		}
		catch (std::filesystem::filesystem_error &e) {
			LOG(("Failed to move database: %1").arg(e.what()));
		}
	}

	try {
		storage.sync_schema();
	}
	catch (...) {
		auto time = base::unixtime::now();

		LOG(("Failed to sync database schema"));
		LOG(("Moving current database just in case"));

		if (std::filesystem::exists("./tdata/ayudata.db")) {
			std::filesystem::rename("./tdata/ayudata.db", QString("./tdata/ayudata_%1.db").arg(time).toStdString());
		}

		if (std::filesystem::exists("./tdata/ayudata.db-shm")) {
			std::filesystem::rename("./tdata/ayudata.db-shm",
									QString("./tdata/ayudata_%1.db-shm").arg(time).toStdString());
		}

		if (std::filesystem::exists("./tdata/ayudata.db-wal")) {
			std::filesystem::rename("./tdata/ayudata.db-wal",
									QString("./tdata/ayudata_%1.db-wal").arg(time).toStdString());
		}

		storage.sync_schema();
	}

	storage.begin_transaction();
	storage.commit();
}

void addEditedMessage(const EditedMessage &message)
{
	storage.begin_transaction();
	storage.insert(message);
	storage.commit();
}

std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId)
{
	return storage.get_all<EditedMessage>(
		where(
			c(&EditedMessage::userId) == userId and
				c(&EditedMessage::dialogId) == dialogId and
				c(&EditedMessage::messageId) == messageId
		)
	);
}

bool hasRevisions(ID userId, ID dialogId, ID messageId)
{
	return storage.count<EditedMessage>(
		where(
			c(&EditedMessage::userId) == userId and
				c(&EditedMessage::dialogId) == dialogId and
				c(&EditedMessage::messageId) == messageId
		)
	) > 0;
}

}
