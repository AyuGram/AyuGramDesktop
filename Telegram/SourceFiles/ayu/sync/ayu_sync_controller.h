// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#pragma once

#include "models.h"
#include "ayu/libs/json.hpp"
#include "utils/ayu_pipe_wrapper.h"

using json = nlohmann::json;

const std::string AgentFilename =
#ifdef _WIN32
	"AyuSync.Agent.exe";
#else
	"AyuSync.Agent";
#endif

const std::string AgentPath = "./AyuSync/" + AgentFilename;

namespace AyuSync
{
	class ayu_sync_controller
	{
	public:
		void initializeAgent();

		void onSyncForce(SyncForce ev);
		void onSyncBatch(json ev);
		void onSyncRead(SyncRead ev);

		void invokeHandler(json p);

	private:
		void receiver();

		std::unique_ptr<ayu_pipe_wrapper> pipe;
	};

	ayu_sync_controller& getControllerInstance();

	bool isAgentDownloaded();
	bool isAgentRunning();
}
