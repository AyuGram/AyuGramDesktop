#pragma once

namespace AyuFeatures::StreamerMode::Impl {

void enableHook();
void disableHook();
void hideWidgetWindow(QWidget *widget);
void showWidgetWindow(QWidget *widget);

}