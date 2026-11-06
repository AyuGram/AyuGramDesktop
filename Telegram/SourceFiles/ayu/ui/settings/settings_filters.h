// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

class AyuFilters : public Section<AyuFilters> {
public:
	AyuFilters(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;
	void fillTopBarMenu(const Ui::Menu::MenuCallback &addAction) override;

private:
	void setupContent();
};

[[nodiscard]] Type AyuFiltersId();

} // namespace Settings
