// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/components/image_view.h"

#include "ayu/features/message_shot/message_shot.h"
#include "ayu/utils/telegram_helpers.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_chat.h"
#include "ui/painter.h"

ImageView::ImageView(QWidget *parent)
	: RpWidget(parent) {
}

void ImageView::setImage(const QImage &image) {
	if (this->image == image) {
		return;
	}

	const auto set = [=]
	{
		this->prevImage = this->image;
		this->image = image;

		if (!this->prevImage.isNull()
			&& !image.isNull()
			&& this->prevImage.size() == image.size()) {
			computeDiffImages(this->prevImage, image);
		} else {
			this->baseImage = QImage();
			this->prevDiffImage = QImage();
			this->newDiffImage = QImage();
		}

		const auto size = image.size() / style::DevicePixelRatio();
		setMinimumSize(size.grownBy(st::imageViewInnerPadding));

		if (this->animation.animating()) {
			this->animation.stop();
		}

		if (this->prevImage.isNull()) {
			update();
			return;
		}

		this->animation.start(
			[=]
			{
				update();
			},
			0.0,
			1.0,
			300,
			anim::easeInCubic);
	};

	if (this->image.isNull()) {
		set();
		return;
	}

	dispatchToMainThread(set, 100);
}

void ImageView::computeDiffImages(const QImage &prev, const QImage &curr) {
	const auto prevConverted = prev.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	const auto currConverted = curr.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	const auto w = prevConverted.width();
	const auto h = prevConverted.height();

	auto base = currConverted.copy();
	auto prevDiff = prevConverted.copy();
	auto newDiff = currConverted.copy();

	for (auto y = 0; y < h; ++y) {
		const auto *prevLine = reinterpret_cast<const QRgb *>(prevConverted.constScanLine(y));
		const auto *currLine = reinterpret_cast<const QRgb *>(currConverted.constScanLine(y));
		auto *baseLine = reinterpret_cast<QRgb *>(base.scanLine(y));
		auto *prevDiffLine = reinterpret_cast<QRgb *>(prevDiff.scanLine(y));
		auto *newDiffLine = reinterpret_cast<QRgb *>(newDiff.scanLine(y));

		for (auto x = 0; x < w; ++x) {
			if (prevLine[x] == currLine[x]) {
				prevDiffLine[x] = 0;
				newDiffLine[x] = 0;
			} else {
				baseLine[x] = 0;
			}
		}
	}

	this->baseImage = base;
	this->prevDiffImage = prevDiff;
	this->newDiffImage = newDiff;
}

QImage ImageView::getImage() const {
	return image;
}

void ImageView::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto brush = QBrush(AyuFeatures::MessageShot::makeDefaultBackgroundColor());

	QPainterPath path;
	path.addRoundedRect(rect(), st::roundRadiusLarge, st::roundRadiusLarge);

	p.fillPath(path, brush);

	if (!baseImage.isNull()) {
		const auto realRect = rect().marginsRemoved(st::imageViewInnerPadding);

		const auto resizedRect = QRect(
			(realRect.width() - image.width() / style::DevicePixelRatio()) / 2 + st::imageViewInnerPadding.left(),
			(realRect.height() - image.height() / style::DevicePixelRatio()) / 2 + st::imageViewInnerPadding.top(),
			image.width() / style::DevicePixelRatio(),
			image.height() / style::DevicePixelRatio());

		p.drawImage(resizedRect, baseImage);

		const auto t = animation.value(1.0);

		if (t < 1.0) {
			p.setOpacity(1.0 - t);
			p.drawImage(resizedRect, prevDiffImage);
			p.setOpacity(1.0);
		}

		if (t > 0.0) {
			p.setOpacity(t);
			p.drawImage(resizedRect, newDiffImage);
			p.setOpacity(1.0);
		}
	} else {
		if (!prevImage.isNull()) {
			const auto realRect = rect().marginsRemoved(st::imageViewInnerPadding);

			const auto resizedRect = QRect(
				(realRect.width() - prevImage.width() / style::DevicePixelRatio()) / 2 + st::imageViewInnerPadding.left(),
				(realRect.height() - prevImage.height() / style::DevicePixelRatio()) / 2 + st::imageViewInnerPadding.top(),
				prevImage.width() / style::DevicePixelRatio(),
				prevImage.height() / style::DevicePixelRatio());

			const auto opacity = 1.0 - animation.value(1.0);
			p.setOpacity(opacity);
			p.drawImage(resizedRect, prevImage);
			p.setOpacity(1.0);
		}

		if (!image.isNull()) {
			const auto realRect = rect().marginsRemoved(st::imageViewInnerPadding);

			const auto resizedRect = QRect(
				(realRect.width() - image.width() / style::DevicePixelRatio()) / 2 + st::imageViewInnerPadding.left(),
				(realRect.height() - image.height() / style::DevicePixelRatio()) / 2 + st::imageViewInnerPadding.top(),
				image.width() / style::DevicePixelRatio(),
				image.height() / style::DevicePixelRatio());

			const auto opacity = animation.value(1.0);
			p.setOpacity(opacity);
			p.drawImage(resizedRect, image);
			p.setOpacity(1.0);
		}
	}
}

void ImageView::mousePressEvent(QMouseEvent *e) {
}
