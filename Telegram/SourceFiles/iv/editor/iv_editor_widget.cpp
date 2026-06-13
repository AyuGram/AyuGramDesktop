/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/editor/iv_editor_widget.h"

#include "base/qthelp_url.h"
#include "base/qt/qt_common_adapters.h"
#include "chat_helpers/emoji_suggestions_widget.h"
#include "chat_helpers/message_field.h"
#include "data/data_msg_id.h"
#include "data/data_types.h"
#include "data/stickers/data_custom_emoji.h"
#include "iv/editor/iv_editor_text_entities.h"
#include "iv/markdown/iv_markdown_article_paint.h"
#include "iv/markdown/iv_markdown_prepare_links.h"
#include "iv/markdown/iv_markdown_prepare_native_richtext.h"
#include "lang/lang_keys.h"
#include "main/session/session_show.h"
#include "menu/menu_checked_action.h"
#include "spellcheck/spellcheck_highlight_syntax.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/image/image_location.h"
#include "ui/painter.h"
#include "ui/text/text_entity.h"
#include "ui/ui_utility.h"
#include "ui/widgets/elastic_scroll.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/scroll_area.h"

#include "styles/palette.h"
#include "styles/style_chat.h"
#include "styles/style_iv.h"
#include "styles/style_menu_icons.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDate>
#include <QtCore/QEvent>
#include <QtCore/QMimeData>
#include <QtCore/QPointer>
#include <QtGui/QClipboard>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QInputMethodEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QTextBlock>
#include <QtGui/QTouchEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QTextCursor>
#include <QtGui/QTextDocument>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTextEdit>
#include <QAction>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Iv::Editor {
namespace {

[[nodiscard]] std::vector<Ui::Text::SpecialColor> HighlightColors(
		not_null<const Ui::ChatStyle*> style) {
	auto result = Ui::SyntaxHighlightColors(style);

	const auto &fg = style->lightButtonFg();
	const auto &bg = style->lightButtonBgOver();
	result.push_back({ &fg->p, &fg->p, &bg->b, &bg->b });

	Ensures(result.size() == Markdown::kNativeIvLinkSpecialColorIndex);
	return result;
}

[[nodiscard]] std::unique_ptr<Ui::ChatTheme> CreateStandaloneChatTheme() {
	const auto palette = style::main_palette::get();
	return std::make_unique<Ui::ChatTheme>(Ui::ChatThemeDescriptor{
		.preparePalette = [=](style::palette &copy) {
			copy = *palette;
		},
		.backgroundData = {
			.colors = { palette->windowBg()->c },
		},
	});
}

[[nodiscard]] const style::margins &EditorBodyPadding() {
	return st::ivEditorBodyPadding;
}

constexpr auto kRetainedLeafFieldLimit = 50;
thread_local Widget *PreservingExternalFieldRestore = nullptr;

void EnableQTextEditLineMetrics(style::TextStyle &style) {
	style.qtextEditLineMetrics = true;
}

void EnableQTextEditLineMetrics(style::Markdown &style) {
	EnableQTextEditLineMetrics(style.body);
	EnableQTextEditLineMetrics(style.heading1);
	EnableQTextEditLineMetrics(style.heading2);
	EnableQTextEditLineMetrics(style.heading3);
	EnableQTextEditLineMetrics(style.heading4);
	EnableQTextEditLineMetrics(style.heading5);
	EnableQTextEditLineMetrics(style.heading6);
	EnableQTextEditLineMetrics(style.code);
	EnableQTextEditLineMetrics(style.displayMath.fallbackStyle);
	EnableQTextEditLineMetrics(style.table.headerStyle);
	EnableQTextEditLineMetrics(style.table.bodyStyle);
	EnableQTextEditLineMetrics(style.details.summaryStyle);
	EnableQTextEditLineMetrics(style.embedPost.authorStyle);
	EnableQTextEditLineMetrics(style.embedPost.dateStyle);
	EnableQTextEditLineMetrics(style.placeholder.labelStyle);
	EnableQTextEditLineMetrics(style.audio.titleStyle);
	EnableQTextEditLineMetrics(style.audio.subtitleStyle);
	EnableQTextEditLineMetrics(style.channel.titleStyle);
	EnableQTextEditLineMetrics(style.channel.subtitleStyle);
	EnableQTextEditLineMetrics(style.channel.button.textStyle);
	EnableQTextEditLineMetrics(style.relatedArticle.titleStyle);
	EnableQTextEditLineMetrics(style.relatedArticle.subtitleStyle);
	EnableQTextEditLineMetrics(style.relatedArticle.footerStyle);
}

[[nodiscard]] style::Markdown CreateEditorMarkdownStyle() {
	auto result = st::messageMarkdown;
	EnableQTextEditLineMetrics(result);
	return result;
}

[[nodiscard]] int CompareSelectionPositions(
		Markdown::MarkdownArticleSelectionPosition a,
		Markdown::MarkdownArticleSelectionPosition b) {
	if (a.segment != b.segment) {
		return (a.segment < b.segment) ? -1 : 1;
	}
	if (a.offset != b.offset) {
		return (a.offset < b.offset) ? -1 : 1;
	}
	return 0;
}

[[nodiscard]] Markdown::MarkdownArticleSelection NormalizeSelection(
		Markdown::MarkdownArticleSelection selection) {
	if (selection.empty()) {
		return {};
	}
	if (CompareSelectionPositions(selection.from, selection.to) > 0) {
		std::swap(selection.from, selection.to);
	}
	return selection;
}

[[nodiscard]] Markdown::MarkdownArticleSelectionEndpoint MakeSelectionEndpoint(
		const Markdown::MarkdownArticleHitTestResult &hit) {
	return {
		.segment = hit.segmentIndex,
		.direct = hit.direct,
	};
}

[[nodiscard]] bool RedirectTextToField(const QString &text) {
	for (const auto &ch : text) {
		if (ch.unicode() >= 32) {
			return true;
		}
	}
	return false;
}

struct InlineFieldTrimResult {
	TextWithTags text;
	int left = 0;
};

[[nodiscard]] InlineFieldTrimResult TrimInlineFieldText(
		TextWithTags text,
		bool trimLeft) {
	auto from = 0;
	auto till = int(text.text.size());
	if (trimLeft) {
		while (from < till && text.text[from].isSpace()) {
			++from;
		}
	}
	while (till > from && text.text[till - 1].isSpace()) {
		--till;
	}
	if (from == 0 && till == text.text.size()) {
		return { std::move(text), 0 };
	}
	text.text = text.text.mid(from, till - from);
	for (auto i = text.tags.begin(); i != text.tags.end();) {
		const auto tagFrom = i->offset;
		const auto tagTill = i->offset + i->length;
		const auto clippedFrom = std::max(tagFrom, from);
		const auto clippedTill = std::min(tagTill, till);
		if (clippedTill <= clippedFrom || i->length <= 0) {
			i = text.tags.erase(i);
		} else {
			i->offset = clippedFrom - from;
			i->length = clippedTill - clippedFrom;
			++i;
		}
	}
	return { std::move(text), from };
}

[[nodiscard]] int MapEditorOffsetToRichOffset(
		const std::vector<RichTextEditorOffsetReplacement> &replacements,
		int offset) {
	auto delta = 0;
	for (const auto &replacement : replacements) {
		if (replacement.richLength <= 0) {
			continue;
		}
		const auto richStart = replacement.richOffset;
		const auto editorStart = richStart + delta;
		const auto editorEnd = editorStart + replacement.editorLength;
		if (offset < editorStart) {
			break;
		} else if (offset <= editorEnd) {
			return richStart
				+ ((offset == editorEnd) ? replacement.richLength : 0);
		}
		delta += replacement.editorLength - replacement.richLength;
	}
	return offset - delta;
}

[[nodiscard]] auto ClipboardPasteInsertContext(
		std::optional<State::ActiveTextInsertContext> context)
-> std::optional<State::ActiveTextInsertContext> {
	if (context) {
		context->selected = TextWithEntities();
	}
	return context;
}

[[nodiscard]] QString ValidateInstantViewEditorLink(QString link) {
	const auto normal = qthelp::validate_url(link);
	if (!normal.isEmpty()) {
		return normal;
	}
	link = link.trimmed();
	const auto hasPayload = [&](const QString &prefix) {
		return link.startsWith(prefix)
			&& !link.mid(prefix.size()).trimmed().isEmpty();
	};
	if (hasPayload(u"mailto:"_q)
		|| hasPayload(u"tel:"_q)
		|| (link.startsWith(u"#"_q)
			&& !Markdown::NormalizeFragmentId(link).isEmpty())) {
		return link;
	}
	return QString();
}

[[nodiscard]] bool ImeEventProducesInput(
		const QInputMethodEvent &e,
		const QTextCursor &cursor) {
	return !e.commitString().isEmpty()
		|| e.preeditString() != cursor.block().layout()->preeditAreaText()
		|| e.replacementLength() > 0;
}

using PreparedEditBlockContainerPath
	= Markdown::PreparedEditBlockContainerPath;
using PreparedEditBlockContainerStep
	= Markdown::PreparedEditBlockContainerStep;
using PreparedEditBlockContainerKind
	= Markdown::PreparedEditBlockContainerKind;
using PreparedEditBlockPath = Markdown::PreparedEditBlockPath;
using PreparedEditBlockSource = Markdown::PreparedEditBlockSource;
using PreparedEditHit = Markdown::PreparedEditHit;
using PreparedEditHitKind = Markdown::PreparedEditHitKind;
using PreparedEditLeafKind = Markdown::PreparedEditLeafKind;
using PreparedEditLeafSource = Markdown::PreparedEditLeafSource;
using PreparedEditListItemSource = Markdown::PreparedEditListItemSource;
using PreparedEditSelection = Markdown::PreparedEditSelection;
using PreparedEditSelectionKind = Markdown::PreparedEditSelectionKind;
using PreparedEditTableCellRange = Markdown::PreparedEditTableCellRange;
using PreparedEditTableCellSource = Markdown::PreparedEditTableCellSource;
using PreparedEditTableRowSource = Markdown::PreparedEditTableRowSource;
using ApplyResult = State::ApplyResult;
using PreparedMutationKind = State::PreparedMutationKind;
using GroupedMediaItem = RichPage::GroupedMediaItem;
using ListItem = RichPage::ListItem;
using RelatedArticle = RichPage::RelatedArticle;
using RichText = RichPage::RichText;
using TableCell = RichPage::TableCell;
using TableRow = RichPage::TableRow;
using Block = RichPage::Block;

template <typename Range, typename Equals>
[[nodiscard]] bool RangesEqual(
		const Range &a,
		const Range &b,
		Equals equals) {
	return (a.size() == b.size())
		&& std::equal(a.begin(), a.end(), b.begin(), equals);
}

[[nodiscard]] bool RichTextEquals(const RichText &a, const RichText &b) {
	return (a.text == b.text)
		&& (a.anchorId == b.anchorId)
		&& (a.anchorIds == b.anchorIds);
}

[[nodiscard]] bool GroupedMediaItemEquals(
		const GroupedMediaItem &a,
		const GroupedMediaItem &b) {
	return (a.kind == b.kind)
		&& (a.photo == b.photo)
		&& (a.document == b.document)
		&& (a.photoId == b.photoId)
		&& (a.documentId == b.documentId)
		&& (a.width == b.width)
		&& (a.height == b.height)
		&& (a.autoplay == b.autoplay)
		&& (a.loop == b.loop)
		&& (a.spoiler == b.spoiler);
}

[[nodiscard]] bool TableCellEquals(const TableCell &a, const TableCell &b) {
	return RichTextEquals(a.text, b.text)
		&& (a.colspan == b.colspan)
		&& (a.rowspan == b.rowspan)
		&& (a.header == b.header)
		&& (a.alignment == b.alignment)
		&& (a.verticalAlignment == b.verticalAlignment);
}

[[nodiscard]] bool TableRowEquals(const TableRow &a, const TableRow &b) {
	return RangesEqual(a.cells, b.cells, TableCellEquals);
}

[[nodiscard]] bool RelatedArticleEquals(
		const RelatedArticle &a,
		const RelatedArticle &b) {
	return (a.url == b.url)
		&& (a.webpageId == b.webpageId)
		&& (a.photo == b.photo)
		&& (a.photoId == b.photoId)
		&& (a.title == b.title)
		&& (a.description == b.description)
		&& (a.author == b.author)
		&& (a.publishedDate == b.publishedDate);
}

[[nodiscard]] bool BlockEquals(const Block &a, const Block &b);

[[nodiscard]] bool ListItemEquals(const ListItem &a, const ListItem &b) {
	return (a.taskState == b.taskState)
		&& (a.number == b.number)
		&& (a.anchorId == b.anchorId)
		&& RichTextEquals(a.text, b.text)
		&& RangesEqual(a.blocks, b.blocks, BlockEquals);
}

[[nodiscard]] bool BlockEquals(const Block &a, const Block &b) {
	return (a.kind == b.kind)
		&& (a.anchorId == b.anchorId)
		&& RichTextEquals(a.text, b.text)
		&& RichTextEquals(a.caption, b.caption)
		&& (a.language == b.language)
		&& (a.formula == b.formula)
		&& (a.url == b.url)
		&& (a.html == b.html)
		&& (a.author == b.author)
		&& (a.username == b.username)
		&& (a.channelTitle == b.channelTitle)
		&& (a.audioTitle == b.audioTitle)
		&& (a.audioPerformer == b.audioPerformer)
		&& (a.audioFileName == b.audioFileName)
		&& (a.date == b.date)
		&& (a.audioDuration == b.audioDuration)
		&& (a.headingLevel == b.headingLevel)
		&& (a.width == b.width)
		&& (a.height == b.height)
		&& (a.zoom == b.zoom)
		&& (a.photoId == b.photoId)
		&& (a.documentId == b.documentId)
		&& (a.channelId == b.channelId)
		&& (a.fullWidth == b.fullWidth)
		&& (a.fixedHeight == b.fixedHeight)
		&& (a.allowScrolling == b.allowScrolling)
		&& (a.autoplay == b.autoplay)
		&& (a.loop == b.loop)
		&& (a.spoiler == b.spoiler)
		&& (a.open == b.open)
		&& (a.bordered == b.bordered)
		&& (a.striped == b.striped)
		&& (a.pullquote == b.pullquote)
		&& (a.listKind == b.listKind)
		&& (a.mediaIntent == b.mediaIntent)
		&& (a.photo == b.photo)
		&& (a.document == b.document)
		&& (a.peer == b.peer)
		&& (a.latitude == b.latitude)
		&& (a.longitude == b.longitude)
		&& (a.accessHash == b.accessHash)
		&& RangesEqual(a.blocks, b.blocks, BlockEquals)
		&& RangesEqual(a.listItems, b.listItems, ListItemEquals)
		&& RangesEqual(a.mediaItems, b.mediaItems, GroupedMediaItemEquals)
		&& RangesEqual(a.tableRows, b.tableRows, TableRowEquals)
		&& RangesEqual(
			a.relatedArticles,
			b.relatedArticles,
			RelatedArticleEquals);
}

[[nodiscard]] bool RichPageEquals(const RichPage &a, const RichPage &b) {
	return (a.url == b.url)
		&& (a.rtl == b.rtl)
		&& (a.part == b.part)
		&& (a.views == b.views)
		&& RangesEqual(a.blocks, b.blocks, BlockEquals);
}

[[nodiscard]] bool SnapshotEquals(
		const State::Snapshot &a,
		const State::Snapshot &b) {
	return RichPageEquals(a.richPage, b.richPage)
		&& (a.activeLeaf == b.activeLeaf)
		&& (a.temporaryDownParagraph == b.temporaryDownParagraph);
}

struct NormalizedIntegerRange {
	int from = -1;
	int till = -1;

	[[nodiscard]] bool empty() const {
		return (from < 0) || (till <= from);
	}
};

[[nodiscard]] NormalizedIntegerRange NormalizeIntegerRange(int a, int b) {
	if (a < 0 || b < 0) {
		return {};
	}
	return {
		.from = std::min(a, b),
		.till = std::max(a, b) + 1,
	};
}

[[nodiscard]] PreparedEditSelection BlockSelectionFromIndexes(
		PreparedEditBlockContainerPath container,
		int first,
		int second) {
	const auto range = NormalizeIntegerRange(first, second);
	if (range.empty()) {
		return {};
	}
	return {
		.kind = PreparedEditSelectionKind::Blocks,
		.blocks = {
			.container = std::move(container),
			.from = range.from,
			.till = range.till,
		},
	};
}

[[nodiscard]] int CompareIntegers(int a, int b) {
	return (a < b) ? -1 : (a > b) ? 1 : 0;
}

[[nodiscard]] int ComparePreparedEditBlockContainerSteps(
		const PreparedEditBlockContainerStep &a,
		const PreparedEditBlockContainerStep &b) {
	if (const auto result = CompareIntegers(
			static_cast<int>(a.kind),
			static_cast<int>(b.kind))) {
		return result;
	} else if (const auto result = CompareIntegers(
			a.blockIndex,
			b.blockIndex)) {
		return result;
	}
	return CompareIntegers(a.listItemIndex, b.listItemIndex);
}

[[nodiscard]] int ComparePreparedEditBlockContainerPaths(
		const PreparedEditBlockContainerPath &a,
		const PreparedEditBlockContainerPath &b) {
	const auto common = std::min(a.steps.size(), b.steps.size());
	for (auto i = size_t(); i != common; ++i) {
		if (const auto result = ComparePreparedEditBlockContainerSteps(
				a.steps[i],
				b.steps[i])) {
			return result;
		}
	}
	return CompareIntegers(int(a.steps.size()), int(b.steps.size()));
}

[[nodiscard]] int ComparePreparedEditBlockPaths(
		const PreparedEditBlockPath &a,
		const PreparedEditBlockPath &b) {
	if (const auto result = ComparePreparedEditBlockContainerPaths(
			a.container,
			b.container)) {
		return result;
	}
	return CompareIntegers(a.index, b.index);
}

[[nodiscard]] bool SamePreparedEditBlockPath(
		const PreparedEditBlockPath &a,
		const PreparedEditBlockPath &b) {
	return (ComparePreparedEditBlockPaths(a, b) == 0);
}

[[nodiscard]] bool ValidPreparedEditBlockPath(
		const PreparedEditBlockPath &path) {
	return (path.index >= 0);
}

[[nodiscard]] PreparedEditBlockSource PreparedEditBlockSourceFromPath(
		PreparedEditBlockPath path) {
	return { .path = std::move(path) };
}

enum class StructuralOwnerKind {
	None,
	Block,
	ListItem,
	TableRow,
	TableCell,
};

struct StructuralOwner {
	StructuralOwnerKind kind = StructuralOwnerKind::None;
	std::optional<PreparedEditBlockSource> block;
	std::optional<PreparedEditListItemSource> listItem;
	std::optional<PreparedEditTableRowSource> tableRow;
	std::optional<PreparedEditTableCellSource> tableCell;

