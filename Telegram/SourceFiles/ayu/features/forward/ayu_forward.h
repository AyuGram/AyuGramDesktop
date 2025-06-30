#pragma once

#include <main/main_session.h>

#include "history/history.h"

namespace AyuForward {
bool isForwarding(const PeerId &id);
void cancelForward(const PeerId &id, const Main::Session &session);
std::pair<QString, QString> stateName(const PeerId &id);

class ForwardState
{
public:
	enum class State
	{
		Preparing,
		Downloading,
		Sending,
		Finished
	};
	void updateBottomBar(const Main::Session &session, const PeerId *peer, const State &st);

	int totalChunks;
	int currentChunk;
	int totalMessages;
	int sentMessages;

	State state = State::Preparing;
	bool stopRequested = false;

};

bool isAyuForwardNeeded(const std::vector<not_null<HistoryItem*>> &items);
bool isAyuForwardNeeded(not_null<HistoryItem*> item);
bool isFullAyuForwardNeeded(not_null<HistoryItem*> item);
void intelligentForward(
	not_null<Main::Session*> session,
	const Api::SendAction &action,
	Data::ResolvedForwardDraft draft);
void forwardMessages(
	not_null<Main::Session*> session,
	const Api::SendAction &action,
	bool forwardState,
	Data::ResolvedForwardDraft draft);

}
