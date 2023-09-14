// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_variant.h"

namespace AyuUi
{

void VoiceConfirmBox(not_null<Ui::GenericBox *> box, Ui::ConfirmBoxArgs &&args);

[[nodiscard]] object_ptr<Ui::GenericBox> MakeConfirmBox(
	Ui::ConfirmBoxArgs &&args);

[[nodiscard]] object_ptr<Ui::GenericBox> MakeInformBox(v::text::data text);

} // namespace Ui
