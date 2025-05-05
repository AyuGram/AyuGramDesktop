// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu_settings.h"

#include "ayu/ui/ayu_logo.h"

#include "lang_auto.h"
#include "core/application.h"

#include "rpl/lifetime.h"
#include "rpl/producer.h"
#include "rpl/variable.h"

#include <fstream>

#include "ayu_worker.h"
#include "window/window_controller.h"

using json = nlohmann::json;

namespace AyuSettings {

const std::string filename = "tdata/ayu_settings.json";

std::optional<AyuGramSettings> settings = std::nullopt;

rpl::variable<bool> sendReadMessagesReactive;
rpl::variable<bool> sendReadStoriesReactive;
rpl::variable<bool> sendOnlinePacketsReactive;
rpl::variable<bool> sendUploadProgressReactive;
rpl::variable<bool> sendOfflinePacketAfterOnlineReactive;

rpl::variable<bool> ghostModeEnabled;

rpl::variable<QString> deletedMarkReactive;
rpl::variable<QString> editedMarkReactive;

rpl::variable<int> showPeerIdReactive;

rpl::variable<bool> hideFromBlockedReactive;
rpl::event_stream<> historyUpdateReactive;

rpl::lifetime lifetime = rpl::lifetime();

bool ghostModeEnabled_util(const AyuGramSettings &settingsUtil) {
	return
		!settingsUtil.sendReadMessages
		&& !settingsUtil.sendReadStories
		&& !settingsUtil.sendOnlinePackets
		&& !settingsUtil.sendUploadProgress
		&& settingsUtil.sendOfflinePacketAfterOnline;
}

void initialize() {
	if (settings.has_value()) {
		return;
	}

	settings = AyuGramSettings();

	sendReadMessagesReactive.value() | rpl::filter(
		[=](bool val)
		{
			return (val != settings->sendReadMessages);
		}) | start_with_next(
		[=](bool val)
		{
			ghostModeEnabled =
				ghostModeEnabled_util(settings.value());
		},
		lifetime);
	// ..
	sendReadStoriesReactive.value() | rpl::filter(
		[=](bool val)
		{
			return (val != settings->sendReadStories);
		}) | start_with_next(
		[=](bool val)
		{
			ghostModeEnabled =
				ghostModeEnabled_util(settings.value());
		},
		lifetime);
	// ..
	sendOnlinePacketsReactive.value() | rpl::filter(
		[=](bool val)
		{
			return (val != settings->sendOnlinePackets);
		}) | start_with_next(
		[=](bool val)
		{
			ghostModeEnabled =
				ghostModeEnabled_util(settings
					.value());
		},
		lifetime);
	// ..
	sendUploadProgressReactive.value() | rpl::filter(
		[=](bool val)
		{
			return (val != settings->sendUploadProgress);
		}) | start_with_next(
		[=](bool val)
		{
			ghostModeEnabled =
				ghostModeEnabled_util(settings
					.value());
		},
		lifetime);
	// ..
	sendOfflinePacketAfterOnlineReactive.value() | rpl::filter(
		[=](bool val)
		{
			return (val
				!= settings->sendOfflinePacketAfterOnline);
		}) | start_with_next(
		[=](bool val)
		{
			ghostModeEnabled =
				ghostModeEnabled_util(
					settings.value());
		},
		lifetime);
}

void postinitialize() {
	sendReadMessagesReactive = settings->sendReadMessages;
	sendReadStoriesReactive = settings->sendReadStories;
	sendUploadProgressReactive = settings->sendUploadProgress;
	sendOfflinePacketAfterOnlineReactive = settings->sendOfflinePacketAfterOnline;
	sendOnlinePacketsReactive = settings->sendOnlinePackets;

	deletedMarkReactive = settings->deletedMark;
	editedMarkReactive = settings->editedMark;
	showPeerIdReactive = settings->showPeerId;

	hideFromBlockedReactive = settings->hideFromBlocked;

	ghostModeEnabled = ghostModeEnabled_util(settings.value());
}

AyuGramSettings &getInstance() {
	initialize();
	return settings.value();
}

void load() {
	std::ifstream file(filename);
	if (!file.good()) {
		return;
	}

	initialize();

	try {
		json p;
		file >> p;
		file.close();

		try {
			settings = p.get<AyuGramSettings>();
		} catch (...) {
			LOG(("AyuGramSettings: failed to parse settings file"));
		}
	} catch (...) {
		LOG(("AyuGramSettings: failed to read settings file (not json-like)"));
	}

	if (cGhost()) {
		settings->sendReadMessages = false;
		settings->sendReadStories = false;
		settings->sendOnlinePackets = false;
		settings->sendUploadProgress = false;
		settings->sendOfflinePacketAfterOnline = true;
	}

	postinitialize();
}

void save() {
	initialize();

	json p = settings.value();

	std::ofstream file;
	file.open(filename);
	file << p.dump(4);
	file.close();

	postinitialize();
}

AyuGramSettings::AyuGramSettings() {
	// ~ Ghost essentials
	sendReadMessages = true;
	sendReadStories = true;
	sendOnlinePackets = true;
	sendUploadProgress = true;
	sendOfflinePacketAfterOnline = false;

	markReadAfterAction = true;
	useScheduledMessages = false;
	sendWithoutSound = false;

	// ~ Message edits & deletion history
	saveDeletedMessages = true;
	saveMessagesHistory = true;

	saveForBots = false;

	// ~ Message filters
	hideFromBlocked = false;

	// ~ QoL toggles
	disableAds = true;
	disableStories = false;
	disableCustomBackgrounds = true;
	showOnlyAddedEmojisAndStickers = false;
	collapseSimilarChannels = true;
	hideSimilarChannels = false;

	wideMultiplier = 1.0;

	spoofWebviewAsAndroid = false;
	increaseWebviewHeight = false;
	increaseWebviewWidth = false;

	disableNotificationsDelay = false;
	localPremium = false;

	// ~ Customization
	appIcon =
#ifdef Q_OS_MAC
		AyuAssets::DEFAULT_MACOS_ICON
#else
		AyuAssets::DEFAULT_ICON
#endif
	;
	simpleQuotesAndReplies = true;
	replaceBottomInfoWithIcons = true;
	deletedMark = "🧹";
	editedMark = Core::IsAppLaunched() ? tr::lng_edited(tr::now) : QString("edited");
	recentStickersCount = 100;

	// context menu items
	// 0 - hide
	// 1 - show normally
	// 2 - show with SHIFT or CTRL pressed
	showReactionsPanelInContextMenu = 1;
	showViewsPanelInContextMenu = 1;
	showHideMessageInContextMenu = 0;
	showUserMessagesInContextMenu = 2;
	showMessageDetailsInContextMenu = 2;

	showAttachButtonInMessageField = true;
	showCommandsButtonInMessageField = true;
	showEmojiButtonInMessageField = true;
	showMicrophoneButtonInMessageField = true;
	showAutoDeleteButtonInMessageField = true;

	showAttachPopup = true;
	showEmojiPopup = true;

	showLReadToggleInDrawer = false;
	showSReadToggleInDrawer = true;
	showGhostToggleInDrawer = true;
	showStreamerToggleInDrawer = false;

	showGhostToggleInTray = true;
	showStreamerToggleInTray = false;

	monoFont = "";

	hideNotificationCounters = false;
	hideNotificationBadge = false;
	hideAllChatsFolder = false;

	/*
		 * channelBottomButton = 0 means "Hide"
		 * channelBottomButton = 1 means "Mute"/"Unmute"
		 * channelBottomButton = 2 means "Discuss" + fallback to "Mute"/"Unmute"
	*/
	channelBottomButton = 2;

	/*
		 * showPeerId = 0 means no ID shown
		 * showPeerId = 1 means ID shown as for Telegram API devs
		 * showPeerId = 2 means ID shown as for Bot API devs (-100)
	*/
	showPeerId = 2;
	showMessageSeconds = false;
	showMessageShot = true;

	// ~ Confirmations
	stickerConfirmation = false;
	gifConfirmation = false;
	voiceConfirmation = false;
}

void set_sendReadMessages(bool val) {
	settings->sendReadMessages = val;
	sendReadMessagesReactive = val;
}

void set_sendReadStories(bool val) {
	settings->sendReadStories = val;
	sendReadStoriesReactive = val;
}

void set_sendOnlinePackets(bool val) {
	settings->sendOnlinePackets = val;
	sendOnlinePacketsReactive = val;
}

void set_sendUploadProgress(bool val) {
	settings->sendUploadProgress = val;
	sendUploadProgressReactive = val;
}

void set_sendOfflinePacketAfterOnline(bool val) {
	settings->sendOfflinePacketAfterOnline = val;
	sendOfflinePacketAfterOnlineReactive = val;
}

void set_ghostModeEnabled(bool val) {
	set_sendReadMessages(!val);
	set_sendReadStories(!val);
	set_sendOnlinePackets(!val);
	set_sendUploadProgress(!val);
	set_sendOfflinePacketAfterOnline(val);

	if (const auto window = Core::App().activeWindow()) {
		if (const auto session = window->maybeSession()) {
			AyuWorker::markAsOnline(session); // mark as online to get offline instantly
		}
	}
}

void set_markReadAfterAction(bool val) {
	settings->markReadAfterAction = val;
}

void set_useScheduledMessages(bool val) {
	settings->useScheduledMessages = val;
}

void set_sendWithoutSound(bool val) {
	settings->sendWithoutSound = val;
}

void set_saveDeletedMessages(bool val) {
	settings->saveDeletedMessages = val;
}

void set_saveMessagesHistory(bool val) {
	settings->saveMessagesHistory = val;
}

void set_saveForBots(bool val) {
	settings->saveForBots = val;
}

void set_hideFromBlocked(bool val) {
	settings->hideFromBlocked = val;
	hideFromBlockedReactive = val;
}

void set_disableAds(bool val) {
	settings->disableAds = val;
}

void set_disableStories(bool val) {
	settings->disableStories = val;
}

void set_disableCustomBackgrounds(bool val) {
	settings->disableCustomBackgrounds = val;
}

void set_showOnlyAddedEmojisAndStickers(bool val) {
	settings->showOnlyAddedEmojisAndStickers = val;
}

void set_collapseSimilarChannels(bool val) {
	settings->collapseSimilarChannels = val;
}

void set_hideSimilarChannels(bool val) {
	settings->hideSimilarChannels = val;
}

void set_wideMultiplier(double val) {
	settings->wideMultiplier = val;
}

void set_spoofWebviewAsAndroid(bool val) {
	settings->spoofWebviewAsAndroid = val;
}

void set_increaseWebviewHeight(bool val) {
	settings->increaseWebviewHeight = val;
}

void set_increaseWebviewWidth(bool val) {
	settings->increaseWebviewWidth = val;
}

void set_disableNotificationsDelay(bool val) {
	settings->disableNotificationsDelay = val;
}

void set_localPremium(bool val) {
	settings->localPremium = val;
}

void set_appIcon(QString val) {
	settings->appIcon = std::move(val);
}

void set_simpleQuotesAndReplies(bool val) {
	settings->simpleQuotesAndReplies = val;
}

void set_replaceBottomInfoWithIcons(bool val) {
	settings->replaceBottomInfoWithIcons = val;
}

void set_deletedMark(QString val) {
	settings->deletedMark = std::move(val);
	deletedMarkReactive = settings->deletedMark;
}

void set_editedMark(QString val) {
	settings->editedMark = std::move(val);
	editedMarkReactive = settings->editedMark;
}

void set_recentStickersCount(int val) {
	settings->recentStickersCount = val;
}

void set_showReactionsPanelInContextMenu(int val) {
	settings->showReactionsPanelInContextMenu = val;
}

void set_showViewsPanelInContextMenu(int val) {
	settings->showViewsPanelInContextMenu = val;
}

void set_showHideMessageInContextMenu(int val) {
	settings->showHideMessageInContextMenu = val;
}

void set_showUserMessagesInContextMenu(int val) {
	settings->showUserMessagesInContextMenu = val;
}

void set_showMessageDetailsInContextMenu(int val) {
	settings->showMessageDetailsInContextMenu = val;
}

void set_showAttachButtonInMessageField(bool val) {
	settings->showAttachButtonInMessageField = val;
	triggerHistoryUpdate();
}

void set_showCommandsButtonInMessageField(bool val) {
	settings->showCommandsButtonInMessageField = val;
	triggerHistoryUpdate();
}

void set_showEmojiButtonInMessageField(bool val) {
	settings->showEmojiButtonInMessageField = val;
	triggerHistoryUpdate();
}

void set_showMicrophoneButtonInMessageField(bool val) {
	settings->showMicrophoneButtonInMessageField = val;
	triggerHistoryUpdate();
}

void set_showAutoDeleteButtonInMessageField(bool val) {
	settings->showAutoDeleteButtonInMessageField = val;
	triggerHistoryUpdate();
}

void set_showAttachPopup(bool val) {
	settings->showAttachPopup = val;
	triggerHistoryUpdate();
}

void set_showEmojiPopup(bool val) {
	settings->showEmojiPopup = val;
	triggerHistoryUpdate();
}

void set_showLReadToggleInDrawer(bool val) {
	settings->showLReadToggleInDrawer = val;
}

void set_showSReadToggleInDrawer(bool val) {
	settings->showSReadToggleInDrawer = val;
}

void set_showGhostToggleInDrawer(bool val) {
	settings->showGhostToggleInDrawer = val;
}

void set_showStreamerToggleInDrawer(bool val) {
	settings->showStreamerToggleInDrawer = val;
}

void set_showGhostToggleInTray(bool val) {
	settings->showGhostToggleInTray = val;
}

void set_showStreamerToggleInTray(bool val) {
	settings->showStreamerToggleInTray = val;
}

void set_monoFont(QString val) {
	settings->monoFont = val;
}

void set_showPeerId(int val) {
	settings->showPeerId = val;
	showPeerIdReactive = val;
}

void set_hideNotificationCounters(bool val) {
	settings->hideNotificationCounters = val;
}

void set_hideNotificationBadge(bool val) {
	settings->hideNotificationBadge = val;
}

void set_hideAllChatsFolder(bool val) {
	settings->hideAllChatsFolder = val;
}

void set_channelBottomButton(int val) {
	settings->channelBottomButton = val;
}

void set_showMessageSeconds(bool val) {
	settings->showMessageSeconds = val;
}

void set_showMessageShot(bool val) {
	settings->showMessageShot = val;
}

void set_stickerConfirmation(bool val) {
	settings->stickerConfirmation = val;
}

void set_gifConfirmation(bool val) {
	settings->gifConfirmation = val;
}

void set_voiceConfirmation(bool val) {
	settings->voiceConfirmation = val;
}

bool isUseScheduledMessages() {
	return isGhostModeActive() && settings->useScheduledMessages;
}

bool isGhostModeActive() {
	return ghostModeEnabled.current();
}

rpl::producer<QString> get_deletedMarkReactive() {
	return deletedMarkReactive.value();
}

rpl::producer<QString> get_editedMarkReactive() {
	return editedMarkReactive.value();
}

rpl::producer<int> get_showPeerIdReactive() {
	return showPeerIdReactive.value();
}

rpl::producer<bool> get_ghostModeEnabledReactive() {
	return ghostModeEnabled.value();
}

rpl::producer<bool> get_hideFromBlockedReactive() {
	return hideFromBlockedReactive.value();
}

void triggerHistoryUpdate() {
	historyUpdateReactive.fire({});
}

rpl::producer<> get_historyUpdateReactive() {
	return historyUpdateReactive.events();
}

}
