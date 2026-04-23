/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "info/info_content_widget.h"
#include "storage/storage_shared_media.h"

namespace Ui {
class ElasticScroll;
class ChatStyle;
class ChatTheme;
} // namespace Ui

namespace HistoryView {
class ListWidget;
} // namespace HistoryView

namespace Data {
class ForumTopic;
class SavedSublist;
} // namespace Data

namespace Info::Polls {

class ListMemento final : public ContentMemento {
public:
	ListMemento(
		not_null<PeerData*> peer,
		PeerId migratedPeerId);
	ListMemento(not_null<Data::ForumTopic*> topic);
	ListMemento(not_null<Data::SavedSublist*> sublist);

	object_ptr<ContentWidget> createWidget(
		QWidget *parent,
		not_null<Controller*> controller,
		const QRect &geometry) override;

	[[nodiscard]] Section section() const override;

	void setScrollTop(int scrollTop) {
		_scrollTop = scrollTop;
	}
	[[nodiscard]] int scrollTop() const {
		return _scrollTop;
	}

private:
	int _scrollTop = 0;

};

class ListWidget final : public ContentWidget {
public:
	ListWidget(
		QWidget *parent,
		not_null<Controller*> controller);
	~ListWidget();

	bool showInternal(
		not_null<ContentMemento*> memento) override;

	void setInternalState(
		const QRect &geometry,
		not_null<ListMemento*> memento);

	void fillTopBarMenu(
		const Ui::Menu::MenuCallback &addAction) override;

	rpl::producer<QString> title() override;
	rpl::producer<int> desiredHeightValue() const override;

	rpl::producer<SelectedItems> selectedListValue() const override;
	void selectionAction(SelectionAction action) override;

private:
	void setupSearch();
	void saveState(not_null<ListMemento*> memento);
	void restoreState(not_null<ListMemento*> memento);

	std::shared_ptr<ContentMemento> doCreateMemento() override;

	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

	class Inner;
	const std::unique_ptr<Inner> _inner;

};

} // namespace Info::Polls
