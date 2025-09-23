// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_filters.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/features/filters/filters_cache_controller.h"
#include "boxes/peer_list_box.h"
#include "filters/peer_global_exclusion.h"
#include "filters/settings_filters_list.h"
#include "settings/settings_common.h"
#include "styles/style_settings.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

rpl::producer<QString> AyuFilters::title() {
	return tr::ayu_CategoryFilters();
}

AyuFilters::AyuFilters(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent) {
	setupContent(controller);
}

void SetupFiltersSettings(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSkip(container);
	AddSubsectionTitle(container, tr::ayu_RegexFilters());

	AddButtonWithIcon(
		container,
		tr::ayu_RegexFiltersEnable(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->filtersEnabled)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->filtersEnabled);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_filtersEnabled(enabled);
			AyuSettings::save();

			FiltersCacheController::rebuildCache();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_RegexFiltersEnableSharedInChats(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->filtersEnabledInChats)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->filtersEnabledInChats);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_filtersEnabledInChats(enabled);
			AyuSettings::save();

			FiltersCacheController::rebuildCache();
		},
		container->lifetime());


	AddButtonWithIcon(
		container,
		tr::ayu_FiltersHideFromBlocked(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->hideFromBlocked)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->hideFromBlocked);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_hideFromBlocked(enabled);
			AyuSettings::save();

			FiltersCacheController::rebuildCache();
		},
		container->lifetime());
	AddSkip(container);
}

void SetupShared(not_null<Window::SessionController*> controller,
				 Ui::VerticalLayout *container) {
	Ui::AddSkip(container);

	auto button = container->add(object_ptr<Ui::SettingsButton>(
		container,
		tr::ayu_RegexFiltersShared()
	));
	button->addClickHandler([=]
	{
		controller->dialogId = std::nullopt; // ensure we're handling shared filters
		controller->showExclude = false;
		controller->showSettings(AyuFiltersList::Id());
	});
}

void SetupExclusions(
	not_null<Window::SessionController*> controller,
	not_null<Ui::VerticalLayout*> container
) {
	container->add(object_ptr<Ui::SettingsButton>(
		container,
		rpl::single(QString("Exclusions"))
	))->addClickHandler([=]
	{
		auto ctrl = std::make_unique<GlobalExclusionListController>(
			&controller->session(),
			controller
		);

		auto box = Box<PeerListBox>(std::move(ctrl),
									[](not_null<PeerListBox*> box)
									{
										box->setTitle(rpl::single(QString("Exclusions")));
										box->addButton(tr::lng_close(), [=] { box->closeBox(); });
									});

		controller->show(std::move(box));
	});
}

void SetupMessageFilters(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_RegexFilters());

	AddButtonWithIcon(
		container,
		tr::ayu_FiltersHideFromBlocked(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->hideFromBlocked)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->hideFromBlocked);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_hideFromBlocked(enabled);
			AyuSettings::save();
		},
		container->lifetime());
}

void AyuFilters::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	SetupFiltersSettings(content);

	AddDivider(content);

	SetupShared(controller, content);

	SetupExclusions(controller, content);

	ResizeFitChild(this, content);
}

} // namespace Settings
