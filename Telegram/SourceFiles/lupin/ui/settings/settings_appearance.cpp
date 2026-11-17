// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/ui/settings/settings_appearance.h"

#include "lang_auto.h"
#include "lupin/lupin_settings.h"
#include "ayu/ayu_ui_settings.h"
#include "lupin/ui/boxes/font_selector.h"
#include "lupin/ui/components/avatar_corners_preview.h"
#include "lupin/ui/components/icon_picker.h"
#include "lupin/ui/settings/lupin_builder.h"
#include "lupin/ui/settings/settings_lupin_utils.h"
#include "lupin/ui/settings/settings_main.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "inline_bots/bot_attach_web_view.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_lupin_icons.h"
#include "styles/style_lupin_styles.h"
#include "styles/style_dialogs.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/themes/window_theme.h"
#include "window/window_session_controller.h"

namespace Settings {

using namespace Builder;
using namespace LupinBuilder;

namespace {

constexpr auto kLupinThemePrefKey = std::string_view("lupin/themeIndex");

const char *LupinThemePaths[] = {
	":/gui/lupingram.tdesktop-theme",
	":/gui/lupingram-amber.tdesktop-theme",
	":/gui/lupingram-ice.tdesktop-theme",
	":/gui/lupingram-crimson.tdesktop-theme",
};

void ApplyLupinTheme(int index) {
	if (index < 0 || index >= int(sizeof(LupinThemePaths) / sizeof(LupinThemePaths[0]))) {
		return;
	}
	Window::Theme::ApplyDefaultWithPath(
		QString::fromLatin1(LupinThemePaths[index]));
	Window::Theme::KeepApplied();
	Core::App().settings().writePref<int>(kLupinThemePrefKey, index);
	Core::App().saveSettingsDelayed();
}

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

void BuildAppIcon(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle({
		.id = u"lupin/appIcon"_q,
		.title = tr::lupin_AppIconHeader(),
	});

	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		return {
			.widget = object_ptr<IconPicker>(ctx.container),
			.margin = st::settingsButtonNoIcon.padding,
		};
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	builder.addDivider();
	builder.addSkip();
	lupin.addSettingToggle({
		.id = u"lupin/hideNotificationBadge"_q,
		.title = tr::lupin_HideNotificationBadge(),
		.getter = &LupinSettings::hideNotificationBadge,
		.setter = &LupinSettings::setHideNotificationBadge,
	});
	builder.addSkip();
	builder.addDividerText(tr::lupin_HideNotificationBadgeDescription());
	builder.addSkip();
#else
    builder.addDivider();
    builder.addSkip();
#endif
}

void BuildAvatarCorners(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	auto *settings = &LupinSettings::getInstance();
	const auto controller = builder.controller();

	const auto mapRadius = [](int val)
	{
		if (val == 0) {
			return tr::lupin_AvatarCornersSquare(tr::now).toUpper();
		} else if (val == AyuUiSettings::kMaxAvatarCorners) {
			return tr::lupin_AvatarCornersCircle(tr::now).toUpper();
		}
		return QString::number(val);
	};

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		const auto container = ctx.container;
		auto title = object_ptr<Ui::FlatLabel>(
			container,
			tr::lupin_AvatarCorners(),
			st::defaultSubsectionTitle);
		const auto titleRaw = title.data();

		const auto badge = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
			container,
			object_ptr<Ui::FlatLabel>(
				container,
				settings->avatarCornersValue() | rpl::map(mapRadius),
				st::settingsPremiumNewBadge),
			st::lupinBetaBadgePadding);
		badge->show();
		badge->setAttribute(Qt::WA_TransparentForMouseEvents);
		badge->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(badge);
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgActive);
			const auto r = st::lupinBetaBadgePadding.left();
			p.drawRoundedRect(badge->rect(), r, r);
		}, badge->lifetime());

		titleRaw->geometryValue() | rpl::on_next([=](QRect geometry) {
			badge->moveToLeft(
				geometry.x()
					+ titleRaw->textMaxWidth()
					+ st::settingsPremiumNewBadgePosition.x(),
				geometry.y()
					+ (geometry.height() - badge->height()) / 2);
		}, badge->lifetime());

		return {
			.widget = std::move(title),
			.margin = st::defaultSubsectionTitlePadding,
		};
	}, [] {
		return SearchEntry{
			.id = u"lupin/avatarCorners"_q,
			.title = tr::lupin_AvatarCorners(tr::now),
		};
	});

	auto *previewRaw = static_cast<AvatarCornersPreview*>(nullptr);
	builder.add([&](const Builder::WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<AvatarCornersPreview>(
			ctx.container,
			controller);
		previewRaw = preview.data();
		const auto vMargin = st::settingsButtonNoIcon.padding
			- st::defaultDialogRow.padding;
		return {
			.widget = std::move(preview),
			.margin = QMargins(0, vMargin.top(), 0, vMargin.bottom()),
		};
	});

	lupin.addSlider({
		.id = u"lupin/avatarCornersSlider"_q,
		.title = rpl::single(QString()),
		.showTitle = false,
		.steps = AyuUiSettings::kMaxAvatarCorners + 1,
		.current = settings->avatarCorners(),
		.onChanged = [=](int val) {
			LupinSettings::getInstance().setAvatarCorners(val);
			if (previewRaw) {
				previewRaw->update();
			}
		},
		.onFinalChanged = [=](int val) {
			LupinSettings::getInstance().setAvatarCorners(val);
			ShowRestartPrompt(controller);
		},
	});

	lupin.addSettingToggle({
		.id = u"lupin/singleCornerRadius"_q,
		.title = tr::lupin_SingleCornerRadius(),
		.getter = &LupinSettings::singleCornerRadius,
		.setter = &LupinSettings::setSingleCornerRadius,
	});

	builder.addSkip();
	builder.addDividerText(tr::lupin_SingleCornerRadiusDescription());
	builder.addSkip();
}

