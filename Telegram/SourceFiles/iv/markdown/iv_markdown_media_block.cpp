/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_media_block.h"

#include "iv/markdown/iv_markdown_article.h"
#include "iv/markdown/iv_markdown_article_layout_blocks.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "lang/lang_keys.h"
#include "ui/dynamic_image.h"
#include "ui/effects/spoiler_mess.h"
#include "ui/grouped_layout.h"
#include "ui/image/image_prepare.h"

#include "rpl/lifetime.h"

#include "styles/palette.h"
#include "styles/style_iv.h"
#include "styles/style_widgets.h"

#include <QtGui/QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Iv::Markdown {
namespace {

constexpr auto kIvMarkedTextOptions = TextParseOptions{
	TextParseMultiline,
	0,
	0,
	Qt::LayoutDirectionAuto,
};

constexpr auto kMaxGroupedMediaLayoutItems = 10;

[[nodiscard]] bool PaintDynamicImage(
		Painter &p,
		const std::shared_ptr<Ui::DynamicImage> &image,
		QRect rect) {
	if (!image || rect.isEmpty()) {
		return false;
	}
	if (const auto frame = image->image(std::max(rect.width(), rect.height()));
		!frame.isNull()) {
		p.drawImage(rect, frame);
		return true;
	}
	return false;
}

[[nodiscard]] bool PaintResolvedImages(
		Painter &p,
		QRect rect,
		const std::shared_ptr<Ui::DynamicImage> &thumbnail,
		const std::shared_ptr<Ui::DynamicImage> &full,
		const std::shared_ptr<Ui::DynamicImage> &previousThumbnail,
		const std::shared_ptr<Ui::DynamicImage> &previousFull) {
	const auto paintedThumbnail = PaintDynamicImage(p, thumbnail, rect);
	const auto paintedFull = PaintDynamicImage(p, full, rect);
	if (paintedThumbnail || paintedFull) {
		return true;
	}
	const auto paintedPreviousThumbnail = PaintDynamicImage(
		p,
		previousThumbnail,
		rect);
	const auto paintedPreviousFull = PaintDynamicImage(p, previousFull, rect);
	return paintedPreviousThumbnail || paintedPreviousFull;
}

[[nodiscard]] QImage ResolvedImageFrame(
		const std::shared_ptr<Ui::DynamicImage> &current,
		const std::shared_ptr<Ui::DynamicImage> &previous,
		int size) {
	if (current) {
		if (const auto frame = current->image(size); !frame.isNull()) {
			return frame;
		}
	}
	if (previous) {
		return previous->image(size);
	}
	return QImage();
}

[[nodiscard]] QImage PrepareWithBlurredBackground(
		QSize outer,
		QSize inner,
		QImage large,
		QImage blurred) {
	const auto ratio = style::DevicePixelRatio();
	auto background = QImage(
		outer * ratio,
		QImage::Format_ARGB32_Premultiplied);
	background.setDevicePixelRatio(ratio);
	if (blurred.isNull()) {
		background.fill(Qt::black);
		if (large.isNull()) {
			return background;
		}
	}
	auto p = QPainter(&background);
	if (!blurred.isNull()) {
		auto cover = blurred.scaled(
			outer * ratio,
			Qt::KeepAspectRatioByExpanding,
			Qt::SmoothTransformation);
		if (cover.size() != outer * ratio) {
			cover = cover.copy(QRect(
				QPoint(
					(cover.width() - outer.width() * ratio) / 2,
					(cover.height() - outer.height() * ratio) / 2),
				outer * ratio));
		}
		cover = Images::Blur(std::move(cover), true);
		cover.setDevicePixelRatio(ratio);
		p.drawImage(QPoint(), cover);
		p.fillRect(QRect(QPoint(), outer), QColor(0, 0, 0, 48));
	}
	if (!large.isNull()) {
		auto image = large.scaled(
			inner * ratio,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		image.setDevicePixelRatio(ratio);
		p.drawImage(
			(outer.width() - inner.width()) / 2,
			(outer.height() - inner.height()) / 2,
			image);
	}
	return background;
}

[[nodiscard]] const style::Markdown &PaintStyle(
		const MarkdownArticlePaintContext &context,
		const style::Markdown &st) {
	return context.paintMarkdownStyle(st);
}

[[nodiscard]] MarkdownArticlePaintContext ClippedContext(
		const MarkdownArticlePaintContext &context,
		QRect clip) {
	auto result = context;
	result.clip = clip;
	return result;
}

void PaintImageSpoiler(Painter &p, QRect rect) {
	Ui::FillSpoilerRect(p, rect, Ui::DefaultImageSpoiler().frame());
}

[[nodiscard]] bool UpdateResolvedImage(
		std::shared_ptr<Ui::DynamicImage> *current,
		std::shared_ptr<Ui::DynamicImage> *previous,
		const std::shared_ptr<Ui::DynamicImage> &next) {
	if (!current || !previous || !next) {
		return false;
	} else if (next == *current) {
		return false;
	} else if (next == *previous) {
		std::swap(*current, *previous);
		return false;
	}
	*previous = std::move(*current);
	*current = next;
	return true;
}

template <typename Runtime, typename SubscribeCallback>
void RefreshResolvedImages(
		const std::shared_ptr<Runtime> &runtime,
		QSize size,
		QSize *requestedSize,
		std::shared_ptr<Ui::DynamicImage> *thumbnail,
		std::shared_ptr<Ui::DynamicImage> *previousThumbnail,
		std::shared_ptr<Ui::DynamicImage> *full,
		std::shared_ptr<Ui::DynamicImage> *previousFull,
		SubscribeCallback &&subscribe) {
	if (!runtime || !requestedSize || size.isEmpty() || (*requestedSize == size)) {
		return;
	}
	*requestedSize = size;
	if (UpdateResolvedImage(
			thumbnail,
			previousThumbnail,
			runtime->thumbnail(size))) {
		subscribe(*thumbnail);
	}
	if (UpdateResolvedImage(full, previousFull, runtime->full(size))) {
		subscribe(*full);
	}
}

void PaintPhotoProgress(
		Painter &p,
		QRect rect,
		const style::MarkdownPhoto &style,
		double progress) {
	const auto size = std::min({
		style.progressSize,
		rect.width(),
		rect.height(),
	});
	if (size <= 0) {
		return;
	}
	const auto thickness = std::max(style.progressWidth, 1);
	const auto ring = QRect(
		rect.center().x() - (size / 2),
		rect.center().y() - (size / 2),
		size,
		size);
	auto hq = PainterHighQualityEnabler(p);
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(style.progressBg->c, thickness));
	p.drawEllipse(ring);
	p.setPen(QPen(style.progressFg->c, thickness));
	p.drawArc(
		ring,
		90 * 16,
		-int(std::round(360. * 16. * std::clamp(progress, 0., 1.))));
}

[[nodiscard]] int MediaHeightForWidth(
		int width,
		int aspectWidth,
		int aspectHeight) {
	aspectWidth = std::max(aspectWidth, 1);
	aspectHeight = std::max(aspectHeight, 1);
	return std::max(
		int((int64(width) * aspectHeight + aspectWidth - 1) / aspectWidth),
		1);
}

void SetPlainTextLeaf(
		Ui::Text::String *leaf,
		const style::TextStyle &textStyle,
		const QString &text,
		int width) {
	*leaf = Ui::Text::String(TextMinResizeWidth(width));
	leaf->setMarkedText(
		textStyle,
		TextWithEntities::Simple(text),
		kIvMarkedTextOptions);
}

[[nodiscard]] int LeafHeight(
		const Ui::Text::String &leaf,
		const style::TextStyle &textStyle,
		int width) {
	return std::max(
		leaf.countHeight(width, true),
		TextLineHeight(textStyle));
}

[[nodiscard]] int LeafFirstLineBaseline(
		const Ui::Text::String &leaf,
		const QRect &textRect,
		const style::TextStyle &textStyle) {
	const auto lines = leaf.countLinesGeometry(textRect.width(), true);
	return textRect.y() + (lines.empty()
		? TextLineBaseline(textStyle)
		: lines.front().baseline);
}

void PaintTextLeaf(
		Painter &p,
		const Ui::Text::String &leaf,
		const MarkdownArticlePaintContext &context,
		QRect rect,
		int width,
		style::align align = style::al_left) {
	const auto availableWidth = std::max(width, 1);
	leaf.draw(p, {
		.position = rect.topLeft(),
		.availableWidth = availableWidth,
		.geometry = TextGeometry(availableWidth),
		.align = align,
		.clip = context.clip,
		.palette = &p.textPalette(),
		.pre = context.caches.pre,
		.blockquote = context.caches.blockquote,
		.colors = context.caches.colors,
		.spoiler = Ui::Text::DefaultSpoilerCache(),
		.now = context.now,
	});
}

void PaintCardSurface(
		Painter &p,
		QRect rect,
		int border,
		const style::color &borderFg,
		const style::color &bg,
		int radius) {
	if (rect.isEmpty()) {
		return;
	}
	if (border <= 0) {
		if (radius > 0) {
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(bg->c);
			p.drawRoundedRect(rect, radius, radius);
		} else {
			p.fillRect(rect, bg->c);
		}
		return;
	}
	const auto half = border / 2.;
	const auto inner = QRectF(rect).marginsRemoved({
		half,
		half,
		half,
		half,
	});
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(QPen(borderFg->c, border));
	p.setBrush(bg->c);
	p.drawRoundedRect(inner, radius, radius);
}

[[nodiscard]] QString ChannelCopyText(const PreparedChannelBlockData &channel) {
	return channel.title;
}

[[nodiscard]] QString GroupedMediaCopyText(
		const PreparedGroupedMediaBlockData &grouped) {
	auto photos = 0;
	auto videos = 0;
	for (const auto &item : grouped.items) {
		if (item.media.kind == PreparedMediaItemKind::Photo) {
			++photos;
		} else {
			++videos;
		}
	}
	if (photos && !videos) {
		return tr::lng_media_selected_photo(tr::now, lt_count, photos);
	} else if (videos && !photos) {
		return tr::lng_media_selected_video(tr::now, lt_count, videos);
	}
	return QString();
}

[[nodiscard]] QString GroupedMediaItemCopyText(PreparedMediaItemKind kind) {
	return (kind == PreparedMediaItemKind::Photo)
		? u"Photo"_q
		: tr::lng_in_dlg_video(tr::now);
}

[[nodiscard]] int GroupedMediaMinWidth(int width, int spacing) {
	return std::max((width - 2 * spacing) / 3, 1);
}

[[nodiscard]] int GroupedMediaLayoutWidth(
		const std::vector<Ui::GroupMediaLayout> &layout) {
	auto result = 0;
	for (const auto &part : layout) {
		result = std::max(
			result,
			part.geometry.x() + part.geometry.width());
	}
	return result;
}

[[nodiscard]] int GroupedMediaLayoutHeight(
		const std::vector<Ui::GroupMediaLayout> &layout) {
	auto result = 0;
	for (const auto &part : layout) {
		result = std::max(
			result,
			part.geometry.y() + part.geometry.height());
	}
	return result;
}

[[nodiscard]] QPainterPath RoundedRectPath(QRect rect, int radius) {
	auto path = QPainterPath();
	path.addRoundedRect(QRectF(rect), radius, radius);
	return path;
}

void PaintRoundButton(
		Painter &p,
		QRect rect,
		const style::color &bg,
		const style::icon &icon) {
	if (rect.isEmpty()) {
		return;
	}
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(Qt::NoPen);
	p.setBrush(bg->c);
	p.drawEllipse(rect);
	icon.paintInCenter(p, rect);
}

enum class ImageBackedMediaKind {
	Photo,
	Video,
	Map,
};

struct MapDescriptor {
	double latitude = 0.;
	double longitude = 0.;
	uint64 accessHash = 0;
	int zoom = 0;
	QString url;
};

class ImageBackedMediaBlock final : public MediaBlock {
public:
	ImageBackedMediaBlock(
		const PreparedPhotoBlockData &prepared,
		std::shared_ptr<MediaRuntime> mediaRuntime);

