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

namespace Data {
struct Group;
}

using namespace FiltersController;

namespace FiltersCacheController {

void fireUpdate();
[[nodiscard]] rpl::producer<> updates();

void rebuildCache();

std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>> buildExclusions(
	const std::vector<RegexFilterGlobalExclusion> &exclusions,
	const std::vector<HashablePattern> &shared);

std::optional<bool> isFiltered(not_null<HistoryItem*> item);
void putFiltered(not_null<HistoryItem*> item, const Data::Group *group, bool res);

void invalidate(not_null<HistoryItem*> item);

std::optional<std::vector<ReversiblePattern>> getPatternsByDialogId(uint64 dialogId);
std::optional<const std::unordered_set<HashablePattern, PatternHasher>> getExclusionsByDialogId(long long dialogId);
const std::vector<HashablePattern> &getSharedPatterns();

}
