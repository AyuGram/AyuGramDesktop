// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/data/entities.h"

class History;

namespace AyuMessages {

void addEditedMessage(not_null<HistoryItem *> item);
std::vector<AyuMessageBase> getEditedMessages(
	ID userId,
	ID dialogId,
	ID messageId,
	ID minId,
	ID maxId,
	int totalLimit);
bool hasRevisions(not_null<HistoryItem*> item);

[[nodiscard]] bool addDeletedMessage(not_null<HistoryItem*> item);
[[nodiscard]] bool canUnloadDeletedMessage(not_null<HistoryItem*> item);
std::vector<not_null<HistoryItem*>> restoreDeletedMessages(
	not_null<History*> history,
	const QVector<MTPMessage> &slice,
	bool newer);
std::vector<AyuMessageBase> getDeletedMessages(
	ID userId,
	ID dialogId,
	ID topicId,
	ID minId,
	ID maxId,
	int totalLimit,
	const QString &searchQuery = QString());
bool hasDeletedMessages(not_null<PeerData*> peer, ID topicId);
void removeDeletedMessage(not_null<HistoryItem*> item);
void clearDeletedMessages(not_null<PeerData*> peer, ID topicId);

}