	ImageBackedMediaBlock(
		const PreparedVideoBlockData &prepared,
		std::shared_ptr<MediaRuntime> mediaRuntime);

	ImageBackedMediaBlock(
		const PreparedMapBlockData &prepared,
		std::shared_ptr<MediaRuntime> mediaRuntime);

	[[nodiscard]] uint64 stableId() const override;

	[[nodiscard]] int resizeGetHeight(int width) override;

	void setGeometry(QRect geometry) override;

	[[nodiscard]] QRect geometry() const override;

	[[nodiscard]] int firstLineBaseline() const override;

	void paint(
			Painter &p,
			const MarkdownArticlePaintContext &context) const override;

	[[nodiscard]] ClickHandlerPtr linkAt(QPoint point) const override;

	[[nodiscard]] MediaActivation activationAt(QPoint point) const override;

	[[nodiscard]] MediaBlockSelectionData selectionData() const override;

private:
	void ensureResolved(QSize size);

	void ensurePhotoResolved(QSize size);

	void ensureVideoResolved(QSize size);

	void ensureMapResolved(QSize size);

	template <typename Runtime>
	void resolveImages(
			const std::shared_ptr<Runtime> &runtime,
			QSize size);

	void subscribeImage(const std::shared_ptr<Ui::DynamicImage> &image);

	[[nodiscard]] bool loading() const;

	[[nodiscard]] double progress() const;

	const ImageBackedMediaKind _kind;
	const uint64 _stableId = 0;
	const QString _copyText;
	const int _aspectWidth = 0;
	const int _aspectHeight = 0;
	const std::shared_ptr<MediaRuntime> _mediaRuntime;
	const uint64 _photoId = 0;
	const uint64 _documentId = 0;
	const QString _url;
	const bool _viewerOpen = false;
	const bool _spoiler = false;
	const MapDescriptor _map;

