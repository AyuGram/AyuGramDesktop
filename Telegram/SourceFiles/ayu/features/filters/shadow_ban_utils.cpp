// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "shadow_ban_utils.h"

#include <QStringList>

#include "filters_cache_controller.h"
#include "ayu/ayu_settings.h"
#include "ayu/data/entities.h"

void ShadowBanUtils::addShadowBan(ID userId) {
	auto &settings = AyuSettings::getInstance();
	if (settings.shadowBanIds.insert(userId).second) {
		setShadowBanList();
	}
}

void ShadowBanUtils::removeShadowBan(ID userId) {
	auto &settings = AyuSettings::getInstance();
	if (settings.shadowBanIds.erase(userId) > 0) {
		setShadowBanList();
	}
}

bool ShadowBanUtils::isShadowBanned(ID userId) {
	const auto &settings = AyuSettings::getInstance();
	return settings.shadowBanIds.contains(userId);
}

void ShadowBanUtils::setShadowBanList() {
	FiltersCacheController::rebuildCache();
	AyuSettings::fire_filtersUpdate();
	AyuSettings::save();
}
