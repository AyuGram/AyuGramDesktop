/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/editor/iv_editor_box.h"

#include <QtCore/QDir>

#include "base/algorithm.h"
#include "base/event_filter.h"
#include "base/unique_qptr.h"
#include "boxes/premium_preview_box.h"
#include "chat_helpers/compose/compose_show.h"
#include "data/data_msg_id.h"
#include "data/data_types.h"
#include "data/data_emoji_statuses.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "ui/emoji_config.h"
#include "ui/painter.h"
#include "data/data_document.h"
#include "data/stickers/data_custom_emoji.h"
#include "data/stickers/data_stickers.h"
#include "chat_helpers/tabbed_panel.h"
#include "chat_helpers/tabbed_selector.h"
#include "iv/editor/iv_editor_state.h"
#include "iv/editor/iv_editor_widget.h"
#include "iv/editor/iv_editor_window.h"
#include "lang/lang_keys.h"
#include "ui/rect_part.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/tooltip.h"

#include <crl/crl_on_main.h>
#include <rpl/never.h>

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtGui/QCloseEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "styles/style_chat_helpers.h"
#include "styles/style_iv.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"

namespace Iv::Editor {
namespace {

struct ToolbarButton {
	object_ptr<Ui::IconButton> widget;
	Fn<rpl::producer<TextWithEntities>()> tooltipFactory;
};

class Toolbar final : public Ui::RpWidget {
public:
	Toolbar(
		QWidget *parent,
		not_null<Widget*> editor,
		QPointer<QWidget> tooltipParent,
		Fn<void(not_null<Widget*>, QPointer<QWidget>)> requestMedia,
		Fn<void(not_null<Widget*>, QPointer<QWidget>, rpl::producer<>)> requestMap,
		Fn<void(not_null<Ui::IconButton*>)> toggleEmoji);

	int resizeGetHeight(int width) override;
	[[nodiscard]] Ui::IconButton *emojiButton() const;
	void hideShownTooltip();

protected:
	bool eventFilter(QObject *object, QEvent *event) override;

private:
	not_null<Ui::IconButton*> addButton(
		QString label,
		Fn<rpl::producer<TextWithEntities>()> tooltipFactory,
		const style::icon *icon,
		Fn<void()> callback);
	void addInsertButtons();
	void showHeadingMenu(not_null<Ui::IconButton*> button);
	void showTooltip(not_null<Ui::IconButton*> button);
	void hideTooltip();
	void updateTooltipGeometry();
	[[nodiscard]] ToolbarButton *buttonData(not_null<Ui::IconButton*> button);

