/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// Packs a file into a password-protected .7z with 7-Zip once it has been saved with Save As.
#include "ayu/features/zip/save_zip.h"

#include "ayu/ayu_settings.h"
#include "base/timer.h"
#include "core/application.h"
#include "data/data_document.h"
#include "main/main_session.h"
#include "window/window_controller.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>

namespace AyuFeatures::Zip {
namespace {

constexpr auto kPollEach = crl::time(500);
constexpr auto kPollLimit = 2 * 60 * 60 * 2;
constexpr auto kProcessLimit = 10 * 60 * crl::time(1000);

void Toast(const QString &text) {
	if (const auto window = Core::App().activeWindow()) {
		window->showToast(text);
	}
}

[[nodiscard]] QString FreeArchivePath(const QString &file) {
	const auto base = file + QStringLiteral(".7z");
	if (!QFileInfo::exists(base)) {
		return base;
	}
	for (auto i = 2; i != 1000; ++i) {
		const auto candidate = file + QStringLiteral(" (%1).7z").arg(i);
		if (!QFileInfo::exists(candidate)) {
			return candidate;
		}
	}
	return base;
}

struct Job {
	base::Timer timer;
	DocumentData *document = nullptr;
	base::weak_ptr<Main::Session> session;
	QString path;
	QDateTime started = QDateTime::currentDateTime();
	int ticks = 0;
};

} // namespace

QString FindSevenZip() {
	auto candidates = QStringList();
#ifdef Q_OS_WIN
	for (const auto &name : { "ProgramFiles", "ProgramW6432", "ProgramFiles(x86)" }) {
		const auto base = qEnvironmentVariable(name);
		if (!base.isEmpty()) {
			candidates.push_back(base + QStringLiteral("/7-Zip/7z.exe"));
		}
	}
	for (const auto &key : {
		QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\7-Zip"),
		QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\7-Zip"),
	}) {
		const auto settings = QSettings(key, QSettings::NativeFormat);
		for (const auto &value : { "Path64", "Path" }) {
			const auto folder = settings.value(QLatin1String(value)).toString();
			if (!folder.isEmpty()) {
				candidates.push_back(folder + QStringLiteral("/7z.exe"));
			}
		}
	}
#endif // Q_OS_WIN
	for (const auto &path : candidates) {
		if (QFileInfo(path).isFile()) {
			return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
		}
	}
	// Only the program itself: on Windows a 7z.cmd or 7z.bat shim would go through cmd.exe.
	for (const auto &name : {
#ifdef Q_OS_WIN
		"7z.exe",
#else // Q_OS_WIN
		"7z", "7zz", "7za",
#endif // !Q_OS_WIN
	}) {
		const auto found = QStandardPaths::findExecutable(QLatin1String(name));
		if (!found.isEmpty()) {
			return found;
		}
	}
	return {};
}

bool Enabled() {
	const auto &settings = AyuSettings::getInstance();
	return settings.zipOnSave() && !settings.zipPassword().isEmpty();
}

void Archive(
		const QString &file,
		Fn<void(QString archive, QString error)> done) {
	const auto program = FindSevenZip();
	const auto password = AyuSettings::getInstance().zipPassword().toUtf8();
	if (program.isEmpty()) {
		done({}, QStringLiteral("7-Zip is not installed."));
		return;
	} else if (password.isEmpty()) {
		done({}, QStringLiteral("Set the zip password in the settings first."));
		return;
	}
	const auto info = QFileInfo(file);
	if (!info.isFile()) {
		done({}, QStringLiteral("The saved file is not there."));
		return;
	}
	const auto archive = FreeArchivePath(info.absoluteFilePath());
	const auto process = new QProcess();
	process->setWorkingDirectory(info.absolutePath());
	process->setProgram(program);
	// The password goes in on standard input, never on the command line where other programs could read it.
	process->setArguments({
		QStringLiteral("a"),
		QStringLiteral("-t7z"),
		QStringLiteral("-mx=5"),
		QStringLiteral("-mhe=on"),
		QStringLiteral("-y"),
		QStringLiteral("-bd"),
		QStringLiteral("-p"),
		QStringLiteral("--"),
		QDir::toNativeSeparators(archive),
		QDir::toNativeSeparators(info.absoluteFilePath()),
	});
	const auto timeout = new base::Timer([=] {
		process->kill();
	});
	QObject::connect(
		process,
		&QProcess::errorOccurred,
		process,
		[=](QProcess::ProcessError error) {
			if (error == QProcess::FailedToStart) {
				timeout->cancel();
				crl::on_main([=] { delete timeout; });
				process->deleteLater();
				done({}, QStringLiteral("7-Zip could not be started."));
			}
		});
	QObject::connect(
		process,
		QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		process,
		[=](int code, QProcess::ExitStatus status) {
			timeout->cancel();
			crl::on_main([=] { delete timeout; });
			const auto ok = (status == QProcess::NormalExit)
				&& (code == 0)
				&& QFileInfo(archive).size() > 0;
			process->deleteLater();
			if (ok && QFile::remove(info.absoluteFilePath())) {
				done(archive, {});
			} else if (ok) {
				done(archive, QStringLiteral("Packed, but the plain file could not be removed."));
			} else {
				QFile::remove(archive);
				done({}, QStringLiteral("7-Zip failed (code %1).").arg(code));
			}
		});
	process->start();
	timeout->callOnce(kProcessLimit);
	process->write(password + "\n" + password + "\n");
	process->closeWriteChannel();
}

void ArchiveAfterSave(not_null<DocumentData*> document, const QString &path) {
	if (!Enabled() || path.isEmpty()) {
		return;
	}
	if (FindSevenZip().isEmpty()) {
		Toast(QStringLiteral("7-Zip is not installed, so the file was saved as it is."));
		return;
	}
	const auto job = new Job();
	job->document = document;
	job->session = base::make_weak(&document->session());
	job->path = path;
	job->timer.setCallback([=] {
		++job->ticks;
		if (!job->session) {
			job->timer.cancel();
			crl::on_main([=] { delete job; });
			return;
		}
		const auto loading = job->document->loading();
		if (loading && job->ticks < kPollLimit) {
			return;
		}
		job->timer.cancel();
		const auto file = job->path;
		// A canceled or failed download leaves the old file (if any) and a timeout leaves a partial one.
		const auto info = QFileInfo(file);
		const auto fresh = !loading
			&& info.isFile()
			&& (info.lastModified() >= job->started.addSecs(-2));
		crl::on_main([=] { delete job; });
		if (!fresh) {
			return;
		}
		Archive(file, [](QString archive, QString error) {
			Toast(error.isEmpty()
				? QStringLiteral("Saved as %1").arg(QFileInfo(archive).fileName())
				: error);
		});
	});
	job->timer.callEach(kPollEach);
}

} // namespace AyuFeatures::Zip
