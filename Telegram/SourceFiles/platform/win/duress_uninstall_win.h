/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// Windows self-uninstall / purge helper after duress wipe.
#pragma once

#include <QtCore/QString>

namespace Platform::DuressUninstall {

// Stage a hidden helper in %TEMP%, pass intent paths, quit this process.
// Helper waits for PID exit then deletes install/AppData/shortcuts/registry
// for both AyuGram and AyuGram names. Never elevates (no UAC prompt).
void schedulePurge(
	const QString &workingDir,
	const QString &exeDir);

} // namespace Platform::DuressUninstall
