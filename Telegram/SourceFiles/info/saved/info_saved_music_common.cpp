/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/saved/info_saved_music_common.h"

#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_saved_music.h"
#include "data/data_emoji_statuses.h"
#include "history/history.h"
#include "data/data_saved_sublist.h"
#include "history/history_item.h"
#include "info/info_controller.h"
#include "info/info_memento.h"
#include "info/profile/info_profile_music_button.h"
#include "info/saved/info_saved_music_widget.h"
#include "ui/text/format_song_document_name.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"

// AyuGram includes
#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/components/saved_music.h"
#include "ayu/utils/now_playing_client.h"
#include "ayu/utils/telegram_helpers.h"
#include "core/file_utilities.h"
#include "data/data_document.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_menu_icons.h"
#include "ui/widgets/box_content_divider.h"
#include "ui/widgets/popup_menu.h"
#include "base/timer.h"


namespace Info::Saved {

namespace {

struct SavedMusicState {
	std::optional<Ayu::NowPlaying::Track> liveTrack;
	HistoryItem *savedItem = nullptr;
	std::optional<QColor> color;
	bool liveQueryDone = false;
	bool savedQueryDone = false;
};

[[nodiscard]] Profile::MusicButtonData DocumentMusicButtonData(
		not_null<DocumentData*> document,
		not_null<HistoryItem*> message,
		not_null<PeerData*> peer) {
	const auto name = Ui::Text::FormatSongNameFor(document);
	DocumentId patternEmojiId = peer->profileBackgroundEmojiId();
	if (!patternEmojiId) {
		patternEmojiId = peer->backgroundEmojiId();
	}
	if (!patternEmojiId) {
		patternEmojiId = peer->isPremium() ? 5271627010681108586ULL : 5325674462522144646ULL;
	}

	return {
		.name = name,
		.title = name.composedName().title,
		.performer = name.composedName().performer,
		.album = QString(),
		.msgId = message->fullId(),
		.mediaView = document->createMediaView(),
		.patternEmojiId = patternEmojiId,
		.session = &message->history()->session(),
		.isLiveTrack = false,
		.isPremium = peer->isPremium(),
	};
}

} // namespace

rpl::producer<bool> SetupSavedMusic(
		not_null<Ui::VerticalLayout*> container,
		not_null<Info::Controller*> controller,
		not_null<PeerData*> peer,
		rpl::producer<std::optional<QColor>> topBarColor) {
	const auto hasMusic = container->lifetime().make_state<
		rpl::variable<bool>>(false);
	const auto state = container->lifetime().make_state<SavedMusicState>();

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

	const auto updateWidget = [=] {
		if (!state->liveQueryDone) {
			return;
		}

		auto shown = false;
		if (state->liveTrack && state->liveTrack->isPlaying) {
			const auto &track = *state->liveTrack;
			DocumentId patternEmojiId = 0;
			if (track.platform.compare(u"SPOTIFY"_q, Qt::CaseInsensitive) == 0) {
				patternEmojiId = 5271857023359681001ULL;
			} else if (track.platform.compare(u"TELEGRAM"_q, Qt::CaseInsensitive) == 0) {
				patternEmojiId = peer->profileBackgroundEmojiId();
				if (!patternEmojiId) {
					patternEmojiId = peer->backgroundEmojiId();
				}
				if (!patternEmojiId) {
					const auto emojiStatus = peer->emojiStatusId();
					patternEmojiId = emojiStatus.collectible
						? emojiStatus.collectible->documentId
						: emojiStatus.documentId;
				}
				if (!patternEmojiId) {
					patternEmojiId = peer->isPremium() ? 5271627010681108586ULL : 5325674462522144646ULL;
				}
			} else {
				patternEmojiId = peer->isPremium() ? 5271627010681108586ULL : 5325674462522144646ULL;
			}

			const auto songUrl = track.songUrl;
			const auto buttonData = Profile::MusicButtonData{
				.name = Ui::Text::FormatSongName(QString(), track.title, track.performer),
				.title = track.title,
				.performer = track.performer,
				.album = track.album,
				.msgId = FullMsgId(),
				.mediaView = nullptr,
				.patternEmojiId = patternEmojiId,
				.session = &controller->session(),
				.songUrl = track.songUrl,
				.previewUrl = track.previewUrl,
				.coverUrl = track.coverUrl,
				.platform = track.platform,
				.isLiveTrack = true,
				.isPremium = peer->isPremium(),
			};

			if (divider->entity()->count() > 0) {
				const auto first = divider->entity()->widgetAt(0);
				auto *existing = dynamic_cast<Profile::AyuMusicButton*>(first);
				if (!existing) {
					if (const auto wrap = dynamic_cast<Ui::SlideWrap<Profile::AyuMusicButton>*>(first)) {
						existing = wrap->entity();
					}
				}
				if (existing && existing->isLiveTrack()) {
					existing->updateData(buttonData);
					existing->setClickedCallback([songUrl, window = controller, peer] {
						if (!songUrl.isEmpty()) {
							File::OpenUrl(songUrl);
						} else {
							window->showSection(Info::Saved::MakeMusic(peer));
						}
					});
					divider->toggle(true, anim::type::normal);
					*hasMusic = true;
					return;
				}
			}

			while (divider->entity()->count()) {
				const auto widget = divider->entity()->widgetAt(0);
				divider->entity()->removeWidget(widget);
				widget->deleteLater();
			}

			auto musicButton = divider->entity()->add(object_ptr<Ui::SlideWrap<Profile::AyuMusicButton>>(
				divider->entity(),
				object_ptr<Profile::AyuMusicButton>(
					divider->entity(),
					buttonData,
					state->color,
					[songUrl, window = controller, peer] {
						if (!songUrl.isEmpty()) {
							File::OpenUrl(songUrl);
						} else {
							window->showSection(Info::Saved::MakeMusic(peer));
						}
					})));

			musicButton->hide(anim::type::instant);
			musicButton->ease = anim::easeOutCubic;
			musicButton->setDuration(250);
			musicButton->entity()->setAcceptBoth(true);

			musicButton->entity()->clicks() | rpl::filter([=](Qt::MouseButton mouseButton) {
				return mouseButton == Qt::RightButton;
			}) | rpl::on_next([=] {
				const auto &settings = AyuSettings::getInstance();

				const auto contextMenu = Ui::CreateChild<Ui::PopupMenu>(
					musicButton->entity(),
					st::popupMenuWithIcons);

				if (!songUrl.isEmpty()) {
					contextMenu->addAction(
						tr::lng_open_link(tr::now),
						[=] {
							File::OpenUrl(songUrl);
						},
						&st::menuIconPalette);
					contextMenu->addAction(
						tr::lng_context_copy_link(tr::now),
						[=] {
							TextUtilities::SetClipboardText({ songUrl });
						},
						&st::menuIconPalette);
				}

				contextMenu->addAction(
					settings.adaptiveCoverColor()
						? tr::ayu_DisableColorfulCover(tr::now)
						: tr::ayu_EnableColorfulCover(tr::now),
					[=] {
						AyuSettings::getInstance().setAdaptiveCoverColor(!AyuSettings::getInstance().adaptiveCoverColor());
						musicButton->entity()->updateData(buttonData);
					},
					&st::menuIconPalette);

				contextMenu->popup(QCursor::pos());
			}, musicButton->lifetime());

			const auto weak = base::make_weak(musicButton);
			musicButton->entity()->onReady() | rpl::on_next([=] {
				dispatchToMainThread([=] {
					if (const auto strong = weak.get()) {
						strong->show(anim::type::normal);
					}
				}, st::widgetFadeDuration);
			}, musicButton->lifetime());

			shown = true;
		} else {
			while (divider->entity()->count()) {
				const auto widget = divider->entity()->widgetAt(0);
				divider->entity()->removeWidget(widget);
				widget->deleteLater();
			}
		}

		if (!shown && state->savedItem) {
			if (const auto document = state->savedItem->media()
					? state->savedItem->media()->document()
					: nullptr) {
				const auto buttonData = DocumentMusicButtonData(document, state->savedItem, peer);
				auto musicButton = divider->entity()->add(object_ptr<Ui::SlideWrap<Profile::AyuMusicButton>>(
					divider->entity(),
					object_ptr<Profile::AyuMusicButton>(
						divider->entity(),
						buttonData,
						state->color,
						[window = controller, peer] {
							window->showSection(Info::Saved::MakeMusic(peer));
						})));

				musicButton->hide(anim::type::instant);
				musicButton->ease = anim::easeOutCubic;
				musicButton->setDuration(250);
				musicButton->entity()->setAcceptBoth(true);

				musicButton->entity()->clicks() | rpl::filter([=](Qt::MouseButton mouseButton) {
					return mouseButton == Qt::RightButton;
				}) | rpl::on_next([=] {
					const auto &settings = AyuSettings::getInstance();

					const auto contextMenu = Ui::CreateChild<Ui::PopupMenu>(
						musicButton->entity(),
						st::popupMenuWithIcons);

					contextMenu->addAction(
						settings.adaptiveCoverColor()
							? tr::ayu_DisableColorfulCover(tr::now)
							: tr::ayu_EnableColorfulCover(tr::now),
						[=] {
							AyuSettings::getInstance().setAdaptiveCoverColor(!AyuSettings::getInstance().adaptiveCoverColor());

							const auto mediaRefreshed = state->savedItem ? state->savedItem->media() : nullptr;
							const auto documentRefreshed = mediaRefreshed
								? mediaRefreshed->document()
								: nullptr;

							if (!documentRefreshed) {
								return;
							}
							musicButton->entity()->updateData(
								DocumentMusicButtonData(documentRefreshed, state->savedItem, peer));
						},
						&st::menuIconPalette);

					contextMenu->popup(QCursor::pos());
				}, musicButton->lifetime());

				const auto weak = base::make_weak(musicButton);
				musicButton->entity()->onReady() | rpl::on_next([=] {
					dispatchToMainThread([=] {
						if (const auto strong = weak.get()) {
							strong->show(anim::type::normal);
						}
					}, st::widgetFadeDuration);
				}, musicButton->lifetime());

				shown = true;
			}
		}

		if (shown) {
			divider->entity()->add(
				object_ptr<Ui::BoxContentDivider>(
					divider->entity(),
					st::boxDividerHeight,
					st::defaultDividerBar),
				style::margins(0, 0, 0, st::infoProfileSkip));
		}

		divider->toggle(shown, anim::type::normal);
		*hasMusic = shown;
	};

	if (peer->isUser()) {
		const auto bareId = getBareID(peer);
		const auto hasExtera = isExteraPeer(bareId) || isSupporterPeer(bareId) || isCustomBadgePeer(bareId);
		if (hasExtera) {
			const auto userId = peerToUser(peer->id).bare;
			Ayu::NowPlaying::Fetch(userId, crl::guard(container, [=](std::optional<Ayu::NowPlaying::Track> track) {
				state->liveTrack = track;
				state->liveQueryDone = true;
				updateWidget();
			}));

			const auto pollTimer = container->lifetime().make_state<base::Timer>();
			pollTimer->setCallback([=] {
				Ayu::NowPlaying::Fetch(userId, crl::guard(container, [=](std::optional<Ayu::NowPlaying::Track> track) {
					if (state->liveTrack != track) {
						state->liveTrack = track;
						updateWidget();
					}
				}), true);
			});
			pollTimer->callEach(25000);
		} else {
			state->liveQueryDone = true;
		}
	} else {
		state->liveQueryDone = true;
	}

	rpl::combine(
		std::move(musicValue),
		std::move(topBarColor)
	) | rpl::on_next([=](
			HistoryItem *item,
			std::optional<QColor> color) {
		state->savedItem = item;
		state->color = color;
		state->savedQueryDone = true;
		updateWidget();
	}, container->lifetime());

	divider->finishAnimating();

	return hasMusic->value();
}

} // namespace Info::Saved
