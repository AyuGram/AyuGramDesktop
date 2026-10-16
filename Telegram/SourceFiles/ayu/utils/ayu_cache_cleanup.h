/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// startup cache removal (unlink-only, Windows retries via SecureWipe).
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace Ayu::CacheCleanup {

struct Options {
	bool clearCache = true;
	bool clearEmoji = false;
	bool clearAyuDatabase = false;
};

struct Result {
	int removed = 0;
	int skipped = 0;
};

// Collect absolute paths under {workingDir}/tdata that match the wipe set.
// Never includes key_*, account maps, or ayu_settings.json.
[[nodiscard]] QStringList collectTargets(
	const QString &workingDir,
	const Options &options);

// Delete cache/media_cache/temp (and optional emoji / ayudata.db) before DBs open.
// Uses Storage::SecureWipe::UnlinkOnly with retries. Failures are skipped.
[[nodiscard]] Result run(const QString &workingDir, const Options &options);

} // namespace Ayu::CacheCleanup
