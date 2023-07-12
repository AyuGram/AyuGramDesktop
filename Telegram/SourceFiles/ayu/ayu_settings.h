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
#include "rpl/producer.h"

namespace AyuSettings
{
	class AyuGramSettings
	{
	public:
		AyuGramSettings()
		{
			// ~ Ghost essentials
			sendReadPackets = true;
			sendOnlinePackets = true;
			sendUploadProgress = true;

			sendOfflinePacketAfterOnline = false;
			markReadAfterSend = true;

			useScheduledMessages = false;

			// ~ Message edits & deletion history
			saveDeletedMessages = true;
			saveMessagesHistory = true;

			// ~ QoL toggles
			enableAds = false;

			// ~ Customization
			deletedMark = "🧹";
			editedMark = tr::lng_edited(tr::now);
			recentStickersCount = 20;
			showGhostToggleInDrawer = true;

			/*
			 * showPeerId = 0 means no ID shown
			 * showPeerId = 1 means ID shown as for Telegram API devs
			 * showPeerId = 2 means ID shown as for Bot API devs (-100)
			 */
			showPeerId = 2;

			showMessageSeconds = false;

			// ~ Beta functionality
			stickerConfirmation = false;
			GIFConfirmation = false;
			voiceConfirmation = false;

			// profile settings

			copyUsernameAsLink = true;
		}

		bool sendReadPackets;
		bool sendOnlinePackets;
		bool sendUploadProgress;
		bool sendOfflinePacketAfterOnline;
		bool markReadAfterSend;
		bool useScheduledMessages;
		bool saveDeletedMessages;
		bool saveMessagesHistory;
		bool enableAds;
		QString deletedMark;
		QString editedMark;
		int recentStickersCount;
		bool showGhostToggleInDrawer;
		int showPeerId;
		bool showMessageSeconds;
		bool stickerConfirmation;
		bool GIFConfirmation;
		bool voiceConfirmation;
		bool copyUsernameAsLink;

	public:
		void set_sendReadPackets(bool val);

		void set_sendOnlinePackets(bool val);

		void set_sendUploadProgress(bool val);

		void set_sendOfflinePacketAfterOnline(bool val);

		void set_markReadAfterSend(bool val);

		void set_useScheduledMessages(bool val);

		void set_keepDeletedMessages(bool val);

		void set_keepMessagesHistory(bool val);

		void set_enableAds(bool val);

		void set_deletedMark(QString val);

		void set_editedMark(QString val);

		void set_recentStickersCount(int val);

		void set_showGhostToggleInDrawer(bool val);

		void set_showPeerId(int val);

		void set_showMessageSeconds(bool val);

		void set_stickerConfirmation(bool val);

		void set_GIFConfirmation(bool val);

		void set_voiceConfirmation(bool val);

		void set_copyUsernameAsLink(bool val);
	};

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
		AyuGramSettings,
		sendReadPackets,
		sendOnlinePackets,
		sendUploadProgress,
		sendOfflinePacketAfterOnline,
		markReadAfterSend,
		useScheduledMessages,
		saveDeletedMessages,
		saveMessagesHistory,
		enableAds,
		deletedMark,
		editedMark,
		recentStickersCount,
		showGhostToggleInDrawer,
		showPeerId,
		showMessageSeconds,
		stickerConfirmation,
		GIFConfirmation,
		voiceConfirmation,
		copyUsernameAsLink
	);

	AyuGramSettings& getInstance();

	void load();

	void save();

	rpl::producer<QString> get_deletedMarkReactive();

	rpl::producer<QString> get_editedMarkReactive();

	rpl::producer<int> get_showPeerIdReactive();

	// computed fields
	bool get_ghostModeEnabled();

	rpl::producer<bool> get_ghostModeEnabledReactive();
}