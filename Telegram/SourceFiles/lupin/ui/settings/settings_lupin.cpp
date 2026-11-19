// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/ui/settings/settings_lupin.h"

#include "lang_auto.h"
#include "lupin/lupin_settings.h"
#include "lupin/ui/lupin_userpic.h"
#include "lupin/ui/settings/lupin_builder.h"
#include "lupin/ui/settings/settings_lupin_utils.h"
#include "lupin/ui/settings/settings_main.h"
#include "boxes/peer_list_box.h"
#include "core/application.h"
#include "data/data_user.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_lupin_icons.h"
#include "styles/style_lupin_styles.h"
#include "styles/style_chat.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_window.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/text/text.h"
#include "ui/toast/toast.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/menu/menu_item_base.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

using namespace Builder;
using namespace LupinBuilder;

namespace {

struct GhostPickerState {
	rpl::variable<uint64> selectedUserId;
	base::unique_qptr<Ui::PopupMenu> menu;
	Ui::LinkButton *pickerButton = nullptr;
	Fn<void()> refreshCheckboxes;
};

struct AccountUserpicGeometry {
	QRect outer;
	QRect inner;
};

[[nodiscard]] AccountUserpicGeometry AccountUserpic(int height) {
	const auto line = st::mainMenuAccountLine;
	const auto skip = 2 * line + st::lineWidth;
	const auto full = st::mainMenuAccountSize + 2 * skip;
	const auto outer = QRect(
		st::defaultWhoRead.photoLeft
			+ (st::defaultWhoRead.photoSize - full) / 2,
		(height - full) / 2,
		full,
		full);
	return {
		.outer = outer,
		.inner = QRect(
			outer.x() + skip,
			outer.y() + skip,
			st::mainMenuAccountSize,
			st::mainMenuAccountSize),
	};
}

void PaintAccountOutline(Painter &p, QRect outer) {
	const auto line = st::mainMenuAccountLine;
	const auto shift = st::lineWidth + (line * 0.5);
	const auto rect = QRectF(outer).marginsRemoved(QMarginsF(
		shift,
		shift,
		shift,
		shift));
	auto hq = PainterHighQualityEnabler(p);
	auto pen = st::windowBgActive->p;
	pen.setWidthF(line);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	AyuUserpic::PaintShape(p, rect);
}

class AccountAction final : public Ui::Menu::ItemBase {
public:
	AccountAction(
		not_null<Ui::Menu::Menu*> parent,
		const style::Menu &st,
		UserData *user,
		bool active,
		Fn<void()> callback)
	: ItemBase(parent, st)
	, _dummyAction(Ui::CreateChild<QAction>(parent.get()))
	, _user(user)
	, _active(active)
	, _st(st)
	, _height(st::defaultWhoRead.photoSkip * 2 + st::defaultWhoRead.photoSize) {
		setAcceptBoth(true);
		fitToMenuWidth();

		_text.setText(_st.itemStyle, user->name());
		const auto goodWidth = st::defaultWhoRead.nameLeft
			+ _text.maxWidth()
			+ _st.itemPadding.right();
		setMinWidth(std::clamp(goodWidth, _st.widthMin, _st.widthMax));

		setActionTriggered(std::move(callback));

		paintRequest(
		) | rpl::on_next([=] {
			paint(Painter(this));
		}, lifetime());

		enableMouseSelecting();
	}

	not_null<QAction*> action() const override { return _dummyAction; }
	bool isEnabled() const override { return true; }

protected:
	int contentHeight() const override { return _height; }

private:
	void paint(Painter &&p) {
		const auto selected = isSelected();
		if (selected && _st.itemBgOver->c.alpha() < 255) {
			p.fillRect(0, 0, width(), _height, _st.itemBg);
		}
		const auto bg = selected ? _st.itemBgOver : _st.itemBg;
		p.fillRect(0, 0, width(), _height, bg);
		if (isEnabled()) {
			paintRipple(p, 0, 0);
		}

		const auto userpic = AccountUserpic(_height);
		_user->paintUserpicLeft(
			p,
			_userpicView,
			userpic.inner.x(),
			userpic.inner.y(),
			width(),
			userpic.inner.width());
		if (_active) {
			PaintAccountOutline(p, userpic.outer);
		}

		p.setPen(selected ? _st.itemFgOver : _st.itemFg);
		_text.drawLeftElided(
			p,
			st::defaultWhoRead.nameLeft,
			(_height - _st.itemStyle.font->height) / 2,
			width() - st::defaultWhoRead.nameLeft - _st.itemPadding.right(),
			width());
	}

