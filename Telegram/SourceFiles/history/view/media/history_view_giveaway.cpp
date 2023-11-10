/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_giveaway.h"

#include "base/unixtime.h"
#include "boxes/gift_premium_box.h"
#include "chat_helpers/stickers_gift_box_pack.h"
#include "chat_helpers/stickers_dice_pack.h"
#include "countries/countries_instance.h"
#include "data/data_channel.h"
#include "data/data_media_types.h"
#include "history/view/media/history_view_media_generic.h"
#include "history/view/history_view_element.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/effects/credits_graphics.h"
#include "ui/text/text_utilities.h"
#include "styles/style_chat.h"

namespace HistoryView {

constexpr auto kOutlineRatio = 0.85;

auto GenerateGiveawayStart(
	not_null<Element*> parent,
	not_null<Data::GiveawayStart*> data)
-> Fn<void(
		not_null<MediaGeneric*>,
		Fn<void(std::unique_ptr<MediaGenericPart>)>)> {
	return [=](
			not_null<MediaGeneric*> media,
			Fn<void(std::unique_ptr<MediaGenericPart>)> push) {
		const auto months = data->months;
		const auto quantity = data->quantity;

		using Data = StickerWithBadgePart::Data;
		const auto sticker = [=] {
			const auto &session = parent->history()->session();
			auto &packs = session.giftBoxStickersPacks();
			return Data{
				.sticker = packs.lookup(months),
				.size = st::msgServiceGiftBoxStickerSize,
				.stopOnLastFrame = true,
			};
		};
		push(std::make_unique<StickerWithBadgePart>(
			parent,
			nullptr,
			sticker,
			st::chatGiveawayStickerPadding,
			data->credits
				? QString::number(data->credits)
				: tr::lng_prizes_badge(
					tr::now,
					lt_amount,
					QString::number(quantity)),
			data->credits
				? Ui::CreditsWhiteDoubledIcon(
					st::chatGiveawayCreditsIconHeight,
					kOutlineRatio)
				: QImage(),
			data->credits
				? std::make_optional(st::creditsBg3->c)
				: std::nullopt));

		auto pushText = [&](
				TextWithEntities text,
				QMargins margins = {},
				const base::flat_map<uint16, ClickHandlerPtr> &links = {}) {
			push(std::make_unique<MediaGenericTextPart>(
				std::move(text),
				margins,
				st::defaultTextStyle,
				links));
		};
		pushText(
			Ui::Text::Bold(
				tr::lng_prizes_title(tr::now, lt_count, quantity)),
			st::chatGiveawayPrizesTitleMargin);

		if (!data->additionalPrize.isEmpty()) {
			pushText(
				tr::lng_prizes_additional(
					tr::now,
					lt_count,
					quantity,
					lt_prize,
					TextWithEntities{ data->additionalPrize },
					Ui::Text::RichLangValue),
				st::chatGiveawayPrizesMargin);
			push(std::make_unique<TextDelimeterPart>(
				tr::lng_prizes_additional_with(tr::now),
				st::chatGiveawayPrizesWithPadding));
		}

		pushText((data->credits && (quantity == 1))
			? tr::lng_prizes_credits_about_single(
				tr::now,
				lt_amount,
				tr::lng_prizes_credits_about_amount(
					tr::now,
					lt_count,
					data->credits,
					Ui::Text::RichLangValue),
				Ui::Text::RichLangValue)
			: (data->credits && (quantity > 1))
			? tr::lng_prizes_credits_about(
				tr::now,
				lt_count,
				quantity,
				lt_amount,
				tr::lng_prizes_credits_about_amount(
					tr::now,
					lt_count,
					data->credits,
					Ui::Text::RichLangValue),
				Ui::Text::RichLangValue)
			: tr::lng_prizes_about(
				tr::now,
				lt_count,
				quantity,
				lt_duration,
				Ui::Text::Bold(GiftDuration(months)),
				Ui::Text::RichLangValue),
			st::chatGiveawayPrizesMargin);
		pushText(
			Ui::Text::Bold(tr::lng_prizes_participants(tr::now)),
			st::chatGiveawayPrizesTitleMargin);

		const auto hasChannel = ranges::any_of(
			data->channels,
			&ChannelData::isBroadcast);
		const auto hasGroup = ranges::any_of(
			data->channels,
			&ChannelData::isMegagroup);
		const auto mixed = (hasChannel && hasGroup);
		pushText({ (data->all
			? (mixed
				? tr::lng_prizes_participants_all_mixed
				: hasGroup
				? tr::lng_prizes_participants_all_group
				: tr::lng_prizes_participants_all)
			: (mixed
				? tr::lng_prizes_participants_new_mixed
				: hasGroup
				? tr::lng_prizes_participants_new_group
				: tr::lng_prizes_participants_new))(
					tr::now,
					lt_count,
					data->channels.size()),
		}, st::chatGiveawayParticipantsMargin);

		auto list = ranges::views::all(
			data->channels
		) | ranges::views::transform([](not_null<ChannelData*> channel) {
			return not_null<PeerData*>(channel);
		}) | ranges::to_vector;
		push(std::make_unique<PeerBubbleListPart>(
			parent,
			std::move(list)));

		const auto &instance = Countries::Instance();
		auto countries = QStringList();
		for (const auto &country : data->countries) {
			const auto name = instance.countryNameByISO2(country);
			const auto flag = instance.flagEmojiByISO2(country);
			countries.push_back(flag + QChar(0xA0) + name);
		}
		if (const auto count = countries.size()) {
			auto united = countries.front();
			for (auto i = 1; i != count; ++i) {
				united = ((i + 1 == count)
					? tr::lng_prizes_countries_and_last
					: tr::lng_prizes_countries_and_one)(
						tr::now,
						lt_countries,
						united,
						lt_country,
						countries[i]);
			}
			pushText({
				tr::lng_prizes_countries(tr::now, lt_countries, united),
			}, st::chatGiveawayPrizesMargin);
		}

		pushText(
			Ui::Text::Bold(tr::lng_prizes_date(tr::now)),
			(countries.empty()
				? st::chatGiveawayNoCountriesTitleMargin
				: st::chatGiveawayPrizesMargin));
		pushText({
			langDateTime(base::unixtime::parse(data->untilDate)),
		}, st::chatGiveawayEndDateMargin);
	};
}

auto GenerateGiveawayResults(
	not_null<Element*> parent,
	not_null<Data::GiveawayResults*> data)
-> Fn<void(
		not_null<MediaGeneric*>,
		Fn<void(std::unique_ptr<MediaGenericPart>)>)> {
	return [=](
			not_null<MediaGeneric*> media,
			Fn<void(std::unique_ptr<MediaGenericPart>)> push) {
		const auto quantity = data->winnersCount;

		using Data = StickerWithBadgePart::Data;
		const auto sticker = [=] {
			const auto &session = parent->history()->session();
			auto &packs = session.diceStickersPacks();
			const auto &emoji = Stickers::DicePacks::kPartyPopper;
			return Data{
				.sticker = packs.lookup(emoji, 0),
				.skipTop = st::chatGiveawayWinnersTopSkip,
				.size = st::maxAnimatedEmojiSize,
				.stopOnLastFrame = true,
			};
		};
		push(std::make_unique<StickerWithBadgePart>(
			parent,
			nullptr,
			sticker,
			st::chatGiveawayStickerPadding,
			data->credits
				? QString::number(data->credits)
				: tr::lng_prizes_badge(
					tr::now,
					lt_amount,
					QString::number(quantity)),
			data->credits
				? Ui::CreditsWhiteDoubledIcon(
					st::chatGiveawayCreditsIconHeight,
					kOutlineRatio)
				: QImage(),
			data->credits
				? std::make_optional(st::creditsBg3->c)
				: std::nullopt));

		auto pushText = [&](
				TextWithEntities text,
				QMargins margins = {},
				const base::flat_map<uint16, ClickHandlerPtr> &links = {}) {
			push(std::make_unique<MediaGenericTextPart>(
				std::move(text),
				margins,
				st::defaultTextStyle,
				links));
		};
		const auto isSingleWinner = (data->winnersCount == 1);
		pushText(
			(isSingleWinner
				? tr::lng_prizes_results_title_one
				: tr::lng_prizes_results_title)(tr::now, Ui::Text::Bold),
			st::chatGiveawayPrizesTitleMargin);
		const auto showGiveawayHandler = JumpToMessageClickHandler(
			data->channel,
			data->launchId,
			parent->data()->fullId());
		pushText(
			tr::lng_prizes_results_about(
				tr::now,
				lt_count,
				quantity,
				lt_link,
				Ui::Text::Link(tr::lng_prizes_results_link(tr::now)),
				Ui::Text::RichLangValue),
			st::chatGiveawayPrizesMargin,
			{ { 1, showGiveawayHandler } });
		pushText(
			(isSingleWinner
				? tr::lng_prizes_results_winner
				: tr::lng_prizes_results_winners)(tr::now, Ui::Text::Bold),
			st::chatGiveawayPrizesTitleMargin);

		push(std::make_unique<PeerBubbleListPart>(
			parent,
			data->winners));
		if (data->winnersCount > data->winners.size()) {
			pushText(
				Ui::Text::Bold(tr::lng_prizes_results_more(
					tr::now,
					lt_count,
					data->winnersCount - data->winners.size())),
				st::chatGiveawayNoCountriesTitleMargin);
		}
		pushText({ (data->credits && isSingleWinner)
			? tr::lng_prizes_credits_results_one(
				tr::now,
				lt_count,
				data->credits)
			: (data->credits && !isSingleWinner)
			? tr::lng_prizes_credits_results_all(
				tr::now,
				lt_count,
				data->credits)
			: data->unclaimedCount
			? tr::lng_prizes_results_some(tr::now)
			: isSingleWinner
			? tr::lng_prizes_results_one(tr::now)
			: tr::lng_prizes_results_all(tr::now)
		}, st::chatGiveawayEndDateMargin);
	};
	paintText(_prizesTitle, _prizesTitleTop, paintw);
	paintText(_prizes, _prizesTop, _prizesWidth);
	paintText(_participantsTitle, _participantsTitleTop, paintw);
	paintText(_participants, _participantsTop, _participantsWidth);
	if (!_countries.isEmpty()) {
		paintText(_countries, _countriesTop, _countriesWidth);
	}
	paintText(_winnersTitle, _winnersTitleTop, paintw);
	paintText(_winners, _winnersTop, paintw);
	paintChannels(p, context);
}

void Giveaway::paintBadge(Painter &p, const PaintContext &context) const {
	validateBadge(context);

	const auto badge = _badge.size() / _badge.devicePixelRatio();
	const auto left = (width() - badge.width()) / 2;
	const auto top = st::chatGiveawayBadgeTop;
	const auto rect = QRect(left, top, badge.width(), badge.height());
	const auto paintContent = [&](QPainter &q) {
		q.drawImage(rect.topLeft(), _badge);
	};

	{
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(context.messageStyle()->msgFileBg);
		const auto half = st::chatGiveawayBadgeStroke / 2.;
		const auto inner = QRectF(rect).marginsRemoved(
			{ half, half, half, half });
		const auto radius = inner.height() / 2.;
		p.drawRoundedRect(inner, radius, radius);
	}

	if (!usesBubblePattern(context)) {
		paintContent(p);
	} else {
		Ui::PaintPatternBubblePart(
			p,
			context.viewport,
			context.bubblesPattern->pixmap,
			rect,
			paintContent,
			_badgeCache);
	}
}

void Giveaway::paintChannels(
		Painter &p,
		const PaintContext &context) const {
	if (_channels.empty()) {
		return;
	}

	const auto size = _channels[0].geometry.height();
	const auto st = context.st;
	const auto stm = context.messageStyle();
	const auto selected = context.selected();
	const auto padding = st::chatGiveawayChannelPadding;
	for (const auto &channel : _channels) {
		const auto &thumbnail = channel.thumbnail;
		const auto &geometry = channel.geometry;
		if (!_subscribedToThumbnails) {
			thumbnail->subscribeToUpdates([view = parent()] {
				view->history()->owner().requestViewRepaint(view);
			});
		}

		const auto colorIndex = channel.colorIndex;
		const auto cache = context.outbg
			? stm->replyCache[st->colorPatternIndex(colorIndex)].get()
			: st->coloredReplyCache(selected, colorIndex).get();
		if (channel.corners[0].isNull() || channel.bg != cache->bg) {
			channel.bg = cache->bg;
			channel.corners = Images::CornersMask(size / 2);
			for (auto &image : channel.corners) {
				style::colorizeImage(image, cache->bg, &image);
			}
		}
		p.setPen(cache->icon);
		Ui::DrawRoundedRect(p, geometry, channel.bg, channel.corners);
		if (channel.ripple) {
			channel.ripple->paint(
				p,
				geometry.x(),
				geometry.y(),
				width(),
				&cache->bg2);
			if (channel.ripple->empty()) {
				channel.ripple = nullptr;
			}
		}

		p.drawImage(geometry.topLeft(), thumbnail->image(size));
		const auto left = size + padding.left();
		const auto top = padding.top();
		const auto available = geometry.width() - left - padding.right();
		channel.name.draw(p, {
			.position = { geometry.left() + left, geometry.top() + top },
			.outerWidth = width(),
			.availableWidth = available,
			.align = style::al_left,
			.palette = &stm->textPalette,
			.now = context.now,
			.elisionLines = 1,
			.elisionBreakEverywhere = true,
		});
	}
	_subscribedToThumbnails = 1;
}

void Giveaway::ensureStickerCreated() const {
	if (_sticker) {
		return;
	}
	const auto &session = _parent->history()->session();
	auto &packs = session.giftBoxStickersPacks();
	if (const auto document = packs.lookup(_months)) {
		if (const auto sticker = document->sticker()) {
			const auto skipPremiumEffect = false;
			_sticker.emplace(_parent, document, skipPremiumEffect, _parent);
			_sticker->setDiceIndex(sticker->alt, 1);
			_sticker->setGiftBoxSticker(true);
			_sticker->initSize();
		}
	}
}

void Giveaway::validateBadge(const PaintContext &context) const {
	const auto stm = context.messageStyle();
	const auto &badgeFg = stm->historyFileRadialFg->c;
	const auto &badgeBorder = stm->msgBg->c;
	if (!_badge.isNull()
		&& _badgeFg == badgeFg
		&& _badgeBorder == badgeBorder) {
		return;
	}
	const auto &font = st::chatGiveawayBadgeFont;
	_badgeFg = badgeFg;
	_badgeBorder = badgeBorder;
	const auto text = tr::lng_prizes_badge(
		tr::now,
		lt_amount,
		QString::number(_quantity));
	const auto width = font->width(text);
	const auto inner = QRect(0, 0, width, font->height);
	const auto rect = inner.marginsAdded(st::chatGiveawayBadgePadding);
	const auto size = rect.size();
	const auto ratio = style::DevicePixelRatio();
	_badge = QImage(size * ratio, QImage::Format_ARGB32_Premultiplied);
	_badge.setDevicePixelRatio(ratio);
	_badge.fill(Qt::transparent);

	auto p = QPainter(&_badge);
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(QPen(_badgeBorder, st::chatGiveawayBadgeStroke * 1.));
	p.setBrush(Qt::NoBrush);
	const auto half = st::chatGiveawayBadgeStroke / 2.;
	const auto smaller = QRectF(
		rect.translated(-rect.topLeft())
	).marginsRemoved({ half, half, half, half });
	const auto radius = smaller.height() / 2.;
	p.drawRoundedRect(smaller, radius, radius);
	p.setPen(_badgeFg);
	p.setFont(font);
	p.drawText(
		st::chatGiveawayBadgePadding.left(),
		st::chatGiveawayBadgePadding.top() + font->ascent,
		text);
}

TextState Giveaway::textState(QPoint point, StateRequest request) const {
	auto result = TextState(_parent);

	if (width() < st::msgPadding.left() + st::msgPadding.right() + 1) {
		return result;
	}

	for (const auto &channel : _channels) {
		if (channel.geometry.contains(point)) {
			result.link = channel.link;
			_lastPoint = point;
			return result;
		}
	}
	return result;
}

void Giveaway::clickHandlerActiveChanged(
		const ClickHandlerPtr &p,
		bool active) {
}

void Giveaway::clickHandlerPressedChanged(
		const ClickHandlerPtr &p,
		bool pressed) {
	for (auto &channel : _channels) {
		if (channel.link != p) {
			continue;
		}
		if (pressed) {
			if (!channel.ripple) {
				const auto owner = &parent()->history()->owner();
				channel.ripple = std::make_unique<Ui::RippleAnimation>(
					st::defaultRippleAnimation,
					Ui::RippleAnimation::RoundRectMask(
						channel.geometry.size(),
						channel.geometry.height() / 2),
					[=] { owner->requestViewRepaint(parent()); });
			}
			channel.ripple->add(_lastPoint - channel.geometry.topLeft());
		} else if (channel.ripple) {
			channel.ripple->lastStop();
		}
		break;
	}
}

bool Giveaway::hideFromName() const {
	return !parent()->data()->Has<HistoryMessageForwarded>();
}

bool Giveaway::hasHeavyPart() const {
	return _subscribedToThumbnails;
}

void Giveaway::unloadHeavyPart() {
	if (_subscribedToThumbnails) {
		_subscribedToThumbnails = 0;
		for (const auto &channel : _channels) {
			channel.thumbnail->subscribeToUpdates(nullptr);
		}
	}
}

QMargins Giveaway::inBubblePadding() const {
	auto lshift = st::msgPadding.left();
	auto rshift = st::msgPadding.right();
	auto bshift = isBubbleBottom() ? st::msgPadding.top() : st::mediaInBubbleSkip;
	auto tshift = isBubbleTop() ? st::msgPadding.bottom() : st::mediaInBubbleSkip;
	return QMargins(lshift, tshift, rshift, bshift);
}

} // namespace HistoryView
