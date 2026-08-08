// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/message_history/history_item.h"

#include "history/history_item.h"
#include "api/api_text_entities.h"
#include "ayu/data/entities.h"
#include "ayu/ui/message_history/history_inner.h"
#include "ayu/utils/ayu_mapper.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "data/data_channel.h"
#include "data/data_file_origin.h"
#include "data/data_forum_topic.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/stickers/data_stickers_set.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/view/history_view_element.h"
#include "ui/basic_click_handlers.h"
#include "ui/text/text_utilities.h"

namespace MessageHistory {

OwnedItem::OwnedItem(std::nullptr_t) {
}

OwnedItem::OwnedItem(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<HistoryItem*> data)
	: _data(data), _view(_data->createView(delegate)) {
}

OwnedItem::OwnedItem(OwnedItem &&other)
	: _data(base::take(other._data)), _view(base::take(other._view)) {
}

OwnedItem &OwnedItem::operator=(OwnedItem &&other) {
	_data = base::take(other._data);
	_view = base::take(other._view);
	return *this;
}

OwnedItem::~OwnedItem() {
	clearView();
	if (_data) {
		_data->destroy();
	}
}

void OwnedItem::refreshView(
	not_null<HistoryView::ElementDelegate*> delegate) {
	_view = _data->createView(delegate);
}

void OwnedItem::clearView() {
	_view = nullptr;
}

void GenerateItems(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<History*> history,
	AyuMessageBase message,
	Fn<void(OwnedItem item, TimeId sentDate, MsgId)> callback) {
	PeerData *from = history->owner().userLoaded(message.fromId);
	if (!from) {
		from = history->owner().channelLoaded(message.fromId);
	}
	if (!from) {
		from = reinterpret_cast<PeerData*>(history->owner().chatLoaded(message.fromId));
	}
	const auto date = message.entityCreateDate;
	const auto addPart = [&](
		not_null<HistoryItem*> item,
		TimeId sentDate = 0,
		MsgId realId = MsgId())
	{
		return callback(OwnedItem(delegate, item), sentDate, realId);
	};

	const auto resolveMedia = [&]() -> MTPMessageMedia {
		if (message.documentType == AyuMapper::kDocumentTypeNone
			|| message.documentSerialized.empty()) {
			return MTP_messageMediaEmpty();
		}

		if (message.documentType == AyuMapper::kDocumentTypePhoto) {
			auto result = MTP_messageMediaEmpty();
			AyuMapper::deserializeObject<MTPInputPhoto>(
				message.documentSerialized
			).match([&](const MTPDinputPhoto &inp) {
				const auto photo = history->owner().photo(inp.vid().v);
				if (!photo->hasExact(Data::PhotoSize::Large)) {
					return;
				}
				photo->mtpInput().match([&](const MTPDinputPhoto &pi) {
					using Flag = MTPDmessageMediaPhoto::Flag;
					result = MTP_messageMediaPhoto(
						MTP_flags(Flag::f_photo),
						MTP_photo(
							MTP_flags(0),
							MTP_long(photo->id),
							MTP_long(pi.vaccess_hash().v),
							MTP_bytes(pi.vfile_reference().v),
							MTP_int(photo->date()),
							MTP_vector<MTPPhotoSize>(),
							MTPVector<MTPVideoSize>(),
							MTP_int(photo->getDC())),
						MTPint(),
						MTPDocument());
				}, [](const MTPDinputPhotoEmpty &) {});
			}, [](const MTPDinputPhotoEmpty &) {});
			return result;
		}

		const auto buildDocumentAttributes = [](
			not_null<DocumentData*> doc) -> QVector<MTPDocumentAttribute>
		{
			auto attrs = QVector<MTPDocumentAttribute>();
			const auto filename = doc->filename();
			if (!filename.isEmpty()) {
				attrs.push_back(MTP_documentAttributeFilename(
					MTP_string(filename)));
			}
			const auto dims = doc->dimensions;
			if (dims.width() > 0 && dims.height() > 0) {
				if (doc->hasDuration() && !doc->hasMimeType(u"image/gif"_q)) {
					auto flags = MTPDdocumentAttributeVideo::Flags(0);
					using VideoFlag = MTPDdocumentAttributeVideo::Flag;
					if (doc->isVideoMessage()) {
						flags |= VideoFlag::f_round_message;
					}
					if (doc->supportsStreaming()) {
						flags |= VideoFlag::f_supports_streaming;
					}
					attrs.push_back(MTP_documentAttributeVideo(
						MTP_flags(flags),
						MTP_double(doc->duration() / 1000.),
						MTP_int(dims.width()),
						MTP_int(dims.height()),
						MTPint(),
						MTPdouble(),
						MTPstring()));
				} else {
					attrs.push_back(MTP_documentAttributeImageSize(
						MTP_int(dims.width()),
						MTP_int(dims.height())));
				}
			} else if (doc->hasDuration() && (doc->isVideoFile() || doc->isVideoMessage())) {
				auto flags = MTPDdocumentAttributeVideo::Flags(0);
				using VideoFlag = MTPDdocumentAttributeVideo::Flag;
				if (doc->isVideoMessage()) {
					flags |= VideoFlag::f_round_message;
				}
				if (doc->supportsStreaming()) {
					flags |= VideoFlag::f_supports_streaming;
				}
				attrs.push_back(MTP_documentAttributeVideo(
					MTP_flags(flags),
					MTP_double(doc->duration() / 1000.),
					MTP_int(0),
					MTP_int(0),
					MTPint(),
					MTPdouble(),
					MTPstring()));
			}
			if (doc->type == AnimatedDocument) {
				attrs.push_back(MTP_documentAttributeAnimated());
			} else if (doc->type == StickerDocument) {
				if (const auto sticker = doc->sticker()) {
					attrs.push_back(MTP_documentAttributeSticker(
						MTP_flags(0),
						MTP_string(sticker->alt),
						Data::InputStickerSet(sticker->set),
						MTPMaskCoords()));
				}
			} else if (const auto song = doc->song()) {
				const auto flags = MTPDdocumentAttributeAudio::Flag::f_title
					| MTPDdocumentAttributeAudio::Flag::f_performer;
				attrs.push_back(MTP_documentAttributeAudio(
					MTP_flags(flags),
					MTP_int(int(doc->duration() / 1000)),
					MTP_string(song->title),
					MTP_string(song->performer),
					MTPstring()));
			} else if (doc->voice()) {
				attrs.push_back(MTP_documentAttributeAudio(
					MTP_flags(MTPDdocumentAttributeAudio::Flag::f_voice),
					MTP_int(int(doc->duration() / 1000)),
					MTPstring(),
					MTPstring(),
					MTPbytes()));
			}
			return attrs;
		};

		auto result = MTP_messageMediaEmpty();
		AyuMapper::deserializeObject<MTPInputDocument>(
			message.documentSerialized
		).match([&](const MTPDinputDocument &inp) {
			const auto doc = history->owner().document(inp.vid().v);
			if (!doc->hasRemoteLocation()) {
				return;
			}
			doc->mtpInput().match([&](const MTPDinputDocument &di) {
				using Flag = MTPDmessageMediaDocument::Flag;
				result = MTP_messageMediaDocument(
					MTP_flags(Flag::f_document),
					MTP_document(
						MTP_flags(0),
						MTP_long(doc->id),
						MTP_long(di.vaccess_hash().v),
						MTP_bytes(doc->fileReference()),
						MTP_int(doc->date),
						MTP_string(doc->mimeString()),
						MTP_long(doc->size),
						MTP_vector<MTPPhotoSize>(),
						MTPVector<MTPVideoSize>(),
						MTP_int(doc->getDC()),
						MTP_vector<MTPDocumentAttribute>(
							buildDocumentAttributes(doc))),
					MTPVector<MTPDocument>(),
					MTPPhoto(),
					MTPint(),
					MTPint());
			}, [](const MTPDinputDocumentEmpty &) {});
		}, [](const MTPDinputDocumentEmpty &) {});
		return result;
	};

	const auto makeSimpleTextMessage = [&](
		TextWithEntities &&text,
		MTPMessageMedia &&media)
	{
		base::flags<MessageFlag> flags = MessageFlag::AdminLogEntry;
		if (from) {
			flags |= MessageFlag::HasFromId;
		} else {
			flags |= MessageFlag::HasPostAuthor;
		}
		if (!message.postAuthor.empty()) {
			flags |= MessageFlag::HasPostAuthor;
		}

		return history->makeMessage({
										.id = history->nextNonHistoryEntryId(),
										.flags = flags,
										.from = from ? from->id : 0,
										.date = date,
										.postAuthor = !message.postAuthor.empty()
														  ? QString::fromStdString(message.postAuthor)
														  : from
																? QString()
																: QString("unknown user: %1").arg(message.fromId),
									},
									std::move(text),
									std::move(media));
	};

	const auto addSimpleTextMessage = [&](TextWithEntities &&text)
	{
		addPart(makeSimpleTextMessage(std::move(text), resolveMedia()));
	};

	const auto text = QString::fromStdString(message.text);
	auto textAndEntities = Ui::Text::WithEntities(text);
	const auto entities = AyuMapper::deserializeTextWithEntities(message.textEntities);
	textAndEntities.entities = Api::EntitiesFromMTP(&history->session(), entities.v);
	addSimpleTextMessage(std::move(textAndEntities));
}

} // namespace MessageHistory
