// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_chats.h"

#include <styles/style_ayu_icons.h>

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/edit_mark_box.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "core/application.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/vertical_list.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

rpl::producer<QString> AyuChats::title() {
	return tr::ayu_CategoryChats();
}

AyuChats::AyuChats(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent) {
	setupContent(controller);
}

void SetupStickersAndEmojiSettings(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::lng_settings_stickers_emoji()/*rpl::single(QString("Stickers and Emoji"))*/);

	AddButtonWithIcon(
		container,
		tr::ayu_ShowOnlyAddedEmojisAndStickers(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->showOnlyAddedEmojisAndStickers)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showOnlyAddedEmojisAndStickers);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showOnlyAddedEmojisAndStickers(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	std::vector checkboxes = {
		NestedEntry{
			tr::ayu_HideReactionsInChannels(tr::now), !settings->showChannelReactions, [=](bool enabled)
			{
				AyuSettings::set_hideChannelReactions(!enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			tr::ayu_HideReactionsInGroups(tr::now), !settings->showGroupReactions, [=](bool enabled)
			{
				AyuSettings::set_hideGroupReactions(!enabled);
				AyuSettings::save();
			}
		}
	};

	AddCollapsibleToggle(container, tr::ayu_HideReactions(), checkboxes, false);

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

void SetupRecentStickersLimit(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	container->add(
		object_ptr<Button>(container,
						   tr::ayu_SettingsRecentStickersCount(),
						   st::settingsButtonNoIcon)
	)->setAttribute(Qt::WA_TransparentForMouseEvents);

	auto recentStickersLimitSlider = MakeSliderWithLabel(
		container,
		st::autoDownloadLimitSlider,
		st::settingsScaleLabel,
		0,
		st::settingsScaleLabel.style.font->width("8%%"));
	container->add(std::move(recentStickersLimitSlider.widget), st::recentStickersLimitPadding);
	const auto slider = recentStickersLimitSlider.slider;
	const auto label = recentStickersLimitSlider.label;

	const auto updateLabel = [=](int amount)
	{
		label->setText(QString::number(amount));
	};
	updateLabel(settings->recentStickersCount);

	slider->setPseudoDiscrete(
		200 + 1,
		[=](int amount)
		{
			return amount;
		},
		settings->recentStickersCount,
		[=](int amount)
		{
			updateLabel(amount);
		},
		[=](int amount)
		{
			updateLabel(amount);
			AyuSettings::set_recentStickersCount(amount);
			AyuSettings::save();
		});

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

void SetupGroupsAndChannels(not_null<Ui::VerticalLayout*> container, not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container,
					   tr::lng_premium_double_limits_subtitle_channels()
					   /*rpl::single(QString("Groups and Channels"))*/);

	const auto options = std::vector{
		tr::ayu_ChannelBottomButtonHide(tr::now),
		tr::ayu_ChannelBottomButtonMute(tr::now),
		tr::ayu_ChannelBottomButtonDiscuss(tr::now),
	};

	AddChooseButtonWithIconAndRightText(
		container,
		controller,
		settings->channelBottomButton,
		options,
		tr::ayu_ChannelBottomButton(),
		tr::ayu_ChannelBottomButton(),
		[=](int index)
		{
			AyuSettings::set_channelBottomButton(index);
			AyuSettings::save();
		});

	AddButtonWithIcon(
		container,
		tr::ayu_QuickAdminShortcuts(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->quickAdminShortcuts)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->quickAdminShortcuts);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_quickAdminShortcuts(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_SettingsShowMessageShot(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->showMessageShot)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showMessageShot);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showMessageShot(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddSkip(container);
	AddDividerText(container, tr::ayu_SettingsShowMessageShotDescription());
	AddSkip(container);
}

void SetupMarks(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::lng_settings_messages()/*rpl::single(QString("Messages"))*/);

	AddButtonWithLabel(
		container,
		tr::ayu_DeletedMarkText(),
		AyuSettings::get_deletedMarkReactive(),
		st::settingsButtonNoIcon
	)->addClickHandler(
		[=]()
		{
			auto box = Box<EditMarkBox>(
				tr::ayu_DeletedMarkText(),
				settings->deletedMark,
				QString("🧹"),
				[=](const QString &value)
				{
					AyuSettings::set_deletedMark(value);
					AyuSettings::save();
				}
			);
			Ui::show(std::move(box));
		});

	AddButtonWithLabel(
		container,
		tr::ayu_EditedMarkText(),
		AyuSettings::get_editedMarkReactive(),
		st::settingsButtonNoIcon
	)->addClickHandler(
		[=]()
		{
			auto box = Box<EditMarkBox>(
				tr::ayu_EditedMarkText(),
				settings->editedMark,
				tr::lng_edited(tr::now),
				[=](const QString &value)
				{
					AyuSettings::set_editedMark(value);
					AyuSettings::save();
				}
			);
			Ui::show(std::move(box));
		});

	AddButtonWithIcon(
		container,
		tr::ayu_ReplaceMarksWithIcons(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->replaceBottomInfoWithIcons)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->replaceBottomInfoWithIcons);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_replaceBottomInfoWithIcons(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	AddButtonWithIcon(
		container,
		tr::ayu_HideShareButton(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->hideFastShare)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->hideFastShare);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_hideFastShare(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_SimpleQuotesAndReplies(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->simpleQuotesAndReplies)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->simpleQuotesAndReplies);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_simpleQuotesAndReplies(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

void SetupWideMessagesMultiplier(not_null<Ui::VerticalLayout*> container,
								 not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	container->add(
		object_ptr<Button>(container,
						   tr::ayu_SettingsWideMultiplier(),
						   st::settingsButtonNoIcon)
	)->setAttribute(Qt::WA_TransparentForMouseEvents);

	auto wideMultiplierSlider = MakeSliderWithLabel(
		container,
		st::autoDownloadLimitSlider,
		st::settingsScaleLabel,
		0,
		st::settingsScaleLabel.style.font->width("8%%%"));
	container->add(std::move(wideMultiplierSlider.widget), st::recentStickersLimitPadding);
	const auto slider = wideMultiplierSlider.slider;
	const auto label = wideMultiplierSlider.label;

	const auto updateLabel = [=](double val)
	{
		label->setText(QString::number(val, 'f', 2));
	};

	constexpr auto kSizeAmount = 61; // (4.00 - 1.00) / 0.05 + 1
	constexpr auto kMinSize = 1.00;
	constexpr auto kStep = 0.05;

	const auto valueToIndex = [=](double value)
	{
		return static_cast<int>(std::round((value - kMinSize) / kStep));
	};
	const auto indexToValue = [=](int index)
	{
		return kMinSize + index * kStep;
	};

	updateLabel(settings->wideMultiplier);

	slider->setPseudoDiscrete(
		kSizeAmount,
		[=](int index) { return index; },
		valueToIndex(settings->wideMultiplier),
		[=](int index)
		{
			updateLabel(indexToValue(index));
		},
		[=](int index)
		{
			updateLabel(indexToValue(index));
			AyuSettings::set_wideMultiplier(indexToValue(index));
			AyuSettings::save();

			// fix slider
			crl::on_main([=]
			{
				controller->show(Ui::MakeConfirmBox({
					.text = tr::lng_settings_need_restart(),
					.confirmed = []
					{
						Core::Restart();
					},
					.confirmText = tr::lng_settings_restart_now(),
					.cancelText = tr::lng_settings_restart_later(),
				}));
			});
		});

	AddSkip(container);
	AddDividerText(container, tr::ayu_SettingsWideMultiplierDescription());
	AddSkip(container);
}

void SetupContextMenuElements(not_null<Ui::VerticalLayout*> container,
							  not_null<Window::SessionController*> controller) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_ContextMenuElementsHeader());

	const auto options = std::vector{
		tr::ayu_SettingsContextMenuItemHidden(tr::now),
		tr::ayu_SettingsContextMenuItemShown(tr::now),
		tr::ayu_SettingsContextMenuItemExtended(tr::now),
	};

	AddChooseButtonWithIconAndRightText(
		container,
		controller,
		settings->showReactionsPanelInContextMenu,
		options,
		tr::ayu_SettingsContextMenuReactionsPanel(),
		tr::ayu_SettingsContextMenuTitle(),
		st::menuIconReactions,
		[=](int index)
		{
			AyuSettings::set_showReactionsPanelInContextMenu(index);
			AyuSettings::save();
		});
	AddChooseButtonWithIconAndRightText(
		container,
		controller,
		settings->showViewsPanelInContextMenu,
		options,
		tr::ayu_SettingsContextMenuViewsPanel(),
		tr::ayu_SettingsContextMenuTitle(),
		st::menuIconShowInChat,
		[=](int index)
		{
			AyuSettings::set_showViewsPanelInContextMenu(index);
			AyuSettings::save();
		});

	AddChooseButtonWithIconAndRightText(
		container,
		controller,
		settings->showHideMessageInContextMenu,
		options,
		tr::ayu_ContextHideMessage(),
		tr::ayu_SettingsContextMenuTitle(),
		st::menuIconClear,
		[=](int index)
		{
			AyuSettings::set_showHideMessageInContextMenu(index);
			AyuSettings::save();
		});
	AddChooseButtonWithIconAndRightText(
		container,
		controller,
		settings->showUserMessagesInContextMenu,
		options,
		tr::ayu_UserMessagesMenuText(),
		tr::ayu_SettingsContextMenuTitle(),
		st::menuIconTTL,
		[=](int index)
		{
			AyuSettings::set_showUserMessagesInContextMenu(index);
			AyuSettings::save();
		});
	AddChooseButtonWithIconAndRightText(
		container,
		controller,
		settings->showMessageDetailsInContextMenu,
		options,
		tr::ayu_MessageDetailsPC(),
		tr::ayu_SettingsContextMenuTitle(),
		st::menuIconInfo,
		[=](int index)
		{
			AyuSettings::set_showMessageDetailsInContextMenu(index);
			AyuSettings::save();
		});

	AddSkip(container);
	AddDividerText(container, tr::ayu_SettingsContextMenuDescription());
	AddSkip(container);
}

void SetupMessageFieldElements(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_MessageFieldElementsHeader());

	AddButtonWithIcon(
		container,
		tr::ayu_MessageFieldElementAttach(),
		st::settingsButton,
		{&st::messageFieldAttachIcon}
	)->toggleOn(
		rpl::single(settings->showAttachButtonInMessageField)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showAttachButtonInMessageField);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showAttachButtonInMessageField(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_MessageFieldElementCommands(),
		st::settingsButton,
		{&st::messageFieldCommandsIcon}
	)->toggleOn(
		rpl::single(settings->showCommandsButtonInMessageField)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showCommandsButtonInMessageField);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showCommandsButtonInMessageField(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_MessageFieldElementTTL(),
		st::settingsButton,
		{&st::messageFieldTTLIcon}
	)->toggleOn(
		rpl::single(settings->showAutoDeleteButtonInMessageField)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showAutoDeleteButtonInMessageField);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showAutoDeleteButtonInMessageField(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_MessageFieldElementEmoji(),
		st::settingsButton,
		{&st::messageFieldEmojiIcon}
	)->toggleOn(
		rpl::single(settings->showEmojiButtonInMessageField)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showEmojiButtonInMessageField);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showEmojiButtonInMessageField(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_MessageFieldElementVoice(),
		st::settingsButton,
		{&st::messageFieldVoiceIcon}
	)->toggleOn(
		rpl::single(settings->showMicrophoneButtonInMessageField)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showMicrophoneButtonInMessageField);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showMicrophoneButtonInMessageField(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

void SetupMessageFieldPopups(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_MessageFieldPopupsHeader());

	AddButtonWithIcon(
		container,
		tr::ayu_MessageFieldElementAttach(),
		st::settingsButton,
		{&st::messageFieldAttachIcon}
	)->toggleOn(
		rpl::single(settings->showAttachPopup)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showAttachPopup);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showAttachPopup(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_MessageFieldElementEmoji(),
		st::settingsButton,
		{&st::messageFieldEmojiIcon}
	)->toggleOn(
		rpl::single(settings->showEmojiPopup)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->showEmojiPopup);
		}) | rpl::start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_showEmojiPopup(enabled);
			AyuSettings::save();
		},
		container->lifetime());
}

void AyuChats::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	AddSkip(content);
	SetupStickersAndEmojiSettings(content);
	SetupRecentStickersLimit(content);

	SetupGroupsAndChannels(content, controller);

	SetupMarks(content);
	SetupWideMessagesMultiplier(content, controller);

	SetupContextMenuElements(content, controller);
	SetupMessageFieldElements(content);

	SetupMessageFieldPopups(content);
	AddSkip(content);

	ResizeFitChild(this, content);
}

} // namespace Settings