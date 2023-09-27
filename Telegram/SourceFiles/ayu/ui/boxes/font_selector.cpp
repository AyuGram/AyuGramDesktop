// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "font_selector.h"

#include "data/data_peer_values.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/multi_select.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/dropdown_menu.h"
#include "ui/widgets/box_content_divider.h"
#include "ui/text/text_entity.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/effects/ripple_animation.h"
#include "ui/toast/toast.h"
#include "ui/text/text_options.h"
#include "ui/painter.h"
#include "storage/localstorage.h"
#include "boxes/premium_preview_box.h"
#include "ui/boxes/confirm_box.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "core/application.h"
#include "lang/lang_instance.h"
#include "lang/lang_cloud_manager.h"
#include "settings/settings_common.h"
#include "spellcheck/spellcheck_types.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_passport.h"
#include "styles/style_chat_helpers.h"

#include <QtGui/QGuiApplication>
#include <QFontDatabase>
#include <QApplication>

#include <ayu/ayu_settings.h>

struct Font
{
	QString FontName;
	QString id;
};

namespace AyuUi
{

class Rows : public Ui::RpWidget
{
public:
	Rows(
		QWidget *parent,
		const std::vector<Font> &data,
		const QString &chosen);

	void filter(const QString &query);

	int count() const;

	int selected() const;

	void setSelected(int selected);

	rpl::producer<bool> hasSelection() const;

	rpl::producer<bool> isEmpty() const;

	void activateSelected();

	rpl::producer<Font> activations() const;

	void changeChosen(const QString &chosen);

	Ui::ScrollToRequest rowScrollRequest(int index) const;

	static int DefaultRowHeight();

protected:
	int resizeGetHeight(int newWidth) override;

	void paintEvent(QPaintEvent *e) override;

	void mouseMoveEvent(QMouseEvent *e) override;

	void mousePressEvent(QMouseEvent *e) override;

	void mouseReleaseEvent(QMouseEvent *e) override;

	void leaveEventHook(QEvent *e) override;

private:
	struct Row
	{
		Font data;
		Ui::Text::String title = {st::boxWideWidth / 2};
		Ui::Text::String description = {st::boxWideWidth / 2};
		int top = 0;
		int height = 0;
		mutable std::unique_ptr<Ui::RippleAnimation> ripple;
		mutable std::unique_ptr<Ui::RippleAnimation> menuToggleRipple;
		bool menuToggleForceRippled = false;
		int titleHeight = 0;
		int descriptionHeight = 0;
		QStringList keywords;
		std::unique_ptr<Ui::RadioView> check;
		bool removed = false;
	};

	struct RowSelection
	{
		int index = 0;

		inline bool operator==(const RowSelection &other) const
		{
			return (index == other.index);
		}
	};

	using Selection = std::variant<v::null_t, RowSelection>;

	void updateSelected(Selection selected);

	void updatePressed(Selection pressed);

	Rows::Row &rowByIndex(int index);

	const Rows::Row &rowByIndex(int index) const;

	Rows::Row &rowBySelection(Selection selected);

	const Rows::Row &rowBySelection(Selection selected) const;

	std::unique_ptr<Ui::RippleAnimation> &rippleBySelection(
		Selection selected);

	[[maybe_unused]] const std::unique_ptr<Ui::RippleAnimation> &rippleBySelection(
		Selection selected) const;

	std::unique_ptr<Ui::RippleAnimation> &rippleBySelection(
		not_null<Row *> row,
		Selection selected);

	[[maybe_unused]] const std::unique_ptr<Ui::RippleAnimation> &rippleBySelection(
		not_null<const Row *> row,
		Selection selected) const;

	void addRipple(Selection selected, QPoint position);

	void ensureRippleBySelection(Selection selected);

	void ensureRippleBySelection(not_null<Row *> row, Selection selected);

	int indexFromSelection(Selection selected) const;

	int countAvailableWidth() const;

	int countAvailableWidth(int newWidth) const;

	void repaint(Selection selected);

	void repaint(int index);

	void repaint(const Row &row);

	void repaintChecked(not_null<const Row *> row);

	void activateByIndex(int index);

	void setForceRippled(not_null<Row *> row, bool rippled);

	void restore(not_null<Row *> row);

	std::vector<Row> _rows;
	std::vector<not_null<Row *>> _filtered;
	Selection _selected;
	Selection _pressed;
	QString _chosen;
	QStringList _query;

	bool _areOfficial = false;
	bool _mouseSelection = false;
	QPoint _globalMousePosition;