	[[nodiscard]] bool valid() const {
		return (kind != StructuralOwnerKind::None);
	}
};

[[nodiscard]] StructuralOwner StructuralOwnerFromBlock(
		const PreparedEditBlockSource &source) {
	if (!ValidPreparedEditBlockPath(source.path)) {
		return {};
	}
	return {
		.kind = StructuralOwnerKind::Block,
		.block = source,
	};
}

[[nodiscard]] StructuralOwner StructuralOwnerFromListItem(
		const PreparedEditListItemSource &source) {
	if (!ValidPreparedEditBlockPath(source.block)
		|| source.listItemIndex < 0) {
		return {};
	}
	return {
		.kind = StructuralOwnerKind::ListItem,
		.block = PreparedEditBlockSourceFromPath(source.block),
		.listItem = source,
	};
}

[[nodiscard]] StructuralOwner StructuralOwnerFromTableRow(
		const PreparedEditTableRowSource &source) {
	if (!ValidPreparedEditBlockPath(source.block)
		|| source.tableRowIndex < 0) {
		return {};
	}
	return {
		.kind = StructuralOwnerKind::TableRow,
		.block = PreparedEditBlockSourceFromPath(source.block),
		.tableRow = source,
	};
}

[[nodiscard]] PreparedEditTableRowSource PreparedEditTableRowFromCell(
		const PreparedEditTableCellSource &source) {
	return {
		.block = source.block,
		.tableRowIndex = source.tableRowIndex,
	};
}

[[nodiscard]] StructuralOwner StructuralOwnerFromTableCell(
		const PreparedEditTableCellSource &source) {
	if (!ValidPreparedEditBlockPath(source.block)
		|| source.tableRowIndex < 0
		|| source.tableCellIndex < 0
		|| source.column < 0
		|| source.colspan <= 0
		|| source.rowspan <= 0) {
		return {};
	}
	return {
		.kind = StructuralOwnerKind::TableCell,
		.block = PreparedEditBlockSourceFromPath(source.block),
		.tableRow = PreparedEditTableRowFromCell(source),
		.tableCell = source,
	};
}

[[nodiscard]] StructuralOwner StructuralOwnerFromLeaf(
		const PreparedEditLeafSource &source) {
	if (!ValidPreparedEditBlockPath(source.block)) {
		return {};
	}
	switch (source.kind) {
	case PreparedEditLeafKind::ListItemText:
		return StructuralOwnerFromListItem({
			.block = source.block,
			.listItemIndex = source.listItemIndex,
		});
	case PreparedEditLeafKind::TableCellText:
		return {};
	case PreparedEditLeafKind::BlockText:
	case PreparedEditLeafKind::BlockCaption:
	case PreparedEditLeafKind::MathFormula:
		return StructuralOwnerFromBlock(
			PreparedEditBlockSourceFromPath(source.block));
	}
	return {};
}

[[nodiscard]] StructuralOwner StructuralOwnerFromHit(
		const PreparedEditHit &hit) {
	if (!hit.valid()) {
		return {};
	}
	switch (hit.kind) {
	case PreparedEditHitKind::Block:
		if (hit.block) {
			return StructuralOwnerFromBlock(*hit.block);
		}
		break;
	case PreparedEditHitKind::ListItem:
		if (hit.listItem) {
			return StructuralOwnerFromListItem(*hit.listItem);
		}
		break;
	case PreparedEditHitKind::TableRow:
		if (hit.tableRow) {
			return StructuralOwnerFromTableRow(*hit.tableRow);
		}
		break;
	case PreparedEditHitKind::TableCell:
		if (hit.tableCell) {
			return StructuralOwnerFromTableCell(*hit.tableCell);
		}
		break;
	case PreparedEditHitKind::Leaf:
		if (hit.leaf) {
			return StructuralOwnerFromLeaf(*hit.leaf);
		}
		break;
	case PreparedEditHitKind::None:
		break;
	}
	return hit.leaf ? StructuralOwnerFromLeaf(*hit.leaf) : StructuralOwner();
}

[[nodiscard]] std::optional<PreparedEditTableCellSource> TableCellFromOwner(
		const StructuralOwner &owner) {
	return owner.tableCell;
}

[[nodiscard]] PreparedEditTableCellRange TableRangeFromCell(
		const PreparedEditTableCellSource &source) {
	if (source.tableRowIndex < 0
		|| source.column < 0
		|| source.rowspan <= 0
		|| source.colspan <= 0) {
		return {};
	}
	return {
		.block = source.block,
		.rowFrom = source.tableRowIndex,
		.rowTill = source.tableRowIndex + source.rowspan,
		.columnFrom = source.column,
		.columnTill = source.column + source.colspan,
	};
}

[[nodiscard]] bool SameTableRangeBlock(
		const PreparedEditTableCellRange &a,
		const PreparedEditTableCellRange &b) {
	return !a.empty()
		&& !b.empty()
		&& SamePreparedEditBlockPath(a.block, b.block);
}

[[nodiscard]] bool TableRangeContainsCell(
		const PreparedEditTableCellRange &range,
		const PreparedEditTableCellSource &source) {
	const auto cell = TableRangeFromCell(source);
	return SameTableRangeBlock(range, cell)
		&& (range.rowFrom <= cell.rowFrom)
		&& (range.rowTill >= cell.rowTill)
		&& (range.columnFrom <= cell.columnFrom)
		&& (range.columnTill >= cell.columnTill);
}

[[nodiscard]] PreparedEditTableCellRange TableRangesUnion(
		const PreparedEditTableCellRange &a,
		const PreparedEditTableCellRange &b) {
	if (!SameTableRangeBlock(a, b)) {
		return {};
	}
	return {
		.block = a.block,
		.rowFrom = std::min(a.rowFrom, b.rowFrom),
		.rowTill = std::max(a.rowTill, b.rowTill),
		.columnFrom = std::min(a.columnFrom, b.columnFrom),
		.columnTill = std::max(a.columnTill, b.columnTill),
	};
}

[[nodiscard]] std::optional<PreparedEditTableRowSource> TableRowFromOwner(
		const StructuralOwner &owner) {
	return owner.tableRow;
}

[[nodiscard]] std::optional<PreparedEditListItemSource> ListItemFromOwner(
		const StructuralOwner &owner) {
	return owner.listItem;
}

[[nodiscard]] bool IsBlockOwner(const StructuralOwner &owner) {
	return (owner.kind == StructuralOwnerKind::Block);
}

[[nodiscard]] std::optional<PreparedEditBlockPath> BlockPathFromOwner(
		const StructuralOwner &owner) {
	if (owner.kind == StructuralOwnerKind::Block && owner.block) {
		return owner.block->path;
	} else if (owner.kind == StructuralOwnerKind::ListItem
		&& owner.listItem) {
		return owner.listItem->block;
	} else if (owner.kind == StructuralOwnerKind::TableRow
		&& owner.tableRow) {
		return owner.tableRow->block;
	} else if (owner.kind == StructuralOwnerKind::TableCell
		&& owner.tableCell) {
		return owner.tableCell->block;
	}
	return std::nullopt;
}

struct LiftedPreparedEditBlocks {
	PreparedEditBlockContainerPath container;
	int first = -1;
	int second = -1;
};

[[nodiscard]] PreparedEditBlockContainerPath PreparedEditBlockContainerPrefix(
		const PreparedEditBlockContainerPath &path,
		int count) {
	auto result = PreparedEditBlockContainerPath();
	const auto till = std::clamp(count, 0, int(path.steps.size()));
	result.steps.insert(
		result.steps.end(),
		path.steps.begin(),
		path.steps.begin() + till);
	return result;
}

[[nodiscard]] int CommonPreparedEditBlockContainerSize(
		const PreparedEditBlockContainerPath &a,
		const PreparedEditBlockContainerPath &b) {
	const auto common = std::min(a.steps.size(), b.steps.size());
	for (auto i = size_t(); i != common; ++i) {
		if (ComparePreparedEditBlockContainerSteps(
				a.steps[i],
				b.steps[i]) != 0) {
			return int(i);
		}
	}
	return int(common);
}

[[nodiscard]] int LiftedPreparedEditBlockIndex(
		const PreparedEditBlockPath &path,
		int commonContainerSize) {
	if (commonContainerSize == int(path.container.steps.size())) {
		return path.index;
	} else if (commonContainerSize >= 0
		&& commonContainerSize < int(path.container.steps.size())) {
		return path.container.steps[commonContainerSize].blockIndex;
	}
	return -1;
}

[[nodiscard]] std::optional<LiftedPreparedEditBlocks>
LiftPreparedEditBlocksToCommonContainer(
		const PreparedEditBlockPath &a,
		const PreparedEditBlockPath &b) {
	if (!ValidPreparedEditBlockPath(a) || !ValidPreparedEditBlockPath(b)) {
		return std::nullopt;
	}
	const auto common = CommonPreparedEditBlockContainerSize(
		a.container,
		b.container);
	auto result = LiftedPreparedEditBlocks{
		.container = PreparedEditBlockContainerPrefix(a.container, common),
		.first = LiftedPreparedEditBlockIndex(a, common),
		.second = LiftedPreparedEditBlockIndex(b, common),
	};
	if (result.first < 0 || result.second < 0) {
		return std::nullopt;
	}
	return result;
}

[[nodiscard]] PreparedEditSelection LiftedBlockSelection(
		const PreparedEditBlockPath &anchor,
		const PreparedEditBlockPath &focus) {
	const auto lifted = LiftPreparedEditBlocksToCommonContainer(anchor, focus);
	if (!lifted) {
		return {};
	}
	return BlockSelectionFromIndexes(
		lifted->container,
		lifted->first,
		lifted->second);
}

[[nodiscard]] auto ListItemSourcesFromBlockPath(
		const PreparedEditBlockPath &path)
-> std::vector<PreparedEditListItemSource> {
	auto result = std::vector<PreparedEditListItemSource>();
	for (auto i = int(path.container.steps.size()); i != 0; --i) {
		const auto stepIndex = i - 1;
		const auto &step = path.container.steps[stepIndex];
		if (step.kind != PreparedEditBlockContainerKind::ListItemChildren
			|| step.blockIndex < 0
			|| step.listItemIndex < 0) {
			continue;
		}
		result.push_back({
			.block = {
				.container = PreparedEditBlockContainerPrefix(
					path.container,
					stepIndex),
				.index = step.blockIndex,
			},
			.listItemIndex = step.listItemIndex,
		});
	}
	return result;
}

[[nodiscard]] auto ListItemSourcesFromOwner(
		const StructuralOwner &owner,
		const std::optional<PreparedEditBlockPath> &block)
-> std::vector<PreparedEditListItemSource> {
	auto result = std::vector<PreparedEditListItemSource>();
	if (const auto listItem = ListItemFromOwner(owner)) {
		result.push_back(*listItem);
	}
	if (!block) {
		return result;
	}
	for (const auto &source : ListItemSourcesFromBlockPath(*block)) {
		if (std::find(result.begin(), result.end(), source) == result.end()) {
			result.push_back(source);
		}
	}
	return result;
}

[[nodiscard]] PreparedEditSelection ListItemSelectionFromSources(
		const std::vector<PreparedEditListItemSource> &anchorSources,
		const std::vector<PreparedEditListItemSource> &focusSources) {
	for (const auto &anchorListItem : anchorSources) {
		for (const auto &focusListItem : focusSources) {
			if (!SamePreparedEditBlockPath(
					anchorListItem.block,
					focusListItem.block)) {
				continue;
			}
			const auto range = NormalizeIntegerRange(
				anchorListItem.listItemIndex,
				focusListItem.listItemIndex);
			if (!range.empty()) {
				return {
					.kind = PreparedEditSelectionKind::ListItems,
					.listItems = {
						.block = anchorListItem.block,
						.from = range.from,
						.till = range.till,
					},
				};
			}
		}
	}
	return {};
}

[[nodiscard]] bool IsMultiListItemSelection(
		const PreparedEditSelection &selection) {
	return !selection.empty()
		&& (selection.kind == PreparedEditSelectionKind::ListItems)
		&& (selection.listItems.till > selection.listItems.from + 1);
}

[[nodiscard]] int FieldNaturalHeight(not_null<Ui::InputField*> field) {
	const auto margins = field->fullTextMargins();
	return std::max(
		int(std::ceil(field->document()->size().height()))
			+ margins.top()
			+ margins.bottom(),
		1);
}

[[nodiscard]] QPoint LocalPosition(QWheelEvent *e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return e->position().toPoint();
#else // Qt >= 6.0
	return e->pos();
#endif // Qt >= 6.0
}

[[nodiscard]] QPoint GlobalPosition(QWheelEvent *e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return e->globalPosition().toPoint();
#else // Qt >= 6.0
	return e->globalPos();
#endif // Qt >= 6.0
}

} // namespace

Widget::Widget(
	QWidget *parent,
	WidgetServices services,
	not_null<PeerData*> peer,
	std::shared_ptr<State> state,
	Fn<void(RichMessageLimitError)> showLimitToast)
: Ui::RpWidget(parent)
, _session(services.session)
, _show(std::move(services.show))
, _outer(services.outer)
, _customEmojiPaused(std::move(services.customEmojiPaused))
, _peer(peer)
, _state(std::move(state))
, _showLimitToast(std::move(showLimitToast))
, _articleStyle(std::make_shared<style::Markdown>(
	CreateEditorMarkdownStyle()))
, _article(std::make_shared<Markdown::MarkdownArticle>(*_articleStyle))
, _theme(CreateStandaloneChatTheme())
, _style(std::make_unique<Ui::ChatStyle>(style::main_palette::get())) {
	_style->apply(_theme.get());
	_highlightColors = HighlightColors(_style.get());

	setMouseTracking(true);
	setAttribute(Qt::WA_AcceptTouchEvents);
	setFocusPolicy(Qt::StrongFocus);
	setAttribute(Qt::WA_InputMethodEnabled);

	std::move(services.imeCompositionStarts) | rpl::filter([=] {
		return redirectImeToField();
	}) | rpl::on_next([=] {
		if (prepareFieldForInput()) {
			_field->setFocusFast();
		}
	}, lifetime());

	Spellchecker::HighlightReady(
	) | rpl::on_next([=](Spellchecker::HighlightProcessId processId) {
		if (_article && _article->highlightProcessDone(processId)) {
			update();
		}
	}, _highlightReadyLifetime);

	const auto weak = QPointer<Widget>(this);
	_article->setTextRepaintCallbacks(
		[=] {
			if (weak) {
				weak->update();
			}
		},
		[=](QRect rect) {
			if (!weak) {
				return;
			} else if (rect.isEmpty()) {
				weak->update();
			} else {
				weak->update(rect.translated(weak->articleTopLeft()));
			}
		});

	const auto &fieldStyle = inlineFieldStyleFor(
		Markdown::MarkdownArticleTextLeafStyle());
	_activeFieldStyleKey = fieldStyle.key;
	_field = base::make_unique_q<Ui::InputField>(
		this,
		*fieldStyle.style,
		Ui::InputField::Mode::MultiLine,
		rpl::single(QString()));
	setupInlineField();
	refreshPreparedContent();
	_history.push_back(captureHistoryEntry());
	_historyIndex = 0;
}

void Widget::activateInitialNode() {
	const auto ordinal = (_activeOrdinal >= 0)
		? _activeOrdinal
		: _state->activeTextOrdinal();
	if (ordinal < 0) {
		const auto first = _article->firstEditableSegmentIndex();
		const auto fallback = editableOrdinalForSegment(first);
		if (fallback < 0) {
			return;
		}
		activateTextOrdinal(fallback, 0);
		return;
	}
	activateTextOrdinal(ordinal, 0);
}

void Widget::activateSegment(int segmentIndex, int cursorOffset) {
	const auto ordinal = editableOrdinalForSegment(segmentIndex);
	if (ordinal < 0) {
		return;
	}
	activateTextOrdinal(ordinal, cursorOffset);
}

bool Widget::prepareFieldForInput() {
	if (hasStructuralSelection()) {
		if (const auto target = removeCurrentStructuralSelection(true)) {
			activateTextOrdinal(*target, 0);
		} else {
			activateInitialNode();
		}
	} else if (_field->isHidden()) {
		activateInitialNode();
	}
	return !_field->isHidden();
}

bool Widget::replayKeyIntoField(QKeyEvent *e) {
	if (!RedirectTextToField(e->text()) || !prepareFieldForInput()) {
		return false;
	}
	_field->setFocusFast();
	QCoreApplication::sendEvent(_field->rawTextEdit(), e);
	return true;
}

bool Widget::replayImeIntoField(QInputMethodEvent *e) {
	const auto cursor = _field->rawTextEdit()->textCursor();
	if (!ImeEventProducesInput(*e, cursor) || !prepareFieldForInput()) {
		return false;
	}
	_field->setFocusFast();
	QCoreApplication::sendEvent(_field->rawTextEdit(), e);
	return true;
}

ApplyResult Widget::commitInlineField() {
	const auto result = applyFieldTextToState();
	if (result != ApplyResult::Failed) {
		return result;
	}
	revertInlineFieldToState();
	showLastLimitToast();
	return result;
}

void Widget::hideInlineFieldAndRefresh() {
	if (_field->isHidden()) {
		return;
	}
	const auto committed = recordMutationTransaction([&] {
		const auto committed = commitInlineField();
		_pendingOrdinal = -1;
		_pendingCursorOffset = 0;
		hideInlineField();
		clearInlineFieldEditSession();
		return committed;
	});
	refreshAfterInlineFieldCommit(committed);
}

void Widget::acceptInlineField() {
	hideInlineFieldAndRefresh();
}

void Widget::refreshPreparedContent() {
	setDocument(_state->prepared());
	relayoutCurrentContent();
}

void Widget::refreshPreparedLeafAtActiveSource() {
	if (const auto source = _state->activePreparedLeafSource()) {
		_article->updatePreparedLeaf(*source, _state->prepared());
		relayoutCurrentContent();
	} else {
		refreshPreparedContent();
	}
}

void Widget::applyExternalRichPageMutation(Fn<bool(RichPage&)> mutation) {
	if (!mutation) {
		return;
	}
	auto live = captureHistoryEntry();
	for (auto &entry : _history) {
		if (!mutation(entry.snapshot.richPage)) {
			return;
		}
	}
	if (!mutation(live.snapshot.richPage)) {
		return;
	}
	const auto wasPreservingExternalFieldRestore
		= PreservingExternalFieldRestore;
	PreservingExternalFieldRestore = this;
	const auto preserveExternalFieldRestore = gsl::finally([&] {
		PreservingExternalFieldRestore = wasPreservingExternalFieldRestore;
	});
	restoreHistoryEntry(live);
	_fieldUndoAvailable = !_field->isHidden()
		? _field->isUndoAvailable()
		: false;
	_fieldRedoAvailable = !_field->isHidden()
		? _field->isRedoAvailable()
		: false;
}

void Widget::relayoutCurrentContent() {
	const auto width = std::max(
		widthNoMargins(),
		parentWidget() ? parentWidget()->width() : 0);
	if (width > 0) {
		resizeToWidth(width);
	} else {
		update();
	}
}

void Widget::syncInlineFieldGeometry() {
	syncInlineFieldGeometry(widthNoMargins());
}

