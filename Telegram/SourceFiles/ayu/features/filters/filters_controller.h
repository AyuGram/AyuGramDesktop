// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "unicode/regex.h"

#include <string>
#include <vector>

using namespace icu_78;

namespace FiltersController {

bool isEnabled(PeerData *peer);
bool isBlocked(not_null<HistoryItem*> item);
bool isBlocked(not_null<PeerData*> peer);
bool filtered(not_null<HistoryItem*> historyItem);

void invalidate(not_null<HistoryItem*> item);

struct ReversiblePattern
{
	std::shared_ptr<RegexPattern> pattern;
	bool reversed;
};

struct HashablePattern
{
	std::vector<char> id;
	ReversiblePattern pattern;

	bool operator==(const HashablePattern &other) const {
		return id == other.id;
	}
};

struct PatternHasher
{
	std::size_t operator()(const HashablePattern &p) const {
		std::string_view view(p.id.data(), p.id.size());
		return std::hash<std::string_view>{}(view);
	}
};

}
