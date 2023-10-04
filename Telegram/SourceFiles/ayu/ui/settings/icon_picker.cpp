// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "icon_picker.h"
#include "ayu/ayu_settings.h"
#include "core/application.h"
#include "styles/style_layers.h"
#include "tray.h"

#include "ayu/ui/ayu_assets.h"
#include "main/main_domain.h"
#include "styles/style_ayu_styles.h"
#include "ui/painter.h"
#include "window/main_window.h"

#ifdef Q_OS_WIN
#include "ayu/utils/windows_utils.h"
#endif

const QVector<QString> icons{
	AyuSettings::DEFAULT_ICON,
	AyuSettings::ALT_ICON,
	AyuSettings::DISCORD_ICON,
	AyuSettings::SPOTIFY_ICON,
	AyuSettings::EXTERA_ICON,
	AyuSettings::NOTHING_ICON,
	AyuSettings::BARD_ICON,
	AyuSettings::YAPLUS_ICON
};

const auto rows = icons.size() / 4 + std::min(1, icons.size() % 4);

void drawIcon(QPainter &p, const QImage &icon, int xOffset, int yOffset, bool selected)
{
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

IconPicker::IconPicker(QWidget *parent)
	: RpWidget(parent)
{
	setMinimumSize(st::boxWidth, (st::cpIconSize + st::cpPadding) * rows);
}

void IconPicker::paintEvent(QPaintEvent *e)
{
	Painter p(this);
	PainterHighQualityEnabler hq(p);

	for (int row = 0; row < rows; row++) {
		const auto columns = std::min(4, icons.size() - row * 4);
		for (int i = 0; i < columns; i++) {
			auto const idx = i + row * 4;

			const auto &iconName = icons[idx];
			auto icon = loadPreview(iconName)
				.scaled(st::cpIconSize, st::cpIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			drawIcon(p,
					 icon,
					 (st::cpIconSize + st::cpSpacingX) * i,
					 row * (st::cpIconSize + st::cpSpacingY),
					 currentAppLogoName() == iconName);
		}
	}
}

void IconPicker::mousePressEvent(QMouseEvent *e)
{
	auto settings = &AyuSettings::getInstance();
	auto changed = false;

	auto x = e->pos().x();
	for (int row = 0; row < rows; row++) {
		const auto columns = std::min(4, icons.size() - row * 4);
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

#ifdef Q_OS_WIN
		loadAppIco();
		reloadAppIconFromTaskBar();
#endif

		Window::OverrideApplicationIcon(currentAppLogo());
		Core::App().refreshApplicationIcon();
		Core::App().tray().updateIconCounters();
		Core::App().domain().notifyUnreadBadgeChanged();

		repaint();
	}
}
