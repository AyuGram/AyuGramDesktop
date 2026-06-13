/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_inner_widget.h"

#include "info/info_controller.h"
#include "info/info_memento.h"
#include "info/profile/info_profile_widget.h"
#include "info/profile/info_profile_icon.h"
#include "info/profile/info_profile_members.h"
#include "info/profile/info_profile_music_button.h"
#include "info/profile/info_profile_top_bar.h"
#include "info/profile/info_profile_actions.h"
#include "info/media/info_media_buttons.h"
#include "info/saved/info_saved_music_widget.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_forum_topic.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_file_origin.h"
#include "data/data_user.h"
#include "data/data_saved_music.h"
#include "data/data_saved_sublist.h"
#include "info/saved/info_saved_music_common.h"
#include "info_profile_actions.h"
#include "main/main_session.h"
#include "apiwrap.h"
#include "api/api_peer_photo.h"
#include "lang/lang_keys.h"
#include "ui/text/custom_emoji_helper.h"
#include "ui/text/format_song_document_name.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/shadow.h"
#include "ui/wrap/fade_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/ui_utility.h"
#include "styles/style_info.h"

// AyuGram includes
#include "ayu/ayu_settings.h"


namespace Info {
namespace Profile {

namespace {

void AddSavedMusic(
		not_null<Ui::VerticalLayout*> layout,
		not_null<Controller*> controller,
		not_null<PeerData*> peer,
		rpl::producer<std::optional<QColor>> topBarColor) {
	const auto wrap = layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			layout,
			object_ptr<Ui::VerticalLayout>(layout)));
	Info::Saved::SetupSavedMusic(
		wrap->entity(),
		controller,
		peer,
		std::move(topBarColor));
	using namespace rpl::mappers;
	wrap->toggleOn(
		wrap->entity()->heightValue() | rpl::map(_1 > 0),
		anim::type::instant);
}

void AddUnofficialSecurityRiskWarning(
		not_null<Ui::VerticalLayout*> layout,
		not_null<UserData*> user) {
	const auto wrap = layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			layout,
			object_ptr<Ui::VerticalLayout>(layout)));
	const auto content = wrap->entity();
	user->session().changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::FullInfo
	) | rpl::on_next([=] {
		while (content->count()) {
			delete content->widgetAt(0);
		}
		if (user->unofficialSecurityRisk()) {
			auto helper = Ui::Text::CustomEmojiHelper();
			auto icon = helper.paletteDependent({
				.factory = [] {
					const auto s = st::infoSecurityRiskIconSize;
					const auto ratio = style::DevicePixelRatio();
					const auto rect = QRect(0, 0, s, s);
					auto result = QImage(
						rect.size() * ratio,
						QImage::Format_ARGB32_Premultiplied);
					result.setDevicePixelRatio(ratio);
					result.fill(Qt::transparent);

					auto p = QPainter(&result);
					auto hq = PainterHighQualityEnabler(p);
					p.setPen(Qt::NoPen);
					p.setBrush(st::attentionButtonFg);
					p.drawEllipse(rect);

					p.setPen(st::windowFgActive);
					p.setFont(st::semiboldFont);
					p.drawText(rect, u"!"_q, style::al_center);

					p.end();
					return result;
				},
				.margin = st::infoSecurityRiskIconMargin,
			});
			auto label = object_ptr<Ui::FlatLabel>(
				content,
				tr::lng_profile_unofficial_warning(
					lt_icon,
					rpl::single(std::move(icon)),
					lt_name,
					rpl::single(TextWithEntities{ user->firstName }),
					tr::marked),
				st::defaultDividerLabel.label,
				st::defaultPopupMenu,
				helper.context([=] { content->update(); }));
			content->add(object_ptr<Ui::DividerLabel>(
				content,
				std::move(label),
				st::defaultBoxDividerLabelPadding,
				st::defaultDividerLabel.bar,
				RectParts()));
		}
		content->resizeToWidth(content->width());
	}, content->lifetime());
	using namespace rpl::mappers;
	wrap->toggleOn(
		content->heightValue() | rpl::map(_1 > 0),
		anim::type::instant);
}

} // namespace

InnerWidget::InnerWidget(
	QWidget *parent,
	not_null<Controller*> controller,
	Origin origin)
: RpWidget(parent)
, _controller(controller)
, _peer(_controller->key().peer())
, _migrated(_controller->migrated())
, _topic(_controller->key().topic())
, _sublist(_controller->key().sublist())
, _content(setupContent(this, origin)) {
	_content->heightValue(
	) | rpl::on_next([this](int height) {
		if (!_inResize) {
			resizeToWidth(width());
			updateDesiredHeight();
		}
	}, lifetime());
}

rpl::producer<> InnerWidget::backRequest() const {
	return _backClicks.events();
}

