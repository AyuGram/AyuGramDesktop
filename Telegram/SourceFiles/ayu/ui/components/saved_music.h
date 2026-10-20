// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ui/widgets/buttons.h"
#include "base/timer.h"

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace Ui::Text {
class CustomEmoji;
} // namespace Ui::Text

namespace Data {
class DocumentMedia;
}

namespace Media::Audio {
class Track;
}

namespace Info::Profile {

struct MusicButtonData;

struct ResultCover
{
	QPixmap pix;
	QColor bg;
	QColor accent;
	bool noCover = false;
};

class AyuMusicButton final : public Ui::RippleButton
{
public:
	AyuMusicButton(QWidget *parent, MusicButtonData data, std::optional<QColor> overrideBg, Fn<void()> handler);
	~AyuMusicButton();

	void updateData(MusicButtonData data);
	[[nodiscard]] bool isLiveTrack() const {
		return _isLiveTrack;
	}

	rpl::producer<> onReady() const {
		return _onReady.events();
	}

private:
	void downloadAndMakeCover(FullMsgId msgId);
	void makeCover();
	void setupPlayerTracking();
	void togglePlay();
	[[nodiscard]] bool isCurrentTrackPlaying() const;
	[[nodiscard]] bool hasPlayButton() const;
	[[nodiscard]] QRect playButtonRect(int customWidth = 0) const;
	void paintPlayButton(Painter &p);
	void preparePatternImage();

	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

	Ui::FlatLabel *_performer = nullptr;
	Ui::FlatLabel *_title = nullptr;
	Ui::FlatLabel *_album = nullptr;
	std::shared_ptr<Data::DocumentMedia> _mediaView;
	std::optional<ResultCover> _currentCover;
	rpl::event_stream<> _onReady;

	std::unique_ptr<Ui::Text::CustomEmoji> _patternEmoji;
	QImage _patternImage;

	QString _performerText;
	QString _titleText;
	QString _albumText;
	FullMsgId _msgId;

	QString _songUrl;
	QString _previewUrl;
	QString _coverUrl;
	QString _platform;
	bool _isLiveTrack = false;
	std::unique_ptr<::Media::Audio::Track> _previewTrack;
	base::Timer _previewTrackTimer;
	QByteArray _previewBytes;
	bool _previewDownloading = false;

	std::optional<QColor> _overrideBg;
	bool _playButtonPressed = false;
	bool _playButtonHovered = false;

};

} // namespace Info::Profile