	const QPointer<Widget> _editor;
	const QPointer<QWidget> _tooltipParent;
	const Fn<void(not_null<Widget*>, QPointer<QWidget>)> _requestMedia;
	const Fn<void(not_null<Widget*>, QPointer<QWidget>, rpl::producer<>)> _requestMap;
	const Fn<void(not_null<Ui::IconButton*>)> _toggleEmoji;
	std::vector<ToolbarButton> _buttons;
	base::unique_qptr<Ui::ImportantTooltip> _tooltip;
	base::unique_qptr<Ui::PopupMenu> _menu;
	Ui::IconButton *_emojiButton = nullptr;
	Ui::IconButton *_hovered = nullptr;

};

[[nodiscard]] QRect DefaultWindowGeometry() {
	const auto size = st::ivEditorWindowDefaultSize;
	auto result = QRect(QPoint(), size);
	if (const auto screen = QGuiApplication::primaryScreen()) {
		result.moveCenter(screen->availableGeometry().center());
	}
	return result;
}

[[nodiscard]] QString HeadingLabel(int level) {
	switch (level) {
	case 1: return tr::lng_article_insert_heading1(tr::now);
	case 2: return tr::lng_article_insert_heading2(tr::now);
	case 3: return tr::lng_article_insert_heading3(tr::now);
	case 4: return tr::lng_article_insert_heading4(tr::now);
	case 5: return tr::lng_article_insert_heading5(tr::now);
	case 6: return tr::lng_article_insert_heading6(tr::now);
	}
	return tr::lng_article_insert_heading1(tr::now);
}

[[nodiscard]] QString SubmitText(const ShowWindowDescriptor &descriptor) {
	if (!descriptor.submitLabel.isEmpty()) {
		return descriptor.submitLabel;
	}
	switch (descriptor.submitType) {
	case ShowWindowDescriptor::SubmitType::Send:
		return tr::lng_send_button(tr::now);
	case ShowWindowDescriptor::SubmitType::Save:
		return tr::lng_settings_save(tr::now);
	}
	return tr::lng_send_button(tr::now);
}

[[nodiscard]] bool IsEmojiDocument(not_null<DocumentData*> document) {
	const auto info = document->sticker();
	return info && (info->setType == Data::StickersType::Emoji);
}

Toolbar::Toolbar(
	QWidget *parent,
	not_null<Widget*> editor,
	QPointer<QWidget> tooltipParent,
	Fn<void(not_null<Widget*>, QPointer<QWidget>)> requestMedia,
	Fn<void(not_null<Widget*>, QPointer<QWidget>, rpl::producer<>)> requestMap,
	Fn<void(not_null<Ui::IconButton*>)> toggleEmoji)
: Ui::RpWidget(parent)
, _editor(editor.get())
, _tooltipParent(std::move(tooltipParent))
, _requestMedia(std::move(requestMedia))
, _requestMap(std::move(requestMap))
, _toggleEmoji(std::move(toggleEmoji)) {
	setMouseTracking(true);
	addInsertButtons();
}

not_null<Ui::IconButton*> Toolbar::addButton(
		QString label,
		Fn<rpl::producer<TextWithEntities>()> tooltipFactory,
		const style::icon *icon,
		Fn<void()> callback) {
	auto button = object_ptr<Ui::IconButton>(this, st::ivEditorToolbarButton);
	const auto raw = button.data();
	raw->setAccessibleName(label);
	raw->setIconOverride(icon, icon);
	raw->setClickedCallback([=, callback = std::move(callback)] {
		hideTooltip();
		callback();
	});
	raw->installEventFilter(this);
	_buttons.push_back({
		std::move(button),
		std::move(tooltipFactory),
	});
	return raw;
}

void Toolbar::addInsertButtons() {
	const auto insert = [=](State::InsertAction action) {
		if (_editor) {
			_editor->insertBlock(action);
		}
	};
	const auto insertType = [=](State::InsertBlockType type) {
		insert({ .type = type });
	};

	const auto heading = addButton(
		tr::lng_article_insert_heading(tr::now),
		[] { return tr::lng_article_insert_heading(tr::marked); },
		&st::ivEditorToolbarHeadingIcon,
		[] {});
	heading->setClickedCallback([=] {
		hideTooltip();
		showHeadingMenu(heading);
	});
	_emojiButton = addButton(
		tr::lng_article_insert_emoji(tr::now),
		[] { return tr::lng_article_insert_emoji(tr::marked); },
		&st::ivEditorToolbarEmojiIcon,
		[=] {
			if (_toggleEmoji && _emojiButton) {
				_toggleEmoji(not_null<Ui::IconButton*>(_emojiButton));
			}
		});
	addButton(
		tr::lng_article_insert_blockquote(tr::now),
		[] { return tr::lng_article_insert_blockquote(tr::marked); },
		&st::ivEditorToolbarBlockquoteIcon,
		[=] { insertType(State::InsertBlockType::Blockquote); });
	addButton(
		tr::lng_article_insert_code(tr::now),
		[] { return tr::lng_article_insert_code(tr::marked); },
		&st::ivEditorToolbarCodeIcon,
		[=] { insertType(State::InsertBlockType::Code); });
	addButton(
		tr::lng_article_insert_math(tr::now),
		[] { return tr::lng_article_insert_math(tr::marked); },
		&st::ivEditorToolbarMathIcon,
		[=] { insertType(State::InsertBlockType::Math); });
	addButton(
		tr::lng_article_insert_divider(tr::now),
		[] { return tr::lng_article_insert_divider(tr::marked); },
		&st::ivEditorToolbarDividerIcon,
		[=] { insertType(State::InsertBlockType::Divider); });
	addButton(
		tr::lng_article_insert_ordered_list(tr::now),
		[] { return tr::lng_article_insert_ordered_list(tr::marked); },
		&st::ivEditorToolbarOrderedListIcon,
		[=] { insertType(State::InsertBlockType::OrderedList); });
	addButton(
		tr::lng_article_insert_bullet_list(tr::now),
		[] { return tr::lng_article_insert_bullet_list(tr::marked); },
		&st::ivEditorToolbarBulletListIcon,
		[=] { insertType(State::InsertBlockType::BulletList); });
	addButton(
		tr::lng_article_insert_task_list(tr::now),
		[] { return tr::lng_article_insert_task_list(tr::marked); },
		&st::ivEditorToolbarTaskListIcon,
		[=] { insertType(State::InsertBlockType::TaskList); });
	addButton(
		tr::lng_article_insert_pullquote(tr::now),
		[] { return tr::lng_article_insert_pullquote(tr::marked); },
		&st::ivEditorToolbarPullquoteIcon,
		[=] { insertType(State::InsertBlockType::Pullquote); });
	if (_requestMedia) {
		addButton(
			tr::lng_article_insert_media(tr::now),
			[] { return tr::lng_article_insert_media(tr::marked); },
			&st::ivEditorToolbarAttachIcon,
			[=] {
				if (_editor) {
					_requestMedia(
						not_null<Widget*>(_editor.data()),
						_tooltipParent);
				}
			});
	}
	addButton(
		tr::lng_article_insert_details(tr::now),
		[] { return tr::lng_article_insert_details(tr::marked); },
		&st::ivEditorToolbarDetailsIcon,
		[=] { insertType(State::InsertBlockType::Details); });
	addButton(
		tr::lng_article_insert_table(tr::now),
		[] { return tr::lng_article_insert_table(tr::marked); },
		&st::ivEditorToolbarTableIcon,
		[=] { insertType(State::InsertBlockType::Table); });

	if (_requestMap) {
		addButton(
			tr::lng_article_insert_map(tr::now),
			[] { return tr::lng_article_insert_map(tr::marked); },
			&st::menuIconAddress,
			[=] {
				if (_editor) {
					const auto parent = _tooltipParent;
					auto closeRequests = parent
						? static_cast<Ui::RpWidget*>(parent.data())->death()
						: rpl::never<>();
					_requestMap(
						not_null<Widget*>(_editor.data()),
						parent,
						std::move(closeRequests));
				}
			});
	}
}

void Toolbar::showHeadingMenu(not_null<Ui::IconButton*> button) {
	_menu = base::make_unique_q<Ui::PopupMenu>(this);
	for (const auto level : std::array{ 1, 2, 3, 4, 5, 6 }) {
		_menu->addAction(
			HeadingLabel(level),
			[=] {
				if (_editor) {
					_editor->insertBlock({
						.type = State::InsertBlockType::Heading,
						.headingLevel = level,
					});
				}
			});
	}
	_menu->popup(button->mapToGlobal(QPoint(0, button->height())));
}

int Toolbar::resizeGetHeight(int width) {
	const auto padding = st::ivEditorToolbarPadding;
	const auto buttonWidth = st::ivEditorToolbarButton.width;
	const auto buttonHeight = st::ivEditorToolbarButton.height;
	const auto right = width - padding.right();
	auto left = padding.left();
	auto top = padding.top();
	for (const auto &button : _buttons) {
		if (left > padding.left() && left + buttonWidth > right) {
			left = padding.left();
			top += buttonHeight + st::ivEditorToolbarRowSkip;
		}
		button.widget->moveToLeft(left, top, width);
		left += buttonWidth + st::ivEditorToolbarButtonSkip;
	}
	updateTooltipGeometry();
	return top + buttonHeight + padding.bottom();
}

Ui::IconButton *Toolbar::emojiButton() const {
	return _emojiButton;
}

void Toolbar::hideShownTooltip() {
	hideTooltip();
}

bool Toolbar::eventFilter(QObject *object, QEvent *event) {
	for (const auto &data : _buttons) {
		if (data.widget.data() != object) {
			continue;
		}
		const auto button = data.widget.data();
		if (event->type() == QEvent::Enter) {
			showTooltip(not_null<Ui::IconButton*>(button));
		} else if (event->type() == QEvent::Leave && _hovered == button) {
			hideTooltip();
		}
		break;
	}
	return Ui::RpWidget::eventFilter(object, event);
}

void Toolbar::showTooltip(not_null<Ui::IconButton*> button) {
	hideTooltip();
	const auto data = buttonData(button);
	if (!data) {
		return;
	}
	_hovered = button;
	const auto tooltipParent = _tooltipParent
		? _tooltipParent.data()
		: (parentWidget() ? parentWidget() : this);
	_tooltip.reset(Ui::CreateChild<Ui::ImportantTooltip>(
		tooltipParent,
		Ui::MakeNiceTooltipLabel(
			tooltipParent,
			data->tooltipFactory(),
			st::boxWideWidth,
			st::defaultImportantTooltipLabel),
		st::defaultImportantTooltip));
	_tooltip->setAttribute(Qt::WA_TransparentForMouseEvents);
	_tooltip->toggleFast(false);
	updateTooltipGeometry();
	_tooltip->raise();
	_tooltip->toggleAnimated(true);
}

void Toolbar::hideTooltip() {
	_hovered = nullptr;
	if (_tooltip) {
		_tooltip->toggleFast(false);
		_tooltip = nullptr;
	}
}

void Toolbar::updateTooltipGeometry() {
	if (!_tooltip || !_hovered) {
		return;
	}
	const auto tooltipParent = _tooltip->parentWidget();
	const auto geometry = Ui::MapFrom(
		tooltipParent,
		_hovered,
		_hovered->rect());
	_tooltip->pointAt(geometry, RectPart::Top | RectPart::Center);
}

ToolbarButton *Toolbar::buttonData(not_null<Ui::IconButton*> button) {
	for (auto &data : _buttons) {
		if (data.widget.data() == button.get()) {
			return &data;
		}
	}
	return nullptr;
}

} // namespace

struct WindowHost::Impl final {
public:
	explicit Impl(ShowWindowDescriptor descriptor);
	~Impl();

private:
	void setupWindow(ShowWindowDescriptor &&descriptor);
	void setupEmojiPanel(const ShowWindowDescriptor &descriptor);
	void layout();
	void updateEmojiPanelGeometry();
	void updateEditorVisibleTopBottom();
	void setEmojiPanelInteractionActive(bool active);
	void finishCloseFromAcceptedEvent();
	void finishClose();
	[[nodiscard]] bool confirmCancel();
	void submit();

