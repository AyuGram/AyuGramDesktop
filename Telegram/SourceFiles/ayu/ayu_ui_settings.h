// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <QString>

namespace AyuUiSettings {

void setMonoFont(const QString &font);
QString getMonoFont();

void setWideMultiplier(double multiplier);
double getWideMultiplier();
int getWideMultiplied(int value, double multiplier);

void setMaterialSwitches(bool enabled);
bool getMaterialSwitches();

} // namespace AyuUiSettings