	rpl::event_stream<bool> _hasSelection;
	rpl::event_stream<Font> _activations;
	rpl::event_stream<bool> _isEmpty;

};

class Content : public Ui::RpWidget
{
public:
	Content(
		QWidget *parent,
		const std::vector<Font> &fonts);

	Ui::ScrollToRequest jump(int rows);

	void filter(const QString &query);

	rpl::producer<Font> activations() const;

	void changeChosen(const QString &chosen);

	void activateBySubmit();

private:
	void setupContent(
		const std::vector<Font> &fonts);

	Fn<Ui::ScrollToRequest(int rows)> _jump;
	Fn<void(const QString &query)> _filter;
	Fn<rpl::producer<Font>()> _activations;
	Fn<void(const QString &chosen)> _changeChosen;
	Fn<void()> _activateBySubmit;

};

std::vector<Font> PrepareFonts()
{
	auto fonts = std::vector<Font>();
	QFontDatabase base;

	for (const auto &font : base.families()) {
		Font fontItem = {
			.FontName=font,
			.id=font
		};

		fonts.push_back(fontItem);
	}


	return fonts;
}

Rows::Rows(
	QWidget *parent,
	const std::vector<Font> &data,
	const QString &chosen)
	: RpWidget(parent), _chosen(chosen)
{
	const auto descriptionOptions = TextParseOptions{
		TextParseMultiline,
		0,
		0,
		Qt::LayoutDirectionAuto
	};
	_rows.reserve(data.size());
	for (const auto &item : data) {
		_rows.push_back(Row{item});
		auto &row = _rows.back();
		row.check = std::make_unique<Ui::RadioView>(
			st::langsRadio,
			(row.data.id == _chosen),
			[=, row = &row]
			{ repaint(*row); });
		row.title.setText(
			st::defaultTextStyle,
			item.FontName,
			Ui::NameTextOptions());
		row.keywords = TextUtilities::PrepareSearchWords(
			item.FontName + ' ' + item.id);
	}
	resizeToWidth(width());
	setAttribute(Qt::WA_MouseTracking);
	update();
}

void Rows::mouseMoveEvent(QMouseEvent *e)
{
	const auto position = e->globalPos();
	if (!_mouseSelection && position == _globalMousePosition) {
		return;
	}
	_mouseSelection = true;
	_globalMousePosition = position;
	const auto index = [&]
	{
		const auto y = e->pos().y();
		if (y < 0) {
			return -1;
		}
		for (auto i = 0, till = count(); i != till; ++i) {
			const auto &row = rowByIndex(i);
			if (row.top + row.height > y) {
				return i;
			}
		}
		return -1;
	}();
	const auto row = (index >= 0) ? &rowByIndex(index) : nullptr;
	if (index < 0) {
		updateSelected({});
	}
	else if (!row->removed) {
		updateSelected(RowSelection{index});
	}
	else {
		updateSelected({});
	}
}

void Rows::mousePressEvent(QMouseEvent *e)
{
	updatePressed(_selected);
	if (!v::is_null(_pressed)
		&& !rowBySelection(_pressed).menuToggleForceRippled) {
		addRipple(_pressed, e->pos());
	}
}

void Rows::addRipple(Selection selected, QPoint position)
{
	Expects(!v::is_null(selected));

	ensureRippleBySelection(selected);

	const auto &row = rowBySelection(selected);
	auto &ripple = rippleBySelection(&row, selected);
	const auto topleft = QPoint(0, row.top);
	ripple->add(position - topleft);
}

void Rows::ensureRippleBySelection(Selection selected)
{
	ensureRippleBySelection(&rowBySelection(selected), selected);
}

void Rows::ensureRippleBySelection(not_null<Row *> row, Selection selected)
{
	auto &ripple = rippleBySelection(row, selected);
	if (ripple) {
		return;
	}
	auto mask = Ui::RippleAnimation::RectMask({width(), row->height});
	ripple = std::make_unique<Ui::RippleAnimation>(
		st::defaultRippleAnimation,
		std::move(mask),
		[=]
		{ repaintChecked(row); });
}

void Rows::mouseReleaseEvent(QMouseEvent *e)
{
	const auto pressed = _pressed;
	updatePressed({});
	if (pressed == _selected) {
		v::match(pressed, [&](RowSelection data)
		{
			activateByIndex(data.index);
		}, [](v::null_t)
				 { });
	}
}

void Rows::restore(not_null<Row *> row)
{
	row->removed = false;
}

void Rows::setForceRippled(not_null<Row *> row, bool rippled)
{
	repaint(*row);
}

void Rows::activateByIndex(int index)
{
	_activations.fire_copy(rowByIndex(index).data);
}

void Rows::leaveEventHook(QEvent *e)
{
	updateSelected({});
}

void Rows::filter(const QString &query)
{
	updateSelected({});
	updatePressed({});

	_query = TextUtilities::PrepareSearchWords(query);

	const auto skip = [](
		const QStringList &haystack,
		const QStringList &needles)
	{
		const auto find = [](
			const QStringList &haystack,
			const QString &needle)
		{
			for (const auto &item : haystack) {
				if (item.startsWith(needle)) {
					return true;
				}
			}
			return false;
		};
		for (const auto &needle : needles) {
			if (!find(haystack, needle)) {
				return true;
			}
		}
		return false;
	};

	if (!_query.isEmpty()) {
		_filtered.clear();
		_filtered.reserve(_rows.size());
		for (auto &row : _rows) {
			if (!skip(row.keywords, _query)) {
				_filtered.push_back(&row);
			}
			else {
				row.ripple = nullptr;
			}
		}
	}

	resizeToWidth(width());
	Ui::SendPendingMoveResizeEvents(this);

	_isEmpty.fire(count() == 0);
}

int Rows::count() const
{
	return _query.isEmpty() ? _rows.size() : _filtered.size();
}

int Rows::indexFromSelection(Selection selected) const
{
	return v::match(selected, [&](RowSelection data)
	{
		return data.index;
	}, [](v::null_t)
					{
						return -1;
					});
}

int Rows::selected() const
{
	return indexFromSelection(_selected);
}

void Rows::activateSelected()
{
	const auto index = selected();
	if (index >= 0) {
		activateByIndex(index);
	}
}

rpl::producer<Font> Rows::activations() const
{
	return _activations.events();
}

void Rows::changeChosen(const QString &chosen)
{
	for (const auto &row : _rows) {
		row.check->setChecked(row.data.id == chosen, anim::type::normal);
	}
}

void Rows::setSelected(int selected)
{
	_mouseSelection = false;
	const auto limit = count();
	if (selected >= 0 && selected < limit) {
		updateSelected(RowSelection{selected});
	}
	else {
		updateSelected({});
	}
}

rpl::producer<bool> Rows::hasSelection() const
{
	return _hasSelection.events();
}

rpl::producer<bool> Rows::isEmpty() const
{
	return _isEmpty.events_starting_with(
		count() == 0
	) | rpl::distinct_until_changed();
}

void Rows::repaint(Selection selected)
{
	v::match(selected, [](v::null_t)
	{
	}, [&](const auto &data)
			 {
				 repaint(data.index);
			 });
}

void Rows::repaint(int index)
{
	if (index >= 0) {
		repaint(rowByIndex(index));
	}
}

void Rows::repaint(const Row &row)
{
	update(0, row.top, width(), row.height);
}

void Rows::repaintChecked(not_null<const Row *> row)
{
	const auto found = (ranges::find(_filtered, row) != end(_filtered));
	if (_query.isEmpty() || found) {
		repaint(*row);
	}
}

void Rows::updateSelected(Selection selected)
{
	const auto changed = (v::is_null(_selected) != v::is_null(selected));
	repaint(_selected);
	_selected = selected;
	repaint(_selected);
	if (changed) {
		_hasSelection.fire(!v::is_null(_selected));
	}
}

void Rows::updatePressed(Selection pressed)
{
	if (!v::is_null(_pressed)) {
		if (!rowBySelection(_pressed).menuToggleForceRippled) {
			if (const auto ripple = rippleBySelection(_pressed).get()) {
				ripple->lastStop();
			}
		}
	}
	_pressed = pressed;
}

Rows::Row &Rows::rowByIndex(int index)
{
	Expects(index >= 0 && index < count());

	return _query.isEmpty() ? _rows[index] : *_filtered[index];
}

const Rows::Row &Rows::rowByIndex(int index) const
{
	Expects(index >= 0 && index < count());

	return _query.isEmpty() ? _rows[index] : *_filtered[index];
}

Rows::Row &Rows::rowBySelection(Selection selected)
{
	return rowByIndex(indexFromSelection(selected));
}

const Rows::Row &Rows::rowBySelection(Selection selected) const
{
	return rowByIndex(indexFromSelection(selected));
}

std::unique_ptr<Ui::RippleAnimation> &Rows::rippleBySelection(
	Selection selected)
{
	return rippleBySelection(&rowBySelection(selected), selected);
}

const std::unique_ptr<Ui::RippleAnimation> &Rows::rippleBySelection(
	Selection selected) const
{
	return rippleBySelection(&rowBySelection(selected), selected);
}

std::unique_ptr<Ui::RippleAnimation> &Rows::rippleBySelection(
	not_null<Row *> row,
	Selection selected)
{
	return row->ripple;
}

const std::unique_ptr<Ui::RippleAnimation> &Rows::rippleBySelection(
	not_null<const Row *> row,
	Selection selected) const
{
	return const_cast<Rows *>(this)->rippleBySelection(
		const_cast<Row *>(row.get()),
		selected);
}

Ui::ScrollToRequest Rows::rowScrollRequest(int index) const
{
	const auto &row = rowByIndex(index);
	return Ui::ScrollToRequest(row.top, row.top + row.height);
}

int Rows::DefaultRowHeight()
{
	return st::passportRowPadding.top()
		+ st::semiboldFont->height
		+ st::passportRowSkip
		+ st::normalFont->height
		+ st::passportRowPadding.bottom();
}

int Rows::resizeGetHeight(int newWidth)
{
	const auto availableWidth = countAvailableWidth(newWidth);
	auto result = 0;
	for (auto i = 0, till = count(); i != till; ++i) {
		auto &row = rowByIndex(i);
		row.top = result;
		row.titleHeight = row.title.countHeight(availableWidth);
		row.descriptionHeight = row.description.countHeight(availableWidth);
		row.height = st::passportRowPadding.top()
			+ row.titleHeight
			+ st::passportRowSkip
			+ row.descriptionHeight
			+ st::passportRowPadding.bottom();
		result += row.height;
	}
	return result;
}

int Rows::countAvailableWidth(int newWidth) const
{
	const auto right = 0;
	return newWidth
		- st::passportRowPadding.left()
		- st::langsRadio.diameter
		- st::passportRowPadding.left()
		- right
		- st::passportRowIconSkip;
}

int Rows::countAvailableWidth() const
{
	return countAvailableWidth(width());
}

void Rows::paintEvent(QPaintEvent *e)
{
	Painter p(this);

	const auto clip = e->rect();

	const auto checkLeft = st::passportRowPadding.left();
	const auto left = checkLeft
		+ st::langsRadio.diameter
		+ st::passportRowPadding.left();
	const auto availableWidth = countAvailableWidth();
	const auto selectedIndex = indexFromSelection(!v::is_null(_pressed) ? _pressed : _selected);
	for (auto i = 0, till = count(); i != till; ++i) {
		const auto &row = rowByIndex(i);
		if (row.top + row.height <= clip.y()) {
			continue;
		}
		else if (row.top >= clip.y() + clip.height()) {
			break;
		}
		p.setOpacity(row.removed ? st::stickersRowDisabledOpacity : 1.);
		p.translate(0, row.top);
		const auto guard = gsl::finally([&]
										{ p.translate(0, -row.top); });

		const auto selected = (selectedIndex == i);
		if (selected && !row.removed) {
			p.fillRect(0, 0, width(), row.height, st::windowBgOver);
		}

		if (row.ripple) {
			row.ripple->paint(p, 0, 0, width());
			if (row.ripple->empty()) {
				row.ripple.reset();
			}
		}

		const auto checkTop = (row.height - st::defaultRadio.diameter) / 2;
		row.check->paint(p, checkLeft, checkTop, width());

		auto top = st::passportRowPadding.top();

		p.setPen(st::passportRowTitleFg);
		row.title.drawLeft(p, left, top, availableWidth, width());
		top += row.titleHeight + st::passportRowSkip;

		p.setPen(selected ? st::windowSubTextFgOver : st::windowSubTextFg);
		row.description.drawLeft(p, left, top, availableWidth, width());
		top += row.descriptionHeight + st::passportRowPadding.bottom();
	}
}

Content::Content(
	QWidget *parent,
	const std::vector<Font> &fonts)
	: RpWidget(parent)
{
	setupContent(fonts);
}

void Content::setupContent(
	const std::vector<Font> &fonts)
{
	using namespace rpl::mappers;

	const auto current = AyuFonts::getMainFont();
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	const auto add = [&](const std::vector<Font> &list)
	{
		if (list.empty()) {
			return (Rows *)nullptr;
		}
		const auto wrap = content->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				content,
				object_ptr<Ui::VerticalLayout>(content)));
		const auto inner = wrap->entity();
		inner->add(object_ptr<Ui::FixedHeightWidget>(
			inner,
			st::defaultBox.margin.top()));
		const auto rows = inner->add(object_ptr<Rows>(
			inner,
			list,
			current));
		inner->add(object_ptr<Ui::FixedHeightWidget>(
			inner,
			st::defaultBox.margin.top()));

