// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

namespace Window {
class Controller;
class SessionController;
} // namespace Window

namespace Settings {

class AyuMain : public Section<AyuMain> {
public:
	AyuMain(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();
};

[[nodiscard]] Type AyuMainId();

} // namespace Settings
