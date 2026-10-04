// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
//
// Code is based on https://github.com/androidx/androidx/blob/androidx-main/palette/palette/src/main/java/androidx/palette/graphics/ColorCutQuantizer.java
#include "ayu/ui/utils/color_cut_quantizer.h"

#include "color_utils.h"

#include <algorithm>
#include <cmath>

namespace Ayu::Ui {

ColorCutQuantizer::ColorCutQuantizer(
	const std::vector<int> &pixels,
	int maxColors,
	const std::vector<Filter*> &filters)
	: _histogram(1 << (QUANTIZE_WORD_WIDTH * 3), 0)
	  , _filters(filters) {
	std::vector<int> quantizedPixels(pixels.size());

	for (size_t i = 0; i < pixels.size(); ++i) {
		const auto quantizedColor = quantizeFromRgb888(pixels[i]);
		quantizedPixels[i] = quantizedColor;
		_histogram[quantizedColor]++;
	}

	int distinctColorCount = 0;
	for (size_t color = 0; color < _histogram.size(); ++color) {
		if (_histogram[color] > 0 && shouldIgnoreColor(color)) {
			_histogram[color] = 0;
		}
		if (_histogram[color] > 0) {
			distinctColorCount++;
		}
	}

	_colors.reserve(distinctColorCount);
	for (size_t color = 0; color < _histogram.size(); ++color) {
		if (_histogram[color] > 0) {
			_colors.push_back(color);
		}
	}

	if (distinctColorCount <= maxColors) {
		_quantizedColors.reserve(_colors.size());
		for (const auto color : _colors) {
			_quantizedColors.emplace_back(approximateToRgb888(color), _histogram[color]);
		}
	} else {
		_quantizedColors = quantizePixels(maxColors);
	}
}

std::vector<Swatch> ColorCutQuantizer::quantizedColors() const {
	return _quantizedColors;
}

std::vector<Swatch> ColorCutQuantizer::quantizePixels(int maxColors) {
	using VboxComparator = std::function<bool(const Vbox &, const Vbox &)>;
	VboxComparator comparator = [](const Vbox &lhs, const Vbox &rhs)
	{
		return lhs.volume() < rhs.volume();
	};

	std::priority_queue<Vbox, std::vector<Vbox>, VboxComparator> pq(comparator);

	pq.emplace(0, _colors.size() - 1, this);

	splitBoxes(pq, maxColors);

	std::vector<Vbox> vboxes;
	while (!pq.empty()) {
		vboxes.push_back(pq.top());
		pq.pop();
	}

	return generateAverageColors(vboxes);
}

void ColorCutQuantizer::splitBoxes(
	std::priority_queue<Vbox, std::vector<Vbox>, std::function<bool(const Vbox &, const Vbox &)>> &queue,
	int maxSize) {
	while (static_cast<int>(queue.size()) < maxSize) {
		if (queue.empty()) {
			return;
		}

		auto vbox = queue.top();
		queue.pop();

		if (vbox.canSplit()) {
			queue.push(vbox.splitBox());
			queue.push(vbox);
		} else {
			return;
		}
	}
}

std::vector<Swatch> ColorCutQuantizer::generateAverageColors(const std::vector<Vbox> &vboxes) {
	std::vector<Swatch> colors;
	colors.reserve(vboxes.size());

	for (const auto &vbox : vboxes) {
		auto swatch = vbox.averageColor();
		if (!shouldIgnoreColor(swatch)) {
			colors.push_back(swatch);
		}
	}

	return colors;
}

bool ColorCutQuantizer::shouldIgnoreColor(int color565) const {
	const auto rgb = approximateToRgb888(color565);
	_tempHsl = ColorUtils::colorToHSL(rgb);
	return shouldIgnoreColor(rgb, _tempHsl);
}

bool ColorCutQuantizer::shouldIgnoreColor(const Swatch &swatch) const {
	return shouldIgnoreColor(swatch.rgb(), swatch.hsl());
}

bool ColorCutQuantizer::shouldIgnoreColor(QRgb rgb, const std::array<float, 3> &hsl) const {
	if (!_filters.empty()) {
		for (const auto filter : _filters) {
			if (!(*filter)(rgb, hsl)) {
				return true;
			}
		}
	}
	return false;
}

int ColorCutQuantizer::quantizeFromRgb888(QRgb color) {
	const auto r = modifyWordWidth(qRed(color), 8, QUANTIZE_WORD_WIDTH);
	const auto g = modifyWordWidth(qGreen(color), 8, QUANTIZE_WORD_WIDTH);
	const auto b = modifyWordWidth(qBlue(color), 8, QUANTIZE_WORD_WIDTH);
	return r << (QUANTIZE_WORD_WIDTH + QUANTIZE_WORD_WIDTH) | g << QUANTIZE_WORD_WIDTH | b;
}

int ColorCutQuantizer::approximateToRgb888(int r, int g, int b) {
	return qRgb(
		modifyWordWidth(r, QUANTIZE_WORD_WIDTH, 8),
		modifyWordWidth(g, QUANTIZE_WORD_WIDTH, 8),
		modifyWordWidth(b, QUANTIZE_WORD_WIDTH, 8));
}

int ColorCutQuantizer::approximateToRgb888(int color) {
	return approximateToRgb888(quantizedRed(color), quantizedGreen(color), quantizedBlue(color));
}

int ColorCutQuantizer::quantizedRed(int color) {
	return (color >> (QUANTIZE_WORD_WIDTH + QUANTIZE_WORD_WIDTH)) & QUANTIZE_WORD_MASK;
}

int ColorCutQuantizer::quantizedGreen(int color) {
	return (color >> QUANTIZE_WORD_WIDTH) & QUANTIZE_WORD_MASK;
}

int ColorCutQuantizer::quantizedBlue(int color) {
	return color & QUANTIZE_WORD_MASK;
}

int ColorCutQuantizer::modifyWordWidth(int value, int currentWidth, int targetWidth) {
	int newValue;
	if (targetWidth > currentWidth) {
		newValue = value << (targetWidth - currentWidth);
	} else {
		newValue = value >> (currentWidth - targetWidth);
	}
	return newValue & ((1 << targetWidth) - 1);
}

void ColorCutQuantizer::modifySignificantOctet(
	std::vector<int> &colors,
	int dimension,
	int lower,
	int upper) {
	switch (dimension) {
		case COMPONENT_RED: break;
		case COMPONENT_GREEN: for (int i = lower; i <= upper; ++i) {
				const auto color = colors[i];
				colors[i] = quantizedGreen(color) << (QUANTIZE_WORD_WIDTH + QUANTIZE_WORD_WIDTH)
					| quantizedRed(color) << QUANTIZE_WORD_WIDTH
					| quantizedBlue(color);
			}
			break;
		case COMPONENT_BLUE: for (int i = lower; i <= upper; ++i) {
				const auto color = colors[i];
				colors[i] = quantizedBlue(color) << (QUANTIZE_WORD_WIDTH + QUANTIZE_WORD_WIDTH)
					| quantizedGreen(color) << QUANTIZE_WORD_WIDTH
					| quantizedRed(color);
			}
			break;
		default: break;
	}
}

ColorCutQuantizer::Vbox::Vbox(int lowerIndex, int upperIndex, ColorCutQuantizer *quantizer)
	: _lowerIndex(lowerIndex)
	  , _upperIndex(upperIndex)
	  , _population(0)
	  , _minRed(0), _maxRed(0)
	  , _minGreen(0), _maxGreen(0)
	  , _minBlue(0), _maxBlue(0)
	  , _quantizer(quantizer) {
	fitBox();
}

int ColorCutQuantizer::Vbox::volume() const {
	return (_maxRed - _minRed + 1) * (_maxGreen - _minGreen + 1) * (_maxBlue - _minBlue + 1);
}

bool ColorCutQuantizer::Vbox::canSplit() const {
	return colorCount() > 1;
}

int ColorCutQuantizer::Vbox::colorCount() const {
	return 1 + _upperIndex - _lowerIndex;
}

void ColorCutQuantizer::Vbox::fitBox() {
	const auto &colors = _quantizer->_colors;
	const auto &hist = _quantizer->_histogram;

	int minRed = std::numeric_limits<int>::max();
	int minGreen = std::numeric_limits<int>::max();
	int minBlue = std::numeric_limits<int>::max();
	int maxRed = std::numeric_limits<int>::min();
	int maxGreen = std::numeric_limits<int>::min();
	int maxBlue = std::numeric_limits<int>::min();
	int count = 0;

	for (int i = _lowerIndex; i <= _upperIndex; ++i) {
		const auto color = colors[i];
		count += hist[color];

		const auto r = quantizedRed(color);
		const auto g = quantizedGreen(color);
		const auto b = quantizedBlue(color);

		maxRed = std::max(maxRed, r);
		minRed = std::min(minRed, r);
		maxGreen = std::max(maxGreen, g);
		minGreen = std::min(minGreen, g);
		maxBlue = std::max(maxBlue, b);
		minBlue = std::min(minBlue, b);
	}

	_minRed = minRed;
	_maxRed = maxRed;
	_minGreen = minGreen;
	_maxGreen = maxGreen;
	_minBlue = minBlue;
	_maxBlue = maxBlue;
	_population = count;
}

ColorCutQuantizer::Vbox ColorCutQuantizer::Vbox::splitBox() {
	if (!canSplit()) {
		throw std::runtime_error("Cannot split a box with only 1 color");
	}

	const auto splitPoint = findSplitPoint();
	auto newBox = Vbox(splitPoint + 1, _upperIndex, _quantizer);

	_upperIndex = splitPoint;
	fitBox();

	return newBox;
}

int ColorCutQuantizer::Vbox::longestColorDimension() const {
	const auto redLength = _maxRed - _minRed;
	const auto greenLength = _maxGreen - _minGreen;
	const auto blueLength = _maxBlue - _minBlue;

	if (redLength >= greenLength && redLength >= blueLength) {
		return COMPONENT_RED;
	} else if (greenLength >= redLength && greenLength >= blueLength) {
		return COMPONENT_GREEN;
	} else {
		return COMPONENT_BLUE;
	}
}

int ColorCutQuantizer::Vbox::findSplitPoint() {
	const auto longestDimension = longestColorDimension();
	auto &colors = _quantizer->_colors;
	const auto &hist = _quantizer->_histogram;

	modifySignificantOctet(colors, longestDimension, _lowerIndex, _upperIndex);
	std::sort(colors.begin() + _lowerIndex, colors.begin() + _upperIndex + 1);
	modifySignificantOctet(colors, longestDimension, _lowerIndex, _upperIndex);

	const auto midPoint = _population / 2;
	int count = 0;
	for (int i = _lowerIndex; i <= _upperIndex; ++i) {
		count += hist[colors[i]];
		if (count >= midPoint) {
			return std::min(_upperIndex - 1, i);
		}
	}

	return _lowerIndex;
}

Swatch ColorCutQuantizer::Vbox::averageColor() const {
	const auto &colors = _quantizer->_colors;
	const auto &hist = _quantizer->_histogram;

	int redSum = 0;
	int greenSum = 0;
	int blueSum = 0;
	int totalPopulation = 0;

	for (int i = _lowerIndex; i <= _upperIndex; ++i) {
		const auto color = colors[i];
		const auto colorPopulation = hist[color];

		totalPopulation += colorPopulation;
		redSum += colorPopulation * quantizedRed(color);
		greenSum += colorPopulation * quantizedGreen(color);
		blueSum += colorPopulation * quantizedBlue(color);
	}

	const auto redMean = static_cast<int>(std::round(redSum / static_cast<float>(totalPopulation)));
	const auto greenMean = static_cast<int>(std::round(greenSum / static_cast<float>(totalPopulation)));
	const auto blueMean = static_cast<int>(std::round(blueSum / static_cast<float>(totalPopulation)));

	return Swatch(approximateToRgb888(redMean, greenMean, blueMean), totalPopulation);
}

} // namespace Ayu::Ui
