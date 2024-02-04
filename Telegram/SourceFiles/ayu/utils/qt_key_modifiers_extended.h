// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#pragma once

#include "base/qt/qt_key_modifiers.h"

namespace base {

[[nodiscard]] inline bool IsExtendedContextMenuModifierPressed() {
	return IsShiftPressed() || IsCtrlPressed();
}

} // namespace base
