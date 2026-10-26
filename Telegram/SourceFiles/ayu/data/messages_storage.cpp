// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/messages_storage.h"

#include "api/api_text_entities.h"
#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "ayu/utils/ayu_mapper.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/unixtime.h"
#include "core/version.h"
#include "data/data_document.h"
#include "data/data_forum_topic.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "media/player/media_player_instance.h"
#include "storage/serialize_common.h"
#include "storage/serialize_document.h"
#include "storage/serialize_peer.h"

#include <algorithm>
#include <array>

namespace AyuMessages {
namespace {

constexpr auto kSnapshotVersion = 1;
constexpr auto kDeletedMessagesPerPage = 100;

enum class SavedMedia : qint32 {
	None,
	Photo,
	Document,
};

void WriteOptionalPeer(QDataStream &stream, PeerData *peer) {
	stream << bool(peer);
	if (peer) {
		Serialize::writePeer(stream, peer);
	}
}

PeerData *ReadOptionalPeer(
		not_null<Main::Session*> session,
		int appVersion,
		QDataStream &stream) {
	auto present = false;
	stream >> present;
	if (!present) {
		return nullptr;
	}
	const auto result = Serialize::readPeer(session, appVersion, stream);
	if (!result) {
		stream.setStatus(QDataStream::ReadCorruptData);
	}
	return result;
}

void WriteForwarded(
		QDataStream &stream,
		const HistoryMessageForwarded *forwarded) {
	stream << bool(forwarded);
	if (!forwarded) {
		return;
	}
	WriteOptionalPeer(stream, forwarded->originalSender);
	WriteOptionalPeer(stream, forwarded->savedFromPeer);
	WriteOptionalPeer(stream, forwarded->savedFromSender);
	stream << qint32(forwarded->originalDate)
		<< (forwarded->originalHiddenSenderInfo
			? forwarded->originalHiddenSenderInfo->name : QString())
		<< forwarded->originalPostAuthor << qint64(forwarded->originalId.bare)
		<< qint64(forwarded->savedFromMsgId.bare)
		<< qint32(forwarded->savedFromDate)
		<< (forwarded->savedFromHiddenSenderInfo
			? forwarded->savedFromHiddenSenderInfo->name : QString())
		<< forwarded->psaType << forwarded->imported
		<< forwarded->savedFromOutgoing;
}

std::optional<MTPMessageFwdHeader> ReadForwarded(
		not_null<Main::Session*> session,
		int appVersion,
		QDataStream &stream) {
	auto present = false;
	stream >> present;
	if (!present) {
		return std::nullopt;
	}
	const auto from = ReadOptionalPeer(session, appVersion, stream);
	const auto savedPeer = ReadOptionalPeer(session, appVersion, stream);
	const auto savedSender = ReadOptionalPeer(session, appVersion, stream);
	auto date = qint32();
	auto name = QString();
	auto author = QString();
	auto post = qint64();
	auto savedId = qint64();
	auto savedDate = qint32();
	auto savedName = QString();
	auto psa = QString();
	auto imported = false;
	auto outgoing = false;
	stream >> date >> name >> author >> post >> savedId >> savedDate
		>> savedName >> psa >> imported >> outgoing;
	using Flag = MTPDmessageFwdHeader::Flag;
	return MTP_messageFwdHeader(
		MTP_flags((from ? Flag::f_from_id : Flag())
			| (!name.isEmpty() ? Flag::f_from_name : Flag())
			| (post ? Flag::f_channel_post : Flag())
			| (!author.isEmpty() ? Flag::f_post_author : Flag())
			| (savedPeer ? Flag::f_saved_from_peer : Flag())
			| (savedSender ? Flag::f_saved_from_id : Flag())
			| (!savedName.isEmpty() ? Flag::f_saved_from_name : Flag())
			| (savedDate ? Flag::f_saved_date : Flag())
			| (!psa.isEmpty() ? Flag::f_psa_type : Flag())
			| (imported ? Flag::f_imported : Flag())
			| (outgoing ? Flag::f_saved_out : Flag())),
		peerToMTP(from ? from->id : PeerId()),
		MTP_string(name),
		MTP_int(date),
		MTP_int(post),
		MTP_string(author),
		peerToMTP(savedPeer ? savedPeer->id : PeerId()),
		MTP_int(savedId),
		peerToMTP(savedSender ? savedSender->id : PeerId()),
		MTP_string(savedName),
		MTP_int(savedDate),
		MTP_string(psa));
}

void WritePhoto(QDataStream &stream, not_null<PhotoData*> photo) {
	const auto inputPhoto = photo->mtpInput();
	const auto &input = inputPhoto.c_inputPhoto();
	stream << quint64(photo->id) << quint64(input.vaccess_hash().v)
		<< photo->fileReference() << qint32(photo->date())
		<< qint32(photo->getDC()) << photo->hasAttachedStickers()
		<< photo->inlineThumbnailBytes();
	const auto sizes = {
		Data::PhotoSize::Small,
		Data::PhotoSize::Thumbnail,
		Data::PhotoSize::Large,
	};
	for (const auto size : sizes) {
		stream << photo->location(size).serialize()
			<< qint32(photo->imageByteSize(size));
	}
	for (const auto size : { Data::PhotoSize::Small, Data::PhotoSize::Large }) {
		stream << photo->videoLocation(size).serialize()
			<< qint32(photo->videoByteSize(size));
	}
	stream << qint64(photo->videoStartPosition());
}

PhotoData *ReadPhoto(
		not_null<Main::Session*> session,
		QDataStream &stream) {
	auto id = quint64();
	auto access = quint64();
	auto reference = QByteArray();
	auto date = qint32();
	auto dc = qint32();
	auto stickers = false;
	auto thumbnail = QByteArray();
	stream >> id >> access >> reference >> date >> dc >> stickers >> thumbnail;
	auto images = std::array<ImageWithLocation, 5>();
	for (auto &image : images) {
		auto serialized = QByteArray();
		auto bytes = qint32();
		stream >> serialized >> bytes;
		const auto location = ImageLocation::FromSerialized(serialized);
		if (!location) {
			stream.setStatus(QDataStream::ReadCorruptData);
			return nullptr;
		}
		image.location = *location;
		image.bytesCount = bytes;
	}
	auto videoStart = qint64();
	stream >> videoStart;
	if (stream.status() != QDataStream::Ok || !id) {
		return nullptr;
	}
	return session->data().photo(
		id,
		access,
		reference,
		date,
		dc,
		stickers,
		thumbnail,
		images[0],
		images[1],
		images[2],
		images[3],
		images[4],
		videoStart);
}

std::vector<char> SaveSnapshot(not_null<HistoryItem*> item) {
	const auto media = item->media();
	const auto document = media ? media->document() : nullptr;
	const auto photo = media ? media->photo() : nullptr;
	const auto forwarded = item->Get<HistoryMessageForwarded>();
	const auto reply = item->Get<HistoryMessageReply>();
	if (item->isService()
		|| item->richPage()
		|| item->Has<HistoryMessageReplyMarkup>()
		|| (reply && reply->fields().externalMedia)
		|| (forwarded && forwarded->story)
		|| (media
			&& !dynamic_cast<Data::MediaFile*>(media)
			&& !dynamic_cast<Data::MediaPhoto*>(media))) {
		return {};
	}
	auto writer = Serialize::ByteArrayWriter();
	auto &stream = writer.underlying();
	stream << qint32(kSnapshotVersion) << qint32(AppVersion);
	Serialize::writePeer(stream, item->from());
	stream << item->out() << item->isPost() << item->invertMedia()
		<< quint64(item->replyToFullId().peer.value)
		<< qint64(item->replyToId().bare)
		<< qint64(item->topicRootId().bare)
		<< (media && media->hasSpoiler());
	WriteOptionalPeer(stream, item->viaBot());
	WriteForwarded(stream, forwarded);
	const auto &quote = reply ? reply->fields().quote : TextWithEntities();
	auto entities = mtpBuffer();
	Api::EntitiesToMTP(
		&item->history()->session(),
		quote.entities,
		Api::ConvertOption::WithLocal).write(entities);
	stream << quote.text
		<< QByteArray(
			reinterpret_cast<const char*>(entities.data()),
			entities.size() * sizeof(mtpPrime))
		<< qint32(reply ? reply->fields().quoteOffset : 0)
		<< quint64(item->replyToStory().peer.value)
		<< qint32(item->replyToStory().story)
		<< qint32(reply ? reply->fields().todoItemId : 0)
		<< (reply ? reply->fields().pollOption : QByteArray())
		<< quint64(reply ? reply->fields().monoforumPeerId.value : 0);
	const auto kind = document ? SavedMedia::Document
		: photo ? SavedMedia::Photo : SavedMedia::None;
	stream << qint32(kind);
	if (document) {
		const auto sticker = document->sticker();
		stream << quint64(sticker ? sticker->set.id : 0)
			<< quint64(sticker ? sticker->set.accessHash : 0)
			<< (sticker ? sticker->set.shortName : QString());
		Serialize::Document::writeToStream(stream, document);
		const auto song = document->song();
		const auto voice = document->voice();
		stream << qint32(document->type) << qint64(document->duration())
			<< qint64(document->size)
			<< (song ? song->title : QString())
			<< (song ? song->performer : QString())
			<< (voice ? documentWaveformEncode5bit(voice->waveform) : QByteArray());
	} else if (photo) {
		WritePhoto(stream, photo);
	}
	if (stream.status() != QDataStream::Ok) {
		return {};
	}
	const auto bytes = std::move(writer).result();
	return { bytes.begin(), bytes.end() };
}

HistoryItem *RestoreSnapshot(
		not_null<History*> history,
		const DeletedMessage &message) {
	const auto session = &history->session();
	using Flag = MTPDmessage::Flag;
	const auto sender = (uint64(message.fromId)
		== (history->peer->id.value & PeerId::kChatTypeMask))
		? history->peer->id
		: peerFromUser(UserId(message.fromId));
	auto fields = HistoryItemCommonFields{
		.id = message.messageId,
		.flags = MessageFlag::HasFromId,
		.from = sender,
		.date = message.date,
		.editDate = (message.flags & int(Flag::f_edit_date)) ? message.editDate : 0,
		.postAuthor = QString::fromStdString(message.postAuthor),
		.groupedId = uint64(message.groupedId),
	};
	if (!fields.postAuthor.isEmpty()) {
		fields.flags |= MessageFlag::HasPostAuthor;
	}
	if (message.flags & int(Flag::f_views)) {
		fields.flags |= MessageFlag::HasViews;
	}
	auto text = TextWithEntities{ QString::fromStdString(message.text) };
	if (!message.textEntities.empty()) {
		const auto entities = AyuMapper::deserializeTextWithEntities(
			message.textEntities);
		text.entities = Api::EntitiesFromMTP(session, entities.v);
	}
	auto photo = static_cast<PhotoData*>(nullptr);
	auto document = static_cast<DocumentData*>(nullptr);
	auto forwarded = std::optional<MTPMessageFwdHeader>();
	if (!message.snapshot.empty()) {
		auto reader = Serialize::ByteArrayReader(QByteArray(
			message.snapshot.data(), message.snapshot.size()));
		auto &stream = reader.underlying();
		auto version = qint32();
		auto appVersion = qint32();
		stream >> version >> appVersion;
		if (version != kSnapshotVersion || appVersion > AppVersion) {
			return nullptr;
		}
		const auto from = Serialize::readPeer(session, appVersion, stream);
		if (!from) {
			return nullptr;
		}
		fields.from = from->id;
		auto outgoing = false;
		auto post = false;
		auto invert = false;
		auto replyPeer = quint64();
		auto reply = qint64();
		auto topic = qint64();
		auto spoiler = false;
		auto kind = qint32();
		stream >> outgoing >> post >> invert >> replyPeer >> reply >> topic
			>> spoiler;
		if (const auto bot = ReadOptionalPeer(session, appVersion, stream)) {
			fields.viaBotId = peerToUser(bot->id);
		}
		forwarded = ReadForwarded(session, appVersion, stream);
		auto quoteEntities = QByteArray();
		auto quoteOffset = qint32();
		auto storyPeer = quint64();
		auto story = qint32();
		auto todo = qint32();
		auto monoforum = quint64();
		stream >> fields.replyTo.quote.text >> quoteEntities >> quoteOffset
			>> storyPeer >> story >> todo >> fields.replyTo.pollOption >> monoforum;
		if (!quoteEntities.isEmpty()) {
			const auto entities = AyuMapper::deserializeTextWithEntities(
				{ quoteEntities.begin(), quoteEntities.end() });
			fields.replyTo.quote.entities = Api::EntitiesFromMTP(
				session,
				entities.v);
		}
		fields.replyTo.quoteOffset = quoteOffset;
		fields.replyTo.storyId = FullStoryId{ PeerId(storyPeer), story };
		fields.replyTo.todoItemId = todo;
		fields.replyTo.monoforumPeerId = PeerId(monoforum);
		stream >> kind;
		fields.flags |= (outgoing ? MessageFlag::Outgoing : MessageFlags())
			| (post ? MessageFlag::Post : MessageFlags())
			| (invert ? MessageFlag::InvertMedia : MessageFlags());
		fields.replyTo.messageId = FullMsgId(PeerId(replyPeer), MsgId(reply));
		fields.replyTo.topicRootId = MsgId(topic);
		if (reply || topic || story) {
			fields.flags |= MessageFlag::HasReplyInfo;
		}
		fields.mediaSpoiler = spoiler;
		if (kind == qint32(SavedMedia::Document)) {
			auto setId = quint64();
			auto setAccess = quint64();
			auto setName = QString();
			stream >> setId >> setAccess >> setName;
			document = Serialize::Document::readStickerFromStream(
				session,
				appVersion,
				stream,
				Serialize::Document::StickerSetInfo(setId, setAccess, setName));
			auto type = qint32();
			auto duration = qint64();
			auto size = qint64();
			auto title = QString();
			auto performer = QString();
			auto waveform = QByteArray();
			stream >> type >> duration >> size >> title >> performer >> waveform;
			if (!document) {
				return nullptr;
			}
			document->size = size;
			if (type == SongDocument || type == VoiceDocument) {
				using Flag = MTPDdocumentAttributeAudio::Flag;
				document->setattributes({ MTP_documentAttributeAudio(
					MTP_flags((type == VoiceDocument ? Flag::f_voice : Flag())
						| Flag::f_title | Flag::f_performer | Flag::f_waveform),
					MTP_int(duration / 1000),
					MTP_string(title),
					MTP_string(performer),
					MTP_bytes(waveform)) });
			}
		} else if (kind == qint32(SavedMedia::Photo)) {
			photo = ReadPhoto(session, stream);
			if (!photo) {
				return nullptr;
			}
		} else if (kind != qint32(SavedMedia::None)) {
			return nullptr;
		}
		if (!reader.ok() || !reader.atEnd()) {
			return nullptr;
		}
	} else {
		fields.flags |= (message.flags & int(Flag::f_out))
			? MessageFlag::Outgoing : MessageFlags();
		fields.flags |= (message.flags & int(Flag::f_post))
			? MessageFlag::Post : MessageFlags();
		fields.replyTo.messageId = FullMsgId(
			history->peer->id,
			message.replyMessageId);
		fields.replyTo.topicRootId = message.topicId;
		if (message.replyMessageId || message.topicId) {
			fields.flags |= MessageFlag::HasReplyInfo;
		}
	}
	auto item = static_cast<HistoryItem*>(nullptr);
	if (document) {
		item = history->makeMessage(
			std::move(fields),
			not_null<DocumentData*>(document),
			text);
	} else if (photo) {
		item = history->makeMessage(
			std::move(fields),
			not_null<PhotoData*>(photo),
			text);
	} else {
		item = history->makeMessage(
			std::move(fields),
			text,
			MTP_messageMediaEmpty());
	}
	if (forwarded) {
		item->updateForwardedInfo(&*forwarded);
	}
	if (message.flags & int(Flag::f_views)) {
		item->changeViewsCount(message.views);
	}
	item->setDeleted(true);
	item->markDeletedAnimated();
	return item;
}

} // namespace

template<typename DerivedMessage>
std::vector<AyuMessageBase> convertToBase(const std::vector<DerivedMessage> &messages) {
	std::vector<AyuMessageBase> based;
	based.reserve(messages.size());
	for (const auto &msg : messages) {
		based.push_back(static_cast<AyuMessageBase>(msg));
	}
	return based;
}

void map(not_null<HistoryItem*> item, AyuMessageBase &message) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;

