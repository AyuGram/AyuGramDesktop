// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_ayu_utils.h"

#include "lang_auto.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/boxes/confirm_box.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QGraphicsOpacityEffect>

class PainterHighQualityEnabler;

namespace Settings {

void ShowRestartPrompt(not_null<Window::SessionController*> controller) {
	crl::on_main([=] {
		controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_settings_need_restart(),
			.confirmed = [] { Core::Restart(); },
			.confirmText = tr::lng_settings_restart_now(),
			.cancelText = tr::lng_settings_restart_later(),
		}));
	});
}

void AddBetaBadge(not_null<Button*> parent) {
	const auto badge = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
		parent.get(),
		object_ptr<Ui::FlatLabel>(
			parent,
			rpl::single(QString("BETA")),
			st::settingsPremiumNewBadge),
		st::ayuBetaBadgePadding);
	badge->show();
	badge->setAttribute(Qt::WA_TransparentForMouseEvents);
	badge->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(badge);
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowBgActive);
		const auto r = st::ayuBetaBadgePadding.left();
		p.drawRoundedRect(badge->rect(), r, r);
	}, badge->lifetime());

	parent->sizeValue(
	) | rpl::on_next([=](QSize size) {
		const auto &st = parent->st();
		badge->moveToLeft(
			st.padding.left()
				+ parent->fullTextWidth()
				+ st::settingsPremiumNewBadgePosition.x(),
			st.padding.top()
				+ (st.style.font->height - badge->height()) / 2);
	}, badge->lifetime());
}

