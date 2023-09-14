// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "mainwindow.h"
#include "base/call_delayed.h"
#include "base/unixtime.h"
#include "base/platform/base_platform_info.h"
#include "base/qt/qt_common_adapters.h"
#include "base/qt/qt_key_modifiers.h"
#include "boxes/about_sponsored_box.h"
#include "boxes/delete_messages_box.h"
#include "boxes/premium_preview_box.h"
#include "boxes/report_messages_box.h"
#include "boxes/sticker_set_box.h"
#include "boxes/translate_box.h"
#include "chat_helpers/emoji_interactions.h"
#include "chat_helpers/message_field.h"
#include "core/application.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_document.h"
#include "data/data_file_click_handler.h"
#include "data/data_file_origin.h"
#include "data/data_forum_topic.h"
#include "data/data_histories.h"
#include "data/data_media_types.h"
#include "data/data_message_reactions.h"
#include "data/data_peer_values.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_poll.h"
#include "data/data_session.h"
#include "data/data_sponsored_messages.h"
#include "data/data_user.h"
#include "data/stickers/data_stickers.h"
#include "dialogs/ui/dialogs_video_userpic.h"
#include "history/history_inner_widget.h"
#include "history/history_widget.h"
#include "history/view/history_view_translate_tracker.h"
#include "lang/lang_keys.h"
#include "layout/layout_selection.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "main/session/send_as_peers.h"
#include "menu/menu_item_download_files.h"
#include "settings/settings_premium.h"
#include "styles/style_chat.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h" // st::windowMinWidth
#include "ui/cached_round_corners.h"
#include "ui/inactive_press.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/boxes/report_box.h"
#include "ui/controls/delete_message_context_action.h"
#include "ui/controls/who_reacted_context_action.h"
#include "ui/effects/message_sending_animation_controller.h"
#include "ui/effects/path_shift_gradient.h"
#include "ui/effects/reaction_fly_animation.h"
#include "ui/image/image.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_entity.h"
#include "ui/text/text_options.h"
#include "ui/widgets/popup_menu.h"
#include "window/notifications_manager.h"
#include "window/window_adaptive.h"
#include "window/window_controller.h"
#include "window/window_controller.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"

#include "history/view/history_view_context_menu.h"

#include "ayu/ayu_settings.h"
#include "styles/style_info.h"

namespace AyuUi
{

class AyuPopupMenu
{
public:
	AyuPopupMenu(HistoryInner *parent);

	void addHistoryAction(HistoryItem *item);

	void addHideMessageAction(HistoryItem *item) const;

	void addReadUntilAction(HistoryItem *item) const;

	std::unique_ptr<Ui::PopupMenu> _ayuSubMenu;
};

}
