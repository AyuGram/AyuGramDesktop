// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/ui/boxes/local_messages_manager.h"

#include "ayu/ui/boxes/local_message_editor.h"
#include "ayu/data/messages_storage.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang_auto.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/controls/userpic_button.h"
#include "ui/text/format_values.h"
#include "ui/boxes/confirm_box.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_menu_icons.h"

#include <QtCore/QDateTime>

namespace AyuUi {

namespace {

class LocalMessageRow : public Ui::RpWidget {
public:
	LocalMessageRow(
		QWidget *parent,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] not_null<HistoryItem*> item() const {
		return _item;
	}

	rpl::producer<> editRequests() const {
		return _editRequests.events();
	}

	rpl::producer<> deleteRequests() const {
		return _deleteRequests.events();
	}

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	void setupContent();
	void showContextMenu(QPoint position);

	const not_null<HistoryItem*> _item;
	const not_null<Window::SessionController*> _controller;

	Ui::UserpicButton* _userpic = nullptr;
	QString _dateText;
	QString _previewText;

	rpl::event_stream<> _editRequests;
	rpl::event_stream<> _deleteRequests;
};

LocalMessageRow::LocalMessageRow(
	QWidget *parent,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller)
: RpWidget(parent)
, _item(item)
, _controller(controller) {
	setupContent();
}

void LocalMessageRow::setupContent() {
	const auto height = st::defaultPeerListItem.height;
	resize(width(), height);

	_userpic = Ui::CreateChild<Ui::UserpicButton>(
		this,
		st::defaultUserpicButton);
	_userpic->move(st::defaultPeerListItem.photoPosition);

	if (const auto from = _item->from()) {
		if (const auto user = _controller->session().data().user(from)) {
			_userpic->showUser(user);
		} else {
			_userpic->showPeer(_item->history()->peer);
		}
	} else {
		_userpic->showPeer(_item->history()->peer);
	}

	// Format date
	const auto dateTime = QDateTime::fromSecsSinceEpoch(_item->date());
	_dateText = dateTime.toString("dd.MM.yyyy hh:mm");

	// Format preview text
	const auto text = _item->originalText().text;
	_previewText = text.left(100);
	if (text.length() > 100) {
		_previewText += "...";
	}
	if (_previewText.isEmpty()) {
		_previewText = tr::ayu_LocalMessageEmptyText(tr::now);
	}
}

void LocalMessageRow::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	p.fillRect(e->rect(), st::windowBg);

	const auto left = st::defaultPeerListItem.namePosition.x();
	const auto top = st::defaultPeerListItem.namePosition.y();
	const auto available = width() - left - st::boxLayerScroll.width;

	// Draw sender name
	p.setPen(st::contactsNameFg);
	p.setFont(st::defaultPeerListItem.nameStyle.font);

	QString fromName;
	if (const auto from = _item->from()) {
		if (const auto user = _controller->session().data().user(from)) {
			fromName = user->name();
		} else {
			fromName = tr::ayu_LocalMessageUnknownSender(tr::now);
		}
	} else {
		fromName = _item->history()->peer->name();
	}

	const auto nameWidth = available / 2;
	p.drawText(
		QRect(left, top, nameWidth, st::defaultPeerListItem.nameStyle.font->height),
		fromName,
		QTextOption(Qt::AlignLeft | Qt::AlignVCenter));

	// Draw date
	p.setPen(st::contactsStatusFg);
	p.setFont(st::defaultPeerListItem.statusFont);
	const auto dateWidth = available / 4;
	p.drawText(
		QRect(left + nameWidth, top, dateWidth, st::defaultPeerListItem.statusFont->height),
		_dateText,
		QTextOption(Qt::AlignRight | Qt::AlignVCenter));

	// Draw preview text
	const auto previewTop = top + st::defaultPeerListItem.nameStyle.font->height + st::lineWidth;
	p.drawText(
		QRect(left, previewTop, available, st::defaultPeerListItem.statusFont->height),
		_previewText,
		QTextOption(Qt::AlignLeft | Qt::AlignVCenter));

