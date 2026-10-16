/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The privacy veil: a calm screen over the chats while the window is out of focus.
#pragma once

namespace AyuFeatures::Veil {

// Watches the application state and covers the windows after the configured delay.
void Start();

// Covers every window now, whatever the setting says.
void Cover();

[[nodiscard]] bool Covered();

} // namespace AyuFeatures::Veil
