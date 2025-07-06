// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "ayu/libs/json.hpp"
#include "ayu/libs/json_ext.hpp"

// json.hpp in some build environments may not provide helper macros.
//  To ensure successful compilation, define them here when missing.
#ifndef NLOHMANN_JSON_TO
#define NLOHMANN_JSON_TO(v1) nlohmann_json_j[#v1] = nlohmann_json_t.v1;
#endif
#ifndef NLOHMANN_JSON_FROM
#define NLOHMANN_JSON_FROM(v1) nlohmann_json_j.at(#v1).get_to(nlohmann_json_t.v1);
#endif
#ifndef NLOHMANN_JSON_FROM_WITH_DEFAULT
#define NLOHMANN_JSON_FROM_WITH_DEFAULT(v1) \
    nlohmann_json_t.v1 = nlohmann_json_j.value(#v1, nlohmann_json_default_obj.v1);
#endif
#ifndef NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Type, ...)               \
    inline void to_json(nlohmann::json& nlohmann_json_j,                       \
                        const Type& nlohmann_json_t) {                          \
        NLOHMANN_JSON_EXPAND(                                                   \
            NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__))                 \
    }                                                                           \
    inline void from_json(const nlohmann::json& nlohmann_json_j,                \
                          Type& nlohmann_json_t) {                              \
        const Type nlohmann_json_default_obj{};                                 \
        NLOHMANN_JSON_EXPAND(                                                   \
            NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM_WITH_DEFAULT, __VA_ARGS__))   \
    }
#endif
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
	bool sendWithoutSound;

	bool saveDeletedMessages;
	bool saveMessagesHistory;

	bool saveForBots;

	bool hideFromBlocked;

	bool disableAds;
	bool disableStories;
	bool disableCustomBackgrounds;
	bool showOnlyAddedEmojisAndStickers;
	bool collapseSimilarChannels;
	bool hideSimilarChannels;

	double wideMultiplier;

	bool spoofWebviewAsAndroid;
	bool increaseWebviewHeight;
	bool increaseWebviewWidth;

	bool disableNotificationsDelay;
	bool localPremium;
	bool hideChannelReactions;
	bool hideGroupReactions;

	QString appIcon;
	bool simpleQuotesAndReplies;
	bool replaceBottomInfoWithIcons;
	QString deletedMark;
	QString editedMark;
	int recentStickersCount;

	int showReactionsPanelInContextMenu;
	int showViewsPanelInContextMenu;
	int showHideMessageInContextMenu;
	int showUserMessagesInContextMenu;
	int showMessageDetailsInContextMenu;

	bool showAttachButtonInMessageField;
	bool showCommandsButtonInMessageField;
	bool showEmojiButtonInMessageField;
	bool showMicrophoneButtonInMessageField;
	bool showAutoDeleteButtonInMessageField;

	bool showAttachPopup;
	bool showEmojiPopup;

	bool showLReadToggleInDrawer;
	bool showSReadToggleInDrawer;
	bool showGhostToggleInDrawer;
	bool showStreamerToggleInDrawer;

	bool showGhostToggleInTray;
	bool showStreamerToggleInTray;

	QString monoFont;

	bool hideNotificationCounters;
	bool hideNotificationBadge;
	bool hideAllChatsFolder;

	int channelBottomButton;

	int showPeerId;
	bool showMessageSeconds;
	bool showMessageShot;

	bool stickerConfirmation;
	bool gifConfirmation;
	bool voiceConfirmation;
};

void set_sendReadMessages(bool val);
void set_sendReadStories(bool val);
void set_sendOnlinePackets(bool val);
void set_sendUploadProgress(bool val);
void set_sendOfflinePacketAfterOnline(bool val);

void set_ghostModeEnabled(bool val);

void set_markReadAfterAction(bool val);
void set_useScheduledMessages(bool val);
void set_sendWithoutSound(bool val);

void set_saveDeletedMessages(bool val);
void set_saveMessagesHistory(bool val);

void set_saveForBots(bool val);

void set_hideFromBlocked(bool val);

void set_disableAds(bool val);
void set_disableStories(bool val);
void set_disableCustomBackgrounds(bool val);
void set_showOnlyAddedEmojisAndStickers(bool val);
void set_collapseSimilarChannels(bool val);
void set_hideSimilarChannels(bool val);

void set_wideMultiplier(double val);

void set_spoofWebviewAsAndroid(bool val);
void set_increaseWebviewHeight(bool val);
void set_increaseWebviewWidth(bool val);