	QRect _geometry;
	MediaActivation _activation;
	std::shared_ptr<PhotoRuntime> _photoRuntime;
	std::shared_ptr<DocumentRuntime> _documentRuntime;
	std::shared_ptr<MapRuntime> _mapRuntime;
	std::shared_ptr<Ui::DynamicImage> _thumbnailImage;
	std::shared_ptr<Ui::DynamicImage> _fullImage;
	std::shared_ptr<Ui::DynamicImage> _previousThumbnailImage;
	std::shared_ptr<Ui::DynamicImage> _previousFullImage;
	QSize _requestedImageSize;
	QSize _mapRuntimeSize;
	bool _photoRuntimeResolved = false;
	bool _documentRuntimeResolved = false;
	bool _mapRuntimeResolved = false;
};

ImageBackedMediaBlock::ImageBackedMediaBlock(
	const PreparedPhotoBlockData &prepared,
	std::shared_ptr<MediaRuntime> mediaRuntime)
: _kind(ImageBackedMediaKind::Photo)
, _stableId(prepared.id.value)
, _copyText(u"Photo"_q)
, _aspectWidth(prepared.width)
, _aspectHeight(prepared.height)
, _mediaRuntime(std::move(mediaRuntime))
, _photoId(prepared.photoId)
, _url(prepared.urlOverride)
, _viewerOpen(prepared.viewerOpen)
, _spoiler(prepared.spoiler) {
	if (!_url.isEmpty()) {
		_activation.kind = MediaActivationKind::ExternalUrl;
		_activation.url = _url;
	}
}

ImageBackedMediaBlock::ImageBackedMediaBlock(
	const PreparedVideoBlockData &prepared,
	std::shared_ptr<MediaRuntime> mediaRuntime)
: _kind(ImageBackedMediaKind::Video)
, _stableId(prepared.id.value)
, _copyText(tr::lng_in_dlg_video(tr::now))
, _aspectWidth(prepared.media.width)
, _aspectHeight(prepared.media.height)
, _mediaRuntime(std::move(mediaRuntime))
, _documentId(prepared.media.id)
, _spoiler(prepared.media.spoiler) {
}

ImageBackedMediaBlock::ImageBackedMediaBlock(
	const PreparedMapBlockData &prepared,
	std::shared_ptr<MediaRuntime> mediaRuntime)
: _kind(ImageBackedMediaKind::Map)
, _stableId(prepared.id.value)
, _copyText(tr::lng_maps_point(tr::now))
, _aspectWidth(prepared.width)
, _aspectHeight(prepared.height)
, _mediaRuntime(std::move(mediaRuntime))
, _map(MapDescriptor {
	.latitude = prepared.latitude,
	.longitude = prepared.longitude,
	.accessHash = prepared.accessHash,
	.zoom = prepared.zoom,
	.url = prepared.url,
}) {
	if (!_map.url.isEmpty()) {
		_activation.kind = MediaActivationKind::ExternalUrl;
		_activation.url = _map.url;
	}
}

uint64 ImageBackedMediaBlock::stableId() const {
	return _stableId;
}

int ImageBackedMediaBlock::resizeGetHeight(int width) {
	return MediaHeightForWidth(width, _aspectWidth, _aspectHeight);
}

void ImageBackedMediaBlock::setGeometry(QRect geometry) {
	_geometry = geometry;
	ensureResolved(geometry.size());
}

QRect ImageBackedMediaBlock::geometry() const {
	return _geometry;
}

int ImageBackedMediaBlock::firstLineBaseline() const {
	return _geometry.y();
}

void ImageBackedMediaBlock::paint(
		Painter &p,
		const MarkdownArticlePaintContext &context) const {
	const auto visible = context.clip.intersected(_geometry);
	if (visible.isEmpty()) {
		return;
	}
	const auto &st = PaintStyle(context, layoutStyle());
	p.save();
	p.setClipRect(visible);
	const auto painted = PaintResolvedImages(
			p,
			_geometry,
			_thumbnailImage,
			_fullImage,
			_previousThumbnailImage,
			_previousFullImage);
	if (painted && _spoiler) {
		PaintImageSpoiler(p, _geometry);
	}

	if (loading()) {
		PaintPhotoProgress(
			p,
			_geometry,
			st.photo,
			progress());
	}
	p.restore();
}

ClickHandlerPtr ImageBackedMediaBlock::linkAt(QPoint point) const {
	Q_UNUSED(point);
	return nullptr;
}

MediaActivation ImageBackedMediaBlock::activationAt(
		QPoint point) const {
	return _geometry.contains(point) ? _activation : MediaActivation();
}

MediaBlockSelectionData ImageBackedMediaBlock::selectionData() const {
	return {
		.copyText = _copyText,
	};
}

void ImageBackedMediaBlock::ensureResolved(QSize size) {
	if (size.isEmpty()) {
		return;
	}

	switch (_kind) {
	case ImageBackedMediaKind::Photo:
		ensurePhotoResolved(size);
		break;
	case ImageBackedMediaKind::Video:
		ensureVideoResolved(size);
		break;
	case ImageBackedMediaKind::Map:
		ensureMapResolved(size);
		break;
	}
}

void ImageBackedMediaBlock::ensurePhotoResolved(QSize size) {
	if (!_photoRuntimeResolved) {
		_photoRuntimeResolved = true;
		if (_mediaRuntime) {
			_photoRuntime = _mediaRuntime->resolvePhoto(_photoId);
		}
		if (_url.isEmpty() && _viewerOpen && _photoRuntime) {
			_activation.kind = MediaActivationKind::Photo;
			_activation.photo = _photoRuntime;
		}
	}
	resolveImages(_photoRuntime, size);
}

void ImageBackedMediaBlock::ensureVideoResolved(QSize size) {
	if (!_documentRuntimeResolved) {
		_documentRuntimeResolved = true;
		if (_mediaRuntime) {
			_documentRuntime = _mediaRuntime->resolveDocument(_documentId);
		}
		if (_documentRuntime) {
			_activation.kind = MediaActivationKind::Document;
			_activation.document = _documentRuntime;
		}
	}
	resolveImages(_documentRuntime, size);
}

void ImageBackedMediaBlock::ensureMapResolved(QSize size) {
	if (!_mapRuntimeResolved || (_mapRuntimeSize != size)) {
		_mapRuntimeResolved = true;
		_mapRuntimeSize = size;
		if (_mediaRuntime) {
			_mapRuntime = _mediaRuntime->resolveMap(
				_map.latitude,
				_map.longitude,
				_map.accessHash,
				size,
				_map.zoom);
		}
	}
	resolveImages(_mapRuntime, size);
}

template <typename Runtime>
void ImageBackedMediaBlock::resolveImages(
		const std::shared_ptr<Runtime> &runtime,
		QSize size) {
	RefreshResolvedImages(
		runtime,
		size,
		&_requestedImageSize,
		&_thumbnailImage,
		&_previousThumbnailImage,
		&_fullImage,
		&_previousFullImage,
		[&](const std::shared_ptr<Ui::DynamicImage> &image) {
			subscribeImage(image);
		});
}

void ImageBackedMediaBlock::subscribeImage(const std::shared_ptr<Ui::DynamicImage> &image) {
	if (!image) {
		return;
	}

	const auto weak = std::weak_ptr<MediaBlock>(shared_from_this());
	image->subscribeToUpdates([weak] {
		if (const auto block = weak.lock()) {
			if (const auto host = block->host()) {
				host->requestRepaint(block->geometry());
			}
		}
	});
}

bool ImageBackedMediaBlock::loading() const {
	if (_photoRuntime) {
		return _photoRuntime->loading();
	} else if (_documentRuntime) {
		return _documentRuntime->loading();
	} else if (_mapRuntime) {
		return _mapRuntime->loading();
	}
	return false;
}

double ImageBackedMediaBlock::progress() const {
	if (_photoRuntime) {
		return _photoRuntime->progress();
	} else if (_documentRuntime) {
		return _documentRuntime->progress();
	} else if (_mapRuntime) {
		return _mapRuntime->progress();
	}
	return 0.;
}

class ChannelMediaBlock final : public MediaBlock {
public:
	ChannelMediaBlock(
		const PreparedChannelBlockData &prepared,
		std::shared_ptr<MediaRuntime> mediaRuntime);

