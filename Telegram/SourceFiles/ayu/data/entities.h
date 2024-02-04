// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include <string>

#define ID long long

template<typename TableName>
class AyuMessageBase
{
public:
	ID fakeId;
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
	int views;
	int fwdFlags;
	ID fwdFromId;
	std::string fwdName;
	int fwdDate;
	std::string fwdPostAuthor;
	int replyFlags;
	int replyMessageId;
	ID replyPeerId;
	int replyTopId;
	bool replyForumTopic;
	std::vector<char> replySerialized;
	int entityCreateDate;
	std::string text;
	std::vector<char> textEntities;
	std::string mediaPath;
	std::string hqThumbPath;
	int documentType;
	std::vector<char> documentSerialized;
	std::vector<char> thumbsSerialized;
	std::vector<char> documentAttributesSerialized;
	std::string mimeType;
};

using DeletedMessage = AyuMessageBase<struct DeletedMessageTag>;

using EditedMessage = AyuMessageBase<struct EditedMessageTag>;

class DeletedDialog
{
public:
	ID fakeId;
	ID userId;
	ID dialogId;
	ID peerId;
	std::unique_ptr<int> folderId; // nullable
	int topMessage;
	int lastMessageDate;
	int flags;
	int entityCreateDate;
};

class RegexFilter
{
public:
	std::vector<char> id;
	std::string text;
	bool enabled;
	bool caseInsensitive;
	std::unique_ptr<ID> dialogId; // nullable
};

class RegexFilterGlobalExclusion
{
public:
	ID fakeId;
	ID dialogId;
	std::vector<char> filterId;
};

class SpyMessageRead
{
public:
	ID fakeId;
	ID userId;
	ID dialogId;
	int messageId;
	int entityCreateDate;
};

class SpyMessageContentsRead
{
public:
	ID fakeId;
	ID userId;
	ID dialogId;
	int messageId;
	int entityCreateDate;
};
