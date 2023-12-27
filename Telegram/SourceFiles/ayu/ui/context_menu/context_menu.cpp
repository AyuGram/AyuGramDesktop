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

#include "mainwidget.h"
#include "styles/style_ayu_icons.h"
#include "ui/widgets/popup_menu.h"

#include "ayu/ui/sections/edited/edited_log_section.h"
#include "ayu/utils/telegram_helpers.h"


namespace AyuUi
{

void AddHistoryAction(not_null<Ui::PopupMenu *> menu, HistoryItem *item)
{
	if (AyuMessages::getInstance().hasRevisions(item)) {
		menu->addAction(tr::ayu_EditsHistoryMenuText(tr::now), [=]
		{
			item->history()->session().tryResolveWindow()
				->showSection(std::make_shared<EditedLog::SectionMemento>(item->history()->peer, item));
		}, &st::ayuEditsHistoryIcon);
	}
}

void AddHideMessageAction(not_null<Ui::PopupMenu *> menu, HistoryItem *item)
{
	// todo: rework
//	const auto settings = &AyuSettings::getInstance();
//	const auto history = item->history();
//	menu->addAction(tr::ayu_ContextHideMessage(tr::now), [=]()
//	{
//		const auto initSaveDeleted = settings->saveDeletedMessages;
//
//		settings->set_saveDeletedMessages(false);
//		history->destroyMessage(item);
//		settings->set_saveDeletedMessages(initSaveDeleted);
//	}, &st::menuIconClear);
}

void AddUserMessagesAction(not_null<Ui::PopupMenu *> menu, HistoryItem *item)
{
	if (item->history()->peer->isChat() || item->history()->peer->isMegagroup()) {
		menu->addAction(tr::ayu_UserMessagesMenuText(tr::now), [=]
		{
			if (const auto window = item->history()->session().tryResolveWindow()) {
				if (const auto mainWidget = window->widget()->sessionController()) {
					const auto peer = item->history()->peer;
					const auto key = (peer && !peer->isUser())
									 ? item->topic()
									   ? Dialogs::Key{item->topic()}
									   : Dialogs::Key{item->history()}
									 : Dialogs::Key();
					mainWidget->content()->searchMessages("", key, item->from()->asUser());
				}
			}
		}, &st::menuIconInfo);
	}
}

void AddReadUntilAction(not_null<Ui::PopupMenu *> menu, HistoryItem *item)
{
	const auto settings = &AyuSettings::getInstance();
	if (settings->sendReadMessages) {
		return;
	}

	menu->addAction(tr::ayu_ReadUntilMenuText(tr::now), [=]()
	{
		readHistory(item);
	}, &st::menuIconShowInChat);
}

} // namespace AyuUi
