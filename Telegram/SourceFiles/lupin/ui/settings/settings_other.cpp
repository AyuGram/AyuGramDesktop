// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/ui/settings/settings_other.h"

#include "lang_auto.h"
#include "lupin/lupin_settings.h"
#include "lupin/ui/boxes/donate_qr_box.h"
#include "lupin/ui/settings/lupin_builder.h"
#include "lupin/ui/settings/settings_lupin_utils.h"
#include "lupin/ui/settings/settings_main.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/integration.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/vertical_list.h"
#include "ui/boxes/confirm_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "window/themes/window_theme.h"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QSvgRenderer>

namespace Settings {

using namespace Builder;
using namespace LupinBuilder;

namespace {

void BuildCrashReporting(SectionBuilder &builder, LupinSectionBuilder &lupin) {
#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	builder.addSkip();
	builder.addSubsectionTitle(tr::lupin_CategoryOther());

	lupin.addSettingToggle({
		.id = u"lupin/crashReporting"_q,
		.altIds = { u"lupin/crashlytics"_q },
		.title = tr::lupin_CrashReporting(),
		.getter = &LupinSettings::crashReporting,
		.setter = &LupinSettings::setCrashReporting,
		.icon = { &st::menuIconReport },
	});
	builder.addSkip();
	builder.addDividerText(tr::lupin_CrashReportingDescription());
#endif
}

void BuildOtherThings(SectionBuilder &builder) {
	const auto controller = builder.controller();

	builder.addSkip();
	builder.addButton({
		.id = u"lupin/registerUrlScheme"_q,
		.title = tr::lupin_RegisterURLScheme(),
		.icon = { &st::menuIconLink },
		.onClick = [=] {
			Core::Application::RegisterUrlScheme();
			controller->showToast(tr::lng_box_done(tr::now));
		},
	});
	builder.addButton({
		.id = u"lupin/resetSettings"_q,
		.title = tr::lupin_ResetSettings(),
		.icon = { &st::menuIconRestore },
		.onClick = [=] {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::lupin_ResetSettingsConfirmation(tr::rich),
				.confirmed = [=](Fn<void()> &&close) {
					LupinSettings::reset();
					controller->showToast(tr::lng_box_done(tr::now));
					close();
				},
				.confirmText = tr::lng_box_yes(),
			}));
		},
	});
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = LupinOther::Id(),
	.parentId = LupinMain::Id(),
	.title = &tr::lupin_CategoryOther,
	.icon = &st::menuIconFave,
}, [](SectionBuilder &builder) {
	auto lupin = LupinSectionBuilder(builder);

	builder.addSkip();
	BuildCrashReporting(builder, lupin);
	BuildOtherThings(builder);
});

} // namespace

rpl::producer<QString> LupinOther::title() {
	return tr::lupin_CategoryOther();
}

LupinOther::LupinOther(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void LupinOther::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type LupinOtherId() {
	return LupinOther::Id();
}

} // namespace Settings
