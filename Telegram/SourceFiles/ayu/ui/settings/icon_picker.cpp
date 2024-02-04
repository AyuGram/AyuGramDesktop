// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "icon_picker.h"
#include "tray.h"
#include "ayu/ayu_settings.h"
#include "core/application.h"
#include "styles/style_layers.h"

#include "ayu/ui/ayu_logo.h"
#include "main/main_domain.h"
#include "styles/style_ayu_styles.h"
#include "ui/painter.h"
#include "window/main_window.h"

#ifdef Q_OS_WIN
#include "ayu/utils/windows_utils.h"
#endif

const QVector icons{
	AyuAssets::DEFAULT_ICON,
	AyuAssets::ALT_ICON,
	AyuAssets::DISCORD_ICON,
	AyuAssets::SPOTIFY_ICON,
	AyuAssets::EXTERA_ICON,
	AyuAssets::NOTHING_ICON,
	AyuAssets::BARD_ICON,
	AyuAssets::YAPLUS_ICON
};

const auto rows = static_cast<int>(icons.size()) / 4 + std::min(1, static_cast<int>(icons.size()) % 4);

void drawIcon(QPainter &p, const QImage &icon, int xOffset, int yOffset, bool selected) {
	xOffset += st::cpPadding;

	if (selected) {
		p.save();
		p.setPen(QPen(st::iconPreviewStroke, st::cpPenSize));
		p.drawEllipse(xOffset + st::cpSelectedPadding,
					  yOffset + st::cpSelectedPadding,
					  st::cpIconSize + st::cpSelectedPadding * 2,
					  st::cpIconSize + st::cpSelectedPadding * 2);
		p.restore();
	}

	auto rect = QRect(xOffset + st::cpImagePadding, yOffset + st::cpImagePadding, st::cpIconSize, st::cpIconSize);
	p.drawImage(rect, icon);
}

void applyIcon() {
#ifdef Q_OS_WIN
	AyuAssets::loadAppIco();
	reloadAppIconFromTaskBar();
#endif

	Window::OverrideApplicationIcon(AyuAssets::currentAppLogo());
	Core::App().refreshApplicationIcon();
	Core::App().tray().updateIconCounters();
	Core::App().domain().notifyUnreadBadgeChanged();
}

IconPicker::IconPicker(QWidget *parent)
	: RpWidget(parent) {
	setMinimumSize(st::boxWidth, (st::cpIconSize + st::cpPadding) * rows);
}

void IconPicker::paintEvent(QPaintEvent *e) {
	Painter p(this);
	PainterHighQualityEnabler hq(p);

	auto offset = st::boxWidth / 2 - (st::cpIconSize + st::cpSpacingX) * 2;

	for (int row = 0; row < rows; row++) {
		const auto columns = std::min(4, static_cast<int>(icons.size()) - row * 4);
		for (int i = 0; i < columns; i++) {
			auto const idx = i + row * 4;

			const auto &iconName = icons[idx];
			auto icon = AyuAssets::loadPreview(iconName)
				.scaled(st::cpIconSize, st::cpIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			drawIcon(p,
					 icon,
					 (st::cpIconSize + st::cpSpacingX) * i + offset,
					 row * (st::cpIconSize + st::cpSpacingY),
					 AyuAssets::currentAppLogoName() == iconName);
		}
	}
}

void IconPicker::mousePressEvent(QMouseEvent *e) {
	auto settings = &AyuSettings::getInstance();
	auto changed = false;

	auto x = e->pos().x();
	for (int row = 0; row < rows; row++) {
		const auto columns = std::min(4, static_cast<int>(icons.size()) - row * 4);
		for (int i = 0; i < columns; i++) {
			auto const idx = i + row * 4;
			auto const xOffset = (st::cpIconSize + st::cpSpacingX) * i + st::cpPadding;
			auto const yOffset = row * (st::cpIconSize + st::cpSpacingY);

			if (x >= xOffset && x <= xOffset + st::cpIconSize && e->pos().y() >= yOffset
				&& e->pos().y() <= yOffset + st::cpIconSize) {
				const auto &iconName = icons[idx];
				if (settings->appIcon != iconName) {
					settings->set_appIcon(iconName);
					changed = true;
					break;
				}
			}
		}
	}

	if (changed) {
		AyuSettings::save();
		applyIcon();

		repaint();
	}
}
