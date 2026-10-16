/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ayu/ui/boxes/vault_boxes.h"

#include "storage/storage_vault.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "window/window_session_controller.h"
#include "lang_auto.h"
#include "settings.h"

#include "styles/style_boxes.h"
#include "styles/style_layers.h"

namespace AyuUi {
namespace {

constexpr auto kMinimumPasswordLength = 8;
constexpr auto kMaximumPasswordLength = 256;

not_null<Ui::PasswordInput*> AddPasswordField(
		not_null<Ui::GenericBox*> box,
		const QString &placeholder) {
	const auto &st = st::defaultInputField;
	auto container = object_ptr<Ui::RpWidget>(box.get());
	container->resize(container->width(), st.heightMin);
	const auto field = Ui::CreateChild<Ui::PasswordInput>(
		container.data(),
		st,
		rpl::single(placeholder));
	container->geometryValue(
	) | rpl::on_next([=](const QRect &r) {
		field->resize(r.width(), field->height());
		field->moveToLeft(0, 0);
	}, container->lifetime());
	box->addRow(std::move(container));
	return field;
}

void AddNote(not_null<Ui::GenericBox*> box, const QString &text) {
	box->addRow(object_ptr<Ui::FlatLabel>(
		box.get(),
		rpl::single(text),
		st::boxLabel));
}

} // namespace

void VaultEnableBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Fn<void()> changed) {
	box->setTitle(rpl::single(QString("Encrypt data when closed")));
	AddNote(
		box,
		QString(
			"Your chats and your login are encrypted with this password when the app "
			"closes, and opened again when you start it. There is no recovery: if you "
			"forget the password you sign in again. If the app is killed instead of "
			"closed, the data stays readable until the next normal close."));
	const auto first = AddPasswordField(box, QString("New password"));
	const auto second = AddPasswordField(box, QString("Repeat the password"));
	const auto save = [=] {
		const auto text = first->getLastText();
		const auto password = text.toUtf8();
		if (text.size() < kMinimumPasswordLength) {
			first->showError();
			controller->showToast(QString("Use at least 8 characters."));
			return;
		} else if (text.size() > kMaximumPasswordLength) {
			first->showError();
			controller->showToast(QString("Use at most 256 characters."));
			return;
		} else if (password != second->getLastText().toUtf8()) {
			second->showError();
			return;
		}
		if (!Storage::Vault::Enable(cWorkingDir(), password)) {
			controller->showToast(QString("The vault could not be turned on."));
			return;
		}
		controller->showToast(QString("The vault is on. Your data is encrypted when you close the app."));
		box->closeBox();
		if (changed) {
			changed();
		}
	};
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void VaultDisableBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		Fn<void()> changed) {
	box->setTitle(rpl::single(QString("Turn the vault off")));
	AddNote(box, QString("Enter the vault password. Your data will stay readable on disk."));
	const auto field = AddPasswordField(box, QString("Vault password"));
	const auto save = [=] {
		if (!Storage::Vault::Disable(cWorkingDir(), field->getLastText().toUtf8())) {
			field->showError();
			return;
		}
		controller->showToast(QString("The vault is off."));
		box->closeBox();
		if (changed) {
			changed();
		}
	};
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace AyuUi
