#pragma once

#define QS_HAS_JSON

#include <lang_auto.h>
#include "qserializer.h"

namespace AyuSettings {
    class AyuGramSettings : public QSerializer {
    Q_GADGET

    public:
        AyuGramSettings() {
            migrationVersion = 0;

            sendReadPackets = true;
            sendOnlinePackets = true;
            sendOfflinePacketAfterOnline = false;
            sendUploadProgress = true;
            useScheduledMessages = false;
            keepDeletedMessages = false;
            keepMessagesHistory = false;
            deletedMark = "🧹";
            editedMark = tr::lng_edited(tr::now);
            ghostMode = true;
        }

        QS_SERIALIZABLE

    QS_FIELD(int, migrationVersion)

    QS_FIELD(bool, sendReadPackets)

    QS_FIELD(bool, sendOnlinePackets)

    QS_FIELD(bool, sendOfflinePacketAfterOnline)

    QS_FIELD(bool, sendUploadProgress)

    QS_FIELD(bool, useScheduledMessages)

    QS_FIELD(bool, keepDeletedMessages)

    QS_FIELD(bool, keepMessagesHistory)

    QS_FIELD(QString, deletedMark)

    QS_FIELD(QString, editedMark)

    QS_FIELD(bool, ghostMode)

    public:
        void set_migrationVersion(int val) {
            migrationVersion = val;
        }

        void set_sendReadPackets(bool val) {
            sendReadPackets = val;
        }

        void set_sendOnlinePackets(bool val) {
            sendOnlinePackets = val;
        }

        void set_sendOfflinePacketAfterOnline(bool val) {
            sendOfflinePacketAfterOnline = val;
        }

        void set_sendUploadProgress(bool val) {
            sendUploadProgress = val;
        }

        void set_useScheduledMessages(bool val) {
            useScheduledMessages = val;
        }

        void set_keepDeletedMessages(bool val) {
            keepDeletedMessages = val;
        }

        void set_keepMessagesHistory(bool val) {
            keepMessagesHistory = val;
        }

        void set_deletedMark(QString val) {
            deletedMark = val;
        }

        void set_editedMark(QString val) {
            editedMark = val;
        }

        void set_ghostMode(bool val) {
            ghostMode = val;
        }
    };

    AyuGramSettings &getInstance();

    void load();

    void save();
}