		rows->isEmpty() | rpl::start_with_next([=](bool empty)
											   {
												   wrap->toggle(!empty, anim::type::instant);
											   }, rows->lifetime());

		return rows;
	};
	const auto main = add(fonts);
	const auto divider = content->add(
		object_ptr<Ui::SlideWrap<Ui::BoxContentDivider>>(
			content,
			object_ptr<Ui::BoxContentDivider>(content)));
	const auto empty = content->add(
		object_ptr<Ui::SlideWrap<Ui::FixedHeightWidget>>(
			content,
			object_ptr<Ui::FixedHeightWidget>(
				content,
				st::membersAbout.style.font->height * 9)));
	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		empty->entity(),
		rpl::single(qs("No fonts found.")),
		st::membersAbout);
	empty->entity()->sizeValue(
	) | rpl::start_with_next([=](QSize size)
							 {
								 label->move(
									 (size.width() - label->width()) / 2,
									 (size.height() - label->height()) / 2);
							 }, label->lifetime());

	empty->toggleOn(
		main ? main->isEmpty() : rpl::single(true),
		anim::type::instant);

	Ui::ResizeFitChild(this, content);


	divider->hide(anim::type::instant);

	const auto count = [](Rows *widget)
	{
		return widget ? widget->count() : 0;
	};
	const auto selected = [](Rows *widget)
	{
		return widget ? widget->selected() : -1;
	};
	const auto rowsCount = [=]
	{
		return count(main);
	};
	const auto selectedIndex = [=]
	{
		if (const auto index = selected(main); index >= 0) {
			return index;
		}

		return -1;
	};
	const auto setSelectedIndex = [=](int index)
	{
		const auto first = count(main);
		if (index >= first) {
			if (main) {
				main->setSelected(-1);
			}
		}
		else {
			if (main) {
				main->setSelected(index);
			}
		}
	};
	const auto selectedCoords = [=]
	{
		const auto coords = [=](Rows *rows, int index)
		{
			const auto result = rows->rowScrollRequest(index);
			const auto shift = rows->mapToGlobal({0, 0}).y()
				- mapToGlobal({0, 0}).y();
			return Ui::ScrollToRequest(
				result.ymin + shift,
				result.ymax + shift);
		};
		if (const auto index = selected(main); index >= 0) {
			return coords(main, index);
		}
		return Ui::ScrollToRequest(-1, -1);
	};
	_jump = [=](int rows)
	{
		const auto count = rowsCount();
		const auto now = selectedIndex();
		if (now >= 0) {
			const auto changed = now + rows;
			if (changed < 0) {
				setSelectedIndex((now > 0) ? 0 : -1);
			}
			else if (changed >= count) {
				setSelectedIndex(count - 1);
			}
			else {
				setSelectedIndex(changed);
			}
		}
		else if (rows > 0) {
			setSelectedIndex(0);
		}
		return selectedCoords();
	};
	const auto filter = [](Rows *widget, const QString &query)
	{
		if (widget) {
			widget->filter(query);
		}
	};
	_filter = [=](const QString &query)
	{
		filter(main, query);
	};
	_activations = [=]
	{
		if (!main) {
			return rpl::never<Font>() | rpl::type_erased();
		}
		return rpl::merge(
			main->activations()
		) | rpl::type_erased();
	};
	_changeChosen = [=](const QString &chosen)
	{
		if (main) {
			main->changeChosen(chosen);
		}
	};
	_activateBySubmit = [=]
	{
		if (selectedIndex() < 0) {
			_jump(1);
		}
		if (main) {
			main->activateSelected();
		}
	};
};

