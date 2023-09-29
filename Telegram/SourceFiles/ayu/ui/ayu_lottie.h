// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "lottie/lottie_icon.h"
#include "ayu/ayu_lottie.h"

namespace AyuUi
{

class AyuLottieImpl : public AyuLottie
{
	std::unique_ptr<Lottie::Icon> getLottie(const QString &text) override;
};

}
