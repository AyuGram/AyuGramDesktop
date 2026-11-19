// This is the source code of LupinGram for Desktop.
#include "lupin/ui/components/snow_overlay.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtWidgets/QWidget>

#include <algorithm>
#include <cmath>
#include <vector>

namespace LupinUi {
namespace {

constexpr auto kFlakes = 220;
constexpr auto kStepMs = 33;

struct Flake {
	float x = 0.f;
	float y = 0.f;
	float radius = 2.f;
	float speed = 1.f;
	float drift = 0.f;
	float phase = 0.f;
	float spin = 0.f;
	int alpha = 200;
};

class SnowWidget final : public QWidget {
public:
	explicit SnowWidget(QWidget *parent) : QWidget(parent) {
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAttribute(Qt::WA_NoSystemBackground);
		setAttribute(Qt::WA_TranslucentBackground);
		parent->installEventFilter(this);
		_timer.setInterval(kStepMs);
		QObject::connect(&_timer, &QTimer::timeout, this, [this] {
			step();
		});
		fit();
		hide();
	}

	void setRunning(bool running) {
		if (running) {
			fit();
			show();
			raise();
			_timer.start();
		} else {
			_timer.stop();
			hide();
		}
	}

protected:
	bool eventFilter(QObject *watched, QEvent *e) override {
		if (watched == parentWidget() && e->type() == QEvent::Resize) {
			fit();
			raise();
		}
		return QWidget::eventFilter(watched, e);
	}

	void paintEvent(QPaintEvent *) override {
		auto p = QPainter(this);
		p.setRenderHint(QPainter::Antialiasing);
		for (const auto &flake : _flakes) {
			if (flake.radius > 3.6f) {
				// Near flakes are drawn as six-armed snowflakes.
				auto pen = QPen(QColor(235, 246, 255, flake.alpha));
				pen.setWidthF(1.3);
				pen.setCapStyle(Qt::RoundCap);
				p.setPen(pen);
				const auto angle = flake.phase + _tick * flake.spin;
				const auto r = double(flake.radius) * 1.5;
				for (auto arm = 0; arm != 3; ++arm) {
					const auto a = angle + arm * 1.0471975511965976;
					const auto dx = std::cos(a) * r;
					const auto dy = std::sin(a) * r;
					p.drawLine(
						QPointF(flake.x - dx, flake.y - dy),
						QPointF(flake.x + dx, flake.y + dy));
				}
				p.setPen(Qt::NoPen);
				p.setBrush(QColor(255, 255, 255, flake.alpha));
				p.drawEllipse(QPointF(flake.x, flake.y), 1.6, 1.6);
				continue;
			}
			p.setPen(Qt::NoPen);
			if (flake.radius > 2.4f) {
				p.setBrush(QColor(150, 185, 225, 70));
				p.drawEllipse(
					QPointF(flake.x, flake.y),
					flake.radius + 1.6,
					flake.radius + 1.6);
			}
			p.setBrush(QColor(250, 252, 255, flake.alpha));
			p.drawEllipse(QPointF(flake.x, flake.y), flake.radius, flake.radius);
		}
	}

private:
	void fit() {
		if (const auto parent = parentWidget()) {
			setGeometry(parent->rect());
		}
		const auto w = float(std::max(width(), 1));
		const auto h = float(std::max(height(), 1));
		if (_flakes.empty()) {
			_flakes.resize(kFlakes);
			for (auto &flake : _flakes) {
				reset(flake, w, h, true);
			}
		}
	}

	void reset(Flake &flake, float w, float h, bool anywhere) {
		auto *rng = QRandomGenerator::global();
		const auto layer = rng->bounded(100);
		const auto unit = [&] { return float(rng->bounded(1000)) / 1000.f; };
		if (layer < 60) { // Far: small, slow, faint.
			flake.radius = 1.0f + unit() * 1.0f;
			flake.speed = 0.5f + unit() * 0.4f;
			flake.alpha = 120 + rng->bounded(50);
		} else if (layer < 90) { // Middle.
			flake.radius = 2.0f + unit() * 1.4f;
			flake.speed = 1.0f + unit() * 0.6f;
			flake.alpha = 190 + rng->bounded(35);
		} else { // Near: large six-armed snowflakes.
			flake.radius = 3.8f + unit() * 2.2f;
			flake.speed = 1.6f + unit() * 0.8f;
			flake.alpha = 235;
		}
		flake.x = float(rng->bounded(std::max(int(w), 1)));
		flake.y = anywhere ? float(rng->bounded(std::max(int(h), 1))) : -8.f;
		flake.drift = 0.25f + unit() * 0.5f;
		flake.phase = unit() * 6.28f;
		flake.spin = (unit() - 0.5f) * 0.05f;
	}

	void step() {
		const auto w = float(std::max(width(), 1));
		const auto h = float(std::max(height(), 1));
		_tick += 1.f;
		// Gentle gusts of wind that come and go.
		const auto wind = std::sin(_tick * 0.0045f) * 0.55f;
		for (auto &flake : _flakes) {
			flake.y += flake.speed;
			flake.x += std::sin(_tick * 0.03f + flake.phase) * flake.drift
				+ wind * (0.4f + flake.radius * 0.2f);
			if (flake.y > h + 8.f || flake.x < -8.f || flake.x > w + 8.f) {
				reset(flake, w, h, false);
			}
		}
		update();
	}

	QTimer _timer;
	std::vector<Flake> _flakes;
	float _tick = 0.f;

};

std::vector<QPointer<SnowWidget>> &Overlays() {
	static auto result = std::vector<QPointer<SnowWidget>>();
	return result;
}

bool &WantSnow() {
	static auto value = false;
	return value;
}

} // namespace

void AttachSnow(QWidget *parent) {
	if (!parent) {
		return;
	}
	const auto widget = new SnowWidget(parent);
	Overlays().push_back(QPointer<SnowWidget>(widget));
	widget->setRunning(WantSnow());
}

void SetSnowEnabled(bool enabled) {
	WantSnow() = enabled;
	for (auto &overlay : Overlays()) {
		if (overlay) {
			overlay->setRunning(enabled);
		}
	}
}

} // namespace LupinUi
