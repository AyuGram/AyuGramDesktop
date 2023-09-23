// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "edit_edited_mark.h"

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
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/special_fields.h"

#include <QtGui/QGuiApplication>
#include "storage/localstorage.h"

#include "ayu/ayu_settings.h"

EditEditedMarkBox::EditEditedMarkBox(QWidget *)
	:
	_text(
		this,
		st::defaultInputField,
		tr::ayu_EditedMarkText(),
		AyuSettings::getInstance().editedMark)
{
}

void EditEditedMarkBox::prepare()
{
	const auto defaultEditedMark = tr::lng_edited(tr::now);
	auto newHeight = st::contactPadding.top() + _text->height();

	setTitle(tr::ayu_EditedMarkText());

	newHeight += st::boxPadding.bottom() + st::contactPadding.bottom();
	setDimensions(st::boxWidth, newHeight);

	addLeftButton(tr::ayu_BoxActionReset(), [=]
	{ _text->setText(defaultEditedMark); });
	addButton(tr::lng_settings_save(), [=]
	{ save(); });
	addButton(tr::lng_cancel(), [=]
	{ closeBox(); });

	const auto submitted = [=]
	{ submit(); };
	_text->submits(
	) | rpl::start_with_next(submitted, _text->lifetime());
}

void EditEditedMarkBox::setInnerFocus()
{
	_text->setFocusFast();
}

void EditEditedMarkBox::submit()
{
	if (_text->getLastText().trimmed().isEmpty()) {
		_text->setFocus();
		_text->showError();
	}
	else {
		save();
	}
}

void EditEditedMarkBox::resizeEvent(QResizeEvent *e)
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

void EditEditedMarkBox::save()
{
	const auto settings = &AyuSettings::getInstance();
	settings->set_editedMark(_text->getLastText());
	AyuSettings::save();

	closeBox();
}
