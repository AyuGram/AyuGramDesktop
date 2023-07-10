// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#pragma once


#include <string>

#define ID long long

// https://github.com/AyuGram/AyuGram4A/blob/rewrite/TMessagesProj/src/main/java/com/radolyn/ayugram/database/entities/AyuMessageBase.java
class AyuMessageBase
{
public:
	ID userId;
	ID dialogId;
	ID groupedId;
	ID peerId;
	ID fromId;
	ID topicId;
	int messageId;
	int date;

	int flags;

	int editDate;
	bool editHide;
	bool out;

	int entityCreateDate;

	std::string text; // plain text
	std::string textEntities; // TL serialized
	std::string mediaPath; // full path
	int documentType; // see DOCUMENT_TYPE_*
	std::string documentSerialized; // for sticker; TL serialized
	std::string thumbsSerialized; // for video/etc.; TL serialized
	std::string documentAttributesSerialized; // for video/voice/etc.; TL serialized
	std::string mimeType;
};

class DeletedMessage : public AyuMessageBase
{
public:
	[[nodiscard]] long long get_user_id() const
	{
		return userId;
	}

	void set_user_id(long long user_id)
	{
		userId = user_id;
	}

	[[nodiscard]] long long get_dialog_id() const
	{
		return dialogId;
	}

	void set_dialog_id(long long dialog_id)
	{
		dialogId = dialog_id;
	}

	[[nodiscard]] long long get_grouped_id() const
	{
		return groupedId;
	}

	void set_grouped_id(long long grouped_id)
	{
		groupedId = grouped_id;
	}

	[[nodiscard]] long long get_peer_id() const
	{
		return peerId;
	}

	void set_peer_id(long long peer_id)
	{
		peerId = peer_id;
	}

	[[nodiscard]] long long get_from_id() const
	{
		return fromId;
	}

	void set_from_id(long long from_id)
	{
		fromId = from_id;
	}

	[[nodiscard]] long long get_topic_id() const
	{
		return topicId;
	}

	void set_topic_id(long long topic_id)
	{
		topicId = topic_id;
	}

	[[nodiscard]] int get_message_id() const
	{
		return messageId;
	}

	void set_message_id(int message_id)
	{
		messageId = message_id;
	}

	[[nodiscard]] int get_date() const
	{
		return date;
	}

	void set_date(int date)
	{
		this->date = date;
	}

	[[nodiscard]] int get_flags() const
	{
		return flags;
	}

	void set_flags(int flags)
	{
		this->flags = flags;
	}

	[[nodiscard]] int get_edit_date() const
	{
		return editDate;
	}

	void set_edit_date(int edit_date)
	{
		editDate = edit_date;
	}

	[[nodiscard]] bool is_edit_hide() const
	{
		return editHide;
	}

	void set_edit_hide(bool edit_hide)
	{
		editHide = edit_hide;
	}

	[[nodiscard]] bool is_out() const
	{
		return out;
	}

	void set_out(bool out)
	{
		this->out = out;
	}

	[[nodiscard]] int get_entity_create_date() const
	{
		return entityCreateDate;
	}

	void set_entity_create_date(int entity_create_date)
	{
		entityCreateDate = entity_create_date;
	}

	[[nodiscard]] std::string get_text() const
	{
		return text;
	}

	void set_text(const std::string& text)
	{
		this->text = text;
	}

	[[nodiscard]] std::string get_text_entities() const
	{
		return textEntities;
	}

	void set_text_entities(const std::string& text_entities)
	{
		textEntities = text_entities;
	}

	[[nodiscard]] std::string get_media_path() const
	{
		return mediaPath;
	}

	void set_media_path(const std::string& media_path)
	{
		mediaPath = media_path;
	}

	[[nodiscard]] int get_document_type() const
	{
		return documentType;
	}

	void set_document_type(int document_type)
	{
		documentType = document_type;
	}

	[[nodiscard]] std::string get_document_serialized() const
	{
		return documentSerialized;
	}

	void set_document_serialized(const std::string& document_serialized)
	{
		documentSerialized = document_serialized;
	}

	[[nodiscard]] std::string get_thumbs_serialized() const
	{
		return thumbsSerialized;
	}

	void set_thumbs_serialized(const std::string& thumbs_serialized)
	{
		thumbsSerialized = thumbs_serialized;
	}

