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

namespace Settings {

rpl::producer<QString> AyuGeneral::title() {
	return tr::ayu_CategoryGeneral();
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
		tr::ayu_TranslationProvider(),
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
										.title = tr::ayu_TranslationProvider(),
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
		QString(tr::ayu_SettingsShowID_Hide(tr::now)),
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
		tr::ayu_SettingsShowID(),
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
										.title = tr::ayu_SettingsShowID(),
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

	AddSubsectionTitle(container, tr::ayu_CategoryGeneral());

	AddButtonWithIcon(
		container,
		tr::ayu_DisableStories(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->disableStories)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->disableStories);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_disableStories(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	std::vector checkboxes = {
		NestedEntry{
			tr::ayu_CollapseSimilarChannels(tr::now), settings->collapseSimilarChannels, [=](bool enabled)
			{
				AyuSettings::set_collapseSimilarChannels(enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			tr::ayu_HideSimilarChannelsTab(tr::now), settings->hideSimilarChannels, [=](bool enabled)
			{
				AyuSettings::set_hideSimilarChannels(enabled);
				AyuSettings::save();
			}
		}
	};

	AddCollapsibleToggle(container, tr::ayu_DisableSimilarChannels(), checkboxes, true);

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	AddButtonWithIcon(
		container,
		tr::ayu_SettingsShowMessageSeconds(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->showMessageSeconds)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showMessageSeconds);
		}) | start_with_next(
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

	AddSubsectionTitle(container, rpl::single(QString("Webview")));

	AddButtonWithIcon(
		container,
		tr::ayu_SettingsSpoofWebviewAsAndroid(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->spoofWebviewAsAndroid)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->spoofWebviewAsAndroid);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_spoofWebviewAsAndroid(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	std::vector webviewCheckboxes = {
		NestedEntry{
			tr::ayu_SettingsIncreaseWebviewHeight(tr::now), settings->increaseWebviewHeight, [=](bool enabled)
			{
				AyuSettings::set_increaseWebviewHeight(enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			tr::ayu_SettingsIncreaseWebviewWidth(tr::now), settings->increaseWebviewWidth, [=](bool enabled)
			{
				AyuSettings::set_increaseWebviewWidth(enabled);
				AyuSettings::save();
			}
		}
	};

	AddCollapsibleToggle(container, tr::ayu_SettingsBiggerWindow(), webviewCheckboxes, false);

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	// todo: move into a single checkbox with dropdown
	AddSubsectionTitle(container, tr::ayu_ConfirmationsTitle());

	AddButtonWithIcon(
		container,
		tr::ayu_StickerConfirmation(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->stickerConfirmation)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->stickerConfirmation);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_stickerConfirmation(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_GIFConfirmation(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->gifConfirmation)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->gifConfirmation);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_gifConfirmation(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_VoiceConfirmation(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->voiceConfirmation)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->voiceConfirmation);
		}) | start_with_next(
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