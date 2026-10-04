// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ui/rp_widget.h"

namespace Window {
class SessionController;
} // namespace Window

class MessagePreview final : public Ui::RpWidget {
public:
	MessagePreview(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	void setBubbleRadius(int radius);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void updateWidgetSize(int width, bool animate = false);
	void refresh();

	const not_null<Window::SessionController*> _controller;

	class PreviewDelegate;
	struct State;
	const not_null<State*> _state;

};
