// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/message_history/history_section.h"

#include "apiwrap.h"
#include "ayu/ui/message_history/history_inner.h"
#include "base/timer.h"
#include "core/shortcuts.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "profile/profile_back_button.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_info.h"
#include "ui/effects/animations.h"
#include "ui/ui_utility.h"
#include "ui/boxes/confirm_box.h"
#include "ui/controls/userpic_button.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/shadow.h"
#include "window/window_adaptive.h"
#include "window/window_session_controller.h"
#include "window/themes/window_theme.h"

namespace MessageHistory {

class FixedBar final : public Ui::RpWidget
{
public:
	FixedBar(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		bool searchEnabled);

	[[nodiscard]] rpl::producer<> searchCancelRequests() const;
	[[nodiscard]] rpl::producer<QString> searchRequests() const;

	// When animating mode is enabled the content is hidden and the
	// whole fixed bar acts like a back button.
	void setAnimatingMode(bool enabled);

	void goBack();
	void showSearch();
	bool setSearchFocus() {
		if (_searchShown) {
			_field->setFocus();
			return true;
		}
		return false;
	}

protected:
	void mousePressEvent(QMouseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

private:
	void toggleSearch();
	void cancelSearch();
	void searchUpdated();
	void applySearch();
	void searchAnimationCallback();

	not_null<Window::SessionController*> _controller;
	not_null<PeerData*> _peer;
	object_ptr<Ui::InputField> _field;
	object_ptr<Profile::BackButton> _backButton;
	object_ptr<Ui::IconButton> _search;
	object_ptr<Ui::CrossButton> _cancel;

	Ui::Animations::Simple _searchShownAnimation;
	bool _searchShown = false;
	bool _searchEnabled = true;
	bool _animatingMode = false;
	base::Timer _searchTimer;

	rpl::event_stream<> _searchCancelRequests;
	rpl::event_stream<QString> _searchRequests;
};

object_ptr<Window::SectionWidget> SectionMemento::createWidget(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	Window::Column column,
	const QRect &geometry) {
	if (column == Window::Column::Third) {
		return nullptr;
	}
	auto result = object_ptr<Widget>(parent, controller, _peer, _item, _topicId);
	result->setInternalState(geometry, this);
	return result;
}

FixedBar::FixedBar(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	bool searchEnabled) : Ui::RpWidget(parent)
, _controller(controller)
, _peer(peer)
, _field(this, st::defaultMultiSelectSearchField, tr::lng_dlg_filter())
, _backButton(this)
, _search(this, st::topBarSearch)
, _cancel(this, st::historyAdminLogCancelSearch)
, _searchEnabled(searchEnabled) {
	_backButton->moveToLeft(0, 0);
	_backButton->setClickedCallback([=] { goBack(); });
	_search->setClickedCallback([=] { showSearch(); });
	_cancel->setClickedCallback([=] { cancelSearch(); });
	_field->hide();
	_field->cancelled() | rpl::on_next([=] {
		cancelSearch();
	}, _field->lifetime());
	_field->changes() | rpl::on_next([=] {
		searchUpdated();
	}, _field->lifetime());
	_field->submits(
	) | rpl::on_next([=] { applySearch(); }, _field->lifetime());
	_searchTimer.setCallback([=] { applySearch(); });

	_cancel->hide(anim::type::instant);
	if (!_searchEnabled) {
		_search->hide();
	}

	Info::Profile::NameValue(peer) | rpl::on_next([=](QString name) {
		_backButton->setText(name);
	}, _backButton->lifetime());
	_backButton->setWidget(Ui::CreateChild<Ui::UserpicButton>(
		_backButton.get(),
		peer,
		st::topBarInfoButton));
}

void FixedBar::goBack() {
	_controller->showBackFromStack();
}

void FixedBar::showSearch() {
	if (_searchEnabled && !_searchShown) {
		toggleSearch();
	}
}

void FixedBar::toggleSearch() {
	_searchShown = !_searchShown;
	_cancel->toggle(_searchShown, anim::type::normal);
	_searchShownAnimation.start(
		[=] { searchAnimationCallback(); },
		_searchShown ? 0. : 1.,
		_searchShown ? 1. : 0.,
		st::historyAdminLogSearchSlideDuration);
	_search->setDisabled(_searchShown);
	if (_searchShown) {
		_field->show();
		_field->setFocus();
	} else {
		_searchCancelRequests.fire({});
	}
}

void FixedBar::searchAnimationCallback() {
	if (!_searchShownAnimation.animating()) {
		_field->setVisible(_searchShown);
		_search->setIconOverride(
			_searchShown ? &st::topBarSearch.icon : nullptr,
			_searchShown ? &st::topBarSearch.icon : nullptr);
		_search->setRippleColorOverride(
			_searchShown ? &st::topBarBg : nullptr);
		_search->setCursor(
			_searchShown ? style::cur_default : style::cur_pointer);
		_backButton->setOpacity(1.);
	}
	resizeToWidth(width());
}

void FixedBar::cancelSearch() {
	if (_searchShown) {
		if (!_field->getLastText().isEmpty()) {
			_field->clear();
			_field->setFocus();
			applySearch();
		} else {
			toggleSearch();
		}
	}
}

void FixedBar::searchUpdated() {
	if (_field->getLastText().isEmpty()) {
		applySearch();
	} else {
		_searchTimer.callOnce(AutoSearchTimeout);
	}
}

void FixedBar::applySearch() {
	_searchRequests.fire_copy(_field->getLastText());
}

rpl::producer<> FixedBar::searchCancelRequests() const {
	return _searchCancelRequests.events();
}

rpl::producer<QString> FixedBar::searchRequests() const {
	return _searchRequests.events();
}

int FixedBar::resizeGetHeight(int newWidth) {
	const auto offset = st::historySendRight + st::lineWidth;
	const auto searchShownLeft = st::topBarArrowPadding.left();
	const auto searchHiddenLeft = _searchEnabled
		? newWidth - _search->width() - offset
		: newWidth;
	const auto searchShown = _searchShownAnimation.value(_searchShown
		? 1.
		: 0.);
	const auto searchCurrentLeft = anim::interpolate(
		searchHiddenLeft,
		searchShownLeft,
		searchShown);
	if (_searchEnabled) {
		_search->moveToLeft(searchCurrentLeft, 0);
	}
	_backButton->setOpacity(1. - searchShown);
	_backButton->resizeToWidth(searchCurrentLeft);
	_backButton->moveToLeft(0, 0);

	const auto cancelLeft = newWidth - _cancel->width() - offset;
	_cancel->moveToLeft(cancelLeft, 0);

	const auto newHeight = _backButton->height();
	const auto fieldLeft = searchShownLeft + _search->width();
	_field->setGeometryToLeft(
		fieldLeft,
		st::historyAdminLogSearchTop,
		cancelLeft - fieldLeft,
		_field->height());

	return newHeight;
}

void FixedBar::setAnimatingMode(bool enabled) {
	if (_animatingMode != enabled) {
		_animatingMode = enabled;
		setCursor(_animatingMode ? style::cur_pointer : style::cur_default);
		if (_animatingMode) {
			setAttribute(Qt::WA_OpaquePaintEvent, false);
			hideChildren();
		} else {
			setAttribute(Qt::WA_OpaquePaintEvent);
			showChildren();
			_field->hide();
			_cancel->setVisible(false);
			if (!_searchEnabled) {
				_search->hide();
			}
		}
		show();
	}
}

void FixedBar::paintEvent(QPaintEvent *e) {
	if (!_animatingMode) {
		auto p = QPainter(this);
		p.fillRect(e->rect(), st::topBarBg);
	}
}

void FixedBar::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		goBack();
	} else {
		Ui::RpWidget::mousePressEvent(e);
	}
}

