// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include <utility>

#include "ayu/ui/context_menu/menu_item_subtext.h"

#include "ayu/utils/telegram_helpers.h"
#include "ayu/database/entities.h"
#include "mainwindow.h"
#include "qguiapplication.h"

#include "ui/widgets/menu/menu_action.h"
#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "base/unixtime.h"
#include "styles/style_chat.h"
#include "styles/style_menu_icons.h"
#include "window/window_session_controller.h"

namespace Ui
{
namespace
{

class ActionWithSubText : public Menu::ItemBase
{
public:
	ActionWithSubText(
		not_null<RpWidget *> parent,
		const style::Menu &st,
		const style::icon &icon,
		Fn<void()> callback,
		const QString &title,
		QString subtext);

	bool isEnabled() const override;
	not_null<QAction *> action() const override;

	void handleKeyPress(not_null<QKeyEvent *> e) override;

protected:
	QPoint prepareRippleStartPosition() const override;
	QImage prepareRippleMask() const override;

	int contentHeight() const override;

	void prepare(const QString &title);
	void paint(Painter &p);

	const not_null<QAction *> _dummyAction;
	const style::Menu &_st;
	const style::icon &_icon;

	Text::String _title;
	int _textWidth = 0;
	QString _subText;
	const int _height;

};

class ActionStickerPackAuthor final : public ActionWithSubText
{
public:
	ActionStickerPackAuthor(not_null<Menu::Menu *> menu, not_null<Main::Session *> session, ID authorId);

private:
	not_null<Main::Session *> _session;

