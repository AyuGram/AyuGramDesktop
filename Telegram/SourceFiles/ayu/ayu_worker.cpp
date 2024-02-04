// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_worker.h"

#include "apiwrap.h"
#include "ayu_settings.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "data/data_user.h"
#include "database/entities.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"

namespace AyuWorker
{
std::unordered_map<ID, bool> state;

void markAsOnline(not_null<Main::Session *> session)
{
	state[session->userId().bare] = true;
}

void lateInit()
{
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			const auto id = session->userId().bare;
			state[id] = true;
		}
	}
}

void runOnce()
{
	if (!Core::App().domain().started()) {
		return;
	}

	if (state.empty()) {
		lateInit();
	}

	const auto settings = &AyuSettings::getInstance();
	if (!settings->sendOfflinePacketAfterOnline) {
		return;
	}

	const auto t = base::unixtime::now();
	const auto invalidateAll = cOtherOnline() >= t;

	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			const auto id = session->userId().bare;
			if (!state.contains(id)) {
				state[id] = true; // newly added account, I suppose
			}

			if (invalidateAll || state[id] || session->user()->lastseen().isOnline(t)) {
				session->api().request(MTPaccount_UpdateStatus(
					MTP_bool(true)
				)).send();
				state[id] = false;

				LOG(("[AyuGram] Sent offline for account with uid %1, invalidate %2").arg(id).arg(invalidateAll));
			}
		}
	}
}

[[noreturn]] void loop()
{
	while (true) {
		runOnce();
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}
}

void initialize()
{
	std::thread t(loop);
	t.detach();
}
}
