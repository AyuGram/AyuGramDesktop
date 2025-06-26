// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "edit_mark_box.h"

#include <utility>

#include "boxes/peer_list_controllers.h"
#include "lang/lang_keys.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/special_fields.h"

#include "ayu/ayu_settings.h"

EditMarkBox::EditMarkBox(QWidget *,
						 rpl::producer<QString> title,
						 const QString &currentValue,
						 QString defaultValue,
						 const Fn<void(const QString &)> &saveCallback)
	: _title(title)
	  , _defaultValue(std::move(defaultValue))
	  , _saveCallback(saveCallback)
	  , _text(
		  this,
		  st::defaultInputField,
		  title,
		  currentValue) {
}

void EditMarkBox::prepare() {
	auto newHeight = st::contactPadding.top() + _text->height();

	setTitle(_title);

	newHeight += st::boxPadding.bottom() + st::contactPadding.bottom();
	setDimensions(st::boxWidth, newHeight);

	addLeftButton(tr::ayu_BoxActionReset(),
				  [=]
				  {
					  _text->setText(_defaultValue);
				  });

	addButton(tr::lng_settings_save(),
			  [=]
			  {
				  save();
			  });
	addButton(tr::lng_cancel(),
			  [=]
			  {
				  closeBox();
			  });

	const auto submitted = [=]
	{
		submit();
	};
	_text->submits(
	) | rpl::start_with_next(submitted, _text->lifetime());
}

void EditMarkBox::setInnerFocus() {
	_text->setFocusFast();
}

void EditMarkBox::submit() {
	if (_text->getLastText().trimmed().isEmpty()) {
		_text->setFocus();
		_text->showError();
	} else {
		save();
	}
}

void EditMarkBox::resizeEvent(QResizeEvent *e) {
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

void EditMarkBox::save() {
	_saveCallback(_text->getLastText());
	closeBox();
}
