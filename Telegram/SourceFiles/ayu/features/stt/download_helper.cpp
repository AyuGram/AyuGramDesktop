// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/stt/download_helper.h"

#include "base/debug_log.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Ayu::STT {

void DownloadWithProgress(
		const QString &url,
		const QString &destPath,
		const QString &sha256Expected,
		const std::function<void(int percent)>& onProgress,
		const std::function<void(bool ok)> &onDone) {
	(void)QDir().mkpath(QFileInfo(destPath).absolutePath());

	const auto tmpPath = destPath + u".tmp"_q;
	const auto file = new QFile(tmpPath);
	if (!file->open(QIODevice::WriteOnly)) {
		LOG(("DownloadWithProgress: failed to open temp file: %1").arg(tmpPath));
		file->deleteLater();
		onDone(false);
		return;
	}

	const auto downloader = new QNetworkAccessManager();
	const auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
	auto request = QNetworkRequest(QUrl(url));
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	const auto reply = downloader->get(request);

	QObject::connect(reply, &QNetworkReply::downloadProgress,
		[onProgress](const qint64 received, const qint64 total) {
			if (total > 0) {
				onProgress(static_cast<int>(received * 100 / total));
			}
		});
	QObject::connect(reply, &QNetworkReply::readyRead, [=] {
		const auto chunk = reply->readAll();
		hash->addData(chunk);
		if (file->write(chunk) < 0) {
			reply->abort();
		}
	});
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		file->close();
		file->deleteLater();
		downloader->deleteLater();

		const auto fail = [=] {
			QFile::remove(tmpPath);
			onDone(false);
		};

		if (reply->error() != QNetworkReply::NoError) {
			LOG(("DownloadWithProgress: network error for %1: %2 (http status %3)"
				).arg(url, reply->errorString()
				).arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
			fail();
			reply->deleteLater();
			return;
		}

		if (!sha256Expected.isEmpty()) {
			if (const auto actual = QString::fromUtf8(hash->result().toHex()); actual.compare(sha256Expected, Qt::CaseInsensitive) != 0) {
				LOG(("DownloadWithProgress: sha256 mismatch for %1: expected %2, got %3, size %4"
					).arg(url, sha256Expected, actual
					).arg(QFileInfo(tmpPath).size()));
				fail();
				reply->deleteLater();
				return;
			}
		}

		QFile::remove(destPath);
		QFile::rename(tmpPath, destPath);
		onDone(true);
		reply->deleteLater();
	});
}

} // namespace Ayu::STT
