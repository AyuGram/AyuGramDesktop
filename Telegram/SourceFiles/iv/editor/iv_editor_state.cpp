/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/editor/iv_editor_state.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Iv::Editor {
namespace {

using Block = RichPage::Block;
using BlockContainerKind = State::BlockContainerKind;
using BlockContainerPath = State::BlockContainerPath;
using ApplyResult = State::ApplyResult;
using BlockKind = RichPage::BlockKind;
using BoundaryAction = State::BoundaryTarget::Action;
using BlockPath = State::BlockPath;
using FieldMode = State::FieldMode;
using InsertBlockType = State::InsertBlockType;
using InsertionAnchor = State::InsertionAnchor;
using LeafKind = State::LeafKind;
using LeafPath = State::LeafPath;
using ListItem = RichPage::ListItem;
using ListKind = RichPage::ListKind;
using NativeInstantViewLeafUpdateResult
	= Markdown::NativeInstantViewLeafUpdateResult;
using PreparedBlockContainerKind = Markdown::PreparedEditBlockContainerKind;
using PreparedEditLeafKind = Markdown::PreparedEditLeafKind;
using PreparedEditLeafSource = Markdown::PreparedEditLeafSource;
using PreparedEditSelectionKind = Markdown::PreparedEditSelectionKind;
using PreparedBlockContainerPath = Markdown::PreparedEditBlockContainerPath;
using PreparedBlockPath = Markdown::PreparedEditBlockPath;
using PreparedBlockContainerStep = Markdown::PreparedEditBlockContainerStep;
using PreparedEditSelection = Markdown::PreparedEditSelection;
using PreparedMutationKind = State::PreparedMutationKind;
using RemovalKind = State::RemovalKind;
using RemovalTarget = State::RemovalTarget;
using RichText = RichPage::RichText;
using TableCell = RichPage::TableCell;
using TableRow = RichPage::TableRow;
using TaskState = RichPage::TaskState;
using TextNodeDescriptor = State::TextNodeDescriptor;

constexpr auto kMaxRichTextNodeLength = 16000;
constexpr auto kMaxCommittedFieldLength = 256 * 1024;

[[nodiscard]] TextWithEntities MakeText(QString text) {
	auto result = TextWithEntities();
	result.text = std::move(text);
	return result;
}

[[nodiscard]] std::vector<TextWithEntities> SplitFieldText(
		TextWithEntities text) {
	auto result = std::vector<TextWithEntities>();
	auto left = std::move(text);
	auto consumed = 0;
	while (!left.text.isEmpty() && consumed < kMaxCommittedFieldLength) {
		auto part = TextWithEntities();
		const auto limit = std::min(
			kMaxRichTextNodeLength,
			kMaxCommittedFieldLength - consumed);
		if (!TextUtilities::CutPart(part, left, limit)
			|| part.text.isEmpty()) {
			break;
		}
		consumed += part.text.size();
		result.push_back(std::move(part));
	}
	return result;
}

[[nodiscard]] TextWithEntities JoinText(
		TextWithEntities before,
		TextWithEntities selected,
		TextWithEntities after) {
	before.append(std::move(selected));
	before.append(std::move(after));
	return before;
}

[[nodiscard]] BlockContainerPath BlockChildrenContainer(BlockPath path) {
	auto result = std::move(path.container);
	result.steps.push_back({
		.kind = BlockContainerKind::BlockChildren,
		.blockIndex = path.index,
	});
	return result;
}

[[nodiscard]] BlockContainerPath ListItemChildrenContainer(
		BlockPath path,
		int itemIndex) {
	auto result = std::move(path.container);
	result.steps.push_back({
		.kind = BlockContainerKind::ListItemChildren,
		.blockIndex = path.index,
		.listItemIndex = itemIndex,
	});
	return result;
}

[[nodiscard]] PreparedBlockContainerPath ToPreparedBlockContainerPath(
		const BlockContainerPath &path) {
	auto result = PreparedBlockContainerPath();
	result.steps.reserve(path.steps.size());
	for (const auto &step : path.steps) {
		auto converted = PreparedBlockContainerStep();
		converted.blockIndex = step.blockIndex;
		converted.listItemIndex = step.listItemIndex;
		switch (step.kind) {
		case BlockContainerKind::Root:
			continue;
		case BlockContainerKind::BlockChildren:
			converted.kind = PreparedBlockContainerKind::BlockChildren;
			break;
		case BlockContainerKind::ListItemChildren:
			converted.kind = PreparedBlockContainerKind::ListItemChildren;
			break;
		}
		result.steps.push_back(converted);
	}
	return result;
}

[[nodiscard]] bool ContainerHasPrefix(
		const BlockContainerPath &path,
		const BlockContainerPath &prefix) {
	if (path.steps.size() < prefix.steps.size()) {
		return false;
	}
	return std::equal(
		prefix.steps.begin(),
		prefix.steps.end(),
		path.steps.begin());
}

[[nodiscard]] bool IndexInRange(int index, int from, int till) {
	return (index >= from) && (index < till);
}

[[nodiscard]] bool ShiftBlockContainerPathAfterRemovedBlock(
		BlockContainerPath &path,
		const BlockPath &removed) {
	if (!ContainerHasPrefix(path, removed.container)) {
		return true;
	}
	const auto size = removed.container.steps.size();
	if (path.steps.size() == size) {
		return true;
	}
	auto &step = path.steps[size];
	if (step.blockIndex == removed.index) {
		return false;
	} else if (step.blockIndex > removed.index) {
		--step.blockIndex;
	}
	return true;
}

[[nodiscard]] bool ShiftBlockPathAfterRemovedBlock(
		BlockPath &path,
		const BlockPath &removed) {
	if (path.container == removed.container) {
		if (path.index == removed.index) {
			return false;
		} else if (path.index > removed.index) {
			--path.index;
		}
		return true;
	}
	return ShiftBlockContainerPathAfterRemovedBlock(path.container, removed);
}

[[nodiscard]] std::optional<int> BlockIndexInContainer(
		const LeafPath &leaf,
		const BlockContainerPath &container) {
	if (leaf.block.container == container) {
		return leaf.block.index;
	}
	if (!ContainerHasPrefix(leaf.block.container, container)
		|| leaf.block.container.steps.size() <= container.steps.size()) {
		return std::nullopt;
	}
	const auto &step = leaf.block.container.steps[container.steps.size()];
	return (step.kind == BlockContainerKind::BlockChildren
			|| step.kind == BlockContainerKind::ListItemChildren)
		? std::make_optional(step.blockIndex)
		: std::nullopt;
}

[[nodiscard]] std::optional<int> ListItemIndexForLeaf(
		const LeafPath &leaf,
		const BlockPath &block) {
	if (leaf.block == block && leaf.kind == LeafKind::ListItemText) {
		return leaf.listItemIndex;
	}
	if (!ContainerHasPrefix(leaf.block.container, block.container)
		|| leaf.block.container.steps.size() <= block.container.steps.size()) {
		return std::nullopt;
	}
	const auto &step = leaf.block.container.steps[block.container.steps.size()];
	return (step.kind == BlockContainerKind::ListItemChildren
			&& step.blockIndex == block.index)
		? std::make_optional(step.listItemIndex)
		: std::nullopt;
}

[[nodiscard]] std::optional<int> TableRowIndexForLeaf(
		const LeafPath &leaf,
		const BlockPath &block) {
	if (!(leaf.block == block)) {
		return std::nullopt;
	}
	if (leaf.kind == LeafKind::BlockText) {
		return -1;
	}
	return (leaf.kind == LeafKind::TableCellText)
		? std::make_optional(leaf.tableRowIndex)
		: std::nullopt;
}

[[nodiscard]] std::optional<int> TableCellIndexForLeaf(
		const LeafPath &leaf,
		const BlockPath &block,
		int rowIndex) {
	return (leaf.block == block
			&& leaf.kind == LeafKind::TableCellText
			&& leaf.tableRowIndex == rowIndex)
		? std::make_optional(leaf.tableCellIndex)
		: std::nullopt;
}

using TableGridOccupancyRow = std::vector<char>;
using TableGridOccupancy = std::vector<TableGridOccupancyRow>;

struct TableGridCellReference {
	int rowIndex = -1;
	int cellIndex = -1;
	int rowFrom = -1;
	int rowTill = -1;
	int columnFrom = -1;
	int columnTill = -1;
};

struct TableGrid {
	std::vector<TableGridCellReference> cells;
	TableGridOccupancy occupancy;
	int rowCount = 0;
	int columnCount = 0;
};

[[nodiscard]] int NormalizeTableSpan(int span) {
	return std::max(span, 1);
}

[[nodiscard]] int ClampTableRowspan(
		int rawRowspan,
		int row,
		int rowCount) {
	if ((row < 0) || (row >= rowCount) || (rowCount <= 0)) {
		return 0;
	}
	const auto remainingRows = int64(rowCount) - row;
	return int(std::min<int64>(NormalizeTableSpan(rawRowspan), remainingRows));
}

[[nodiscard]] int ClampTableColspan(
		int rawColspan,
		int column,
		int maxColumns) {
	if ((column < 0) || (column >= maxColumns) || (maxColumns <= 0)) {
		return 0;
	}
	const auto remainingColumns = int64(maxColumns) - column;
	return int(std::min<int64>(
		NormalizeTableSpan(rawColspan),
		remainingColumns));
}

[[nodiscard]] bool CanOccupyTableSlots(
		const TableGridOccupancy &occupancy,
		int row,
		int column,
		int rowspan,
		int colspan) {
	if ((row < 0)
		|| (column < 0)
		|| (rowspan <= 0)
		|| (colspan <= 0)
		|| (row >= int(occupancy.size()))) {
		return false;
	}
	const auto rowLimit = int(std::min<int64>(
		int64(row) + rowspan,
		occupancy.size()));
	const auto columnLimit64 = int64(column) + colspan;
	if (columnLimit64 <= column) {
		return false;
	}
	const auto columnLimit = int(std::min<int64>(
		columnLimit64,
		std::numeric_limits<int>::max()));
	for (auto currentRow = row; currentRow < rowLimit; ++currentRow) {
		const auto &occupied = occupancy[currentRow];
		const auto occupiedLimit = std::min(columnLimit, int(occupied.size()));
		for (auto currentColumn = column;
			currentColumn < occupiedLimit;
			++currentColumn) {
			if (occupied[currentColumn]) {
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] int FirstAvailableTableColumn(
		const TableGridOccupancy &occupancy,
		int row,
		int rowspan,
		int colspan,
		int maxColumns) {
	if ((row < 0)
		|| (row >= int(occupancy.size()))
		|| (rowspan <= 0)
		|| (colspan <= 0)
		|| (maxColumns <= 0)) {
		return -1;
	}
	for (auto column = 0; column < maxColumns; ++column) {
		const auto effectiveColspan = ClampTableColspan(
			colspan,
			column,
			maxColumns);
		if (effectiveColspan <= 0) {
			continue;
		}
		if (CanOccupyTableSlots(
				occupancy,
				row,
				column,
				rowspan,
				effectiveColspan)) {
			return column;
		}
	}
	return -1;
}

void MarkTableSlots(
		TableGridOccupancy *occupancy,
		int row,
		int column,
		int rowspan,
		int colspan) {
	if ((row < 0)
		|| (column < 0)
		|| (rowspan <= 0)
		|| (colspan <= 0)
		|| (row >= int(occupancy->size()))) {
		return;
	}
	const auto rowLimit = int(std::min<int64>(
		int64(row) + rowspan,
		occupancy->size()));
	const auto columnLimit64 = int64(column) + colspan;
	if (columnLimit64 <= column) {
		return;
	}
	const auto columnLimit = int(std::min<int64>(
		columnLimit64,
		std::numeric_limits<int>::max()));
	for (auto currentRow = row; currentRow < rowLimit; ++currentRow) {
		auto &occupied = (*occupancy)[currentRow];
		if (columnLimit > int(occupied.size())) {
			occupied.resize(columnLimit, false);
		}
		for (auto currentColumn = column;
			currentColumn < columnLimit;
			++currentColumn) {
			occupied[currentColumn] = true;
		}
	}
}

[[nodiscard]] int TableGridColumnCount(
		const TableGridOccupancy &occupancy) {
	auto result = 0;
	for (const auto &row : occupancy) {
		result = std::max(result, int(row.size()));
	}
	return result;
}

[[nodiscard]] TableGrid BuildTableGrid(const Block &table) {
	const auto &limits = Markdown::PrepareTableRenderLimitsForIv();
	auto result = TableGrid();
	result.rowCount = std::min(int(table.tableRows.size()), limits.maxRows);
	if (result.rowCount < 0) {
		result.rowCount = 0;
	}
	result.occupancy = TableGridOccupancy(result.rowCount);
	auto occupiedSlotCountSoFar = int64(0);

	for (auto rowIndex = 0; rowIndex != result.rowCount; ++rowIndex) {
		const auto &row = table.tableRows[rowIndex];
		for (auto cellIndex = 0, cellCount = int(row.cells.size());
				cellIndex != cellCount;
				++cellIndex) {
			const auto &cell = row.cells[cellIndex];
			const auto normalizedColspan = NormalizeTableSpan(cell.colspan);
			const auto rowspan = ClampTableRowspan(
				cell.rowspan,
				rowIndex,
				result.rowCount);
			if (rowspan <= 0) {
				continue;
			}
			const auto column = FirstAvailableTableColumn(
				result.occupancy,
				rowIndex,
				rowspan,
				normalizedColspan,
				limits.maxColumns);
			if (column < 0) {
				continue;
			}
			const auto colspan = ClampTableColspan(
				normalizedColspan,
				column,
				limits.maxColumns);
			if (colspan <= 0) {
				continue;
			}
			const auto occupiedSlotGrowth = int64(rowspan) * colspan;
			if (occupiedSlotGrowth > limits.maxCells
				|| (occupiedSlotCountSoFar + occupiedSlotGrowth)
					> limits.maxCells) {
				continue;
			}
			result.cells.push_back({
				.rowIndex = rowIndex,
				.cellIndex = cellIndex,
				.rowFrom = rowIndex,
				.rowTill = rowIndex + rowspan,
				.columnFrom = column,
				.columnTill = column + colspan,
			});
			MarkTableSlots(
				&result.occupancy,
				rowIndex,
				column,
				rowspan,
				colspan);
			occupiedSlotCountSoFar += occupiedSlotGrowth;
		}
	}
	result.columnCount = TableGridColumnCount(result.occupancy);
	return result;
}

template <typename Range>
[[nodiscard]] bool TableGridCellIntersectsRange(
		const TableGridCellReference &cell,
		const Range &range) {
	return (cell.rowFrom < range.rowTill)
		&& (cell.rowTill > range.rowFrom)
		&& (cell.columnFrom < range.columnTill)
		&& (cell.columnTill > range.columnFrom);
}

template <typename Range>
[[nodiscard]] bool TableGridCellContainedInRange(
		const TableGridCellReference &cell,
		const Range &range) {
	return (cell.rowFrom >= range.rowFrom)
		&& (cell.rowTill <= range.rowTill)
		&& (cell.columnFrom >= range.columnFrom)
		&& (cell.columnTill <= range.columnTill);
}

template <typename Range>
[[nodiscard]] std::vector<TableGridCellReference> SelectedTableGridCells(
		const TableGrid &grid,
		const Range &range) {
	auto result = std::vector<TableGridCellReference>();
	result.reserve(grid.cells.size());
	for (const auto &cell : grid.cells) {
		if (TableGridCellIntersectsRange(cell, range)) {
			result.push_back(cell);
		}
	}
	return result;
}

template <typename Range>
[[nodiscard]] bool TableGridRangeCovered(
		const TableGrid &grid,
		const Range &range) {
	if ((range.rowFrom < 0)
		|| (range.rowTill <= range.rowFrom)
		|| (range.columnFrom < 0)
		|| (range.columnTill <= range.columnFrom)
		|| (range.rowTill > grid.rowCount)
		|| (range.columnTill > grid.columnCount)) {
		return false;
	}
	for (auto row = range.rowFrom; row != range.rowTill; ++row) {
		if (row >= int(grid.occupancy.size())) {
			return false;
		}
		const auto &occupied = grid.occupancy[row];
		for (auto column = range.columnFrom;
				column != range.columnTill;
				++column) {
			if (column >= int(occupied.size()) || !occupied[column]) {
				return false;
			}
		}
	}
	return true;
}

template <typename Range>
[[maybe_unused]] [[nodiscard]] bool CleanTableGridUniteRange(
		const TableGrid &grid,
		const Range &range) {
	const auto selected = SelectedTableGridCells(grid, range);
	if (selected.empty()) {
		return false;
	}
	for (const auto &cell : selected) {
		if (!TableGridCellContainedInRange(cell, range)) {
			return false;
		}
	}
	return TableGridRangeCovered(grid, range);
}

template <typename Range>
[[nodiscard]] bool TableGridRangeSpansAllRows(
		const TableGrid &grid,
		const Range &range) {
	return (range.rowFrom == 0)
		&& (range.rowTill == grid.rowCount)
		&& (range.columnFrom >= 0)
		&& (range.columnTill > range.columnFrom)
		&& (range.columnTill <= grid.columnCount);
}

template <typename Range>
[[nodiscard]] bool TableGridRangeSpansAllColumns(
		const TableGrid &grid,
		const Range &range) {
	return (range.rowFrom >= 0)
		&& (range.rowTill > range.rowFrom)
		&& (range.rowTill <= grid.rowCount)
		&& (range.columnFrom == 0)
		&& (range.columnTill == grid.columnCount);
}

template <typename Range>
[[nodiscard]] bool TableGridRangeCoversFullTable(
		const TableGrid &grid,
		const Range &range) {
	return TableGridRangeSpansAllRows(grid, range)
		&& TableGridRangeSpansAllColumns(grid, range);
}

template <typename Range>
[[nodiscard]] int TableGridCellColumnIntersection(
		const TableGridCellReference &cell,
		const Range &range) {
	const auto from = std::max(cell.columnFrom, range.columnFrom);
	const auto till = std::min(cell.columnTill, range.columnTill);
	return std::max(till - from, 0);
}

[[nodiscard]] bool TableGridCellMatchesLeaf(
		const TableGridCellReference &cell,
		const LeafPath &leaf,
		const BlockPath &block) {
	const auto index = TableCellIndexForLeaf(leaf, block, cell.rowIndex);
	return index && *index == cell.cellIndex;
}

[[nodiscard]] TableCell MakeDefaultTableCell() {
	return TableCell();
}

[[nodiscard]] TableCell MakeDefaultTableCell(bool header) {
	auto result = MakeDefaultTableCell();
	result.header = header;
	return result;
}

[[nodiscard]] const TableCell *TableGridCellAt(
		const Block &table,
		const TableGrid &grid,
		int row,
		int column) {
	if (row < 0 || row >= grid.rowCount || column < 0) {
		return nullptr;
	}
	for (const auto &reference : grid.cells) {
		if (reference.rowFrom <= row
			&& reference.rowTill > row
			&& reference.columnFrom <= column
			&& reference.columnTill > column) {
			return &table.tableRows[reference.rowIndex].cells[
				reference.cellIndex];
		}
	}
	return nullptr;
}

[[nodiscard]] int IncrementTableSpan(int span) {
	const auto normalized = NormalizeTableSpan(span);
	return (normalized == std::numeric_limits<int>::max())
		? normalized
		: normalized + 1;
}

void InsertTableCellBeforeVisualColumn(
		TableRow *row,
		const TableGrid &grid,
		int rowIndex,
		int column,
		TableCell insertedCell) {
	auto insertAt = int(row->cells.size());
	for (const auto &reference : grid.cells) {
		if (reference.rowIndex == rowIndex
			&& reference.columnFrom >= column) {
			insertAt = std::min(reference.cellIndex, int(row->cells.size()));
			break;
		}
	}
	row->cells.insert(
		row->cells.begin() + insertAt,
		std::move(insertedCell));
}

void InsertTableCellBeforeVisualColumn(
		TableRow *row,
		const TableGrid &grid,
		int rowIndex,
		int column) {
	InsertTableCellBeforeVisualColumn(
		row,
		grid,
		rowIndex,
		column,
		MakeDefaultTableCell());
}

[[nodiscard]] bool BlockCanOwnChildContainer(const Block &block) {
	return (block.kind == BlockKind::Quote)
		|| (block.kind == BlockKind::Details);
}

[[nodiscard]] bool BlockSupportsBlockText(const Block &block) {
	switch (block.kind) {
	case BlockKind::Heading:
	case BlockKind::Paragraph:
	case BlockKind::Footer:
	case BlockKind::Code:
	case BlockKind::Quote:
	case BlockKind::Table:
	case BlockKind::Details:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] bool BlockSupportsBlockCaption(const Block &block) {
	switch (block.kind) {
	case BlockKind::Quote:
	case BlockKind::Photo:
	case BlockKind::Video:
	case BlockKind::Audio:
	case BlockKind::Map:
		return true;
	default:
		return false;
	}
}

[[nodiscard]] bool StringIsEmpty(const QString &text) {
	return text.trimmed().isEmpty();
}

[[nodiscard]] bool CanEditBlocks(const std::vector<Block> &blocks);

[[nodiscard]] bool CanEditBlock(const Block &block) {
	switch (block.kind) {
	case BlockKind::Heading:
	case BlockKind::Paragraph:
	case BlockKind::Footer:
	case BlockKind::Code:
	case BlockKind::Divider:
	case BlockKind::Anchor:
	case BlockKind::GroupedMedia:
	case BlockKind::Photo:
	case BlockKind::Video:
	case BlockKind::Audio:
	case BlockKind::Math:
	case BlockKind::Table:
	case BlockKind::Map:
		return true;
	case BlockKind::Quote:
	case BlockKind::Details:
		return CanEditBlocks(block.blocks);
	case BlockKind::List:
		return ranges::all_of(block.listItems, [](const ListItem &item) {
			return CanEditBlocks(item.blocks);
		});
	case BlockKind::Unsupported:
	case BlockKind::Thinking:
	case BlockKind::AuthorDate:
	case BlockKind::Embed:
	case BlockKind::EmbedPost:
	case BlockKind::Channel:
	case BlockKind::RelatedArticles:
		return false;
	}
	return false;
}

[[nodiscard]] bool CanEditBlocks(const std::vector<Block> &blocks) {
	return ranges::all_of(blocks, &CanEditBlock);
}

} // namespace

State::State()
: State(std::make_shared<RichPage>(), nullptr, RichMessageLimits()) {
}

State::State(
	std::shared_ptr<RichPage> richPage,
	std::shared_ptr<Markdown::MediaRuntime> mediaRuntime,
	RichMessageLimits limits)
: _richPage(richPage ? std::move(richPage) : std::make_shared<RichPage>())
, _mediaRuntime(std::move(mediaRuntime))
, _limits(std::move(limits)) {
	if (_richPage->blocks.empty()) {
		_richPage->blocks.push_back(MakeParagraphBlock());
	}
	rebuild();
}

const RichPage &State::richPage() const {
	return *_richPage;
}

const Markdown::MarkdownArticleContent &State::prepared() const {
	return _prepared;
}

template <typename Result, typename Callback>
Result State::applyCheckedMutation(Result failure, Callback &&callback) {
	_lastLimitError = std::nullopt;
	auto candidate = State(
		std::make_shared<RichPage>(*_richPage),
		_mediaRuntime,
		_limits);
	candidate._activeTextOrdinal = _activeTextOrdinal;
	candidate._lastLimitError = std::nullopt;
	candidate._temporaryDownParagraph = _temporaryDownParagraph;
	const auto outcome = callback(candidate);
	if (!outcome.apply) {
		return outcome.result;
	}
	if (const auto error = ValidateRichMessage(*candidate._richPage, _limits)) {
		_lastLimitError = error;
		return failure;
	}
	commitCheckedMutation(std::move(candidate));
	return outcome.result;
}

void State::commitCheckedMutation(State state) {
	_richPage = std::move(state._richPage);
	_prepared = std::move(state._prepared);
	_textNodes = std::move(state._textNodes);
	_activeTextOrdinal = state._activeTextOrdinal;
	_lastPreparedMutationKind = (
		state._lastPreparedMutationKind == PreparedMutationKind::FullRebuild)
		? state._lastPreparedMutationKind
		: PreparedMutationKind::FullRebuild;
	_lastLimitError = std::nullopt;
	_temporaryDownParagraph = std::move(state._temporaryDownParagraph);
}

const std::vector<TextNodeDescriptor> &State::textNodes() const {
	return _textNodes;
}

State::Snapshot State::snapshot() const {
	return {
		.richPage = *_richPage,
		.activeLeaf = activeLeafPath(),
		.temporaryDownParagraph = _temporaryDownParagraph,
	};
}

void State::restoreSnapshot(Snapshot snapshot) {
	_richPage = std::make_shared<RichPage>(std::move(snapshot.richPage));
	_activeTextOrdinal = -1;
	_lastLimitError = std::nullopt;
	_temporaryDownParagraph = std::move(snapshot.temporaryDownParagraph);
	rebuild();
	if (snapshot.activeLeaf && (textNodeOrdinal(*snapshot.activeLeaf) >= 0)) {
		const auto activated = activateRebuiltLeaf(*snapshot.activeLeaf);
		Assert(activated);
	} else {
		ensureActiveTextOrdinal();
	}
}

std::optional<LeafPath> State::activeLeafPath() const {
	if (const auto descriptor = textNode(_activeTextOrdinal)) {
		return descriptor->leaf;
	}
	return std::nullopt;
}

int State::textOrdinalForLeafPath(const LeafPath &path) const {
	return textNodeOrdinal(path);
}

void State::clearTemporaryDownParagraph() {
	_temporaryDownParagraph = std::nullopt;
}

void State::clearTemporaryDownParagraphIfInvalid() {
	if (!_temporaryDownParagraph
		|| _temporaryDownParagraph->kind != LeafKind::BlockText
		|| (textNodeOrdinal(*_temporaryDownParagraph) < 0)) {
		clearTemporaryDownParagraph();
		return;
	}
	const auto owner = block(_temporaryDownParagraph->block);
	if (!owner
		|| owner->kind != BlockKind::Paragraph
		|| !BlockIsEmpty(*owner)) {
		clearTemporaryDownParagraph();
	}
}

int State::textOrdinalForLeaf(
		const Markdown::PreparedEditLeafSource &source) const {
	const auto leaf = convertLeafPath(source);
	return leaf ? textOrdinalForLeafPath(*leaf) : -1;
}

PreparedMutationKind State::lastPreparedMutationKind() const {
	return _lastPreparedMutationKind;
}

std::optional<PreparedEditLeafSource> State::activePreparedLeafSource() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	return descriptor ? convertPreparedLeafSource(*descriptor) : std::nullopt;
}

int State::textNodeCount() const {
	return int(_textNodes.size());
}

int State::activeTextOrdinal() const {
	return _activeTextOrdinal;
}

bool State::setActiveTextByOrdinal(int ordinal) {
	if (ordinal < 0 || ordinal >= textNodeCount()) {
		return false;
	}
	if (_temporaryDownParagraph
		&& !(_textNodes[ordinal].leaf == *_temporaryDownParagraph)) {
		clearTemporaryDownParagraph();
	}
	_activeTextOrdinal = ordinal;
	return true;
}

TextWithEntities State::activeText() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return TextWithEntities();
	}
	if (const auto current = richText(descriptor->leaf)) {
		return StripEditModeWrapperEntities(current->text);
	}
	if (const auto current = rawText(descriptor->leaf)) {
		return MakeText(*current);
	}
	return TextWithEntities();
}

ApplyResult State::applyActiveText(TextWithEntities text) {
	_lastLimitError = std::nullopt;
	_lastPreparedMutationKind = PreparedMutationKind::None;
	return applyActiveTextWithLocalLimit(std::move(text));
}

ApplyResult State::applyActiveTextUnchecked(TextWithEntities text) {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return ApplyResult::Failed;
	}
	if (auto current = richText(descriptor->leaf)) {
		if (current->text == text) {
			return ApplyResult::Unchanged;
		}
		current->text = std::move(text);
		if (leafMutationKeepsTextNodes(*descriptor)) {
			if (_temporaryDownParagraph
				&& (descriptor->leaf == *_temporaryDownParagraph)
				&& !RichTextIsEmpty(*current)) {
				clearTemporaryDownParagraph();
			}
			if (updatePreparedActiveLeaf(*descriptor)) {
				_lastPreparedMutationKind = PreparedMutationKind::LeafOnly;
			} else {
				rebuildPrepared();
			}
		} else {
			rebuild();
		}
		return ApplyResult::Changed;
	}
	if (auto current = rawText(descriptor->leaf)) {
		if (*current == text.text) {
			return ApplyResult::Unchanged;
		}
		*current = std::move(text.text);
		if (leafMutationKeepsTextNodes(*descriptor)) {
			if (updatePreparedActiveLeaf(*descriptor)) {
				_lastPreparedMutationKind = PreparedMutationKind::LeafOnly;
			} else {
				rebuildPrepared();
			}
		} else {
			rebuild();
		}
		return ApplyResult::Changed;
	}
	return ApplyResult::Failed;
}

ApplyResult State::applyActiveTextWithLocalLimit(TextWithEntities text) {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return ApplyResult::Failed;
	}
	auto chunks = SplitFieldText(std::move(text));
	if (chunks.size() <= 1) {
		return applyActiveTextUnchecked(chunks.empty()
			? TextWithEntities()
			: std::move(chunks.front()));
	}
	auto first = chunks.front();
	if (const auto result = applySplitParagraphText(
			*descriptor,
			std::move(chunks)); result != ApplyResult::Failed) {
		return result;
	}
	return applyActiveTextUnchecked(std::move(first));
}

