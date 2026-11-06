// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

class AyuGhost : public Section<AyuGhost> {
public:
	AyuGhost(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

	not_null<Window::SessionController*> _controller;
};

[[nodiscard]] Type AyuGhostId();

} // namespace Settings
