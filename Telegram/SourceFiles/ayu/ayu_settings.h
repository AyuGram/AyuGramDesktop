// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "lang_auto.h"
#include "ayu/libs/json.hpp"
#include "ayu/libs/json_ext.hpp"
#include "ayu/ui/ayu_assets.h"
#include "rpl/producer.h"

namespace AyuSettings
{

class AyuGramSettings
{
public:
	AyuGramSettings()
	{
		// ~ Ghost essentials
		sendReadMessages = true;
		sendReadStories = true;
		sendOnlinePackets = true;
		sendUploadProgress = true;
		sendOfflinePacketAfterOnline = false;

		markReadAfterSend = true;
		markReadAfterReaction = true;
		markReadAfterPoll = true;
		useScheduledMessages = false;

		// ~ Message edits & deletion history
		saveDeletedMessages = false;
		saveMessagesHistory = true;

		// ~ QoL toggles
		disableAds = true;
		disableStories = false;
		collapseSimilarChannels = true;
		hideSimilarChannels = false;

		uploadSpeedBoost = false;

		disableNotificationsDelay = false;
		localPremium = false;
		copyUsernameAsLink = true;

		// ~ Customization
		appIcon = AyuAssets::DEFAULT_ICON;
		simpleQuotesAndReplies = true;
		deletedMark = "🧹";
		editedMark = tr::lng_edited(tr::now);
		recentStickersCount = 50;

		// context menu items
		// 0 - hide
		// 1 - show normally
		// 2 - show with SHIFT or CTRL pressed
		showReactionsPanelInContextMenu = 1;
		showViewsPanelInContextMenu = 1;
		showHideMessageInContextMenu = 0;
		showUserMessagesInContextMenu = 2;
		showMessageDetailsInContextMenu = 2;

		showLReadToggleInDrawer = true;
		showSReadToggleInDrawer = true;
		showGhostToggleInDrawer = true;
		showStreamerToggleInDrawer = false;

		showGhostToggleInTray = true;
		showStreamerToggleInTray = false;

		mainFont = "";
		monoFont = "";

		hideNotificationCounters = false;
		hideAllChatsFolder = false;

		/*
		 * showPeerId = 0 means no ID shown
		 * showPeerId = 1 means ID shown as for Telegram API devs
		 * showPeerId = 2 means ID shown as for Bot API devs (-100)
		 */
		showPeerId = 2;
		showMessageSeconds = false;

		// ~ Confirmations
		stickerConfirmation = false;
		gifConfirmation = false;
		voiceConfirmation = false;
	}

	bool sendReadMessages;
	bool sendReadStories;
	bool sendOnlinePackets;
	bool sendUploadProgress;
	bool sendOfflinePacketAfterOnline;

	bool markReadAfterSend;
	bool markReadAfterReaction;
	bool markReadAfterPoll;
	bool useScheduledMessages;

	bool saveDeletedMessages;
	bool saveMessagesHistory;

	bool disableAds;
	bool disableStories;
	bool collapseSimilarChannels;
	bool hideSimilarChannels;

	bool uploadSpeedBoost;

	bool disableNotificationsDelay;
	bool localPremium;
	bool copyUsernameAsLink;

	QString appIcon;
	bool simpleQuotesAndReplies;
	QString deletedMark;
	QString editedMark;
	int recentStickersCount;

	int showReactionsPanelInContextMenu;
	int showViewsPanelInContextMenu;
	int showHideMessageInContextMenu;
	int showUserMessagesInContextMenu;
	int showMessageDetailsInContextMenu;

	bool showLReadToggleInDrawer;
	bool showSReadToggleInDrawer;
	bool showGhostToggleInDrawer;
	bool showStreamerToggleInDrawer;

	bool showGhostToggleInTray;
	bool showStreamerToggleInTray;

	QString mainFont;
	QString monoFont;

	bool hideNotificationCounters;
	bool hideAllChatsFolder;

