#include "ayu_state.h"

namespace AyuState {
    void setAllowSendReadPacket(bool val, int resetAfter) {
        allowSendReadPacket.val = val;
        allowSendReadPacket.resetAfter = resetAfter;
    }

    bool getAllowSendPacket() {
        auto settings = &AyuSettings::getInstance();
        return settings->sendReadPackets || processVariable(allowSendReadPacket);
    }
}