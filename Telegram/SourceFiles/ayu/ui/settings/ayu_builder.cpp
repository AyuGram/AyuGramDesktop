// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/ayu_builder.h"

#include "ayu/ayu_settings.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings::AyuBuilder {
namespace {

[[nodiscard]] QString ResolveTitle(rpl::producer<QString> title) {
	auto result = QString();
	auto lifetime = rpl::lifetime();
	std::move(title).start(
		[&](QString value) { result = std::move(value); },
		[](auto&&) {},
		[] {},
		lifetime);
	return result;
}

} // namespace

AyuSectionBuilder::AyuSectionBuilder(Builder::SectionBuilder &builder)
: _builder(builder) {
}

Builder::SectionBuilder &AyuSectionBuilder::base() {
	return _builder;
}

Ui::SettingsButton *AyuSectionBuilder::addSettingToggle(
		SettingToggleArgs &&args) {
	auto getter = [g = args.getter] {
		return (AyuSettings::getInstance().*g)();
	};
	auto setter = [s = args.setter](bool v) {
		(AyuSettings::getInstance().*s)(v);
	};
	return addToggle({
		.id = std::move(args.id),
		.altIds = std::move(args.altIds),
		.title = std::move(args.title),
		.getter = std::move(getter),
		.setter = std::move(setter),
		.icon = std::move(args.icon),
		.keywords = std::move(args.keywords),
		.shown = std::move(args.shown),
	});
}

Ui::SettingsButton *AyuSectionBuilder::addToggle(ToggleArgs &&args) {
	auto getter = std::move(args.getter);
	auto setter = std::move(args.setter);
	const auto initialValue = getter();

	const auto button = _builder.addButton({
		.id = std::move(args.id),
		.altIds = std::move(args.altIds),
		.title = std::move(args.title),
		.st = args.icon.icon ? nullptr : &st::settingsButtonNoIcon,
		.icon = std::move(args.icon),
		.toggled = rpl::single(initialValue),
		.keywords = std::move(args.keywords),
		.shown = std::move(args.shown),
	});
	if (button) {
		button->toggledValue(
		) | rpl::filter(
			[=](bool enabled) {
				return (enabled != getter());
			}
		) | rpl::on_next(
			[=](bool enabled) {
				setter(enabled);
			},
			button->lifetime());
	}
	return button;
}

Fn<void()> AyuSectionBuilder::addCollapsibleToggle(
		CollapsibleToggleArgs &&args) {
	auto resolvedTitle = ResolveTitle(rpl::duplicate(args.title));
	auto checkboxes = std::move(args.checkboxes);
	auto toggledWhenAll = args.toggledWhenAll;
	auto id = std::move(args.id);
	auto altIds = std::move(args.altIds);
	auto keywords = std::move(args.keywords);

	Fn<void()> result;

	_builder.add([&](const Builder::BuildContext &ctx) {
		v::match(ctx, [&](const Builder::WidgetContext &wctx) {
			auto toggle = AddCollapsibleToggle(
				wctx.container,
				std::move(args.title),
				std::move(checkboxes),
				toggledWhenAll);
			result = std::move(toggle.refresh);
			if (!id.isEmpty() && wctx.highlights && toggle.widget) {
				wctx.highlights->push_back({
					id,
					{ toggle.widget, {} },
				});
			}
		}, [&](const Builder::SearchContext &sctx) {
			if (!id.isEmpty()) {
				sctx.entries->push_back({
					.id = id,
					.altIds = altIds,
					.title = resolvedTitle,
					.keywords = keywords,
					.section = sctx.sectionId,
				});
			}
			for (const auto &cb : checkboxes) {
				if (!cb.checkboxLabel.isEmpty()) {
					sctx.entries->push_back({
						.id = id + u"/"_q + cb.checkboxLabel,
						.title = cb.checkboxLabel,
						.section = sctx.sectionId,
						.checkIcon = cb.getter()
							? Builder::SearchEntryCheckIcon::Checked
							: Builder::SearchEntryCheckIcon::Unchecked,
					});
				}
			}
		});
	});
	return result;
}

