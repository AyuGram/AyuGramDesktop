// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "history/history.h"
#include "main/main_session.h"

#include <atomic>

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

	explicit ForwardState(int totalChunks)
	: totalChunks(totalChunks) {
	}

	ForwardState(const ForwardState &other)
	: totalChunks(other.totalChunks)
	, currentChunk(other.currentChunk)
	, totalMessages(other.totalMessages)
	, sentMessages(other.sentMessages)
	, state(other.state)
	, stopRequested(other.stopRequested.load()) {
	}

	void updateBottomBar(const Main::Session &session, const PeerId *peer, const State &st);

	int totalChunks = 0;
	int currentChunk = 0;
	int totalMessages = 0;
	int sentMessages = 0;

	State state = State::Preparing;
	std::atomic<bool> stopRequested = false;

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
