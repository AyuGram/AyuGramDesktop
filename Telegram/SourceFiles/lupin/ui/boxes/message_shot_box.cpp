// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/ui/boxes/message_shot_box.h"

#include "lang_auto.h"
#include "lupin/lupin_settings.h"
#include "lupin/ui/boxes/theme_selector_box.h"
#include "lupin/ui/components/image_view.h"
#include "lupin/utils/telegram_helpers.h"
#include "boxes/abstract_box.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_todo_list.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item_components.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "styles/style_lupin_styles.h"
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
	LupinFeatures::MessageShot::ShotConfig config)
	: _config(std::move(config)) {
}

void MessageShotBox::prepare() {
	setupContent();
}

void MessageShotBox::setupContent() {
	_selectedPalette = LupinFeatures::MessageShot::getPersistedPalette();
	if (!_selectedPalette) {
		_selectedPalette = std::make_shared<style::palette>();
	}
	LupinFeatures::MessageShot::setPersistedPalette(_selectedPalette);

	LupinFeatures::MessageShot::ensureChatThemesRefreshed();

	auto &settings = LupinSettings::getInstance();
	auto &shotSettings = settings.messageShotSettings();
	const auto savedSimpleQuotesAndReplies = settings.simpleQuotesAndReplies();
	settings.setSimpleQuotesAndReplies(!shotSettings.showColorfulReplies());

	using namespace Settings;

	auto savedThemeApplyResult = LupinFeatures::MessageShot::SavedThemeApplyResult::Failed;
	const auto hasSavedTheme = shotSettings.embeddedThemeType() != -1
		|| shotSettings.cloudThemeId() != 0;
	if (hasSavedTheme) {
		savedThemeApplyResult = LupinFeatures::MessageShot::applySavedThemePalette(
			_selectedPalette,
			nullptr);
		if (savedThemeApplyResult != LupinFeatures::MessageShot::SavedThemeApplyResult::Failed) {
			_config.st = std::make_shared<Ui::ChatStyle>(_selectedPalette.get());
		} else {
			shotSettings.clearTheme();
			_config.st = std::make_shared<Ui::ChatStyle>(_config.controller->chatStyle());
		}
	}

	LupinFeatures::MessageShot::setShotConfig(_config);

	setTitle(rpl::single(tr::lupin_MessageShotTopBarText(tr::now)));

	auto wrap = object_ptr<Ui::VerticalLayout>(this);
	const auto content = wrap.data();
	setInnerWidget(object_ptr<Ui::OverrideMargins>(this, std::move(wrap)));

	AddSubsectionTitle(content, tr::lupin_MessageShotPreview());

	const auto imageView = content->add(object_ptr<ImageView>(content), st::imageViewPadding);

	AddSkip(content);
	AddDivider(content);
	AddSkip(content);
	AddSubsectionTitle(content, tr::lupin_MessageShotPreferences());

	auto hasReactions = false;
	auto hasReplies = false;
	auto hasHeaderDecorations = false;
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
		if (!hasHeaderDecorations) {
			const auto drawChannelBadge = [&] {
				if (item->isDiscussionPost()) {
					return true;
				} else if (item->author()->isMegagroup()) {
					if (const auto signedInfo = item->Get<HistoryMessageSigned>()) {
						if (!signedInfo->viaBusinessBot) {
							return false;
						}
					}
				}
				return item->history()->peer->isMegagroup()
					&& item->author()->isChannel()
					&& !item->out();
			}();

			auto badgeText = QString();
			if (item->isDiscussionPost()) {
				badgeText = tr::lng_channel_badge(tr::now);
			} else if (item->author()->isMegagroup()) {
				if (const auto signedInfo = item->Get<HistoryMessageSigned>()) {
					if (!signedInfo->viaBusinessBot) {
						badgeText = signedInfo->author;
					}
				}
			} else if (drawChannelBadge) {
				badgeText = tr::lng_channel_badge(tr::now);
			} else if (const auto chat = item->history()->peer->asChat()) {
				if (const auto user = item->author()->asUser()) {
					const auto rank = chat->memberRanks.find(peerToUser(user->id));
					if (rank != chat->memberRanks.end()) {
						badgeText = rank->second;
					}
				}
			} else if (const auto channel = item->history()->peer->asMegagroup()) {
				if (const auto user = item->author()->asUser()) {
					const auto info = channel->mgInfo.get();
					const auto userId = peerToUser(user->id);
					const auto isCreator = info && (info->creator == user);
					const auto isAdmin = info && info->admins.contains(userId);
					if (isCreator || isAdmin) {
						const auto rank = info->memberRanks.find(userId);
						if (rank != info->memberRanks.end()
							&& !rank->second.isEmpty()) {
							badgeText = rank->second;
						} else if (isCreator) {
							badgeText = tr::lng_owner_badge(tr::now);
						} else {
							badgeText = tr::lng_admin_badge(tr::now);
						}
					} else {
						badgeText = item->fromRank();
					}
				}
			}

			hasHeaderDecorations = drawChannelBadge
				|| !badgeText.isEmpty()
				|| (item->boostsApplied() > 0);
		}
		if (hasReactions
			&& hasReplies
			&& hasHeaderDecorations
			&& hasSpoilers) {
			break;
		}
	}

	const auto firstPreviewLatch = std::make_shared<TimedCountDownLatch>(1);
	const auto generation = content->lifetime().make_state<int>(0);
	const auto weak = base::make_weak(this);

	const auto updatePreview = [=]
	{
		const auto currentGeneration = ++(*generation);
		LupinFeatures::MessageShot::Make(this, _config, [=](const QImage &image, bool final)
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

	if (savedThemeApplyResult == LupinFeatures::MessageShot::SavedThemeApplyResult::AwaitingAsync) {
		const auto weakBox = base::make_weak(this);
		LupinFeatures::MessageShot::subscribeToCloudThemeLoad(
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
			LupinFeatures::MessageShot::resolveThemeName());

	AddButtonWithLabel(
		content,
		tr::lupin_MessageShotTheme(),
		selectedTheme->value(),
		st::settingsButtonNoIcon
	)->addClickHandler(
		[=]
		{
			LupinFeatures::MessageShot::setChoosingTheme(true);

			auto box = Box<ThemeSelectorBox>(_config.controller);
			box->paletteSelected() | rpl::on_next(
				[=](const style::palette &palette) mutable
				{
					_selectedPalette->reset();
					_selectedPalette->load(palette.save());

					_config.st = std::make_shared<Ui::ChatStyle>(_selectedPalette.get());

					auto &shot = LupinSettings::getInstance().messageShotSettings();
					const auto embedded = LupinFeatures::MessageShot::getSelectedFromDefault();
					const auto cloud = LupinFeatures::MessageShot::getSelectedFromCustom();
					if (cloud.has_value()) {
						const auto accountId = _config.controller->session().userId().bare;
						shot.setCloudTheme(accountId, cloud->id, cloud->accessHash, cloud->documentId, cloud->title);
					} else if (embedded != Window::Theme::EmbeddedType(-1)) {
						const auto color = LupinFeatures::MessageShot::getSelectedColorFromDefault();
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
					LupinFeatures::MessageShot::setChoosingTheme(false);
				},
				content->lifetime());

			Ui::show(std::move(box), Ui::LayerOption::KeepOther);
		});
	AddButtonWithIcon(
		content,
		tr::lupin_MessageShotShowBackground(),
		st::settingsButtonNoIcon
	)->toggleOn(rpl::single(shotSettings.showBackground())
	)->toggledValue(
	) | rpl::skip(1) | on_next(
		[=](bool enabled)
		{
			LupinSettings::getInstance().messageShotSettings().setShowBackground(enabled);
			updatePreview();
		},
		content->lifetime());

	auto latestToggle = AddButtonWithIcon(
		content,
		tr::lupin_MessageShotShowDate(),
		st::settingsButtonNoIcon
	);
	latestToggle->toggleOn(rpl::single(shotSettings.showDate())
	)->toggledValue(
	) | rpl::skip(1) | on_next(
		[=](bool enabled)
		{
			LupinSettings::getInstance().messageShotSettings().setShowDate(enabled);
			updatePreview();
		},
		content->lifetime());

	if (hasReactions) {
		latestToggle = AddButtonWithIcon(
			content,
			tr::lupin_MessageShotShowReactions(),
			st::settingsButtonNoIcon
		);
		latestToggle->toggleOn(rpl::single(shotSettings.showReactions())
		)->toggledValue(
		) | rpl::skip(1) | on_next(
			[=](bool enabled)
			{
				LupinSettings::getInstance().messageShotSettings().setShowReactions(enabled);
				updatePreview();
			},
			content->lifetime());
	}

	if (hasHeaderDecorations) {
		latestToggle = AddButtonWithIcon(
			content,
			tr::lupin_MessageShotShowHeaderDecorations(),
			st::settingsButtonNoIcon
		);
		latestToggle->toggleOn(rpl::single(shotSettings.showHeaderDecorations())
		)->toggledValue(
		) | rpl::skip(1) | on_next(
			[=](bool enabled)
			{
				LupinSettings::getInstance().messageShotSettings().setShowHeaderDecorations(enabled);
				updatePreview();
			},
			content->lifetime());
	}

	if (hasReplies) {
		latestToggle = AddButtonWithIcon(
			content,
			tr::lupin_MessageShotShowColorfulReplies(),
			st::settingsButtonNoIcon
		);
		latestToggle->toggleOn(rpl::single(shotSettings.showColorfulReplies())
		)->toggledValue(
		) | rpl::skip(1) | on_next(
			[=](bool enabled)
			{
				auto &currentSettings = LupinSettings::getInstance();
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
			tr::lupin_MessageShotRevealSpoilers(),
			st::settingsButtonNoIcon
		);
		latestToggle->toggleOn(rpl::single(shotSettings.revealSpoilers())
		)->toggledValue(
		) | rpl::skip(1) | on_next(
			[=](bool enabled)
			{
				LupinSettings::getInstance().messageShotSettings().setRevealSpoilers(enabled);
				updatePreview();
			},
			content->lifetime());
	}

	AddSkip(content);

	addButton(tr::lupin_MessageShotSave(),
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
	addButton(tr::lupin_MessageShotCopy(),
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
			LupinFeatures::MessageShot::resetCustomSelected();
			LupinFeatures::MessageShot::resetDefaultSelected();
			LupinFeatures::MessageShot::resetShotConfig();

			LupinSettings::getInstance().setSimpleQuotesAndReplies(savedSimpleQuotesAndReplies);
		},
		content->lifetime());

	setDimensionsToContent(boxWidth, content);

	scrollToWidget(latestToggle);
}
