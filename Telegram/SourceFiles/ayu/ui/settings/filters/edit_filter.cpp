// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "edit_filter.h"

#include <styles/style_layers.h>
#include <ui/widgets/fields/masked_input_field.h>
#include <styles/style_window.h>
#include <ui/toast/toast.h>
#include <ui/text/text_utilities.h>

#include "ayu/ayu_settings.h"

#include "lang_auto.h"

#include "boxes/connection_box.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"

#include "ayu/data/ayu_database.h"
#include "ayu/features/filters/filters_cache_controller.h"
#include "ayu/utils/telegram_helpers.h"
#include "ui/qt_object_factory.h"
#include "ui/ui_utility.h"
#include "ui/vertical_list.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"

#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "base/event_filter.h"
#include "ui/effects/animations.h"
#include "styles/style_window.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/text/text.h"
#include "media/view/media_view_overlay_widget.h"

#include "apiwrap.h"
#include "api/api_attached_stickers.h"
#include "api/api_peer_photo.h"
#include "base/qt/qt_common_adapters.h"
#include "base/timer_rpl.h"
#include "lang/lang_keys.h"
#include "menu/menu_sponsored.h"
#include "boxes/premium_preview_box.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "core/file_utilities.h"
#include "core/mime_type.h"
#include "core/ui_integration.h"
#include "core/crash_reports.h"
#include "core/sandbox.h"
#include "core/shortcuts.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/dropdown_menu.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/buttons.h"
#include "ui/layers/layer_manager.h"
#include "ui/text/text_utilities.h"
#include "ui/platform/ui_platform_window_title.h"
#include "ui/toast/toast.h"
#include "ui/text/format_values.h"
#include "ui/item_text_options.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/power_saving.h"
#include "ui/cached_round_corners.h"
#include "ui/gl/gl_window.h"
#include "ui/boxes/confirm_box.h"
#include "ui/ui_utility.h"
#include "info/info_memento.h"
#include "info/info_controller.h"
#include "info/statistics/info_statistics_widget.h"
#include "boxes/delete_messages_box.h"
#include "boxes/report_messages_box.h"
#include "media/audio/media_audio.h"
#include "media/view/media_view_group_thumbs.h"
#include "media/view/media_view_pip.h"
#include "media/view/media_view_overlay_raster.h"
#include "media/view/media_view_overlay_opengl.h"
#include "media/view/media_view_playback_sponsored.h"
#include "media/stories/media_stories_share.h"
#include "media/stories/media_stories_view.h"
#include "media/streaming/media_streaming_document.h"
#include "media/streaming/media_streaming_player.h"
#include "media/player/media_player_instance.h"
#include "history/history.h"
#include "history/history_item_helpers.h"
#include "history/view/media/history_view_media.h"
#include "history/view/reactions/history_view_reactions_selector.h"
#include "data/components/sponsored_messages.h"
#include "data/data_session.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_user.h"
#include "data/data_media_rotation.h"
#include "data/data_photo_media.h"
#include "data/data_document_media.h"
#include "data/data_document_resolver.h"
#include "data/data_file_click_handler.h"
#include "data/data_download_manager.h"
#include "window/themes/window_theme_preview.h"
#include "window/window_peer_menu.h"
#include "window/window_controller.h"
#include "base/platform/base_platform_info.h"
#include "base/power_save_blocker.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "base/qt_signal_producer.h"
#include "base/event_filter.h"
#include "main/main_account.h"
#include "main/main_domain.h" // Domain::activeSessionValue.
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "layout/layout_document_generic_preview.h"
#include "platform/platform_overlay_widget.h"
#include "storage/file_download.h"
#include "storage/storage_account.h"
#include "calls/calls_instance.h"
#include "styles/style_media_view.h"
#include "styles/style_calls.h"
#include "styles/style_chat.h"
#include "styles/style_menu_icons.h"


#include <QtWidgets/QApplication>
#include <QtCore/QBuffer>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtGui/QScreen>

#include <kurlmimedata.h>

class PainterHighQualityEnabler;

