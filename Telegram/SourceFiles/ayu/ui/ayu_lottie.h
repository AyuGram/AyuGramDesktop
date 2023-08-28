// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#pragma once

#include "lottie/lottie_icon.h"

namespace AyuUi {
	std::unique_ptr<Lottie::Icon> getLottie(const QString& text);
}
