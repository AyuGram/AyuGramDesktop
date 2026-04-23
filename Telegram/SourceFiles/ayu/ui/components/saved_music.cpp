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
#include "data/data_session.h"
#include "info/profile/info_profile_music_button.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "styles/palette.h"
#include "styles/style_info.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/image/image.h"
#include "ui/widgets/labels.h"
#include "window/themes/window_theme.h"

#include <QSvgRenderer>

namespace Info::Profile {

namespace {

QColor performerColor(255, 255, 255, 153); // white 60%

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

struct Cover
{
	QPixmap pixToDraw;
	QPixmap pixToBg;
	bool noCover;
};

QPixmap MakeNoCoverImage(const QSize &size) {
	static QPixmap result;
	static auto resultTheme = Window::Theme::Background()->id();
	if (!result.isNull() && result.size() == size && resultTheme == Window::Theme::Background()->id()) {
		return result;
	}
	resultTheme = Window::Theme::Background()->id();

	auto image = QImage(size, QImage::Format_ARGB32);
	{
		auto p = Painter(&image);
		auto hq = PainterHighQualityEnabler(p);

		const auto bgColor = Window::Theme::IsNightMode()
								 ? st::windowBoldFg->c.darker()
								 : st::windowBoldFg->c.lighter();
		image.fill(bgColor);

		auto svgIcon = QSvgRenderer(u":/gui/icons/ayu/nocover.svg"_q);
		p.setPen(st::windowBoldFg->p);
		svgIcon.render(&p, QRect(0, 0, size.width(), size.height()));
	}
	const auto img = Image(std::move(image));
	result = img.pix(size, Images::PrepareArgs{.options = Images::Option::RoundSmall});
	return result;
}

} // namespace

Cover GetCurrentCover(
	const std::shared_ptr<Data::DocumentMedia> &dataMedia,
	const QSize &size) {
	if (!dataMedia) {
		return {
			.pixToDraw = MakeNoCoverImage(size),
			.pixToBg = MakeNoCoverImage(size),
			.noCover = true
		};
	}

	auto cover = QPixmap();
	const auto scaled = [&](not_null<Image*> image)
	{
		const auto aspectRatio = Qt::KeepAspectRatioByExpanding;
		const auto targetSize = size * style::DevicePixelRatio();
		return image->size().scaled(targetSize, aspectRatio);
	};
	const auto args = Images::PrepareArgs{
		.options = Images::Option::RoundSmall,
		.outer = size,
	};
	if (const auto normal = dataMedia->thumbnail()) {
		return {
			.pixToDraw = normal->pixNoCache(scaled(normal), args),
			.pixToBg = normal->pixNoCache(),
			.noCover = false
		};
	}

	return {
		.pixToDraw = MakeNoCoverImage(size),
		.pixToBg = MakeNoCoverImage(size),
		.noCover = true
	};
}

std::optional<QRgb> ExtractColorFromCover(const QPixmap &cover) {
	const auto palette = Ayu::Ui::Palette::from(cover).generate();

	const auto *swatch = palette.darkVibrantSwatch();
	if (!swatch) {
		swatch = palette.mutedSwatch();
	}
	if (!swatch) {
		swatch = palette.darkMutedSwatch();
	}
	if (!swatch) {
		swatch = palette.dominantSwatch();
	}

	if (!swatch) {
		return std::nullopt;
	}

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

	return adjustedColor;
}

AyuMusicButton::AyuMusicButton(
	QWidget *parent,
	MusicButtonData data,
	std::optional<QColor> overrideBg,
	Fn<void()> handler)
	: RippleButton(parent, st::infoMusicButtonRipple)
	  , _performer(std::make_unique<Ui::FlatLabel>(
		  this,
		  data.performer,
		  st::infoMusicButtonPerformer))
	  , _title(std::make_unique<Ui::FlatLabel>(
		  this,
		  data.title,
		  st::infoMusicButtonTitle))
	  , _mediaView(data.mediaView)
	  , _overrideBg(overrideBg) {
	_performerText = data.performer;
	_titleText = data.title;
	rpl::combine(
		_title->naturalWidthValue(),
		_performer->naturalWidthValue()
	) | rpl::on_next([=]
							 {
								 resizeToWidth(widthNoMargins());
							 },
							 lifetime());

	_title->setAttribute(Qt::WA_TransparentForMouseEvents);
	_performer->setAttribute(Qt::WA_TransparentForMouseEvents);

	downloadAndMakeCover(data.msgId);

	setClickedCallback(std::move(handler));
}

AyuMusicButton::~AyuMusicButton() = default;

void AyuMusicButton::updateData(MusicButtonData data) {
	_performer->setText(data.performer);
	_title->setText(data.title);
	_performerText = data.performer;
	_titleText = data.title;
	_mediaView = data.mediaView;
	downloadAndMakeCover(data.msgId);

	resizeToWidth(widthNoMargins());
}

void AyuMusicButton::downloadAndMakeCover(FullMsgId msgId) {
	if (_mediaView && _mediaView->owner()->isSongWithCover() && !_mediaView->thumbnail()) {
		const auto settings = &_mediaView->owner()->session().settings().autoDownload();
		// Data::AutoDownload::Type::Music always returns false
		if (settings->shouldDownload(Data::AutoDownload::Source::User,
									 Data::AutoDownload::Type::File,
									 _mediaView->owner()->size)) {
			_mediaView->thumbnailWanted(Data::FileOrigin(msgId));
			_mediaView->owner()->owner().session().downloaderTaskFinished(
			) | rpl::take_while([=]
			{
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
	crl::async(
		[=, mediaView = _mediaView, performerText = _performerText, titleText = _titleText, overrideBg = _overrideBg]()
		{
			const auto &settings = AyuSettings::getInstance();
			const auto &font = st::infoMusicButtonTitle.style.font;
			const auto skip = st::normalFont->spacew / 2;
			const auto size = font->height + skip + font->height;

			auto cover = GetCurrentCover(mediaView, QSize(size, size));

			if (cover.noCover) {
				const auto pix = Ayu::Ui::Itunes::FetchCover(performerText, titleText, size);
				if (!pix.isNull()) {
					const auto img = Image(pix.toImage());
					const auto args = Images::PrepareArgs{
						.options = Images::Option::RoundSmall,
						.outer = QSize(size, size),
					};
					cover.pixToDraw = img.pix(QSize(size, size), args);
					cover.pixToBg = pix;
					cover.noCover = false;
				}
			}

			QColor bgColor;
			if (cover.noCover || !settings.adaptiveCoverColor()) {
				bgColor = GetNoCoverBgColor(overrideBg);
			} else {
				if (const auto extractedColor = ExtractColorFromCover(cover.pixToBg)) {
					bgColor = QColor::fromRgb(*extractedColor);
				} else {
					// example: fully black image
					cover.noCover = true;
					bgColor = GetNoCoverBgColor(overrideBg);
				}
			}

			crl::on_main([weak, cover = std::move(cover), bgColor, overrideBg]() mutable
			{
				const auto strong = weak.get();
				if (!strong) {
					return;
				}

				strong->_currentCover = {
					.pix = cover.pixToDraw,
					.bg = bgColor,
					.noCover = cover.noCover,
				};

				const auto &settings2 = AyuSettings::getInstance();
				const auto &cover2 = *strong->_currentCover;

				if (!cover2.noCover && settings2.adaptiveCoverColor() && !cover2.pix.isNull()) {
					strong->_title->setTextColorOverride(Qt::white);
					strong->_performer->setTextColorOverride(performerColor);
				} else {
					strong->_title->setTextColorOverride(overrideBg ? st::groupCallMembersFg->c : st::windowBoldFg->c);
					strong->_performer->setTextColorOverride(
						overrideBg ? st::groupCallMembersFg->c : st::windowBoldFg->c);
				}

				strong->repaint();
				strong->_title->repaint();
				strong->_performer->repaint();

				strong->_onReady.fire({});
			});
		});
}

void AyuMusicButton::paintEvent(QPaintEvent *e) {
	if (!_currentCover) {
		return;
	}

	auto p = Painter(this);

	const auto &font = st::infoMusicButtonTitle.style.font;
	const auto skip = st::normalFont->spacew / 2;
	const auto size = font->height + skip + font->height;

	const auto &settings = AyuSettings::getInstance();
	const auto cover = _currentCover.value();
	if (cover.noCover || !settings.adaptiveCoverColor()) {
		p.fillRect(e->rect(), cover.bg);
		paintRipple(p, QPoint());
	} else {
		QRadialGradient gradient(rect().topRight(), rect().width() * 2.0);
		gradient.setColorAt(0, cover.bg);
		gradient.setColorAt(1, QColor::fromRgb(AdjustHsl(cover.bg.rgb(), 1.5f)));
		p.fillRect(rect(), gradient);
	}

	if (!cover.pix.isNull()) {
		auto hq = PainterHighQualityEnabler(p);
		const auto coverRect = QRect(st::infoMusicButtonPadding.left(), st::infoMusicButtonPadding.top(), size, size);
		p.drawPixmap(coverRect, cover.pix);
	}
}

int AyuMusicButton::resizeGetHeight(int newWidth) {
	const auto padding = st::infoMusicButtonPadding;
	const auto &font = st::infoMusicButtonTitle.style.font;

	const auto top = padding.top();
	const auto skip = st::normalFont->spacew / 2;

	const auto coverSize = font->height + skip + font->height;

	const auto available = newWidth - padding.left() - padding.right() - coverSize;
	_title->resizeToNaturalWidth(available);
	_title->moveToLeft(st::infoMusicButtonPadding.left() + padding.left() + coverSize, top);
	_performer->resizeToNaturalWidth(available);
	_performer->moveToLeft(st::infoMusicButtonPadding.left() + padding.left() + coverSize, top + font->height + skip);

	return padding.top() + font->height + skip + font->height + padding.bottom();
}

} // namespace Info::Profile
