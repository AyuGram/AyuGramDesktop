// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/ui/settings/settings_general.h"

#include "lang_auto.h"
#include "lupin/lupin_settings.h"
#include "lupin/ui/settings/lupin_builder.h"
#include "lupin/ui/settings/settings_lupin_utils.h"
#include "lupin/ui/settings/settings_main.h"
#include "base/platform/base_platform_info.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "platform/platform_translate_provider.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

namespace Settings {

using namespace Builder;
using namespace LupinBuilder;

namespace {

void BuildTranslator(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lng_translate_settings_subtitle());

	auto *settings = &LupinSettings::getInstance();

	const auto options = std::vector{
		std::pair(TranslationProvider::Telegram, QString("Telegram")),
		std::pair(TranslationProvider::Google, QString("Google")),
		std::pair(TranslationProvider::Yandex, QString("Yandex")),
	};
	const auto nativeAvailable = Platform::IsTranslateProviderAvailable();
	auto availableOptions = options;
	if (nativeAvailable) {
		availableOptions.push_back(std::pair(
			TranslationProvider::Native,
			[] {
				if constexpr (Platform::IsMac()) {
					return QString("macOS");
				} else if constexpr (Platform::IsWindows()) {
					return QString("Windows");
				} else {
					return QString("Linux");
				}
			}()));
	}
	auto optionLabels = std::vector<QString>();
	optionLabels.reserve(availableOptions.size());
	for (const auto &option : availableOptions) {
		optionLabels.push_back(option.second);
	}

	const auto getIndex = [=](TranslationProvider val) {
		const auto i = ranges::find(
			availableOptions,
			val,
			&std::pair<TranslationProvider, QString>::first);
		return (i != end(availableOptions))
			? int(i - begin(availableOptions))
			: 0;
	};

	auto currentVal = LupinSettings::getInstance().translationProviderValue()
		| rpl::map(getIndex)
		| rpl::map([=](int val) { return availableOptions[val].second; });

