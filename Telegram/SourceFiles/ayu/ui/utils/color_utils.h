// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <QColor>
#include <array>
#include <cmath>

namespace Ayu::Ui {

class ColorUtils
{
public:
	static std::array<float, 3> RGBToHSL(int r, int g, int b);
	static std::array<float, 3> colorToHSL(QRgb color);

	static QRgb HSLToRGB(float h, float s, float l);
	static QRgb HSLToRGB(const std::array<float, 3> &hsl);

	static double calculateLuminance(QRgb color);
	static double calculateContrast(QRgb foreground, QRgb background);
	static int calculateMinimumAlpha(QRgb foreground, QRgb background, float minContrastRatio);
	static QRgb compositeColors(QRgb foreground, QRgb background);
	static QRgb setAlphaComponent(QRgb color, int alpha);

private:
	static constexpr float constrain(float value, float min, float max) {
		return value < min ? min : (value > max ? max : value);
	}

	static constexpr int constrain(int value, int min, int max) {
		return value < min ? min : (value > max ? max : value);
	}

	static void colorToXYZ(QRgb color, double *outXyz);
	static int compositeAlpha(int foregroundAlpha, int backgroundAlpha);
	static int compositeComponent(int fgC, int fgA, int bgC, int bgA, int a);
};

} // namespace Ayu::Ui