void Content::filter(const QString &query)
{
	_filter(query);
}

rpl::producer<Font> Content::activations() const
{
	return _activations();
}

void Content::changeChosen(const QString &chosen)
{
	_changeChosen(chosen);
}

void Content::activateBySubmit()
{
	_activateBySubmit();
}

Ui::ScrollToRequest Content::jump(int rows)
{
	return _jump(rows);
}

} // namespace

AyuUi::FontSelectorBox::FontSelectorBox(QWidget *, Window::SessionController *controller, Fn<void(QString font)> hook)
	: _controller(controller), _hook(hook)
{
}

void AyuUi::FontSelectorBox::prepare()
{
	addButton(tr::lng_box_ok(), [=]
	{

		_hook(_selectedFont);

		_controller->show(Ui::MakeConfirmBox({
												 .text = tr::lng_settings_need_restart(),
												 .confirmed = []
												 { Core::Restart(); },
												 .confirmText = tr::lng_settings_restart_now(),
												 .cancelText = tr::lng_settings_restart_later(),
											 }));
		closeBox();
	});

	addLeftButton(tr::ayu_BoxActionReset(), [=]
	{
		_hook(qs(""));

		_controller->show(Ui::MakeConfirmBox({
												 .text = tr::lng_settings_need_restart(),
												 .confirmed = []
												 { Core::Restart(); },
												 .confirmText = tr::lng_settings_restart_now(),
												 .cancelText = tr::lng_settings_restart_later(),
											 }));


		closeBox();
	});

	setTitle(tr::ayu_CustomizeFontTitle());

	const auto topContainer = Ui::CreateChild<Ui::VerticalLayout>(this);
	setupTop(topContainer);
	const auto select = topContainer->add(
		object_ptr<Ui::MultiSelect>(
			topContainer,
			st::defaultMultiSelect,
			tr::lng_participant_filter()));
	topContainer->resizeToWidth(st::boxWidth);

	using namespace rpl::mappers;

	const auto fonts = AyuUi::PrepareFonts();
	const auto inner = setInnerWidget(
		object_ptr<AyuUi::Content>(this, fonts),
		st::boxScroll,
		topContainer->height());
	inner->resizeToWidth(st::boxWidth);

	const auto max = lifetime().make_state<int>(0);
	rpl::combine(
		inner->heightValue(),
		topContainer->heightValue(),
		_1 + _2
	) | rpl::start_with_next([=](int height)
							 {
								 accumulate_max(*max, height);
								 setDimensions(st::boxWidth, qMin(*max, st::boxMaxListHeight));
							 }, inner->lifetime());
	topContainer->heightValue(
	) | rpl::start_with_next([=](int height)
							 {
								 setInnerTopSkip(height);
							 }, inner->lifetime());

	select->setSubmittedCallback([=](Qt::KeyboardModifiers)
								 {
									 inner->activateBySubmit();
								 });
	select->setQueryChangedCallback([=](const QString &query)
									{
										inner->filter(query);
									});
	select->setCancelledCallback([=]
								 {
									 select->clearQuery();
								 });

	inner->activations(
	) | rpl::start_with_next([=](const Font &font)
							 {
								 if (inner) {
									 inner->changeChosen(font.id);
									 _selectedFont = font.id;
								 }
							 }, inner->lifetime());

	_setInnerFocus = [=]
	{
		select->setInnerFocus();
	};
	_jump = [=](int rows)
	{
		return inner->jump(rows);
	};
}