not_null<Ui::RpWidget*> AddInnerToggle(not_null<Ui::VerticalLayout*> container,
									   const style::SettingsButton &st,
									   std::vector<not_null<Ui::AbstractCheckView*>> innerCheckViews,
									   not_null<Ui::SlideWrap<>*> wrap,
									   rpl::producer<QString> buttonLabel,
									   bool toggledWhenAll,
									   std::vector<Fn<bool()>> lockChecks,
									   rpl::event_stream<> *lockChanges) {
	const auto button = container->add(object_ptr<Ui::SettingsButton>(
		container,
		nullptr,
		st::settingsButtonNoIcon));

	const auto toggleButton = Ui::CreateChild<Ui::SettingsButton>(
		container.get(),
		nullptr,
		st);

	struct State final
	{
		State(const style::Toggle &st, Fn<void()> c)
			: checkView(st, false, c) {
		}

		Ui::ToggleView checkView;
		Ui::Animations::Simple animation;
		rpl::event_stream<> anyChanges;
		std::vector<not_null<Ui::AbstractCheckView*>> innerChecks;
		std::vector<Fn<bool()>> lockChecks;
	};
	const auto state = button->lifetime().make_state<State>(
		st.toggle,
		[=]
		{
			toggleButton->update();
		});
	state->innerChecks = std::move(innerCheckViews);
	state->lockChecks = std::move(lockChecks);
	const auto countChecked = [=]
	{
		return ranges::count_if(
			state->innerChecks,
			[](const auto &v)
			{
				return v->checked();
			});
	};
	const auto countUnlockedChecked = [=]
	{
		auto count = 0;
		for (auto i = 0u; i < state->innerChecks.size(); ++i) {
			if (i < state->lockChecks.size() && state->lockChecks[i] && state->lockChecks[i]()) {
				continue;
			}
			if (state->innerChecks[i]->checked()) {
				++count;
			}
		}
		return count;
	};
	const auto countTotal = [=]
	{
		auto total = static_cast<int>(state->innerChecks.size());
		for (auto i = 0u; i < state->lockChecks.size(); ++i) {
			if (state->lockChecks[i] && state->lockChecks[i]()) {
				--total;
			}
		}
		return total;
	};
	for (const auto &innerCheck : state->innerChecks) {
		innerCheck->checkedChanges(
		) | rpl::to_empty | start_to_stream(
			state->anyChanges,
			button->lifetime());
	}
	if (lockChanges) {
		lockChanges->events(
		) | start_to_stream(
			state->anyChanges,
			button->lifetime());
	}
	const auto checkView = &state->checkView;
	{
		const auto separator = Ui::CreateChild<Ui::RpWidget>(container.get());
		separator->paintRequest(
		) | on_next([=, bg = st.textBgOver]
							{
								auto p = QPainter(separator);
								p.fillRect(separator->rect(), bg);
							},
							separator->lifetime());
		const auto separatorHeight = 2 * st.toggle.border
			+ st.toggle.diameter;
		button->geometryValue(
		) | on_next([=](const QRect &r)
							{
								const auto w = st::rightsButtonToggleWidth;
								constexpr auto kLineWidth = 1;
								toggleButton->setGeometry(
									r.x() + r.width() - w,
									r.y(),
									w,
									r.height());
								separator->setGeometry(
									toggleButton->x() - kLineWidth,
									r.y() + (r.height() - separatorHeight) / 2,
									kLineWidth,
									separatorHeight);
							},
							toggleButton->lifetime());

		const auto checkWidget = Ui::CreateChild<Ui::RpWidget>(toggleButton);
		checkWidget->resize(checkView->getSize());
		checkWidget->paintRequest(
		) | on_next([=]
							{
								auto p = QPainter(checkWidget);
								checkView->paint(p, 0, 0, checkWidget->width());
							},
							checkWidget->lifetime());
		toggleButton->sizeValue(
		) | on_next([=](const QSize &s)
							{
								checkWidget->moveToRight(
									st.toggleSkip,
									(s.height() - checkWidget->height()) / 2);
							},
							toggleButton->lifetime());
	}

	const auto totalInnerChecks = state->innerChecks.size();

	state->anyChanges.events_starting_with(
		rpl::empty_value()
	) | rpl::map(countUnlockedChecked) | on_next([=](int count)
												 {
													 const auto total = countTotal();
													 if (toggledWhenAll) {
														 checkView->setChecked(total > 0 && count == total,
																			   anim::type::normal);
													 } else {
														 checkView->setChecked(count != 0,
																			   anim::type::normal);
													 }
												 },
												 toggleButton->lifetime());
	checkView->setLocked(false);
	checkView->finishAnimating();

	const auto label = Ui::CreateChild<Ui::FlatLabel>(
		button,
		combine(
			std::move(buttonLabel),
			state->anyChanges.events_starting_with(
				rpl::empty_value()
			) | rpl::map(countChecked)
		) | rpl::map([=](const QString &t, int checked)
		{
			auto count = tr::bold("  "
				+ QString::number(checked)
				+ '/'
				+ QString::number(totalInnerChecks));
			return TextWithEntities::Simple(t).append(std::move(count));
		}),
		st::boxLabel);
	label->setAttribute(Qt::WA_TransparentForMouseEvents);
	const auto arrow = Ui::CreateChild<Ui::RpWidget>(button);
	{
		const auto &icon = st::permissionsExpandIcon;
		arrow->resize(icon.size());
		arrow->paintRequest(
		) | on_next([=, &icon]
							{
								auto p = QPainter(arrow);
								const auto center = QPointF(
									icon.width() / 2.,
									icon.height() / 2.);
								const auto progress = state->animation.value(
									wrap->toggled() ? 1. : 0.);
								auto hq = std::optional<PainterHighQualityEnabler>();
								if (progress > 0.) {
									hq.emplace(p);
									p.translate(center);
									p.rotate(progress * 180.);
									p.translate(-center);
								}
								icon.paint(p, 0, 0, arrow->width());
							},
							arrow->lifetime());
	}
	button->sizeValue(
	) | on_next([=, &st](const QSize &s)
						{
							const auto labelLeft = st.padding.left();
							const auto labelRight = s.width() - toggleButton->width();

							label->resizeToWidth(labelRight - labelLeft - arrow->width());
							label->moveToLeft(
								labelLeft,
								(s.height() - label->height()) / 2);
							arrow->moveToLeft(
								std::min(
									labelLeft + label->naturalWidth(),
									labelRight - arrow->width()),
								(s.height() - arrow->height()) / 2);
						},
						button->lifetime());
	wrap->toggledValue(
	) | rpl::skip(1) | on_next([=](bool toggled)
									   {
										   state->animation.start(
											   [=]
											   {
												   arrow->update();
											   },
											   toggled ? 0. : 1.,
											   toggled ? 1. : 0.,
											   st::slideWrapDuration,
											   anim::easeOutCubic);
									   },
									   button->lifetime());
	wrap->ease = anim::easeOutCubic;

	button->clicks(
	) | on_next([=]
						{
							wrap->toggle(!wrap->toggled(), anim::type::normal);
						},
						button->lifetime());

	toggleButton->clicks(
	) | on_next([=]
						{
							const auto checked = !checkView->checked();
							for (auto i = 0u; i < state->innerChecks.size(); ++i) {
								if (i < state->lockChecks.size() && state->lockChecks[i] && state->lockChecks[i]()) {
									continue;
								}
								state->innerChecks[i]->setChecked(checked, anim::type::normal);
							}
						},
						toggleButton->lifetime());

	return button;
}

