// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/components/saved_music.h"

#include "ayu/ayu_settings.h"
#include "ayu/ui/utils/color_utils.h"
#include "ayu/ui/utils/itunes_search.h"
#include "ayu/ui/utils/palette.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "info/profile/info_profile_music_button.h"
#include "lang_auto.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "media/audio/media_audio.h"
#include "media/player/media_player_instance.h"
#include "styles/palette.h"
#include "styles/style_info.h"
#include "ui/image/image.h"
#include "ui/painter.h"
#include "ui/text/custom_emoji_instance.h"
#include "ui/ui_utility.h"
#include "ui/widgets/labels.h"
#include "window/themes/window_theme.h"

#include <QtGui/QPainterPath>
#include <QSvgRenderer>

#include "ayu/utils/now_playing_client.h"
#include "media/audio/media_audio_track.h"
#include "core/file_utilities.h"

namespace Info::Profile {

namespace {

constexpr auto performerColor = QColor(255, 255, 255, 160);
constexpr auto albumColor = QColor(255, 255, 255, 140);

constexpr DocumentId kSpotifyPatternEmojiId = 5271857023359681001ULL;
constexpr DocumentId kMusicPatternEmojiId = 5271627010681108586ULL;
constexpr DocumentId kTelegramPatternEmojiId = 5325674462522144646ULL;

struct PatternPoint {
	float xOffset;
	float yOffset;
	float size;
	float alpha;
};

constexpr PatternPoint kNowPlayingPattern[] = {
	{ -5.5f,   20.0f, 20.0f, 0.35f },
	{ -5.5f,  -20.0f, 20.0f, 0.35f },
	{ -36.0f, -42.0f, 22.0f, 0.375f },
	{ -36.0f,   0.0f, 25.0f, 0.425f },
	{ -36.0f,  42.0f, 22.0f, 0.375f },
	{ -70.0f,  22.0f, 23.0f, 0.35f },
	{ -70.0f, -22.0f, 23.0f, 0.35f },
	{ -99.0f,  46.0f, 21.0f, 0.275f },
	{ -99.0f,   0.0f, 22.0f, 0.325f },
	{ -99.0f, -46.0f, 21.0f, 0.275f },
	{ -128.0f,-23.0f, 20.0f, 0.225f },
	{ -128.0f, 23.0f, 20.0f, 0.225f },
};

QRgb AdjustHsl(QRgb color, float luminance, float saturation = -1.0f) {
	auto hsl = Ayu::Ui::ColorUtils::colorToHSL(color);

	if (saturation > 0.0f) {
		hsl[1] = std::min(hsl[1] * saturation, 1.0f);
	}

	hsl[2] = std::min(hsl[2] * luminance, 1.0f);

	return Ayu::Ui::ColorUtils::HSLToRGB(hsl);
}

QRgb BlendARGB(QRgb color1, QRgb color2, float ratio) {
	const auto inverseRatio = 1.0f - ratio;
	const auto r = static_cast<int>(qRed(color1) * inverseRatio + qRed(color2) * ratio);
	const auto g = static_cast<int>(qGreen(color1) * inverseRatio + qGreen(color2) * ratio);
	const auto b = static_cast<int>(qBlue(color1) * inverseRatio + qBlue(color2) * ratio);
	const auto a = static_cast<int>(qAlpha(color1) * inverseRatio + qAlpha(color2) * ratio);
	return qRgba(r, g, b, a);
}

QColor GetNoCoverBgColor(std::optional<QColor> overrideBg) {
	if (overrideBg) {
		return Ui::BlendColors(
			*overrideBg,
			Qt::black,
			st::infoProfileTopBarActionButtonBgOpacity);
	}

	return st::shadowFg->c;
}

struct Cover {
	QPixmap pixToDraw;
	QPixmap pixToBg;
	bool noCover = false;
};

QPixmap MakeNoCoverImage(const QSize &size) {
	static auto result = QPixmap();
	static auto resultTheme = -1;
	const auto currentTheme = Window::Theme::Background()->id();
	if (!result.isNull() && result.size() == size && resultTheme == currentTheme) {
		return result;
	}
	resultTheme = currentTheme;

	auto image = QImage(size, QImage::Format_ARGB32);
	{
		auto p = Painter(&image);
		auto hq = PainterHighQualityEnabler(p);

		const auto bgColor = Window::Theme::IsNightMode()
			? st::windowBoldFg->c.darker()
			: st::windowBoldFg->c.lighter();
		p.fillRect(QRect(QPoint(), size), bgColor);

		auto svg = QSvgRenderer(u":/gui/icons/settings/music.svg"_q);
		const auto svgSize = size / 2;
		svg.render(&p, QRect((size.width() - svgSize.width()) / 2,
							 (size.height() - svgSize.height()) / 2,
							 svgSize.width(),
							 svgSize.height()));
	}

	result = QPixmap::fromImage(image);
	return result;
}

Cover GetCurrentCover(const std::shared_ptr<Data::DocumentMedia> &mediaView, const QSize &size) {
	if (!mediaView) {
		return { MakeNoCoverImage(size), QPixmap(), true };
	}

	const auto thumbnail = mediaView->thumbnail();
	if (!thumbnail) {
		return { MakeNoCoverImage(size), QPixmap(), true };
	}

	const auto args = Images::PrepareArgs{
		.options = Images::Option::RoundSmall,
		.outer = size,
	};

	return { thumbnail->pix(size, args), thumbnail->pix(size), false };
}

struct Colors {
	QColor bg;
	QColor accent;
};

Colors ExtractColorsFromCover(const QImage &cover, const QColor &fallbackBg, const QColor &fallbackAccent) {
	if (cover.isNull()) {
		return { fallbackBg, fallbackAccent };
	}

	const auto palette = Ayu::Ui::Palette::from(cover).generate();

	const auto *darkVibrant = palette.darkVibrantSwatch();
	const auto *muted = palette.mutedSwatch();
	const auto *dominant = palette.dominantSwatch();
	const auto *darkMuted = palette.darkMutedSwatch();
	const auto *vibrant = palette.vibrantSwatch();
	const auto *lightVibrant = palette.lightVibrantSwatch();

	const Ayu::Ui::Swatch *swatch = nullptr;
	if (darkVibrant) {
		swatch = darkVibrant;
	} else if (muted) {
		swatch = muted;
	} else if (dominant) {
		swatch = dominant;
	} else if (darkMuted) {
		swatch = darkMuted;
	} else if (vibrant) {
		swatch = vibrant;
	} else if (lightVibrant) {
		swatch = lightVibrant;
	}

	auto bg = fallbackBg;
	if (swatch) {
		const auto extractedColor = swatch->rgb();
		constexpr auto whiteColor = qRgb(255, 255, 255);
		const auto contrast = Ayu::Ui::ColorUtils::calculateContrast(whiteColor, extractedColor);

		auto adjustedColor = extractedColor;
		if (contrast > 15.0f) {
			adjustedColor = AdjustHsl(extractedColor, 2.0f);
		} else if (contrast < 10.0f) {
			adjustedColor = AdjustHsl(extractedColor, 0.5f);
		}

		if (Ayu::Ui::ColorUtils::calculateContrast(whiteColor, adjustedColor) < 3.0f) {
			adjustedColor = BlendARGB(adjustedColor, qRgb(0, 0, 0), 0.3f);
		}
		bg = QColor::fromRgb(adjustedColor);
	}

	// WHY: Android darkens the card background to maintain high contrast with white text and match dark theme.
	auto bgHsl = Ayu::Ui::ColorUtils::colorToHSL(bg.rgb());
	bgHsl[1] = std::clamp(bgHsl[1], 0.35f, 0.60f);
	if (bgHsl[2] > 0.20f) {
		bgHsl[2] = 0.18f;
	} else if (bgHsl[2] < 0.12f) {
		bgHsl[2] = 0.15f;
	}
	bg = QColor::fromRgb(Ayu::Ui::ColorUtils::HSLToRGB(bgHsl));

	const auto accent = QColor::fromRgb(AdjustHsl(bg.rgb(), 2.5f));
	return { bg, accent };
}

Colors ExtractColorsFromCover(const QPixmap &cover, const QColor &fallbackBg, const QColor &fallbackAccent) {
	return ExtractColorsFromCover(cover.toImage(), fallbackBg, fallbackAccent);
}

} // namespace

AyuMusicButton::AyuMusicButton(
	QWidget *parent,
	MusicButtonData data,
	std::optional<QColor> overrideBg,
	Fn<void()> handler)
: RippleButton(parent, st::infoMusicButtonRipple)
, _performer(Ui::CreateChild<Ui::FlatLabel>(
	this,
	data.performer,
	st::infoMusicButtonPerformer))
, _title(Ui::CreateChild<Ui::FlatLabel>(
	this,
	data.title,
	st::infoMusicButtonTitle))
, _album(Ui::CreateChild<Ui::FlatLabel>(
	this,
	data.album,
	st::infoMusicButtonPerformer))
, _mediaView(data.mediaView)
, _performerText(data.performer)
, _titleText(data.title)
, _albumText(data.album)
, _msgId(data.msgId)
, _songUrl(data.songUrl)
, _previewUrl(data.previewUrl)
, _coverUrl(data.coverUrl)
, _platform(data.platform)
, _isLiveTrack(data.isLiveTrack)
, _overrideBg(overrideBg) {
	if (data.album.isEmpty()) {
		_album->hide();
	}
	if (data.session) {
		DocumentId id = 0;
		if (data.isLiveTrack) {
			if (data.platform.compare(u"SPOTIFY"_q, Qt::CaseInsensitive) == 0) {
				id = kSpotifyPatternEmojiId;
			} else if (data.platform.compare(u"TELEGRAM"_q, Qt::CaseInsensitive) == 0) {
				id = data.patternEmojiId
					? data.patternEmojiId
					: (data.isPremium ? kMusicPatternEmojiId : kTelegramPatternEmojiId);
			} else {
				id = data.patternEmojiId
					? data.patternEmojiId
					: (data.isPremium ? kMusicPatternEmojiId : kTelegramPatternEmojiId);
			}
		} else {
			id = data.patternEmojiId
				? data.patternEmojiId
				: (data.isPremium ? kMusicPatternEmojiId : kTelegramPatternEmojiId);
		}
		_patternEmoji = data.session->data().customEmojiManager().create(
			id,
			[=] {
				_patternImage = QImage();
				update();
			});
	}

	rpl::combine(
		_title->naturalWidthValue(),
		_performer->naturalWidthValue(),
		_album->naturalWidthValue()
	) | rpl::on_next([=] {
		resizeToWidth(widthNoMargins());
	}, lifetime());

	_title->setAttribute(Qt::WA_TransparentForMouseEvents);
	_performer->setAttribute(Qt::WA_TransparentForMouseEvents);
	_album->setAttribute(Qt::WA_TransparentForMouseEvents);

	if (hasPlayButton()) {
		setupPlayerTracking();
	}
	downloadAndMakeCover(data.msgId);

	style::PaletteChanged(
	) | rpl::on_next([=] {
		_patternImage = QImage();
		makeCover();
	}, lifetime());

	setClickedCallback(std::move(handler));
}

AyuMusicButton::~AyuMusicButton() = default;

bool AyuMusicButton::hasPlayButton() const {
	return !_previewUrl.isEmpty()
		&& !_platform.isEmpty()
		&& (_platform.compare(u"TELEGRAM"_q, Qt::CaseInsensitive) != 0);
}

void AyuMusicButton::updateData(MusicButtonData data) {
	_performer->setText(data.performer);
	_title->setText(data.title);
	if (!data.album.isEmpty()) {
		_album->setText(data.album);
		_album->show();
	} else {
		_album->hide();
	}
	_performerText = data.performer;
	_titleText = data.title;
	_albumText = data.album;
	_mediaView = data.mediaView;
	_msgId = data.msgId;
	_songUrl = data.songUrl;
	_previewUrl = data.previewUrl;
	_coverUrl = data.coverUrl;
	_platform = data.platform;
	_isLiveTrack = data.isLiveTrack;
	_previewTrack.reset();
	_previewBytes.clear();
	_previewDownloading = false;
	_previewTrackTimer.cancel();

	if (data.session) {
		DocumentId id = 0;
		if (data.isLiveTrack) {
			if (data.platform.compare(u"SPOTIFY"_q, Qt::CaseInsensitive) == 0) {
				id = kSpotifyPatternEmojiId;
			} else if (data.platform.compare(u"TELEGRAM"_q, Qt::CaseInsensitive) == 0) {
				id = data.patternEmojiId
					? data.patternEmojiId
					: (data.isPremium ? kMusicPatternEmojiId : kTelegramPatternEmojiId);
			} else {
				id = data.patternEmojiId
					? data.patternEmojiId
					: (data.isPremium ? kMusicPatternEmojiId : kTelegramPatternEmojiId);
			}
		} else {
			id = data.patternEmojiId
				? data.patternEmojiId
				: (data.isPremium ? kMusicPatternEmojiId : kTelegramPatternEmojiId);
		}
		_patternEmoji = data.session->data().customEmojiManager().create(
			id,
			[=] {
				_patternImage = QImage();
				update();
			});
	}
	downloadAndMakeCover(data.msgId);

	resizeToWidth(widthNoMargins());
}

void AyuMusicButton::downloadAndMakeCover(FullMsgId msgId) {
	if (_mediaView && _mediaView->owner()->isSongWithCover() && !_mediaView->thumbnail()) {
		const auto settings = &_mediaView->owner()->session().settings().autoDownload();
		if (settings->shouldDownload(Data::AutoDownload::Source::User,
									 Data::AutoDownload::Type::File,
									 _mediaView->owner()->size)) {
			_mediaView->thumbnailWanted(Data::FileOrigin(msgId));
			_mediaView->owner()->owner().session().downloaderTaskFinished(
			) | rpl::take_while([=] {
				if (_mediaView->thumbnail()) {
					makeCover();
				}
				return !_mediaView->thumbnail();
			}) | rpl::start(lifetime());
			return;
		}
	}

	makeCover();
}

void AyuMusicButton::makeCover() {
	const auto weak = base::make_weak(this);
	const auto coverSize = style::ConvertScale(64);
	const auto fallbackAccent = st::windowActiveTextFg->c;
	const auto fallbackBg = GetNoCoverBgColor(_overrideBg);
	crl::async([=,
		mediaView = _mediaView,
		performerText = _performerText,
		titleText = _titleText,
		albumText = _albumText,
		coverUrl = _coverUrl] {
		const auto &settings = AyuSettings::getInstance();

		QImage loadedCoverImage;
		auto albumName = albumText;

		if (!coverUrl.isEmpty()) {
			const auto imgBytes = Ayu::NowPlaying::DownloadBytes(QUrl(coverUrl), 3000);
			if (!imgBytes.isEmpty()) {
				QImage img;
				if (img.loadFromData(imgBytes)) {
					loadedCoverImage = std::move(img);
				}
			}
		}

		if (loadedCoverImage.isNull()) {
			const auto trackInfo = Ayu::Ui::Itunes::FetchTrackInfo(performerText, titleText, 300);
			if (!trackInfo.cover.isNull()) {
				loadedCoverImage = trackInfo.cover;
			}
			if (albumName.isEmpty() && !trackInfo.album.isEmpty() && trackInfo.album.trimmed().compare(titleText.trimmed(), Qt::CaseInsensitive) != 0) {
				albumName = trackInfo.album;
			}
		} else if (albumName.isEmpty()) {
			const auto trackInfo = Ayu::Ui::Itunes::FetchTrackInfo(performerText, titleText, 300);
			if (!trackInfo.album.isEmpty() && trackInfo.album.trimmed().compare(titleText.trimmed(), Qt::CaseInsensitive) != 0) {
				albumName = trackInfo.album;
			}
		}

		auto bgColor = QColor();
		auto accentColor = fallbackAccent;
		if (loadedCoverImage.isNull() || !settings.adaptiveCoverColor()) {
			bgColor = fallbackBg;
		} else {
			const auto colors = ExtractColorsFromCover(loadedCoverImage, fallbackBg, accentColor);
			bgColor = colors.bg;
			accentColor = colors.accent;
		}

		crl::on_main([weak, loadedCoverImage = std::move(loadedCoverImage), mediaView, bgColor, accentColor, albumName, overrideBg = _overrideBg, coverSize]() mutable {
			const auto strong = weak.get();
			if (!strong) {
				return;
			}

			auto cover = GetCurrentCover(mediaView, QSize(coverSize, coverSize));
			const auto hadLoadedCover = !loadedCoverImage.isNull();
			if (hadLoadedCover) {
				cover.pixToBg = QPixmap::fromImage(loadedCoverImage);
				const auto prepared = Image(std::move(loadedCoverImage));
				const auto args = Images::PrepareArgs{
					.options = Images::Option::RoundSmall,
					.outer = QSize(coverSize, coverSize),
				};
				cover.pixToDraw = prepared.pix(QSize(coverSize, coverSize), args);
				cover.noCover = false;
			}

			const auto &settings2 = AyuSettings::getInstance();
			if (cover.noCover || !settings2.adaptiveCoverColor()) {
				bgColor = GetNoCoverBgColor(overrideBg);
			} else if (!hadLoadedCover && !cover.noCover) {
				const auto colors = ExtractColorsFromCover(cover.pixToBg, GetNoCoverBgColor(overrideBg), accentColor);
				bgColor = colors.bg;
				accentColor = colors.accent;
			}

			strong->_currentCover = ResultCover{
				.pix = cover.pixToDraw,
				.bg = bgColor,
				.accent = accentColor,
				.noCover = cover.noCover,
			};
			strong->_patternImage = QImage();

			if (!albumName.isEmpty()) {
				strong->_albumText = albumName;
				strong->_album->setText(albumName);
				strong->_album->show();
			} else {
				strong->_albumText = QString();
				strong->_album->setText(QString());
				strong->_album->hide();
			}

			const auto &cover2 = *strong->_currentCover;

			if (!cover2.noCover && settings2.adaptiveCoverColor() && !cover2.pix.isNull()) {
				const auto subColor = QColor(255, 255, 255, 153);
				strong->_title->setTextColorOverride(Qt::white);
				strong->_performer->setTextColorOverride(subColor);
				strong->_album->setTextColorOverride(subColor);
			} else {
				const auto baseColor = overrideBg ? st::groupCallMembersFg->c : st::windowBoldFg->c;
				const auto subColor = overrideBg ? st::groupCallMembersFg->c : st::windowSubTextFg->c;
				strong->_title->setTextColorOverride(baseColor);
				strong->_performer->setTextColorOverride(subColor);
				strong->_album->setTextColorOverride(subColor);
			}

			strong->resizeToWidth(strong->widthNoMargins());
			strong->repaint();
			strong->_title->repaint();
			strong->_performer->repaint();
			if (!strong->_album->isHidden()) {
				strong->_album->repaint();
			}

			strong->_onReady.fire({});
		});
	});
}

void AyuMusicButton::preparePatternImage() {
	if (!_patternEmoji || !_patternEmoji->ready()) {
		return;
	}
	const auto ratio = style::DevicePixelRatio();
	const auto emojiW = _patternEmoji->width();
	const auto emojiSize = std::max({
		emojiW,
		Ui::Emoji::GetCustomSizeNormal(),
		st::emojiSize
	});
	const auto padding = 2;
	const auto canvasSize = emojiSize + (padding * 2);
	_patternImage = QImage(
		QSize(canvasSize, canvasSize) * ratio,
		QImage::Format_ARGB32_Premultiplied);
	_patternImage.setDevicePixelRatio(ratio);
	_patternImage.fill(Qt::transparent);

	auto painter = QPainter(&_patternImage);
	auto hq = PainterHighQualityEnabler(painter);
	const auto textColor = (_currentCover && _currentCover->accent.isValid())
		? _currentCover->accent
		: Qt::white;
	const auto drawX = padding + (emojiSize - emojiW) / 2;
	const auto drawY = padding + (emojiSize - emojiW) / 2;
	_patternEmoji->paint(painter, {
		.textColor = textColor,
		.now = crl::now(),
		.position = QPoint(drawX, drawY),
	});
}

void AyuMusicButton::paintEvent(QPaintEvent *e) {
	if (!_currentCover) {
		return;
	}

	auto p = Painter(this);
	auto hq = PainterHighQualityEnabler(p);

	const auto &settings = AyuSettings::getInstance();
	const auto cover = *_currentCover;

	const auto cardMarginH = style::ConvertScale(12);
	const auto cardMarginTop = style::ConvertScale(12);
	const auto cardMarginBottom = style::ConvertScale(10);
	const auto cardRadius = style::ConvertScale(12);
	const auto coverPadding = style::ConvertScale(12);
	const auto coverSize = style::ConvertScale(64);
	const auto coverRadius = style::ConvertScale(8);

	const auto cardRect = QRect(
		cardMarginH,
		cardMarginTop,
		width() - (cardMarginH * 2),
		height() - cardMarginTop - cardMarginBottom);

	QPainterPath cardPath;
	cardPath.addRoundedRect(cardRect, cardRadius, cardRadius);

	p.save();
	p.setClipPath(cardPath);

	if (cover.noCover || !settings.adaptiveCoverColor()) {
		p.fillPath(cardPath, cover.bg);
		paintRipple(p, QPoint());
	} else {
		const auto centerX = cardRect.right();
		const auto centerY = cardRect.top() + (cardRect.height() / 2.0);
		const auto radius = cardRect.width() * 2.0;
		QRadialGradient gradient(QPointF(centerX, centerY), radius);
		gradient.setColorAt(0.0, cover.bg);
		gradient.setColorAt(1.0, QColor::fromRgb(AdjustHsl(cover.bg.rgb(), 1.4f)));
		p.fillPath(cardPath, gradient);
		paintRipple(p, QPoint());
	}

	if (_patternImage.isNull()) {
		preparePatternImage();
	}
	if (!_patternImage.isNull()) {
		const auto cardW = cardRect.width();
		const auto cardH = cardRect.height();
		const auto right = cardRect.left() + cardW;
		const auto centerY = cardRect.top() + (cardH / 2.0f);

		const auto baseAlpha = cover.noCover ? 0.4f : 1.0f;
		for (const auto &pt : kNowPlayingPattern) {
			const auto s = style::ConvertScaleExact(pt.size);
			const auto xOff = style::ConvertScaleExact(pt.xOffset);
			const auto yOff = style::ConvertScaleExact(pt.yOffset);
			const auto x = right + xOff - (s / 2.0f);
			const auto y = centerY + yOff - (s / 2.0f);
			p.setOpacity(pt.alpha * baseAlpha);
			p.drawImage(QRectF(x, y, s, s), _patternImage);
		}
		p.setOpacity(1.0);
	}

	const auto coverRect = QRect(
		cardRect.left() + coverPadding,
		cardRect.top() + (cardRect.height() - coverSize) / 2,
		coverSize,
		coverSize);
	if (!cover.pix.isNull()) {
		QPainterPath coverPath;
		coverPath.addRoundedRect(coverRect, coverRadius, coverRadius);
		p.save();
		p.setClipPath(coverPath);
		p.drawPixmap(coverRect, cover.pix);
		p.restore();
	}

	p.restore();

	if (hasPlayButton()) {
		paintPlayButton(p);
	}
}

void AyuMusicButton::paintPlayButton(Painter &p) {
	if (!hasPlayButton()) {
		return;
	}
	const auto btnRect = playButtonRect();
	const auto isPlaying = isCurrentTrackPlaying();

	auto hq = PainterHighQualityEnabler(p);

	const auto cover = _currentCover.value_or(ResultCover{});
	auto btnBg = cover.accent.isValid()
		? cover.accent
		: (cover.bg.isValid() ? cover.bg.lighter(130) : st::windowActiveTextFg->c);

	if (_playButtonPressed) {
		btnBg = btnBg.darker(115);
	} else if (_playButtonHovered) {
		btnBg = btnBg.lighter(115);
	}

	p.setPen(Qt::NoPen);
	p.setBrush(btnBg);
	p.drawEllipse(btnRect);

	const auto cx = btnRect.center().x();
	const auto cy = btnRect.center().y();
	p.setPen(Qt::NoPen);
	p.setBrush(Qt::white);

	if (isPlaying) {
		const auto barW = style::ConvertScale(3);
		const auto barH = style::ConvertScale(12);
		const auto gap = style::ConvertScale(4);
		const auto radius = style::ConvertScale(1);
		const auto leftX = cx - (gap / 2) - barW;
		const auto rightX = cx + (gap / 2);
		const auto topY = cy - (barH / 2);
		p.drawRoundedRect(QRect(leftX, topY, barW, barH), radius, radius);
		p.drawRoundedRect(QRect(rightX, topY, barW, barH), radius, radius);
	} else {
		const auto dx1 = style::ConvertScaleExact(-3.5);
		const auto dy1 = style::ConvertScaleExact(-6.5);
		const auto dx2 = style::ConvertScaleExact(6.0);
		QPolygonF triangle;
		triangle << QPointF(cx + dx1, cy + dy1)
		         << QPointF(cx + dx1, cy - dy1)
		         << QPointF(cx + dx2, cy);
		p.drawPolygon(triangle);
	}
}

QRect AyuMusicButton::playButtonRect(int customWidth) const {
	if (!hasPlayButton()) {
		return QRect();
	}
	const auto w = (customWidth > 0) ? customWidth : width();
	const auto playBtnSize = style::ConvertScale(32);
	const auto cardMarginH = style::ConvertScale(12);
	const auto cardMarginTop = style::ConvertScale(12);
	const auto cardMarginBottom = style::ConvertScale(10);
	const auto cardHeight = height() - cardMarginTop - cardMarginBottom;
	const auto cardRect = QRect(cardMarginH, cardMarginTop, w - (cardMarginH * 2), cardHeight);
	const auto x = cardRect.left() + cardRect.width() - style::ConvertScale(14) - playBtnSize;
	const auto top = cardRect.top() + (cardRect.height() - playBtnSize) / 2;
	return QRect(x, top, playBtnSize, playBtnSize);
}

int AyuMusicButton::resizeGetHeight(int newWidth) {
	const auto cardMarginH = style::ConvertScale(12);
	const auto cardMarginTop = style::ConvertScale(12);
	const auto cardMarginBottom = style::ConvertScale(10);
	const auto coverPadding = style::ConvertScale(12);
	const auto coverSize = style::ConvertScale(64);
	const auto lineSkip = style::ConvertScale(3);

	const auto playBtnReserve = hasPlayButton()
		? (style::ConvertScale(32) + style::ConvertScale(14) + style::ConvertScale(12))
		: style::ConvertScale(14);
	const auto textLeft = cardMarginH + coverPadding + coverSize + style::ConvertScale(12);
	const auto textRight = cardMarginH + (newWidth - cardMarginH * 2) - playBtnReserve;
	const auto textWidth = std::max(textRight - textLeft, 10);

	_title->resizeToNaturalWidth(textWidth);
	_performer->resizeToNaturalWidth(textWidth);

	auto textHeight = _title->height() + lineSkip + _performer->height();
	if (_album && !_album->isHidden()) {
		_album->resizeToNaturalWidth(textWidth);
		textHeight += lineSkip + _album->height();
	}

	const auto minCardHeight = coverSize + coverPadding * 2;
	const auto cardHeight = std::max(minCardHeight, textHeight + coverPadding * 2);
	const auto cardRect = QRect(cardMarginH, cardMarginTop, newWidth - (cardMarginH * 2), cardHeight);

	const auto textTop = cardRect.top() + (cardHeight - textHeight) / 2;
	_title->moveToLeft(textLeft, textTop);
	_performer->moveToLeft(textLeft, textTop + _title->height() + lineSkip);
	if (_album && !_album->isHidden()) {
		_album->moveToLeft(textLeft, textTop + _title->height() + lineSkip + _performer->height() + lineSkip);
	}

	return cardHeight + cardMarginTop + cardMarginBottom;
}

void AyuMusicButton::mousePressEvent(QMouseEvent *e) {
	if (hasPlayButton() && e->button() == Qt::LeftButton && playButtonRect().contains(e->pos())) {
		_playButtonPressed = true;
		update(playButtonRect());
		return;
	}
	RippleButton::mousePressEvent(e);
}

void AyuMusicButton::mouseReleaseEvent(QMouseEvent *e) {
	if (hasPlayButton() && _playButtonPressed) {
		_playButtonPressed = false;
		if (playButtonRect().contains(e->pos())) {
			togglePlay();
		}
		update(playButtonRect());
		return;
	}
	RippleButton::mouseReleaseEvent(e);
}

void AyuMusicButton::mouseMoveEvent(QMouseEvent *e) {
	if (hasPlayButton()) {
		const auto hovered = playButtonRect().contains(e->pos());
		if (_playButtonHovered != hovered) {
			_playButtonHovered = hovered;
			update(playButtonRect());
		}
	}
	RippleButton::mouseMoveEvent(e);
}

void AyuMusicButton::leaveEventHook(QEvent *e) {
	if (_playButtonHovered || _playButtonPressed) {
		_playButtonHovered = false;
		_playButtonPressed = false;
		if (hasPlayButton()) {
			update(playButtonRect());
		}
	}
	RippleButton::leaveEventHook(e);
}

void AyuMusicButton::setupPlayerTracking() {
	::Media::Player::instance()->updatedNotifier(
	) | rpl::on_next([=](const ::Media::Player::TrackState &state) {
		if (state.id.type() == AudioMsgId::Type::Song) {
			update(playButtonRect());
		}
	}, lifetime());
}

bool AyuMusicButton::isCurrentTrackPlaying() const {
	if (_isLiveTrack) {
		return _previewTrack && _previewTrack->isActive();
	}
	if (!_mediaView) {
		return false;
	}
	const auto document = _mediaView->owner();
	if (!document) {
		return false;
	}
	const auto state = ::Media::Player::instance()->getState(AudioMsgId::Type::Song);
	return (state.id.audio() == document) && ::Media::Player::ShowPauseIcon(state.state);
}

void AyuMusicButton::togglePlay() {
	if (_isLiveTrack) {
		if (_previewTrack && _previewTrack->isActive()) {
			_previewTrack.reset();
			_previewTrackTimer.cancel();
			update(playButtonRect());
			return;
		}
		if (!_previewBytes.isEmpty()) {
			_previewTrack = ::Media::Audio::Current().createTrack();
			_previewTrack->fillFromData(bytes::make_vector(_previewBytes));
			_previewTrack->playOnce();
			const auto weak = base::make_weak(this);
			_previewTrackTimer.setCallback([weak] {
				if (const auto strong = weak.get()) {
					if (!strong->_previewTrack || !strong->_previewTrack->isActive()) {
						strong->_previewTrackTimer.cancel();
						strong->update(strong->playButtonRect());
					}
				}
			});
			_previewTrackTimer.callEach(250);
			update(playButtonRect());
			return;
		}
		if (_previewUrl.isEmpty() || _previewDownloading) {
			return;
		}
		_previewDownloading = true;
		const auto weak = base::make_weak(this);
		crl::async([weak, previewUrl = _previewUrl] {
			const auto bytes = Ayu::NowPlaying::DownloadBytes(QUrl(previewUrl), 5000);
			crl::on_main([weak, bytes, previewUrl] {
				const auto strong = weak.get();
				if (!strong) {
					return;
				}
				if (strong->_previewUrl != previewUrl) {
					return;
				}
				strong->_previewDownloading = false;
				if (bytes.isEmpty()) {
					return;
				}
				strong->_previewBytes = bytes;
				strong->_previewTrack = ::Media::Audio::Current().createTrack();
				strong->_previewTrack->fillFromData(bytes::make_vector(bytes));
				strong->_previewTrack->playOnce();
				strong->_previewTrackTimer.setCallback([weak] {
					if (const auto s = weak.get()) {
						if (!s->_previewTrack || !s->_previewTrack->isActive()) {
							s->_previewTrackTimer.cancel();
							s->update(s->playButtonRect());
						}
					}
				});
				strong->_previewTrackTimer.callEach(250);
				strong->update(strong->playButtonRect());
			});
		});
		return;
	}
	if (!_mediaView || !_mediaView->owner()) {
		return;
	}
	::Media::Player::instance()->playPause({ _mediaView->owner(), _msgId });
}

} // namespace Info::Profile
