// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/ui/context_menu/context_menu.h"

#include "apiwrap.h"
#include "lang_auto.h"
#include "mainwidget.h"
#include "ayu/ayu_settings.h"
#include "ayu/ayu_state.h"
#include "ayu/data/messages_storage.h"
#include "ayu/features/filters/shadow_ban_utils.h"
#include "ayu/ui/context_menu/menu_item_subtext.h"
#include "ayu/utils/qt_key_modifiers_extended.h"
#include "history/history_item_components.h"

#include "core/mime_type.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_menu_icons.h"
#include "styles/style_layers.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "window/window_peer_menu.h"

#include "ayu/ui/message_history/history_section.h"
#include "ayu/ui/settings/filters/edit_filter.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/call_delayed.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "data/data_channel.h"
#include "data/data_user.h"
#include "data/data_chat.h"
#include "data/data_forum_topic.h"
#include "data/data_search_controller.h"
#include "data/data_session.h"
#include "history/view/history_view_context_menu.h"
#include "history/view/history_view_element.h"
#include "ui/boxes/confirm_box.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

namespace AyuUi {

namespace {

void DeleteMyMessagesAfterConfirm(not_null<PeerData*> peer) {
	const auto session = &peer->session();

	auto collected = std::make_shared<std::vector<MsgId>>();

	const auto removeNext = std::make_shared<Fn<void(int)>>();
	const auto requestNext = std::make_shared<Fn<void(MsgId)>>();

	*removeNext = [=](int index)
	{
		if (index >= int(collected->size())) {
			DEBUG_LOG(("Deleted all %1 my messages in this chat").arg(collected->size()));
			return;
		}

		QVector<MTPint> ids;
		ids.reserve(std::min<int>(100, collected->size() - index));
		for (auto i = 0; i < 100 && (index + i) < int(collected->size()); ++i) {
			ids.push_back(MTP_int((*collected)[index + i].bare));
		}

		const auto batch = index / 100 + 1;
		const auto done = [=](const MTPmessages_AffectedMessages &result)
		{
			session->api().applyAffectedMessages(peer, result);
			if (peer->isChannel()) {
				session->data().processMessagesDeleted(peer->id, ids);
			} else {
				session->data().processNonChannelMessagesDeleted(ids);
			}
			const auto deleted = index + ids.size();
			DEBUG_LOG(("Deleted batch %1, total deleted %2/%3").arg(batch).arg(deleted).arg(collected->size()));
			const auto delay = crl::time(500 + base::RandomValue<int>() % 500);
			base::call_delayed(delay, [=] { (*removeNext)(deleted); });
		};
		const auto fail = [=](const MTP::Error &error)
		{
			DEBUG_LOG(("Delete batch failed: %1").arg(error.type()));
			const auto delay = crl::time(1000);
			base::call_delayed(delay, [=] { (*removeNext)(index); });
		};

		if (const auto channel = peer->asChannel()) {
			session->api()
				.request(MTPchannels_DeleteMessages(channel->inputChannel, MTP_vector<MTPint>(ids)))
				.done(done)
				.fail(fail)
				.handleFloodErrors()
				.send();
		} else {
			using Flag = MTPmessages_DeleteMessages::Flag;
			session->api()
				.request(MTPmessages_DeleteMessages(MTP_flags(Flag::f_revoke), MTP_vector<MTPint>(ids)))
				.done(done)
				.fail(fail)
				.handleFloodErrors()
				.send();
		}
	};

	*requestNext = [=](MsgId from)
	{
		using Flag = MTPmessages_Search::Flag;
		auto request = MTPmessages_Search(
			MTP_flags(Flag::f_from_id),
			peer->input,
			MTP_string(),
			MTP_inputPeerSelf(),
			MTPInputPeer(),
			MTPVector<MTPReaction>(),
			MTP_int(0),
			// top_msg_id
			MTP_inputMessagesFilterEmpty(),
			MTP_int(0),
			// min_date
			MTP_int(0),
			// max_date
			MTP_int(from.bare),
			MTP_int(0),
			// add_offset
			MTP_int(100),
			MTP_int(0),
			// max_id
			MTP_int(0),
			// min_id
			MTP_long(0)); // hash

		session->api()
			.request(std::move(request))
			.done([=](const Api::HistoryRequestResult &result)
			{
				auto parsed = Api::ParseHistoryResult(peer, from, Data::LoadDirection::Before, result);
				MsgId minId;
				int batchCount = 0;
				for (const auto &id : parsed.messageIds) {
					if (!minId || id < minId) minId = id;
					collected->push_back(id);
					++batchCount;
				}
				DEBUG_LOG(("Batch found %1 my messages, total %2").arg(batchCount).arg(collected->size()));
				if (parsed.messageIds.size() == 100 && minId) {
					(*requestNext)(minId - MsgId(1));
				} else {
					DEBUG_LOG(("Found %1 my messages in this chat (SEARCH)").arg(collected->size()));
					(*removeNext)(0);
				}
			})
			.fail([=](const MTP::Error &error) { DEBUG_LOG(("History fetch failed: %1").arg(error.type())); })
			.send();
	};

	(*requestNext)(MsgId(0));
}

Fn<void()> DeleteMyMessagesHandler(not_null<Window::SessionController*> controller, not_null<PeerData*> peer) {
	return [=]
	{
		if (!controller->showFrozenError()) {
			controller->show(Ui::MakeConfirmBox({
				.text = tr::ayu_DeleteOwnMessagesConfirmation(tr::now),
				.confirmed =
				[=](Fn<void()> &&close)
				{
					DeleteMyMessagesAfterConfirm(peer);
					close();
				},
				.confirmText = tr::lng_box_delete(),
				.cancelText = tr::lng_cancel(),
				.confirmStyle = &st::attentionBoxButton,
			}));
		}
	};
}

}

bool needToShowItem(int state) {
	return state == 1 || (state == 2 && base::IsExtendedContextMenuModifierPressed());
}

void AddDeletedMessagesActions(PeerData *peerData,
							   Data::Thread *thread,
							   not_null<Window::SessionController*> sessionController,
							   const Window::PeerMenuCallback &addCallback) {
	if (!peerData) {
		return;
	}

	const auto topic = peerData->isForum() ? thread->asTopic() : nullptr;
	const auto topicId = topic ? topic->rootId().bare : 0;

	// const auto has = AyuMessages::hasDeletedMessages(peerData, topicId);
	// if (!has) {
	// 	return;
	// }

	addCallback(
		tr::ayu_ViewDeletedMenuText(tr::now),
		[=]
		{
			sessionController->session().tryResolveWindow()
				->showSection(std::make_shared<MessageHistory::SectionMemento>(peerData, nullptr, topicId));
		},
		&st::menuIconArchive);
	// todo view filters
}

void AddJumpToBeginningAction(PeerData *peerData,
							  Data::Thread *thread,
							  not_null<Window::SessionController*> sessionController,
							  const Window::PeerMenuCallback &addCallback) {
	const auto user = peerData->asUser();
	const auto group = peerData->isChat() ? peerData->asChat() : nullptr;
	const auto chat = peerData->isMegagroup()
						  ? peerData->asMegagroup()
						  : peerData->isChannel()
								? peerData->asChannel()
								: nullptr;
	const auto topic = peerData->isForum() ? thread->asTopic() : nullptr;
	if (!user && !group && !chat && !topic) {
		return;
	}
	if (topic && topic->creating()) {
		return;
	}

	const auto controller = sessionController;
	const auto jumpToDate = [=](auto history, auto callback)
	{
		const auto weak = base::make_weak(controller);
		controller->session().api().resolveJumpToDate(
			history,
			QDate(2013, 8, 1),
			[=](not_null<PeerData*> peer, MsgId id)
			{
				if (const auto strong = weak.get()) {
					callback(peer, id);
				}
			});
	};

	const auto showPeerHistory = [=](auto peer, MsgId id)
	{
		controller->showPeerHistory(
			peer,
			Window::SectionShow::Way::Forward,
			id);
	};

	const auto showTopic = [=](auto topic, MsgId id)
	{
		controller->showTopic(
			topic,
			id,
			Window::SectionShow::Way::Forward);
	};

	addCallback(
		tr::ayu_JumpToBeginning(tr::now),
		[=]
		{
			if (user) {
				jumpToDate(controller->session().data().history(user), showPeerHistory);
			} else if (group && !chat) {
				jumpToDate(controller->session().data().history(group), showPeerHistory);
			} else if (chat && !topic) {
				if (!chat->migrateFrom() && chat->availableMinId() == 1) {
					showPeerHistory(chat, 1);
				} else {
					jumpToDate(controller->session().data().history(chat), showPeerHistory);
				}
			} else if (topic) {
				if (topic->isGeneral()) {
					showTopic(topic, 1);
				} else {
					jumpToDate(
						topic,
						[=](not_null<PeerData*>, MsgId id)
						{
							showTopic(topic, id);
						});
				}
			}
		},
		&st::ayuMenuIconToBeginning);
}

void AddOpenChannelAction(PeerData *peerData,
						  not_null<Window::SessionController*> sessionController,
						  const Window::PeerMenuCallback &addCallback) {
	if (!peerData || !peerData->isMegagroup()) {
		return;
	}

	const auto chat = peerData->asMegagroup()->discussionLink();
	if (!chat) {
		return;
	}

	addCallback(
		tr::lng_context_open_channel(tr::now),
		[=]
		{
			sessionController->showPeerHistory(chat, Window::SectionShow::Way::Forward);
		},
		&st::menuIconChannel);
}

void AddShadowBanAction(PeerData *peerData,
						const Window::PeerMenuCallback &addCallback) {
	const auto &settings = AyuSettings::getInstance();
	if (!peerData || !peerData->isUser() || !settings.filtersEnabled) {
		return;
	}

	const auto realId = getDialogIdFromPeer(peerData);
	const auto shadowBanned = ShadowBanUtils::isShadowBanned(realId);
	const auto toggleShadowBan = [=]
	{
		if (shadowBanned) {
			ShadowBanUtils::removeShadowBan(realId);
		} else {
			ShadowBanUtils::addShadowBan(realId);
		}
	};

	addCallback({
		.text = (shadowBanned
					 ? tr::ayu_FiltersQuickUnshadowBan(tr::now)
					 : tr::ayu_FiltersQuickShadowBan(tr::now)),
		.handler = toggleShadowBan,
		.icon = shadowBanned ? &st::menuIconShowInChat : &st::menuIconStealth,
	});
}

void AddDeleteOwnMessagesAction(PeerData *peerData,
								Data::ForumTopic *topic,
								not_null<Window::SessionController*> sessionController,
								const Window::PeerMenuCallback &addCallback) {
	if (topic) {
		return;
	}
	const auto isGroup = peerData->isChat() || peerData->isMegagroup();
	if (!isGroup) {
		return;
	}
	if (const auto chat = peerData->asChat()) {
		if (!chat->amIn() || chat->amCreator() || chat->hasAdminRights()) {
			return;
		}
	} else if (const auto channel = peerData->asChannel()) {
		if (!channel->isMegagroup() || !channel->amIn() || channel->amCreator() || channel->hasAdminRights()) {
			return;
		}
	} else {
		return;
	}
	addCallback(
		tr::ayu_DeleteOwnMessages(tr::now),
		DeleteMyMessagesHandler(sessionController, peerData),
		&st::menuIconTTL);
}

void AddHistoryAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	if (item->hideEditedBadge()) {
		return;
	}

