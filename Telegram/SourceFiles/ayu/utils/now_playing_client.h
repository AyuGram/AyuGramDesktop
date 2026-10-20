// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <QtCore/QString>
#include <QtCore/QUrl>
#include <optional>

enum class NowPlayingServiceType;

namespace Ayu::NowPlaying {

struct Track {
	QString title;
	QString performer;
	QString album;
	QString coverUrl;
	QString previewUrl;
	QString songUrl;
	QString platform;
	QString deviceName;
	bool isPlaying = false;

	friend bool operator==(const Track &, const Track &) = default;
};

void Fetch(uint64 userId, Fn<void(std::optional<Track>)> callback, bool force = false);
void FetchProfile(uint64 userId, Fn<void(NowPlayingServiceType, QString)> callback);
QByteArray DownloadBytes(const QUrl &url, int timeoutMs = 3000);

} // namespace Ayu::NowPlaying
