#include "ayu/data/deleted_history_query.h"
#include "ayu/data/history_page.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

struct MessageFields {
	long long fakeId = 0;
	long long userId = 0;
	long long dialogId = 0;
	int messageId = 0;
	int entityCreateDate = 0;
	std::string text;
};

struct Message : MessageFields {
};

void Check(bool condition, const char *description) {
	if (!condition) {
		std::cerr << description << '\n';
		std::exit(1);
	}
}

auto MakeStorage(const std::string &filename) {
	using namespace sqlite_orm;
	return make_storage(
		filename,
		make_table<Message>(
			"DeletedMessage",
			make_column("fakeId", &Message::fakeId, primary_key()),
			make_column("userId", &Message::userId),
			make_column("dialogId", &Message::dialogId),
			make_column("messageId", &Message::messageId),
			make_column("entityCreateDate", &Message::entityCreateDate),
			make_column("text", &Message::text)));
}

template <typename Storage>
void FillFixture(Storage &storage) {
	auto nextId = 1LL;
	const auto add = [&](
			long long userId,
			long long dialogId,
			int messageId,
			int captured,
			const std::string &text) {
		auto message = Message();
		message.fakeId = nextId++;
		message.userId = userId;
		message.dialogId = dialogId;
		message.messageId = messageId;
		message.entityCreateDate = captured;
		message.text = text;
		storage.replace(message);
	};
	for (auto messageId = 1; messageId <= 75; ++messageId) {
		for (auto version = 0; version < 30; ++version) {
			add(1, 20, messageId, 1000 + version, "old");
		}
		add(1, 20, messageId, 1029, "winner");
		add(1, 20, messageId, 1, "later insertion of older snapshot");
		add(2, 20, messageId, 100000, "other account");
		add(1, 21, messageId, 100000, "other dialog");
	}
	add(1, 20, 0, 100000, "zero id");
	add(1, 20, -1, 100000, "local id");
	add(5000000000LL, -1000000000123LL, 42, 1000, "wide scope");
}

template <typename Storage>
std::vector<AyuMessages::HistoryPageEntry> CombinedPage(
		Storage &storage,
		std::span<const int> liveIds,
		AyuMessages::HistoryPageRequest request,
		bool onlyNewer = false) {
	auto deleted = std::vector<Message>();
	if (!onlyNewer) {
		deleted = AyuDatabase::GetDeletedHistoryMessages<Message>(
			storage, 1, 20, 0, request.offsetId, request.limit, false);
	}
	if (request.offsetId > 0 && request.addOffset < 0) {
		auto newer = AyuDatabase::GetDeletedHistoryMessages<Message>(
			storage, 1, 20, request.offsetId - 1, 0, request.limit, true);
		deleted.insert(deleted.end(), newer.begin(), newer.end());
	}
	Check(
		deleted.size() <= std::size_t(2 * request.limit),
		"Local query candidates are unbounded");
	auto deletedEntries = std::vector<AyuMessages::HistoryPageEntry>();
	for (const auto &message : deleted) {
		Check(
			message.text == "winner",
			"Combined page selected an old snapshot");
		deletedEntries.push_back({ message.messageId, int(deletedEntries.size()), true, false });
	}
	auto server = std::vector<AyuMessages::HistoryPageEntry>();
	auto anchor = 0;
	if (request.offsetId) {
		anchor = int(std::count_if(liveIds.begin(), liveIds.end(), [&](int id) {
			return id >= request.offsetId;
		}));
	}
	const auto first = std::clamp(anchor + request.addOffset, 0, int(liveIds.size()));
	for (auto i = first; i < int(liveIds.size()) && server.size() < std::size_t(request.limit); ++i) {
		server.push_back({ liveIds[i], int(server.size()), false, false });
	}
	auto page = AyuMessages::MergeHistoryPage(server, deletedEntries, request);
	for (const auto &entry : page) {
		const auto &source = entry.deleted ? deletedEntries : server;
		Check(
			entry.index >= 0 && std::size_t(entry.index) < source.size(),
			"Combined page escaped source index");
		Check(
			source[entry.index].id == entry.id,
			"Combined page source index points to another ID");
		const auto live = std::find(liveIds.begin(), liveIds.end(), entry.id) != liveIds.end();
		Check(entry.deleted != live, "Deleted snapshot replaced a live anchor");
	}
	if (onlyNewer) {
		std::erase_if(page, [&](const auto &entry) {
			return entry.id < request.offsetId;
		});
	}
	return page;
}

