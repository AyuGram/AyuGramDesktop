// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/forward/ayu_forward_rich.h"

#include "ayu/features/forward/ayu_sync.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/flat_map.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "history/history.h"
#include "iv/iv_rich_message_serializer.h"
#include "iv/iv_rich_page.h"
#include "storage/localimageloader.h"

namespace AyuForward {
namespace {

using Block = Iv::RichPage::Block;
using BlockKind = Iv::RichPage::BlockKind;

struct RichMedia
{
	base::flat_map<PhotoId, not_null<PhotoData*>> photos;
	base::flat_map<DocumentId, not_null<DocumentData*>> documents;
};

[[nodiscard]] PhotoData *resolvePhoto(not_null<Main::Session*> session, PhotoId id, PhotoData *photo) {
	return photo ? photo : (id ? session->data().photo(id).get() : nullptr);
}

[[nodiscard]] DocumentData *resolveDocument(not_null<Main::Session*> session,
											DocumentId id,
											DocumentData *document) {
	return document ? document : (id ? session->data().document(id).get() : nullptr);
}

[[nodiscard]] bool isSerializableKind(BlockKind kind) {
	switch (kind) {
	case BlockKind::Unsupported:
	case BlockKind::AuthorDate:
	case BlockKind::Embed:
	case BlockKind::EmbedPost:
	case BlockKind::Channel:
	case BlockKind::RelatedArticles:
		return false;
	default:
		return true;
	}
}

void collectMedia(not_null<Main::Session*> session, const std::vector<Block> &blocks, RichMedia &media) {
	const auto addPhoto = [&](PhotoId id, PhotoData *photo)
	{
		if (const auto resolved = resolvePhoto(session, id, photo)) {
			media.photos.emplace(resolved->id, resolved);
		}
	};
	const auto addDocument = [&](DocumentId id, DocumentData *document)
	{
		if (const auto resolved = resolveDocument(session, id, document)) {
			media.documents.emplace(resolved->id, resolved);
		}
	};

	for (const auto &block : blocks) {
		if (!isSerializableKind(block.kind)) {
			continue;
		}

		switch (block.kind) {
		case BlockKind::Photo:
			addPhoto(block.photoId, block.photo);
			break;
		case BlockKind::Video:
		case BlockKind::Audio:
			addDocument(block.documentId, block.document);
			break;
		case BlockKind::GroupedMedia:
			for (const auto &item : block.mediaItems) {
				if (item.kind == BlockKind::Photo) {
					addPhoto(item.photoId, item.photo);
				} else if (item.kind == BlockKind::Video) {
					addDocument(item.documentId, item.document);
				}
			}
			break;
		default:
			break;
		}

		collectMedia(session, block.blocks, media);
		for (const auto &item : block.listItems) {
			collectMedia(session, item.blocks, media);
		}
	}
}

[[nodiscard]] RichMedia reuploadMedia(not_null<Main::Session*> session,
									  not_null<PeerData*> peer,
									  Data::FileOrigin origin,
									  const RichMedia &source,
									  const Fn<bool()> &cancelled) {
	auto result = RichMedia();

	const auto ensureUploaded = [&](const QString &path,
									int64 expected,
									const Fn<void()> &load,
									SendMediaType type,
									bool forceFile)
	{
		const auto ready = [&]
		{
			const auto size = AyuSync::fileSize(path);
			return size > 0 && size >= expected;
		};
		if (!ready()) {
			load();
			if (!ready()) {
				return AyuSync::UploadedFile();
			}
		}
		return AyuSync::uploadFileSync(session, peer, path, type, forceFile);
	};

	for (const auto &[id, photo] : source.photos) {
		if (cancelled()) {
			return result;
		}

		const auto uploaded = ensureUploaded(
			AyuSync::filePath(session, photo),
			photo->imageByteSize(Data::PhotoSize::Large),
			[&] {
				AyuSync::loadPhotoSync(
					session,
					photo,
					origin,
					cancelled);
			},
			SendMediaType::Photo,
			false);
		if (uploaded.photo) {
			result.photos.emplace(id, uploaded.photo);
		} else {
			LOG(("AyuForward: failed to transfer photo %1 for rich message").arg(id));
		}
	}

	for (const auto &[id, document] : source.documents) {
		if (cancelled()) {
			return result;
		}

		const auto playable = document->isVideoFile()
			|| document->isGifv()
			|| document->isSong()
			|| document->isAudioFile()
			|| document->isVoiceMessage();
		const auto path = AyuSync::loadDocumentSync(
			session,
			document,
			origin,
			cancelled);
		const auto pathInfo = QFileInfo(path);
		auto uploaded = AyuSync::UploadedFile();
		if (!path.isEmpty()
			&& pathInfo.isFile()
			&& pathInfo.size() == document->size) {
			uploaded = AyuSync::uploadFileSync(
				session,
				peer,
				path,
				SendMediaType::File,
				!playable,
				AyuSync::documentFileName(document));
		}
		if (uploaded.document) {
			result.documents.emplace(id, uploaded.document);
		} else {
			LOG(("AyuForward: failed to transfer document %1 for rich message").arg(id));
		}
	}

	return result;
}

[[nodiscard]] bool runOnMainSync(Fn<void()> callback) {
	auto latch = std::make_shared<TimedCountDownLatch>(1);
	crl::on_main([latch, callback = std::move(callback)]
	{
		callback();
		latch->countDown();
	});
	return latch->await(std::chrono::minutes(1));
}

void sanitizeRichText(Iv::RichPage::RichText &text) {
	text.anchorId = QString();
	text.anchorIds.clear();
	const auto autolink = [](const EntityInText &entity)
	{
		switch (entity.type()) {
		case EntityType::Mention:
		case EntityType::Hashtag:
		case EntityType::BotCommand:
		case EntityType::Cashtag:
		case EntityType::Url:
		case EntityType::Email:
		case EntityType::Phone:
		case EntityType::BankCard:
			return true;
		default:
			return false;
		}
	};
	const auto removed = std::ranges::remove_if(text.text.entities, autolink);
	text.text.entities.erase(removed.begin(), removed.end());
}

void pruneBlocks(not_null<Main::Session*> session, std::vector<Block> &blocks, const RichMedia &remap);

[[nodiscard]] bool prepareBlock(not_null<Main::Session*> session, Block &block, const RichMedia &remap) {
	if (!isSerializableKind(block.kind)) {
		return false;
	}

	const auto remapPhoto = [&](PhotoId &id, PhotoData *&photo)
	{
		const auto resolved = resolvePhoto(session, id, photo);
		const auto i = resolved ? remap.photos.find(resolved->id) : remap.photos.end();
		if (i == remap.photos.end()) {
			return false;
		}
		photo = i->second;
		id = i->second->id;
		return true;
	};
	const auto remapDocument = [&](DocumentId &id, DocumentData *&document)
	{
		const auto resolved = resolveDocument(session, id, document);
		const auto i = resolved ? remap.documents.find(resolved->id) : remap.documents.end();
		if (i == remap.documents.end()) {
			return false;
		}
		document = i->second;
		id = i->second->id;
		return true;
	};

	switch (block.kind) {
	case BlockKind::Photo:
		if (!remapPhoto(block.photoId, block.photo)) {
			return false;
		}
		break;
	case BlockKind::Video:
	case BlockKind::Audio:
		if (!remapDocument(block.documentId, block.document)) {
			return false;
		}
		break;
	case BlockKind::GroupedMedia: {
		auto &items = block.mediaItems;
		for (auto i = items.begin(); i != items.end();) {
			auto kept = false;
			if (i->kind == BlockKind::Photo) {
				kept = remapPhoto(i->photoId, i->photo);
			} else if (i->kind == BlockKind::Video) {
				kept = remapDocument(i->documentId, i->document);
			}
			i = kept ? (i + 1) : items.erase(i);
		}
		if (items.empty()) {
			return false;
		}
		break;
	}
	case BlockKind::Anchor:
		if (block.anchorId.isEmpty()) {
			return false;
		}
		break;
	case BlockKind::Quote:
		if (block.pullquote && !block.blocks.empty()) {
			return false;
		}
		break;
	case BlockKind::Code:
		if (!block.blocks.empty()) {
			return false;
		}
		break;
	case BlockKind::Map:
		if (block.zoom <= 0) {
			return false;
		}
		break;
	default:
		break;
	}

	sanitizeRichText(block.text);
	sanitizeRichText(block.caption);
	if (block.kind != BlockKind::Anchor) {
		block.anchorId = QString();
	}
	for (auto &row : block.tableRows) {
		for (auto &cell : row.cells) {
			sanitizeRichText(cell.text);
		}
	}

	pruneBlocks(session, block.blocks, remap);
	for (auto &item : block.listItems) {
		sanitizeRichText(item.text);
		item.anchorId = QString();
		pruneBlocks(session, item.blocks, remap);
	}
	return true;
}

void pruneBlocks(not_null<Main::Session*> session, std::vector<Block> &blocks, const RichMedia &remap) {
	auto write = blocks.begin();
	for (auto read = blocks.begin(); read != blocks.end(); ++read) {
		if (prepareBlock(session, *read, remap)) {
			if (write != read) {
				*write = std::move(*read);
			}
			++write;
		}
	}
	blocks.erase(write, blocks.end());
}

} // namespace

bool forwardRichMessage(
	not_null<Main::Session*> session,
	FullMsgId itemId,
	const Api::SendAction &action,
	Fn<bool()> cancelled) {
	if (!cancelled) {
		cancelled = []
		{
			return false;
		};
	}

	const auto source = AyuSync::loadFullRichPageSync(session, itemId);
	if (!source || cancelled()) {
		return false;
	}

	struct PrepareState
	{
		Iv::RichPage page;
		RichMedia media;
		bool premiumBlocked = false;
	};
	const auto prepared = std::make_shared<PrepareState>();

	const auto preparedOk = runOnMainSync([=]
	{
		prepared->page = *source;
		prepared->page.part = false;
		prepared->page.views = 0;
		prepared->premiumBlocked = !session->premium()
			&& Iv::RichPageUsesPremiumFormatting(prepared->page);
		if (!prepared->premiumBlocked) {
			collectMedia(session, prepared->page.blocks, prepared->media);
		}
	});
	if (!preparedOk || prepared->premiumBlocked || cancelled()) {
		return false;
	}

	const auto uploaded = reuploadMedia(
		session,
		action.history->peer,
		itemId,
		prepared->media,
		cancelled);
	if (cancelled()) {
		return false;
	}

	const auto serialized = std::make_shared<std::optional<MTPInputRichMessage>>();
	const auto serializedOk = runOnMainSync([=]
	{
		pruneBlocks(session, prepared->page.blocks, uploaded);
		if (prepared->page.blocks.empty()) {
			return;
		}
		const auto result = Iv::SerializeInputRichMessage(
			session,
			prepared->page,
			Iv::SerializeInputRichMessageMode::FinalSubmit);
		if (result.status == Iv::SerializeInputRichMessageStatus::Success) {
			*serialized = result.value;
		}
	});
	if (!serializedOk || !serialized->has_value() || cancelled()) {
		return false;
	}

	return AyuSync::sendRichMessageSync(session, **serialized, action);
}

} // namespace AyuForward
