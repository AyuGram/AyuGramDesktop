/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// Windows self-uninstall / purge helper after duress wipe.
#include "platform/win/duress_uninstall_win.h"

#include "core/data_folder.h"
#include "storage/storage_duress_inventory.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>

#include <windows.h>

namespace Platform::DuressUninstall {
namespace {

// AyuGram's own installer id (Telegram/build/setup.iss), never the official client's.
constexpr auto kUninstallKey = "{C301E21A-9009-4E52-A839-57C59077EA0F}_is1";

[[nodiscard]] QString CmdEscape(QString path) {
	// cmd expands %NAME% even inside quotes.
	return path.replace('%', QStringLiteral("%%"));
}

[[nodiscard]] QStringList buildIntentPaths(
		const QString &workingDir,
		const QString &exeDir) {
	auto paths = Storage::DuressInventory::collectWipePaths(
		workingDir,
		exeDir);
	// Shortcuts (known names only)
	const auto desktop = QStandardPaths::writableLocation(
		QStandardPaths::DesktopLocation);
	for (const auto &name : {
		QStringLiteral("AyuGram.lnk"),
		QStringLiteral("AyuGram Desktop.lnk"),
	}) {
		paths.push_back(desktop + '/' + name);
	}
	const auto programs = QStandardPaths::writableLocation(
		QStandardPaths::ApplicationsLocation);
	for (const auto &name : Storage::DuressInventory::knownAppDataFolderNames()) {
		paths.push_back(programs + '/' + name);
	}
	paths.removeDuplicates();
	return paths;
}

[[nodiscard]] QString writeHelperScript(
		const QStringList &paths,
		const QStringList &foldersIfEmpty,
		qint64 pid) {
	const auto temp = QStandardPaths::writableLocation(
		QStandardPaths::TempLocation);
	const auto script = temp + QStringLiteral("/syrg_purge_%1.cmd").arg(pid);
	QFile file(script);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return {};
	}
	QTextStream out(&file);
	out.setCodec("UTF-8");
	out << "@echo off\r\n";
	out << "chcp 65001 >nul\r\n";
	out << "setlocal\r\n";
	out << "cd /d \"%TEMP%\"\r\n";
	// Wait for parent PID to exit (max ~60s). ping, not timeout: timeout refuses to run without a console.
	out << "set /a n=0\r\n";
	out << ":wait\r\n";
	out << "tasklist /FI \"PID eq " << pid << "\" | find \"" << pid << "\" >nul\r\n";
	out << "if errorlevel 1 goto wipe\r\n";
	out << "ping -n 2 127.0.0.1 >nul\r\n";
	out << "set /a n+=1\r\n";
	out << "if %n% LSS 60 goto wait\r\n";
	out << ":wipe\r\n";
	// A scanner can hold a file for a moment after the process is gone, so go through the list three times.
	out << "set /a r=0\r\n";
	out << ":purge\r\n";
	for (const auto &path : paths) {
		const auto native = CmdEscape(QDir::toNativeSeparators(path));
		// cmd's "if exist dir\" is also true for a file, so the kind is decided here.
		out << (QFileInfo(path).isDir() ? "rmdir /s /q \"" : "del /f /q \"")
			<< native
			<< "\" >nul 2>&1\r\n";
	}
	out << "set /a r+=1\r\n";
	out << "if %r% GEQ 3 goto done\r\n";
	out << "ping -n 3 127.0.0.1 >nul\r\n";
	out << "goto purge\r\n";
	out << ":done\r\n";
	// The program and profile folders go only if nothing of the user's is left in them.
	for (const auto &folder : foldersIfEmpty) {
		out << "rmdir \"" << CmdEscape(QDir::toNativeSeparators(folder)) << "\" >nul 2>&1\r\n";
	}
	// Inno uninstall registry (HKCU only, no UAC)
	out << "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" << kUninstallKey << "\" /f >nul 2>&1\r\n";
	out << "reg delete \"HKCU\\Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" << kUninstallKey << "\" /f >nul 2>&1\r\n";
	// Self-delete
	out << "del /f /q \"%~f0\"\r\n";
	file.close();
	return script;
}

} // namespace

void schedulePurge(
		const QString &workingDir,
		const QString &exeDir) {
	const auto paths = buildIntentPaths(workingDir, exeDir);
	const auto pid = QCoreApplication::applicationPid();
	const auto script = writeHelperScript(paths, { exeDir, workingDir }, pid);
	if (script.isEmpty()) {
		return;
	}
	auto process = QProcess();
	process.setProgram(QStringLiteral("cmd.exe"));
	process.setNativeArguments(
		QStringLiteral("/D /C \"\"%1\"\"").arg(QDir::toNativeSeparators(script)));
	process.setCreateProcessArgumentsModifier([](
			QProcess::CreateProcessArguments *args) {
		args->flags |= CREATE_NO_WINDOW;
		args->inheritHandles = false; // the helper must not keep this process's open log files alive
	});
	process.startDetached();
}

} // namespace Platform::DuressUninstall