	message.userId = userId;
	message.dialogId = getDialogIdFromPeer(item->history()->peer);
	message.groupedId = item->groupId().raw();
	message.peerId = item->history()->peer->id.value & PeerId::kChatTypeMask;
	message.fromId = item->from()->id.value & PeerId::kChatTypeMask;
	if (item->topic()) {
		message.topicId = item->topicRootId().bare;
	} else {
		message.topicId = 0;
	}
	message.messageId = item->id.bare;
	message.date = item->date();
	message.flags = AyuMapper::mapItemFlagsToMTPFlags(item);

	if (const auto edited = item->Get<HistoryMessageEdited>()) {
		message.editDate = edited->date;
	} else {
		message.editDate = base::unixtime::now();
	}

	message.views = item->viewsCount();
	message.fwdFlags = 0;
	message.fwdFromId = 0;
	// message.fwdName
	message.fwdDate = 0;
	// message.fwdPostAuthor
	if (const auto msgsigned = item->Get<HistoryMessageSigned>()) {
		message.postAuthor = msgsigned->author.toStdString();
	}
	message.replyFlags = 0;
	message.replyMessageId = 0;
	message.replyPeerId = 0;
	message.replyTopId = 0;
	message.replyForumTopic = false;
	// message.replySerialized
	// message.replyMarkupSerialized
	message.entityCreateDate = base::unixtime::now();

