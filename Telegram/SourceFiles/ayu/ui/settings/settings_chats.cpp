// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_chats.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/edit_mark_box.h"
#include "ayu/ui/components/message_preview.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <memory>

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

struct PreviewState {
	MessagePreview *widget = nullptr;
};

void BuildStickersAndEmoji(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::lng_settings_stickers_emoji());

	ayu.addSettingToggle({
		.id = u"ayu/showOnlyAddedEmojisAndStickers"_q,
		.title = tr::ayu_ShowOnlyAddedEmojisAndStickers(),
		.getter = &AyuSettings::showOnlyAddedEmojisAndStickers,
		.setter = &AyuSettings::setShowOnlyAddedEmojisAndStickers,
	});

	ayu.addCollapsibleToggle({
		.id = u"ayu/hideReactions"_q,
		.title = tr::ayu_HideReactions(),
		.checkboxes = {
			NestedEntry{
				tr::ayu_HideReactionsInChannels(tr::now),
				[] { return !AyuSettings::getInstance().showChannelReactions(); },
				[](bool v) { AyuSettings::getInstance().setShowChannelReactions(!v); }
			},
			NestedEntry{
				tr::ayu_HideReactionsInGroups(tr::now),
				[] { return !AyuSettings::getInstance().showGroupReactions(); },
				[](bool v) { AyuSettings::getInstance().setShowGroupReactions(!v); }
			},
			NestedEntry{
				tr::ayu_HideReactionsInPrivateChats(tr::now),
				[] { return !AyuSettings::getInstance().showPrivateChatReactions(); },
				[](bool v) { AyuSettings::getInstance().setShowPrivateChatReactions(!v); }
			}
		},
		.toggledWhenAll = false,
	});

	ayu.addSectionDivider();
}

void BuildRecentStickersLimit(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	ayu.addSlider({
		.id = u"ayu/recentStickersCount"_q,
		.title = tr::ayu_SettingsRecentStickersCount(),
		.steps = 200 + 1,
		.current = settings->recentStickersCount(),
		.indexToValue = [](int index) { return index; },
		.onChanged = nullptr,
		.onFinalChanged = [](int amount) {
			AyuSettings::getInstance().setRecentStickersCount(amount);
		},
		.formatLabel = [](int amount) { return QString::number(amount); },
	});

	ayu.addSectionDivider();
}

void BuildGroupsAndChannels(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::lng_premium_double_limits_subtitle_channels());

	ayu.addChooseButton({
		.id = u"ayu/channelBottomButton"_q,
		.altIds = { u"ayu/bottomButton"_q },
		.title = tr::ayu_ChannelBottomButton(),
		.boxTitle = tr::ayu_ChannelBottomButton(),
		.initialSelection = static_cast<int>(settings->channelBottomButton()),
		.options = {
			tr::ayu_ChannelBottomButtonHide(tr::now),
			tr::ayu_ChannelBottomButtonMute(tr::now),
			tr::ayu_ChannelBottomButtonDiscuss(tr::now),
		},
		.setter = [](int index) {
			AyuSettings::getInstance().setChannelBottomButton(
				static_cast<ChannelBottomButton>(index));
		},
	});

	ayu.addSettingToggle({
		.id = u"ayu/quickAdminShortcuts"_q,
		.title = tr::ayu_QuickAdminShortcuts(),
		.getter = &AyuSettings::quickAdminShortcuts,
		.setter = &AyuSettings::setQuickAdminShortcuts,
	});
	ayu.addSettingToggle({
		.id = u"ayu/showMessageShot"_q,
		.title = tr::ayu_SettingsShowMessageShot(),
		.getter = &AyuSettings::showMessageShot,
		.setter = &AyuSettings::setShowMessageShot,
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_SettingsShowMessageShotDescription());
	builder.addSkip();
}

