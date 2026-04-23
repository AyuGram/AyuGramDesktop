// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <map>
#include <QColor>
#include <QPixmap>
#include <vector>

namespace Ayu::Ui {

class Swatch
{
public:
	Swatch(QRgb color, int population);

	[[nodiscard]] QRgb rgb() const;
	[[nodiscard]] int red() const;
	[[nodiscard]] int green() const;
	[[nodiscard]] int blue() const;
	[[nodiscard]] int population() const;
	[[nodiscard]] std::array<float, 3> hsl() const;

	[[nodiscard]] QColor titleTextColor() const;
	[[nodiscard]] QColor bodyTextColor() const;

private:
	void ensureTextColorsGenerated() const;

	int _red;
	int _green;
	int _blue;
	QRgb _rgb;
	int _population;

	mutable bool _generatedTextColors = false;
	mutable QColor _titleTextColor;
	mutable QColor _bodyTextColor;
	mutable std::array<float, 3> _hsl;
	mutable bool _hslCalculated = false;
};

class Target
{
public:
	static const Target LIGHT_VIBRANT;
	static const Target VIBRANT;
	static const Target DARK_VIBRANT;
	static const Target LIGHT_MUTED;
	static const Target MUTED;
	static const Target DARK_MUTED;

	Target();
	Target(const Target &from);

	[[nodiscard]] float minimumSaturation() const;
	[[nodiscard]] float targetSaturation() const;
	[[nodiscard]] float maximumSaturation() const;
	[[nodiscard]] float minimumLightness() const;
	[[nodiscard]] float targetLightness() const;
	[[nodiscard]] float maximumLightness() const;
	[[nodiscard]] float saturationWeight() const;
	[[nodiscard]] float lightnessWeight() const;
	[[nodiscard]] float populationWeight() const;
	[[nodiscard]] bool isExclusive() const;

	void normalizeWeights();

	bool operator==(const Target &other) const;

private:
	static void setDefaultLightLightnessValues(Target &target);
	static void setDefaultNormalLightnessValues(Target &target);
	static void setDefaultDarkLightnessValues(Target &target);
	static void setDefaultVibrantSaturationValues(Target &target);
	static void setDefaultMutedSaturationValues(Target &target);
	static void setTargetDefaultValues(std::array<float, 3> &values);
	static void setDefaultWeights(Target &target);

	std::array<float, 3> _saturationTargets;
	std::array<float, 3> _lightnessTargets;
	std::array<float, 3> _weights;
	bool _isExclusive = true;
};

class Palette
{
public:
	using Filter = std::function<bool(QRgb, const std::array<float, 3> &)>;

	class Builder;

	[[nodiscard]] const std::vector<Swatch> &swatches() const;
	[[nodiscard]] const std::vector<Target> &targets() const;

	[[nodiscard]] const Swatch *vibrantSwatch() const;
	[[nodiscard]] const Swatch *lightVibrantSwatch() const;
	[[nodiscard]] const Swatch *darkVibrantSwatch() const;
	[[nodiscard]] const Swatch *mutedSwatch() const;
	[[nodiscard]] const Swatch *lightMutedSwatch() const;
	[[nodiscard]] const Swatch *darkMutedSwatch() const;
	[[nodiscard]] const Swatch *dominantSwatch() const;

	[[nodiscard]] QRgb vibrantColor(QRgb defaultColor) const;
	[[nodiscard]] QRgb lightVibrantColor(QRgb defaultColor) const;
	[[nodiscard]] QRgb darkVibrantColor(QRgb defaultColor) const;
	[[nodiscard]] QRgb mutedColor(QRgb defaultColor) const;
	[[nodiscard]] QRgb lightMutedColor(QRgb defaultColor) const;
	[[nodiscard]] QRgb darkMutedColor(QRgb defaultColor) const;
	[[nodiscard]] QRgb dominantColor(QRgb defaultColor) const;

	[[nodiscard]] const Swatch *swatchForTarget(const Target &target) const;
	[[nodiscard]] QRgb colorForTarget(const Target &target, QRgb defaultColor) const;

	static Builder from(const QPixmap &pixmap);
	static Builder from(const QImage &image);
	static Palette fromSwatches(const std::vector<Swatch> &swatches);

	static constexpr int DEFAULT_RESIZE_BITMAP_AREA = 112 * 112;
	static constexpr int DEFAULT_CALCULATE_NUMBER_COLORS = 16;
	static constexpr float MIN_CONTRAST_TITLE_TEXT = 3.0f;
	static constexpr float MIN_CONTRAST_BODY_TEXT = 4.5f;

private:
	Palette(
		std::vector<Swatch> swatches,
		std::vector<Target> targets);

	void generate();
	const Swatch *generateScoredTarget(const Target &target);
	const Swatch *getMaxScoredSwatchForTarget(const Target &target);
	bool shouldBeScoredForTarget(const Swatch &swatch, const Target &target);
	float generateScore(const Swatch &swatch, const Target &target);
	const Swatch *findDominantSwatch();

	std::vector<Swatch> _swatches;
	std::vector<Target> _targets;
	std::vector<std::pair<Target, const Swatch*>> _selectedSwatches;
	std::set<QRgb> _usedColors;
	const Swatch *_dominantSwatch = nullptr;

	friend class Builder;
};

class Palette::Builder
{
public:
	explicit Builder(const QPixmap &pixmap);
	explicit Builder(const QImage &image);
	explicit Builder(const std::vector<Swatch> &swatches);

	Builder &maximumColorCount(int colors);
	Builder &resizeBitmapArea(int area);
	Builder &clearFilters();
	Builder &addFilter(Filter filter);
	Builder &setRegion(int left, int top, int right, int bottom);
	Builder &clearRegion();
	Builder &addTarget(const Target &target);
	Builder &clearTargets();

	[[nodiscard]] Palette generate();

private:
	std::vector<int> getPixelsFromImage(const QImage &image);
	QImage scaleBitmapDown(const QImage &image);

	std::vector<Swatch> _swatches;
	QImage _image;
	std::vector<Target> _targets;
	int _maxColors = DEFAULT_CALCULATE_NUMBER_COLORS;
	int _resizeArea = DEFAULT_RESIZE_BITMAP_AREA;
	std::vector<Filter> _filters;
	QRect _region;
	bool _hasRegion = false;
	bool _hasImage = false;

	static Filter DEFAULT_FILTER;
};

} // namespace Ayu::Ui
