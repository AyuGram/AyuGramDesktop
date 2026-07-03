// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/entities.h"

class HistoryItem;

namespace AyuMapper {

constexpr int kDocumentTypeNone = 0;
constexpr int kDocumentTypePhoto = 1;
constexpr int kDocumentTypeVideoNote = 2;
constexpr int kDocumentTypeAnimation = 3;
constexpr int kDocumentTypeVideo = 4;
constexpr int kDocumentTypeVoice = 5;
constexpr int kDocumentTypeSticker = 6;
constexpr int kDocumentTypeAudio = 7;
constexpr int kDocumentTypeFile = 8;

template<typename MTPObject>
[[nodiscard]] MTPObject deserializeObject(std::vector<char> serialized);

template<typename MTPObject>
[[nodiscard]] std::vector<char> serializeObject(MTPObject object);

std::pair<std::string, std::vector<char>> serializeTextWithEntities(not_null<HistoryItem*> item);
[[nodiscard]] MTPVector<MTPMessageEntity> deserializeTextWithEntities(std::vector<char> serialized);
int mapItemFlagsToMTPFlags(not_null<HistoryItem*> item);
void mapMediaToMessage(not_null<HistoryItem*> item, AyuMessageBase &message);

} // namespace AyuMapper