void AyuUi::FontSelectorBox::setupTop(not_null<Ui::VerticalLayout *> container)
{
	if (!_controller) {
		return;
	}
}

void AyuUi::FontSelectorBox::keyPressEvent(QKeyEvent *e)
{
	const auto key = e->key();
	if (key == Qt::Key_Escape) {
		closeBox();
		return;
	}
	const auto selected = [&]
	{
		if (key == Qt::Key_Up) {
			return _jump(-1);
		}
		else if (key == Qt::Key_Down) {
			return _jump(1);
		}
		else if (key == Qt::Key_PageUp) {
			return _jump(-rowsInPage());
		}
		else if (key == Qt::Key_PageDown) {
			return _jump(rowsInPage());
		}
		return Ui::ScrollToRequest(-1, -1);
	}();
	if (selected.ymin >= 0 && selected.ymax >= 0) {
		scrollToY(selected.ymin, selected.ymax);
	}
}

int AyuUi::FontSelectorBox::rowsInPage() const
{
	return std::max(height() / AyuUi::Rows::DefaultRowHeight(), 1);
}

void AyuUi::FontSelectorBox::setInnerFocus()
{
	_setInnerFocus();
}

base::binary_guard
AyuUi::FontSelectorBox::Show(Window::SessionController *controller, const Fn<void(QString font)> hook)
{
	auto result = base::binary_guard();

	Ui::show(Box<FontSelectorBox>(controller, hook));

	return result;
}