	int showPeerId;
	bool showMessageSeconds;

	bool stickerConfirmation;
	bool gifConfirmation;
	bool voiceConfirmation;

public:
	void set_sendReadMessages(bool val);
	void set_sendReadStories(bool val);
	void set_sendOnlinePackets(bool val);
	void set_sendUploadProgress(bool val);
	void set_sendOfflinePacketAfterOnline(bool val);

	void set_ghostModeEnabled(bool val);

	void set_markReadAfterSend(bool val);
	void set_markReadAfterReaction(bool val);
	void set_markReadAfterPoll(bool val);
	void set_useScheduledMessages(bool val);

	void set_saveDeletedMessages(bool val);
	void set_saveMessagesHistory(bool val);

	void set_disableAds(bool val);
	void set_disableStories(bool val);
	void set_collapseSimilarChannels(bool val);
	void set_hideSimilarChannels(bool val);

	void set_uploadSpeedBoost(bool val);

	void set_disableNotificationsDelay(bool val);
	void set_localPremium(bool val);
	void set_copyUsernameAsLink(bool val);

	void set_appIcon(QString val);
	void set_simpleQuotesAndReplies(bool val);
	void set_deletedMark(QString val);
	void set_editedMark(QString val);
	void set_recentStickersCount(int val);

	void set_showReactionsPanelInContextMenu(int val);
	void set_showViewsPanelInContextMenu(int val);
	void set_showHideMessageInContextMenu(int val);
	void set_showUserMessagesInContextMenu(int val);
	void set_showMessageDetailsInContextMenu(int val);

	void set_showLReadToggleInDrawer(bool val);
	void set_showSReadToggleInDrawer(bool val);
	void set_showGhostToggleInDrawer(bool val);
	void set_showStreamerToggleInDrawer(bool val);

	void set_showGhostToggleInTray(bool val);
	void set_showStreamerToggleInTray(bool val);

	void set_mainFont(QString val);
	void set_monoFont(QString val);

	void set_hideNotificationCounters(bool val);
	void set_hideAllChatsFolder(bool val);

	void set_showPeerId(int val);
	void set_showMessageSeconds(bool val);

	void set_stickerConfirmation(bool val);
	void set_gifConfirmation(bool val);
	void set_voiceConfirmation(bool val);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	AyuGramSettings,
	sendReadMessages,
	sendReadStories,
	sendOnlinePackets,
	sendUploadProgress,
	sendOfflinePacketAfterOnline,
	markReadAfterSend,
	markReadAfterReaction,
	markReadAfterPoll,
	useScheduledMessages,
	saveDeletedMessages,
	saveMessagesHistory,
	disableAds,
	disableStories,
	collapseSimilarChannels,
	hideSimilarChannels,
	uploadSpeedBoost,
	disableNotificationsDelay,
	localPremium,
	copyUsernameAsLink,
	appIcon,
	simpleQuotesAndReplies,
	deletedMark,
	editedMark,
	recentStickersCount,
	showReactionsPanelInContextMenu,
	showViewsPanelInContextMenu,
	showHideMessageInContextMenu,
	showUserMessagesInContextMenu,
	showMessageDetailsInContextMenu,
	showLReadToggleInDrawer,
	showSReadToggleInDrawer,
	showGhostToggleInDrawer,
	showStreamerToggleInDrawer,
	showGhostToggleInTray,
	showStreamerToggleInTray,
	mainFont,
	monoFont,
	hideNotificationCounters,
	hideAllChatsFolder,
	showPeerId,
	showMessageSeconds,
	stickerConfirmation,
	gifConfirmation,
	voiceConfirmation
);

AyuGramSettings &getInstance();

void load();
void save();

rpl::producer<QString> get_deletedMarkReactive();
rpl::producer<QString> get_editedMarkReactive();

rpl::producer<int> get_showPeerIdReactive();

bool get_ghostModeEnabled();
rpl::producer<bool> get_ghostModeEnabledReactive();

}
