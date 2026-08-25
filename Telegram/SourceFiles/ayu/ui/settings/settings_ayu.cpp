// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_ayu.h"

#include "lang_auto.h"
#include "base/event_filter.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/ayu_userpic.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "boxes/peer_list_box.h"
#include "core/application.h"
#include "data/data_user.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_ayu_styles.h"
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
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "ayu/features/stt/download_helper.h"
#include "ayu/features/stt/ggml_backend_detector.h"
#include "ayu/features/stt/ggml_backend_manager.h"
#include "ayu/features/stt/stt_manager.h"
#include "ayu/features/stt/whisper_service.h"

#include <QtCore/QFile>

#include "range/v3/algorithm/find.hpp"
#include "rpl/combine.h"

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

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
				st::ayuGhostModeGlobalIcon.instance(st::historyPeerUserpicFg->c));
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
		? tr::ayu_GhostModeGlobalSettings(tr::now)
		: GetAccountName(userId);
}

void selectGhostProfile(GhostPickerState *state, uint64 userId) {
	if (state->selectedUserId.current() == userId) {
		return;
	}

	auto wasGlobal = (state->selectedUserId.current() == 0);
	auto nowGlobal = (userId == 0);

	AyuSettings::getInstance().setUseGlobalGhostMode(nowGlobal);

	state->selectedUserId = userId;

	state->pickerButton->setText(PickerLabel(userId));

	state->refreshCheckboxes();

	if (wasGlobal != nowGlobal) {
		Ui::Toast::Show(nowGlobal
			? tr::ayu_GhostModeSwitchedToGlobalSettings(tr::now)
			: tr::ayu_GhostModeSwitchedToIndividualSettings(tr::now));
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

			if (activeCount <= 1 && !AyuSettings::getInstance().useGlobalGhostMode()) {
				auto userId = controller->session().userId().bare;
				auto &src = AyuSettings::ghost(userId);
				auto &dst = AyuSettings::ghost(0);
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
				AyuSettings::getInstance().setUseGlobalGhostMode(true);
			}

			const auto isGlobal = AyuSettings::getInstance().useGlobalGhostMode();
			auto initialUserId = isGlobal
				? uint64(0)
				: controller->session().userId().bare;

			const auto state = container->lifetime().make_state<GhostPickerState>();
			state->selectedUserId = initialUserId;

			const auto title = AddSubsectionTitle(container, tr::ayu_GhostEssentialsHeader());

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
					tr::ayu_DontReadMessages(tr::now),
					[state] { return !AyuSettings::ghost(state->selectedUserId.current()).sendReadMessages(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendReadMessages(!v); },
					[state] { return AyuSettings::ghost(state->selectedUserId.current()).sendReadMessagesLocked(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendReadMessagesLocked(v); }
				},
				NestedEntry{
					tr::ayu_DontReadStories(tr::now),
					[state] { return !AyuSettings::ghost(state->selectedUserId.current()).sendReadStories(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendReadStories(!v); },
					[state] { return AyuSettings::ghost(state->selectedUserId.current()).sendReadStoriesLocked(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendReadStoriesLocked(v); }
				},
				NestedEntry{
					tr::ayu_DontSendOnlinePackets(tr::now),
					[state] { return !AyuSettings::ghost(state->selectedUserId.current()).sendOnlinePackets(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendOnlinePackets(!v); },
					[state] { return AyuSettings::ghost(state->selectedUserId.current()).sendOnlinePacketsLocked(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendOnlinePacketsLocked(v); }
				},
				NestedEntry{
					tr::ayu_DontSendUploadProgress(tr::now),
					[state] { return !AyuSettings::ghost(state->selectedUserId.current()).sendUploadProgress(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendUploadProgress(!v); },
					[state] { return AyuSettings::ghost(state->selectedUserId.current()).sendUploadProgressLocked(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendUploadProgressLocked(v); }
				},
				NestedEntry{
					tr::ayu_SendOfflinePacketAfterOnline(tr::now),
					[state] { return AyuSettings::ghost(state->selectedUserId.current()).sendOfflinePacketAfterOnline(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendOfflinePacketAfterOnline(v); },
					[state] { return AyuSettings::ghost(state->selectedUserId.current()).sendOfflinePacketAfterOnlineLocked(); },
					[state](bool v) { AyuSettings::ghost(state->selectedUserId.current()).setSendOfflinePacketAfterOnlineLocked(v); }
				},
			};

			auto collapsible = AddCollapsibleToggle(
				container,
				tr::ayu_GhostModeToggle(),
				std::move(checkboxes),
				true,
				tr::ayu_GhostModeOptionShiftDescription(tr::now));
			state->refreshCheckboxes = std::move(collapsible.refresh);
			if (wctx.highlights && collapsible.widget) {
				wctx.highlights->push_back(std::make_pair(
					u"ayu/ghostModeToggle"_q,
					HighlightEntry{ collapsible.widget, {} }));
			}

			const auto markReadButton = AddButtonWithIcon(
				container,
				tr::ayu_MarkReadAfterAction(),
				st::settingsButtonNoIcon
			);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"ayu/markReadAfterAction"_q,
					HighlightEntry{ markReadButton.get(), {} }));
			}
			markReadButton->toggleOn(
				state->selectedUserId.value()
				| rpl::map([](uint64 id) {
					return AyuSettings::ghost(id).markReadAfterActionValue();
				}) | rpl::flatten_latest()
			)->toggledValue(
			) | rpl::filter(
				[=](bool enabled) {
					return enabled != AyuSettings::ghost(state->selectedUserId.current()).markReadAfterAction();
				}
			) | on_next(
				[=](bool enabled) {
					auto &ghost = AyuSettings::ghost(state->selectedUserId.current());
					ghost.setMarkReadAfterAction(enabled);
					if (enabled) {
						ghost.setUseScheduledMessages(false);
					}
				},
				container->lifetime());
			AddSkip(container);
			AddDividerText(container, tr::ayu_MarkReadAfterActionDescription());

			AddSkip(container);
			const auto scheduleButton = AddButtonWithIcon(
				container,
				tr::ayu_UseScheduledMessages(),
				st::settingsButtonNoIcon
			);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"ayu/useScheduledMessages"_q,
					HighlightEntry{ scheduleButton.get(), {} }));
			}
			scheduleButton->toggleOn(
				state->selectedUserId.value()
				| rpl::map([](uint64 id) {
					return AyuSettings::ghost(id).useScheduledMessagesValue();
				}) | rpl::flatten_latest()
			)->toggledValue(
			) | rpl::filter(
				[=](bool enabled) {
					return enabled != AyuSettings::ghost(state->selectedUserId.current()).useScheduledMessages();
				}
			) | on_next(
				[=](bool enabled) {
					auto &ghost = AyuSettings::ghost(state->selectedUserId.current());
					ghost.setUseScheduledMessages(enabled);
					if (enabled) {
						ghost.setMarkReadAfterAction(false);
					}
				},
				container->lifetime());
			AddSkip(container);
			AddDividerText(container, tr::ayu_UseScheduledMessagesDescription());

			AddSkip(container);
			const auto silentOptions = std::vector<QString>{
				tr::ayu_SendWithoutSoundByDefaultNever(tr::now),
				tr::ayu_SendWithoutSoundByDefaultInGhostMode(tr::now),
				tr::ayu_SendWithoutSoundByDefaultAlways(tr::now),
			};
			const auto silentOptionText = state->selectedUserId.value(
			) | rpl::map([=](uint64 id) {
				return AyuSettings::ghost(id).sendWithoutSoundValue(
				) | rpl::map([=](SendWithoutSoundOption value) {
					return silentOptions[static_cast<int>(value)];
				});
			}) | rpl::flatten_latest();
			const auto silentButton = AddButtonWithLabel(
				container,
				tr::ayu_SendWithoutSoundByDefault(),
				std::move(silentOptionText),
				st::settingsButtonNoIcon);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"ayu/sendWithoutSound"_q,
					HighlightEntry{ silentButton.get(), {} }));
			}
			silentButton->addClickHandler([=] {
				controller->show(Box([=](not_null<Ui::GenericBox*> box) {
					const auto save = [=](int index) {
						AyuSettings::ghost(state->selectedUserId.current()
						).setSendWithoutSound(
							static_cast<SendWithoutSoundOption>(index));
					};
					SingleChoiceBox(box, {
						.title = tr::ayu_SendWithoutSoundByDefault(),
						.options = silentOptions,
						.initialSelection = static_cast<int>(
							AyuSettings::ghost(state->selectedUserId.current()
							).sendWithoutSound()),
						.callback = save,
					});
				}));
			});
			AddSkip(container);
			AddDividerText(container, tr::ayu_SendWithoutSoundByDefaultDescription());

			AddSkip(container);
			const auto suggestGhostModeButton = AddButtonWithIcon(
				container,
				tr::ayu_SuggestGhostModeBeforeViewingStory(),
				st::settingsButtonNoIcon);
			if (wctx.highlights) {
				wctx.highlights->push_back(std::make_pair(
					u"ayu/suggestGhostModeBeforeViewingStory"_q,
					HighlightEntry{ suggestGhostModeButton.get(), {} }));
			}
			suggestGhostModeButton->toggleOn(
				state->selectedUserId.value()
				| rpl::map([](uint64 id) {
					return AyuSettings::ghost(id).suggestGhostModeBeforeViewingStoryValue();
				}) | rpl::flatten_latest()
			)->toggledValue(
			) | rpl::filter(
				[=](bool enabled) {
					return enabled != AyuSettings::ghost(state->selectedUserId.current()).suggestGhostModeBeforeViewingStory();
				}
			) | on_next(
				[=](bool enabled) {
					AyuSettings::ghost(state->selectedUserId.current()).setSuggestGhostModeBeforeViewingStory(enabled);
				},
				container->lifetime());
			AddSkip(container);
			AddDividerText(container, tr::ayu_SuggestGhostModeBeforeViewingStoryDescription());

			auto showMenu = [=] {
				state->menu = base::make_unique_q<Ui::PopupMenu>(
					pickerButton,
					st::defaultPopupMenu);

				state->menu->addAction(
					base::make_unique_q<GlobalAction>(
						state->menu->menu(),
						st::defaultPopupMenu.menu,
						tr::ayu_GhostModeGlobalSettings(tr::now),
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
				.id = u"ayu/ghostModeToggle"_q,
				.title = tr::ayu_GhostModeToggle(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"ayu/markReadAfterAction"_q,
				.title = tr::ayu_MarkReadAfterAction(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"ayu/useScheduledMessages"_q,
				.title = tr::ayu_UseScheduledMessages(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"ayu/sendWithoutSound"_q,
				.title = tr::ayu_SendWithoutSoundByDefault(tr::now),
				.section = sctx.sectionId,
			});
			sctx.entries->push_back({
				.id = u"ayu/suggestGhostModeBeforeViewingStory"_q,
				.title = tr::ayu_SuggestGhostModeBeforeViewingStory(tr::now),
				.section = sctx.sectionId,
			});
		});
	});
}

