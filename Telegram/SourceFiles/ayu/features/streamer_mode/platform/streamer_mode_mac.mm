// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/streamer_mode/platform/streamer_mode_mac.h"

#include "core/application.h"
#include "window/window_controller.h"

#include <Cocoa/Cocoa.h>

namespace AyuFeatures::StreamerMode::Impl {

namespace {

NSWindow *NativeWindow(QWidget *widget) {
	return [reinterpret_cast<NSView*>(widget->window()->winId()) window];
}

} // namespace

void enableHook() {
	Core::App().enumerateWindows([&](not_null<Window::Controller*> w) {
		NativeWindow(w->widget()).sharingType = NSWindowSharingNone;
	});
}

void disableHook() {
	Core::App().enumerateWindows([&](not_null<Window::Controller*> w) {
		NativeWindow(w->widget()).sharingType = NSWindowSharingReadOnly;
	});
}

void hideWidgetWindow(QWidget *widget) {
	NativeWindow(widget).sharingType = NSWindowSharingNone;
}

void showWidgetWindow(QWidget *widget) {
	NativeWindow(widget).sharingType = NSWindowSharingReadOnly;
}

}
