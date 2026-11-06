// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

namespace Window {
class SessionController;
}

namespace Ui {

class GenericBox;

void FillDonateInfoBox(not_null<Ui::GenericBox*> box, not_null<Window::SessionController*> controller);

} // namespace Ui
