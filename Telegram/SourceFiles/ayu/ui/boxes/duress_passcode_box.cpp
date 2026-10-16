#include "ayu/ui/boxes/duress_passcode_box.h"

#include "storage/storage_duress.h"
#include "lang_auto.h"
#include "ui/layers/generic_box.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"

namespace AyuUi {
namespace {

not_null<Ui::PasswordInput*> AddPasswordField(
		not_null<Ui::GenericBox*> box,
		rpl::producer<QString> placeholder) {
	const auto &st = st::defaultInputField;
	auto container = object_ptr<Ui::RpWidget>(box.get());
	container->resize(container->width(), st.heightMin);
	const auto field = Ui::CreateChild<Ui::PasswordInput>(
		container.data(),
		st,
		std::move(placeholder));
	container->geometryValue(
	) | rpl::on_next([=](const QRect &r) {
		field->resize(r.width(), field->height());
		field->moveToLeft(0, 0);
	}, container->lifetime());
	box->addRow(std::move(container));
	return field;
}

} // namespace

void DuressPasscodeBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(rpl::single(QString("Duress password")));
	const auto realField = AddPasswordField(
		box,
		rpl::single(QString("Current local passcode")));
	const auto duressField = AddPasswordField(
		box,
		rpl::single(QString("Duress password")));
	const auto confirmField = AddPasswordField(
		box,
		rpl::single(QString("Confirm duress password")));
	box->addRow(object_ptr<Ui::FlatLabel>(
		box.get(),
		rpl::single(QString(
			"Entering the duress password on the lock screen wipes all "
			"local app data and removes the application. "
			"Leave the new fields empty to clear an existing duress password.")),
		st::boxLabel));

	const auto save = [=] {
		const auto real = realField->text().toUtf8();
		const auto duress = duressField->text().toUtf8();
		const auto confirm = confirmField->text().toUtf8();
		if (duress != confirm) {
			confirmField->showError();
			return;
		}
		if (duress.isEmpty()) {
			if (Storage::Duress::clearDuressPasscode(real)) {
				controller->showToast(QString("Duress password cleared"));
				box->closeBox();
			} else {
				realField->showError();
			}
			return;
		}
		if (!Storage::Duress::setDuressPasscode(real, duress)) {
			realField->showError();
			duressField->showError();
			return;
		}
		controller->showToast(QString("Duress password set"));
		box->closeBox();
	};
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace AyuUi
