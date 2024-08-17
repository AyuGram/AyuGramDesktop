// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024

#include "streamer_mode.h"

#include "window/window_controller.h"

#if defined Q_OS_WINRT || defined Q_OS_WIN
#include "ayu/features/streamer_mode/platform/streamer_mode_win.h"
#elif defined Q_OS_MAC // Q_OS_WINRT || Q_OS_WIN
#include "ayu/features/streamer_mode/platform/streamer_mode_mac.h"
#else // Q_OS_WINRT || Q_OS_WIN || Q_OS_MAC
#include "ayu/features/streamer_mode/platform/streamer_mode_linux.h"
#endif // else for Q_OS_WINRT || Q_OS_WIN || Q_OS_MAC

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
	Impl::enableHook();
}

void disable()
{
	isEnabledVal = false;
	Impl::disableHook();
}

void hideWidgetWindow(QWidget *widget)
{
	Impl::hideWidgetWindow(widget);
}

void showWidgetWindow(QWidget *widget)
{
	Impl::showWidgetWindow(widget);
}

}
