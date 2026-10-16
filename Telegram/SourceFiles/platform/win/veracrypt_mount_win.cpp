/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// VeraCrypt CLI mount helper (Windows).
#include "platform/win/veracrypt_mount_win.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>

namespace Platform::VeraCrypt {
namespace {

[[nodiscard]] QStringList candidatePaths() {
	QStringList out;
	const auto pf = qEnvironmentVariable("ProgramFiles");
	const auto pf86 = qEnvironmentVariable("ProgramFiles(x86)");
	for (const auto &base : { pf, pf86 }) {
		if (!base.isEmpty()) {
			out.push_back(base + QStringLiteral("/VeraCrypt/VeraCrypt.exe"));
		}
	}
	out.push_back(QStringLiteral("C:/Program Files/VeraCrypt/VeraCrypt.exe"));
	out.push_back(QStringLiteral("C:/Program Files (x86)/VeraCrypt/VeraCrypt.exe"));
	return out;
}

// Mounting over the Windows drive can only fail, or hide the system.
[[nodiscard]] QString systemDriveLetter() {
	const auto drive = qEnvironmentVariable("SystemDrive");
	return drive.isEmpty() ? QStringLiteral("C") : drive.left(1).toUpper();
}

} // namespace

QString findExecutable() {
	for (const auto &path : candidatePaths()) {
		if (QFileInfo::exists(path)) {
			return QDir::toNativeSeparators(path);
		}
	}
	const auto onPath = QStandardPaths::findExecutable(
		QStringLiteral("VeraCrypt.exe"));
	if (!onPath.isEmpty()) {
		return QDir::toNativeSeparators(onPath);
	}
	return {};
}

MountResult mountVolume(const MountRequest &request) {
	auto exe = request.veracryptExe;
	if (exe.isEmpty()) {
		exe = findExecutable();
	}
	if (exe.isEmpty()) {
		return { false, QStringLiteral("VeraCrypt.exe not found") };
	}
	if (request.volumePath.isEmpty() || request.driveLetter.isEmpty()) {
		return { false, QStringLiteral("Volume and drive letter required") };
	}
	auto letter = request.driveLetter;
	letter.remove(QLatin1Char(':'));
	letter = letter.left(1).toUpper();
	if (letter.isEmpty()
		|| letter.at(0) < QLatin1Char('A')
		|| letter.at(0) > QLatin1Char('Z')) {
		return { false, QStringLiteral("The drive letter must be A to Z") };
	}
	if (letter == systemDriveLetter()) {
		return { false, QStringLiteral("The data folder must be on a drive other than the Windows drive") };
	}

	// /q only: the password is typed in VeraCrypt's own prompt (no /p), which /s would suppress.
	const auto args = QStringList{
		QStringLiteral("/v"),
		QDir::toNativeSeparators(request.volumePath),
		QStringLiteral("/l"),
		letter,
		QStringLiteral("/q"),
	};
	if (!QProcess::startDetached(exe, args)) {
		return { false, QStringLiteral("Failed to launch VeraCrypt") };
	}
	return { true, QStringLiteral("VeraCrypt mount started") };
}

} // namespace Platform::VeraCrypt
