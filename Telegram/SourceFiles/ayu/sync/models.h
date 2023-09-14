// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include <string>
#include <vector>

#include "ayu/libs/json.hpp"
#include "ayu/database/entities.h"

using json = nlohmann::json;

class SyncEvent
{
public:
	std::string type = "sync_unspecified";
	ID userId = 0;
};

class SyncBatch : public SyncEvent
{
public:
	explicit SyncBatch()
	{
		type = "sync_batch";
	}

	class SyncBatchArgs
	{
	public:
		std::vector<json> events;
	};

	SyncBatchArgs args{};
};

class SyncForce : public SyncEvent
{
public:
	explicit SyncForce()
	{
		type = "sync_force";
	}

	class SyncForceArgs
	{
	public:
		int fromDate;
	};

	SyncForceArgs args{};
};

class SyncForceFinish : public SyncEvent
{
public:
	explicit SyncForceFinish()
	{
		type = "sync_force_finish";
	}

	class SyncForceFinishArgs
	{
	public:
		short dummy; // required to be JSON serializable
	};

	SyncForceFinishArgs args{};
};

class SyncRead : public SyncEvent
{
public:
	explicit SyncRead()
	{
		type = "sync_read";
	}

	class SyncReadArgs
	{
	public:
		ID dialogId;
		int untilId;
		int unread;
	};

	SyncReadArgs args{};
};

class SyncDeletedMessage : public SyncEvent
{
public:
	explicit SyncDeletedMessage()
	{
		type = "sync_deleted_message";
	}

	class SyncDeletedMessageArgs
	{
	public:
		json message;
	};

	SyncDeletedMessageArgs args{};
};

class SyncEditedMessage : public SyncEvent
{
public:
	explicit SyncEditedMessage()
	{
		type = "sync_edited_message";
	}

	class SyncEditedMessageArgs
	{
	public:
		json message;
	};

	SyncEditedMessageArgs args{};
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncEvent, type, userId)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncBatch::SyncBatchArgs, events)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncBatch, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForce::SyncForceArgs, fromDate)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForce, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForceFinish::SyncForceFinishArgs, dummy)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForceFinish, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncRead::SyncReadArgs, dialogId, untilId, unread)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncRead, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncDeletedMessage::SyncDeletedMessageArgs, message)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncDeletedMessage, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncEditedMessage::SyncEditedMessageArgs, message)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncEditedMessage, type, userId, args)
