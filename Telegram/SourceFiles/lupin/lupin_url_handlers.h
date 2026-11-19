// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "base/qthelp_regex.h"
#include "window/window_session_controller.h"

#include <QtCore/QUrl>

namespace LupinUrlHandlers {

using Match = qthelp::RegularExpressionMatch;

bool ResolveUser(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool ResolveChat(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleLupin(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleSupport(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleLupinSettings(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool TryHandleSpotify(const QString &url);

[[nodiscard]] bool IsWebUrl(const QUrl &url);
[[nodiscard]] bool IsUnsafeExternalUrl(const QString &url);

}
