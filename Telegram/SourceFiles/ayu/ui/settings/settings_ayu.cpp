// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "settings_ayu.h"
#include "ayu/ayu_settings.h"
#include "ayu/sync/ayu_sync_controller.h"
#include "ayu/ui/boxes/edit_deleted_mark.h"
#include "ayu/ui/boxes/edit_edited_mark.h"

#include "apiwrap.h"
#include "lang_auto.h"
#include "mainwindow.h"
#include "api/api_blocked_peers.h"
#include "boxes/connection_box.h"
#include "core/application.h"
#include "data/data_session.h"
#include "lang/lang_instance.h"
#include "main/main_session.h"
#include "platform/platform_specific.h"
#include "settings/settings_common.h"
#include "storage/localstorage.h"
#include "styles/style_basic.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings
{
	rpl::producer<QString> Ayu::title()
	{
		return tr::ayu_AyuPreferences();
	}

	Ayu::Ayu(
		QWidget* parent,
		not_null<Window::SessionController*> controller)
		: Section(parent)
	{
		setupContent(controller);
	}

	void Ayu::SetupGhostEssentials(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_GhostEssentialsHeader());

		AddButton(
			container,
			tr::ayu_DontReadMessages(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(!settings->sendReadMessages)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled == settings->sendReadMessages);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_sendReadMessages(!enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_DontReadStories(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(!settings->sendReadStories)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled == settings->sendReadStories);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_sendReadStories(!enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_DontSendOnlinePackets(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(!settings->sendOnlinePackets)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled == settings->sendOnlinePackets);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_sendOnlinePackets(!enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_DontSendUploadProgress(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(!settings->sendUploadProgress)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled == settings->sendUploadProgress);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_sendUploadProgress(!enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_SendOfflinePacketAfterOnline(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->sendOfflinePacketAfterOnline)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->sendOfflinePacketAfterOnline);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_sendOfflinePacketAfterOnline(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_MarkReadAfterSend(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->markReadAfterSend)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->markReadAfterSend);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_markReadAfterSend(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_UseScheduledMessages(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->useScheduledMessages)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->useScheduledMessages);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_useScheduledMessages(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupSpyEssentials(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_SpyEssentialsHeader());

		AddButton(
			container,
			tr::ayu_SaveDeletedMessages(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->saveDeletedMessages)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->saveDeletedMessages);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_keepDeletedMessages(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_SaveMessagesHistory(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->saveMessagesHistory)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->saveMessagesHistory);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_keepMessagesHistory(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupQoLToggles(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_QoLTogglesHeader());

		AddButton(
			container,
			tr::ayu_DisableAds(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->disableAds)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->disableAds);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_disableAds(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_DisableStories(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->disableStories)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->disableStories);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_disableStories(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_CopyUsernameAsLink(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->copyUsernameAsLink)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->copyUsernameAsLink);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_copyUsernameAsLink(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupCustomization(not_null<Ui::VerticalLayout*> container,
	                             not_null<Window::SessionController*> controller)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_CustomizationHeader());

		auto btn = AddButtonWithLabel(
			container,
			tr::ayu_DeletedMarkText(),
			AyuSettings::get_deletedMarkReactive(),
			st::settingsButtonNoIcon
		);
		btn->addClickHandler([=]()
		{
			auto box = Box<EditDeletedMarkBox>();
			Ui::show(std::move(box));
		});

		auto btn2 = AddButtonWithLabel(
			container,
			tr::ayu_EditedMarkText(),
			AyuSettings::get_editedMarkReactive(),
			st::settingsButtonNoIcon
		);
		btn2->addClickHandler([=]()
		{
			auto box = Box<EditEditedMarkBox>();
			Ui::show(std::move(box));
		});

		SetupRecentStickersLimitSlider(container);

		SetupShowPeerId(container, controller);

		AddButton(
			container,
			tr::ayu_ShowGhostToggleInDrawer(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->showGhostToggleInDrawer)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->showGhostToggleInDrawer);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_showGhostToggleInDrawer(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_SettingsShowMessageSeconds(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->showMessageSeconds)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->showMessageSeconds);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_showMessageSeconds(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddDividerText(container, tr::ayu_SettingsCustomizationHint());
	}

	void Ayu::SetupShowPeerId(not_null<Ui::VerticalLayout*> container,
	                          not_null<Window::SessionController*> controller)
	{
		auto settings = &AyuSettings::getInstance();

		const auto options = std::vector{
			QString(tr::ayu_SettingsShowID_Hide(tr::now)),
			QString("Telegram API"),
			QString("Bot API")
		};

		auto currentVal = AyuSettings::get_showPeerIdReactive() | rpl::map([=](int val)
		{
			return options[val];
		});

		const auto button = AddButtonWithLabel(
			container,
			tr::ayu_SettingsShowID(),
			currentVal,
			st::settingsButtonNoIcon);
		button->addClickHandler([=]
		{
			controller->show(Box([=](not_null<Ui::GenericBox*> box)
			{
				const auto save = [=](int index)
				{
					settings->set_showPeerId(index);
					AyuSettings::save();
				};
				SingleChoiceBox(box, {
					                .title = tr::ayu_SettingsShowID(),
					                .options = options,
					                .initialSelection = settings->showPeerId,
					                .callback = save,
				                });
			}));
		});
	}

	void Ayu::SetupRecentStickersLimitSlider(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		container->add(
			CreateButton(
				container,
				tr::ayu_SettingsRecentStickersCount(),
				st::settingsButtonNoIcon)
		);

		auto recentStickersLimitSlider = MakeSliderWithLabel(
			container,
			st::settingsScale,
			st::settingsScaleLabel,
			st::normalFont->spacew * 2,
			st::settingsScaleLabel.style.font->width("300%"));
		container->add(
			std::move(recentStickersLimitSlider.widget),
			st::settingsScalePadding);
		const auto slider = recentStickersLimitSlider.slider;
		const auto label = recentStickersLimitSlider.label;

		const auto updateLabel = [=](int amount)
		{
			label->setText(QString::number(amount));
		};
		updateLabel(settings->recentStickersCount);

		slider->setPseudoDiscrete(
			100 + 1, // thx tg
			[=](int amount) { return amount; },
			settings->recentStickersCount,
			[=](int amount) { updateLabel(amount); },
			[=](int amount)
			{
				updateLabel(amount);

				settings->set_recentStickersCount(amount);
				AyuSettings::save();
			});
	}

	void Ayu::SetupAyuSync(not_null<Ui::VerticalLayout*> container)
	{
		AddSubsectionTitle(container, tr::ayu_AyuSyncHeader());

		auto text = AyuSync::isAgentDownloaded() ? tr::ayu_AyuSyncOpenPreferences() : tr::ayu_AyuSyncDownloadAgent();

		AddButton(
			container,
			text,
			st::settingsButtonNoIcon
		)->addClickHandler([=]
		{
			auto controller = &AyuSync::getInstance();
			controller->initializeAgent();
		});
	}

	void Ayu::SetupSendConfirmations(not_null<Ui::VerticalLayout*> container)
	{
		auto settings = &AyuSettings::getInstance();

		AddSubsectionTitle(container, tr::ayu_ConfirmationsTitle());

		AddButton(
			container,
			tr::ayu_StickerConfirmation(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->stickerConfirmation)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->stickerConfirmation);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_stickerConfirmation(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_GIFConfirmation(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->gifConfirmation)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->gifConfirmation);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_gifConfirmation(enabled);
			AyuSettings::save();
		}, container->lifetime());

		AddButton(
			container,
			tr::ayu_VoiceConfirmation(),
			st::settingsButtonNoIcon
		)->toggleOn(
			rpl::single(settings->voiceConfirmation)
		)->toggledValue(
		) | rpl::filter([=](bool enabled)
		{
			return (enabled != settings->voiceConfirmation);
		}) | start_with_next([=](bool enabled)
		{
			settings->set_voiceConfirmation(enabled);
			AyuSettings::save();
		}, container->lifetime());
	}

	void Ayu::SetupAyuGramSettings(not_null<Ui::VerticalLayout*> container,
	                               not_null<Window::SessionController*> controller)
	{
		AddSkip(container);
		SetupGhostEssentials(container);

		AddDivider(container);

		AddSkip(container);
		SetupSpyEssentials(container);

		AddDivider(container);

		AddSkip(container);
		SetupQoLToggles(container);

		AddDivider(container);

		AddSkip(container);
		SetupCustomization(container, controller);

		AddSkip(container);
		SetupAyuSync(container);

		AddDivider(container);

		AddSkip(container);
		SetupSendConfirmations(container);

		AddDividerText(container, tr::ayu_SettingsWatermark());
	}

	void Ayu::setupContent(not_null<Window::SessionController*> controller)
	{
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

		SetupAyuGramSettings(content, controller);

		ResizeFitChild(this, content);
	}
} // namespace Settings
