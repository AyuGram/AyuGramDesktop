// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

class AyuMessageBase;
class PeerData;

namespace AyuMapper {

template<typename MTPObject>
[[nodiscard]] MTPObject deserializeObject(std::vector<char> serialized);

template<typename MTPObject>
[[nodiscard]] std::vector<char> serializeObject(MTPObject object);

std::pair<std::string, std::vector<char>> serializeTextWithEntities(not_null<HistoryItem*> item);
[[nodiscard]] MTPVector<MTPMessageEntity> deserializeTextWithEntities(std::vector<char> serialized);
int mapItemFlagsToMTPFlags(not_null<HistoryItem*> item);

[[nodiscard]] MTPMessage DeletedMessageToMTP(
	not_null<PeerData*> peer,
	const AyuMessageBase &message);

} // namespace AyuMapper
