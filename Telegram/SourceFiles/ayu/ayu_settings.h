#pragma once

#include "rpl/producer.h"

#define QS_HAS_JSON

#include "lang_auto.h"
#include "qserializer.h"

namespace AyuSettings {
    class AyuGramSettings : public QSerializer {
    Q_GADGET

    public:
        AyuGramSettings() {
            sendReadPackets = true;
            sendOnlinePackets = true;
            sendOfflinePacketAfterOnline = false;
            sendUploadProgress = true;
            useScheduledMessages = false;
            keepDeletedMessages = false;
            keepMessagesHistory = false;
            deletedMark = "🧹";
            editedMark = tr::lng_edited(tr::now);
        }

        QS_SERIALIZABLE

    QS_FIELD(bool, sendReadPackets)

    QS_FIELD(bool, sendOnlinePackets)

    QS_FIELD(bool, sendOfflinePacketAfterOnline)

    QS_FIELD(bool, sendUploadProgress)

    QS_FIELD(bool, useScheduledMessages)

    QS_FIELD(bool, keepDeletedMessages)

    QS_FIELD(bool, keepMessagesHistory)

    QS_FIELD(QString, deletedMark)

    QS_FIELD(QString, editedMark)

    public:
        void set_sendReadPackets(bool val);
        void set_sendOnlinePackets(bool val);
        void set_sendOfflinePacketAfterOnline(bool val);
        void set_sendUploadProgress(bool val);
        void set_useScheduledMessages(bool val);
        void set_keepDeletedMessages(bool val);
        void set_keepMessagesHistory(bool val);
        void set_deletedMark(QString val);
        void set_editedMark(QString val);
    };

    AyuGramSettings &getInstance();

    void load();

    void save();

    rpl::variable<bool> get_sendReadPacketsReactive();
    rpl::variable<bool> get_sendOnlinePacketsReactive();
    rpl::variable<bool> get_sendOfflinePacketAfterOnlineReactive();
    rpl::variable<bool> get_sendUploadProgressReactive();
    rpl::variable<bool> get_useScheduledMessagesReactive();
    rpl::variable<bool> get_keepDeletedMessagesReactive();
    rpl::variable<bool> get_keepMessagesHistoryReactive();
    rpl::variable<QString> get_deletedMarkReactive();
    rpl::variable<QString> get_editedMarkReactive();

    // computed fields
    rpl::variable<bool> get_ghostModeEnabled();
}