void BuildAppearance(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	auto *settings = &LupinSettings::getInstance();

	builder.addSubsectionTitle(tr::lupin_CategoryAppearance());

	lupin.addChooseButton({
		.id = u"lupin/theme"_q,
		.title = tr::lupin_ThemePickerTitle(),
		.boxTitle = tr::lupin_ThemePickerBoxTitle(),
		.initialSelection = Core::App().settings().readPref<int>(
			kLupinThemePrefKey,
			0),
		.options = {
			tr::lupin_ThemeMatrix(tr::now),
			tr::lupin_ThemeAmber(tr::now),
			tr::lupin_ThemeIce(tr::now),
			tr::lupin_ThemeCrimson(tr::now),
		},
		.setter = [](int index) { ApplyLupinTheme(index); },
	});

	lupin.addSettingToggle({
		.id = u"lupin/materialSwitches"_q,
		.altIds = { u"lupin/newSwitchStyle"_q },
		.title = tr::lupin_MaterialSwitches(),
		.getter = &LupinSettings::materialSwitches,
		.setter = &LupinSettings::setMaterialSwitches,
	});
	lupin.addSettingToggle({
		.id = u"lupin/disableCustomBackgrounds"_q,
		.altIds = { u"lupin/customThemes"_q },
		.title = tr::lupin_DisableCustomBackgrounds(),
		.getter = &LupinSettings::disableCustomBackgrounds,
		.setter = &LupinSettings::setDisableCustomBackgrounds,
	});
	lupin.addSettingToggle({
		.id = u"lupin/hidePremiumStatuses"_q,
		.title = tr::lupin_HidePremiumStatuses(),
		.getter = &LupinSettings::hidePremiumStatuses,
		.setter = &LupinSettings::setHidePremiumStatuses,
	});

	const auto controller = builder.controller();
	builder.addButton({
		.id = u"lupin/monoFont"_q,
		.title = tr::lupin_MonospaceFont(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(
			settings->monoFont().isEmpty()
				? tr::lupin_FontDefault(tr::now)
				: settings->monoFont()),
		.onClick = [=] {
			LupinUi::FontSelectorBox::Show(
				controller,
				[=](const QString &font) {
					LupinSettings::getInstance().setMonoFont(font);
				});
		},
	});

	lupin.addSectionDivider();
}

void BuildChatFolders(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lupin_ChatFoldersHeader());

	lupin.addSettingToggle({
		.id = u"lupin/hideNotificationCounters"_q,
		.altIds = { u"lupin/tabCounter"_q },
		.title = tr::lupin_HideNotificationCounters(),
		.getter = &LupinSettings::hideNotificationCounters,
		.setter = &LupinSettings::setHideNotificationCounters,
	});
	lupin.addSettingToggle({
		.id = u"lupin/hideAllChatsFolder"_q,
		.altIds = { u"lupin/hideAllChats"_q },
		.title = tr::lupin_HideAllChats(),
		.getter = &LupinSettings::hideAllChatsFolder,
		.setter = &LupinSettings::setHideAllChatsFolder,
	});

	lupin.addSectionDivider();
}

