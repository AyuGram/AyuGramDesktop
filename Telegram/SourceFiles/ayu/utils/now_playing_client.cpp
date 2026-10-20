// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/utils/now_playing_client.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "ayu/ayu_settings.h"
#include "base/assertion.h"
#include "crl/crl_async.h"
#include "crl/crl_on_main.h"
#include "crl/crl_time.h"

namespace crl {

inline bool is_main_thread() {
	return QThread::currentThread() == QCoreApplication::instance()->thread();
}

} // namespace crl

namespace Ayu::NowPlaying {
namespace {

struct CacheEntry {
	std::optional<Track> track;
	crl::time time = 0;
};

QMap<uint64, CacheEntry> gCache;
QMap<uint64, std::vector<Fn<void(std::optional<Track>)>>> gPendingCallbacks;

} // namespace

QByteArray DownloadBytes(const QUrl &url, int timeoutMs) {
	if (!url.isValid()) {
		return {};
	}
	thread_local QNetworkAccessManager nam;
	QNetworkRequest req(url);
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	req.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
	auto reply = nam.get(req);
	QEventLoop loop;
	QTimer timer;
	timer.setSingleShot(true);
	QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	timer.start(timeoutMs);
	loop.exec();
	if (timer.isActive()) {
		timer.stop();
	} else {
		reply->abort();
	}
	const auto result = (reply->error() == QNetworkReply::NoError)
		? reply->readAll()
		: QByteArray();
	reply->deleteLater();
	return result;
}

void Fetch(uint64 userId, Fn<void(std::optional<Track>)> callback, bool force) {
	Assert(crl::is_main_thread());
	if (!userId) {
		callback(std::nullopt);
		return;
	}

	if (!force) {
		const auto it = gCache.find(userId);
		if (it != gCache.end() && (crl::now() - it->time < 15000)) {
			callback(it->track);
			return;
		}
	}

	const auto pendingIt = gPendingCallbacks.find(userId);
	if (pendingIt != gPendingCallbacks.end()) {
		pendingIt->push_back(std::move(callback));
		return;
	}

	gPendingCallbacks[userId].push_back(std::move(callback));

	crl::async([userId] {
		const auto url = QUrl(u"https://api.exteragram.app/api/v1/profiles/%1/now-playing"_q.arg(userId));
		const auto json = DownloadBytes(url, 2500);
		auto track = std::optional<Track>();
		if (!json.isEmpty()) {
			const auto doc = QJsonDocument::fromJson(json);
			if (doc.isObject()) {
				const auto obj = doc.object();
				const auto isPlaying = obj.value(u"isPlaying"_q).toBool(false);
				const auto trackName = obj.value(u"trackName"_q).toString();
				if (isPlaying && !trackName.trimmed().isEmpty()) {
					Track t;
					t.title = trackName.trimmed();
					const auto artistsVal = obj.value(u"artists"_q);
					if (artistsVal.isArray()) {
						QStringList list;
						for (const auto &a : artistsVal.toArray()) {
							list.append(a.toString());
						}
						t.performer = list.join(u", "_q);
					} else {
						t.performer = artistsVal.toString();
					}
					t.album = obj.value(u"albumName"_q).toString();
					t.coverUrl = obj.value(u"coverUrl"_q).toString();
					t.previewUrl = obj.value(u"previewUrl"_q).toString();
					t.songUrl = obj.value(u"songUrl"_q).toString();
					t.platform = obj.value(u"platform"_q).toString();
					t.deviceName = obj.value(u"deviceName"_q).toString();
					t.isPlaying = true;
					track = t;
				}
			}
		}

		crl::on_main([userId, track = std::move(track)] {
			gCache[userId] = { track, crl::now() };
			auto callbacks = std::move(gPendingCallbacks[userId]);
			gPendingCallbacks.remove(userId);
			for (const auto &cb : callbacks) {
				cb(track);
			}
		});
	});
}

void FetchProfile(uint64 userId, Fn<void(NowPlayingServiceType, QString)> callback) {
	if (!userId) {
		callback(NowPlayingServiceType::None, QString());
		return;
	}

	crl::async([userId, callback = std::move(callback)] {
		const auto url = QUrl(u"https://api.exteragram.app/api/v1/profiles/%1"_q.arg(userId));
		const auto json = DownloadBytes(url, 3000);
		auto service = NowPlayingServiceType::None;
		auto username = QString();

		if (!json.isEmpty()) {
			const auto doc = QJsonDocument::fromJson(json);
			if (doc.isObject()) {
				const auto obj = doc.object();
				const auto npVal = obj.value(u"nowPlaying"_q);
				if (npVal.isObject()) {
					const auto npObj = npVal.toObject();
					const auto serviceStr = npObj.value(u"serviceType"_q).toString();
					username = npObj.value(u"username"_q).toString();
					if (serviceStr.compare(u"LAST_FM"_q, Qt::CaseInsensitive) == 0) {
						service = NowPlayingServiceType::LastFm;
					} else if (serviceStr.compare(u"STATS_FM"_q, Qt::CaseInsensitive) == 0) {
						service = NowPlayingServiceType::StatsFm;
					}
				}
			}
		}

		crl::on_main([service, username = std::move(username), callback = std::move(callback)] {
			callback(service, username);
		});
	});
}

} // namespace Ayu::NowPlaying