FieldMode State::activeFieldMode() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	return descriptor ? descriptor->mode : FieldMode::Rich;
}

QString State::activeRawText() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return QString();
	}
	if (const auto current = rawText(descriptor->leaf)) {
		return *current;
	}
	if (const auto current = richText(descriptor->leaf)) {
		return StripEditModeWrapperEntities(current->text).text;
	}
	return QString();
}

QString State::activePlaceholderText() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return QString();
	}
	const auto owner = block(descriptor->leaf.block);
	if (!owner) {
		return QString();
	}
	switch (descriptor->leaf.kind) {
	case LeafKind::BlockText:
		switch (owner->kind) {
		case BlockKind::Paragraph:
		case BlockKind::Footer:
		case BlockKind::Code:
		case BlockKind::Quote:
			return u"Text"_q;
		case BlockKind::Heading:
		case BlockKind::Details:
			return u"Header"_q;
		default:
			return QString();
		}
	case LeafKind::BlockCaption:
		switch (owner->kind) {
		case BlockKind::Quote:
		case BlockKind::Photo:
		case BlockKind::Video:
		case BlockKind::Audio:
		case BlockKind::Map:
			return u"Caption"_q;
		default:
			return QString();
		}
	case LeafKind::ListItemText:
		return u"Text"_q;
	case LeafKind::TableCellText:
		return u"Cell"_q;
	case LeafKind::MathFormula:
		return u"x^2 + y^2"_q;
	}
	return QString();
}

ApplyResult State::applyActiveRawText(QString text) {
	_lastLimitError = std::nullopt;
	_lastPreparedMutationKind = PreparedMutationKind::None;
	return applyActiveRawTextWithLocalLimit(std::move(text));
}

ApplyResult State::applyActiveRawTextUnchecked(QString text) {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return ApplyResult::Failed;
	}
	if (auto current = rawText(descriptor->leaf)) {
		if (*current == text) {
			return ApplyResult::Unchanged;
		}
		*current = std::move(text);
		if (leafMutationKeepsTextNodes(*descriptor)) {
			if (updatePreparedActiveLeaf(*descriptor)) {
				_lastPreparedMutationKind = PreparedMutationKind::LeafOnly;
			} else {
				rebuildPrepared();
			}
		} else {
			rebuild();
		}
		return ApplyResult::Changed;
	}
	if (auto current = richText(descriptor->leaf)) {
		auto updated = MakeText(std::move(text));
		if (current->text == updated) {
			return ApplyResult::Unchanged;
		}
		current->text = std::move(updated);
		if (leafMutationKeepsTextNodes(*descriptor)) {
			if (_temporaryDownParagraph
				&& (descriptor->leaf == *_temporaryDownParagraph)
				&& !RichTextIsEmpty(*current)) {
				clearTemporaryDownParagraph();
			}
			if (updatePreparedActiveLeaf(*descriptor)) {
				_lastPreparedMutationKind = PreparedMutationKind::LeafOnly;
			} else {
				rebuildPrepared();
			}
		} else {
			rebuild();
		}
		return ApplyResult::Changed;
	}
	return ApplyResult::Failed;
}

ApplyResult State::applyActiveRawTextWithLocalLimit(QString text) {
	auto chunks = SplitFieldText(MakeText(std::move(text)));
	return applyActiveRawTextUnchecked(chunks.empty()
		? QString()
		: std::move(chunks.front().text));
}

ApplyResult State::applySplitParagraphText(
		const TextNodeDescriptor &descriptor,
		std::vector<TextWithEntities> chunks) {
	if (chunks.empty()) {
		return applyActiveTextUnchecked(TextWithEntities());
	}
	const auto makeParagraph = [&](TextWithEntities text) {
		auto paragraph = MakeParagraphBlock();
		paragraph.text.text = std::move(text);
		return paragraph;
	};
	const auto focus = [&](LeafPath leaf) {
		rebuild();
		if (!activateRebuiltLeaf(leaf)) {
			ensureActiveTextOrdinal();
		}
	};
	if (descriptor.leaf.kind == LeafKind::BlockText) {
		const auto path = descriptor.leaf.block;
		if (auto owner = block(path)) {
			if (owner->kind == BlockKind::Paragraph) {
				auto container = blockContainer(path.container);
				if (!container
					|| path.index < 0
					|| path.index >= int(container->size())) {
					return ApplyResult::Failed;
				}
				clearTemporaryDownParagraph();
				owner->text.text = std::move(chunks.front());
				auto blocks = std::vector<Block>();
				blocks.reserve(chunks.size() - 1);
				for (auto i = 1; i != int(chunks.size()); ++i) {
					blocks.push_back(makeParagraph(std::move(chunks[i])));
				}
				container->insert(
					container->begin() + path.index + 1,
					std::make_move_iterator(blocks.begin()),
					std::make_move_iterator(blocks.end()));
				focus(descriptor.leaf);
				return ApplyResult::Changed;
			} else if (owner->kind == BlockKind::Quote && !owner->pullquote) {
				clearTemporaryDownParagraph();
				auto firstText = std::move(owner->text);
				firstText.text = std::move(chunks.front());
				auto blocks = std::vector<Block>();
				blocks.reserve(chunks.size());
				auto first = MakeParagraphBlock();
				first.text = std::move(firstText);
				blocks.push_back(std::move(first));
				for (auto i = 1; i != int(chunks.size()); ++i) {
					blocks.push_back(makeParagraph(std::move(chunks[i])));
				}
				owner->text = RichText();
				owner->blocks.insert(
					owner->blocks.begin(),
					std::make_move_iterator(blocks.begin()),
					std::make_move_iterator(blocks.end()));
				focus({
					.kind = LeafKind::BlockText,
					.block = {
						.container = BlockChildrenContainer(path),
						.index = 0,
					},
				});
				return ApplyResult::Changed;
			}
		}
	} else if (descriptor.leaf.kind == LeafKind::ListItemText) {
		const auto path = descriptor.leaf.block;
		if (auto item = listItem(path, descriptor.leaf.listItemIndex)) {
			clearTemporaryDownParagraph();
			auto firstText = std::move(item->text);
			firstText.text = std::move(chunks.front());
			auto blocks = std::vector<Block>();
			blocks.reserve(chunks.size());
			auto first = MakeParagraphBlock();
			first.anchorId = std::move(item->anchorId);
			first.text = std::move(firstText);
			blocks.push_back(std::move(first));
			for (auto i = 1; i != int(chunks.size()); ++i) {
				blocks.push_back(makeParagraph(std::move(chunks[i])));
			}
			item->anchorId.clear();
			item->text = RichText();
			item->blocks.insert(
				item->blocks.begin(),
				std::make_move_iterator(blocks.begin()),
				std::make_move_iterator(blocks.end()));
			focus({
				.kind = LeafKind::BlockText,
				.block = {
					.container = ListItemChildrenContainer(
						path,
						descriptor.leaf.listItemIndex),
					.index = 0,
				},
			});
			return ApplyResult::Changed;
		}
	}
	return ApplyResult::Failed;
}

std::optional<RichMessageLimitError> State::lastLimitError() const {
	return _lastLimitError;
}

std::optional<QString> State::codeBlockLanguage(int ordinal) const {
	const auto descriptor = textNode(ordinal);
	if (!descriptor || descriptor->leaf.kind != LeafKind::BlockText) {
		return std::nullopt;
	}
	const auto owner = block(descriptor->leaf.block);
	return (owner && owner->kind == BlockKind::Code)
		? std::make_optional(owner->language)
		: std::nullopt;
}

bool State::setCodeBlockLanguage(int ordinal, QString language) {
	const auto descriptor = textNode(ordinal);
	if (!descriptor || descriptor->leaf.kind != LeafKind::BlockText) {
		return false;
	}
	auto owner = block(descriptor->leaf.block);
	if (!owner || owner->kind != BlockKind::Code) {
		return false;
	}
	owner->language = std::move(language).trimmed();
	rebuild();
	return true;
}

bool State::toggleTaskState(
		const Markdown::PreparedEditListItemSource &source) {
	const auto blockPath = convertBlockPath(source.block);
	if (!blockPath) {
		return false;
	}
	auto item = listItem(*blockPath, source.listItemIndex);
	if (!item || item->taskState == TaskState::None) {
		return false;
	}
	item->taskState = (item->taskState == TaskState::Unchecked)
		? TaskState::Checked
		: TaskState::Unchecked;
	rebuild();
	return true;
}

bool State::toggleDetailsOpen(
		const Markdown::PreparedEditBlockSource &source) {
	const auto path = convertBlockPath(source);
	if (!path) {
		return false;
	}
	auto owner = block(*path);
	if (!owner || owner->kind != BlockKind::Details) {
		return false;
	}
	owner->open = !owner->open;
	rebuild();
	return true;
}

State::TableSelectionInfo State::tableSelectionInfo(
		const Markdown::PreparedEditTableCellRange &range) const {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return {};
	}
	const auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return {};
	}
	const auto grid = BuildTableGrid(*owner);
	const auto selected = SelectedTableGridCells(grid, *validated);
	if (selected.empty()) {
		return {};
	}
	auto result = TableSelectionInfo{
		.valid = true,
		.allHeader = true,
		.allAlignCenter = true,
		.allAlignRight = true,
		.allAlignMiddle = true,
		.allAlignBottom = true,
		.singleCell = (selected.size() == 1),
		.canDeleteRows = TableGridRangeSpansAllColumns(grid, *validated),
		.canDeleteColumns = TableGridRangeSpansAllRows(grid, *validated),
		.canDeleteTable = TableGridRangeCoversFullTable(grid, *validated),
		.selectedRows = validated->rowTill - validated->rowFrom,
		.selectedColumns = validated->columnTill - validated->columnFrom,
		.bordered = owner->bordered,
		.striped = owner->striped,
	};
	for (const auto &reference : selected) {
		const auto &cell = owner->tableRows[reference.rowIndex].cells[
			reference.cellIndex];
		if (!cell.header) {
			result.allHeader = false;
		}
		if (cell.alignment != RichPage::TableAlignment::Center) {
			result.allAlignCenter = false;
		}
		if (cell.alignment != RichPage::TableAlignment::Right) {
			result.allAlignRight = false;
		}
		if (cell.verticalAlignment
			!= RichPage::TableVerticalAlignment::Middle) {
			result.allAlignMiddle = false;
		}
		if (cell.verticalAlignment
			!= RichPage::TableVerticalAlignment::Bottom) {
			result.allAlignBottom = false;
		}
		if (result.singleCell) {
			result.canSplitCell = (NormalizeTableSpan(cell.colspan) > 1)
				|| (NormalizeTableSpan(cell.rowspan) > 1);
		}
	}
	result.canUniteCells = !result.singleCell
		&& CleanTableGridUniteRange(grid, *validated);
	return result;
}

