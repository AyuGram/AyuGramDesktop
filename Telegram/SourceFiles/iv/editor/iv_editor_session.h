/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "api/api_common.h"
#include "base/basic_types.h"
#include "menu/menu_send_details.h"

class HistoryItem;
class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace Iv::Editor {

void ShowComposeBox(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	Api::SendAction action,
	Fn<SendMenu::Details()> sendMenuDetails);
void ShowEditBox(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

} // namespace Iv::Editor
