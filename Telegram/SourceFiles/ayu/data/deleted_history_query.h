#pragma once

#include "ayu/libs/sqlite/sqlite_orm.h"

#include <cstdint>
#include <vector>

namespace AyuDatabase {

template <typename Message, typename Storage>
[[nodiscard]] std::vector<Message> GetDeletedHistoryMessages(
		Storage &storage,
		std::int64_t userId,
		std::int64_t dialogId,
		std::int64_t minId,
		std::int64_t maxId,
		int totalLimit,
		bool ascending) {
	using namespace sqlite_orm;

	if (totalLimit <= 0) {
		return {};
	}

	const auto messageId = column<Message>(&Message::messageId);
	const auto captured = column<Message>(&Message::entityCreateDate);
	const auto fakeId = column<Message>(&Message::fakeId);
	const auto scope = (column<Message>(&Message::userId) == userId)
		&& (column<Message>(&Message::dialogId) == dialogId)
		&& (messageId > 0)
		&& ((messageId > minId) || (minId == 0))
		&& ((messageId < maxId) || (maxId == 0));
	const auto order = ascending
		? order_by(messageId).asc()
		: order_by(messageId).desc();
	const auto latestDates = select(
		columns(messageId, max(captured)),
		where(scope),
		group_by(messageId),
		order,
		limit(totalLimit));
	const auto winners = select(
		max(fakeId),
		where(scope && in(columns(messageId, captured), latestDates)),
		group_by(messageId));
	return storage.template get_all<Message>(
		where(in(fakeId, winners)),
		order,
		limit(totalLimit));
}

} // namespace AyuDatabase
