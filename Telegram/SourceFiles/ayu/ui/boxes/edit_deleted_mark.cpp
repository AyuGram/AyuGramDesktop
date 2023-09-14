// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "edit_deleted_mark.h"

#include "base/random.h"
#include "boxes/peer_list_controllers.h"
#include "boxes/premium_limits_box.h"
#include "data/data_cloud_file.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "ui/ui_utility.h"
#include "ui/unread_badge.h"
#include "ui/controls/userpic_button.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/fields/special_fields.h"

#include <QtGui/QGuiApplication>
#include "storage/localstorage.h"

#include "ayu/ayu_settings.h"

EditDeletedMarkBox::EditDeletedMarkBox(QWidget *)
	:
	_text(
		this,
		st::defaultInputField,
		tr::ayu_DeletedMarkText(),
		AyuSettings::getInstance().deletedMark)
{
}

void EditDeletedMarkBox::prepare()
{
	const auto defaultDeletedMark = "🧹";
	auto newHeight = st::contactPadding.top() + _text->height();

	setTitle(tr::ayu_DeletedMarkText());

	newHeight += st::boxPadding.bottom() + st::contactPadding.bottom();
	setDimensions(st::boxWidth, newHeight);

	addLeftButton(tr::ayu_BoxActionReset(), [=]
	{ _text->setText(defaultDeletedMark); });

	addButton(tr::lng_settings_save(), [=]
	{ save(); });
	addButton(tr::lng_cancel(), [=]
	{ closeBox(); });

	connect(_text, &Ui::InputField::submitted, [=]
	{ submit(); });
}

void EditDeletedMarkBox::setInnerFocus()
{
	_text->setFocusFast();
}

void EditDeletedMarkBox::submit()
{
	if (_text->getLastText().trimmed().isEmpty()) {
		_text->setFocus();
		_text->showError();
	}
	else {
		save();
	}
}

void EditDeletedMarkBox::resizeEvent(QResizeEvent *e)
{
	BoxContent::resizeEvent(e);

	_text->resize(
		width()
			- st::boxPadding.left()
			- st::newGroupInfoPadding.left()
			- st::boxPadding.right(),
		_text->height());

	const auto left = st::boxPadding.left() + st::newGroupInfoPadding.left();
	_text->moveToLeft(left, st::contactPadding.top());
}

void EditDeletedMarkBox::save()
{
	const auto settings = &AyuSettings::getInstance();
	settings->set_deletedMark(_text->getLastText());
	AyuSettings::save();

	closeBox();
}
