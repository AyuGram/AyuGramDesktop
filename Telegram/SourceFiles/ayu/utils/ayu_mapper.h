// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/entities.h"
#include "mtproto/connection_abstract.h"
#include "mtproto/details/mtproto_dump_to_text.h"
#include "scheme.h"

class HistoryItem;

namespace AyuMapper {

constexpr int kDocumentTypeNone = 0;
constexpr int kDocumentTypePhoto = 1;
constexpr int kDocumentTypeVoice = 2;
constexpr int kDocumentTypeVideoNote = 3;
constexpr int kDocumentTypeAnimation = 4;
constexpr int kDocumentTypeVideo = 5;
constexpr int kDocumentTypeSticker = 6;
constexpr int kDocumentTypeAudio = 7;
constexpr int kDocumentTypeFile = 8;

template<typename MTPObject>
[[nodiscard]] std::vector<char> serializeObject(MTPObject object) {
	mtpBuffer buffer;
	object.write(buffer);

	const auto from = reinterpret_cast<const char*>(buffer.data());
	const auto end = from + buffer.size() * sizeof(mtpPrime);

	return std::vector<char>(from, end);
}

template<typename MTPObject>
[[nodiscard]] MTPObject deserializeObject(const std::vector<char> &serialized) {
	if (serialized.empty()) {
		return MTPObject();
	}
	if (serialized.size() % sizeof(mtpPrime) != 0) {
		return MTPObject();
	}
	const auto from = reinterpret_cast<const mtpPrime*>(serialized.data());
	const auto end = from + serialized.size() / sizeof(mtpPrime);

	auto current = from;
	MTPObject result;
	if (!result.read(current, end)) {
		return MTPObject();
	}

	return result;
}

std::pair<std::string, std::vector<char>> serializeTextWithEntities(not_null<HistoryItem*> item);
[[nodiscard]] MTPVector<MTPMessageEntity> deserializeTextWithEntities(const std::vector<char> &serialized);
int mapItemFlagsToMTPFlags(not_null<HistoryItem*> item);

void mapMediaToMessage(not_null<HistoryItem*> item, AyuMessageBase &message);

} // namespace AyuMapper
