// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#include "image_view.h"

#include "styles/style_ayu_styles.h"

#include "ayu/utils/telegram_helpers.h"
#include "styles/style_chat.h"
#include "ui/painter.h"
#include "window/themes/window_theme.h"

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

		setMinimumSize(image.size().grownBy(st::imageViewInnerPadding));

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

QImage ImageView::getImage() const {
	return image;
}

void ImageView::paintEvent(QPaintEvent *e) {
	Painter p(this);
	//	PainterHighQualityEnabler hq(p);

	auto brush = QBrush(st::boxBg); // copy

	if (Window::Theme::IsNightMode()) {
		brush.setColor(brush.color().lighter(120));
	} else {
		brush.setColor(brush.color().darker(105));
	}

	QPainterPath path;
	path.addRoundedRect(rect(), st::bubbleRadiusLarge, st::bubbleRadiusLarge);

	p.fillPath(path, brush);

	if (!prevImage.isNull()) {
		const auto realRect = rect().marginsRemoved(st::imageViewInnerPadding);

		const auto resizedRect = QRect(
			(realRect.width() - prevImage.width()) / 2 + st::imageViewInnerPadding.left(),
			(realRect.height() - prevImage.height()) / 2 + st::imageViewInnerPadding.top(),
			prevImage.width(),
			prevImage.height());

		const auto opacity = 1.0 - animation.value(1.0);
		p.setOpacity(opacity);
		p.drawImage(resizedRect, prevImage);
		p.setOpacity(1.0);
	}

	if (!image.isNull()) {
		const auto realRect = rect().marginsRemoved(st::imageViewInnerPadding);

		const auto resizedRect = QRect(
			(realRect.width() - image.width()) / 2 + st::imageViewInnerPadding.left(),
			(realRect.height() - image.height()) / 2 + st::imageViewInnerPadding.top(),
			image.width(),
			image.height());

		const auto opacity = animation.value(1.0);
		p.setOpacity(opacity);
		p.drawImage(resizedRect, image);
		p.setOpacity(1.0);
	}
}

void ImageView::mousePressEvent(QMouseEvent *e) {
}