void AyuSectionBuilder::addChooseButton(ChooseButtonArgs &&args) {
	auto options = std::move(args.options);
	auto setter = std::move(args.setter);
	auto initialSelection = args.initialSelection;
	auto id = std::move(args.id);
	auto altIds = std::move(args.altIds);
	auto keywords = std::move(args.keywords);
	auto icon = std::move(args.icon);
	auto resolvedTitle = ResolveTitle(rpl::duplicate(args.title));

	_builder.add([&](const Builder::BuildContext &ctx) {
		v::match(ctx, [&](const Builder::WidgetContext &wctx) {
			AddChooseButtonWithIconAndRightTextInner(
				wctx.container,
				wctx.controller,
				initialSelection,
				options,
				std::move(args.title),
				std::move(args.boxTitle),
				icon.icon ? st::settingsButton : st::settingsButtonNoIcon,
				std::move(icon),
				setter);
		}, [&](const Builder::SearchContext &sctx) {
			if (!id.isEmpty()) {
				sctx.entries->push_back({
					.id = id,
					.altIds = altIds,
					.title = resolvedTitle,
					.keywords = keywords,
					.section = sctx.sectionId,
				});
			}
		});
	});
}

void AyuSectionBuilder::addSlider(SliderArgs &&args) {
	auto id = std::move(args.id);
	auto altIds = std::move(args.altIds);
	auto keywords = std::move(args.keywords);
	auto resolvedTitle = ResolveTitle(rpl::duplicate(args.title));

	_builder.add([&](const Builder::BuildContext &ctx) {
		v::match(ctx, [&](const Builder::WidgetContext &wctx) {
			const auto container = wctx.container;
			if (args.showTitle) {
				container->add(
					object_ptr<Button>(container,
						std::move(args.title),
						st::settingsButtonNoIcon)
				)->setAttribute(Qt::WA_TransparentForMouseEvents);
			}

			auto sliderWithLabel = MakeSliderWithLabel(
				container,
				st::autoDownloadLimitSlider,
				st::settingsScaleLabel,
				0,
				args.showTitle ? st::settingsScaleLabel.style.font->width("8%%%") : 0);
			container->add(
				std::move(sliderWithLabel.widget),
				st::recentStickersLimitPadding);
			const auto slider = sliderWithLabel.slider;
			const auto label = sliderWithLabel.label;

			auto formatLabel = std::move(args.formatLabel);
			auto indexToValue = std::move(args.indexToValue);
			auto onChanged = std::move(args.onChanged);
			auto onFinalChanged = std::move(args.onFinalChanged);

			if (formatLabel) {
				label->setText(formatLabel(args.current));
			}

			slider->setPseudoDiscrete(
				args.steps,
				[=](int index) {
					return indexToValue ? indexToValue(index) : index;
				},
				args.current,
				[=](int value) {
					if (formatLabel) {
						label->setText(formatLabel(value));
					}
					if (onChanged) {
						onChanged(value);
					}
				},
				[=](int value) {
					if (formatLabel) {
						label->setText(formatLabel(value));
					}
					if (onFinalChanged) {
						onFinalChanged(value);
					}
				});
		}, [&](const Builder::SearchContext &sctx) {
			if (!id.isEmpty() && args.showTitle) {
				sctx.entries->push_back({
					.id = id,
					.altIds = altIds,
					.title = resolvedTitle,
					.keywords = keywords,
					.section = sctx.sectionId,
				});
			}
		});
	});
}

void AyuSectionBuilder::addBetaBadge(not_null<Ui::SettingsButton*> button) {
	AddBetaBadge(button);
}

void AyuSectionBuilder::addSectionDivider() {
	_builder.addSkip();
	_builder.addDivider();
	_builder.addSkip();
}

} // namespace Settings::AyuBuilder
