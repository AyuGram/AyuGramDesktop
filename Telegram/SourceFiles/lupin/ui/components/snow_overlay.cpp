// This is the source code of LupinGram for Desktop.
#include "lupin/ui/components/snow_overlay.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QRandomGenerator>
#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QWidget>

#include <algorithm>
#include <cmath>
#include <vector>

namespace LupinUi {
namespace {

constexpr auto kFlakes = 90;
constexpr auto kStepMs = 33;

struct Flake {
	float x = 0.f;
	float y = 0.f;
	float radius = 2.f;
	float speed = 1.f;
	float drift = 0.f;
	float phase = 0.f;
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
		p.setPen(Qt::NoPen);
		for (const auto &flake : _flakes) {
			const auto glow = QColor(150, 185, 225, 90);
			const auto core = QColor(250, 252, 255, 235);
			p.setBrush(glow);
			p.drawEllipse(
				QPointF(flake.x, flake.y),
				flake.radius + 1.6,
				flake.radius + 1.6);
			p.setBrush(core);
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
		flake.x = float(rng->bounded(int(w)));
		flake.y = anywhere ? float(rng->bounded(int(h))) : -6.f;
		flake.radius = 1.4f + float(rng->bounded(24)) / 10.f;
		flake.speed = 0.7f + flake.radius * 0.45f;
		flake.drift = 0.25f + float(rng->bounded(10)) / 20.f;
		flake.phase = float(rng->bounded(628)) / 100.f;
	}

	void step() {
		const auto w = float(std::max(width(), 1));
		const auto h = float(std::max(height(), 1));
		_tick += 1.f;
		for (auto &flake : _flakes) {
			flake.y += flake.speed;
			flake.x += std::sin(_tick * 0.03f + flake.phase) * flake.drift;
			if (flake.y > h + 6.f || flake.x < -8.f || flake.x > w + 8.f) {
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
