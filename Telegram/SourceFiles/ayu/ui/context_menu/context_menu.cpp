// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "context_menu.h"
#include "lang_auto.h"
#include "message_history_box.h"
#include "ayu/ayu_state.h"
#include "ayu/database/ayu_database.h"
#include "ayu/messages/ayu_messages_controller.h"

#include "history/history_inner_widget.h"
#include "settings/settings_common.h"
#include "styles/style_chat.h"
#include "ui/widgets/popup_menu.h"

namespace AyuUi
{
	AyuPopupMenu::AyuPopupMenu(HistoryInner* parent)
	{
		_ayuSubMenu = std::make_unique<Ui::PopupMenu>(parent, st::popupMenuWithIcons);
	}

	void AyuPopupMenu::addHistoryAction(HistoryItem* item)
	{
		if (AyuMessages::getInstance().hasRevisions(item))
		{
			_ayuSubMenu->addAction(tr::ayu_EditsHistoryMenuText(tr::now), [=]
			{
				auto box = Box<MessageHistoryBox>(item);
				show(std::move(box));
			}, &st::menuIconInfo);
		}
	}

	void AyuPopupMenu::addHideMessageAction(HistoryItem* item) const
	{
		const auto settings = &AyuSettings::getInstance();
		const auto history = item->history();
		_ayuSubMenu->addAction(tr::ayu_ContextHideMessage(tr::now), [=]()
		{
			const auto initSaveDeleted = settings->saveDeletedMessages;

			settings->set_keepDeletedMessages(false);
			history->destroyMessage(item);
			settings->set_keepDeletedMessages(initSaveDeleted);
		}, &st::menuIconClear);
	}

	void AyuPopupMenu::addReadUntilAction(HistoryItem* item) const
	{
		const auto history = item->history();
		_ayuSubMenu->addAction(tr::ayu_ReadUntilMenuText(tr::now), [=]()
		{
			AyuState::setAllowSendReadPacket(true);
			history->session().data().histories().readInboxOnNewMessage(item);
		}, &st::menuIconShowInChat);
	}
} // namespace AyuUi
