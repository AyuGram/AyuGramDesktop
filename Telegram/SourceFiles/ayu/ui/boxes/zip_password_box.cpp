/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ayu/ui/boxes/zip_password_box.h"

#include "ayu/ayu_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "lang_auto.h"

#include "styles/style_boxes.h"
#include "styles/style_layers.h"

namespace AyuUi {
namespace {

void AddNote(not_null<Ui::GenericBox*> box, const QString &text) {
	box->addRow(object_ptr<Ui::FlatLabel>(
		box.get(),
		rpl::single(text),
		st::boxLabel));
}

// 7-Zip reads the password from the console in the OEM code page, so keep to plain keyboard characters.
[[nodiscard]] bool IsPlainPassword(const QString &text) {
	for (const auto ch : text) {
		if (ch.unicode() < 0x20 || ch.unicode() > 0x7E) {
			return false;
		}
	}
	return true;
}

} // namespace

void ZipPasswordBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(rpl::single(QString("Zip password")));
	AddNote(
		box,
		QString(
			"Files you save with Save As are packed into a .7z with this password and "
			"encrypted file names. It is kept in the settings file, so turn the vault "
			"on if the file must not be readable. Leave it empty to clear it."));
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box.get(),
		st::defaultInputField,
		rpl::single(QString("Password")),
		AyuSettings::getInstance().zipPassword()));
	const auto save = [=] {
		if (!IsPlainPassword(field->getLastText())) {
			field->showError();
			controller->showToast(QString("Use basic keyboard characters only."));
			return;
		}
		AyuSettings::getInstance().setZipPassword(field->getLastText());
		controller->showToast(QString(field->getLastText().isEmpty()
			? "Zip password cleared"
			: "Zip password saved"));
		box->closeBox();
	};
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace AyuUi
