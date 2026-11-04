/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/core_types.h"

#include <memory>
#include <vector>

#ifdef _DEBUG
#include "scheme.h"
#endif // _DEBUG

namespace MTP {
class Instance;
namespace details {
class SerializedRequest;
} // namespace details
} // namespace MTP

namespace Test {

void ObserveDeletedHistoryRpc(
	MTP::Instance *instance,
	mtpRequestId requestId,
	const MTP::details::SerializedRequest &request);

#ifdef _DEBUG

struct DeletedHistoryRpcDialog {
	int64 userId = 0;
	QVector<MTPMessage> messages;
};

struct DeletedHistoryRpcStamp {
	mtpRequestId requestId = 0;
	int64 userId = 0;
	int offsetId = 0;
	int addOffset = 0;
	int limit = 0;
	int returned = 0;
	bool answered = false;
};

// Registers only the supplied synthetic dialogs on one isolated instance.
// The process must be launched with network denied from startup.
class DeletedHistoryRpcFixture final {
public:
	DeletedHistoryRpcFixture(
		not_null<MTP::Instance*> instance,
		std::vector<DeletedHistoryRpcDialog> dialogs);
	~DeletedHistoryRpcFixture();
	[[nodiscard]] std::vector<DeletedHistoryRpcStamp> stamps() const;
	[[nodiscard]] int pending() const;

	struct State;

private:
	std::shared_ptr<State> _state;

};

#endif // _DEBUG

} // namespace Test
