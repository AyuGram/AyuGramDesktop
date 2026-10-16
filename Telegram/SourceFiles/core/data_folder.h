/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// AyuGram sidecar workdir + strict data-folder checks (Windows-first).
#pragma once

#include <QtCore/QString>

namespace Core::DataFolder {

enum class ResolveStatus {
	UseDefault,           // no sidecar / ignore sidecar
	AppliedSidecar,       // sidecar path applied to cWorkingDir
	StrictUnavailable,    // sidecar set, root missing, strict on — do not create default profile
	SidecarUnavailable,   // sidecar set, root missing, strict off — keep prior default
};

struct SidecarConfig {
	QString workDir;          // absolute, trailing '/' when non-empty
	bool strictDataFolder = false;
};

// %APPDATA%/AyuGramConfig on Windows; ~/.config/AyuGramConfig elsewhere.
[[nodiscard]] QString sidecarRoot();
[[nodiscard]] QString workdirSidecarPath();
[[nodiscard]] QString configSidecarPath();

[[nodiscard]] SidecarConfig readSidecar();
[[nodiscard]] bool writeSidecar(const SidecarConfig &config);

[[nodiscard]] bool isPathAvailable(const QString &path);
// Drive/root exists and is usable as a parent for tdata (Windows: drive letter present).
[[nodiscard]] bool isDataFolderRootAvailable(const QString &path);

// Apply sidecar when -workdir was not provided. Call after portable/default
// workdir selection and before mkpath(tdata). May call cForceWorkingDir.
[[nodiscard]] ResolveStatus applySidecarWorkDir(bool hasCliWorkDir);

[[nodiscard]] bool startupBlocked();
void clearStartupBlocked();

// Strict folder missing at startup (e.g. VeraCrypt volume not mounted).
// Shows a native Retry / Cancel prompt naming the folder; returns true
// when the user chose Retry. No Qt application object is required.
[[nodiscard]] bool promptStrictUnavailable(const QString &workDir);

} // namespace Core::DataFolder
