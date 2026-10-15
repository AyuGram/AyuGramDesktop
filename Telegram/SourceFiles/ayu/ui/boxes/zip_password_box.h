/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The box that sets the 7-Zip password.
#pragma once

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace AyuUi {

void ZipPasswordBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);

} // namespace AyuUi
