/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_document.h"
#include "iv/markdown/iv_markdown_prepare.h"

#include <memory>

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Iv::Markdown {

[[nodiscard]] std::unique_ptr<Ui::RpWidget> CreateMarkdownPreviewWidget(
	QWidget *parent,
	const PreparedDocument &document,
	Fn<void(Event)> callback,
	const OpenOptions &options = {});
[[nodiscard]] std::unique_ptr<Ui::RpWidget> CreateMarkdownPreviewWidget(
	QWidget *parent,
	MarkdownArticleContent content,
	std::shared_ptr<MathRenderer> renderer,
	Fn<void(Event)> callback,
	const OpenOptions &options = {});
bool UpdateMarkdownPreviewWidget(
	Ui::RpWidget *preview,
	MarkdownArticleContent content,
	const OpenOptions &options);
enum class MarkdownPreviewScrollMode {
	Instant,
	Animated,
};
bool ScrollMarkdownPreviewToAnchor(
	Ui::RpWidget *preview,
	const QString &anchorId,
	MarkdownPreviewScrollMode mode = MarkdownPreviewScrollMode::Instant);
void ScrollMarkdownPreviewToY(
	Ui::RpWidget *preview,
	int top,
	MarkdownPreviewScrollMode mode = MarkdownPreviewScrollMode::Instant);
[[nodiscard]] int MarkdownPreviewScrollTop(Ui::RpWidget *preview);
[[nodiscard]] rpl::producer<int> MarkdownPreviewScrollTopValue(
	Ui::RpWidget *preview);

} // namespace Iv::Markdown
