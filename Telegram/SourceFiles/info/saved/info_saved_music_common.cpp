/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/saved/info_saved_music_common.h"

#include "data/data_peer.h"
#include "data/data_saved_music.h"
#include "data/data_saved_sublist.h"
#include "history/history_item.h"
#include "info/info_controller.h"
#include "info/info_memento.h"
#include "info/profile/info_profile_music_button.h"
#include "info/saved/info_saved_music_widget.h"
#include "ui/text/format_song_document_name.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"

// AyuGram includes
#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/components/saved_music.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_document.h"
#include "styles/style_menu_icons.h"
#include "ui/widgets/popup_menu.h"

namespace Info::Saved {

namespace {

[[nodiscard]] Profile::MusicButtonData DocumentMusicButtonData(
		not_null<DocumentData*> document, not_null<HistoryItem*> message) {
	const auto name = Ui::Text::FormatSongNameFor(document);

	return {
		.name = name,
		.title = name.composedName().title,
		.performer = name.composedName().performer,
		.msgId = message->fullId(),
		.mediaView = document->createMediaView(),
	};
}

} // namespace

void SetupSavedMusic(
		not_null<Ui::VerticalLayout*> container,
		not_null<Info::Controller*> controller,
		not_null<PeerData*> peer,
		rpl::producer<std::optional<QColor>> topBarColor) {
	auto musicValue = Data::SavedMusic::Supported(peer->id)
		? Data::SavedMusicList(
			peer,
			nullptr,
			1
		) | rpl::map([=](const Data::SavedMusicSlice &data) {
			return data.size() ? data[0].get() : nullptr;
		}) | rpl::type_erased
		: rpl::single<HistoryItem*>((HistoryItem*)(nullptr));

	const auto divider = container->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			container,
			object_ptr<Ui::VerticalLayout>(container)));

	rpl::combine(
		std::move(musicValue),
		std::move(topBarColor)
	) | rpl::on_next([=](
			HistoryItem *item,
			std::optional<QColor> color) {
		while (divider->entity()->count()) {
			delete divider->entity()->widgetAt(0);
		}
		if (item) {
			if (const auto document = item->media()
					? item->media()->document()
					: nullptr) {
				auto musicButton = divider->entity()->add(object_ptr<Ui::SlideWrap<Profile::AyuMusicButton>>(
					divider->entity(),
					object_ptr<Profile::AyuMusicButton>(
						divider->entity(),
						DocumentMusicButtonData(document, item),
						color,
						[window = controller, peer]
						{
							window->showSection(Info::Saved::MakeMusic(peer));
						})));

				musicButton->hide(anim::type::instant);
				musicButton->ease = anim::easeOutCubic;
				musicButton->setDuration(250);
				musicButton->entity()->setAcceptBoth(true);
				musicButton->entity()->clicks() | rpl::filter([=](Qt::MouseButton mouseButton)
				{
					return mouseButton == Qt::RightButton;
				}) | rpl::on_next([=]
										  {
											  const auto &settings = AyuSettings::getInstance();

											  const auto contextMenu = new Ui::PopupMenu(
												  nullptr,
												  st::popupMenuWithIcons);
											  contextMenu->setAttribute(Qt::WA_DeleteOnClose);

											  contextMenu->addAction(
												  settings.adaptiveCoverColor()
													  ? tr::ayu_DisableColorfulCover(tr::now)
													  : tr::ayu_EnableColorfulCover(tr::now),
												  [=]
												  {
													  AyuSettings::getInstance().setAdaptiveCoverColor(!AyuSettings::getInstance().adaptiveCoverColor());

													  const auto mediaRefreshed = item ? item->media() : nullptr;
													  const auto documentRefreshed = mediaRefreshed
														  ? mediaRefreshed->document()
														  : nullptr;

													  if (!documentRefreshed) {
														  return;
													  }
													  musicButton->entity()->updateData(
														  DocumentMusicButtonData(documentRefreshed, item));
												  },
												  &st::menuIconPalette);

											  contextMenu->popup(QCursor::pos());
										  },
										  musicButton->lifetime());

				const auto weak = base::make_weak(musicButton);
				musicButton->entity()->onReady() | rpl::on_next(
					[=]
					{
						// fix animation glitch
						dispatchToMainThread(
							[=]
							{
								if (const auto strong = weak.get()) {
									strong->show(anim::type::normal);
								}
							},
							st::widgetFadeDuration);
					},
					musicButton->lifetime());
			}
			divider->toggle(true, anim::type::normal);
		}
	}, container->lifetime());
	divider->finishAnimating();
}

} // namespace Info::Saved