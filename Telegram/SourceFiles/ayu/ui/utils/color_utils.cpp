// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
//
// Code is based on https://github.com/androidx/androidx
#include "ayu/ui/utils/color_utils.h"

#include <algorithm>
#include <cmath>
#include <QColor>

namespace Ayu::Ui {

std::array<float, 3> ColorUtils::RGBToHSL(int r, int g, int b) {
	const auto rf = r / 255.0f;
	const auto gf = g / 255.0f;
	const auto bf = b / 255.0f;

	const auto max = std::max({rf, gf, bf});
	const auto min = std::min({rf, gf, bf});
	const auto deltaMaxMin = max - min;

	float h = 0.0f, s = 0.0f;
	const auto l = (max + min) / 2.0f;

	if (max == min) {
		h = s = 0.0f;
	} else {
		if (max == rf) {
			h = std::fmod((gf - bf) / deltaMaxMin, 6.0f);
		} else if (max == gf) {
			h = ((bf - rf) / deltaMaxMin) + 2.0f;
		} else {
			h = ((rf - gf) / deltaMaxMin) + 4.0f;
		}

		s = deltaMaxMin / (1.0f - std::abs(2.0f * l - 1.0f));
	}

	h = std::fmod(h * 60.0f, 360.0f);
	if (h < 0) {
		h += 360.0f;
	}

	return {
		constrain(h, 0.0f, 360.0f),
		constrain(s, 0.0f, 1.0f),
		constrain(l, 0.0f, 1.0f)
	};
}

std::array<float, 3> ColorUtils::colorToHSL(QRgb color) {
	return RGBToHSL(qRed(color), qGreen(color), qBlue(color));
}

QRgb ColorUtils::HSLToRGB(float h, float s, float l) {
	const auto c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
	const auto m = l - 0.5f * c;
	const auto x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));

	const auto hueSegment = static_cast<int>(h / 60.0f);

	int r = 0, g = 0, b = 0;

	switch (hueSegment) {
		case 0: r = static_cast<int>(std::round(255 * (c + m)));
			g = static_cast<int>(std::round(255 * (x + m)));
			b = static_cast<int>(std::round(255 * m));
			break;
		case 1: r = static_cast<int>(std::round(255 * (x + m)));
			g = static_cast<int>(std::round(255 * (c + m)));
			b = static_cast<int>(std::round(255 * m));
			break;
		case 2: r = static_cast<int>(std::round(255 * m));
			g = static_cast<int>(std::round(255 * (c + m)));
			b = static_cast<int>(std::round(255 * (x + m)));
			break;
		case 3: r = static_cast<int>(std::round(255 * m));
			g = static_cast<int>(std::round(255 * (x + m)));
			b = static_cast<int>(std::round(255 * (c + m)));
			break;
		case 4: r = static_cast<int>(std::round(255 * (x + m)));
			g = static_cast<int>(std::round(255 * m));
			b = static_cast<int>(std::round(255 * (c + m)));
			break;
		case 5:
		case 6: r = static_cast<int>(std::round(255 * (c + m)));
			g = static_cast<int>(std::round(255 * m));
			b = static_cast<int>(std::round(255 * (x + m)));
			break;
	}

	r = constrain(r, 0, 255);
	g = constrain(g, 0, 255);
	b = constrain(b, 0, 255);

	return qRgb(r, g, b);
}

QRgb ColorUtils::HSLToRGB(const std::array<float, 3> &hsl) {
	return HSLToRGB(hsl[0], hsl[1], hsl[2]);
}

void ColorUtils::colorToXYZ(QRgb color, double *outXyz) {
	auto r = qRed(color) / 255.0;
	auto g = qGreen(color) / 255.0;
	auto b = qBlue(color) / 255.0;

	if (r > 0.04045) {
		r = std::pow((r + 0.055) / 1.055, 2.4);
	} else {
		r = r / 12.92;
	}

	if (g > 0.04045) {
		g = std::pow((g + 0.055) / 1.055, 2.4);
	} else {
		g = g / 12.92;
	}

	if (b > 0.04045) {
		b = std::pow((b + 0.055) / 1.055, 2.4);
	} else {
		b = b / 12.92;
	}

	outXyz[0] = 100 * (r * 0.4124 + g * 0.3576 + b * 0.1805);
	outXyz[1] = 100 * (r * 0.2126 + g * 0.7152 + b * 0.0722);
	outXyz[2] = 100 * (r * 0.0193 + g * 0.1192 + b * 0.9505);
}

double ColorUtils::calculateLuminance(QRgb color) {
	double xyz[3];
	colorToXYZ(color, xyz);
	return xyz[1] / 100.0;
}

double ColorUtils::calculateContrast(QRgb foreground, QRgb background) {
	if (qAlpha(background) != 255) {
		return -1.0;
	}

	if (qAlpha(foreground) < 255) {
		foreground = compositeColors(foreground, background);
	}

	const auto luminance1 = calculateLuminance(foreground) + 0.05;
	const auto luminance2 = calculateLuminance(background) + 0.05;

	return std::max(luminance1, luminance2) / std::min(luminance1, luminance2);
}

int ColorUtils::calculateMinimumAlpha(QRgb foreground, QRgb background, float minContrastRatio) {
	if (qAlpha(background) != 255) {
		return -1;
	}

	auto testForeground = setAlphaComponent(foreground, 255);
	auto testRatio = calculateContrast(testForeground, background);
	if (testRatio < minContrastRatio) {
		return -1;
	}

	constexpr int MAX_ITERATIONS = 10;
	constexpr int PRECISION = 1;

	int numIterations = 0;
	int minAlpha = 0;
	int maxAlpha = 255;

	while (numIterations <= MAX_ITERATIONS && (maxAlpha - minAlpha) > PRECISION) {
		const auto testAlpha = (minAlpha + maxAlpha) / 2;

		testForeground = setAlphaComponent(foreground, testAlpha);
		testRatio = calculateContrast(testForeground, background);

		if (testRatio < minContrastRatio) {
			minAlpha = testAlpha;
		} else {
			maxAlpha = testAlpha;
		}

		numIterations++;
	}

	return maxAlpha;
}

int ColorUtils::compositeAlpha(int foregroundAlpha, int backgroundAlpha) {
	return 0xFF - (((0xFF - backgroundAlpha) * (0xFF - foregroundAlpha)) / 0xFF);
}

int ColorUtils::compositeComponent(int fgC, int fgA, int bgC, int bgA, int a) {
	if (a == 0) return 0;
	return ((0xFF * fgC * fgA) + (bgC * bgA * (0xFF - fgA))) / (a * 0xFF);
}

QRgb ColorUtils::compositeColors(QRgb foreground, QRgb background) {
	const auto bgAlpha = qAlpha(background);
	const auto fgAlpha = qAlpha(foreground);
	const auto a = compositeAlpha(fgAlpha, bgAlpha);

	const auto r = compositeComponent(qRed(foreground),
									  fgAlpha,
									  qRed(background),
									  bgAlpha,
									  a);
	const auto g = compositeComponent(qGreen(foreground),
									  fgAlpha,
									  qGreen(background),
									  bgAlpha,
									  a);
	const auto b = compositeComponent(qBlue(foreground),
									  fgAlpha,
									  qBlue(background),
									  bgAlpha,
									  a);

	return qRgba(r, g, b, a);
}

QRgb ColorUtils::setAlphaComponent(QRgb color, int alpha) {
	return qRgba(qRed(color), qGreen(color), qBlue(color), alpha);
}

} // namespace Ayu::Ui

