// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/hidden_folders/hidden_folders.h"

#include "ayu/ayu_settings.h"
#include "data/data_chat_filters.h"
#include "lang/lang_keys.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "styles/style_ayu_icons.h"

namespace AyuFeatures::HiddenFolders {

bool IsHidden(const FilterId id) {
	return id && AyuSettings::getInstance().isFolderHidden(id);
}

bool ToggleHidden(const FilterId id) {
	if (!id) {
		return false;
	}
	auto &settings = AyuSettings::getInstance();
	if (IsHidden(id)) {
		settings.removeHiddenFolder(id);
	} else {
		settings.addHiddenFolder(id);
	}
	return IsHidden(id);
}

std::vector<Data::ChatFilter> VisibleOnly(
		const std::vector<Data::ChatFilter> &list) {
	auto result = std::vector<Data::ChatFilter>();
	result.reserve(list.size());
	for (const auto &filter : list) {
		if (!IsHidden(filter.id())) {
			result.push_back(filter);
		}
	}
	return result;
}

void AddToggleAction(const Ui::Menu::MenuCallback &addAction, const FilterId id) {
	if (!id) {
		return;
	}
	const auto hidden = IsHidden(id);
	addAction(
		hidden ? tr::ayu_ShowFolder(tr::now) : tr::ayu_HideFolder(tr::now),
		[=] { ToggleHidden(id); },
		hidden ? &st::ayuFolderShowIcon : &st::ayuFolderHideIcon);
}

} // namespace AyuFeatures::HiddenFolders
