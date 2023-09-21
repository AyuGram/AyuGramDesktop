// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "base/options.h"
#include "settings/settings_common.h"

class BoxContent;

namespace Window
{
class Controller;
class SessionController;
} // namespace Window

extern const char kStreamerMode[];

extern base::options::toggle StreamerMode;

namespace Settings
{

class Ayu : public Section<Ayu>
{
public:
	Ayu(QWidget *parent, not_null<Window::SessionController *> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void AddPlatformOption(
		not_null<Window::SessionController *> window,
		not_null<Ui::VerticalLayout *> container,
		base::options::option<bool> &option,
		rpl::producer<> resetClicks);

	void SetupGhostModeToggle(not_null<Ui::VerticalLayout *> container);

	void SetupReadAfterActionToggle(not_null<Ui::VerticalLayout *> container);

	void SetupGhostEssentials(not_null<Ui::VerticalLayout *> container);

	void SetupSpyEssentials(not_null<Ui::VerticalLayout *> container);

	void SetupQoLToggles(not_null<Ui::VerticalLayout *> container);

	void SetupAppIcon(not_null<Ui::VerticalLayout *> container);

	void SetupCustomization(not_null<Ui::VerticalLayout *> container,
							not_null<Window::SessionController *> controller);

	void SetupShowPeerId(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller);

	void SetupRecentStickersLimitSlider(not_null<Ui::VerticalLayout *> container);

	void SetupAyuSync(not_null<Ui::VerticalLayout *> container);

	void SetupSendConfirmations(not_null<Ui::VerticalLayout *> container);

	void SetupExperimental(not_null<Ui::VerticalLayout *> container,
						   not_null<Window::SessionController *> controller);

	void SetupAyuGramSettings(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> null);

	void setupContent(not_null<Window::SessionController *> controller);
};

} // namespace Settings
