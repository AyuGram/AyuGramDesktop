/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_polls.h"

#include "api/api_common.h"
#include "api/api_text_entities.h"
#include "api/api_updates.h"
#include "apiwrap.h"
#include "base/random.h"
#include "data/business/data_shortcut_messages.h"
#include "data/data_changes.h"
#include "data/data_histories.h"
#include "data/data_poll.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h" // ShouldSendSilent
#include "main/main_session.h"

// AyuGram includes
#include "ayu/ayu_settings.h"
#include "ayu/utils/telegram_helpers.h"


namespace Api {

Polls::Polls(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
}

void Polls::create(
		const PollData &data,
		const TextWithEntities &text,
		SendAction action,
		Fn<void()> done,
		Fn<void(bool fileReferenceExpired)> fail) {
	_session->api().sendAction(action);

	const auto history = action.history;
	const auto peer = history->peer;
	const auto topicRootId = action.replyTo.messageId
		? action.replyTo.topicRootId
		: 0;
	const auto monoforumPeerId = action.replyTo.monoforumPeerId;
	auto sendFlags = MTPmessages_SendMedia::Flags(0);
	if (action.replyTo) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_reply_to;
	}
	const auto clearCloudDraft = action.clearDraft;
	if (clearCloudDraft) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_clear_draft;
		history->clearLocalDraft(topicRootId, monoforumPeerId);
		history->clearCloudDraft(topicRootId, monoforumPeerId);
		history->startSavingCloudDraft(topicRootId, monoforumPeerId);
	}
	const auto silentPost = ShouldSendSilent(peer, action.options);
	const auto starsPaid = std::min(
		peer->starsPerMessageChecked(),
		action.options.starsApproved);
	if (silentPost) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_silent;
	}
	if (action.options.scheduled) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_date;
		if (action.options.scheduleRepeatPeriod) {
			sendFlags |= MTPmessages_SendMedia::Flag::f_schedule_repeat_period;
		}
	}
	if (action.options.shortcutId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_quick_reply_shortcut;
	}
	if (action.options.effectId) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_effect;
	}
	if (action.options.suggest) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_suggested_post;
	}
	if (starsPaid) {
		action.options.starsApproved -= starsPaid;
		sendFlags |= MTPmessages_SendMedia::Flag::f_allow_paid_stars;
	}
	const auto sendAs = action.options.sendAs;
	if (sendAs) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_send_as;
	}
	auto sentEntities = Api::EntitiesToMTP(
		_session,
		text.entities,
		Api::ConvertOption::SkipLocal);
	if (!sentEntities.v.isEmpty()) {
		sendFlags |= MTPmessages_SendMedia::Flag::f_entities;
	}
	auto &histories = history->owner().histories();
	const auto randomId = base::RandomValue<uint64>();
	histories.sendPreparedMessage(
		history,
		action.replyTo,
		randomId,
		Data::Histories::PrepareMessage<MTPmessages_SendMedia>(
			MTP_flags(sendFlags),
			peer->input(),
			Data::Histories::ReplyToPlaceholder(),
			PollDataToInputMedia(&data),
			MTP_string(text.text),
			MTP_long(randomId),
			MTPReplyMarkup(),
			sentEntities,
			MTP_int(action.options.scheduled),
			MTP_int(action.options.scheduleRepeatPeriod),
			(sendAs ? sendAs->input() : MTP_inputPeerEmpty()),
			Data::ShortcutIdToMTP(_session, action.options.shortcutId),
			MTP_long(action.options.effectId),
			MTP_long(starsPaid),
			SuggestToMTP(action.options.suggest)
		), [=](const MTPUpdates &result, const MTP::Response &response) {
		if (clearCloudDraft) {
			history->finishSavingCloudDraft(
				topicRootId,
				monoforumPeerId,
				UnixtimeFromMsgId(response.outerMsgId));
		}
		_session->changes().historyUpdated(
			history,
			(action.options.scheduled
				? Data::HistoryUpdate::Flag::ScheduledSent
				: Data::HistoryUpdate::Flag::MessageSent));
		done();
	}, [=](const MTP::Error &error, const MTP::Response &response) {
		if (clearCloudDraft) {
			history->finishSavingCloudDraft(
				topicRootId,
				monoforumPeerId,
				UnixtimeFromMsgId(response.outerMsgId));
		}
		const auto expired = (error.code() == 400)
			&& error.type().startsWith(u"FILE_REFERENCE_"_q);
		fail(expired);
	});
}

