// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/stt/ggml_backend_manager.h"

#include "ayu/features/stt/download_helper.h"
#include "ayu/libs/json.hpp"
#include "base/debug_log.h"
#include "base/zlib_help.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#if defined(Q_OS_MAC)
#include <sys/xattr.h>
#endif

namespace Ayu::STT {

namespace {

constexpr int kZipEntrySizeLimit = 300 * 1024 * 1024;
constexpr int kMaxDownloadAttempts = 3;
constexpr int kRetryDelayMs = 2000;

QString ShortCommit() {
	return QString::fromUtf8(WHISPER_CPP_COMMIT).left(7);
}

QString ReleaseTag() {
	return u"ggml-backends-"_q + ShortCommit();
}

QString AssetFileName(const GgmlBackendKind kind) {
	const auto commit = ShortCommit();
#if defined(Q_OS_MAC)
	return kind == GgmlBackendKind::Metal
		? u"ggml-metal-macos-universal-"_q + commit + u".zip"_q
		: QString();
#elif defined(Q_OS_WIN)
	switch (kind) {
	case GgmlBackendKind::CUDA: return u"ggml-cuda-windows-x64-"_q + commit + u".zip"_q;
	case GgmlBackendKind::Vulkan: return u"ggml-vulkan-windows-x64-"_q + commit + u".zip"_q;
	case GgmlBackendKind::Metal: return QString();
	}
	return QString();
#else
	switch (kind) {
	case GgmlBackendKind::CUDA: return u"ggml-cuda-linux-x64-"_q + commit + u".zip"_q;
	case GgmlBackendKind::Vulkan: return u"ggml-vulkan-linux-x64-"_q + commit + u".zip"_q;
	case GgmlBackendKind::Metal: return QString();
	}
	return QString();
#endif
}

QString MainModuleFileName(GgmlBackendKind kind) {
#if defined(Q_OS_MAC)
	return kind == GgmlBackendKind::Metal ? u"libggml-metal.so"_q : QString();
#elif defined(Q_OS_WIN)
	switch (kind) {
	case GgmlBackendKind::CUDA: return u"ggml-cuda.dll"_q;
	case GgmlBackendKind::Vulkan: return u"ggml-vulkan.dll"_q;
	case GgmlBackendKind::Metal: return QString();
	}
	return QString();
#else
	switch (kind) {
	case GgmlBackendKind::CUDA: return u"libggml-cuda.so"_q;
	case GgmlBackendKind::Vulkan: return u"libggml-vulkan.so"_q;
	case GgmlBackendKind::Metal: return QString();
	}
	return QString();
#endif
}

QString KindDirectory(const GgmlBackendKind kind) {
	const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	const auto sub = kind == GgmlBackendKind::CUDA ? u"cuda"_q
		: kind == GgmlBackendKind::Vulkan ? u"vulkan"_q : u"metal"_q;
	return base + u"/whisper_gpu/"_q + sub;
}

QString ReleaseBaseUrl() {
	return u"https://github.com/mvsrcdst/AyuGramDesktop"_q
		+ u"/releases/download/"_q + ReleaseTag() + u"/"_q;
}

QString ManifestUrl() {
	return ReleaseBaseUrl() + u"manifest.json"_q;
}

QString AssetUrl(const QString &fileName) {
	return ReleaseBaseUrl() + fileName;
}

bool ExtractZipInto(const QString &zipPath, const QString &destDir) {
	QFile zip(zipPath);
	if (!zip.open(QIODevice::ReadOnly)) {
		return false;
	}
	const auto bytes = zip.readAll();
	zip.close();

	zlib::FileToRead reader(bytes);
	if (reader.goToFirstFile() != UNZ_OK) {
		return false;
	}
	do {
		const auto name = reader.getCurrentFileName();
		if (name.isEmpty()) {
			continue;
		}
		const auto content = reader.readCurrentFileContent(kZipEntrySizeLimit);
		if (content.isEmpty()) {
			continue;
		}
		QFile out(destDir + u"/"_q + name);
		if (out.open(QIODevice::WriteOnly)) {
			out.write(content);
			out.close();
		}
	} while (reader.goToNextFile() == UNZ_OK);
	return true;
}

#if defined(Q_OS_MAC)
void StripQuarantine(const QString &dir) {
	QDirIterator it(dir, QDir::Files);
	while (it.hasNext()) {
		const auto path = it.next().toUtf8();
		removexattr(path.constData(), "com.apple.quarantine", 0);
	}
}
#endif

void DownloadAttempt(
	GgmlBackendKind kind,
	const QString &assetName,
	int attemptsLeft,
	const std::function<void(int percent)>& onProgress,
	const std::function<void(bool ok)>& onDone);

void RetryOrGiveUp(
		const GgmlBackendKind kind,
		const QString &assetName,
		const int attemptsLeft,
		const std::function<void(int percent)>& onProgress,
		const std::function<void(bool ok)>& onDone) {
	if (attemptsLeft > 1) {
		QTimer::singleShot(kRetryDelayMs, [=] {
			DownloadAttempt(kind, assetName, attemptsLeft - 1, onProgress, onDone);
		});
	} else {
		onDone(false);
	}
}

void DownloadAttempt(
		const GgmlBackendKind kind,
		const QString &assetName,
		const int attemptsLeft,
		const std::function<void(int percent)>& onProgress,
		const std::function<void(bool ok)>& onDone) {
	const auto manifestNetwork = new QNetworkAccessManager();
	const auto reply = manifestNetwork->get(QNetworkRequest(QUrl(ManifestUrl())));
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		const auto body = reply->readAll();
		const auto networkError = reply->error();
		reply->deleteLater();
		manifestNetwork->deleteLater();

		if (networkError != QNetworkReply::NoError) {
			LOG(("GgmlBackendManager: manifest fetch failed: %1").arg(ManifestUrl()));
			RetryOrGiveUp(kind, assetName, attemptsLeft, onProgress, onDone);
			return;
		}

		const auto manifest = nlohmann::json::parse(
			body.constData(), body.constData() + body.size(), nullptr, false);
		if (manifest.is_discarded() || !manifest.is_array()) {
			LOG(("GgmlBackendManager: manifest parse failed, body size=%1").arg(body.size()));
			RetryOrGiveUp(kind, assetName, attemptsLeft, onProgress, onDone);
			return;
		}

		auto sha256 = QString();
		for (const auto &entry : manifest) {
			if (entry.value("filename", std::string())
					== assetName.toStdString()) {
				sha256 = QString::fromStdString(
					entry.value("sha256", std::string()));
				break;
			}
		}
		if (sha256.isEmpty()) {
			LOG(("GgmlBackendManager: no manifest entry for asset: %1").arg(assetName));
			RetryOrGiveUp(kind, assetName, attemptsLeft, onProgress, onDone);
			return;
		}

		// Remove any backend downloaded for a previous whisper.cpp commit
		// before pulling the current one - it's dead weight once the
		// pinned commit moves (see backendDirectory()'s comment).
		QDir(KindDirectory(kind)).removeRecursively();
		const auto destDir = GgmlBackendManager::backendDirectory(kind);
		(void)QDir().mkpath(destDir);
		const auto zipPath = destDir + u"/download.zip"_q;

		DownloadWithProgress(
			AssetUrl(assetName),
			zipPath,
			sha256,
			onProgress,
			[=](const bool ok) {
				if (!ok) {
					LOG(("GgmlBackendManager: asset download failed: %1").arg(zipPath));
				}
				auto extracted = false;
				if (ok) {
					if (!ExtractZipInto(zipPath, destDir)) {
						LOG(("GgmlBackendManager: zip extraction failed: %1").arg(zipPath));
					} else {
#if defined(Q_OS_MAC)
						StripQuarantine(destDir);
#endif
						extracted = QFile::exists(
							destDir + u"/"_q + MainModuleFileName(kind));
						if (!extracted) {
							LOG(("GgmlBackendManager: main module missing after extract: %1"
								).arg(destDir + u"/"_q + MainModuleFileName(kind)));
						}
					}
				}
				QFile::remove(zipPath);
				if (!extracted) {
					QDir(destDir).removeRecursively();
				}
				if (extracted) {
					onDone(true);
				} else {
					RetryOrGiveUp(kind, assetName, attemptsLeft, onProgress, onDone);
				}
			});
	});
}

} // namespace