void Widget::insertBlock(State::InsertAction action) {
	recordMutationTransaction([&] {
		const auto context = activeTextInsertContext();
		auto committed = ApplyResult::Unchanged;
		if (!context && !_field->isHidden()) {
			committed = commitInlineField();
			if (committed == ApplyResult::Failed) {
				return MutationTransactionResult{
					.committed = committed,
					.failed = true,
				};
			}
		}
		const auto hadStructuralSelection = hasStructuralSelection();
		const auto restoreField = context.has_value();
		if (hadStructuralSelection || restoreField) {
			_pendingOrdinal = -1;
			_pendingCursorOffset = 0;
			hideInlineField();
			clearInlineFieldEditSession();
		}
		auto restore = restoreField;
		const auto restoreInlineField = gsl::finally([&] {
			if (!restore) {
				return;
			}
			_field->show();
			syncInlineFieldGeometry();
			updateInlineFieldHeightOverride();
			syncArticleVisibleTopBottom();
			revealActiveInlineField();
			_field->raise();
			_field->setFocusFast();
		});
		const auto applied = hadStructuralSelection
			? _state->replaceStructuralSelectionWithBlock(
				_structuralSelection,
				action,
				context)
			: _state->insertBlockAfterActive(action, context);
		if (!applied) {
			showLastLimitToast();
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		restore = false;
		if (hadStructuralSelection) {
			clearSelection();
		}
		refreshPreparedContent();
		activateTextOrdinal(_state->activeTextOrdinal(), 0);
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
}

void Widget::insertPreparedBlock(RichPage::Block block) {
	auto blocks = std::vector<RichPage::Block>();
	blocks.push_back(std::move(block));
	insertPreparedBlocks(std::move(blocks));
}

void Widget::insertPreparedBlocks(std::vector<RichPage::Block> blocks) {
	if (blocks.empty()) {
		return;
	}
	recordMutationTransaction([&] {
		const auto context = activeTextInsertContext();
		auto committed = ApplyResult::Unchanged;
		if (!context && !_field->isHidden()) {
			committed = commitInlineField();
			if (committed == ApplyResult::Failed) {
				return MutationTransactionResult{
					.committed = committed,
					.failed = true,
				};
			}
		}
		const auto hadStructuralSelection = hasStructuralSelection();
		const auto restoreField = context.has_value();
		if (hadStructuralSelection || restoreField) {
			_pendingOrdinal = -1;
			_pendingCursorOffset = 0;
			hideInlineField();
			clearInlineFieldEditSession();
		}
		auto restore = restoreField;
		const auto restoreInlineField = gsl::finally([&] {
			if (!restore) {
				return;
			}
			_field->show();
			syncInlineFieldGeometry();
			updateInlineFieldHeightOverride();
			syncArticleVisibleTopBottom();
			revealActiveInlineField();
			_field->raise();
			_field->setFocusFast();
		});
		const auto applied = hadStructuralSelection
			? _state->replaceStructuralSelectionWithPreparedBlocks(
				_structuralSelection,
				std::move(blocks),
				context)
			: _state->insertPreparedBlocksAfterActive(
				std::move(blocks),
				context);
		if (!applied) {
			showLastLimitToast();
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		restore = false;
		if (hadStructuralSelection) {
			clearSelection();
		}
		refreshPreparedContent();
		activateTextOrdinal(_state->activeTextOrdinal(), 0);
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
}

TextForMimeData Widget::currentSelectionTextForClipboard() const {
	return _article
		? _article->textForSelection(
			_selection,
			&_selectionEndpoints,
			hasStructuralSelection() ? &_structuralSelection : nullptr)
		: TextForMimeData();
}

void Widget::copyCurrentSelectionToClipboard() {
	auto structured = std::optional<ClipboardData>();
	if (hasStructuralSelection()) {
		structured = _state->structuredClipboardDataForSelection(
			_structuralSelection);
	}
	const auto text = currentSelectionTextForClipboard();
	auto mimeData = structured
		? MimeDataFromClipboardData(*structured)
		: TextUtilities::MimeDataFromText(text);
	if (!mimeData) {
		return;
	}
	if (structured) {
		if (const auto textMimeData = TextUtilities::MimeDataFromText(text)) {
			for (const auto &format : textMimeData->formats()) {
				mimeData->setData(format, textMimeData->data(format));
			}
		}
	}
	QApplication::clipboard()->setMimeData(mimeData.release());
}

void Widget::pasteStructuredClipboardData(const ClipboardData &data) {
	const auto blocks = std::get_if<ClipboardBlockData>(&data);
	const auto items = std::get_if<ClipboardListItemsData>(&data);
	if (blocks) {
		if (blocks->blocks.empty()) {
			return;
		}
	} else if (!items || items->items.empty()) {
		return;
	}
	recordMutationTransaction([&] {
		const auto context = ClipboardPasteInsertContext(
			activeTextInsertContext());
		auto committed = ApplyResult::Unchanged;
		if (!context && !_field->isHidden()) {
			committed = commitInlineField();
			if (committed == ApplyResult::Failed) {
				return MutationTransactionResult{
					.committed = committed,
					.failed = true,
				};
			}
		}
		const auto hadStructuralSelection = hasStructuralSelection();
		const auto restoreField = context.has_value();
		if (hadStructuralSelection || restoreField) {
			_pendingOrdinal = -1;
			_pendingCursorOffset = 0;
			hideInlineField();
			clearInlineFieldEditSession();
		}
		auto restore = restoreField;
		const auto restoreInlineField = gsl::finally([&] {
			if (!restore) {
				return;
			}
			_field->show();
			syncInlineFieldGeometry();
			updateInlineFieldHeightOverride();
			syncArticleVisibleTopBottom();
			revealActiveInlineField();
			_field->raise();
			_field->setFocusFast();
		});
		const auto applied = hadStructuralSelection
			? (blocks
				? _state->replaceStructuralSelectionWithPreparedBlocks(
					_structuralSelection,
					blocks->blocks,
					context)
				: _state->replaceStructuralSelectionWithClipboardListItems(
					_structuralSelection,
					*items,
					context))
			: (blocks
				? _state->insertPreparedBlocksAfterActive(
					blocks->blocks,
					context)
				: _state->pasteClipboardListItemsAfterActive(
					*items,
					context));
		if (!applied) {
			showLastLimitToast();
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		restore = false;
		if (hadStructuralSelection) {
			clearSelection();
		}
		refreshPreparedContent();
		activateTextOrdinal(_state->activeTextOrdinal(), 0);
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
}

bool Widget::handleClipboardKey(QKeyEvent *e) {
	if (e == QKeySequence::Copy) {
		if (_selection.empty() && !hasStructuralSelection()) {
			return false;
		}
		copyCurrentSelectionToClipboard();
		e->accept();
		return true;
	} else if ((e == QKeySequence::Paste) && _field->isHidden()) {
		if (const auto data = ClipboardDataFromMimeData(
				QApplication::clipboard()->mimeData())) {
			pasteStructuredClipboardData(*data);
			e->accept();
			return true;
		} else if (prepareFieldForInput()) {
			_field->setFocusFast();
			QCoreApplication::sendEvent(_field->rawTextEdit(), e);
			e->accept();
			return true;
		}
	}
	return false;
}

void Widget::truncateHistoryRedo() {
	if ((_historyIndex < 0) || (_historyIndex >= int(_history.size()))) {
		return;
	}
	const auto next = _history.begin() + _historyIndex + 1;
	if (next != _history.end()) {
		_history.erase(next, _history.end());
		removeRetainedLeafFieldsAfter(_historyIndex);
	}
}

bool Widget::canPerformFieldUndoRedo(bool redo) const {
	if (_field->isHidden()) {
		return false;
	}
	const auto document = _field->rawTextEdit()->document();
	const auto steps = redo
		? document->availableRedoSteps()
		: document->availableUndoSteps();
	const auto localRedoAvailable = (document->availableRedoSteps() > 0)
		|| _fieldRedoAvailable
		|| _field->isRedoAvailable();
	if (!redo
		&& localRedoAvailable
		&& activeInlineFieldTextMatchesState()) {
		return false;
	}
	const auto available = (steps > 0)
		|| (redo
			? (_fieldRedoAvailable || _field->isRedoAvailable())
			: (_fieldUndoAvailable || _field->isUndoAvailable()));
	if (!available) {
		return false;
	}
	const auto &noopState = redo
		? _fieldRedoNoopState
		: _fieldUndoNoopState;
	return !noopState || (_field->getTextWithTags() != *noopState);
}

bool Widget::activeInlineFieldTextMatchesState() const {
	if (_field->isHidden() || !_fieldLeaf) {
		return false;
	}
	const auto activeLeaf = _state->activeLeafPath();
	if (!activeLeaf || (*activeLeaf != *_fieldLeaf)) {
		return false;
	}
	const auto trimLeft = !_state->codeBlockLanguage(
		_state->activeTextOrdinal()).has_value();
	if (_state->activeFieldMode() == State::FieldMode::Raw) {
		return _field->getTextWithTags() == TrimInlineFieldText(
			{ _state->activeRawText(), {} },
			trimLeft).text;
	}
	const auto activeText = ConvertRichTextToEditorTags(_state->activeText());
	return _field->getTextWithTags() == TrimInlineFieldText(
		activeText.text,
		trimLeft).text;
}

bool Widget::canPerformHistoryUndoRedo(bool redo) const {
	if ((_historyIndex < 0) || (_historyIndex >= int(_history.size()))) {
		return false;
	}
	return redo
		? (_historyIndex + 1 < int(_history.size()))
		: (_historyIndex > 0);
}

bool Widget::canPerformUndoRedo(bool redo) const {
	return canPerformFieldUndoRedo(redo) || canPerformHistoryUndoRedo(redo);
}

bool Widget::handleUndoRedoShortcut(QKeyEvent *e) {
	auto redo = std::optional<bool>();
	if (e == QKeySequence::Undo) {
		redo = false;
	} else if (e == QKeySequence::Redo) {
		redo = true;
	}
	if (!redo) {
		return false;
	}
	const auto redoValue = *redo;
	if (canPerformFieldUndoRedo(redoValue)) {
		if (performFieldUndoRedo(redoValue)) {
			e->accept();
			return true;
		}
	}
	if (canPerformHistoryUndoRedo(redoValue)) {
		crl::on_main(this, [=] {
			performUndoRedo(redoValue, false);
		});
	}
	e->accept();
	return true;
}

bool Widget::handleSelectAllShortcut(QKeyEvent *e) {
	if (e != QKeySequence::SelectAll) {
		return false;
	}
	if (!_field->isHidden()) {
		const auto committed = recordMutationTransaction([&] {
			const auto committed = commitInlineField();
			if (committed != ApplyResult::Failed) {
				_pendingOrdinal = -1;
				_pendingCursorOffset = 0;
				hideInlineField();
				clearInlineFieldEditSession();
			}
			return committed;
		});
		if (committed == ApplyResult::Failed) {
			e->accept();
			return true;
		}
		refreshAfterInlineFieldCommit(committed);
	}
	const auto blockCount = int(_state->richPage().blocks.size());
	_selection = {};
	_selectionEndpoints = {};
	_articleSelectionDrag = {};
	setStructuralSelection(blockCount > 0
		? BlockSelectionFromIndexes(
			PreparedEditBlockContainerPath(),
			0,
			blockCount - 1)
		: PreparedEditSelection());
	setFocus();
	update();
	e->accept();
	return true;
}

bool Widget::performFieldUndoRedo(bool redo) {
	if (!canPerformFieldUndoRedo(redo)) {
		return false;
	}
	const auto before = _field->getTextWithTags();
	const auto wasPerformingUndoRedo = _performingUndoRedo;
	_performingUndoRedo = true;
	const auto guard = gsl::finally([&] {
		_performingUndoRedo = wasPerformingUndoRedo;
	});
	if (redo) {
		_field->redo();
	} else {
		_field->undo();
	}
	if (_field->isHidden()) {
		return false;
	}
	const auto document = _field->rawTextEdit()->document();
	_fieldUndoAvailable = (document->availableUndoSteps() > 0)
		|| _field->isUndoAvailable();
	_fieldRedoAvailable = (document->availableRedoSteps() > 0)
		|| _field->isRedoAvailable();
	const auto after = _field->getTextWithTags();
	if (after != before) {
		clearFieldUndoRedoNoopState();
		return true;
	}
	if (redo) {
		_fieldRedoNoopState = after;
	} else {
		_fieldUndoNoopState = after;
	}
	return false;
}

void Widget::performUndoRedo(bool redo, bool allowFieldLocal) {
	if (allowFieldLocal && performFieldUndoRedo(redo)) {
		return;
	}
	if (!canPerformHistoryUndoRedo(redo)) {
		return;
	}
	const auto nextIndex = _historyIndex + (redo ? 1 : -1);
	if ((nextIndex < 0) || (nextIndex >= int(_history.size()))) {
		return;
	}
	const auto previousIndex = _historyIndex;
	const auto wasPerformingUndoRedo = _performingUndoRedo;
	_performingUndoRedo = true;
	const auto guard = gsl::finally([&] {
		_performingUndoRedo = wasPerformingUndoRedo;
	});
	const auto wasRetainingFieldHistoryIndexOverride
		= _retainingFieldHistoryIndexOverride;
	_retainingFieldHistoryIndexOverride = previousIndex;
	const auto retainingFieldHistoryIndexOverride = gsl::finally([&] {
		_retainingFieldHistoryIndexOverride
			= wasRetainingFieldHistoryIndexOverride;
	});
	retainActiveLeafField();
	_historyIndex = nextIndex;
	const auto wasRestoringHistoryRedo = _restoringHistoryRedo;
	_restoringHistoryRedo = redo;
	const auto restoringHistoryRedo = gsl::finally([&] {
		_restoringHistoryRedo = wasRestoringHistoryRedo;
	});
	restoreHistoryEntry(_history[_historyIndex]);
	_fieldUndoAvailable = !_field->isHidden()
		? _field->isUndoAvailable()
		: false;
	_fieldRedoAvailable = !_field->isHidden()
		? _field->isRedoAvailable()
		: false;
	clearFieldUndoRedoNoopState();
}

void Widget::clearFieldUndoRedoNoopState() {
	_fieldUndoNoopState = std::nullopt;
	_fieldRedoNoopState = std::nullopt;
}

void Widget::insertHeading1() {
	insertBlock({
		.type = State::InsertBlockType::Heading,
		.headingLevel = 1,
	});
}

void Widget::insertBlockquote() {
	insertBlock({ .type = State::InsertBlockType::Blockquote });
}

void Widget::insertEmoji(EmojiPtr emoji) {
	if (!emoji || !prepareFieldForInput()) {
		return;
	}
	_field->setFocusFast();
	Ui::InsertEmojiAtCursor(_field->textCursor(), emoji);
}

void Widget::insertCustomEmoji(not_null<DocumentData*> document) {
	if (!prepareFieldForInput()) {
		return;
	}
	_field->setFocusFast();
	Data::InsertCustomEmoji(_field.get(), document);
}

void Widget::setInlineFieldExternalInteractionActive(bool active) {
	_inlineFieldExternalInteractionActive = active;
}

int Widget::resizeGetHeight(int newWidth) {
	if (!_article) {
		return 1;
	}
	const auto width = std::max(newWidth, 1);
	const auto padding = EditorBodyPadding();
	_articleHeight = _article->resizeGetHeight(articleWidth(width));
	syncArticleVisibleTopBottom();
	ensurePendingActivation();
	syncInlineFieldGeometry(width);
	const auto fieldBottom = !_field->isHidden()
		? (_field->y() + _field->height())
		: 0;
	return std::max(
		std::max(
			_articleHeight + padding.top() + padding.bottom(),
			fieldBottom),
		st::ivEditorMinHeight);
}

void Widget::visibleTopBottomUpdated(int visibleTop, int visibleBottom) {
	_visibleRange = Ui::VisibleRange{
		.top = visibleTop,
		.bottom = visibleBottom,
	};
	syncArticleVisibleTopBottom();
}

bool Widget::eventFilter(QObject *object, QEvent *event) {
	if (_field) {
		const auto raw = _field->rawTextEdit();
		if (object == raw.get() || object == raw->viewport()) {
			const auto type = event->type();
			if (type == QEvent::Wheel) {
				if (_article && _activeSegmentIndex >= 0) {
					const auto wheel = static_cast<QWheelEvent*>(event);
					auto articlePoint = std::optional<QPoint>();
					if (const auto widget = qobject_cast<QWidget*>(object)) {
						articlePoint = widget->mapTo(this, LocalPosition(wheel))
							- articleTopLeft();
					} else {
						articlePoint = mapFromGlobal(GlobalPosition(wheel))
							- articleTopLeft();
					}
					if (!articlePoint) {
						const auto segmentRect = _article->segmentRect(
							_activeSegmentIndex);
						if (!segmentRect.isEmpty()) {
							articlePoint = segmentRect.center();
						}
					}
					if (articlePoint
						&& handleHorizontalScrollWheel(wheel, *articlePoint)) {
						return true;
					}
				}
			} else if (type == QEvent::KeyPress) {
				const auto keyEvent = static_cast<QKeyEvent*>(event);
				if (handleUndoRedoShortcut(keyEvent)
					|| handleSelectAllShortcut(keyEvent)
					|| handleTabNavigation(keyEvent)
					|| handleStructuralSelectionKey(keyEvent)
					|| handleFieldKey(keyEvent)) {
					return true;
				}
			} else if (type == QEvent::ContextMenu) {
				const auto context = static_cast<QContextMenuEvent*>(event);
				if (handleFieldContextMenuEvent(object, context)) {
					return true;
				}
			} else if ((type == QEvent::MouseButtonPress
				|| type == QEvent::MouseMove
				|| type == QEvent::MouseButtonRelease)
				&& handleFieldMouseEvent(event)) {
				return true;
			}
		}
	}
	return Ui::RpWidget::eventFilter(object, event);
}

bool Widget::eventHook(QEvent *e) {
	if (e->type() == QEvent::TouchBegin
		|| e->type() == QEvent::TouchUpdate
		|| e->type() == QEvent::TouchEnd
		|| e->type() == QEvent::TouchCancel) {
		auto *ev = static_cast<QTouchEvent*>(e);
		if (ev->device()->type() == base::TouchDevice::TouchScreen) {
			const auto active = (_horizontalScrollDrag
				== HorizontalScrollDrag::Touch);
			touchEvent(ev);
			if (active
				|| (_horizontalScrollDrag == HorizontalScrollDrag::Touch)) {
				return true;
			}
		}
	}
	return Ui::RpWidget::eventHook(e);
}

void Widget::contextMenuEvent(QContextMenuEvent *e) {
	if (!_article) {
		Ui::RpWidget::contextMenuEvent(e);
		return;
	}
	const auto articlePoint = e->pos() - articleTopLeft();
	const auto editHit = _article->editHitTest(articlePoint);
	const auto owner = StructuralOwnerFromHit(editHit);
	const auto cell = TableCellFromOwner(owner);
	if (!cell) {
		Ui::RpWidget::contextMenuEvent(e);
		return;
	}
	const auto range = effectiveTableRangeForCell(*cell);
	if (range.empty()) {
		Ui::RpWidget::contextMenuEvent(e);
		return;
	}
	showTableContextMenu(range, e->globalPos());
	e->accept();
}

void Widget::focusInEvent(QFocusEvent *e) {
	Ui::RpWidget::focusInEvent(e);
	if (!_settingField && !_field->isHidden()) {
		_field->setFocusFast();
	}
}

bool Widget::focusNextPrevChild(bool next) {
	if (hasFocus() && _field->isHidden() && moveTabBoundary(next)) {
		return true;
	}
	return Ui::RpWidget::focusNextPrevChild(next);
}

void Widget::keyPressEvent(QKeyEvent *e) {
	if (handleUndoRedoShortcut(e)) {
		return;
	} else if (handleSelectAllShortcut(e)) {
		return;
	} else if (handleClipboardKey(e)) {
		return;
	} else if (handleStructuralSelectionKey(e)) {
		return;
	} else if (_field->isHidden() && handleTabNavigation(e)) {
		return;
	} else if (redirectKeyToField(e) && replayKeyIntoField(e)) {
		e->accept();
		return;
	}
	Ui::RpWidget::keyPressEvent(e);
}

bool Widget::handleHorizontalScrollWheel(
		QWheelEvent *e,
		QPoint articlePoint) {
	const auto phase = e->phase();
	if (phase == Qt::NoScrollPhase) {
		_horizontalScrollLock = std::nullopt;
	} else if (phase == Qt::ScrollBegin) {
		_horizontalScrollLock = std::nullopt;
	}
	if (!_article) {
		return false;
	}
	const auto delta = Ui::ScrollDeltaF(e);
	const auto horizontal = (std::abs(delta.x()) > std::abs(delta.y()));
	if (phase != Qt::NoScrollPhase
		&& phase != Qt::ScrollBegin
		&& !_horizontalScrollLock) {
		_horizontalScrollLock = horizontal ? Qt::Horizontal : Qt::Vertical;
	}
	if (!_article->horizontalScrollHit(articlePoint).scrollable) {
		return false;
	}
	if (horizontal) {
		if (_horizontalScrollLock == Qt::Vertical) {
			return false;
		}
		if (_article->consumeHorizontalScroll(
				articlePoint,
				int(std::round(delta.x())))) {
			syncInlineFieldGeometry();
		}
		e->accept();
		return true;
	}
	if (_horizontalScrollLock == Qt::Horizontal) {
		e->accept();
		return true;
	}
	return false;
}

std::optional<PreparedEditTableCellSource> Widget::activeTableCellSourceAt(
		QObject *object,
		const QContextMenuEvent &e) const {
	if (!_article || _activeSegmentIndex < 0) {
		return std::nullopt;
	}
	const auto cellAt = [&](QPoint articlePoint) {
		const auto owner = StructuralOwnerFromHit(
			_article->editHitTest(articlePoint));
		return TableCellFromOwner(owner);
	};
	if (const auto widget = qobject_cast<QWidget*>(object)) {
		if (const auto cell = cellAt(
				widget->mapTo(this, e.pos()) - articleTopLeft())) {
			return cell;
		}
	}
	const auto segmentRect = _article->segmentRect(_activeSegmentIndex);
	return !segmentRect.isEmpty()
		? cellAt(segmentRect.center())
		: std::optional<PreparedEditTableCellSource>();
}

bool Widget::handleFieldContextMenuEvent(
		QObject *object,
		QContextMenuEvent *e) {
	const auto cell = activeTableCellSourceAt(object, *e);
	if (!cell) {
		return false;
	}
	const auto range = effectiveTableRangeForCell(*cell);
	if (range.empty() || !_state->tableSelectionInfo(range).valid) {
		return false;
	}
	const auto raw = _field->rawTextEdit();
	const auto menu = raw->createStandardContextMenu();
	if (!menu) {
		return false;
	}
	const auto before = menu->actions().empty()
		? nullptr
		: menu->actions().front();
	const auto changeTable = new QAction(
		tr::lng_article_table_change(tr::now),
		menu);
	menu->insertAction(before, changeTable);
	menu->insertSeparator(before);
	const auto setupPopupMenu = [=](not_null<Ui::PopupMenu*> popup) {
		changeTable->setMenu(new QMenu(menu));
		const auto submenu = popup->ensureSubmenu(
			changeTable,
			st::popupMenuWithIcons);
		fillTableChangeMenu(submenu, range);
	};
	auto copied = std::make_shared<QContextMenuEvent>(
		e->reason(),
		e->pos(),
		e->globalPos());
	_fieldContextMenuRequests.fire({
		.menu = menu,
		.event = std::move(copied),
		.setupPopupMenu = setupPopupMenu,
	});
	e->accept();
	return true;
}

PreparedEditTableCellRange Widget::effectiveTableRangeForCell(
		const PreparedEditTableCellSource &source) {
	const auto single = TableRangeFromCell(source);
	if (single.empty()) {
		return {};
	}
	if (const auto selected = _state->tableContextRangeForSelection(
			_structuralSelection,
			source)) {
		return *selected;
	}
	clearSelection();
	return single;
}

void Widget::showTableContextMenu(
		const PreparedEditTableCellRange &range,
		QPoint globalPos) {
	const auto menu = Ui::CreateChild<Ui::PopupMenu>(
		this,
		st::popupMenuWithIcons);
	fillTableChangeMenu(menu, range);
	if (menu->empty()) {
		menu->deleteLater();
		return;
	}
	menu->popup(globalPos);
}

void Widget::fillTableChangeMenu(
		not_null<Ui::PopupMenu*> menu,
		const PreparedEditTableCellRange &range) {
	const auto info = _state->tableSelectionInfo(range);
	if (!info.valid) {
		return;
	}
	menu->addAction(
		tr::lng_article_table_add_row(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->addTableRow(range, false);
			});
		},
		&st::menuIconTableSubmenuRowAbove);
	menu->addAction(
		tr::lng_article_table_add_row(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->addTableRow(range, true);
			});
		},
		&st::menuIconTableSubmenuRowBelow);
	menu->addAction(
		tr::lng_article_table_add_column(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->addTableColumn(range, false);
			});
		},
		&st::menuIconTableSubmenuColumnLeft);
	menu->addAction(
		tr::lng_article_table_add_column(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->addTableColumn(range, true);
			});
		},
		&st::menuIconTableSubmenuColumnRight);
	menu->addSeparator();
	Menu::AddCheckedAction(
		menu,
		tr::lng_article_table_header(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->setTableHeader(range, !info.allHeader);
			});
		},
		nullptr,
		info.allHeader);
	menu->addSeparator();
	Menu::AddCheckedAction(
		menu,
		tr::lng_article_table_align_center(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->setTableAlignment(
					range,
					info.allAlignCenter
						? RichPage::TableAlignment::Left
						: RichPage::TableAlignment::Center);
			});
		},
		nullptr,
		info.allAlignCenter);
	Menu::AddCheckedAction(
		menu,
		tr::lng_article_table_align_right(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->setTableAlignment(
					range,
					info.allAlignRight
						? RichPage::TableAlignment::Left
						: RichPage::TableAlignment::Right);
			});
		},
		nullptr,
		info.allAlignRight);
	menu->addSeparator();
	Menu::AddCheckedAction(
		menu,
		tr::lng_article_table_align_middle(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->setTableVerticalAlignment(
					range,
					info.allAlignMiddle
						? RichPage::TableVerticalAlignment::Top
						: RichPage::TableVerticalAlignment::Middle);
			});
		},
		nullptr,
		info.allAlignMiddle);
	Menu::AddCheckedAction(
		menu,
		tr::lng_article_table_align_bottom(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->setTableVerticalAlignment(
					range,
					info.allAlignBottom
						? RichPage::TableVerticalAlignment::Top
						: RichPage::TableVerticalAlignment::Bottom);
			});
		},
		nullptr,
		info.allAlignBottom);
	if (info.canSplitCell) {
		menu->addSeparator();
		menu->addAction(
			tr::lng_article_table_split_cell(tr::now),
			[=] {
				applyTableChange([=] {
					return _state->splitTableCell(range);
				});
			});
	} else if (info.canUniteCells) {
		menu->addSeparator();
		menu->addAction(
			tr::lng_article_table_unite_cells(tr::now),
			[=] {
				applyTableChange([=] {
					return _state->uniteTableCells(range);
				});
			});
	}
	const auto hasDeleteAction = info.canDeleteTable
		|| info.canDeleteRows
		|| info.canDeleteColumns;
	if (hasDeleteAction) {
		menu->addSeparator();
		if (info.canDeleteTable) {
			menu->addAction(
				tr::lng_article_table_delete_table(tr::now),
				[=] {
					applyTableChange([=] {
						return _state->removeTable(range);
					});
				},
				&st::menuIconTableSubmenuDelete);
		} else {
			if (info.canDeleteRows) {
				menu->addAction(
					(info.selectedRows == 1)
						? tr::lng_article_table_delete_row(tr::now)
						: tr::lng_article_table_delete_rows(tr::now),
					[=] {
						applyTableChange([=] {
							return _state->removeTableRows(range);
						});
					},
					&st::menuIconTableSubmenuDelete);
			}
			if (info.canDeleteColumns) {
				menu->addAction(
					(info.selectedColumns == 1)
						? tr::lng_article_table_delete_column(tr::now)
						: tr::lng_article_table_delete_columns(tr::now),
					[=] {
						applyTableChange([=] {
							return _state->removeTableColumns(range);
						});
					},
					&st::menuIconTableSubmenuDelete);
			}
		}
	}
	menu->addSeparator();
	Menu::AddCheckedAction(
		menu,
		tr::lng_article_table_bordered(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->setTableBordered(range, !info.bordered);
			});
		},
		nullptr,
		info.bordered);
	Menu::AddCheckedAction(
		menu,
		tr::lng_article_table_striped(tr::now),
		[=] {
			applyTableChange([=] {
				return _state->setTableStriped(range, !info.striped);
			});
		},
		nullptr,
		info.striped);
}

