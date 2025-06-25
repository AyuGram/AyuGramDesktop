
#include "ayu_forward.h"

#include <lang_auto.h>
#include <base/random.h>
#include <data/data_peer.h>
#include <history/history_item.h>
#include <styles/style_boxes.h>

#include "apiwrap.h"
#include "ayu_sync.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "storage/file_download.h"
#include "storage/localimageloader.h"
#include "storage/storage_account.h"
#include "storage/storage_media_prepare.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/text/text_utilities.h"

namespace AyuForward {
    std::unordered_map<PeerId, ForwardState> forwardStates;

    bool isForwarding(const PeerId &id) {
        const auto state = forwardStates.find(id);
        return id.value
               && state != forwardStates.end()
               && state->second.state != ForwardState::State::Finished
               && state->second.currentChunk < state->second.totalChunks
               && !state->second.stopRequested
               && state->second.totalChunks
               && state->second.totalMessages;
    }

    QString stateName(const PeerId &id) {
        const auto fwState = forwardStates.find(id);

        if (fwState == forwardStates.end()) {
            return QString();
        }
    	const auto state = fwState->second;

        QString messagesString = tr::ayu_AyuForwardStatusChunkCount(tr::now,
        	lt_count1,
        	QString::number(state.sentMessages),
        	lt_count2,
        	QString::number(state.totalMessages)
        	);
        QString chunkString = tr::ayu_AyuForwardStatusChunkCount(tr::now,
        	lt_count1,
        	QString::number(state.currentChunk + 1),
        	lt_count2,
        	QString::number(state.totalChunks)
        	);

    	const auto partString = state.totalChunks <= 1 ? messagesString : (messagesString + " • " + chunkString);

    	QString status;
    	if (state.state == ForwardState::State::Preparing) {
    		status = tr::ayu_AyuForwardStatusPreparing(tr::now);
    	} else if (state.state  == ForwardState::State::Downloading) {
    		status = tr::ayu_AyuForwardStatusLoadingMedia(tr::now);
    	} else if (state.state  == ForwardState::State::Sending) {
    		status = tr::ayu_AyuForwardStatusForwarding(tr::now);
    	} else { // ForwardState::State::Finished
    		status = tr::ayu_AyuForwardStatusFinished(tr::now);
    	}


    	return status.append("\n").append(partString);// +  "\n" + partString;
    }

	void ForwardState::updateBottomBar(not_null<Main::Session *> session, const PeerData *peer, const State &st) {
        state = st;
        forwardStates[peer->id] = *this;
        session->changes().peerUpdated(session->data().peer(peer->id), Data::PeerUpdate::Flag::Rights);
    }


	static Ui::PreparedList prepareMedia(not_null<Main::Session*> session, const std::vector<not_null<HistoryItem*>> &items, int &i) {
		const auto prepare = [&] (not_null<Data::Media*> media){
			auto prepared = Ui::PreparedFile(AyuSync::filePath(session, media));
			Storage::PrepareDetails(prepared, st::sendMediaPreviewSize, 1280);
			return prepared;
		};

    	Ui::PreparedList list;
    	const auto startItem = items[i];
    	const auto media = startItem->media();
    	const auto groupId = startItem->groupId();

    	list.files.emplace_back(prepare(media));

    	if (!groupId.value) {
    		return list;
    	}

    	for (int k = i + 1; k < items.size(); ++k) {
    		const auto nextItem = items[k];
    		if (nextItem->groupId() != groupId) {
    			break;
    		}

    		if (const auto nextMedia = nextItem->media()) {
    			list.files.emplace_back(prepare(nextMedia));
    			i = k;
    		}
    	}

    	return list;
    }

	void sendMedia(
		not_null<Main::Session*> session,
		Ui::PreparedList &&list,
		not_null<Data::Media*> primaryMedia,
		Api::MessageToSend &&message,
		uint64 newGroupId) {


    	if (const auto document = primaryMedia->document()) {
    		if (document->isGifv() || document->sticker()) {
    			AyuSync::sendGifOrStickSync(session, message, document);
    			return;
    		}
    	}

    	const auto mediaType = [&] {
    		if (const auto document = primaryMedia->document()) {
    			if (document->isVoiceMessage()) {
    				return SendMediaType::Audio;
    			} else if (document->isVideoMessage()) {
    				return SendMediaType::Round;
    			} else {
    				return SendMediaType::File;
    			}
			}
			return SendMediaType::Photo;
		}();


    	auto group = std::make_shared<SendingAlbum>();
    	group->groupId = newGroupId;

    	AyuSync::sendDocumentSync(
			session,
			std::move(list),
			mediaType,
			std::move(message.textWithTags),
			group,
			message.action);
    }


    bool isAyuForwardNeeded(const std::vector<not_null<HistoryItem *>> &items) {
        for (const auto &item : items) {
            if (isAyuForwardNeeded(item)) {
                return true;
            }
        }
        return false;
    }

