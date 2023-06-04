#include "ayu_settings.h"
#include "rpl/lifetime.h"

namespace AyuSettings {
    const QString filename = "tdata/ayu_settings.json";
    std::optional<AyuGramSettings> settings = std::nullopt;

    rpl::variable<bool> sendReadPacketsReactive;
    rpl::variable<bool> sendOnlinePacketsReactive;
    rpl::variable<bool> sendOfflinePacketAfterOnlineReactive;
    rpl::variable<bool> sendUploadProgressReactive;
    rpl::variable<bool> useScheduledMessagesReactive;
    rpl::variable<bool> keepDeletedMessagesReactive;
    rpl::variable<bool> keepMessagesHistoryReactive;
    rpl::variable<QString> deletedMarkReactive;
    rpl::variable<QString> editedMarkReactive;
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
        sendOfflinePacketAfterOnlineReactive = settings->sendOfflinePacketAfterOnline;
        sendUploadProgressReactive = settings->sendUploadProgress;
        useScheduledMessagesReactive = settings->useScheduledMessages;
        keepDeletedMessagesReactive = settings->keepDeletedMessages;
        keepMessagesHistoryReactive = settings->keepMessagesHistory;
        deletedMarkReactive = settings->deletedMark;
        editedMarkReactive = settings->editedMark;
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

    void AyuGramSettings::set_sendOfflinePacketAfterOnline(bool val) {
        sendOfflinePacketAfterOnline = val;
        sendOfflinePacketAfterOnlineReactive = val;
    }

    void AyuGramSettings::set_sendUploadProgress(bool val) {
        sendUploadProgress = val;
        sendUploadProgressReactive = val;
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

    rpl::variable<bool> get_sendReadPacketsReactive() {
        return sendReadPacketsReactive;
    }

    rpl::variable<bool> get_sendOnlinePacketsReactive() {
        return sendOnlinePacketsReactive;
    }

    rpl::variable<bool> get_sendOfflinePacketAfterOnlineReactive() {
        return sendOfflinePacketAfterOnlineReactive;
    }

    rpl::variable<bool> get_sendUploadProgressReactive() {
        return sendUploadProgressReactive;
    }

    rpl::variable<bool> get_useScheduledMessagesReactive() {
        return useScheduledMessagesReactive;
    }

    rpl::variable<bool> get_keepDeletedMessagesReactive() {
        return keepDeletedMessagesReactive;
    }

    rpl::variable<bool> get_keepMessagesHistoryReactive() {
        return keepMessagesHistoryReactive;
    }

    rpl::variable<QString> get_deletedMarkReactive() {
        return deletedMarkReactive;
    }

    rpl::variable<QString> get_editedMarkReactive() {
        return editedMarkReactive;
    }

    rpl::variable<bool> get_ghostModeEnabled() {
        return ghostModeEnabled;
    }
}
