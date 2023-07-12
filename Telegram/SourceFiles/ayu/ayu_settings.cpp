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

using json = nlohmann::json;

namespace AyuSettings
{
	const QString filename = "tdata/ayu_settings.json";
	std::optional<AyuGramSettings> settings = std::nullopt;

	rpl::variable<bool> sendReadPacketsReactive;
	rpl::variable<bool> sendOnlinePacketsReactive;
	rpl::variable<bool> sendUploadProgressReactive;
	rpl::variable<bool> sendOfflinePacketAfterOnlineReactive;

	rpl::variable<QString> deletedMarkReactive;
	rpl::variable<QString> editedMarkReactive;
	rpl::variable<int> showPeerIdReactive;

	rpl::variable<bool> ghostModeEnabled;

	rpl::lifetime lifetime = rpl::lifetime();

	bool ghostModeEnabled_util(AyuGramSettings& settingsUtil)
	{
		return (!settingsUtil.sendReadPackets
			&& !settingsUtil.sendOnlinePackets
			&& !settingsUtil.sendUploadProgress
			&& settingsUtil.sendOfflinePacketAfterOnline);
	}

	void initialize()
	{
		if (settings.has_value())
		{
			return;
		}

		settings = AyuGramSettings();

		sendReadPacketsReactive.value() | rpl::filter([=](bool val)
		{
			return (val != settings->sendReadPackets);
		}) | start_with_next([=](bool val)
		{
			ghostModeEnabled = ghostModeEnabled_util(settings.value());
		}, lifetime);
		sendOnlinePacketsReactive.value() | rpl::filter([=](bool val)
		{
			return (val != settings->sendOnlinePackets);
		}) | start_with_next([=](bool val)
		{
			ghostModeEnabled = ghostModeEnabled_util(settings.value());
		}, lifetime);
		sendUploadProgressReactive.value() | rpl::filter([=](bool val)
		{
			return (val != settings->sendUploadProgress);
		}) | start_with_next([=](bool val)
		{
			ghostModeEnabled = ghostModeEnabled_util(settings.value());
		}, lifetime);
		sendOfflinePacketAfterOnlineReactive.value() | rpl::filter([=](bool val)
		{
			return (val != settings->sendOfflinePacketAfterOnline);
		}) | start_with_next([=](bool val)
		{
			ghostModeEnabled = ghostModeEnabled_util(settings.value());
		}, lifetime);
	}

	void postinitialize()
	{
		sendReadPacketsReactive = settings->sendReadPackets;
		sendOnlinePacketsReactive = settings->sendOnlinePackets;

		deletedMarkReactive = settings->deletedMark;
		editedMarkReactive = settings->editedMark;
		showPeerIdReactive = settings->showPeerId;

		ghostModeEnabled = ghostModeEnabled_util(settings.value());
	}

	AyuGramSettings& getInstance()
	{
		initialize();
		return settings.value();
	}

	void load()
	{
		QFile file(filename);
		if (!file.exists())
		{
			return;
		}
		file.open(QIODevice::ReadOnly);
		QByteArray data = file.readAll();
		file.close();

		initialize();
		json p = json::parse(data);
		try
		{
			settings = p.get<AyuGramSettings>();
		}
		catch (...)
		{
			LOG(("AyuGramSettings: failed to parse settings file"));
		}
		postinitialize();
	}

	void save()
	{
		initialize();

		json p = settings.value();

		QFile file(filename);
		file.open(QIODevice::WriteOnly);
		file.write(p.dump().c_str());
		file.close();

		postinitialize();
	}

	void AyuGramSettings::set_sendReadPackets(bool val)
	{
		sendReadPackets = val;
		sendReadPacketsReactive = val;
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

	void AyuGramSettings::set_markReadAfterSend(bool val)
	{
		markReadAfterSend = val;
	}

	void AyuGramSettings::set_useScheduledMessages(bool val)
	{
		useScheduledMessages = val;
	}

	void AyuGramSettings::set_keepDeletedMessages(bool val)
	{
		saveDeletedMessages = val;
	}

	void AyuGramSettings::set_keepMessagesHistory(bool val)
	{
		saveMessagesHistory = val;
	}

	void AyuGramSettings::set_enableAds(bool val)
	{
		enableAds = val;
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

	void AyuGramSettings::set_showGhostToggleInDrawer(bool val)
	{
		showGhostToggleInDrawer = val;
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

	void AyuGramSettings::set_stickerConfirmation(bool val)
	{
		stickerConfirmation = val;
	}

	void AyuGramSettings::set_GIFConfirmation(bool val)
	{
		GIFConfirmation = val;
	}

	void AyuGramSettings::set_voiceConfirmation(bool val)
	{
		voiceConfirmation = val;
	}

	void AyuGramSettings::set_copyUsernameAsLink(bool val)
	{
		copyUsernameAsLink = val;
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