CollapsibleToggleResult AddCollapsibleToggle(not_null<Ui::VerticalLayout*> container,
						  rpl::producer<QString> title,
						  std::vector<NestedEntry> checkboxes,
						  bool toggledWhenAll) {
	struct CheckboxEntry {
		not_null<Ui::AbstractCheckView*> checkView;
		Ui::Checkbox *checkbox = nullptr;
		Ui::RippleButton *button = nullptr;
	};

	struct CollapsibleState {
		std::vector<CheckboxEntry> entries;
		std::vector<NestedEntry> checkboxes;
		rpl::event_stream<> lockChanges;
	};
	const auto cState = container->lifetime().make_state<CollapsibleState>();
	cState->checkboxes = std::move(checkboxes);

	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		container,
		object_ptr<Ui::VerticalLayout>(container));
	const auto verticalLayout = wrap->entity();
	auto innerChecks = std::vector<not_null<Ui::AbstractCheckView*>>();
	auto lockChecks = std::vector<Fn<bool()>>();

	const auto hasAnyLock = ranges::any_of(cState->checkboxes, [](const NestedEntry &e) {
		return e.lockGetter != nullptr;
	});

	for (auto i = 0u; i < cState->checkboxes.size(); ++i) {
		const auto &entry = cState->checkboxes[i];
		const auto checkbox = verticalLayout->add(
			object_ptr<Ui::Checkbox>(
				verticalLayout,
				entry.checkboxLabel,
				entry.getter(),
				st::settingsCheckbox),
			st::powerSavingButton.padding);
		const auto button = Ui::CreateChild<Ui::RippleButton>(
			verticalLayout,
			st::defaultRippleAnimation);
		button->stackUnder(checkbox);
		combine(
			verticalLayout->widthValue(),
			checkbox->geometryValue()
		) | on_next([=](int w, const QRect &r)
							{
								button->setGeometry(0, r.y(), w, r.height());
							},
							button->lifetime());
		checkbox->setAttribute(Qt::WA_TransparentForMouseEvents);
		const auto checkView = checkbox->checkView();

		const auto idx = i;
		button->setClickedCallback([=]
		{
			const auto &e = cState->checkboxes[idx];
			if (e.lockGetter && e.lockSetter) {
				if (button->clickModifiers() & Qt::ShiftModifier) {
					const auto currentlyLocked = e.lockGetter();
					if (!currentlyLocked) {
						// Count already locked entries, deny if would lock all.
						auto lockedCount = 0;
						for (const auto &ce : cState->checkboxes) {
							if (ce.lockGetter && ce.lockGetter()) {
								++lockedCount;
							}
						}
						if (lockedCount + 1 >= static_cast<int>(cState->checkboxes.size())) {
							return;
						}
					}
					e.lockSetter(!currentlyLocked);
					// Update opacity.
					const auto &ce = cState->entries[idx];
					if (!currentlyLocked) {
						auto *effect = new QGraphicsOpacityEffect(ce.checkbox);
						effect->setOpacity(0.4);
						ce.checkbox->setGraphicsEffect(effect);
					} else {
						ce.checkbox->setGraphicsEffect(nullptr);
					}
					cState->lockChanges.fire({});
					return;
				}
				// Normal click on locked entry: ignore.
				if (e.lockGetter()) {
					return;
				}
			}
			checkView->setChecked(
				!checkView->checked(),
				anim::type::normal);
		});

		checkView->checkedChanges(
		) | on_next([=](bool checked)
							{
							},
							verticalLayout->lifetime());

		checkView->checkedValue(
		) | on_next([=](bool enabled)
							{
								cState->checkboxes[idx].setter(enabled);
							},
							container->lifetime());

		cState->entries.push_back(CheckboxEntry{ checkView, checkbox, button });
		innerChecks.push_back(checkView);

		if (hasAnyLock) {
			lockChecks.push_back(entry.lockGetter
				? entry.lockGetter
				: Fn<bool()>(nullptr));
		}
	}

	// Apply initial lock visuals.
	for (auto i = 0u; i < cState->entries.size(); ++i) {
		const auto &entry = cState->checkboxes[i];
		if (entry.lockGetter && entry.lockGetter()) {
			auto *effect = new QGraphicsOpacityEffect(cState->entries[i].checkbox);
			effect->setOpacity(0.4);
			cState->entries[i].checkbox->setGraphicsEffect(effect);
		}
	}

	const auto raw = wrap.data();
	raw->hide(anim::type::instant);
	const auto toggleWidget = AddInnerToggle(
		container,
		st::powerSavingButtonNoIcon,
		innerChecks,
		raw,
		std::move(title),
		toggledWhenAll,
		std::move(lockChecks),
		&cState->lockChanges);
	container->add(std::move(wrap));
	container->widthValue(
	) | on_next([=](int w)
						{
							raw->resizeToWidth(w);
						},
						raw->lifetime());

	return {
		.refresh = [cState] {
			for (auto i = 0u; i < cState->entries.size(); ++i) {
				cState->entries[i].checkView->setChecked(
					cState->checkboxes[i].getter(), anim::type::normal);
				// Refresh lock visuals.
				const auto &entry = cState->checkboxes[i];
				if (entry.lockGetter) {
					if (entry.lockGetter()) {
						auto *effect = new QGraphicsOpacityEffect(cState->entries[i].checkbox);
						effect->setOpacity(0.4);
						cState->entries[i].checkbox->setGraphicsEffect(effect);
					} else {
						cState->entries[i].checkbox->setGraphicsEffect(nullptr);
					}
				}
			}
		},
		.widget = toggleWidget,
	};
}

