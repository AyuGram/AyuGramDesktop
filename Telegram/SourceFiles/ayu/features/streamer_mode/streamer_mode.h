// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#pragma once

#include "ui/rp_widget.h"

namespace AyuFeatures::StreamerMode
{
	bool isEnabled();
	void enable();
	void disable();
	void hideWidgetWindow(QWidget* widget);
	void showWidgetWindow(QWidget* widget);
}
