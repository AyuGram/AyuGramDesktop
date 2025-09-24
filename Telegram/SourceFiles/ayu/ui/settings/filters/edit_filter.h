// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "ayu/data/entities.h"
#include "settings/settings_common_session.h"

#include "ui/wrap/vertical_layout.h"
#include "chat_helpers/emoji_suggestions_widget.h"
#include "boxes/premium_limits_box.h"
#include "info/profile/info_profile_values.h"
#include "window/window_session_controller.h"
#include "base/unixtime.h"

class BoxContent;

namespace Window {
class Controller;
class SessionController;
} // namespace Window

namespace Settings {

class AyuEditFilters : public Section<AyuEditFilters>
{
public:
	AyuEditFilters(QWidget *parent, not_null<Window::SessionController*> controller);
	void setupSettings(not_null<Ui::VerticalLayout*> container, QWidget *parent);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent(not_null<Window::SessionController*> controller, QWidget *parent);

	RegexFilter currentFilter;
};

object_ptr<Ui::GenericBox> RegexEditBox(RegexFilter *filter,
										const Fn<void(RegexFilter)> &onDone,
										std::optional<long long> dialogId = std::nullopt,
										bool showToast = false);
} // namespace Settings