// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_profile_values.h"
#include "ayu/ayu_settings.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_peer.h"
#include "ui/text/text_utilities.h"

constexpr auto kMaxChannelId = -1000000000000;

QString IDString(not_null<PeerData*> peer) {
	auto resultId = QString::number(getBareID(peer));

	const auto settings = &AyuSettings::getInstance();
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

rpl::producer<TextWithEntities> IDValue(not_null<PeerData*> peer) {
	return rpl::single(IDString(peer)) | Ui::Text::ToWithEntities();
}

rpl::producer<TextWithEntities> IDValue(MsgId topicRootId) {
	return rpl::single(IDString(topicRootId)) | Ui::Text::ToWithEntities();
}
