// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "settings/settings_common.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/slide_wrap.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

struct NestedEntry
{
	QString checkboxLabel;
	bool initial;
	std::function<void(bool)> callback;
};

QString asBeta(const QString &text);
rpl::producer<QString> asBeta(rpl::producer<QString> text);

not_null<Ui::RpWidget*> AddInnerToggle(not_null<Ui::VerticalLayout*> container,
									   const style::SettingsButton &st,
									   std::vector<not_null<Ui::AbstractCheckView*>> innerCheckViews,
									   not_null<Ui::SlideWrap<>*> wrap,
									   rpl::producer<QString> buttonLabel,
									   bool toggledWhenAll);

void AddCollapsibleToggle(not_null<Ui::VerticalLayout*> container,
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

} // namespace Settings