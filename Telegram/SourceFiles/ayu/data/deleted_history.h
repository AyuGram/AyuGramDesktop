#pragma once

#include "ayu/data/history_page.h"

class History;
class PeerData;

namespace AyuMessages {

struct DeletedHistoryPage {
	QVector<MTPMessage> messages;
	bool restorationEnabled = false;
};

[[nodiscard]] bool HasDeletedHistory(not_null<PeerData*> peer);

[[nodiscard]] DeletedHistoryPage RestoreDeletedHistory(
	not_null<PeerData*> peer,
	const QVector<MTPMessage> &messages,
	HistoryPageRequest request,
	bool onlyNewer);

void ApplyDeletedHistory(
	not_null<History*> history,
	const DeletedHistoryPage &page);

} // namespace AyuMessages