	// Draw flags indicators
	const auto flagsTop = previewTop + st::defaultPeerListItem.statusFont->height + st::lineWidth;
	QString flagsText;
	if (_item->out()) flagsText += "📤 ";
	if (_item->isSilent()) flagsText += "🔇 ";
	if (_item->isPinned()) flagsText += "📌 ";
	if (_item->hasViews()) flagsText += QString("👁 %1 ").arg(_item->viewsCount());

	if (!flagsText.isEmpty()) {
		p.setPen(st::contactsStatusFgOnline);
		p.setFont(st::normalFont);
		p.drawText(
			QRect(left, flagsTop, available, st::normalFont->height),
			flagsText.trimmed(),
			QTextOption(Qt::AlignLeft | Qt::AlignVCenter));
	}
}

void LocalMessageRow::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		_editRequests.fire({});
	}
	RpWidget::mousePressEvent(e);
}

void LocalMessageRow::contextMenuEvent(QContextMenuEvent *e) {
	showContextMenu(e->globalPos());
}

void LocalMessageRow::showContextMenu(QPoint position) {
	auto menu = base::make_unique_q<Ui::PopupMenu>(
		this,
		st::popupMenuWithIcons);

	menu->addAction(
		tr::ayu_LocalMessageEdit(tr::now),
		[=] { _editRequests.fire({}); },
		&st::menuIconEdit);

	menu->addAction(
		tr::ayu_LocalMessageDelete(tr::now),
		[=] { _deleteRequests.fire({}); },
		&st::menuIconDelete);

	menu->popup(position);
}

} // namespace

LocalMessagesManager::LocalMessagesManager(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	not_null<History*> history)
: _box(box)
, _controller(controller)
, _history(history)
, _scroll(_box->addRow(object_ptr<Ui::ScrollArea>(_box)))
, _content(_scroll->setOwnedWidget(object_ptr<Ui::VerticalLayout>(_scroll))) {
	setupContent();
}

void LocalMessagesManager::setupContent() {
	_box->setTitle(tr::ayu_LocalMessagesManagerTitle());
	_box->setWidth(st::boxWideWidth);

	setupHeader();
	setupMessagesList();
	setupButtons();

	_scroll->setMaxHeight(st::boxMaxListHeight);
	refreshMessagesList();
}

void LocalMessagesManager::setupHeader() {
	// Search field
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessagesSearch(),
		st::boxLabel));

	_searchField = _content->add(object_ptr<Ui::InputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessagesSearchPlaceholder(),
		QString()));

	_searchField->changes() | rpl::start_with_next([=] {
		refreshMessagesList();
	}, _searchField->lifetime());

	// Stats
	const auto statsLabel = _content->add(object_ptr<Ui::FlatLabel>(
		_content,
		QString(),
		st::boxDividerLabel));

	const auto updateStats = [=] {
		const auto total = _localMessages.size();
		statsLabel->setText(tr::ayu_LocalMessagesStats(tr::now, lt_count, total));
	};

	// Add separator
	_content->add(object_ptr<Ui::BoxContentDivider>(_content));

	// Create new message button
	const auto createButton = _content->add(object_ptr<Ui::RoundButton>(
		_content,
		tr::ayu_LocalMessageCreateNew(),
		st::defaultActiveButton));

	createButton->setClickedCallback([=] {
		createNewMessage();
	});

	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));

	// Update stats initially
	rpl::combine(
		rpl::single(rpl::empty_value()),
		_searchField->changes()
	) | rpl::start_with_next([=] {
		updateStats();
	}, _content->lifetime());
}

void LocalMessagesManager::setupMessagesList() {
	// Messages will be added dynamically in refreshMessagesList()
}

void LocalMessagesManager::setupButtons() {
	_box->addButton(tr::lng_close(), [=] {
		_box->closeBox();
	});
}