	void searchAuthor(ID authorId);
};

TextParseOptions MenuTextOptions = {
	TextParseLinks, // flags
	0, // maxw
	0, // maxh
	Qt::LayoutDirectionAuto, // dir
};

ActionWithSubText::ActionWithSubText(
	not_null<RpWidget *> parent,
	const style::Menu &st,
	const style::icon &icon,
	Fn<void()> callback,
	const QString &title,
	QString subtext)
	: ItemBase(parent, st),
	  _dummyAction(new QAction(parent)),
	  _st(st),
	  _icon(icon),
	  _subText(std::move(subtext)),
	  _height(st::ttlItemPadding.top()
				  + _st.itemStyle.font->height
				  + st::ttlItemTimerFont->height
				  + st::ttlItemPadding.bottom())
{
	setAcceptBoth(true);
	initResizeHook(parent->sizeValue());
	setClickedCallback(std::move(callback));

	paintRequest(
	) | rpl::start_with_next([=]
							 {
								 Painter p(this);
								 paint(p);
							 }, lifetime());

	enableMouseSelecting();
	prepare(title);
}

void ActionWithSubText::paint(Painter &p)
{
	const auto selected = isSelected();
	if (selected && _st.itemBgOver->c.alpha() < 255) {
		p.fillRect(0, 0, width(), _height, _st.itemBg);
	}
	p.fillRect(0, 0, width(), _height, selected ? _st.itemBgOver : _st.itemBg);
	if (isEnabled()) {
		paintRipple(p, 0, 0);
	}

	const auto normalHeight = _st.itemPadding.top()
		+ _st.itemStyle.font->height
		+ _st.itemPadding.bottom();
	const auto deltaHeight = _height - normalHeight;
	_icon.paint(
		p,
		_st.itemIconPosition + QPoint(0, deltaHeight / 2),
		width());

	p.setPen(selected ? _st.itemFgOver : _st.itemFg);
	_title.drawLeftElided(
		p,
		_st.itemPadding.left(),
		st::ttlItemPadding.top(),
		_textWidth,
		width());

	p.setFont(st::ttlItemTimerFont);
	p.setPen(selected ? _st.itemFgShortcutOver : _st.itemFgShortcut);
	p.drawTextLeft(
		_st.itemPadding.left(),
		st::ttlItemPadding.top() + _st.itemStyle.font->height,
		width(),
		_subText);
}

void ActionWithSubText::prepare(const QString &title)
{
	_title.setMarkedText(
		_st.itemStyle,
		{title},
		MenuTextOptions);
	const auto textWidth = _title.maxWidth();
	const auto &padding = _st.itemPadding;

	const auto goodWidth = padding.left()
		+ textWidth
		+ padding.right();
	const auto subTextMaxWidth = padding.left()
		+ st::ttlItemTimerFont->width(_subText)
		+ padding.right();

	const auto w = std::clamp(
		std::max({goodWidth, subTextMaxWidth}),
		_st.widthMin,
		_st.widthMax);
	_textWidth = w - (goodWidth - textWidth);
	setMinWidth(w);
	update();
}

bool ActionWithSubText::isEnabled() const
{
	return true;
}

not_null<QAction *> ActionWithSubText::action() const
{
	return _dummyAction;
}

QPoint ActionWithSubText::prepareRippleStartPosition() const
{
	return mapFromGlobal(QCursor::pos());
}

QImage ActionWithSubText::prepareRippleMask() const
{
	return Ui::RippleAnimation::RectMask(size());
}

int ActionWithSubText::contentHeight() const
{
	return _height;
}

void ActionWithSubText::handleKeyPress(not_null<QKeyEvent *> e)
{
	if (!isSelected()) {
		return;
	}
	const auto key = e->key();
	if (key == Qt::Key_Enter || key == Qt::Key_Return) {
		setClicked(Menu::TriggeredSource::Keyboard);
	}
}

ActionStickerPackAuthor::ActionStickerPackAuthor(not_null<Menu::Menu *> menu,
												 not_null<Main::Session *> session,
												 ID authorId)
	: ActionWithSubText(menu, menu->st(), st::menuIconStickers, [=]
{ }, tr::ayu_MessageDetailsPackOwnerPC(tr::now), QString(tr::ayu_MessageDetailsPackOwnerFetchingPC(tr::now))),
	  _session(session)
{
	searchAuthor(authorId);
}

void ActionStickerPackAuthor::searchAuthor(ID authorId)
{
	const auto pointer = Ui::MakeWeak(this);
	searchById(authorId, _session, [=](const QString &username, UserData *user)
	{
		if (!pointer) {
			LOG(("ContextActionStickerAuthor: searchById callback after destruction"));
			return;
		}
		if (username.isEmpty() && !user) {
			_subText = QString(tr::ayu_MessageDetailsPackOwnerNotFoundPC(tr::now));
			setClickedCallback(
				[=]
				{
					const auto text =
						QString("int32: %1\nint64: %2").arg(authorId).arg(0x100000000L + authorId);
					QGuiApplication::clipboard()->setText(text);
				});

			crl::on_main(
				[=]
				{
					update();
				});
			return;
		}

		const auto title = username.isEmpty() ? user ? user->name() : QString() : username;
		const auto callback = [=]
		{
			if (user) {
				if (const auto window = _session->tryResolveWindow()) {
					if (const auto mainWidget = window->widget()->sessionController()) {
						mainWidget->showPeer(user);
					}
				}
			}
			else {
				QGuiApplication::clipboard()->setText(title);
			}
		};

		setClickedCallback(callback);

		_subText = QString(title);
		crl::on_main(
			[=]
			{
				update();
			});
	});
}

} // namespace

base::unique_qptr<Menu::ItemBase> ContextActionWithSubText(
	not_null<Menu::Menu *> menu,
	const style::icon &icon,
	const QString &title,
	const QString &subtext,
	Fn<void()> callback)
{
	if (!callback) {
		callback = [=]()
		{
			QGuiApplication::clipboard()->setText(subtext);
		};
	}

	return base::make_unique_q<ActionWithSubText>(
		menu,
		menu->st(),
		icon,
		std::move(callback),
		title,
		subtext);
}

base::unique_qptr<Menu::ItemBase> ContextActionStickerAuthor(
	not_null<Menu::Menu *> menu,
	not_null<Main::Session *> session,
	ID authorId)
{
	return base::make_unique_q<ActionStickerPackAuthor>(menu, session, authorId);
}

} // namespace Ui