	[[nodiscard]] std::string get_document_attributes_serialized() const
	{
		return documentAttributesSerialized;
	}

	void set_document_attributes_serialized(const std::string& document_attributes_serialized)
	{
		documentAttributesSerialized = document_attributes_serialized;
	}

	[[nodiscard]] std::string get_mime_type() const
	{
		return mimeType;
	}

	void set_mime_type(const std::string& mime_type)
	{
		mimeType = mime_type;
	}
};

class EditedMessage : public AyuMessageBase
{
public:
	[[nodiscard]] long long get_user_id() const
	{
		return userId;
	}

	void set_user_id(long long user_id)
	{
		userId = user_id;
	}

	[[nodiscard]] long long get_dialog_id() const
	{
		return dialogId;
	}

	void set_dialog_id(long long dialog_id)
	{
		dialogId = dialog_id;
	}

	[[nodiscard]] long long get_grouped_id() const
	{
		return groupedId;
	}

	void set_grouped_id(long long grouped_id)
	{
		groupedId = grouped_id;
	}

	[[nodiscard]] long long get_peer_id() const
	{
		return peerId;
	}

	void set_peer_id(long long peer_id)
	{
		peerId = peer_id;
	}

	[[nodiscard]] long long get_from_id() const
	{
		return fromId;
	}

	void set_from_id(long long from_id)
	{
		fromId = from_id;
	}

	[[nodiscard]] long long get_topic_id() const
	{
		return topicId;
	}

	void set_topic_id(long long topic_id)
	{
		topicId = topic_id;
	}

	[[nodiscard]] int get_message_id() const
	{
		return messageId;
	}

	void set_message_id(int message_id)
	{
		messageId = message_id;
	}

	[[nodiscard]] int get_date() const
	{
		return date;
	}

	void set_date(int date)
	{
		this->date = date;
	}

	[[nodiscard]] int get_flags() const
	{
		return flags;
	}

	void set_flags(int flags)
	{
		this->flags = flags;
	}

	[[nodiscard]] int get_edit_date() const
	{
		return editDate;
	}

	void set_edit_date(int edit_date)
	{
		editDate = edit_date;
	}

	[[nodiscard]] bool is_edit_hide() const
	{
		return editHide;
	}

	void set_edit_hide(bool edit_hide)
	{
		editHide = edit_hide;
	}

	[[nodiscard]] bool is_out() const
	{
		return out;
	}

	void set_out(bool out)
	{
		this->out = out;
	}

	[[nodiscard]] int get_entity_create_date() const
	{
		return entityCreateDate;
	}

	void set_entity_create_date(int entity_create_date)
	{
		entityCreateDate = entity_create_date;
	}

	[[nodiscard]] std::string get_text() const
	{
		return text;
	}

	void set_text(const std::string& text)
	{
		this->text = text;
	}

	[[nodiscard]] std::string get_text_entities() const
	{
		return textEntities;
	}

	void set_text_entities(const std::string& text_entities)
	{
		textEntities = text_entities;
	}

	[[nodiscard]] std::string get_media_path() const
	{
		return mediaPath;
	}

	void set_media_path(const std::string& media_path)
	{
		mediaPath = media_path;
	}

	[[nodiscard]] int get_document_type() const
	{
		return documentType;
	}

	void set_document_type(int document_type)
	{
		documentType = document_type;
	}

	[[nodiscard]] std::string get_document_serialized() const
	{
		return documentSerialized;
	}

	void set_document_serialized(const std::string& document_serialized)
	{
		documentSerialized = document_serialized;
	}

	[[nodiscard]] std::string get_thumbs_serialized() const
	{
		return thumbsSerialized;
	}

	void set_thumbs_serialized(const std::string& thumbs_serialized)
	{
		thumbsSerialized = thumbs_serialized;
	}

	[[nodiscard]] std::string get_document_attributes_serialized() const
	{
		return documentAttributesSerialized;
	}

	void set_document_attributes_serialized(const std::string& document_attributes_serialized)
	{
		documentAttributesSerialized = document_attributes_serialized;
	}

	[[nodiscard]] std::string get_mime_type() const
	{
		return mimeType;
	}

	void set_mime_type(const std::string& mime_type)
	{
		mimeType = mime_type;
	}
};