std::optional<Markdown::PreparedEditTableCellRange>
State::tableContextRangeForSelection(
		const Markdown::PreparedEditSelection &selection,
		const Markdown::PreparedEditTableCellSource &source) const {
	if (source.tableRowIndex < 0
		|| source.column < 0
		|| source.rowspan <= 0
		|| source.colspan <= 0) {
		return std::nullopt;
	}
	const auto sourceBlock = convertBlockPath(source.block);
	if (!sourceBlock) {
		return std::nullopt;
	}
	const auto owner = block(*sourceBlock);
	if (!owner || owner->kind != BlockKind::Table) {
		return std::nullopt;
	}
	const auto grid = BuildTableGrid(*owner);
	if (grid.rowCount <= 0 || grid.columnCount <= 0) {
		return std::nullopt;
	}
	const auto fullTableRange = [&] {
		return Markdown::PreparedEditTableCellRange{
			.block = source.block,
			.rowFrom = 0,
			.rowTill = grid.rowCount,
			.columnFrom = 0,
			.columnTill = grid.columnCount,
		};
	};
	const auto sourceIntersects = [&](const auto &range) {
		return (source.tableRowIndex < range.rowTill)
			&& (source.tableRowIndex + source.rowspan > range.rowFrom)
			&& (source.column < range.columnTill)
			&& (source.column + source.colspan > range.columnFrom);
	};
	switch (selection.kind) {
	case PreparedEditSelectionKind::TableCells: {
		const auto range = validateTableCellRange(selection.tableCells);
		if (!range || range->block != *sourceBlock) {
			return std::nullopt;
		}
		return sourceIntersects(*range)
			? std::make_optional(selection.tableCells)
			: std::nullopt;
	}
	case PreparedEditSelectionKind::TableRows: {
		const auto range = validateTableRowRange(selection.tableRows);
		if (!range || range->block != *sourceBlock) {
			return std::nullopt;
		}
		if (source.tableRowIndex >= range->till
			|| source.tableRowIndex + source.rowspan <= range->from) {
			return std::nullopt;
		}
		return Markdown::PreparedEditTableCellRange{
			.block = source.block,
			.rowFrom = range->from,
			.rowTill = range->till,
			.columnFrom = 0,
			.columnTill = grid.columnCount,
		};
	}
	case PreparedEditSelectionKind::Blocks: {
		const auto range = validateBlockRange(selection.blocks);
		if (!range
			|| sourceBlock->container != range->container
			|| sourceBlock->index < range->from
			|| sourceBlock->index >= range->till) {
			return std::nullopt;
		}
		return fullTableRange();
	}
	case PreparedEditSelectionKind::ListItems:
	case PreparedEditSelectionKind::None:
		return std::nullopt;
	}
	return std::nullopt;
}

bool State::canRemoveStructuralSelection(
		const Markdown::PreparedEditSelection &selection) const {
	switch (selection.kind) {
	case PreparedEditSelectionKind::Blocks:
		return validateBlockRange(selection.blocks).has_value();
	case PreparedEditSelectionKind::ListItems:
		return validateListItemRange(selection.listItems).has_value();
	case PreparedEditSelectionKind::TableRows:
		return validateTableRowRange(selection.tableRows).has_value();
	case PreparedEditSelectionKind::TableCells: {
		const auto range = validateTableCellRange(selection.tableCells);
		if (!range) {
			return false;
		}
		const auto owner = block(range->block);
		if (!owner || owner->kind != BlockKind::Table) {
			return false;
		}
		return TableGridRangeSpansAllRows(BuildTableGrid(*owner), *range);
	}
	case PreparedEditSelectionKind::None:
		return false;
	}
	return false;
}

auto State::structuredClipboardDataForSelection(
		const Markdown::PreparedEditSelection &selection) const
-> std::optional<ClipboardData> {
	switch (selection.kind) {
	case PreparedEditSelectionKind::Blocks: {
		const auto range = validateBlockRange(selection.blocks);
		const auto blocks = range ? blockContainer(range->container) : nullptr;
		if (!range || !blocks) {
			return std::nullopt;
		}
		auto data = ClipboardBlockData();
		data.blocks = std::vector<Block>(
			blocks->begin() + range->from,
			blocks->begin() + range->till);
		return ClipboardData(std::move(data));
	}
	case PreparedEditSelectionKind::ListItems: {
		const auto range = validateListItemRange(selection.listItems);
		const auto owner = range ? block(range->block) : nullptr;
		if (!range || !owner || owner->kind != BlockKind::List) {
			return std::nullopt;
		}
		auto data = ClipboardListItemsData();
		data.listKind = owner->listKind;
		data.items = std::vector<ListItem>(
			owner->listItems.begin() + range->from,
			owner->listItems.begin() + range->till);
		data.taskList = std::any_of(
			data.items.begin(),
			data.items.end(),
			[](const ListItem &item) {
				return item.taskState != TaskState::None;
			});
		return ClipboardData(std::move(data));
	}
	case PreparedEditSelectionKind::TableRows:
	case PreparedEditSelectionKind::TableCells:
	case PreparedEditSelectionKind::None:
		return std::nullopt;
	}
	return std::nullopt;
}

bool State::addTableRow(
		const Markdown::PreparedEditTableCellRange &range,
		bool after) {
	return applyCheckedMutation(false, [range, after](State &candidate) {
		const auto applied = candidate.addTableRowUnchecked(range, after);
		return CheckedMutationResult<bool>{
			.apply = applied,
			.result = applied,
		};
	});
}

bool State::addTableRowUnchecked(
		const Markdown::PreparedEditTableCellRange &range,
		bool after) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	const auto insertAt = after ? validated->rowTill : validated->rowFrom;
	const auto grid = BuildTableGrid(*owner);
	if (insertAt < 0 || insertAt > int(owner->tableRows.size())) {
		return false;
	}
	const auto sourceRow = after
		? validated->rowTill - 1
		: validated->rowFrom;

	auto coveredColumns = std::vector<char>(grid.columnCount, false);
	for (const auto &reference : grid.cells) {
		if (reference.rowFrom >= insertAt || reference.rowTill <= insertAt) {
			continue;
		}
		auto &cell = owner->tableRows[reference.rowIndex].cells[
			reference.cellIndex];
		cell.rowspan = IncrementTableSpan(cell.rowspan);
		for (auto column = reference.columnFrom;
				column != reference.columnTill;
				++column) {
			coveredColumns[column] = true;
		}
	}

	auto row = TableRow();
	if (grid.columnCount <= 0) {
		row.cells.push_back(MakeDefaultTableCell());
	} else {
		row.cells.reserve(grid.columnCount);
		for (auto column = 0; column != grid.columnCount; ++column) {
			if (!coveredColumns[column]) {
				const auto source = TableGridCellAt(
					*owner,
					grid,
					sourceRow,
					column);
				row.cells.push_back(MakeDefaultTableCell(
					source ? source->header : false));
			}
		}
	}
	owner->tableRows.insert(
		owner->tableRows.begin() + insertAt,
		std::move(row));
	rebuild();
	return true;
}

bool State::addTableColumn(
		const Markdown::PreparedEditTableCellRange &range,
		bool after) {
	return applyCheckedMutation(false, [range, after](State &candidate) {
		const auto applied = candidate.addTableColumnUnchecked(range, after);
		return CheckedMutationResult<bool>{
			.apply = applied,
			.result = applied,
		};
	});
}

bool State::addTableColumnUnchecked(
		const Markdown::PreparedEditTableCellRange &range,
		bool after) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	const auto insertAt = after
		? validated->columnTill
		: validated->columnFrom;
	const auto grid = BuildTableGrid(*owner);
	const auto sourceColumn = after
		? validated->columnTill - 1
		: validated->columnFrom;

	auto coveredRows = std::vector<char>(owner->tableRows.size(), false);
	for (const auto &reference : grid.cells) {
		if (reference.columnFrom >= insertAt
			|| reference.columnTill <= insertAt) {
			continue;
		}
		auto &cell = owner->tableRows[reference.rowIndex].cells[
			reference.cellIndex];
		cell.colspan = IncrementTableSpan(cell.colspan);
		for (auto row = reference.rowFrom; row != reference.rowTill; ++row) {
			coveredRows[row] = true;
		}
	}

	for (auto rowIndex = 0;
			rowIndex != int(owner->tableRows.size());
			++rowIndex) {
		if (coveredRows[rowIndex]) {
			continue;
		}
		const auto source = TableGridCellAt(
			*owner,
			grid,
			rowIndex,
			sourceColumn);
		InsertTableCellBeforeVisualColumn(
			&owner->tableRows[rowIndex],
			grid,
			rowIndex,
			insertAt,
			MakeDefaultTableCell(source ? source->header : false));
	}
	rebuild();
	return true;
}

bool State::setTableHeader(
		const Markdown::PreparedEditTableCellRange &range,
		bool header) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	const auto selected = SelectedTableGridCells(
		BuildTableGrid(*owner),
		*validated);
	if (selected.empty()) {
		return false;
	}
	auto changed = false;
	for (const auto &reference : selected) {
		auto &cell = owner->tableRows[reference.rowIndex].cells[
			reference.cellIndex];
		if (cell.header != header) {
			cell.header = header;
			changed = true;
		}
	}
	if (changed) {
		rebuild();
	}
	return true;
}

bool State::setTableAlignment(
		const Markdown::PreparedEditTableCellRange &range,
		RichPage::TableAlignment alignment) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	const auto selected = SelectedTableGridCells(
		BuildTableGrid(*owner),
		*validated);
	if (selected.empty()) {
		return false;
	}
	auto changed = false;
	for (const auto &reference : selected) {
		auto &cell = owner->tableRows[reference.rowIndex].cells[
			reference.cellIndex];
		if (cell.alignment != alignment) {
			cell.alignment = alignment;
			changed = true;
		}
	}
	if (changed) {
		rebuild();
	}
	return true;
}

bool State::setTableVerticalAlignment(
		const Markdown::PreparedEditTableCellRange &range,
		RichPage::TableVerticalAlignment alignment) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	const auto selected = SelectedTableGridCells(
		BuildTableGrid(*owner),
		*validated);
	if (selected.empty()) {
		return false;
	}
	auto changed = false;
	for (const auto &reference : selected) {
		auto &cell = owner->tableRows[reference.rowIndex].cells[
			reference.cellIndex];
		if (cell.verticalAlignment != alignment) {
			cell.verticalAlignment = alignment;
			changed = true;
		}
	}
	if (changed) {
		rebuild();
	}
	return true;
}

bool State::splitTableCell(
		const Markdown::PreparedEditTableCellRange &range) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	const auto grid = BuildTableGrid(*owner);
	const auto selected = SelectedTableGridCells(grid, *validated);
	if (selected.size() != 1) {
		return false;
	}
	const auto reference = selected.front();
	auto &cell = owner->tableRows[reference.rowIndex].cells[
		reference.cellIndex];
	if (cell.rowspan <= 1 && cell.colspan <= 1) {
		return false;
	}
	const auto header = cell.header;

	cell.rowspan = 1;
	cell.colspan = 1;
	for (auto rowIndex = reference.rowFrom;
			rowIndex != reference.rowTill;
			++rowIndex) {
		auto &row = owner->tableRows[rowIndex];
		for (auto column = reference.columnTill;
				column != reference.columnFrom;
				--column) {
			const auto currentColumn = column - 1;
			if (rowIndex == reference.rowFrom
				&& currentColumn == reference.columnFrom) {
				continue;
			}
			InsertTableCellBeforeVisualColumn(
				&row,
				grid,
				rowIndex,
				currentColumn,
				MakeDefaultTableCell(header));
		}
	}
	rebuild();
	return true;
}

bool State::uniteTableCells(
		const Markdown::PreparedEditTableCellRange &range) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	const auto grid = BuildTableGrid(*owner);
	const auto selected = SelectedTableGridCells(grid, *validated);
	if (selected.size() <= 1
		|| !CleanTableGridUniteRange(grid, *validated)) {
		return false;
	}
	const auto keeper = *std::min_element(
		selected.begin(),
		selected.end(),
		[](const auto &a, const auto &b) {
			if (a.rowFrom != b.rowFrom) {
				return a.rowFrom < b.rowFrom;
			} else if (a.columnFrom != b.columnFrom) {
				return a.columnFrom < b.columnFrom;
			} else if (a.rowIndex != b.rowIndex) {
				return a.rowIndex < b.rowIndex;
			}
			return a.cellIndex < b.cellIndex;
		});

	auto &keeperCell = owner->tableRows[keeper.rowIndex].cells[
		keeper.cellIndex];
	keeperCell.rowspan = validated->rowTill - validated->rowFrom;
	keeperCell.colspan = validated->columnTill - validated->columnFrom;

	auto toErase = selected;
	toErase.erase(
		std::remove_if(
			toErase.begin(),
			toErase.end(),
			[&](const TableGridCellReference &cell) {
				return cell.rowIndex == keeper.rowIndex
					&& cell.cellIndex == keeper.cellIndex;
			}),
		toErase.end());
	std::sort(
		toErase.begin(),
		toErase.end(),
		[](const auto &a, const auto &b) {
			if (a.rowIndex != b.rowIndex) {
				return a.rowIndex > b.rowIndex;
			}
			return a.cellIndex > b.cellIndex;
		});
	for (const auto &reference : toErase) {
		auto &row = owner->tableRows[reference.rowIndex];
		row.cells.erase(row.cells.begin() + reference.cellIndex);
	}
	rebuild();
	return true;
}

bool State::removeTableRows(
		const Markdown::PreparedEditTableCellRange &range) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	const auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	if (!TableGridRangeSpansAllColumns(BuildTableGrid(*owner), *validated)) {
		return false;
	}
	return removeStructuralSelection({
		.kind = PreparedEditSelectionKind::TableRows,
		.tableRows = {
			.block = range.block,
			.from = validated->rowFrom,
			.till = validated->rowTill,
		},
	}, true).has_value();
}

bool State::removeTableColumns(
		const Markdown::PreparedEditTableCellRange &range) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	const auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	if (!TableGridRangeSpansAllRows(BuildTableGrid(*owner), *validated)) {
		return false;
	}
	return removeStructuralSelection({
		.kind = PreparedEditSelectionKind::TableCells,
		.tableCells = range,
	}, true).has_value();
}

bool State::removeTable(
		const Markdown::PreparedEditTableCellRange &range) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	const auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	if (!TableGridRangeCoversFullTable(BuildTableGrid(*owner), *validated)) {
		return false;
	}
	return removeStructuralSelection({
		.kind = PreparedEditSelectionKind::Blocks,
		.blocks = {
			.container = range.block.container,
			.from = range.block.index,
			.till = range.block.index + 1,
		},
	}, true).has_value();
}

bool State::setTableBordered(
		const Markdown::PreparedEditTableCellRange &range,
		bool bordered) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	if (owner->bordered != bordered) {
		owner->bordered = bordered;
		rebuild();
	}
	return true;
}

bool State::setTableStriped(
		const Markdown::PreparedEditTableCellRange &range,
		bool striped) {
	const auto validated = validateTableCellRange(range);
	if (!validated) {
		return false;
	}
	auto owner = block(validated->block);
	if (!owner || owner->kind != BlockKind::Table) {
		return false;
	}
	if (owner->striped != striped) {
		owner->striped = striped;
		rebuild();
	}
	return true;
}

int State::activeTextLength() const {
	return activeRawText().size();
}

std::optional<int> State::previousEditableOrdinal() const {
	return adjacentEditableOrdinal(false);
}

std::optional<int> State::nextEditableOrdinal() const {
	return adjacentEditableOrdinal(true);
}

State::BoundaryTarget State::activeBoundaryTarget(bool forward) const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return {};
	}
	return boundaryTargetForLeaf(
		descriptor->leaf,
		descriptor,
		forward,
		true);
}

State::BoundaryTarget State::boundaryTargetForLeaf(
		const LeafPath &leaf,
		const TextNodeDescriptor *descriptor,
		bool forward,
		bool allowRemoveDirectly) const {
	const auto ordinal = textNodeOrdinal(leaf);
	if (ordinal < 0) {
		return {};
	}
	const auto prioritizeStructuralStep = [&](const BoundaryTarget &target) {
		if (target.action != BoundaryAction::StructuralSelection) {
			return false;
		}
		switch (target.structuralSelection.kind) {
		case PreparedEditSelectionKind::Blocks:
			if (const auto range = validateBlockRange(
					target.structuralSelection.blocks)) {
				return leafWillBeRemoved(leaf, *range);
			}
			break;
		case PreparedEditSelectionKind::ListItems:
			if (const auto range = validateListItemRange(
					target.structuralSelection.listItems)) {
				return leafWillBeRemoved(leaf, *range);
			}
			break;
		default:
			break;
		}
		return false;
	};
	const auto removeDirectly = allowRemoveDirectly
		&& descriptor
		&& removalTargetIsEmpty(descriptor->removalTarget)
		&& shouldRemoveActiveOwnerDirectly(*descriptor);
	auto steps = std::vector<BoundaryTarget>();
	collectBoundarySteps(
		_richPage->blocks,
		BlockContainerPath(),
		forward,
		&steps);
	for (auto i = 0, count = int(steps.size()); i != count; ++i) {
		const auto &step = steps[i];
		if (step.action == BoundaryAction::Text
			&& step.textOrdinal == ordinal) {
			const auto next = (i + 1 < count)
				? steps[i + 1]
				: BoundaryTarget();
			if (prioritizeStructuralStep(next)) {
				return next;
			}
			if (removeDirectly) {
				return {
					.action = BoundaryAction::RemoveActiveOwner,
				};
			}
			return next;
		}
	}
	return removeDirectly
		? BoundaryTarget{
			.action = BoundaryAction::RemoveActiveOwner,
		}
		: BoundaryTarget();
}

bool State::isActiveTopLevelParagraph() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor || !descriptor->leaf.block.container.steps.empty()) {
		return false;
	}
	const auto owner = block(descriptor->leaf.block);
	return owner && owner->kind == BlockKind::Paragraph;
}

bool State::activeLeafUsesQuoteCaptionColor() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor || descriptor->leaf.kind != LeafKind::BlockCaption) {
		return false;
	}
	const auto owner = block(descriptor->leaf.block);
	return owner && owner->kind == BlockKind::Quote && !owner->pullquote;
}

bool State::activeLeafUsesQuotePlaceholderColor() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return false;
	}
	const auto &leaf = descriptor->leaf;
	const auto owner = block(leaf.block);
	if (owner
		&& owner->kind == BlockKind::Quote
		&& !owner->pullquote
		&& (leaf.kind == LeafKind::BlockText
			|| leaf.kind == LeafKind::BlockCaption)) {
		return true;
	}
	auto container = leaf.block.container;
	while (!container.steps.empty()) {
		const auto step = container.steps.back();
		container.steps.pop_back();
		if (step.kind != BlockContainerKind::BlockChildren) {
			continue;
		}
		const auto ancestor = block({
			.container = container,
			.index = step.blockIndex,
		});
		if (ancestor
			&& ancestor->kind == BlockKind::Quote
			&& !ancestor->pullquote) {
			return true;
		}
	}
	return false;
}

bool State::shouldRemoveActiveOwnerDirectly(
		const TextNodeDescriptor &descriptor) const {
	switch (descriptor.removalTarget.kind) {
	case RemovalKind::Block: {
		const auto owner = block(descriptor.removalTarget.block);
		if (!owner) {
			return false;
		}
		switch (owner->kind) {
		case BlockKind::Heading:
		case BlockKind::Paragraph:
		case BlockKind::Footer:
		case BlockKind::Code:
		case BlockKind::Math:
			return true;
		default:
			return false;
		}
	}
	case RemovalKind::ListItem:
		if (const auto owner = listItem(
				descriptor.removalTarget.block,
				descriptor.removalTarget.listItemIndex)) {
			return owner->blocks.empty();
		}
		return false;
	case RemovalKind::TableCell:
		return false;
	}
	return false;
}

std::optional<int> State::removeActiveOwnerAndSelectAdjacent(bool forward) {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor || !removalTargetIsEmpty(descriptor->removalTarget)) {
		return std::nullopt;
	}
	const auto target = descriptor->removalTarget;
	auto first = _activeTextOrdinal;
	auto last = _activeTextOrdinal;
	while (first > 0 && _textNodes[first - 1].removalTarget == target) {
		--first;
	}
	while (last + 1 < textNodeCount()
		&& _textNodes[last + 1].removalTarget == target) {
		++last;
	}
	auto adjacent = std::optional<LeafPath>();
	if (forward) {
		for (auto i = last + 1, count = textNodeCount(); i != count; ++i) {
			if (!(_textNodes[i].removalTarget == target)) {
				adjacent = _textNodes[i].leaf;
				break;
			}
		}
	} else {
		for (auto i = first; i != 0; --i) {
			if (!(_textNodes[i - 1].removalTarget == target)) {
				adjacent = _textNodes[i - 1].leaf;
				break;
			}
		}
	}
	if (!removeTarget(target)) {
		return std::nullopt;
	}
	rebuild();
	if (adjacent && (!forward || target.kind == RemovalKind::TableCell)) {
		const auto ordinal = textNodeOrdinal(*adjacent);
		if (setActiveTextByOrdinal(ordinal)) {
			return _activeTextOrdinal;
		}
	}
	if (!textNodeCount()) {
		return std::nullopt;
	}
	const auto fallback = forward
		? std::min(first, textNodeCount() - 1)
		: std::max(std::min(first - 1, textNodeCount() - 1), 0);
	if (!setActiveTextByOrdinal(fallback)) {
		ensureActiveTextOrdinal();
	}
	return _activeTextOrdinal;
}

