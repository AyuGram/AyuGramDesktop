// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu/ui/context_menu/context_menu.h"

#include <styles/style_menu_icons.h>

#include "lang_auto.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "ayu/ayu_settings.h"
#include "ayu/ayu_state.h"
#include "../../data/messages_storage.h"
#include "ayu/ui/context_menu/menu_item_subtext.h"
#include "ayu/utils/qt_key_modifiers_extended.h"
#include "history/history_item_components.h"

#include "core/mime_type.h"
#include "styles/style_ayu_icons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "window/window_peer_menu.h"

#include "ayu/ui/sections/edited/edited_log_section.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/unixtime.h"
#include "history/view/history_view_element.h"
#include "window/window_session_controller.h"

namespace AyuUi {

bool needToShowItem(int state) {
	return state == 1 || state == 2 && base::IsExtendedContextMenuModifierPressed();
}

void AddHistoryAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	if (AyuMessages::hasRevisions(item)) {
		menu->addAction(tr::ayu_EditsHistoryMenuText(tr::now),
						[=]
						{
							item->history()->session().tryResolveWindow()
								->showSection(std::make_shared<EditedLog::SectionMemento>(item->history()->peer, item));
						},
						&st::ayuEditsHistoryIcon);
	}
}

void AddHideMessageAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	const auto settings = &AyuSettings::getInstance();
	if (!needToShowItem(settings->showHideMessageInContextMenu)) {
		return;
	}

	const auto history = item->history();
	menu->addAction(tr::ayu_ContextHideMessage(tr::now),
					[=]()
					{
						item->destroy();
						history->requestChatListMessage();

						AyuState::hide(item);
					},
					&st::menuIconClear);
}

void AddUserMessagesAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	const auto settings = &AyuSettings::getInstance();
	if (!needToShowItem(settings->showUserMessagesInContextMenu)) {
		return;
	}

	if (item->history()->peer->isChat() || item->history()->peer->isMegagroup()) {
		menu->addAction(tr::ayu_UserMessagesMenuText(tr::now),
						[=]
						{
							if (const auto window = item->history()->session().tryResolveWindow()) {
								if (const auto mainWidget = window->widget()->sessionController()) {
									const auto peer = item->history()->peer;
									const auto key = (peer && !peer->isUser())
														 ? item->topic()
															   ? Dialogs::Key{item->topic()}
															   : Dialogs::Key{item->history()}
														 : Dialogs::Key();
									mainWidget->content()->searchMessages(QString(), key, item->from()->asUser());
								}
							}
						},
						&st::menuIconTTL);
	}
}

void AddMessageDetailsAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	const auto settings = &AyuSettings::getInstance();
	if (!needToShowItem(settings->showMessageDetailsInContextMenu)) {
		return;
	}

	if (item->isLocal()) {
		return;
	}

	const auto view = item->mainView();
	const auto forwarded = item->Get<HistoryMessageForwarded>();
	const auto views = item->Get<HistoryMessageViews>();
	const auto media = item->media();

	const auto isSticker = media && media->document() && media->document()->sticker();
	const auto isForwarded = forwarded && !forwarded->story && forwarded->psaType.isEmpty();

	const auto messageId = QString::number(item->id.bare);
	const auto messageDate = base::unixtime::parse(item->date());
	const auto messageEditDate = base::unixtime::parse(view ? view->displayedEditDate() : TimeId(0));

#pragma clang diagnostic push
#pragma ide diagnostic ignored "NullDereference"
	const auto messageForwardedDate =
		isForwarded
			? base::unixtime::parse(forwarded->originalDate)
			: QDateTime();
