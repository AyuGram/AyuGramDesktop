// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/ayu_userpic.h"

#include "ayu/ayu_settings.h"
#include "ayu/ayu_ui_settings.h"
#include "media/streaming/media_streaming_common.h"
#include "ui/image/image_prepare.h"
#include "ui/style/style_core.h"
#include "base/algorithm.h"

#include <QRectF>

namespace AyuUserpic {

bool ShouldOverrideShape(Ui::PeerUserpicShape shape) {
	using Shape = Ui::PeerUserpicShape;
	switch (shape) {
	case Shape::Circle:
	case Shape::Auto:
		return true;
	case Shape::Monoforum:
	case Shape::Forum:
		return AyuSettings::getInstance().singleCornerRadius();
	}
	return false;
}

int ComputeRadius(int pixelSize) {
	const auto corners = AyuUiSettings::getAvatarCorners();
	if (corners >= AyuUiSettings::kMaxAvatarCorners) return pixelSize / 2;
	if (corners <= 0) return 0;
	return int(double(corners) / AyuUiSettings::kMaxAvatarCorners * pixelSize / 2.0);
}

double ComputeRadiusF(double size) {
	const auto corners = AyuUiSettings::getAvatarCorners();
	if (corners >= AyuUiSettings::kMaxAvatarCorners) return size / 2.0;
	if (corners <= 0) return 0.0;
	return double(corners) / AyuUiSettings::kMaxAvatarCorners * size / 2.0;
}

bool IsCircle() {
	return AyuUiSettings::getAvatarCorners() >= AyuUiSettings::kMaxAvatarCorners;
}

uint8 PackedState() {
	return uint8(AyuUiSettings::getAvatarCorners() & 0x1F)
		| (AyuSettings::getInstance().singleCornerRadius() ? 0x20 : 0);
}

void PaintShape(QPainter &p, int x, int y, int size) {
	const auto corners = AyuUiSettings::getAvatarCorners();
	if (corners >= AyuUiSettings::kMaxAvatarCorners) {
		p.drawEllipse(x, y, size, size);
	} else if (corners <= 0) {
		p.drawRect(x, y, size, size);
	} else {
		const auto r = double(corners) / AyuUiSettings::kMaxAvatarCorners * size / 2.0;
		p.drawRoundedRect(x, y, size, size, r, r);
	}
}

void PaintShape(QPainter &p, const QRectF &rect) {
	const auto corners = AyuUiSettings::getAvatarCorners();
	if (corners >= AyuUiSettings::kMaxAvatarCorners) {
		p.drawEllipse(rect);
	} else if (corners <= 0) {
		p.drawRect(rect);
	} else {
		const auto r = double(corners) / AyuUiSettings::kMaxAvatarCorners
			* std::min(rect.width(), rect.height()) / 2.0;
		p.drawRoundedRect(rect, r, r);
	}
}

QPointF OnlineBadgePosition(int photoSize, double badgeSize, double stroke) {
	const auto corners = AyuUiSettings::getAvatarCorners();
	const auto r = double(corners) / AyuUiSettings::kMaxAvatarCorners * photoSize / 2.0;
	const auto edge = photoSize - r * (1.0 - std::cos(M_PI / 4.0));
	const auto maxPos = (stroke > 0)
		? photoSize - badgeSize / 2.0 - (badgeSize / 2.0 + stroke / 2.0) / std::sqrt(2.0)
		: double(photoSize) - badgeSize;
	const auto pos = std::min(edge - badgeSize / 2.0, maxPos);
	return QPointF(pos, pos);
}

QRect OnlineBadgeRect(int photoSize, int badgeSize, int stroke) {
	const auto pos = OnlineBadgePosition(photoSize, badgeSize, stroke);
	return QRect(
		int(base::SafeRound(pos.x())),
		int(base::SafeRound(pos.y())),
		badgeSize,
		badgeSize);
}

void ApplyFrameRounding(
		::Media::Streaming::FrameRequest &request,
		std::array<QImage, 4> &cornersCache,
		QImage &ellipseCache,
		QSize size) {
	const auto minSide = std::min(size.width(), size.height());
	const auto r = ComputeRadius(minSide);
	const auto ratio = style::DevicePixelRatio();
	if (r > 0 && r < minSide / 2) {
		if (cornersCache[0].width() != r * ratio) {
			cornersCache = Images::CornersMask(r);
		}
		request.rounding = Images::CornersMaskRef(cornersCache);
	} else if (r >= minSide / 2) {
		if (ellipseCache.size() != request.outer) {
			ellipseCache = Images::EllipseMask(size);
		}
		request.mask = ellipseCache;
	}
}

} // namespace AyuUserpic
