// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

namespace AyuMapper {

std::pair<std::string, std::vector<char>> serializeTextWithEntities(not_null<HistoryItem*> item);
int mapItemFlagsToMTPFlags(not_null<HistoryItem*> item);

} // namespace AyuMapper
