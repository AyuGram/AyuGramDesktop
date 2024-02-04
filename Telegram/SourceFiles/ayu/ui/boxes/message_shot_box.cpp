// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#include "message_shot_box.h"

#include <QFileDialog>
#include <QGuiApplication>
#include "styles/style_ayu_styles.h"

#include "lang_auto.h"
#include "theme_selector_box.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/components/image_view.h"
#include "boxes/abstract_box.h"
#include "core/core_settings.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"

MessageShotBox::MessageShotBox(
	QWidget *parent,
	AyuFeatures::MessageShot::ShotConfig config)
	: _config(std::move(config)) {
}

void MessageShotBox::prepare() {
	setupContent();
}

void MessageShotBox::setupContent() {
	_selectedPalette = std::make_shared<style::palette>();

	const auto settings = &AyuSettings::getInstance();
	const auto savedShowColorfulReplies = !settings->simpleQuotesAndReplies;

	using namespace Settings;

	AyuFeatures::MessageShot::setShotConfig(_config);

	setTitle(rpl::single(QString("Message Shot")));

	auto wrap = object_ptr<Ui::VerticalLayout>(this);
	const auto content = wrap.data();
	setInnerWidget(object_ptr<Ui::OverrideMargins>(this, std::move(wrap)));

	AddSubsectionTitle(content, tr::ayu_MessageShotPreview());

	const auto imageView = content->add(object_ptr<ImageView>(content), st::imageViewPadding);

	AddSkip(content);
	AddDivider(content);
	AddSkip(content);
	AddSubsectionTitle(content, tr::ayu_MessageShotPreferences());

	const auto updatePreview = [=]
	{
		const auto image = AyuFeatures::MessageShot::Make(this, _config);
		imageView->setImage(image);
	};

	auto selectedTheme =
		content->lifetime().make_state<rpl::variable<QString>>(tr::ayu_MessageShotThemeDefault(tr::now));

	AddButtonWithLabel(
		content,
		tr::ayu_MessageShotTheme(),
		selectedTheme->value(),
		st::settingsButtonNoIcon
	)->addClickHandler(
		[=]
		{
			AyuFeatures::MessageShot::setChoosingTheme(true);

			auto box = Box<ThemeSelectorBox>(_config.controller);
			box->paletteSelected() | rpl::start_with_next(
				[=](const style::palette &palette) mutable
				{
					_selectedPalette->reset();
					_selectedPalette->load(palette.save());

					_config.st = std::make_shared<Ui::ChatStyle>(
						_selectedPalette.get());

					updatePreview();
				},
				content->lifetime());

			box->themeNameChanged() | rpl::start_with_next(
				[=](const QString &name)
				{
					selectedTheme->force_assign(name);
				},
				content->lifetime());

			box->boxClosing() | rpl::start_with_next(
				[=]
				{
					AyuFeatures::MessageShot::setChoosingTheme(false);
				},
				content->lifetime());

			Ui::show(std::move(box), Ui::LayerOption::KeepOther);
		});

	AddButtonWithIcon(
		content,
		tr::ayu_MessageShotShowDate(),
		st::settingsButtonNoIcon
	)->toggleOn(rpl::single(false)
	)->toggledValue(
	) | start_with_next(
		[=](bool enabled)
		{
			_config.showDate = enabled;

			updatePreview();
		},
		content->lifetime());

	AddButtonWithIcon(
		content,
		tr::ayu_MessageShotShowReactions(),
		st::settingsButtonNoIcon
	)->toggleOn(rpl::single(false)
	)->toggledValue(
	) | start_with_next(
		[=](bool enabled)
		{
			_config.showReactions = enabled;

			updatePreview();
		},
		content->lifetime());

	AddButtonWithIcon(
		content,
		tr::ayu_MessageShotShowColorfulReplies(),
		st::settingsButtonNoIcon
	)->toggleOn(rpl::single(savedShowColorfulReplies)
	)->toggledValue(
	) | start_with_next(
		[=](bool enabled)
		{
			const auto settings = &AyuSettings::getInstance();
			settings->set_simpleQuotesAndReplies(!enabled);

			_config.st = std::make_shared<Ui::ChatStyle>(_config.st.get());
			updatePreview();
		},
		content->lifetime());

	AddSkip(content);

	addButton(tr::ayu_MessageShotSave(),
			  [=]
			  {
				  const auto image = imageView->getImage();
				  const auto path = QFileDialog::getSaveFileName(
					  this,
					  tr::lng_save_file(tr::now),
					  QString(),
					  "*.png");

				  if (!path.isEmpty()) {
					  image.save(path);
				  }
			  });
	addButton(tr::ayu_MessageShotCopy(),
			  [=]
			  {
				  QGuiApplication::clipboard()->setImage(imageView->getImage());
			  });

	updatePreview();

	const auto boxWidth = imageView->getImage().width() + (st::boxPadding.left() + st::boxPadding.right()) * 4;

	boxClosing() | rpl::start_with_next(
		[=]
		{
			AyuFeatures::MessageShot::resetCustomSelected();
			AyuFeatures::MessageShot::resetDefaultSelected();
			AyuFeatures::MessageShot::resetShotConfig();

			const auto settings = &AyuSettings::getInstance();
			settings->set_simpleQuotesAndReplies(!savedShowColorfulReplies);
		},
		content->lifetime());

	setDimensionsToContent(boxWidth, content);
}
