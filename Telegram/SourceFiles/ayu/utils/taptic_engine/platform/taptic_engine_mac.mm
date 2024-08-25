#include "taptic_engine_mac.h"

#ifdef Q_OS_MAC

#import <AppKit/AppKit.h>

namespace TapticEngine {
namespace Impl {

NSHapticFeedbackManager *hapticFeedbackManager = nil;

void init() {
    if (@available(macOS 10.11, *)) {
        hapticFeedbackManager = [NSHapticFeedbackManager defaultPerformer];
    }
}

void performHapticFeedback(NSHapticFeedbackPattern pattern) {
    if (@available(macOS 10.11, *)) {
        if (hapticFeedbackManager) {
            [hapticFeedbackManager performFeedbackPattern:pattern performanceTime:NSHapticFeedbackPerformanceTimeNow];
        }
    }
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
