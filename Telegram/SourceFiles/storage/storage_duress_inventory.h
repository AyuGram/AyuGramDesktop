/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// confined wipe-root inventory for duress (all app data categories).
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <vector>

namespace Storage::DuressInventory {

enum class Category {
	TdataEntire,           // whole {workdir}/tdata (keys, accounts, caches, settings, DBs)
	WorkingDirExtras,      // DebugLogs, tupdates, tcache, tdumps, modules under workdir
	InstallDir,            // Inno {app} / portable exe dir (binaries + leftover data)
	AppDataAyuGram,        // %APPDATA%\AyuGram Desktop
	AppDataSidecar,       // %APPDATA%\AyuGramConfig (sidecar)
	ConfiguredDownloadDir, // Downloads\<AppName>\ or settings downloadPath if under known roots
	ConfiguredExportDir,   // export path if configured under known roots
};

struct Root {
	Category category = Category::TdataEntire;
	QString path;          // absolute, normalized
	QString confineRoot;   // wipe must stay under this prefix
	bool overwrite = true; // 1-pass before unlink
};

// Build wipe roots for a running/installed layout. Never invents arbitrary user paths.
// downloadPath/exportPath are included ONLY when they resolve under Downloads\<AppName>,
// workdir, install dir, or AppData app folders.
[[nodiscard]] std::vector<Root> collectWipeRoots(
	const QString &workingDir,
	const QString &exeDir,
	const QString &configuredDownloadPath = {},
	const QString &configuredExportPath = {});

// Flat unique absolute paths (directories or files) for contract tests / helpers.
[[nodiscard]] QStringList collectWipePaths(
	const QString &workingDir,
	const QString &exeDir,
	const QString &configuredDownloadPath = {},
	const QString &configuredExportPath = {});

[[nodiscard]] QString categoryName(Category category);

// What the app itself puts in its program folder (the binaries and the
// leftovers the installer's uninstaller would also remove).
[[nodiscard]] QStringList installDirEntries();

// Known safe AppData / Downloads folder names (AyuGram + AyuGram).
[[nodiscard]] QStringList knownAppDataFolderNames();
[[nodiscard]] QStringList knownDownloadFolderNames();

} // namespace Storage::DuressInventory
