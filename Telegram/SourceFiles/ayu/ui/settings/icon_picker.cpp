// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
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

const QVector<QString> icons{
	AyuAssets::DEFAULT_ICON,
	AyuAssets::ALT_ICON,
	AyuAssets::DISCORD_ICON,
	AyuAssets::SPOTIFY_ICON,
	AyuAssets::EXTERA_ICON,
	AyuAssets::NOTHING_ICON,
	AyuAssets::BARD_ICON,
	AyuAssets::YAPLUS_ICON,
	AyuAssets::CHIBI_ICON,
	AyuAssets::CHIBI2_ICON,
};

const auto rows = static_cast<int>(icons.size()) / 4 + std::min(1, static_cast<int>(icons.size()) % 4);

void drawIcon(QPainter &p, const QImage &icon, int xOffset, int yOffset, float strokeOpacity) {
	xOffset += st::cpPadding;

	p.save();
	p.setPen(QPen(st::boxDividerBg, 0));
	p.setBrush(QBrush(st::boxDividerBg));
	p.setOpacity(strokeOpacity);
	p.drawRoundedRect(
		xOffset + st::cpSelectedPadding,
		yOffset + st::cpSelectedPadding,
		st::cpIconSize + st::cpSelectedPadding * 2,
		st::cpIconSize + st::cpSelectedPadding * 2,
		st::cpSelectedRounding,
		st::cpSelectedRounding
	);
	p.restore();

	auto rect = QRect(
		xOffset + st::cpImagePadding,
		yOffset + st::cpImagePadding,
		st::cpIconSize,
		st::cpIconSize
	);
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
	setMinimumSize(st::boxWidth, (st::cpIconSize + st::cpPadding) * rows - st::cpPadding);
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
			if (iconName.isEmpty()) {
				continue;
			}

			auto icon = AyuAssets::loadPreview(iconName)
				.scaled(st::cpIconSize, st::cpIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

			auto opacity = 0.0f;
			if (iconName == wasSelected) {
				opacity = 1.0f - animation.value(1.0f);
			} else if (iconName == AyuAssets::currentAppLogoName()) {
				opacity = wasSelected.isEmpty() ? 1.0f : animation.value(1.0f);
			}

			drawIcon(
				p,
				icon,
				(st::cpIconSize + st::cpSpacingX) * i + offset,
				row * (st::cpIconSize + st::cpSpacingY),
				opacity
			);
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
				if (iconName.isEmpty()) {
					break;
				}

				if (settings->appIcon != iconName) {
					wasSelected = settings->appIcon;
					animation.start(
						[=]
						{
							update();
						},
						0.0,
						1.0,
						200,
						anim::easeOutCubic
					);

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
