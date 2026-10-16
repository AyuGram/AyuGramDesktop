/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The password window of the vault, shown at launch before Qt exists.
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <functional>

namespace Platform::VaultPrompt {

// Asks until `check` accepts a password (true) or the user quits (false).
[[nodiscard]] bool Ask(
	const QString &folder,
	const std::function<bool(const QByteArray &)> &check);

} // namespace Platform::VaultPrompt