void BuildSpyEssentials(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_SpyEssentialsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/saveDeletedMessages"_q,
		.title = tr::ayu_SaveDeletedMessages(),
		.getter = &AyuSettings::saveDeletedMessages,
		.setter = &AyuSettings::setSaveDeletedMessages,
	});
	ayu.addSettingToggle({
		.id = u"ayu/saveMessagesHistory"_q,
		.title = tr::ayu_SaveMessagesHistory(),
		.getter = &AyuSettings::saveMessagesHistory,
		.setter = &AyuSettings::setSaveMessagesHistory,
	});

	ayu.addSectionDivider();

	ayu.addSettingToggle({
		.id = u"ayu/saveForBots"_q,
		.title = tr::ayu_MessageSavingSaveForBots(),
		.getter = &AyuSettings::saveForBots,
		.setter = &AyuSettings::setSaveForBots,
	});
}

void BuildSTT(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	const auto ggmlKind = Ayu::STT::BestAvailableGgmlBackendKind();
	const auto backendProgress = std::make_shared<rpl::variable<int>>(-1);
	const auto startDownloadIfNeeded = [=] {
		if (!ggmlKind
				|| !settings->sttEnabled()
				|| !settings->sttHardwareAcceleration()
				|| backendProgress->current() >= 0
				|| Ayu::STT::GgmlBackendManager::isDownloaded(*ggmlKind)) {
			return;
		}
		LOG(("AyuGram STT: downloading GPU backend, kind=%1"
			).arg(static_cast<int>(*ggmlKind)));
		*backendProgress = 0;
		Ayu::STT::GgmlBackendManager::download(
			*ggmlKind,
			[=](const int percent) {
				*backendProgress = percent;
			},
			[=](const bool ok) {
				LOG(("AyuGram STT: GPU backend download %1, kind=%2"
					).arg(ok ? u"ok"_q : u"failed"_q).arg(static_cast<int>(*ggmlKind)));
				*backendProgress = -1;
			});
	};
	startDownloadIfNeeded();

	builder.addSubsectionTitle(tr::ayu_SttSectionTitle());

	ayu.addToggle({
		.id = u"ayu/sttEnabled"_q,
		.title = tr::ayu_SttEnabled(),
		.getter = [=] { return settings->sttEnabled(); },
		.setter = [=](const bool val) {
			settings->setSttEnabled(val);
			if (val) {
				Ayu::STT::STTManager::requestPermission();
				startDownloadIfNeeded();
			} else {
				Ayu::STT::WhisperService::instance().freeContext(true);
			}
		},
	});

	const auto isEnabled = [=] {
		return AyuSettings::getInstance().sttEnabledValue();
	};

#if defined(Q_OS_MAC)
	const auto isWhisper = [=] {
		return AyuSettings::getInstance().sttEngineValue()
			| rpl::map([](const STTEngine e) { return e == STTEngine::Whisper; });
	};
#else
	const auto isWhisper = [=]() {
		return rpl::single(true);
	};
#endif

	builder.addSkip();
	builder.addDividerText(tr::ayu_SttSectionDescription());

#if defined(Q_OS_MAC)
	const auto engineOptions = std::vector<QString>{
		u"Apple Speech"_q,
		u"Whisper"_q,
	};

	auto currentEngine = AyuSettings::getInstance().sttEngineValue()
		| rpl::map([=](STTEngine val) {
			return engineOptions[static_cast<int>(val)];
		});

	builder.addButton({
		.id = u"ayu/sttEngine"_q,
		.title = tr::ayu_SttEngine(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentEngine),
		.onClick = [=] {
			if (const auto controller = Core::App().activeWindow()->sessionController()) {
				controller->show(Box([=](not_null<Ui::GenericBox*> box) {
					SingleChoiceBox(box, {
						.title = tr::ayu_SttEngine(),
						.options = engineOptions,
						.initialSelection = static_cast<int>(settings->sttEngine()),
						.callback = [=](int index) {
							settings->setSttEngine(static_cast<STTEngine>(index));
							Ayu::STT::STTManager::requestPermission();
							if (static_cast<STTEngine>(index) != STTEngine::Whisper) {
								Ayu::STT::WhisperService::instance().freeContext();
							}
						},
					});
				}));
			}
		},
		.shown = isEnabled(),
	});
#endif

	auto accelerationTitle = rpl::combine(
		tr::ayu_SttHardwareAcceleration(),
		backendProgress->value()
	) | rpl::map([](const QString &title, const int percent) {
		return (percent >= 0)
			? title + u" ("_q + QString::number(percent) + u"%)"_q
			: title;
	});

	ayu.addToggle({
		.id = u"ayu/sttHardwareAcceleration"_q,
		.title = std::move(accelerationTitle),
		// No GPU backend detected - hardware acceleration will not be enabled.
		.getter = [=] { return ggmlKind && settings->sttHardwareAcceleration(); },
		.setter = [=](const bool val) {
			settings->setSttHardwareAcceleration(val);
			Ayu::STT::WhisperService::instance().freeContext(!val);
			if (val) {
				startDownloadIfNeeded();
			}
		},
		.shown = rpl::combine(isEnabled(), isWhisper())
			| rpl::map([](const bool e, const bool w) { return e && w; }),
		.disabled = !ggmlKind,
		.tooltip = ggmlKind
			? QString()
			: tr::ayu_SttHardwareAccelerationUnavailable(tr::now),
	});

	builder.addDividerText(tr::ayu_SttHardwareAccelerationDescription());

	const auto whisperModelOptions = std::vector<QString>{
		u"Tiny (~75 MB)"_q,
		u"Base (~142 MB)"_q,
		u"Small (~466 MB)"_q,
	};

	auto currentModel = AyuSettings::getInstance().whisperModelTypeValue()
		| rpl::map([=](WhisperModel val) {
			return whisperModelOptions[static_cast<int>(val)];
		});

	builder.addButton({
		.id = u"ayu/whisperModel"_q,
		.title = tr::ayu_SttWhisperModel(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentModel),
		.onClick = [=] {
			if (const auto controller = Core::App().activeWindow()->sessionController()) {
				controller->show(Box([=](const not_null<Ui::GenericBox*> box) {
					SingleChoiceBox(box, {
						.title = tr::ayu_SttWhisperModel(),
						.options = whisperModelOptions,
						.initialSelection = static_cast<int>(settings->whisperModelType()),
						.callback = [=](int index) {
							settings->setWhisperModelType(static_cast<WhisperModel>(index));
						},
					});
				}));
			}
		},
		.shown = rpl::combine(isEnabled(), isWhisper())
			| rpl::map([](const bool e, const bool w) { return e && w; }),
	});

	// -1 = idle, 0-100 = downloading progress percent
	const auto downloadProgress = std::make_shared<rpl::variable<int>>(-1);

	auto downloadLabelStream = rpl::combine(
		AyuSettings::getInstance().whisperModelTypeValue(),
		downloadProgress->value()
	) | rpl::map([=](WhisperModel model, const int progress) -> QString {
		if (progress >= 0) {
			return QString::number(progress) + u"%..."_q;
		}

		return Ayu::STT::STTManager::modelExists(static_cast<int>(model))
			? tr::ayu_SttModelDownloaded(tr::now)
			: tr::ayu_SttDownloadModel(tr::now);
	});

	const auto downloadButton = builder.addButton({
		.id = u"ayu/sttDownloadModel"_q,
		.title = tr::ayu_SttModelTitle(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(downloadLabelStream),
		.onClick = [=] {
			if (downloadProgress->current() >= 0) {
				return;
			}

			const auto modelType = static_cast<int>(settings->whisperModelType());
			if (Ayu::STT::STTManager::modelExists(modelType)) {
				if (const auto controller = Core::App().activeWindow()->sessionController()) {
					controller->showToast(tr::ayu_SttModelAlreadyDownloaded(tr::now));
				}
				return;
			}

			const auto url = Ayu::STT::STTManager::modelUrl(modelType);
			const auto dest = Ayu::STT::STTManager::modelPath(modelType);

			*downloadProgress = 0;
			Ayu::STT::DownloadWithProgress(
				url,
				dest,
				QString(), // no known sha256 for these files
				[=](const int percent) {
					*downloadProgress = percent;
				},
				[=](const bool ok) {
					if (const auto controller = Core::App().activeWindow()->sessionController()) {
						controller->showToast(ok
							? tr::ayu_SttDownloadComplete(tr::now)
							: tr::ayu_SttDownloadFailed(tr::now));
					}
					*downloadProgress = -1;
				});
		},
		.shown = rpl::combine(isEnabled(), isWhisper())
			| rpl::map([](bool e, bool w) { return e && w; }),
	});

	// AyuGram: right-click the model row to delete the downloaded file.
	const auto modelContextMenu = downloadButton->lifetime()
		.make_state<base::unique_qptr<Ui::PopupMenu>>();
	const auto showModelContextMenu = [=] {
		if (downloadProgress->current() >= 0) {
			return false;
		}
		const auto modelType = static_cast<int>(settings->whisperModelType());
		if (!Ayu::STT::STTManager::modelExists(modelType)) {
			return false;
		}
		*modelContextMenu = base::make_unique_q<Ui::PopupMenu>(
			downloadButton,
			st::popupMenuWithIcons);
		modelContextMenu->get()->addAction(
			tr::lng_selected_delete(tr::now),
			[=] {
				Ayu::STT::WhisperService::instance().freeContext();
				QFile::remove(Ayu::STT::STTManager::modelPath(modelType));
				*downloadProgress = 0;
				*downloadProgress = -1;
			},
			&st::menuIconDelete);
		modelContextMenu->get()->popup(QCursor::pos());
		return true;
	};
	base::install_event_filter(downloadButton, [=](not_null<QEvent*> e) {
		if (e->type() == QEvent::ContextMenu && showModelContextMenu()) {
			return base::EventFilterResult::Cancel;
		}
		return base::EventFilterResult::Continue;
	});

	const auto langOptions = std::vector<std::pair<QString, QString>>{
		{u"auto"_q, tr::ayu_SttLanguageAuto(tr::now)},
		{u"en"_q,   u"English"_q},
		{u"zh"_q,   u"中文"_q},
		{u"hi"_q,   u"हिन्दी"_q},
		{u"es"_q,   u"Español"_q},
		{u"fr"_q,   u"Français"_q},
		{u"ar"_q,   u"العربية"_q},
		{u"ru"_q,   u"Русский"_q},
		{u"pt"_q,   u"Português"_q},
		{u"id"_q,   u"Indonesia"_q},
		{u"de"_q,   u"Deutsch"_q},
		{u"ja"_q,   u"日本語"_q},
		{u"tr"_q,   u"Türkçe"_q},
		{u"ko"_q,   u"한국어"_q},
		{u"vi"_q,   u"Tiếng Việt"_q},
		{u"it"_q,   u"Italiano"_q},
		{u"fa"_q,   u"فارسی"_q},
		{u"uk"_q,   u"Українська"_q},
		{u"nl"_q,   u"Nederlands"_q},
		{u"el"_q,   u"Ελληνικά"_q},
		{u"he"_q,   u"עברית"_q},
	};
	auto langLabels = std::vector<QString>();
	langLabels.reserve(langOptions.size());
	for (const auto &val: langOptions | std::views::values) {
		langLabels.push_back(val);
	}

	const auto getLangIndex = [=](const QString &lang) {
		const auto it = ranges::find(langOptions, lang, &std::pair<QString, QString>::first);
		return (it != end(langOptions)) ? static_cast<int>(it - begin(langOptions)) : 0;
	};

	const auto autoLabel = [=]() -> QString {
#if defined(Q_OS_MAC)
		return settings->sttEngine() == STTEngine::AppleSpeech
			? tr::ayu_SttLanguageSystem(tr::now)
			: tr::ayu_SttLanguageAuto(tr::now);
#else
		return tr::ayu_SttLanguageAuto(tr::now);
#endif
	};

	auto currentLang = rpl::combine(
		AyuSettings::getInstance().sttLanguageValue(),
		AyuSettings::getInstance().sttEngineValue()
	) | rpl::map([=](const QString &lang, STTEngine) {
		const auto idx = getLangIndex(lang);
		return idx == 0 ? autoLabel() : langOptions[idx].second;
	});

	builder.addButton({
		.id = u"ayu/sttLanguage"_q,
		.title = tr::ayu_SttLanguage(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentLang),
		.onClick = [=] {
			if (const auto controller = Core::App().activeWindow()->sessionController()) {
				controller->show(Box([=](const not_null<Ui::GenericBox*> box) {
					auto labels = langLabels;
					labels[0] = autoLabel();
					SingleChoiceBox(box, {
						.title = tr::ayu_SttLanguage(),
						.options = labels,
						.initialSelection = getLangIndex(settings->sttLanguage()),
						.callback = [=](const int index) {
							settings->setSttLanguage(langOptions[index].first);
						},
					});
				}));
			}
		},
		.shown = isEnabled(),
	});
}

void BuildOther(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_MessageSavingOtherHeader());

	ayu.addSettingToggle({
		.id = u"ayu/localPremium"_q,
		.title = tr::ayu_LocalPremium(),
		.getter = &AyuSettings::localPremium,
		.setter = &AyuSettings::setLocalPremium,
	});
	ayu.addSettingToggle({
		.id = u"ayu/disableAds"_q,
		.title = tr::ayu_DisableAds(),
		.getter = &AyuSettings::disableAds,
		.setter = &AyuSettings::setDisableAds,
	});
}

const auto kMeta = BuildHelper({
	.id = AyuGhost::Id(),
	.parentId = AyuMain::Id(),
	.title = u"AyuGram"_q,
	.icon = &st::menuIconGroupReactions,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildGhostEssentials(builder);

	builder.addSkip();
	BuildSpyEssentials(builder, ayu);

	ayu.addSectionDivider();
	BuildSTT(builder, ayu);

	ayu.addSectionDivider();
	BuildOther(builder, ayu);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AyuGhost::title() {
	return rpl::single(QString("AyuGram"));
}

AyuGhost::AyuGhost(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller)
, _controller(controller) {
	setupContent();
}

void AyuGhost::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuGhostId() {
	return AyuGhost::Id();
}

} // namespace Settings
