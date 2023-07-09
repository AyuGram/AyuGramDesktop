#pragma once

#include "ayu/libs/json.hpp"

#include <string>
#include <vector>

#define ID long long

class SyncEvent
{
public:
	std::string type = "sync_unspecified";
	ID userId = 0;
};

class SyncBatch : public SyncEvent
{
public:
	std::string type = "sync_batch";
	ID userId;

	class SyncBatchArgs
	{
	public:
		std::vector<SyncEvent> events;
	};

	SyncBatchArgs args;
};

class SyncRead : public SyncEvent
{
public:
	std::string type = "sync_read";
	ID userId;

	class SyncReadArgs
	{
	public:
		ID dialogId;
		int untilId;
		int unread;
	};

	SyncReadArgs args;
};

class SyncForce : public SyncEvent
{
public:
	std::string type = "sync_force";
	ID userId;

	class SyncForceArgs
	{
	public:
		int fromDate;
	};

	SyncForceArgs args;
};

class SyncForceFinish : public SyncEvent
{
public:
	std::string type = "sync_force_finish";
	ID userId;

	class SyncForceFinishArgs
	{
	public:
		short dummy; // required to be JSON serializable
	};

	SyncForceFinishArgs args;
};


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncEvent, type, userId)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncBatch::SyncBatchArgs, events)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncBatch, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncRead::SyncReadArgs, dialogId, untilId, unread)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncRead, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForce::SyncForceArgs, fromDate)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForce, type, userId, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForceFinish::SyncForceFinishArgs, dummy)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SyncForceFinish, type, userId, args)
