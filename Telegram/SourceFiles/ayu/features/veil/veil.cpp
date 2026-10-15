/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The privacy veil: a calm screen over the chats while the window is out of focus.
#include "ayu/features/veil/veil.h"

#include "ayu/ayu_settings.h"
#include "base/timer.h"
#include "core/application.h"
#include "lottie/lottie_icon.h"
#include "main/main_domain.h"
#include "settings/settings_common.h"
#include "storage/storage_domain.h"
#include "ui/effects/animations.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "window/window_controller.h"
#include "lang_auto.h"
#include "mainwindow.h"

#include "styles/style_ayu_styles.h"

#include <algorithm>

#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QRadialGradient>

namespace AyuFeatures::Veil {
namespace {

constexpr auto kFadeDuration = crl::time(180);
constexpr auto kKeepOnTopEach = crl::time(250);
constexpr auto kGlowAlpha = 26;

class Overlay final : public Ui::RpWidget {
public:
	explicit Overlay(not_null<Ui::RpWidget*> body);

	void dismiss();

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private:
	void layoutContent();

	Ui::Animations::Simple _shown;
	base::Timer _keepOnTop;
	object_ptr<Ui::RpWidget> _icon = { nullptr };
	bool _closing = false;

};

void DrawWordmark(QPainter &p, int outerWidth, int top) {
	const auto &font = st::ayuVeilNameFont;
	const auto name = u"AyuGram"_q;
	p.setFont(font);
	p.setPen(st::windowFg);
	p.drawText(
		QRect(0, top, outerWidth, font->height),
		Qt::AlignHCenter | Qt::AlignTop,
		name);
}

[[nodiscard]] std::vector<QPointer<Overlay>> &Overlays() {
	static auto result = std::vector<QPointer<Overlay>>();
	return result;
}

Overlay::Overlay(not_null<Ui::RpWidget*> body)
: RpWidget(body) {
	setFocusPolicy(Qt::StrongFocus);

	auto icon = Settings::CreateLottieIcon(
		this,
		{
			.name = u"local_passcode_enter"_q,
			.sizeOverride = QSize(st::ayuVeilIconSize, st::ayuVeilIconSize),
		});
	_icon = std::move(icon.widget);
	_icon->setAttribute(Qt::WA_TransparentForMouseEvents);

	body->sizeValue(
	) | rpl::on_next([=](QSize size) {
		setGeometry(QRect(QPoint(), size));
		layoutContent();
	}, lifetime());

	// A box or a toast opened while the veil is up must not end up above it.
	_keepOnTop.setCallback([=] { raise(); });
	_keepOnTop.callEach(kKeepOnTopEach);

	show();
	raise();
	setFocus();
	_icon->show();
	icon.animate(anim::repeat::loop);
	_shown.start([=] { update(); }, 0., 1., kFadeDuration);
}

void Overlay::layoutContent() {
	const auto content = st::ayuVeilIconSize
		+ st::ayuVeilNameSkip
		+ st::ayuVeilNameFont->height
		+ st::ayuVeilHintSkip
		+ st::normalFont->height;
	const auto top = std::max((height() - content) / 2, 0);
	_icon->move((width() - _icon->width()) / 2, top);
}

void Overlay::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	p.setOpacity(_shown.value(_closing ? 0. : 1.));
	p.fillRect(rect(), st::windowBg);

	const auto center = QPointF(width() / 2., height() / 2.);
	auto glow = QRadialGradient(center, st::ayuVeilGlowRadius);
	auto accent = st::windowBgActive->c;
	accent.setAlpha(kGlowAlpha);
	glow.setColorAt(0., accent);
	accent.setAlpha(0);
	glow.setColorAt(1., accent);
	p.fillRect(rect(), glow);

	const auto nameTop = _icon->y() + _icon->height() + st::ayuVeilNameSkip;
	DrawWordmark(p, width(), nameTop);

	const auto hintTop = nameTop
		+ st::ayuVeilNameFont->height
		+ st::ayuVeilHintSkip;
	p.setFont(st::normalFont);
	p.setPen(st::windowSubTextFg);
	p.drawText(
		QRect(0, hintTop, width(), st::normalFont->height),
		Qt::AlignHCenter | Qt::AlignTop,
		tr::ayu_VeilHint(tr::now));
}

void Overlay::mousePressEvent(QMouseEvent *e) {
	dismiss();
}

void Overlay::keyPressEvent(QKeyEvent *e) {
	dismiss();
}

void Overlay::dismiss() {
	if (_closing) {
		return;
	}
	_closing = true;
	_shown.start([=] {
		update();
		if (!_shown.animating()) {
			deleteLater();
		}
	}, 1., 0., kFadeDuration);
	if (const auto window = Core::App().activeWindow()) {
		window->widget()->setInnerFocus();
	}
}

} // namespace

bool Covered() {
	auto &all = Overlays();
	all.erase(
		std::remove_if(all.begin(), all.end(), [](const QPointer<Overlay> &o) {
			return o.isNull();
		}),
		all.end());
	return !all.empty();
}

void Cover() {
	if (!Core::IsAppLaunched()
		|| Core::Quitting()
		|| Core::App().passcodeLocked()) {
		return;
	}
	Core::App().enumerateWindows([&](not_null<Window::Controller*> window) {
		const auto body = window->widget()->bodyWidget();
		if (!body) {
			return;
		}
		for (const auto &existing : Overlays()) {
			if (existing && existing->parentWidget() == body) {
				return;
			}
		}
		Overlays().push_back(QPointer<Overlay>(new Overlay(body)));
	});
	const auto &settings = AyuSettings::getInstance();
	if (settings.veilLock()
		&& Core::App().domain().local().hasLocalPasscode()) {
		Core::App().lockByPasscode();
	}
}

void Start() {
	// WHY: lives until the process ends; a static would be destroyed after the QApplication is gone.
	const auto timer = new base::Timer([] {
		if (QGuiApplication::applicationState() != Qt::ApplicationActive) {
			Cover();
		}
	});
	QObject::connect(
		qGuiApp,
		&QGuiApplication::applicationStateChanged,
		qGuiApp,
		[=](Qt::ApplicationState state) {
			timer->cancel();
			if (state == Qt::ApplicationActive) {
				return;
			}
			const auto delay = AyuSettings::getInstance().veilDelay();
			if (delay < 0) {
				return;
			} else if (delay == 0) {
				Cover();
			} else {
				timer->callOnce(delay * crl::time(1000));
			}
		});
}

} // namespace AyuFeatures::Veil
