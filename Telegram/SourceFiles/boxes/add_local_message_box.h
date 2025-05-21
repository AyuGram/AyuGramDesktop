#pragma once

#include "ui/layers/box_content.h"
#include "ui/widgets/input_fields.h"
#include "ui/widgets/buttons.h"

namespace Ui {
class VerticalLayout;
} // namespace Ui

class AddLocalMessageBox : public Ui::BoxContent {
public:
	AddLocalMessageBox(QWidget*, not_null<Window::SessionController*> controller);

	struct LocalMessageData {
		QString senderName;
		QString messageText;
	};

	rpl::producer<LocalMessageData> saveLocalMessageRequests() const;

protected:
	void prepare() override;
	void setInnerFocus() override;

private:
	void setupControls();
	void save();

	not_null<Window::SessionController*> _controller;
	object_ptr<Ui::InputField> _senderField;
	object_ptr<Ui::InputField> _messageField;
	object_ptr<Ui::RoundButton> _submitButton;

	rpl::event_stream<LocalMessageData> _saveLocalMessageRequests;

};
