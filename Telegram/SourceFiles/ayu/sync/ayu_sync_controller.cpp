// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_sync_controller.h"
#include "ayu/libs/process.hpp"
#include "ayu/sync/models.h"
#include "ayu/sync/utils/process_utils.hpp"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_session.h"
#include "history/history.h"


#include <QString>
#include <thread>

#include "data/data_histories.h"

#include "history/history_item.h"
#include "history/view/history_view_element.h"

namespace AyuSync
{

std::optional<ayu_sync_controller> controller = std::nullopt;

bool isAgentDownloaded()
{
	return std::filesystem::exists(AgentPath);
}

bool isAgentRunning()
{
	return isProcessRunning(AgentFilename);
}

void initialize()
{
	if (controller.has_value()) {
		return;
	}

	controller = ayu_sync_controller();
}

ayu_sync_controller &getInstance()
{
	initialize();
	return controller.value();
}

void ayu_sync_controller::initializeAgent()
{
	if (!isAgentDownloaded()) {
		return;
	}

	if (isAgentRunning()) {
		killProcess(AgentFilename);
	}

	auto configPath = std::filesystem::absolute("./tdata/sync_preferences.json");
	auto process = nes::process{AgentPath, {configPath.string(), ""}, nes::process_options::none};
	process.detach();

	std::thread receiverThread(&ayu_sync_controller::receiver, this);
	receiverThread.detach();

	initialized = true;
}

void ayu_sync_controller::syncRead(not_null<History *> history, MsgId untilId)
{
	if (!initialized) {
		return;
	}

	SyncRead ev;
	ev.userId = history->owner().session().userId().bare;

	ev.args.dialogId = getDialogIdFromPeer(history->peer);
	ev.args.untilId = untilId.bare;
	ev.args.unread = history->unreadCount();

	pipe->send(ev);
}

void ayu_sync_controller::receiver()
{
	pipe = std::make_unique<ayu_pipe_wrapper>();
	pipe->connect();

	LOG(("Pipe created"));

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

void ayu_sync_controller::invokeHandler(json p)
{
	LOG(("Invoking handler on %1").arg(p.dump().c_str()));

	auto userId = p["userId"].get<long>();
	auto type = p["type"].get<std::string>();

	LOG(("userId: %1, type: %2").arg(userId).arg(type.c_str()));

	if (!accountExists(userId)) {
		LOG(("Sync for unknown account: %1").arg(userId));
		return;
	}

	if (type == "sync_force") {
		auto ev = p.get<SyncForce>();
		onSyncForce(ev);
	}
	else if (type == "sync_batch") {
		onSyncBatch(p);
	}
	else if (type == "sync_read") {
		auto ev = p.get<SyncRead>();
		onSyncRead(ev);
	}
	else {
		LOG(("Unknown sync type: %1").arg(type.c_str()));
	}
}

void ayu_sync_controller::onSyncForce(SyncForce ev)
{
	auto session = getSession(ev.userId);
	auto histories = session->data().chatsList();

	SyncBatch readsBatchEvent;
	readsBatchEvent.userId = ev.userId;

	for (const auto &row : histories->indexed()->all()) {
		if (const auto history = row->history()) {
			auto dialogId = getDialogIdFromPeer(history->peer);

			SyncRead readEv;
			readEv.userId = ev.userId;

			history->calculateFirstUnreadMessage();
			auto unreadElement = history->firstUnreadMessage();

			if (!unreadElement && history->unreadCount()) {
				LOG(("No unread can be calculated for %1").arg(dialogId));
				continue;
			}

			auto untilId = unreadElement ? unreadElement->data()->id.bare : history->lastMessage()->id.bare;

			readEv.args.dialogId = dialogId;
			readEv.args.untilId = untilId;
			readEv.args.unread = history->unreadCount();

			readsBatchEvent.args.events.emplace_back(readEv);
		}
	}

	pipe->send(readsBatchEvent);

	// send finish event
	SyncForceFinish newEv;
	newEv.userId = ev.userId;

	pipe->send(newEv);
}

void ayu_sync_controller::onSyncBatch(json ev)
{
	for (auto &item : ev["args"]["events"]) {
		invokeHandler(item);
	}
}

void ayu_sync_controller::onSyncRead(SyncRead ev)
{
	dispatchToMainThread([=]
						 {
							 auto session = getSession(ev.userId);
							 auto history = getHistoryFromDialogId(ev.args.dialogId, session);

							 if (history->folderKnown()) {
								 history->inboxRead(ev.args.untilId, ev.args.unread);
							 }
							 else {
								 LOG(("Unknown dialog %1").arg(ev.args.dialogId));
							 }
						 });
}

}
