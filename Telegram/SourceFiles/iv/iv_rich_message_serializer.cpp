/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/iv_rich_message_serializer.h"

#include "base/flat_map.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "history/history.h"
#include "history/history_item.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "iv/markdown/iv_markdown_prepare_links.h"
#include "iv/markdown/iv_markdown_prepare_serialize.h"
#include "main/main_session.h"
#include "ui/text/text_utilities.h"

#include <algorithm>

namespace Iv {
namespace {

using Block = RichPage::Block;
using BlockKind = RichPage::BlockKind;
using GroupedMediaItem = RichPage::GroupedMediaItem;
using ListKind = RichPage::ListKind;
using RichText = RichPage::RichText;
using TableAlignment = RichPage::TableAlignment;
using TableCell = RichPage::TableCell;
using TableVerticalAlignment = RichPage::TableVerticalAlignment;
using TaskState = RichPage::TaskState;

constexpr auto kDefaultMapWidth = 400;
constexpr auto kDefaultMapHeight = 200;
constexpr auto kNoEntityIndex = -1;

[[nodiscard]] QString FormulaTexFromSource(QString source) {
	source = source.trimmed();
	if (source.size() >= 2
		&& source.front() == QChar('$')
		&& source.back() == QChar('$')) {
		source = source.mid(1, source.size() - 2).trimmed();
	}
	return source;
}

struct SerializeContext {
	not_null<Main::Session*> session;
	base::flat_map<uint64, MTPInputPhoto> photos;
	base::flat_map<uint64, MTPInputDocument> documents;
	base::flat_map<uint64, MTPInputUser> users;
};

[[nodiscard]] int EntitySerializationOrder(EntityType type) {
	switch (type) {
	case EntityType::CustomUrl: return 0;
	case EntityType::MentionName: return 1;
	case EntityType::Bold: return 2;
	case EntityType::Italic: return 3;
	case EntityType::Underline: return 4;
	case EntityType::StrikeOut: return 5;
	case EntityType::Code: return 6;
	case EntityType::Subscript: return 7;
	case EntityType::Superscript: return 8;
	case EntityType::Marked: return 9;
	case EntityType::Spoiler: return 10;
	case EntityType::CustomEmoji: return 11;
	case EntityType::FormattedDate: return 12;
	case EntityType::Mention: return 13;
	case EntityType::Hashtag: return 14;
	case EntityType::BotCommand: return 15;
	case EntityType::Cashtag: return 16;
	case EntityType::Url: return 17;
	case EntityType::Email: return 18;
	case EntityType::Phone: return 19;
	case EntityType::BankCard: return 20;
	case EntityType::Invalid:
	case EntityType::Semibold:
	case EntityType::MediaTimestamp:
	case EntityType::Colorized:
	case EntityType::Pre:
	case EntityType::Blockquote:
		break;
	}
	return 100;
}

[[nodiscard]] MTPRichText MakePlainRichText(const QString &text) {
	return text.isEmpty()
		? MTP_textEmpty()
		: MTP_textPlain(MTP_string(text));
}

[[nodiscard]] MTPRichText JoinRichTextParts(QVector<MTPRichText> &&parts) {
	if (parts.isEmpty()) {
		return MTP_textEmpty();
	} else if (parts.size() == 1) {
		return std::move(parts.front());
	}
	return MTP_textConcat(MTP_vector<MTPRichText>(std::move(parts)));
}

[[nodiscard]] MTPRichText WrapRichTextAnchor(
		MTPRichText text,
		const QString &anchorId) {
	return anchorId.isEmpty()
		? text
		: MTP_textAnchor(std::move(text), MTP_string(anchorId));
}

[[nodiscard]] bool HasRichTextContent(const RichText &text) {
	return !text.text.empty() || !text.anchorId.isEmpty();
}

[[nodiscard]] PhotoData *ResolvePhotoData(
		SerializeContext *context,
		uint64 id,
		PhotoData *photo) {
	return photo
		? photo
		: (id ? context->session->data().photo(id).get() : nullptr);
}

[[nodiscard]] DocumentData *ResolveDocumentData(
		SerializeContext *context,
		uint64 id,
		DocumentData *document) {
	return document
		? document
		: (id ? context->session->data().document(id).get() : nullptr);
}

[[nodiscard]] std::optional<MTPInputPhoto> ResolveInputPhoto(
		SerializeContext *context,
		uint64 id,
		PhotoData *photo) {
	const auto resolved = ResolvePhotoData(context, id, photo);
	if (!resolved) {
		return std::nullopt;
	}
	const auto input = resolved->mtpInput();
	return (input.type() == mtpc_inputPhoto
		&& input.c_inputPhoto().vid().v
		&& input.c_inputPhoto().vaccess_hash().v
		&& !resolved->fileReference().isEmpty())
		? std::make_optional(input)
		: std::nullopt;
}

[[nodiscard]] std::optional<MTPInputDocument> ResolveInputDocument(
		SerializeContext *context,
		uint64 id,
		DocumentData *document) {
	const auto resolved = ResolveDocumentData(context, id, document);
	if (!resolved) {
		return std::nullopt;
	}
	const auto input = resolved->mtpInput();
	return (resolved->hasRemoteLocation()
		&& input.type() == mtpc_inputDocument
		&& input.c_inputDocument().vid().v
		&& input.c_inputDocument().vaccess_hash().v
		&& !resolved->fileReference().isEmpty())
		? std::make_optional(input)
		: std::nullopt;
}

[[nodiscard]] std::optional<uint64> CollectPhoto(
		SerializeContext *context,
		uint64 id,
		PhotoData *photo) {
	if (const auto input = ResolveInputPhoto(context, id, photo)) {
		const auto serverId = uint64(input->c_inputPhoto().vid().v);
		context->photos.emplace(serverId, *input);
		return serverId;
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<uint64> CollectDocument(
		SerializeContext *context,
		uint64 id,
		DocumentData *document = nullptr) {
	if (const auto input = ResolveInputDocument(context, id, document)) {
		const auto serverId = uint64(input->c_inputDocument().vid().v);
		context->documents.emplace(serverId, *input);
		return serverId;
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<uint64> CollectMentionUser(
		SerializeContext *context,
		const QString &data) {
	const auto fields = TextUtilities::MentionNameDataToFields(data);
	if (!fields.userId || fields.selfId != context->session->userId().bare) {
		return std::nullopt;
	}
	if (context->users.find(fields.userId) != end(context->users)) {
		return fields.userId;
	}
	if (fields.userId == fields.selfId) {
		context->users.emplace(fields.userId, MTP_inputUserSelf());
		return fields.userId;
	}
	const auto user = context->session->data().user(UserId(fields.userId));
	if (user->isLoaded()) {
		context->users.emplace(fields.userId, user->inputUser());
		return fields.userId;
	}
	if (const auto item = user->owner().messageWithPeer(user->id)) {
		context->users.emplace(
			fields.userId,
			MTP_inputUserFromMessage(
				item->history()->peer->input(),
				MTP_int(int(item->id.bare)),
				MTP_long(fields.userId)));
		return fields.userId;
	}
	if (!fields.accessHash) {
		return std::nullopt;
	}
	context->users.emplace(
		fields.userId,
		MTP_inputUser(
			MTP_long(fields.userId),
			MTP_long(fields.accessHash)));
	return fields.userId;
}

[[nodiscard]] std::vector<EntityInText> SortedRichTextEntities(
		const TextWithEntities &text) {
	auto result = std::vector<EntityInText>();
	result.reserve(text.entities.size());
	const auto textLength = text.text.size();
	for (const auto &entity : text.entities) {
		const auto till = entity.offset() + entity.length();
		if (entity.offset() < 0
			|| entity.length() <= 0
			|| till > textLength) {
			continue;
		}
		result.push_back(entity);
	}
	std::sort(result.begin(), result.end(), [](const EntityInText &a, const EntityInText &b) {
		if (a.offset() != b.offset()) {
			return a.offset() < b.offset();
		}
		if (a.length() != b.length()) {
			return a.length() > b.length();
		}
		return EntitySerializationOrder(a.type())
			< EntitySerializationOrder(b.type());
	});
	return result;
}

[[nodiscard]] bool SkipEntityForRange(
		const std::vector<EntityInText> &entities,
		int index,
		int skipIndex) {
	if (index == skipIndex) {
		return true;
	} else if (skipIndex == kNoEntityIndex) {
		return false;
	}
	const auto &entity = entities[index];
	const auto &skip = entities[skipIndex];
	return (index < skipIndex)
		&& (entity.offset() == skip.offset())
		&& (entity.length() == skip.length());
}

[[nodiscard]] int FindOuterEntityAt(
		const std::vector<EntityInText> &entities,
		int position,
		int till,
		int skipIndex) {
	const auto count = int(entities.size());
	for (auto index = 0; index != count; ++index) {
		const auto &entity = entities[index];
		if (SkipEntityForRange(entities, index, skipIndex)) {
			continue;
		}
		if (entity.offset() == position
			&& entity.offset() + entity.length() <= till) {
			return index;
		}
		if (entity.offset() > position) {
			break;
		}
	}
	return kNoEntityIndex;
}

[[nodiscard]] std::optional<MTPRichText> SerializeRichTextRange(
		const QString &text,
		const std::vector<EntityInText> &entities,
		int from,
		int till,
		SerializeContext *context,
		int skipIndex);

[[nodiscard]] std::optional<MTPRichText> SerializeRichTextEntity(
		const QString &text,
		const std::vector<EntityInText> &entities,
		int entityIndex,
		SerializeContext *context) {
	const auto &entity = entities[entityIndex];
	const auto from = entity.offset();
	const auto length = entity.length();
	const auto segment = text.mid(from, length);
	const auto inner = SerializeRichTextRange(
		text,
		entities,
		from,
		from + length,
		context,
		entityIndex);
	if (!inner) {
		return std::nullopt;
	}
	switch (entity.type()) {
	case EntityType::Bold:
		return MTP_textBold(*inner);
	case EntityType::Italic:
		return MTP_textItalic(*inner);
	case EntityType::Underline:
		return MTP_textUnderline(*inner);
	case EntityType::StrikeOut:
		return MTP_textStrike(*inner);
	case EntityType::Code:
		return MTP_textFixed(*inner);
	case EntityType::Subscript:
		return MTP_textSubscript(*inner);
	case EntityType::Superscript:
		return MTP_textSuperscript(*inner);
	case EntityType::Marked:
		return MTP_textMarked(*inner);
	case EntityType::Spoiler:
		return MTP_textSpoiler(*inner);
	case EntityType::Mention:
		return MTP_textMention(*inner);
	case EntityType::Hashtag:
		return MTP_textHashtag(*inner);
	case EntityType::BotCommand:
		return MTP_textBotCommand(*inner);
	case EntityType::Cashtag:
		return MTP_textCashtag(*inner);
	case EntityType::Url:
		return MTP_textAutoUrl(*inner);
	case EntityType::Email:
		return MTP_textAutoEmail(*inner);
	case EntityType::Phone:
		return MTP_textAutoPhone(*inner);
	case EntityType::BankCard:
		return MTP_textBankCard(*inner);
	case EntityType::CustomUrl: {
		const auto data = entity.data();
		if (data.startsWith(u"mailto:"_q)) {
			return MTP_textEmail(*inner, MTP_string(data.mid(7)));
		} else if (data.startsWith(u"tel:"_q)) {
			return MTP_textPhone(*inner, MTP_string(data.mid(4)));
		}
		const auto decoded = DecodeRichPageLinkUrl(data);
		return MTP_textUrl(
			*inner,
			MTP_string(decoded ? decoded->url : data),
			MTP_long(0));
	}
	case EntityType::MentionName: {
		const auto userId = CollectMentionUser(context, entity.data());
		return userId
			? std::make_optional(MTP_textMentionName(
				*inner,
				MTP_long(*userId)))
			: std::nullopt;
	}
	case EntityType::CustomEmoji: {
		if (const auto parsed = Markdown::ParseInlineTextObjectEntity(
				entity.data())) {
			switch (parsed->kind) {
			case Markdown::InlineTextObjectKind::Formula: {
				const auto formula = std::get_if<
					Markdown::InlineTextObjectFormulaData>(&parsed->data);
				if (!formula) {
					return std::nullopt;
				}
				const auto source = !formula->trimmedTex.isEmpty()
					? formula->trimmedTex
					: FormulaTexFromSource(formula->copySource);
				return source.isEmpty()
					? std::optional<MTPRichText>(
						MakePlainRichText(segment))
					: std::optional<MTPRichText>(
						MTP_textMath(MTP_string(source)));
			}
			case Markdown::InlineTextObjectKind::IvImage: {
				const auto image = std::get_if<
					Markdown::InlineTextObjectIvImageData>(&parsed->data);
				return std::optional<MTPRichText>(MakePlainRichText(
					(image && !image->replacementText.isEmpty())
						? image->replacementText
						: u"[image]"_q));
			}
			}
		}
		const auto documentId = ::Data::ParseCustomEmojiData(entity.data());
		const auto collected = documentId
			? CollectDocument(context, documentId)
			: std::nullopt;
		return collected
			? std::optional<MTPRichText>(MTP_textCustomEmoji(
				MTP_long(*collected),
				MTP_string(segment.isEmpty() ? u"@"_q : segment)))
			: std::optional<MTPRichText>(MakePlainRichText(segment));
	}
	case EntityType::FormattedDate: {
		const auto [date, flags] = DeserializeFormattedDateData(entity.data());
		if (!date) {
			return *inner;
		}
		using Flag = MTPDtextDate::Flag;
		auto mtpFlags = MTPDtextDate::Flags();
		if (flags & FormattedDateFlag::Relative) {
			mtpFlags |= Flag::f_relative;
		}
		if (flags & FormattedDateFlag::ShortTime) {
			mtpFlags |= Flag::f_short_time;
		}
		if (flags & FormattedDateFlag::LongTime) {
			mtpFlags |= Flag::f_long_time;
		}
		if (flags & FormattedDateFlag::ShortDate) {
			mtpFlags |= Flag::f_short_date;
		}
		if (flags & FormattedDateFlag::LongDate) {
			mtpFlags |= Flag::f_long_date;
		}
		if (flags & FormattedDateFlag::DayOfWeek) {
			mtpFlags |= Flag::f_day_of_week;
		}
		return MTP_textDate(
			MTP_flags(mtpFlags),
			*inner,
			MTP_int(date));
	}
	case EntityType::Invalid:
	case EntityType::Semibold:
	case EntityType::MediaTimestamp:
	case EntityType::Colorized:
	case EntityType::Pre:
	case EntityType::Blockquote:
		break;
	}
	return *inner;
}

[[nodiscard]] std::optional<MTPRichText> SerializeRichTextRange(
		const QString &text,
		const std::vector<EntityInText> &entities,
		int from,
		int till,
		SerializeContext *context,
		int skipIndex) {
	auto parts = QVector<MTPRichText>();
	auto position = from;
	while (position < till) {
		auto nextEntityStart = till;
		const auto count = int(entities.size());
		for (auto index = 0; index != count; ++index) {
			const auto &entity = entities[index];
			if (SkipEntityForRange(entities, index, skipIndex)) {
				continue;
			}
			if (entity.offset() >= position
				&& entity.offset() + entity.length() <= till) {
				nextEntityStart = entity.offset();
				break;
			}
		}
		if (nextEntityStart > position) {
			parts.push_back(MakePlainRichText(
				text.mid(position, nextEntityStart - position)));
			position = nextEntityStart;
			continue;
		}
		const auto entity = FindOuterEntityAt(
			entities,
			position,
			till,
			skipIndex);
		if (entity == kNoEntityIndex) {
			parts.push_back(MakePlainRichText(text.mid(position, 1)));
			++position;
			continue;
		}
		const auto wrapped = SerializeRichTextEntity(
			text,
			entities,
			entity,
			context);
		if (!wrapped) {
			return std::nullopt;
		}
		parts.push_back(*wrapped);
		const auto &wrappedEntity = entities[entity];
		position = wrappedEntity.offset() + wrappedEntity.length();
	}
	return JoinRichTextParts(std::move(parts));
}

[[nodiscard]] std::optional<MTPRichText> SerializeRichTextWithAnchor(
		const RichText &text,
		const QString &anchorId,
		SerializeContext *context) {
	const auto entities = SortedRichTextEntities(text.text);
	auto result = SerializeRichTextRange(
		text.text.text,
		entities,
		0,
		text.text.text.size(),
		context,
		kNoEntityIndex);
	if (!result) {
		return std::nullopt;
	}
	*result = WrapRichTextAnchor(std::move(*result), text.anchorId);
	*result = WrapRichTextAnchor(std::move(*result), anchorId);
	return result;
}

[[nodiscard]] std::optional<MTPPageCaption> SerializeCaption(
		const RichText &caption,
		const QString &anchorId,
		SerializeContext *context) {
	const auto text = SerializeRichTextWithAnchor(caption, anchorId, context);
	return text
		? std::make_optional(MTP_pageCaption(*text, MTP_textEmpty()))
		: std::nullopt;
}

[[nodiscard]] std::optional<MTPPageBlock> SerializeGroupedMediaItem(
		const GroupedMediaItem &item,
		SerializeContext *context) {
	const auto caption = SerializeCaption(RichText(), QString(), context);
	if (!caption) {
		return std::nullopt;
	}
	switch (item.kind) {
	case BlockKind::Photo: {
		const auto photoId = CollectPhoto(context, item.photoId, item.photo);
		if (!photoId) {
			return std::nullopt;
		}
		using Flag = MTPDpageBlockPhoto::Flag;
		auto flags = MTPDpageBlockPhoto::Flags();
		if (item.spoiler) {
			flags |= Flag::f_spoiler;
		}
		return MTP_pageBlockPhoto(
			MTP_flags(flags),
			MTP_long(*photoId),
			*caption,
			MTPstring(),
			MTPlong());
	}
	case BlockKind::Video: {
		const auto documentId = CollectDocument(
			context,
			item.documentId,
			item.document);
		if (!documentId) {
			return std::nullopt;
		}
		using Flag = MTPDpageBlockVideo::Flag;
		auto flags = MTPDpageBlockVideo::Flags();
		if (item.autoplay) {
			flags |= Flag::f_autoplay;
		}
		if (item.loop) {
			flags |= Flag::f_loop;
		}
		if (item.spoiler) {
			flags |= Flag::f_spoiler;
		}
		return MTP_pageBlockVideo(
			MTP_flags(flags),
			MTP_long(*documentId),
			*caption);
	}
	default:
		return std::nullopt;
	}
}

[[nodiscard]] std::optional<QVector<MTPPageBlock>> SerializeGroupedMediaItems(
		const std::vector<GroupedMediaItem> &items,
		SerializeContext *context) {
	auto result = QVector<MTPPageBlock>();
	result.reserve(items.size());
	for (const auto &item : items) {
		const auto serialized = SerializeGroupedMediaItem(item, context);
		if (!serialized) {
			return std::nullopt;
		}
		result.push_back(*serialized);
	}
	return result;
}

[[nodiscard]] std::optional<QVector<MTPPageBlock>> SerializeBlocks(
		const std::vector<Block> &blocks,
		SerializeContext *context);

[[nodiscard]] std::optional<MTPPageBlock> SerializeParagraphBlock(
		const RichText &text,
		const QString &anchorId,
		SerializeContext *context) {
	const auto serialized = SerializeRichTextWithAnchor(text, anchorId, context);
	return serialized
		? std::make_optional(MTP_pageBlockParagraph(*serialized))
		: std::nullopt;
}

[[nodiscard]] bool AppendSerializedParagraphBlock(
		QVector<MTPPageBlock> *blocks,
		const RichText &text,
		const QString &anchorId,
		SerializeContext *context) {
	const auto paragraph = SerializeParagraphBlock(text, anchorId, context);
	if (!paragraph) {
		return false;
	}
	blocks->push_back(*paragraph);
	return true;
}

[[nodiscard]] std::optional<MTPPageTableCell> SerializeTableCell(
		const TableCell &cell,
		SerializeContext *context) {
	using Flag = MTPDpageTableCell::Flag;
	auto flags = MTPDpageTableCell::Flags();
	if (cell.header) {
		flags |= Flag::f_header;
	}
	switch (cell.alignment) {
	case TableAlignment::Center:
		flags |= Flag::f_align_center;
		break;
	case TableAlignment::Right:
		flags |= Flag::f_align_right;
		break;
	case TableAlignment::Left:
		break;
	}
	switch (cell.verticalAlignment) {
	case TableVerticalAlignment::Middle:
		flags |= Flag::f_valign_middle;
		break;
	case TableVerticalAlignment::Bottom:
		flags |= Flag::f_valign_bottom;
		break;
	case TableVerticalAlignment::Top:
		break;
	}
	const auto colspan = std::max(cell.colspan, 1);
	const auto rowspan = std::max(cell.rowspan, 1);
	if (colspan != 1) {
		flags |= Flag::f_colspan;
	}
	if (rowspan != 1) {
		flags |= Flag::f_rowspan;
	}
	const auto hasText = HasRichTextContent(cell.text);
	auto text = MTPRichText(MTP_textEmpty());
	if (hasText) {
		flags |= Flag::f_text;
		const auto serialized = SerializeRichTextWithAnchor(
			cell.text,
			QString(),
			context);
		if (!serialized) {
			return std::nullopt;
		}
		text = *serialized;
	}
	return MTP_pageTableCell(
		MTP_flags(flags),
		std::move(text),
		(colspan != 1 ? MTP_int(colspan) : MTPint()),
		(rowspan != 1 ? MTP_int(rowspan) : MTPint()));
}

[[nodiscard]] std::optional<MTPPageBlock> SerializeBlock(
		const Block &block,
		SerializeContext *context) {
	switch (block.kind) {
	case BlockKind::Heading: {
		const auto text = SerializeRichTextWithAnchor(
			block.text,
			block.anchorId,
			context);
		if (!text) {
			return std::nullopt;
		}
		switch (std::clamp(block.headingLevel, 1, 6)) {
		case 1: return MTP_pageBlockHeading1(*text);
		case 2: return MTP_pageBlockHeading2(*text);
		case 3: return MTP_pageBlockHeading3(*text);
		case 4: return MTP_pageBlockHeading4(*text);
		case 5: return MTP_pageBlockHeading5(*text);
		case 6: return MTP_pageBlockHeading6(*text);
		}
		return std::nullopt;
	}
	case BlockKind::Paragraph: {
		return SerializeParagraphBlock(block.text, block.anchorId, context);
	}
	case BlockKind::Footer: {
		const auto text = SerializeRichTextWithAnchor(
			block.text,
			block.anchorId,
			context);
		return text
			? std::make_optional(MTP_pageBlockFooter(*text))
			: std::nullopt;
	}
	case BlockKind::Divider:
		return MTP_pageBlockDivider();
	case BlockKind::Anchor:
		return block.anchorId.isEmpty()
			? std::nullopt
			: std::make_optional(MTP_pageBlockAnchor(
				MTP_string(block.anchorId)));
	case BlockKind::Quote: {
		const auto caption = SerializeRichTextWithAnchor(
			block.caption,
			block.blocks.empty() ? QString() : block.anchorId,
			context);
		if (!caption) {
			return std::nullopt;
		}
		if (block.pullquote) {
			if (!block.blocks.empty()) {
				return std::nullopt;
			}
			const auto text = SerializeRichTextWithAnchor(
				block.text,
				block.anchorId,
				context);
			return text
				? std::make_optional(MTP_pageBlockPullquote(
					*text,
					*caption))
				: std::nullopt;
		}
		if (block.blocks.empty()) {
			const auto text = SerializeRichTextWithAnchor(
				block.text,
				block.anchorId,
				context);
			return text
				? std::make_optional(MTP_pageBlockBlockquote(
					*text,
					*caption))
				: std::nullopt;
		}
		auto blocks = QVector<MTPPageBlock>();
		if (HasRichTextContent(block.text)
			&& !AppendSerializedParagraphBlock(
				&blocks,
				block.text,
				QString(),
				context)) {
			return std::nullopt;
		}
		const auto nested = SerializeBlocks(block.blocks, context);
		if (!nested) {
			return std::nullopt;
		}
		blocks += *nested;
		return MTP_pageBlockBlockquoteBlocks(
			MTP_vector<MTPPageBlock>(std::move(blocks)),
			*caption);
	}
	case BlockKind::List: {
		if (block.listKind == ListKind::Ordered) {
			auto items = QVector<MTPPageListOrderedItem>();
			items.reserve(block.listItems.size());
			for (auto i = 0, count = int(block.listItems.size()); i != count; ++i) {
				const auto &item = block.listItems[i];
				const auto number = !item.number.isEmpty()
					? item.number
					: QString::number(i + 1);
				if (!item.blocks.empty()) {
					using Flag = MTPDpageListOrderedItemBlocks::Flag;
					auto flags = MTPDpageListOrderedItemBlocks::Flags();
					if (item.taskState != TaskState::None) {
						flags |= Flag::f_checkbox;
					}
					if (item.taskState == TaskState::Checked) {
						flags |= Flag::f_checked;
					}
					flags |= Flag::f_num;
					auto blocks = QVector<MTPPageBlock>();
					if (HasRichTextContent(item.text) || !item.anchorId.isEmpty()) {
						if (!AppendSerializedParagraphBlock(
								&blocks,
								item.text,
								item.anchorId,
								context)) {
							return std::nullopt;
						}
					}
					const auto nested = SerializeBlocks(item.blocks, context);
					if (!nested) {
						return std::nullopt;
					}
					blocks += *nested;
					items.push_back(MTP_pageListOrderedItemBlocks(
						MTP_flags(flags),
						MTP_string(number),
						MTP_vector<MTPPageBlock>(std::move(blocks)),
						MTPint(),
						MTPstring()));
				} else {
					using Flag = MTPDpageListOrderedItemText::Flag;
					auto flags = MTPDpageListOrderedItemText::Flags();
					if (item.taskState != TaskState::None) {
						flags |= Flag::f_checkbox;
					}
					if (item.taskState == TaskState::Checked) {
						flags |= Flag::f_checked;
					}
					flags |= Flag::f_num;
					const auto text = SerializeRichTextWithAnchor(
						item.text,
						item.anchorId,
						context);
					if (!text) {
						return std::nullopt;
					}
					items.push_back(MTP_pageListOrderedItemText(
						MTP_flags(flags),
						MTP_string(number),
						*text,
						MTPint(),
						MTPstring()));
				}
			}
			return MTP_pageBlockOrderedList(
				MTP_flags(0),
				MTP_vector<MTPPageListOrderedItem>(std::move(items)),
				MTPint(),
				MTPstring());
		}
		auto items = QVector<MTPPageListItem>();
		items.reserve(block.listItems.size());
		for (const auto &item : block.listItems) {
			if (!item.blocks.empty()) {
				using Flag = MTPDpageListItemBlocks::Flag;
				auto flags = MTPDpageListItemBlocks::Flags();
				if (item.taskState != TaskState::None) {
					flags |= Flag::f_checkbox;
				}
				if (item.taskState == TaskState::Checked) {
					flags |= Flag::f_checked;
				}
				auto blocks = QVector<MTPPageBlock>();
				if (HasRichTextContent(item.text) || !item.anchorId.isEmpty()) {
					if (!AppendSerializedParagraphBlock(
							&blocks,
							item.text,
							item.anchorId,
							context)) {
						return std::nullopt;
					}
				}
				const auto nested = SerializeBlocks(item.blocks, context);
				if (!nested) {
					return std::nullopt;
				}
				blocks += *nested;
				items.push_back(MTP_pageListItemBlocks(
					MTP_flags(flags),
					MTP_vector<MTPPageBlock>(std::move(blocks))));
			} else {
				using Flag = MTPDpageListItemText::Flag;
				auto flags = MTPDpageListItemText::Flags();
				if (item.taskState != TaskState::None) {
					flags |= Flag::f_checkbox;
				}
				if (item.taskState == TaskState::Checked) {
					flags |= Flag::f_checked;
				}
				const auto text = SerializeRichTextWithAnchor(
					item.text,
					item.anchorId,
					context);
				if (!text) {
					return std::nullopt;
				}
				items.push_back(MTP_pageListItemText(MTP_flags(flags), *text));
			}
		}
		return MTP_pageBlockList(MTP_vector<MTPPageListItem>(std::move(items)));
	}
	case BlockKind::Photo: {
		const auto photoId = CollectPhoto(context, block.photoId, block.photo);
		const auto caption = SerializeCaption(block.caption, block.anchorId, context);
		if (!photoId || !caption) {
			return std::nullopt;
		}
		using Flag = MTPDpageBlockPhoto::Flag;
		auto flags = MTPDpageBlockPhoto::Flags();
		if (block.spoiler) {
			flags |= Flag::f_spoiler;
		}
		return MTP_pageBlockPhoto(
			MTP_flags(flags),
			MTP_long(*photoId),
			*caption,
			MTPstring(),
			MTPlong());
	}
	case BlockKind::Video: {
		const auto documentId = CollectDocument(
			context,
			block.documentId,
			block.document);
		const auto caption = SerializeCaption(block.caption, block.anchorId, context);
		if (!documentId || !caption) {
			return std::nullopt;
		}
		using Flag = MTPDpageBlockVideo::Flag;
		auto flags = MTPDpageBlockVideo::Flags();
		if (block.autoplay) {
			flags |= Flag::f_autoplay;
		}
		if (block.loop) {
			flags |= Flag::f_loop;
		}
		if (block.spoiler) {
			flags |= Flag::f_spoiler;
		}
		return MTP_pageBlockVideo(
			MTP_flags(flags),
			MTP_long(*documentId),
			*caption);
	}
	case BlockKind::Audio: {
		const auto documentId = CollectDocument(
			context,
			block.documentId,
			block.document);
		const auto caption = SerializeCaption(block.caption, block.anchorId, context);
		return (documentId && caption)
			? std::make_optional(MTP_pageBlockAudio(
				MTP_long(*documentId),
				*caption))
			: std::nullopt;
	}
	case BlockKind::Math:
		return MTP_pageBlockMath(MTP_string(block.formula));
	case BlockKind::Table: {
		using Flag = MTPDpageBlockTable::Flag;
		auto flags = MTPDpageBlockTable::Flags();
		if (block.bordered) {
			flags |= Flag::f_bordered;
		}
		if (block.striped) {
			flags |= Flag::f_striped;
		}
		const auto title = SerializeRichTextWithAnchor(
			block.text,
			block.anchorId,
			context);
		if (!title) {
			return std::nullopt;
		}
		auto rows = QVector<MTPPageTableRow>();
		rows.reserve(block.tableRows.size());
		for (const auto &row : block.tableRows) {
			auto cells = QVector<MTPPageTableCell>();
			cells.reserve(row.cells.size());
			for (const auto &cell : row.cells) {
				const auto serialized = SerializeTableCell(cell, context);
				if (!serialized) {
					return std::nullopt;
				}
				cells.push_back(*serialized);
			}
			rows.push_back(MTP_pageTableRow(
				MTP_vector<MTPPageTableCell>(std::move(cells))));
		}
		return MTP_pageBlockTable(
			MTP_flags(flags),
			*title,
			MTP_vector<MTPPageTableRow>(std::move(rows)));
	}
	case BlockKind::Details: {
		using Flag = MTPDpageBlockDetails::Flag;
		auto flags = block.open ? Flag::f_open : Flag();
		const auto title = SerializeRichTextWithAnchor(
			block.text,
			block.anchorId,
			context);
		const auto blocks = SerializeBlocks(block.blocks, context);
		return (title && blocks)
			? std::make_optional(MTP_pageBlockDetails(
				MTP_flags(flags),
				MTP_vector<MTPPageBlock>(*blocks),
				*title))
			: std::nullopt;
	}
	case BlockKind::Map: {
		const auto width = (block.width > 0)
			? block.width
			: kDefaultMapWidth;
		const auto height = (block.height > 0)
			? block.height
			: kDefaultMapHeight;
		if (block.zoom <= 0) {
			return std::nullopt;
		}
		const auto caption = SerializeCaption(block.caption, block.anchorId, context);
		return caption
			? std::make_optional(MTP_inputPageBlockMap(
				MTP_inputGeoPoint(
					MTP_flags(0),
					MTP_double(block.latitude),
					MTP_double(block.longitude),
					MTPint()),
				MTP_int(block.zoom),
				MTP_int(width),
				MTP_int(height),
				*caption))
			: std::nullopt;
	}
	case BlockKind::Code: {
		if (!block.blocks.empty()) {
			return std::nullopt;
		}
		const auto text = SerializeRichTextWithAnchor(
			block.text,
			block.anchorId,
			context);
		return text
			? std::make_optional(MTP_pageBlockPreformatted(
				*text,
				MTP_string(block.language)))
			: std::nullopt;
	}
	case BlockKind::GroupedMedia: {
		const auto items = SerializeGroupedMediaItems(
			block.mediaItems,
			context);
		const auto caption = SerializeCaption(
			block.caption,
			block.anchorId,
			context);
		if (!items || items->isEmpty() || !caption) {
			return std::nullopt;
		}
		if (block.mediaIntent == RichPage::GroupedMediaIntent::Slideshow) {
			return MTP_pageBlockSlideshow(
				MTP_vector<MTPPageBlock>(std::move(*items)),
				*caption);
		}
		return MTP_pageBlockCollage(
			MTP_vector<MTPPageBlock>(std::move(*items)),
			*caption);
	}
	case BlockKind::Unsupported:
	case BlockKind::Thinking:
	case BlockKind::AuthorDate:
	case BlockKind::Embed:
	case BlockKind::EmbedPost:
	case BlockKind::Channel:
	case BlockKind::RelatedArticles:
		break;
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<QVector<MTPPageBlock>> SerializeBlocks(
		const std::vector<Block> &blocks,
		SerializeContext *context) {
	auto result = QVector<MTPPageBlock>();
	result.reserve(blocks.size());
	for (const auto &block : blocks) {
		const auto serialized = SerializeBlock(block, context);
		if (!serialized) {
			return std::nullopt;
		}
		result.push_back(*serialized);
	}
	return result;
}

} // namespace

std::optional<MTPInputRichMessage> SerializeInputRichMessage(
		not_null<Main::Session*> session,
		const RichPage &page) {
	auto context = SerializeContext{ session };
	auto blocks = SerializeBlocks(page.blocks, &context);
	if (!blocks) {
		return std::nullopt;
	}
	auto photos = QVector<MTPInputPhoto>();
	photos.reserve(context.photos.size());
	for (const auto &[id, input] : context.photos) {
		photos.push_back(input);
	}
	auto documents = QVector<MTPInputDocument>();
	documents.reserve(context.documents.size());
	for (const auto &[id, input] : context.documents) {
		documents.push_back(input);
	}
	auto users = QVector<MTPInputUser>();
	users.reserve(context.users.size());
	for (const auto &[id, input] : context.users) {
		users.push_back(input);
	}
	using Flag = MTPDinputRichMessage::Flag;
	auto flags = MTPDinputRichMessage::Flags();
	if (page.rtl) {
		flags |= Flag::f_rtl;
	}
	if (!photos.isEmpty()) {
		flags |= Flag::f_photos;
	}
	if (!documents.isEmpty()) {
		flags |= Flag::f_documents;
	}
	if (!users.isEmpty()) {
		flags |= Flag::f_users;
	}
	return MTP_inputRichMessage(
		MTP_flags(flags),
		MTP_vector<MTPPageBlock>(std::move(*blocks)),
		MTP_vector<MTPInputPhoto>(std::move(photos)),
		MTP_vector<MTPInputDocument>(std::move(documents)),
		MTP_vector<MTPInputUser>(std::move(users)));
}

} // namespace Iv
