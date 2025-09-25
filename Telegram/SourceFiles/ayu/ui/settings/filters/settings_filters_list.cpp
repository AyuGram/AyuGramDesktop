// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_filters_list.h"

#include <styles/style_layers.h>
#include <styles/style_media_view.h>

#include "edit_filter.h"
#include "ayu/ayu_settings.h"

#include "lang_auto.h"

#include "boxes/connection_box.h"
#include "settings/settings_common.h"
#include "storage/localstorage.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include "../../components/icon_picker.h"
#include "ayu/data/ayu_database.h"
#include "ayu/features/filters/filters_cache_controller.h"
#include "ayu/features/filters/filters_utils.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_channel.h"
#include "rpl/mappers.h"
#include "ui/qt_object_factory.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

rpl::producer<QString> AyuFiltersList::title() {
	if (!dialogId.has_value()) {
		return tr::ayu_RegexFiltersShared();
	}

	const auto did = abs(dialogId.value());
	const auto from = getPeerFromDialogId(did);

	// todo: shorten based on available space
	// because it may break on custom fonts
	QString res;
	if (from) {
		auto name = from->topBarNameText();
		if (name.length() > 18) {
			name = name.left(17) + "…";
		}
		res = name;
	} else {
		res = tr::ayu_RegexFiltersHeader(tr::now) + " (" + QString::number(did) + ")";
	}

	return rpl::single(res);
}

AyuFiltersList::AyuFiltersList(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent), _controller(controller), _content(Ui::CreateChild<Ui::VerticalLayout>(this)) {
	if (_controller->dialogId.has_value()) {
		dialogId = _controller->dialogId.value();
	}

	setupContent(controller);
}

void AyuFiltersList::addNewFilter(const RegexFilter &filter, bool exclusion) {
	// stolen from EditPrivacyBox
	auto state = lifetime().make_state<RegexFilter>(filter);
	auto buttonText = lifetime().make_state<rpl::variable<QString>>(
		QString::fromStdString(state->text));
	auto isRemoved = lifetime().make_state<rpl::variable<bool>>(false);

	auto wrap = _content->add(
		object_ptr<Ui::SlideWrap<Button>>(
			_content,
			object_ptr<Button>(
				_content,
				buttonText->value(),
				st::settingsButtonNoIcon
			)
		)
	);

	const auto button = wrap->entity();

	wrap->toggleOn(isRemoved->value() | rpl::map(!rpl::mappers::_1));

	if (!state->enabled) {
		button->setColorOverride(st::storiesComposeGrayText->c);
	}

	auto defaultClickHandler = [=, this]() mutable
	{
		auto _contextMenu = new Ui::PopupMenu(this, st::popupMenuWithIcons);
		_contextMenu->setAttribute(Qt::WA_DeleteOnClose);

		_contextMenu->addAction(tr::lng_theme_edit(tr::now),
								[=, this]
								{
									_controller->show(RegexEditBox(state,
																   [=](const RegexFilter &done) mutable
																   {
																	   buttonText->force_assign(
																		   QString::fromStdString(done.text));
																	   *state = done;
																   }));
								},
								&st::menuIconEdit);

		_contextMenu->addAction(
			state->enabled ? tr::lng_settings_auto_night_disable(tr::now) : tr::lng_sure_enable(tr::now),
			[=]
			{
				state->enabled = !state->enabled;
				AyuDatabase::updateRegexFilter(*state);
				FiltersCacheController::rebuildCache();

				AyuSettings::fire_filtersUpdate();


				if (!state->enabled) {
					button->setColorOverride(st::storiesComposeGrayText->c);
				} else {
					button->setColorOverride({});
				}
				button->update();
			},
			state->enabled ? &st::menuIconBlock : &st::menuIconUnblock);

		_contextMenu->addSeparator();

		_contextMenu->addAction(tr::lng_theme_delete(tr::now),
								[=, this]
								{
									AyuDatabase::deleteFilter(state->id);
									AyuDatabase::deleteExclusionsByFilterId(state->id);
									FiltersCacheController::rebuildCache();

									AyuSettings::fire_filtersUpdate();

									isRemoved->force_assign(true);

									this->update();

									updateGeometry();
									repaint();

									// remove headers if there are no more filters left
									if (filters.empty() && filtersTitle) {
										filtersTitle->hide();
									}
									if (exclusions.empty() && excludedTitle) {
										excludedTitle->hide();
									}


									// resize();
									// update();
								},
								&st::menuIconDelete);

		_contextMenu->popup(QCursor::pos());
	};

	// we've opened filters list from top "Exclude" button
	// on click, close the section
	auto exclusionsClickHandler = [=, this]() mutable
	{
		Expects(dialogId.has_value());

		RegexFilterGlobalExclusion exclusion;
		exclusion.filterId = state->id;
		exclusion.dialogId = dialogId.value();

		AyuDatabase::addRegexExclusion(exclusion);
		FiltersCacheController::rebuildCache();

		AyuSettings::fire_filtersUpdate();

		_controller->showExclude = true;
		_controller->dialogId = dialogId;

		_controller->showSettings(AyuFiltersList::Id());
	};
	auto deleteExclusionsClickHandler = [=, this]() mutable
	{
		auto _contextMenu = new Ui::PopupMenu(this, st::popupMenuWithIcons);
		_contextMenu->setAttribute(Qt::WA_DeleteOnClose);

		_contextMenu->addAction(tr::lng_theme_delete(tr::now),
								[=, this]
								{
									Expects(dialogId.has_value());

									AyuDatabase::deleteExclusion(dialogId.value(), state->id);
									FiltersCacheController::rebuildCache();

									AyuSettings::fire_filtersUpdate();

									isRemoved->force_assign(true);

									this->update();

									updateGeometry();
									repaint();

									// remove headers if there are no more filters left
									if (filters.empty() && filtersTitle) {
										filtersTitle->hide();
									}
									if (exclusions.empty() && excludedTitle) {
										excludedTitle->hide();
									}
								},
								&st::menuIconDelete);

		_contextMenu->popup(QCursor::pos());
	};

	if (exclusion) {
		button->addClickHandler(deleteExclusionsClickHandler);
	} else if (dialogId.has_value() && _controller->showExclude.has_value() && !_controller->showExclude.value()) {
		button->addClickHandler(exclusionsClickHandler);
	} else {
		button->addClickHandler(defaultClickHandler);
	}


	crl::on_main(this,
				 [=, this]
				 {
					 adjustSize();
					 updateGeometry();
				 });
}

