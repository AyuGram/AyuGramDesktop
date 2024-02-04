// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#pragma once

#include "ui/layers/box_content.h"
#include "ui/style/style_core_palette.h"
#include "window/window_session_controller.h"

using Callback = Fn<void(style::palette &)>;

class ThemeSelectorBox : public Ui::BoxContent
{
public:
	ThemeSelectorBox(QWidget *parent, not_null<Window::SessionController*> controller);

	rpl::producer<style::palette> paletteSelected();
	rpl::producer<QString> themeNameChanged();

protected:
	void prepare() override;

private:
	void setupContent();

	not_null<Window::SessionController*> _controller;

	rpl::event_stream<style::palette> _palettes;
	rpl::event_stream<QString> _themeNames;

	style::palette _selectedPalette;
};
