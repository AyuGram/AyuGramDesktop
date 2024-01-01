// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include <ayu/database/entities.h>
#include "ayu/ui/sections/edited/edited_log_item.h"

#include "ayu/ui/sections/edited/edited_log_inner.h"
#include "history/view/history_view_element.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "history/history_location_manager.h"
#include "api/api_chat_participants.h"
#include "api/api_text_entities.h"
#include "data/data_channel.h"
#include "data/data_file_origin.h"
#include "data/data_forum_topic.h"
#include "data/data_user.h"
#include "data/data_session.h"
#include "data/data_message_reaction_id.h"
#include "data/stickers/data_custom_emoji.h"
#include "lang/lang_keys.h"
#include "ui/text/format_values.h"
#include "ui/text/text_utilities.h"
#include "ui/basic_click_handlers.h"
#include "boxes/sticker_set_box.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "main/main_session.h"
#include "window/notifications_manager.h"
#include "window/window_session_controller.h"

namespace EditedLog
{
namespace
{

const auto CollectChanges = [](
	auto &phraseMap,
	auto plusFlags,
	auto minusFlags)
{
	auto withPrefix = [&phraseMap](auto flags, QChar prefix)
	{
		auto result = QString();
		for (auto &phrase : phraseMap) {
			if (flags & phrase.first) {
				result.append('\n' + (prefix + phrase.second(tr::now)));
			}
		}
		return result;
	};
	const auto kMinus = QChar(0x2212);
	return withPrefix(plusFlags & ~minusFlags, '+')
		+ withPrefix(minusFlags & ~plusFlags, kMinus);
};

TextWithEntities GenerateAdminChangeText(
	not_null<ChannelData *> channel,
	const TextWithEntities &user,
	ChatAdminRightsInfo newRights,
	ChatAdminRightsInfo prevRights)
{
	using Flag = ChatAdminRight;
	using Flags = ChatAdminRights;

	auto result = tr::lng_admin_log_promoted(
		tr::now,
		lt_user,
		user,
		Ui::Text::WithEntities);

	const auto useInviteLinkPhrase = channel->isMegagroup()
		&& channel->anyoneCanAddMembers();
	const auto invitePhrase = useInviteLinkPhrase
							  ? tr::lng_admin_log_admin_invite_link
							  : tr::lng_admin_log_admin_invite_users;
	const auto callPhrase = channel->isBroadcast()
							? tr::lng_admin_log_admin_manage_calls_channel
							: tr::lng_admin_log_admin_manage_calls;
	static auto phraseMap = std::map<Flags, tr::phrase<>>{
		{Flag::ChangeInfo, tr::lng_admin_log_admin_change_info},
		{Flag::PostMessages, tr::lng_admin_log_admin_post_messages},
		{Flag::EditMessages, tr::lng_admin_log_admin_edit_messages},
		{Flag::DeleteMessages, tr::lng_admin_log_admin_delete_messages},
		{Flag::BanUsers, tr::lng_admin_log_admin_ban_users},
		{Flag::InviteByLinkOrAdd, invitePhrase},
		{Flag::ManageTopics, tr::lng_admin_log_admin_manage_topics},
		{Flag::PinMessages, tr::lng_admin_log_admin_pin_messages},
		{Flag::ManageCall, tr::lng_admin_log_admin_manage_calls},
		{Flag::AddAdmins, tr::lng_admin_log_admin_add_admins},
		{Flag::Anonymous, tr::lng_admin_log_admin_remain_anonymous},
	};
	phraseMap[Flag::InviteByLinkOrAdd] = invitePhrase;
	phraseMap[Flag::ManageCall] = callPhrase;

	if (!channel->isMegagroup()) {
		// Don't display "Ban users" changes in channels.
		newRights.flags &= ~Flag::BanUsers;
		prevRights.flags &= ~Flag::BanUsers;
	}

	const auto changes = CollectChanges(
		phraseMap,
		newRights.flags,
		prevRights.flags);
	if (!changes.isEmpty()) {
		result.text.append('\n' + changes);
	}

	return result;
};

QString GeneratePermissionsChangeText(
	ChatRestrictionsInfo newRights,
	ChatRestrictionsInfo prevRights)
{
	using Flag = ChatRestriction;
	using Flags = ChatRestrictions;

	static auto phraseMap = std::map<Flags, tr::phrase<>>{
		{Flag::ViewMessages, tr::lng_admin_log_banned_view_messages},
		{Flag::SendOther, tr::lng_admin_log_banned_send_messages},
		{Flag::SendPhotos, tr::lng_admin_log_banned_send_photos},
		{Flag::SendVideos, tr::lng_admin_log_banned_send_videos},
		{Flag::SendMusic, tr::lng_admin_log_banned_send_music},
		{Flag::SendFiles, tr::lng_admin_log_banned_send_files},
		{
			Flag::SendVoiceMessages,
			tr::lng_admin_log_banned_send_voice_messages},
		{
			Flag::SendVideoMessages,
			tr::lng_admin_log_banned_send_video_messages},
		{Flag::SendStickers
			 | Flag::SendGifs
			 | Flag::SendInline
			 | Flag::SendGames, tr::lng_admin_log_banned_send_stickers},
		{Flag::EmbedLinks, tr::lng_admin_log_banned_embed_links},
		{Flag::SendPolls, tr::lng_admin_log_banned_send_polls},
		{Flag::ChangeInfo, tr::lng_admin_log_admin_change_info},
		{Flag::AddParticipants, tr::lng_admin_log_admin_invite_users},
		{Flag::CreateTopics, tr::lng_admin_log_admin_create_topics},
		{Flag::PinMessages, tr::lng_admin_log_admin_pin_messages},
	};
	return CollectChanges(phraseMap, prevRights.flags, newRights.flags);
}

TextWithEntities GeneratePermissionsChangeText(
	PeerId participantId,
	const TextWithEntities &user,
	ChatRestrictionsInfo newRights,
	ChatRestrictionsInfo prevRights)
{
	using Flag = ChatRestriction;

	const auto newFlags = newRights.flags;
	const auto newUntil = newRights.until;
	const auto prevFlags = prevRights.flags;
	const auto indefinitely = ChannelData::IsRestrictedForever(newUntil);
	if (newFlags & Flag::ViewMessages) {
		return tr::lng_admin_log_banned(
			tr::now,
			lt_user,
			user,
			Ui::Text::WithEntities);
	}
	else if (newFlags == 0
		&& (prevFlags & Flag::ViewMessages)
		&& !peerIsUser(participantId)) {
		return tr::lng_admin_log_unbanned(
			tr::now,
			lt_user,
			user,
			Ui::Text::WithEntities);
	}
	const auto untilText = indefinitely
						   ? tr::lng_admin_log_restricted_forever(tr::now)
						   : tr::lng_admin_log_restricted_until(
			tr::now,
			lt_date,
			langDateTime(base::unixtime::parse(newUntil)));
	auto result = tr::lng_admin_log_restricted(
		tr::now,
		lt_user,
		user,
		lt_until,
		TextWithEntities{untilText},
		Ui::Text::WithEntities);
	const auto changes = GeneratePermissionsChangeText(newRights, prevRights);
	if (!changes.isEmpty()) {
		result.text.append('\n' + changes);
	}
	return result;
}

QString PublicJoinLink()
{
	return u"(public_join_link)"_q;
}

QString ExtractInviteLink(const MTPExportedChatInvite &data)
{
	return data.match([&](const MTPDchatInviteExported &data)
					  {
						  return qs(data.vlink());
					  }, [&](const MTPDchatInvitePublicJoinRequests &data)
					  {
						  return PublicJoinLink();
					  });
}

QString ExtractInviteLinkLabel(const MTPExportedChatInvite &data)
{
	return data.match([&](const MTPDchatInviteExported &data)
					  {
						  return qs(data.vtitle().value_or_empty());
					  }, [&](const MTPDchatInvitePublicJoinRequests &data)
					  {
						  return PublicJoinLink();
					  });
}

QString InternalInviteLinkUrl(const MTPExportedChatInvite &data)
{
	const auto base64 = ExtractInviteLink(data).toUtf8().toBase64();
	return "internal:show_invite_link/?link=" + QString::fromLatin1(base64);
}

QString GenerateInviteLinkText(const MTPExportedChatInvite &data)
{
	const auto label = ExtractInviteLinkLabel(data);
	return label.isEmpty() ? ExtractInviteLink(data).replace(
		u"https://"_q,
		QString()
	).replace(
		u"t.me/joinchat/"_q,
		QString()
	) : label;
}

TextWithEntities GenerateInviteLinkLink(const MTPExportedChatInvite &data)
{
	const auto text = GenerateInviteLinkText(data);
	return text.endsWith(Ui::kQEllipsis)
		   ? TextWithEntities{.text = text}
		   : Ui::Text::Link(text, InternalInviteLinkUrl(data));
}

TextWithEntities GenerateInviteLinkChangeText(
	const MTPExportedChatInvite &newLink,
	const MTPExportedChatInvite &prevLink)
{
	auto link = TextWithEntities{GenerateInviteLinkText(newLink)};
	if (!link.text.endsWith(Ui::kQEllipsis)) {
		link.entities.push_back({
									EntityType::CustomUrl,
									0,
									int(link.text.size()),
									InternalInviteLinkUrl(newLink)});
	}
	auto result = tr::lng_admin_log_edited_invite_link(
		tr::now,
		lt_link,
		link,
		Ui::Text::WithEntities);
	result.text.append('\n');

	const auto label = [](const MTPExportedChatInvite &link)
	{
		return link.match([](const MTPDchatInviteExported &data)
						  {
							  return qs(data.vtitle().value_or_empty());
						  }, [&](const MTPDchatInvitePublicJoinRequests &data)
						  {
							  return PublicJoinLink();
						  });
	};
	const auto expireDate = [](const MTPExportedChatInvite &link)
	{
		return link.match([](const MTPDchatInviteExported &data)
						  {
							  return data.vexpire_date().value_or_empty();
						  }, [&](const MTPDchatInvitePublicJoinRequests &data)
						  {
							  return TimeId();
						  });
	};
	const auto usageLimit = [](const MTPExportedChatInvite &link)
	{
		return link.match([](const MTPDchatInviteExported &data)
						  {
							  return data.vusage_limit().value_or_empty();
						  }, [&](const MTPDchatInvitePublicJoinRequests &data)
						  {
							  return 0;
						  });
	};
	const auto requestApproval = [](const MTPExportedChatInvite &link)
	{
		return link.match([](const MTPDchatInviteExported &data)
						  {
							  return data.is_request_needed();
						  }, [&](const MTPDchatInvitePublicJoinRequests &data)
						  {
							  return true;
						  });
	};
	const auto wrapDate = [](TimeId date)
	{
		return date
			   ? langDateTime(base::unixtime::parse(date))
			   : tr::lng_group_invite_expire_never(tr::now);
	};
	const auto wrapUsage = [](int count)
	{
		return count
			   ? QString::number(count)
			   : tr::lng_group_invite_usage_any(tr::now);
	};
	const auto wasLabel = label(prevLink);
	const auto nowLabel = label(newLink);
	const auto wasExpireDate = expireDate(prevLink);
	const auto nowExpireDate = expireDate(newLink);
	const auto wasUsageLimit = usageLimit(prevLink);
	const auto nowUsageLimit = usageLimit(newLink);
	const auto wasRequestApproval = requestApproval(prevLink);
	const auto nowRequestApproval = requestApproval(newLink);
	if (wasLabel != nowLabel) {
		result.text.append('\n').append(
			tr::lng_admin_log_invite_link_label(
				tr::now,
				lt_previous,
				wasLabel,
				lt_limit,
				nowLabel));
	}
	if (wasExpireDate != nowExpireDate) {
		result.text.append('\n').append(
			tr::lng_admin_log_invite_link_expire_date(
				tr::now,
				lt_previous,
				wrapDate(wasExpireDate),
				lt_limit,
				wrapDate(nowExpireDate)));
	}
	if (wasUsageLimit != nowUsageLimit) {
		result.text.append('\n').append(
			tr::lng_admin_log_invite_link_usage_limit(
				tr::now,
				lt_previous,
				wrapUsage(wasUsageLimit),
				lt_limit,
				wrapUsage(nowUsageLimit)));
	}
	if (wasRequestApproval != nowRequestApproval) {
		result.text.append('\n').append(
			nowRequestApproval
			? tr::lng_admin_log_invite_link_request_needed(tr::now)
			: tr::lng_admin_log_invite_link_request_not_needed(tr::now));
	}

	result.entities.push_front(
		EntityInText(EntityType::Italic, 0, result.text.size()));
	return result;
};

auto GenerateParticipantString(
	not_null<Main::Session *> session,
	PeerId participantId)
{
	// User name in "User name (@username)" format with entities.
	const auto peer = session->data().peer(participantId);
	auto name = TextWithEntities{peer->name()};
	if (const auto user = peer->asUser()) {
		const auto data = TextUtilities::MentionNameDataFromFields({
																	   .selfId = session->userId().bare,
																	   .userId = peerToUser(user->id).bare,
																	   .accessHash = user->accessHash(),
																   });
		name.entities.push_back({
									EntityType::MentionName,
									0,
									int(name.text.size()),
									data,
								});
	}
	const auto username = peer->userName();
	if (username.isEmpty()) {
		return name;
	}
	auto mention = TextWithEntities{'@' + username};
	mention.entities.push_back({
								   EntityType::Mention,
								   0,
								   int(mention.text.size())});
	return tr::lng_admin_log_user_with_username(
		tr::now,
		lt_name,
		name,
		lt_mention,
		mention,
		Ui::Text::WithEntities);
}

auto GenerateParticipantChangeText(
	not_null<ChannelData *> channel,
	const Api::ChatParticipant &participant,
	std::optional<Api::ChatParticipant> oldParticipant = std::nullopt)
{
	using Type = Api::ChatParticipant::Type;
	const auto oldRights = oldParticipant
						   ? oldParticipant->rights()
						   : ChatAdminRightsInfo();
	const auto oldRestrictions = oldParticipant
								 ? oldParticipant->restrictions()
								 : ChatRestrictionsInfo();

	const auto generateOther = [&](PeerId participantId)
	{
		auto user = GenerateParticipantString(
			&channel->session(),
			participantId);
		if (oldParticipant && oldParticipant->type() == Type::Admin) {
			return GenerateAdminChangeText(
				channel,
				user,
				ChatAdminRightsInfo(),
				oldRights);
		}
		else if (oldParticipant && oldParticipant->type() == Type::Banned) {
			return GeneratePermissionsChangeText(
				participantId,
				user,
				ChatRestrictionsInfo(),
				oldRestrictions);
		}
		else if (oldParticipant
			&& oldParticipant->type() == Type::Restricted
			&& (participant.type() == Type::Member
				|| participant.type() == Type::Left)) {
			return GeneratePermissionsChangeText(
				participantId,
				user,
				ChatRestrictionsInfo(),
				oldRestrictions);
		}
		return tr::lng_admin_log_invited(
			tr::now,
			lt_user,
			user,
			Ui::Text::WithEntities);
	};

	auto result = [&]
	{
		const auto &peerId = participant.id();
		switch (participant.type()) {
			case Api::ChatParticipant::Type::Creator: {
				// No valid string here :(
				const auto user = GenerateParticipantString(
					&channel->session(),
					peerId);
				if (peerId == channel->session().userPeerId()) {
					return GenerateAdminChangeText(
						channel,
						user,
						participant.rights(),
						oldRights);
				}
				return tr::lng_admin_log_transferred(
					tr::now,
					lt_user,
					user,
					Ui::Text::WithEntities);
			}
			case Api::ChatParticipant::Type::Admin: {
				const auto user = GenerateParticipantString(
					&channel->session(),
					peerId);
				return GenerateAdminChangeText(
					channel,
					user,
					participant.rights(),
					oldRights);
			}
			case Api::ChatParticipant::Type::Restricted:
			case Api::ChatParticipant::Type::Banned: {
				const auto user = GenerateParticipantString(
					&channel->session(),
					peerId);
				return GeneratePermissionsChangeText(
					peerId,
					user,
					participant.restrictions(),
					oldRestrictions);
			}
			case Api::ChatParticipant::Type::Left:
			case Api::ChatParticipant::Type::Member: return generateOther(peerId);
		};
		Unexpected("Participant type in GenerateParticipantChangeText.");
	}();

	result.entities.push_front(
		EntityInText(EntityType::Italic, 0, result.text.size()));
	return result;
}

TextWithEntities GenerateParticipantChangeText(
	not_null<ChannelData *> channel,
	const MTPChannelParticipant &participant,
	std::optional<MTPChannelParticipant> oldParticipant = std::nullopt)
{
	return GenerateParticipantChangeText(
		channel,
		Api::ChatParticipant(participant, channel),
		oldParticipant
		? std::make_optional(Api::ChatParticipant(
			*oldParticipant,
			channel))
		: std::nullopt);
}

TextWithEntities GenerateDefaultBannedRightsChangeText(
	not_null<ChannelData *> channel,
	ChatRestrictionsInfo rights,
	ChatRestrictionsInfo oldRights)
{
	auto result = TextWithEntities{
		tr::lng_admin_log_changed_default_permissions(tr::now)
	};
	const auto changes = GeneratePermissionsChangeText(rights, oldRights);
	if (!changes.isEmpty()) {
		result.text.append('\n' + changes);
	}
	result.entities.push_front(
		EntityInText(EntityType::Italic, 0, result.text.size()));
	return result;
}

[[nodiscard]] bool IsTopicClosed(const MTPForumTopic &topic)
{
	return topic.match([](const MTPDforumTopic &data)
					   {
						   return data.is_closed();
					   }, [](const MTPDforumTopicDeleted &)
					   {
						   return false;
					   });
}

[[nodiscard]] bool IsTopicHidden(const MTPForumTopic &topic)
{
	return topic.match([](const MTPDforumTopic &data)
					   {
						   return data.is_hidden();
					   }, [](const MTPDforumTopicDeleted &)
					   {
						   return false;
					   });
}

[[nodiscard]] TextWithEntities GenerateTopicLink(
	not_null<ChannelData *> channel,
	const MTPForumTopic &topic)
{
	return topic.match([&](const MTPDforumTopic &data)
					   {
						   return Ui::Text::Link(
							   Data::ForumTopicIconWithTitle(
								   data.vid().v,
								   data.vicon_emoji_id().value_or_empty(),
								   qs(data.vtitle())),
							   u"internal:url:https://t.me/c/%1/%2"_q.arg(
								   peerToChannel(channel->id).bare).arg(
								   data.vid().v));
					   }, [](const MTPDforumTopicDeleted &)
					   {
						   return TextWithEntities{u"Deleted"_q};
					   });
}

} // namespace

OwnedItem::OwnedItem(std::nullptr_t)
{
}

OwnedItem::OwnedItem(
	not_null<HistoryView::ElementDelegate *> delegate,
	not_null<HistoryItem *> data)
	: _data(data), _view(_data->createView(delegate))
{
}

OwnedItem::OwnedItem(OwnedItem &&other)
	: _data(base::take(other._data)), _view(base::take(other._view))
{
}

OwnedItem &OwnedItem::operator=(OwnedItem &&other)
{
	_data = base::take(other._data);
	_view = base::take(other._view);
	return *this;
}

OwnedItem::~OwnedItem()
{
	clearView();
	if (_data) {
		_data->destroy();
	}
}

void OwnedItem::refreshView(
	not_null<HistoryView::ElementDelegate *> delegate)
{
	_view = _data->createView(delegate);
}

void OwnedItem::clearView()
{
	_view = nullptr;
}

void GenerateItems(
	not_null<HistoryView::ElementDelegate *> delegate,
	not_null<History *> history,
	EditedMessage message,
	Fn<void(OwnedItem item, TimeId sentDate, MsgId)> callback)
{
	const auto session = &history->session();
	const auto id = message.fakeId;
	PeerData* from = history->owner().userLoaded(message.fromId);
	if (!from) {
		from = history->owner().channelLoaded(message.fromId);
	}
	if (!from) {
		from = reinterpret_cast<PeerData *>(history->owner().chatLoaded(message.fromId));
	}
	if (!from) {
		return;
	}
	const auto date = message.entityCreateDate;
	const auto addPart = [&](
		not_null<HistoryItem *> item,
		TimeId sentDate = 0,
		MsgId realId = MsgId())
	{
		return callback(OwnedItem(delegate, item), sentDate, realId);
	};

	const auto fromName = from->name();
	const auto fromLink = from->createOpenLink();
	const auto fromLinkText = Ui::Text::Link(fromName, QString());

	const auto addSimpleServiceMessage = [&](
		const TextWithEntities &text,
		MsgId realId = MsgId(),
		PhotoData *photo = nullptr)
	{
		auto message = PreparedServiceText{text};
		message.links.push_back(fromLink);
		addPart(
			history->makeMessage(
				history->nextNonHistoryEntryId(),
				MessageFlag::AdminLogEntry,
				date,
				std::move(message),
				peerToUser(from->id),
				photo),
			0,
			realId);
	};

	const auto makeSimpleTextMessage = [&](TextWithEntities &&text)
	{
		const auto bodyFlags = MessageFlag::HasFromId | MessageFlag::Local;
		const auto bodyReplyTo = FullReplyTo();
		const auto bodyViaBotId = UserId();
		const auto bodyGroupedId = uint64();
		return history->makeMessage(
			history->nextNonHistoryEntryId(),
			bodyFlags,
			bodyReplyTo,
			bodyViaBotId,
			date,
			peerToUser(from->id),
			QString(),
			std::move(text),
			MTP_messageMediaEmpty(),
			HistoryMessageMarkupData(),
			bodyGroupedId);
	};

	const auto addSimpleTextMessage = [&](TextWithEntities &&text)
	{
		addPart(makeSimpleTextMessage(std::move(text)));
	};

	const auto text = QString::fromStdString(message.text);
	addSimpleTextMessage(Ui::Text::WithEntities(text));
}

} // namespace EditedLog
