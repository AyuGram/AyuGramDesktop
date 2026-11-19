// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/lupin_state.h"

#include "lupin/lupin_settings.h"

namespace LupinState {

std::unordered_map<PeerId, std::unordered_set<MsgId>> hiddenMessages;
Main::Session *disableGhostModeOnStoryCloseSession = nullptr;

void hide(PeerId peerId, MsgId messageId) {
	hiddenMessages[peerId].insert(messageId);
}

void hide(not_null<HistoryItem*> item) {
	hide(item->history()->peer->id, item->id);
}

bool isHidden(PeerId peerId, MsgId messageId) {
	const auto it = hiddenMessages.find(peerId);
	if (it != hiddenMessages.end()) {
		return it->second.contains(messageId);
	}
	return false;
}

bool isHidden(not_null<HistoryItem*> item) {
	return isHidden(item->history()->peer->id, item->id);
}

void setDisableGhostModeOnStoryClose(Main::Session *session) {
	disableGhostModeOnStoryCloseSession = session;
}

void disableGhostModeOnStoryClose(Main::Session *session) {
	if (disableGhostModeOnStoryCloseSession != session) {
		return;
	}
	disableGhostModeOnStoryCloseSession = nullptr;
	if (session) {
		LupinSettings::ghost(session).setGhostModeEnabled(false);
	}
}

}
