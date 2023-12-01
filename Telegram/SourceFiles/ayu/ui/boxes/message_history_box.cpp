// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "message_history_box.h"
#include "ayu/ayu_settings.h"
#include "ayu/database/ayu_database.h"
#include "ayu/messages/ayu_messages_controller.h"

#include "history/history.h"
#include "settings/settings_common.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "ui/effects/scroll_content_shadow.h"
#include "ui/vertical_list.h"

using namespace Settings;

namespace AyuUi
{

MessageHistoryBox::MessageHistoryBox(QWidget *, HistoryItem *item)
	: _content(this), _scroll(base::make_unique_q<Ui::ScrollArea>(this, st::boxScroll))
{
	setupControls();
	addEditedMessagesToLayout(item);
}

void MessageHistoryBox::setupControls()
{
	_content.create(this);

	_content->resizeToWidth(st::boxWideWidth);
	_content->moveToLeft(0, 0);

	_content->heightValue(
	) | start_to_stream(_contentHeight, _content->lifetime());

	_scroll->setOwnedWidget(
		object_ptr<RpWidget>::fromRaw(_content));
}

void MessageHistoryBox::resizeEvent(QResizeEvent *e)
{
	_scroll->resize(width(), height() - st::boxPhotoPadding.top() - st::boxPadding.bottom());
	_scroll->move(0, st::boxPadding.top());

	if (_content) {
		_content->resize(_scroll->width(), _content->height());
	}
}

void MessageHistoryBox::prepare()
{
	setTitle(tr::ayu_EditsHistoryTitle());

	setDimensions(st::boxWideWidth, 900);
	SetupShadowsToScrollContent(this, _scroll, _contentHeight.events());
}

void MessageHistoryBox::addEditedMessagesToLayout(HistoryItem *item)
{
	auto messages = AyuMessages::getInstance().getEditedMessages(item);
	if (messages.empty()) {
		return;
	}

	for (const auto &message : messages) {
		AddSkip(_content);
		AddDividerText(_content, rpl::single(QString::fromStdString(message.text)));
		AddSkip(_content);
	}
}

}
