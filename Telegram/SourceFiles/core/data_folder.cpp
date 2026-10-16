/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// AyuGram sidecar workdir + strict data-folder checks (Windows-first).
#include "core/data_folder.h"

#include "logs.h"
#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

#ifdef Q_OS_WIN
#include "platform/win/veracrypt_mount_win.h"
#include <windows.h>
#endif // Q_OS_WIN

namespace Core::DataFolder {
namespace {

bool gStartupBlocked = false;

[[nodiscard]] QString normalizeDir(QString path) {
	path = QDir::cleanPath(path);
	if (path.isEmpty()) {
		return path;
	}
	if (!path.endsWith('/')) {
		path += '/';
	}
	return path;
}

[[nodiscard]] bool writeTextFile(const QString &path, const QString &content) {
	QFileInfo(path).dir().mkpath(QLatin1String("."));
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		return false;
	}
	file.write(content.toUtf8());
	file.close();
	return true;
}

[[nodiscard]] QString readTextFile(const QString &path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return {};
	}
	return QString::fromUtf8(file.readAll()).trimmed();
}

} // namespace

QString sidecarRoot() {
#ifdef Q_OS_WIN
	const auto appData = QStandardPaths::writableLocation(
		QStandardPaths::AppDataLocation);
	// AppDataLocation may already include the Qt app name (AyuGramDesktop).
	// Spec wants a sibling %APPDATA%\AyuGramConfig independent of Qt app name.
	const auto roaming = qEnvironmentVariable("APPDATA");
	if (!roaming.isEmpty()) {
		return normalizeDir(roaming + QStringLiteral("/AyuGramConfig"));
	}
	Q_UNUSED(appData);
	return normalizeDir(
		QDir::homePath() + QStringLiteral("/AppData/Roaming/AyuGramConfig"));
#else // Q_OS_WIN
	const auto config = QStandardPaths::writableLocation(
		QStandardPaths::GenericConfigLocation);
	if (!config.isEmpty()) {
		return normalizeDir(config + QStringLiteral("/AyuGramConfig"));
	}
	return normalizeDir(QDir::homePath() + QStringLiteral("/.config/AyuGramConfig"));
#endif // !Q_OS_WIN
}

QString workdirSidecarPath() {
	return sidecarRoot() + QStringLiteral("workdir.txt");
}

QString configSidecarPath() {
	return sidecarRoot() + QStringLiteral("config.json");
}

SidecarConfig readSidecar() {
	SidecarConfig config;
	const auto work = readTextFile(workdirSidecarPath());
	if (!work.isEmpty()) {
		config.workDir = normalizeDir(QDir(work).absolutePath());
	}

	QFile file(configSidecarPath());
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		const auto doc = QJsonDocument::fromJson(file.readAll());
		file.close();
		if (doc.isObject()) {
			const auto obj = doc.object();
			if (config.workDir.isEmpty() && obj.contains(QLatin1String("workdir"))) {
				config.workDir = normalizeDir(
					QDir(obj.value(QLatin1String("workdir")).toString()).absolutePath());
			}
			config.strictDataFolder = obj.value(
				QLatin1String("strictDataFolder")).toBool(false);
			config.veracryptVolume = obj.value(
				QLatin1String("veracryptVolume")).toString();
		}
	}

	// Early ayu_settings can mirror strict when tdata already exists under a known dir.
	// Prefer sidecar config.json as source of truth before workdir exists.
	return config;
}

bool writeSidecar(const SidecarConfig &config) {
	QDir().mkpath(sidecarRoot());
	auto ok = true;
	if (!config.workDir.isEmpty()) {
		ok = writeTextFile(workdirSidecarPath(), normalizeDir(config.workDir)) && ok;
	} else if (QFile::exists(workdirSidecarPath())) {
		ok = QFile::remove(workdirSidecarPath()) && ok;
	}

	QJsonObject obj;
	if (!config.workDir.isEmpty()) {
		obj.insert(QLatin1String("workdir"), normalizeDir(config.workDir));
	}
	obj.insert(QLatin1String("strictDataFolder"), config.strictDataFolder);
	if (!config.veracryptVolume.isEmpty()) {
		obj.insert(QLatin1String("veracryptVolume"), config.veracryptVolume);
	}
	QSaveFile file(configSidecarPath());
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return false;
	}
	file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
	return file.commit() && ok;
}

bool isPathAvailable(const QString &path) {
	if (path.isEmpty()) {
		return false;
	}
	return QFileInfo::exists(path) || QFileInfo::exists(QFileInfo(path).absolutePath());
}

