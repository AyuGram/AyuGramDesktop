// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/streamer_mode/streamer_mode.h"

#include "ayu/features/streamer_mode/platform/platform_streamer_mode.h"
#include "core/application.h"
#include "window/window_controller.h"

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

namespace AyuFeatures::StreamerMode {

namespace {

constexpr auto kHiddenProperty = "AyuStreamerModeHidden";

[[nodiscard]] bool IsWindowCaptureExcluded(not_null<QWidget*> widget) {
	return widget->property(kHiddenProperty).toBool();
}

void SetWindowCaptureExcluded(QWidget *widget, bool excluded) {
	const auto window = widget->window();
	Platform::SetWindowCaptureExcluded(window, excluded);
	window->setProperty(kHiddenProperty, excluded);
}

} // namespace

void apply(bool enabled) {
	Core::App().enumerateWindows([=](not_null<Window::Controller*> window) {
		SetWindowCaptureExcluded(window->widget(), enabled);
	});
	for (const auto widget : QApplication::topLevelWidgets()) {
		if (!widget->windowHandle()) {
			continue;
		}
		if (enabled) {
			if (widget->isVisible()
				&& !IsWindowCaptureExcluded(widget)) {
				SetWindowCaptureExcluded(widget, true);
			}
		} else if (IsWindowCaptureExcluded(widget)) {
			SetWindowCaptureExcluded(widget, false);
		}
	}
}

void hideWidgetWindow(QWidget *widget) {
	SetWindowCaptureExcluded(widget, true);
}

void showWidgetWindow(QWidget *widget) {
	SetWindowCaptureExcluded(widget, false);
}

} // namespace AyuFeatures::StreamerMode
