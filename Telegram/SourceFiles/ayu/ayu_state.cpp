// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "ayu_state.h"

namespace AyuState
{

void setAllowSendReadPacket(bool val, int resetAfter)
{
	allowSendReadPacket.val = val;
	allowSendReadPacket.resetAfter = resetAfter;
}

bool getAllowSendPacket()
{
	auto settings = &AyuSettings::getInstance();
	return settings->sendReadMessages || processVariable(allowSendReadPacket);
}

}
