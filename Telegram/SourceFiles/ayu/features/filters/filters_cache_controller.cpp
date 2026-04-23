// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/filters/filters_cache_controller.h"

#include "ayu/data/ayu_database.h"
#include "ayu/features/filters/filters_controller.h"
#include "data/data_groups.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "rpl/event_stream.h"

#include <unordered_set>

static std::mutex mutex;

namespace FiltersCacheController {

rpl::event_stream<> filtersUpdateStream;

void fireUpdate() {
	filtersUpdateStream.fire({});
}

rpl::producer<> updates() {
	return filtersUpdateStream.events();
}

std::optional<std::vector<HashablePattern>> sharedPatterns;
std::optional<std::unordered_map<long long, std::vector<ReversiblePattern>>> patternsByDialogId;

std::optional<std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>>> exclusionsByDialogId;

std::unordered_map<long long, std::unordered_map<std::optional<int>, std::optional<bool>>> filteredMessages;

void rebuildCache() {
	std::lock_guard lock(mutex);

	const auto filters = AyuDatabase::getAllRegexFilters();
	const auto exclusions = AyuDatabase::getAllFiltersExclusions();

	std::vector<HashablePattern> shared;
	std::unordered_map<long long, std::vector<ReversiblePattern>> byDialogId;

	for (const auto &filter : filters) {
		if (!filter.enabled || filter.text.empty()) {
			continue;
		}

		int flags = UREGEX_MULTILINE;
		if (filter.caseInsensitive) flags |= UREGEX_CASE_INSENSITIVE;

		auto status = U_ZERO_ERROR;
		auto pattern = RegexPattern::compile(UnicodeString::fromUTF8(filter.text), flags, status);

		if (!pattern) {
			continue;
		}

		if (filter.dialogId.has_value()) {
			byDialogId[filter.dialogId.value()].push_back({std::shared_ptr<RegexPattern>(pattern), filter.reversed});
		} else {
			shared.push_back({filter.id, {std::shared_ptr<RegexPattern>(pattern), filter.reversed}});
		}
	}

	auto exclByDialogId = buildExclusions(exclusions, shared);

	sharedPatterns = shared;
	patternsByDialogId = byDialogId;
	exclusionsByDialogId = exclByDialogId;
	filteredMessages.clear();
}

std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>> buildExclusions(
	const std::vector<RegexFilterGlobalExclusion> &exclusions,
	const std::vector<HashablePattern> &shared) {
	std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>> exclusionsByDialogId;

	for (const auto &exclusion : exclusions) {
		auto &exclusionSet = exclusionsByDialogId[exclusion.dialogId];

		for (const auto &filter : shared) {
			if (filter.id == exclusion.filterId) {
				exclusionSet.insert(filter);
				break;
			}
		}
	}
	return exclusionsByDialogId;
}

std::optional<bool> isFiltered(not_null<HistoryItem*> item) {
	std::lock_guard lock(mutex);
	auto dialogIt = filteredMessages.find(item->history()->peer->id.value);

	if (dialogIt == filteredMessages.end()) {
		return std::nullopt;
	}

	const auto it = dialogIt->second.find(item->id.bare);
	if (it == dialogIt->second.end()) {
		return std::nullopt;
	}

	return it->second;
}

void putFiltered(not_null<HistoryItem*> item, const Data::Group *group, bool res) {
	std::lock_guard lock(mutex);
	filteredMessages[item->history()->peer->id.value][item->id.bare] = res;
	if (group && res) {
		for (const auto& groupItem : group->items) {
			filteredMessages[item->history()->peer->id.value][groupItem->id.bare] = true;
		}
	}
}

void invalidateSingle(not_null<HistoryItem*> item) {
	const auto dialogIt = filteredMessages.find(item->history()->peer->id.value);

	if (dialogIt == filteredMessages.end()) {
		return;
	}

	const auto it = dialogIt->second.find(item->id.bare);
	if (it == dialogIt->second.end()) {
		return;
	}

	dialogIt->second.erase(it);
}

void invalidate(not_null<HistoryItem*> item) {
	std::lock_guard lock(mutex);
	if (const auto group = item->history()->owner().groups().find(item)) {
		for (const auto& groupItem : group->items) {
			invalidateSingle(groupItem);
		}
	} else {
		invalidateSingle(item);
	}
}

std::optional<std::vector<ReversiblePattern>> getPatternsByDialogId(uint64 dialogId) {
	if (!patternsByDialogId.has_value()) {
		rebuildCache();
	}
	const auto it = patternsByDialogId.value().find(dialogId);
	if (it == patternsByDialogId.value().end()) {
		return std::nullopt;
	}

	return it->second;
}

std::optional<const std::unordered_set<HashablePattern, PatternHasher>> getExclusionsByDialogId(long long dialogId) {
	std::lock_guard lock(mutex);

	if (!exclusionsByDialogId.has_value()) {
		rebuildCache();
	}
	const auto it = exclusionsByDialogId.value().find(dialogId);
	if (it == exclusionsByDialogId.value().end()) {
		return std::nullopt;
	}
	return it->second;
}

const std::vector<HashablePattern> &getSharedPatterns() {
	if (!sharedPatterns.has_value()) {
		rebuildCache();
	}
	return sharedPatterns.value();
}

}