#pragma clang diagnostic pop

	const auto
		messageViews = item->hasViews() && item->viewsCount() > 0 ? QString::number(item->viewsCount()) : QString();
	const auto messageForwards = views && views->forwardsCount > 0 ? QString::number(views->forwardsCount) : QString();

	const auto mediaSize = media ? getMediaSize(item) : QString();
	const auto mediaMime = media ? getMediaMime(item) : QString();
	// todo: bitrate (?)
	const auto mediaName = media ? getMediaName(item) : QString();
	const auto mediaResolution = media ? getMediaResolution(item) : QString();
	const auto mediaDC = media ? getMediaDC(item) : QString();

	const auto hasAnyPostField =
		!messageViews.isEmpty() ||
		!messageForwards.isEmpty();

	const auto hasAnyMediaField =
		!mediaSize.isEmpty() ||
		!mediaMime.isEmpty() ||
		!mediaName.isEmpty() ||
		!mediaResolution.isEmpty() ||
		!mediaDC.isEmpty();

	const auto callback = Ui::Menu::CreateAddActionCallback(menu);

	callback(Window::PeerMenuCallback::Args{
		.text = tr::ayu_MessageDetailsPC(tr::now),
		.handler = nullptr,
		.icon = &st::menuIconInfo,
		.fillSubmenu = [&](not_null<Ui::PopupMenu*> menu2)
		{
			if (hasAnyPostField) {
				if (!messageViews.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconShowInChat,
						tr::ayu_MessageDetailsViewsPC(tr::now),
						messageViews
					));
				}

				if (!messageForwards.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconViewReplies,
						tr::ayu_MessageDetailsSharesPC(tr::now),
						messageForwards
					));
				}

				menu2->addSeparator();
			}

			menu2->addAction(Ui::ContextActionWithSubText(
				menu2->menu(),
				st::menuIconInfo,
				QString("ID"),
				messageId
			));

			menu2->addAction(Ui::ContextActionWithSubText(
				menu2->menu(),
				st::menuIconSchedule,
				tr::ayu_MessageDetailsDatePC(tr::now),
				formatDateTime(messageDate)
			));

			if (view && view->displayedEditDate()) {
				menu2->addAction(Ui::ContextActionWithSubText(
					menu2->menu(),
					st::menuIconEdit,
					tr::ayu_MessageDetailsEditedDatePC(tr::now),
					formatDateTime(messageEditDate)
				));
			}

			if (isForwarded) {
				menu2->addAction(Ui::ContextActionWithSubText(
					menu2->menu(),
					st::menuIconTTL,
					tr::ayu_MessageDetailsForwardedDatePC(tr::now),
					formatDateTime(messageForwardedDate)
				));
			}

			if (media && hasAnyMediaField) {
				menu2->addSeparator();

				if (!mediaSize.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconDownload,
						tr::ayu_MessageDetailsFileSizePC(tr::now),
						mediaSize
					));
				}

				if (!mediaMime.isEmpty()) {
					const auto mime = Core::MimeTypeForName(mediaMime);

					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconShowAll,
						tr::ayu_MessageDetailsMimeTypePC(tr::now),
						mime.name()
					));
				}

				if (!mediaName.isEmpty()) {
					auto const shortified = mediaName.length() > 20 ? "…" + mediaName.right(20) : mediaName;

					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::ayuEditsHistoryIcon,
						tr::ayu_MessageDetailsFileNamePC(tr::now),
						shortified,
						[=]
						{
							QGuiApplication::clipboard()->setText(mediaName);
						}
					));
				}

				if (!mediaResolution.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconStats,
						tr::ayu_MessageDetailsResolutionPC(tr::now),
						mediaResolution
					));
				}

				if (!mediaDC.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconBoosts,
						tr::ayu_MessageDetailsDatacenterPC(tr::now),
						mediaDC
					));
				}

				if (isSticker) {
					const auto authorId = media->document()->sticker()->set.id >> 32;

					if (authorId != 0) {
						menu2->addAction(Ui::ContextActionStickerAuthor(
							menu2->menu(),
							&item->history()->session(),
							authorId
						));
					}
				}
			}
		},
	});
}

void AddReadUntilAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	if (item->isLocal()) {
		return;
	}

	const auto settings = &AyuSettings::getInstance();
	if (settings->sendReadMessages) {
		return;
	}

	menu->addAction(tr::ayu_ReadUntilMenuText(tr::now),
					[=]()
					{
						readHistory(item);
					},
					&st::menuIconShowInChat);
}

} // namespace AyuUi
