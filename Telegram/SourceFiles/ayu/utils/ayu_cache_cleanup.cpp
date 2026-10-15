/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// startup cache removal via SecureWipe::UnlinkOnly.
#include "ayu/utils/ayu_cache_cleanup.h"

#include "storage/storage_secure_wipe.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>

namespace Ayu::CacheCleanup {
namespace {

[[nodiscard]] QString normalizeWorkingDir(QString workingDir) {
	workingDir = QDir::cleanPath(workingDir);
	if (!workingDir.isEmpty() && !workingDir.endsWith('/')) {
		workingDir += '/';
	}
	return workingDir;
}

[[nodiscard]] QString tdataPath(const QString &workingDir) {
	return normalizeWorkingDir(workingDir) + QStringLiteral("tdata/");
}

// The login lives in key_* and the 16-hex account files (with an s or 0/1 suffix), plus settings and maps.
[[nodiscard]] bool isLoginFile(const QString &path) {
	const auto name = QFileInfo(path).fileName();
	static const auto account = QRegularExpression(
		QStringLiteral("^[0-9A-Fa-f]{16}[s01]?$"));
	return name.startsWith(QLatin1String("key_"))
		|| name.startsWith(QLatin1String("settings"))
		|| name == QLatin1String("map")
		|| account.match(name).hasMatch();
}

void appendIfExists(QStringList &out, const QString &path) {
	if (path.isEmpty() || isLoginFile(path)) {
		return;
	}
	if (QFileInfo::exists(path)) {
		out.push_back(QFileInfo(path).absoluteFilePath());
	}
}

} // namespace

QStringList collectTargets(const QString &workingDir, const Options &options) {
	QStringList targets;
	const auto tdata = tdataPath(workingDir);
	if (tdata.isEmpty() || !QDir(tdata).exists()) {
		return targets;
	}

	if (options.clearCache) {
		appendIfExists(targets, tdata + QStringLiteral("temp"));
		const auto entries = QDir(tdata).entryInfoList(
			QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
		for (const auto &entry : entries) {
			const auto name = entry.fileName();
			if (name.startsWith(QLatin1String("temp_"))) {
				appendIfExists(targets, entry.absoluteFilePath());
				continue;
			}
			// Per-account DB roots: user_<name>/{cache,media_cache}
			if (name.startsWith(QLatin1String("user_"))) {
				appendIfExists(
					targets,
					entry.absoluteFilePath() + QStringLiteral("/cache"));
				appendIfExists(
					targets,
					entry.absoluteFilePath() + QStringLiteral("/media_cache"));
			}
		}
	}

	if (options.clearEmoji) {
		appendIfExists(targets, tdata + QStringLiteral("emoji"));
	}

	if (options.clearAyuDatabase) {
		appendIfExists(targets, tdata + QStringLiteral("ayudata.db"));
		const auto rotated = QDir(tdata).entryInfoList(
			{ QStringLiteral("ayudata_*.db") },
			QDir::Files | QDir::Hidden);
		for (const auto &file : rotated) {
			appendIfExists(targets, file.absoluteFilePath());
		}
	}

	targets.removeDuplicates();
	return targets;
}

Result run(const QString &workingDir, const Options &options) {
	Result result;
	const auto tdata = tdataPath(workingDir);
	if (tdata.isEmpty()) {
		return result;
	}

	Storage::SecureWipe::Options wipeOptions;
	wipeOptions.mode = Storage::SecureWipe::Mode::UnlinkOnly;
	wipeOptions.maxRetries = 3;
	wipeOptions.retryDelayMs = 100;
	wipeOptions.confineRoot = tdata;

	const auto targets = collectTargets(workingDir, options);
	for (const auto &path : targets) {
		const auto info = QFileInfo(path);
		if (!info.exists()) {
			continue;
		}
		if (info.isDir()) {
			const auto nested = Storage::SecureWipe::wipeDirectory(
				path,
				wipeOptions);
			result.removed += nested.filesRemoved + nested.directoriesRemoved;
			result.skipped += nested.filesSkipped;
		} else {
			const auto one = Storage::SecureWipe::wipeFile(path, wipeOptions);
			result.removed += one.filesRemoved;
			result.skipped += one.filesSkipped;
		}
	}
	return result;
}

} // namespace Ayu::CacheCleanup
