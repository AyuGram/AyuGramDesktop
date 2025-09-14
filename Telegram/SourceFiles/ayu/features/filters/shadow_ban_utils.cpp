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

std::unordered_set<ID> ShadowBanUtils::shadowBanList;

void ShadowBanUtils::reloadShadowBan() {
	loadShadowBanList();
}


void ShadowBanUtils::addShadowBan(ID userId) {
	if (shadowBanList.insert(userId).second) {
		setShadowBanList();
	}
}

void ShadowBanUtils::removeShadowBan(ID userId) {
	if (shadowBanList.erase(userId) > 0) {
		setShadowBanList();
	}
}

bool ShadowBanUtils::isShadowBanned(ID userId) {
	return shadowBanList.contains(userId);
}

void ShadowBanUtils::loadShadowBanList() {
	auto &settings = AyuSettings::getInstance();

	if (settings.shadowBanIds.isEmpty()) {
		return;
	}
	const auto idList = settings.shadowBanIds.split(',', Qt::SkipEmptyParts);

	for (const auto &id : idList) {
		shadowBanList.insert(id.toLongLong());
	}
}
const std::unordered_set<ID> &ShadowBanUtils::getShadowBanList() {
	return shadowBanList;
}
void ShadowBanUtils::setShadowBanList() {
	QStringList idStringList;
	idStringList.reserve(shadowBanList.size());
	for (const auto &id : shadowBanList) {
		idStringList.push_back(QString::number(id));
	}

	FiltersCacheController::rebuildCache();

	AyuSettings::set_shadowBanIds(idStringList.join(","));
	AyuSettings::save();
}