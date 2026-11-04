#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace AyuMessages {

struct HistoryPageRequest {
	int offsetId = 0;
	int addOffset = 0;
	int limit = 0;
};

struct HistoryPageEntry {
	int id = 0;
	int index = 0;
	bool deleted = false;
	bool empty = false;
};

[[nodiscard]] inline std::vector<HistoryPageEntry> MergeHistoryPage(
		std::span<const HistoryPageEntry> server,
		std::span<const HistoryPageEntry> deleted,
		HistoryPageRequest request) {
	if (request.limit <= 0) {
		return {};
	}
	auto merged = std::vector<HistoryPageEntry>();
	merged.reserve(server.size() + deleted.size());
	const auto append = [&](
			std::span<const HistoryPageEntry> values,
			bool fromDeleted) {
		for (auto entry : values) {
			if (entry.id > 0) {
				entry.deleted = fromDeleted;
				merged.push_back(entry);
			}
		}
	};
	append(server, false);
	append(deleted, true);
	std::stable_sort(
		merged.begin(),
		merged.end(),
		[](const auto &a, const auto &b) {
			if (a.id != b.id) {
				return a.id > b.id;
			}
			const auto aRank = a.deleted ? 1 : a.empty ? 0 : 2;
			const auto bRank = b.deleted ? 1 : b.empty ? 0 : 2;
			return aRank > bRank;
		});
	const auto end = std::unique(
		merged.begin(),
		merged.end(),
		[](const auto &a, const auto &b) { return a.id == b.id; });
	merged.erase(end, merged.end());
	const auto anchor = request.offsetId
		? std::find_if(
			merged.begin(),
			merged.end(),
			[&](const auto &entry) { return entry.id < request.offsetId; })
		: merged.begin();
	const auto size = static_cast<std::int64_t>(merged.size());
	const auto start = std::clamp<std::int64_t>(
		static_cast<std::int64_t>(anchor - merged.begin()) + request.addOffset,
		0,
		size);
	const auto finish = std::min<std::int64_t>(size, start + request.limit);
	return { merged.begin() + start, merged.begin() + finish };
}

} // namespace AyuMessages
