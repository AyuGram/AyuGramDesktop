// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "taptic_engine_mac.h"

#ifdef Q_OS_MAC

#import <AppKit/AppKit.h>

namespace TapticEngine {
namespace Impl {

void performHapticFeedback(NSHapticFeedbackPattern pattern) {
    [[NSHapticFeedbackManager defaultPerformer]
	    performFeedbackPattern:pattern
	    performanceTime:NSHapticFeedbackPerformanceTimeDrawCompleted];
}

void generateGeneric() {
    performHapticFeedback(NSHapticFeedbackPatternGeneric);
}

void generateAlignment() {
    performHapticFeedback(NSHapticFeedbackPatternAlignment);
}

void generateLevelChange() {
    performHapticFeedback(NSHapticFeedbackPatternLevelChange);
}

}
}

#endif