void BuildTrayElements(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lupin_TrayElementsHeader());

	lupin.addSettingToggle({
		.id = u"lupin/showGhostToggleInTray"_q,
		.title = tr::lupin_EnableGhostModeTray(),
		.getter = &LupinSettings::showGhostToggleInTray,
		.setter = &LupinSettings::setShowGhostToggleInTray,
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	lupin.addSettingToggle({
		.id = u"lupin/showStreamerToggleInTray"_q,
		.title = tr::lupin_EnableStreamerModeTray(),
		.getter = &LupinSettings::showStreamerToggleInTray,
		.setter = &LupinSettings::setShowStreamerToggleInTray,
	});
#endif

	lupin.addSectionDivider();
}

void BuildDrawerElements(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lupin_DrawerElementsHeader());

	lupin.addSettingToggle({
		.id = u"lupin/showMyProfileInDrawer"_q,
		.title = tr::lng_menu_my_profile(),
		.getter = &LupinSettings::showMyProfileInDrawer,
		.setter = &LupinSettings::setShowMyProfileInDrawer,
		.icon = { &st::menuIconProfile },
	});

	const auto controller = builder.controller();
	if (controller && HasDrawerBots(controller)) {
		lupin.addSettingToggle({
			.id = u"lupin/showBotsInDrawer"_q,
			.title = tr::lng_filters_type_bots(),
			.getter = &LupinSettings::showBotsInDrawer,
			.setter = &LupinSettings::setShowBotsInDrawer,
			.icon = { &st::menuIconBot },
		});
	}

	lupin.addSettingToggle({
		.id = u"lupin/showNewGroupInDrawer"_q,
		.title = tr::lng_create_group_title(),
		.getter = &LupinSettings::showNewGroupInDrawer,
		.setter = &LupinSettings::setShowNewGroupInDrawer,
		.icon = { &st::menuIconGroups },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showNewChannelInDrawer"_q,
		.title = tr::lng_create_channel_title(),
		.getter = &LupinSettings::showNewChannelInDrawer,
		.setter = &LupinSettings::setShowNewChannelInDrawer,
		.icon = { &st::menuIconChannel },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showContactsInDrawer"_q,
		.title = tr::lng_menu_contacts(),
		.getter = &LupinSettings::showContactsInDrawer,
		.setter = &LupinSettings::setShowContactsInDrawer,
		.icon = { &st::menuIconUserShow },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showCallsInDrawer"_q,
		.title = tr::lng_menu_calls(),
		.getter = &LupinSettings::showCallsInDrawer,
		.setter = &LupinSettings::setShowCallsInDrawer,
		.icon = { &st::menuIconPhone },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showSavedMessagesInDrawer"_q,
		.title = tr::lng_saved_messages(),
		.getter = &LupinSettings::showSavedMessagesInDrawer,
		.setter = &LupinSettings::setShowSavedMessagesInDrawer,
		.icon = { &st::menuIconSavedMessages },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showLReadToggleInDrawer"_q,
		.title = tr::lupin_LReadMessages(),
		.getter = &LupinSettings::showLReadToggleInDrawer,
		.setter = &LupinSettings::setShowLReadToggleInDrawer,
		.icon = { &st::lupinLReadMenuIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showSReadToggleInDrawer"_q,
		.title = tr::lupin_SReadMessages(),
		.getter = &LupinSettings::showSReadToggleInDrawer,
		.setter = &LupinSettings::setShowSReadToggleInDrawer,
		.icon = { &st::lupinSReadMenuIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showNightModeToggleInDrawer"_q,
		.title = tr::lng_menu_night_mode(),
		.getter = &LupinSettings::showNightModeToggleInDrawer,
		.setter = &LupinSettings::setShowNightModeToggleInDrawer,
		.icon = { &st::menuIconNightMode },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showGhostToggleInDrawer"_q,
		.title = tr::lupin_GhostModeToggle(),
		.getter = &LupinSettings::showGhostToggleInDrawer,
		.setter = &LupinSettings::setShowGhostToggleInDrawer,
		.icon = { &st::lupinGhostIcon },
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	lupin.addSettingToggle({
		.id = u"lupin/showStreamerToggleInDrawer"_q,
		.title = tr::lupin_StreamerModeToggle(),
		.getter = &LupinSettings::showStreamerToggleInDrawer,
		.setter = &LupinSettings::setShowStreamerToggleInDrawer,
		.icon = { &st::lupinStreamerModeMenuIcon },
	});
#endif

	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = LupinAppearance::Id(),
	.parentId = LupinMain::Id(),
	.title = &tr::lupin_CategoryAppearance,
	.icon = &st::menuIconPalette,
}, [](SectionBuilder &builder) {
	auto lupin = LupinSectionBuilder(builder);

	builder.addSkip();
	BuildAppIcon(builder, lupin);
	BuildAvatarCorners(builder, lupin);
	BuildAppearance(builder, lupin);
	BuildChatFolders(builder, lupin);
	BuildTrayElements(builder, lupin);
	BuildDrawerElements(builder, lupin);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> LupinAppearance::title() {
	return tr::lupin_CategoryAppearance();
}

LupinAppearance::LupinAppearance(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void LupinAppearance::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type LupinAppearanceId() {
	return LupinAppearance::Id();
}

} // namespace Settings
