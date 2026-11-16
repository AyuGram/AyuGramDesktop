// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/ui/settings/settings_chats.h"

#include "lang_auto.h"
#include "lupin/lupin_settings.h"
#include "lupin/ui/boxes/edit_mark_box.h"
#include "lupin/ui/components/message_preview.h"
#include "lupin/ui/settings/lupin_builder.h"
#include "lupin/ui/settings/settings_lupin_utils.h"
#include "lupin/ui/settings/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_lupin_icons.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <memory>

namespace Settings {

using namespace Builder;
using namespace LupinBuilder;

namespace {

struct PreviewState {
	MessagePreview *widget = nullptr;
};

void BuildStickersAndEmoji(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lng_settings_stickers_emoji());

	lupin.addSettingToggle({
		.id = u"lupin/showOnlyAddedEmojisAndStickers"_q,
		.title = tr::lupin_ShowOnlyAddedEmojisAndStickers(),
		.getter = &LupinSettings::showOnlyAddedEmojisAndStickers,
		.setter = &LupinSettings::setShowOnlyAddedEmojisAndStickers,
	});

	lupin.addSettingToggle({
		.id = u"lupin/unlimitedRecentStickers"_q,
		.altIds = { u"lupin/recentStickersCount"_q },
		.title = tr::lupin_SettingsUnlimitedRecentStickers(),
		.getter = &LupinSettings::unlimitedRecentStickers,
		.setter = &LupinSettings::setUnlimitedRecentStickers,
	});

	lupin.addCollapsibleToggle({
		.id = u"lupin/hideReactions"_q,
		.title = tr::lupin_HideReactions(),
		.checkboxes = {
			NestedEntry{
				tr::lupin_HideReactionsInChannels(tr::now),
				[] { return !LupinSettings::getInstance().showChannelReactions(); },
				[](bool v) { LupinSettings::getInstance().setShowChannelReactions(!v); }
			},
			NestedEntry{
				tr::lupin_HideReactionsInGroups(tr::now),
				[] { return !LupinSettings::getInstance().showGroupReactions(); },
				[](bool v) { LupinSettings::getInstance().setShowGroupReactions(!v); }
			},
			NestedEntry{
				tr::lupin_HideReactionsInPrivateChats(tr::now),
				[] { return !LupinSettings::getInstance().showPrivateChatReactions(); },
				[](bool v) { LupinSettings::getInstance().setShowPrivateChatReactions(!v); }
			}
		},
		.toggledWhenAll = false,
	});