	const auto edited = item->Get<HistoryMessageEdited>();
	if (!edited) {
		return;
	}

	const auto has = AyuMessages::hasRevisions(item);
	if (!has) {
		return;
	}

	menu->addAction(
		tr::ayu_EditsHistoryMenuText(tr::now),
		[=]
		{
			item->history()->session().tryResolveWindow()
				->showSection(
					std::make_shared<MessageHistory::SectionMemento>(item->history()->peer, item, 0));
		},
		&st::ayuEditsHistoryIcon);
}

void AddHideMessageAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	const auto &settings = AyuSettings::getInstance();
	if (!needToShowItem(settings.showHideMessageInContextMenu)) {
		return;
	}

	if (item->history()->peer->isSelf()) {
		return;
	}

	const auto history = item->history();
	const auto owner = &history->owner();
	menu->addAction(
		tr::ayu_ContextHideMessage(tr::now),
		[=]()
		{
			const auto ids = owner->itemOrItsGroup(item);
			for (const auto &fullId : ids) {
				if (const auto current = owner->message(fullId)) {
					current->destroy();
					AyuState::hide(current);
				}
			}
			history->requestChatListMessage();
		},
		&st::menuIconClear);
}

void AddUserMessagesAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	const auto &settings = AyuSettings::getInstance();
	if (!needToShowItem(settings.showUserMessagesInContextMenu)) {
		return;
	}

	if (item->history()->peer->isChat() || item->history()->peer->isMegagroup()) {
		menu->addAction(
			tr::ayu_UserMessagesMenuText(tr::now),
			[=]
			{
				if (const auto controller = item->history()->session().tryResolveWindow()) {
					const auto peer = item->history()->peer;
					const auto key = (peer && !peer->isUser())
										 ? item->topic()
											   ? Dialogs::Key{item->topic()}
											   : Dialogs::Key{item->history()}
										 : Dialogs::Key{item->history()};
					controller->searchInChat(key, item->from());
				}
			},
			&st::menuIconTTL);
	}
}

void AddMessageDetailsAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	const auto &settings = AyuSettings::getInstance();
	if (!needToShowItem(settings.showMessageDetailsInContextMenu)) {
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

	const auto emojiPacks = HistoryView::CollectEmojiPacks(item, HistoryView::EmojiPacksSource::Message);
	auto containsSingleCustomEmojiPack = emojiPacks.size() == 1;
	if (!containsSingleCustomEmojiPack && emojiPacks.size() > 1) {
		const auto author = emojiPacks.front().id >> 32;
		auto sameAuthor = true;
		for (const auto &pack : emojiPacks) {
			if (pack.id >> 32 != author) {
				sameAuthor = false;
				break;
			}
		}

		containsSingleCustomEmojiPack = sameAuthor;
	}

	const auto isForwarded = forwarded && !forwarded->story && forwarded->psaType.isEmpty();

	const auto messageId = QString::number(item->id.bare);
	const auto messageDate = base::unixtime::parse(item->date());
	const auto messageEditDate = base::unixtime::parse(view ? view->displayedEditDate() : TimeId(0));

	const auto messageForwardedDate =
		isForwarded && forwarded
			? base::unixtime::parse(forwarded->originalDate)
			: QDateTime();

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
					const auto authorId = getUserIdFromPackId(media->document()->sticker()->set.id);

					if (authorId != 0) {
						menu2->addAction(Ui::ContextActionStickerAuthor(
							menu2->menu(),
							&item->history()->session(),
							authorId
						));
					}
				}
			}

			if (containsSingleCustomEmojiPack) {
				const auto authorId = getUserIdFromPackId(emojiPacks.front().id);

				if (authorId != 0) {
					menu2->addAction(Ui::ContextActionStickerAuthor(
						menu2->menu(),
						&item->history()->session(),
						authorId
					));
				}
			}
		},
	});
}

void AddReadUntilAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	if (item->isLocal() || item->isService() || item->out() || item->isDeleted()) {
		return;
	}

	if (item->history()->peer->isSelf()) {
		return;
	}

	const auto &settings = AyuSettings::getInstance();
	if (settings.sendReadMessages) {
		return;
	}

	menu->addAction(
		tr::ayu_ReadUntilMenuText(tr::now),
		[=]()
		{
			readHistory(item);
			if (item->media() && item->media()->ttlSeconds() <= 0 && item->unsupportedTTL() <= 0 && !item->out() && item
				->isUnreadMedia()) {
				const auto ids = MTP_vector<MTPint>(1, MTP_int(item->id));
				if (const auto channel = item->history()->peer->asChannel()) {
					item->history()->session().api().request(MTPchannels_ReadMessageContents(
						channel->inputChannel,
						ids
					)).send();
				} else {
					item->history()->session().api().request(MTPmessages_ReadMessageContents(
						ids
					)).done([=](const MTPmessages_AffectedMessages &result)
					{
						item->history()->session().api().applyAffectedMessages(
							item->history()->peer,
							result);
					}).send();
				}
				item->markContentsRead();
			}
		},
		&st::menuIconShowInChat);
}

void AddBurnAction(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
	if (!item->media() || item->media()->ttlSeconds() <= 0 && item->unsupportedTTL() <= 0 || item->out() ||
		!item->isUnreadMedia()) {
		return;
	}

	menu->addAction(
		tr::ayu_ExpireMediaContextMenuText(tr::now),
		[=]()
		{
			const auto ids = MTP_vector<MTPint>(1, MTP_int(item->id));
			const auto callback = [=]()
			{
				if (const auto window = Core::App().activeWindow()) {
					if (const auto controller = window->sessionController()) {
						controller->showToast(tr::lng_box_ok(tr::now));
					}
				}
			};

			if (const auto channel = item->history()->peer->asChannel()) {
				item->history()->session().api().request(MTPchannels_ReadMessageContents(
					channel->inputChannel,
					ids
				)).done([=]()
				{
					callback();
				}).send();
			} else {
				item->history()->session().api().request(MTPmessages_ReadMessageContents(
					ids
				)).done([=](const MTPmessages_AffectedMessages &result)
				{
					item->history()->session().api().applyAffectedMessages(
						item->history()->peer,
						result);
					callback();
				}).send();
			}
			item->markContentsRead();
		},
		&st::menuIconTTLAny);
}

void AddCreateFilterAction(not_null<Ui::PopupMenu*> menu,
						   not_null<Window::SessionController*> controller,
						   HistoryItem *item,
						   const QString &selectedText) {
	const auto &settings = AyuSettings::getInstance();
	if (!needToShowItem(settings.showAddFilterInContextMenu) || !settings.filtersEnabled) {
		return;
	}

	if (!item || selectedText.isEmpty()) {
		return;
	}

	menu->addAction(
		tr::ayu_RegexFilterQuickAdd(tr::now),
		[=]
		{
			RegexFilter filter;
			filter.text = selectedText.toStdString();
			filter.reversed = false;

			controller->show(Settings::RegexEditBox(&filter, {}, getDialogIdFromPeer(item->history()->peer), true));
		},
		&st::menuIconAddToFolder);
}

} // namespace AyuUi
