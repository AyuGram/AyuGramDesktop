/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/layers/generic_box.h"

namespace Window {
class SessionController;
}

void AboutBox(not_null<Ui::GenericBox*> box, Window::SessionController* controller);

namespace Ui {
class LinkButton;
class FlatLabel;
} // namespace Ui

class AboutBox : public Ui::BoxContent {
public:
	AboutBox(QWidget*, Window::SessionController* controller);

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private:
	void showVersionHistory();

	object_ptr<Ui::LinkButton> _version;
	object_ptr<Ui::FlatLabel> _text;
	Window::SessionController* _controller;

};

QString currentVersionText();
