// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/data/ayu_database.h"

#include <ranges>

#include "entities.h"
#include "ayu/libs/sqlite/sqlite_orm.h"

#include "base/unixtime.h"

using namespace sqlite_orm;
auto storage = make_storage(
	"./tdata/ayudata.db",
	make_table<DeletedMessage>(
		"DeletedMessage",
		make_column("fakeId", &DeletedMessage::fakeId, primary_key().autoincrement()),
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
		make_column("replySerialized", &DeletedMessage::replySerialized),
		make_column("entityCreateDate", &DeletedMessage::entityCreateDate),
		make_column("text", &DeletedMessage::text),
		make_column("textEntities", &DeletedMessage::textEntities),
		make_column("mediaPath", &DeletedMessage::mediaPath),
		make_column("hqThumbPath", &DeletedMessage::hqThumbPath),
		make_column("documentType", &DeletedMessage::documentType),
		make_column("documentSerialized", &DeletedMessage::documentSerialized),
		make_column("thumbsSerialized", &DeletedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &DeletedMessage::documentAttributesSerialized),
		make_column("mimeType", &DeletedMessage::mimeType)
	),
	make_table<EditedMessage>(
		"EditedMessage",
		make_column("fakeId", &EditedMessage::fakeId, primary_key().autoincrement()),
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
		make_column("replySerialized", &EditedMessage::replySerialized),
		make_column("entityCreateDate", &EditedMessage::entityCreateDate),
		make_column("text", &EditedMessage::text),
		make_column("textEntities", &EditedMessage::textEntities),
		make_column("mediaPath", &EditedMessage::mediaPath),
		make_column("hqThumbPath", &EditedMessage::hqThumbPath),
		make_column("documentType", &EditedMessage::documentType),
		make_column("documentSerialized", &EditedMessage::documentSerialized),
		make_column("thumbsSerialized", &EditedMessage::thumbsSerialized),
		make_column("documentAttributesSerialized", &EditedMessage::documentAttributesSerialized),
		make_column("mimeType", &EditedMessage::mimeType)
	),
	make_table<DeletedDialog>(
		"DeletedDialog",
		make_column("fakeId", &DeletedDialog::fakeId, primary_key().autoincrement()),
		make_column("userId", &DeletedDialog::userId),
		make_column("dialogId", &DeletedDialog::dialogId),
		make_column("peerId", &DeletedDialog::peerId),
		make_column("folderId", &DeletedDialog::folderId),
		make_column("topMessage", &DeletedDialog::topMessage),
		make_column("lastMessageDate", &DeletedDialog::lastMessageDate),
		make_column("flags", &DeletedDialog::flags),
		make_column("entityCreateDate", &DeletedDialog::entityCreateDate)
	),
	make_table<RegexFilter>(
		"RegexFilter",
		make_column("id", &RegexFilter::id),
		make_column("text", &RegexFilter::text),
		make_column("enabled", &RegexFilter::enabled),
		make_column("reversed", &RegexFilter::reversed),
		make_column("caseInsensitive", &RegexFilter::caseInsensitive),
		make_column("dialogId", &RegexFilter::dialogId)
	),
	make_table<RegexFilterGlobalExclusion>(
		"RegexFilterGlobalExclusion",
		make_column("fakeId", &RegexFilterGlobalExclusion::fakeId, primary_key().autoincrement()),
		make_column("dialogId", &RegexFilterGlobalExclusion::dialogId),
		make_column("filterId", &RegexFilterGlobalExclusion::filterId)
	),
	make_table<SpyMessageRead>(
		"SpyMessageRead",
		make_column("fakeId", &SpyMessageRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageRead::userId),
		make_column("dialogId", &SpyMessageRead::dialogId),
		make_column("messageId", &SpyMessageRead::messageId),
		make_column("entityCreateDate", &SpyMessageRead::entityCreateDate)
	),
	make_table<SpyMessageContentsRead>(
		"SpyMessageContentsRead",
		make_column("fakeId", &SpyMessageContentsRead::fakeId, primary_key().autoincrement()),
		make_column("userId", &SpyMessageContentsRead::userId),
		make_column("dialogId", &SpyMessageContentsRead::dialogId),
		make_column("messageId", &SpyMessageContentsRead::messageId),
		make_column("entityCreateDate", &SpyMessageContentsRead::entityCreateDate)
	)
);

