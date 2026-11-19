// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/ui/utils/lupin_profile_values.h"

#include "lupin/lupin_settings.h"
#include "lupin/utils/telegram_helpers.h"
#include "data/data_peer.h"
#include "lang/lang_text_entity.h"

constexpr auto kMaxChannelId = -1000000000000;

QString IDString(const not_null<PeerData*> peer) {
	auto resultId = QString::number(getBareID(peer));

	const auto &settings = LupinSettings::getInstance();
	if (settings.showPeerId() == PeerIdDisplay::BotApi) {
		if (peer->isChannel()) {
			resultId = QString::number(peerToChannel(peer->id).bare - kMaxChannelId).prepend("-");
		} else if (peer->isChat()) {
			resultId = resultId.prepend("-");
		}
	}

	return resultId;
}

QString IDString(MsgId topicRootId) {
	return QString::number(topicRootId.bare);
}

rpl::producer<TextWithEntities> IDValue(not_null<PeerData*> peer) {
	return LupinSettings::getInstance().showPeerIdValue(
	) | rpl::map([=](PeerIdDisplay display) {
		return (display == PeerIdDisplay::Hidden)
			? TextWithEntities()
			: tr::marked(IDString(peer));
	});
}

rpl::producer<TextWithEntities> IDValue(MsgId topicRootId) {
	return LupinSettings::getInstance().showPeerIdValue(
	) | rpl::map([=](PeerIdDisplay display) {
		return (display == PeerIdDisplay::Hidden)
			? TextWithEntities()
			: tr::marked(IDString(topicRootId));
	});
}
