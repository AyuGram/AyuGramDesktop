// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_settings.h"

#include "rpl/lifetime.h"
#include "rpl/producer.h"
#include "rpl/variable.h"

#include <fstream>

using json = nlohmann::json;

namespace AyuSettings
{

const std::string filename = "tdata/ayu_settings.json";

std::optional<AyuGramSettings> settings = std::nullopt;

rpl::variable<bool> sendReadMessagesReactive;

rpl::variable<bool> sendReadStoriesReactive;

rpl::variable<bool> sendOnlinePacketsReactive;

rpl::variable<bool> sendUploadProgressReactive;

rpl::variable<bool> sendOfflinePacketAfterOnlineReactive;

rpl::variable<QString> deletedMarkReactive;

rpl::variable<QString> editedMarkReactive;

rpl::variable<int> showPeerIdReactive;

rpl::variable<bool> ghostModeEnabled;

rpl::lifetime lifetime = rpl::lifetime();

bool ghostModeEnabled_util(AyuGramSettings &settingsUtil)
{
	return
		!settingsUtil.sendReadMessages
			&& !settingsUtil.sendReadStories
			&& !settingsUtil.sendOnlinePackets
			&& !settingsUtil.sendUploadProgress
			&& settingsUtil.sendOfflinePacketAfterOnline;
}

void initialize()
{
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
		}, lifetime);
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
		}, lifetime);
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
		}, lifetime);
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
		}, lifetime);
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
		}, lifetime);
}

void postinitialize()
{
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

AyuGramSettings &getInstance()
{
	initialize();
	return settings.value();
}

void load()
{
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
		}
		catch (...) {
			LOG(("AyuGramSettings: failed to parse settings file"));
		}
	}
	catch (...) {
		LOG(("AyuGramSettings: failed to read settings file (not json-like)"));
	}
	postinitialize();
}

void save()
{
	initialize();

	json p = settings.value();

	std::ofstream file;
	file.open(filename);
	file << p.dump(4);
	file.close();

	postinitialize();
}

void AyuGramSettings::set_sendReadMessages(bool val)
{
	sendReadMessages = val;
	sendReadMessagesReactive = val;
}

void AyuGramSettings::set_sendReadStories(bool val)
{
	sendReadStories = val;
	sendReadStoriesReactive = val;
}

void AyuGramSettings::set_sendOnlinePackets(bool val)
{
	sendOnlinePackets = val;
	sendOnlinePacketsReactive = val;
}

void AyuGramSettings::set_sendUploadProgress(bool val)
{
	sendUploadProgress = val;
	sendUploadProgressReactive = val;
}

void AyuGramSettings::set_sendOfflinePacketAfterOnline(bool val)
{
	sendOfflinePacketAfterOnline = val;
	sendOfflinePacketAfterOnlineReactive = val;
}

void AyuGramSettings::set_ghostModeEnabled(bool val)
{
	set_sendReadMessages(!val);
	set_sendReadStories(!val);
	set_sendOnlinePackets(!val);
	set_sendUploadProgress(!val);
	set_sendOfflinePacketAfterOnline(val);
}

void AyuGramSettings::set_markReadAfterSend(bool val)
{
	markReadAfterSend = val;
}

void AyuGramSettings::set_markReadAfterReaction(bool val)
{
	markReadAfterReaction = val;
}

void AyuGramSettings::set_markReadAfterPoll(bool val)
{
	markReadAfterPoll = val;
}

void AyuGramSettings::set_useScheduledMessages(bool val)
{
	useScheduledMessages = val;
}

void AyuGramSettings::set_saveDeletedMessages(bool val)
{
	saveDeletedMessages = val;
}

void AyuGramSettings::set_saveMessagesHistory(bool val)
{
	saveMessagesHistory = val;
}

void AyuGramSettings::set_disableAds(bool val)
{
	disableAds = val;
}

void AyuGramSettings::set_disableStories(bool val)
{
	disableStories = val;
}

