// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "history/history.h"
#include "main/main_session.h"

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
	const Data::ResolvedForwardDraft &draft);
void forwardMessages(
	not_null<Main::Session*> session,
	const Api::SendAction &action,
	bool forwardState,
	const Data::ResolvedForwardDraft &draft);

}
