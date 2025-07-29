// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/ui/boxes/donate_info_box.h"

#include <QSvgRenderer>

#include "lang_auto.h"
#include "ayu/utils/rc_manager.h"
#include "core/ui_integration.h"
#include "data/data_session.h"
#include "data/stickers/data_custom_emoji.h"
#include "info/channel_statistics/earn/earn_icons.h"
#include "lang/lang_text_entity.h"

#include "info/profile/info_profile_icon.h"
#include "main/main_session.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_channel_earn.h"
#include "styles/style_giveaway.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_premium.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

namespace Ui {
namespace {

QImage MakeSupportLogo() {
	const auto s = Size(st::supportLogoSize);
	auto svg = QSvgRenderer(QString(":/gui/icons/ayu/donates/support_logo.svg"));
	auto image = QImage(
		s * style::DevicePixelRatio(),
		QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(style::DevicePixelRatio());
	image.fill(Qt::transparent);
	{
		auto p = QPainter(&image);
		svg.render(&p, Rect(s));
	}
	return image;
}

object_ptr<Ui::RpWidget> CreateTopLogoWidget(
	not_null<Ui::RpWidget*> parent) {
	auto w = object_ptr<Ui::RpWidget>(parent);
	const auto raw = w.data();

	const auto logo = MakeSupportLogo();

	raw->paintRequest(
	) | rpl::start_with_next(
		[=](QRect)
		{
			QPainter p(raw);
			PainterHighQualityEnabler hq(p);

			const auto original = logo.size() / style::DevicePixelRatio();
			const auto maxWidth = raw->width() - rect::m::sum::h(st::boxRowPadding);
			const auto maxHeight = raw->height();
			if (original.isEmpty() || maxWidth <= 0 || maxHeight <= 0) {
				return;
			}
			const auto scale = std::min(
				double(maxWidth) / double(original.width()),
				double(maxHeight) / double(original.height()));
			const auto target = QSize(
				int(original.width() * scale),
				int(original.height() * scale));
			const auto x = (raw->width() - target.width()) / 2;
			const auto y = (raw->height() - target.height()) / 2;
			const auto rect = QRect(QPoint(x, y), target);
			p.drawImage(rect, logo);
		},
		raw->lifetime());

	return w;
}

object_ptr<Ui::RpWidget> InfoRow(
	not_null<Ui::RpWidget*> parent,
	not_null<Main::Session*> session,
	const QString &title,
	const TextWithEntities &text,
	not_null<const style::icon*> icon) {
	auto row = object_ptr<Ui::VerticalLayout>(parent);
	const auto raw = row.data();

	raw->add(
		object_ptr<Ui::FlatLabel>(
			raw,
			rpl::single(title) | Ui::Text::ToBold(),
			st::defaultFlatLabel),
		st::settingsPremiumRowTitlePadding);

	const auto label = raw->add(
		object_ptr<Ui::FlatLabel>(
			raw,
			st::boxDividerLabel),
		st::settingsPremiumRowAboutPadding);

	label->setMarkedText(
		text,
		Core::TextContext({
			.session = std::move(session),
		})
	);

	object_ptr<Info::Profile::FloatingIcon>(
		raw,
		*icon,
		st::starrefInfoIconPosition);

	return row;
}

} // namespace

void FillDonateInfoBox(not_null<Ui::GenericBox*> box, not_null<Window::SessionController*> controller) {
	// box->setStyle(st::starrefFooterBox);
	box->setStyle(st::giveawayGiftCodeBox);
	box->setNoContentMargin(true);
	box->setWidth(int(st::aboutWidth * 1.1));
	box->verticalLayout()->resizeToWidth(box->width());

	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	const auto logoWidget = box->verticalLayout()->add(
		CreateTopLogoWidget(box->verticalLayout()));

	logoWidget->resize(st::supportLogoSize, st::supportLogoSize);

	Ui::AddSkip(box->verticalLayout());

	box->verticalLayout()->add(
		object_ptr<Ui::CenterWrap<Ui::FlatLabel>>(
			box,
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				tr::ayu_SupportBoxHeader()
				| Ui::Text::ToBold(),
				st::boxTitle)),
		st::boxRowPadding);

	box->verticalLayout()->add(
		object_ptr<Ui::FlatLabel>(
			box->verticalLayout(),
			tr::ayu_SupportBoxInfo(),
			st::starrefCenteredText),
		st::boxRowPadding);

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	const auto tonSymbol = Ui::Text::SingleCustomEmoji(
		controller->session().data().customEmojiManager().registerInternalEmoji(
			Ui::Earn::IconCurrencyColored(
				st::boxDividerLabel.style.font,
				st::boxDividerLabel.textFg->c),
			st::channelEarnCurrencyLearnMargins));

	const auto dollarAmount = RCManager::getInstance().donateAmountUsd().prepend("$");
	const auto tonAmount = RCManager::getInstance().donateAmountTon();
	const auto rubleAmount = RCManager::getInstance().donateAmountRub().append("₽");

	const auto innerText = TextWithEntities{}.append(tonSymbol).append(tonAmount).append(", ").append(rubleAmount);
	const auto str = tr::ayu_SupportBoxMakeDonationInfo(
		tr::now,
		lt_amount1,
		TextWithEntities{dollarAmount},
		lt_amount2,
		innerText,
		Ui::Text::RichLangValue
	);

	box->verticalLayout()->add(InfoRow(
		box->verticalLayout(),
		&controller->session(),
		tr::ayu_SupportBoxMakeDonationHeader(tr::now),
		str,
		&st::menuIconEarn));

	Ui::AddSkip(box->verticalLayout());

	const auto username = RCManager::getInstance().donateUsername();
	auto usernameTrimmed = username;
	if (usernameTrimmed.startsWith('@')) {
		usernameTrimmed.remove(0, 1);
	}
	const TextWithEntities proofText = tr::ayu_SupportBoxSendProofInfo(
		tr::now,
		lt_item,
		Ui::Text::Link(username, controller->session().createInternalLinkFull(usernameTrimmed)),
		Ui::Text::RichLangValue);
	box->verticalLayout()->add(InfoRow(
		box->verticalLayout(),
		&controller->session(),
		tr::ayu_SupportBoxSendProofHeader(tr::now),
		proofText,
		&st::menuIconPhoto));

	Ui::AddSkip(box->verticalLayout());

	box->verticalLayout()->add(InfoRow(
		box->verticalLayout(),
		&controller->session(),
		tr::ayu_SupportBoxReceiveBadgeHeader(tr::now),
		TextWithEntities{
			tr::ayu_SupportBoxReceiveBadgeInfo(tr::now)
		},
		&st::menuIconStarRefShare));

	const auto closeButton = box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	const auto buttonWidth = box->width()
		- rect::m::sum::h(st::starrefFooterBox.buttonPadding);
	closeButton->widthValue() | rpl::filter([=]
	{
		return (closeButton->widthNoMargins() != buttonWidth);
	}) | rpl::start_with_next([=]
							  {
								  closeButton->resizeToWidth(buttonWidth);
							  },
							  closeButton->lifetime());
}

} // namespace Ui