Widget::Widget(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	HistoryItem *item,
	ID topicId)
	: Window::SectionWidget(parent, controller, rpl::single<PeerData*>(peer)),
	  _scroll(this, st::historyScroll, false),
	  _fixedBar(this, controller, peer, !item),
	  _fixedBarShadow(this),
	  _item(item),
	  _topicId(topicId) {
	_fixedBar->move(0, 0);
	_fixedBar->resizeToWidth(width());
	_fixedBar->searchCancelRequests(
	) | rpl::on_next([=] {
		setInnerFocus();
	}, lifetime());
	_fixedBar->searchRequests(
	) | rpl::on_next([=](const QString &query) {
		_inner->applySearch(query);
	}, lifetime());
	_fixedBar->show();

	_fixedBarShadow->raise();

	controller->adaptive().value(
	) | rpl::on_next([=]
							 {
								 updateAdaptiveLayout();
							 },
							 lifetime());

	_inner = _scroll->setOwnedWidget(object_ptr<InnerWidget>(this, controller, peer, item, topicId));
	_inner->scrollToSignal(
	) | rpl::on_next([=](int top)
							 {
								 _scroll->scrollToY(top);
							 },
							 lifetime());

	_scroll->move(0, _fixedBar->height());
	_scroll->show();
	_scroll->scrolls(
	) | rpl::on_next([=]
							 {
								 onScroll();
							 },
							 lifetime());

	setupShortcuts();
}

