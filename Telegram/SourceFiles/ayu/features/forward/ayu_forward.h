
#pragma once


#include <main/main_session.h>

#include "history/history.h"


namespace AyuForward {
bool isForwarding(const PeerId &id);
QString stateName(const PeerId &id);

class ForwardState {
public:
	enum class State {
		Preparing,
		Downloading,
		Sending,
		Finished
	};

	void updateBottomBar(not_null<Main::Session *> session, const PeerData *peer, const State &st);

    int totalChunks;
    int currentChunk;
    int totalMessages;
    int sentMessages;

    State state = State::Preparing;
    bool stopRequested = false;

};


bool isAyuForwardNeeded(const std::vector<not_null<HistoryItem *>> &items);
bool isAyuForwardNeeded(not_null<HistoryItem *> item);
bool isFullAyuForwardNeeded(not_null<PeerData *> peer, not_null<History*> history);
void intelligentForward(const PeerData* peer,
						const std::vector<not_null<HistoryItem *>> &item, not_null<Main::Session *> session, not_null<History *> history, const Api::SendAction &action);
void forwardMessages(const PeerData* peer, std::vector<not_null<HistoryItem*>> item, not_null<Main::Session *> session, not_null<History *> history, const Api::SendAction &action, bool forwardState);


}