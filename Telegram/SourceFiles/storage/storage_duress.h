/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// duress password classify / wipe / purge orchestration.
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace Storage::Duress {

inline constexpr int kLogoutTimeoutMs = 4000;
inline constexpr char kVerifierFileName[] = "ayugram_dv";

enum class PasscodeKind {
	Empty,
	Wrong,
	Flood,
	Real,
	Duress,
};

// Opaque verifier under tdata/ (does not alter key_* format for older builds).
[[nodiscard]] QString verifierPath();
[[nodiscard]] bool isConfigured();
[[nodiscard]] bool setDuressPasscode(
	const QByteArray &realPasscode,
	const QByteArray &duressPasscode);
[[nodiscard]] bool clearDuressPasscode(const QByteArray &realPasscode);
[[nodiscard]] bool matchesDuress(const QByteArray &utf8Passcode);

// Classify without side effects. Real check uses Domain when available.
[[nodiscard]] PasscodeKind classifyPasscode(const QByteArray &utf8Passcode);

// Warm MTP logout for all accounts, bounded by kLogoutTimeoutMs. Cold = no-op.
void bestEffortWarmLogout();

// Wipes the keys, then the inventory (confined to its roots), starts the Windows purge helper and exits.
void runDuressSequence();

} // namespace Storage::Duress
