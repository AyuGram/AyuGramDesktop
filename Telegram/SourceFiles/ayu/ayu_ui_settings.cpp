// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu_ui_settings.h"

namespace AyuUiSettings {

namespace {

QString MonoFont;
double WideMultiplier = 1.0;
bool MaterialSwitches = false;

} // namespace

void setMonoFont(const QString &font) {
	MonoFont = font;
}

QString getMonoFont() {
	return MonoFont;
}

void setWideMultiplier(double multiplier) {
	WideMultiplier = multiplier;
}

double getWideMultiplier() {
	return WideMultiplier;
}

int getWideMultiplied(int value, double multiplier) {
	return static_cast<int>(value * multiplier);
}

void setMaterialSwitches(bool enabled) {
	MaterialSwitches = enabled;
}

bool getMaterialSwitches() {
	return MaterialSwitches;
}

} // namespace AyuUiSettings
