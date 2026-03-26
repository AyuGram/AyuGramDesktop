// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/message_shot_box.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/theme_selector_box.h"
#include "ayu/ui/components/image_view.h"
#include "ayu/utils/telegram_helpers.h"
#include "boxes/abstract_box.h"
#include "data/data_todo_list.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"

#include <memory>
#include <QFileDialog>
#include <QGuiApplication>

MessageShotBox::MessageShotBox(
	QWidget *parent,
	AyuFeatures::MessageShot::ShotConfig config)
	: _config(std::move(config)) {
}

void MessageShotBox::prepare() {
	setupContent();
}

void MessageShotBox::setupContent() {
	_selectedPalette = AyuFeatures::MessageShot::getPersistedPalette();
	if (!_selectedPalette) {
		_selectedPalette = std::make_shared<style::palette>();
	}
	AyuFeatures::MessageShot::setPersistedPalette(_selectedPalette);

	AyuFeatures::MessageShot::ensureChatThemesRefreshed();

	auto &settings = AyuSettings::getInstance();
	auto &shotSettings = settings.messageShotSettings();
	const auto savedSimpleQuotesAndReplies = settings.simpleQuotesAndReplies();
	settings.setSimpleQuotesAndReplies(!shotSettings.showColorfulReplies());

	using namespace Settings;

	auto savedThemeApplyResult = AyuFeatures::MessageShot::SavedThemeApplyResult::Failed;
	const auto hasSavedTheme = shotSettings.embeddedThemeType() != -1
		|| shotSettings.cloudThemeId() != 0;
	if (hasSavedTheme) {
		savedThemeApplyResult = AyuFeatures::MessageShot::applySavedThemePalette(
			_selectedPalette,
			nullptr);
		if (savedThemeApplyResult != AyuFeatures::MessageShot::SavedThemeApplyResult::Failed) {
			_config.st = std::make_shared<Ui::ChatStyle>(_selectedPalette.get());
		} else {
			shotSettings.clearTheme();
			_config.st = std::make_shared<Ui::ChatStyle>(_config.controller->chatStyle());
		}
	}

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

	auto hasReactions = false;
	auto hasReplies = false;
	auto hasSpoilers = false;
	for (const auto &item : _config.messages) {
		if (!hasReactions && !item->reactions().empty()) {
			hasReactions = true;
		}
		if (!hasReplies) {
			if (item->replyTo().replying()
				|| (item->media() && item->media()->webpage())) {
				hasReplies = true;
			}
		}
		if (!hasSpoilers) {
			for (const auto &entity : item->originalText().entities) {
				if (entity.type() == EntityType::Spoiler) {
					hasSpoilers = true;
					break;
				}
			}
			if (!hasSpoilers && item->media()) {
				if (item->media()->hasSpoiler()) {
					hasSpoilers = true;
				} else if (const auto todoList = item->media()->todolist()) {
					for (const auto &entity : todoList->title.entities) {
						if (entity.type() == EntityType::Spoiler) {
							hasSpoilers = true;
							break;
						}
					}
					if (!hasSpoilers) {
						for (const auto &task : todoList->items) {
							for (const auto &entity : task.text.entities) {
								if (entity.type() == EntityType::Spoiler) {
									hasSpoilers = true;
									break;
								}
							}
							if (hasSpoilers) break;
						}
					}
				}
			}
		}
		if (hasReactions && hasReplies && hasSpoilers) {
			break;
		}
	}

	const auto firstPreviewLatch = std::make_shared<TimedCountDownLatch>(1);
	const auto generation = content->lifetime().make_state<int>(0);
	const auto weak = base::make_weak(this);

	const auto updatePreview = [=]
	{
		const auto currentGeneration = ++(*generation);
		AyuFeatures::MessageShot::Make(this, _config, [=](const QImage &image, bool final)
		{
			if (!weak || currentGeneration != *generation) {
				return;
			}

			if (final || imageView->getImage().isNull()) {
				imageView->setImage(image);
			}
			firstPreviewLatch->countDown();
		});
	};

	if (savedThemeApplyResult == AyuFeatures::MessageShot::SavedThemeApplyResult::AwaitingAsync) {
		const auto weakBox = base::make_weak(this);
		AyuFeatures::MessageShot::subscribeToCloudThemeLoad(
			_config.controller,
			_selectedPalette,
			[=] {
				if (!weakBox) {
					return;
				}
				_config.st = std::make_shared<Ui::ChatStyle>(_selectedPalette.get());
				updatePreview();
			});
	}

	auto selectedTheme =
		content->lifetime().make_state<rpl::variable<QString>>(
			AyuFeatures::MessageShot::resolveThemeName());

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
			box->paletteSelected() | rpl::on_next(
				[=](const style::palette &palette) mutable
				{
					_selectedPalette->reset();
					_selectedPalette->load(palette.save());

					_config.st = std::make_shared<Ui::ChatStyle>(_selectedPalette.get());

					auto &shot = AyuSettings::getInstance().messageShotSettings();
					const auto embedded = AyuFeatures::MessageShot::getSelectedFromDefault();
					const auto cloud = AyuFeatures::MessageShot::getSelectedFromCustom();
					if (cloud.has_value()) {
						const auto accountId = _config.controller->session().userId().bare;
						shot.setCloudTheme(accountId, cloud->id, cloud->accessHash, cloud->documentId, cloud->title);
					} else if (embedded != Window::Theme::EmbeddedType(-1)) {
						const auto color = AyuFeatures::MessageShot::getSelectedColorFromDefault();
						shot.setEmbeddedTheme(static_cast<int>(embedded), color ? color->rgb() : 0);
					} else {
						shot.clearTheme();
					}

					updatePreview();
				},
				content->lifetime());

			box->themeNameChanged() | rpl::on_next(
				[=](const QString &name)
				{
					selectedTheme->force_assign(name);
				},
				content->lifetime());

			box->boxClosing() | rpl::on_next(
				[=]
				{
					AyuFeatures::MessageShot::setChoosingTheme(false);
				},
				content->lifetime());

			Ui::show(std::move(box), Ui::LayerOption::KeepOther);
		});
	AddButtonWithIcon(
		content,
		tr::ayu_MessageShotShowBackground(),
		st::settingsButtonNoIcon
	)->toggleOn(rpl::single(shotSettings.showBackground())
	)->toggledValue(
	) | rpl::skip(1) | on_next(
		[=](bool enabled)
		{
			AyuSettings::getInstance().messageShotSettings().setShowBackground(enabled);
			updatePreview();
		},
		content->lifetime());

	auto latestToggle = AddButtonWithIcon(
		content,
		tr::ayu_MessageShotShowDate(),
		st::settingsButtonNoIcon
	);
	latestToggle->toggleOn(rpl::single(shotSettings.showDate())
	)->toggledValue(
	) | rpl::skip(1) | on_next(
		[=](bool enabled)
		{
			AyuSettings::getInstance().messageShotSettings().setShowDate(enabled);
			updatePreview();
		},
		content->lifetime());

	if (hasReactions) {
		latestToggle = AddButtonWithIcon(
			content,
			tr::ayu_MessageShotShowReactions(),
			st::settingsButtonNoIcon
		);
		latestToggle->toggleOn(rpl::single(shotSettings.showReactions())
		)->toggledValue(
		) | rpl::skip(1) | on_next(
			[=](bool enabled)
			{
				AyuSettings::getInstance().messageShotSettings().setShowReactions(enabled);
				updatePreview();
			},
			content->lifetime());
	}

	if (hasReplies) {
		latestToggle = AddButtonWithIcon(
			content,
			tr::ayu_MessageShotShowColorfulReplies(),
			st::settingsButtonNoIcon
		);
		latestToggle->toggleOn(rpl::single(shotSettings.showColorfulReplies())
		)->toggledValue(
		) | rpl::skip(1) | on_next(
			[=](bool enabled)
			{
				auto &currentSettings = AyuSettings::getInstance();
				currentSettings.messageShotSettings().setShowColorfulReplies(enabled);
				currentSettings.setSimpleQuotesAndReplies(!enabled);

				_config.st = std::make_shared<Ui::ChatStyle>(_config.st.get());
				updatePreview();
			},
			content->lifetime());
	}

	if (hasSpoilers) {
		latestToggle = AddButtonWithIcon(
			content,
			tr::ayu_MessageShotRevealSpoilers(),
			st::settingsButtonNoIcon
		);
		latestToggle->toggleOn(rpl::single(shotSettings.revealSpoilers())
		)->toggledValue(
		) | rpl::skip(1) | on_next(
			[=](bool enabled)
			{
				AyuSettings::getInstance().messageShotSettings().setRevealSpoilers(enabled);
				updatePreview();
			},
			content->lifetime());
	}

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

			  	  _tookShot = true;
				  closeBox();
			  });
	addButton(tr::ayu_MessageShotCopy(),
			  [=]
			  {
				  QGuiApplication::clipboard()->setImage(imageView->getImage());

			  	  _tookShot = true;
				  closeBox();
			  });

	updatePreview();
	firstPreviewLatch->await(std::chrono::seconds(1));

	const auto boxWidth = imageView->getImage().width() / style::DevicePixelRatio() + (st::boxPadding.left() + st::boxPadding.right()) * 4;

	boxClosing() | rpl::on_next(
		[=]
		{
			AyuFeatures::MessageShot::resetCustomSelected();
			AyuFeatures::MessageShot::resetDefaultSelected();
			AyuFeatures::MessageShot::resetShotConfig();

			AyuSettings::getInstance().setSimpleQuotesAndReplies(savedSimpleQuotesAndReplies);
		},
		content->lifetime());

	setDimensionsToContent(boxWidth, content);

	scrollToWidget(latestToggle);
}
