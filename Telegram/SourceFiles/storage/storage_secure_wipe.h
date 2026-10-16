/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// 1-pass overwrite + unlink helpers (Windows-first retries).
#pragma once

#include <QtCore/QString>

namespace Storage::SecureWipe {

enum class Mode {
	OverwriteThenUnlink, // duress / secure wipe
	UnlinkOnly,          // startup cache cleanup
};

struct Options {
	Mode mode = Mode::OverwriteThenUnlink;
	int maxRetries = 3;
	int retryDelayMs = 100;
	// Refuse to follow junctions/symlinks that escape this root (empty = no check).
	QString confineRoot;
};

struct Result {
	int filesProcessed = 0;
	int filesRemoved = 0;
	int filesSkipped = 0;
	int directoriesRemoved = 0;
	bool ok = true; // false only on hard errors (e.g. path escapes confineRoot)
};

[[nodiscard]] Result wipeFile(const QString &path, const Options &options = {});
[[nodiscard]] Result wipeDirectory(
	const QString &path,
	const Options &options = {});

// Overwrite an open-able file once, flush, then delete.
[[nodiscard]] bool overwriteAndUnlinkFile(
	const QString &path,
	int maxRetries = 3,
	int retryDelayMs = 100);

[[nodiscard]] bool unlinkFileWithRetries(
	const QString &path,
	int maxRetries = 3,
	int retryDelayMs = 100);

} // namespace Storage::SecureWipe