	auto serializedText = AyuMapper::serializeTextWithEntities(item);
	message.text = serializedText.first;
	message.textEntities = serializedText.second;

	// todo: implement mapping
	message.mediaPath = "/";
	// message.hqThumbPath
	message.documentType = 0; // document type none
	// message.documentSerialized
	// message.thumbsSerialized
	// message.documentAttributesSerialized
	// message.mimeType
}

void addEditedMessage(not_null<HistoryItem *> item) {
	EditedMessage message;
	map(item, message);

	if (message.text.empty()) {
		return;
	}

	AyuDatabase::addEditedMessage(message);
}

std::vector<AyuMessageBase> getEditedMessages(
		ID userId,
		ID dialogId,
		ID messageId,
		ID minId,
		ID maxId,
		int totalLimit) {
	return convertToBase(AyuDatabase::getEditedMessages(
		userId,
		dialogId,
		messageId,
		minId,
		maxId,
		totalLimit));
}

bool hasRevisions(not_null<HistoryItem*> item) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto msgId = item->id.bare;

	return AyuDatabase::hasRevisions(userId, dialogId, msgId);
}

bool addDeletedMessage(not_null<HistoryItem*> item) {
	auto message = DeletedMessage();
	map(item, message);
	message.snapshot = SaveSnapshot(item);

	if (message.text.empty() && message.snapshot.empty()) {
		return false;
	}

	const auto saved = AyuDatabase::addDeletedMessage(message);
	return saved && !message.snapshot.empty();
}

