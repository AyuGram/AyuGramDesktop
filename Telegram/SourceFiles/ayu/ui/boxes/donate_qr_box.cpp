// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/ui/boxes/donate_qr_box.h"

#include "qr/qr_generate.h"
#include "styles/style_boxes.h"
#include "styles/style_intro.h"
#include "styles/style_layers.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/controls/invite_link_label.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtSvg/QSvgRenderer>

#include "lang_auto.h"
#include "styles/style_giveaway.h"
#include "ui/toast/toast.h"

namespace Ui {
namespace {

[[nodiscard]] QImage MakeQrWithIcon(
		const QByteArray &payload,
		const QString &iconPath,
		int pixel,
		int max) {
	Expects(!payload.isEmpty());

	const auto data = Qr::Encode(payload, Qr::Redundancy::Default);
	Expects(data.size > 0);
	if (max > 0 && data.size * pixel > max) {
		pixel = std::max(max / data.size, 1);
	}

	auto qr = Qr::Generate(
		data,
		pixel * style::DevicePixelRatio(),
		Qt::black,
		Qt::white);

	{
		QPainter p(&qr);
		PainterHighQualityEnabler hq(p);
		const auto size = qr.rect().size();
		constexpr auto kCenterRatio = 0.20;
		const auto centerRect = Rect(size)
			- Margins((size.width() - (size.width() * kCenterRatio)) / 2);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		p.drawEllipse(centerRect);

		QSvgRenderer svg(iconPath);
		if (svg.isValid()) {
			svg.render(&p, centerRect);
		}
	}
	return qr;
}

} // namespace

void FillDonateQrBox(
	not_null<Ui::GenericBox*> box,
	const QString &address,
	const QString &iconResourcePath) {
	box->setStyle(st::giveawayGiftCodeBox);
	box->setNoContentMargin(true);
	box->setWidth(int(st::aboutWidth * 1.25));
	box->setTitle(tr::lng_group_invite_context_qr());
	box->verticalLayout()->resizeToWidth(box->width());

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	const auto qrWidget = box->verticalLayout()->add(
		object_ptr<Ui::RpWidget>(box->verticalLayout()));

	struct State {
		QImage qrImage;
		int qrMaxSize = 0;
	};
	const auto state = qrWidget->lifetime().make_state<State>();

	const auto recompute = [=] {
		const auto qrMaxSize = int(st::aboutWidth * 1.25) - st::boxRowPadding.left() - st::boxRowPadding.right();
		state->qrMaxSize = qrMaxSize;

		const auto remainder = qrMaxSize % st::introQrPixel;
		const auto downTo = remainder ? (qrMaxSize - remainder) : qrMaxSize;
		state->qrImage = MakeQrWithIcon(
			address.toUtf8(),
			iconResourcePath,
			st::introQrPixel,
			downTo).scaled(
			Size(qrMaxSize * style::DevicePixelRatio()),
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);

		qrWidget->resize(
			state->qrMaxSize,
			state->qrMaxSize);
	};

	recompute();

	qrWidget->paintRequest(
	) | rpl::start_with_next([=](QRect) {
		QPainter p(qrWidget);
		PainterHighQualityEnabler hq(p);

		const auto size = state->qrImage.size() / style::DevicePixelRatio();
		const auto rect = Rect(
			(qrWidget->width() - size.width()) / 2,
			(qrWidget->height() - size.height()) / 2,
			size);

		QPainterPath path;
		path.addRoundedRect(rect, st::roundRadiusLarge, st::roundRadiusLarge);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		p.drawPath(path);
		p.setClipPath(path);

		const auto padding = st::boxRowPadding.left();
		const auto innerRect = rect - Margins(padding);

		p.drawImage(innerRect, state->qrImage);
	}, qrWidget->lifetime());

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	const auto label = box->lifetime().make_state<Ui::InviteLinkLabel>(
		box->verticalLayout(),
		rpl::single(address),
		Fn<base::unique_qptr<Ui::PopupMenu>()>());
	box->verticalLayout()->add(label->take(), st::boxRowPadding);

	const auto copyButton = box->addButton(tr::lng_chat_link_copy(), [=] {
		QGuiApplication::clipboard()->setText(address);
		Ui::Toast::Show(tr::lng_text_copied(tr::now));
	});
	const auto buttonWidth = box->width()
	- rect::m::sum::h(st::giveawayGiftCodeBox.buttonPadding);
	copyButton->widthValue() | rpl::filter([=] {
		return (copyButton->widthNoMargins() != buttonWidth);
	}) | rpl::start_with_next([=] {
		copyButton->resizeToWidth(buttonWidth);
	}, copyButton->lifetime());

	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });
}

} // namespace Ui
