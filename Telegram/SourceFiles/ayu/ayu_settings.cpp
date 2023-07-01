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

namespace AyuSettings {
    const QString filename = "tdata/ayu_settings.json";
    std::optional<AyuGramSettings> settings = std::nullopt;

    rpl::variable<bool> sendReadPacketsReactive;
    rpl::variable<bool> sendOnlinePacketsReactive;

    rpl::variable<QString> deletedMarkReactive;
    rpl::variable<QString> editedMarkReactive;
    rpl::variable<int> showPeerIdReactive;

    rpl::variable<bool> ghostModeEnabled;

    rpl::lifetime lifetime = rpl::lifetime();

    void initialize() {
        if (settings.has_value()) {
            return;
        }

        settings = std::optional(AyuGramSettings());

        sendReadPacketsReactive.value() | rpl::filter([=](bool val) {
            return (val != settings->sendReadPackets);
        }) | rpl::start_with_next([=](bool val) {
            ghostModeEnabled = !settings->sendReadPackets && !settings->sendOnlinePackets;
        }, lifetime);
        sendOnlinePacketsReactive.value() | rpl::filter([=](bool val) {
            return (val != settings->sendOnlinePackets);
        }) | rpl::start_with_next([=](bool val) {
            ghostModeEnabled = !settings->sendReadPackets && !settings->sendOnlinePackets;
        }, lifetime);
    }

    void postinitialize() {
        sendReadPacketsReactive = settings->sendReadPackets;
        sendOnlinePacketsReactive = settings->sendOnlinePackets;

        deletedMarkReactive = settings->deletedMark;
        editedMarkReactive = settings->editedMark;
        showPeerIdReactive = settings->showPeerId;

        ghostModeEnabled = !settings->sendReadPackets && !settings->sendOnlinePackets;
    }

    AyuGramSettings &getInstance() {
        initialize();
        return settings.value();
    }

    void load() {
        QFile file(filename);
        if (!file.exists()) {
            return;
        }
        file.open(QIODevice::ReadOnly);
        QByteArray json = file.readAll();
        file.close();

        initialize();
        settings->fromJson(json);
        postinitialize();
    }

    void save() {
        initialize();

        QByteArray json = settings->toRawJson();

        QFile file(filename);
        file.open(QIODevice::WriteOnly);
        file.write(json);
        file.close();

        postinitialize();
    }

    void AyuGramSettings::set_sendReadPackets(bool val) {
        sendReadPackets = val;
        sendReadPacketsReactive = val;
    }

    void AyuGramSettings::set_sendOnlinePackets(bool val) {
        sendOnlinePackets = val;
        sendOnlinePacketsReactive = val;
    }

    void AyuGramSettings::set_sendUploadProgress(bool val) {
        sendUploadProgress = val;
    }

    void AyuGramSettings::set_sendOfflinePacketAfterOnline(bool val) {
        sendOfflinePacketAfterOnline = val;
    }

    void AyuGramSettings::set_markReadAfterSend(bool val) {
        markReadAfterSend = val;
    }

    void AyuGramSettings::set_useScheduledMessages(bool val) {
        useScheduledMessages = val;
    }

    void AyuGramSettings::set_keepDeletedMessages(bool val) {
        keepDeletedMessages = val;
    }

    void AyuGramSettings::set_keepMessagesHistory(bool val) {
        keepMessagesHistory = val;
    }

    void AyuGramSettings::set_enableAds(bool val) {
        enableAds = val;
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

    void AyuGramSettings::set_showGhostToggleInDrawer(bool val) {
        showGhostToggleInDrawer = val;
    }

    void AyuGramSettings::set_showPeerId(int val) {
        showPeerId = val;
        showPeerIdReactive = val;
    }

    void AyuGramSettings::set_showMessageSeconds(bool val) {
        showMessageSeconds = val;
    }

    void AyuGramSettings::set_stickerConfirmation(bool val) {
        stickerConfirmation = val;
    }

    void AyuGramSettings::set_GIFConfirmation(bool val) {
        GIFConfirmation = val;
    }

    void AyuGramSettings::set_voiceConfirmation(bool val) {
        voiceConfirmation = val;
    }

    bool AyuGramSettings::get_ghostModeEnabled() const {
        return (!sendReadPackets
                && !sendOnlinePackets
                && !sendUploadProgress
                && sendOfflinePacketAfterOnline);
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
