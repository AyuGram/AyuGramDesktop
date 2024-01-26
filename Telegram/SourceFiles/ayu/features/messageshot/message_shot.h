// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "window/window_session_controller.h"
#include "history/view/history_view_list_widget.h"
#include "ui/chat/chat_style.h"
#include "window/themes/window_themes_embedded.h"

namespace AyuFeatures::MessageShot {

struct ShotConfig {
	not_null<Window::SessionController*> controller;
	std::shared_ptr<Ui::ChatStyle> st;
	std::vector<not_null<HistoryItem*>> messages;

	bool showDate;
	bool showReactions;
};

enum RenderPart
{
	Date,
	Reactions,
};

void setShotConfig(ShotConfig& config);
void resetShotConfig();
ShotConfig getShotConfig();

// for default themes
void setDefaultSelected(Window::Theme::EmbeddedType type);
Window::Theme::EmbeddedType getSelectedFromDefault();

void setDefaultSelectedColor(QColor color);
std::optional<QColor> getSelectedColorFromDefault();

// for custom themes
void setCustomSelected(Data::CloudTheme theme);
std::optional<Data::CloudTheme> getSelectedFromCustom();

// resets
void resetDefaultSelected();
void resetCustomSelected();

rpl::producer<> resetDefaultSelectedEvents();
rpl::producer<> resetCustomSelectedEvents();

bool ignoreRender(RenderPart part);
bool isTakingShot();

bool isChoosingTheme();
bool setChoosingTheme(bool val);

void setTheme(Data::CloudTheme theme);
rpl::producer<Data::CloudTheme> themeChosen();

void setPalette(style::palette& palette);
rpl::producer<style::palette> paletteChosen();

QImage Make(not_null<QWidget*> box, const ShotConfig &config);

void Wrapper(not_null<HistoryView::ListWidget*> widget);

}
