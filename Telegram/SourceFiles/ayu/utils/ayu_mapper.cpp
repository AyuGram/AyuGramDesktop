// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/utils/ayu_mapper.h"

#include "apiwrap.h"
#include "api/api_text_entities.h"
#include "ayu/data/entities.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "mtproto/connection_abstract.h"
#include "mtproto/details/mtproto_dump_to_text.h"

namespace AyuMapper {

constexpr auto kMessageFlagUnread = 0x00000001;
constexpr auto kMessageFlagOut = 0x00000002;
constexpr auto kMessageFlagForwarded = 0x00000004;
constexpr auto kMessageFlagReply = 0x00000008;
constexpr auto kMessageFlagMention = 0x00000010;
constexpr auto kMessageFlagContentUnread = 0x00000020;
constexpr auto kMessageFlagHasMarkup = 0x00000040;
constexpr auto kMessageFlagHasEntities = 0x00000080;
constexpr auto kMessageFlagHasFromId = 0x00000100;
constexpr auto kMessageFlagHasMedia = 0x00000200;
constexpr auto kMessageFlagHasViews = 0x00000400;
constexpr auto kMessageFlagHasBotId = 0x00000800;
constexpr auto kMessageFlagIsSilent = 0x00001000;
constexpr auto kMessageFlagIsPost = 0x00004000;
constexpr auto kMessageFlagEdited = 0x00008000;
constexpr auto kMessageFlagHasPostAuthor = 0x00010000;
constexpr auto kMessageFlagIsGrouped = 0x00020000;
constexpr auto kMessageFlagFromScheduled = 0x00040000;
constexpr auto kMessageFlagHasReactions = 0x00100000;
constexpr auto kMessageFlagHideEdit = 0x00200000;
constexpr auto kMessageFlagRestricted = 0x00400000;
constexpr auto kMessageFlagHasReplies = 0x00800000;
constexpr auto kMessageFlagIsPinned = 0x01000000;
constexpr auto kMessageFlagHasTTL = 0x02000000;
constexpr auto kMessageFlagInvertMedia = 0x08000000;
constexpr auto kMessageFlagHasSavedPeer = 0x10000000;

template<typename MTPObject>
std::vector<char> serializeObject(MTPObject object) {
	mtpBuffer buffer;
	object.write(buffer);

	const auto from = reinterpret_cast<char*>(buffer.data());
	const auto end = from + buffer.size() * sizeof(mtpPrime);

	std::vector<char> entities(from, end);
	return entities;
}

template<typename MTPObject>
MTPObject deserializeObject(std::vector<char> serialized) {
	gsl::span<char> span(serialized.data(), serialized.size());

	auto from = reinterpret_cast<const mtpPrime*>(span.data());
	const auto end = from + span.size() / sizeof(mtpPrime);

	MTPObject data;
	if (!data.read(from, end)) {
		LOG(("AyuMapper: Failed to deserialize object"));
	}
	return data;
}

std::pair<std::string, std::vector<char>> serializeTextWithEntities(not_null<HistoryItem*> item) {
	if (item->emptyText()) {
		return std::make_pair("", std::vector<char>());
	}
	auto textWithEntities = item->originalText();


	std::vector<char> entities;
	if (!textWithEntities.entities.empty()) {
		const auto mtpEntities = Api::EntitiesToMTP(
			&item->history()->session(),
			textWithEntities.entities,
			Api::ConvertOption::WithLocal);

		entities = serializeObject(mtpEntities);
	}

	return std::make_pair(textWithEntities.text.toStdString(), entities);
}

MTPVector<MTPMessageEntity> deserializeTextWithEntities(std::vector<char> serialized) {
	return deserializeObject<MTPVector<MTPMessageEntity>>(serialized);
}

int mapItemFlagsToMTPFlags(not_null<HistoryItem*> item) {
	int flags = 0;

	const auto thread = item->topic()
							? reinterpret_cast<Data::Thread*>(item->topic())
							: item->history();
	if (item->unread(thread)) {
		flags |= kMessageFlagUnread;
	}

	if (item->out()) {
		flags |= kMessageFlagOut;
	}

	if (item->Get<HistoryMessageForwarded>()) {
		flags |= kMessageFlagForwarded;
	}

	if (item->Get<HistoryMessageReply>()) {
		flags |= kMessageFlagReply;
	}

	if (item->mentionsMe()) {
		flags |= kMessageFlagMention;
	}

	if (item->hasUnreadMediaFlag()) {
		flags |= kMessageFlagContentUnread;
	}

	if (item->definesReplyKeyboard()) {
		flags |= kMessageFlagHasMarkup;
	}

	if (!item->originalText().entities.empty()) {
		flags |= kMessageFlagHasEntities;
	}

	if (item->displayFrom()) {
		// todo: maybe wrong
		flags |= kMessageFlagHasFromId;
	}

	if (item->media()) {
		flags |= kMessageFlagHasMedia;
	}

	if (item->hasViews()) {
		flags |= kMessageFlagHasViews;
	}

	if (item->viaBot()) {
		flags |= kMessageFlagHasBotId;
	}

	if (item->isSilent()) {
		flags |= kMessageFlagIsSilent;
	}

	if (item->isPost()) {
		flags |= kMessageFlagIsPost;
	}

	if (item->Get<HistoryMessageEdited>()) {
		flags |= kMessageFlagEdited;
	}

	if (item->Get<HistoryMessageSigned>()) {
		flags |= kMessageFlagHasPostAuthor;
	}

	if (item->groupId()) {
		flags |= kMessageFlagIsGrouped;
	}

	if (item->isScheduled()) {
		flags |= kMessageFlagFromScheduled;
	}

	if (!item->reactions().empty()) {
		flags |= kMessageFlagHasReactions;
	}

	if (item->hideEditedBadge()) {
		flags |= kMessageFlagHideEdit;
	}

	if (item->hasPossibleRestrictions()) {
		flags |= kMessageFlagRestricted;
	}

	if (item->repliesCount() > 0) {
		flags |= kMessageFlagHasReplies;
	}

	if (item->isPinned()) {
		flags |= kMessageFlagIsPinned;
	}

	if (item->ttlDestroyAt() > 0) {
		flags |= kMessageFlagHasTTL;
	}

	if (item->invertMedia()) {
		flags |= kMessageFlagInvertMedia;
	}

	if (item->savedFromSender()) {
		// todo: maybe wrong
		flags |= kMessageFlagHasSavedPeer;
	}

	return flags;
}

MTPMessage DeletedMessageToMTP(
		not_null<PeerData*> peer,
		const AyuMessageBase &message) {
	Expects(peer->asUser() != nullptr);

	const auto outgoing = (message.flags & kMessageFlagOut) != 0;
	const auto self = peer->session().userPeerId();
	auto from = outgoing ? self : peer->id;
	if (message.fromId == peer->session().userId().bare) {
		from = self;
	} else if (message.fromId == peerToUser(peer->id).bare) {
		from = peer->id;
	}

	auto flags = MTPDmessage::Flags(MTPDmessage::Flag::f_from_id);
	if (outgoing) {
		flags |= MTPDmessage::Flag::f_out;
	}
	if (message.flags & kMessageFlagIsSilent) {
		flags |= MTPDmessage::Flag::f_silent;
	}
	if ((message.flags & kMessageFlagEdited) && message.editDate > 0) {
		flags |= MTPDmessage::Flag::f_edit_date;
	}

	auto entities = MTP_vector<MTPMessageEntity>();
	if (!message.textEntities.empty()) {
		entities = deserializeTextWithEntities(message.textEntities);
		if (!entities.v.empty()) {
			flags |= MTPDmessage::Flag::f_entities;
		}
	}

	return MTP_message(
		MTP_flags(flags),
		MTP_int(message.messageId),
		peerToMTP(from),
		MTPint(), // from_boosts_applied
		MTPstring(), // from_rank
		peerToMTP(peer->id),
		MTPPeer(), // saved_peer_id
		MTPMessageFwdHeader(), // fwd_from
		MTPlong(), // via_bot_id
		MTPlong(), // via_business_bot_id
		MTPPeer(), // guestchat_via_from
		MTPMessageReplyHeader(), // reply_to
		MTP_int(message.date),
		MTP_string(QString::fromStdString(message.text)),
		MTP_messageMediaEmpty(),
		MTPReplyMarkup(), // reply_markup
		entities,
		MTPint(), // views
		MTPint(), // forwards
		MTPMessageReplies(), // replies
		MTP_int(message.editDate),
		MTPstring(), // post_author
		MTPlong(), // grouped_id
		MTPMessageReactions(), // reactions
		MTPVector<MTPRestrictionReason>(), // restriction_reason
		MTPint(), // ttl_period
		MTPint(), // quick_reply_shortcut_id
		MTPlong(), // effect
		MTPFactCheck(), // factcheck
		MTPint(), // report_delivery_until_date
		MTPlong(), // paid_message_stars
		MTPSuggestedPost(), // suggested_post
		MTPint(), // schedule_repeat_period
		MTPstring(), // summary_from_language
		MTPRichMessage());
}

}