GgmlBackendManager &GgmlBackendManager::instance() {
	static GgmlBackendManager self;
	return self;
}

QString GgmlBackendManager::backendDirectory(const GgmlBackendKind kind) {
	return KindDirectory(kind) + u"/"_q + ShortCommit();
}

QString GgmlBackendManager::loadedModulePath(const GgmlBackendKind kind) {
	const auto moduleName = MainModuleFileName(kind);
	if (moduleName.isEmpty()) {
		return {};
	}
	const auto path = backendDirectory(kind) + u"/"_q + moduleName;
	return QFile::exists(path) ? path : QString();
}

bool GgmlBackendManager::isDownloaded(const GgmlBackendKind kind) {
	return !loadedModulePath(kind).isEmpty();
}

void GgmlBackendManager::download(
		const GgmlBackendKind kind,
		const std::function<void(int percent)> &onProgress,
		const std::function<void(bool ok)> &onDone) {
	const auto assetName = AssetFileName(kind);
	if (assetName.isEmpty()) {
		onDone(false);
		return;
	}
	DownloadAttempt(kind, assetName, kMaxDownloadAttempts, onProgress, onDone);
}

void GgmlBackendManager::remove(const GgmlBackendKind kind) {
	if (auto dir = QDir(KindDirectory(kind)); dir.exists()) {
		dir.removeRecursively();
	}
}

} // namespace Ayu::STT
