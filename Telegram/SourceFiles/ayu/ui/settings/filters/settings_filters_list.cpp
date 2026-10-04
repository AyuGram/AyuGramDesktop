// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/filters/settings_filters_list.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "ayu/features/filters/filters_cache_controller.h"
#include "ayu/features/filters/filters_utils.h"
#include "ayu/ui/components/icon_picker.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/filters/edit_filter.h"
#include "ayu/ui/settings/filters/per_dialog_filter.h"
#include "ayu/utils/telegram_helpers.h"
#include "boxes/connection_box.h"
#include "data/data_channel.h"
#include "info/info_wrap_widget.h"
#include "settings/settings_common.h"
#include "storage/localstorage.h"
#include "styles/style_boxes.h"
#include "styles/style_media_view.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"
#include "ui/qt_object_factory.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

rpl::producer<QString> AyuFiltersList::title() {
	if (shadowBan) {
		return tr::ayu_FiltersShadowBan();
	}
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
	: Section(parent, controller), _controller(controller), _content(Ui::CreateChild<Ui::VerticalLayout>(this)),
	  shadowBan(_controller->shadowBan) {
	if (_controller->dialogId.has_value()) {
		dialogId = _controller->dialogId.value();
	}

	setupContent(controller);
}

void AyuFiltersList::checkBeforeClose(Fn<void()> close) {
	_controller->showExclude = true;
	_controller->shadowBan = false;
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

	auto defaultClickHandler = [=, dialogId = dialogId]() mutable
	{
		auto _contextMenu = new Ui::PopupMenu(this, st::popupMenuWithIcons);
		_contextMenu->setAttribute(Qt::WA_DeleteOnClose);

		_contextMenu->addAction(
			tr::lng_theme_edit(tr::now),
			[=]
			{
				_controller->show(
					RegexEditBox(
						state,
						nullptr,
						dialogId
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
				FiltersCacheController::fireUpdate();
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
				FiltersCacheController::fireUpdate();
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
		FiltersCacheController::fireUpdate();

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
				FiltersCacheController::fireUpdate();
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
			AddSectionDivider(container);
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

void AyuFiltersList::initializeShadowBan(not_null<Ui::VerticalLayout*> container) {
	auto ctrl = container->lifetime().make_state<PerDialogFiltersListController>(
		&_controller->session(),
		_controller,
		true // shadowBan
	);

	auto list = object_ptr<Ui::PaddingWrap<PeerListContent>>(
		container,
		object_ptr<PeerListContent>(
			container,
			ctrl),
		QMargins(0, -st::peerListBox.padding.top(), 0, -st::peerListBox.padding.bottom()));

	// delegate is not initialized at this moment
	if (AyuSettings::getInstance().shadowBanIds().size() > 0) {
		AddSkip(container);

		filtersTitle = AddSubsectionTitle(container, tr::ayu_RegexFiltersHeader());
		const auto content = container->add(std::move(list));

		AddSkip(container);

		auto delegate = container->lifetime().make_state<PeerListContentDelegateSimple>();
		delegate->setContent(content->entity());
		ctrl->setDelegate(delegate);
	} else {
		Ui::AddDividerText(container, tr::ayu_RegexFiltersListEmpty());
	}
}

void AyuFiltersList::setupContent(not_null<Window::SessionController*> controller) {
	if (shadowBan) {
		initializeShadowBan(_content);
	} else {
		initializeSharedFilters(_content);
	}

	ResizeFitChild(this, _content);
}

} // namespace Settings
