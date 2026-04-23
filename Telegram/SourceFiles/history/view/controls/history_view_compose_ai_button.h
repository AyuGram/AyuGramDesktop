/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/effects/animations.h"
#include "ui/widgets/buttons.h"

namespace HistoryView::Controls {

class ComposeAiButton final : public Ui::RippleButton {
public:
	ComposeAiButton(QWidget *parent, const style::IconButton &st);

protected:
	void paintEvent(QPaintEvent *e) override;
	void onStateChanged(State was, StateChangeSource source) override;

	[[nodiscard]] QImage prepareRippleMask() const override;
	[[nodiscard]] QPoint prepareRippleStartPosition() const override;

private:
	const style::IconButton &_st;
	Ui::Animations::Simple _animation;

};

} // namespace HistoryView::Controls
