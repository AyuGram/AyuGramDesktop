// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "data/data_cloud_themes.h"
#include "window/themes/window_themes_embedded.h"
#include "window/window_session_controller.h"
#include "ui/style/style_core_palette.h"

#include <memory>
#include <optional>

namespace AyuFeatures::MessageShot {

enum class SavedThemeApplyResult {
	AppliedSync,
	AwaitingAsync,
	Failed,
};

void setDefaultSelected(Window::Theme::EmbeddedType type);
Window::Theme::EmbeddedType getSelectedFromDefault();

void setDefaultSelectedColor(QColor color);
std::optional<QColor> getSelectedColorFromDefault();

void setCustomSelected(Data::CloudTheme theme);
std::optional<Data::CloudTheme> getSelectedFromCustom();

void resetDefaultSelected();
void resetCustomSelected();

rpl::producer<> resetDefaultSelectedEvents();
rpl::producer<> resetCustomSelectedEvents();

void setTheme(Data::CloudTheme theme);
rpl::producer<Data::CloudTheme> themeChosen();

void setPalette(const style::palette &palette);
rpl::producer<style::palette> paletteChosen();

void ensureChatThemesRefreshed();
QString resolveThemeName();
QString embeddedThemeDisplayName(Window::Theme::EmbeddedType type);
std::shared_ptr<style::palette> getPersistedPalette();
void setPersistedPalette(std::shared_ptr<style::palette> palette);
SavedThemeApplyResult applySavedThemePalette(
	std::shared_ptr<style::palette> palette,
	Fn<void()> onApplied);
void subscribeToCloudThemeLoad(
	not_null<Window::SessionController*> controller,
	std::shared_ptr<style::palette> palette,
	Fn<void()> onApplied);

}
