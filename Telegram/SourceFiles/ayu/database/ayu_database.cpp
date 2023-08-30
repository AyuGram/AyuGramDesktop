// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "ayu_database.h"

#include "entities.h"
#include "ayu/libs/sqlite/sqlite_orm.h"
#include "ayu/utils/telegram_helpers.h"

using namespace sqlite_orm;
auto storage = make_storage(
	"ayugram.db",
	make_table(
		"editedmessage",
		make_column("userId", &EditedMessage::get_user_id, &EditedMessage::set_user_id),
		make_column("dialogId", &EditedMessage::get_dialog_id, &EditedMessage::set_dialog_id),
		make_column("groupedId", &EditedMessage::get_grouped_id, &EditedMessage::set_grouped_id),
		make_column("peerId", &EditedMessage::get_peer_id, &EditedMessage::set_peer_id),
		make_column("fromId", &EditedMessage::get_from_id, &EditedMessage::set_from_id),
		make_column("topicId", &EditedMessage::get_topic_id, &EditedMessage::set_topic_id),
		make_column("messageId", &EditedMessage::get_message_id, &EditedMessage::set_message_id),
		make_column("date", &EditedMessage::get_date, &EditedMessage::set_date),
		make_column("flags", &EditedMessage::get_flags, &EditedMessage::set_flags),
		make_column("editDate", &EditedMessage::get_edit_date, &EditedMessage::set_edit_date),
		make_column("editHide", &EditedMessage::is_edit_hide, &EditedMessage::set_edit_hide),
		make_column("out", &EditedMessage::is_out, &EditedMessage::set_out),
		make_column("entityCreateDate", &EditedMessage::get_entity_create_date, &EditedMessage::set_entity_create_date),
		make_column("text", &EditedMessage::get_text, &EditedMessage::set_text),
		make_column("textEntities", &EditedMessage::get_text_entities, &EditedMessage::set_text_entities),
		make_column("mediaPath", &EditedMessage::get_media_path, &EditedMessage::set_media_path),
		make_column("documentType", &EditedMessage::get_document_type, &EditedMessage::set_document_type),
		make_column("documentSerialized", &EditedMessage::get_document_serialized,
		            &EditedMessage::set_document_serialized),
		make_column("thumbsSerialized", &EditedMessage::get_thumbs_serialized, &EditedMessage::set_thumbs_serialized),
		make_column("documentAttributesSerialized", &EditedMessage::get_document_attributes_serialized,
		            &EditedMessage::set_document_attributes_serialized),
		make_column("mimeType", &EditedMessage::get_mime_type, &EditedMessage::set_mime_type)
	),
	make_table(
		"deletedmessage",
		make_column("userId", &DeletedMessage::get_user_id, &DeletedMessage::set_user_id),
		make_column("dialogId", &DeletedMessage::get_dialog_id, &DeletedMessage::set_dialog_id),
		make_column("groupedId", &DeletedMessage::get_grouped_id, &DeletedMessage::set_grouped_id),
		make_column("peerId", &DeletedMessage::get_peer_id, &DeletedMessage::set_peer_id),
		make_column("fromId", &DeletedMessage::get_from_id, &DeletedMessage::set_from_id),
		make_column("topicId", &DeletedMessage::get_topic_id, &DeletedMessage::set_topic_id),
		make_column("messageId", &DeletedMessage::get_message_id, &DeletedMessage::set_message_id),
		make_column("date", &DeletedMessage::get_date, &DeletedMessage::set_date),
		make_column("flags", &DeletedMessage::get_flags, &DeletedMessage::set_flags),
		make_column("editDate", &DeletedMessage::get_edit_date, &DeletedMessage::set_edit_date),
		make_column("editHide", &DeletedMessage::is_edit_hide, &DeletedMessage::set_edit_hide),
		make_column("out", &DeletedMessage::is_out, &DeletedMessage::set_out),
		make_column("entityCreateDate", &DeletedMessage::get_entity_create_date,
		            &DeletedMessage::set_entity_create_date),
		make_column("text", &DeletedMessage::get_text, &DeletedMessage::set_text),
		make_column("textEntities", &DeletedMessage::get_text_entities, &DeletedMessage::set_text_entities),
		make_column("mediaPath", &DeletedMessage::get_media_path, &DeletedMessage::set_media_path),
		make_column("documentType", &DeletedMessage::get_document_type, &DeletedMessage::set_document_type),
		make_column("documentSerialized", &DeletedMessage::get_document_serialized,
		            &DeletedMessage::set_document_serialized),
		make_column("thumbsSerialized", &DeletedMessage::get_thumbs_serialized, &DeletedMessage::set_thumbs_serialized),
		make_column("documentAttributesSerialized", &DeletedMessage::get_document_attributes_serialized,
		            &DeletedMessage::set_document_attributes_serialized),
		make_column("mimeType", &DeletedMessage::get_mime_type, &DeletedMessage::set_mime_type)
	)
);

namespace AyuDatabase
{
	void initialize()
	{
		storage.sync_schema();

		storage.begin_transaction();
		storage.commit();
	}

	void addEditedMessage(const EditedMessage& message)
	{
		storage.begin_transaction();
		storage.insert(message);
		storage.commit();
	}

	std::vector<EditedMessage> getEditedMessages(ID userId, ID dialogId, ID messageId)
	{
		return storage.get_all<EditedMessage>(
			where(
				c(&EditedMessage::get_user_id) == userId and
				c(&EditedMessage::get_dialog_id) == dialogId and
				c(&EditedMessage::get_message_id) == messageId
			)
		);
	}

	bool hasRevisions(ID userId, ID dialogId, ID messageId)
	{
		return storage.count<EditedMessage>(
			where(
				c(&EditedMessage::get_user_id) == userId and
				c(&EditedMessage::get_dialog_id) == dialogId and
				c(&EditedMessage::get_message_id) == messageId
			)
		) > 0;
	}
}
