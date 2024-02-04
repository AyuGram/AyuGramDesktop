// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "history/history.h"
#include "history/history_item.h"

namespace AyuState {

void hide(PeerId peerId, MsgId messageId);
void hide(not_null<HistoryItem*> item);
bool isHidden(PeerId peerId, MsgId messageId);
bool isHidden(not_null<HistoryItem*> item);

}
