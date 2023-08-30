// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#ifndef WIN32

#include "streamer_mode.h"

#include "core/application.h"
#include "window/window_controller.h"

namespace AyuFeatures::StreamerMode
{
	bool isEnabledVal;

	bool isEnabled()
	{
		return isEnabledVal;
	}

	void enable()
	{
		isEnabledVal = true;
	}

	void disable()
	{
		isEnabledVal = false;
	}

	void hideWidgetWindow(QWidget* widget) {

	}

	void showWidgetWindow(QWidget* widget) {

	}
}

#endif