    bool isAyuForwardNeeded(not_null<HistoryItem *> item) {
        if (item->isDeleted() || item->isAyuNoForwards() || item->ttlDestroyAt()) {
            return true;
        }
        return false;
    }

    bool isFullAyuForwardNeeded(not_null<PeerData *> peer, not_null<History*> history) {
        return peer->isAyuNoForwards() || history->peer->isAyuNoForwards();
    }

    struct ForwardChunk {
        bool isAyuForwardNeeded;
        std::vector<not_null<HistoryItem *>> items;
    };

    void intelligentForward(const PeerData* peer, const std::vector<not_null<HistoryItem *>> &items, not_null<Main::Session *> session, not_null<History *> history, const Api::SendAction &action) {
        history->setForwardDraft(action.replyTo.topicRootId, {});

        auto chunks = std::vector<ForwardChunk>();

        auto currentArray = std::vector<not_null<HistoryItem *>>();
        auto currentChunk = ForwardChunk({
                .isAyuForwardNeeded = isAyuForwardNeeded(items[0]),
                .items = currentArray
            });

        for (const auto &item : items) {
            if (isAyuForwardNeeded(item) != currentChunk.isAyuForwardNeeded) {
                currentChunk.items = currentArray;
                chunks.push_back(currentChunk);

                currentArray = std::vector<not_null<HistoryItem *>>();
                currentChunk = ForwardChunk({
                                         .isAyuForwardNeeded = isAyuForwardNeeded(item),
                                         .items = currentArray
                                     });
            }
            currentArray.push_back(item);
        }

        currentChunk.items = currentArray;
        chunks.push_back(currentChunk);

        auto state =  std::make_shared<ForwardState>(chunks.size());
        forwardStates[peer->id] = *state;

        for (const auto &chunk : chunks) {
            if (chunk.isAyuForwardNeeded) {
                forwardMessages(peer, chunk.items, session, history, action, true);
            } else {
                state->totalMessages = chunk.items.size();
                state->sentMessages = 0;
                state->updateBottomBar(session, peer, ForwardState::State::Sending);

                AyuSync::forwardMessagesSync(session, chunk.items, action);

                state->sentMessages = state->totalMessages;
                state->updateBottomBar(session, peer, ForwardState::State::Finished);
            }

            state->currentChunk++;
        }
        state->updateBottomBar(session, peer,  ForwardState::State::Finished);

    }

    void forwardMessages(const PeerData *peer, std::vector<not_null<HistoryItem *> > items,
                         not_null<Main::Session *> session,
                         not_null<History *> history, const Api::SendAction &action, bool forwardState) {
        history->setForwardDraft(action.replyTo.topicRootId, {});

        ForwardState state;
        if (forwardState) {
            state = forwardStates[peer->id];
        } else {
            state = ForwardState(1);
        }

        forwardStates[peer->id] = state;

        std::unordered_map<uint64, uint64> groupIds;


        std::vector<not_null<HistoryItem *> > toBeDownloaded;

        for (const auto item: items) {
            if (item->media()) {
                toBeDownloaded.push_back(item);
            }
            if (item->groupId()) {
                const auto currentId = groupIds.find(item->groupId().value);
                if (currentId == groupIds.end()) {
                    groupIds[item->groupId().value] = base::RandomValue<uint64>();
                }
            }
        }
        if (toBeDownloaded.size()) {
            state.updateBottomBar(session, peer, ForwardState::State::Downloading);
            AyuSync::loadDocuments(session, toBeDownloaded);
        }

        state.totalMessages = items.size();
        state.sentMessages = 0;
        state.updateBottomBar(session, peer,  ForwardState::State::Sending);

        for (int i = 0; i < items.size(); i++) {
            const auto item = items[i];


            if (state.stopRequested) {
                state.updateBottomBar(session, peer, ForwardState::State::Finished);
                return;
            }

            auto message = Api::MessageToSend(Api::SendAction(session->data().history(peer->id)));

            message.textWithTags.text = item->originalText().text;
            message.textWithTags.tags = TextUtilities::ConvertEntitiesToTextTags(item->originalText().entities);

            const auto groupId = item->groupId().value;
            const auto newGroupId = groupIds.contains(groupId) ? groupIds.find(groupId)->second : 0;


        	if (const auto media = item->media()) {
        		if (const auto poll = media->poll()) {
        			// need to implement extract of the text
        			continue;
        		}
        		sendMedia(session, prepareMedia(session, items, i), media, std::move(message), newGroupId);
        	} else {
        		AyuSync::sendMessageSync(session, message);
        	}
            state.sentMessages += 1;
        }
        state.updateBottomBar(session, peer, ForwardState::State::Finished);
    }

} // namespace AyuFeatures::AyuForward
