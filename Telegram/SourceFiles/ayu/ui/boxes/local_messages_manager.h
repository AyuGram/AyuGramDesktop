// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "ui/layers/generic_box.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/vertical_layout.h"

namespace Ui {
class RoundButton;
class InputField;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

class History;
class HistoryItem;
class PeerData;

namespace AyuUi {

struct LocalMessageData;

class LocalMessagesManager {
public:
	LocalMessagesManager(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<History*> history);

private:
	void setupContent();
	void setupHeader();
	void setupMessagesList();
	void setupButtons();
	
	void refreshMessagesList();
	void addMessageRow(not_null<HistoryItem*> item);
	void editMessage(not_null<HistoryItem*> item);
	void deleteMessage(not_null<HistoryItem*> item);
	void createNewMessage();
	
	const not_null<Ui::GenericBox*> _box;
	const not_null<Window::SessionController*> _controller;
	const not_null<History*> _history;
	
	not_null<Ui::ScrollArea*> _scroll;
	not_null<Ui::VerticalLayout*> _content;
	Ui::InputField* _searchField = nullptr;
	
	std::vector<not_null<HistoryItem*>> _localMessages;
};

void LocalMessagesManagerBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	not_null<History*> history);

} // namespace AyuUi