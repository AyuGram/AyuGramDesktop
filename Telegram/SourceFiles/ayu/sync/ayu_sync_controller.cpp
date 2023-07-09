// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "ayu_sync_controller.h"
#include "ayu/libs/process.hpp"
#include "ayu/sync/utils/process_utils.hpp"
#include "ayu/sync/models.h"
#include "data/data_session.h"
#include "history/history.h"
#include "ayu/sync/utils/telegram_helpers.h"


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
            auto process = nes::process{AgentPath, {configPath.string(), ""}, nes::process_options::none};
            process.detach();
        }

        std::thread receiverThread(&ayu_sync_controller::receiver, this);
        receiverThread.detach();
    }

    void ayu_sync_controller::receiver() {
        pipe = std::make_unique<ayu_pipe_wrapper>();

        while (true) {
            auto p = pipe->receive();
            if (p == std::nullopt) {
                continue;
            }

            std::string s = p->dump();
            LOG(("[AyuSync] Received message: %1").arg(QString::fromStdString(s)));

            invokeHandler(p.value());
        }
    }

    void ayu_sync_controller::invokeHandler(json p) {
        LOG(("Invoking handler on %1").arg(p.dump().c_str()));

        auto userId = p["userId"].get<long>();
        auto type = p["type"].get<std::string>();

        LOG(("userId: %1, type: %2").arg(userId).arg(type.c_str()));

        // todo: check if account exists

        if (type == "sync_force") {
            auto ev = p.template get<SyncForce>();
            onSyncForce(ev);
        } else if (type == "sync_batch") {
            onSyncBatch(p);
        } else if (type == "sync_read") {
            auto ev = p.template get<SyncRead>();
            onSyncRead(ev);
        } else {
            LOG(("Unknown sync type: %1").arg(type.c_str()));
        }
    }

    void ayu_sync_controller::onSyncForce(SyncForce ev) {

    }

    void ayu_sync_controller::onSyncBatch(json ev) {

    }

    void ayu_sync_controller::onSyncRead(SyncRead ev) {
        auto session = getSession(ev.userId);

        auto peer = PeerId(abs(ev.args.dialogId));

        auto history = session->data().history(peer);
        if (history) {
            history->inboxRead(ev.args.untilId, ev.args.unread);
        }
    }
}