	lupin.addSectionDivider();
}

void BuildGroupsAndChannels(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	auto *settings = &LupinSettings::getInstance();

	builder.addSubsectionTitle(tr::lng_premium_double_limits_subtitle_channels());

	lupin.addChooseButton({
		.id = u"lupin/channelBottomButton"_q,
		.altIds = { u"lupin/bottomButton"_q },
		.title = tr::lupin_ChannelBottomButton(),
		.boxTitle = tr::lupin_ChannelBottomButton(),
		.initialSelection = static_cast<int>(settings->channelBottomButton()),
		.options = {
			tr::lupin_ChannelBottomButtonHide(tr::now),
			tr::lupin_ChannelBottomButtonMute(tr::now),
			tr::lupin_ChannelBottomButtonDiscuss(tr::now),
		},
		.setter = [](int index) {
			LupinSettings::getInstance().setChannelBottomButton(
				static_cast<ChannelBottomButton>(index));
		},
	});

	lupin.addSettingToggle({
		.id = u"lupin/quickAdminShortcuts"_q,
		.title = tr::lupin_QuickAdminShortcuts(),
		.getter = &LupinSettings::quickAdminShortcuts,
		.setter = &LupinSettings::setQuickAdminShortcuts,
	});
	lupin.addSettingToggle({
		.id = u"lupin/disableGreetingSticker"_q,
		.title = tr::lupin_DisableGreetingSticker(),
		.getter = &LupinSettings::disableGreetingSticker,
		.setter = &LupinSettings::setDisableGreetingSticker,
	});
	lupin.addSettingToggle({
		.id = u"lupin/showMessageShot"_q,
		.title = tr::lupin_SettingsShowMessageShot(),
		.getter = &LupinSettings::showMessageShot,
		.setter = &LupinSettings::setShowMessageShot,
	});

	builder.addSkip();
	builder.addDividerText(tr::lupin_SettingsShowMessageShotDescription());
	builder.addSkip();
}

void BuildMarks(
		SectionBuilder &builder,
		LupinSectionBuilder &lupin,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &LupinSettings::getInstance();
	const auto controller = builder.controller();

	builder.addSubsectionTitle(tr::lng_settings_messages());

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<MessagePreview>(ctx.container, controller);
		previewState->widget = preview.data();
		return {
			.widget = std::move(preview),
			.margin = style::margins(
				0,
				st::defaultVerticalListSkip,
				0,
				st::settingsPrivacySkipTop),
		};
	});

	lupin.addSettingToggle({
		.id = u"lupin/replaceBottomInfoWithIcons"_q,
		.altIds = { u"lupin/replaceEditedWithIcon"_q },
		.title = tr::lupin_ReplaceMarksWithIcons(),
		.getter = &LupinSettings::replaceBottomInfoWithIcons,
		.setter = &LupinSettings::setReplaceBottomInfoWithIcons,
	});

	builder.scope([&] {
		builder.addButton({
			.id = u"lupin/deletedMark"_q,
			.title = tr::lupin_DeletedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = LupinSettings::getInstance().deletedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::lupin_DeletedMarkText(),
					settings->deletedMark(),
					QString("🧹"),
					[=](const QString &value) {
						LupinSettings::getInstance().setDeletedMark(value);
					});
				Ui::show(std::move(box));
			},
		});

		builder.addButton({
			.id = u"lupin/editedMark"_q,
			.title = tr::lupin_EditedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = LupinSettings::getInstance().editedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::lupin_EditedMarkText(),
					settings->editedMark(),
					tr::lng_edited(tr::now),
					[=](const QString &value) {
						LupinSettings::getInstance().setEditedMark(value);
					});
				Ui::show(std::move(box));
			},
		});
	}, LupinSettings::getInstance().replaceBottomInfoWithIconsValue()
		| rpl::map([](bool v) { return !v; }));

	lupin.addSettingToggle({
		.id = u"lupin/removeMessageTail"_q,
		.title = tr::lupin_RemoveMessageTail(),
		.getter = &LupinSettings::removeMessageTail,
		.setter = &LupinSettings::setRemoveMessageTail,
	});

	lupin.addSettingToggle({
		.id = u"lupin/hideFastShare"_q,
		.altIds = { u"lupin/hideShareButton"_q },
		.title = tr::lupin_HideShareButton(),
		.getter = &LupinSettings::hideFastShare,
		.setter = &LupinSettings::setHideFastShare,
	});
	lupin.addSettingToggle({
		.id = u"lupin/simpleQuotesAndReplies"_q,
		.altIds = { u"lupin/disableColorfulReplies"_q, u"lupin/replyElements"_q },
		.title = tr::lupin_SimpleQuotesAndReplies(),
		.getter = &LupinSettings::simpleQuotesAndReplies,
		.setter = &LupinSettings::setSimpleQuotesAndReplies,
	});

	const auto semiTransparent = lupin.addSettingToggle({
		.id = u"lupin/semiTransparentDeletedMessages"_q,
		.altIds = { u"lupin/translucentDeletedMessages"_q },
		.title = tr::lupin_SemiTransparentDeletedMessages(),
		.getter = &LupinSettings::semiTransparentDeletedMessages,
		.setter = &LupinSettings::setSemiTransparentDeletedMessages,
	});
	if (semiTransparent) {
		lupin.addBetaBadge(semiTransparent);
	}

	lupin.addSectionDivider();
}

