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
#include "info/info_wrap_widget.h"
#include "ui/qt_object_factory.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
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

void AyuFiltersList::checkBeforeClose(Fn<void()> close) {
	_controller->showExclude = true;
	close();
}

void AyuFiltersList::addNewFilter(const RegexFilter &filter, bool exclusion) {
	const auto state = lifetime().make_state<RegexFilter>(filter);
	const auto button = _content->add(
	object_ptr<Button>(
			_content,
			rpl::single(QString::fromStdString(state->text).replace("\n", " ")),
			st::settingsButtonNoIcon
		)
	);

	if (!state->enabled) {
		button->setColorOverride(st::storiesComposeGrayText->c);
	}

	auto defaultClickHandler = [=, this]() mutable
	{
		auto _contextMenu = new Ui::PopupMenu(this, st::popupMenuWithIcons);
		_contextMenu->setAttribute(Qt::WA_DeleteOnClose);

		_contextMenu->addAction(
			tr::lng_theme_edit(tr::now),
			[=, this]
			{
				_controller->show(
					RegexEditBox(
						state,
						nullptr
						));
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
			},
			state->enabled ? &st::menuIconBlock : &st::menuIconUnblock);

		_contextMenu->addSeparator();

		_contextMenu->addAction(
			tr::lng_theme_delete(tr::now),
			[=]
			{
				AyuDatabase::deleteFilter(state->id);
				AyuDatabase::deleteExclusionsByFilterId(state->id);
				FiltersCacheController::rebuildCache();
				AyuSettings::fire_filtersUpdate();
			},
			&st::menuIconDelete);

		_contextMenu->popup(QCursor::pos());
	};

	// we've opened filters list from top "Exclude" button
	// on click, close the section
	auto exclusionsClickHandler = [=, controller = _controller, dialogId = dialogId]() mutable
	{
		Expects(dialogId.has_value());

		/*
		└── class Info::WrapWidget
			└── class Info::Settings::Widget
				└── class Ui::ScrollArea
					└── class QWidget
						└── class Ui::PaddingWrap<class Ui::RpWidget>
							└── class Settings::AyuFiltersList
		 */
		// controller->showBackFromStack() doesn't work (closes box completely)
		// so as a workaround, use WrapWidget
		const auto wrap = dynamic_cast<Info::WrapWidget*>(parent()->parent()->parent()->parent()->parent());

		const RegexFilterGlobalExclusion newExclusion = {
			.dialogId = dialogId.value(),
			.filterId = state->id
		};

		AyuDatabase::addRegexExclusion(newExclusion);
		FiltersCacheController::rebuildCache();
		AyuSettings::fire_filtersUpdate();

		controller->dialogId = dialogId;
		controller->showExclude = true;

		wrap->showBackFromStackInternal(Window::SectionShow(anim::type::normal));
	};
	auto deleteExclusionsClickHandler = [=, this]() mutable
	{
		auto _contextMenu = new Ui::PopupMenu(this, st::popupMenuWithIcons);
		_contextMenu->setAttribute(Qt::WA_DeleteOnClose);

		_contextMenu->addAction(
			tr::lng_theme_delete(tr::now),
			[=, this]
			{
				Expects(dialogId.has_value());

				AyuDatabase::deleteExclusion(dialogId.value(), state->id);
				FiltersCacheController::rebuildCache();
				AyuSettings::fire_filtersUpdate();
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


	crl::on_main(
		this,
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

			auto rangeToRemove = std::ranges::remove_if(
				filters,
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

	if (filters.empty() && exclusions.empty()) {
		Ui::AddDividerText(container, tr::ayu_RegexFiltersListEmpty());
	}
}

void AyuFiltersList::setupContent(not_null<Window::SessionController*> controller) {
	initializeSharedFilters(_content);

	ResizeFitChild(this, _content);
}

} // namespace Settings