	std::unique_ptr<Window> _window;
	object_ptr<Ui::RpWidget> _top = { nullptr };
	object_ptr<Ui::ScrollArea> _scroll = { nullptr };
	QPointer<Widget> _editor;
	object_ptr<Toolbar> _toolbar = { nullptr };
	object_ptr<Ui::RoundButton> _cancel = { nullptr };
	object_ptr<Ui::RoundButton> _submit = { nullptr };
	base::unique_qptr<ChatHelpers::TabbedPanel> _emojiPanel;
	Fn<bool()> _cancelled;
	Fn<bool()> _confirmed;
	Fn<void()> _closed;
	rpl::lifetime _lifetime;
	bool _emojiPanelInteractionActive = false;
	bool _closingApproved = false;
	bool _closedNotified = false;

};

WindowHost::Impl::Impl(ShowWindowDescriptor descriptor) {
	setupWindow(std::move(descriptor));
}

WindowHost::Impl::~Impl() {
	setEmojiPanelInteractionActive(false);
}

void WindowHost::Impl::setupWindow(ShowWindowDescriptor &&descriptor) {
	const auto title = tr::lng_article_editor_title(tr::now);

	_window = std::make_unique<Window>();
	const auto window = _window.get();
	window->setTitle(title);
	window->setWindowTitle(title);
	window->setMinimumSize(st::ivEditorWindowMinSize);
	window->setGeometry(DefaultWindowGeometry());

	window->body()->paintRequest() | rpl::on_next([=](QRect clip) {
		QPainter(window->body().get()).fillRect(clip, st::windowBg);
	}, window->body()->lifetime());

	_top = object_ptr<Ui::RpWidget>(window->body().get());
	_top->paintRequest() | rpl::on_next([=](QRect clip) {
		QPainter(_top.data()).fillRect(clip, st::windowBg);
	}, _top->lifetime());

	_scroll = object_ptr<Ui::ScrollArea>(window->body().get(), st::boxScroll);
	_editor = _scroll->setOwnedWidget(object_ptr<Widget>(
		_scroll.data(),
		WidgetServices{
			.session = descriptor.session,
			.show = descriptor.show,
			.outer = window->body(),
			.customEmojiPaused = std::move(descriptor.customEmojiPaused),
			.imeCompositionStarts = window->imeCompositionStarts(),
		},
		descriptor.peer,
		descriptor.state,
		std::move(descriptor.showLimitToast)));
	const auto editor = not_null<Widget*>(_editor.data());
	const auto body = QPointer<QWidget>(window->body().get());

	_toolbar = object_ptr<Toolbar>(
		_top.data(),
		editor,
		body,
		std::move(descriptor.requestMedia),
		std::move(descriptor.requestMap),
		[=](not_null<Ui::IconButton*>) {
			_toolbar->hideShownTooltip();
			setEmojiPanelInteractionActive(true);
			updateEmojiPanelGeometry();
			_emojiPanel->toggleAnimated();
		});
	_cancel = object_ptr<Ui::RoundButton>(
		_top.data(),
		tr::lng_cancel(),
		st::ivEditorCancelButton);
	_cancel->setClickedCallback([=] {
		if (confirmCancel()) {
			finishClose();
		}
	});
	_submit = object_ptr<Ui::RoundButton>(
		_top.data(),
		rpl::single(SubmitText(descriptor)),
		st::ivEditorSubmitButton);
	_submit->setClickedCallback([=] { submit(); });
	if (descriptor.setupSubmitButton) {
		descriptor.setupSubmitButton(not_null<Ui::RpWidget*>(_submit.data()));
	}

	_cancelled = std::move(descriptor.cancelled);
	_confirmed = std::move(descriptor.confirmed);
	_closed = std::move(descriptor.closed);

	setupEmojiPanel(descriptor);

	window->body()->sizeValue() | rpl::on_next([=](QSize) {
		layout();
	}, _lifetime);
	rpl::combine(
		_scroll->scrollTopValue(),
		_scroll->heightValue()
	) | rpl::on_next([=](int, int) {
		updateEditorVisibleTopBottom();
	}, _lifetime);
	window->events() | rpl::on_next([=](not_null<QEvent*> e) {
		if (e->type() == QEvent::Close) {
			const auto event = static_cast<QCloseEvent*>(e.get());
			if (_closingApproved) {
				event->accept();
				return;
			}
			if (confirmCancel()) {
				event->accept();
				finishCloseFromAcceptedEvent();
			} else {
				event->ignore();
			}
		} else if (e->type() == QEvent::KeyPress) {
			const auto event = static_cast<QKeyEvent*>(e.get());
			if (event->key() == Qt::Key_Escape) {
				event->accept();
				if (confirmCancel()) {
					finishClose();
				}
			}
		}
	}, _lifetime);

	layout();
	_top->show();
	_scroll->show();
	window->show();
	editor->activateInitialNode();
}

void WindowHost::Impl::setupEmojiPanel(const ShowWindowDescriptor &descriptor) {
	using Selector = ChatHelpers::TabbedSelector;
	_emojiPanel = base::make_unique_q<ChatHelpers::TabbedPanel>(
		_window->body().get(),
		ChatHelpers::TabbedPanelDescriptor{
			.ownedSelector = object_ptr<Selector>(
				nullptr,
				ChatHelpers::TabbedSelectorDescriptor{
					.show = descriptor.show,
					.st = st::defaultEmojiPan,
					.level = ChatHelpers::PauseReason::Layer,
					.mode = Selector::Mode::EmojiOnly,
					.features = {
						.stickersSettings = false,
						.openStickerSets = false,
					},
				}),
		});
	const auto panel = _emojiPanel.get();
	panel->setDesiredHeightValues(
		1.,
		st::emojiPanMinHeight / 2,
		st::emojiPanMinHeight);
	panel->setDropDown(true);
	panel->hide();
	panel->selector()->setCurrentPeer(descriptor.peer);
	panel->selector()->emojiChosen(
	) | rpl::on_next([=](ChatHelpers::EmojiChosen data) {
		setEmojiPanelInteractionActive(true);
		if (_editor) {
			_editor->insertEmoji(data.emoji);
		}
	}, _lifetime);
	panel->selector()->customEmojiChosen(
	) | rpl::on_next([=](ChatHelpers::FileChosen data) {
		const auto document = data.document;
		if (!IsEmojiDocument(document)) {
			return;
		}
		if (document->isPremiumEmoji()
			&& !descriptor.session->premium()
			&& !Data::AllowEmojiWithoutPremium(descriptor.peer, document)) {
			ShowPremiumPreviewBox(
				descriptor.show,
				PremiumFeature::AnimatedEmoji);
		} else if (_editor) {
			setEmojiPanelInteractionActive(true);
			_editor->insertCustomEmoji(document);
		}
	}, _lifetime);

	if (const auto button = _toolbar->emojiButton()) {
		button->installEventFilter(panel);
		base::install_event_filter(panel, button, [=](not_null<QEvent*> event) {
			const auto type = event->type();
			if (type == QEvent::Enter || type == QEvent::MouseButtonPress) {
				setEmojiPanelInteractionActive(true);
				updateEmojiPanelGeometry();
			} else if (type == QEvent::Leave && panel->isHidden()) {
				setEmojiPanelInteractionActive(false);
			}
			return base::EventFilterResult::Continue;
		});
	}
	base::install_event_filter(panel, [=](not_null<QEvent*> event) {
		const auto type = event->type();
		if (type == QEvent::Hide || type == QEvent::Close) {
			setEmojiPanelInteractionActive(false);
		} else if (type == QEvent::Show) {
			setEmojiPanelInteractionActive(true);
		}
		return base::EventFilterResult::Continue;
	});
}

void WindowHost::Impl::layout() {
	if (!_window || !_top || !_toolbar || !_editor) {
		return;
	}
	const auto width = _window->body()->width();
	const auto height = _window->body()->height();
	const auto padding = st::ivEditorTopControlsPadding;
	const auto buttonsWidth = _cancel->width()
		+ st::ivEditorTopControlsButtonSkip
		+ _submit->width();
	const auto toolbarWidth = std::max(
		width
			- padding.right()
			- buttonsWidth
			- st::ivEditorTopControlsContentSkip,
		0);
	const auto toolbarHeight = _toolbar->resizeGetHeight(toolbarWidth);
	const auto buttonsHeight = std::max(_cancel->height(), _submit->height());
	const auto topHeight = std::max(
		toolbarHeight,
		padding.top() + buttonsHeight + padding.bottom());
	const auto buttonsTop = padding.top()
		+ (topHeight - padding.top() - padding.bottom() - buttonsHeight) / 2;
	_top->setGeometry(0, 0, width, topHeight);
	_toolbar->setGeometry(0, 0, toolbarWidth, toolbarHeight);
	_submit->moveToRight(padding.right(), buttonsTop, width);
	_cancel->moveToRight(
		padding.right()
			+ _submit->width()
			+ st::ivEditorTopControlsButtonSkip,
		buttonsTop,
		width);
	_scroll->setGeometry(0, topHeight, width, std::max(height - topHeight, 0));
	_editor->resizeToWidth(std::max(_scroll->width(), 1));
	updateEditorVisibleTopBottom();
	updateEmojiPanelGeometry();
}

void WindowHost::Impl::updateEmojiPanelGeometry() {
	if (!_emojiPanel || !_toolbar) {
		return;
	}
	const auto button = _toolbar->emojiButton();
	if (!button) {
		return;
	}
	const auto local = _emojiPanel->parentWidget()->mapFromGlobal(
		button->mapToGlobal(QPoint()));
	_emojiPanel->setDropDown(true);
	_emojiPanel->moveTopRight(
		local.y() + button->height(),
		local.x() + button->width());
}

void WindowHost::Impl::updateEditorVisibleTopBottom() {
	if (!_scroll || !_editor) {
		return;
	}
	const auto scrollTop = _scroll->scrollTop();
	_editor->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
}

void WindowHost::Impl::setEmojiPanelInteractionActive(bool active) {
	if (_emojiPanelInteractionActive == active || !_editor) {
		_emojiPanelInteractionActive = active;
		return;
	}
	_emojiPanelInteractionActive = active;
	_editor->setInlineFieldExternalInteractionActive(active);
}

void WindowHost::Impl::finishCloseFromAcceptedEvent() {
	_closingApproved = true;
	if (_closedNotified) {
		return;
	}
	_closedNotified = true;
	setEmojiPanelInteractionActive(false);
	if (_emojiPanel) {
		_emojiPanel->hideFast();
	}
	if (const auto closed = base::take(_closed)) {
		crl::on_main(closed);
	}
}

void WindowHost::Impl::finishClose() {
	finishCloseFromAcceptedEvent();
	if (_window) {
		_window->close();
	}
}

bool WindowHost::Impl::confirmCancel() {
	return !_cancelled || _cancelled();
}

void WindowHost::Impl::submit() {
	if (!_editor) {
		return;
	}
	if (_editor->commitInlineField() == State::ApplyResult::Failed) {
		return;
	}
	if (!_confirmed || _confirmed()) {
		finishClose();
	}
}

WindowHost::WindowHost(ShowWindowDescriptor descriptor)
: _impl(std::make_unique<Impl>(std::move(descriptor))) {
}

WindowHost::~WindowHost() = default;

std::unique_ptr<WindowHost> ShowWindow(ShowWindowDescriptor descriptor) {
	if (!descriptor.state) {
		descriptor.state = std::make_shared<State>();
	}
	return std::unique_ptr<WindowHost>(new WindowHost(std::move(descriptor)));
}

} // namespace Iv::Editor