void Widget::applyTableChange(Fn<bool()> change) {
	recordMutationTransaction([&] {
		const auto committed = commitInlineField();
		if (committed == ApplyResult::Failed) {
			return MutationTransactionResult{
				.committed = committed,
				.failed = true,
			};
		}
		_pendingOrdinal = -1;
		_pendingCursorOffset = 0;
		hideInlineField();
		if (_article) {
			_article->clearTextLeafHeightOverride();
		}
		clearSelection();
		setFocus();
		if (!change()) {
			refreshAfterInlineFieldCommit(committed);
			showLastLimitToast();
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		refreshPreparedContent();
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
}

void Widget::touchEvent(QTouchEvent *e) {
	if (e->type() == QEvent::TouchCancel) {
		_pendingTouchHorizontalScrollPoint = std::nullopt;
		if (_horizontalScrollDrag != HorizontalScrollDrag::Touch) {
			return;
		}
		_horizontalScrollDrag = HorizontalScrollDrag::None;
		if (_article) {
			_article->endHorizontalScroll();
		}
		e->accept();
		return;
	}
	if (!_article || e->touchPoints().isEmpty()) {
		return;
	}
	const auto articlePoint = mapFromGlobal(
		e->touchPoints().cbegin()->screenPos().toPoint()) - articleTopLeft();
	switch (e->type()) {
	case QEvent::TouchBegin: {
		_pendingTouchHorizontalScrollPoint = std::nullopt;
		const auto hit = _article->horizontalScrollHit(articlePoint);
		if (hit.overScrollbar
			&& _article->beginHorizontalScroll(articlePoint, false)) {
			_horizontalScrollDrag = HorizontalScrollDrag::Touch;
			syncInlineFieldGeometry();
			e->accept();
		} else if (hit.overViewport) {
			_pendingTouchHorizontalScrollPoint = articlePoint;
		}
	} break;
	case QEvent::TouchUpdate:
		if (_horizontalScrollDrag == HorizontalScrollDrag::Touch) {
			if (_article->updateHorizontalScroll(articlePoint)) {
				syncInlineFieldGeometry();
			}
			e->accept();
		} else if (_pendingTouchHorizontalScrollPoint) {
			const auto delta = articlePoint - *_pendingTouchHorizontalScrollPoint;
			if (delta.manhattanLength() < QApplication::startDragDistance()) {
				break;
			}
			const auto horizontal = (std::abs(delta.x()) > std::abs(delta.y()));
			if (!horizontal) {
				_pendingTouchHorizontalScrollPoint = std::nullopt;
				break;
			}
			if (_article->beginHorizontalScroll(
					*_pendingTouchHorizontalScrollPoint,
					true)) {
				_horizontalScrollDrag = HorizontalScrollDrag::Touch;
				if (_article->updateHorizontalScroll(articlePoint)) {
					syncInlineFieldGeometry();
				}
				e->accept();
			}
			_pendingTouchHorizontalScrollPoint = std::nullopt;
		}
		break;
	case QEvent::TouchEnd:
		_pendingTouchHorizontalScrollPoint = std::nullopt;
		if (_horizontalScrollDrag == HorizontalScrollDrag::Touch) {
			_horizontalScrollDrag = HorizontalScrollDrag::None;
			_article->endHorizontalScroll();
			e->accept();
		}
		break;
	default:
		break;
	}
}

void Widget::wheelEvent(QWheelEvent *e) {
	if (handleHorizontalScrollWheel(
			e,
			LocalPosition(e) - articleTopLeft())) {
		return;
	}
	e->ignore();
}

bool Widget::redirectKeyToField(QKeyEvent *e) const {
	if (!hasFocus()) {
		return false;
	}
	const auto modifiers = e->modifiers()
		& ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
	return (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier)
		&& (e->key() != Qt::Key_Shift)
		&& RedirectTextToField(e->text());
}

void Widget::inputMethodEvent(QInputMethodEvent *e) {
	if (!_field) {
		Ui::RpWidget::inputMethodEvent(e);
		return;
	}
	const auto cursor = _field->rawTextEdit()->textCursor();
	if (!ImeEventProducesInput(*e, cursor) || !redirectImeToField()) {
		Ui::RpWidget::inputMethodEvent(e);
		return;
	}
	if (!replayImeIntoField(e)) {
		Ui::RpWidget::inputMethodEvent(e);
		return;
	}
	e->accept();
	return;
}

QVariant Widget::inputMethodQuery(Qt::InputMethodQuery query) const {
	if (!_field) {
		return Ui::RpWidget::inputMethodQuery(query);
	}
	return _field->rawTextEdit()->inputMethodQuery(query);
}

bool Widget::redirectImeToField() const {
	return hasFocus()
		&& (hasStructuralSelection() || _field->isHidden());
}

void Widget::mouseMoveEvent(QMouseEvent *e) {
	const auto articlePoint = e->pos() - articleTopLeft();
	if (_horizontalScrollDrag == HorizontalScrollDrag::Mouse) {
		if (_article->updateHorizontalScroll(articlePoint)) {
			syncInlineFieldGeometry();
		}
		e->accept();
		return;
	}
	if (!_articleSelectionDrag.active) {
		auto cursor = style::cur_default;
		const auto controlHit = _article->editControlHitTest(articlePoint);
		if (controlHit.valid()) {
			cursor = style::cur_pointer;
		} else {
			const auto hit = _article->hitTest(
				articlePoint,
				Ui::Text::StateRequest::Flag::LookupSymbol);
			if (hit.valid() && hit.codeHeaderCopy) {
				cursor = style::cur_pointer;
			} else if (hit.valid()
				&& hit.direct
				&& _article->segmentIsText(hit.segmentIndex)) {
				cursor = style::cur_text;
			}
		}
		setCursor(cursor);
		Ui::RpWidget::mouseMoveEvent(e);
		return;
	}
	const auto hit = _article->hitTest(
		articlePoint,
		Ui::Text::StateRequest::Flag::LookupSymbol);
	const auto editHit = _article->editHitTest(articlePoint);
	updateArticleSelection(articlePoint, hit, editHit);
	e->accept();
}

void Widget::mousePressEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		Ui::RpWidget::mousePressEvent(e);
		return;
	}
	_trackingPointerPress = true;
	_pressedControl = {};
	_pressedControlPoint = std::nullopt;
	auto articlePoint = e->pos() - articleTopLeft();
	const auto horizontalScrollHit = _article->horizontalScrollHit(
		articlePoint);
	if (horizontalScrollHit.overScrollbar
		&& _article->beginHorizontalScroll(articlePoint, false)) {
		_horizontalScrollDrag = HorizontalScrollDrag::Mouse;
		syncInlineFieldGeometry();
		e->accept();
		return;
	}
	const auto controlHit = _article->editControlHitTest(articlePoint);
	if (controlHit.valid()) {
		_pressedControl = controlHit;
		_pressedControlPoint = articlePoint;
		e->accept();
		return;
	}
	auto hit = _article->hitTest(
		articlePoint,
		Ui::Text::StateRequest::Flag::LookupSymbol);
	const auto editHit = _article->editHitTest(articlePoint);
	const auto startedBelow = (articlePoint.y() >= _articleHeight);
	if (hit.codeHeaderCopy) {
		startArticleSelection(articlePoint, hit, editHit);
		e->accept();
		return;
	}
	if (hit.valid() && hit.direct && _article->segmentIsText(hit.segmentIndex)) {
		startArticleSelection(articlePoint, hit, editHit);
		e->accept();
		return;
	}
	if (startedBelow) {
		if (editHit.valid()) {
			startArticleSelection(
				articlePoint,
				hit,
				editHit,
				false,
				true);
		} else {
			clearSelection();
		}
		e->accept();
		return;
	}
	if (editHit.valid()) {
		startArticleSelection(articlePoint, hit, editHit);
		e->accept();
		return;
	}
	clearSelection();
	e->accept();
}

void Widget::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() != Qt::LeftButton) {
		Ui::RpWidget::mouseReleaseEvent(e);
		return;
	}
	const auto guard = gsl::finally([&] {
		_trackingPointerPress = false;
	});
	const auto finishDrag = gsl::finally([&] {
		finishArticleSelection();
	});
	const auto articlePoint = e->pos() - articleTopLeft();
	if (_horizontalScrollDrag == HorizontalScrollDrag::Mouse) {
		const auto changed = _article->updateHorizontalScroll(articlePoint);
		_article->endHorizontalScroll();
		_horizontalScrollDrag = HorizontalScrollDrag::None;
		if (changed) {
			syncInlineFieldGeometry();
		}
		e->accept();
		return;
	}
	const auto controlHit = _article->editControlHitTest(articlePoint);
	const auto applyControlToggle = [&](auto &&toggle, auto &&afterRefresh) {
		const auto hadVisibleField = !_field->isHidden();
		auto toggled = false;
		const auto result = recordMutationTransaction([&] {
			const auto committed = commitInlineField();
			if (committed == ApplyResult::Failed) {
				return MutationTransactionResult{
					.committed = committed,
					.failed = true,
				};
			}
			_pendingOrdinal = -1;
			_pendingCursorOffset = 0;
			hideInlineField();
			clearInlineFieldEditSession();
			toggled = toggle();
			if (toggled) {
				refreshPreparedContent();
			} else if (hadVisibleField) {
				refreshAfterInlineFieldCommit(committed);
			}
			clearTextSelection();
			clearStructuralSelection();
			setFocus();
			if (toggled) {
				afterRefresh();
			}
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed) || toggled,
			};
		});
		return !result.failed && toggled;
	};
	if (_pressedControl.valid()) {
		const auto pressedControl = _pressedControl;
		const auto pressedControlPoint = _pressedControlPoint;
		_pressedControl = {};
		_pressedControlPoint = std::nullopt;
		const auto matchedControl = pressedControlPoint
			&& ((articlePoint - *pressedControlPoint).manhattanLength()
				< QApplication::startDragDistance())
			&& (controlHit == pressedControl);
		if (matchedControl) {
			switch (pressedControl.kind) {
			case Markdown::MarkdownArticleEditControlHitKind::TaskMarker:
				if (pressedControl.listItem) {
					applyControlToggle([&] {
						return _state->toggleTaskState(*pressedControl.listItem);
					}, [&] {
						_article->addTaskMarkerRipple(
							*pressedControl.listItem,
							articlePoint);
					});
				}
				break;
			case Markdown::MarkdownArticleEditControlHitKind::DetailsToggle:
				if (pressedControl.block) {
					applyControlToggle([&] {
						return _state->toggleDetailsOpen(*pressedControl.block);
					}, [] {
					});
				}
				break;
			case Markdown::MarkdownArticleEditControlHitKind::None:
				break;
			}
		}
		e->accept();
		return;
	}
	const auto hit = _article->hitTest(
		articlePoint,
		Ui::Text::StateRequest::Flag::LookupSymbol);
	const auto editHit = _article->editHitTest(articlePoint);
	const auto formulaOrdinalFromEditHit = [&] {
		return editHit.leaf
			&& (editHit.leaf->kind
				== Markdown::PreparedEditLeafKind::MathFormula)
			? _state->textOrdinalForLeaf(*editHit.leaf)
			: -1;
	};
	const auto directEditableHit = [&] {
		return (hit.valid()
			&& hit.direct
			&& _article->segmentIsEditable(hit.segmentIndex))
			|| (formulaOrdinalFromEditHit() >= 0);
	};
	const auto commitVisibleInlineField = [&] {
		if (_field->isHidden()) {
			return false;
		}
		const auto committed = recordMutationTransaction([&] {
			const auto committed = commitInlineField();
			if (committed != ApplyResult::Failed) {
				_pendingOrdinal = -1;
				_pendingCursorOffset = 0;
				hideInlineField();
				clearInlineFieldEditSession();
			}
			return committed;
		});
		if (committed == ApplyResult::Failed) {
			return false;
		}
		refreshAfterInlineFieldCommit(committed);
		return true;
	};
	const auto focusOrActivateInitial = [&] {
		if (_field->isHidden()) {
			activateInitialNode();
		} else {
			_field->setFocusFast();
		}
	};
	const auto editCodeBlockLanguage = [&] {
		if (!hit.codeHeaderCopy) {
			return false;
		}
		auto languageHit = hit;
		if (!_field->isHidden()) {
			if (!commitVisibleInlineField()) {
				return true;
			}
			languageHit = _article->hitTest(
				articlePoint,
				Ui::Text::StateRequest::Flag::LookupSymbol);
		}
		const auto ordinal = languageHit.codeHeaderCopy
			? editableOrdinalForSegment(languageHit.segmentIndex)
			: -1;
		if (const auto now = _state->codeBlockLanguage(ordinal)) {
			const auto weak = QPointer<Widget>(this);
			DefaultEditLanguageCallback(_show)(
				*now,
				[=](QString language) {
					if (!weak) {
						return;
					}
					weak->recordMutationTransaction([&] {
						const auto changed = weak->_state->setCodeBlockLanguage(
							ordinal,
							language);
						if (changed) {
							weak->refreshPreparedContent();
							weak->update();
						}
						return changed;
					});
				});
		}
		return true;
	};
	if (_articleSelectionDrag.active) {
		const auto fromField = _articleSelectionDrag.fromField;
		const auto pendingCodeHeader = _articleSelectionDrag.codeHeader;
		const auto startedBelow = _articleSelectionDrag.startedBelow;
		const auto updateOnRelease
			= (_articleSelectionDrag.mode != DragSelectionMode::None)
			|| (!pendingCodeHeader
				&& (!startedBelow || articlePoint.y() < _articleHeight));
		if (updateOnRelease) {
			updateArticleSelection(articlePoint, hit, editHit);
		}
		if (hasStructuralSelection()) {
			commitVisibleInlineField();
			e->accept();
			return;
		}
		if (_articleSelectionDrag.mode == DragSelectionMode::Text) {
			const auto selection = _selection;
			const auto sameSegmentSelection = !selection.empty()
				&& (selection.from.segment == selection.to.segment)
				&& _article->segmentIsText(selection.from.segment);
			const auto selectionOrdinal = sameSegmentSelection
				? editableOrdinalForSegment(selection.from.segment)
				: -1;
			if (!fromField && selectionOrdinal >= 0) {
				const auto selectionFrom = selection.from.offset;
				const auto selectionTo = selection.to.offset;
				clearTextSelection();
				if (_field->isHidden() || commitVisibleInlineField()) {
					activateTextOrdinal(
						selectionOrdinal,
						selectionFrom,
						selectionTo);
				}
				e->accept();
				return;
			} else if (fromField) {
				e->accept();
				return;
			}
		}
		if (pendingCodeHeader
			&& _articleSelectionDrag.mode == DragSelectionMode::None
			&& editCodeBlockLanguage()) {
			e->accept();
			return;
		}
		const auto changed = !_selection.empty()
			|| _selectionEndpoints.from.valid()
			|| _selectionEndpoints.to.valid()
			|| hasStructuralSelection();
		_selection = {};
		_selectionEndpoints = {};
		setStructuralSelection({});
		if (changed) {
			update();
		}
	} else if (hit.codeHeaderCopy && editCodeBlockLanguage()) {
		e->accept();
		return;
	}
	if (directEditableHit()) {
		const auto formulaOrdinal = formulaOrdinalFromEditHit();
		const auto segmentHit = hit.valid()
			&& hit.direct
			&& _article->segmentIsEditable(hit.segmentIndex);
		const auto targetOrdinal = segmentHit
			? editableOrdinalForSegment(hit.segmentIndex)
			: formulaOrdinal;
		const auto offset = segmentHit
			? _article->selectionOffsetFromHit(
				hit,
				TextSelectType::Letters)
			: 0;
		if (targetOrdinal >= 0
			&& !_field->isHidden()
			&& hit.segmentIndex == _activeSegmentIndex) {
			auto cursor = _field->textCursor();
			cursor.setPosition(std::clamp(
				offset,
				0,
				int(_field->getLastText().size())));
			_field->setTextCursor(cursor);
			_field->setFocusFast();
		} else if (targetOrdinal >= 0) {
			if (_field->isHidden() || commitVisibleInlineField()) {
				activateTextOrdinal(targetOrdinal, offset);
			}
		}
	} else if (articlePoint.y() >= _articleHeight) {
		activateTrailingParagraph();
	} else {
		focusOrActivateInitial();
	}
	e->accept();
}

