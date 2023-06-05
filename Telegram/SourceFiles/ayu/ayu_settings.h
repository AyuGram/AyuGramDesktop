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

            /*
             * showPeerId = 0 means no ID shown
             * showPeerId = 1 means ID shown as for Telegram API devs
             * showPeerId = 2 means ID shown as for Bot API devs (-100)
             */
            showPeerId = 2;
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

    QS_FIELD(int, showPeerId)

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
        void set_showPeerId(int val);
    };

    AyuGramSettings &getInstance();

    void load();

    void save();

    rpl::producer<bool> get_sendReadPacketsReactive();
    rpl::producer<bool> get_sendOnlinePacketsReactive();
    rpl::producer<bool> get_sendOfflinePacketAfterOnlineReactive();
    rpl::producer<bool> get_sendUploadProgressReactive();
    rpl::producer<bool> get_useScheduledMessagesReactive();
    rpl::producer<bool> get_keepDeletedMessagesReactive();
    rpl::producer<bool> get_keepMessagesHistoryReactive();
    rpl::producer<QString> get_deletedMarkReactive();
    rpl::producer<QString> get_editedMarkReactive();
    rpl::producer<int> get_showPeerId();

    // computed fields
    rpl::producer<bool> get_ghostModeEnabled();
}