void Polls::sendVotes(
		FullMsgId itemId,
		const std::vector<QByteArray> &options) {
	if (_pollVotesRequestIds.contains(itemId)) {
		return;
	}
	const auto item = _session->data().message(itemId);
	const auto media = item ? item->media() : nullptr;
	const auto poll = media ? media->poll() : nullptr;
	if (!item) {
		return;
	}

	const auto showSending = poll && !options.empty();
	const auto hideSending = [=] {
		if (showSending) {
			if (const auto item = _session->data().message(itemId)) {
				poll->sendingVotes.clear();
				_session->data().requestItemRepaint(item);
			}
		}
	};
	if (showSending) {
		poll->sendingVotes = options;
		_session->data().requestItemRepaint(item);
	}

	auto prepared = QVector<MTPbytes>();
	prepared.reserve(options.size());
	ranges::transform(
		options,
		ranges::back_inserter(prepared),
		[](const QByteArray &option) { return MTP_bytes(option); });
	const auto requestId = _api.request(MTPmessages_SendVote(
		item->history()->peer->input(),
		MTP_int(item->id),
		MTP_vector<MTPbytes>(prepared)
	)).done([=](const MTPUpdates &result) {
		_pollVotesRequestIds.erase(itemId);
		hideSending();
		_session->updates().applyUpdates(result);

		const auto &ghost = AyuSettings::ghost(_session);
		if (!ghost.sendReadMessages() && ghost.markReadAfterAction() && item)
		{
			readHistory(item);
		}
	}).fail([=] {
		_pollVotesRequestIds.erase(itemId);
		hideSending();
	}).send();
	_pollVotesRequestIds.emplace(itemId, requestId);
}

void Polls::addAnswer(
		FullMsgId itemId,
		const TextWithEntities &text,
		const PollMedia &media,
		Fn<void()> done,
		Fn<void(QString)> fail) {
	if (_pollAddAnswerRequestIds.contains(itemId)) {
		return;
	}
	const auto item = _session->data().message(itemId);
	if (!item) {
		return;
	}
	const auto sentEntities = Api::EntitiesToMTP(
		_session,
		text.entities,
		Api::ConvertOption::SkipLocal);
	using Flag = MTPDinputPollAnswer::Flag;
	const auto flags = media
		? Flag::f_media
		: Flag();
	const auto answer = MTP_inputPollAnswer(
		MTP_flags(flags),
		MTP_textWithEntities(
			MTP_string(text.text),
			sentEntities),
		media ? PollMediaToMTP(media) : MTPInputMedia());
	const auto requestId = _api.request(MTPmessages_AddPollAnswer(
		item->history()->peer->input(),
		MTP_int(item->id),
		answer
	)).done([=](const MTPUpdates &result) {
		_pollAddAnswerRequestIds.erase(itemId);
		_session->updates().applyUpdates(result);
		if (done) {
			done();
		}
	}).fail([=](const MTP::Error &error) {
		_pollAddAnswerRequestIds.erase(itemId);
		if (fail) {
			fail(error.type());
		}
	}).send();
	_pollAddAnswerRequestIds.emplace(itemId, requestId);
}

void Polls::deleteAnswer(FullMsgId itemId, const QByteArray &option) {
	if (_pollVotesRequestIds.contains(itemId)) {
		return;
	}
	const auto item = _session->data().message(itemId);
	if (!item) {
		return;
	}
	const auto requestId = _api.request(MTPmessages_DeletePollAnswer(
		item->history()->peer->input(),
		MTP_int(item->id),
		MTP_bytes(option)
	)).done([=](const MTPUpdates &result) {
		_pollVotesRequestIds.erase(itemId);
		_session->updates().applyUpdates(result);
	}).fail([=] {
		_pollVotesRequestIds.erase(itemId);
	}).send();
	_pollVotesRequestIds.emplace(itemId, requestId);
}

void Polls::close(not_null<HistoryItem*> item) {
	const auto itemId = item->fullId();
	if (_pollCloseRequestIds.contains(itemId)) {
		return;
	}
	const auto media = item ? item->media() : nullptr;
	const auto poll = media ? media->poll() : nullptr;
	if (!poll) {
		return;
	}
	const auto requestId = _api.request(MTPmessages_EditMessage(
		MTP_flags(MTPmessages_EditMessage::Flag::f_media),
		item->history()->peer->input(),
		MTP_int(item->id),
		MTPstring(),
		PollDataToInputMedia(poll, true),
		MTPReplyMarkup(),
		MTPVector<MTPMessageEntity>(),
		MTP_int(0), // schedule_date
		MTP_int(0), // schedule_repeat_period
		MTPint() // quick_reply_shortcut_id
	)).done([=](const MTPUpdates &result) {
		_pollCloseRequestIds.erase(itemId);
		_session->updates().applyUpdates(result);
	}).fail([=] {
		_pollCloseRequestIds.erase(itemId);
	}).send();
	_pollCloseRequestIds.emplace(itemId, requestId);
}

void Polls::reloadResults(not_null<HistoryItem*> item) {
	const auto itemId = item->fullId();
	if (!item->isRegular() || _pollReloadRequestIds.contains(itemId)) {
		return;
	}
	const auto media = item->media();
	const auto poll = media ? media->poll() : nullptr;
	const auto pollHash = poll ? poll->hash : uint64(0);
	const auto requestId = _api.request(MTPmessages_GetPollResults(
		item->history()->peer->input(),
		MTP_int(item->id),
		MTP_long(pollHash)
	)).done([=](const MTPUpdates &result) {
		_pollReloadRequestIds.erase(itemId);
		_session->updates().applyUpdates(result);
	}).fail([=] {
		_pollReloadRequestIds.erase(itemId);
	}).send();
	_pollReloadRequestIds.emplace(itemId, requestId);
}

} // namespace Api
