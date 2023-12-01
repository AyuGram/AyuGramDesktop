// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "ayu_settings.h"

namespace AyuState
{

namespace
{

class AyuStateVariable
{
public:
	bool val;
	int resetAfter;
};

AyuStateVariable allowSendReadPacket;

bool processVariable(AyuStateVariable &variable)
{
	if (variable.resetAfter == -1) {
		return variable.val;
	}

	variable.resetAfter -= 1;
	auto val = variable.val;

	if (variable.resetAfter == 0) {
		variable.val = false;
	}

	return val;
}

}

}
