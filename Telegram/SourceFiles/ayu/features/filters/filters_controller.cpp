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
#include "history/view/history_view_element.h"
#include "unicode/regex.h"

#include <memory>
#include <unordered_set>

namespace FiltersController {

std::unordered_set<long long> showingFilteredMessages;

bool filterBlocked(const not_null<HistoryItem*> item) {
	if (item->from() != item->history()->peer) {
		if (isBlocked(item)) {
			return true;
		}
	}
	if (const auto bot = item->viaBot()) {
		if (isBlocked(bot)) {
			return true;
		}
	}
	return false;
}

std::optional<bool> isFiltered(
		const QString &str,
		long long dialogId,
		const std::shared_ptr<const FiltersCacheController::Cache> &cache) {
	if (str.isEmpty()) {
		return std::nullopt;
	}

	const auto icuStr = icu::UnicodeString(reinterpret_cast<const UChar*>(str.constData()), str.length());

	const auto matches = [&](const ReversiblePattern &pattern)
	{
		UErrorCode status = U_ZERO_ERROR;

		const auto matcher = std::unique_ptr<icu::RegexMatcher>(pattern.pattern->matcher(icuStr, status));
		if (U_FAILURE(status) || !matcher) {
			LOG(("FILTER FAILED: %1").arg(u_errorName(status)));
			return false;
		}

		const auto match = matcher->find();
		const auto reversed = pattern.reversed;

		if ((!reversed && match) || (reversed && !match)) {
			return true;
		}
		return false;
	};

	if (const auto i = cache->patternsByDialogId.find(dialogId); i != cache->patternsByDialogId.end()) {
		for (const auto &pattern : i->second) {
			if (matches(pattern)) {
				return true;
			}
		}
	}

	const auto exclusions = cache->exclusionsByDialogId.find(dialogId);
	if (!cache->sharedPatterns.empty()) {
		for (const auto &pattern : cache->sharedPatterns) {
			if (exclusions != cache->exclusionsByDialogId.end() && exclusions->second.contains(pattern)) {
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
	if (!settings.filtersEnabled()) {
		return false;
	}
	if (peer->isBroadcast()) {
		return settings.filtersEnabledInChannels();
	}
	if (peer->isChat() || peer->isMegagroup()) {
		return settings.filtersEnabledInGroups();
	}
	if (peer->isUser()) {
		return settings.filtersEnabledInPrivate();
	}
	return true;
}

bool isBlocked(const not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();

	auto shadowBanMatched = false;
	const auto blocked = [&]() -> bool
	{
		const auto isShadowBanned = [&](PeerData *peer) {
			return peer
				&& (peer->isUser() || peer->isBroadcast())
				&& settings.isShadowBanned(getDialogIdFromPeer(peer));
		};

		if (isShadowBanned(item->from())
			&& item->from()->id != item->history()->peer->id) {
			shadowBanMatched = true;
			return true;
		}

		if (item->from()->isUser()
			&& item->from()->asUser()->isBlocked()) {
			// don't hide messages if it's a dialog with blocked user
			return item->from()->asUser()->id != item->history()->peer->id;
		}

		if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
			if (const auto originalSender = forwarded->originalSender) {
				const auto originalShadowBanned = isShadowBanned(originalSender);
				if (originalShadowBanned
					|| (originalSender->isUser()
						&& originalSender->asUser()->isBlocked())) {
					shadowBanMatched = originalShadowBanned;
					return true;
				}
			}
		}
		return false;
	}();

	return settings.filtersEnabled()
		&& (shadowBanMatched || settings.hideFromBlocked())
		&& blocked;
}

bool isBlocked(const not_null<PeerData*> peer) {
	const auto &settings = AyuSettings::getInstance();
	return settings.filtersEnabled() &&
	(
		(peer->isUser() && peer->asUser()->isBlocked() && settings.hideFromBlocked()) ||
		((peer->isUser() || peer->isBroadcast()) && settings.isShadowBanned(getDialogIdFromPeer(peer)))
	);
}

static base::flat_set<not_null<const HistoryItem*>> notifiedDuplicates;

const HistoryItem *getPreviousNonService(const not_null<HistoryItem*> item) {
	const auto history = item->history();
	const auto &blocks = history->blocks;
	bool foundSelf = false;
	const HistoryItem *lastNonServiceInBlocks = nullptr;

	for (auto bIt = blocks.rbegin(); bIt != blocks.rend(); ++bIt) {
		const auto &msgs = (*bIt)->messages;
		for (auto mIt = msgs.rbegin(); mIt != msgs.rend(); ++mIt) {
			const auto data = (*mIt)->data();
			if (data == item) {
				foundSelf = true;
				continue;
			}
			if (data->isService()) {
				continue;
			}
			if (!lastNonServiceInBlocks) {
				lastNonServiceInBlocks = data;
			}
			if (foundSelf) {
				return data;
			}
		}
	}
	return foundSelf ? nullptr : lastNonServiceInBlocks;
}

const HistoryItem *getDuplicateHead(const not_null<HistoryItem*> item) {
	if (!AyuSettings::getInstance().collapseDuplicates() || item->isService()) {
		return nullptr;
	}
	const auto &text = item->originalText().text;
	if (text.isEmpty()) {
		return nullptr;
	}

	const auto prev = getPreviousNonService(item);
	if (!prev) {
		return nullptr;
	}

	if (prev->from() != item->from() || prev->originalText().text != text) {
		return nullptr;
	}

	const HistoryItem *head = prev;
	while (const auto earlier = getPreviousNonService(const_cast<HistoryItem*>(head))) {
		if (earlier->from() == item->from() && earlier->originalText().text == text) {
			head = earlier;
		} else {
			break;
		}
	}

	return head;
}

void notifyDuplicateHead(
		not_null<const HistoryItem*> duplicateItem,
		not_null<const HistoryItem*> headItem) {
	if (notifiedDuplicates.contains(duplicateItem)) {
		return;
	}
	if (notifiedDuplicates.size() > 2000) {
		notifiedDuplicates.clear();
	}
	notifiedDuplicates.insert(duplicateItem);

	const auto headPtr = const_cast<HistoryItem*>(headItem.get());
	crl::on_main([=] {
		headPtr->history()->owner().requestItemViewRefresh(headPtr);
	});
}

bool isDuplicateMessage(const not_null<HistoryItem*> item) {
	if (!AyuSettings::getInstance().collapseDuplicates() || item->isService()) {
		return false;
	}
	const auto head = getDuplicateHead(item);
	if (!head) {
		return false;
	}
	notifyDuplicateHead(item, head);
	return true;
}

int countDuplicateGroupSize(const not_null<HistoryItem*> item) {
	if (!AyuSettings::getInstance().collapseDuplicates() || item->isService()) {
		return 1;
	}
	const auto &text = item->originalText().text;
	if (text.isEmpty()) {
		return 1;
	}

	int count = 1;
	const auto history = item->history();
	const auto &blocks = history->blocks;
	bool foundSelf = false;

	for (const auto &block : blocks) {
		for (const auto &element : block->messages) {
			const auto nextData = element->data();
			if (nextData == item) {
				foundSelf = true;
				continue;
			}
			if (!foundSelf || nextData->isService()) {
				continue;
			}
			if (nextData->from() == item->from()
				&& nextData->originalText().text == text) {
				count++;
			} else {
				return count;
			}
		}
	}
	return count;
}

bool isBlockedOrRegexFiltered(const not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return false;
	}

	if (item->out()) {
		return false;
	}

	if (filterBlocked(item)) {
		FiltersCacheController::putHiddenBlockedMessage(item);
		return true;
	}

	if (!isEnabled(item->history()->peer)) return false;

	const auto cached = FiltersCacheController::isFiltered(item);
	if (cached.has_value()) {
		return cached.value();
	}
	const auto group = item->history()->owner().groups().find(item);
	const auto cache = FiltersCacheController::snapshot();
	const auto res = isFiltered(
		FilterUtils::extractAllText(item, group),
		getDialogIdFromPeer(item->history()->peer),
		cache);

	if (res.has_value()) {
		FiltersCacheController::putFiltered(item, group, res.value(), cache);
		return res.value();
	}
	return false;
}

bool filtered(const not_null<HistoryItem*> item) {
	if (showingFilteredMessages.contains(item->history()->peer->id.value)) {
		return false;
	}

	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return false;
	}

	if (!isEnabled(item->history()->peer)) return false;

	if (settings.collapseDuplicates() && isDuplicateMessage(item)) {
		return true;
	}

	return isBlockedOrRegexFiltered(item);
}

std::optional<bool> filteredMessagesShown(not_null<PeerData*> peer) {
	if (!showingFilteredMessages.contains(peer->id.value)
		&& !FiltersCacheController::hasFilteredMessages(peer)) {
		return std::nullopt;
	}
	return showingFilteredMessages.contains(peer->id.value);
}

void toggleFilteredMessagesShown(not_null<PeerData*> peer) {
	if (showingFilteredMessages.contains(peer->id.value)) {
		showingFilteredMessages.erase(peer->id.value);
	} else {
		showingFilteredMessages.insert(peer->id.value);
	}
	FiltersCacheController::fireUpdate();
}

void invalidate(not_null<HistoryItem*> item) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.filtersEnabled()) {
		return;
	}

	FiltersCacheController::invalidate(item);
}

}
