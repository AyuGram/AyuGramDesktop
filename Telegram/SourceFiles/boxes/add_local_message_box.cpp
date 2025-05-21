#include "boxes/add_local_message_box.h"

#include "ui/widgets/labels.h"
#include "ui/widgets/input_fields.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/layout.h"
#include "ui/wrap/padding_wrap.h"
#include "lang/lang_keys.h"
#include "window/window_session_controller.h"
#include "main/main_session.h"
#include "data/data_user.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"

AddLocalMessageBox::AddLocalMessageBox(QWidget*, not_null<Window::SessionController*> controller)
: BoxContent(controller->uiShow())
, _controller(controller)
, _senderField(
	this,
	st::defaultInputField,
	tr::lng_local_message_sender_ph(), // Placeholder for sender name
	_controller->session().user()->name()) // Default to current user's name
, _messageField(
	this,
	st::defaultInputFieldContext, // Use context for multiline
	tr::lng_local_message_text_ph()) { // Placeholder for message text
	setupControls();
}

void AddLocalMessageBox::setupControls() {
	const auto content =verticalLayout();
	setTitle(tr::lng_box_add_local_message_title());

	content->add(object_ptr<Ui::FlatLabel>(
		content,
		tr::lng_local_message_sender_label(), // "Sender:"
		st::boxLabel));
	content->add(object_ptr<Ui::PaddingWrap<Ui::InputField>>(
		content,
		base::duplicate(_senderField),
		st::boxPadding));

	content->add(object_ptr<Ui::FlatLabel>(
		content,
		tr::lng_local_message_text_label(), // "Message Text:"
		st::boxLabel));
	_messageField->setInstantInserts(true);
	_messageField->setSubmitSettings(Ui::InputField::SubmitSettings::Both);
	_messageField->setMaxHeight(st::boxMaxListHeight / 2); // Allow ample space for text
	content->add(object_ptr<Ui::PaddingWrap<Ui::InputField>>(
		content,
		base::duplicate(_messageField),
		st::boxPadding));
	
	_submitButton = addButton(tr::lng_box_ok(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });
}

void AddLocalMessageBox::prepare() {
	setDimensions(st::boxWidth, layout()->heightForWidth(st::boxWidth));
	_senderField->setFocus();
}

void AddLocalMessageBox::setInnerFocus() {
	_senderField->setFocus();
}

void AddLocalMessageBox::save() {
	const auto senderName = _senderField->getLastText().trimmed();
	const auto messageText = _messageField->getLastText().trimmed();

	if (messageText.isEmpty()) {
		_messageField->showError();
		return;
	}

	_saveLocalMessageRequests.fire({senderName, messageText});
	closeBox();
}

rpl::producer<AddLocalMessageBox::LocalMessageData> AddLocalMessageBox::saveLocalMessageRequests() const {
	return _saveLocalMessageRequests.events();
}
