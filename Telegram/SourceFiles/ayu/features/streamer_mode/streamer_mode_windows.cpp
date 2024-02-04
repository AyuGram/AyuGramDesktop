// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#ifdef WIN32

#include "streamer_mode.h"

#include "core/application.h"
#include "window/window_controller.h"

namespace AyuFeatures::StreamerMode {

bool isEnabledVal;

bool isEnabled() {
	return isEnabledVal;
}

void enable() {
	auto handle = Core::App().activeWindow()->widget()->psHwnd();
	SetWindowDisplayAffinity(handle, WDA_EXCLUDEFROMCAPTURE);

	isEnabledVal = true;
}

void disable() {
	auto handle = Core::App().activeWindow()->widget()->psHwnd();
	SetWindowDisplayAffinity(handle, WDA_NONE);

	isEnabledVal = false;
}

void hideWidgetWindow(QWidget *widget) {
	auto handle = reinterpret_cast<HWND>(widget->window()->winId());
	SetWindowDisplayAffinity(handle, WDA_EXCLUDEFROMCAPTURE);
}

void showWidgetWindow(QWidget *widget) {
	auto handle = reinterpret_cast<HWND>(widget->window()->winId());
	SetWindowDisplayAffinity(handle, WDA_NONE);
}

}

#endif