	[[nodiscard]] uint64 stableId() const override;

	[[nodiscard]] int resizeGetHeight(int width) override;

	void setGeometry(QRect geometry) override;

	[[nodiscard]] QRect geometry() const override;

	[[nodiscard]] int firstLineBaseline() const override;

	void paint(
			Painter &p,
			const MarkdownArticlePaintContext &context) const override;

	[[nodiscard]] ClickHandlerPtr linkAt(QPoint point) const override;

	[[nodiscard]] MediaActivation activationAt(QPoint point) const override;

	[[nodiscard]] MediaBlockSelectionData selectionData() const override;

private:
	void resolveChannel();

	void rebuildLayout(int width);

	void applyGeometry();

	void handleJoinedChange();

	const uint64 _stableId = 0;
	const uint64 _channelId = 0;
	const QString _titleText;
	const QString _copyText;
	const QString _username;
	const std::shared_ptr<MediaRuntime> _mediaRuntime;
	std::shared_ptr<ChannelRuntime> _channelRuntime;
	MediaActivation _openActivation;
	MediaActivation _joinActivation;
	QRect _geometry;
	Ui::Text::String _titleLeaf;
	QString _actionText;
	ClickHandlerPtr _joinLink;
	QRect _titleRect;
	QRect _actionRect;
	rpl::lifetime _joinedChangesLifetime;
	int _layoutWidth = 1;
	int _height = 1;
	int _titleWidth = 1;
	int _actionWidth = 0;
	int _actionOuterWidth = 0;
	int _actionOuterHeight = 0;
	int _textHeight = 0;
	int _cardContentHeight = 0;
	int _firstLineBaseline = 0;
	bool _joinVisible = false;
	bool _channelResolved = false;
};

ChannelMediaBlock::ChannelMediaBlock(
	const PreparedChannelBlockData &prepared,
	std::shared_ptr<MediaRuntime> mediaRuntime)
: _stableId(prepared.id.value)
, _channelId(prepared.channelId)
, _titleText(prepared.title)
, _copyText(ChannelCopyText(prepared))
, _username(prepared.username)
, _mediaRuntime(std::move(mediaRuntime)) {
	resolveChannel();
	if (_mediaRuntime) {
		_mediaRuntime->channelJoinedChanges() | rpl::on_next([=](uint64 id) {
			if (id == _channelId) {
				handleJoinedChange();
			}
		}, _joinedChangesLifetime);
	}
}

uint64 ChannelMediaBlock::stableId() const {
	return _stableId;
}

int ChannelMediaBlock::resizeGetHeight(int width) {
	rebuildLayout(width);
	return _height;
}

void ChannelMediaBlock::setGeometry(QRect geometry) {
	if (_layoutWidth != std::max(geometry.width(), 1)) {
		rebuildLayout(geometry.width());
	}
	_geometry = QRect(
		geometry.topLeft(),
		QSize(_layoutWidth, _height));
	applyGeometry();
}

QRect ChannelMediaBlock::geometry() const {
	return _geometry;
}

int ChannelMediaBlock::firstLineBaseline() const {
	return _firstLineBaseline;
}

void ChannelMediaBlock::paint(
		Painter &p,
		const MarkdownArticlePaintContext &context) const {
	const auto visible = context.clip.intersected(_geometry);
	if (visible.isEmpty()) {
		return;
	}
	const auto visibleContext = ClippedContext(context, visible);
	const auto &layout = layoutStyle().channel;
	const auto &paint = PaintStyle(context, layoutStyle()).channel;
	const auto &buttonLayout = layout.button;
	const auto &buttonPaint = paint.button;
	p.save();
	p.setClipRect(visible);
	PaintCardSurface(
		p,
		_geometry,
		layout.border,
		paint.borderFg,
		paint.bg,
		layout.radius);
	p.setPen(paint.titleFg->c);
	PaintTextLeaf(
		p,
		_titleLeaf,
		visibleContext,
		_titleRect,
		_titleWidth);
	if (_joinVisible && !_actionRect.isEmpty()) {
		const auto innerRect = _actionRect.marginsRemoved(buttonLayout.padding);
		PaintCardSurface(
			p,
			_actionRect,
			buttonLayout.border,
			buttonPaint.borderFg,
			buttonPaint.bg,
			buttonLayout.radius);
		p.setPen(buttonPaint.textFg->c);
		p.setFont((ClickHandler::showAsActive(_joinLink)
			|| ClickHandler::showAsPressed(_joinLink))
			? buttonLayout.textStyle.font->underline()
			: buttonLayout.textStyle.font);
		p.drawText(innerRect, Qt::AlignCenter, _actionText);
	}
	p.restore();
}

ClickHandlerPtr ChannelMediaBlock::linkAt(QPoint point) const {
	if (_joinVisible
		&& _joinLink
		&& !_actionRect.isEmpty()
		&& _actionRect.contains(point)) {
		return _joinLink;
	}
	return nullptr;
}

MediaActivation ChannelMediaBlock::activationAt(QPoint point) const {
	if (!_geometry.contains(point)) {
		return {};
	}
	if (_joinVisible && !_actionRect.isEmpty() && _actionRect.contains(point)) {
		return _joinActivation;
	}
	return _openActivation;
}

MediaBlockSelectionData ChannelMediaBlock::selectionData() const {
	return {
		.copyText = _copyText,
	};
}

void ChannelMediaBlock::resolveChannel() {
	if (_channelResolved) {
		return;
	}
	_channelResolved = true;
	if (_mediaRuntime) {
		_channelRuntime = _mediaRuntime->resolveChannel(_channelId, _username);
	}
	_openActivation = {};
	_joinActivation = {};
	_joinLink = nullptr;
	if (_channelRuntime) {
		_openActivation.kind = MediaActivationKind::OpenChannel;
		_openActivation.channel = _channelRuntime;
	}
}

void ChannelMediaBlock::rebuildLayout(int width) {
	resolveChannel();
	const auto &card = layoutStyle().channel;
	const auto &padding = card.padding;
	const auto &button = card.button;
	const auto &buttonPadding = button.padding;
	const auto &titleStyle = card.titleStyle;
	const auto &actionStyle = button.textStyle;
	_layoutWidth = std::max(width, 1);
	_joinVisible = _channelRuntime && _channelRuntime->joinVisible();
	if (_joinVisible && _channelRuntime) {
		_joinActivation.kind = MediaActivationKind::JoinChannel;
		_joinActivation.channel = _channelRuntime;
		if (!_joinLink) {
			_joinLink = std::make_shared<LambdaClickHandler>(
				[runtime = _channelRuntime](ClickContext context) {
					runtime->join(context.button);
				});
		}
	} else {
		_joinActivation = {};
		_joinLink = nullptr;
	}

	auto actionTextHeight = 0;
	auto actionOuterWidth = 0;
	auto actionOuterHeight = 0;
	if (_joinVisible) {
		_actionText = tr::lng_iv_join_channel(tr::now);
		_actionWidth = std::max(actionStyle.font->width(_actionText), 1);
		actionTextHeight = TextLineHeight(actionStyle);
		actionOuterWidth = _actionWidth
			+ buttonPadding.left()
			+ buttonPadding.right();
		actionOuterHeight = actionTextHeight
			+ buttonPadding.top()
			+ buttonPadding.bottom();
	} else {
		_actionText = QString();
		_actionWidth = 0;
	}

	_titleWidth = std::max(
		_layoutWidth
			- padding.left()
			- (_joinVisible
				? (actionOuterWidth + card.buttonSkip)
				: padding.right()),
		1);
	SetPlainTextLeaf(
		&_titleLeaf,
		titleStyle,
		_titleText,
		_titleWidth);
	const auto titleHeight = LeafHeight(
		_titleLeaf,
		titleStyle,
		_titleWidth);

	_textHeight = titleHeight;
	_actionOuterWidth = actionOuterWidth;
	_actionOuterHeight = actionOuterHeight;
	_cardContentHeight = std::max(_textHeight, _actionOuterHeight);
	_height = padding.top() + _cardContentHeight + padding.bottom();
}

void ChannelMediaBlock::applyGeometry() {
	const auto &card = layoutStyle().channel;
	const auto &padding = card.padding;
	const auto &titleStyle = card.titleStyle;
	const auto contentLeft = _geometry.x() + padding.left();
	const auto textTop = _geometry.y() + padding.top()
		+ std::max((_cardContentHeight - _textHeight) / 2, 0);
	const auto titleHeight = LeafHeight(
		_titleLeaf,
		titleStyle,
		_titleWidth);
	_titleRect = QRect(
		contentLeft,
		textTop,
		_titleWidth,
		titleHeight);
	_firstLineBaseline = LeafFirstLineBaseline(
		_titleLeaf,
		_titleRect,
		titleStyle);
	if (_joinVisible && _actionOuterWidth > 0 && _actionOuterHeight > 0) {
		_actionRect = QRect(
			_geometry.x() + _layoutWidth - _actionOuterWidth,
			_geometry.y(),
			_actionOuterWidth,
			_height);
	} else {
		_actionRect = QRect();
	}
}

void ChannelMediaBlock::handleJoinedChange() {
	if (_geometry.width() <= 0 && _layoutWidth <= 0) {
		_channelResolved = false;
		resolveChannel();
		return;
	}
	const auto previousGeometry = _geometry;
	const auto previousTitleRect = _titleRect;
	const auto previousActionRect = _actionRect;
	const auto previousHeight = _height;
	const auto previousJoinVisible = _joinVisible;
	_channelResolved = false;
	resolveChannel();
	rebuildLayout((_geometry.width() > 0) ? _geometry.width() : _layoutWidth);
	if (_geometry.width() > 0) {
		_geometry = QRect(
			previousGeometry.topLeft(),
			QSize(_layoutWidth, _height));
		applyGeometry();
	}
	if (_height != previousHeight) {
		requestRelayout(QRect());
	} else if (_joinVisible != previousJoinVisible
		|| _titleRect != previousTitleRect
		|| _actionRect != previousActionRect) {
		requestRepaint(previousGeometry);
	}
}

class GroupedMediaBlock final : public MediaBlock {
public:
	GroupedMediaBlock(
		const PreparedGroupedMediaBlockData &prepared,
		std::shared_ptr<MediaRuntime> mediaRuntime);

