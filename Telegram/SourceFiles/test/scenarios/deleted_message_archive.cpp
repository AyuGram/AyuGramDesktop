/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#ifdef _DEBUG

#include "test/test_runner.h"
#include "test/test_log.h"
#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "ayu/data/messages_storage.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_inner_widget.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "mainwindow.h"
#include "mtproto/mtproto_proxy_data.h"
#include "window/window_session_controller.h"

#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <malloc/malloc.h>
#endif

namespace Test {
namespace {

constexpr auto kSelf = ID(900000000001LL);
constexpr auto kPeer = ID(900000000002LL);
constexpr auto kMediaPeer = ID(900000000003LL);
constexpr auto kBatchSize = 1000;
constexpr auto kBatches = 6;
constexpr auto kTotal = kBatchSize * kBatches;
constexpr auto kLargeFileSize = int64(5) * 1024 * 1024 * 1024;

struct State {
	base::weak_ptr<Main::Session> session;
	History *history = nullptr;
	History *mediaHistory = nullptr;
	bool flushed = false;
};

QString Footprint() {
#ifdef Q_OS_MAC
	auto info = task_vm_info_data_t();
	auto count = mach_msg_type_number_t(TASK_VM_INFO_COUNT);
	const auto result = task_info(
		mach_task_self(),
		TASK_VM_INFO,
		reinterpret_cast<task_info_t>(&info),
		&count);
	auto heap = malloc_statistics_t();
	malloc_zone_statistics(nullptr, &heap);
	return u"%1 heapInUse=%2 heapReserved=%3 heapBlocks=%4"_q
		.arg((result == KERN_SUCCESS) ? info.phys_footprint : 0)
		.arg(heap.size_in_use)
		.arg(heap.size_allocated)
		.arg(heap.blocks_in_use);
#else
	return u"unavailable"_q;
#endif
}

MTPUser FixtureUser() {
	using Flag = MTPDuser::Flag;
	return MTP_user(
		MTP_flags(Flag::f_self | Flag::f_first_name),
		MTP_long(kSelf),
		MTPlong(),
		MTP_string("Archive test"),
		MTPstring(),
		MTPstring(),
		MTPstring(),
		MTPUserProfilePhoto(),
		MTPUserStatus(),
		MTPint(),
		MTPVector<MTPRestrictionReason>(),
		MTPstring(),
		MTPstring(),
		MTPEmojiStatus(),
		MTPVector<MTPUsername>(),
		MTPRecentStory(),
		MTPPeerColor(),
		MTPPeerColor(),
		MTPint(),
		MTPlong(),
		MTPlong(),
		MTPlong());
}

HistoryItemCommonFields Fields(not_null<Main::Session*> session, int id) {
	return {
		.id = id,
		.flags = MessageFlag::HasFromId | MessageFlag::Outgoing,
		.from = session->userPeerId(),
		.date = 1700000000 + id,
	};
}

QString MessageText(int id) {
	return (id == kTotal)
		? u"Archive %1: deleted message restored in the chat."_q.arg(id)
		: u"Archive %1 "_q.arg(id) + QString(4096, QChar('x'));
}

void QueueCleanup(const std::shared_ptr<State> &state) {
	state->flushed = false;
	state->history->unloadDeletedMessagesLater();
	state->mediaHistory->unloadDeletedMessagesLater();
	crl::on_main(state->session, [=] {
		state->flushed = true;
	});
}

void CheckAbsent(const std::shared_ptr<State> &state, int from, int till) {
	auto remaining = 0;
	for (auto id = from; id <= till; ++id) {
		remaining += (state->history->owner().message(state->history->peer, id) != nullptr);
	}
	Check(remaining == 0, u"archived objects released"_q,
		u"from=%1 till=%2 remaining=%3"_q.arg(from).arg(till).arg(remaining));
}

void CreateMediaFixtures(const std::shared_ptr<State> &state) {
	const auto session = state->session.get();
	const auto photo = session->data().processPhoto(MTP_photo(
		MTP_flags(0),
		MTP_long(81001),
		MTP_long(123456),
		MTP_bytes("fixture-photo"),
		MTP_int(1700000000),
		MTP_vector<MTPPhotoSize>({ MTP_photoSize(
			MTP_string("x"), MTP_int(96), MTP_int(64), MTP_int(2048)) }),
		MTPVector<MTPVideoSize>(),
		MTP_int(2)));
	const auto photoItem = state->mediaHistory->makeMessage(
		Fields(session, 11), photo, TextWithEntities());
	processMessageDelete(photoItem);
	Check(photoItem->isDeletedSaved(), u"captionless photo persisted"_q);
	for (auto id = 12; id <= 14; ++id) {
		const auto filename = (id == 12) ? "archive-fixture.ogg"
			: (id == 13) ? "archive-fixture.mp3" : "archive-fixture.bin";
		const auto mime = (id == 12) ? "audio/ogg"
			: (id == 13) ? "audio/mpeg" : "application/octet-stream";
		auto attributes = QVector<MTPDocumentAttribute>{
			MTP_documentAttributeFilename(MTP_string(filename)),
		};
		if (id != 14) {
			using Flag = MTPDdocumentAttributeAudio::Flag;
			attributes.push_back(MTP_documentAttributeAudio(
				MTP_flags(Flag::f_title | Flag::f_performer | Flag::f_waveform
					| ((id == 12) ? Flag::f_voice : Flag())),
				MTP_int(91),
				MTP_string("Fixture title"),
				MTP_string("Fixture performer"),
				MTP_bytes(QByteArray::fromHex("1f00"))));
		}
		const auto document = session->data().processDocument(MTP_document(
			MTP_flags(0),
			MTP_long(81000 + id),
			MTP_long(987654),
			MTP_bytes("fixture-document"),
			MTP_int(1700000000),
			MTP_string(mime),
			MTP_long(id == 14 ? kLargeFileSize : 4096),
			MTPVector<MTPPhotoSize>(),
			MTPVector<MTPVideoSize>(),
			MTP_int(2),
			MTP_vector<MTPDocumentAttribute>(attributes)));
		Check((id == 12) ? document->isVoiceMessage()
			: (id == 13) ? document->isSong() : document->type == FileDocument,
			u"media fixture has the expected original type"_q,
			u"id=%1 type=%2"_q.arg(id).arg(document->type));
		const auto item = state->mediaHistory->makeMessage(
			Fields(session, id), document, TextWithEntities());
		processMessageDelete(item);
		Check(item->isDeletedSaved(), u"captionless document persisted"_q,
			u"id=%1 type=%2 size=%3"_q.arg(id).arg(document->type).arg(document->size));
	}
}

void CheckMedia(const std::shared_ptr<State> &state) {
	const auto restored = AyuMessages::restoreDeletedMessages(state->mediaHistory, {}, false);
	Check(restored.size() == 4, u"all media messages restored"_q,
		u"count=%1"_q.arg(restored.size()));
	for (const auto item : restored) {
		const auto media = item->media();
		Check(item->isDeleted() && item->isDeletedSaved() && media,
			u"restored media has durable deleted state"_q,
			u"id=%1"_q.arg(item->id.bare));
		if (!media) {
			continue;
		}
		if (item->id == MsgId(11)) {
			const auto photo = media->photo();
			Check(photo && photo->id == 81001 && photo->width() == 96
				&& photo->height() == 64 && photo->getDC() == 2,
				u"photo metadata survives restart"_q);
		} else {
			const auto document = media->document();
			Check(document != nullptr, u"document restored"_q,
				u"id=%1"_q.arg(item->id.bare));
			if (!document) {
				continue;
			}
			if (item->id == MsgId(12)) {
				Check(document->isVoiceMessage() && document->duration() == 91000,
					u"voice type and duration survive restart"_q);
			} else if (item->id == MsgId(13)) {
				const auto song = document->song();
				Check(song && song->title == u"Fixture title"_q
					&& song->performer == u"Fixture performer"_q
					&& document->duration() == 91000,
					u"song metadata survives restart"_q);
			} else {
				Check(document->size == kLargeFileSize,
					u"file size above 4 GiB survives restart"_q,
					u"size=%1"_q.arg(document->size));
			}
		}
	}
}

void CheckHistory(const std::shared_ptr<State> &state) {
	auto ids = base::flat_set<MsgId>();
	auto pages = 0;
	auto largestPage = 0;
	auto previousCount = size_t(0);
	do {
		state->history->addOlderSlice({});
		auto pageCount = 0;
		for (const auto &block : state->history->blocks) {
			for (const auto &view : block->messages) {
				const auto item = view->data();
				if (ids.emplace(item->id).second) {
					++pageCount;
					if (item->originalText().text != MessageText(int(item->id.bare))
						|| !item->isDeleted() || !item->isDeletedSaved()) {
						Fail(u"restored history content mismatch"_q,
							u"id=%1"_q.arg(item->id.bare));
					}
				}
			}
		}
		largestPage = std::max(largestPage, pageCount);
		++pages;
		if (ids.size() == previousCount && !state->history->loadedAtTop()) {
			Fail(u"archive pagination made no progress"_q);
			break;
		}
		previousCount = ids.size();
	} while (!state->history->loadedAtTop() && pages < 100);
	Check(ids.size() == kTotal && state->history->loadedAtTop(),
		u"complete history restored into real chat blocks"_q,
		u"count=%1 pages=%2 largestPage=%3"_q.arg(ids.size()).arg(pages).arg(largestPage));
	Check(largestPage <= 100, u"archive page size is bounded"_q,
		u"largestPage=%1"_q.arg(largestPage));
}

} // namespace

void SetupScenario(not_null<Runner*> runner) {
	const auto state = std::make_shared<State>();
	const auto reading = qEnvironmentVariable("AYU_ARCHIVE_TEST_PHASE") == u"read"_q;
	runner->waitEvent(u"launch_finished"_q);
	runner->add({
		.name = u"create isolated unauthenticated fixture session"_q,
		.run = [=] {
			auto &account = Core::App().domain().active();
			if (account.sessionExists() && account.session().userId().bare != kSelf) {
				Fail(u"refusing to use a real account"_q);
				return;
			}
			Core::App().setCurrentProxy(MTP::ProxyData{
				.type = MTP::ProxyData::Type::Socks5,
				.host = u"127.0.0.1"_q,
				.port = 9,
			}, MTP::ProxyData::Settings::Enabled);
			if (!account.sessionExists()) {
				account.createSession(FixtureUser());
			}
			const auto session = &account.session();
			state->session = base::make_weak(session);
			for (const auto id : { kPeer, kMediaPeer }) {
				const auto user = session->data().user(UserId(id));
				user->setName(u"Archive fixture"_q, QString(), QString(), QString());
				user->setLoadedStatus(PeerData::LoadedStatus::Normal);
			}
			state->history = session->data().history(peerFromUser(UserId(kPeer)));
			state->mediaHistory = session->data().history(peerFromUser(UserId(kMediaPeer)));
			Check(AyuSettings::getInstance().saveDeletedMessages(), u"archive setting enabled"_q);
			Note(u"initialFootprint=%1"_q.arg(Footprint()));
		},
	});
	if (!reading) {
		for (auto batch = 0; batch < kBatches; ++batch) {
			runner->add({
				.name = u"persist and unload archive batch %1"_q.arg(batch),
				.run = [=] {
					if (!state->session || !state->history) {
						Fail(u"fixture session unavailable"_q);
						state->flushed = true;
						return;
					}
					for (auto offset = 1; offset <= kBatchSize; ++offset) {
						const auto id = batch * kBatchSize + offset;
						const auto item = state->history->makeMessage(
							Fields(state->session.get(), id),
							TextWithEntities{ MessageText(id) },
							MTP_messageMediaEmpty());
						processMessageDelete(item);
						if (!item->isDeletedSaved()) {
							Fail(u"archive write failed"_q, u"id=%1"_q.arg(id));
						}
					}
					Note(u"batch=%1 liveFootprint=%2"_q.arg(batch).arg(Footprint()));
					QueueCleanup(state);
				},
				.until = [=] { return state->flushed; },
				.then = [=] {
					if (state->session && state->history) {
						CheckAbsent(state, batch * kBatchSize + 1, (batch + 1) * kBatchSize);
						Note(u"batch=%1 unloadedFootprint=%2"_q.arg(batch).arg(Footprint()));
					}
				},
			});
		}
		runner->add({
			.name = u"persist captionless media fixtures"_q,
			.run = [=] {
				CreateMediaFixtures(state);
				QueueCleanup(state);
			},
			.until = [=] { return state->flushed; },
		});
	}
	runner->add({
		.name = reading ? u"restore after process restart"_q : u"restore after memory eviction"_q,
		.run = [=] {
			if (!state->session || !state->history) {
				Fail(u"fixture session unavailable for restore"_q);
				return;
			}
			CheckHistory(state);
			CheckMedia(state);
			Note(u"restoredFootprint=%1"_q.arg(Footprint()));
		},
	});
	runner->add({
		.name = u"show archived messages in the chat"_q,
		.run = [=] {
			if (!state->session || !state->history) {
				Fail(u"fixture unavailable for chat rendering"_q);
				return;
			}
			state->history->addNewerSlice({});
			if (const auto window = state->session->tryResolveWindow()) {
				window->showPeerHistory(
					state->history,
					Window::SectionShow::Way::ClearStack,
					ShowAtTheEndMsgId);
			} else {
				Fail(u"fixture window unavailable"_q);
			}
		},
	});
	runner->captureWidget(
		reading ? u"archive_after_restart"_q : u"archive_before_restart"_q,
		[=]() -> QWidget* {
			if (state->session) {
				if (const auto window = state->session->tryResolveWindow()) {
					return window->widget();
				}
			}
			return nullptr;
		},
		[=](QWidget*) {
			return state->history && state->history->delegateMixin()->hasCurrent();
		});
	runner->add({
		.name = u"keep the currently displayed archive loaded"_q,
		.run = [=] { QueueCleanup(state); },
		.until = [=] { return state->flushed; },
		.then = [=] {
			Check(state->history->owner().message(state->history->peer, kTotal) != nullptr,
				u"active chat survives cleanup"_q);
		},
	});
	runner->add({
		.name = u"unload reconstructed views and message data"_q,
		.run = [=] {
			if (const auto window = state->session->tryResolveWindow()) {
				window->clearSectionStack(anim::type::instant);
			}
			QueueCleanup(state);
		},
		.until = [=] { return state->flushed; },
		.then = [=] {
			CheckAbsent(state, 1, kTotal);
			Check(state->history->blocks.empty(), u"restored message views released"_q);
			Note(u"finalFootprint=%1"_q.arg(Footprint()));
		},
	});
	if (reading) {
		for (auto cycle = 0; cycle < 4; ++cycle) {
			runner->add({
				.name = u"reopen archived history %1"_q.arg(cycle),
				.run = [=] {
					CheckHistory(state);
					state->history->addNewerSlice({});
					if (const auto window = state->session->tryResolveWindow()) {
						window->showPeerHistory(
							state->history,
							Window::SectionShow::Way::ClearStack,
							ShowAtTheEndMsgId);
					}
				},
			});
			runner->captureWidget(
				u"archive_reopened_%1"_q.arg(cycle),
				[=]() -> QWidget* {
					if (const auto window = state->session->tryResolveWindow()) {
						return window->widget();
					}
					return nullptr;
				},
				[=](QWidget*) { return state->history->delegateMixin()->hasCurrent(); });
			runner->add({
				.name = u"unload reopened history %1"_q.arg(cycle),
				.run = [=] {
					if (const auto window = state->session->tryResolveWindow()) {
						window->clearSectionStack(anim::type::instant);
					}
					QueueCleanup(state);
				},
				.until = [=] { return state->flushed; },
				.then = [=] {
					CheckAbsent(state, 1, kTotal);
					Check(state->history->blocks.empty(), u"reopened message views released"_q);
					Note(u"cycle=%1 unloadedFootprint=%2"_q.arg(cycle).arg(Footprint()));
				},
			});
		}
	}
	runner->add({
		.name = u"release scenario references"_q,
		.run = [=] {
			state->history = nullptr;
			state->mediaHistory = nullptr;
			state->session = nullptr;
		},
	});
}

} // namespace Test

#endif // _DEBUG
