/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/settings_common_session.h"

#include "settings/cloud_password/settings_cloud_password_email_confirm.h"
#include "settings/settings_chat.h"
#include "settings/settings_main.h"

// AyuGram includes
#include "ayu/ui/settings/settings_ayu.h"


// AyuGram includes
#include "ayu/ui/settings/settings_ayu.h"


namespace Settings {

void FillMenu(
		not_null<Window::SessionController*> controller,
		Type type,
		Fn<void(Type)> showOther,
		Ui::Menu::MenuCallback addAction) {
	const auto window = &controller->window();
	if (type == Chat::Id()) {
		addAction(
			tr::lng_settings_bg_theme_create(tr::now),
			[=] { window->show(Box(Window::Theme::CreateBox, window)); },
			&st::menuIconChangeColors);
	} else if (type == CloudPasswordEmailConfirmId()) {
		const auto api = &controller->session().api();
		if (const auto state = api->cloudPassword().stateCurrent()) {
			if (state->unconfirmedPattern.isEmpty()) {
				return;
			}
		}
		addAction(
			tr::lng_settings_password_abort(tr::now),
			[=] { api->cloudPassword().clearUnconfirmedPassword(); },
			&st::menuIconCancel);
	} else if (type == Ayu::Id()) {
		addAction(
			tr::ayu_RegisterURLScheme(tr::now),
			[=] { Core::Application::RegisterUrlScheme(); },
			&st::menuIconLinks);
		addAction(
			tr::lng_restart_button(tr::now),
			[=] { Core::Restart(); },
			&st::menuIconRestore);
	} else {
		const auto &list = Core::App().domain().accounts();
		if (list.size() < Core::App().domain().maxAccounts()) {
			addAction(tr::lng_menu_add_account(tr::now), [=] {
				Core::App().domain().addActivated(MTP::Environment{});
			}, &st::menuIconAddAccount);
		}
		if (!controller->session().supportMode()) {
			addAction(
				tr::lng_settings_information(tr::now),
				[=] { showOther(Information::Id()); },
				&st::menuIconInfo);
		}
		addAction({
			.text = tr::lng_settings_logout(tr::now),
			.handler = [=] { window->showLogoutConfirmation(); },
			.icon = &st::menuIconLeaveAttention,
			.isAttention = true,
		});
	}
}

bool HasMenu(Type type) {
	return (type == ::Settings::CloudPasswordEmailConfirmId())
		|| (type == Main::Id())
		|| (type == Chat::Id())
		|| (type == Ayu::Id());
}

} // namespace Settings
