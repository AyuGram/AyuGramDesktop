// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#pragma once

#include "ayu/libs/json.hpp"
#include "ayu/libs/json_ext.hpp"
#include "rpl/producer.h"

namespace AyuSettings {

class AyuGramSettings
{
public:
	AyuGramSettings();

	bool sendReadMessages;
	bool sendReadStories;
	bool sendOnlinePackets;
	bool sendUploadProgress;
	bool sendOfflinePacketAfterOnline;

	bool markReadAfterAction;
	bool useScheduledMessages;

	bool saveDeletedMessages;
	bool saveMessagesHistory;

	bool hideFromBlocked;

	bool disableAds;
	bool disableStories;
	bool disableCustomBackgrounds;
	bool collapseSimilarChannels;
	bool hideSimilarChannels;

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

	QString monoFont;

	bool hideNotificationCounters;
	bool hideAllChatsFolder;

	int showPeerId;
	bool showMessageSeconds;
	bool showMessageShot;

	bool stickerConfirmation;
	bool gifConfirmation;
	bool voiceConfirmation;

	void set_sendReadMessages(bool val);
	void set_sendReadStories(bool val);
	void set_sendOnlinePackets(bool val);
	void set_sendUploadProgress(bool val);
	void set_sendOfflinePacketAfterOnline(bool val);

	void set_ghostModeEnabled(bool val);

	void set_markReadAfterAction(bool val);
	void set_useScheduledMessages(bool val);

	void set_saveDeletedMessages(bool val);
	void set_saveMessagesHistory(bool val);

	void set_hideFromBlocked(bool val);

	void set_disableAds(bool val);
	void set_disableStories(bool val);
	void set_disableCustomBackgrounds(bool val);
	void set_collapseSimilarChannels(bool val);
	void set_hideSimilarChannels(bool val);

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

	void set_monoFont(QString val);

	void set_hideNotificationCounters(bool val);
	void set_hideAllChatsFolder(bool val);

	void set_showPeerId(int val);
	void set_showMessageSeconds(bool val);
	void set_showMessageShot(bool val);

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
	markReadAfterAction,
	useScheduledMessages,
	saveDeletedMessages,
	saveMessagesHistory,
	hideFromBlocked,
	disableAds,
	disableStories,
	disableCustomBackgrounds,
	collapseSimilarChannels,
	hideSimilarChannels,
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
	monoFont,
	hideNotificationCounters,
	hideAllChatsFolder,
	showPeerId,
	showMessageSeconds,
	showMessageShot,
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