	[[nodiscard]] uint64 stableId() const override;

	[[nodiscard]] int resizeGetHeight(int width) override;

	void setGeometry(QRect geometry) override;

	[[nodiscard]] QRect geometry() const override;

	[[nodiscard]] int firstLineBaseline() const override;

	void paint(
			Painter &p,
			const MarkdownArticlePaintContext &context) const override;

	[[nodiscard]] ClickHandlerPtr linkAt(QPoint point) const override;

	[[nodiscard]] MediaActivation activationAt(QPoint point) const override;

	[[nodiscard]] MediaBlockSelectionData selectionData() const override;

private:
	struct ItemState {
		PreparedMediaItemKind kind = PreparedMediaItemKind::Photo;
		uint64 id = 0;
		QSize original;
		QString copyText;
		QRect relativeRect;
		QRect rect;
		MediaActivation activation;
		std::shared_ptr<PhotoRuntime> photoRuntime;
		std::shared_ptr<DocumentRuntime> documentRuntime;
		std::shared_ptr<Ui::DynamicImage> thumbnailImage;
		std::shared_ptr<Ui::DynamicImage> fullImage;
		std::shared_ptr<Ui::DynamicImage> previousThumbnailImage;
		std::shared_ptr<Ui::DynamicImage> previousFullImage;
		QSize requestedSize;
		bool spoiler = false;
		bool runtimeResolved = false;
	};

	void rebuildLayout(int width);

	void clearCollageLayout();

	[[nodiscard]] int fallbackHeight(int width) const;

	void applyGeometry();

	void resolveRuntime(ItemState &item);

	void resolveImages(ItemState &item);

	void subscribeImage(
			const std::shared_ptr<Ui::DynamicImage> &image,
			const ItemState *item);

	void handleImageUpdate(int index);

	void paintItem(
		Painter &p,
		const ItemState &item,
		const style::Markdown &st) const;

	[[nodiscard]] bool itemLoading(const ItemState &item) const;

	[[nodiscard]] double itemProgress(const ItemState &item) const;

	void paintActiveItem(Painter &p, const style::Markdown &st) const;

	[[nodiscard]] bool paintActiveItemWithBlurredBackground(
		Painter &p,
		const ItemState &item) const;

	void paintNavigation(
		Painter &p,
		const style::Markdown &st) const;

	void ensureNavigationLinks();

	void updateNavigationRects();

	void stepActiveIndex(int delta);

	[[nodiscard]] int activeItemForegroundHeight(int width) const;

	[[nodiscard]] int activeItemHeight(int width) const;

	[[nodiscard]] int slideshowNavigationFrameHeight(int width) const;

	[[nodiscard]] ItemState *activeItem();

	[[nodiscard]] const ItemState *activeItem() const;