bool isDataFolderRootAvailable(const QString &path) {
	if (path.isEmpty()) {
		return false;
	}
	const auto abs = QDir::cleanPath(QDir(path).absolutePath());
#ifdef Q_OS_WIN
	// "V:/something" → need V:\ to exist.
	if (abs.size() >= 2 && abs.at(1) == QLatin1Char(':')) {
		const auto root = QString(abs.left(2)) + QLatin1Char('\\');
		const auto attr = GetFileAttributesW(root.toStdWString().c_str());
		return attr != INVALID_FILE_ATTRIBUTES;
	}
	// UNC paths: require the share path to be reachable.
	if (abs.startsWith(QLatin1String("//"))
		|| abs.startsWith(QLatin1String("\\\\"))) {
		return QFileInfo::exists(abs) || QDir(abs).exists();
	}
#endif // Q_OS_WIN
	// POSIX / fallback: parent must exist (mount point present).
	const auto parent = QFileInfo(abs).absolutePath();
	return QDir(parent).exists();
}

ResolveStatus applySidecarWorkDir(bool hasCliWorkDir) {
	gStartupBlocked = false;
	if (hasCliWorkDir) {
		return ResolveStatus::UseDefault;
	}
	const auto sidecar = readSidecar();
	// Merge strict from early settings if config.json omitted it but tdata exists later — not here.

	if (sidecar.workDir.isEmpty()) {
		return ResolveStatus::UseDefault;
	}

	// A different volume can take over a drive letter: strict also needs the folder itself.
	if (isDataFolderRootAvailable(sidecar.workDir)
		&& (!sidecar.strictDataFolder || QDir(sidecar.workDir).exists())) {
		cForceWorkingDir(sidecar.workDir);
		LOG(("DataFolder: applied AyuGram sidecar workdir: %1"
			).arg(cWorkingDir()));
		return ResolveStatus::AppliedSidecar;
	}

	if (sidecar.strictDataFolder) {
		gStartupBlocked = true;
		LOG(("FATAL: Strict data folder unavailable (AyuGram sidecar): %1"
			).arg(sidecar.workDir));
		return ResolveStatus::StrictUnavailable;
	}

	LOG(("DataFolder: sidecar workdir unavailable, strict off, keeping default: %1"
		).arg(sidecar.workDir));
	return ResolveStatus::SidecarUnavailable;
}

bool tryAutoMount() {
#ifdef Q_OS_WIN
	// Cap wait so a cancelled VeraCrypt password cannot stall log init for minutes.
	constexpr auto kWaitMs = 20000;
	constexpr auto kPollMs = 500;
	const auto sidecar = readSidecar();
	if (sidecar.workDir.isEmpty() || sidecar.veracryptVolume.isEmpty()) {
		return false;
	}
	const auto native = QDir::toNativeSeparators(sidecar.workDir);
	if (native.size() < 2
		|| native.at(1) != QLatin1Char(':')
		|| native.at(0).toUpper() < QLatin1Char('A')
		|| native.at(0).toUpper() > QLatin1Char('Z')) {
		return false;
	}
	const auto drive = native.left(1).toUpper();
	const auto mount = Platform::VeraCrypt::mountVolume({
		.volumePath = sidecar.veracryptVolume,
		.driveLetter = drive,
	});
	if (!mount.ok) {
		return false;
	}
	for (auto waited = 0; waited < kWaitMs; waited += kPollMs) {
		if (isDataFolderRootAvailable(sidecar.workDir)) {
			return true;
		}
		Sleep(kPollMs);
	}
	return false;
#else // Q_OS_WIN
	return false;
#endif // !Q_OS_WIN
}

bool promptStrictUnavailable(const QString &workDir) {
	const auto text = QString(
		"The data folder is not available:\n\n%1\n\n"
		"Strict data folder is on, so AyuGram will not start with a "
		"default profile. Mount the volume (for example in VeraCrypt) "
		"and press Retry, or press Cancel to exit."
	).arg(QDir::toNativeSeparators(workDir));
#ifdef Q_OS_WIN
	const auto wide = text.toStdWString();
	const auto result = MessageBoxW(
		nullptr,
		wide.c_str(),
		L"AyuGram",
		MB_RETRYCANCEL | MB_ICONWARNING | MB_SETFOREGROUND);
	return (result == IDRETRY);
#else // Q_OS_WIN
	fprintf(stderr, "%s\n", text.toUtf8().constData());
	return false;
#endif // !Q_OS_WIN
}

bool startupBlocked() {
	return gStartupBlocked;
}

void clearStartupBlocked() {
	gStartupBlocked = false;
}

} // namespace Core::DataFolder