template <typename Storage>
void CheckCombinedPages(Storage &storage, std::span<const int> liveIds) {
	constexpr auto kPageSize = 7;
	const auto changes = storage.total_changes();
	auto seen = std::vector<int>();
	auto offset = 0;
	for (auto guard = 0; guard != 100; ++guard) {
		const auto page = CombinedPage(storage, liveIds, { offset, 0, kPageSize });
		if (page.empty()) {
			break;
		}
		Check(
			page.size() == std::min(std::size_t(kPageSize), 75 - seen.size()),
			"Older combined page omitted an available ID");
		for (const auto &entry : page) {
			seen.push_back(entry.id);
		}
		Check(
			!offset || page.back().id < offset,
			"Older combined cursor did not advance");
		offset = page.back().id;
	}
	Check(seen.size() == 75, "Older combined traversal lost or repeated IDs");
	for (auto i = 0; i != 75; ++i) {
		Check(
			seen[i] == 75 - i,
			"Older combined traversal skipped or repeated a boundary");
	}
	seen.clear();
	auto newest = 0;
	for (auto guard = 0; guard != 100; ++guard) {
		const auto page = CombinedPage(storage, liveIds, { newest + 1, -kPageSize, kPageSize }, true);
		if (page.empty()) {
			break;
		}
		Check(
			page.size() == std::min(std::size_t(kPageSize), 75 - seen.size()),
			"Newer combined page omitted an available ID");
		for (auto i = page.rbegin(); i != page.rend(); ++i) {
			Check(i->id > newest, "Newer combined page repeated an old ID");
			seen.push_back(i->id);
		}
		newest = page.front().id;
	}
	Check(seen.size() == 75, "Newer combined traversal lost or repeated IDs");
	for (auto i = 0; i != 75; ++i) {
		Check(
			seen[i] == i + 1,
			"Newer combined traversal skipped or repeated a boundary");
	}
	const auto centered = CombinedPage(storage, liveIds, { 38, -3, kPageSize });
	Check(centered.size() == kPageSize, "Centered gap page is short");
	for (auto i = 0; i != kPageSize; ++i) {
		Check(centered[i].id == 40 - i, "Centered gap page has incorrect IDs");
	}
	Check(
		storage.total_changes() == changes,
		"Combined traversal wrote to the fixture");
}

void CheckPersistedReconstruction() {
	auto random = std::random_device();
	auto directory = std::filesystem::path();
	for (auto attempt = 0; attempt != 10; ++attempt) {
		const auto candidate = std::filesystem::temp_directory_path()
			/ ("ayugram-query-test-" + std::to_string(random()) + "-" + std::to_string(random()));
		if (std::filesystem::create_directory(candidate)) {
			directory = candidate;
			break;
		}
	}
	Check(!directory.empty(), "Could not create synthetic test directory");
	std::filesystem::permissions(directory, std::filesystem::perms::owner_all);
	const auto filename = (directory / "fixture.sqlite").string();
	const auto anchors = std::vector<int>{ 50, 25 };
	for (auto pass = 0; pass != 2; ++pass) {
		auto storage = MakeStorage(filename);
		storage.open_forever();
		if (!pass) {
			storage.sync_schema();
			FillFixture(storage);
		} else {
			Check(
				storage.total_changes() == 0,
				"Reopen unexpectedly changed fixture");
		}
		CheckCombinedPages(storage, {});
		CheckCombinedPages(storage, anchors);
	}
	Check(
		std::filesystem::remove_all(directory) > 0,
		"Could not remove synthetic fixture");
}

} // namespace

int main() {
	auto storage = MakeStorage("");
	storage.open_forever();
	storage.sync_schema();
	FillFixture(storage);
	const auto changes = storage.total_changes();
	const auto query = [&](long long minId, long long maxId, int limit, bool ascending) {
		return AyuDatabase::GetDeletedHistoryMessages<Message>(
			storage, 1, 20, minId, maxId, limit, ascending);
	};
	for (const auto ascending : { true, false }) {
		auto ids = std::vector<int>();
		auto bound = 0LL;
		for (;;) {
			const auto page = query(ascending ? bound : 0, ascending ? 0 : bound, 20, ascending);
			Check(page.size() <= 20, "Page exceeds limit");
			if (page.empty()) {
				break;
			}
			Check(
				page.size() == std::min(std::size_t(20), 75 - ids.size()),
				"Duplicates consumed the page limit");
			for (const auto &message : page) {
				Check(
					message.text == "winner",
					"Winner is not latest capture and fakeId");
				Check(
					message.userId == 1 && message.dialogId == 20,
					"Query crossed account/dialog scope");
				ids.push_back(message.messageId);
			}
			bound = page.back().messageId;
		}
		Check(ids.size() == 75, "Pagination lost or duplicated messages");
		for (auto index = 0; index != 75; ++index) {
			Check(
				ids[index] == (ascending ? index + 1 : 75 - index),
				"Unexpected page order");
		}
		const auto bounded = query(10, 20, 100, ascending);
		Check(bounded.size() == 9, "Bounds are not exclusive");
		for (auto index = 0; index != 9; ++index) {
			Check(
				bounded[index].messageId == (ascending ? 11 + index : 19 - index),
				"Bounded query order changed");
		}
	}
	Check(query(20, 20, 20, true).empty(), "Equal bounds must be empty");
	Check(query(30, 20, 20, false).empty(), "Reversed bounds must be empty");
	Check(query(0, 0, 0, true).empty(), "Zero limit must be empty");
	Check(query(0, 0, -1, false).empty(), "Negative limit must be empty");
	Check(
		AyuDatabase::GetDeletedHistoryMessages<Message>(storage, 3, 20, 0, 0, 20, true).empty(),
		"Unknown account returned messages");
	Check(
		AyuDatabase::GetDeletedHistoryMessages<Message>(storage, 1, 22, 0, 0, 20, true).empty(),
		"Unknown dialog returned messages");
	const auto wideScope = AyuDatabase::GetDeletedHistoryMessages<Message>(
		storage, 5000000000LL, -1000000000123LL, 0, 0, 1, true);
	Check(
		wideScope.size() == 1 && wideScope.front().text == "wide scope",
		"Scope IDs were truncated");
	Check(
		query(0, 1LL << 56, 100, true).size() == 75,
		"Wide upper bound was truncated");
	Check(
		storage.total_changes() == changes,
		"Read query wrote to the database");
	CheckCombinedPages(storage, {});
	const auto anchors = std::vector<int>{ 50, 25 };
	CheckCombinedPages(storage, anchors);
	CheckPersistedReconstruction();
	std::cout << "deleted history query and combined persistence tests passed\n";
}
