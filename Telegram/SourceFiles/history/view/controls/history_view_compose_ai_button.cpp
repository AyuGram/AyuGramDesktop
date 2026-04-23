/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/controls/history_view_compose_ai_button.h"

#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "styles/style_chat_helpers.h"

namespace HistoryView::Controls {
namespace {

constexpr auto kAnimationDuration = crl::time(640);

} // namespace

ComposeAiButton::ComposeAiButton(
	QWidget *parent,
	const style::IconButton &st)
: RippleButton(parent, st.ripple)
, _st(st) {
	resize(_st.width, _st.height);
	setCursor(style::cur_pointer);

	shownValue(
	) | rpl::on_next([=](bool shown) {
		if (shown) {
			_animation.start([=] { update(); }, 0., 1., kAnimationDuration);
		} else {
			_animation.stop();
		}
	}, lifetime());
}

void ComposeAiButton::paintEvent(QPaintEvent *e) {
	Painter p(this);
	PainterHighQualityEnabler hq(p);

	const auto over = isDown() || isOver() || forceRippled();
	paintRipple(p, _st.rippleAreaPosition);

	const auto progress = _animation.value(1.);
	auto star1Opacity = 1.;
	auto star2Opacity = 1.;
	if (progress < 0.25) {
		star1Opacity = 1. - (progress / 0.25);
	} else if (progress < 0.5) {
		star1Opacity = 0.;
		star2Opacity = 1. - ((progress - 0.25) / 0.25);
	} else if (progress < 0.75) {
		star1Opacity = (progress - 0.5) / 0.25;
		star2Opacity = 0.;
	} else {
		star2Opacity = (progress - 0.75) / 0.25;
	}

	const auto part = [&](const style::icon &icon) {
		if (over) {
			icon.paintInCenter(p, rect(), st::historyComposeIconFgOver->c);
		} else {
			icon.paintInCenter(p, rect());
		}
	};
	part(st::historyAiComposeButtonLetters);
	if (star1Opacity > 0.) {
		p.setOpacity(star1Opacity);
		part(st::historyAiComposeButtonStar1);
	}
	if (star2Opacity > 0.) {
		p.setOpacity(star2Opacity);
		part(st::historyAiComposeButtonStar2);
	}
}

void ComposeAiButton::onStateChanged(State was, StateChangeSource source) {
	RippleButton::onStateChanged(was, source);
	update();
}

QImage ComposeAiButton::prepareRippleMask() const {
	return Ui::RippleAnimation::EllipseMask(
		QSize(_st.rippleAreaSize, _st.rippleAreaSize));
}

QPoint ComposeAiButton::prepareRippleStartPosition() const {
	const auto result = mapFromGlobal(QCursor::pos()) - _st.rippleAreaPosition;
	const auto rect = QRect(0, 0, _st.rippleAreaSize, _st.rippleAreaSize);
	return rect.contains(result)
		? result
		: DisabledRippleStartPosition();
}

} // namespace HistoryView::Controls