object_ptr<Ui::RpWidget> InnerWidget::setupContent(
		not_null<RpWidget*> parent,
		Origin origin) {
	if (const auto user = _peer->asUser()) {
		user->session().changes().peerFlagsValue(
			user,
			Data::PeerUpdate::Flag::FullInfo
		) | rpl::on_next([=] {
			auto &photos = user->session().api().peerPhoto();
			if (const auto original = photos.nonPersonalPhoto(user)) {
				// Preload it for the edit contact box.
				_nonPersonalView = original->createMediaView();
				const auto id = peerToUser(user->id);
				original->load(Data::FileOriginFullUser{ id });
			}
		}, lifetime());
	}

	auto result = object_ptr<Ui::VerticalLayout>(parent);

	const auto musicPeer = _sublist
		? _sublist->sublistPeer().get()
		: _peer.get();
	AddSavedMusic(
		result.data(),
		_controller,
		musicPeer,
		_topBarColor.value());
	if (const auto user = _peer->asUser()) {
		AddUnofficialSecurityRiskWarning(result.data(), user);
	}

	auto stack = SectionStack(result.data());
	if (_topic && _topic->creating()) {
		stack.finalize();
		return result;
	}

	BuildProfileDetailsSections(
		stack,
		_controller,
		_peer,
		_topic,
		_sublist,
		origin);

	auto sharedTracker = Ui::MultiSlideTracker();
	{
		auto sharedMediaWidget = setupSharedMedia(
			result.data(),
			sharedTracker);
		const auto raw = sharedMediaWidget.data();
		_sharedMediaWrap = raw;
		stack.addPlainSeparator();
		stack.add(Section{
			.widget = std::move(sharedMediaWidget),
			.shown = raw->toggledValue(),
		});
	}
	if (_topic || _sublist) {
		stack.finalize();
		return result;
	}
	if (auto manage = SetupChannelMembersAndManage(
			_controller,
			result.data(),
			_peer)) {
		const auto raw = static_cast<Ui::SlideWrap<Ui::RpWidget>*>(
			manage.data());
		stack.addPlainSeparator();
		stack.add(Section{
			.widget = std::move(manage),
			.shown = raw->toggledValue(),
		});
	}
	if (auto actions = SetupActions(_controller, result.data(), _peer)) {
		stack.addPlainSeparator();
		stack.add(Section{
			.widget = std::move(actions),
			.shown = rpl::single(true),
		});
	}
	if ((_peer->isChat() || _peer->isMegagroup())
		&& !_peer->isMonoforum()) {
		stack.addPlainSeparator();
		stack.add(makeMembersSection(result.data()));
	}
	stack.finalize();
	return result;
}

Section InnerWidget::makeMembersSection(not_null<QWidget*> parent) {
	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto raw = wrap.data();
	const auto inner = raw->entity();
	_members = inner->add(object_ptr<Members>(inner, _controller));
	_members->scrollToRequests(
	) | rpl::on_next([this](Ui::ScrollToRequest request) {
		auto min = (request.ymin < 0)
			? request.ymin
			: MapFrom(this, _members, QPoint(0, request.ymin)).y();
		auto max = (request.ymin < 0)
			? MapFrom(this, _members, QPoint()).y()
			: (request.ymax < 0)
			? request.ymax
			: MapFrom(this, _members, QPoint(0, request.ymax)).y();
		_scrollToRequests.fire({ min, max });
	}, _members->lifetime());
	_members->onlineCountValue(
	) | rpl::on_next([=](int count) {
		_onlineCount.fire_copy(count);
	}, _members->lifetime());

	using namespace rpl::mappers;
	raw->toggleOn(
		_members->fullCountValue() | rpl::map(_1 > 0),
		anim::type::instant);
	return Section{
		.widget = std::move(wrap),
		.shown = raw->toggledValue(),
	};
}

