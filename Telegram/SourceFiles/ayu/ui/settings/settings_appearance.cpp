// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_appearance.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/font_selector.h"
#include "ayu/ui/components/icon_picker.h"
#include "inline_bots/bot_attach_web_view.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

namespace {

bool HasDrawerBots(not_null<Window::SessionController*> controller) {
	// todo: maybe iterate through all accounts
	const auto bots = &controller->session().attachWebView();
	for (const auto &bot : bots->attachBots()) {
		if (!bot.inMainMenu || !bot.media) {
			continue;
		}

		return true;
	}

	return false;
}

}

rpl::producer<QString> AyuAppearance::title() {
	return tr::ayu_CategoryAppearance();
}

AyuAppearance::AyuAppearance(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent) {
	setupContent(controller);
}

void SetupAppIcon(not_null<Ui::VerticalLayout*> container) {
	AddSubsectionTitle(container, tr::ayu_AppIconHeader());
	container->add(
		object_ptr<IconPicker>(container),
		st::settingsCheckboxPadding);

#ifdef Q_OS_WIN
	auto *settings = &AyuSettings::getInstance();

	AddDivider(container);
	AddSkip(container);
	AddButtonWithIcon(
		container,
		tr::ayu_HideNotificationBadge(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->hideNotificationBadge)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->hideNotificationBadge);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_hideNotificationBadge(enabled);
			AyuSettings::save();
		},
		container->lifetime());
	AddSkip(container);
	AddDividerText(container, tr::ayu_HideNotificationBadgeDescription());
	AddSkip(container);
#endif
}

void SetupAppearance(not_null<Ui::VerticalLayout*> container, not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_CategoryAppearance());

	AddButtonWithIcon(
		container,
		tr::ayu_MaterialSwitches(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->materialSwitches)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->materialSwitches);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_materialSwitches(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_RemoveMessageTail(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->removeMessageTail)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->removeMessageTail);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_removeMessageTail(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_DisableCustomBackgrounds(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->disableCustomBackgrounds)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->disableCustomBackgrounds);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_disableCustomBackgrounds(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	const auto monoButton = AddButtonWithLabel(
		container,
		tr::ayu_MonospaceFont(),
		rpl::single(
			settings->monoFont.isEmpty() ? tr::ayu_FontDefault(tr::now) : settings->monoFont
		),
		st::settingsButtonNoIcon);
	const auto monoGuard = Ui::CreateChild<base::binary_guard>(monoButton.get());

	monoButton->addClickHandler(
		[=]
		{
			*monoGuard = AyuUi::FontSelectorBox::Show(
				controller,
				[=](QString font)
				{
					AyuSettings::set_monoFont(std::move(font));
					AyuSettings::save();
				});
		});

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

void SetupChatFolders(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_ChatFoldersHeader());

	AddButtonWithIcon(
		container,
		tr::ayu_HideNotificationCounters(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->hideNotificationCounters)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->hideNotificationCounters);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_hideNotificationCounters(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_HideAllChats(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->hideAllChatsFolder)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->hideAllChatsFolder);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_hideAllChatsFolder(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

void SetupDrawerElements(not_null<Ui::VerticalLayout*> container, not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_DrawerElementsHeader());

	AddButtonWithIcon(
		container,
		tr::lng_menu_my_profile(),
		st::settingsButton,
		{&st::menuIconProfile}
	)->toggleOn(
		rpl::single(settings->showMyProfileInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showMyProfileInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showMyProfileInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	if (HasDrawerBots(controller)) {
		AddButtonWithIcon(
			container,
			tr::lng_filters_type_bots(),
			st::settingsButton,
			{&st::menuIconBot}
		)->toggleOn(
			rpl::single(settings->showBotsInDrawer)
		)->toggledValue(
		) | rpl::filter(
			[=](bool enabled)
			{
				return (enabled != settings->showBotsInDrawer);
			}) | rpl::start_with_next(
			[=](bool enabled)
			{
				AyuSettings::set_showBotsInDrawer(enabled);
				AyuSettings::save();
			},
			container->lifetime());
	}

	AddButtonWithIcon(
		container,
		tr::lng_create_group_title(),
		st::settingsButton,
		{&st::menuIconGroups}
	)->toggleOn(
		rpl::single(settings->showNewGroupInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showNewGroupInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showNewGroupInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::lng_create_channel_title(),
		st::settingsButton,
		{&st::menuIconChannel}
	)->toggleOn(
		rpl::single(settings->showNewChannelInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showNewChannelInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showNewChannelInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::lng_menu_contacts(),
		st::settingsButton,
		{&st::menuIconUserShow}
	)->toggleOn(
		rpl::single(settings->showContactsInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showContactsInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showContactsInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::lng_menu_calls(),
		st::settingsButton,
		{&st::menuIconPhone}
	)->toggleOn(
		rpl::single(settings->showCallsInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showCallsInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showCallsInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::lng_saved_messages(),
		st::settingsButton,
		{&st::menuIconSavedMessages}
	)->toggleOn(
		rpl::single(settings->showSavedMessagesInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showSavedMessagesInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showSavedMessagesInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_LReadMessages(),
		st::settingsButton,
		{&st::ayuLReadMenuIcon}
	)->toggleOn(
		rpl::single(settings->showLReadToggleInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showLReadToggleInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showLReadToggleInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_SReadMessages(),
		st::settingsButton,
		{&st::ayuSReadMenuIcon}
	)->toggleOn(
		rpl::single(settings->showSReadToggleInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showSReadToggleInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showSReadToggleInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::lng_menu_night_mode(),
		st::settingsButton,
		{&st::menuIconNightMode}
	)->toggleOn(
		rpl::single(settings->showNightModeToggleInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showNightModeToggleInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showNightModeToggleInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_GhostModeToggle(),
		st::settingsButton,
		{&st::ayuGhostIcon}
	)->toggleOn(
		rpl::single(settings->showGhostToggleInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showGhostToggleInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showGhostToggleInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());

#ifdef WIN32
	AddButtonWithIcon(
		container,
		tr::ayu_StreamerModeToggle(),
		st::settingsButton,
		{&st::ayuStreamerModeMenuIcon}
	)->toggleOn(
		rpl::single(settings->showStreamerToggleInDrawer)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showStreamerToggleInDrawer);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showStreamerToggleInDrawer(enabled);
			AyuSettings::save();
		},
		container->lifetime());
#endif

	AddSkip(container);
}

void SetupTrayElements(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_TrayElementsHeader());

	AddButtonWithIcon(
		container,
		tr::ayu_EnableGhostModeTray(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->showGhostToggleInTray)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showGhostToggleInTray);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showGhostToggleInTray(enabled);
			AyuSettings::save();
		},
		container->lifetime());

#ifdef WIN32
	AddButtonWithIcon(
		container,
		tr::ayu_EnableStreamerModeTray(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->showStreamerToggleInTray)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showStreamerToggleInTray);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showStreamerToggleInTray(enabled);
			AyuSettings::save();
		},
		container->lifetime());
#endif

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

void AyuAppearance::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	AddSkip(content);

	SetupAppIcon(content);
	SetupAppearance(content, controller);
	SetupChatFolders(content);
	SetupTrayElements(content);
	SetupDrawerElements(content, controller);
	AddSkip(content);

	ResizeFitChild(this, content);
}

} // namespace Settings