// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/filters/per_dialog_filter.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "ayu/ui/settings/filters/settings_filters_list.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "styles/style_menu_icons.h"
#include "ui/painter.h"
#include "window/window_session_controller.h"

#include <utility>

namespace Settings {

PerDialogFiltersListRow::PerDialogFiltersListRow(ID dialogId)
	: PeerListRow(PeerListRowId(dialogId))
	  , _dialogId(dialogId)
	  , peerId(PeerId(PeerIdHelper(abs(dialogId)))) {
}

ID PerDialogFiltersListRow::dialogId() const {
	return _dialogId;
}

QString PerDialogFiltersListRow::generateName() {
	if (const auto from = getPeerFromDialogId(peerId.value & PeerId::kChatTypeMask)) {
		this->setPeer(from);
		return PeerListRow::generateName();
	}
	return QString("UNKNOWN (ID: %1)").arg(QString::number(peerId.value & PeerId::kChatTypeMask));
}

PaintRoundImageCallback PerDialogFiltersListRow::generatePaintUserpicCallback(bool forceRound) {
	if (const auto from = getPeerFromDialogId(peerId.value & PeerId::kChatTypeMask)) {
		this->setPeer(from);
		return PeerListRow::generatePaintUserpicCallback(forceRound);
	}

	return [=](Painter &p, int x, int y, int outerWidth, int size) mutable
	{
		using namespace Ui;
		const auto realId = peerId.value & PeerId::kChatTypeMask;
		auto _userpicEmpty = std::make_unique<EmptyUserpic>(
			EmptyUserpic::UserpicColor(realId % 7),
			QString("U")); // U - Unknown
		_userpicEmpty->paintCircle(p, x, y, outerWidth, size);
	};
}

PerDialogFiltersListController::PerDialogFiltersListController(not_null<Main::Session*> session,
															   not_null<Window::SessionController*> controller,
															   bool shadowBan)
	: _session(session)
	  , _controller(controller)
	  , shadowBan(shadowBan) {
}

Main::Session &PerDialogFiltersListController::session() const {
	return *_session;
}

void PerDialogFiltersListController::prepareShadowBan() {
	const auto &settings = AyuSettings::getInstance();
	const auto &shadowBanned = settings.shadowBanIds();

	for (const auto id : shadowBanned) {
		delegate()->peerListAppendRow(std::make_unique<PerDialogFiltersListRow>(id));
	}
}

void PerDialogFiltersListController::prepare() {
	if (shadowBan) {
		prepareShadowBan();
		return;
	}
	const auto filters = AyuDatabase::getAllRegexFilters();
	const auto exclusions = AyuDatabase::getAllFiltersExclusions();

	if (filters.empty() && exclusions.empty()) {
		return;
	}

	countsByDialogIds.clear();

	for (const auto &filter : filters) {
		if (filter.dialogId.has_value()) {
			countsByDialogIds[filter.dialogId.value()].filters++;
		}
	}
	for (const auto &exclusion : exclusions) {
		countsByDialogIds[exclusion.dialogId].exclusions++;
	}

	for (const auto &[id, count] : countsByDialogIds) {
		auto row = std::make_unique<PerDialogFiltersListRow>(id);
		auto status = QString();
		if (count.filters > 0) {
			status += tr::ayu_RegexFiltersAmount(tr::now, lt_count, count.filters);
			if (count.exclusions > 0) {
				status += ", ";
			}
		}
		if (count.exclusions > 0) {
			status += tr::ayu_RegexFiltersExcludedAmount(tr::now, lt_count, count.exclusions);
		}

		row->setCustomStatus(status, false);

		delegate()->peerListAppendRow(std::move(row));
	}

	// sortByName();

	delegate()->peerListRefreshRows();
}

void PerDialogFiltersListController::rowClicked(not_null<PeerListRow*> peer) {
	ID did;
	if (const auto row = dynamic_cast<PerDialogFiltersListRow*>(peer.get())) {
		did = row->dialogId();
	} else if (peer->special()) {
		const auto pred = static_cast<long long>(peer->id() & PeerId::kChatTypeMask);
		did = countsByDialogIds.contains(pred) ? pred : -pred;
	} else {
		did = getDialogIdFromPeer(peer->peer());
	}
	if (shadowBan) {
		auto _contextMenu = new Ui::PopupMenu(nullptr, st::popupMenuWithIcons);
		_contextMenu->setAttribute(Qt::WA_DeleteOnClose);

		_contextMenu->addAction(
			tr::lng_theme_delete(tr::now),
			[=]
			{
				if (AyuSettings::getInstance().isShadowBanned(did)) {
					AyuSettings::getInstance().removeShadowBan(did);
				} else {
					AyuSettings::getInstance().addShadowBan(did);
				}
			},
			&st::menuIconDelete);

		_contextMenu->popup(QCursor::pos());
		return;
	}
	_controller->dialogId = did;
	_controller->showExclude = true;
	_controller->showSettings(AyuFiltersList::Id());
}

}