void Widget::paintEvent(QPaintEvent *e) {
	if (!_article) {
		return;
	}
	auto p = Painter(this);
	p.setTextPalette(st::inTextPalette);
	const auto topLeft = articleTopLeft();
	p.translate(topLeft);
	_article->paint(
		p,
		textPaintContext(e->rect().translated(-topLeft.x(), -topLeft.y())));
}

void Widget::resizeEvent(QResizeEvent *e) {
	Ui::RpWidget::resizeEvent(e);
	syncInlineFieldGeometry();
}

void Widget::setDocument(const Markdown::MarkdownArticleContent &prepared) {
	_article->setContent(prepared);
}

Markdown::MarkdownArticleTextLeafStyle Widget::inlineFieldStyleForSegment(
		int segmentIndex) const {
	return _article
		? _article->editableStyleForSegment(segmentIndex)
		: Markdown::MarkdownArticleTextLeafStyle();
}

const Widget::CachedInlineFieldStyle &Widget::inlineFieldStyleFor(
		const Markdown::MarkdownArticleTextLeafStyle &leafStyle) {
	return inlineFieldStyleFor(normalizedInlineFieldStyle(leafStyle));
}

const Widget::CachedInlineFieldStyle &Widget::inlineFieldStyleFor(
		const InlineFieldStyleData &data) {
	auto key = inlineFieldStyleKey(data);
	auto textFg = data.textFg;
	auto ownedTextFg = std::shared_ptr<style::owned_color>();
	auto ownedTextMarkBg = std::make_shared<style::owned_color>(
		data.textMarkBg);
	auto textMarkBg = ownedTextMarkBg->color();
	if (_inlineFieldTextColorOverride
		&& data.textFg.get() == _inlineFieldTextColorOverride->color().get()) {
		ownedTextFg = std::make_shared<style::owned_color>(data.textFg->c);
		textFg = ownedTextFg->color();
		key.textFg = textFg;
	}
	for (const auto &cached : _fieldStyles) {
		if (cached.key == key) {
			cached.ownedTextMarkBg->update(data.textMarkBg);
			return cached;
		}
	}
	auto fieldStyle = std::make_shared<style::InputField>(
		st::ivEditorInputField);
	fieldStyle->style = *data.textStyle;
	fieldStyle->style.font = data.italic
		? data.textStyle->font->italic()
		: data.textStyle->font;
	fieldStyle->style.lineHeight = data.lineHeight;
	fieldStyle->textFg = textFg;
	fieldStyle->textMarkBg = textMarkBg;
	fieldStyle->textAlign = data.align;
	fieldStyle->placeholderFont = fieldStyle->style.font;
	fieldStyle->placeholderAlign = data.align;
	_fieldStyles.push_back({
		.key = key,
		.style = std::move(fieldStyle),
		.ownedTextFg = std::move(ownedTextFg),
		.ownedTextMarkBg = std::move(ownedTextMarkBg),
	});
	return _fieldStyles.back();
}

std::optional<QColor> Widget::activeQuoteCaptionColor() {
	if (!_state->activeLeafUsesQuoteCaptionColor()) {
		return std::nullopt;
	}
	return Markdown::NonPullquoteQuoteCaptionColor(
		textPaintContext(QRect()),
		*_articleStyle);
}

std::optional<QColor> Widget::activeQuotePlaceholderColor() {
	if (!_state->activeLeafUsesQuotePlaceholderColor()) {
		return std::nullopt;
	}
	return Markdown::NonPullquoteQuoteCaptionColor(
		textPaintContext(QRect()),
		*_articleStyle);
}

void Widget::refreshInlineFieldTextColorOverride() {
	const auto color = activeQuoteCaptionColor();
	if (!color) {
		if (_inlineFieldTextColorOverride) {
			_activeFieldStyleKey = std::nullopt;
			_inlineFieldTextColorOverride.reset();
		}
		return;
	}
	if (_inlineFieldTextColorOverride) {
		_inlineFieldTextColorOverride->update(*color);
	} else {
		_inlineFieldTextColorOverride.emplace(*color);
	}
}

Widget::InlineFieldStyleData Widget::normalizedInlineFieldStyle(
		const Markdown::MarkdownArticleTextLeafStyle &leafStyle) const {
	const auto valid = leafStyle.valid();
	const auto textStyle = valid
		? leafStyle.textStyle
		: &_articleStyle->body;
	const auto lineHeight = (valid && leafStyle.lineHeight > 0)
		? leafStyle.lineHeight
		: std::max(textStyle->lineHeight, textStyle->font->height);
	return {
		.textStyle = textStyle,
		.lineHeight = lineHeight,
		.textFg = _inlineFieldTextColorOverride
			? _inlineFieldTextColorOverride->color()
			: (valid ? leafStyle.textColor : _articleStyle->textColor),
		.textMarkBg = valid
			? leafStyle.markBg
			: _articleStyle->textPalette.markBg->c,
		.align = valid ? leafStyle.align : style::al_left,
		.italic = valid ? leafStyle.italic : false,
	};
}

Widget::InlineFieldStyleKey Widget::inlineFieldStyleKey(
		const InlineFieldStyleData &data) const {
	const auto textStyle = data.textStyle
		? data.textStyle
		: &_articleStyle->body;
	return {
		.font = data.italic
			? textStyle->font->italic()
			: textStyle->font,
		.lineHeight = data.lineHeight,
		.textFg = data.textFg,
		.align = data.align,
	};
}

void Widget::ensureInlineFieldForSegment(int segmentIndex) {
	_revivedRetainedField = false;
	refreshInlineFieldTextColorOverride();
	const auto data = normalizedInlineFieldStyle(
		inlineFieldStyleForSegment(segmentIndex));
	const auto key = inlineFieldStyleKey(data);
	const auto mode = _state->activeFieldMode();
	const auto leaf = _state->activeLeafPath();
	const auto fieldLeafMismatch = leaf
		&& _fieldLeaf
		&& (*_fieldLeaf != *leaf);
	if (_activeFieldStyleKey
		&& leaf
		&& _fieldLeaf
		&& (*_fieldLeaf == *leaf)
		&& *_activeFieldStyleKey == key
		&& _fieldMode == mode) {
		return;
	}
	if (leaf) {
		if (auto revived = reviveRetainedLeafField(
				_historyIndex,
				*leaf,
				mode,
				key)) {
			_field = std::move(revived);
			_activeFieldStyleKey = key;
			_fieldMode = mode;
			_fieldLeaf = *leaf;
			refreshInlineFieldPlaceholderColor();
			_fieldUndoAvailable = _field->isUndoAvailable();
			_fieldRedoAvailable = _field->isRedoAvailable();
			_revivedRetainedField = true;
			clearFieldUndoRedoNoopState();
			return;
		}
	}
	const auto needsRecreate = !_activeFieldStyleKey
		|| (*_activeFieldStyleKey != key)
		|| (_fieldMode != mode)
		|| fieldLeafMismatch;
	if (!needsRecreate) {
		_activeFieldStyleKey = key;
		_fieldMode = mode;
		return;
	}
	const auto &cached = inlineFieldStyleFor(data);
	_activeFieldStyleKey = cached.key;
	_fieldMode = mode;
	recreateInlineField(*cached.style);
}

void Widget::setupInlineField() {
	if (_fieldMode == State::FieldMode::Rich) {
		const auto allowPremiumEmoji = [peer = _peer](
				not_null<DocumentData*> emoji) {
			return Data::AllowEmojiWithoutPremium(peer, emoji);
		};
		_field->setInstantViewEditorTagsEnabled(true);
		InitMessageFieldHandlers({
			.session = _session,
			.show = _show,
			.field = _field.get(),
			.customEmojiPaused = _customEmojiPaused,
			.allowPremiumEmoji = allowPremiumEmoji,
			.fieldStyle = &_field->st(),
			.linkValidator = ValidateInstantViewEditorLink,
		});
		Ui::Emoji::SuggestionsController::Init(
			_outer,
			_field.get(),
			_session,
			{
				.suggestCustomEmoji = true,
				.allowCustomWithoutPremium = allowPremiumEmoji,
			});
		auto messageFieldMimeHook = WrappedMessageFieldMimeHook(
			Ui::InputField::MimeDataHook(),
			_field.get());
		_field->setMimeDataHook([=,
				messageFieldMimeHook = std::move(messageFieldMimeHook)](
				not_null<const QMimeData*> data,
				Ui::InputField::MimeAction action) {
			return handleIvClipboardMime(data, action)
				|| (messageFieldMimeHook
					? messageFieldMimeHook(data, action)
					: false);
		});
	} else {
		_field->setInstantViewEditorTagsEnabled(false);
		_field->setInstantReplacesEnabled(
			rpl::single(false),
			rpl::single(false));
		_field->setMarkdownReplacesEnabled(
			rpl::single(Ui::MarkdownEnabledState{
				Ui::MarkdownDisabled()
			}));
	}
	_field->setDocumentMargin(0.);
	_field->setAdditionalMargins({});
	_field->setSubmitSettings(Ui::InputField::SubmitSettings::None);
	_field->setMaxHeight(std::numeric_limits<int>::max());
	refreshInlineFieldPlaceholderColor();
	const auto raw = _field->rawTextEdit();
	raw->installEventFilter(this);
	raw->viewport()->installEventFilter(this);
	_field->setExtendedContextMenu(_fieldContextMenuRequests.events());

	_field->heightChanges(
	) | rpl::on_next([=] {
		updateInlineFieldHeightOverride();
	}, _field->lifetime());
	_field->focusedChanges(
	) | rpl::on_next([=](bool focused) {
		if (!focused
			&& !_settingField
			&& !_trackingPointerPress
			&& !_inlineFieldExternalInteractionActive) {
			const auto committed = recordMutationTransaction([=] {
				return commitInlineField();
			});
			if (committed == ApplyResult::Changed) {
				refreshAfterInlineFieldCommit(committed);
			}
		}
	}, _field->lifetime());
	const auto field = QPointer<Ui::InputField>(_field.get());
	QObject::connect(
		raw->document(),
		&QTextDocument::contentsChange,
		_field.get(),
		[this, field](int, int, int) {
			if (!field || (_field.get() != field.data())) {
				return;
			}
			const auto hadRedo = _fieldRedoAvailable;
			const auto hadHistoryRedo
				= (_historyIndex + 1 < int(_history.size()));
			if (!_restoringHistory
				&& !_performingUndoRedo
				&& !_settingField
				&& !_suppressHistoryRedoInvalidation
				&& (hadRedo || hadHistoryRedo)) {
				truncateHistoryRedo();
			}
			if (!_restoringHistory && !_performingUndoRedo && !_settingField) {
				clearFieldUndoRedoNoopState();
			}
			crl::on_main(this, [=] {
				if (!field || (_field.get() != field.data())) {
					return;
				}
				_fieldUndoAvailable = field->isUndoAvailable();
				_fieldRedoAvailable = field->isRedoAvailable();
			});
		});
	_fieldUndoAvailable = _field->isUndoAvailable();
	_fieldRedoAvailable = _field->isRedoAvailable();

	hideInlineField();
}

void Widget::recreateInlineField(const style::InputField &st) {
	const auto text = _field->getTextWithTags();
	const auto cursor = _field->textCursor();
	const auto position = cursor.position();
	const auto anchor = cursor.anchor();
	const auto wasHidden = _field->isHidden();
	const auto hadFocus = _field->hasFocus();

	_settingField = true;
	_field = base::make_unique_q<Ui::InputField>(
		this,
		st,
		Ui::InputField::Mode::MultiLine,
		rpl::single(QString()));
	setupInlineField();
	refreshInlineFieldPlaceholder();
	const auto wasSuppressingHistoryRedoInvalidation
		= _suppressHistoryRedoInvalidation;
	_suppressHistoryRedoInvalidation = true;
	const auto suppressRedoInvalidation = gsl::finally([&] {
		_suppressHistoryRedoInvalidation
			= wasSuppressingHistoryRedoInvalidation;
	});
	_field->setTextWithTags(text, Ui::InputField::HistoryAction::Clear);
	auto restored = _field->textCursor();
	const auto size = int(_field->getLastText().size());
	const auto restoredAnchor = std::clamp(anchor, 0, size);
	const auto restoredPosition = std::clamp(position, 0, size);
	restored.setPosition(restoredAnchor);
	if (restoredPosition != restoredAnchor) {
		restored.setPosition(restoredPosition, QTextCursor::KeepAnchor);
	}
	_field->setTextCursor(restored);
	if (!wasHidden) {
		_field->show();
		_field->raise();
		if (hadFocus) {
			_field->setFocusFast();
		}
	}
	_fieldLeaf = std::nullopt;
	_settingField = false;
	_fieldUndoAvailable = _field->isUndoAvailable();
	_fieldRedoAvailable = _field->isRedoAvailable();
	clearFieldUndoRedoNoopState();
}

void Widget::ensureInlineFieldCreated() {
	if (_field) {
		return;
	}
	const auto &fieldStyle = inlineFieldStyleFor(
		Markdown::MarkdownArticleTextLeafStyle());
	_activeFieldStyleKey = fieldStyle.key;
	_fieldMode = State::FieldMode::Rich;
	_field = base::make_unique_q<Ui::InputField>(
		this,
		*fieldStyle.style,
		Ui::InputField::Mode::MultiLine,
		rpl::single(QString()));
	setupInlineField();
	clearFieldUndoRedoNoopState();
}

void Widget::refreshInlineFieldPlaceholder() {
	_field->setPlaceholder(rpl::single(_state->activePlaceholderText()));
	refreshInlineFieldPlaceholderColor();
}

void Widget::refreshInlineFieldPlaceholderColor() {
	auto color = activeQuotePlaceholderColor().value_or(
		_articleStyle->supplementaryTextColor->c);
	color.setAlphaF(color.alphaF() * 0.5);
	if (_inlineFieldPlaceholderColorOverride) {
		_inlineFieldPlaceholderColorOverride->update(color);
	} else {
		_inlineFieldPlaceholderColorOverride.emplace(color);
	}
	_field->setPlaceholderColorOverride(
		_inlineFieldPlaceholderColorOverride->color());
}

