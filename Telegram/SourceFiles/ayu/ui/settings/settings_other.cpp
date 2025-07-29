// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_other.h"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QSvgRenderer>

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/donate_qr_box.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/integration.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/vertical_list.h"
#include "ui/boxes/confirm_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "window/themes/window_theme.h"

namespace Settings {
namespace {

struct Asset
{
	QString icon;
	QColor background;
};

Asset getAsset(const QString &name) {
	const auto isNightMode = Window::Theme::IsNightMode();
	const auto normalized = name.toLower();
	QString icon = QString(":/gui/icons/ayu/donates/%1.svg").arg(normalized);
	QColor background = isNightMode ? QColor(0xEEEEEE) : QColor(0x242B2C);
	return {
		.icon = std::move(icon),
		.background = std::move(background)
	};
}

QImage getImage(const QString &name) {
	const auto iconData = getAsset(name);

	const auto factor = style::DevicePixelRatio();
	const auto size = st::menuIconLink.size();
	auto image = QImage(
		size * factor,
		QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(factor);
	image.fill(Qt::transparent);
	{
		auto p = QPainter(&image);
		auto hq = PainterHighQualityEnabler(p);

		p.setPen(Qt::NoPen);
		p.setBrush(iconData.background);
		p.drawRoundedRect(Rect(size), size.width() / 4., size.height() / 4.);
		p.setBrush(Qt::transparent);

		auto svgIcon = QSvgRenderer(iconData.icon);
		svgIcon.render(&p, Rect(size));
	}

	return image;
}

[[nodiscard]] not_null<Ui::SettingsButton*> AddDonate(not_null<Ui::SettingsButton*> button, const QString &name) {
	const auto btnContainer = Ui::CreateChild<Ui::RpWidget>(button);
	const auto &buttonSt = button->st();
	const auto fullHeight = buttonSt.height
		+ rect::m::sum::v(buttonSt.padding);

	const auto iconWidget = Ui::CreateChild<Ui::RpWidget>(button.get());

	auto icon = getImage(name);
	iconWidget->resize(icon.size() / style::DevicePixelRatio());
	iconWidget->paintRequest(
	) | rpl::start_with_next([=]
							 {
								 auto p = QPainter(iconWidget);
								 p.drawImage(0, 0, icon);
							 },
							 iconWidget->lifetime());

	button->sizeValue(
	) | rpl::start_with_next([=](const QSize &s)
							 {
								 iconWidget->moveToLeft(
									 button->st().iconLeft
									 + (st::menuIconShop.width() - iconWidget->width()) / 2,
									 (s.height() - iconWidget->height()) / 2);
								 btnContainer->moveToLeft(
									 iconWidget->x() - (fullHeight - iconWidget->height()) / 2,
									 0);
							 },
							 iconWidget->lifetime());

	btnContainer->resize(fullHeight, fullHeight);

	return button;
}

void AddCryptoDonate(const QString &name, const QString &address, not_null<Ui::VerticalLayout*> container) {
	const auto button = AddDonate(
		AddButtonWithIcon(
			container,
			rpl::single(name),
			st::settingsButton),
		name);
	button->setClickedCallback([=]
	{
		auto box = Box(
			Ui::FillDonateQrBox,
			address,
			getAsset(name).icon);
		Ui::show(std::move(box));
	});
}

}

rpl::producer<QString> AyuOther::title() {
	return tr::ayu_CategoryOther();
}

AyuOther::AyuOther(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent) {
	setupContent(controller);
}

void SetupDonations(not_null<Ui::VerticalLayout*> container, not_null<Window::SessionController*> controller) {
	AddSubsectionTitle(container, tr::ayu_SupportHeader());
	AddDonate(
		AddButtonWithIcon(
			container,
			rpl::single(QString("Boosty")),
			st::settingsButton),
		"boosty"
	)->setClickedCallback([=]
	{
		QDesktopServices::openUrl(QString("https://boosty.to/alexeyzavar"));
	});
	AddCryptoDonate("TON", QString("UQA4i8U8vP3mYUZSV3KqDQEHPwmhninEqCkkKc7BITQ652de"), container);
	AddCryptoDonate("Bitcoin", QString("bc1qdk6qq4mzq5yap3fpy0qau3246w3m3uwac9f0xd"), container);
	AddCryptoDonate("Ethereum", QString("0x405589857C8DFAb45B2027c68ad1e58877FDa347"), container);
	AddCryptoDonate("Solana", QString("8ZHQpPxpsdRjsWoBcF1dmvRM5dB6zEhJ3jMBFZjYfyHs"), container);
	AddCryptoDonate("Tron", QString("TRpbajq38qU8joThgAfKJLyEPbNjzsdPJ1"), container);
	AddSkip(container);

	AddDividerText(container,
				   tr::ayu_SupportDescription2(
					   lt_item,
					   rpl::single(
						   Ui::Text::Link(tr::ayu_SupportDescription1(tr::now), QString("tg://support"))
					   ),
					   Ui::Text::WithEntities
				   )
	);
}

void SetupCrashReporting(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSkip(container);
	AddSubsectionTitle(container, tr::ayu_CategoryOther());

	AddButtonWithIcon(
		container,
		tr::ayu_CrashReporting(),
		st::settingsButton,
		{&st::menuIconReport}
	)->toggleOn(
		rpl::single(settings->crashReporting)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->crashReporting);
		}) | start_with_next(
		[=](bool enabled)
		{
			AyuSettings::set_crashReporting(enabled);
			AyuSettings::save();
		},
		container->lifetime());
	AddSkip(container);
	AddDividerText(container, tr::ayu_CrashReportingDescription());
}

void SetupOtherThings(not_null<Ui::VerticalLayout*> container, not_null<Window::SessionController*> controller) {
	AddSkip(container);
	AddButtonWithIcon(
		container,
		tr::ayu_RegisterURLScheme(),
		st::settingsButton,
		{&st::menuIconLink}
	)->setClickedCallback([=]
	{
		Core::Application::RegisterUrlScheme();
		controller->showToast(tr::lng_box_done(tr::now));
	});
	AddButtonWithIcon(
		container,
		tr::ayu_ResetSettings(),
		st::settingsButton,
		{&st::menuIconRestore}
	)->setClickedCallback([=]
	{
		controller->show(Ui::MakeConfirmBox({
			.text = tr::ayu_ResetSettingsConfirmation(Ui::Text::RichLangValue),
			.confirmed = [=](Fn<void()> &&close)
			{
				AyuSettings::reset();
				close();
			},
			.confirmText = tr::lng_box_yes(),
		}));
	});
	AddSkip(container);
}

void AyuOther::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	AddSkip(content);
	SetupDonations(content, controller);
#ifndef TDESKTOP_DISABLE_AUTOUPDATE
	SetupCrashReporting(content);
#endif
	SetupOtherThings(content, controller);

	ResizeFitChild(this, content);
}

} // namespace Settings
