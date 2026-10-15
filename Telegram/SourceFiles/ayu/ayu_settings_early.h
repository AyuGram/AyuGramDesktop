/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// read a few flags from tdata/ayu_settings.json before full AyuSettings load.
#pragma once

#include <QtCore/QString>

namespace AyuSettingsEarly {

struct Flags {
	bool clearCacheOnStartup = false;
	bool clearEmojiCacheOnStartup = false;
	bool clearAyuDatabaseOnStartup = false;
};

// settingsPath empty → cWorkingDir() + "tdata/ayu_settings.json"
[[nodiscard]] Flags read(const QString &settingsPath = {});

[[nodiscard]] QString defaultSettingsPath();

} // namespace AyuSettingsEarly
