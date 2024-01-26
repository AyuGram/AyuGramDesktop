// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "ui/layers/box_content.h"
#include "window/window_main_menu.h"

namespace AyuUi
{

class ServerReadConfirmationBox : public Ui::BoxContent
{
public:
	ServerReadConfirmationBox(QWidget *, not_null<Window::SessionController *> controller);

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;

private:
	void ReadAllPeers();

	not_null<Window::SessionController *> _controller;
	object_ptr<Ui::FlatLabel> _text = {nullptr};
};

}
