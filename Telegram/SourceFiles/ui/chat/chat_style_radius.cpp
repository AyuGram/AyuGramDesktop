/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/chat/chat_style_radius.h"
#include "ui/chat/chat_style.h"
#include "base/options.h"

#include "ui/chat/chat_theme.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "styles/style_chat.h"

namespace Ui {
namespace {

constexpr auto kBubbleRadiusSliderMax = 16;

int AppliedBubbleRadius = 16;
int BubbleRadiusOverride = -1;

[[nodiscard]] int ClampBubbleRadiusValue(int value) {
	return (value < 0)
		? 0
		: (value > kBubbleRadiusSliderMax)
		? kBubbleRadiusSliderMax
		: value;
}

[[nodiscard]] int EffectiveBubbleRadiusValue() {
	return (BubbleRadiusOverride >= 0)
		? BubbleRadiusOverride
		: AppliedBubbleRadius;
}

[[nodiscard]] int MapBubbleRadius(int sliderValue, int maximum) {
	if (sliderValue <= 0 || maximum <= 0) {
		return 0;
	} else if (sliderValue >= kBubbleRadiusSliderMax) {
		return maximum;
	}
	const auto result = (sliderValue * maximum + (kBubbleRadiusSliderMax / 2))
		/ kBubbleRadiusSliderMax;
	return (result < 0) ? 0 : (result > maximum) ? maximum : result;
}

base::options::toggle UseSmallMsgBubbleRadius({
	.id = kOptionUseSmallMsgBubbleRadius,
	.name = "Use small message bubble radius",
	.description = "Makes most message bubbles square-ish.",
	.restartRequired = true,
});

} // namespace

const char kOptionUseSmallMsgBubbleRadius[] = "use-small-msg-bubble-radius";

void SetAppliedBubbleRadius(int value) {
	AppliedBubbleRadius = ClampBubbleRadiusValue(value);
}

void SetBubbleRadiusOverride(int value) {
	BubbleRadiusOverride = ClampBubbleRadiusValue(value);
}

void ClearBubbleRadiusOverride() {
	BubbleRadiusOverride = -1;
}

int BubbleRadiusSmall() {
	static auto cachedValue = -1;
	static auto cachedRadius = st::bubbleRadiusSmall;
	const auto value = EffectiveBubbleRadiusValue();
	if (cachedValue != value) {
		cachedValue = value;
		cachedRadius = MapBubbleRadius(value, st::bubbleRadiusSmall);
	}
	return cachedRadius;
}

int BubbleRadiusLarge() {
	static auto cachedValue = -1;
	static auto cachedRadius = st::bubbleRadiusLarge;
	const auto value = EffectiveBubbleRadiusValue();
	if (cachedValue != value) {
		cachedValue = value;
		cachedRadius = MapBubbleRadius(value, st::bubbleRadiusLarge);
	}
	return cachedRadius;
}

int MsgFileThumbRadiusSmall() {
	static auto cachedValue = -1;
	static auto cachedRadius = st::msgFileThumbRadiusSmall;
	const auto value = EffectiveBubbleRadiusValue();
	if (cachedValue != value) {
		cachedValue = value;
		cachedRadius = MapBubbleRadius(value, st::msgFileThumbRadiusSmall);
	}
	return cachedRadius;
}

int MsgFileThumbRadiusLarge() {
	static auto cachedValue = -1;
	static auto cachedRadius = st::msgFileThumbRadiusLarge;
	const auto value = EffectiveBubbleRadiusValue();
	if (cachedValue != value) {
		cachedValue = value;
		cachedRadius = MapBubbleRadius(value, st::msgFileThumbRadiusLarge);
	}
	return cachedRadius;
}

}
