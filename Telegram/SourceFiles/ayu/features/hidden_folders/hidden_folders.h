// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#pragma once

#include "data/data_types.h" // FilterId

#include <vector>

namespace Data {
class ChatFilter;
} // namespace Data

namespace Ui::Menu {
struct MenuCallback;
} // namespace Ui::Menu

namespace AyuFeatures::HiddenFolders {

[[nodiscard]] bool IsHidden(FilterId id);
bool ToggleHidden(FilterId id);
[[nodiscard]] std::vector<Data::ChatFilter> VisibleOnly(const std::vector<Data::ChatFilter> &list);
void AddToggleAction(const Ui::Menu::MenuCallback &addAction, FilterId id);

template <typename Id, typename IsHiddenFn>
[[nodiscard]] std::vector<Id> ReorderVisible(
		std::vector<Id> fullOrder,
		int oldVisiblePosition,
		int newVisiblePosition,
		IsHiddenFn isHidden) {
	if (oldVisiblePosition == newVisiblePosition) {
		return fullOrder;
	}
	auto visibleAt = std::vector<int>();
	visibleAt.reserve(fullOrder.size());
	for (auto i = 0; i < int(fullOrder.size()); ++i) {
		if (!isHidden(fullOrder[i])) {
			visibleAt.push_back(i);
		}
	}
	if (oldVisiblePosition < 0
		|| oldVisiblePosition >= int(visibleAt.size())
		|| newVisiblePosition < 0
		|| newVisiblePosition >= int(visibleAt.size())) {
		return fullOrder;
	}

	auto visibleIds = std::vector<Id>();
	visibleIds.reserve(visibleAt.size());
	for (const auto index : visibleAt) {
		visibleIds.push_back(fullOrder[index]);
	}
	const auto moved = visibleIds[oldVisiblePosition];
	visibleIds.erase(visibleIds.begin() + oldVisiblePosition);
	visibleIds.insert(visibleIds.begin() + newVisiblePosition, moved);

	auto result = fullOrder;
	auto cursor = 0;
	for (auto i = 0; i < int(result.size()); ++i) {
		if (!isHidden(fullOrder[i])) {
			result[i] = visibleIds[cursor++];
		}
	}
	return result;
}

} // namespace AyuFeatures::HiddenFolders