void AyuGramSettings::set_collapseSimilarChannels(bool val)
{
	collapseSimilarChannels = val;
}

void AyuGramSettings::set_hideSimilarChannels(bool val)
{
	hideSimilarChannels = val;
}

void AyuGramSettings::set_disableNotificationsDelay(bool val)
{
	disableNotificationsDelay = val;
}

void AyuGramSettings::set_localPremium(bool val)
{
	localPremium = val;
}

void AyuGramSettings::set_copyUsernameAsLink(bool val)
{
	copyUsernameAsLink = val;
}

void AyuGramSettings::set_appIcon(QString val)
{
	appIcon = std::move(val);
}

void AyuGramSettings::set_simpleQuotesAndReplies(bool val)
{
	simpleQuotesAndReplies = val;
}

void AyuGramSettings::set_deletedMark(QString val)
{
	deletedMark = std::move(val);
	deletedMarkReactive = deletedMark;
}

void AyuGramSettings::set_editedMark(QString val)
{
	editedMark = std::move(val);
	editedMarkReactive = editedMark;
}

void AyuGramSettings::set_recentStickersCount(int val)
{
	recentStickersCount = val;
}

void AyuGramSettings::set_showReactionsPanelInContextMenu(int val)
{
	showReactionsPanelInContextMenu = val;
}

void AyuGramSettings::set_showViewsPanelInContextMenu(int val)
{
	showViewsPanelInContextMenu = val;
}

void AyuGramSettings::set_showHideMessageInContextMenu(int val)
{
	showHideMessageInContextMenu = val;
}

void AyuGramSettings::set_showUserMessagesInContextMenu(int val)
{
	showUserMessagesInContextMenu = val;
}

void AyuGramSettings::set_showMessageDetailsInContextMenu(int val)
{
	showMessageDetailsInContextMenu = val;
}

void AyuGramSettings::set_showLReadToggleInDrawer(bool val)
{
	showLReadToggleInDrawer = val;
}

void AyuGramSettings::set_showSReadToggleInDrawer(bool val)
{
	showSReadToggleInDrawer = val;
}

void AyuGramSettings::set_showGhostToggleInDrawer(bool val)
{
	showGhostToggleInDrawer = val;
}

void AyuGramSettings::set_showStreamerToggleInDrawer(bool val)
{
	showStreamerToggleInDrawer = val;
}

void AyuGramSettings::set_showGhostToggleInTray(bool val)
{
	showGhostToggleInTray = val;
}

void AyuGramSettings::set_showStreamerToggleInTray(bool val)
{
	showStreamerToggleInTray = val;
}

void AyuGramSettings::set_mainFont(QString val)
{
	mainFont = val;
}

void AyuGramSettings::set_monoFont(QString val)
{
	monoFont = val;
}

void AyuGramSettings::set_showPeerId(int val)
{
	showPeerId = val;
	showPeerIdReactive = val;
}

void AyuGramSettings::set_showMessageSeconds(bool val)
{
	showMessageSeconds = val;
}

void AyuGramSettings::set_hideAllChatsFolder(bool val)
{
	hideAllChatsFolder = val;
}

void AyuGramSettings::set_stickerConfirmation(bool val)
{
	stickerConfirmation = val;
}

void AyuGramSettings::set_gifConfirmation(bool val)
{
	gifConfirmation = val;
}

void AyuGramSettings::set_voiceConfirmation(bool val)
{
	voiceConfirmation = val;
}

bool get_ghostModeEnabled()
{
	return ghostModeEnabled.current();
}

rpl::producer<QString> get_deletedMarkReactive()
{
	return deletedMarkReactive.value();
}

rpl::producer<QString> get_editedMarkReactive()
{
	return editedMarkReactive.value();
}

rpl::producer<int> get_showPeerIdReactive()
{
	return showPeerIdReactive.value();
}

rpl::producer<bool> get_ghostModeEnabledReactive()
{
	return ghostModeEnabled.value();
}

}