void Widget::updateAdaptiveLayout() {
	_fixedBarShadow->moveToLeft(
		controller()->adaptive().isOneColumn()
			? 0
			: st::lineWidth,
		_fixedBar->height());
}

not_null<PeerData*> Widget::channel() const {
	return _inner->peer();
}

Dialogs::RowDescriptor Widget::activeChat() const {
	return {
		channel()->owner().history(channel()),
		FullMsgId(channel()->id, ShowAtUnreadMsgId)
	};
}

QPixmap Widget::grabForShowAnimation(const Window::SectionSlideParams &params) {
	if (params.withTopBarShadow) _fixedBarShadow->hide();
	auto result = Ui::GrabWidget(this);
	if (params.withTopBarShadow) _fixedBarShadow->show();
	return result;
}

void Widget::doSetInnerFocus() {
	if (!_fixedBar->setSearchFocus()) {
		_inner->setFocus();
	}
}

bool Widget::showInternal(
	not_null<Window::SectionMemento*> memento,
	const Window::SectionShow &params) {
	if (auto logMemento = dynamic_cast<SectionMemento*>(memento.get())) {
		if (logMemento->getPeer() == channel()) {
			restoreState(logMemento);
			return true;
		}
	}
	return false;
}

void Widget::setInternalState(const QRect &geometry, not_null<SectionMemento*> memento) {
	setGeometry(geometry);
	Ui::SendPendingMoveResizeEvents(this);
	restoreState(memento);
}

void Widget::setupShortcuts() {
	Shortcuts::Requests(
	) | rpl::filter([=] {
		return Ui::AppInFocus()
			&& Ui::InFocusChain(this)
			&& !controller()->isLayerShown()
			&& isActiveWindow();
	}) | rpl::on_next([=](not_null<Shortcuts::Request*> request) {
		using Command = Shortcuts::Command;
		request->check(Command::Search, 2) && request->handle([=] {
			_fixedBar->showSearch();
			return true;
		});
	}, lifetime());
}

std::shared_ptr<Window::SectionMemento> Widget::createMemento() {
	auto result = std::make_shared<SectionMemento>(channel(), _item, _topicId);
	saveState(result.get());
	return result;
}

void Widget::saveState(not_null<SectionMemento*> memento) {
	memento->setScrollTop(_scroll->scrollTop());
	_inner->saveState(memento);
}

void Widget::restoreState(not_null<SectionMemento*> memento) {
	_inner->restoreState(memento);
	auto scrollTop = memento->getScrollTop();
	_scroll->scrollToY(scrollTop);
	_inner->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
}

void Widget::resizeEvent(QResizeEvent *e) {
	if (!width() || !height()) {
		return;
	}

	auto contentWidth = width();

	auto newScrollTop = _scroll->scrollTop() + topDelta();
	_fixedBar->resizeToWidth(contentWidth);
	_fixedBarShadow->resize(contentWidth, st::lineWidth);

	auto bottom = height();
	auto scrollHeight = bottom - _fixedBar->height();
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
}

void Widget::paintEvent(QPaintEvent *e) {
	if (animatingShow()) {
		SectionWidget::paintEvent(e);
		return;
	} else if (controller()->contentOverlapped(this, e)) {
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

void Widget::onScroll() {
	int scrollTop = _scroll->scrollTop();
	_inner->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
}

void Widget::showAnimatedHook(
	const Window::SectionSlideParams &params) {
	_fixedBar->setAnimatingMode(true);
	if (params.withTopBarShadow) _fixedBarShadow->show();
}

void Widget::showFinishedHook() {
	_fixedBar->setAnimatingMode(false);
}

bool Widget::floatPlayerHandleWheelEvent(QEvent *e) {
	return _scroll->viewportEvent(e);
}

QRect Widget::floatPlayerAvailableRect() {
	return mapToGlobal(_scroll->geometry());
}

} // namespace MessageHistory
