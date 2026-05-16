// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {
class GenericBox;
} // namespace Ui

namespace AyuUi {

void FillDeleteMyMessagesBox(
	not_null<Ui::GenericBox*> box,
	not_null<PeerData*> peer,
	not_null<Window::SessionController*> controller);

} // namespace AyuUi
