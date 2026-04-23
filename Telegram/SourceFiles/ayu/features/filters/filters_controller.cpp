// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/filters/filters_controller.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/filters/filters_cache_controller.h"
#include "ayu/features/filters/filters_utils.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_peer.h"
#include "data/data_peer_id.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "unicode/regex.h"

namespace FiltersController {

bool filterBlocked(const not_null<HistoryItem*> item) {
	if (item->from() != item->history()->peer) {
		if (isBlocked(item)) {
			return true;
		}
	}
	return false;
}

std::optional<bool> isFiltered(const QString &str, uint64 dialogId) {
	if (str.isEmpty()) {
		return std::nullopt;
	}

	const auto icuStr = UnicodeString(reinterpret_cast<const UChar*>(str.constData()), str.length());

	const auto matches = [&](const ReversiblePattern &pattern)
	{
		UErrorCode status = U_ZERO_ERROR;

		auto match = pattern.pattern->matcher(icuStr, status)->find();
		if (U_FAILURE(status)) {
			LOG(("FILTER FAILED: %1").arg(u_errorName(status)));
			return false;
		}

		const auto reversed = pattern.reversed;

		if ((!reversed && match) || (reversed && !match)) {
			return true;
		}
		return false;
	};

	const auto &dialogPatterns = FiltersCacheController::getPatternsByDialogId(dialogId);
	if (dialogPatterns.has_value() && !dialogPatterns.value().empty()) {
		for (const auto &pattern : dialogPatterns.value()) {
			if (matches(pattern)) {
				return true;
			}
		}
	}

	const auto &exclusions = FiltersCacheController::getExclusionsByDialogId(dialogId);
	const auto &sharedPatterns = FiltersCacheController::getSharedPatterns();
	if (!sharedPatterns.empty()) {
		for (const auto &pattern : sharedPatterns) {
			if (exclusions.has_value() && exclusions.value().contains(pattern)) {
				continue;
			}
			if (matches(pattern.pattern)) {
				return true;
			}
		}
	}
	return false;
}

bool isEnabled(not_null<PeerData*> peer) {
	const auto &settings = AyuSettings::getInstance();
	return settings.filtersEnabled() && (settings.filtersEnabledInChats() || peer->isBroadcast());
}

bool isBlocked(const not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();

	const auto blocked = [&]() -> bool
	{
		if (item->from()->isUser() &&
			item->from()->asUser()->isBlocked()) {
			// don't hide messages if it's a dialog with blocked user
			return item->from()->asUser()->id != item->history()->peer->id;
		}

		if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
			if (forwarded->originalSender &&
				forwarded->originalSender->isUser() &&
				forwarded->originalSender->asUser()->isBlocked()) {
				return true;
			}
		}
		return false;
	}();

	return settings.filtersEnabled() &&
	(
		((item->from()->isUser() || item->from()->isBroadcast()) && settings.isShadowBanned(getDialogIdFromPeer(item->from()))) ||
		(settings.hideFromBlocked() && blocked)
	);
}

bool isBlocked(const not_null<PeerData*> peer) {
	const auto &settings = AyuSettings::getInstance();
	return settings.filtersEnabled() &&
	(
		(peer->isUser() && peer->asUser()->isBlocked() && settings.hideFromBlocked()) ||
		((peer->isUser() || peer->isBroadcast()) && settings.isShadowBanned(getDialogIdFromPeer(peer)))
	);
}

bool filtered(const not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return false;
	}

	if (item->out()) {
		return false;
	}

	if (filterBlocked(item)) return true;

	if (!isEnabled(item->history()->peer)) return false;

	const auto cached = FiltersCacheController::isFiltered(item);
	if (cached.has_value()) {
		return cached.value();
	}
	const auto group = item->history()->owner().groups().find(item);
	const auto res = isFiltered(FilterUtils::extractAllText(item, group), getDialogIdFromPeer(item->history()->peer));

	// sometimes item has empty text.
	// so we cache result only if
	// processed item is filterable
	if (res.has_value()) {
		FiltersCacheController::putFiltered(item, group, res.value());
		return res.value();
	}
	return false;
}

void invalidate(not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return;
	}

	FiltersCacheController::invalidate(item);
}

}
