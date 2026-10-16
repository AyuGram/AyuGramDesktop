/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ayu/ui/boxes/duress_program_box.h"

#include "ayu/ayu_settings.h"
#include "base/weak_ptr.h"
#include "core/file_utilities.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "lang_auto.h"
#include "settings.h"

#include "styles/style_boxes.h"
#include "styles/style_layers.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

namespace AyuUi {
namespace {

void AddNote(not_null<Ui::GenericBox*> box, const QString &text) {
	box->addRow(object_ptr<Ui::FlatLabel>(
		box.get(),
		rpl::single(text),
		st::boxLabel));
}

[[nodiscard]] bool IsProgram(const QString &path) {
	const auto info = QFileInfo(path);
#ifdef Q_OS_WIN
	return info.isFile()
		&& info.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0;
#else // Q_OS_WIN
	return info.isFile() && info.isExecutable();
#endif // !Q_OS_WIN
}

// Wiped before the program starts, so it would never run.
[[nodiscard]] bool IsInsideWipedFolder(const QString &path) {
	const auto clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
	const auto inside = [&](const QString &root) {
		const auto base = QDir::cleanPath(QDir::fromNativeSeparators(root));
		return !base.isEmpty()
			&& clean.startsWith(base + '/', Qt::CaseInsensitive);
	};
	return inside(cWorkingDir()) || inside(cExeDir());
}

} // namespace

void DuressProgramBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(rpl::single(QString("Program to start on duress")));
	AddNote(
		box,
		QString(
			"After a duress wipe this program is started on its own, then the app exits. "
			"Choose a program that is not inside the app's folders."));
	const auto current = AyuSettings::getInstance().duressLaunchPath();
	AddNote(box, current.isEmpty() ? QString("None chosen") : current);
	const auto weak = base::make_weak(box);
	box->addButton(rpl::single(QString("Choose program")), [=] {
		FileDialog::GetOpenPath(
			controller->window().widget().get(),
			QString("Choose a program"),
			QString("Programs (*.exe);;All files (*)"),
			[=](FileDialog::OpenResult &&result) {
				if (result.paths.isEmpty()) {
					return;
				}
				const auto path = QDir::toNativeSeparators(result.paths.front());
				if (!IsProgram(path)) {
					controller->showToast(QString("That is not a program."));
					return;
				} else if (IsInsideWipedFolder(path)) {
					controller->showToast(QString("Choose a program outside the app's folders."));
					return;
				}
				AyuSettings::getInstance().setDuressLaunchPath(path);
				controller->showToast(QString("Saved."));
				if (const auto strong = weak.get()) {
					strong->closeBox();
				}
			});
	});
	if (!current.isEmpty()) {
		box->addButton(rpl::single(QString("Remove")), [=] {
			AyuSettings::getInstance().setDuressLaunchPath(QString());
			controller->showToast(QString("Removed."));
			box->closeBox();
		});
	}
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace AyuUi
