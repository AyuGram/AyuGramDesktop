/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// confined wipe-root inventory for duress.
#include "storage/storage_duress_inventory.h"

#include "core/data_folder.h"
#include "core/version.h"
#include "settings.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

namespace Storage::DuressInventory {
namespace {

[[nodiscard]] QString normDir(QString path) {
	path = QDir::cleanPath(path);
	if (!path.isEmpty() && !path.endsWith('/')) {
		path += '/';
	}
	return path;
}

[[nodiscard]] bool existsPath(const QString &path) {
	return !path.isEmpty() && QFileInfo::exists(path);
}

[[nodiscard]] bool isUnder(const QString &path, const QString &root) {
	if (path.isEmpty() || root.isEmpty()) {
		return false;
	}
	const auto abs = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
	auto absRoot = QDir::cleanPath(root);
	if (!absRoot.endsWith('/')) {
		absRoot += '/';
	}
	return abs == QDir::cleanPath(root)
		|| abs.startsWith(absRoot, Qt::CaseInsensitive);
}

void addRoot(
		std::vector<Root> &out,
		Category category,
		const QString &path,
		const QString &confineRoot,
		bool overwrite = true) {
	if (!existsPath(path)) {
		return;
	}
	const auto abs = QFileInfo(path).absoluteFilePath();
	if (!isUnder(abs, confineRoot) && QDir::cleanPath(abs) != QDir::cleanPath(confineRoot)) {
		return;
	}
	for (const auto &existing : out) {
		if (QDir::cleanPath(existing.path) == QDir::cleanPath(abs)) {
			return;
		}
	}
	out.push_back(Root{
		.category = category,
		.path = abs,
		.confineRoot = normDir(confineRoot),
		.overwrite = overwrite,
	});
}

[[nodiscard]] QString downloadsLocation() {
	return normDir(QStandardPaths::writableLocation(
		QStandardPaths::DownloadLocation));
}

[[nodiscard]] QString roamingAppData() {
#ifdef Q_OS_WIN
	const auto roaming = qEnvironmentVariable("APPDATA");
	if (!roaming.isEmpty()) {
		return normDir(roaming);
	}
#endif // Q_OS_WIN
	return normDir(QStandardPaths::writableLocation(
		QStandardPaths::AppDataLocation));
}

// Allow configured download/export only when under a known app root.
[[nodiscard]] bool isAllowedExternalAppPath(
		const QString &path,
		const QString &workingDir,
		const QString &exeDir) {
	if (path.isEmpty()) {
		return false;
	}
	const auto abs = QFileInfo(path).absoluteFilePath();
	if (isUnder(abs, workingDir) || isUnder(abs, exeDir)) {
		return true;
	}
	const auto appData = roamingAppData();
	for (const auto &name : knownAppDataFolderNames()) {
		if (isUnder(abs, appData + name)) {
			return true;
		}
	}
	const auto downloads = downloadsLocation();
	for (const auto &name : knownDownloadFolderNames()) {
		if (isUnder(abs, downloads + name)) {
			return true;
		}
	}
	// Sidecar
	if (isUnder(abs, Core::DataFolder::sidecarRoot())) {
		return true;
	}
	return false;
}

} // namespace

QStringList installDirEntries() {
	return {
		cExeName(),
		QStringLiteral("Updater.exe"),
		QStringLiteral("modules"),
		QStringLiteral("unins000.exe"),
		QStringLiteral("unins000.dat"),
		QStringLiteral("unins000.msg"),
		QStringLiteral("data"),
		QStringLiteral("data_config"),
		QStringLiteral("log.txt"),
	};
}

QStringList knownAppDataFolderNames() {
	return {
		QStringLiteral("AyuGram Desktop"),
		QStringLiteral("AyuGram"),
		AppName.utf16(),
	};
}

QStringList knownDownloadFolderNames() {
	return {
		QStringLiteral("AyuGram Desktop"),
		QStringLiteral("AyuGram"),
		QStringLiteral("Telegram Desktop"),
		AppName.utf16(),
	};
}

QString categoryName(Category category) {
	switch (category) {
	case Category::TdataEntire: return QStringLiteral("tdata");
	case Category::WorkingDirExtras: return QStringLiteral("workdir_extras");
	case Category::InstallDir: return QStringLiteral("install_dir");
	case Category::AppDataAyuGram: return QStringLiteral("appdata_ayugram");
	case Category::AppDataSidecar: return QStringLiteral("appdata_sidecar");
	case Category::ConfiguredDownloadDir: return QStringLiteral("download_dir");
	case Category::ConfiguredExportDir: return QStringLiteral("export_dir");
	}
	return QStringLiteral("unknown");
}

std::vector<Root> collectWipeRoots(
		const QString &workingDir,
		const QString &exeDir,
		const QString &configuredDownloadPath,
		const QString &configuredExportPath) {
	std::vector<Root> out;
	const auto work = normDir(workingDir);
	const auto exe = normDir(exeDir);
	if (work.isEmpty()) {
		return out;
	}

	// 1) Entire tdata — keys, user_*, caches, media_cache, temps, ayu_settings,
	//    ayudata.db, emoji/fonts, drafts/maps, stickers, etc.
	const auto tdata = work + QStringLiteral("tdata");
	addRoot(out, Category::TdataEntire, tdata, work);

	// 2) Other workdir trees commonly holding residue
	for (const auto &name : {
		QStringLiteral("DebugLogs"),
		QStringLiteral("tupdates"),
		QStringLiteral("tcache"),
		QStringLiteral("tdumps"),
		QStringLiteral("modules"),
		QStringLiteral("log.txt"),
	}) {
		addRoot(out, Category::WorkingDirExtras, work + name, work);
	}

	// The early log is written next to the program until the profile folder is known.
	for (const auto &[folder, category] : {
		std::pair(work, Category::WorkingDirExtras),
		std::pair(exe, Category::InstallDir),
	}) {
		const auto logs = QDir(folder).entryList(
			{ QStringLiteral("log_start*.txt") },
			QDir::Files);
		for (const auto &name : logs) {
			addRoot(out, category, folder + name, folder);
		}
	}

	// 3) The program folder may be shared with the user's files: list only what the app put there.
	if (!exe.isEmpty()) {
		for (const auto &name : installDirEntries()) {
			const auto isDataFile = (name == QLatin1String("data"))
				|| (name == QLatin1String("data_config"));
			if (isDataFile && QFileInfo(exe + name).isDir()) {
				continue;
			}
			addRoot(out, Category::InstallDir, exe + name, exe, true);
		}
	}

	// 4) AppData AyuGram / AyuGram folders
	const auto appData = roamingAppData();
	for (const auto &name : knownAppDataFolderNames()) {
		const auto folder = appData + name;
		const auto category = name.contains(QLatin1String("AyuGramConfig"), Qt::CaseInsensitive)
			? Category::AppDataSidecar
			: Category::AppDataAyuGram;
		addRoot(out, category, folder, folder);
	}
	// Explicit sidecar root
	addRoot(
		out,
		Category::AppDataSidecar,
		Core::DataFolder::sidecarRoot(),
		Core::DataFolder::sidecarRoot());

	// 5) Default Downloads\<AppName>\ folders
	const auto downloads = downloadsLocation();
	for (const auto &name : knownDownloadFolderNames()) {
		addRoot(
			out,
			Category::ConfiguredDownloadDir,
			downloads + name,
			downloads + name);
	}

	// 6) Configured download/export — only if under known roots
	if (isAllowedExternalAppPath(configuredDownloadPath, work, exe)) {
		addRoot(
			out,
			Category::ConfiguredDownloadDir,
			configuredDownloadPath,
			configuredDownloadPath);
	}
	if (isAllowedExternalAppPath(configuredExportPath, work, exe)) {
		addRoot(
			out,
			Category::ConfiguredExportDir,
			configuredExportPath,
			configuredExportPath);
	}

	return out;
}

QStringList collectWipePaths(
		const QString &workingDir,
		const QString &exeDir,
		const QString &configuredDownloadPath,
		const QString &configuredExportPath) {
	QStringList paths;
	for (const auto &root : collectWipeRoots(
			workingDir,
			exeDir,
			configuredDownloadPath,
			configuredExportPath)) {
		paths.push_back(root.path);
	}
	paths.removeDuplicates();
	return paths;
}

} // namespace Storage::DuressInventory
