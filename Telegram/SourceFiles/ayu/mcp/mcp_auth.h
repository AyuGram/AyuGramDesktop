// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace AyuMcp {

[[nodiscard]] QString token();
[[nodiscard]] bool authorized(const QByteArray &authorizationHeader);
[[nodiscard]] bool hostAllowed(const QByteArray &hostHeader, quint16 port);

} // namespace AyuMcp
