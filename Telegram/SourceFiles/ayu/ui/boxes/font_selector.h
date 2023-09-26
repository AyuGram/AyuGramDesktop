//
// Created by MaxPlays on 12/09/2023.
//

#ifndef TELEGRAM_FONT_SELECTOR_H
#define TELEGRAM_FONT_SELECTOR_H

/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "lang/lang_cloud_manager.h"
#include "boxes/abstract_box.h"
#include "base/binary_guard.h"

struct LanguageId;
struct Font;

namespace Ui
{

class MultiSelect;
struct ScrollToRequest;
class VerticalLayout;

} // namespace Ui

namespace Window
{

class SessionController;

} // namespace Window
namespace AyuUi
{

class FontSelectorBox: public Ui::BoxContent
{
public:
	FontSelectorBox(QWidget *, Window::SessionController *controller, Fn<void(QString font)> hook);

	void setInnerFocus() override;

	static base::binary_guard Show(Window::SessionController *controller, const Fn<void(QString font)> hook);
private:
	QString _selectedFont;

protected:
	void prepare() override;

	void keyPressEvent(QKeyEvent *e) override;

private:
	void setupTop(not_null<Ui::VerticalLayout *> container);

	[[nodiscard]] int rowsInPage() const;

	Window::SessionController *_controller = nullptr;
	rpl::event_stream<bool> _translateChatTurnOff;
	Fn<void()> _setInnerFocus;
	Fn<Ui::ScrollToRequest(int rows)> _jump;
	Fn<void(QString font)> _hook;

};

}

#endif //TELEGRAM_FONT_SELECTOR_H
