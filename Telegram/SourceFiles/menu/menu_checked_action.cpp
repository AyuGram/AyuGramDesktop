/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "menu/menu_checked_action.h"

#include "base/unique_qptr.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu_common.h"
#include "ui/widgets/popup_menu.h"
#include "ui/painter.h"

#include "styles/style_media_player.h"

namespace {

class CheckedAction final : public Ui::Menu::Action {
public:
	CheckedAction(
		not_null<Ui::Menu::Menu*> parent,
		const style::Menu &st,
		not_null<QAction*> action,
		const style::icon *icon,
		bool checked);

private:
	void paintEvent(QPaintEvent *e) override;

	const bool _checked = false;

};

CheckedAction::CheckedAction(
	not_null<Ui::Menu::Menu*> parent,
	const style::Menu &st,
	not_null<QAction*> action,
	const style::icon *icon,
	bool checked)
: Ui::Menu::Action(parent, st, action, icon, icon)
, _checked(checked) {
	setMinWidth(minWidth() + st.itemRightSkip + st::mediaPlayerMenuCheck.width());
}

void CheckedAction::paintEvent(QPaintEvent *e) {
	Ui::Menu::Action::paintEvent(e);

	if (!_checked) {
		return;
	}

	Painter p(this);
	const auto &icon = st::mediaPlayerMenuCheck;
	const auto left = width() - st().itemRightSkip - icon.width();
	const auto top = (height() - icon.height()) / 2;
	icon.paint(p, left, top, width());
}

} // namespace

namespace Menu {

not_null<QAction*> AddCheckedAction(
		not_null<Ui::PopupMenu*> menu,
		const QString &text,
		Fn<void()> callback,
		const style::icon *icon,
		bool checked) {
	auto item = base::make_unique_q<CheckedAction>(
		menu->menu(),
		menu->st().menu,
		Ui::Menu::CreateAction(
			menu->menu().get(),
			text,
			std::move(callback)),
		icon,
		checked);
	return menu->addAction(std::move(item));
}

} // namespace Menu