void Widget::setInlineFieldFromActiveState(int selectionFrom, int selectionTo) {
	ensureInlineFieldForSegment(_activeSegmentIndex);
	const auto revivedRetainedField = _revivedRetainedField;
	_revivedRetainedField = false;
	refreshInlineFieldPlaceholder();
	_settingField = true;
	const auto activeLeaf = _state->activeLeafPath();
	const auto preserveRetainedFieldSession = _restoringHistory
		&& (PreservingExternalFieldRestore == this)
		&& activeLeaf
		&& _fieldLeaf
		&& (*_fieldLeaf == *activeLeaf);
	auto cursorSelectionFrom = selectionFrom;
	auto cursorSelectionTo = selectionTo;
	auto trimmedLeft = 0;
	const auto trimLeft = !_state->codeBlockLanguage(
		_state->activeTextOrdinal()).has_value();
	const auto wasSuppressingHistoryRedoInvalidation
		= _suppressHistoryRedoInvalidation;
	_suppressHistoryRedoInvalidation = true;
	const auto suppressRedoInvalidation = gsl::finally([&] {
		_suppressHistoryRedoInvalidation
			= wasSuppressingHistoryRedoInvalidation;
	});
	if (preserveRetainedFieldSession) {
		_fieldLeaf = activeLeaf;
		_settingField = false;
		_fieldUndoAvailable = _field->isUndoAvailable();
		_fieldRedoAvailable = _field->isRedoAvailable();
		return;
	}
	const auto preserveRestoredRetainedField = [&](const TextWithTags &text) {
		const auto document = _field->rawTextEdit()->document();
		const auto matchingHistoryDirection = _restoringHistoryRedo
			&& (*_restoringHistoryRedo
				? (document->availableRedoSteps() > 0
					|| _field->isRedoAvailable())
				: (document->availableUndoSteps() > 0
					|| _field->isUndoAvailable()));
		return revivedRetainedField
			&& _restoringHistory
			&& activeLeaf
			&& _fieldLeaf
			&& (*_fieldLeaf == *activeLeaf)
			&& (matchingHistoryDirection || (_field->getTextWithTags() == text));
	};
	const auto finishWithRetainedField = [&] {
		_fieldLeaf = activeLeaf;
		_settingField = false;
		_fieldUndoAvailable = _field->isUndoAvailable();
		_fieldRedoAvailable = _field->isRedoAvailable();
	};
	const auto resetFieldHistory = !activeLeaf
		|| !_fieldLeaf
		|| (*_fieldLeaf != *activeLeaf);
	if (_state->activeFieldMode() == State::FieldMode::Raw) {
		const auto trimmed = TrimInlineFieldText(
			{ _state->activeRawText(), {} },
			trimLeft);
		if (preserveRestoredRetainedField(trimmed.text)) {
			_article->clearEditableHeightOverride();
			finishWithRetainedField();
			return;
		}
		if (resetFieldHistory || (_field->getTextWithTags() != trimmed.text)) {
			_field->setTextWithTags(
				trimmed.text,
				Ui::InputField::HistoryAction::Clear);
		}
		trimmedLeft = trimmed.left;
		_article->clearEditableHeightOverride();
	} else {
		const auto activeText = ConvertRichTextToEditorTags(
			_state->activeText());
		const auto trimmed = TrimInlineFieldText(activeText.text, trimLeft);
		if (preserveRestoredRetainedField(trimmed.text)) {
			finishWithRetainedField();
			return;
		}
		if (resetFieldHistory || (_field->getTextWithTags() != trimmed.text)) {
			_field->setTextWithTags(
				trimmed.text,
				Ui::InputField::HistoryAction::Clear);
		}
		cursorSelectionFrom = MapRichTextOffsetToEditorOffset(
			activeText.replacements,
			selectionFrom);
		cursorSelectionTo = MapRichTextOffsetToEditorOffset(
			activeText.replacements,
			selectionTo);
		trimmedLeft = trimmed.left;
	}
	cursorSelectionFrom -= trimmedLeft;
	cursorSelectionTo -= trimmedLeft;
	auto cursor = _field->textCursor();
	const auto size = int(_field->getLastText().size());
	const auto from = std::clamp(cursorSelectionFrom, 0, size);
	const auto to = std::clamp(cursorSelectionTo, 0, size);
	cursor.setPosition(from);
	if (to != from) {
		cursor.setPosition(to, QTextCursor::KeepAnchor);
	}
	_field->setTextCursor(cursor);
	_fieldLeaf = _state->activeLeafPath();
	_settingField = false;
	_fieldUndoAvailable = _field->isUndoAvailable();
	_fieldRedoAvailable = _field->isRedoAvailable();
	clearFieldUndoRedoNoopState();
}

void Widget::activateTextOrdinal(int ordinal, int cursorOffset) {
	activateTextOrdinal(ordinal, cursorOffset, cursorOffset);
}

void Widget::activateTextOrdinal(
		int ordinal,
		int selectionFrom,
		int selectionTo) {
	const auto targetLeaf = [&]() -> std::optional<State::LeafPath> {
		const auto &nodes = _state->textNodes();
		return (ordinal >= 0 && ordinal < int(nodes.size()))
			? std::make_optional(nodes[ordinal].leaf)
			: std::nullopt;
	}();
	if (targetLeaf
		&& _fieldLeaf
		&& (*_fieldLeaf != *targetLeaf)) {
		retainActiveLeafField();
	}
	if (!_state->setActiveTextByOrdinal(ordinal)) {
		return;
	}
	_boundarySelectionOrigin = std::nullopt;
	_activeOrdinal = ordinal;
	_pendingOrdinal = -1;
	_pendingCursorOffset = 0;

	const auto previousSegmentIndex = _activeSegmentIndex;
	const auto segmentIndex = segmentIndexForEditableOrdinal(ordinal);
	if (segmentIndex < 0) {
		_activeSegmentIndex = -1;
		_pendingOrdinal = ordinal;
		_pendingCursorOffset = selectionTo;
		hideInlineField();
		return;
	}

	if (_article && previousSegmentIndex != segmentIndex) {
		_article->clearEditableHeightOverride();
	}
	if (previousSegmentIndex != segmentIndex) {
		clearDisplayMathEditSession();
	}
	_activeSegmentIndex = segmentIndex;
	if (_article->segmentIsDisplayMath(_activeSegmentIndex)) {
		if (!_activeSegmentIsDisplayMath) {
			const auto blockRect = _article->displayMathBlockRect(
				_activeSegmentIndex);
			_activeSegmentIsDisplayMath = true;
			_activeDisplayMathBaselineHeight = std::max(blockRect.height(), 1);
		}
	} else {
		clearDisplayMathEditSession();
	}
	setInlineFieldFromActiveState(selectionFrom, selectionTo);
	_field->show();
	syncInlineFieldGeometry();
	updateInlineFieldHeightOverride();
	syncArticleVisibleTopBottom();
	revealActiveInlineField();
	_field->raise();
	_field->setFocusFast();
}

QRect Widget::activeInlineFieldRevealRect() const {
	const auto raw = _field->rawTextEdit();
	const auto cursor = _field->textCursor();
	auto positionCursor = cursor;
	positionCursor.setPosition(cursor.position());
	auto revealRect = raw->cursorRect(positionCursor);
	if (cursor.hasSelection()) {
		auto anchorCursor = cursor;
		anchorCursor.setPosition(cursor.anchor());
		revealRect = revealRect.united(raw->cursorRect(anchorCursor));
	}
	if (!revealRect.isValid() || revealRect.isEmpty()) {
		return _field->rect();
	}
	revealRect.moveTopLeft(
		raw->viewport()->mapTo(_field, revealRect.topLeft()));
	return revealRect;
}

QRect Widget::mapFieldLocalRectToScrollContent(
		QWidget *inner,
		QRect rect) const {
	rect.moveTopLeft(_field->mapTo(inner, rect.topLeft()));
	return rect;
}

void Widget::revealActiveInlineField() {
	if (_field->isHidden() || _activeSegmentIndex < 0) {
		return;
	}
	if (_article->revealSegment(_activeSegmentIndex)) {
		syncInlineFieldGeometry();
		if (_field->isHidden()) {
			return;
		}
	}
	const auto scrollIn = [&](auto &&scroll) {
		if (const auto inner = scroll->widget()) {
			const auto localRect = mapFieldLocalRectToScrollContent(
				inner,
				activeInlineFieldRevealRect());
			scroll->scrollToY(
				localRect.y(),
				localRect.y() + localRect.height());
		}
	};
	for (auto parent = parentWidget(); parent; parent = parent->parentWidget()) {
		if (const auto scroll = dynamic_cast<Ui::ScrollArea*>(parent)) {
			scrollIn(scroll);
			return;
		}
		if (const auto scroll = dynamic_cast<Ui::ElasticScroll*>(parent)) {
			scrollIn(scroll);
			return;
		}
	}
}

