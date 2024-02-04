// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "theme_selector_box.h"

#include "lang_auto.h"
#include "ayu/features/messageshot/message_shot.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "settings/settings_chat.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/themes/window_theme.h"
#include "window/themes/window_themes_cloud_list.h"
#include "window/themes/window_theme_preview.h"

ThemeSelectorBox::ThemeSelectorBox(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: _controller(controller) {
}

rpl::producer<style::palette> ThemeSelectorBox::paletteSelected() {
	return _palettes.events();
}

rpl::producer<QString> ThemeSelectorBox::themeNameChanged() {
	return _themeNames.events();
}

void ThemeSelectorBox::prepare() {
	setupContent();
}

void ThemeSelectorBox::setupContent() {
	using namespace Settings;

	setTitle(tr::ayu_MessageShotThemeSelectTitle());

	auto wrap2 = object_ptr<Ui::VerticalLayout>(this);
	const auto container = wrap2.data();

	setInnerWidget(object_ptr<Ui::OverrideMargins>(
		this,
		std::move(wrap2)));

	AddSubsectionTitle(container, tr::lng_settings_themes());
	AddSkip(container, st::settingsThemesTopSkip);

	Settings::SetupDefaultThemes(&_controller->window(), container);

	AddSkip(container);

	using namespace Window::Theme;
	using namespace rpl::mappers;

	const auto wrap = container->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			container,
			object_ptr<Ui::VerticalLayout>(container))
	)->setDuration(0);
	const auto inner = wrap->entity();

	AddDivider(inner);
	AddSkip(inner);

	const auto title = AddSubsectionTitle(
		inner,
		tr::lng_settings_bg_cloud_themes());
	const auto showAll = Ui::CreateChild<Ui::LinkButton>(
		inner,
		tr::lng_settings_bg_show_all(tr::now));

	rpl::combine(
		title->topValue(),
		inner->widthValue(),
		showAll->widthValue()
	) | rpl::start_with_next([=](int top, int outerWidth, int width)
							 {
								 showAll->moveToRight(
									 st::defaultSubsectionTitlePadding.left(),
									 top,
									 outerWidth);
							 },
							 showAll->lifetime());

	Ui::AddSkip(inner, st::settingsThemesTopSkip);

	const auto list = inner->lifetime().make_state<CloudList>(
		inner,
		_controller);
	inner->add(
		list->takeWidget(),
		style::margins(
			st::settingsButtonNoIcon.padding.left(),
			0,
			st::settingsButtonNoIcon.padding.right(),
			0));

	list->allShown(
	) | rpl::start_with_next([=](bool shown)
							 {
								 showAll->setVisible(!shown);
							 },
							 showAll->lifetime());

	showAll->addClickHandler([=]
	{
		list->showAll();
	});

	wrap->setDuration(0)->toggleOn(list->empty() | rpl::map(!_1));

	_controller->session().data().cloudThemes().refresh();

	AyuFeatures::MessageShot::themeChosen(
	) | rpl::start_with_next(
		[=](Data::CloudTheme theme)
		{
			const auto document = _controller->session().data().document(theme.documentId);
			const auto documentView = document->createMediaView();

			document->save(
				Data::FileOriginTheme(theme.id, theme.accessHash),
				QString());

			const auto innerCallback = [=]
			{
				auto preview = Window::Theme::PreviewFromFile(
					documentView->bytes(),
					document->location().name(),
					theme);

				_selectedPalette = preview->instance.palette;

				auto name = theme.title;
				_themeNames.fire(std::move(name));
			};

			if (documentView->loaded()) {
				innerCallback();
			} else {
				_controller->session().downloaderTaskFinished(
				) | rpl::filter(
					[=]
					{
						return documentView->loaded();
					}) | rpl::start_with_next(
					[=]
					{
						innerCallback();
					},
					lifetime());
			}
		},
		lifetime());

	AyuFeatures::MessageShot::paletteChosen(
	) | rpl::start_with_next([=](const auto &palette)
							 {
								 _themeNames.fire(tr::ayu_MessageShotThemeDefault(tr::now));
								 _selectedPalette = palette;
							 },
							 lifetime());

	addButton(tr::ayu_MessageShotThemeApply(),
			  [=]
			  {
				  _palettes.fire(std::move(_selectedPalette));
				  closeBox();
			  });

	setDimensionsToContent(st::boxWidth, container);
}
