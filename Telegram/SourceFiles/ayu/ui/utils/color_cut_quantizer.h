// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/ui/utils/palette.h"

#include <functional>
#include <queue>
#include <vector>

namespace Ayu::Ui {

class ColorCutQuantizer
{
public:
	using Filter = std::function<bool(QRgb, const std::array<float, 3> &)>;

	ColorCutQuantizer(
		const std::vector<int> &pixels,
		int maxColors,
		const std::vector<Filter*> &filters);

	[[nodiscard]] std::vector<Swatch> quantizedColors() const;

private:
	class Vbox
	{
	public:
		Vbox(int lowerIndex, int upperIndex, ColorCutQuantizer *quantizer);

		[[nodiscard]] int volume() const;
		[[nodiscard]] bool canSplit() const;
		[[nodiscard]] int colorCount() const;

		void fitBox();
		Vbox splitBox();
		[[nodiscard]] Swatch averageColor() const;

	private:
		[[nodiscard]] int longestColorDimension() const;
		[[nodiscard]] int findSplitPoint();

		int _lowerIndex;
		int _upperIndex;
		int _population;
		int _minRed, _maxRed;
		int _minGreen, _maxGreen;
		int _minBlue, _maxBlue;
		ColorCutQuantizer *_quantizer;
	};

	static constexpr int COMPONENT_RED = -3;
	static constexpr int COMPONENT_GREEN = -2;
	static constexpr int COMPONENT_BLUE = -1;
	static constexpr int QUANTIZE_WORD_WIDTH = 5;
	static constexpr int QUANTIZE_WORD_MASK = (1 << QUANTIZE_WORD_WIDTH) - 1;

	std::vector<Swatch> quantizePixels(int maxColors);
	void splitBoxes(
		std::priority_queue<Vbox, std::vector<Vbox>, std::function<bool(const Vbox &, const Vbox &)>> &queue,
		int maxSize);
	std::vector<Swatch> generateAverageColors(const std::vector<Vbox> &vboxes);

	[[nodiscard]] bool shouldIgnoreColor(int color565) const;
	[[nodiscard]] bool shouldIgnoreColor(const Swatch &swatch) const;
	[[nodiscard]] bool shouldIgnoreColor(QRgb rgb, const std::array<float, 3> &hsl) const;

	static int quantizeFromRgb888(QRgb color);
	static int approximateToRgb888(int r, int g, int b);
	static int approximateToRgb888(int color);
	static int quantizedRed(int color);
	static int quantizedGreen(int color);
	static int quantizedBlue(int color);
	static int modifyWordWidth(int value, int currentWidth, int targetWidth);
	static void modifySignificantOctet(std::vector<int> &colors, int dimension, int lower, int upper);

	std::vector<int> _colors;
	std::vector<int> _histogram;
	std::vector<Swatch> _quantizedColors;
	std::vector<Filter*> _filters;
	mutable std::array<float, 3> _tempHsl;

	friend class Vbox;
};

} // namespace Ayu::Ui