	const uint64 _stableId = 0;
	const PreparedGroupedMediaIntent _intent = PreparedGroupedMediaIntent::Collage;
	const QString _copyText;
	const QSize _fallbackSize;
	const std::shared_ptr<MediaRuntime> _mediaRuntime;
	std::vector<ItemState> _items;
	QRect _geometry;
	QRect _previousRect;
	QRect _nextRect;
	ClickHandlerPtr _previousLink;
	ClickHandlerPtr _nextLink;
	int _layoutWidth = 1;
	int _contentWidth = 1;
	int _height = 1;
	int _activeIndex = 0;
	bool _useCollageLayout = false;
};

GroupedMediaBlock::GroupedMediaBlock(
	const PreparedGroupedMediaBlockData &prepared,
	std::shared_ptr<MediaRuntime> mediaRuntime)
: _stableId(prepared.id.value)
, _intent(prepared.intent)
, _copyText(GroupedMediaCopyText(prepared))
, _fallbackSize(prepared.items.empty()
	? QSize()
	: QSize(
		std::max(prepared.items.front().media.width, 1),
		std::max(prepared.items.front().media.height, 1)))
, _mediaRuntime(std::move(mediaRuntime)) {
	_items.reserve(prepared.items.size());
	for (const auto &item : prepared.items) {
		auto state = ItemState();
		state.kind = item.media.kind;
		state.id = item.media.id;
		state.original = QSize(
			std::max(item.media.width, 1),
			std::max(item.media.height, 1));
		state.copyText = GroupedMediaItemCopyText(item.media.kind);
		state.spoiler = item.media.spoiler;
		_items.push_back(std::move(state));
	}
}

uint64 GroupedMediaBlock::stableId() const {
	return _stableId;
}

int GroupedMediaBlock::resizeGetHeight(int width) {
	rebuildLayout(width);
	return _height;
}

void GroupedMediaBlock::setGeometry(QRect geometry) {
	rebuildLayout(geometry.width());
	const auto contentWidth = std::max(_contentWidth, 1);
	_height = std::max(_height, 1);
	_geometry = QRect(
		geometry.topLeft()
			+ QPoint(
				std::max((geometry.width() - contentWidth) / 2, 0),
				0),
		QSize(contentWidth, _height));
	ensureNavigationLinks();
	applyGeometry();
}

QRect GroupedMediaBlock::geometry() const {
	return _geometry;
}

int GroupedMediaBlock::firstLineBaseline() const {
	return _geometry.y();
}

void GroupedMediaBlock::paint(
		Painter &p,
		const MarkdownArticlePaintContext &context) const {
	const auto visible = context.clip.intersected(_geometry);
	if (visible.isEmpty()) {
		return;
	}
	const auto &layout = layoutStyle();
	const auto &st = PaintStyle(context, layout);
	const auto &layoutGrouped = layout.groupedMedia;
	p.save();
	p.setClipRect(visible);
	const auto path = RoundedRectPath(_geometry, layoutGrouped.radius);
	p.setClipPath(path, Qt::IntersectClip);
	if (_intent == PreparedGroupedMediaIntent::Slideshow) {
		paintActiveItem(p, st);
		paintNavigation(p, st);
	} else if (_useCollageLayout) {
		for (const auto &item : _items) {
			paintItem(p, item, st);
		}
	}
	p.restore();
}

ClickHandlerPtr GroupedMediaBlock::linkAt(QPoint point) const {
	if (_intent == PreparedGroupedMediaIntent::Slideshow) {
		if (_previousRect.contains(point)) {
			return _previousLink;
		} else if (_nextRect.contains(point)) {
			return _nextLink;
		}
	}
	return nullptr;
}

MediaActivation GroupedMediaBlock::activationAt(QPoint point) const {
	if (!_geometry.contains(point)) {
		return {};
	} else if (_intent == PreparedGroupedMediaIntent::Slideshow) {
		if (_previousRect.contains(point) || _nextRect.contains(point)) {
			return {};
		}
		if (const auto item = activeItem()) {
			return item->activation;
		}
		return {};
	} else if (!_useCollageLayout) {
		return {};
	}
	for (const auto &item : _items) {
		if (item.rect.contains(point)) {
			return item.activation;
		}
	}
	return {};
}

MediaBlockSelectionData GroupedMediaBlock::selectionData() const {
	return {
		.copyText = _copyText,
	};
}

void GroupedMediaBlock::rebuildLayout(int width) {
	_layoutWidth = std::max(width, 1);
	_contentWidth = _layoutWidth;
	_height = (_intent == PreparedGroupedMediaIntent::Slideshow)
		? activeItemHeight(_layoutWidth)
		: fallbackHeight(_layoutWidth);
	_useCollageLayout = false;
	for (auto &item : _items) {
		item.relativeRect = QRect();
	}
	if (_items.empty()) {
		return;
	} else if (_intent == PreparedGroupedMediaIntent::Slideshow) {
		return;
	} else if (_intent != PreparedGroupedMediaIntent::Collage) {
		return;
	}

	const auto spacing = layoutStyle().groupedMedia.itemSkip;
	auto top = 0;
	auto maxWidth = 0;
	auto index = 0;
	const auto count = int(_items.size());
	while (index != count) {
		const auto batchCount = std::min(
			count - index,
			kMaxGroupedMediaLayoutItems);
		auto sizes = std::vector<QSize>();
		sizes.reserve(batchCount);
		for (auto i = 0; i != batchCount; ++i) {
			sizes.push_back(_items[index + i].original);
		}
		const auto layout = Ui::LayoutMediaGroup(
			sizes,
			_layoutWidth,
			GroupedMediaMinWidth(_layoutWidth, spacing),
			spacing);
		if (int(layout.size()) != batchCount) {
			clearCollageLayout();
			return;
		}
		for (auto i = 0; i != batchCount; ++i) {
			_items[index + i].relativeRect = layout[i].geometry.translated(0, top);
		}
		maxWidth = std::max(maxWidth, GroupedMediaLayoutWidth(layout));
		top += GroupedMediaLayoutHeight(layout);
		index += batchCount;
		if (index != count) {
			top += spacing;
		}
	}
	_contentWidth = std::max(maxWidth, 1);
	_height = std::max(top, 1);
	_useCollageLayout = true;
}

void GroupedMediaBlock::clearCollageLayout() {
	_contentWidth = _layoutWidth;
	_height = fallbackHeight(_layoutWidth);
	_useCollageLayout = false;
	for (auto &item : _items) {
		item.relativeRect = QRect();
		item.rect = QRect();
	}
}

int GroupedMediaBlock::fallbackHeight(int width) const {
	if (_fallbackSize.isEmpty()) {
		return std::max(layoutStyle().placeholder.minHeight, 1);
	}
	return MediaHeightForWidth(
		width,
		_fallbackSize.width(),
		_fallbackSize.height());
}

void GroupedMediaBlock::applyGeometry() {
	_previousRect = QRect();
	_nextRect = QRect();
	if (_intent == PreparedGroupedMediaIntent::Slideshow) {
		for (auto &item : _items) {
			item.rect = QRect();
		}
		if (auto item = activeItem()) {
			item->rect = _geometry;
			resolveRuntime(*item);
			resolveImages(*item);
		}
		updateNavigationRects();
		return;
	} else if (!_useCollageLayout) {
		for (auto &item : _items) {
			item.rect = QRect();
		}
		return;
	}
	for (auto &item : _items) {
		item.rect = item.relativeRect.isEmpty()
			? QRect()
			: item.relativeRect.translated(_geometry.topLeft());
		resolveRuntime(item);
		resolveImages(item);
	}
}

void GroupedMediaBlock::resolveRuntime(ItemState &item) {
	if (item.runtimeResolved) {
		return;
	}
	item.runtimeResolved = true;
	if (item.kind == PreparedMediaItemKind::Photo) {
		if (_mediaRuntime) {
			item.photoRuntime = _mediaRuntime->resolvePhoto(item.id);
		}
		if (item.photoRuntime) {
			item.activation.kind = MediaActivationKind::Photo;
			item.activation.photo = item.photoRuntime;
		}
	} else {
		if (_mediaRuntime) {
			item.documentRuntime = _mediaRuntime->resolveDocument(item.id);
		}
		if (item.documentRuntime) {
			item.activation.kind = MediaActivationKind::Document;
			item.activation.document = item.documentRuntime;
		}
	}
}

void GroupedMediaBlock::resolveImages(ItemState &item) {
	if (item.rect.isEmpty()) {
		return;
	}
	if (item.photoRuntime) {
		RefreshResolvedImages(
			item.photoRuntime,
			item.rect.size(),
			&item.requestedSize,
			&item.thumbnailImage,
			&item.previousThumbnailImage,
			&item.fullImage,
			&item.previousFullImage,
			[&](const std::shared_ptr<Ui::DynamicImage> &image) {
				subscribeImage(image, &item);
			});
	} else if (item.documentRuntime) {
		RefreshResolvedImages(
			item.documentRuntime,
			item.rect.size(),
			&item.requestedSize,
			&item.thumbnailImage,
			&item.previousThumbnailImage,
			&item.fullImage,
			&item.previousFullImage,
			[&](const std::shared_ptr<Ui::DynamicImage> &image) {
				subscribeImage(image, &item);
			});
	}
}

void GroupedMediaBlock::subscribeImage(
		const std::shared_ptr<Ui::DynamicImage> &image,
		const ItemState *item) {
	if (!image || !item) {
		return;
	}
	const auto index = int(item - _items.data());
	const auto weak = std::weak_ptr<GroupedMediaBlock>(
		std::static_pointer_cast<GroupedMediaBlock>(shared_from_this()));
	image->subscribeToUpdates([weak, index] {
		if (const auto block = weak.lock()) {
			block->handleImageUpdate(index);
		}
	});
}

void GroupedMediaBlock::handleImageUpdate(int index) {
	if (index < 0 || index >= int(_items.size())) {
		return;
	} else if (_intent == PreparedGroupedMediaIntent::Slideshow) {
		if (index == _activeIndex) {
			requestRepaint(_geometry);
		}
		return;
	}
	requestRepaint(_items[index].rect);
}

void GroupedMediaBlock::paintItem(
		Painter &p,
		const ItemState &item,
		const style::Markdown &st) const {
	if (item.rect.isEmpty()) {
		return;
	}
	const auto painted = PaintResolvedImages(
			p,
			item.rect,
			item.thumbnailImage,
			item.fullImage,
			item.previousThumbnailImage,
			item.previousFullImage);
	if (painted && item.spoiler) {
		PaintImageSpoiler(p, item.rect);
	}
	if (itemLoading(item)) {
		PaintPhotoProgress(
			p,
			item.rect,
			st.photo,
			itemProgress(item));
	}
}

bool GroupedMediaBlock::itemLoading(const ItemState &item) const {
	if (item.photoRuntime) {
		return item.photoRuntime->loading();
	} else if (item.documentRuntime) {
		return item.documentRuntime->loading();
	}
	return false;
}

double GroupedMediaBlock::itemProgress(const ItemState &item) const {
	if (item.photoRuntime) {
		return item.photoRuntime->progress();
	} else if (item.documentRuntime) {
		return item.documentRuntime->progress();
	}
	return 0.;
}

void GroupedMediaBlock::paintActiveItem(
		Painter &p,
		const style::Markdown &st) const {
	const auto item = activeItem();
	if (!item) {
		return;
	}
	const auto foregroundHeight = activeItemForegroundHeight(_geometry.width());
	const auto painted = (foregroundHeight < _geometry.height())
		? paintActiveItemWithBlurredBackground(p, *item)
		: PaintResolvedImages(
			p,
			_geometry,
			item->thumbnailImage,
			item->fullImage,
			item->previousThumbnailImage,
			item->previousFullImage);
	if (painted && item->spoiler) {
		PaintImageSpoiler(p, _geometry);
	}
	if (itemLoading(*item)) {
		PaintPhotoProgress(
			p,
			_geometry,
			st.photo,
			itemProgress(*item));
	}
}

bool GroupedMediaBlock::paintActiveItemWithBlurredBackground(
		Painter &p,
		const ItemState &item) const {
	const auto foregroundHeight = std::clamp(
		activeItemForegroundHeight(_geometry.width()),
		1,
		std::max(_geometry.height(), 1));
	if (foregroundHeight >= _geometry.height()) {
		return false;
	}
	const auto size = std::max(_geometry.width(), _geometry.height());
	auto large = ResolvedImageFrame(
		item.fullImage,
		item.previousFullImage,
		size);
	if (large.isNull()) {
		large = ResolvedImageFrame(
			item.thumbnailImage,
			item.previousThumbnailImage,
			size);
	}
	if (large.isNull()) {
		return false;
	}
	auto blurred = ResolvedImageFrame(
		item.thumbnailImage,
		item.previousThumbnailImage,
		size);
	if (blurred.isNull()) {
		blurred = large;
	}
	const auto prepared = PrepareWithBlurredBackground(
		_geometry.size(),
		QSize(_geometry.width(), foregroundHeight),
		std::move(large),
		std::move(blurred));
	if (prepared.isNull()) {
		return false;
	}
	p.drawImage(_geometry.topLeft(), prepared);
	return true;
}

void GroupedMediaBlock::paintNavigation(
		Painter &p,
		const style::Markdown &st) const {
	if ((_intent != PreparedGroupedMediaIntent::Slideshow)
		|| (_items.size() < 2)) {
		return;
	}
	const auto &style = st.groupedMedia;
	if (!_previousRect.isEmpty()) {
		const auto active = ClickHandler::showAsActive(_previousLink)
			|| ClickHandler::showAsPressed(_previousLink);
		PaintRoundButton(
			p,
			_previousRect,
			active ? style.navButtonBgOver : style.navButtonBg,
			active ? style.navPreviousIconOver : style.navPreviousIcon);
	}
	if (!_nextRect.isEmpty()) {
		const auto active = ClickHandler::showAsActive(_nextLink)
			|| ClickHandler::showAsPressed(_nextLink);
		PaintRoundButton(
			p,
			_nextRect,
			active ? style.navButtonBgOver : style.navButtonBg,
			active ? style.navNextIconOver : style.navNextIcon);
	}
}

void GroupedMediaBlock::ensureNavigationLinks() {
	if ((_intent != PreparedGroupedMediaIntent::Slideshow)
		|| (_items.size() < 2)
		|| (_previousLink && _nextLink)) {
		return;
	}
	const auto weak = std::weak_ptr<GroupedMediaBlock>(
		std::static_pointer_cast<GroupedMediaBlock>(shared_from_this()));
	_previousLink = std::make_shared<LambdaClickHandler>([weak] {
		if (const auto block = weak.lock()) {
			block->stepActiveIndex(-1);
		}
	});
	_nextLink = std::make_shared<LambdaClickHandler>([weak] {
		if (const auto block = weak.lock()) {
			block->stepActiveIndex(1);
		}
	});
}

void GroupedMediaBlock::updateNavigationRects() {
	if ((_intent != PreparedGroupedMediaIntent::Slideshow)
		|| (_items.size() < 2)
		|| _geometry.isEmpty()) {
		return;
	}
	const auto &style = layoutStyle().groupedMedia;
	const auto frameHeight = slideshowNavigationFrameHeight(_geometry.width());
	const auto availableWidth = std::max(
		(_geometry.width() - 2 * style.navButtonSkip) / 2,
		0);
	const auto size = std::min({
		style.navButtonSize,
		std::max(frameHeight, 0),
		availableWidth,
	});
	if (size <= 0) {
		return;
	}
	const auto top = _geometry.y() + std::max((frameHeight - size) / 2, 0);
	_previousRect = QRect(
		_geometry.x() + style.navButtonSkip,
		top,
		size,
		size);
	_nextRect = QRect(
		_geometry.x() + _geometry.width() - style.navButtonSkip - size,
		top,
		size,
		size);
}

void GroupedMediaBlock::stepActiveIndex(int delta) {
	if ((_intent != PreparedGroupedMediaIntent::Slideshow)
		|| (_items.size() < 2)) {
		return;
	}
	const auto count = int(_items.size());
	const auto next = (int(_activeIndex) + delta % count + count) % count;
	if (next == _activeIndex) {
		return;
	}
	const auto width = std::max(
		(_geometry.width() > 0) ? _geometry.width() : _layoutWidth,
		1);
	const auto previousGeometry = _geometry;
	const auto previousHeight = activeItemHeight(width);
	_activeIndex = next;
	const auto nextHeight = activeItemHeight(width);
	_height = nextHeight;
	if (_geometry.isEmpty()) {
		return;
	} else if (previousHeight != nextHeight) {
		_geometry = QRect(
			previousGeometry.topLeft(),
			QSize(previousGeometry.width(), nextHeight));
		applyGeometry();
		requestRelayout(QRect());
		return;
	}
	applyGeometry();
	requestRepaint(previousGeometry.united(_geometry));
}

int GroupedMediaBlock::activeItemForegroundHeight(int width) const {
	if (const auto item = activeItem()) {
		return MediaHeightForWidth(
			width,
			item->original.width(),
			item->original.height());
	}
	return fallbackHeight(width);
}

int GroupedMediaBlock::activeItemHeight(int width) const {
	return std::max(
		activeItemForegroundHeight(width),
		std::max(layoutStyle().groupedMedia.slideshowMinHeight, 1));
}

int GroupedMediaBlock::slideshowNavigationFrameHeight(int width) const {
	auto result = std::numeric_limits<int>::max();
	for (const auto &item : _items) {
		result = std::min(
			result,
			MediaHeightForWidth(
				width,
				item.original.width(),
				item.original.height()));
	}
	if (result == std::numeric_limits<int>::max()) {
		result = fallbackHeight(width);
	}
	return std::max(
		result,
		std::max(layoutStyle().groupedMedia.slideshowMinHeight, 1));
}

GroupedMediaBlock::ItemState *GroupedMediaBlock::activeItem() {
	return (_activeIndex >= 0 && _activeIndex < int(_items.size()))
		? &_items[_activeIndex]
		: nullptr;
}

const GroupedMediaBlock::ItemState *GroupedMediaBlock::activeItem() const {
	return (_activeIndex >= 0 && _activeIndex < int(_items.size()))
		? &_items[_activeIndex]
		: nullptr;
}

} // namespace