void set_disableNotificationsDelay(bool val);
void set_localPremium(bool val);
void set_hideChannelReactions(bool val);
void set_hideGroupReactions(bool val);

void set_appIcon(const QString &val);
void set_simpleQuotesAndReplies(bool val);
void set_replaceBottomInfoWithIcons(bool val);
void set_deletedMark(const QString &val);
void set_editedMark(const QString &val);
void set_recentStickersCount(int val);

void set_showReactionsPanelInContextMenu(int val);
void set_showViewsPanelInContextMenu(int val);
void set_showHideMessageInContextMenu(int val);
void set_showUserMessagesInContextMenu(int val);
void set_showMessageDetailsInContextMenu(int val);

void set_showAttachButtonInMessageField(bool val);
void set_showCommandsButtonInMessageField(bool val);
void set_showEmojiButtonInMessageField(bool val);
void set_showMicrophoneButtonInMessageField(bool val);
void set_showAutoDeleteButtonInMessageField(bool val);

void set_showAttachPopup(bool val);
void set_showEmojiPopup(bool val);

void set_showLReadToggleInDrawer(bool val);
void set_showSReadToggleInDrawer(bool val);
void set_showGhostToggleInDrawer(bool val);
void set_showStreamerToggleInDrawer(bool val);

void set_showGhostToggleInTray(bool val);
void set_showStreamerToggleInTray(bool val);

void set_monoFont(const QString &val);

void set_hideNotificationCounters(bool val);
void set_hideNotificationBadge(bool val);
void set_hideAllChatsFolder(bool val);

void set_channelBottomButton(int val);

void set_showPeerId(int val);
void set_showMessageSeconds(bool val);
void set_showMessageShot(bool val);

void set_stickerConfirmation(bool val);
void set_gifConfirmation(bool val);
void set_voiceConfirmation(bool val);

inline void to_json(nlohmann::json &nlohmann_json_j, const AyuGramSettings &nlohmann_json_t) {
	NLOHMANN_JSON_TO(sendReadMessages)
	NLOHMANN_JSON_TO(sendReadStories)
	NLOHMANN_JSON_TO(sendOnlinePackets)
	NLOHMANN_JSON_TO(sendUploadProgress)
	NLOHMANN_JSON_TO(sendOfflinePacketAfterOnline)
	NLOHMANN_JSON_TO(markReadAfterAction)
	NLOHMANN_JSON_TO(useScheduledMessages)
	NLOHMANN_JSON_TO(sendWithoutSound)
	NLOHMANN_JSON_TO(saveDeletedMessages)
	NLOHMANN_JSON_TO(saveMessagesHistory)
	NLOHMANN_JSON_TO(saveForBots)
	NLOHMANN_JSON_TO(hideFromBlocked)
	NLOHMANN_JSON_TO(disableAds)
	NLOHMANN_JSON_TO(disableStories)
	NLOHMANN_JSON_TO(disableCustomBackgrounds)
	NLOHMANN_JSON_TO(showOnlyAddedEmojisAndStickers)
	NLOHMANN_JSON_TO(collapseSimilarChannels)
	NLOHMANN_JSON_TO(hideSimilarChannels)
	NLOHMANN_JSON_TO(wideMultiplier)
	NLOHMANN_JSON_TO(spoofWebviewAsAndroid)
	NLOHMANN_JSON_TO(increaseWebviewHeight)
	NLOHMANN_JSON_TO(increaseWebviewWidth)
	NLOHMANN_JSON_TO(disableNotificationsDelay)
	NLOHMANN_JSON_TO(localPremium)
	NLOHMANN_JSON_TO(appIcon)
	NLOHMANN_JSON_TO(simpleQuotesAndReplies)
	NLOHMANN_JSON_TO(replaceBottomInfoWithIcons)
	NLOHMANN_JSON_TO(deletedMark)
	NLOHMANN_JSON_TO(editedMark)
	NLOHMANN_JSON_TO(recentStickersCount)
	NLOHMANN_JSON_TO(showReactionsPanelInContextMenu)
	NLOHMANN_JSON_TO(showViewsPanelInContextMenu)
	NLOHMANN_JSON_TO(showHideMessageInContextMenu)
	NLOHMANN_JSON_TO(showUserMessagesInContextMenu)
	NLOHMANN_JSON_TO(showMessageDetailsInContextMenu)
	NLOHMANN_JSON_TO(showAttachButtonInMessageField)
	NLOHMANN_JSON_TO(showCommandsButtonInMessageField)
	NLOHMANN_JSON_TO(showEmojiButtonInMessageField)
	NLOHMANN_JSON_TO(showMicrophoneButtonInMessageField)
	NLOHMANN_JSON_TO(showAutoDeleteButtonInMessageField)
	NLOHMANN_JSON_TO(showAttachPopup)
	NLOHMANN_JSON_TO(showEmojiPopup)
	NLOHMANN_JSON_TO(showLReadToggleInDrawer)
	NLOHMANN_JSON_TO(showSReadToggleInDrawer)
	NLOHMANN_JSON_TO(showGhostToggleInDrawer)
	NLOHMANN_JSON_TO(showStreamerToggleInDrawer)
	NLOHMANN_JSON_TO(showGhostToggleInTray)
	NLOHMANN_JSON_TO(showStreamerToggleInTray)
	NLOHMANN_JSON_TO(monoFont)
	NLOHMANN_JSON_TO(hideChannelReactions)
	NLOHMANN_JSON_TO(hideGroupReactions)
	NLOHMANN_JSON_TO(hideNotificationCounters)
	NLOHMANN_JSON_TO(hideNotificationBadge)
	NLOHMANN_JSON_TO(hideAllChatsFolder)
	NLOHMANN_JSON_TO(channelBottomButton)
	NLOHMANN_JSON_TO(showPeerId)
	NLOHMANN_JSON_TO(showMessageSeconds)
	NLOHMANN_JSON_TO(showMessageShot)
	NLOHMANN_JSON_TO(stickerConfirmation)
	NLOHMANN_JSON_TO(gifConfirmation)
	NLOHMANN_JSON_TO(voiceConfirmation)
}