void Widget::activateTrailingParagraph() {
	recordMutationTransaction([&] {
		const auto committed = commitInlineField();
		if (committed == ApplyResult::Failed) {
			return MutationTransactionResult{
				.committed = committed,
				.failed = true,
			};
		}
		const auto ordinal = _state->ensureTrailingParagraphActive();
		if (!ordinal) {
			showLastLimitToast();
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		refreshPreparedContent();
		activateTextOrdinal(*ordinal, _state->activeText().text.size());
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
}

void Widget::revertInlineFieldToState() {
	if (_field->isHidden() || _activeSegmentIndex < 0) {
		return;
	}
	const auto cursor = _field->textCursor();
	setInlineFieldFromActiveState(cursor.anchor(), cursor.position());
	syncInlineFieldGeometry();
	updateInlineFieldHeightOverride();
}

std::optional<State::ActiveTextInsertContext>
Widget::activeTextInsertContext() const {
	if (_settingField
		|| _field->isHidden()
		|| (_activeSegmentIndex < 0)
		|| (_state->activeFieldMode() == State::FieldMode::Raw)) {
		return std::nullopt;
	}
	auto full = ConvertEditorTagsToRichText(_field->getTextWithAppliedMarkdown());
	const auto cursor = _field->textCursor();
	auto from = richOffsetForFieldOffset(full, cursor.selectionStart());
	auto till = richOffsetForFieldOffset(full, cursor.selectionEnd());
	from = std::clamp(from, 0, int(full.text.size()));
	till = std::clamp(till, from, int(full.text.size()));
	auto before = TextWithEntities();
	auto selected = TextWithEntities();
	auto after = std::move(full);
	if (from > 0) {
		[[maybe_unused]] const auto cut = TextUtilities::CutPart(
			before,
			after,
			from);
	}
	if (till > from) {
		[[maybe_unused]] const auto cut = TextUtilities::CutPart(
			selected,
			after,
			till - from);
	}
	return State::ActiveTextInsertContext{
		.before = std::move(before),
		.selected = std::move(selected),
		.after = std::move(after),
	};
}

bool Widget::handleIvClipboardMime(
		not_null<const QMimeData*> data,
		Ui::InputField::MimeAction action) {
	const auto modifiers = QApplication::keyboardModifiers();
	if ((modifiers & Qt::ControlModifier)
		&& (modifiers & Qt::ShiftModifier)) {
		return false;
	}
	if (!ClipboardPasteInsertContext(activeTextInsertContext())) {
		return false;
	}
	const auto clipboardData = ClipboardDataFromMimeData(data.get());
	if (!clipboardData) {
		return false;
	} else if (action == Ui::InputField::MimeAction::Check) {
		return true;
	}
	crl::on_main(this, [=, clipboardData = *clipboardData] {
		pasteStructuredClipboardData(clipboardData);
	});
	return true;
}

int Widget::richOffsetForFieldOffset(
		const TextWithEntities &text,
		int offset) const {
	const auto replacements = ConvertRichTextToEditorTags(text).replacements;
	return std::clamp(
		MapEditorOffsetToRichOffset(replacements, offset),
		0,
		int(text.text.size()));
}

ApplyResult Widget::applyFieldTextToState() {
	if (_settingField || _field->isHidden()) {
		return ApplyResult::Unchanged;
	}
	if (_state->activeFieldMode() == State::FieldMode::Raw) {
		return _state->applyActiveRawText(_field->getLastText());
	}
	const auto text = _field->getTextWithAppliedMarkdown();
	return _state->applyActiveText(ConvertEditorTagsToRichText(text));
}

bool Widget::showLastLimitToast() {
	if (_showLimitToast) {
		if (const auto error = _state->lastLimitError()) {
			_showLimitToast(*error);
			return true;
		}
	}
	return false;
}

void Widget::hideInlineField() {
	if (_field->isHidden()) {
		return;
	}
	const auto wasSettingField = _settingField;
	_settingField = true;
	const auto guard = gsl::finally([&] {
		_settingField = wasSettingField;
	});
	_field->hide();
}

void Widget::activateTextOrdinalAtEnd(int ordinal) {
	if (!_state->setActiveTextByOrdinal(ordinal)) {
		return;
	}
	activateTextOrdinal(ordinal, _state->activeTextLength());
}

bool Widget::handleFieldKey(QKeyEvent *e) {
	if (_field->isHidden()) {
		return false;
	}
	const auto key = e->key();
	if (key == Qt::Key_Escape) {
		hideInlineFieldAndRefresh();
		e->accept();
		return true;
	}
	const auto modifiers = e->modifiers()
		& ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
	if (modifiers != Qt::NoModifier) {
		return false;
	}
	const auto cursor = _field->textCursor();
	if (cursor.hasSelection()) {
		return false;
	}
	const auto atStart = cursor.atStart();
	const auto atEnd = cursor.atEnd();
	auto handled = false;
	if (atStart
		&& (key == Qt::Key_Left
			|| key == Qt::Key_Up
			|| key == Qt::Key_PageUp)) {
		handled = moveBoundary(false, false);
	} else if (atEnd && key == Qt::Key_Down) {
		recordMutationTransaction([&] {
			const auto committed = commitInlineField();
			if (committed == ApplyResult::Failed) {
				handled = true;
				return MutationTransactionResult{
					.committed = committed,
					.failed = true,
				};
			} else if (const auto target
				= _state->removeTemporaryDownParagraphAndMove();
				target.action != State::BoundaryTarget::Action::None) {
				refreshPreparedContent();
				switch (target.action) {
				case State::BoundaryTarget::Action::Text:
					activateTextOrdinal(target.textOrdinal, 0);
					break;
				case State::BoundaryTarget::Action::StructuralSelection:
					_boundarySelectionOrigin = std::nullopt;
					_selection = {};
					_selectionEndpoints = {};
					_articleSelectionDrag = {};
					setStructuralSelection(target.structuralSelection);
					_pendingOrdinal = -1;
					_pendingCursorOffset = 0;
					hideInlineField();
					clearInlineFieldEditSession();
					update();
					break;
				case State::BoundaryTarget::Action::None:
				case State::BoundaryTarget::Action::RemoveActiveOwner:
					break;
				}
				handled = true;
				return MutationTransactionResult{
					.committed = committed,
					.changed = true,
				};
			} else if (const auto target = _state->moveActiveSpecialBlockDown()) {
				refreshPreparedContent();
				activateTextOrdinal(*target, 0);
				handled = true;
				return MutationTransactionResult{
					.committed = committed,
					.changed = true,
				};
			}
			auto mutated = false;
			if (_state->lastLimitError()) {
				handled = moveBoundaryAfterCommit(
					committed,
					true,
					false,
					&mutated);
				if (!handled) {
					handled = true;
				}
			} else {
				handled = moveBoundaryAfterCommit(
					committed,
					true,
					true,
					&mutated);
			}
			return MutationTransactionResult{
				.committed = committed,
				.changed = mutated || (committed == ApplyResult::Changed),
			};
		});
	} else if (atEnd
		&& (key == Qt::Key_Right
			|| key == Qt::Key_PageDown)) {
		handled = moveBoundary(true, true);
	} else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
		recordMutationTransaction([&] {
			const auto committed = commitInlineField();
			if (committed == ApplyResult::Failed) {
				handled = true;
				return MutationTransactionResult{
					.committed = committed,
					.failed = true,
				};
			} else if (const auto target = _state->handleActiveListEnter()) {
				refreshPreparedContent();
				activateTextOrdinal(*target, 0);
				handled = true;
				return MutationTransactionResult{
					.committed = committed,
					.changed = true,
				};
			} else if (const auto target = _state->handleActiveHeadingEnter()) {
				refreshPreparedContent();
				activateTextOrdinal(*target, 0);
				handled = true;
				return MutationTransactionResult{
					.committed = committed,
					.changed = true,
				};
			} else if (_state->lastLimitError()) {
				showLastLimitToast();
				handled = true;
			}
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		});
	} else if (atStart && key == Qt::Key_Backspace) {
		handled = removeBoundaryOwner(false);
	} else if (atEnd && key == Qt::Key_Delete) {
		handled = removeBoundaryOwner(true);
	}
	if (handled) {
		e->accept();
	}
	return handled;
}

bool Widget::handleTabNavigation(QKeyEvent *e) {
	const auto key = e->key();
	if (key != Qt::Key_Tab && key != Qt::Key_Backtab) {
		return false;
	}
	const auto modifiers = e->modifiers()
		& ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
	if (modifiers != Qt::NoModifier && modifiers != Qt::ShiftModifier) {
		return false;
	}
	const auto forward = (key != Qt::Key_Backtab)
		&& (modifiers != Qt::ShiftModifier);
	if (!moveTabBoundary(forward)) {
		return false;
	}
	e->accept();
	return true;
}

bool Widget::moveBoundary(bool forward, bool allowTrailing) {
	const auto target = forward
		? _state->nextEditableOrdinal()
		: _state->previousEditableOrdinal();
	const auto addTrailing = forward
		&& allowTrailing
		&& !target
		&& !_state->isActiveTopLevelParagraph();
	if (!target && !addTrailing) {
		return false;
	}
	auto handled = false;
	recordMutationTransaction([&] {
		const auto committed = commitInlineField();
		if (committed == ApplyResult::Failed) {
			handled = true;
			return MutationTransactionResult{
				.committed = committed,
				.failed = true,
			};
		}
		if (target) {
			if (committed == ApplyResult::Changed) {
				refreshAfterInlineFieldCommit(committed);
			}
			if (forward) {
				activateTextOrdinal(*target, 0);
			} else {
				activateTextOrdinalAtEnd(*target);
			}
			handled = true;
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		const auto ordinal = _state->ensureTrailingParagraphActive();
		if (!ordinal) {
			handled = forward
				&& allowTrailing
				&& _state->lastLimitError().has_value();
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		refreshPreparedContent();
		activateTextOrdinal(*ordinal, 0);
		handled = true;
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
	return handled;
}

bool Widget::moveBoundaryAfterCommit(
		ApplyResult committed,
		bool forward,
		bool allowTrailing,
		bool *mutated) {
	if (mutated) {
		*mutated = false;
	}
	const auto target = forward
		? _state->nextEditableOrdinal()
		: _state->previousEditableOrdinal();
	if (target) {
		if (committed == ApplyResult::Changed) {
			refreshAfterInlineFieldCommit(committed);
		}
		if (forward) {
			activateTextOrdinal(*target, 0);
		} else {
			activateTextOrdinalAtEnd(*target);
		}
		return true;
	}
	if (forward && allowTrailing && !_state->isActiveTopLevelParagraph()) {
		const auto ordinal = _state->ensureTrailingParagraphActive();
		if (!ordinal) {
			return _state->lastLimitError().has_value();
		}
		if (mutated) {
			*mutated = true;
		}
		refreshPreparedContent();
		activateTextOrdinal(*ordinal, 0);
		return true;
	}
	return false;
}

bool Widget::moveTabBoundary(bool forward) {
	auto handled = false;
	const auto target = forward
		? _state->nextEditableOrdinal()
		: _state->previousEditableOrdinal();
	if (!target && (!forward || _state->isActiveTopLevelParagraph())) {
		return false;
	}
	recordMutationTransaction([&] {
		auto committed = ApplyResult::Unchanged;
		if (!_field->isHidden()) {
			committed = commitInlineField();
			if (committed == ApplyResult::Failed) {
				handled = true;
				return MutationTransactionResult{
					.committed = committed,
					.failed = true,
				};
			}
		}
		if (target) {
			clearSelection();
			if (committed == ApplyResult::Changed) {
				refreshAfterInlineFieldCommit(committed);
			}
			activateTextOrdinalAtEnd(*target);
			handled = true;
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		clearSelection();
		const auto ordinal = _state->ensureTrailingParagraphActive();
		if (!ordinal) {
			handled = _state->lastLimitError().has_value();
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		}
		refreshPreparedContent();
		activateTextOrdinalAtEnd(*ordinal);
		handled = true;
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
	return handled;
}

bool Widget::removeBoundaryOwner(bool forward) {
	auto handled = false;
	recordMutationTransaction([&] {
		const auto committed = commitInlineField();
		if (committed == ApplyResult::Failed) {
			handled = true;
			return MutationTransactionResult{
				.committed = committed,
				.failed = true,
			};
		}
		const auto target = _state->activeBoundaryTarget(forward);
		using BoundaryAction = State::BoundaryTarget::Action;
		switch (target.action) {
		case BoundaryAction::RemoveActiveOwner: {
			_boundarySelectionOrigin = std::nullopt;
			const auto adjacent = _state->removeActiveOwnerAndSelectAdjacent(
				forward);
			hideInlineField();
			clearInlineFieldEditSession();
			refreshPreparedContent();
			if (adjacent) {
				if (forward) {
					activateTextOrdinal(*adjacent, 0);
				} else {
					activateTextOrdinalAtEnd(*adjacent);
				}
			} else {
				activateInitialNode();
			}
			handled = true;
			return MutationTransactionResult{
				.committed = committed,
				.changed = true,
			};
		}
		case BoundaryAction::Text:
			_boundarySelectionOrigin = std::nullopt;
			if (committed == ApplyResult::Changed) {
				refreshAfterInlineFieldCommit(committed);
			}
			if (forward) {
				activateTextOrdinal(target.textOrdinal, 0);
			} else {
				activateTextOrdinalAtEnd(target.textOrdinal);
			}
			handled = true;
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		case BoundaryAction::StructuralSelection:
			setStructuralSelection(
				target.structuralSelection,
				BoundarySelectionOrigin{
					.ordinal = _activeOrdinal,
					.forward = forward,
				});
			_selection = {};
			_selectionEndpoints = {};
			_articleSelectionDrag = {};
			_pendingOrdinal = -1;
			_pendingCursorOffset = 0;
			hideInlineField();
			clearInlineFieldEditSession();
			refreshAfterInlineFieldCommit(committed);
			update();
			handled = true;
			return MutationTransactionResult{
				.committed = committed,
				.changed = (committed == ApplyResult::Changed),
			};
		case BoundaryAction::None: {
			auto mutated = false;
			handled = moveBoundaryAfterCommit(
				committed,
				forward,
				forward,
				&mutated);
			return MutationTransactionResult{
				.committed = committed,
				.changed = mutated || (committed == ApplyResult::Changed),
			};
		}
		}
		Unexpected("Boundary action.");
	});
	return handled;
}

void Widget::ensurePendingActivation() {
	if (_pendingOrdinal < 0) {
		_activeSegmentIndex = (_activeOrdinal >= 0)
			? segmentIndexForEditableOrdinal(_activeOrdinal)
			: _article->firstEditableSegmentIndex();
		return;
	}
	const auto ordinal = _pendingOrdinal;
	const auto cursorOffset = _pendingCursorOffset;
	_pendingOrdinal = -1;
	_pendingCursorOffset = 0;
	activateTextOrdinal(ordinal, cursorOffset);
}

void Widget::updateInlineFieldHeightOverride() {
	if (_settingField
		|| _field->isHidden()
		|| _activeOrdinal < 0
		|| !_article) {
		return;
	} else if (_syncingInlineFieldGeometry) {
		_pendingHeightOverrideUpdate = true;
		return;
	}
	if (_article->editableIndexForSegment(_activeSegmentIndex) < 0) {
		_article->clearEditableHeightOverride();
		return;
	}
	const auto segmentRect = fieldOuterRectForSegment(_activeSegmentIndex);
	auto height = segmentRect.isEmpty()
		? _field->height()
		: std::max(_field->geometry().bottom() + 1 - segmentRect.y(), 1);
	if (_activeSegmentIsDisplayMath) {
		const auto blockRect = _article->displayMathBlockRect(
			_activeSegmentIndex).translated(articleTopLeft());
		if (!blockRect.isEmpty()) {
			height = std::max(
				_field->geometry().bottom() + 1 - blockRect.y(),
				1);
		}
		height = std::max(height, _activeDisplayMathBaselineHeight);
	}
	_article->setEditableHeightOverrideForSegment(_activeSegmentIndex, height);
	resizeToWidth(std::max(widthNoMargins(), 1));
	update();
}

void Widget::clearDisplayMathEditSession() {
	_activeSegmentIsDisplayMath = false;
	_activeDisplayMathBaselineHeight = 0;
}

void Widget::clearInlineFieldEditSession() {
	clearDisplayMathEditSession();
	if (_article) {
		_article->clearEditableHeightOverride();
	}
	if (!_field->isHidden()
		|| !_fieldLeaf) {
		return;
	}
	const auto activeLeaf = _state->activeLeafPath();
	if (!activeLeaf || (*activeLeaf != *_fieldLeaf)) {
		const auto &fieldStyle = inlineFieldStyleFor(
			Markdown::MarkdownArticleTextLeafStyle());
		_activeFieldStyleKey = fieldStyle.key;
		_fieldMode = State::FieldMode::Rich;
		recreateInlineField(*fieldStyle.style);
		return;
	}
	retainActiveLeafField();
	const auto &fieldStyle = inlineFieldStyleFor(
		Markdown::MarkdownArticleTextLeafStyle());
	_activeFieldStyleKey = fieldStyle.key;
	_fieldMode = State::FieldMode::Rich;
	recreateInlineField(*fieldStyle.style);
}

Widget::HistoryViewState Widget::captureHistoryViewState() const {
	auto result = HistoryViewState();
	if (!_field->isHidden()) {
		const auto leaf = _state->activeLeafPath();
		if (!leaf) {
			return result;
		}
		const auto cursor = _field->textCursor();
		const auto trimLeft = !_state->codeBlockLanguage(
			_state->activeTextOrdinal()).has_value();
		auto anchorOffset = 0;
		auto cursorOffset = 0;
		if (_state->activeFieldMode() == State::FieldMode::Raw) {
			const auto trimmed = TrimInlineFieldText(
				{ _state->activeRawText(), {} },
				trimLeft);
			const auto size = int(_state->activeRawText().size());
			anchorOffset = std::clamp(cursor.anchor() + trimmed.left, 0, size);
			cursorOffset = std::clamp(
				cursor.position() + trimmed.left,
				0,
				size);
		} else {
			const auto activeText = ConvertRichTextToEditorTags(
				_state->activeText());
			const auto trimmed = TrimInlineFieldText(activeText.text, trimLeft);
			const auto size = int(_state->activeText().text.size());
			anchorOffset = std::clamp(
				MapEditorOffsetToRichOffset(
					activeText.replacements,
					cursor.anchor() + trimmed.left),
				0,
				size);
			cursorOffset = std::clamp(
				MapEditorOffsetToRichOffset(
					activeText.replacements,
					cursor.position() + trimmed.left),
				0,
				size);
		}
		result.leafSelection = HistoryLeafSelection{
			.leaf = *leaf,
			.anchorOffset = anchorOffset,
			.cursorOffset = cursorOffset,
		};
	} else if (hasStructuralSelection()) {
		result.structuralSelection = _structuralSelection;
		result.boundarySelectionOrigin = _boundarySelectionOrigin;
	}
	return result;
}

Widget::HistoryEntry Widget::captureHistoryEntry() const {
	return {
		.snapshot = _state->snapshot(),
		.viewState = captureHistoryViewState(),
	};
}

void Widget::restoreHistoryEntry(const HistoryEntry &entry) {
	hideInlineField();
	clearInlineFieldEditSession();
	if (_article && (_horizontalScrollDrag != HorizontalScrollDrag::None)) {
		_article->endHorizontalScroll();
	}
	_selection = {};
	_selectionEndpoints = {};
	setStructuralSelection({});
	_articleSelectionDrag = {};
	_pendingOrdinal = -1;
	_pendingCursorOffset = 0;
	_trackingPointerPress = false;
	_horizontalScrollLock = std::nullopt;
	_pressedControl = {};
	_pressedControlPoint = std::nullopt;
	_horizontalScrollDrag = HorizontalScrollDrag::None;
	_pendingTouchHorizontalScrollPoint = std::nullopt;

	const auto wasRestoring = _restoringHistory;
	_restoringHistory = true;
	const auto guard = gsl::finally([&] {
		_restoringHistory = wasRestoring;
	});

	_state->restoreSnapshot(entry.snapshot);
	refreshPreparedContent();

	if (const auto &selection = entry.viewState.structuralSelection) {
		_activeOrdinal = _state->activeTextOrdinal();
		_activeSegmentIndex = -1;
		_fieldLeaf = std::nullopt;
		clearDisplayMathEditSession();
		setStructuralSelection(
			*selection,
			entry.viewState.boundarySelectionOrigin);
		hideInlineField();
		update();
		return;
	}
	if (const auto &leafSelection = entry.viewState.leafSelection) {
		const auto ordinal = _state->textOrdinalForLeafPath(leafSelection->leaf);
		if (ordinal >= 0) {
			activateTextOrdinal(
				ordinal,
				leafSelection->anchorOffset,
				leafSelection->cursorOffset);
			return;
		}
	}
	_activeOrdinal = _state->activeTextOrdinal();
	_activeSegmentIndex = -1;
	_fieldLeaf = std::nullopt;
	clearDisplayMathEditSession();
	hideInlineField();
	update();
}

bool Widget::mutationTransactionChanged(bool changed) {
	return changed;
}

bool Widget::mutationTransactionChanged(ApplyResult result) {
	return (result == ApplyResult::Changed);
}

bool Widget::mutationTransactionChanged(
		const MutationTransactionResult &result) {
	return result.changed;
}

void Widget::finishMutationTransaction(
		const HistoryEntry &before,
		bool changed,
		int beforeHistoryIndex,
		uint64 beforeRetainToken) {
	if (!changed) {
		return;
	}
	const auto after = captureHistoryEntry();
	if (SnapshotEquals(before.snapshot, after.snapshot)
		&& (before.viewState == after.viewState)) {
		return;
	}
	truncateHistoryRedo();
	_history.push_back(after);
	_historyIndex = int(_history.size()) - 1;
	moveRetainedLeafFields(
		beforeHistoryIndex,
		_historyIndex,
		beforeRetainToken);
}

void Widget::retainActiveLeafField() {
	if (!_field) {
		ensureInlineFieldCreated();
		return;
	} else if (!_fieldLeaf
		|| !_activeFieldStyleKey) {
		return;
	}
	const auto leaf = *_fieldLeaf;
	if (_state->textOrdinalForLeafPath(leaf) < 0) {
		return;
	}
	const auto wasSettingField = _settingField;
	_settingField = true;
	_field->hide();
	_settingField = wasSettingField;
	const auto historyIndex = _retainingFieldHistoryIndexOverride.value_or(
		_historyIndex);
	const auto &fieldStyle = inlineFieldStyleFor(
		Markdown::MarkdownArticleTextLeafStyle());
	auto replacement = base::make_unique_q<Ui::InputField>(
		this,
		*fieldStyle.style,
		Ui::InputField::Mode::MultiLine,
		rpl::single(QString()));
	auto retained = RetainedLeafField{
		.historyIndex = historyIndex,
		.retainToken = ++_retainedLeafFieldToken,
		.leaf = leaf,
		.mode = _fieldMode,
		.styleKey = _activeFieldStyleKey,
	};
	retained.field = std::move(_field);
	_field = std::move(replacement);
	_activeFieldStyleKey = std::nullopt;
	_fieldMode = State::FieldMode::Rich;
	_fieldLeaf = std::nullopt;
	setupInlineField();
	clearFieldUndoRedoNoopState();
	for (auto i = _retainedLeafFields.begin(); i != _retainedLeafFields.end();) {
		if ((i->historyIndex == retained.historyIndex)
			&& (i->leaf == retained.leaf)
			&& (i->mode == retained.mode)
			&& (i->styleKey == retained.styleKey)) {
			i = _retainedLeafFields.erase(i);
		} else {
			++i;
		}
	}
	_retainedLeafFields.push_back(std::move(retained));
	pruneRetainedLeafFields();
}

base::unique_qptr<Ui::InputField> Widget::reviveRetainedLeafField(
		int historyIndex,
		const State::LeafPath &leaf,
		State::FieldMode mode,
		const InlineFieldStyleKey &styleKey) {
	for (auto i = int(_retainedLeafFields.size()) - 1; i >= 0; --i) {
		if ((_retainedLeafFields[i].historyIndex == historyIndex)
			&& (_retainedLeafFields[i].leaf == leaf)
			&& (_retainedLeafFields[i].mode == mode)
			&& _retainedLeafFields[i].styleKey
			&& (*_retainedLeafFields[i].styleKey == styleKey)) {
			auto result = std::move(_retainedLeafFields[i].field);
			_retainedLeafFields.erase(_retainedLeafFields.begin() + i);
			return result;
		}
	}
	return {};
}

void Widget::pruneRetainedLeafFields() {
	for (auto i = _retainedLeafFields.begin(); i != _retainedLeafFields.end();) {
		if (!i->field) {
			i = _retainedLeafFields.erase(i);
		} else {
			++i;
		}
	}
	while (int(_retainedLeafFields.size()) > kRetainedLeafFieldLimit) {
		_retainedLeafFields.erase(_retainedLeafFields.begin());
	}
}

void Widget::removeRetainedLeafFieldsAfter(int historyIndex) {
	for (auto i = _retainedLeafFields.begin(); i != _retainedLeafFields.end();) {
		if (i->historyIndex > historyIndex) {
			i = _retainedLeafFields.erase(i);
		} else {
			++i;
		}
	}
}

void Widget::moveRetainedLeafFields(
		int fromHistoryIndex,
		int toHistoryIndex,
		uint64 afterRetainToken) {
	if (fromHistoryIndex == toHistoryIndex) {
		return;
	}
	for (auto &retained : _retainedLeafFields) {
		if ((retained.historyIndex == fromHistoryIndex)
			&& (retained.retainToken > afterRetainToken)) {
			retained.historyIndex = toHistoryIndex;
		}
	}
}

void Widget::refreshAfterInlineFieldCommit(ApplyResult committed) {
	switch ((committed == ApplyResult::Changed)
		? _state->lastPreparedMutationKind()
		: PreparedMutationKind::None) {
	case PreparedMutationKind::LeafOnly:
		refreshPreparedLeafAtActiveSource();
		break;
	case PreparedMutationKind::FullRebuild:
		refreshPreparedContent();
		break;
	case PreparedMutationKind::None:
		relayoutCurrentContent();
		break;
	}
}

void Widget::ensureArticleLayoutForInlineField(int width) {
	if (!_article || width <= 0) {
		return;
	}
	_articleHeight = _article->resizeGetHeight(articleWidth(width));
}

void Widget::syncArticleVisibleTopBottom() {
	if (!_article) {
		return;
	}
	const auto articleTop = articleTopLeft().y();
	_article->setVisibleTopBottom(
		_visibleRange.top - articleTop,
		_visibleRange.bottom - articleTop);
}

void Widget::syncInlineFieldGeometry(int width) {
	if (_field->isHidden() || width <= 0) {
		return;
	}
	ensureArticleLayoutForInlineField(width);
	if (_activeSegmentIndex >= 0) {
		ensureInlineFieldForSegment(_activeSegmentIndex);
	}
	const auto segmentRect = fieldOuterRectForSegment(_activeSegmentIndex);
	if (segmentRect.isEmpty()) {
		_pendingOrdinal = _activeOrdinal;
		_pendingCursorOffset = _field->textCursor().position();
		hideInlineField();
		_article->clearEditableHeightOverride();
		return;
	}
	const auto margins = _field->fullTextMargins();
	const auto left = segmentRect.x() - margins.left();
	const auto top = segmentRect.y() - margins.top();
	const auto fieldWidth = std::max(
		segmentRect.width() + margins.left() + margins.right(),
		1);
	_syncingInlineFieldGeometry = true;
	_field->resizeToWidth(fieldWidth);
	const auto fieldHeight = FieldNaturalHeight(_field.get());
	_field->setGeometryToLeft(left, top, fieldWidth, fieldHeight, width);
	_field->raise();
	_syncingInlineFieldGeometry = false;
	if (_pendingHeightOverrideUpdate) {
		_pendingHeightOverrideUpdate = false;
		updateInlineFieldHeightOverride();
	}
}

void Widget::setStructuralSelection(
		Markdown::PreparedEditSelection selection,
		std::optional<BoundarySelectionOrigin> origin) {
	_structuralSelection = std::move(selection);
	_boundarySelectionOrigin = std::move(origin);
}

void Widget::clearSelection() {
	const auto changed = !_selection.empty()
		|| _selectionEndpoints.from.valid()
		|| _selectionEndpoints.to.valid()
		|| hasStructuralSelection()
		|| _articleSelectionDrag.active;
	_selection = {};
	_selectionEndpoints = {};
	setStructuralSelection({});
	_articleSelectionDrag = {};
	if (changed) {
		update();
	}
}

void Widget::clearTextSelection() {
	const auto changed = !_selection.empty()
		|| _selectionEndpoints.from.valid()
		|| _selectionEndpoints.to.valid()
		|| (_articleSelectionDrag.active
			&& _articleSelectionDrag.mode == DragSelectionMode::Text);
	_selection = {};
	_selectionEndpoints = {};
	if (_articleSelectionDrag.mode == DragSelectionMode::Text) {
		finishArticleSelection();
	} else {
		_articleSelectionDrag.textSegment = -1;
		_articleSelectionDrag.textOffset = 0;
	}
	if (changed) {
		update();
	}
}

void Widget::clearStructuralSelection() {
	const auto changed = hasStructuralSelection()
		|| (_articleSelectionDrag.active
			&& _articleSelectionDrag.mode == DragSelectionMode::Structural);
	setStructuralSelection({});
	if (_articleSelectionDrag.mode == DragSelectionMode::Structural) {
		finishArticleSelection();
	}
	if (changed) {
		update();
	}
}

bool Widget::hasStructuralSelection() const {
	return !_structuralSelection.empty();
}

void Widget::startArticleSelection(
		QPoint pressPoint,
		const Markdown::MarkdownArticleHitTestResult &hit,
		const PreparedEditHit &editHit,
		bool fromField,
		bool startedBelow) {
	const auto isTextHit = hit.valid()
		&& !hit.codeHeaderCopy
		&& hit.direct
		&& _article->segmentIsText(hit.segmentIndex);
	const auto isDisplayMathHit = hit.valid()
		&& !hit.codeHeaderCopy
		&& hit.direct
		&& _article->segmentIsDisplayMath(hit.segmentIndex);
	const auto displayMathSegment = [&] {
		if (isDisplayMathHit) {
			return hit.segmentIndex;
		}
		if (editHit.leaf
			&& (editHit.leaf->kind
				== Markdown::PreparedEditLeafKind::MathFormula)) {
			const auto ordinal = _state->textOrdinalForLeaf(*editHit.leaf);
			return (ordinal >= 0)
				? segmentIndexForEditableOrdinal(ordinal)
				: -1;
		}
		return -1;
	}();
	if (isTextHit) {
		clearStructuralSelection();
	} else {
		clearTextSelection();
		clearStructuralSelection();
	}
	_articleSelectionDrag = {
		.active = true,
		.fromField = fromField,
		.startedBelow = startedBelow,
		.codeHeader = hit.codeHeaderCopy,
		.pressPoint = pressPoint,
		.anchorHit = editHit,
		.textSegment = -1,
		.textOffset = 0,
		.mode = DragSelectionMode::None,
	};
	if (!isTextHit) {
		if (displayMathSegment >= 0) {
			_articleSelectionDrag.textSegment = displayMathSegment;
			return;
		}
		if (editHit.valid() && !hit.codeHeaderCopy && !startedBelow) {
			_articleSelectionDrag.mode = DragSelectionMode::Structural;
		}
		return;
	}
	const auto offset = _article->selectionOffsetFromHit(
		hit,
		TextSelectType::Letters);
	_articleSelectionDrag.textSegment = hit.segmentIndex;
	_articleSelectionDrag.textOffset = offset;
	_articleSelectionDrag.mode = DragSelectionMode::Text;
	_selection = {
		{ hit.segmentIndex, offset },
		{ hit.segmentIndex, offset },
	};
	_selectionEndpoints = {
		.from = MakeSelectionEndpoint(hit),
		.to = MakeSelectionEndpoint(hit),
	};
	update();
}

void Widget::updateArticleSelection(
		QPoint articlePoint,
		const Markdown::MarkdownArticleHitTestResult &hit,
		const PreparedEditHit &editHit) {
	if (!_articleSelectionDrag.active) {
		return;
	}
	const auto dragSegment = _articleSelectionDrag.textSegment;
	const auto originalMathFormulaHit = [&] {
		return _articleSelectionDrag.anchorHit.leaf
			&& (_articleSelectionDrag.anchorHit.leaf->kind
				== Markdown::PreparedEditLeafKind::MathFormula)
			&& editHit.leaf
			&& (*editHit.leaf == *_articleSelectionDrag.anchorHit.leaf);
	};
	const auto directOriginalTextHit = [&] {
		return (dragSegment >= 0)
			&& hit.valid()
			&& hit.direct
			&& (hit.segmentIndex == dragSegment)
			&& _article->segmentIsText(hit.segmentIndex);
	};
	const auto directOriginalEditableHit = [&] {
		return ((dragSegment >= 0)
			&& hit.valid()
			&& hit.direct
			&& (hit.segmentIndex == dragSegment)
			&& _article->segmentIsEditable(hit.segmentIndex))
			|| originalMathFormulaHit();
	};
	const auto updateTextSelection = [&](bool forceUpdate) {
		const auto offset = _article->selectionOffsetFromHit(
			hit,
			TextSelectType::Letters);
		const auto adjusted = _article->adjustSelection(
			dragSegment,
			TextSelection(
				uint16(std::clamp(
					std::min(_articleSelectionDrag.textOffset, offset),
					0,
					0xFFFF)),
				uint16(std::clamp(
					std::max(_articleSelectionDrag.textOffset, offset),
					0,
					0xFFFF))),
			TextSelectType::Letters);
		const auto selection = NormalizeSelection({
			{ dragSegment, adjusted.from },
			{ dragSegment, adjusted.to },
		});
		const auto endpoints = Markdown::MarkdownArticleSelectionEndpoints{
			.from = _selectionEndpoints.from.valid()
				? _selectionEndpoints.from
				: Markdown::MarkdownArticleSelectionEndpoint{
					dragSegment,
					false },
			.to = MakeSelectionEndpoint(hit),
		};
		const auto endpointsChanged
			= (_selectionEndpoints.from.segment != endpoints.from.segment)
			|| (_selectionEndpoints.from.direct != endpoints.from.direct)
			|| (_selectionEndpoints.to.segment != endpoints.to.segment)
			|| (_selectionEndpoints.to.direct != endpoints.to.direct);
		if (_selection != selection || endpointsChanged || forceUpdate) {
			_selection = selection;
			_selectionEndpoints = endpoints;
			update();
		} else {
			_selectionEndpoints = endpoints;
		}
	};
	const auto clearFieldSelection = [&] {
		if (!_articleSelectionDrag.fromField) {
			return;
		}
		auto cursor = _field->textCursor();
		if (!cursor.hasSelection()) {
			return;
		}
		cursor.clearSelection();
		_field->setTextCursor(cursor);
	};
	if (_articleSelectionDrag.mode == DragSelectionMode::Structural) {
		if (directOriginalTextHit()) {
			const auto forceUpdate = !_structuralSelection.empty();
			setStructuralSelection({});
			_articleSelectionDrag.mode = DragSelectionMode::Text;
			updateTextSelection(forceUpdate);
			return;
		}
		if (directOriginalEditableHit()) {
			const auto changed = !_structuralSelection.empty();
			setStructuralSelection({});
			_articleSelectionDrag.mode = DragSelectionMode::None;
			if (changed) {
				update();
			}
			return;
		}
		const auto selection = structuralSelectionFromHits(
			_articleSelectionDrag.anchorHit,
			editHit);
		if (_structuralSelection != selection) {
			setStructuralSelection(selection);
			update();
		}
		return;
	} else if (_articleSelectionDrag.mode == DragSelectionMode::None) {
		if (directOriginalEditableHit()) {
			return;
		}
		if (!editHit.valid()
			|| (_articleSelectionDrag.startedBelow
				&& articlePoint.y() >= _articleHeight)) {
			return;
		}
		_articleSelectionDrag.mode = DragSelectionMode::Structural;
		const auto selection = structuralSelectionFromHits(
			_articleSelectionDrag.anchorHit,
			editHit);
		if (_structuralSelection != selection) {
			setStructuralSelection(selection);
			update();
		}
		return;
	}
	if (_articleSelectionDrag.mode != DragSelectionMode::Text) {
		return;
	}
	if (directOriginalTextHit()) {
		updateTextSelection(false);
		return;
	}
	const auto selection = structuralSelectionFromHits(
		_articleSelectionDrag.anchorHit,
		editHit);
	const auto changed = !_selection.empty()
		|| _selectionEndpoints.from.valid()
		|| _selectionEndpoints.to.valid()
		|| (_structuralSelection != selection);
	_selection = {};
	_selectionEndpoints = {};
	setStructuralSelection(selection);
	_articleSelectionDrag.mode = DragSelectionMode::Structural;
	clearFieldSelection();
	if (changed) {
		update();
	}
}

void Widget::finishArticleSelection() {
	_articleSelectionDrag = {};
}

bool Widget::handleStructuralSelectionKey(QKeyEvent *e) {
	if (!hasStructuralSelection()) {
		return false;
	}
	const auto key = e->key();
	if (key == Qt::Key_Escape) {
		clearStructuralSelection();
		e->accept();
		return true;
	}
	const auto modifiers = e->modifiers()
		& ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
	if (modifiers != Qt::NoModifier) {
		return false;
	}
	const auto forward = (key == Qt::Key_Delete);
	if (!forward && key != Qt::Key_Backspace) {
		return false;
	}
	if (!_state->canRemoveStructuralSelection(_structuralSelection)) {
		e->accept();
		return true;
	}
	const auto origin = [&]() -> std::optional<BoundarySelectionOrigin> {
		if (_boundarySelectionOrigin
			&& _boundarySelectionOrigin->forward == forward) {
			return _boundarySelectionOrigin;
		}
		return std::nullopt;
	}();
	const auto target = removeCurrentStructuralSelection(forward);
	if (hasStructuralSelection()) {
		e->accept();
		return true;
	}
	auto activatedOrigin = false;
	if (origin && _state->setActiveTextByOrdinal(origin->ordinal)) {
		const auto cursor = origin->forward ? _state->activeTextLength() : 0;
		activateTextOrdinal(origin->ordinal, cursor);
		activatedOrigin = true;
	}
	if (!activatedOrigin) {
		if (target) {
			if (forward) {
				activateTextOrdinal(*target, 0);
			} else {
				activateTextOrdinalAtEnd(*target);
			}
		} else {
			activateInitialNode();
		}
	}
	e->accept();
	return true;
}

std::optional<int> Widget::removeCurrentStructuralSelection(bool forward) {
	if (!hasStructuralSelection()) {
		return std::nullopt;
	}
	const auto selection = _structuralSelection;
	auto target = std::optional<int>();
	const auto result = recordMutationTransaction([&] {
		const auto committed = commitInlineField();
		if (committed == ApplyResult::Failed) {
			return MutationTransactionResult{
				.committed = committed,
				.failed = true,
			};
		}
		_pendingOrdinal = -1;
		_pendingCursorOffset = 0;
		hideInlineField();
		clearInlineFieldEditSession();
		target = _state->removeStructuralSelection(selection, forward);
		clearSelection();
		refreshPreparedContent();
		return MutationTransactionResult{
			.committed = committed,
			.changed = true,
		};
	});
	if (result.failed) {
		return std::nullopt;
	}
	return target;
}

bool Widget::handleFieldMouseEvent(QEvent *event) {
	if (!_article || _field->isHidden() || _activeSegmentIndex < 0) {
		return false;
	}
	const auto type = event->type();
	const auto mouse = static_cast<QMouseEvent*>(event);
	if (type == QEvent::MouseButtonPress) {
		if (mouse->button() != Qt::LeftButton) {
			return false;
		}
		const auto segmentRect = _article->segmentRect(_activeSegmentIndex);
		if (segmentRect.isEmpty()) {
			return false;
		}
		auto anchorHit = _article->editHitTest(segmentRect.center());
		if (!anchorHit.valid()) {
			anchorHit = _article->editHitTest(segmentRect.topLeft());
		}
		if (!anchorHit.valid()) {
			return false;
		}
		clearTextSelection();
		clearStructuralSelection();
		const auto globalPoint = mouse->globalPos();
		const auto articlePoint = mapFromGlobal(globalPoint)
			- articleTopLeft();
		const auto cursor = _field->textCursor();
		_trackingPointerPress = true;
		_articleSelectionDrag = {
			.active = true,
			.fromField = true,
			.startedBelow = false,
			.codeHeader = false,
			.pressPoint = articlePoint,
			.anchorHit = anchorHit,
			.textSegment = _activeSegmentIndex,
			.textOffset = std::clamp(
				cursor.position(),
				0,
				int(_field->getLastText().size())),
			.mode = DragSelectionMode::Text,
		};
		return false;
	} else if (!_articleSelectionDrag.active
		|| !_articleSelectionDrag.fromField) {
		return false;
	} else if (type == QEvent::MouseButtonRelease
		&& mouse->button() != Qt::LeftButton) {
		return false;
	} else if (type == QEvent::MouseMove
		&& !(mouse->buttons() & Qt::LeftButton)) {
		finishArticleSelection();
		_trackingPointerPress = false;
		return false;
	}

	const auto globalPoint = mouse->globalPos();
	const auto articlePoint = mapFromGlobal(globalPoint) - articleTopLeft();
	const auto hit = _article->hitTest(
		articlePoint,
		Ui::Text::StateRequest::Flag::LookupSymbol);
	const auto editHit = _article->editHitTest(articlePoint);
	const auto insideActiveField = _field->rect().contains(
		_field->mapFromGlobal(globalPoint));
	const auto originalSegmentHit = hit.valid()
		&& hit.direct
		&& (hit.segmentIndex == _articleSelectionDrag.textSegment)
		&& _article->segmentIsEditable(hit.segmentIndex);
	const auto originalMathFormulaHit = _articleSelectionDrag.anchorHit.leaf
		&& (_articleSelectionDrag.anchorHit.leaf->kind
			== Markdown::PreparedEditLeafKind::MathFormula)
		&& editHit.leaf
		&& (*editHit.leaf == *_articleSelectionDrag.anchorHit.leaf);
	const auto clearArticleSelection = [&] {
		const auto changed = !_selection.empty()
			|| _selectionEndpoints.from.valid()
			|| _selectionEndpoints.to.valid()
			|| hasStructuralSelection();
		_selection = {};
		_selectionEndpoints = {};
		setStructuralSelection({});
		if (changed) {
			update();
		}
	};
	if (insideActiveField || originalSegmentHit || originalMathFormulaHit) {
		if (_articleSelectionDrag.mode == DragSelectionMode::Structural) {
			clearArticleSelection();
			_articleSelectionDrag.mode = DragSelectionMode::Text;
		}
		if (type == QEvent::MouseButtonRelease) {
			finishArticleSelection();
			_trackingPointerPress = false;
		}
		return false;
	}

	updateArticleSelection(articlePoint, hit, editHit);
	if (type == QEvent::MouseButtonRelease) {
		if (hasStructuralSelection()) {
			const auto committed = recordMutationTransaction([&] {
				return commitInlineField();
			});
			if (committed == ApplyResult::Failed) {
				mouse->accept();
				return true;
			}
			_pendingOrdinal = -1;
			_pendingCursorOffset = 0;
			hideInlineField();
			clearInlineFieldEditSession();
			refreshAfterInlineFieldCommit(committed);
			finishArticleSelection();
			_trackingPointerPress = false;
			mouse->accept();
			return true;
		}
		finishArticleSelection();
		_trackingPointerPress = false;
		return false;
	}
	if (_articleSelectionDrag.mode == DragSelectionMode::Structural) {
		mouse->accept();
		return true;
	}
	return false;
}

PreparedEditSelection Widget::structuralSelectionFromHits(
		const PreparedEditHit &anchor,
		const PreparedEditHit &focus) const {
	const auto anchorOwner = StructuralOwnerFromHit(anchor);
	const auto focusOwner = StructuralOwnerFromHit(focus);
	if (!anchorOwner.valid() || !focusOwner.valid()) {
		return {};
	}
	const auto anchorCell = TableCellFromOwner(anchorOwner);
	const auto focusCell = TableCellFromOwner(focusOwner);
	if (anchorCell && focusCell) {
		const auto range = TableRangesUnion(
			TableRangeFromCell(*anchorCell),
			TableRangeFromCell(*focusCell));
		if (!range.empty()
			&& TableRangeContainsCell(range, *anchorCell)
			&& TableRangeContainsCell(range, *focusCell)) {
			return {
				.kind = PreparedEditSelectionKind::TableCells,
				.tableCells = range,
			};
		}
	}
	const auto anchorRow = TableRowFromOwner(anchorOwner);
	const auto focusRow = TableRowFromOwner(focusOwner);
	if (anchorRow
		&& focusRow
		&& SamePreparedEditBlockPath(anchorRow->block, focusRow->block)) {
		const auto range = NormalizeIntegerRange(
			anchorRow->tableRowIndex,
			focusRow->tableRowIndex);
		if (!range.empty()) {
			return {
				.kind = PreparedEditSelectionKind::TableRows,
				.tableRows = {
					.block = anchorRow->block,
					.from = range.from,
					.till = range.till,
				},
			};
		}
	}
	const auto anchorListItem = ListItemFromOwner(anchorOwner);
	const auto focusListItem = ListItemFromOwner(focusOwner);
	if (anchorListItem
		&& focusListItem
		&& SamePreparedEditBlockPath(
			anchorListItem->block,
			focusListItem->block)) {
		const auto range = NormalizeIntegerRange(
			anchorListItem->listItemIndex,
			focusListItem->listItemIndex);
		if (!range.empty()) {
			return {
				.kind = PreparedEditSelectionKind::ListItems,
				.listItems = {
					.block = anchorListItem->block,
					.from = range.from,
					.till = range.till,
				},
			};
		}
	}
	const auto anchorBlock = BlockPathFromOwner(anchorOwner);
	const auto focusBlock = BlockPathFromOwner(focusOwner);
	if (!anchorBlock || !focusBlock) {
		return {};
	}
	if (ComparePreparedEditBlockContainerPaths(
			anchorBlock->container,
			focusBlock->container) == 0) {
		const auto blockSelection = BlockSelectionFromIndexes(
			anchorBlock->container,
			anchorBlock->index,
			focusBlock->index);
		if (!blockSelection.empty()) {
			return blockSelection;
		}
	}
	const auto listItemsFromChildren = ListItemSelectionFromSources(
		ListItemSourcesFromOwner(anchorOwner, anchorBlock),
		ListItemSourcesFromOwner(focusOwner, focusBlock));
	const auto liftedBlockSelection = LiftedBlockSelection(
		*anchorBlock,
		*focusBlock);
	if (IsBlockOwner(anchorOwner)
		&& IsBlockOwner(focusOwner)
		&& !liftedBlockSelection.empty()
		&& !IsMultiListItemSelection(listItemsFromChildren)) {
		return liftedBlockSelection;
	}
	if (!listItemsFromChildren.empty()) {
		return listItemsFromChildren;
	}
	if (!liftedBlockSelection.empty()) {
		return liftedBlockSelection;
	}
	return {};
}

int Widget::editableOrdinalForSegment(int segmentIndex) const {
	return _article->editableIndexForSegment(segmentIndex);
}

int Widget::segmentIndexForEditableOrdinal(int ordinal) const {
	return _article->segmentIndexForEditableIndex(ordinal);
}

QPoint Widget::articleTopLeft() const {
	const auto padding = EditorBodyPadding();
	return { padding.left(), padding.top() };
}

int Widget::articleWidth(int outerWidth) const {
	const auto padding = EditorBodyPadding();
	return std::max(
		outerWidth - padding.left() - padding.right(),
		1);
}

QRect Widget::outerEditableSegmentRect(int segmentIndex) const {
	const auto rect = _article->logicalSegmentRect(segmentIndex);
	return rect.isEmpty() ? rect : rect.translated(articleTopLeft());
}

QRect Widget::fieldOuterRectForSegment(int segmentIndex) const {
	if (!_article || segmentIndex < 0) {
		return QRect();
	}
	if (!_activeSegmentIsDisplayMath) {
		return outerEditableSegmentRect(segmentIndex);
	}
	const auto rect = _article->displayMathEditRect(segmentIndex);
	return rect.isEmpty() ? rect : rect.translated(articleTopLeft());
}

Markdown::MarkdownArticlePaintContext Widget::textPaintContext(QRect clip) {
	const auto logicalRect = QRect(QPoint(), QSize(
		articleWidth(std::max(widthNoMargins(), 1)),
		std::max(_articleHeight, 1)));
	auto context = Markdown::MarkdownArticlePaintContext(
		_theme->preparePaintContext(
			_style.get(),
			logicalRect,
			logicalRect,
			clip,
			window() ? !window()->isActiveWindow() : false));
	const auto messageStyle = context.messageStyle();
	context.caches = {
		.pre = messageStyle->preCache.get(),
		.blockquote = context.quoteCache({}, 0),
		.colors = _highlightColors,
		.st = &messageStyle->richPageStyle,
		.repaint = [=] {
			crl::on_main(this, [=] {
				update();
			});
		},
		.repaintRect = [=](QRect rect) {
			crl::on_main(this, [=] {
				if (rect.isEmpty()) {
					update();
				} else {
					update(rect.translated(articleTopLeft()));
				}
			});
		},
	};
	const auto hiddenSegmentIndex = _field->isHidden()
		? -1
		: _activeSegmentIndex;
	context.hiddenTextSegmentIndex = hiddenSegmentIndex;
	context.hiddenSegmentIndex = hiddenSegmentIndex;
	context.selectionState.selection = _selection;
	context.selectionState.endpoints = &_selectionEndpoints;
	if (!_structuralSelection.empty()) {
		context.selectionState.structuralSelection = &_structuralSelection;
	}
	return context;
}

} // namespace Iv::Editor
