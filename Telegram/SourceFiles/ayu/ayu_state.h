// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "history/history.h"
#include "history/history_item.h"

namespace Main {
class Session;
} // namespace Main

namespace AyuState {

void hide(PeerId peerId, MsgId messageId);
void hide(not_null<HistoryItem*> item);
bool isHidden(PeerId peerId, MsgId messageId);
bool isHidden(not_null<HistoryItem*> item);

void setDisableGhostModeOnStoryClose(Main::Session *session);
void disableGhostModeOnStoryClose(Main::Session *session);

}