object_ptr<Ui::SlideWrap<Ui::RpWidget>> InnerWidget::setupSharedMedia(
		not_null<RpWidget*> parent,
		Ui::MultiSlideTracker &sharedTracker) {
	using namespace rpl::mappers;
	using MediaType = Media::Type;

	const auto &settings = AyuSettings::getInstance();

	const auto peer = _sublist ? _sublist->sublistPeer() : _peer;
	auto content = object_ptr<Ui::VerticalLayout>(parent);
	auto &tracker = sharedTracker;
	auto addMediaButton = [&](
			MediaType type,
			const style::icon &icon) {
		auto result = Media::AddButton(
			content,
			_controller,
			peer,
			_topic ? _topic->rootId() : MsgId(),
			_sublist ? _sublist->sublistPeer()->id : PeerId(),
			_migrated,
			type,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addCommonGroupsButton = [&](
			not_null<UserData*> user,
			const style::icon &icon) {
		auto result = Media::AddCommonGroupsButton(
			content,
			_controller,
			user,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	const auto addSimilarPeersButton = [&](
			not_null<PeerData*> peer,
			const style::icon &icon) {
		if (settings.hideSimilarChannels()) {
			return;
		}

		auto result = Media::AddSimilarPeersButton(
			content,
			_controller,
			peer,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addStoriesButton = [&](
			not_null<PeerData*> peer,
			const style::icon &icon) {
		if (peer->isChat()) {
			return;
		}
		auto result = Media::AddStoriesButton(
			content,
			_controller,
			peer,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addSavedSublistButton = [&](
			not_null<PeerData*> peer,
			const style::icon &icon) {
		auto result = Media::AddSavedSublistButton(
			content,
			_controller,
			peer,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};
	auto addPeerGiftsButton = [&](
			not_null<PeerData*> peer,
			const style::icon &icon) {
		auto result = Media::AddPeerGiftsButton(
			content,
			_controller,
			peer,
			tracker);
		object_ptr<Profile::FloatingIcon>(
			result,
			icon,
			st::infoSharedMediaButtonIconPosition);
	};

	if (!_topic) {
		addStoriesButton(peer, st::infoIconMediaStories);
		addPeerGiftsButton(peer, st::infoIconMediaGifts);
		addSavedSublistButton(peer, st::infoIconMediaSaved);
	}
	addMediaButton(MediaType::Photo, st::infoIconMediaPhoto);
	addMediaButton(MediaType::Video, st::infoIconMediaVideo);
	addMediaButton(MediaType::File, st::infoIconMediaFile);
	addMediaButton(MediaType::MusicFile, st::infoIconMediaAudio);
	addMediaButton(MediaType::Link, st::infoIconMediaLink);
	addMediaButton(MediaType::Poll, st::infoIconMediaPoll);
	addMediaButton(MediaType::RoundVoiceFile, st::infoIconMediaVoice);
	addMediaButton(MediaType::GIF, st::infoIconMediaGif);
	if (const auto bot = peer->asBot()) {
		addCommonGroupsButton(bot, st::infoIconMediaGroup);
		addSimilarPeersButton(bot, st::infoIconMediaBot);
	} else if (const auto channel = peer->asBroadcast()) {
		addSimilarPeersButton(channel, st::infoIconMediaChannel);
	} else if (const auto user = peer->asUser()) {
		addCommonGroupsButton(user, st::infoIconMediaGroup);
	}

	auto result = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent)
	);

	result->setDuration(
		st::infoSlideDuration
	)->toggleOn(
		tracker.atLeastOneShownValue()
	);

	auto layout = result->entity();

	layout->add(std::move(content));

	return result;
}

int InnerWidget::countDesiredHeight() const {
	return _content->height() + (_members
		? (_members->desiredHeight() - _members->height())
		: 0);
}

void InnerWidget::visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) {
	setChildVisibleTopBottom(_content, visibleTop, visibleBottom);
}

void InnerWidget::saveState(not_null<Memento*> memento) {
	if (_members) {
		memento->setMembersState(_members->saveState());
	}
}

void InnerWidget::restoreState(not_null<Memento*> memento) {
	if (_members) {
		_members->restoreState(memento->membersState());
	}
	if (_sharedMediaWrap) {
		_sharedMediaWrap->finishAnimating();
	}
}

rpl::producer<Ui::ScrollToRequest> InnerWidget::scrollToRequests() const {
	return _scrollToRequests.events();
}

rpl::producer<int> InnerWidget::desiredHeightValue() const {
	return _desiredHeight.events_starting_with(countDesiredHeight());
}

int InnerWidget::resizeGetHeight(int newWidth) {
	_inResize = true;
	auto guard = gsl::finally([&] { _inResize = false; });

	_content->resizeToWidth(newWidth);
	_content->moveToLeft(0, 0);
	updateDesiredHeight();
	return _content->heightNoMargins();
}

void InnerWidget::enableBackButton() {
	_backToggles.force_assign(true);
}

void InnerWidget::showFinished() {
	_showFinished.fire({});
}

bool InnerWidget::hasFlexibleTopBar() const {
	return true;
}

base::weak_qptr<Ui::RpWidget> InnerWidget::createPinnedToTop(
		not_null<Ui::RpWidget*> parent) {
	const auto content = Ui::CreateChild<TopBar>(
		parent,
		TopBar::Descriptor{
			.controller = _controller->parentController(),
			.key = _controller->key(),
			.wrap = _controller->wrapValue(),
			.peer = _sublist ? _sublist->sublistPeer().get() : nullptr,
			.backToggles = _backToggles.value(),
			.showFinished = _showFinished.events(),
		});
	content->backRequest(
	) | rpl::start_to_stream(_backClicks, content->lifetime());
	content->setOnlineCount(_onlineCount.events());
	_topBarColor = content->edgeColor();
	return base::make_weak(not_null<Ui::RpWidget*>{ content });
}

base::weak_qptr<Ui::RpWidget> InnerWidget::createPinnedToBottom(
		not_null<Ui::RpWidget*> parent) {
	return nullptr;
}

} // namespace Profile
} // namespace Info
