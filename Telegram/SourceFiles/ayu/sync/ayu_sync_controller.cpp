// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "ayu_sync_controller.h"
#include "ayu/libs/process.hpp"
#include "ayu/sync/utils/process_utils.hpp"

#include <QString>
#include <thread>

namespace AyuSync {
    std::optional<ayu_sync_controller> controller = std::nullopt;

    bool isAgentDownloaded() {
        return std::filesystem::exists(AgentPath);
    }

    bool isAgentRunning() {
        return is_process_running(AgentFilename);
    }

    void initialize() {
        if (controller.has_value()) {
            return;
        }

        controller = ayu_sync_controller();
    }

    ayu_sync_controller &getControllerInstance() {
        initialize();
        return controller.value();
    }

    void ayu_sync_controller::initializeAgent() {
        if (!isAgentDownloaded()) {
            return;
        }

        if (!isAgentRunning()) {
            auto configPath = std::filesystem::absolute("./tdata/sync_preferences.json");
            auto process = nes::process{AgentPath, {configPath.string()}};
            process.detach();
        }

        pipe = std::make_unique<ayu_pipe_wrapper>();

        std::thread receiverThread(&ayu_sync_controller::receiver, this);
    }

    void ayu_sync_controller::receiver() {
        while (true) {
            auto p = pipe->receive();
            invokeHandler(p);
        }
    }

    void ayu_sync_controller::invokeHandler(json p) {
        DEBUG_LOG(("Invoking handler on %1").arg(p.dump().c_str()));

        auto userId = p["userId"].get<long>();
        auto type = p["type"].get<std::string>();

        DEBUG_LOG(("userId: %1, type: %1").arg(userId).arg(type.c_str()));
    }
}