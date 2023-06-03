#include "confirmation_box.h"
#include "ayu/ayu_settings.h"
#include "window/window_session_controller.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "window/window_peer_menu.h"
#include "styles/style_layers.h"
#include "lang_auto.h"

namespace AyuUi {
    ConfirmationBox::ConfirmationBox(
            QWidget*,
            not_null<Window::SessionController *> controller) : _controller(controller) {
        //
    }

    void ConfirmationBox::prepare() {
        setTitle(rpl::single(QString("Confirmation for SRead")));
        setDimensions(st::boxWideWidth, st::boxPadding.top());

        addButton(rpl::single(QString("OK")), [=, this] {
            ReadAllPeers();
            closeBox();
        });
        addButton(tr::lng_cancel(), [=, this] { closeBox(); });
    }

    void ConfirmationBox::ReadAllPeers() {
        auto settings = &AyuSettings::getInstance();
        auto prev = settings->sendReadPackets;
        settings->set_sendReadPackets(true);

        auto chats = _controller->session().data().chatsList();
        Window::MarkAsReadChatListHack(chats);

        settings->set_sendReadPackets(prev);
    }
}