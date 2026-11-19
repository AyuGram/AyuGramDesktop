// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "lupin/lupin_settings.h"
#include "lupin/ui/settings/settings_lupin_utils.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"

namespace Settings::LupinBuilder {

class LupinSectionBuilder {
public:
	explicit LupinSectionBuilder(Builder::SectionBuilder &builder);

	[[nodiscard]] Builder::SectionBuilder &base();

	struct SettingToggleArgs {
		QString id;
		QStringList altIds;
		rpl::producer<QString> title;
		BoolGetter getter;
		BoolSetter setter;
		IconDescriptor icon;
		QStringList keywords;
		rpl::producer<bool> shown;
	};
	Ui::SettingsButton *addSettingToggle(SettingToggleArgs &&args);

	struct ToggleArgs {
		QString id;
		QStringList altIds;
		rpl::producer<QString> title;
		Fn<bool()> getter;
		Fn<void(bool)> setter;
		IconDescriptor icon;
		QStringList keywords;
		rpl::producer<bool> shown;
	};
	Ui::SettingsButton *addToggle(ToggleArgs &&args);

	struct CollapsibleToggleArgs {
		QString id;
		QStringList altIds;
		rpl::producer<QString> title;
		std::vector<NestedEntry> checkboxes;
		bool toggledWhenAll = true;
		QStringList keywords;
	};
	Fn<void()> addCollapsibleToggle(CollapsibleToggleArgs &&args);

	struct ChooseButtonArgs {
		QString id;
		QStringList altIds;
		rpl::producer<QString> title;
		rpl::producer<QString> boxTitle;
		int initialSelection;
		std::vector<QString> options;
		Fn<void(int)> setter;
		IconDescriptor icon;
		QStringList keywords;
	};
	void addChooseButton(ChooseButtonArgs &&args);

	struct SliderArgs {
		QString id;
		QStringList altIds;
		rpl::producer<QString> title;
		bool showTitle = true;
		int steps;
		int current;
		Fn<int(int)> indexToValue;
		Fn<void(int)> onChanged;
		Fn<void(int)> onFinalChanged;
		Fn<QString(int)> formatLabel;
		QStringList keywords;
	};
	void addSlider(SliderArgs &&args);

	void addBetaBadge(not_null<Ui::SettingsButton*> button);

	void addSectionDivider();

private:
	Builder::SectionBuilder &_builder;
};

} // namespace Settings::LupinBuilder