namespace AyuDatabase {

void moveCurrentDatabase() {
	const auto time = base::unixtime::now();

	if (QFile::exists("./tdata/ayudata.db")) {
		QFile::rename("./tdata/ayudata.db", QString("./tdata/ayudata_%1.db").arg(time));
	}

	if (QFile::exists("./tdata/ayudata.db-shm")) {
		QFile::rename("./tdata/ayudata.db-shm", QString("./tdata/ayudata_%1.db-shm").arg(time));
	}

	if (QFile::exists("./tdata/ayudata.db-wal")) {
		QFile::rename("./tdata/ayudata.db-wal", QString("./tdata/ayudata_%1.db-wal").arg(time));
	}
}

void initialize() {
	auto movePrevious = false;

	try {
		const auto res = storage.sync_schema_simulate(true);
		for (const auto val : res | std::views::values) {
			if (val == sync_schema_result::dropped_and_recreated) {
				movePrevious = true;
				break;
			}
		}
	} catch (...) {
		LOG(("Exception during sync simulation; possibly corrupted database"));
		movePrevious = true;
	}

	if (movePrevious) {
		moveCurrentDatabase();
	}

	try {
		storage.sync_schema(true);
	} catch (...) {
		LOG(("Failed to sync database schema"));
		LOG(("Moving current database just in case"));

		moveCurrentDatabase();

		storage.sync_schema();
	}

	storage.begin_transaction();
	storage.commit();
}

void addEditedMessage(const EditedMessage &message) {
	try {
		storage.begin_transaction();
		storage.insert(message);
		storage.commit();
	} catch (std::exception &ex) {
		LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
	}
}

std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId, ID minId, ID maxId, int totalLimit) {
	return storage.get_all<EditedMessage>(
		where(
			column<EditedMessage>(&EditedMessage::userId) == userId and
			column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
			column<EditedMessage>(&EditedMessage::messageId) == messageId and
			(column<EditedMessage>(&EditedMessage::fakeId) > minId or minId == 0) and
			(column<EditedMessage>(&EditedMessage::fakeId) < maxId or maxId == 0)
		),
		order_by(column<EditedMessage>(&EditedMessage::fakeId)).desc(),
		limit(totalLimit)
	);
}

bool hasRevisions(ID userId, ID dialogId, ID messageId) {
	try {
		return storage.count<EditedMessage>(
			where(
				column<EditedMessage>(&EditedMessage::userId) == userId and
				column<EditedMessage>(&EditedMessage::dialogId) == dialogId and
				column<EditedMessage>(&EditedMessage::messageId) == messageId
			)
		) > 0;
	} catch (std::exception &ex) {
		LOG(("Failed to check if message has revisions: %1").arg(ex.what()));
		return false;
	}
}

void addDeletedMessage(const DeletedMessage &message) {
	try {
		storage.begin_transaction();
		storage.insert(message);
		storage.commit();
	} catch (std::exception &ex) {
		LOG(("Failed to save edited message for some reason: %1").arg(ex.what()));
	}
}

std::vector<DeletedMessage> getDeletedMessages(ID userId, ID dialogId, ID topicId, ID minId, ID maxId, int totalLimit) {
	return storage.get_all<DeletedMessage>(
		where(
			column<DeletedMessage>(&DeletedMessage::userId) == userId and
			column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
			(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) > minId or minId == 0) and
			(column<DeletedMessage>(&DeletedMessage::messageId) < maxId or maxId == 0)
		),
		order_by(column<DeletedMessage>(&DeletedMessage::messageId)).desc(),
		limit(totalLimit)
	);
}

bool hasDeletedMessages(ID userId, ID dialogId, ID topicId) {
	try {
		return storage.count<DeletedMessage>(
			where(
				column<DeletedMessage>(&DeletedMessage::userId) == userId and
				column<DeletedMessage>(&DeletedMessage::dialogId) == dialogId and
				(column<DeletedMessage>(&DeletedMessage::topicId) == topicId or topicId == 0)
			)
		) > 0;
	} catch (std::exception &ex) {
		LOG(("Failed to check if dialog has deleted message: %1").arg(ex.what()));
		return false;
	}
}

}