std::optional<int> State::removeStructuralSelection(
		const Markdown::PreparedEditSelection &selection,
		bool forward) {
	clearTemporaryDownParagraph();
	const auto activate = [&](const LeafPath &leaf) -> std::optional<int> {
		const auto ordinal = textNodeOrdinal(leaf);
		if (setActiveTextByOrdinal(ordinal)) {
			return _activeTextOrdinal;
		}
		return std::nullopt;
	};
	const auto finish = [&](
			auto postMutationFocus,
			std::optional<LeafPath> plannedFocus) -> std::optional<int> {
		rebuild();
		if (const auto focus = postMutationFocus()) {
			if (const auto ordinal = activate(*focus)) {
				return ordinal;
			}
		}
		if (plannedFocus) {
			if (const auto ordinal = activate(*plannedFocus)) {
				return ordinal;
			}
		}
		ensureActiveTextOrdinal();
		return (_activeTextOrdinal >= 0)
			? std::make_optional(_activeTextOrdinal)
			: std::nullopt;
	};
	const auto leafForContainerOwner = [&](
			const BlockContainerPath &container) -> std::optional<LeafPath> {
		if (container.steps.empty()) {
			return std::nullopt;
		}
		auto parent = container;
		const auto step = parent.steps.back();
		parent.steps.pop_back();
		const auto owner = BlockPath{
			.container = parent,
			.index = step.blockIndex,
		};
		if (step.kind == BlockContainerKind::BlockChildren) {
			for (const auto &descriptor : _textNodes) {
				if (leafBelongsToBlock(descriptor.leaf, owner)) {
					return descriptor.leaf;
				}
			}
		} else if (step.kind == BlockContainerKind::ListItemChildren) {
			for (const auto &descriptor : _textNodes) {
				const auto index = ListItemIndexForLeaf(
					descriptor.leaf,
					owner);
				if (index && *index == step.listItemIndex) {
					return descriptor.leaf;
				}
			}
		}
		return std::nullopt;
	};
	const auto leafNearBlockRange = [&](
			const StructuralBlockRange &range,
			bool forward) -> std::optional<LeafPath> {
		if (forward) {
			for (const auto &descriptor : _textNodes) {
				const auto index = BlockIndexInContainer(
					descriptor.leaf,
					range.container);
				if (index && *index >= range.from) {
					return descriptor.leaf;
				}
			}
		} else {
			for (auto i = textNodeCount(); i != 0; --i) {
				const auto &leaf = _textNodes[i - 1].leaf;
				const auto index = BlockIndexInContainer(leaf, range.container);
				if (index && *index < range.from) {
					return leaf;
				}
			}
		}
		return leafForContainerOwner(range.container);
	};
	const auto leafOutsideBlock = [&](
			const BlockPath &path,
			bool forward) -> std::optional<LeafPath> {
		if (forward) {
			for (const auto &descriptor : _textNodes) {
				const auto index = BlockIndexInContainer(
					descriptor.leaf,
					path.container);
				if (index && *index > path.index) {
					return descriptor.leaf;
				}
			}
		} else {
			for (auto i = textNodeCount(); i != 0; --i) {
				const auto &leaf = _textNodes[i - 1].leaf;
				const auto index = BlockIndexInContainer(leaf, path.container);
				if (index && *index < path.index) {
					return leaf;
				}
			}
		}
		return std::nullopt;
	};
	const auto leafNearListItemRange = [&](
			const StructuralListItemRange &range,
			bool forward) -> std::optional<LeafPath> {
		if (forward) {
			for (const auto &descriptor : _textNodes) {
				const auto index = ListItemIndexForLeaf(
					descriptor.leaf,
					range.block);
				if (index && *index >= range.from) {
					return descriptor.leaf;
				}
			}
		} else {
			for (auto i = textNodeCount(); i != 0; --i) {
				const auto &leaf = _textNodes[i - 1].leaf;
				const auto index = ListItemIndexForLeaf(leaf, range.block);
				if (index && *index < range.from) {
					return leaf;
				}
			}
		}
		return leafOutsideBlock(range.block, forward);
	};
	const auto tableTitleLeaf = [&](
			const BlockPath &path) -> std::optional<LeafPath> {
		auto leaf = LeafPath{
			.kind = LeafKind::BlockText,
			.block = path,
		};
		return (textNodeOrdinal(leaf) >= 0)
			? std::make_optional(leaf)
			: std::nullopt;
	};
	const auto leafNearTableRows = [&](
			const StructuralTableRowRange &range,
			bool forward) -> std::optional<LeafPath> {
		const auto cellInDirection = [&](
				bool direction) -> std::optional<LeafPath> {
			if (direction) {
				for (const auto &descriptor : _textNodes) {
					const auto &leaf = descriptor.leaf;
					if (leaf.block == range.block
						&& leaf.kind == LeafKind::TableCellText
						&& leaf.tableRowIndex >= range.from) {
						return leaf;
					}
				}
			} else {
				for (auto i = textNodeCount(); i != 0; --i) {
					const auto &leaf = _textNodes[i - 1].leaf;
					if (leaf.block == range.block
						&& leaf.kind == LeafKind::TableCellText
						&& leaf.tableRowIndex < range.from) {
						return leaf;
					}
				}
			}
			return std::nullopt;
		};
		if (const auto leaf = cellInDirection(forward)) {
			return leaf;
		}
		if (const auto leaf = cellInDirection(!forward)) {
			return leaf;
		}
		if (const auto leaf = tableTitleLeaf(range.block)) {
			return leaf;
		}
		return leafOutsideBlock(range.block, forward);
	};
	const auto leafNearTableCells = [&](
			const StructuralTableCellRange &range,
			bool forward) -> std::optional<LeafPath> {
		if (const auto leaf = firstSelectedLeaf(range)) {
			return leaf;
		}
		return adjacentLeafOutsideRange(range, forward);
	};
	const auto focusForRemovedBlock = [&](
			const BlockPath &path,
			bool forward) {
		return leafNearBlockRange(StructuralBlockRange{
			.container = path.container,
			.from = path.index,
			.till = path.index + 1,
		}, forward);
	};
	switch (selection.kind) {
	case PreparedEditSelectionKind::Blocks: {
		const auto range = validateBlockRange(selection.blocks);
		if (!range) {
			return std::nullopt;
		}
		const auto plannedFocus = plannedFocusForRange(*range, forward);
		const auto blocks = blockContainer(range->container);
		if (!blocks) {
			return std::nullopt;
		}
		blocks->erase(
			blocks->begin() + range->from,
			blocks->begin() + range->till);
		return finish([&] {
			return leafNearBlockRange(*range, forward);
		}, plannedFocus);
	}
	case PreparedEditSelectionKind::ListItems: {
		const auto range = validateListItemRange(selection.listItems);
		if (!range) {
			return std::nullopt;
		}
		const auto plannedFocus = plannedFocusForRange(*range, forward);
		const auto owner = block(range->block);
		if (!owner || owner->kind != BlockKind::List) {
			return std::nullopt;
		}
		owner->listItems.erase(
			owner->listItems.begin() + range->from,
			owner->listItems.begin() + range->till);
		if (owner->listItems.empty()) {
			const auto removed = range->block;
			if (!removeTarget({
					.kind = RemovalKind::Block,
					.block = removed,
				})) {
				return std::nullopt;
			}
			return finish([&] {
				return focusForRemovedBlock(removed, forward);
			}, plannedFocus);
		}
		return finish([&] {
			return leafNearListItemRange(*range, forward);
		}, plannedFocus);
	}
	case PreparedEditSelectionKind::TableRows: {
		const auto range = validateTableRowRange(selection.tableRows);
		if (!range) {
			return std::nullopt;
		}
		const auto plannedFocus = plannedFocusForRange(*range, forward);
		const auto owner = block(range->block);
		if (!owner || owner->kind != BlockKind::Table) {
			return std::nullopt;
		}
		owner->tableRows.erase(
			owner->tableRows.begin() + range->from,
			owner->tableRows.begin() + range->till);
		if (owner->tableRows.empty() && RichTextIsEmpty(owner->text)) {
			const auto removed = range->block;
			if (!removeTarget({
					.kind = RemovalKind::Block,
					.block = removed,
				})) {
				return std::nullopt;
			}
			return finish([&] {
				return focusForRemovedBlock(removed, forward);
			}, plannedFocus);
		}
		return finish([&] {
			return leafNearTableRows(*range, forward);
		}, plannedFocus);
	}
	case PreparedEditSelectionKind::TableCells: {
		const auto range = validateTableCellRange(selection.tableCells);
		if (!range) {
			return std::nullopt;
		}
		const auto owner = block(range->block);
		if (!owner || owner->kind != BlockKind::Table) {
			return std::nullopt;
		}
		const auto grid = BuildTableGrid(*owner);
		if (!TableGridRangeSpansAllRows(grid, *range)) {
			return std::nullopt;
		}
		const auto plannedFocus = plannedFocusForRange(*range, forward);
		const auto selected = SelectedTableGridCells(grid, *range);
		if (selected.empty()) {
			return std::nullopt;
		}
		auto toErase = std::vector<TableGridCellReference>();
		for (const auto &reference : selected) {
			if (TableGridCellContainedInRange(reference, *range)) {
				toErase.push_back(reference);
				continue;
			}
			const auto intersection = TableGridCellColumnIntersection(
				reference,
				*range);
			if (intersection <= 0) {
				continue;
			}
			auto &cell = owner->tableRows[reference.rowIndex].cells[
				reference.cellIndex];
			cell.colspan = std::max(
				(reference.columnTill - reference.columnFrom) - intersection,
				1);
		}
		std::sort(
			toErase.begin(),
			toErase.end(),
			[](const auto &a, const auto &b) {
				if (a.rowIndex != b.rowIndex) {
					return a.rowIndex > b.rowIndex;
				}
				return a.cellIndex > b.cellIndex;
			});
		for (const auto &reference : toErase) {
			auto &row = owner->tableRows[reference.rowIndex];
			row.cells.erase(row.cells.begin() + reference.cellIndex);
		}
		if (std::all_of(
				owner->tableRows.begin(),
				owner->tableRows.end(),
				[](const TableRow &row) {
					return row.cells.empty();
				})) {
			if (RichTextIsEmpty(owner->text)) {
				const auto removed = range->block;
				if (!removeTarget({
						.kind = RemovalKind::Block,
						.block = removed,
					})) {
					return std::nullopt;
				}
				return finish([&] {
					return focusForRemovedBlock(removed, forward);
				}, plannedFocus);
			}
			auto row = TableRow();
			row.cells.push_back(MakeDefaultTableCell());
			owner->tableRows.clear();
			owner->tableRows.push_back(std::move(row));
		}
		return finish([&] {
			return leafNearTableCells(*range, forward);
		}, plannedFocus);
	}
	case PreparedEditSelectionKind::None:
		return std::nullopt;
	}
	return std::nullopt;
}

std::optional<State::BlockContainerPath> State::convertBlockContainerPath(
		const Markdown::PreparedEditBlockContainerPath &path) const {
	auto result = BlockContainerPath();
	result.steps.reserve(path.steps.size());
	const auto *blocks = &_richPage->blocks;
	for (const auto &step : path.steps) {
		if (step.blockIndex < 0 || step.blockIndex >= int(blocks->size())) {
			return std::nullopt;
		}
		const auto &parent = (*blocks)[step.blockIndex];
		auto converted = BlockContainerStep();
		converted.blockIndex = step.blockIndex;
		converted.listItemIndex = step.listItemIndex;
		switch (step.kind) {
		case PreparedBlockContainerKind::Root:
			return std::nullopt;
		case PreparedBlockContainerKind::BlockChildren:
			if (!BlockCanOwnChildContainer(parent)) {
				return std::nullopt;
			}
			converted.kind = BlockContainerKind::BlockChildren;
			blocks = &parent.blocks;
			break;
		case PreparedBlockContainerKind::ListItemChildren:
			if (parent.kind != BlockKind::List
				|| step.listItemIndex < 0
				|| step.listItemIndex >= int(parent.listItems.size())) {
				return std::nullopt;
			}
			converted.kind = BlockContainerKind::ListItemChildren;
			blocks = &parent.listItems[step.listItemIndex].blocks;
			break;
		}
		result.steps.push_back(converted);
	}
	return result;
}

std::optional<State::BlockPath> State::convertBlockPath(
		const Markdown::PreparedEditBlockPath &path) const {
	if (path.index < 0) {
		return std::nullopt;
	}
	const auto container = convertBlockContainerPath(path.container);
	if (!container) {
		return std::nullopt;
	}
	const auto blocks = blockContainer(*container);
	if (!blocks || path.index >= int(blocks->size())) {
		return std::nullopt;
	}
	return BlockPath{
		.container = *container,
		.index = path.index,
	};
}

std::optional<State::BlockPath> State::convertBlockPath(
		const Markdown::PreparedEditBlockSource &source) const {
	return convertBlockPath(source.path);
}

std::optional<State::LeafPath> State::convertLeafPath(
		const Markdown::PreparedEditLeafSource &source) const {
	const auto blockPath = convertBlockPath(source.block);
	if (!blockPath) {
		return std::nullopt;
	}
	auto result = LeafPath();
	result.block = *blockPath;
	switch (source.kind) {
	case PreparedEditLeafKind::BlockText:
		result.kind = LeafKind::BlockText;
		break;
	case PreparedEditLeafKind::BlockCaption:
		result.kind = LeafKind::BlockCaption;
		break;
	case PreparedEditLeafKind::ListItemText:
		if (source.listItemIndex < 0) {
			return std::nullopt;
		}
		result.kind = LeafKind::ListItemText;
		result.listItemIndex = source.listItemIndex;
		break;
	case PreparedEditLeafKind::TableCellText:
		if (source.tableRowIndex < 0 || source.tableCellIndex < 0) {
			return std::nullopt;
		}
		result.kind = LeafKind::TableCellText;
		result.tableRowIndex = source.tableRowIndex;
		result.tableCellIndex = source.tableCellIndex;
		break;
	case PreparedEditLeafKind::MathFormula:
		result.kind = LeafKind::MathFormula;
		break;
	}
	const auto owner = block(result.block);
	if (!owner) {
		return std::nullopt;
	}
	switch (result.kind) {
	case LeafKind::BlockText:
		if (!BlockSupportsBlockText(*owner)) {
			return std::nullopt;
		}
		break;
	case LeafKind::BlockCaption:
		if (!BlockSupportsBlockCaption(*owner)) {
			return std::nullopt;
		}
		break;
	case LeafKind::ListItemText:
		if (owner->kind != BlockKind::List
			|| !listItem(result.block, result.listItemIndex)) {
			return std::nullopt;
		}
		break;
	case LeafKind::TableCellText:
		if (owner->kind != BlockKind::Table
			|| !tableCell(
				result.block,
				result.tableRowIndex,
				result.tableCellIndex)) {
			return std::nullopt;
		}
		break;
	case LeafKind::MathFormula:
		if (owner->kind != BlockKind::Math) {
			return std::nullopt;
		}
		break;
	}
	return result;
}

std::optional<PreparedEditLeafSource> State::convertPreparedLeafSource(
		const LeafPath &path) const {
	const auto owner = block(path.block);
	if (!owner) {
		return std::nullopt;
	}
	auto result = PreparedEditLeafSource();
	result.block = {
		.container = ToPreparedBlockContainerPath(path.block.container),
		.index = path.block.index,
	};
	switch (path.kind) {
	case LeafKind::BlockText:
		if (!BlockSupportsBlockText(*owner)) {
			return std::nullopt;
		}
		result.kind = PreparedEditLeafKind::BlockText;
		break;
	case LeafKind::BlockCaption:
		if (!BlockSupportsBlockCaption(*owner)) {
			return std::nullopt;
		}
		result.kind = PreparedEditLeafKind::BlockCaption;
		break;
	case LeafKind::ListItemText:
		if (owner->kind != BlockKind::List
			|| !listItem(path.block, path.listItemIndex)) {
			return std::nullopt;
		}
		result.kind = PreparedEditLeafKind::ListItemText;
		result.listItemIndex = path.listItemIndex;
		break;
	case LeafKind::TableCellText:
		if (owner->kind != BlockKind::Table
			|| !tableCell(
				path.block,
				path.tableRowIndex,
				path.tableCellIndex)) {
			return std::nullopt;
		}
		result.kind = PreparedEditLeafKind::TableCellText;
		result.tableRowIndex = path.tableRowIndex;
		result.tableCellIndex = path.tableCellIndex;
		break;
	case LeafKind::MathFormula:
		if (owner->kind != BlockKind::Math) {
			return std::nullopt;
		}
		result.kind = PreparedEditLeafKind::MathFormula;
		break;
	}
	return result;
}

std::optional<PreparedEditLeafSource> State::convertPreparedLeafSource(
		const TextNodeDescriptor &descriptor) const {
	return convertPreparedLeafSource(descriptor.leaf);
}

std::optional<State::StructuralBlockRange> State::validateBlockRange(
		const Markdown::PreparedEditBlockRange &range) const {
	if (range.empty()) {
		return std::nullopt;
	}
	const auto container = convertBlockContainerPath(range.container);
	if (!container) {
		return std::nullopt;
	}
	const auto blocks = blockContainer(*container);
	if (!blocks
		|| range.from < 0
		|| range.till > int(blocks->size())) {
		return std::nullopt;
	}
	for (auto i = range.from; i != range.till; ++i) {
		if (!CanEditBlock((*blocks)[i])) {
			return std::nullopt;
		}
	}
	return StructuralBlockRange{
		.container = *container,
		.from = range.from,
		.till = range.till,
	};
}

std::optional<State::StructuralListItemRange> State::validateListItemRange(
		const Markdown::PreparedEditListItemRange &range) const {
	if (range.empty()) {
		return std::nullopt;
	}
	const auto path = convertBlockPath(range.block);
	if (!path) {
		return std::nullopt;
	}
	const auto owner = block(*path);
	if (!owner
		|| owner->kind != BlockKind::List
		|| range.from < 0
		|| range.till > int(owner->listItems.size())) {
		return std::nullopt;
	}
	for (auto i = range.from; i != range.till; ++i) {
		if (!CanEditBlocks(owner->listItems[i].blocks)) {
			return std::nullopt;
		}
	}
	return StructuralListItemRange{
		.block = *path,
		.from = range.from,
		.till = range.till,
	};
}

std::optional<State::StructuralTableRowRange> State::validateTableRowRange(
		const Markdown::PreparedEditTableRowRange &range) const {
	if (range.empty()) {
		return std::nullopt;
	}
	const auto path = convertBlockPath(range.block);
	if (!path) {
		return std::nullopt;
	}
	const auto owner = block(*path);
	if (!owner
		|| owner->kind != BlockKind::Table
		|| range.from < 0
		|| range.till > int(owner->tableRows.size())) {
		return std::nullopt;
	}
	return StructuralTableRowRange{
		.block = *path,
		.from = range.from,
		.till = range.till,
	};
}

std::optional<State::StructuralTableCellRange> State::validateTableCellRange(
		const Markdown::PreparedEditTableCellRange &range) const {
	if (range.empty()) {
		return std::nullopt;
	}
	const auto path = convertBlockPath(range.block);
	if (!path) {
		return std::nullopt;
	}
	const auto owner = block(*path);
	if (!owner || owner->kind != BlockKind::Table) {
		return std::nullopt;
	}
	const auto grid = BuildTableGrid(*owner);
	if (range.rowFrom < 0
		|| range.rowTill > grid.rowCount
		|| range.columnFrom < 0
		|| range.columnTill > grid.columnCount) {
		return std::nullopt;
	}
	auto result = StructuralTableCellRange{
		.block = *path,
		.rowFrom = range.rowFrom,
		.rowTill = range.rowTill,
		.columnFrom = range.columnFrom,
		.columnTill = range.columnTill,
	};
	if (SelectedTableGridCells(grid, result).empty()) {
		return std::nullopt;
	}
	return result;
}

bool State::leafWillBeRemoved(
		const LeafPath &path,
		const StructuralBlockRange &range) const {
	const auto index = BlockIndexInContainer(path, range.container);
	return index && IndexInRange(*index, range.from, range.till);
}

bool State::leafWillBeRemoved(
		const LeafPath &path,
		const StructuralListItemRange &range) const {
	const auto index = ListItemIndexForLeaf(path, range.block);
	return index && IndexInRange(*index, range.from, range.till);
}

