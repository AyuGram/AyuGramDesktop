/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The vault: the profile folder is encrypted while the app is closed and opened with a password at launch.
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <functional>

namespace Storage::Vault {

struct Result {
	int files = 0;
	int skipped = 0;
	bool ok = true;
	QString firstFailed;
};

// Whether this platform can run the vault (the password window is Windows only).
[[nodiscard]] bool Supported();

[[nodiscard]] bool IsEnabled(const QString &workingDir);

// Launcher hooks. Open asks for the password and decrypts; it returns false when the user quits.
[[nodiscard]] bool OpenAtStart(const QString &workingDir);
// Close encrypts the profile folder again with the key kept from Open (or Enable).
void CloseAtExit(const QString &workingDir, const std::function<bool()> &keepCaches);

// Settings: arm the vault with a new password (it takes effect when the app closes), or turn it off.
[[nodiscard]] bool Enable(const QString &workingDir, const QByteArray &utf8Password);
[[nodiscard]] bool Disable(const QString &workingDir, const QByteArray &utf8Password);

// The pieces, for the launcher and for tests.
[[nodiscard]] bool UnlockWithPassword(
	const QString &workingDir,
	const QByteArray &utf8Password);
[[nodiscard]] Result UnsealAll(const QString &workingDir);
[[nodiscard]] Result SealAll(const QString &workingDir, bool keepCaches);
void ForgetKey();

} // namespace Storage::Vault
