// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "ui/layers/generic_box.h"
#include "history/history_item.h"
#include "data/data_msg_id.h"

namespace Ui {
class InputField;
class MaskedInputField;
class Checkbox;
class Dropdown;
class DateTimeEdit;
class UserpicButton;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

class History;
class HistoryItem;
class PeerData;

namespace AyuUi {

struct LocalMessageData {
	QString text;
	PeerId fromId = 0;
	QString postAuthor;
	TimeId date = 0;
	bool silent = false;
	bool pinned = false;
	bool hasViews = false;
	int views = 0;
	bool hasShares = false;
	int shares = 0;
	bool noForwards = false;
	bool invertMedia = false;
	FullReplyTo replyTo;
	uint64 groupedId = 0;
	UserId viaBotId = 0;
	EffectId effectId = 0;
	bool isService = false;
	QString serviceText;
	
	// Media parameters
	bool hasMedia = false;
	QString mediaPath;
	QString mediaCaption;
	
	// Forward parameters
	bool isForwarded = false;
	PeerId forwardFromId = 0;
	QString forwardFromName;
	TimeId forwardDate = 0;
	QString forwardPostAuthor;
	bool forwardSavedFromPeer = false;
	
	// Edit parameters
	bool wasEdited = false;
	TimeId editDate = 0;
	
	// Business parameters
	BusinessShortcutId shortcutId = 0;
	int starsPaid = 0;
};

void LocalMessageEditorBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	not_null<History*> history,
	const LocalMessageData &initialData = {});

} // namespace AyuUi