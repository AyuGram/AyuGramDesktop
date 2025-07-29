// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

namespace Ui {

class GenericBox;

void FillDonateQrBox(
	not_null<Ui::GenericBox*> box,
	const QString &address,
	const QString &iconResourcePath);

} // namespace Ui
