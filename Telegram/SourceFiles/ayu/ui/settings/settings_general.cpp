// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_general.h"

#include "lang_auto.h"
#include "settings_ayu_utils.h"
#include "ayu/ayu_settings.h"
#include "settings/settings_common.h"
#include "styles/style_settings.h"
#include "ui/vertical_list.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "ayu/ui/settings/ayu_hant_helper.h"

namespace Settings {

rpl::producer<QString> AyuGeneral::title() {
	return AYU_T(ayu_CategoryGeneral);
}

AyuGeneral::AyuGeneral(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent) {
	setupContent(controller);
}

void SetupTranslator(not_null<Ui::VerticalLayout*> container,
					 not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::lng_translate_settings_subtitle()/*rpl::single(QString("Translate Messages"))*/);

	const auto options = std::vector{
		QString("Telegram"),
		QString("Google"),
		QString("Yandex")
	};

	const auto getIndex = [=](const QString &val)
	{
		if (val == "telegram") {
			return 0;
		}
		if (val == "google") {
			return 1;
		}
		if (val == "yandex") {
			return 2;
		}
		return 0;
	};

	auto currentVal = AyuSettings::get_translationProviderReactive() | rpl::map(getIndex) | rpl::map(
		[=](int val)
		{
			return options[val];
		});

	const auto button = AddButtonWithLabel(
		container,
		asBeta(AYU_T(ayu_TranslationProvider)),
		currentVal,
		st::settingsButtonNoIcon);
	button->addClickHandler(
		[=]
		{
			controller->show(Box(
				[=](not_null<Ui::GenericBox*> box)
				{
					const auto save = [=](int index)
					{
						const auto provider = (index == 0) ? "telegram" : (index == 1) ? "google" : "yandex";

						AyuSettings::set_translationProvider(provider);
						AyuSettings::save();
					};
					SingleChoiceBox(box,
									{
										.title = AYU_T(ayu_TranslationProvider),
										.options = options,
										.initialSelection = getIndex(settings->translationProvider),
										.callback = save,
									});
				}));
		});
}

void SetupShowPeerId(not_null<Ui::VerticalLayout*> container,
					 not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	const auto options = std::vector{
		QString(AYU_S(ayu_SettingsShowID_Hide)),
		QString("Telegram API"),
		QString("Bot API")
	};

	auto currentVal = AyuSettings::get_showPeerIdReactive() | rpl::map(
		[=](int val)
		{
			return options[val];
		});

	const auto button = AddButtonWithLabel(
		container,
		AYU_T(ayu_SettingsShowID),
		currentVal,
		st::settingsButtonNoIcon);
	button->addClickHandler(
		[=]
		{
			controller->show(Box(
				[=](not_null<Ui::GenericBox*> box)
				{
					const auto save = [=](int index)
					{
						AyuSettings::set_showPeerId(index);
						AyuSettings::save();
					};
					SingleChoiceBox(box,
									{
										.title = AYU_T(ayu_SettingsShowID),
										.options = options,
										.initialSelection = settings->showPeerId,
										.callback = save,
									});
				}));
		});
}

void SetupQoLToggles(not_null<Ui::VerticalLayout*> container, not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	SetupTranslator(container, controller);
	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	AddSubsectionTitle(container, AYU_T(ayu_CategoryGeneral));

	AddButtonWithIcon(
		container,
		AYU_T(ayu_DisableStories),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->disableStories)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->disableStories);
		}) | rpl::on_next(
		[=](bool enabled)
		{
			AyuSettings::set_disableStories(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		AYU_T(ayu_DisableOpenLinkWarning),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->disableOpenLinkWarning)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->disableOpenLinkWarning);
		}) | rpl::on_next(
		[=](bool enabled)
		{
			AyuSettings::set_disableOpenLinkWarning(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	std::vector checkboxes = {
		NestedEntry{
			AYU_S(ayu_CollapseSimilarChannels), settings->collapseSimilarChannels, [=](bool enabled)
			{
				AyuSettings::set_collapseSimilarChannels(enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			AYU_S(ayu_HideSimilarChannelsTab), settings->hideSimilarChannels, [=](bool enabled)
			{
				AyuSettings::set_hideSimilarChannels(enabled);
				AyuSettings::save();
			}
		}
	};

	AddCollapsibleToggle(container, AYU_T(ayu_DisableSimilarChannels), checkboxes, true);


	AddButtonWithIcon(
		container,
		AYU_T(ayu_DisableNotificationsDelay),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->disableNotificationsDelay)
	)->toggledValue(
	) | rpl::filter([=](bool enabled)
	{
		return (enabled != settings->disableNotificationsDelay);
	}) | on_next([=](bool enabled)
						 {
							 AyuSettings::set_disableNotificationsDelay(enabled);
							 AyuSettings::save();
						 },
						 container->lifetime());

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	AddButtonWithIcon(
		container,
		AYU_T(ayu_SettingsShowMessageSeconds),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->showMessageSeconds)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showMessageSeconds);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_showMessageSeconds(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	SetupShowPeerId(container, controller);

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	AddSubsectionTitle(container, rpl::single(AyuHantHelper(qsl("ayu_WebviewHeader"), qsl("Webview"))));

	AddButtonWithIcon(
		container,
		AYU_T(ayu_SettingsSpoofWebviewAsAndroid),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->spoofWebviewAsAndroid)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->spoofWebviewAsAndroid);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_spoofWebviewAsAndroid(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	std::vector webviewCheckboxes = {
		NestedEntry{
			AYU_S(ayu_SettingsIncreaseWebviewHeight), settings->increaseWebviewHeight, [=](bool enabled)
			{
				AyuSettings::set_increaseWebviewHeight(enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			AYU_S(ayu_SettingsIncreaseWebviewWidth), settings->increaseWebviewWidth, [=](bool enabled)
			{
				AyuSettings::set_increaseWebviewWidth(enabled);
				AyuSettings::save();
			}
		}
	};

	AddCollapsibleToggle(container, AYU_T(ayu_SettingsBiggerWindow), webviewCheckboxes, false);

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	// todo: move into a single checkbox with dropdown
	AddSubsectionTitle(container, AYU_T(ayu_ConfirmationsTitle));

	AddButtonWithIcon(
		container,
		AYU_T(ayu_StickerConfirmation),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->stickerConfirmation)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->stickerConfirmation);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_stickerConfirmation(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		AYU_T(ayu_GIFConfirmation),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->gifConfirmation)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->gifConfirmation);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_gifConfirmation(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		AYU_T(ayu_VoiceConfirmation),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->voiceConfirmation)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->voiceConfirmation);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_voiceConfirmation(enabled);
			AyuSettings::save();
		},
		container->lifetime());
}

void AyuGeneral::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	AddSkip(content);
	SetupQoLToggles(content, controller);
	AddSkip(content);

	ResizeFitChild(this, content);
}

} // namespace Settings