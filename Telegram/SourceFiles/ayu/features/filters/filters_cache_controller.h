// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/entities.h"
#include "ayu/features/filters/filters_controller.h"
#include "rpl/producer.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Data {
struct Group;
}

namespace FiltersCacheController {

using HashablePattern = FiltersController::HashablePattern;
using PatternHasher = FiltersController::PatternHasher;
using ReversiblePattern = FiltersController::ReversiblePattern;

void fireUpdate();
[[nodiscard]] rpl::producer<> updates();

void rebuildCache();

struct Cache
{
	std::vector<HashablePattern> sharedPatterns;
	std::unordered_map<ID, std::vector<ReversiblePattern>> patternsByDialogId;
	std::unordered_map<ID, std::unordered_set<HashablePattern, PatternHasher>> exclusionsByDialogId;
};

[[nodiscard]] std::shared_ptr<const Cache> snapshot();

std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>> buildExclusions(
	const std::vector<RegexFilterGlobalExclusion> &exclusions,
	const std::vector<HashablePattern> &shared);

std::optional<bool> isFiltered(not_null<HistoryItem*> item);
bool hasFilteredMessages(not_null<PeerData*> peer);
void putHiddenBlockedMessage(not_null<HistoryItem*> item);
void putFiltered(
	not_null<HistoryItem*> item,
	const Data::Group *group,
	bool res,
	const std::shared_ptr<const Cache> &matchedCache);

void invalidate(not_null<HistoryItem*> item);

}
