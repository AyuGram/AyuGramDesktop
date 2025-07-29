// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_ayu_utils.h"

#include "settings/settings_common.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

class PainterHighQualityEnabler;

namespace Settings {

rpl::producer<QString> asBeta(rpl::producer<QString> text) {
	return std::move(text) | rpl::map([=](const QString &val)
	{
		return val + " β";
	});
}

not_null<Ui::RpWidget*> AddInnerToggle(not_null<Ui::VerticalLayout*> container,
									   const style::SettingsButton &st,
									   std::vector<not_null<Ui::AbstractCheckView*>> innerCheckViews,
									   not_null<Ui::SlideWrap<>*> wrap,
									   rpl::producer<QString> buttonLabel,
									   bool toggledWhenAll) {
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
	};
	const auto state = button->lifetime().make_state<State>(
		st.toggle,
		[=]
		{
			toggleButton->update();
		});
	state->innerChecks = std::move(innerCheckViews);
	const auto countChecked = [=]
	{
		return ranges::count_if(
			state->innerChecks,
			[](const auto &v)
			{
				return v->checked();
			});
	};
	for (const auto &innerCheck : state->innerChecks) {
		innerCheck->checkedChanges(
		) | rpl::to_empty | start_to_stream(
			state->anyChanges,
			button->lifetime());
	}
	const auto checkView = &state->checkView;
	{
		const auto separator = Ui::CreateChild<Ui::RpWidget>(container.get());
		separator->paintRequest(
		) | start_with_next([=, bg = st.textBgOver]
							{
								auto p = QPainter(separator);
								p.fillRect(separator->rect(), bg);
							},
							separator->lifetime());
		const auto separatorHeight = 2 * st.toggle.border
			+ st.toggle.diameter;
		button->geometryValue(
		) | start_with_next([=](const QRect &r)
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
		) | start_with_next([=]
							{
								auto p = QPainter(checkWidget);
								checkView->paint(p, 0, 0, checkWidget->width());
							},
							checkWidget->lifetime());
		toggleButton->sizeValue(
		) | start_with_next([=](const QSize &s)
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
	) | rpl::map(countChecked) | start_with_next([=](int count)
												 {
													 if (toggledWhenAll) {
														 checkView->setChecked(count == totalInnerChecks,
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
			auto count = Ui::Text::Bold("  "
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
		) | start_with_next([=, &icon]
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
	) | start_with_next([=, &st](const QSize &s)
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
	) | rpl::skip(1) | start_with_next([=](bool toggled)
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
	) | start_with_next([=]
						{
							wrap->toggle(!wrap->toggled(), anim::type::normal);
						},
						button->lifetime());

	toggleButton->clicks(
	) | start_with_next([=]
						{
							const auto checked = !checkView->checked();
							for (const auto &innerCheck : state->innerChecks) {
								innerCheck->setChecked(checked, anim::type::normal);
							}
						},
						toggleButton->lifetime());

	return button;
}

void AddCollapsibleToggle(not_null<Ui::VerticalLayout*> container,
						  rpl::producer<QString> title,
						  std::vector<NestedEntry> checkboxes,
						  bool toggledWhenAll) {
	const auto addCheckbox = [&](
		not_null<Ui::VerticalLayout*> verticalLayout,
		const QString &label,
		const bool isCheckedOrig)
	{
		const auto checkView = [&]() -> not_null<Ui::AbstractCheckView*>
		{
			const auto checkbox = verticalLayout->add(
				object_ptr<Ui::Checkbox>(
					verticalLayout,
					label,
					isCheckedOrig,
					st::settingsCheckbox),
				st::powerSavingButton.padding);
			const auto button = Ui::CreateChild<Ui::RippleButton>(
				verticalLayout.get(),
				st::defaultRippleAnimation);
			button->stackUnder(checkbox);
			combine(
				verticalLayout->widthValue(),
				checkbox->geometryValue()
			) | start_with_next([=](int w, const QRect &r)
								{
									button->setGeometry(0, r.y(), w, r.height());
								},
								button->lifetime());
			checkbox->setAttribute(Qt::WA_TransparentForMouseEvents);
			const auto checkView = checkbox->checkView();
			button->setClickedCallback([=]
			{
				checkView->setChecked(
					!checkView->checked(),
					anim::type::normal);
			});

			return checkView;
		}();
		checkView->checkedChanges(
		) | start_with_next([=](bool checked)
							{
							},
							verticalLayout->lifetime());

		return checkView;
	};

	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		container,
		object_ptr<Ui::VerticalLayout>(container));
	const auto verticalLayout = wrap->entity();
	auto innerChecks = std::vector<not_null<Ui::AbstractCheckView*>>();
	for (const auto &entry : checkboxes) {
		const auto c = addCheckbox(verticalLayout, entry.checkboxLabel, entry.initial);
		c->checkedValue(
		) | start_with_next([=](bool enabled)
							{
								entry.callback(enabled);
							},
							container->lifetime());
		innerChecks.push_back(c);
	}

	const auto raw = wrap.data();
	raw->hide(anim::type::instant);
	AddInnerToggle(
		container,
		st::powerSavingButtonNoIcon,
		innerChecks,
		raw,
		std::move(title),
		toggledWhenAll);
	container->add(std::move(wrap));
	container->widthValue(
	) | start_with_next([=](int w)
						{
							raw->resizeToWidth(w);
						},
						raw->lifetime());
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

} // namespace Settings