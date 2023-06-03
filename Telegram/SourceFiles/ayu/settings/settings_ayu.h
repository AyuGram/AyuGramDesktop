/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_common.h"

class BoxContent;

namespace Window {
class Controller;
class SessionController;
} // namespace Window

namespace Settings {
	class Ayu : public Section<Ayu> {
	public:
        Ayu(
                QWidget *parent,
                not_null<Window::SessionController *> controller);
        [[nodiscard]] rpl::producer<QString> title() override;

	private:
        void SetupAyuGramSettings(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> null);
		void setupContent(not_null<Window::SessionController *> controller);
	};

} // namespace Settings
