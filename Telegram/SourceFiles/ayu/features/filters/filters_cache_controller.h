// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "filters_controller.h"
#include "ayu/data/entities.h"

using namespace FiltersController;

namespace FiltersCacheController {


void rebuildCache();

std::unordered_map<long long, std::unordered_set<HashablePattern, PatternHasher>> buildExclusions(
	const std::vector<RegexFilterGlobalExclusion>& exclusions,
	const std::vector<HashablePattern>& shared);

std::optional<bool> isFiltered(not_null<HistoryItem *> item);
void putFiltered(not_null<HistoryItem *> item, bool res);

std::optional<std::vector<ReversiblePattern>> getPatternsByDialogId(uint64 dialogId);
std::optional<const std::unordered_set<HashablePattern, PatternHasher>> getExclusionsByDialogId(long long dialogId);
const std::vector<HashablePattern> &getSharedPatterns();

}