bool State::leafWillBeRemoved(
		const LeafPath &path,
		const StructuralTableRowRange &range) const {
	const auto index = TableRowIndexForLeaf(path, range.block);
	return index && IndexInRange(*index, range.from, range.till);
}

bool State::leafWillBeRemoved(
		const LeafPath &,
		const StructuralTableCellRange &) const {
	return false;
}

bool State::leafBelongsToBlock(
		const LeafPath &leaf,
		const BlockPath &path) const {
	const auto &owner = leaf.block;
	if (owner == path) {
		return true;
	}
	if (!ContainerHasPrefix(owner.container, path.container)) {
		return false;
	}
	if (owner.container.steps.size() <= path.container.steps.size()) {
		return false;
	}
	const auto &step = owner.container.steps[path.container.steps.size()];
	return step.blockIndex == path.index
		&& (step.kind == BlockContainerKind::BlockChildren
			|| step.kind == BlockContainerKind::ListItemChildren);
}

std::optional<State::LeafPath> State::firstSelectedLeaf(
		const StructuralTableCellRange &range) const {
	const auto owner = block(range.block);
	if (!owner || owner->kind != BlockKind::Table) {
		return std::nullopt;
	}
	const auto selected = SelectedTableGridCells(BuildTableGrid(*owner), range);
	for (const auto &descriptor : _textNodes) {
		const auto &leaf = descriptor.leaf;
		if (leaf.block != range.block
			|| leaf.kind != LeafKind::TableCellText) {
			continue;
		}
		for (const auto &reference : selected) {
			if (TableGridCellMatchesLeaf(reference, leaf, range.block)) {
				return leaf;
			}
		}
	}
	return std::nullopt;
}

std::optional<State::LeafPath> State::adjacentLeafOutsideRange(
		const StructuralBlockRange &range,
		bool forward) const {
	if (forward) {
		for (const auto &descriptor : _textNodes) {
			const auto index = BlockIndexInContainer(
				descriptor.leaf,
				range.container);
			if (index && *index >= range.till) {
				return descriptor.leaf;
			}
		}
	} else {
		for (auto i = textNodeCount(); i != 0; --i) {
			const auto &leaf = _textNodes[i - 1].leaf;
			const auto index = BlockIndexInContainer(leaf, range.container);
			if (index && *index < range.from) {
				return leaf;
			}
		}
	}
	return std::nullopt;
}

std::optional<State::LeafPath> State::adjacentLeafOutsideRange(
		const StructuralListItemRange &range,
		bool forward) const {
	if (forward) {
		for (const auto &descriptor : _textNodes) {
			const auto index = ListItemIndexForLeaf(
				descriptor.leaf,
				range.block);
			if (index && *index >= range.till) {
				return descriptor.leaf;
			}
		}
	} else {
		for (auto i = textNodeCount(); i != 0; --i) {
			const auto &leaf = _textNodes[i - 1].leaf;
			const auto index = ListItemIndexForLeaf(leaf, range.block);
			if (index && *index < range.from) {
				return leaf;
			}
		}
	}
	return std::nullopt;
}

std::optional<State::LeafPath> State::adjacentLeafOutsideRange(
		const StructuralTableRowRange &range,
		bool forward) const {
	if (forward) {
		for (const auto &descriptor : _textNodes) {
			const auto index = TableRowIndexForLeaf(
				descriptor.leaf,
				range.block);
			if (index && *index >= range.till) {
				return descriptor.leaf;
			}
		}
	} else {
		for (auto i = textNodeCount(); i != 0; --i) {
			const auto &leaf = _textNodes[i - 1].leaf;
			const auto index = TableRowIndexForLeaf(leaf, range.block);
			if (index && *index < range.from) {
				return leaf;
			}
		}
	}
	return std::nullopt;
}

std::optional<State::LeafPath> State::adjacentLeafOutsideRange(
		const StructuralTableCellRange &range,
		bool forward) const {
	const auto owner = block(range.block);
	if (!owner || owner->kind != BlockKind::Table) {
		return std::nullopt;
	}
	const auto grid = BuildTableGrid(*owner);
	const auto leafIntersectsRange = [&](const LeafPath &leaf) {
		if (leaf.block != range.block
			|| leaf.kind != LeafKind::TableCellText) {
			return false;
		}
		for (const auto &reference : grid.cells) {
			if (TableGridCellMatchesLeaf(reference, leaf, range.block)) {
				return TableGridCellIntersectsRange(reference, range);
			}
		}
		return false;
	};
	auto fallback = std::optional<LeafPath>();
	auto foundIntersecting = false;
	if (forward) {
		for (const auto &descriptor : _textNodes) {
			if (descriptor.leaf.block == range.block
				&& descriptor.leaf.kind == LeafKind::TableCellText) {
				if (leafIntersectsRange(descriptor.leaf)) {
					foundIntersecting = true;
				} else if (foundIntersecting) {
					return descriptor.leaf;
				} else if (!fallback) {
					fallback = descriptor.leaf;
				}
			}
		}
	} else {
		for (auto i = textNodeCount(); i != 0; --i) {
			const auto &leaf = _textNodes[i - 1].leaf;
			if (leaf.block == range.block
				&& leaf.kind == LeafKind::TableCellText) {
				if (leafIntersectsRange(leaf)) {
					foundIntersecting = true;
				} else if (foundIntersecting) {
					return leaf;
				} else if (!fallback) {
					fallback = leaf;
				}
			}
		}
	}
	return foundIntersecting ? std::nullopt : fallback;
}

std::optional<State::LeafPath> State::fallbackFocusLeaf() const {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (descriptor) {
		return descriptor->leaf;
	}
	return !_textNodes.empty()
		? std::make_optional(_textNodes.front().leaf)
		: std::nullopt;
}

std::optional<State::LeafPath> State::plannedFocusForRange(
		const StructuralBlockRange &range,
		bool forward) const {
	if (const auto adjacent = adjacentLeafOutsideRange(range, forward)) {
		return adjacent;
	}
	const auto descriptor = textNode(_activeTextOrdinal);
	if (descriptor && !leafWillBeRemoved(descriptor->leaf, range)) {
		return descriptor->leaf;
	}
	for (const auto &fallback : _textNodes) {
		if (!leafWillBeRemoved(fallback.leaf, range)) {
			return fallback.leaf;
		}
	}
	return fallbackFocusLeaf();
}

std::optional<State::LeafPath> State::plannedFocusForRange(
		const StructuralListItemRange &range,
		bool forward) const {
	if (const auto adjacent = adjacentLeafOutsideRange(range, forward)) {
		return adjacent;
	}
	const auto ownerRange = StructuralBlockRange{
		.container = range.block.container,
		.from = range.block.index,
		.till = range.block.index + 1,
	};
	if (const auto adjacent = adjacentLeafOutsideRange(ownerRange, forward)) {
		return adjacent;
	}
	const auto descriptor = textNode(_activeTextOrdinal);
	if (descriptor && !leafWillBeRemoved(descriptor->leaf, range)) {
		return descriptor->leaf;
	}
	for (const auto &fallback : _textNodes) {
		if (!leafWillBeRemoved(fallback.leaf, range)) {
			return fallback.leaf;
		}
	}
	return fallbackFocusLeaf();
}

std::optional<State::LeafPath> State::plannedFocusForRange(
		const StructuralTableRowRange &range,
		bool forward) const {
	if (const auto adjacent = adjacentLeafOutsideRange(range, forward)) {
		return adjacent;
	}
	const auto ownerRange = StructuralBlockRange{
		.container = range.block.container,
		.from = range.block.index,
		.till = range.block.index + 1,
	};
	if (const auto adjacent = adjacentLeafOutsideRange(ownerRange, forward)) {
		return adjacent;
	}
	const auto descriptor = textNode(_activeTextOrdinal);
	if (descriptor && !leafWillBeRemoved(descriptor->leaf, range)) {
		return descriptor->leaf;
	}
	for (const auto &fallback : _textNodes) {
		if (!leafWillBeRemoved(fallback.leaf, range)) {
			return fallback.leaf;
		}
	}
	return fallbackFocusLeaf();
}

std::optional<State::LeafPath> State::plannedFocusForRange(
		const StructuralTableCellRange &range,
		bool forward) const {
	if (const auto selected = firstSelectedLeaf(range)) {
		return selected;
	}
	if (const auto adjacent = adjacentLeafOutsideRange(range, forward)) {
		return adjacent;
	}
	return fallbackFocusLeaf();
}

State::InsertionAnchor State::resolveActiveInsertionTarget() const {
	auto result = InsertionAnchor{
		.container = BlockContainerPath(),
		.blockIndex = int(_richPage->blocks.size()) - 1,
	};
	if (const auto descriptor = textNode(_activeTextOrdinal)) {
		result = descriptor->insertionAnchor;
	}
	return blockContainer(result.container)
		? result
		: InsertionAnchor{
			.container = BlockContainerPath(),
			.blockIndex = int(_richPage->blocks.size()) - 1,
		};
}

std::optional<int> State::normalizeTextOnlyListItemForInsertion(
		const BlockContainerPath &container) {
	if (container.steps.empty()) {
		return std::nullopt;
	}
	const auto &step = container.steps.back();
	if (step.kind != BlockContainerKind::ListItemChildren) {
		return std::nullopt;
	}
	auto parent = container;
	parent.steps.pop_back();
	auto itemPath = BlockPath{
		.container = parent,
		.index = step.blockIndex,
	};
	auto item = listItem(itemPath, step.listItemIndex);
	if (!item || (item->anchorId.isEmpty() && RichTextIsEmpty(item->text))) {
		return std::nullopt;
	}
	auto paragraph = MakeParagraphBlock();
	paragraph.anchorId = std::move(item->anchorId);
	paragraph.text = std::move(item->text);
	item->anchorId.clear();
	item->text = RichText();
	if (!BlockIsEmpty(paragraph)) {
		clearTemporaryDownParagraph();
		item->blocks.insert(item->blocks.begin(), std::move(paragraph));
		return 0;
	}
	return -1;
}

std::optional<int> State::normalizeTextOnlyQuoteSurface(
		const BlockContainerPath &container,
		bool keepEmptyParagraph) {
	if (container.steps.empty()) {
		return std::nullopt;
	}
	const auto &step = container.steps.back();
	if (step.kind != BlockContainerKind::BlockChildren) {
		return std::nullopt;
	}
	auto parent = container;
	parent.steps.pop_back();
	auto owner = block({
		.container = parent,
		.index = step.blockIndex,
	});
	if (!owner
		|| owner->kind != BlockKind::Quote
		|| owner->pullquote
		|| !owner->blocks.empty()) {
		return std::nullopt;
	}
	auto paragraph = MakeParagraphBlock();
	paragraph.text = std::move(owner->text);
	owner->text = RichText();
	clearTemporaryDownParagraph();
	if (keepEmptyParagraph || !BlockIsEmpty(paragraph)) {
		owner->blocks.insert(owner->blocks.begin(), std::move(paragraph));
		return 0;
	}
	return -1;
}

std::optional<int> State::normalizeTextOnlyQuoteForInsertion(
		const BlockContainerPath &container) {
	return normalizeTextOnlyQuoteSurface(container, false);
}

bool State::shouldReplaceActiveTextOnlyBlock(
		const TextNodeDescriptor &descriptor,
		const std::vector<Block> &blocks) const {
	if (descriptor.leaf.kind != LeafKind::BlockText
		|| descriptor.removalTarget.kind != RemovalKind::Block) {
		return false;
	}
	const auto owner = block(descriptor.removalTarget.block);
	if (!owner || !BlockIsEmpty(*owner)) {
		return false;
	}
	if (owner->kind == BlockKind::Paragraph) {
		return true;
	}
	return owner->kind == BlockKind::Heading
		&& blocks.size() == 1
		&& blocks.front().kind == BlockKind::Heading;
}

std::optional<int> State::activateRebuiltLeaf(const LeafPath &path) {
	const auto ordinal = textNodeOrdinal(path);
	if (setActiveTextByOrdinal(ordinal)) {
		return _activeTextOrdinal;
	}
	ensureActiveTextOrdinal();
	return (_activeTextOrdinal >= 0)
		? std::make_optional(_activeTextOrdinal)
		: std::nullopt;
}

std::optional<State::ParagraphTarget> State::reuseOrInsertParagraph(
		const BlockContainerPath &containerPath,
		int index) {
	const auto blocks = blockContainer(containerPath);
	if (!blocks) {
		return std::nullopt;
	}
	const auto insertAt = std::clamp(index, 0, int(blocks->size()));
	if (insertAt < int(blocks->size())
		&& (*blocks)[insertAt].kind == BlockKind::Paragraph) {
		return ParagraphTarget{
			.leaf = {
				.kind = LeafKind::BlockText,
				.block = {
					.container = containerPath,
					.index = insertAt,
				},
			},
		};
	}
	clearTemporaryDownParagraph();
	blocks->insert(blocks->begin() + insertAt, MakeParagraphBlock());
	return ParagraphTarget{
		.leaf = {
			.kind = LeafKind::BlockText,
			.block = {
				.container = containerPath,
				.index = insertAt,
			},
		},
		.inserted = true,
	};
}

auto State::resolveActiveTextInsertTarget()
-> std::optional<State::ActiveTextInsertTarget> {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return std::nullopt;
	}
	if (descriptor->leaf.kind == LeafKind::ListItemText) {
		const auto surface = normalizeActiveListItemSurface();
		if (!surface) {
			return std::nullopt;
		}
		const auto container = ListItemChildrenContainer(
			surface->path,
			surface->itemIndex);
		return ActiveTextInsertTarget{
			.leaf = {
				.kind = LeafKind::BlockText,
				.block = {
					.container = container,
					.index = 0,
				},
			},
			.anchor = {
				.container = container,
				.blockIndex = 0,
			},
		};
	}
	if (descriptor->leaf.kind == LeafKind::BlockText) {
		if (const auto owner = block(descriptor->leaf.block);
			owner
			&& owner->kind == BlockKind::Quote
			&& !owner->pullquote
			&& owner->blocks.empty()) {
			const auto container = BlockChildrenContainer(
				descriptor->leaf.block);
			if (!normalizeTextOnlyQuoteSurface(container, true)) {
				return std::nullopt;
			}
			return ActiveTextInsertTarget{
				.leaf = {
					.kind = LeafKind::BlockText,
					.block = {
						.container = container,
						.index = 0,
					},
				},
				.anchor = {
					.container = container,
					.blockIndex = 0,
				},
			};
		}
	}
	return ActiveTextInsertTarget{
		.leaf = descriptor->leaf,
		.anchor = descriptor->insertionAnchor,
	};
}

auto State::activeNonPullquoteQuote() const
-> std::optional<State::ActiveNonPullquoteQuote> {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return std::nullopt;
	}
	const auto direct = block(descriptor->leaf.block);
	if (direct && direct->kind == BlockKind::Quote) {
		return direct->pullquote
			? std::nullopt
			: std::make_optional(ActiveNonPullquoteQuote{
				.path = descriptor->leaf.block,
			});
	}
	auto container = descriptor->leaf.block.container;
	while (!container.steps.empty()) {
		const auto step = container.steps.back();
		container.steps.pop_back();
		if (step.kind != BlockContainerKind::BlockChildren) {
			continue;
		}
		const auto path = BlockPath{
			.container = container,
			.index = step.blockIndex,
		};
		const auto owner = block(path);
		if (!owner || owner->kind != BlockKind::Quote) {
			continue;
		}
		if (owner->pullquote) {
			return std::nullopt;
		}
		const auto body = BlockChildrenContainer(path);
		auto lastBodyLeaf = false;
		for (auto i = textNodeCount(); i != 0; --i) {
			const auto &candidate = _textNodes[i - 1].leaf;
			if (ContainerHasPrefix(candidate.block.container, body)) {
				lastBodyLeaf = (candidate == descriptor->leaf);
				break;
			}
		}
		return ActiveNonPullquoteQuote{
			.path = path,
			.activeLeafIsLastEditableBodyLeaf = lastBodyLeaf,
		};
	}
	return std::nullopt;
}

auto State::activeListItemSurface() const
-> std::optional<State::ActiveListItemSurface> {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return std::nullopt;
	}
	auto path = std::optional<BlockPath>();
	if (descriptor->leaf.kind == LeafKind::ListItemText) {
		path = descriptor->leaf.block;
	} else if (descriptor->leaf.kind == LeafKind::BlockText) {
		const auto owner = block(descriptor->leaf.block);
		if (!owner || owner->kind != BlockKind::Paragraph) {
			return std::nullopt;
		}
		const auto &container = descriptor->leaf.block.container;
		if (container.steps.empty()) {
			return std::nullopt;
		}
		const auto &step = container.steps.back();
		if (step.kind != BlockContainerKind::ListItemChildren) {
			return std::nullopt;
		}
		auto parent = container;
		parent.steps.pop_back();
		path = BlockPath{
			.container = parent,
			.index = step.blockIndex,
		};
	} else {
		return std::nullopt;
	}
	const auto owner = block(*path);
	if (!owner || owner->kind != BlockKind::List) {
		return std::nullopt;
	}
	const auto itemIndex = ListItemIndexForLeaf(descriptor->leaf, *path);
	if (!itemIndex) {
		return std::nullopt;
	}
	if (descriptor->leaf.kind == LeafKind::BlockText
		&& descriptor->leaf.block.container
			!= ListItemChildrenContainer(*path, *itemIndex)) {
		return std::nullopt;
	}
	return ActiveListItemSurface{
		.path = *path,
		.itemIndex = *itemIndex,
	};
}

auto State::normalizeActiveListItemSurface()
-> std::optional<State::ActiveListItemSurface> {
	const auto descriptor = textNode(_activeTextOrdinal);
	const auto surface = activeListItemSurface();
	if (!descriptor
		|| !surface
		|| descriptor->leaf.kind != LeafKind::ListItemText) {
		return surface;
	}
	const auto item = listItem(surface->path, surface->itemIndex);
	if (!item) {
		return std::nullopt;
	}
	auto paragraph = MakeParagraphBlock();
	paragraph.anchorId = std::move(item->anchorId);
	paragraph.text = std::move(item->text);
	item->anchorId.clear();
	item->text = RichText();
	clearTemporaryDownParagraph();
	item->blocks.insert(item->blocks.begin(), std::move(paragraph));
	return surface;
}

RichText *State::seedInsertedBlocks(
		std::vector<Block> &blocks,
		TextWithEntities text) {
	for (auto &block : blocks) {
		if (auto target = seedInsertedBlock(block)) {
			if (!text.text.isEmpty()) {
				auto combined = std::move(text);
				combined.append(target->text);
				target->text = std::move(combined);
			}
			return target;
		}
	}
	return nullptr;
}

RichText *State::seedInsertedBlock(Block &block) {
	switch (block.kind) {
	case BlockKind::Heading:
	case BlockKind::Paragraph:
	case BlockKind::Footer:
	case BlockKind::Code:
	case BlockKind::Table:
	case BlockKind::Details:
		return &block.text;
	case BlockKind::Quote:
		if (block.blocks.empty()) {
			return &block.text;
		}
		for (auto &child : block.blocks) {
			if (const auto result = seedInsertedBlock(child)) {
				return result;
			}
		}
		return &block.caption;
	case BlockKind::List:
		for (auto &item : block.listItems) {
			if (!RichTextIsEmpty(item.text) || item.blocks.empty()) {
				return &item.text;
			}
			for (auto &child : item.blocks) {
				if (const auto result = seedInsertedBlock(child)) {
					return result;
				}
			}
		}
		return nullptr;
	case BlockKind::Photo:
	case BlockKind::Video:
	case BlockKind::Audio:
	case BlockKind::Map:
		return &block.caption;
	default:
		return nullptr;
	}
}

bool State::appendInsertedTrailingText(
		const BlockContainerPath &container,
		int insertAt,
		int count,
		TextWithEntities text) {
	if (text.text.isEmpty()) {
		return true;
	}
	const auto blocks = blockContainer(container);
	if (!blocks
		|| insertAt < 0
		|| count < 0
		|| insertAt + count > int(blocks->size())) {
		return false;
	}
	auto &last = (*blocks)[insertAt + count - 1];
	if (last.kind == BlockKind::List) {
		const auto taskState = last.listItems.empty()
			? TaskState::None
			: last.listItems.back().taskState;
		auto item = MakeParagraphListItem(taskState);
		Assert(!item.blocks.empty());
		item.blocks.front().text.text = std::move(text);
		last.listItems.push_back(std::move(item));
		return true;
	}
	const auto paragraph = reuseOrInsertParagraph(container, insertAt + count);
	if (!paragraph) {
		return false;
	}
	const auto target = richText(paragraph->leaf);
	if (!target) {
		return false;
	}
	target->text = std::move(text);
	return true;
}

