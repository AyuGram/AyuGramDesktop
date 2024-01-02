// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "base/options.h"
#include "settings/settings_common_session.h"
#include "settings/settings_common.h"

class BoxContent;

namespace Window
{
class Controller;
class SessionController;
} // namespace Window

namespace Settings
{

class Ayu : public Section<Ayu>
{
public:
	Ayu(QWidget *parent, not_null<Window::SessionController *> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent(not_null<Window::SessionController *> controller);
};

} // namespace Settings