MediaBlock::~MediaBlock() = default;

void MediaBlock::setHost(MediaBlockHost *host) {
	if (_host == host) {
		return;
	}
	_host = host;
	hostUpdated();
}

MediaBlockHost *MediaBlock::host() const {
	return _host;
}

bool MediaBlock::hasHeavyPart() const {
	return false;
}

void MediaBlock::unloadHeavyPart() {
}

void MediaBlock::hideSpoilers() {
}

void MediaBlock::setLayoutStyle(const style::Markdown &st) {
	if (_st == &st) {
		return;
	}
	_st = &st;
	layoutStyleUpdated();
}

const style::Markdown &MediaBlock::layoutStyle() const {
	return *_st;
}

void MediaBlock::requestRepaint(QRect articleRect) const {
	if (_host) {
		_host->requestRepaint(articleRect);
	}
}

void MediaBlock::requestRelayout(QRect articleRect) const {
	if (_host) {
		_host->requestRelayout(articleRect);
	}
}

void MediaBlock::layoutStyleUpdated() {
}

void MediaBlock::hostUpdated() {
}

std::shared_ptr<MediaBlock> CreatePhotoMediaBlock(
		const PreparedPhotoBlockData &prepared,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &st) {
	if (mediaRuntime
		&& prepared.viewerOpen
		&& prepared.urlOverride.isEmpty()) {
		mediaRuntime->registerPhoto(prepared.photoId, prepared.caption);
		if (const auto hosted = mediaRuntime->hostedMediaBlockFactory()) {
			if (const auto block = hosted->createPhoto(prepared)) {
				block->setLayoutStyle(st);
				return block;
			}
		}
	}
	auto result = std::make_shared<ImageBackedMediaBlock>(prepared, mediaRuntime);
	result->setLayoutStyle(st);
	return result;
}

