// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "taptic_engine.h"

#if defined Q_OS_MAC
#include "ayu/utils/taptic_engine/platform/taptic_engine_mac.h"
#else
#include "ayu/utils/taptic_engine/platform/taptic_engine_dummy.h"
#endif

namespace TapticEngine {

void generateGeneric() {
	Impl::generateGeneric();
}

void generateAlignment() {
	Impl::generateAlignment();
}

void generateLevelChange() {
	Impl::generateLevelChange();
}

}
