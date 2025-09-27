// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <unordered_set>

class ShadowBanUtils {
public:
	static void addShadowBan(long long userId);
	static void removeShadowBan(long long userId);
	static bool isShadowBanned(long long userId);

private:
	ShadowBanUtils() = delete;

	static void setShadowBanList();
};
