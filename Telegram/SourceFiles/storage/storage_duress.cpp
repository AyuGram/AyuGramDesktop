/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// duress password classify / wipe / purge orchestration.
#include "storage/storage_duress.h"

#include "storage/storage_duress_inventory.h"
#include "storage/storage_domain.h"
#include "storage/storage_secure_wipe.h"
#include "storage/details/storage_file_utilities.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/data_folder.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "mtproto/mtp_instance.h"
#include "base/random.h"

#include <gsl/gsl>
#include "settings.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QThread>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>

#include <algorithm>
#include <cstdlib>

#ifdef Q_OS_WIN
#include "platform/win/duress_uninstall_win.h"
#endif // Q_OS_WIN

namespace Storage::Duress {
namespace {

using namespace details;

constexpr auto kMagic = "AGDV"; // AyuGram Duress Verifier
constexpr auto kSaltSize = LocalEncryptSaltSize;

struct VerifierBlob {
	QByteArray salt;
	QByteArray keyBytes; // MTP::AuthKey::kSize
};

[[nodiscard]] QByteArray keyBytesOf(const MTP::AuthKeyPtr &key) {
	if (!key) {
		return {};
	}
	const auto span = key->data();
	return QByteArray(
		reinterpret_cast<const char*>(span.data()),
		int(span.size()));
}

[[nodiscard]] std::optional<VerifierBlob> readVerifier() {
	QFile file(verifierPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return std::nullopt;
	}
	const auto raw = file.readAll();
	file.close();
	if (raw.size() < 4 + kSaltSize + MTP::AuthKey::kSize) {
		return std::nullopt;
	}
	if (raw.left(4) != QByteArray(kMagic)) {
		return std::nullopt;
	}
	VerifierBlob blob;
	blob.salt = raw.mid(4, kSaltSize);
	blob.keyBytes = raw.mid(4 + kSaltSize, MTP::AuthKey::kSize);
	if (blob.salt.size() != kSaltSize
		|| blob.keyBytes.size() != MTP::AuthKey::kSize) {
		return std::nullopt;
	}
	return blob;
}

[[nodiscard]] bool writeVerifier(const VerifierBlob &blob) {
	QDir().mkpath(QFileInfo(verifierPath()).absolutePath());
	QByteArray raw;
	raw.reserve(4 + kSaltSize + MTP::AuthKey::kSize);
	raw.append(kMagic);
	raw.append(blob.salt);
	raw.append(blob.keyBytes);
	QFile file(verifierPath());
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	file.write(raw);
	file.close();
	return true;
}

void Accumulate(
		Storage::SecureWipe::Result &total,
		const Storage::SecureWipe::Result &part) {
	total.filesProcessed += part.filesProcessed;
	total.filesRemoved += part.filesRemoved;
	total.filesSkipped += part.filesSkipped;
	total.directoriesRemoved += part.directoriesRemoved;
	total.ok = total.ok && part.ok;
}

// The keys and settings sit directly in tdata, ahead of the slow cache folders.
[[nodiscard]] Storage::SecureWipe::Result WipeTopLevelFiles(
		const QString &tdata) {
	auto options = Storage::SecureWipe::Options();
	options.confineRoot = tdata;
	auto result = Storage::SecureWipe::Result();
	const auto files = QDir(tdata).entryInfoList(
		QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks);
	for (const auto &file : files) {
		Accumulate(
			result,
			Storage::SecureWipe::wipeFile(file.absoluteFilePath(), options));
	}
	return result;
}

// Starts the program the owner chose, detached, so it outlives this process.
void LaunchChosenProgram(const QString &path) {
	if (path.isEmpty()) {
		return;
	}
	const auto info = QFileInfo(path);
	const auto good = info.isAbsolute()
		&& info.isFile()
#ifdef Q_OS_WIN
		&& (info.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0);
#else // Q_OS_WIN
		&& info.isExecutable();
#endif // !Q_OS_WIN
	if (!good) {
		return;
	}
	auto process = QProcess();
	process.setProgram(info.absoluteFilePath());
	process.setWorkingDirectory(info.absolutePath());
#ifdef Q_OS_WIN
	process.setCreateProcessArgumentsModifier([](
			QProcess::CreateProcessArguments *args) {
		args->inheritHandles = false;
	});
#endif // Q_OS_WIN
	process.startDetached();
}

} // namespace

QString verifierPath() {
	return cWorkingDir() + u"tdata/"_q + QString::fromLatin1(kVerifierFileName);
}

bool isConfigured() {
	return readVerifier().has_value();
}

bool setDuressPasscode(
		const QByteArray &realPasscode,
		const QByteArray &duressPasscode) {
	if (duressPasscode.isEmpty()
		|| duressPasscode == realPasscode) {
		return false;
	}
	if (!Core::App().domain().started()
		|| !Core::App().domain().local().checkPasscode(realPasscode)) {
		return false;
	}
	VerifierBlob blob;
	blob.salt = QByteArray(kSaltSize, Qt::Uninitialized);
	base::RandomFill(blob.salt.data(), blob.salt.size());
	const auto key = CreateLocalKey(duressPasscode, blob.salt);
	blob.keyBytes = keyBytesOf(key);
	return writeVerifier(blob);
}

bool clearDuressPasscode(const QByteArray &realPasscode) {
	if (!Core::App().domain().started()
		|| !Core::App().domain().local().checkPasscode(realPasscode)) {
		return false;
	}
	const auto path = verifierPath();
	if (!QFile::exists(path)) {
		return true;
	}
	return Storage::SecureWipe::overwriteAndUnlinkFile(path);
}

bool matchesDuress(const QByteArray &utf8Passcode) {
	if (utf8Passcode.isEmpty()) {
		return false;
	}
	const auto blob = readVerifier();
	if (!blob) {
		return false;
	}
	const auto key = CreateLocalKey(utf8Passcode, blob->salt);
	MTP::AuthKey::Data data{ { gsl::byte{} } };
	if (blob->keyBytes.size() != int(data.size())) {
		return false;
	}
	memcpy(data.data(), blob->keyBytes.constData(), data.size());
	const auto stored = std::make_shared<MTP::AuthKey>(data);
	return key->equals(stored);
}

PasscodeKind classifyPasscode(const QByteArray &utf8Passcode) {
	if (utf8Passcode.isEmpty()) {
		return PasscodeKind::Empty;
	}
	auto &domain = Core::App().domain();
	if (domain.started()) {
		if (domain.local().checkPasscode(utf8Passcode)) {
			return PasscodeKind::Real;
		}
	} else {
		const auto result = domain.start(utf8Passcode);
		if (result == Storage::StartResult::Success) {
			return PasscodeKind::Real;
		}
	}
	if (matchesDuress(utf8Passcode)) {
		return PasscodeKind::Duress;
	}
	return PasscodeKind::Wrong;
}

void bestEffortWarmLogout() {
	auto &domain = Core::App().domain();
	if (!domain.started()) {
		// Cold lock-screen: skip server logout (owner decision D1).
		return;
	}
	QElapsedTimer timer;
	timer.start();
	for (const auto &item : domain.accounts()) {
		if (timer.elapsed() >= kLogoutTimeoutMs) {
			break;
		}
		if (item.account) {
			item.account->logOut();
		}
	}
	// Give logout requests a short, bounded chance to leave the socket.
	while (timer.elapsed() < kLogoutTimeoutMs) {
		QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
		if (timer.elapsed() >= 1000) {
			break;
		}
	}
}

void runDuressSequence() {
	const auto chosenProgram = Core::DataFolder::readSidecar().duressProgram;
	if (Core::IsAppLaunched()) {
		bestEffortWarmLogout();
	}

	const auto workingDir = cWorkingDir();
	const auto exeDir = cExeDir();
	QString downloadPath;
	if (Core::IsAppLaunched()) {
		downloadPath = Core::App().settings().downloadPath();
	}

	const auto roots = DuressInventory::collectWipeRoots(
		workingDir,
		exeDir,
		downloadPath,
		{});

	// Crypto-erase first: without the keys the caches are unreadable even if this is cut short.
	auto total = Storage::SecureWipe::Result();
	Accumulate(total, WipeTopLevelFiles(workingDir + u"tdata/"_q));

	for (const auto &root : roots) {
		// Binaries of the running process are removed by the purge helper
		// after exit; overwriting them in-process would fail on Windows.
		if (root.category == DuressInventory::Category::InstallDir) {
			continue;
		}
		Storage::SecureWipe::Options opts;
		opts.mode = root.overwrite
			? Storage::SecureWipe::Mode::OverwriteThenUnlink
			: Storage::SecureWipe::Mode::UnlinkOnly;
		opts.maxRetries = 3;
		opts.retryDelayMs = 100;
		opts.confineRoot = root.confineRoot;
		const auto info = QFileInfo(root.path);
		if (info.isDir()) {
			Accumulate(
				total,
				Storage::SecureWipe::wipeDirectory(root.path, opts));
		} else if (info.exists()) {
			Accumulate(
				total,
				Storage::SecureWipe::wipeFile(root.path, opts));
		}
	}
	LaunchChosenProgram(chosenProgram);

#ifdef Q_OS_WIN
	Platform::DuressUninstall::schedulePurge(workingDir, exeDir);
#endif // Q_OS_WIN
	// Exit immediately: a normal shutdown would write settings/maps back
	// into the freshly wiped tdata.
	std::_Exit(0);
}

} // namespace Storage::Duress