	const not_null<QAction*> _dummyAction;
	UserData *_user = nullptr;
	mutable Ui::PeerUserpicView _userpicView;
	const bool _active = false;
	const style::Menu &_st;
	Ui::Text::String _text;
	const int _height;
};

class GlobalAction final : public Ui::Menu::ItemBase {
public:
	GlobalAction(
		not_null<Ui::Menu::Menu*> parent,
		const style::Menu &st,
		const QString &text,
		bool active,
		Fn<void()> callback)
	: ItemBase(parent, st)
	, _dummyAction(Ui::CreateChild<QAction>(parent.get()))
	, _active(active)
	, _st(st)
	, _height(st::defaultWhoRead.photoSkip * 2 + st::defaultWhoRead.photoSize) {
		setAcceptBoth(true);
		fitToMenuWidth();

		_text.setText(_st.itemStyle, text);
		const auto goodWidth = st::defaultWhoRead.nameLeft
			+ _text.maxWidth()
			+ _st.itemPadding.right();
		setMinWidth(std::clamp(goodWidth, _st.widthMin, _st.widthMax));

		setActionTriggered(std::move(callback));

		paintRequest(
		) | rpl::on_next([=] {
			paint(Painter(this));
		}, lifetime());

		enableMouseSelecting();
	}

	not_null<QAction*> action() const override { return _dummyAction; }
	bool isEnabled() const override { return true; }

protected:
	int contentHeight() const override { return _height; }

private:
	void paint(Painter &&p) {
		const auto selected = isSelected();
		if (selected && _st.itemBgOver->c.alpha() < 255) {
			p.fillRect(0, 0, width(), _height, _st.itemBg);
		}
		const auto bg = selected ? _st.itemBgOver : _st.itemBg;
		p.fillRect(0, 0, width(), _height, bg);
		if (isEnabled()) {
			paintRipple(p, 0, 0);
		}

		const auto userpic = AccountUserpic(_height);
		{
			auto hq = PainterHighQualityEnabler(p);
			auto rect = QRectF(userpic.inner);
			auto gradient = QLinearGradient(rect.topLeft(), rect.bottomLeft());
			gradient.setStops({
				{ 0., st::historyPeer5UserpicBg->c },
				{ 1., st::historyPeer5UserpicBg2->c },
			});
			p.setPen(Qt::NoPen);
			p.setBrush(gradient);
			AyuUserpic::PaintShape(p, rect);
		}
		{
			auto hq = PainterHighQualityEnabler(p);
			p.drawImage(
				userpic.inner,
				st::lupinGhostModeGlobalIcon.instance(st::historyPeerUserpicFg->c));
		}
		if (_active) {
			PaintAccountOutline(p, userpic.outer);
		}

		p.setPen(selected ? _st.itemFgOver : _st.itemFg);
		_text.drawLeftElided(
			p,
			st::defaultWhoRead.nameLeft,
			(_height - _st.itemStyle.font->height) / 2,
			width() - st::defaultWhoRead.nameLeft - _st.itemPadding.right(),
			width());
	}