std::shared_ptr<MediaBlock> CreateVideoMediaBlock(
		const PreparedVideoBlockData &prepared,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &st) {
	if (mediaRuntime) {
		mediaRuntime->registerDocument(prepared.media.id, prepared.caption);
		if (const auto hosted = mediaRuntime->hostedMediaBlockFactory()) {
			if (const auto block = hosted->createVideo(prepared)) {
				block->setLayoutStyle(st);
				return block;
			}
		}
	}
	auto result = std::make_shared<ImageBackedMediaBlock>(prepared, mediaRuntime);
	result->setLayoutStyle(st);
	return result;
}

std::shared_ptr<MediaBlock> CreateAudioMediaBlock(
		const PreparedAudioBlockData &prepared,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &st) {
	if (mediaRuntime) {
		if (const auto hosted = mediaRuntime->hostedMediaBlockFactory()) {
			if (const auto block = hosted->createAudio(prepared)) {
				block->setLayoutStyle(st);
				return block;
			}
		}
	}
	return nullptr;
}

std::shared_ptr<MediaBlock> CreateMapMediaBlock(
		const PreparedMapBlockData &prepared,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &st) {
	if (mediaRuntime) {
		if (const auto hosted = mediaRuntime->hostedMediaBlockFactory()) {
			if (const auto block = hosted->createMap(prepared)) {
				block->setLayoutStyle(st);
				return block;
			}
		}
	}
	auto result = std::make_shared<ImageBackedMediaBlock>(prepared, mediaRuntime);
	result->setLayoutStyle(st);
	return result;
}

std::shared_ptr<MediaBlock> CreateChannelMediaBlock(
		const PreparedChannelBlockData &prepared,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &st) {
	auto result = std::make_shared<ChannelMediaBlock>(prepared, mediaRuntime);
	result->setLayoutStyle(st);
	return result;
}

std::shared_ptr<MediaBlock> CreateGroupedMediaBlock(
		const PreparedGroupedMediaBlockData &prepared,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &st) {
	if (mediaRuntime) {
		for (const auto &item : prepared.items) {
			switch (item.media.kind) {
			case PreparedMediaItemKind::Photo:
				mediaRuntime->registerPhoto(
					item.media.id,
					prepared.caption);
				break;
			case PreparedMediaItemKind::Document:
				mediaRuntime->registerDocument(
					item.media.id,
					prepared.caption);
				break;
			}
		}
		if (const auto hosted = mediaRuntime->hostedMediaBlockFactory()) {
			if (const auto block = hosted->createGroupedMedia(prepared)) {
				block->setLayoutStyle(st);
				return block;
			}
		}
	}
	auto result = std::make_shared<GroupedMediaBlock>(prepared, mediaRuntime);
	result->setLayoutStyle(st);
	return result;
}

} // namespace Iv::Markdown