void BuildMarks(
		SectionBuilder &builder,
		AyuSectionBuilder &ayu,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &AyuSettings::getInstance();
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

	ayu.addSettingToggle({
		.id = u"ayu/replaceBottomInfoWithIcons"_q,
		.altIds = { u"ayu/replaceEditedWithIcon"_q },
		.title = tr::ayu_ReplaceMarksWithIcons(),
		.getter = &AyuSettings::replaceBottomInfoWithIcons,
		.setter = &AyuSettings::setReplaceBottomInfoWithIcons,
	});

	builder.scope([&] {
		builder.addButton({
			.id = u"ayu/deletedMark"_q,
			.title = tr::ayu_DeletedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = AyuSettings::getInstance().deletedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::ayu_DeletedMarkText(),
					settings->deletedMark(),
					QString("🧹"),
					[=](const QString &value) {
						AyuSettings::getInstance().setDeletedMark(value);
					});
				Ui::show(std::move(box));
			},
		});

		builder.addButton({
			.id = u"ayu/editedMark"_q,
			.title = tr::ayu_EditedMarkText(),
			.st = &st::settingsButtonNoIcon,
			.label = AyuSettings::getInstance().editedMarkValue(),
			.onClick = [=] {
				auto box = Box<EditMarkBox>(
					tr::ayu_EditedMarkText(),
					settings->editedMark(),
					tr::lng_edited(tr::now),
					[=](const QString &value) {
						AyuSettings::getInstance().setEditedMark(value);
					});
				Ui::show(std::move(box));
			},
		});
	}, AyuSettings::getInstance().replaceBottomInfoWithIconsValue()
		| rpl::map([](bool v) { return !v; }));

	ayu.addSettingToggle({
		.id = u"ayu/removeMessageTail"_q,
		.title = tr::ayu_RemoveMessageTail(),
		.getter = &AyuSettings::removeMessageTail,
		.setter = &AyuSettings::setRemoveMessageTail,
	});

	ayu.addSettingToggle({
		.id = u"ayu/hideFastShare"_q,
		.altIds = { u"ayu/hideShareButton"_q },
		.title = tr::ayu_HideShareButton(),
		.getter = &AyuSettings::hideFastShare,
		.setter = &AyuSettings::setHideFastShare,
	});
	ayu.addSettingToggle({
		.id = u"ayu/simpleQuotesAndReplies"_q,
		.altIds = { u"ayu/disableColorfulReplies"_q, u"ayu/replyElements"_q },
		.title = tr::ayu_SimpleQuotesAndReplies(),
		.getter = &AyuSettings::simpleQuotesAndReplies,
		.setter = &AyuSettings::setSimpleQuotesAndReplies,
	});

	const auto semiTransparent = ayu.addSettingToggle({
		.id = u"ayu/semiTransparentDeletedMessages"_q,
		.altIds = { u"ayu/translucentDeletedMessages"_q },
		.title = tr::ayu_SemiTransparentDeletedMessages(),
		.getter = &AyuSettings::semiTransparentDeletedMessages,
		.setter = &AyuSettings::setSemiTransparentDeletedMessages,
	});
	if (semiTransparent) {
		ayu.addBetaBadge(semiTransparent);
	}

	ayu.addSectionDivider();
}

void BuildWideMessagesMultiplier(
		SectionBuilder &builder,
		AyuSectionBuilder &ayu,
		std::shared_ptr<PreviewState> previewState) {
	auto *settings = &AyuSettings::getInstance();

	constexpr auto kMinSize = 1.00;
	constexpr auto kStep = 0.05;

	const auto valueToIndex = [=](double value) {
		return static_cast<int>(std::round((value - kMinSize) / kStep));
	};

	const auto controller = builder.controller();
	ayu.addSlider({
		.id = u"ayu/messageBubbleRadius"_q,
		.title = tr::ayu_MessageBubbleRadius(),
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
			AyuSettings::getInstance().setMessageBubbleRadius(index);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [](int index) {
			return QString::number(index);
		},
	});

	ayu.addSectionDivider();

	ayu.addSlider({
		.id = u"ayu/wideMultiplier"_q,
		.title = tr::ayu_SettingsWideMultiplier(),
		.steps = 61, // (4.00 - 1.00) / 0.05 + 1
		.current = valueToIndex(settings->wideMultiplier()),
		.indexToValue = [](int index) { return index; },
		.onChanged = nullptr,
		.onFinalChanged = [=](int index) {
			AyuSettings::getInstance().setWideMultiplier(
				kMinSize + index * kStep);
			ShowRestartPrompt(controller);
		},
		.formatLabel = [=](int index) {
			return QString::number(kMinSize + index * kStep, 'f', 2);
		},
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_SettingsWideMultiplierDescription());
	builder.addSkip();
}

void BuildContextMenuElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::ayu_ContextMenuElementsHeader());

	const auto options = std::vector{
		tr::ayu_SettingsContextMenuItemHidden(tr::now),
		tr::ayu_SettingsContextMenuItemShown(tr::now),
		tr::ayu_SettingsContextMenuItemExtended(tr::now),
	};

	ayu.addChooseButton({
		.id = u"ayu/showReactionsPanelInContextMenu"_q,
		.title = tr::ayu_SettingsContextMenuReactionsPanel(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showReactionsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowReactionsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconReactions },
	});
	ayu.addChooseButton({
		.id = u"ayu/showViewsPanelInContextMenu"_q,
		.title = tr::ayu_SettingsContextMenuViewsPanel(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showViewsPanelInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowViewsPanelInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconShowInChat },
	});
	ayu.addChooseButton({
		.id = u"ayu/showHideMessageInContextMenu"_q,
		.title = tr::ayu_ContextHideMessage(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showHideMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowHideMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconClear },
	});
	ayu.addChooseButton({
		.id = u"ayu/showUserMessagesInContextMenu"_q,
		.title = tr::ayu_UserMessagesMenuText(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showUserMessagesInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowUserMessagesInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconTTL },
	});
	ayu.addChooseButton({
		.id = u"ayu/showMessageDetailsInContextMenu"_q,
		.title = tr::ayu_MessageDetailsPC(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showMessageDetailsInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowMessageDetailsInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::menuIconInfo },
	});
	ayu.addChooseButton({
		.id = u"ayu/showRepeatMessageInContextMenu"_q,
		.title = tr::ayu_RepeatMessage(),
		.boxTitle = tr::ayu_SettingsContextMenuTitle(),
		.initialSelection = static_cast<int>(settings->showRepeatMessageInContextMenu()),
		.options = options,
		.setter = [](int i) { AyuSettings::getInstance().setShowRepeatMessageInContextMenu(static_cast<ContextMenuVisibility>(i)); },
		.icon = { &st::ayuRepeatMenuIcon },
	});
	if (settings->filtersEnabled()) {
		ayu.addChooseButton({
			.id = u"ayu/showAddFilterInContextMenu"_q,
			.title = tr::ayu_RegexFilterQuickAdd(),
			.boxTitle = tr::ayu_SettingsContextMenuTitle(),
			.initialSelection = static_cast<int>(settings->showAddFilterInContextMenu()),
			.options = options,
			.setter = [](int i) { AyuSettings::getInstance().setShowAddFilterInContextMenu(static_cast<ContextMenuVisibility>(i)); },
			.icon = { &st::menuIconAddToFolder },
		});
	}

	builder.addSkip();
	builder.addDividerText(tr::ayu_SettingsContextMenuDescription());
	builder.addSkip();
}

void BuildMessageFieldElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_MessageFieldElementsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/showAttachButtonInMessageField"_q,
		.title = tr::ayu_MessageFieldElementAttach(),
		.getter = &AyuSettings::showAttachButtonInMessageField,
		.setter = &AyuSettings::setShowAttachButtonInMessageField,
		.icon = { &st::messageFieldAttachIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showCommandsButtonInMessageField"_q,
		.title = tr::ayu_MessageFieldElementCommands(),
		.getter = &AyuSettings::showCommandsButtonInMessageField,
		.setter = &AyuSettings::setShowCommandsButtonInMessageField,
		.icon = { &st::messageFieldCommandsIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showAutoDeleteButtonInMessageField"_q,
		.title = tr::ayu_MessageFieldElementTTL(),
		.getter = &AyuSettings::showAutoDeleteButtonInMessageField,
		.setter = &AyuSettings::setShowAutoDeleteButtonInMessageField,
		.icon = { &st::messageFieldTTLIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showEmojiButtonInMessageField"_q,
		.title = tr::ayu_MessageFieldElementEmoji(),
		.getter = &AyuSettings::showEmojiButtonInMessageField,
		.setter = &AyuSettings::setShowEmojiButtonInMessageField,
		.icon = { &st::messageFieldEmojiIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showMicrophoneButtonInMessageField"_q,
		.title = tr::ayu_MessageFieldElementVoice(),
		.getter = &AyuSettings::showMicrophoneButtonInMessageField,
		.setter = &AyuSettings::setShowMicrophoneButtonInMessageField,
		.icon = { &st::messageFieldVoiceIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showGiftButtonInMessageField"_q,
		.title = tr::lng_profile_action_short_gift(),
		.getter = &AyuSettings::showGiftButtonInMessageField,
		.setter = &AyuSettings::setShowGiftButtonInMessageField,
		.icon = { &st::settingsButtonIconGift },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showAiEditorButtonInMessageField"_q,
		.title = tr::lng_ai_compose_title(),
		.getter = &AyuSettings::showAiEditorButtonInMessageField,
		.setter = &AyuSettings::setShowAiEditorButtonInMessageField,
		.icon = { &st::messageFieldCocoonAiIcon },
	});

	ayu.addSectionDivider();
}

void BuildMessageFieldPopups(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_MessageFieldPopupsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/showAttachPopup"_q,
		.title = tr::ayu_MessageFieldElementAttach(),
		.getter = &AyuSettings::showAttachPopup,
		.setter = &AyuSettings::setShowAttachPopup,
		.icon = { &st::messageFieldAttachIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showEmojiPopup"_q,
		.title = tr::ayu_MessageFieldElementEmoji(),
		.getter = &AyuSettings::showEmojiPopup,
		.setter = &AyuSettings::setShowEmojiPopup,
		.icon = { &st::messageFieldEmojiIcon },
	});
}

const auto kMeta = BuildHelper({
	.id = AyuChats::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::ayu_CategoryChats,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	const auto previewState = std::make_shared<PreviewState>();

	builder.addSkip();
	BuildStickersAndEmoji(builder, ayu);
	BuildRecentStickersLimit(builder, ayu);
	BuildGroupsAndChannels(builder, ayu);
	BuildMarks(builder, ayu, previewState);
	BuildWideMessagesMultiplier(builder, ayu, previewState);
	BuildContextMenuElements(builder, ayu);
	BuildMessageFieldElements(builder, ayu);
	BuildMessageFieldPopups(builder, ayu);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AyuChats::title() {
	return tr::ayu_CategoryChats();
}

AyuChats::AyuChats(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuChats::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuChatsId() {
	return AyuChats::Id();
}

} // namespace Settings
