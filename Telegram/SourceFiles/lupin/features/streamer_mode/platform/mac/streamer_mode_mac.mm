// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "lupin/features/streamer_mode/platform/mac/streamer_mode_mac.h"

#include <QtWidgets/QWidget>

#include <Cocoa/Cocoa.h>

namespace LupinFeatures::StreamerMode::Platform {

void SetWindowCaptureExcluded(
		not_null<QWidget*> widget,
		bool excluded) {
	const auto view = reinterpret_cast<NSView*>(widget->winId());
	view.window.sharingType = excluded
		? NSWindowSharingNone
		: NSWindowSharingReadOnly;
}

} // namespace LupinFeatures::StreamerMode::Platform
