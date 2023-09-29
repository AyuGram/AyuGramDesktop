// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu/ui/sections/edited/edited_log_section.h"

#include "ayu/ui/sections/edited/edited_log_inner.h"
#include "profile/profile_back_button.h"
#include "core/shortcuts.h"
#include "ui/effects/animations.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/shadow.h"
#include "ui/widgets/buttons.h"
#include "ui/ui_utility.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "apiwrap.h"
#include "window/themes/window_theme.h"
#include "window/window_adaptive.h"
#include "window/window_session_controller.h"
#include "ui/boxes/confirm_box.h"
#include "base/timer.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_window.h"
#include "styles/style_info.h"

namespace EditedLog
{

class FixedBar final : public TWidget
{
public:
	FixedBar(
		QWidget *parent,
		not_null<Window::SessionController *> controller,
		not_null<PeerData *> peer);

	// When animating mode is enabled the content is hidden and the
	// whole fixed bar acts like a back button.
	void setAnimatingMode(bool enabled);

	void goBack();

protected:
	void mousePressEvent(QMouseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	not_null<Window::SessionController *> _controller;
	not_null<PeerData *> _peer;
	object_ptr<Profile::BackButton> _backButton;
	object_ptr<Ui::CrossButton> _cancel;

	bool _animatingMode = false;
};

object_ptr<Window::SectionWidget> SectionMemento::createWidget(
	QWidget *parent,
	not_null<Window::SessionController *> controller,
	Window::Column column,
	const QRect &geometry)
{
	if (column == Window::Column::Third) {
		return nullptr;
	}
	auto result = object_ptr<Widget>(parent, controller, _peer, _item);
	result->setInternalState(geometry, this);
	return result;
}

FixedBar::FixedBar(
	QWidget *parent,
	not_null<Window::SessionController *> controller,
	not_null<PeerData *> peer)
	: TWidget(parent), _controller(controller), _peer(peer), _backButton(
	this,
	&controller->session(),
	tr::lng_admin_log_title_all(tr::now),
	controller->adaptive().oneColumnValue()), _cancel(this, st::historyAdminLogCancelSearch)
{
	_backButton->moveToLeft(0, 0);
	_backButton->setClickedCallback([=]
									{ goBack(); });

	_cancel->hide(anim::type::instant);
}

void FixedBar::goBack()
{
	_controller->showBackFromStack();
}

int FixedBar::resizeGetHeight(int newWidth)
{
	auto filterLeft = newWidth;

	auto cancelLeft = filterLeft - _cancel->width();
	_cancel->moveToLeft(cancelLeft, 0);

	auto searchShownLeft = st::topBarArrowPadding.left();
	auto searchHiddenLeft = filterLeft - 0;
	auto searchCurrentLeft = anim::interpolate(searchHiddenLeft, searchShownLeft, 0.0);
	_backButton->resizeToWidth(searchCurrentLeft);
	_backButton->moveToLeft(0, 0);

	auto newHeight = _backButton->height();

	return newHeight;
}

void FixedBar::setAnimatingMode(bool enabled)
{
	if (_animatingMode != enabled) {
		_animatingMode = enabled;
		setCursor(_animatingMode ? style::cur_pointer : style::cur_default);
		if (_animatingMode) {
			setAttribute(Qt::WA_OpaquePaintEvent, false);
			hideChildren();
		}
		else {
			setAttribute(Qt::WA_OpaquePaintEvent);
			showChildren();
			_cancel->setVisible(false);
		}
		show();
	}
}

void FixedBar::paintEvent(QPaintEvent *e)
{
	if (!_animatingMode) {
		auto p = QPainter(this);
		p.fillRect(e->rect(), st::topBarBg);
	}
}

void FixedBar::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton) {
		goBack();
	}
	else {
		TWidget::mousePressEvent(e);
	}
}

Widget::Widget(
	QWidget *parent,
	not_null<Window::SessionController *> controller,
	not_null<PeerData *> peer,
	not_null<HistoryItem *> item)
	: Window::SectionWidget(parent, controller, rpl::single<PeerData *>(peer)),
	  _scroll(this, st::historyScroll, false),
	  _fixedBar(this, controller, peer),
	  _fixedBarShadow(this),
	  _whatIsThis(
		  this,
		  tr::lng_admin_log_about(tr::now),
		  st::historyComposeButton),
	  _item(item)
{
	_fixedBar->move(0, 0);
	_fixedBar->resizeToWidth(width());
	_fixedBar->show();

	_fixedBarShadow->raise();

	controller->adaptive().value(
	) | rpl::start_with_next([=]
							 {
								 updateAdaptiveLayout();
							 }, lifetime());

	_inner = _scroll->setOwnedWidget(object_ptr<InnerWidget>(this, controller, peer, item));
	_inner->scrollToSignal(
	) | rpl::start_with_next([=](int top)
							 {
								 _scroll->scrollToY(top);
							 }, lifetime());

	_scroll->move(0, _fixedBar->height());
	_scroll->show();
	_scroll->scrolls(
	) | rpl::start_with_next([=]
							 {
								 onScroll();
							 }, lifetime());

	_whatIsThis->setClickedCallback([=]
									{
										controller->show(Ui::MakeInformBox(peer->isMegagroup()
																		   ? tr::lng_admin_log_about_text()
																		   : tr::lng_admin_log_about_text_channel()));
									});

	setupShortcuts();
}

