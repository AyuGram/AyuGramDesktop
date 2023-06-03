#include "ui/layers/box_content.h"
#include "window/window_main_menu.h"

namespace AyuUi {
    class ConfirmationBox : public Ui::BoxContent {
    public:
        ConfirmationBox(QWidget*, not_null<Window::SessionController *> controller);
    protected:
        void prepare() override;
    private:
        void ReadAllPeers();
        not_null<Window::SessionController*> _controller;
    };
}