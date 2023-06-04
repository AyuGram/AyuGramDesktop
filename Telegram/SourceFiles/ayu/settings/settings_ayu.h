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
        Ayu(QWidget *parent, not_null<Window::SessionController *> controller);

        [[nodiscard]] rpl::producer<QString> title() override;

    private:
        void SetupAyuGramSettings(not_null<Ui::VerticalLayout *> container);
        void setupContent(not_null<Window::SessionController *> controller);
    };

} // namespace Settings