	const auto button = builder.addButton({
		.id = u"lupin/translationProvider"_q,
		.title = tr::lupin_TranslationProvider(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentVal),
		.onClick = [=] {
			if (const auto controller = Core::App().activeWindow()->sessionController()) {
				controller->show(Box(
						[=](not_null<Ui::GenericBox*> box) {
							const auto save = [=](int index) {
								const auto option = availableOptions[index].first;
								LupinSettings::getInstance().setTranslationProvider(option);

								if constexpr (Platform::IsMac()) {
									if (option == TranslationProvider::Native) {
										controller->showToast(Ui::Toast::Config{
											.text = tr::lng_translate_settings_use_platform_mac_about(tr::now, tr::rich),
											.duration = 6 * crl::time(1000)
										});
									}
								}
							};
							SingleChoiceBox(box, {
								.title = tr::lupin_TranslationProvider(),
								.options = optionLabels,
								.initialSelection = getIndex(settings->translationProvider()),
								.callback = save,
							});
						}));
			}
		},
	});
	if (button) {
		lupin.addBetaBadge(button);
	}
}

void BuildShowPeerId(SectionBuilder &builder) {
	auto *settings = &LupinSettings::getInstance();

	const auto options = std::vector{
		QString(tr::lupin_SettingsShowID_Hide(tr::now)),
		QString("Telegram API"),
		QString("Bot API")
	};

	auto currentVal = LupinSettings::getInstance().showPeerIdValue()
		| rpl::map([=](PeerIdDisplay val) {
			return options[static_cast<int>(val)];
		});

	const auto controller = builder.controller();
	builder.addButton({
		.id = u"lupin/showPeerId"_q,
		.altIds = { u"lupin/showIdAndDc"_q },
		.title = tr::lupin_SettingsShowID(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentVal),
		.onClick = [=] {
			controller->show(Box(
				[=](not_null<Ui::GenericBox*> box) {
					const auto save = [=](int index) {
						LupinSettings::getInstance().setShowPeerId(
							static_cast<PeerIdDisplay>(index));
					};
					SingleChoiceBox(box, {
						.title = tr::lupin_SettingsShowID(),
						.options = options,
						.initialSelection = static_cast<int>(settings->showPeerId()),
						.callback = save,
					});
				}));
		},
	});
}

void BuildQoLToggles(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	auto *settings = &LupinSettings::getInstance();

	BuildTranslator(builder, lupin);
	lupin.addSectionDivider();

	builder.addSubsectionTitle(tr::lupin_CategoryGeneral());

	const auto controller = builder.controller();
	lupin.addToggle({
		.id = u"lupin/disableStories"_q,
		.altIds = { u"lupin/hideStories"_q },
		.title = tr::lupin_DisableStories(),
		.getter = [=] { return settings->disableStories(); },
		.setter = [=](bool enabled) {
			LupinSettings::getInstance().setDisableStories(enabled);
			ShowRestartPrompt(controller);
		},
	});

	lupin.addSettingToggle({
		.id = u"lupin/disableOpenLinkWarning"_q,
		.title = tr::lupin_DisableOpenLinkWarning(),
		.getter = &LupinSettings::disableOpenLinkWarning,
		.setter = &LupinSettings::setDisableOpenLinkWarning,
	});

	lupin.addCollapsibleToggle({
		.id = u"lupin/similarChannels"_q,
		.title = tr::lupin_DisableSimilarChannels(),
		.checkboxes = {
			NestedEntry{
				tr::lupin_CollapseSimilarChannels(tr::now),
				[] { return LupinSettings::getInstance().collapseSimilarChannels(); },
				[](bool v) { LupinSettings::getInstance().setCollapseSimilarChannels(v); }
			},
			NestedEntry{
				tr::lupin_HideSimilarChannelsTab(tr::now),
				[] { return LupinSettings::getInstance().hideSimilarChannels(); },
				[](bool v) { LupinSettings::getInstance().setHideSimilarChannels(v); }
			}
		},
		.toggledWhenAll = true,
	});

	lupin.addSettingToggle({
		.id = u"lupin/disableNotificationsDelay"_q,
		.title = tr::lupin_DisableNotificationsDelay(),
		.getter = &LupinSettings::disableNotificationsDelay,
		.setter = &LupinSettings::setDisableNotificationsDelay,
	});

	lupin.addSectionDivider();

	const auto zalgoButton = builder.addButton({
		.id = u"lupin/filterZalgo"_q,
		.title = tr::lupin_FilterZalgo(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(settings->filterZalgo()),
	});
	if (zalgoButton) {
		zalgoButton->toggledValue(
		) | rpl::filter(
			[=](bool enabled) {
				return (enabled != settings->filterZalgo());
			}
		) | on_next(
			[=](bool enabled) {
				LupinSettings::getInstance().setFilterZalgo(enabled);
				ShowRestartPrompt(controller);
			},
			zalgoButton->lifetime());
		lupin.addBetaBadge(zalgoButton);
	}

	lupin.addSettingToggle({
		.id = u"lupin/improveLinkPreviews"_q,
		.title = tr::lupin_ImproveLinkPreviews(),
		.getter = &LupinSettings::improveLinkPreviews,
		.setter = &LupinSettings::setImproveLinkPreviews,
	});
	lupin.addCollapsibleToggle({
		.id = u"lupin/confirmations"_q,
		.title = tr::lupin_ConfirmationsTitle(),
		.checkboxes = {
			NestedEntry{
				tr::lupin_StickerConfirmation(tr::now),
				[] { return LupinSettings::getInstance().stickerConfirmation(); },
				[](bool v) { LupinSettings::getInstance().setStickerConfirmation(v); }
			},
			NestedEntry{
				tr::lupin_GIFConfirmation(tr::now),
				[] { return LupinSettings::getInstance().gifConfirmation(); },
				[](bool v) { LupinSettings::getInstance().setGifConfirmation(v); }
			},
			NestedEntry{
				tr::lupin_VoiceConfirmation(tr::now),
				[] { return LupinSettings::getInstance().voiceConfirmation(); },
				[](bool v) { LupinSettings::getInstance().setVoiceConfirmation(v); }
			},
			NestedEntry{
				tr::lupin_RoundConfirmation(tr::now),
				[] { return LupinSettings::getInstance().roundConfirmation(); },
				[](bool v) { LupinSettings::getInstance().setRoundConfirmation(v); }
			}
		},
		.toggledWhenAll = false,
	});
	lupin.addSettingToggle({
		.id = u"lupin/showMessageSeconds"_q,
		.altIds = { u"lupin/formatTimeWithSeconds"_q },
		.title = tr::lupin_SettingsShowMessageSeconds(),
		.getter = &LupinSettings::showMessageSeconds,
		.setter = &LupinSettings::setShowMessageSeconds,
	});

	BuildShowPeerId(builder);

	lupin.addSectionDivider();

	builder.addSubsectionTitle(rpl::single(QString("Webview")));

	lupin.addSettingToggle({
		.id = u"lupin/spoofWebviewAsAndroid"_q,
		.title = tr::lupin_SettingsSpoofWebviewAsAndroid(),
		.getter = &LupinSettings::spoofWebviewAsAndroid,
		.setter = &LupinSettings::setSpoofWebviewAsAndroid,
	});

	lupin.addCollapsibleToggle({
		.id = u"lupin/biggerWindow"_q,
		.title = tr::lupin_SettingsBiggerWindow(),
		.checkboxes = {
			NestedEntry{
				tr::lupin_SettingsIncreaseWebviewHeight(tr::now),
				[] { return LupinSettings::getInstance().increaseWebviewHeight(); },
				[](bool v) { LupinSettings::getInstance().setIncreaseWebviewHeight(v); }
			},
			NestedEntry{
				tr::lupin_SettingsIncreaseWebviewWidth(tr::now),
				[] { return LupinSettings::getInstance().increaseWebviewWidth(); },
				[](bool v) { LupinSettings::getInstance().setIncreaseWebviewWidth(v); }
			}
		},
		.toggledWhenAll = false,
	});
}

const auto kMeta = BuildHelper({
	.id = LupinGeneral::Id(),
	.parentId = LupinMain::Id(),
	.title = &tr::lupin_CategoryGeneral,
	.icon = &st::menuIconShowAll,
}, [](SectionBuilder &builder) {
	auto lupin = LupinSectionBuilder(builder);

	builder.addSkip();
	BuildQoLToggles(builder, lupin);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> LupinGeneral::title() {
	return tr::lupin_CategoryGeneral();
}

LupinGeneral::LupinGeneral(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void LupinGeneral::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type LupinGeneralId() {
	return LupinGeneral::Id();
}

} // namespace Settings
