// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu/ui/context_menu/context_menu.h"
#include "lang_auto.h"
#include "ayu/ayu_state.h"
#include "ayu/messages/ayu_messages_controller.h"
#include "ayu/ui/boxes/message_history_box.h"

#include "history/history_inner_widget.h"
#include "settings/settings_common.h"
#include "styles/style_chat.h"
#include "ui/widgets/popup_menu.h"

#include "ayu/ui/sections/edited/edited_log_section.h"


namespace AyuUi
{

void AddHistoryAction(not_null<Ui::PopupMenu *> menu, HistoryItem *item)
{
	if (AyuMessages::getInstance().hasRevisions(item)) {
		menu->addAction(tr::ayu_EditsHistoryMenuText(tr::now), [=]
		{
			item->history()->session().tryResolveWindow()
				->showSection(std::make_shared<EditedLog::SectionMemento>(item->history()->peer, item));
		}, &st::menuIconInfo);
	}
}

void AddHideMessageAction(not_null<Ui::PopupMenu *> menu, HistoryItem *item)
{
	const auto settings = &AyuSettings::getInstance();
	const auto history = item->history();
	menu->addAction(tr::ayu_ContextHideMessage(tr::now), [=]()
	{
		const auto initSaveDeleted = settings->saveDeletedMessages;

		settings->set_saveDeletedMessages(false);
		history->destroyMessage(item);
		settings->set_saveDeletedMessages(initSaveDeleted);
	}, &st::menuIconClear);
}

void AddReadUntilAction(not_null<Ui::PopupMenu *> menu, HistoryItem *item)
{
	const auto settings = &AyuSettings::getInstance();
	if (settings->sendReadMessages) {
		return;
	}

	const auto history = item->history();
	menu->addAction(tr::ayu_ReadUntilMenuText(tr::now), [=]()
	{
		AyuState::setAllowSendReadPacket(true);
		history->session().data().histories().readInboxOnNewMessage(item);
	}, &st::menuIconShowInChat);
}

} // namespace AyuUi