void AddChooseButtonWithIconAndRightTextInner(not_null<Ui::VerticalLayout*> container,
											  not_null<Window::SessionController*> controller,
											  int initialState,
											  std::vector<QString> options,
											  rpl::producer<QString> text,
											  rpl::producer<QString> boxTitle,
											  const style::SettingsButton &st,
											  Settings::IconDescriptor &&descriptor,
											  const Fn<void(int)> &setter) {
	auto reactiveVal = container->lifetime().make_state<rpl::variable<int>>(initialState);

	rpl::producer<QString> rightTextReactive = reactiveVal->value() | rpl::map(
		[=](int val)
		{
			return options[val];
		});

	Settings::AddButtonWithLabel(
		container,
		std::move(text),
		rightTextReactive,
		st,
		std::move(descriptor))->addClickHandler(
		[=]
		{
			controller->show(Box(
				[=](not_null<Ui::GenericBox*> box)
				{
					const auto save = [=](int index) mutable
					{
						setter(index);

						reactiveVal->force_assign(index);
					};
					SingleChoiceBox(box,
									{
										.title = boxTitle,
										.options = options,
										.initialSelection = reactiveVal->current(),
										.callback = save,
									});
				}));
		});
}

void AddChooseButtonWithIconAndRightText(not_null<Ui::VerticalLayout*> container,
										 not_null<Window::SessionController*> controller,
										 int initialState,
										 std::vector<QString> options,
										 rpl::producer<QString> text,
										 rpl::producer<QString> boxTitle,
										 const style::icon &icon,
										 const Fn<void(int)> &setter) {
	AddChooseButtonWithIconAndRightTextInner(
		container,
		controller,
		initialState,
		options,
		std::move(text),
		std::move(boxTitle),
		st::settingsButton,
		{&icon},
		setter);
}

void AddChooseButtonWithIconAndRightText(not_null<Ui::VerticalLayout*> container,
										 not_null<Window::SessionController*> controller,
										 int initialState,
										 std::vector<QString> options,
										 rpl::producer<QString> text,
										 rpl::producer<QString> boxTitle,
										 const Fn<void(int)> &setter) {
	AddChooseButtonWithIconAndRightTextInner(
		container,
		controller,
		initialState,
		options,
		std::move(text),
		std::move(boxTitle),
		st::settingsButtonNoIcon,
		{},
		setter);
}

not_null<Button*> AddToggleInner(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		Fn<bool()> getter,
		Fn<void(bool)> setter,
		const style::SettingsButton &st,
		Settings::IconDescriptor &&descriptor) {
	const auto button = AddButtonWithIcon(
		container,
		std::move(text),
		st,
		std::move(descriptor));
	button->toggleOn(
		rpl::single(getter())
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != getter());
		}) | rpl::on_next(
		[=](bool enabled)
		{
			setter(enabled);
		},
		container->lifetime());
	return button;
}

not_null<Button*> AddToggle(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		Fn<bool()> getter,
		Fn<void(bool)> setter) {
	return AddToggleInner(
		container,
		std::move(text),
		std::move(getter),
		std::move(setter),
		st::settingsButtonNoIcon,
		{});
}

not_null<Button*> AddToggle(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		Fn<bool()> getter,
		Fn<void(bool)> setter,
		const style::icon &icon) {
	return AddToggleInner(
		container,
		std::move(text),
		std::move(getter),
		std::move(setter),
		st::settingsButton,
		{&icon});
}

void AddSectionDivider(not_null<Ui::VerticalLayout*> container) {
	AddSkip(container);
	AddDivider(container);
	AddSkip(container);
}

not_null<Button*> AddSettingToggle(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		BoolGetter getter,
		BoolSetter setter) {
	return AddToggle(
		container,
		std::move(text),
		[getter] { return (AyuSettings::getInstance().*getter)(); },
		[setter](bool v) { (AyuSettings::getInstance().*setter)(v); });
}

not_null<Button*> AddSettingToggle(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		BoolGetter getter,
		BoolSetter setter,
		const style::icon &icon) {
	return AddToggle(
		container,
		std::move(text),
		[getter] { return (AyuSettings::getInstance().*getter)(); },
		[setter](bool v) { (AyuSettings::getInstance().*setter)(v); },
		icon);
}

} // namespace Settings
