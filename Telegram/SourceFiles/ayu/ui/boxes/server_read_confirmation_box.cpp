// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "server_read_confirmation_box.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "styles/style_layers.h"
#include "window/window_session_controller.h"

namespace AyuUi {

ServerReadConfirmationBox::ServerReadConfirmationBox(
	QWidget *,
	not_null<Window::SessionController *> controller)
	: _controller(controller) {
	//
}

void ServerReadConfirmationBox::prepare() {
	_text.create(this, tr::ayu_ReadConfirmationBoxQuestion(), st::boxLabel);

	auto fullHeight = st::boxPadding.top()
		+ _text->height()
		+ st::boxPadding.bottom();

	setDimensions(st::boxWidth, fullHeight);

	addButton(tr::ayu_ReadConfirmationBoxActionText(),
			  [=, this]
			  {
				  ReadAllPeers();
				  closeBox();
			  });
	addButton(tr::lng_cancel(),
			  [=, this]
			  {
				  closeBox();
			  });
}

void ServerReadConfirmationBox::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	const auto &padding = st::boxPadding;
	_text->moveToLeft(padding.left(), padding.top());
}

void ServerReadConfirmationBox::ReadAllPeers() {
	auto settings = &AyuSettings::getInstance();
	auto prev = settings->sendReadMessages;
	settings->set_sendReadMessages(true);

	auto chats = _controller->session().data().chatsList();
	MarkAsReadChatList(chats);

	settings->set_sendReadMessages(prev);
}

}