bool canUnloadDeletedMessage(not_null<HistoryItem*> item) {
	if (!item->isDeletedSaved()
		|| item->history()->owner().queryItemVisibility(item)) {
		return false;
	}
	if (const auto media = item->media()) {
		if (const auto document = media->document()) {
			if (document->loading()) {
				return false;
			}
			const auto types = { AudioMsgId::Type::Song, AudioMsgId::Type::Voice };
			for (const auto type : types) {
				const auto current = Media::Player::instance()->current(type);
				if (current.audio() == document
					&& current.contextId() == item->fullId()) {
					return false;
				}
			}
		} else if (const auto photo = media->photo()) {
			if (photo->loading()) {
				return false;
			}
		}
	}
	return true;
}

std::vector<not_null<HistoryItem*>> restoreDeletedMessages(
		not_null<History*> history,
		const QVector<MTPMessage> &slice,
		bool newer) {
	if (!AyuSettings::getInstance().saveDeletedMessages()) {
		return history->createItems(slice);
	}
	auto first = ID(ServerMaxMsgId.bare);
	auto last = ID(0);
	for (const auto &message : slice) {
		const auto id = ID(IdFromMessage(message).bare);
		if (IsServerMsgId(MsgId(id))) {
			first = std::min(first, id);
			last = std::max(last, id);
		}
	}
	const auto edge = newer ? history->maxMsgId() : history->minMsgId();
	const auto min = newer ? ID(edge.bare)
		: last ? first - 1 : ID(0);
	const auto atEnd = history->loadedAtBottom()
		|| (history->lastMessage() && last >= history->lastMessage()->id.bare);
	const auto max = !newer && edge ? ID(edge.bare)
		: (!last || atEnd) ? ID(ServerMaxMsgId.bare) : last + 1;
	if (min >= max) {
		return history->createItems(slice);
	}
	const auto messages = AyuDatabase::getDeletedMessagesSlice(
		history->session().userId().bare & PeerId::kChatTypeMask,
		getDialogIdFromPeer(history->peer),
		min,
		max,
		kDeletedMessagesPerPage,
		newer);
	const auto clipped = (messages.size() == kDeletedMessagesPerPage);
	const auto boundary = messages.empty() ? ID(0) : messages.back().messageId;
	auto source = QVector<MTPMessage>();
	for (const auto &message : slice) {
		const auto id = IdFromMessage(message).bare;
		if (!clipped || (newer ? id <= boundary : id >= boundary)) {
			source.push_back(message);
		}
	}
	auto result = std::vector<not_null<HistoryItem*>>();
	for (const auto &message : messages) {
		const auto existing = history->owner().message(
			history->peer,
			message.messageId);
		const auto item = existing ? existing : RestoreSnapshot(history, message);
		if (item) {
			if (existing) {
				item->removeMainView(Data::ViewRemovalReason::Detached);
				item->setDeleted(item->isDeletedSaved());
				item->markDeletedAnimated();
			}
			result.push_back(item);
		}
	}
	const auto live = history->createItems(source);
	result.insert(result.end(), live.begin(), live.end());
	ranges::sort(result, ranges::less(), [](not_null<HistoryItem*> item) {
		return item->id;
	});
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

std::vector<AyuMessageBase> getDeletedMessages(
		ID userId,
		ID dialogId,
		ID topicId,
		ID minId,
		ID maxId,
		int totalLimit,
		const QString &searchQuery) {
	return convertToBase(AyuDatabase::getDeletedMessages(
		userId,
		dialogId,
		topicId,
		minId,
		maxId,
		totalLimit,
		searchQuery.toStdString()));
}

bool hasDeletedMessages(not_null<PeerData*> peer, ID topicId) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	return AyuDatabase::hasDeletedMessages(userId, getDialogIdFromPeer(peer), topicId);
}

void removeDeletedMessage(not_null<HistoryItem*> item) {
	const auto peer = item->history()->peer;
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	AyuDatabase::removeDeletedMessage(userId, getDialogIdFromPeer(peer), item->id.bare);
}

void clearDeletedMessages(not_null<PeerData*> peer, ID topicId) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	AyuDatabase::clearDeletedMessages(userId, getDialogIdFromPeer(peer), topicId);
}

}
