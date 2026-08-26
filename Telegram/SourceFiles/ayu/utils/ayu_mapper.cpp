// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/utils/ayu_mapper.h"

#include "apiwrap.h"
#include "api/api_text_entities.h"
#include "core/file_location.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/stickers/data_stickers.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
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

MTPVector<MTPMessageEntity> deserializeTextWithEntities(const std::vector<char> &serialized) {
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

void mapMediaToMessage(not_null<HistoryItem*> item, AyuMessageBase &message) {
	const auto media = item->media();
	if (!media) {
		message.documentType = kDocumentTypeNone;
		return;
	}

	if (const auto photo = media->photo()) {
		message.documentType = kDocumentTypePhoto;

		uint64 accessHash = 0;
		QByteArray fileRef = photo->fileReference();
		photo->mtpInput().match([&](const MTPDinputPhoto &p) {
			accessHash = p.vaccess_hash().v;
			if (fileRef.isEmpty()) {
				fileRef = p.vfile_reference().v;
			}
		}, [](const MTPDinputPhotoEmpty &) {});

		auto photoSizes = QVector<MTPPhotoSize>();
		const auto w = std::max(photo->width(), 1);
		const auto h = std::max(photo->height(), 1);

		const auto stripped = photo->inlineThumbnailBytes();
		if (!stripped.isEmpty()) {
			photoSizes.push_back(MTP_photoStrippedSize(
				MTP_string("i"),
				MTP_bytes(stripped)));
		}

		if (photo->hasExact(Data::PhotoSize::Thumbnail)) {
			if (const auto sz = photo->size(Data::PhotoSize::Thumbnail)) {
				photoSizes.push_back(MTP_photoSize(
					MTP_string("m"),
					MTP_int(sz->width()),
					MTP_int(sz->height()),
					MTP_int(photo->imageByteSize(Data::PhotoSize::Thumbnail))));
			}
		}

		const auto largeByteSize = photo->imageByteSize(Data::PhotoSize::Large);
		photoSizes.push_back(MTP_photoSize(
			MTP_string("y"),
			MTP_int(w),
			MTP_int(h),
			MTP_int(largeByteSize > 0 ? largeByteSize : 0)));

		const auto mtpPhoto = MTP_photo(
			MTP_flags(0),
			MTP_long(photo->id),
			MTP_long(accessHash),
			MTP_bytes(fileRef),
			MTP_int(photo->date()),
			MTP_vector<MTPPhotoSize>(photoSizes),
			MTPVector<MTPVideoSize>(),
			MTP_int(photo->getDC()));

		using PhotoFlag = MTPDmessageMediaPhoto::Flag;
		auto flags = MTPDmessageMediaPhoto::Flags(PhotoFlag::f_photo);
		if (media->hasSpoiler()) {
			flags |= PhotoFlag::f_spoiler;
		}
		const auto mtpMedia = MTP_messageMediaPhoto(
			MTP_flags(flags),
			mtpPhoto,
			MTPint(),
			MTPDocument());

		message.documentSerialized = serializeObject(MTPMessageMedia(mtpMedia));

		const auto loc = photo->location(true);
		if (!loc.isEmpty()) {
			message.mediaPath = loc.name().toStdString();
		}
	} else if (const auto doc = media->document()) {
		if (doc->isVoiceMessage()) {
			message.documentType = kDocumentTypeVoice;
		} else if (doc->isVideoMessage()) {
			message.documentType = kDocumentTypeVideoNote;
		} else if (doc->isAnimation() || doc->isGifv()) {
			message.documentType = kDocumentTypeAnimation;
		} else if (doc->sticker()) {
			message.documentType = kDocumentTypeSticker;
		} else if (doc->isVideoFile()) {
			message.documentType = kDocumentTypeVideo;
		} else if (doc->isAudioFile()) {
			message.documentType = kDocumentTypeAudio;
		} else {
			message.documentType = kDocumentTypeFile;
		}
		message.mimeType = doc->mimeString().toStdString();

		uint64 accessHash = 0;
		QByteArray fileRef = doc->fileReference();
		doc->mtpInput().match([&](const MTPDinputDocument &d) {
			accessHash = d.vaccess_hash().v;
			if (fileRef.isEmpty()) {
				fileRef = d.vfile_reference().v;
			}
		}, [](const MTPDinputDocumentEmpty &) {});

		auto attributes = QVector<MTPDocumentAttribute>();

		const auto filename = doc->filename();
		if (!filename.isEmpty()) {
			attributes.push_back(MTP_documentAttributeFilename(MTP_string(filename)));
		}

		const auto dims = doc->dimensions;
		const auto durationSec = int(doc->duration() / 1000);
		const auto durationDouble = doc->duration() / 1000.;

		if (doc->isVoiceMessage()) {
			auto flags = MTPDdocumentAttributeAudio::Flags(MTPDdocumentAttributeAudio::Flag::f_voice);
			QByteArray waveBytes;
			if (const auto voice = doc->voice()) {
				if (!voice->waveform.isEmpty()) {
					flags |= MTPDdocumentAttributeAudio::Flag::f_waveform;
					waveBytes = documentWaveformEncode5bit(voice->waveform);
				}
			}
			attributes.push_back(MTP_documentAttributeAudio(
				MTP_flags(flags),
				MTP_int(durationSec),
				MTPstring(),
				MTPstring(),
				MTP_bytes(waveBytes)));
		} else if (doc->isVideoMessage() || doc->isVideoFile()) {
			auto flags = MTPDdocumentAttributeVideo::Flags(0);
			using VideoFlag = MTPDdocumentAttributeVideo::Flag;
			if (doc->isVideoMessage()) {
				flags |= VideoFlag::f_round_message;
			}
			if (doc->supportsStreaming()) {
				flags |= VideoFlag::f_supports_streaming;
			}
			attributes.push_back(MTP_documentAttributeVideo(
				MTP_flags(flags),
				MTP_double(durationDouble),
				MTP_int(dims.width()),
				MTP_int(dims.height()),
				MTPint(),
				MTPdouble(),
				MTPstring()));
		} else if (const auto song = doc->song()) {
			const auto flags = MTPDdocumentAttributeAudio::Flag::f_title
				| MTPDdocumentAttributeAudio::Flag::f_performer;
			attributes.push_back(MTP_documentAttributeAudio(
				MTP_flags(flags),
				MTP_int(durationSec),
				MTP_string(song->title),
				MTP_string(song->performer),
				MTPbytes()));
		} else if (const auto sticker = doc->sticker()) {
			MTPInputStickerSet inputSet = MTP_inputStickerSetEmpty();
			if (sticker->set.id != 0) {
				inputSet = MTP_inputStickerSetID(
					MTP_long(sticker->set.id),
					MTP_long(sticker->set.accessHash));
			} else if (!sticker->set.shortName.isEmpty()) {
				inputSet = MTP_inputStickerSetShortName(
					MTP_string(sticker->set.shortName));
			}
			attributes.push_back(MTP_documentAttributeSticker(
				MTP_flags(0),
				MTP_string(sticker->alt),
				inputSet,
				MTPMaskCoords()));
			if (dims.width() > 0 && dims.height() > 0) {
				attributes.push_back(MTP_documentAttributeImageSize(
					MTP_int(dims.width()),
					MTP_int(dims.height())));
			}
		} else if (doc->isAnimation() || doc->isGifv()) {
			attributes.push_back(MTP_documentAttributeAnimated());
			if (dims.width() > 0 && dims.height() > 0) {
				attributes.push_back(MTP_documentAttributeImageSize(
					MTP_int(dims.width()),
					MTP_int(dims.height())));
			}
		} else if (dims.width() > 0 && dims.height() > 0) {
			attributes.push_back(MTP_documentAttributeImageSize(
				MTP_int(dims.width()),
				MTP_int(dims.height())));
		}

		auto thumbs = QVector<MTPPhotoSize>();
		const auto inlineThumb = doc->inlineThumbnailBytes();
		if (!inlineThumb.isEmpty()) {
			thumbs.push_back(MTP_photoStrippedSize(
				MTP_string("i"),
				MTP_bytes(inlineThumb)));
		} else if (doc->hasThumbnail()) {
			const auto loc = doc->thumbnailLocation();
			const auto byteSize = doc->thumbnailByteSize();
			thumbs.push_back(MTP_photoSize(
				MTP_string("m"),
				MTP_int(loc.width() > 0 ? loc.width() : 100),
				MTP_int(loc.height() > 0 ? loc.height() : 100),
				MTP_int(byteSize > 0 ? byteSize : 0)));
		}

		const auto mtpDoc = MTP_document(
			MTP_flags(0),
			MTP_long(doc->id),
			MTP_long(accessHash),
			MTP_bytes(fileRef),
			MTP_int(doc->date),
			MTP_string(doc->mimeString()),
			MTP_long(doc->size),
			MTP_vector<MTPPhotoSize>(thumbs),
			MTPVector<MTPVideoSize>(),
			MTP_int(doc->getDC()),
			MTP_vector<MTPDocumentAttribute>(attributes));

		using DocFlag = MTPDmessageMediaDocument::Flag;
		auto flags = MTPDmessageMediaDocument::Flags(DocFlag::f_document);
		if (doc->isVoiceMessage()) {
			flags |= DocFlag::f_voice;
		} else if (doc->isVideoMessage()) {
			flags |= DocFlag::f_round | DocFlag::f_video;
		} else if (doc->isVideoFile()) {
			flags |= DocFlag::f_video;
		}
		if (media->hasSpoiler()) {
			flags |= DocFlag::f_spoiler;
		}
		if (media->ttlSeconds() > 0) {
			flags |= DocFlag::f_ttl_seconds;
		}
		const auto mtpMedia = MTP_messageMediaDocument(
			MTP_flags(flags),
			mtpDoc,
			MTPVector<MTPDocument>(),
			MTPPhoto(),
			MTP_int(media->ttlSeconds() > 0 ? int(media->ttlSeconds() / 1000) : 0),
			MTPint());

		message.documentSerialized = serializeObject(MTPMessageMedia(mtpMedia));

		const auto loc = doc->location(true);
		if (!loc.isEmpty()) {
			message.mediaPath = loc.name().toStdString();
		} else {
			const auto fp = doc->filepath(true);
			if (!fp.isEmpty()) {
				message.mediaPath = fp.toStdString();
			}
		}
	}
}

}