void AyuFiltersList::initializeSharedFilters(
	not_null<Ui::VerticalLayout*> container) {
	if (dialogId.has_value() && _controller->showExclude.has_value() && _controller->showExclude.value()) {
		filters = AyuDatabase::getByDialogId(dialogId.value());
		exclusions = AyuDatabase::getExcludedByDialogId(dialogId.value());
	} else {
		filters = AyuDatabase::getShared();


		// remove shared filters that already excluded for that peer exclusion
		if (dialogId.has_value() && _controller->showExclude.has_value() && !_controller->showExclude.value()) {
			const auto excludedForDialogId = AyuDatabase::getExcludedByDialogId(dialogId.value());

			auto rangeToRemove = std::ranges::remove_if(filters,
														[&](const RegexFilter &filter)
														{
															for (const auto &excluded : excludedForDialogId) {
																if (excluded == filter) {
																	return true;
																}
															}
															return false;
														});
			filters.erase(rangeToRemove.begin(), rangeToRemove.end());
		}
	}

	if (!filters.empty()) {
		AddSkip(container);
		filtersTitle = AddSubsectionTitle(container, tr::ayu_RegexFiltersHeader());

		for (const auto &filter : filters) {
			addNewFilter(filter);
		}
	}

	if (!exclusions.empty()) {
		if (!filters.empty()) {
			AddSkip(container);
			AddDivider(container);
			AddSkip(container);
		}

		excludedTitle = AddSubsectionTitle(container, tr::ayu_RegexFiltersExcluded());

		for (const auto &exclusion : exclusions) {
			addNewFilter(exclusion, true);
		}
	}
}

void AyuFiltersList::setupContent(not_null<Window::SessionController*> controller) {
	initializeSharedFilters(_content);

	ResizeFitChild(this, _content);
}

} // namespace Settings
