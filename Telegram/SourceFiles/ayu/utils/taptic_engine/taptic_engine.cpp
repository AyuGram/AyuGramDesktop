#include "taptic_engine.h"

#if defined Q_OS_MAC
#include "ayu/utils/taptic_engine/platform/taptic_engine_mac.h"
#else
#include "ayu/utils/taptic_engine/platform/taptic_engine_dummy.h"
#endif

namespace TapticEngine {

void init() {
	Impl::init();
}

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