	const not_null<QAction*> _dummyAction;
	const bool _active = false;
	const style::Menu &_st;
	Ui::Text::String _text;
	const int _height;
};

QString GetAccountName(uint64 userId) {
	for (const auto &account : Core::App().domain().orderedAccounts()) {
		if (account->sessionExists()
			&& account->session().userId().bare == userId) {
			return account->session().user()->name();
		}
	}
	return QString("Unknown");
}

QString PickerLabel(uint64 userId) {
	return (userId == 0)
		? tr::lupin_GhostModeGlobalSettings(tr::now)
		: GetAccountName(userId);
}

void selectGhostProfile(GhostPickerState *state, uint64 userId) {
	if (state->selectedUserId.current() == userId) {
		return;
	}

	auto wasGlobal = (state->selectedUserId.current() == 0);
	auto nowGlobal = (userId == 0);

	LupinSettings::getInstance().setUseGlobalGhostMode(nowGlobal);

	state->selectedUserId = userId;

	state->pickerButton->setText(PickerLabel(userId));

	state->refreshCheckboxes();

	if (wasGlobal != nowGlobal) {
		Ui::Toast::Show(nowGlobal
			? tr::lupin_GhostModeSwitchedToGlobalSettings(tr::now)
			: tr::lupin_GhostModeSwitchedToIndividualSettings(tr::now));
	}
}

void BuildGhostEssentials(SectionBuilder &builder) {
	builder.add([](const BuildContext &ctx) {
		v::match(ctx, [&](const WidgetContext &wctx) {
			const auto container = wctx.container;
			const auto controller = wctx.controller;

			auto activeCount = 0;
			for (const auto &account : Core::App().domain().orderedAccounts()) {
				if (account->sessionExists()) {
					++activeCount;
				}
			}

			if (activeCount <= 1 && !LupinSettings::getInstance().useGlobalGhostMode()) {
				auto userId = controller->session().userId().bare;
				auto &src = LupinSettings::ghost(userId);
				auto &dst = LupinSettings::ghost(0);
				dst.setSendReadMessages(src.sendReadMessages());
				dst.setSendReadStories(src.sendReadStories());
				dst.setSendOnlinePackets(src.sendOnlinePackets());
				dst.setSendUploadProgress(src.sendUploadProgress());
				dst.setSendOfflinePacketAfterOnline(src.sendOfflinePacketAfterOnline());
				dst.setMarkReadAfterAction(src.markReadAfterAction());
				dst.setUseScheduledMessages(src.useScheduledMessages());
				dst.setSendWithoutSound(src.sendWithoutSound());
				dst.setSuggestGhostModeBeforeViewingStory(src.suggestGhostModeBeforeViewingStory());
				dst.setSendReadMessagesLocked(src.sendReadMessagesLocked());
				dst.setSendReadStoriesLocked(src.sendReadStoriesLocked());
				dst.setSendOnlinePacketsLocked(src.sendOnlinePacketsLocked());
				dst.setSendUploadProgressLocked(src.sendUploadProgressLocked());
				dst.setSendOfflinePacketAfterOnlineLocked(src.sendOfflinePacketAfterOnlineLocked());
				LupinSettings::getInstance().setUseGlobalGhostMode(true);
			}

			const auto isGlobal = LupinSettings::getInstance().useGlobalGhostMode();
			auto initialUserId = isGlobal
				? uint64(0)
				: controller->session().userId().bare;

			const auto state = container->lifetime().make_state<GhostPickerState>();
			state->selectedUserId = initialUserId;

			const auto title = AddSubsectionTitle(container, tr::lupin_GhostEssentialsHeader());

			const auto pickerButton = Ui::CreateChild<Ui::LinkButton>(
				container.get(),
				PickerLabel(initialUserId),
				st::ghostPickerButton);
			state->pickerButton = pickerButton;

			const auto arrow = Ui::CreateChild<Ui::AbstractButton>(container.get());
			{
				const auto &icon = st::ghostPickerArrow;
				arrow->resize(icon.size());
				arrow->paintRequest(
				) | rpl::on_next([=, &icon] {
					auto p = QPainter(arrow);
					icon.paint(p, 0, 0, arrow->width());
				}, arrow->lifetime());
			}
			arrow->setCursor(style::cur_pointer);

			const auto showPicker = activeCount > 1;
			pickerButton->setVisible(showPicker);
			arrow->setVisible(showPicker);

			rpl::combine(
				title->geometryValue(),
				container->widthValue(),
				pickerButton->naturalWidthValue()
			) | rpl::on_next([=](QRect r, int width, int natural) {
				pickerButton->resizeToNaturalWidth(width / 2);
				pickerButton->moveToRight(
					st::defaultSubsectionTitlePadding.right() + arrow->width() + st::normalFont->spacew / 2,
					r.y() + (r.height() - pickerButton->height()) / 2,
					width);
				arrow->moveToLeft(
					pickerButton->x() + pickerButton->width() + st::normalFont->spacew / 2,
					r.y() + (r.height() - arrow->height()) / 2);
			}, pickerButton->lifetime());

			std::vector checkboxes{
				NestedEntry{
					tr::lupin_DontReadMessages(tr::now),
					[state] { return !LupinSettings::ghost(state->selectedUserId.current()).sendReadMessages(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendReadMessages(!v); },
					[state] { return LupinSettings::ghost(state->selectedUserId.current()).sendReadMessagesLocked(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendReadMessagesLocked(v); }
				},
				NestedEntry{
					tr::lupin_DontReadStories(tr::now),
					[state] { return !LupinSettings::ghost(state->selectedUserId.current()).sendReadStories(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendReadStories(!v); },
					[state] { return LupinSettings::ghost(state->selectedUserId.current()).sendReadStoriesLocked(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendReadStoriesLocked(v); }
				},
				NestedEntry{
					tr::lupin_DontSendOnlinePackets(tr::now),
					[state] { return !LupinSettings::ghost(state->selectedUserId.current()).sendOnlinePackets(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendOnlinePackets(!v); },
					[state] { return LupinSettings::ghost(state->selectedUserId.current()).sendOnlinePacketsLocked(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendOnlinePacketsLocked(v); }
				},
				NestedEntry{
					tr::lupin_DontSendUploadProgress(tr::now),
					[state] { return !LupinSettings::ghost(state->selectedUserId.current()).sendUploadProgress(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendUploadProgress(!v); },
					[state] { return LupinSettings::ghost(state->selectedUserId.current()).sendUploadProgressLocked(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendUploadProgressLocked(v); }
				},
				NestedEntry{
					tr::lupin_SendOfflinePacketAfterOnline(tr::now),
					[state] { return LupinSettings::ghost(state->selectedUserId.current()).sendOfflinePacketAfterOnline(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendOfflinePacketAfterOnline(v); },
					[state] { return LupinSettings::ghost(state->selectedUserId.current()).sendOfflinePacketAfterOnlineLocked(); },
					[state](bool v) { LupinSettings::ghost(state->selectedUserId.current()).setSendOfflinePacketAfterOnlineLocked(v); }
				},
			};

			auto collapsible = AddCollapsibleToggle(
				container,
				tr::lupin_GhostModeToggle(),
				std::move(checkboxes),
				true,
				tr::lupin_GhostModeOptionShiftDescription(tr::now));
			state->refreshCheckboxes = std::move(collapsible.refresh);
			if (wctx.highlights && collapsible.widget) {
				wctx.highlights->push_back(std::make_pair(
					u"lupin/ghostModeToggle"_q,
					HighlightEntry{ collapsible.widget, {} }));
			}

			const auto markReadButton = AddButtonWithIcon(
				container,
				tr::lupin_MarkReadAfterAction(),
				st::settingsButtonNoIcon
			);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"lupin/markReadAfterAction"_q,
					HighlightEntry{ markReadButton.get(), {} }));
			}
			markReadButton->toggleOn(
				state->selectedUserId.value()
				| rpl::map([](uint64 id) {
					return LupinSettings::ghost(id).markReadAfterActionValue();
				}) | rpl::flatten_latest()
			)->toggledValue(
			) | rpl::filter(
				[=](bool enabled) {
					return enabled != LupinSettings::ghost(state->selectedUserId.current()).markReadAfterAction();
				}
			) | on_next(
				[=](bool enabled) {
					auto &ghost = LupinSettings::ghost(state->selectedUserId.current());
					ghost.setMarkReadAfterAction(enabled);
					if (enabled) {
						ghost.setUseScheduledMessages(false);
					}
				},
				container->lifetime());
			AddSkip(container);
			AddDividerText(container, tr::lupin_MarkReadAfterActionDescription());

			AddSkip(container);
			const auto scheduleButton = AddButtonWithIcon(
				container,
				tr::lupin_UseScheduledMessages(),
				st::settingsButtonNoIcon
			);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"lupin/useScheduledMessages"_q,
					HighlightEntry{ scheduleButton.get(), {} }));
			}
			scheduleButton->toggleOn(
				state->selectedUserId.value()
				| rpl::map([](uint64 id) {
					return LupinSettings::ghost(id).useScheduledMessagesValue();
				}) | rpl::flatten_latest()
			)->toggledValue(
			) | rpl::filter(
				[=](bool enabled) {
					return enabled != LupinSettings::ghost(state->selectedUserId.current()).useScheduledMessages();
				}
			) | on_next(
				[=](bool enabled) {
					auto &ghost = LupinSettings::ghost(state->selectedUserId.current());
					ghost.setUseScheduledMessages(enabled);
					if (enabled) {
						ghost.setMarkReadAfterAction(false);
					}
				},
				container->lifetime());
			AddSkip(container);
			AddDividerText(container, tr::lupin_UseScheduledMessagesDescription());

			AddSkip(container);
			const auto silentOptions = std::vector<QString>{
				tr::lupin_SendWithoutSoundByDefaultNever(tr::now),
				tr::lupin_SendWithoutSoundByDefaultInGhostMode(tr::now),
				tr::lupin_SendWithoutSoundByDefaultAlways(tr::now),
			};
			const auto silentOptionText = state->selectedUserId.value(
			) | rpl::map([=](uint64 id) {
				return LupinSettings::ghost(id).sendWithoutSoundValue(
				) | rpl::map([=](SendWithoutSoundOption value) {
					return silentOptions[static_cast<int>(value)];
				});
			}) | rpl::flatten_latest();
			const auto silentButton = AddButtonWithLabel(
				container,
				tr::lupin_SendWithoutSoundByDefault(),
				std::move(silentOptionText),
				st::settingsButtonNoIcon);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"lupin/sendWithoutSound"_q,
					HighlightEntry{ silentButton.get(), {} }));
			}
			silentButton->addClickHandler([=] {
				controller->show(Box([=](not_null<Ui::GenericBox*> box) {
					const auto save = [=](int index) {
						LupinSettings::ghost(state->selectedUserId.current()
						).setSendWithoutSound(
							static_cast<SendWithoutSoundOption>(index));
					};
					SingleChoiceBox(box, {
						.title = tr::lupin_SendWithoutSoundByDefault(),
						.options = silentOptions,
						.initialSelection = static_cast<int>(
							LupinSettings::ghost(state->selectedUserId.current()
							).sendWithoutSound()),
						.callback = save,
					});
				}));
			});
			AddSkip(container);
			AddDividerText(container, tr::lupin_SendWithoutSoundByDefaultDescription());

			AddSkip(container);
			const auto suggestGhostModeButton = AddButtonWithIcon(
				container,
				tr::lupin_SuggestGhostModeBeforeViewingStory(),
				st::settingsButtonNoIcon);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"lupin/suggestGhostModeBeforeViewingStory"_q,
					HighlightEntry{ suggestGhostModeButton.get(), {} }));
			}
			suggestGhostModeButton->toggleOn(
				state->selectedUserId.value()
				| rpl::map([](uint64 id) {
					return LupinSettings::ghost(id).suggestGhostModeBeforeViewingStoryValue();
				}) | rpl::flatten_latest()
			)->toggledValue(
			) | rpl::filter(
				[=](bool enabled) {
					return enabled != LupinSettings::ghost(state->selectedUserId.current()).suggestGhostModeBeforeViewingStory();
				}
			) | on_next(
				[=](bool enabled) {
					LupinSettings::ghost(state->selectedUserId.current()).setSuggestGhostModeBeforeViewingStory(enabled);
				},
				container->lifetime());
			AddSkip(container);
			AddDividerText(container, tr::lupin_SuggestGhostModeBeforeViewingStoryDescription());

			auto showMenu = [=] {
				state->menu = base::make_unique_q<Ui::PopupMenu>(
					pickerButton,
					st::defaultPopupMenu);

				state->menu->addAction(
					base::make_unique_q<GlobalAction>(
						state->menu->menu(),
						st::defaultPopupMenu.menu,
						tr::lupin_GhostModeGlobalSettings(tr::now),
						state->selectedUserId.current() == 0,
						[=] { selectGhostProfile(state, 0); }));

				for (const auto &account : Core::App().domain().orderedAccounts()) {
					if (!account->sessionExists()) {
						continue;
					}
					auto user = account->session().user();
					auto id = account->session().userId().bare;
					state->menu->addAction(
						base::make_unique_q<AccountAction>(
							state->menu->menu(),
							st::defaultPopupMenu.menu,
							user,
							state->selectedUserId.current() == id,
							[=] { selectGhostProfile(state, id); }));
				}

				state->menu->popup(
					pickerButton->mapToGlobal(
						QPoint(pickerButton->width(), pickerButton->height())));
			};
			pickerButton->setClickedCallback(showMenu);
			arrow->setClickedCallback(showMenu);
		}, [&](const SearchContext &sctx) {
			sctx.entries->push_back({
				.id = u"lupin/ghostModeToggle"_q,
				.title = tr::lupin_GhostModeToggle(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"lupin/markReadAfterAction"_q,
				.title = tr::lupin_MarkReadAfterAction(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"lupin/useScheduledMessages"_q,
				.title = tr::lupin_UseScheduledMessages(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"lupin/sendWithoutSound"_q,
				.title = tr::lupin_SendWithoutSoundByDefault(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"lupin/suggestGhostModeBeforeViewingStory"_q,
				.title = tr::lupin_SuggestGhostModeBeforeViewingStory(tr::now),
				.section = sctx.sectionId,
			});
		});
	});
}

void BuildSpyEssentials(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lupin_SpyEssentialsHeader());

	lupin.addSettingToggle({
		.id = u"lupin/saveDeletedMessages"_q,
		.title = tr::lupin_SaveDeletedMessages(),
		.getter = &LupinSettings::saveDeletedMessages,
		.setter = &LupinSettings::setSaveDeletedMessages,
	});
	lupin.addSettingToggle({
		.id = u"lupin/saveMessagesHistory"_q,
		.title = tr::lupin_SaveMessagesHistory(),
		.getter = &LupinSettings::saveMessagesHistory,
		.setter = &LupinSettings::setSaveMessagesHistory,
	});

	lupin.addSectionDivider();

	lupin.addSettingToggle({
		.id = u"lupin/saveForBots"_q,
		.title = tr::lupin_MessageSavingSaveForBots(),
		.getter = &LupinSettings::saveForBots,
		.setter = &LupinSettings::setSaveForBots,
	});
}

void BuildOther(SectionBuilder &builder, LupinSectionBuilder &lupin) {
	builder.addSubsectionTitle(tr::lupin_MessageSavingOtherHeader());

	lupin.addSettingToggle({
		.id = u"lupin/localPremium"_q,
		.title = tr::lupin_LocalPremium(),
		.getter = &LupinSettings::localPremium,
		.setter = &LupinSettings::setLocalPremium,
	});
	lupin.addSettingToggle({
		.id = u"lupin/disableAds"_q,
		.title = tr::lupin_DisableAds(),
		.getter = &LupinSettings::disableAds,
		.setter = &LupinSettings::setDisableAds,
	});
}

const auto kMeta = BuildHelper({
	.id = LupinGhost::Id(),
	.parentId = LupinMain::Id(),
	.title = u"LupinGram Settings"_q,
	.icon = &st::menuIconGroupReactions,
}, [](SectionBuilder &builder) {
	auto lupin = LupinSectionBuilder(builder);

	builder.addSkip();
	BuildGhostEssentials(builder);

	builder.addSkip();
	BuildSpyEssentials(builder, lupin);

	lupin.addSectionDivider();
	BuildOther(builder, lupin);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> LupinGhost::title() {
	return rpl::single(QString("LupinGram Settings"));
}

LupinGhost::LupinGhost(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller)
, _controller(controller) {
	setupContent();
}

void LupinGhost::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type LupinGhostId() {
	return LupinGhost::Id();
}

} // namespace Settings
