/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The boxes that turn the vault on and off.
#pragma once

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace AyuUi {

void VaultEnableBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	Fn<void()> changed);void VaultDisableBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	Fn<void()> changed);

} // namespace AyuUi
