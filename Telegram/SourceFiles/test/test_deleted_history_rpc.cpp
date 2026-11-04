/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "test/test_deleted_history_rpc.h"

#ifdef _DEBUG

#include "mtproto/details/mtproto_serialized_request.h"
#include "mtproto/mtp_instance.h"
#include "test/test_agent.h"
#include "test/test_log.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>

namespace Test {

struct DeletedHistoryRpcFixture::State {
	QPointer<MTP::Instance> instance;
	std::vector<DeletedHistoryRpcDialog> dialogs;
	std::vector<DeletedHistoryRpcStamp> stamps;
	bool active = true;
};

namespace {

std::weak_ptr<DeletedHistoryRpcFixture::State> Registered;

[[nodiscard]] int MessageId(const MTPMessage &message) {
	return message.match([](const auto &data) { return data.vid().v; });
}

} // namespace

DeletedHistoryRpcFixture::DeletedHistoryRpcFixture(
		not_null<MTP::Instance*> instance,
		std::vector<DeletedHistoryRpcDialog> dialogs)
: _state(std::make_shared<State>()) {
	Expects(Active());
	Expects(Registered.expired());
	_state->instance = instance;
	_state->dialogs = std::move(dialogs);
	for (auto &dialog : _state->dialogs) {
		Expects(dialog.userId > 0);
		ranges::sort(dialog.messages, std::greater<>(), &MessageId);
	}
	Registered = _state;
}

DeletedHistoryRpcFixture::~DeletedHistoryRpcFixture() {
	_state->active = false;
	Registered.reset();
}

std::vector<DeletedHistoryRpcStamp> DeletedHistoryRpcFixture::stamps() const {
	return _state->stamps;
}

int DeletedHistoryRpcFixture::pending() const {
	return int(ranges::count_if(_state->stamps, [](const auto &stamp) {
		return !stamp.answered;
	}));
}

void ObserveDeletedHistoryRpc(
		MTP::Instance *instance,
		mtpRequestId requestId,
		const MTP::details::SerializedRequest &request) {
	const auto state = Active() ? Registered.lock() : nullptr;
	if (!state || !state->active || state->instance != instance || !request) {
		return;
	}
	const auto &buffer = *request;
	if (buffer.size() <= MTP::details::SerializedRequest::kMessageBodyPosition) {
		return;
	}
	const auto begin = buffer.constData();
	const auto bufferEnd = begin + buffer.size();
	auto from = begin + MTP::details::SerializedRequest::kMessageBodyPosition;
	if (from >= bufferEnd || mtpTypeId(*from++) != mtpc_messages_getHistory) {
		return;
	}
	auto peer = MTPInputPeer();
	if (!peer.read(from, bufferEnd) || bufferEnd - from < 8) {
		return;
	}
	auto userId = int64(0);
	peer.match([&](const MTPDinputPeerUser &data) {
		userId = data.vuser_id().v;
	}, [](const auto &) {});
	const auto dialog = ranges::find(state->dialogs, userId,
		&DeletedHistoryRpcDialog::userId);
	if (dialog == end(state->dialogs)) {
		return;
	}
	const auto offsetId = int32(from[0]);
	const auto addOffset = int32(from[2]);
	const auto limit = int32(from[3]);
	if (limit <= 0 || limit > 1000 || from[1] || from[4] || from[5]) {
		Fail(u"synthetic getHistory request has supported bounds"_q,
			u"id=%1 offset=%2 add=%3 limit=%4"_q
				.arg(requestId).arg(offsetId).arg(addOffset).arg(limit));
		return;
	}
	auto start = offsetId
		? int(ranges::count_if(dialog->messages, [&](const auto &message) {
			return MessageId(message) >= offsetId;
		}))
		: 0;
	start = std::clamp(start + addOffset, 0, int(dialog->messages.size()));
	auto messages = QVector<MTPMessage>();
	for (auto i = start
		; i < dialog->messages.size() && messages.size() < limit
		; ++i) {
		messages.push_back(dialog->messages[i]);
	}
	const auto result = MTPmessages_Messages(MTP_messages_messagesSlice(
		MTP_flags(0),
		MTP_int(int(dialog->messages.size())),
		MTPint(),
		MTPint(),
		MTPSearchPostsFlood(),
		MTP_vector<MTPMessage>(messages),
		MTP_vector<MTPForumTopic>(),
		MTP_vector<MTPChat>(),
		MTP_vector<MTPUser>()));
	const auto stampIndex = state->stamps.size();
	state->stamps.push_back({
		.requestId = requestId,
		.userId = userId,
		.offsetId = offsetId,
		.addOffset = addOffset,
		.limit = limit,
		.returned = int(messages.size()),
	});
	Note(u"synthetic getHistory issue id=%1 peer=%2 offset=%3 add=%4 "
		"limit=%5 returned=%6"_q
		.arg(requestId).arg(userId).arg(offsetId).arg(addOffset)
		.arg(limit).arg(messages.size()));
	// Defer until .send() has returned and the caller stored its request id.
	QTimer::singleShot(0, instance, [state, requestId, result, stampIndex] {
		if (!state->active || !state->instance) {
			return;
		}
		const auto instance = state->instance.data();
		if (!instance->hasCallback(requestId)) {
			Fail(u"synthetic getHistory retains its callback"_q,
				u"id=%1"_q.arg(requestId));
			state->stamps[stampIndex].answered = true;
			return;
		}
		auto response = MTP::Response{ .requestId = requestId };
		result.write(response.reply);
		instance->processCallback(response);
		state->stamps[stampIndex].answered = true;
		Note(u"synthetic getHistory answer id=%1 callbackRemaining=%2"_q
			.arg(requestId).arg(instance->hasCallback(requestId)));
	});
}

} // namespace Test

#else // _DEBUG

namespace Test {

void ObserveDeletedHistoryRpc(
		MTP::Instance *,
		mtpRequestId,
		const MTP::details::SerializedRequest &) {
}

} // namespace Test

#endif // _DEBUG
