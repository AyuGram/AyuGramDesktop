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

void drawIcon(QPainter& p, const QImage& icon, int offset, bool selected)
{
	if (selected)
	{
		p.save();
		p.setPen(QPen(st::iconPreviewStroke, 2));
		p.drawEllipse(offset + 2, 2, 68, 68);
		p.restore();
	}

	auto rect = QRect(offset + 4, 4, 64, 64);
	p.drawImage(rect, icon);
}

IconPicker::IconPicker(QWidget* parent) : RpWidget(parent)
{
	setMinimumSize(st::boxWidth, 72);
}

void IconPicker::paintEvent(QPaintEvent* e)
{
	Painter p(this);
	PainterHighQualityEnabler hq(p);

	auto icon1 = logoPreview().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	auto icon2 = logoAltPreview().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	auto icon3 = logoNothingPreview().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	// todo: center
	drawIcon(p, icon1, 0, currentAppLogoName() == AyuSettings::DEFAULT_ICON);
	drawIcon(p, icon2, 0 + 64 + 16, currentAppLogoName() == AyuSettings::ALT_ICON);
	drawIcon(p, icon3, 0 + 64 + 16 + 64 + 16, currentAppLogoName() == AyuSettings::NOTHING_ICON);
}

void IconPicker::mousePressEvent(QMouseEvent* e)
{
	auto settings = &AyuSettings::getInstance();
	auto changed = false;

	auto x = e->pos().x();
	if (x <= 64 && settings->appIcon != AyuSettings::DEFAULT_ICON)
	{
		settings->set_appIcon(AyuSettings::DEFAULT_ICON);
		changed = true;
	}
	else if (x >= 64 + 16 && x <= 64 + 16 + 64 && settings->appIcon != AyuSettings::ALT_ICON)
	{
		settings->set_appIcon(AyuSettings::ALT_ICON);
		changed = true;
	}
	else if (x >= 64 + 16 + 64 + 16 && x < 64 + 16 + 64 + 16 + 64 && settings->appIcon != AyuSettings::NOTHING_ICON)
	{
		settings->set_appIcon(AyuSettings::NOTHING_ICON);
		changed = true;
	}

	if (changed)
	{
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
