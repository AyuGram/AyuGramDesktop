// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "confirmation_box.h"
#include "ayu/ayu_settings.h"
#include "window/window_session_controller.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "window/window_peer_menu.h"
#include "styles/style_layers.h"
#include "lang_auto.h"
#include "ui/text/text_utilities.h"

namespace AyuUi {
    ConfirmationBox::ConfirmationBox(
            QWidget *,
            not_null<Window::SessionController *> controller) : _controller(controller) {
        //
    }

    void ConfirmationBox::prepare() {
//        setTitle(rpl::single(QString("Confirmation for SRead")));
        auto details = TextWithEntities();
        details.text = QString("Do you want to read all messages?");

        _text.create(this, rpl::single(std::move(details)), st::boxLabel);

        auto fullHeight = st::boxPadding.top()
                          + _text->height()
                          + st::boxPadding.bottom();

        setDimensions(st::boxWidth, fullHeight);

        addButton(rpl::single(QString("Read")), [=, this] {
            ReadAllPeers();
            closeBox();
        });
        addButton(tr::lng_cancel(), [=, this] { closeBox(); });
    }

    void ConfirmationBox::resizeEvent(QResizeEvent *e) {
        BoxContent::resizeEvent(e);

        const auto &padding = st::boxPadding;
        _text->moveToLeft(padding.left(), padding.top());
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