inline void from_json(const nlohmann::json &nlohmann_json_j, AyuGramSettings &nlohmann_json_t) {
	const AyuGramSettings nlohmann_json_default_obj{};
	NLOHMANN_JSON_FROM_WITH_DEFAULT(sendReadMessages)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(sendReadStories)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(sendOnlinePackets)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(sendUploadProgress)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(sendOfflinePacketAfterOnline)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(markReadAfterAction)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(useScheduledMessages)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(sendWithoutSound)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(saveDeletedMessages)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(saveMessagesHistory)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(saveForBots)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(hideFromBlocked)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(disableAds)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(disableStories)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(disableCustomBackgrounds)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showOnlyAddedEmojisAndStickers)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(collapseSimilarChannels)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(hideSimilarChannels)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(wideMultiplier)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(spoofWebviewAsAndroid)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(increaseWebviewHeight)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(increaseWebviewWidth)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(disableNotificationsDelay)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(localPremium)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(appIcon)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(simpleQuotesAndReplies)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(replaceBottomInfoWithIcons)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(deletedMark)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(editedMark)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(recentStickersCount)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showReactionsPanelInContextMenu)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showViewsPanelInContextMenu)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showHideMessageInContextMenu)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showUserMessagesInContextMenu)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showMessageDetailsInContextMenu)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showAttachButtonInMessageField)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showCommandsButtonInMessageField)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showEmojiButtonInMessageField)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showMicrophoneButtonInMessageField)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showAutoDeleteButtonInMessageField)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showAttachPopup)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showEmojiPopup)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showLReadToggleInDrawer)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showSReadToggleInDrawer)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showGhostToggleInDrawer)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showStreamerToggleInDrawer)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showGhostToggleInTray)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showStreamerToggleInTray)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(monoFont)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(hideChannelReactions)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(hideGroupReactions)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(hideNotificationCounters)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(hideNotificationBadge)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(hideAllChatsFolder)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(channelBottomButton)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showPeerId)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showMessageSeconds)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(showMessageShot)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(stickerConfirmation)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(gifConfirmation)
	NLOHMANN_JSON_FROM_WITH_DEFAULT(voiceConfirmation)
}

AyuGramSettings &getInstance();

void load();
void save();

rpl::producer<QString> get_deletedMarkReactive();
rpl::producer<QString> get_editedMarkReactive();

rpl::producer<int> get_showPeerIdReactive();

bool isGhostModeActive();
bool isUseScheduledMessages();

rpl::producer<bool> get_ghostModeEnabledReactive();

rpl::producer<bool> get_hideFromBlockedReactive();

void triggerHistoryUpdate();
rpl::producer<> get_historyUpdateReactive();

}