void BuildWideMessagesMultiplier(
		SectionBuilder &builder,
		LupinSectionBuilder &lupin,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &LupinSettings::getInstance();

	constexpr auto kMinSize = 1.00;
	constexpr auto kStep = 0.05;

	const auto valueToIndex = [=](double value) {
		return static_cast<int>(std::round((value - kMinSize) / kStep));
	};

	const auto controller = builder.controller();
	lupin.addSlider({
		.id = u"lupin/messageBubbleRadius"_q,
		.title = tr::lupin_MessageBubbleRadius(),
		.steps = 17,
		.current = settings->messageBubbleRadius(),
		.indexToValue = [](int index) { return index; },
		.onChanged = [=](int index) {
			if (previewState->widget) {
				previewState->widget->setBubbleRadius(index);
			}
		},
		.onFinalChanged = [=](int index) {
			if (previewState->widget) {
				previewState->widget->setBubbleRadius(index);
			}
			LupinSettings::getInstance().setMessageBubbleRadius(index);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [](int index) {
			return QString::number(index);
		},
	});

	lupin.addSectionDivider();

	lupin.addSlider({
		.id = u"lupin/wideMultiplier"_q,
		.title = tr::lupin_SettingsWideMultiplier(),
		.steps = 61, // (4.00 - 1.00) / 0.05 + 1
		.current = valueToIndex(settings->wideMultiplier()),
		.indexToValue = [](int index) { return index; },
		.onChanged = nullptr,
		.onFinalChanged = [=](int index) {
			LupinSettings::getInstance().setWideMultiplier(
				kMinSize + index * kStep);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [=](int index) {
			return QString::number(kMinSize + index * kStep, 'f', 2);
		},
	});

	builder.addSkip();
	builder.addDividerText(tr::lupin_SettingsWideMultiplierDescription());
	builder.addSkip();
}

void BuildContextMenuElements(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	auto *settings = &LupinSettings::getInstance();

	builder.addSubsectionTitle(tr::lupin_ContextMenuElementsHeader());

	const auto options = std::vector{
		tr::lupin_SettingsContextMenuItemHidden(tr::now),
		tr::lupin_SettingsContextMenuItemShown(tr::now),
		tr::lupin_SettingsContextMenuItemExtended(tr::now),
	};

	lupin.addChooseButton({
		.id = u"lupin/showReactionsPanelInContextMenu"_q,
		.title = tr::lupin_SettingsContextMenuReactionsPanel(),
		.boxTitle = tr::lupin_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showReactionsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { LupinSettings::getInstance().setShowReactionsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconReactions },
	});
	lupin.addChooseButton({
		.id = u"lupin/showViewsPanelInContextMenu"_q,
		.title = tr::lupin_SettingsContextMenuViewsPanel(),
		.boxTitle = tr::lupin_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showViewsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { LupinSettings::getInstance().setShowViewsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconShowInChat },
	});
	lupin.addChooseButton({
		.id = u"lupin/showHideMessageInContextMenu"_q,
		.title = tr::lupin_ContextHideMessage(),
		.boxTitle = tr::lupin_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showHideMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { LupinSettings::getInstance().setShowHideMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconClear },
	});
	lupin.addChooseButton({
		.id = u"lupin/showUserMessagesInContextMenu"_q,
		.title = tr::lupin_UserMessagesMenuText(),
		.boxTitle = tr::lupin_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showUserMessagesInContextMenu()),
		.options = options,
		.setter = [](int i) { LupinSettings::getInstance().setShowUserMessagesInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconTTL },
	});
	lupin.addChooseButton({
		.id = u"lupin/showMessageDetailsInContextMenu"_q,
		.title = tr::lupin_MessageDetailsPC(),
		.boxTitle = tr::lupin_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showMessageDetailsInContextMenu()),
		.options = options,
		.setter = [](int i) { LupinSettings::getInstance().setShowMessageDetailsInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconInfo },
	});
	lupin.addChooseButton({
		.id = u"lupin/showRepeatMessageInContextMenu"_q,
		.title = tr::lupin_RepeatMessage(),
		.boxTitle = tr::lupin_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showRepeatMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { LupinSettings::getInstance().setShowRepeatMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::lupinRepeatMenuIcon },
	});
	if (settings->filtersEnabled()) {
		lupin.addChooseButton({
			.id = u"lupin/showAddFilterInContextMenu"_q,
			.title = tr::lupin_RegexFilterQuickAdd(),
			.boxTitle = tr::lupin_SettingsContextMenuTitle(),
			.initialSelection = static_cast<int>(settings->showAddFilterInContextMenu()),
			.options = options,
			.setter = [](int i) { LupinSettings::getInstance().setShowAddFilterInContextMenu(static_cast<ContextMenuVisibility>(i)); },
			.icon = { &st::menuIconAddToFolder },
		});
	}

	builder.addSkip();
	builder.addDividerText(tr::lupin_SettingsContextMenuDescription());
	builder.addSkip();
}

