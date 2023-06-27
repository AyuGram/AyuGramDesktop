/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/layers/generic_box.h"
#include "ui/text/text_variant.h"
#include "ui/boxes/confirm_box.h"

namespace AyuUi {
    void VoiceConfirmBox(not_null<Ui::GenericBox*> box, Ui::ConfirmBoxArgs &&args);

    [[nodiscard]] object_ptr<Ui::GenericBox> MakeConfirmBox(
            Ui::ConfirmBoxArgs &&args);
    [[nodiscard]] object_ptr<Ui::GenericBox> MakeInformBox(v::text::data text);

} // namespace Ui
