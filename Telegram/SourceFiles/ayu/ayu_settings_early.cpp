/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// early JSON flag reader (no full AyuSettings singleton).
#include "ayu/ayu_settings_early.h"

#include "settings.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace AyuSettingsEarly {

QString defaultSettingsPath() {
	return cWorkingDir() + QStringLiteral("tdata/ayu_settings.json");
}

Flags read(const QString &settingsPath) {
	Flags flags;
	const auto path = settingsPath.isEmpty()
		? defaultSettingsPath()
		: settingsPath;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return flags;
	}
	const auto doc = QJsonDocument::fromJson(file.readAll());
	file.close();
	if (!doc.isObject()) {
		return flags;
	}
	const auto obj = doc.object();
	flags.clearCacheOnStartup = obj.value(
		QLatin1String("clearCacheOnStartup")).toBool(false);
	flags.clearEmojiCacheOnStartup = obj.value(
		QLatin1String("clearEmojiCacheOnStartup")).toBool(false);
	flags.clearAyuDatabaseOnStartup = obj.value(
		QLatin1String("clearAyuDatabaseOnStartup")).toBool(false);
	return flags;
}

} // namespace AyuSettingsEarly