void BuildMessageFieldElements(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lupin_MessageFieldElementsHeader());

	lupin.addSettingToggle({
		.id = u"lupin/showAttachButtonInMessageField"_q,
		.title = tr::lupin_MessageFieldElementAttach(),
		.getter = &LupinSettings::showAttachButtonInMessageField,
		.setter = &LupinSettings::setShowAttachButtonInMessageField,
		.icon = { &st::messageFieldAttachIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showCommandsButtonInMessageField"_q,
		.title = tr::lupin_MessageFieldElementCommands(),
		.getter = &LupinSettings::showCommandsButtonInMessageField,
		.setter = &LupinSettings::setShowCommandsButtonInMessageField,
		.icon = { &st::messageFieldCommandsIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showAutoDeleteButtonInMessageField"_q,
		.title = tr::lupin_MessageFieldElementTTL(),
		.getter = &LupinSettings::showAutoDeleteButtonInMessageField,
		.setter = &LupinSettings::setShowAutoDeleteButtonInMessageField,
		.icon = { &st::messageFieldTTLIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showEmojiButtonInMessageField"_q,
		.title = tr::lupin_MessageFieldElementEmoji(),
		.getter = &LupinSettings::showEmojiButtonInMessageField,
		.setter = &LupinSettings::setShowEmojiButtonInMessageField,
		.icon = { &st::messageFieldEmojiIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showMicrophoneButtonInMessageField"_q,
		.title = tr::lupin_MessageFieldElementVoice(),
		.getter = &LupinSettings::showMicrophoneButtonInMessageField,
		.setter = &LupinSettings::setShowMicrophoneButtonInMessageField,
		.icon = { &st::messageFieldVoiceIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showGiftButtonInMessageField"_q,
		.title = tr::lng_profile_action_short_gift(),
		.getter = &LupinSettings::showGiftButtonInMessageField,
		.setter = &LupinSettings::setShowGiftButtonInMessageField,
		.icon = { &st::settingsButtonIconGift },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showAiEditorButtonInMessageField"_q,
		.title = tr::lng_ai_compose_title(),
		.getter = &LupinSettings::showAiEditorButtonInMessageField,
		.setter = &LupinSettings::setShowAiEditorButtonInMessageField,
		.icon = { &st::messageFieldCocoonAiIcon },
	});

	lupin.addSectionDivider();
}

void BuildMessageFieldPopups(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lupin_MessageFieldPopupsHeader());

	lupin.addSettingToggle({
		.id = u"lupin/showAttachPopup"_q,
		.title = tr::lupin_MessageFieldElementAttach(),
		.getter = &LupinSettings::showAttachPopup,
		.setter = &LupinSettings::setShowAttachPopup,
		.icon = { &st::messageFieldAttachIcon },
	});
	lupin.addSettingToggle({
		.id = u"lupin/showEmojiPopup"_q,
		.title = tr::lupin_MessageFieldElementEmoji(),
		.getter = &LupinSettings::showEmojiPopup,
		.setter = &LupinSettings::setShowEmojiPopup,
		.icon = { &st::messageFieldEmojiIcon },
	});
}

const auto kMeta = BuildHelper({
	.id = LupinChats::Id(),
	.parentId = LupinMain::Id(),
	.title = &tr::lupin_CategoryChats,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	auto lupin = LupinSectionBuilder(builder);
	const auto previewState = std::make_shared<PreviewState>();

	builder.addSkip();
	BuildStickersAndEmoji(builder, lupin);
	BuildGroupsAndChannels(builder, lupin);
	BuildMarks(builder, lupin, previewState);
	BuildWideMessagesMultiplier(builder, lupin, previewState);
	BuildContextMenuElements(builder, lupin);
	BuildMessageFieldElements(builder, lupin);
	BuildMessageFieldPopups(builder, lupin);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> LupinChats::title() {
	return tr::lupin_CategoryChats();
}

LupinChats::LupinChats(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void LupinChats::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type LupinChatsId() {
	return LupinChats::Id();
}

} // namespace Settings