void LocalMessagesManager::refreshMessagesList() {
	// Clear existing rows
	while (_content->count() > 5) { // Keep header elements
		delete _content->widgetAt(_content->count() - 1);
	}

	_localMessages.clear();

	// Get local messages for this peer
	if (!AyuMessages::hasLocalMessages(_history->peer, 0)) {
		_content->add(object_ptr<Ui::FlatLabel>(
			_content,
			tr::ayu_LocalMessagesEmpty(),
			st::boxDividerLabel));
		return;
	}

	const auto messages = AyuMessages::getLocalMessages(_history->peer, 0, 0, 0, 1000);
	const auto searchText = _searchField->getLastText().toLower();

	// Convert to HistoryItem and filter
	for (const auto &messageData : messages) {
		// Find the actual HistoryItem by ID
		const auto item = _history->owner().message(_history->peer->id, messageData.messageId);
		if (!item || !item->isLocal()) {
			continue;
		}

		// Apply search filter
		if (!searchText.isEmpty()) {
			const auto text = item->originalText().text.toLower();
			const auto fromName = item->from() 
				? _controller->session().data().user(item->from())->name().toLower()
				: _history->peer->name().toLower();
			
			if (!text.contains(searchText) && !fromName.contains(searchText)) {
				continue;
			}
		}

		_localMessages.push_back(item);
	}

	// Sort by date (newest first)
	std::sort(_localMessages.begin(), _localMessages.end(), [](
		not_null<HistoryItem*> a,
		not_null<HistoryItem*> b) {
		return a->date() > b->date();
	});

	// Add message rows
	for (const auto &item : _localMessages) {
		addMessageRow(item);
	}

	if (_localMessages.empty()) {
		_content->add(object_ptr<Ui::FlatLabel>(
			_content,
			searchText.isEmpty() 
				? tr::ayu_LocalMessagesEmpty()
				: tr::ayu_LocalMessagesNoResults(),
			st::boxDividerLabel));
	}
}

void LocalMessagesManager::addMessageRow(not_null<HistoryItem*> item) {
	const auto row = _content->add(object_ptr<LocalMessageRow>(
		_content,
		item,
		_controller));

	row->editRequests() | rpl::start_with_next([=] {
		editMessage(item);
	}, row->lifetime());

	row->deleteRequests() | rpl::start_with_next([=] {
		deleteMessage(item);
	}, row->lifetime());
}

void LocalMessagesManager::editMessage(not_null<HistoryItem*> item) {
	// Convert HistoryItem to LocalMessageData
	LocalMessageData data;
	data.text = item->originalText().text;
	data.fromId = item->from();
	data.postAuthor = item->postAuthor();
	data.date = item->date();
	data.silent = item->isSilent();
	data.pinned = item->isPinned();
	data.hasViews = item->hasViews();
	data.views = item->viewsCount();
	data.noForwards = item->forbidsForward();
	// ... populate other fields as needed

	_controller->show(Box(
		LocalMessageEditorBox,
		_controller,
		_history,
		data));
}

void LocalMessagesManager::deleteMessage(not_null<HistoryItem*> item) {
	const auto text = tr::ayu_LocalMessageDeleteConfirm(
		tr::now,
		lt_message,
		item->originalText().text.left(50));

	_controller->show(Ui::MakeConfirmBox({
		.text = text,
		.confirmed = [=] {
			// Remove from history
			_history->destroyMessage(item);
			
			// Refresh the list
			refreshMessagesList();
		},
		.confirmText = tr::lng_box_delete(),
	}));
}

void LocalMessagesManager::createNewMessage() {
	_controller->show(Box(
		LocalMessageEditorBox,
		_controller,
		_history,
		LocalMessageData{}));
}

void LocalMessagesManagerBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<History*> history) {
	const auto manager = box->lifetime().make_state<LocalMessagesManager>(
		box,
		controller,
		history);
}

} // namespace AyuUi