std::optional<int> State::ensureTrailingParagraphActive() {
	return applyCheckedMutation(std::optional<int>(), [](State &candidate) {
		const auto result = candidate.ensureTrailingParagraphActiveUnchecked();
		return CheckedMutationResult<std::optional<int>>{
			.apply = result.has_value(),
			.result = result,
		};
	});
}

std::optional<int> State::ensureTrailingParagraphActiveUnchecked() {
	if (_richPage->blocks.empty()
		|| _richPage->blocks.back().kind != BlockKind::Paragraph) {
		clearTemporaryDownParagraph();
		_richPage->blocks.push_back(MakeParagraphBlock());
	}
	const auto path = BlockPath{
		.container = BlockContainerPath(),
		.index = int(_richPage->blocks.size()) - 1,
	};
	rebuild();
	const auto ordinal = textNodeOrdinal({
			.kind = LeafKind::BlockText,
			.block = path,
		});
	if (!setActiveTextByOrdinal(ordinal)) {
		ensureActiveTextOrdinal();
	}
	return (_activeTextOrdinal >= 0)
		? std::make_optional(_activeTextOrdinal)
		: std::nullopt;
}

void State::resyncAfterExternalRichPageMutation() {
	clearTemporaryDownParagraph();
	const auto activeLeaf = [&]() -> std::optional<LeafPath> {
		if (const auto descriptor = textNode(_activeTextOrdinal)) {
			return descriptor->leaf;
		}
		return std::nullopt;
	}();
	rebuild();
	if (activeLeaf && (textNodeOrdinal(*activeLeaf) >= 0)) {
		const auto activated = activateRebuiltLeaf(*activeLeaf);
		Assert(activated);
	} else {
		ensureActiveTextOrdinal();
	}
}

std::optional<int> State::moveActiveSpecialBlockDown() {
	return applyCheckedMutation(std::optional<int>(), [](State &candidate) {
		const auto result = candidate.moveActiveSpecialBlockDownUnchecked();
		return CheckedMutationResult<std::optional<int>>{
			.apply = result.has_value(),
			.result = result,
		};
	});
}

State::BoundaryTarget State::removeTemporaryDownParagraphAndMove() {
	return applyCheckedMutation(BoundaryTarget(), [](State &candidate) {
		const auto result
			= candidate.removeTemporaryDownParagraphAndMoveUnchecked();
		return CheckedMutationResult<BoundaryTarget>{
			.apply = (result.action != BoundaryAction::None),
			.result = result,
		};
	});
}

std::optional<int> State::moveActiveSpecialBlockDownUnchecked() {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor) {
		return std::nullopt;
	}
	auto target = std::optional<LeafPath>();
	auto trackTemporary = false;
	if (const auto quote = activeNonPullquoteQuote()) {
		if (descriptor->leaf.kind == LeafKind::BlockCaption
			&& descriptor->leaf.block == quote->path) {
			if (const auto paragraph = reuseOrInsertParagraph(
					quote->path.container,
					quote->path.index + 1)) {
				target = paragraph->leaf;
				trackTemporary = paragraph->inserted;
			}
		} else if (descriptor->leaf.kind == LeafKind::BlockText
			&& descriptor->leaf.block == quote->path) {
			const auto owner = block(quote->path);
			if (owner
				&& owner->kind == BlockKind::Quote
				&& owner->blocks.empty()) {
				target = LeafPath{
					.kind = LeafKind::BlockCaption,
					.block = quote->path,
				};
			}
		} else if (quote->activeLeafIsLastEditableBodyLeaf) {
			target = LeafPath{
				.kind = LeafKind::BlockCaption,
				.block = quote->path,
			};
		}
	} else if (descriptor->leaf.kind == LeafKind::BlockText) {
		const auto owner = block(descriptor->leaf.block);
		if (owner && owner->kind == BlockKind::Code) {
			if (const auto paragraph = reuseOrInsertParagraph(
					descriptor->leaf.block.container,
					descriptor->leaf.block.index + 1)) {
				target = paragraph->leaf;
				trackTemporary = paragraph->inserted;
			}
		}
	}
	if (!target) {
		return std::nullopt;
	}
	_temporaryDownParagraph = trackTemporary
		? std::make_optional(*target)
		: std::nullopt;
	rebuild();
	return activateRebuiltLeaf(*target);
}

auto State::captureRebuiltBoundaryTarget(
		const BoundaryTarget &target) const
