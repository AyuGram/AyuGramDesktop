// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/ayu_settings.h"
#include "settings/settings_common.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/slide_wrap.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

using BoolGetter = bool (AyuSettings::*)() const;
using BoolSetter = void (AyuSettings::*)(bool);

struct NestedEntry
{
	QString checkboxLabel;
	Fn<bool()> getter;
	Fn<void(bool)> setter;
	Fn<bool()> lockGetter;     // nullptr = no lock support
	Fn<void(bool)> lockSetter;
};

void AddBetaBadge(not_null<Button*> parent);

void ShowRestartPrompt(not_null<Window::SessionController*> controller);

not_null<Ui::RpWidget*> AddInnerToggle(not_null<Ui::VerticalLayout*> container,
									   const style::SettingsButton &st,
									   std::vector<not_null<Ui::AbstractCheckView*>> innerCheckViews,
									   not_null<Ui::SlideWrap<>*> wrap,
									   rpl::producer<QString> buttonLabel,
									   bool toggledWhenAll,
									   std::vector<Fn<bool()>> lockChecks = {},
									   rpl::event_stream<> *lockChanges = nullptr);

struct CollapsibleToggleResult {
	Fn<void()> refresh;
	Ui::RpWidget *widget = nullptr;
};

CollapsibleToggleResult AddCollapsibleToggle(not_null<Ui::VerticalLayout*> container,
						  rpl::producer<QString> title,
						  std::vector<NestedEntry> checkboxes,
						  bool toggledWhenAll);

void AddChooseButtonWithIconAndRightTextInner(not_null<Ui::VerticalLayout*> container,
											  not_null<Window::SessionController*> controller,
											  int initialState,
											  std::vector<QString> options,
											  rpl::producer<QString> text,
											  rpl::producer<QString> boxTitle,
											  const style::SettingsButton &st,
											  Settings::IconDescriptor &&descriptor,
											  const Fn<void(int)> &setter);

void AddChooseButtonWithIconAndRightText(not_null<Ui::VerticalLayout*> container,
										 not_null<Window::SessionController*> controller,
										 int initialState,
										 std::vector<QString> options,
										 rpl::producer<QString> text,
										 rpl::producer<QString> boxTitle,
										 const style::icon &icon,
										 const Fn<void(int)> &setter);

void AddChooseButtonWithIconAndRightText(not_null<Ui::VerticalLayout*> container,
										 not_null<Window::SessionController*> controller,
										 int initialState,
										 std::vector<QString> options,
										 rpl::producer<QString> text,
										 rpl::producer<QString> boxTitle,
										 const Fn<void(int)> &setter);

not_null<Button*> AddToggle(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	Fn<bool()> getter,
	Fn<void(bool)> setter);

not_null<Button*> AddToggle(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	Fn<bool()> getter,
	Fn<void(bool)> setter,
	const style::icon &icon);

void AddSectionDivider(not_null<Ui::VerticalLayout*> container);

not_null<Button*> AddSettingToggle(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	BoolGetter getter,
	BoolSetter setter);

not_null<Button*> AddSettingToggle(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	BoolGetter getter,
	BoolSetter setter,
	const style::icon &icon);

} // namespace Settings
