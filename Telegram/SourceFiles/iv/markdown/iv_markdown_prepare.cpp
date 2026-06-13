/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_prepare.h"
#include "iv/iv_rich_page.h"
#include "iv/markdown/iv_markdown_prepare_blocks.h"
#include "iv/markdown/iv_markdown_prepare_formulas.h"
#include "iv/markdown/iv_markdown_prepare_native_blocks.h"
#include "iv/markdown/iv_markdown_prepare_native_richtext.h"
#include "iv/markdown/iv_markdown_prepare_state.h"

#include <QtCore/QElapsedTimer>

#include <algorithm>
#include <utility>

namespace Iv::Markdown {
namespace {

[[nodiscard]] int CountPreparedContentBlocks(
		const MarkdownArticleContent &content) {
	auto result = CountPreparedBlocks(content.blocks.blocks);
	for (const auto &footnote : content.footnotes) {
		result += CountPreparedBlocks(footnote.blocks);
	}
	return result;
}

[[nodiscard]] PreparedBlock ContentTooLongBlock() {
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Placeholder;
	block.placeholder.label = u"Content Too Long"_q;
	block.placeholder.copyText = block.placeholder.label;
	return block;
}

void KeepPreparedBlocksUpToLimit(
		std::vector<PreparedBlock> *blocks,
		int *remaining) {
	auto keep = size_t(0);
	for (; keep != blocks->size(); ++keep) {
		if (*remaining <= 0) {
			break;
		}
		--*remaining;
		KeepPreparedBlocksUpToLimit(&(*blocks)[keep].children, remaining);
	}
	if (keep != blocks->size()) {
		blocks->erase(blocks->begin() + keep, blocks->end());
	}
}

void ApplySoftPreparedBlockLimit(std::vector<PreparedBlock> *blocks) {
	const auto limit = PrepareLimitsForIv().maxPreparedBlocks;
	if (CountPreparedBlocks(*blocks) <= limit) {
		return;
	}
	auto remaining = std::max(limit - 1, 0);
	KeepPreparedBlocksUpToLimit(blocks, &remaining);
	blocks->push_back(ContentTooLongBlock());
}

} // namespace

const MarkdownPrepareLimits &PrepareLimitsForIv() {
	static const auto result = MarkdownPrepareLimits{
		.tableRender = {
			.maxRows = 128,
			.maxColumns = 20,
			.maxCells = 1024,
		},
		.visualListDepth = 16,
		.visualQuoteDepth = 16,
		.maxPreparedBlocks = 4096,
	};
	return result;
}

const MarkdownPrepareTableRenderLimits &PrepareTableRenderLimitsForIv() {
	return PrepareLimitsForIv().tableRender;
}

MarkdownArticleContent PrepareSynchronously(PrepareRequest request) {
	auto state = PrepareState();
	auto timer = QElapsedTimer();
	timer.start();
	state.request = &request;
	const auto finish = [&] {
		state.result.debug.prepareMs = int(timer.elapsed());
		return std::move(state.result);
	};

	if (!request.document) {
		state.setTerminalFailure(
			PrepareTerminalFailure::InvalidRequest,
			u"missing-document"_q);
		return finish();
	}
	if (const auto invalidStyle = InvalidStyleReason(request.dimensions)
		; !invalidStyle.isEmpty()) {
		state.setTerminalFailure(
			PrepareTerminalFailure::InvalidStyle,
			invalidStyle);
		return finish();
	}

	state.sourceUtf8 = request.document->sourceText.toUtf8();
	state.result.formulas.resize(FormulaSlotCount(*request.document));
	state.result.debug.sourceWarningCount = int(request.document->warnings.size());

	state.result.blocks = PrepareRenderData(*request.document, &state);
	if (CountPreparedContentBlocks(state.result)
		> PrepareLimitsForIv().maxPreparedBlocks) {
		state.setTerminalFailure(
			PrepareTerminalFailure::DocumentTooLarge,
			u"prepared-block-limit"_q);
		ClearPreparedOutput(&state.result);
		return finish();
	}
	MeasurePreparedFormulas(&state);
	if (state.result.failure.failed()) {
		ClearPreparedOutput(&state.result);
	}
	return finish();
}

NativeInstantViewPrepareResult TryPrepareNativeInstantView(
		NativeInstantViewPrepareRequest request) {
	auto state = NativeIvPrepareState();
	auto timer = QElapsedTimer();
	timer.start();
	state.result.mediaRuntime = std::move(request.mediaRuntime);
	state.result.editMode = request.editMode;
	state.dimensions = request.dimensionsOverride.value_or(
		CaptureMarkdownPrepareDimensions());
	state.editMode = request.editMode;
	const auto finish = [&](NativeInstantViewPrepareResultKind kind, QString reason) {
		state.result.debug.prepareMs = int(timer.elapsed());
		return NativeInstantViewPrepareResult{
			.kind = kind,
			.content = std::move(state.result),
			.debugReason = std::move(reason),
		};
	};

	if (!request.richPage) {
		state.setFailure(
			PrepareTerminalFailure::InvalidRequest,
			u"missing-native-iv-rich-page"_q);
		ClearPreparedOutput(&state.result);
		return finish(
			NativeInstantViewPrepareResultKind::Failure,
			state.result.failure.debugReason);
	}

	if (!PrepareNativeIvBlocks(
			*request.richPage,
			&state.result.blocks.blocks,
			&state)) {
		if (state.result.failure.failed()) {
			ClearPreparedOutput(&state.result);
			return finish(
				NativeInstantViewPrepareResultKind::Failure,
				state.result.failure.debugReason);
		}
		(void)PrepareNativeIvPlainPlaceholderBlock(
			u"Prepare Failed"_q,
			&state.result.blocks.blocks);
	}
	ApplySoftPreparedBlockLimit(&state.result.blocks.blocks);
	MeasureNativeIvPreparedFormulas(&state);
	if (state.result.failure.failed()) {
		ClearPreparedOutput(&state.result);
		return finish(
			NativeInstantViewPrepareResultKind::Failure,
			state.result.failure.debugReason);
	}
	return finish(
		NativeInstantViewPrepareResultKind::Supported,
		QString());
}

NativeInstantViewLeafUpdateResult UpdatePreparedNativeInstantViewLeaf(
		MarkdownArticleContent *content,
		const RichPage &page,
		const PreparedEditLeafSource &source) {
	if (!content) {
		return NativeInstantViewLeafUpdateResult::Failed;
	}
	auto state = NativeIvPrepareState();
	state.result.mediaRuntime = content->mediaRuntime;
	state.result.editMode = content->editMode;
	state.result.formulas = content->formulas;
	state.dimensions = CaptureMarkdownPrepareDimensions();
	state.editMode = content->editMode;
	state.nextFormulaIndex = int(content->formulas.size());
	auto blocks = content->blocks.blocks;
	auto formulaRange = NativeIvPreparedLeafFormulaRange{
		.from = state.nextFormulaIndex,
		.till = state.nextFormulaIndex,
	};
	const auto updated = UpdatePreparedNativeIvLeaf(
		&blocks,
		page,
		source,
		&state,
		&formulaRange);
	if (updated != NativeInstantViewLeafUpdateResult::Updated) {
		return updated;
	}
	MeasureNativeIvPreparedFormulas(
		&state,
		formulaRange.from,
		formulaRange.till);
	if (state.result.failure.failed()) {
		return NativeInstantViewLeafUpdateResult::Failed;
	}
	content->blocks.blocks = std::move(blocks);
	content->formulas = std::move(state.result.formulas);
	return NativeInstantViewLeafUpdateResult::Updated;
}

} // namespace Iv::Markdown
