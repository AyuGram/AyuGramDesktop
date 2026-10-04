// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "api/api_common.h"
#include "history/history_item.h"
#include "main/main_session.h"

namespace AyuForward {

bool forwardRichMessage(
	not_null<Main::Session*> session,
	FullMsgId itemId,
	const Api::SendAction &action,
	Fn<bool()> cancelled = nullptr);

} // namespace AyuForward
