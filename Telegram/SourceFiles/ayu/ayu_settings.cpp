#include "ayu_settings.h"
#include "rpl/lifetime.h"
#include "rpl/producer.h"
#include "rpl/variable.h"

namespace AyuSettings {
    const QString filename = "tdata/ayu_settings.json";
    std::optional<AyuGramSettings> settings = std::nullopt;

    rpl::variable<bool> sendReadPacketsReactive;
    rpl::variable<bool> sendOnlinePacketsReactive;
    rpl::variable<bool> sendUploadProgressReactive;

    rpl::variable<bool> sendOfflinePacketAfterOnlineReactive;
    rpl::variable<bool> markReadAfterSendReactive;

    rpl::variable<bool> useScheduledMessagesReactive;

    rpl::variable<bool> keepDeletedMessagesReactive;
    rpl::variable<bool> keepMessagesHistoryReactive;

    rpl::variable<QString> deletedMarkReactive;
    rpl::variable<QString> editedMarkReactive;
    rpl::variable<bool> showGhostToggleInDrawerReactive;
    rpl::variable<int> showPeerIdReactive;
    rpl::variable<bool> showMessageSecondsReactive;

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
        sendUploadProgressReactive = settings->sendUploadProgress;

        sendOfflinePacketAfterOnlineReactive = settings->sendOfflinePacketAfterOnline;
        markReadAfterSendReactive = settings->markReadAfterSend;

        useScheduledMessagesReactive = settings->useScheduledMessages;

        keepDeletedMessagesReactive = settings->keepDeletedMessages;
        keepMessagesHistoryReactive = settings->keepMessagesHistory;

        deletedMarkReactive = settings->deletedMark;
        editedMarkReactive = settings->editedMark;
        showGhostToggleInDrawerReactive = settings->showGhostToggleInDrawer;
        showPeerIdReactive = settings->showPeerId;
        showMessageSecondsReactive = settings->showMessageSeconds;

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
        sendUploadProgressReactive = val;
    }

    void AyuGramSettings::set_sendOfflinePacketAfterOnline(bool val) {
        sendOfflinePacketAfterOnline = val;
        sendOfflinePacketAfterOnlineReactive = val;
    }

    void AyuGramSettings::set_markReadAfterSend(bool val) {
        markReadAfterSend = val;
        markReadAfterSendReactive = val;
    }

    void AyuGramSettings::set_useScheduledMessages(bool val) {
        useScheduledMessages = val;
        useScheduledMessagesReactive = val;
    }

    void AyuGramSettings::set_keepDeletedMessages(bool val) {
        keepDeletedMessages = val;
        keepDeletedMessagesReactive = val;
    }

    void AyuGramSettings::set_keepMessagesHistory(bool val) {
        keepMessagesHistory = val;
        keepMessagesHistoryReactive = val;
    }

    void AyuGramSettings::set_deletedMark(QString val) {
        deletedMark = std::move(val);
        deletedMarkReactive = deletedMark;
    }

    void AyuGramSettings::set_editedMark(QString val) {
        editedMark = std::move(val);
        editedMarkReactive = editedMark;
    }

    void AyuGramSettings::set_showGhostToggleInDrawer(bool val) {
        showGhostToggleInDrawer = val;
        showGhostToggleInDrawerReactive = val;
    }

    void AyuGramSettings::set_showPeerId(int val) {
        showPeerId = val;
        showPeerIdReactive = val;
    }

    void AyuGramSettings::set_showMessageSeconds(bool val) {
        showMessageSeconds = val;
        showMessageSecondsReactive = val;
    }

    rpl::producer<bool> get_sendReadPacketsReactive() {
        return sendReadPacketsReactive.value();
    }

    rpl::producer<bool> get_sendOnlinePacketsReactive() {
        return sendOnlinePacketsReactive.value();
    }

    rpl::producer<bool> get_sendUploadProgressReactive() {
        return sendUploadProgressReactive.value();
    }

    rpl::producer<bool> get_sendOfflinePacketAfterOnlineReactive() {
        return sendOfflinePacketAfterOnlineReactive.value();
    }

    rpl::producer<bool> get_markReadAfterSend() {
        return markReadAfterSendReactive.value();
    }

    rpl::producer<bool> get_useScheduledMessagesReactive() {
        return useScheduledMessagesReactive.value();
    }

    rpl::producer<bool> get_keepDeletedMessagesReactive() {
        return keepDeletedMessagesReactive.value();
    }

    rpl::producer<bool> get_keepMessagesHistoryReactive() {
        return keepMessagesHistoryReactive.value();
    }

    rpl::producer<QString> get_deletedMarkReactive() {
        return deletedMarkReactive.value();
    }

    rpl::producer<QString> get_editedMarkReactive() {
        return editedMarkReactive.value();
    }

    rpl::producer<bool> get_showGhostToggleInDrawerReactive() {
        return showGhostToggleInDrawerReactive.value();
    }

    rpl::producer<int> get_showPeerId() {
        return showPeerIdReactive.value();
    }

    rpl::producer<bool> get_showMessageSeconds() {
        return showMessageSecondsReactive.value();
    }

    rpl::producer<bool> get_ghostModeEnabled() {
        return ghostModeEnabled.value();
    }
}
