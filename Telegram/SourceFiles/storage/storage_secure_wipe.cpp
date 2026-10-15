/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// 1-pass overwrite + unlink helpers (Windows-first retries).
#include "storage/storage_secure_wipe.h"

#include "base/random.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif // Q_OS_WIN

namespace Storage::SecureWipe {
namespace {

[[nodiscard]] QString cleanAbsolute(const QString &path) {
	return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

[[nodiscard]] bool isInsideRoot(const QString &path, const QString &root) {
	if (root.isEmpty()) {
		return true;
	}
	const auto abs = cleanAbsolute(path);
	auto absRoot = cleanAbsolute(root);
	if (!absRoot.endsWith('/')) {
		absRoot += '/';
	}
	return abs == cleanAbsolute(root)
		|| abs.startsWith(absRoot, Qt::CaseInsensitive);
}

void sleepRetry(int delayMs) {
	if (delayMs > 0) {
		QThread::msleep(uint(delayMs));
	}
}

[[nodiscard]] bool fillRandom(QByteArray &buffer) {
	if (buffer.isEmpty()) {
		return true;
	}
	base::RandomFill(buffer.data(), buffer.size());
	return true;
}

#ifdef Q_OS_WIN
[[nodiscard]] bool overwriteFileWin(const QString &path) {
	const auto wpath = QDir::toNativeSeparators(path).toStdWString();
	HANDLE handle = CreateFileW(
		wpath.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_READ,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		nullptr);
	if (handle == INVALID_HANDLE_VALUE) {
		return false;
	}
	LARGE_INTEGER size{};
	if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0) {
		CloseHandle(handle);
		return false;
	}
	auto remaining = size.QuadPart;
	QByteArray chunk(qMin<qint64>(remaining, 1024 * 1024), Qt::Uninitialized);
	while (remaining > 0) {
		const auto toWrite = int(qMin<qint64>(remaining, chunk.size()));
		chunk.resize(toWrite);
		fillRandom(chunk);
		DWORD written = 0;
		if (!WriteFile(
				handle,
				chunk.constData(),
				DWORD(toWrite),
				&written,
				nullptr)
			|| written != DWORD(toWrite)) {
			CloseHandle(handle);
			return false;
		}
		remaining -= written;
	}
	FlushFileBuffers(handle);
	CloseHandle(handle);
	return true;
}

[[nodiscard]] bool deleteFileWin(const QString &path) {
	const auto wpath = QDir::toNativeSeparators(path).toStdWString();
	if (DeleteFileW(wpath.c_str())) {
		return true;
	}
	// Clear read-only and retry once inside caller loop.
	SetFileAttributesW(wpath.c_str(), FILE_ATTRIBUTE_NORMAL);
	return DeleteFileW(wpath.c_str()) != 0;
}
#endif // Q_OS_WIN

[[nodiscard]] bool overwriteFileQt(const QString &path) {
	QFile file(path);
	if (!file.open(QIODevice::ReadWrite)) {
		return false;
	}
	const auto size = file.size();
	if (size < 0) {
		file.close();
		return false;
	}
	if (!file.seek(0)) {
		file.close();
		return false;
	}
	auto remaining = size;
	QByteArray chunk(int(qMin<qint64>(remaining, 1024 * 1024)), Qt::Uninitialized);
	while (remaining > 0) {
		const auto toWrite = int(qMin<qint64>(remaining, chunk.size()));
		chunk.resize(toWrite);
		fillRandom(chunk);
		const auto written = file.write(chunk);
		if (written != toWrite) {
			file.close();
			return false;
		}
		remaining -= written;
	}
	file.flush();
	file.close();
	return true;
}

} // namespace

bool overwriteAndUnlinkFile(
		const QString &path,
		int maxRetries,
		int retryDelayMs) {
	if (path.isEmpty() || !QFileInfo::exists(path) || QFileInfo(path).isDir()) {
		return false;
	}
	for (auto attempt = 0; attempt < maxRetries; ++attempt) {
		if (attempt > 0) {
			sleepRetry(retryDelayMs);
		}
#ifdef Q_OS_WIN
		const auto overwritten = overwriteFileWin(path) || overwriteFileQt(path);
		if (!overwritten) {
			continue;
		}
		if (deleteFileWin(path) || QFile::remove(path)) {
			return true;
		}
#else // Q_OS_WIN
		if (!overwriteFileQt(path)) {
			continue;
		}
		if (QFile::remove(path)) {
			return true;
		}
#endif // !Q_OS_WIN
	}
	return false;
}

bool unlinkFileWithRetries(
		const QString &path,
		int maxRetries,
		int retryDelayMs) {
	if (path.isEmpty()) {
		return false;
	}
	if (!QFileInfo::exists(path)) {
		return true;
	}
	for (auto attempt = 0; attempt < maxRetries; ++attempt) {
		if (attempt > 0) {
			sleepRetry(retryDelayMs);
		}
#ifdef Q_OS_WIN
		if (deleteFileWin(path) || QFile::remove(path)) {
			return true;
		}
#else // Q_OS_WIN
		if (QFile::remove(path)) {
			return true;
		}
#endif // !Q_OS_WIN
	}
	return false;
}

Result wipeFile(const QString &path, const Options &options) {
	Result result;
	if (path.isEmpty()) {
		return result;
	}
	if (!isInsideRoot(path, options.confineRoot)) {
		result.ok = false;
		result.filesSkipped = 1;
		return result;
	}
	const auto info = QFileInfo(path);
	if (!info.exists() || info.isDir()) {
		result.filesSkipped = 1;
		return result;
	}
	// Do not follow symlinks/junctions that escape the tree: skip symlinks.
	if (info.isSymLink()) {
		result.filesSkipped = 1;
		return result;
	}
	result.filesProcessed = 1;
	const auto ok = (options.mode == Mode::UnlinkOnly)
		? unlinkFileWithRetries(path, options.maxRetries, options.retryDelayMs)
		: overwriteAndUnlinkFile(path, options.maxRetries, options.retryDelayMs);
	if (ok) {
		result.filesRemoved = 1;
	} else {
		result.filesSkipped = 1;
	}
	return result;
}

Result wipeDirectory(const QString &path, const Options &options) {
	Result result;
	if (path.isEmpty()) {
		return result;
	}
	if (!isInsideRoot(path, options.confineRoot)) {
		result.ok = false;
		return result;
	}
	QDir dir(path);
	if (!dir.exists()) {
		return result;
	}
	const auto entries = dir.entryInfoList(
		QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
	for (const auto &entry : entries) {
		if (entry.isSymLink()) {
			// Best-effort unlink the link itself without following.
			++result.filesProcessed;
			if (unlinkFileWithRetries(
					entry.absoluteFilePath(),
					options.maxRetries,
					options.retryDelayMs)) {
				++result.filesRemoved;
			} else {
				++result.filesSkipped;
			}
			continue;
		}
		if (entry.isDir()) {
			const auto nested = wipeDirectory(entry.absoluteFilePath(), options);
			result.filesProcessed += nested.filesProcessed;
			result.filesRemoved += nested.filesRemoved;
			result.filesSkipped += nested.filesSkipped;
			result.directoriesRemoved += nested.directoriesRemoved;
			result.ok = result.ok && nested.ok;
		} else {
			const auto one = wipeFile(entry.absoluteFilePath(), options);
			result.filesProcessed += one.filesProcessed;
			result.filesRemoved += one.filesRemoved;
			result.filesSkipped += one.filesSkipped;
			result.ok = result.ok && one.ok;
		}
	}
	if (dir.rmdir(dir.absolutePath())) {
		++result.directoriesRemoved;
	}
	return result;
}

} // namespace Storage::SecureWipe
