// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "data/data_document.h"
#include "ui/widgets/popup_menu.h"

namespace AyuUi {

bool needToShowItem(int state);

void AddHistoryAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item);
void AddHideMessageAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item);
void AddUserMessagesAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item);
void AddMessageDetailsAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item);
void AddReadUntilAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item);

}
