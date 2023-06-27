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
            // ~ Ghost essentials
            sendReadPackets = true;
            sendOnlinePackets = true;
            sendUploadProgress = true;

            sendOfflinePacketAfterOnline = false;
            markReadAfterSend = true;

            useScheduledMessages = false;

            // ~ Message edits & deletion history
            keepDeletedMessages = true;
            keepMessagesHistory = true;

            // ~ QoL toggles
            enableAds = false;

            // ~ Customization
            deletedMark = "🧹";
            editedMark = tr::lng_edited(tr::now);
            recentStickersCount = 20;
            showGhostToggleInDrawer = true;

            // ~ Beta functionality
            stickerConfirmation = false;
            GIFConfirmation = false;
            voiceConfirmation = false;

            /*
             * showPeerId = 0 means no ID shown
             * showPeerId = 1 means ID shown as for Telegram API devs
             * showPeerId = 2 means ID shown as for Bot API devs (-100)
             */
            showPeerId = 2;

            showMessageSeconds = false;
        }

        QS_SERIALIZABLE

    QS_FIELD(bool, sendReadPackets)

    QS_FIELD(bool, sendOnlinePackets)

    QS_FIELD(bool, sendUploadProgress)

    QS_FIELD(bool, sendOfflinePacketAfterOnline)

    QS_FIELD(bool, markReadAfterSend)

    QS_FIELD(bool, useScheduledMessages)

    QS_FIELD(bool, keepDeletedMessages)

    QS_FIELD(bool, keepMessagesHistory)

    QS_FIELD(bool, enableAds)

    QS_FIELD(QString, deletedMark)

    QS_FIELD(QString, editedMark)

    QS_FIELD(int, recentStickersCount)

    QS_FIELD(bool, showGhostToggleInDrawer)

    QS_FIELD(int, showPeerId)

    QS_FIELD(bool, showMessageSeconds)

    QS_FIELD(bool, stickerConfirmation)

    QS_FIELD(bool, GIFConfirmation)

    QS_FIELD(bool, voiceConfirmation)

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

        bool getGhostModeValue();
    };

    AyuGramSettings &getInstance();

    void load();

    void save();

    rpl::producer<QString> get_deletedMarkReactive();
    rpl::producer<QString> get_editedMarkReactive();
    rpl::producer<int> get_showPeerId();

    // computed fields
    rpl::producer<bool> get_ghostModeEnabled();
}
