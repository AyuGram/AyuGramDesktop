// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "base/qthelp_regex.h"
#include "window/window_session_controller.h"

#include <QtCore/QUrl>

namespace AyuUrlHandlers {

using Match = qthelp::RegularExpressionMatch;

bool ResolveUser(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool ResolveChat(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleAyu(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleSupport(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleAyuSettings(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool TryHandleSpotify(const QString &url);

[[nodiscard]] bool IsWebUrl(const QUrl &url);
[[nodiscard]] bool IsUnsafeExternalUrl(const QString &url);

}
