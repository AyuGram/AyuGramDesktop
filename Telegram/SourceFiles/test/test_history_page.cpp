#include "ayu/data/history_page.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

namespace {

using AyuMessages::HistoryPageEntry;
using AyuMessages::HistoryPageRequest;
using AyuMessages::MergeHistoryPage;

void Require(bool condition, std::string_view message) {
	if (!condition) {
		std::cerr << "History page test failed: " << message << '\n';
		std::exit(1);
	}
}

std::vector<int> Ids(std::span<const HistoryPageEntry> entries) {
	auto result = std::vector<int>();
	for (const auto &entry : entries) {
		result.push_back(entry.id);
	}
	return result;
}

std::vector<int> OraclePage(
		std::span<const int> all,
		HistoryPageRequest request) {
	auto anchor = std::int64_t(0);
	if (request.offsetId) {
		for (const auto id : all) {
			anchor += (id >= request.offsetId);
		}
	}
	const auto first = std::max<std::int64_t>(0, anchor + request.addOffset);
	auto result = std::vector<int>();
	for (auto i = first; i < std::int64_t(all.size()); ++i) {
		if (result.size() == std::size_t(request.limit)) {
			break;
		}
		result.push_back(all[i]);
	}
	return result;
}

std::vector<int> UnionIds(
		std::span<const int> server,
		std::span<const int> local) {
	auto result = std::vector<int>(server.begin(), server.end());
	result.insert(result.end(), local.begin(), local.end());
	std::sort(result.begin(), result.end(), std::greater<>());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

std::vector<HistoryPageEntry> Entries(std::span<const int> ids, bool deleted) {
	auto result = std::vector<HistoryPageEntry>();
	for (const auto id : ids) {
		result.push_back({ id, int(result.size()), deleted, false });
	}
	return result;
}

std::vector<HistoryPageEntry> LocalCandidates(
		std::span<const int> local,
		HistoryPageRequest request) {
	auto ids = std::vector<int>();
	if (!request.offsetId) {
		for (const auto id : local) {
			if (ids.size() == std::size_t(request.limit)) {
				break;
			}
			ids.push_back(id);
		}
	} else {
		for (const auto id : local) {
			if (id < request.offsetId && ids.size() < std::size_t(request.limit)) {
				ids.push_back(id);
			}
		}
		auto newer = 0;
		for (auto i = local.rbegin(); i != local.rend(); ++i) {
			if (*i >= request.offsetId && newer < request.limit) {
				ids.push_back(*i);
				++newer;
			}
		}
	}
	return Entries(ids, true);
}

void CheckPage(
		std::span<const HistoryPageEntry> server,
		std::span<const HistoryPageEntry> local,
		std::span<const HistoryPageEntry> result,
		HistoryPageRequest request,
		std::span<const int> expected) {
	Require(
		Ids(result) == std::vector<int>(expected.begin(), expected.end()),
		"page differs from full-union oracle");
	Require(
		result.size() <= std::size_t(request.limit),
		"page exceeds request limit");
	auto previous = std::int64_t(INT_MAX) + 1;
	for (const auto &entry : result) {
		Require(
			entry.id > 0 && entry.id < previous,
			"page IDs are not positive, unique and descending");
		previous = entry.id;
		const auto source = entry.deleted ? local : server;
		Require(
			entry.index >= 0 && std::size_t(entry.index) < source.size(),
			"source index escaped its input");
		Require(
			source[entry.index].id == entry.id,
			"source index points to another message");
	}
}

std::uint64_t ExhaustiveOracle() {
	auto cases = std::uint64_t(0);
	for (auto serverMask = 0; serverMask != 64; ++serverMask) {
		auto serverIds = std::vector<int>();
		for (auto id = 6; id > 0; --id) {
			if (serverMask & (1 << (id - 1))) {
				serverIds.push_back(id);
			}
		}
		for (auto localMask = 0; localMask != 64; ++localMask) {
			auto localIds = std::vector<int>();
			for (auto id = 6; id > 0; --id) {
				if (localMask & (1 << (id - 1))) {
					localIds.push_back(id);
				}
			}
			const auto all = UnionIds(serverIds, localIds);
			for (auto limit = 1; limit != 5; ++limit) {
				for (auto offset = 1; offset != 9; ++offset) {
					for (auto add = -limit; add <= 0; ++add) {
						const auto request = HistoryPageRequest{ offset, add, limit };
						const auto server = Entries(OraclePage(serverIds, request), false);
						const auto local = LocalCandidates(localIds, request);
						CheckPage(
							server,
							local,
							MergeHistoryPage(server, local, request),
							request,
							OraclePage(all, request));
						++cases;
					}
				}
				const auto request = HistoryPageRequest{ 0, 0, limit };
				const auto server = Entries(OraclePage(serverIds, request), false);
				const auto local = LocalCandidates(localIds, request);
				CheckPage(
					server,
					local,
					MergeHistoryPage(server, local, request),
					request,
					OraclePage(all, request));
				++cases;
			}
		}
	}
	return cases;
}

void DuplicateAndBoundaryCases() {
	const auto server = std::vector<HistoryPageEntry>{
		{ 4, 0, true, true },
		{ 4, 1, true, false },
		{ 3, 2, true, true },
		{ 2, 3, true, false },
		{ 1, 4, true, true },
		{ 0, 5, false, false },
		{ -1, 6, false, false },
	};
	auto local = std::vector<HistoryPageEntry>{
		{ 4, 0, false, false },
		{ 3, 1, false, false },
		{ 2, 2, false, false },
	};
	const auto result = MergeHistoryPage(server, local, { 0, 0, 10 });
	Require(
		Ids(result) == std::vector<int>({ 4, 3, 2, 1 }),
		"duplicate or invalid ID handling");
	Require(
		!result[0].deleted
		&& result[0].index == 1
		&& !result[0].empty,
		"real server duplicate did not win");
	Require(
		result[1].deleted && result[1].index == 1,
		"local content did not replace empty server placeholder");
	Require(
		!result[2].deleted && result[2].index == 3,
		"local snapshot replaced real server content");
	Require(
		!result[3].deleted && result[3].empty,
		"unmatched server placeholder changed");
	Require(
		server.front().deleted && !local.front().deleted,
		"input entries were mutated");
	Require(MergeHistoryPage(server, local, { 0, 0, 0 }).empty(), "zero limit");
	Require(
		MergeHistoryPage(server, local, { 0, 0, -1 }).empty(),
		"negative limit");
	local.clear();
	for (auto i = 0; i != 1000; ++i) {
		local.push_back({ 9, i, false, false });
	}
	local.push_back({ 8, 1000, false, false });
	local.push_back({ 7, 1001, false, false });
	const auto duplicatePage = MergeHistoryPage({}, local, { 0, 0, 3 });
	Require(
		Ids(duplicatePage) == std::vector<int>({ 9, 8, 7 }),
		"raw duplicates consumed page capacity");
	Require(
		duplicatePage.front().index == 0,
		"equal local duplicates were not stable");
	const auto extreme = std::vector<HistoryPageEntry>{
		{ INT_MAX, 0, false, false },
		{ INT_MAX - 1, 1, false, false },
		{ 1, 2, false, false },
	};
	Require(
		Ids(MergeHistoryPage({}, extreme, { INT_MAX, INT_MIN, INT_MAX }))
		== std::vector<int>({ INT_MAX, INT_MAX - 1, 1 }),
		"offset arithmetic overflow");
	Require(
		MergeHistoryPage({}, extreme, { INT_MAX, INT_MAX, 1 }).empty(),
		"large positive shift escaped bounds");
	const auto centered = MergeHistoryPage({}, extreme, { INT_MAX, 0, 1 });
	Require(
		Ids(centered) == std::vector<int>({ INT_MAX - 1 }),
		"exclusive anchor");
}

void LocalOnlyPaging() {
	auto ids = std::vector<int>(1001);
	std::iota(ids.rbegin(), ids.rend(), 1);
	auto seen = std::vector<int>();
	auto offset = 0;
	for (auto guard = 0; guard < 100; ++guard) {
		const auto request = HistoryPageRequest{ offset, 0, 30 };
		const auto local = LocalCandidates(ids, request);
		const auto result = MergeHistoryPage({}, local, request);
		if (result.empty()) {
			break;
		}
		Require(result.size() <= 30, "local-only older page is unbounded");
		const auto page = Ids(result);
		seen.insert(seen.end(), page.begin(), page.end());
		Require(!offset || page.back() < offset, "older cursor did not advance");
		offset = page.back();
	}
	Require(
		seen == ids,
		"all-deleted older history skipped or repeated messages");
	auto newest = 200;
	seen.clear();
	for (auto guard = 0; guard < 100; ++guard) {
		const auto request = HistoryPageRequest{ newest + 1, -30, 30 };
		const auto local = LocalCandidates(ids, request);
		const auto result = MergeHistoryPage({}, local, request);
		auto fresh = std::vector<int>();
		for (const auto &entry : result) {
			if (entry.id > newest) {
				fresh.push_back(entry.id);
			}
		}
		if (fresh.empty()) {
			break;
		}
		Require(fresh.size() <= 30, "local-only newer page is unbounded");
		seen.insert(seen.end(), fresh.begin(), fresh.end());
		Require(fresh.front() > newest, "newer cursor did not advance");
		newest = fresh.front();
	}
	std::sort(seen.begin(), seen.end(), std::greater<>());
	Require(
		seen == std::vector<int>(ids.begin(), ids.end() - 200),
		"all-deleted newer history skipped or repeated messages");
	const auto request = HistoryPageRequest{ 501, -15, 30 };
	const auto local = LocalCandidates(ids, request);
	const auto around = MergeHistoryPage({}, local, request);
	CheckPage({}, local, around, request, OraclePage(ids, request));
}

} // namespace

int main() {
	DuplicateAndBoundaryCases();
	LocalOnlyPaging();
	const auto cases = ExhaustiveOracle();
	Require(cases == 475136, "exhaustive coverage changed");
	std::cout << "History page tests passed: " << cases
		<< " exhaustive cases plus boundary and local-only paging cases.\n";
}