void Widget::updateAdaptiveLayout()
{
	_fixedBarShadow->moveToLeft(
		controller()->adaptive().isOneColumn()
		? 0
		: st::lineWidth,
		_fixedBar->height());
}

not_null<PeerData *> Widget::channel() const
{
	return _inner->channel();
}

Dialogs::RowDescriptor Widget::activeChat() const
{
	return {
		channel()->owner().history(channel()),
		FullMsgId(channel()->id, ShowAtUnreadMsgId)
	};
}

QPixmap Widget::grabForShowAnimation(const Window::SectionSlideParams &params)
{
	if (params.withTopBarShadow) _fixedBarShadow->hide();
	auto result = Ui::GrabWidget(this);
	if (params.withTopBarShadow) _fixedBarShadow->show();
	return result;
}

void Widget::doSetInnerFocus()
{
	_inner->setFocus();
}

bool Widget::showInternal(
	not_null<Window::SectionMemento *> memento,
	const Window::SectionShow &params)
{
	if (auto logMemento = dynamic_cast<SectionMemento *>(memento.get())) {
		if (logMemento->getPeer() == channel()) {
			restoreState(logMemento);
			return true;
		}
	}
	return false;
}

void Widget::setInternalState(const QRect &geometry, not_null<SectionMemento *> memento)
{
	setGeometry(geometry);
	Ui::SendPendingMoveResizeEvents(this);
	restoreState(memento);
}

void Widget::setupShortcuts()
{
	// todo: smth
}

std::shared_ptr<Window::SectionMemento> Widget::createMemento()
{
	auto result = std::make_shared<SectionMemento>(channel(), _item);
	saveState(result.get());
	return result;
}

void Widget::saveState(not_null<SectionMemento *> memento)
{
	memento->setScrollTop(_scroll->scrollTop());
	_inner->saveState(memento);
}

void Widget::restoreState(not_null<SectionMemento *> memento)
{
	_inner->restoreState(memento);
	auto scrollTop = memento->getScrollTop();
	_scroll->scrollToY(scrollTop);
	_inner->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
}

void Widget::resizeEvent(QResizeEvent *e)
{
	if (!width() || !height()) {
		return;
	}

	auto contentWidth = width();

	auto newScrollTop = _scroll->scrollTop() + topDelta();
	_fixedBar->resizeToWidth(contentWidth);
	_fixedBarShadow->resize(contentWidth, st::lineWidth);

	auto bottom = height();
	auto scrollHeight = bottom - _fixedBar->height() - _whatIsThis->height();
	auto scrollSize = QSize(contentWidth, scrollHeight);
	if (_scroll->size() != scrollSize) {
		_scroll->resize(scrollSize);
		_inner->resizeToWidth(scrollSize.width(), _scroll->height());
		_inner->restoreScrollPosition();
	}

	if (!_scroll->isHidden()) {
		if (topDelta()) {
			_scroll->scrollToY(newScrollTop);
		}
		auto scrollTop = _scroll->scrollTop();
		_inner->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
	}
	auto fullWidthButtonRect = myrtlrect(0, bottom - _whatIsThis->height(), contentWidth, _whatIsThis->height());
	_whatIsThis->setGeometry(fullWidthButtonRect);
}

void Widget::paintEvent(QPaintEvent *e)
{
	if (animatingShow()) {
		SectionWidget::paintEvent(e);
		return;
	}
	else if (controller()->contentOverlapped(this, e)) {
		return;
	}
	//if (hasPendingResizedItems()) {
	//	updateListSize();
	//}

	//auto ms = crl::now();
	//_historyDownShown.step(ms);

	const auto clip = e->rect();
	SectionWidget::PaintBackground(controller(), _inner->theme(), this, clip);
}

void Widget::onScroll()
{
	int scrollTop = _scroll->scrollTop();
	_inner->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
}

void Widget::showAnimatedHook(
	const Window::SectionSlideParams &params)
{
	_fixedBar->setAnimatingMode(true);
	if (params.withTopBarShadow) _fixedBarShadow->show();
}

void Widget::showFinishedHook()
{
	_fixedBar->setAnimatingMode(false);
}

bool Widget::floatPlayerHandleWheelEvent(QEvent *e)
{
	return _scroll->viewportEvent(e);
}

QRect Widget::floatPlayerAvailableRect()
{
	return mapToGlobal(_scroll->geometry());
}

} // namespace EditedLog

