// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

class AyuOther : public Section<AyuOther> {
public:
	AyuOther(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();
};

[[nodiscard]] Type AyuOtherId();

} // namespace Settings
