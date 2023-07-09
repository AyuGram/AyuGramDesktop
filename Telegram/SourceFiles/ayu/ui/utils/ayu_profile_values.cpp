// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "ui/text/text_utilities.h"
#include "ayu/ayu_settings.h"
#include "ayu_profile_values.h"
#include "data/data_peer.h"


constexpr auto kMaxChannelId = -1000000000000;


QString IDString(not_null<PeerData *> peer) {
    auto resultId = QString::number(peerIsUser(peer->id)
                                    ? peerToUser(peer->id).bare
                                    : peerIsChat(peer->id)
                                      ? peerToChat(peer->id).bare
                                      : peerIsChannel(peer->id)
                                        ? peerToChannel(peer->id).bare
                                        : peer->id.value );

    auto const settings = &AyuSettings::getInstance();
    if (settings->showPeerId == 2) {
        if (peer->isChannel()) {
            resultId = QString::number(peerToChannel(peer->id).bare - kMaxChannelId).prepend("-");
        } else if (peer->isChat()) {
            resultId = resultId.prepend("-");
        }
    }

    return resultId;
}

QString IDString(MsgId topic_root_id) {
    auto resultId = QString::number(topic_root_id.bare);

    return resultId;
}


rpl::producer<TextWithEntities> IDValue(not_null<PeerData *> peer) {
    return rpl::single(IDString(peer)) | Ui::Text::ToWithEntities();
}

rpl::producer<TextWithEntities> IDValue(MsgId topic_root_id) {
    return rpl::single(IDString(topic_root_id)) | Ui::Text::ToWithEntities();
}