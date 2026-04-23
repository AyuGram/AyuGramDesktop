// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
//
// Code is based on:
// - https://github.com/androidx/androidx/blob/androidx-main/palette/palette/src/main/java/androidx/palette/graphics/Palette.java
// - https://github.com/androidx/androidx/blob/androidx-main/palette/palette/src/main/java/androidx/palette/graphics/Target.java
#include "ayu/ui/utils/palette.h"

#include "color_cut_quantizer.h"
#include "color_utils.h"

#include <algorithm>
#include <cmath>
#include <QImage>

namespace Ayu::Ui {

Swatch::Swatch(QRgb color, int population)
	: _red(qRed(color))
	  , _green(qGreen(color))
	  , _blue(qBlue(color))
	  , _rgb(color)
	  , _population(population) {
}

QRgb Swatch::rgb() const {
	return _rgb;
}

int Swatch::red() const {
	return _red;
}

int Swatch::green() const {
	return _green;
}

int Swatch::blue() const {
	return _blue;
}

int Swatch::population() const {
	return _population;
}

std::array<float, 3> Swatch::hsl() const {
	if (!_hslCalculated) {
		_hsl = ColorUtils::RGBToHSL(_red, _green, _blue);
		_hslCalculated = true;
	}
	return _hsl;
}

QColor Swatch::titleTextColor() const {
	ensureTextColorsGenerated();
	return _titleTextColor;
}

QColor Swatch::bodyTextColor() const {
	ensureTextColorsGenerated();
	return _bodyTextColor;
}

void Swatch::ensureTextColorsGenerated() const {
	if (!_generatedTextColors) {
		const auto lightBodyAlpha = ColorUtils::calculateMinimumAlpha(
			qRgb(255, 255, 255),
			_rgb,
			Palette::MIN_CONTRAST_BODY_TEXT);
		const auto lightTitleAlpha = ColorUtils::calculateMinimumAlpha(
			qRgb(255, 255, 255),
			_rgb,
			Palette::MIN_CONTRAST_TITLE_TEXT);

		if (lightBodyAlpha != -1 && lightTitleAlpha != -1) {
			_bodyTextColor = QColor(255, 255, 255, lightBodyAlpha);
			_titleTextColor = QColor(255, 255, 255, lightTitleAlpha);
			_generatedTextColors = true;
			return;
		}

		const auto darkBodyAlpha = ColorUtils::calculateMinimumAlpha(
			qRgb(0, 0, 0),
			_rgb,
			Palette::MIN_CONTRAST_BODY_TEXT);
		const auto darkTitleAlpha = ColorUtils::calculateMinimumAlpha(
			qRgb(0, 0, 0),
			_rgb,
			Palette::MIN_CONTRAST_TITLE_TEXT);

		if (darkBodyAlpha != -1 && darkTitleAlpha != -1) {
			_bodyTextColor = QColor(0, 0, 0, darkBodyAlpha);
			_titleTextColor = QColor(0, 0, 0, darkTitleAlpha);
			_generatedTextColors = true;
			return;
		}

		_bodyTextColor = lightBodyAlpha != -1
							 ? QColor(255, 255, 255, lightBodyAlpha)
							 : QColor(0, 0, 0, darkBodyAlpha);
		_titleTextColor = lightTitleAlpha != -1
							  ? QColor(255, 255, 255, lightTitleAlpha)
							  : QColor(0, 0, 0, darkTitleAlpha);
		_generatedTextColors = true;
	}
}

const Target Target::LIGHT_VIBRANT = []()
{
	Target t;
	setDefaultLightLightnessValues(t);
	setDefaultVibrantSaturationValues(t);
	return t;
}();

const Target Target::VIBRANT = []()
{
	Target t;
	setDefaultNormalLightnessValues(t);
	setDefaultVibrantSaturationValues(t);
	return t;
}();

const Target Target::DARK_VIBRANT = []()
{
	Target t;
	setDefaultDarkLightnessValues(t);
	setDefaultVibrantSaturationValues(t);
	return t;
}();

const Target Target::LIGHT_MUTED = []()
{
	Target t;
	setDefaultLightLightnessValues(t);
	setDefaultMutedSaturationValues(t);
	return t;
}();

const Target Target::MUTED = []()
{
	Target t;
	setDefaultNormalLightnessValues(t);
	setDefaultMutedSaturationValues(t);
	return t;
}();

const Target Target::DARK_MUTED = []()
{
	Target t;
	setDefaultDarkLightnessValues(t);
	setDefaultMutedSaturationValues(t);
	return t;
}();

Target::Target() {
	setTargetDefaultValues(_saturationTargets);
	setTargetDefaultValues(_lightnessTargets);
	setDefaultWeights(*this);
}

Target::Target(const Target &from)
	: _saturationTargets(from._saturationTargets)
	  , _lightnessTargets(from._lightnessTargets)
	  , _weights(from._weights)
	  , _isExclusive(from._isExclusive) {
}

bool Target::operator==(const Target &other) const {
	for (int i = 0; i < 3; ++i) {
		if (_saturationTargets[i] != other._saturationTargets[i]) return false;
		if (_lightnessTargets[i] != other._lightnessTargets[i]) return false;
		if (_weights[i] != other._weights[i]) return false;
	}
	return _isExclusive == other._isExclusive;
}

float Target::minimumSaturation() const {
	return _saturationTargets[0];
}

float Target::targetSaturation() const {
	return _saturationTargets[1];
}

float Target::maximumSaturation() const {
	return _saturationTargets[2];
}

float Target::minimumLightness() const {
	return _lightnessTargets[0];
}

float Target::targetLightness() const {
	return _lightnessTargets[1];
}

float Target::maximumLightness() const {
	return _lightnessTargets[2];
}

float Target::saturationWeight() const {
	return _weights[0];
}

float Target::lightnessWeight() const {
	return _weights[1];
}

float Target::populationWeight() const {
	return _weights[2];
}

bool Target::isExclusive() const {
	return _isExclusive;
}

void Target::normalizeWeights() {
	float sum = 0.0f;
	for (int i = 0; i < _weights.size(); i++) {
		float weight = _weights[i];
		if (weight > 0) {
			sum += weight;
		}
	}
	if (sum != 0.0f) {
		for (int i = 0; i < _weights.size(); i++) {
			if (_weights[i] > 0) {
				_weights[i] /= sum;
			}
		}
	}
}

void Target::setDefaultLightLightnessValues(Target &target) {
	target._lightnessTargets[0] = 0.55f;
	target._lightnessTargets[1] = 0.74f;
}

void Target::setDefaultNormalLightnessValues(Target &target) {
	target._lightnessTargets[0] = 0.3f;
	target._lightnessTargets[1] = 0.5f;
	target._lightnessTargets[2] = 0.7f;
}

void Target::setDefaultDarkLightnessValues(Target &target) {
	target._lightnessTargets[1] = 0.26f;
	target._lightnessTargets[2] = 0.45f;
}

void Target::setDefaultVibrantSaturationValues(Target &target) {
	target._saturationTargets[0] = 0.35f;
	target._saturationTargets[1] = 1.0f;
}

void Target::setDefaultMutedSaturationValues(Target &target) {
	target._saturationTargets[1] = 0.3f;
	target._saturationTargets[2] = 0.4f;
}

void Target::setTargetDefaultValues(std::array<float, 3> &values) {
	values[0] = 0.0f;
	values[1] = 0.5f;
	values[2] = 1.0f;
}

void Target::setDefaultWeights(Target &target) {
	target._weights[0] = 0.24f;
	target._weights[1] = 0.52f;
	target._weights[2] = 0.24f;
}

Palette::Palette(
	std::vector<Swatch> swatches,
	std::vector<Target> targets)
	: _swatches(std::move(swatches))
	  , _targets(std::move(targets)) {
}

const std::vector<Swatch> &Palette::swatches() const {
	return _swatches;
}

const std::vector<Target> &Palette::targets() const {
	return _targets;
}

const Swatch *Palette::vibrantSwatch() const {
	return swatchForTarget(Target::VIBRANT);
}

const Swatch *Palette::lightVibrantSwatch() const {
	return swatchForTarget(Target::LIGHT_VIBRANT);
}

const Swatch *Palette::darkVibrantSwatch() const {
	return swatchForTarget(Target::DARK_VIBRANT);
}

const Swatch *Palette::mutedSwatch() const {
	return swatchForTarget(Target::MUTED);
}

const Swatch *Palette::lightMutedSwatch() const {
	return swatchForTarget(Target::LIGHT_MUTED);
}

const Swatch *Palette::darkMutedSwatch() const {
	return swatchForTarget(Target::DARK_MUTED);
}

const Swatch *Palette::dominantSwatch() const {
	return _dominantSwatch;
}

QRgb Palette::vibrantColor(QRgb defaultColor) const {
	return colorForTarget(Target::VIBRANT, defaultColor);
}

QRgb Palette::lightVibrantColor(QRgb defaultColor) const {
	return colorForTarget(Target::LIGHT_VIBRANT, defaultColor);
}

QRgb Palette::darkVibrantColor(QRgb defaultColor) const {
	return colorForTarget(Target::DARK_VIBRANT, defaultColor);
}

QRgb Palette::mutedColor(QRgb defaultColor) const {
	return colorForTarget(Target::MUTED, defaultColor);
}

QRgb Palette::lightMutedColor(QRgb defaultColor) const {
	return colorForTarget(Target::LIGHT_MUTED, defaultColor);
}

QRgb Palette::darkMutedColor(QRgb defaultColor) const {
	return colorForTarget(Target::DARK_MUTED, defaultColor);
}

QRgb Palette::dominantColor(QRgb defaultColor) const {
	return _dominantSwatch ? _dominantSwatch->rgb() : defaultColor;
}

const Swatch *Palette::swatchForTarget(const Target &target) const {
	for (const auto &[key, swatch] : _selectedSwatches) {
		if (key == target) {
			return swatch;
		}
	}
	return nullptr;
}

QRgb Palette::colorForTarget(const Target &target, QRgb defaultColor) const {
	const auto swatch = swatchForTarget(target);
	return swatch ? swatch->rgb() : defaultColor;
}

void Palette::generate() {
	_selectedSwatches.clear();
	_dominantSwatch = findDominantSwatch();

	for (auto &target : _targets) {
		target.normalizeWeights();
		const auto swatch = generateScoredTarget(target);
		_selectedSwatches.push_back({ target, swatch });
	}

	_usedColors.clear();
}

const Swatch *Palette::generateScoredTarget(const Target &target) {
	const auto maxScoreSwatch = getMaxScoredSwatchForTarget(target);
	if (maxScoreSwatch && target.isExclusive()) {
		_usedColors.insert(maxScoreSwatch->rgb());
	}
	return maxScoreSwatch;
}

const Swatch *Palette::getMaxScoredSwatchForTarget(const Target &target) {
	float maxScore = 0.0f;
	const Swatch *maxScoreSwatch = nullptr;

	for (const auto &swatch : _swatches) {
		if (shouldBeScoredForTarget(swatch, target)) {
			const auto score = generateScore(swatch, target);
			if (!maxScoreSwatch || score > maxScore) {
				maxScoreSwatch = &swatch;
				maxScore = score;
			}
		}
	}

	return maxScoreSwatch;
}

bool Palette::shouldBeScoredForTarget(const Swatch &swatch, const Target &target) {
	const auto hsl = swatch.hsl();
	return hsl[1] >= target.minimumSaturation()
		&& hsl[1] <= target.maximumSaturation()
		&& hsl[2] >= target.minimumLightness()
		&& hsl[2] <= target.maximumLightness()
		&& _usedColors.find(swatch.rgb()) == _usedColors.end();
}

float Palette::generateScore(const Swatch &swatch, const Target &target) {
	const auto hsl = swatch.hsl();

	float saturationScore = 0.0f;
	float luminanceScore = 0.0f;
	float populationScore = 0.0f;

	const auto maxPopulation = _dominantSwatch ? _dominantSwatch->population() : 1;

	if (target.saturationWeight() > 0) {
		saturationScore = target.saturationWeight()
			* (1.0f - std::abs(hsl[1] - target.targetSaturation()));
	}
	if (target.lightnessWeight() > 0) {
		luminanceScore = target.lightnessWeight()
			* (1.0f - std::abs(hsl[2] - target.targetLightness()));
	}
	if (target.populationWeight() > 0) {
		populationScore = target.populationWeight()
			* (static_cast<float>(swatch.population()) / static_cast<float>(maxPopulation));
	}

	return saturationScore + luminanceScore + populationScore;
}

const Swatch *Palette::findDominantSwatch() {
	int maxPop = 0;
	const Swatch *maxSwatch = nullptr;

	for (const auto &swatch : _swatches) {
		if (swatch.population() > maxPop) {
			maxSwatch = &swatch;
			maxPop = swatch.population();
		}
	}

	return maxSwatch;
}

Palette::Builder Palette::from(const QPixmap &pixmap) {
	return Builder(pixmap);
}

Palette::Builder Palette::from(const QImage &image) {
	return Builder(image);
}

Palette Palette::fromSwatches(const std::vector<Swatch> &swatches) {
	return Builder(swatches).generate();
}

Palette::Filter Palette::Builder::DEFAULT_FILTER = [](QRgb rgb, const std::array<float, 3> &hsl)
{
	const bool isBlack = (hsl[2] <= 0.05f);
	const bool isWhite = (hsl[2] >= 0.95f);
	const bool isNearRedILine = (hsl[0] >= 10.0f && hsl[0] <= 37.0f && hsl[1] <= 0.82f);
	return !isWhite && !isBlack && !isNearRedILine;
};

Palette::Builder::Builder(const QPixmap &pixmap)
	: Builder(pixmap.toImage()) {
}

Palette::Builder::Builder(const QImage &image)
	: _image(image)
	  , _hasImage(true) {
	_filters.push_back(DEFAULT_FILTER);

	_targets.push_back(Target::LIGHT_VIBRANT);
	_targets.push_back(Target::VIBRANT);
	_targets.push_back(Target::DARK_VIBRANT);
	_targets.push_back(Target::LIGHT_MUTED);
	_targets.push_back(Target::MUTED);
	_targets.push_back(Target::DARK_MUTED);
}

Palette::Builder::Builder(const std::vector<Swatch> &swatches)
	: _swatches(swatches)
	  , _hasImage(false) {
	_filters.push_back(DEFAULT_FILTER);
}

Palette::Builder &Palette::Builder::maximumColorCount(int colors) {
	_maxColors = colors;
	return *this;
}

Palette::Builder &Palette::Builder::resizeBitmapArea(int area) {
	_resizeArea = area;
	return *this;
}

Palette::Builder &Palette::Builder::clearFilters() {
	_filters.clear();
	return *this;
}

Palette::Builder &Palette::Builder::addFilter(Filter filter) {
	_filters.push_back(std::move(filter));
	return *this;
}

Palette::Builder &Palette::Builder::setRegion(int left, int top, int right, int bottom) {
	if (_hasImage) {
		_region = QRect(0, 0, _image.width(), _image.height());
		_region = _region.intersected(QRect(left, top, right - left, bottom - top));
		_hasRegion = true;
	}
	return *this;
}

Palette::Builder &Palette::Builder::clearRegion() {
	_hasRegion = false;
	_region = QRect();
	return *this;
}

Palette::Builder &Palette::Builder::addTarget(const Target &target) {
	bool found = false;
	for (const auto &t : _targets) {
		if (t == target) {
			found = true;
			break;
		}
	}
	if (!found) {
		_targets.push_back(target);
	}
	return *this;
}

Palette::Builder &Palette::Builder::clearTargets() {
	_targets.clear();
	return *this;
}

Palette Palette::Builder::generate() {
	std::vector<Swatch> swatches;

	if (_hasImage) {
		auto bitmap = scaleBitmapDown(_image);

		if (_hasRegion) {
			const auto scale = static_cast<double>(bitmap.width()) / _image.width();
			const auto left = static_cast<int>(std::floor(_region.left() * scale));
			const auto top = static_cast<int>(std::floor(_region.top() * scale));
			const auto rightExclusive = std::min(
				static_cast<int>(std::ceil((_region.left() + _region.width()) * scale)),
				bitmap.width());
			const auto bottomExclusive = std::min(
				static_cast<int>(std::ceil((_region.top() + _region.height()) * scale)),
				bitmap.height());
			_region = QRect(left, top, rightExclusive - left, bottomExclusive - top);
		}

		const auto pixels = getPixelsFromImage(bitmap);

		std::vector<Filter*> filterPtrs;
		for (auto &filter : _filters) {
			filterPtrs.push_back(&filter);
		}

		ColorCutQuantizer quantizer(
			pixels,
			_maxColors,
			filterPtrs);

		swatches = quantizer.quantizedColors();
	} else {
		swatches = _swatches;
	}

	auto palette = Palette(std::move(swatches), _targets);
	palette.generate();

	return palette;
}

std::vector<int> Palette::Builder::getPixelsFromImage(const QImage &image) {
	std::vector<int> pixels;

	const auto img = image.convertToFormat(QImage::Format_ARGB32);

	if (_hasRegion) {
		pixels.reserve(_region.width() * _region.height());
		const int yStart = _region.top();
		const int yEndExclusive = _region.top() + _region.height();
		const int xStart = _region.left();
		const int xEndExclusive = _region.left() + _region.width();
		for (int y = yStart; y < yEndExclusive; ++y) {
			const auto line = reinterpret_cast<const QRgb*>(img.scanLine(y));
			for (int x = xStart; x < xEndExclusive; ++x) {
				pixels.push_back(line[x]);
			}
		}
	} else {
		pixels.reserve(img.width() * img.height());
		for (int y = 0; y < img.height(); ++y) {
			const auto line = reinterpret_cast<const QRgb*>(img.scanLine(y));
			for (int x = 0; x < img.width(); ++x) {
				pixels.push_back(line[x]);
			}
		}
	}

	return pixels;
}

QImage Palette::Builder::scaleBitmapDown(const QImage &image) {
	const auto area = image.width() * image.height();

	if (_resizeArea > 0 && area > _resizeArea) {
		const auto scale = std::sqrt(static_cast<double>(_resizeArea) / area);
		const auto newWidth = static_cast<int>(std::ceil(image.width() * scale));
		const auto newHeight = static_cast<int>(std::ceil(image.height() * scale));
		return image.scaled(newWidth, newHeight, Qt::IgnoreAspectRatio, Qt::FastTransformation);
	}

	return image;
}

} // namespace Ayu::Ui
