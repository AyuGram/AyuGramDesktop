// Drakeygram Desktop
// Fully independent fork. No external tracking.

#include "ayu/ui/toasts.h"

#include "ui/toast/toast.h"

namespace AyuUi {

void ShowToast(const QString &text) {
	Ui::Toast::Show(text);
}

}
