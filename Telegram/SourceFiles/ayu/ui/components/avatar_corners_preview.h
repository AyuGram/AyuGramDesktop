// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ui/empty_userpic.h"
#include "ui/rp_widget.h"
#include "ui/userpic_view.h"

namespace Ui {
class RippleAnimation;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

class PeerData;

class AvatarCornersPreview final : public Ui::RpWidget {
public:
	AvatarCornersPreview(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	void resolveChannel();
	void subscribeToUpdates();

	const not_null<Window::SessionController*> _controller;
	Ui::EmptyUserpic _emptyUserpic;
	PeerData *_peer = nullptr;
	Ui::PeerUserpicView _userpicView;
	std::unique_ptr<Ui::RippleAnimation> _ripple;
};
