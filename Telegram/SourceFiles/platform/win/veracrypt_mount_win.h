/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// Phase 4 (nice-to-have): detect/mount via VeraCrypt CLI.
#pragma once

#include <QtCore/QString>

namespace Platform::VeraCrypt {

// Common install paths + PATH lookup. Empty if not found.
[[nodiscard]] QString findExecutable();

struct MountRequest {
	QString volumePath;   // container file or device
	QString driveLetter;  // e.g. "V" (no colon)
	QString veracryptExe; // optional override
};

struct MountResult {
	bool ok = false;
	QString message;
};

// Launch VeraCrypt.exe /v /l /q and let VeraCrypt prompt for the password
// (no /p — avoids password on the command line; no /s — would hide that prompt).
[[nodiscard]] MountResult mountVolume(const MountRequest &request);

} // namespace Platform::VeraCrypt