namespace Settings
{

std::vector<char> generate_uuid_bytes()
{
	// stolen somewhere from Internet
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint32_t> dist;

	std::vector<uint8_t> bytes(16);
	for (int i = 0; i < 16; i += 4) {
		uint32_t random_chunk = dist(gen);
		bytes[i] = random_chunk & 0xFF;
		bytes[i + 1] = (random_chunk >> 8) & 0xFF;
		bytes[i + 2] = (random_chunk >> 16) & 0xFF;
		bytes[i + 3] = (random_chunk >> 24) & 0xFF;
	}
	bytes[6] = (bytes[6] & 0x0F) | 0x40;
	bytes[8] = (bytes[8] & 0x3F) | 0x80;

	return std::vector<char>(bytes.begin(), bytes.end());
}

rpl::producer<QString> AyuEditFilters::title()
{
	return tr::ayu_RegexFiltersAdd();
}

object_ptr<Ui::Checkbox> getCheckBox(not_null<Ui::VerticalLayout *> container,
									 const QString &label,
									 bool checked)
{
	return object_ptr<Ui::Checkbox>(
		container,
		label,
		checked,
		st::settingsCheckbox);
}

AyuEditFilters::AyuEditFilters(
	QWidget *parent,
	not_null<Window::SessionController *> controller)
	: Section(parent)
{
	if (!controller->filterId.empty()) {
		currentFilter = AyuDatabase::getById(controller->filterId);
	}
	setupContent(controller, parent);
}

// unused currently, TODO: need to add this on bad regex pattern
not_null<Ui::FlatLabel *> AddError(
	not_null<Ui::VerticalLayout *> content,
	Ui::PasswordInput *input)
{
	const auto error = content->add(
		object_ptr<Ui::CenterWrap<Ui::FlatLabel>>(
			content,
			object_ptr<Ui::FlatLabel>(
				content,
				// Set any text to resize.
				tr::lng_language_name(tr::now),
				st::settingLocalPasscodeError)),
		st::changePhoneDescriptionPadding)->entity();
	error->hide();
	if (input) {
		QObject::connect(input,
						 &Ui::MaskedInputField::changed,
						 [=]
						 {
							 error->hide();
						 });
	}
	return error;
};

void AyuEditFilters::setupSettings(not_null<Ui::VerticalLayout *> container, QWidget *parent)
{
	AddSkip(container);

	const auto add = [&](const QString &label, bool checked, auto &&handle)
	{
		auto check = container->add(
			getCheckBox(container, label, checked),
			st::settingsCheckboxPadding
		);
		check->checkedChanges(
		) | rpl::start_with_next(
			std::forward<decltype(handle)>(handle),
			container->lifetime());
		return check;
	};


	const auto name = container->add(
		object_ptr<Ui::InputField>(
			parent,
			st::windowFilterNameInput,
			Ui::InputField::Mode::MultiLine,
			tr::ayu_RegexFiltersPlaceholder()),
		st::markdownLinkFieldPadding);


	auto enabled = add(
		QString("Enable Filter"),
		currentFilter.enabled,
		[=, this](bool checked)
		{
			currentFilter.enabled = checked;
		});

	auto insensetive = add(
		QString("Case Insensitive"),
		currentFilter.caseInsensitive,
		[=, this](bool checked)
		{
			currentFilter.caseInsensitive = checked;
		});

	auto reversed = add(
		QString("Reversed"),
		currentFilter.reversed,
		[=, this](bool checked)
		{
			currentFilter.reversed = checked;
		});

	name->setText(QString::fromStdString(currentFilter.text));
	name->submits(
	) | rpl::start_with_next([=, this]
							 {
								 currentFilter.text = name->getTextWithTags().text.toStdString();

								 currentFilter.enabled = enabled->checked();
								 currentFilter.caseInsensitive = insensetive->checked();
								 currentFilter.reversed = reversed->checked();

								 if (currentFilter.id.empty()) {
									 currentFilter.id = generate_uuid_bytes();
								 }
								 AyuDatabase::addRegexFilter(currentFilter);

								 FiltersCacheController::rebuildCache();
							 },
							 name->lifetime());
}

void RegexEditBuilder(
	not_null<Ui::GenericBox *> box,
	RegexFilter *filter,
	const Fn<void(RegexFilter)> &onDone,
	std::optional<long long> dialogId,
	bool showToast
)
{
	RegexFilter data;

	if (filter) {
		box->setTitle(tr::ayu_RegexFiltersEdit());
		data = *filter;
	} else {
		box->setTitle(tr::ayu_RegexFiltersAdd());
		data.reversed = false;
	}

	const auto name = box->addRow(
		object_ptr<Ui::InputField>(
			box->verticalLayout(),
			st::windowFilterNameInput,
			Ui::InputField::Mode::MultiLine,
			tr::ayu_RegexFiltersPlaceholder()),
		st::markdownLinkFieldPadding);
	const auto enabled = box->addRow(
		object_ptr<Ui::Checkbox>(
			box,
			tr::ayu_EnableExpression(tr::now),
			data.enabled,
			st::defaultBoxCheckbox),
		st::settingsCheckboxPadding);
	const auto caseInsensitive = box->addRow(
		object_ptr<Ui::Checkbox>(
			box,
			tr::ayu_CaseInsensitiveExpression(tr::now),
			data.caseInsensitive,
			st::defaultBoxCheckbox),
		st::settingsCheckboxPadding);
	const auto reversed = box->addRow(
		object_ptr<Ui::Checkbox>(
			box,
			tr::ayu_ReversedExpression(tr::now),
			data.reversed,
			st::defaultBoxCheckbox),
		st::settingsCheckboxPadding);

	name->setText(QString::fromStdString(data.text));

	auto saveAndClose = [=, id = data.id]
	{
		RegexFilter newFilter;
		newFilter.text = name->getTextWithTags().text.toStdString();
		newFilter.enabled = enabled->checked();
		newFilter.caseInsensitive = caseInsensitive->checked();
		newFilter.reversed = reversed->checked();

		if (!showToast && dialogId.has_value()) {
			newFilter.dialogId = dialogId;
		}

		if (!id.empty()) {
			newFilter.id = id;
		} else {
			newFilter.id = generate_uuid_bytes();
		}

		box->closeBox();

		crl::async([=]
				   {
					   AyuDatabase::addRegexFilter(newFilter);
					   FiltersCacheController::rebuildCache();

					   crl::on_main([=]
									{
										if (onDone) {
											onDone(newFilter);
										}
										AyuSettings::fire_filtersUpdate();

										if (showToast) {
											const auto onClick = [=](const auto &...) mutable{
												newFilter.dialogId = dialogId;

												AyuDatabase::updateRegexFilter(newFilter);
												FiltersCacheController::rebuildCache();
												AyuSettings::fire_filtersUpdate();

												return true;
											};
											Ui::Toast::Show(Ui::Toast::Config{
												// .text = tr::ayu_RegexFilterBulletinText(
												// 	tr::now,
												// 	lt_link,
												// 	Ui::Text::Link(
												// 		Ui::Text::Bold(
												// 			tr::ayu_RegexFilterBulletinAction(tr::now))),
												// 	Ui::Text::RichLangValue),

												// TODO: reconsider
												.text = tr::ayu_RegexFilterBulletinText(
													tr::now
													//,
//													lt_link,
//													Ui::Text::Link(
//														Ui::Text::Bold(
//															tr::ayu_RegexFilterBulletinAction(tr::now))),
//													Ui::Text::WithEntities
													),
												.filter = onClick,
												.adaptive = true
											});
										}
									});
				   });
	};

	name->submits() | rpl::start_with_next(saveAndClose, name->lifetime());
	box->addButton(tr::lng_settings_save(), saveAndClose);
	box->addButton(tr::lng_cancel(), [=]
	{ box->closeBox(); });
}

object_ptr<Ui::GenericBox> RegexEditBox(RegexFilter *filter,
										const Fn<void(RegexFilter)> &onDone,
										std::optional<long long> dialogId,
										bool showToast)
{
	return Box(RegexEditBuilder, filter, onDone, dialogId, showToast);
}

void AyuEditFilters::setupContent(not_null<Window::SessionController *> controller, QWidget *parent)
{
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	setupSettings(content, parent);

	ResizeFitChild(this, content);
}

} // namespace Settings