-> std::optional<RebuiltBoundaryTarget> {
	switch (target.action) {
	case BoundaryAction::Text:
		if (const auto descriptor = textNode(target.textOrdinal)) {
			return RebuiltBoundaryTarget{
				.action = BoundaryAction::Text,
				.leaf = descriptor->leaf,
			};
		}
		break;
	case BoundaryAction::StructuralSelection:
		switch (target.structuralSelection.kind) {
		case PreparedEditSelectionKind::Blocks:
			if (const auto range = validateBlockRange(
					target.structuralSelection.blocks);
				range
				&& (range->till == range->from + 1)) {
				return RebuiltBoundaryTarget{
					.action = BoundaryAction::StructuralSelection,
					.block = {
						.container = range->container,
						.index = range->from,
					},
				};
			}
			break;
		case PreparedEditSelectionKind::ListItems:
			if (const auto range = validateListItemRange(
					target.structuralSelection.listItems);
				range
				&& (range->till == range->from + 1)) {
				return RebuiltBoundaryTarget{
					.action = BoundaryAction::StructuralSelection,
					.block = range->block,
					.listItemIndex = range->from,
				};
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return std::nullopt;
}

void State::shiftRebuiltBoundaryTargetAfterRemovedBlock(
		RebuiltBoundaryTarget &target,
		const BlockPath &removed) const {
	switch (target.action) {
	case BoundaryAction::Text:
		if (!ShiftBlockPathAfterRemovedBlock(target.leaf.block, removed)) {
			target = RebuiltBoundaryTarget();
		}
		break;
	case BoundaryAction::StructuralSelection:
		if (!ShiftBlockPathAfterRemovedBlock(target.block, removed)) {
			target = RebuiltBoundaryTarget();
		}
		break;
	default:
		target = RebuiltBoundaryTarget();
		break;
	}
}

State::BoundaryTarget State::materializeBoundaryTarget(
		const RebuiltBoundaryTarget &target) const {
	switch (target.action) {
	case BoundaryAction::Text:
		if (const auto ordinal = textNodeOrdinal(target.leaf); ordinal >= 0) {
			return {
				.action = BoundaryAction::Text,
				.textOrdinal = ordinal,
			};
		}
		break;
	case BoundaryAction::StructuralSelection:
		if (target.listItemIndex >= 0) {
			const auto owner = block(target.block);
			if (owner
				&& owner->kind == BlockKind::List
				&& target.listItemIndex < int(owner->listItems.size())
				&& CanEditBlocks(owner->listItems[target.listItemIndex].blocks)) {
				return {
					.action = BoundaryAction::StructuralSelection,
					.structuralSelection = preparedSelectionForListItem(
						target.block,
						target.listItemIndex),
				};
			}
		} else if (const auto owner = block(target.block);
			owner && CanEditBlock(*owner)) {
			return {
				.action = BoundaryAction::StructuralSelection,
				.structuralSelection = preparedSelectionForBlock(target.block),
			};
		}
		break;
	default:
		break;
	}
	return {};
}

State::BoundaryTarget State::removeTemporaryDownParagraphAndMoveUnchecked() {
	const auto tracked = _temporaryDownParagraph;
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!tracked
		|| !descriptor
		|| !(descriptor->leaf == *tracked)) {
		return {};
	}
	const auto owner = block(tracked->block);
	if (!owner || owner->kind != BlockKind::Paragraph) {
		clearTemporaryDownParagraph();
		return {};
	}
	if (!BlockIsEmpty(*owner)) {
		clearTemporaryDownParagraph();
		return {};
	}
	const auto next = boundaryTargetForLeaf(
		*tracked,
		descriptor,
		true,
		false);
	if (next.action == BoundaryAction::None) {
		return {};
	}
	auto rebuiltTarget = captureRebuiltBoundaryTarget(next);
	if (!rebuiltTarget) {
		return {};
	}
	const auto removed = tracked->block;
	if (!removeTarget({
			.kind = RemovalKind::Block,
			.block = removed,
		})) {
		return {};
	}
	shiftRebuiltBoundaryTargetAfterRemovedBlock(*rebuiltTarget, removed);
	if (rebuiltTarget->action == BoundaryAction::None) {
		return {};
	}
	rebuild();
	const auto materialized = materializeBoundaryTarget(*rebuiltTarget);
	if (materialized.action == BoundaryAction::Text) {
		if (!setActiveTextByOrdinal(materialized.textOrdinal)) {
			ensureActiveTextOrdinal();
		}
	}
	return materialized;
}

std::optional<int> State::handleActiveHeadingEnter() {
	return applyCheckedMutation(std::optional<int>(), [](State &candidate) {
		const auto result = candidate.handleActiveHeadingEnterUnchecked();
		return CheckedMutationResult<std::optional<int>>{
			.apply = result.has_value(),
			.result = result,
		};
	});
}

std::optional<int> State::handleActiveHeadingEnterUnchecked() {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor || descriptor->leaf.kind != LeafKind::BlockText) {
		return std::nullopt;
	}
	const auto path = descriptor->leaf.block;
	const auto blocks = blockContainer(path.container);
	if (!blocks
		|| path.index < 0
		|| path.index >= int(blocks->size())
		|| (*blocks)[path.index].kind != BlockKind::Heading) {
		return std::nullopt;
	}
	const auto insertAt = path.index + 1;
	if (insertAt < 0 || insertAt > int(blocks->size())) {
		return std::nullopt;
	}
	clearTemporaryDownParagraph();
	blocks->insert(blocks->begin() + insertAt, MakeParagraphBlock());
	const auto target = LeafPath{
		.kind = LeafKind::BlockText,
		.block = {
			.container = path.container,
			.index = insertAt,
		},
	};
	rebuild();
	return activateRebuiltLeaf(target);
}

std::optional<int> State::handleActiveListEnter() {
	return applyCheckedMutation(std::optional<int>(), [](State &candidate) {
		const auto result = candidate.handleActiveListEnterUnchecked();
		return CheckedMutationResult<std::optional<int>>{
			.apply = result.has_value(),
			.result = result,
		};
	});
}

std::optional<int> State::handleActiveListEnterUnchecked() {
	const auto surface = normalizeActiveListItemSurface();
	if (!surface) {
		return std::nullopt;
	}
	const auto owner = block(surface->path);
	const auto item = listItem(surface->path, surface->itemIndex);
	if (!owner || owner->kind != BlockKind::List || !item) {
		return std::nullopt;
	}
	auto target = std::optional<LeafPath>();
	const auto trailingEmpty = (surface->itemIndex + 1
			== int(owner->listItems.size()))
		&& (item->blocks.size() == 1)
		&& (item->blocks.front().kind == BlockKind::Paragraph)
		&& ListItemIsEmpty(*item);
	if (trailingEmpty) {
		clearTemporaryDownParagraph();
		owner->listItems.erase(owner->listItems.begin() + surface->itemIndex);
		if (owner->listItems.empty()) {
			const auto blocks = blockContainer(surface->path.container);
			if (!blocks
				|| surface->path.index < 0
					|| surface->path.index >= int(blocks->size())) {
				return std::nullopt;
			}
			clearTemporaryDownParagraph();
			blocks->erase(blocks->begin() + surface->path.index);
			if (const auto paragraph = reuseOrInsertParagraph(
					surface->path.container,
					surface->path.index)) {
				target = paragraph->leaf;
			}
		} else {
			if (const auto paragraph = reuseOrInsertParagraph(
					surface->path.container,
					surface->path.index + 1)) {
				target = paragraph->leaf;
			}
		}
	} else {
		clearTemporaryDownParagraph();
		owner->listItems.insert(
			owner->listItems.begin() + surface->itemIndex + 1,
			MakeParagraphListItem(item->taskState));
		target = LeafPath{
			.kind = LeafKind::BlockText,
			.block = {
				.container = ListItemChildrenContainer(
					surface->path,
					surface->itemIndex + 1),
				.index = 0,
			},
		};
	}
	if (!target) {
		return std::nullopt;
	}
	rebuild();
	return activateRebuiltLeaf(*target);
}

bool State::pasteClipboardListItemsAfterActive(
		const ClipboardListItemsData &data,
		std::optional<ActiveTextInsertContext> context) {
	if (data.items.empty()) {
		return false;
	}
	return applyCheckedMutation(false, [
			data,
			context = std::move(context)](State &candidate) mutable {
		auto block = Block();
		block.kind = BlockKind::List;
		block.listKind = data.listKind;
		block.listItems = data.items;
		auto blocks = std::vector<Block>();
		blocks.push_back(std::move(block));
		candidate.normalizeInsertedBlockAnchors(blocks);

		const auto sameList = [&] {
			const auto descriptor = candidate.textNode(
				candidate._activeTextOrdinal);
			const auto surface = candidate.activeListItemSurface();
			auto owner = surface ? candidate.block(surface->path) : nullptr;
			if (!descriptor
				|| !surface
				|| !owner
				|| owner->kind != BlockKind::List) {
				return false;
			}
			const auto taskList = std::any_of(
				owner->listItems.begin(),
				owner->listItems.end(),
				[](const ListItem &item) {
					return item.taskState != TaskState::None;
				});
			if (owner->listKind != data.listKind || (taskList != data.taskList)) {
				return false;
			}
			auto insertContext = context
				? *context
				: ActiveTextInsertContext{
					.before = candidate.activeText(),
				};
			const auto itemContainer = ListItemChildrenContainer(
				surface->path,
				surface->itemIndex);
			auto activeBlockIndex = -1;
			switch (descriptor->leaf.kind) {
			case LeafKind::ListItemText:
				break;
			case LeafKind::BlockText:
				if (descriptor->leaf.block.container != itemContainer) {
					return false;
				}
				activeBlockIndex = descriptor->leaf.block.index;
				break;
			default:
				return false;
			}
			if (const auto normalized
				= candidate.normalizeTextOnlyListItemForInsertion(
					itemContainer)) {
				if (descriptor->leaf.kind == LeafKind::ListItemText) {
					activeBlockIndex = *normalized;
				} else if (*normalized >= 0) {
					++activeBlockIndex;
				}
			} else if (descriptor->leaf.kind == LeafKind::ListItemText) {
				activeBlockIndex = -1;
			}
			owner = candidate.block(surface->path);
			if (!owner
				|| owner->kind != BlockKind::List
				|| surface->itemIndex < 0
				|| surface->itemIndex >= int(owner->listItems.size())) {
				return false;
			}
			const auto item = &owner->listItems[surface->itemIndex];
			if (activeBlockIndex >= 0) {
				if (activeBlockIndex >= int(item->blocks.size())
					|| item->blocks[activeBlockIndex].kind
						!= BlockKind::Paragraph) {
					return false;
				}
			} else if (!item->blocks.empty()) {
				return false;
			}

			enum class OriginalParagraphSide {
				None,
				Leading,
				Trailing,
			};

			enum class OriginalItemSide {
				None,
				Leading,
				Trailing,
			};

			const auto makeParagraph = [](TextWithEntities text) {
				auto paragraph = State::MakeParagraphBlock();
				paragraph.text.text = std::move(text);
				return paragraph;
			};

			candidate.clearTemporaryDownParagraph();
			auto current = std::move(owner->listItems[surface->itemIndex]);
			auto insertedItems = std::move(blocks.front().listItems);
			const auto insertedCount = int(insertedItems.size());
			auto leading = ListItem();
			leading.taskState = current.taskState;
			auto trailing = ListItem();
			trailing.taskState = current.taskState;
			auto activeParagraph = Block();
			if (activeBlockIndex >= 0) {
				activeParagraph = std::move(current.blocks[activeBlockIndex]);
			}
			for (auto i = 0; i < std::max(activeBlockIndex, 0); ++i) {
				leading.blocks.push_back(std::move(current.blocks[i]));
			}
			auto originalParagraphSide = OriginalParagraphSide::None;
			if (activeBlockIndex >= 0 && !insertContext.before.text.isEmpty()) {
				activeParagraph.text.text = std::move(insertContext.before);
				leading.blocks.push_back(std::move(activeParagraph));
				originalParagraphSide = OriginalParagraphSide::Leading;
			}
			if (activeBlockIndex >= 0 && !insertContext.after.text.isEmpty()) {
				if (originalParagraphSide == OriginalParagraphSide::None) {
					activeParagraph.text.text = std::move(insertContext.after);
					trailing.blocks.push_back(std::move(activeParagraph));
					originalParagraphSide = OriginalParagraphSide::Trailing;
				} else {
					trailing.blocks.push_back(makeParagraph(
						std::move(insertContext.after)));
				}
			}
			if (activeBlockIndex >= 0) {
				for (auto i = activeBlockIndex + 1;
					i < int(current.blocks.size());
					++i) {
					trailing.blocks.push_back(std::move(current.blocks[i]));
				}
			}
			auto originalItemSide = OriginalItemSide::None;
			switch (originalParagraphSide) {
			case OriginalParagraphSide::Leading:
				originalItemSide = OriginalItemSide::Leading;
				break;
			case OriginalParagraphSide::Trailing:
				originalItemSide = OriginalItemSide::Trailing;
				break;
			case OriginalParagraphSide::None:
				break;
			}
			const auto keepLeading = !State::ListItemIsEmpty(leading);
			const auto keepTrailing = !State::ListItemIsEmpty(trailing);
			if (originalItemSide == OriginalItemSide::None) {
				if (keepLeading) {
					originalItemSide = OriginalItemSide::Leading;
				} else if (keepTrailing) {
					originalItemSide = OriginalItemSide::Trailing;
				}
			}
			if (originalItemSide == OriginalItemSide::Leading) {
				leading.number = std::move(current.number);
				leading.anchorId = std::move(current.anchorId);
				leading.text = std::move(current.text);
			} else if (originalItemSide == OriginalItemSide::Trailing) {
				trailing.number = std::move(current.number);
				trailing.anchorId = std::move(current.anchorId);
				trailing.text = std::move(current.text);
			}

			auto replacement = std::vector<ListItem>();
			replacement.reserve(
				insertedCount
				+ (keepLeading ? 1 : 0)
				+ (keepTrailing ? 1 : 0));
			if (keepLeading) {
				replacement.push_back(std::move(leading));
			}
			const auto insertedFrom = surface->itemIndex + int(keepLeading);
			replacement.insert(
				replacement.end(),
				std::make_move_iterator(insertedItems.begin()),
				std::make_move_iterator(insertedItems.end()));
			if (keepTrailing) {
				replacement.push_back(std::move(trailing));
			}
			owner->listItems.erase(owner->listItems.begin() + surface->itemIndex);
			owner->listItems.insert(
				owner->listItems.begin() + surface->itemIndex,
				std::make_move_iterator(replacement.begin()),
				std::make_move_iterator(replacement.end()));
			candidate.rebuild();
			for (auto i = 0, count = candidate.textNodeCount(); i != count; ++i) {
				const auto itemIndex = ListItemIndexForLeaf(
					candidate._textNodes[i].leaf,
					surface->path);
				if (itemIndex
					&& (*itemIndex >= insertedFrom)
					&& (*itemIndex < insertedFrom + insertedCount)
					&& candidate.setActiveTextByOrdinal(i)) {
					return true;
				}
			}
			candidate.ensureActiveTextOrdinal();
			return true;
		}();
		const auto applied = sameList
			? true
			: candidate.insertBlocksAfterActiveUnchecked(
				std::move(blocks),
				std::move(context));
		return CheckedMutationResult<bool>{
			.apply = applied,
			.result = applied,
		};
	});
}

bool State::replaceStructuralSelectionWithBlock(
		const Markdown::PreparedEditSelection &selection,
		InsertAction action,
		std::optional<ActiveTextInsertContext> context) {
	_lastLimitError = std::nullopt;
	auto candidate = State(
		std::make_shared<RichPage>(*_richPage),
		_mediaRuntime,
		_limits);
	candidate._activeTextOrdinal = _activeTextOrdinal;
	candidate._lastLimitError = std::nullopt;
	candidate._temporaryDownParagraph = _temporaryDownParagraph;
	if (!candidate.removeStructuralSelection(selection, true)) {
		_lastLimitError = candidate._lastLimitError;
		return false;
	}
	if (!candidate.insertBlockAfterActive(action, std::move(context))) {
		_lastLimitError = candidate._lastLimitError;
		return false;
	}
	commitCheckedMutation(std::move(candidate));
	return true;
}

bool State::replaceStructuralSelectionWithPreparedBlocks(
		const Markdown::PreparedEditSelection &selection,
		std::vector<Block> blocks,
		std::optional<ActiveTextInsertContext> context) {
	_lastLimitError = std::nullopt;
	auto candidate = State(
		std::make_shared<RichPage>(*_richPage),
		_mediaRuntime,
		_limits);
	candidate._activeTextOrdinal = _activeTextOrdinal;
	candidate._lastLimitError = std::nullopt;
	candidate._temporaryDownParagraph = _temporaryDownParagraph;
	if (!candidate.removeStructuralSelection(selection, true)) {
		_lastLimitError = candidate._lastLimitError;
		return false;
	}
	if (!candidate.insertPreparedBlocksAfterActive(
			std::move(blocks),
			std::move(context))) {
		_lastLimitError = candidate._lastLimitError;
		return false;
	}
	commitCheckedMutation(std::move(candidate));
	return true;
}

bool State::replaceStructuralSelectionWithClipboardListItems(
		const Markdown::PreparedEditSelection &selection,
		const ClipboardListItemsData &data,
		std::optional<ActiveTextInsertContext> context) {
	_lastLimitError = std::nullopt;
	auto candidate = State(
		std::make_shared<RichPage>(*_richPage),
		_mediaRuntime,
		_limits);
	candidate._activeTextOrdinal = _activeTextOrdinal;
	candidate._lastLimitError = std::nullopt;
	candidate._temporaryDownParagraph = _temporaryDownParagraph;
	if (!candidate.removeStructuralSelection(selection, true)) {
		_lastLimitError = candidate._lastLimitError;
		return false;
	}
	if (!candidate.pasteClipboardListItemsAfterActive(
			data,
			std::move(context))) {
		_lastLimitError = candidate._lastLimitError;
		return false;
	}
	commitCheckedMutation(std::move(candidate));
	return true;
}

void State::insertHeading1AfterActive() {
	(void)insertBlockAfterActive({
		.type = InsertBlockType::Heading,
		.headingLevel = 1,
	});
}

void State::insertBlockquoteAfterActive() {
	(void)insertBlockAfterActive({
		.type = InsertBlockType::Blockquote,
	});
}

bool State::insertBlocksAfterActiveWithContextUnchecked(
		std::vector<Block> &blocks,
		const ActiveTextInsertContext &context) {
	const auto descriptor = textNode(_activeTextOrdinal);
	if (!descriptor || descriptor->leaf.kind == LeafKind::TableCellText) {
		return false;
	}
	const auto target = resolveActiveTextInsertTarget();
	if (!target) {
		return false;
	}
	auto container = target->anchor.container;
	auto insertAt = target->anchor.blockIndex + 1;
	auto removeSource = false;
	if (target->leaf.kind == LeafKind::BlockText) {
		if (const auto owner = block(target->leaf.block);
			owner
			&& context.before.text.isEmpty()
			&& ((owner->kind == BlockKind::Paragraph)
				|| (owner->kind == BlockKind::Heading))) {
			removeSource = true;
			container = target->leaf.block.container;
			insertAt = target->leaf.block.index;
		}
	}
	auto *destination = blockContainer(container);
	if (!destination) {
		return false;
	}
	if (removeSource) {
		if (insertAt < 0 || insertAt >= int(destination->size())) {
			return false;
		}
	} else {
		const auto current = richText(target->leaf);
		if (!current) {
			return false;
		}
		current->text = context.before;
	}
	(void)seedInsertedBlocks(blocks, context.selected);
	if (removeSource) {
		destination->erase(destination->begin() + insertAt);
	}
	const auto count = int(blocks.size());
	if (insertAt < 0 || insertAt > int(destination->size())) {
		return false;
	}
	destination->insert(
		destination->begin() + insertAt,
		std::make_move_iterator(blocks.begin()),
		std::make_move_iterator(blocks.end()));
	if (!appendInsertedTrailingText(
			container,
			insertAt,
			count,
			context.after)) {
		return false;
	}
	rebuild();
	focusInsertedBlocks(container, insertAt, count);
	return true;
}

bool State::insertBlockAfterActive(
		InsertAction action,
		std::optional<ActiveTextInsertContext> context) {
	return applyCheckedMutation(false, [action, context = std::move(context)](
			State &candidate) mutable {
		auto blocks = std::vector<Block>();
		blocks.push_back(candidate.makeBlock(action));
		const auto applied = candidate.insertBlocksAfterActiveUnchecked(
			std::move(blocks),
			std::move(context));
		return CheckedMutationResult<bool>{
			.apply = applied,
			.result = applied,
		};
	});
}

bool State::insertPreparedBlockAfterActive(Block block) {
	auto blocks = std::vector<Block>();
	blocks.push_back(std::move(block));
	return insertPreparedBlocksAfterActive(std::move(blocks));
}

bool State::insertPreparedBlocksAfterActive(
		std::vector<Block> blocks,
		std::optional<ActiveTextInsertContext> context) {
	return applyCheckedMutation(false, [
			blocks = std::move(blocks),
			context = std::move(context)](State &candidate) mutable {
		const auto applied = candidate.insertBlocksAfterActiveUnchecked(
			std::move(blocks),
			std::move(context));
		return CheckedMutationResult<bool>{
			.apply = applied,
			.result = applied,
		};
	});
}

bool State::insertBlocksAfterActiveUnchecked(
		std::vector<Block> blocks,
		std::optional<ActiveTextInsertContext> context) {
	if (blocks.empty()) {
		return false;
	}
	clearTemporaryDownParagraph();
	normalizeInsertedBlockAnchors(blocks);
	if (context) {
		if (insertBlocksAfterActiveWithContextUnchecked(blocks, *context)) {
			return true;
		}
		if (applyActiveTextUnchecked(JoinText(
				context->before,
				context->selected,
				context->after)) == ApplyResult::Failed) {
			return false;
		}
	}
	const auto descriptor = textNode(_activeTextOrdinal);
	if (descriptor && shouldReplaceActiveTextOnlyBlock(*descriptor, blocks)) {
		const auto path = descriptor->removalTarget.block;
		const auto container = blockContainer(path.container);
		if (container
			&& path.index >= 0
			&& path.index < int(container->size())) {
			const auto insertAt = path.index;
			const auto count = int(blocks.size());
			container->erase(container->begin() + insertAt);
			container->insert(
				container->begin() + insertAt,
				std::make_move_iterator(blocks.begin()),
				std::make_move_iterator(blocks.end()));
			rebuild();
			focusInsertedBlocks(path.container, insertAt, count);
			return true;
		}
	}
	auto anchor = resolveActiveInsertionTarget();
	if (const auto normalized = normalizeTextOnlyListItemForInsertion(
			anchor.container)) {
		anchor.blockIndex = *normalized;
	} else if (const auto normalized = normalizeTextOnlyQuoteForInsertion(
			anchor.container)) {
		anchor.blockIndex = *normalized;
	}
	auto *container = blockContainer(anchor.container);
	if (!container) {
		anchor = InsertionAnchor{
			.container = BlockContainerPath(),
			.blockIndex = int(_richPage->blocks.size()) - 1,
		};
		container = &_richPage->blocks;
	}
	const auto insertAt = std::clamp(
		anchor.blockIndex + 1,
		0,
		int(container->size()));
	const auto count = int(blocks.size());
	container->insert(
		container->begin() + insertAt,
		std::make_move_iterator(blocks.begin()),
		std::make_move_iterator(blocks.end()));
	rebuild();
	focusInsertedBlocks(anchor.container, insertAt, count);
	return true;
}

std::vector<Block> *State::blockContainer(const BlockContainerPath &path) {
	auto *blocks = &_richPage->blocks;
	for (const auto &step : path.steps) {
		if (step.blockIndex < 0 || step.blockIndex >= int(blocks->size())) {
			return nullptr;
		}
		auto &parent = (*blocks)[step.blockIndex];
		if (step.kind == BlockContainerKind::BlockChildren) {
			blocks = &parent.blocks;
		} else if (step.kind == BlockContainerKind::ListItemChildren) {
			if (step.listItemIndex < 0
				|| step.listItemIndex >= int(parent.listItems.size())) {
				return nullptr;
			}
			blocks = &parent.listItems[step.listItemIndex].blocks;
		} else {
			return nullptr;
		}
	}
	return blocks;
}

const std::vector<Block> *State::blockContainer(
		const BlockContainerPath &path) const {
	const auto *blocks = &_richPage->blocks;
	for (const auto &step : path.steps) {
		if (step.blockIndex < 0 || step.blockIndex >= int(blocks->size())) {
			return nullptr;
		}
		const auto &parent = (*blocks)[step.blockIndex];
		if (step.kind == BlockContainerKind::BlockChildren) {
			blocks = &parent.blocks;
		} else if (step.kind == BlockContainerKind::ListItemChildren) {
			if (step.listItemIndex < 0
				|| step.listItemIndex >= int(parent.listItems.size())) {
				return nullptr;
			}
			blocks = &parent.listItems[step.listItemIndex].blocks;
		} else {
			return nullptr;
		}
	}
	return blocks;
}

Block *State::block(const BlockPath &path) {
	const auto blocks = blockContainer(path.container);
	if (!blocks || path.index < 0 || path.index >= int(blocks->size())) {
		return nullptr;
	}
	return &(*blocks)[path.index];
}

const Block *State::block(const BlockPath &path) const {
	const auto blocks = blockContainer(path.container);
	if (!blocks || path.index < 0 || path.index >= int(blocks->size())) {
		return nullptr;
	}
	return &(*blocks)[path.index];
}

ListItem *State::listItem(const BlockPath &blockPath, int itemIndex) {
	const auto list = block(blockPath);
	if (!list
		|| itemIndex < 0
		|| itemIndex >= int(list->listItems.size())) {
		return nullptr;
	}
	return &list->listItems[itemIndex];
}

const ListItem *State::listItem(
		const BlockPath &blockPath,
		int itemIndex) const {
	const auto list = block(blockPath);
	if (!list
		|| itemIndex < 0
		|| itemIndex >= int(list->listItems.size())) {
		return nullptr;
	}
	return &list->listItems[itemIndex];
}

TableCell *State::tableCell(
		const BlockPath &blockPath,
		int rowIndex,
		int cellIndex) {
	const auto table = block(blockPath);
	if (!table
		|| rowIndex < 0
		|| rowIndex >= int(table->tableRows.size())) {
		return nullptr;
	}
	auto &row = table->tableRows[rowIndex];
	if (cellIndex < 0 || cellIndex >= int(row.cells.size())) {
		return nullptr;
	}
	return &row.cells[cellIndex];
}

const TableCell *State::tableCell(
		const BlockPath &blockPath,
		int rowIndex,
		int cellIndex) const {
	const auto table = block(blockPath);
	if (!table
		|| rowIndex < 0
		|| rowIndex >= int(table->tableRows.size())) {
		return nullptr;
	}
	const auto &row = table->tableRows[rowIndex];
	if (cellIndex < 0 || cellIndex >= int(row.cells.size())) {
		return nullptr;
	}
	return &row.cells[cellIndex];
}

RichText *State::richText(const LeafPath &path) {
	switch (path.kind) {
	case LeafKind::BlockText:
		if (const auto owner = block(path.block)) {
			return &owner->text;
		}
		return nullptr;
	case LeafKind::BlockCaption:
		if (const auto owner = block(path.block)) {
			return &owner->caption;
		}
		return nullptr;
	case LeafKind::ListItemText:
		if (const auto item = listItem(path.block, path.listItemIndex)) {
			return &item->text;
		}
		return nullptr;
	case LeafKind::TableCellText:
		if (const auto cell = tableCell(
				path.block,
				path.tableRowIndex,
				path.tableCellIndex)) {
			return &cell->text;
		}
		return nullptr;
	case LeafKind::MathFormula:
		return nullptr;
	}
	return nullptr;
}

const RichText *State::richText(const LeafPath &path) const {
	switch (path.kind) {
	case LeafKind::BlockText:
		if (const auto owner = block(path.block)) {
			return &owner->text;
		}
		return nullptr;
	case LeafKind::BlockCaption:
		if (const auto owner = block(path.block)) {
			return &owner->caption;
		}
		return nullptr;
	case LeafKind::ListItemText:
		if (const auto item = listItem(path.block, path.listItemIndex)) {
			return &item->text;
		}
		return nullptr;
	case LeafKind::TableCellText:
		if (const auto cell = tableCell(
				path.block,
				path.tableRowIndex,
				path.tableCellIndex)) {
			return &cell->text;
		}
		return nullptr;
	case LeafKind::MathFormula:
		return nullptr;
	}
	return nullptr;
}

QString *State::rawText(const LeafPath &path) {
	if (path.kind != LeafKind::MathFormula) {
		return nullptr;
	}
	const auto owner = block(path.block);
	return owner ? &owner->formula : nullptr;
}

const QString *State::rawText(const LeafPath &path) const {
	if (path.kind != LeafKind::MathFormula) {
		return nullptr;
	}
	const auto owner = block(path.block);
	return owner ? &owner->formula : nullptr;
}

const TextNodeDescriptor *State::textNode(int ordinal) const {
	return (ordinal >= 0 && ordinal < textNodeCount())
		? &_textNodes[ordinal]
		: nullptr;
}

int State::textNodeOrdinal(const LeafPath &path) const {
	for (auto i = 0, count = textNodeCount(); i != count; ++i) {
		if (_textNodes[i].leaf == path) {
			return i;
		}
	}
	return -1;
}

bool State::leafMutationKeepsTextNodes(
		const TextNodeDescriptor &descriptor) const {
	switch (descriptor.leaf.kind) {
	case LeafKind::BlockText:
		if (const auto owner = block(descriptor.leaf.block)) {
			switch (owner->kind) {
			case BlockKind::Heading:
			case BlockKind::Paragraph:
			case BlockKind::Footer:
			case BlockKind::Code:
			case BlockKind::Details:
				return true;
			case BlockKind::Quote:
				return owner->blocks.empty();
			case BlockKind::Table:
				return !owner->text.text.text.isEmpty();
			default:
				return false;
			}
		}
		return false;
	case LeafKind::BlockCaption:
	case LeafKind::TableCellText:
	case LeafKind::MathFormula:
		return true;
	case LeafKind::ListItemText:
		if (const auto item = listItem(
				descriptor.leaf.block,
				descriptor.leaf.listItemIndex)) {
			return !RichTextIsEmpty(item->text) || item->blocks.empty();
		}
		return false;
	}
	return false;
}

bool State::updatePreparedActiveLeaf(
		const TextNodeDescriptor &descriptor) {
	const auto source = convertPreparedLeafSource(descriptor);
	if (!source) {
		return false;
	}
	return (Markdown::UpdatePreparedNativeInstantViewLeaf(
		&_prepared,
		*_richPage,
		*source) == NativeInstantViewLeafUpdateResult::Updated);
}

void State::rebuild() {
	_lastPreparedMutationKind = PreparedMutationKind::FullRebuild;
	rebuildTextNodes();
	ensureEditableNodes();
	ensureActiveTextOrdinal();
	clearTemporaryDownParagraphIfInvalid();
	rebuildPrepared();
}

void State::rebuildPrepared() {
	_lastPreparedMutationKind = PreparedMutationKind::FullRebuild;
	_prepared = Markdown::TryPrepareNativeInstantView({
		.richPage = _richPage,
		.mediaRuntime = _mediaRuntime,
		.editMode = true,
	}).content;
}

void State::rebuildTextNodes() {
	_textNodes.clear();
	_textNodes.reserve(_richPage->blocks.size() * 3);
	rebuildTextNodes(_richPage->blocks, BlockContainerPath());
}

void State::rebuildTextNodes(
		const std::vector<Block> &blocks,
		const BlockContainerPath &container) {
	for (auto i = 0, count = int(blocks.size()); i != count; ++i) {
		const auto path = BlockPath{
			.container = container,
			.index = i,
		};
		const auto &block = blocks[i];
		switch (block.kind) {
		case BlockKind::Heading:
		case BlockKind::Paragraph:
		case BlockKind::Footer:
		case BlockKind::Code:
			appendBlockTextNode(path, LeafKind::BlockText);
			break;
		case BlockKind::Quote:
			if (block.blocks.empty()) {
				appendBlockTextNode(
					path,
					LeafKind::BlockText,
					FieldMode::Rich,
					!block.pullquote
						? std::make_optional(InsertionAnchor{
							.container = BlockChildrenContainer(path),
							.blockIndex = -1,
						})
						: std::nullopt);
			}
			rebuildTextNodes(block.blocks, BlockChildrenContainer(path));
			appendBlockTextNode(path, LeafKind::BlockCaption);
			break;
		case BlockKind::List:
			for (auto j = 0, itemCount = int(block.listItems.size());
					j != itemCount;
					++j) {
				const auto &item = block.listItems[j];
				if (!RichTextIsEmpty(item.text) || item.blocks.empty()) {
					appendListItemTextNode(path, j);
				}
				if (!item.blocks.empty()) {
					rebuildTextNodes(
						item.blocks,
						ListItemChildrenContainer(path, j));
				}
			}
			break;
		case BlockKind::Photo:
		case BlockKind::Video:
		case BlockKind::Audio:
		case BlockKind::Map:
			appendBlockTextNode(path, LeafKind::BlockCaption);
			break;
		case BlockKind::Math:
			appendBlockTextNode(path, LeafKind::MathFormula, FieldMode::Raw);
			break;
		case BlockKind::Table:
			appendBlockTextNode(path, LeafKind::BlockText);
			for (auto j = 0, rowCount = int(block.tableRows.size());
					j != rowCount;
					++j) {
				const auto &row = block.tableRows[j];
				for (auto k = 0, cellCount = int(row.cells.size());
						k != cellCount;
						++k) {
					appendTableCellTextNode(path, j, k);
				}
			}
			break;
		case BlockKind::Details:
			appendBlockTextNode(
				path,
				LeafKind::BlockText,
				FieldMode::Rich,
				InsertionAnchor{
					.container = BlockChildrenContainer(path),
					.blockIndex = -1,
				});
			rebuildTextNodes(block.blocks, BlockChildrenContainer(path));
			break;
		default:
			rebuildTextNodes(block.blocks, BlockChildrenContainer(path));
			break;
		}
	}
}

void State::appendBlockTextNode(
		const BlockPath &path,
		LeafKind kind,
		FieldMode mode,
		std::optional<InsertionAnchor> insertionAnchor) {
	_textNodes.push_back({
		.leaf = {
			.kind = kind,
			.block = path,
		},
		.insertionAnchor = insertionAnchor.value_or(InsertionAnchor{
			.container = path.container,
			.blockIndex = path.index,
		}),
		.removalTarget = {
			.kind = RemovalKind::Block,
			.block = path,
		},
		.mode = mode,
	});
}

void State::appendListItemTextNode(const BlockPath &path, int itemIndex) {
	_textNodes.push_back({
		.leaf = {
			.kind = LeafKind::ListItemText,
			.block = path,
			.listItemIndex = itemIndex,
		},
		.insertionAnchor = {
			.container = ListItemChildrenContainer(path, itemIndex),
			.blockIndex = -1,
		},
		.removalTarget = {
			.kind = RemovalKind::ListItem,
			.block = path,
			.listItemIndex = itemIndex,
		},
		.mode = FieldMode::Rich,
	});
}

void State::appendTableCellTextNode(
		const BlockPath &path,
		int rowIndex,
		int cellIndex) {
	_textNodes.push_back({
		.leaf = {
			.kind = LeafKind::TableCellText,
			.block = path,
			.tableRowIndex = rowIndex,
			.tableCellIndex = cellIndex,
		},
		.insertionAnchor = {
			.container = path.container,
			.blockIndex = path.index,
		},
		.removalTarget = {
			.kind = RemovalKind::TableCell,
			.block = path,
			.tableRowIndex = rowIndex,
			.tableCellIndex = cellIndex,
		},
		.mode = FieldMode::Rich,
	});
}

void State::ensureActiveTextOrdinal() {
	if (_textNodes.empty()) {
		_activeTextOrdinal = -1;
	} else if (_activeTextOrdinal < 0 || _activeTextOrdinal >= textNodeCount()) {
		_activeTextOrdinal = 0;
	}
}

void State::ensureEditableNodes() {
	if (!_textNodes.empty()) {
		return;
	}
	_richPage->blocks.push_back(MakeParagraphBlock());
	rebuildTextNodes();
}

void State::focusInsertedBlocks(
		const BlockContainerPath &container,
		int from,
		int count) {
	for (auto blockIndex = from; blockIndex != from + count; ++blockIndex) {
		const auto path = BlockPath{
			.container = container,
			.index = blockIndex,
		};
		for (auto i = 0, textCount = textNodeCount(); i != textCount; ++i) {
			if (descriptorBelongsToBlock(_textNodes[i], path)
				&& setActiveTextByOrdinal(i)) {
				return;
			}
		}
	}
	ensureActiveTextOrdinal();
}

std::optional<int> State::adjacentEditableOrdinal(bool forward) const {
	if (_activeTextOrdinal < 0) {
		return std::nullopt;
	}
	const auto ordinal = _activeTextOrdinal + (forward ? 1 : -1);
	return (ordinal >= 0 && ordinal < textNodeCount())
		? std::make_optional(ordinal)
		: std::nullopt;
}

void State::collectBoundarySteps(
		const std::vector<Block> &blocks,
		const BlockContainerPath &container,
		bool forward,
		std::vector<BoundaryTarget> *steps) const {
	const auto collectBlock = [&](int index) {
		const auto path = BlockPath{
			.container = container,
			.index = index,
		};
		const auto &block = blocks[index];
		switch (block.kind) {
		case BlockKind::Heading:
		case BlockKind::Paragraph:
		case BlockKind::Footer:
		case BlockKind::Code:
			appendBoundaryTextStep({
				.kind = LeafKind::BlockText,
				.block = path,
			}, steps);
			break;
		case BlockKind::Quote:
			if (forward) {
				if (block.blocks.empty()) {
					appendBoundaryTextStep({
						.kind = LeafKind::BlockText,
						.block = path,
					}, steps);
				}
				appendBoundaryTextStep({
					.kind = LeafKind::BlockCaption,
					.block = path,
				}, steps);
				collectBoundarySteps(
					block.blocks,
					BlockChildrenContainer(path),
					forward,
					steps);
			} else {
				collectBoundarySteps(
					block.blocks,
					BlockChildrenContainer(path),
					forward,
					steps);
				appendBoundaryTextStep({
					.kind = LeafKind::BlockCaption,
					.block = path,
				}, steps);
				if (block.blocks.empty()) {
					appendBoundaryTextStep({
						.kind = LeafKind::BlockText,
						.block = path,
					}, steps);
				}
			}
			appendBoundaryBlockStep(path, steps);
			break;
		case BlockKind::List:
			if (forward) {
				for (auto j = 0, itemCount = int(block.listItems.size());
						j != itemCount;
						++j) {
					const auto &item = block.listItems[j];
					if (!RichTextIsEmpty(item.text) || item.blocks.empty()) {
						appendBoundaryTextStep({
							.kind = LeafKind::ListItemText,
							.block = path,
							.listItemIndex = j,
						}, steps);
					}
					if (!item.blocks.empty()) {
						collectBoundarySteps(
							item.blocks,
							ListItemChildrenContainer(path, j),
							forward,
							steps);
						appendBoundaryListItemStep(path, j, steps);
					}
				}
			} else {
				for (auto j = int(block.listItems.size()); j != 0; --j) {
					const auto itemIndex = j - 1;
					const auto &item = block.listItems[itemIndex];
					if (!item.blocks.empty()) {
						collectBoundarySteps(
							item.blocks,
							ListItemChildrenContainer(path, itemIndex),
							forward,
							steps);
					}
					if (!RichTextIsEmpty(item.text) || item.blocks.empty()) {
						appendBoundaryTextStep({
							.kind = LeafKind::ListItemText,
							.block = path,
							.listItemIndex = itemIndex,
						}, steps);
					}
					if (!item.blocks.empty()) {
						appendBoundaryListItemStep(path, itemIndex, steps);
					}
				}
			}
			break;
		case BlockKind::Photo:
		case BlockKind::Video:
		case BlockKind::Audio:
		case BlockKind::Map:
			appendBoundaryTextStep({
				.kind = LeafKind::BlockCaption,
				.block = path,
			}, steps);
			appendBoundaryBlockStep(path, steps);
			break;
		case BlockKind::Math:
			appendBoundaryTextStep({
				.kind = LeafKind::MathFormula,
				.block = path,
			}, steps);
			break;
		case BlockKind::Table:
			if (forward) {
				if (!block.text.text.text.isEmpty()) {
					appendBoundaryTextStep({
						.kind = LeafKind::BlockText,
						.block = path,
					}, steps);
				}
				for (auto j = 0, rowCount = int(block.tableRows.size());
						j != rowCount;
						++j) {
					const auto &row = block.tableRows[j];
					for (auto k = 0, cellCount = int(row.cells.size());
							k != cellCount;
							++k) {
						appendBoundaryTextStep({
							.kind = LeafKind::TableCellText,
							.block = path,
							.tableRowIndex = j,
							.tableCellIndex = k,
						}, steps);
					}
				}
			} else {
				for (auto j = int(block.tableRows.size()); j != 0; --j) {
					const auto rowIndex = j - 1;
					const auto &row = block.tableRows[rowIndex];
					for (auto k = int(row.cells.size()); k != 0; --k) {
						appendBoundaryTextStep({
							.kind = LeafKind::TableCellText,
							.block = path,
							.tableRowIndex = rowIndex,
							.tableCellIndex = k - 1,
						}, steps);
					}
				}
				if (!block.text.text.text.isEmpty()) {
					appendBoundaryTextStep({
						.kind = LeafKind::BlockText,
						.block = path,
					}, steps);
				}
			}
			appendBoundaryBlockStep(path, steps);
			break;
		case BlockKind::Details:
			if (forward) {
				appendBoundaryTextStep({
					.kind = LeafKind::BlockText,
					.block = path,
				}, steps);
				collectBoundarySteps(
					block.blocks,
					BlockChildrenContainer(path),
					forward,
					steps);
			} else {
				collectBoundarySteps(
					block.blocks,
					BlockChildrenContainer(path),
					forward,
					steps);
				appendBoundaryTextStep({
					.kind = LeafKind::BlockText,
					.block = path,
				}, steps);
			}
			appendBoundaryBlockStep(path, steps);
			break;
		default: {
			const auto before = steps->size();
			if (!block.blocks.empty()) {
				collectBoundarySteps(
					block.blocks,
					BlockChildrenContainer(path),
					forward,
					steps);
			}
			if (steps->size() == before) {
				appendBoundaryBlockStep(path, steps);
			}
		} break;
		}
	};
	if (forward) {
		for (auto i = 0, count = int(blocks.size()); i != count; ++i) {
			collectBlock(i);
		}
	} else {
		for (auto i = int(blocks.size()); i != 0; --i) {
			collectBlock(i - 1);
		}
	}
}

void State::appendBoundaryTextStep(
		LeafPath leaf,
		std::vector<BoundaryTarget> *steps) const {
	const auto ordinal = textNodeOrdinal(leaf);
	if (ordinal >= 0) {
		steps->push_back({
			.action = BoundaryAction::Text,
			.textOrdinal = ordinal,
		});
	}
}

void State::appendBoundaryBlockStep(
		const BlockPath &path,
		std::vector<BoundaryTarget> *steps) const {
	const auto owner = block(path);
	if (owner && CanEditBlock(*owner)) {
		steps->push_back({
			.action = BoundaryAction::StructuralSelection,
			.structuralSelection = preparedSelectionForBlock(path),
		});
	}
}

void State::appendBoundaryListItemStep(
		const BlockPath &path,
		int itemIndex,
		std::vector<BoundaryTarget> *steps) const {
	const auto owner = block(path);
	if (!owner
		|| owner->kind != BlockKind::List
		|| itemIndex < 0
		|| itemIndex >= int(owner->listItems.size())
		|| !CanEditBlocks(owner->listItems[itemIndex].blocks)) {
		return;
	}
	steps->push_back({
		.action = BoundaryAction::StructuralSelection,
		.structuralSelection = preparedSelectionForListItem(path, itemIndex),
	});
}

PreparedEditSelection State::preparedSelectionForBlock(
		const BlockPath &path) const {
	return {
		.kind = PreparedEditSelectionKind::Blocks,
		.blocks = {
			.container = ToPreparedBlockContainerPath(path.container),
			.from = path.index,
			.till = path.index + 1,
		},
	};
}

PreparedEditSelection State::preparedSelectionForListItem(
		const BlockPath &path,
		int itemIndex) const {
	return {
		.kind = PreparedEditSelectionKind::ListItems,
		.listItems = {
			.block = PreparedBlockPath{
				.container = ToPreparedBlockContainerPath(path.container),
				.index = path.index,
			},
			.from = itemIndex,
			.till = itemIndex + 1,
		},
	};
}

bool State::descriptorBelongsToBlock(
		const TextNodeDescriptor &descriptor,
		const BlockPath &path) const {
	return leafBelongsToBlock(descriptor.leaf, path);
}

bool State::removalTargetIsEmpty(const RemovalTarget &target) const {
	switch (target.kind) {
	case RemovalKind::Block:
		if (const auto owner = block(target.block)) {
			return BlockIsEmpty(*owner);
		}
		return false;
	case RemovalKind::ListItem:
		if (const auto item = listItem(target.block, target.listItemIndex)) {
			return ListItemIsEmpty(*item);
		}
		return false;
	case RemovalKind::TableCell:
		if (const auto cell = tableCell(
				target.block,
				target.tableRowIndex,
				target.tableCellIndex)) {
			return RichTextIsEmpty(cell->text);
		}
		return false;
	}
	return false;
}

bool State::removeTarget(const RemovalTarget &target) {
	clearTemporaryDownParagraph();
	switch (target.kind) {
	case RemovalKind::Block: {
		const auto blocks = blockContainer(target.block.container);
		if (!blocks
			|| target.block.index < 0
			|| target.block.index >= int(blocks->size())) {
			return false;
		}
		blocks->erase(blocks->begin() + target.block.index);
		return true;
	}
	case RemovalKind::ListItem: {
		const auto owner = block(target.block);
		if (!owner
			|| target.listItemIndex < 0
			|| target.listItemIndex >= int(owner->listItems.size())) {
			return false;
		}
		owner->listItems.erase(owner->listItems.begin() + target.listItemIndex);
		return true;
	}
	case RemovalKind::TableCell: {
		const auto cell = tableCell(
			target.block,
			target.tableRowIndex,
			target.tableCellIndex);
		if (!cell) {
			return false;
		}
		cell->text = RichText();
		return true;
	}
	}
	return false;
}

void State::normalizeInsertedBlockAnchors(std::vector<Block> &blocks) {
	for (auto &block : blocks) {
		normalizeInsertedBlockAnchors(blocks, block);
	}
}

void State::normalizeInsertedBlockAnchors(
		std::vector<Block> &root,
		Block &block) {
	const auto normalize = [&](QString &id) {
		if (id.isEmpty()) {
			return;
		}
		const auto base = id;
		id.clear();
		auto candidate = base;
		for (auto suffix = 2;
			anchorIdExists(candidate) || anchorIdExists(root, candidate);
			++suffix) {
			candidate = base + u"-%1"_q.arg(suffix);
		}
		id = std::move(candidate);
	};
	normalize(block.anchorId);
	normalizeInsertedRichTextAnchors(root, block.text);
	normalizeInsertedRichTextAnchors(root, block.caption);
	for (auto &child : block.blocks) {
		normalizeInsertedBlockAnchors(root, child);
	}
	for (auto &item : block.listItems) {
		normalize(item.anchorId);
		normalizeInsertedRichTextAnchors(root, item.text);
		for (auto &child : item.blocks) {
			normalizeInsertedBlockAnchors(root, child);
		}
	}
	for (auto &row : block.tableRows) {
		for (auto &cell : row.cells) {
			normalizeInsertedRichTextAnchors(root, cell.text);
		}
	}
}

void State::normalizeInsertedRichTextAnchors(
		std::vector<Block> &root,
		RichText &text) {
	const auto normalize = [&](QString &id) {
		if (id.isEmpty()) {
			return;
		}
		const auto base = id;
		id.clear();
		auto candidate = base;
		for (auto suffix = 2;
			anchorIdExists(candidate) || anchorIdExists(root, candidate);
			++suffix) {
			candidate = base + u"-%1"_q.arg(suffix);
		}
		id = std::move(candidate);
	};
	normalize(text.anchorId);
	for (auto &id : text.anchorIds) {
		normalize(id);
	}
}

bool State::anchorIdExists(const QString &id) const {
	return anchorIdExists(_richPage->blocks, id);
}

bool State::anchorIdExists(
		const std::vector<Block> &blocks,
		const QString &id) const {
	for (const auto &block : blocks) {
		if (block.anchorId == id
			|| block.text.anchorId == id
			|| block.caption.anchorId == id
			|| ranges::contains(block.text.anchorIds, id)
			|| ranges::contains(block.caption.anchorIds, id)) {
			return true;
		}
		if (anchorIdExists(block.blocks, id)) {
			return true;
		}
		for (const auto &item : block.listItems) {
			if (item.anchorId == id
				|| item.text.anchorId == id
				|| ranges::contains(item.text.anchorIds, id)) {
				return true;
			}
			if (anchorIdExists(item.blocks, id)) {
				return true;
			}
		}
		for (const auto &row : block.tableRows) {
			for (const auto &cell : row.cells) {
				if (cell.text.anchorId == id
					|| ranges::contains(cell.text.anchorIds, id)) {
					return true;
				}
			}
		}
	}
	return false;
}

QString State::nextAnchorId() const {
	for (auto i = 1;; ++i) {
		auto result = u"anchor-%1"_q.arg(i);
		if (!anchorIdExists(result)) {
			return result;
		}
	}
}

Block State::makeBlock(InsertAction action) const {
	switch (action.type) {
	case InsertBlockType::Heading:
		return MakeHeadingBlock(action.headingLevel);
	case InsertBlockType::Blockquote:
		return MakeQuoteBlock(false);
	case InsertBlockType::Code:
		return MakeCodeBlock();
	case InsertBlockType::Math:
		return MakeMathBlock();
	case InsertBlockType::Footer:
		return MakeFooterBlock();
	case InsertBlockType::Divider:
		return MakeDividerBlock();
	case InsertBlockType::Anchor:
		return MakeAnchorBlock(nextAnchorId());
	case InsertBlockType::OrderedList:
		return MakeListBlock(ListKind::Ordered);
	case InsertBlockType::BulletList:
		return MakeListBlock(ListKind::Bullet);
	case InsertBlockType::TaskList:
		return MakeListBlock(ListKind::Bullet, TaskState::Unchecked);
	case InsertBlockType::Pullquote:
		return MakeQuoteBlock(true);
	case InsertBlockType::Photo:
		return MakeMediaBlock(BlockKind::Photo);
	case InsertBlockType::Video:
		return MakeMediaBlock(BlockKind::Video);
	case InsertBlockType::Audio:
		return MakeMediaBlock(BlockKind::Audio);
	case InsertBlockType::Details:
		return MakeDetailsBlock();
	case InsertBlockType::Table:
		return MakeTableBlock();
	case InsertBlockType::Map:
		return MakeMapBlock(action.latitude, action.longitude);
	}
	return MakeParagraphBlock();
}

TextWithEntities State::MakeText(QString text) {
	auto result = TextWithEntities();
	result.text = std::move(text);
	return result;
}

Block State::MakeParagraphBlock() {
	auto block = Block();
	block.kind = BlockKind::Paragraph;
	return block;
}

Block State::MakeFooterBlock() {
	auto block = Block();
	block.kind = BlockKind::Footer;
	return block;
}

Block State::MakeHeadingBlock(int level) {
	auto block = Block();
	block.kind = BlockKind::Heading;
	block.headingLevel = std::clamp(level, 1, 6);
	return block;
}

Block State::MakeQuoteBlock(bool pullquote) {
	auto block = Block();
	block.kind = BlockKind::Quote;
	block.pullquote = pullquote;
	return block;
}

Block State::MakeCodeBlock() {
	auto block = Block();
	block.kind = BlockKind::Code;
	return block;
}

Block State::MakeMathBlock() {
	auto block = Block();
	block.kind = BlockKind::Math;
	return block;
}

Block State::MakeDividerBlock() {
	auto block = Block();
	block.kind = BlockKind::Divider;
	return block;
}

Block State::MakeAnchorBlock(QString anchorId) {
	auto block = Block();
	block.kind = BlockKind::Anchor;
	block.anchorId = std::move(anchorId);
	return block;
}

Block State::MakeListBlock(ListKind kind, TaskState taskState) {
	auto block = Block();
	block.kind = BlockKind::List;
	block.listKind = kind;
	block.listItems.reserve(3);
	for (auto i = 0; i != 3; ++i) {
		auto item = ListItem();
		item.taskState = taskState;
		block.listItems.push_back(std::move(item));
	}
	return block;
}

ListItem State::MakeParagraphListItem(TaskState taskState) {
	auto item = ListItem();
	item.taskState = taskState;
	item.blocks.push_back(MakeParagraphBlock());
	return item;
}

Block State::MakeDetailsBlock() {
	auto block = Block();
	block.kind = BlockKind::Details;
	block.blocks.push_back(MakeParagraphBlock());
	return block;
}

Block State::MakeTableBlock() {
	auto block = Block();
	block.kind = BlockKind::Table;
	block.bordered = true;
	block.tableRows.reserve(3);
	for (auto rowIndex = 0; rowIndex != 3; ++rowIndex) {
		auto row = RichPage::TableRow();
		row.cells.reserve(3);
		for (auto cellIndex = 0; cellIndex != 3; ++cellIndex) {
			auto cell = TableCell();
			cell.header = (rowIndex == 0);
			row.cells.push_back(std::move(cell));
		}
		block.tableRows.push_back(std::move(row));
	}
	return block;
}

Block State::MakeMediaBlock(BlockKind kind) {
	auto block = Block();
	block.kind = kind;
	return block;
}

Block State::MakeMapBlock(double latitude, double longitude) {
	auto block = Block();
	block.kind = BlockKind::Map;
	block.latitude = latitude;
	block.longitude = longitude;
	return block;
}

bool State::RichTextIsEmpty(const RichText &text) {
	return StringIsEmpty(text.text.text)
		&& text.anchorId.isEmpty()
		&& text.anchorIds.empty();
}

bool State::ListItemIsEmpty(const ListItem &item) {
	if (!RichTextIsEmpty(item.text) || !item.anchorId.isEmpty()) {
		return false;
	}
	for (const auto &block : item.blocks) {
		if (!BlockIsEmpty(block)) {
			return false;
		}
	}
	return true;
}

bool State::BlockIsEmpty(const Block &block) {
	if (!RichTextIsEmpty(block.text)
		|| !RichTextIsEmpty(block.caption)
		|| !StringIsEmpty(block.formula)
		|| !block.language.isEmpty()
		|| !block.anchorId.isEmpty()
		|| !block.url.isEmpty()
		|| !block.html.isEmpty()
		|| !block.author.isEmpty()
		|| !block.username.isEmpty()
		|| !block.channelTitle.isEmpty()
		|| !block.audioTitle.isEmpty()
		|| !block.audioPerformer.isEmpty()
		|| !block.audioFileName.isEmpty()
		|| block.photo
		|| block.document
		|| block.peer
		|| block.photoId
		|| block.documentId
		|| block.channelId
		|| block.accessHash
		|| block.latitude != 0.
		|| block.longitude != 0.
		|| !block.mediaItems.empty()) {
		return false;
	}
	for (const auto &child : block.blocks) {
		if (!BlockIsEmpty(child)) {
			return false;
		}
	}
	for (const auto &item : block.listItems) {
		if (!ListItemIsEmpty(item)) {
			return false;
		}
	}
	for (const auto &row : block.tableRows) {
		for (const auto &cell : row.cells) {
			if (!RichTextIsEmpty(cell.text)) {
				return false;
			}
		}
	}
	return true;
}

bool State::StripWrapperEntityInEditMode(EntityType type) {
	switch (type) {
	case EntityType::Url:
	case EntityType::Email:
	case EntityType::Hashtag:
	case EntityType::Cashtag:
	case EntityType::Mention:
	case EntityType::BotCommand:
	case EntityType::Phone:
	case EntityType::BankCard:
		return true;
	default:
		return false;
	}
}

TextWithEntities State::StripEditModeWrapperEntities(TextWithEntities text) {
	auto filtered = EntitiesInText();
	filtered.reserve(text.entities.size());
	for (const auto &entity : text.entities) {
		if (!StripWrapperEntityInEditMode(entity.type())) {
			filtered.push_back(entity);
		}
	}
	text.entities = std::move(filtered);
	return text;
}

bool CanEditRichPage(const RichPage &page) {
	return CanEditBlocks(page.blocks);
}

bool CanEditRichPage(const std::shared_ptr<const RichPage> &page) {
	return page && CanEditRichPage(*page);
}

} // namespace Iv::Editor
