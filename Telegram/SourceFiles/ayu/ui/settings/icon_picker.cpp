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

const auto padding = 14;

void drawIcon(QPainter &p, const QImage &icon, int xOffset, int yOffset, bool selected)
{
	xOffset += padding;

	if (selected) {
		p.save();
		p.setPen(QPen(st::iconPreviewStroke, 2));
		p.drawEllipse(xOffset + 2, yOffset + 2, 68, 68);
		p.restore();
	}

	auto rect = QRect(xOffset + 4, yOffset + 4, 64, 64);
	p.drawImage(rect, icon);
}

IconPicker::IconPicker(QWidget *parent)
	: RpWidget(parent)
{
	setMinimumSize(st::boxWidth, 72 * rows);
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
			auto icon = loadPreview(iconName).scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			drawIcon(p, icon, (64 + 16) * i, row * (64 + 8), currentAppLogoName() == iconName);
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
			auto const xOffset = (64 + 16) * i + padding;
			auto const yOffset = row * (64 + 8);

			if (x >= xOffset && x <= xOffset + 64 && e->pos().y() >= yOffset && e->pos().y() <= yOffset + 64) {
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
