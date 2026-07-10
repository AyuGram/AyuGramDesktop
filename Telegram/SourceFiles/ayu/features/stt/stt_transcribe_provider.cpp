// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/stt/stt_transcribe_provider.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/stt/stt_manager.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"

#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <utility>
#include <rpl/rpl.h>

namespace Ayu::STT {
namespace {

constexpr crl::time kFileDownloadTimeoutMs = 60000;

} // namespace

bool ShouldTranscribeLocally(const not_null<HistoryItem*> item) {
	if (!AyuSettings::getInstance().sttEnabled()) {
		return false;
	}
	const auto doc = item->media() ? item->media()->document() : nullptr;
	return doc && (doc->isVoiceMessage() || doc->isVideoMessage());
}

void RequestLocalTranscribe(
		const not_null<HistoryItem*> item,
		const std::function<void(QString)>& done) {
	const auto session = &item->history()->session();
	const auto id = item->fullId();
	const auto doc = item->media()->document();

	const auto weak = base::make_weak(session);
	const auto runEngine = [=](const QString &filePath, bool isTmp) {
		STTManager::instance().transcribe(filePath, [=](QString text) {
			if (isTmp) {
				QFile::remove(filePath);
			}
			if (weak) {
				done(std::move(text));
			}
		});
	};

	if (const auto ready = doc->filepath(true); !ready.isEmpty()) {
		runEngine(ready, false);
		return;
	}

	// Not cached yet: subscribe to the downloader signal instead of polling.
	const auto tmpPath = QStandardPaths::writableLocation(
		QStandardPaths::TempLocation)
		+ u"/ayugram_stt_%1.oga"_q.arg(id.msg.bare);
	doc->save(id, tmpPath);

	struct DownloadWait {
		rpl::lifetime lifetime;
		base::Timer timeout;
	};
	const auto state = std::make_shared<DownloadWait>();

	const auto finish = [=](const QString &path, bool isTmp) {
		state->timeout.cancel();
		state->lifetime.destroy();
		runEngine(path, isTmp);
	};

	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		if (const auto ready = doc->filepath(true); !ready.isEmpty()) {
			finish(ready, false);
		} else if (QFile::exists(tmpPath)) {
			finish(tmpPath, true);
		}
	}, state->lifetime);

	state->timeout.setCallback([=] {
		state->lifetime.destroy();
		if (weak) {
			done(QString());
		}
	});
	state->timeout.callOnce(kFileDownloadTimeoutMs);
}

} // namespace Ayu::STT
