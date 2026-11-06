// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include <QImage>
#include <QString>

namespace AyuAssets {

QString appIcoPath();
void loadAppIco();
void loadIcons();
QImage loadPreview(const QString &name);
QString currentAppLogoName();
QImage currentAppLogo();
QImage currentAppLogoPad();

}
