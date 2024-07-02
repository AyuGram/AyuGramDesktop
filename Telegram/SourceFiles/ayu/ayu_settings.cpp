// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
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

	// ~ Message edits & deletion history
	saveDeletedMessages = true;
	saveMessagesHistory = true;

	// ~ Message filters
	hideFromBlocked = false;

	// ~ QoL toggles
	disableAds = true;
	disableStories = false;
	disableCustomBackgrounds = true;
	collapseSimilarChannels = true;
	hideSimilarChannels = false;

	spoofWebviewAsAndroid = false;
	increaseWebviewHeight = false;
	increaseWebviewWidth = false;

	disableNotificationsDelay = false;
	localPremium = false;

	// ~ Customization
	appIcon = AyuAssets::DEFAULT_ICON;
	simpleQuotesAndReplies = true;
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

	showLReadToggleInDrawer = false;
	showSReadToggleInDrawer = true;
	showGhostToggleInDrawer = true;
	showStreamerToggleInDrawer = false;

	showGhostToggleInTray = true;
	showStreamerToggleInTray = false;

	monoFont = "";

	hideNotificationCounters = false;
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

void AyuGramSettings::set_sendReadMessages(bool val) {
	sendReadMessages = val;
	sendReadMessagesReactive = val;
}

void AyuGramSettings::set_sendReadStories(bool val) {
	sendReadStories = val;
	sendReadStoriesReactive = val;
}

void AyuGramSettings::set_sendOnlinePackets(bool val) {
	sendOnlinePackets = val;
	sendOnlinePacketsReactive = val;
}

void AyuGramSettings::set_sendUploadProgress(bool val) {
	sendUploadProgress = val;
	sendUploadProgressReactive = val;
}

void AyuGramSettings::set_sendOfflinePacketAfterOnline(bool val) {
	sendOfflinePacketAfterOnline = val;
	sendOfflinePacketAfterOnlineReactive = val;
}

void AyuGramSettings::set_ghostModeEnabled(bool val) {
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

void AyuGramSettings::set_markReadAfterAction(bool val) {
	markReadAfterAction = val;
}

void AyuGramSettings::set_useScheduledMessages(bool val) {
	useScheduledMessages = val;
}

void AyuGramSettings::set_saveDeletedMessages(bool val) {
	saveDeletedMessages = val;
}

void AyuGramSettings::set_saveMessagesHistory(bool val) {
	saveMessagesHistory = val;
}

void AyuGramSettings::set_hideFromBlocked(bool val) {
	hideFromBlocked = val;
}

void AyuGramSettings::set_disableAds(bool val) {
	disableAds = val;
}

void AyuGramSettings::set_disableStories(bool val) {
	disableStories = val;
}

void AyuGramSettings::set_disableCustomBackgrounds(bool val) {
	disableCustomBackgrounds = val;
}

void AyuGramSettings::set_collapseSimilarChannels(bool val) {
	collapseSimilarChannels = val;
}

void AyuGramSettings::set_hideSimilarChannels(bool val) {
	hideSimilarChannels = val;
}

void AyuGramSettings::set_spoofWebviewAsAndroid(bool val) {
	spoofWebviewAsAndroid = val;
}

void AyuGramSettings::set_increaseWebviewHeight(bool val) {
	increaseWebviewHeight = val;
}

void AyuGramSettings::set_increaseWebviewWidth(bool val) {
	increaseWebviewWidth = val;
}

void AyuGramSettings::set_disableNotificationsDelay(bool val) {
	disableNotificationsDelay = val;
}

void AyuGramSettings::set_localPremium(bool val) {
	localPremium = val;
}

void AyuGramSettings::set_appIcon(QString val) {
	appIcon = std::move(val);
}

void AyuGramSettings::set_simpleQuotesAndReplies(bool val) {
	simpleQuotesAndReplies = val;
}

void AyuGramSettings::set_deletedMark(QString val) {
	deletedMark = std::move(val);
	deletedMarkReactive = deletedMark;
}

void AyuGramSettings::set_editedMark(QString val) {
	editedMark = std::move(val);
	editedMarkReactive = editedMark;
}

void AyuGramSettings::set_recentStickersCount(int val) {
	recentStickersCount = val;
}

void AyuGramSettings::set_showReactionsPanelInContextMenu(int val) {
	showReactionsPanelInContextMenu = val;
}

void AyuGramSettings::set_showViewsPanelInContextMenu(int val) {
	showViewsPanelInContextMenu = val;
}

void AyuGramSettings::set_showHideMessageInContextMenu(int val) {
	showHideMessageInContextMenu = val;
}

void AyuGramSettings::set_showUserMessagesInContextMenu(int val) {
	showUserMessagesInContextMenu = val;
}

void AyuGramSettings::set_showMessageDetailsInContextMenu(int val) {
	showMessageDetailsInContextMenu = val;
}

void AyuGramSettings::set_showLReadToggleInDrawer(bool val) {
	showLReadToggleInDrawer = val;
}

void AyuGramSettings::set_showSReadToggleInDrawer(bool val) {
	showSReadToggleInDrawer = val;
}

void AyuGramSettings::set_showGhostToggleInDrawer(bool val) {
	showGhostToggleInDrawer = val;
}

void AyuGramSettings::set_showStreamerToggleInDrawer(bool val) {
	showStreamerToggleInDrawer = val;
}

void AyuGramSettings::set_showGhostToggleInTray(bool val) {
	showGhostToggleInTray = val;
}

void AyuGramSettings::set_showStreamerToggleInTray(bool val) {
	showStreamerToggleInTray = val;
}

void AyuGramSettings::set_monoFont(QString val) {
	monoFont = val;
}

void AyuGramSettings::set_showPeerId(int val) {
	showPeerId = val;
	showPeerIdReactive = val;
}

void AyuGramSettings::set_hideNotificationCounters(bool val) {
	hideNotificationCounters = val;
}

void AyuGramSettings::set_hideAllChatsFolder(bool val) {
	hideAllChatsFolder = val;
}

void AyuGramSettings::set_channelBottomButton(int val) {
	channelBottomButton = val;
}

void AyuGramSettings::set_showMessageSeconds(bool val) {
	showMessageSeconds = val;
}

void AyuGramSettings::set_showMessageShot(bool val) {
	showMessageShot = val;
}

void AyuGramSettings::set_stickerConfirmation(bool val) {
	stickerConfirmation = val;
}

void AyuGramSettings::set_gifConfirmation(bool val) {
	gifConfirmation = val;
}

void AyuGramSettings::set_voiceConfirmation(bool val) {
	voiceConfirmation = val;
}

bool get_ghostModeEnabled() {
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

}
