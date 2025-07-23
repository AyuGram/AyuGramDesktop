// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/ui/boxes/local_message_editor.h"

#include "ayu/data/messages_storage.h"
#include "ayu/ui/context_menu/context_menu.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "lang_auto.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/masked_input_field.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/controls/userpic_button.h"
#include "ui/text/format_values.h"
#include "ui/text/text_utilities.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"

#include <QtCore/QDateTime>

namespace AyuUi {

namespace {

constexpr auto kMaxMessageLength = 4096;
constexpr auto kMaxCaptionLength = 1024;
constexpr auto kMaxPostAuthorLength = 255;

class LocalMessageEditor {
public:
	LocalMessageEditor(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const LocalMessageData &initialData);

private:
	void setupContent();
	void setupBasicFields();
	void setupAdvancedFields();
	void setupMessageTypeSection();
	void setupAuthorSection();
	void setupTimingSection();
	void setupFlagsSection();
	void setupStatsSection();
	void setupReplySection();
	void setupForwardSection();
	void setupMediaSection();
	void setupButtons();
	
	void updateFromUser();
	void updateDateTime();
	void updatePreview();
	void saveMessage();
	
	[[nodiscard]] MessageFlags buildFlags() const;
	[[nodiscard]] HistoryItemCommonFields buildFields() const;
	
	const not_null<Ui::GenericBox*> _box;
	const not_null<Window::SessionController*> _controller;
	const not_null<History*> _history;
	LocalMessageData _data;
	
	// UI Elements
	not_null<Ui::ScrollArea*> _scroll;
	not_null<Ui::VerticalLayout*> _content;
	
	// Basic fields
	Ui::InputField* _textField = nullptr;
	Ui::InputField* _fromField = nullptr;
	Ui::UserpicButton* _fromUserpic = nullptr;
	
	// Message type
	Ui::Checkbox* _isServiceMessage = nullptr;
	Ui::SlideWrap<Ui::InputField>* _serviceTextWrap = nullptr;
	
	// Author section
	Ui::InputField* _postAuthorField = nullptr;
	Ui::InputField* _viaBotField = nullptr;
	
	// Timing section
	Ui::MaskedInputField* _dateField = nullptr;
	Ui::MaskedInputField* _timeField = nullptr;
	Ui::Checkbox* _wasEditedCheck = nullptr;
	Ui::MaskedInputField* _editDateField = nullptr;
	Ui::MaskedInputField* _editTimeField = nullptr;
	
	// Flags section
	Ui::Checkbox* _silentCheck = nullptr;
	Ui::Checkbox* _pinnedCheck = nullptr;
	Ui::Checkbox* _noForwardsCheck = nullptr;
	Ui::Checkbox* _invertMediaCheck = nullptr;
	
	// Stats section
	Ui::Checkbox* _hasViewsCheck = nullptr;
	Ui::MaskedInputField* _viewsField = nullptr;
	Ui::Checkbox* _hasSharesCheck = nullptr;
	Ui::MaskedInputField* _sharesField = nullptr;
	
	// Reply section
	Ui::InputField* _replyToField = nullptr;
	Ui::InputField* _replyQuoteField = nullptr;
	
	// Forward section
	Ui::Checkbox* _isForwardedCheck = nullptr;
	Ui::SlideWrap<Ui::VerticalLayout>* _forwardWrap = nullptr;
	Ui::InputField* _forwardFromField = nullptr;
	Ui::InputField* _forwardFromNameField = nullptr;
	Ui::MaskedInputField* _forwardDateField = nullptr;
	Ui::MaskedInputField* _forwardTimeField = nullptr;
	Ui::InputField* _forwardPostAuthorField = nullptr;
	
	// Media section
	Ui::Checkbox* _hasMediaCheck = nullptr;
	Ui::SlideWrap<Ui::VerticalLayout>* _mediaWrap = nullptr;
	Ui::InputField* _mediaPathField = nullptr;
	Ui::InputField* _mediaCaptionField = nullptr;
	
	// Business section
	Ui::MaskedInputField* _starsPaidField = nullptr;
	Ui::MaskedInputField* _effectIdField = nullptr;
	Ui::MaskedInputField* _groupedIdField = nullptr;
};

LocalMessageEditor::LocalMessageEditor(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	not_null<History*> history,
	const LocalMessageData &initialData)
: _box(box)
, _controller(controller)
, _history(history)
, _data(initialData)
, _scroll(_box->addRow(object_ptr<Ui::ScrollArea>(_box)))
, _content(_scroll->setOwnedWidget(object_ptr<Ui::VerticalLayout>(_scroll))) {
	
	// Initialize default values
	if (_data.date == 0) {
		_data.date = base::unixtime::now();
	}
	if (_data.fromId == 0) {
		_data.fromId = _history->peer->id;
	}
	
	setupContent();
}

void LocalMessageEditor::setupContent() {
	_box->setTitle(tr::ayu_LocalMessageEditorTitle());
	_box->setWidth(st::boxWideWidth);
	
	setupBasicFields();
	setupMessageTypeSection();
	setupAuthorSection();
	setupTimingSection();
	setupFlagsSection();
	setupStatsSection();
	setupReplySection();
	setupForwardSection();
	setupMediaSection();
	setupButtons();
	
	_scroll->setMaxHeight(st::boxMaxListHeight);
	updatePreview();
}

void LocalMessageEditor::setupBasicFields() {
	// Text field
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageText(),
		st::boxLabel));
	
	_textField = _content->add(object_ptr<Ui::InputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageTextPlaceholder(),
		_data.text));
	_textField->setMaxLength(kMaxMessageLength);
	_textField->heightValue() | rpl::start_with_next([=](int height) {
		if (height > st::defaultInputField.heightMin * 3) {
			_textField->setMaxHeight(st::defaultInputField.heightMin * 3);
		}
	}, _textField->lifetime());
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupMessageTypeSection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageType(),
		st::boxLabel));
	
	_isServiceMessage = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageIsService(),
		_data.isService,
		st::defaultCheckbox));
	
	_serviceTextWrap = _content->add(object_ptr<Ui::SlideWrap<Ui::InputField>>(
		_content,
		object_ptr<Ui::InputField>(
			_content,
			st::defaultInputField,
			tr::ayu_LocalMessageServiceText(),
			_data.serviceText)));
	
	_serviceTextWrap->toggle(_data.isService, anim::type::instant);
	
	_isServiceMessage->checkedChanges() | rpl::start_with_next([=](bool checked) {
		_data.isService = checked;
		_serviceTextWrap->toggle(checked, anim::type::normal);
	}, _isServiceMessage->lifetime());
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupAuthorSection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageAuthor(),
		st::boxLabel));
	
	// From user field with userpic
	const auto fromContainer = _content->add(object_ptr<Ui::RpWidget>(_content));
	fromContainer->resize(fromContainer->width(), st::defaultInputField.height);
	
	_fromUserpic = Ui::CreateChild<Ui::UserpicButton>(
		fromContainer,
		st::defaultUserpicButton);
	_fromUserpic->move(0, 0);
	
	_fromField = Ui::CreateChild<Ui::InputField>(
		fromContainer,
		st::defaultInputField,
		tr::ayu_LocalMessageFrom(),
		QString());
	_fromField->move(st::defaultUserpicButton.size.width() + st::boxMediumSkip, 0);
	
	fromContainer->widthValue() | rpl::start_with_next([=](int width) {
		_fromField->resize(
			width - st::defaultUserpicButton.size.width() - st::boxMediumSkip,
			st::defaultInputField.height);
	}, fromContainer->lifetime());
	
	// Post author field (for channels)
	_postAuthorField = _content->add(object_ptr<Ui::InputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessagePostAuthor(),
		_data.postAuthor));
	_postAuthorField->setMaxLength(kMaxPostAuthorLength);
	
	// Via bot field
	_viaBotField = _content->add(object_ptr<Ui::InputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageViaBot(),
		QString()));
	
	updateFromUser();
	
	_fromField->changes() | rpl::start_with_next([=] {
		updateFromUser();
	}, _fromField->lifetime());
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupTimingSection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageTiming(),
		st::boxLabel));
	
	// Date and time fields
	const auto dateTimeContainer = _content->add(object_ptr<Ui::RpWidget>(_content));
	dateTimeContainer->resize(dateTimeContainer->width(), st::defaultInputField.height);
	
	_dateField = Ui::CreateChild<Ui::MaskedInputField>(
		dateTimeContainer,
		st::defaultInputField,
		tr::ayu_LocalMessageDate(),
		QString());
	_dateField->setMask("99.99.9999");
	_dateField->move(0, 0);
	
	_timeField = Ui::CreateChild<Ui::MaskedInputField>(
		dateTimeContainer,
		st::defaultInputField,
		tr::ayu_LocalMessageTime(),
		QString());
	_timeField->setMask("99:99:99");
	
	dateTimeContainer->widthValue() | rpl::start_with_next([=](int width) {
		const auto fieldWidth = (width - st::boxMediumSkip) / 2;
		_dateField->resize(fieldWidth, st::defaultInputField.height);
		_timeField->resize(fieldWidth, st::defaultInputField.height);
		_timeField->move(fieldWidth + st::boxMediumSkip, 0);
	}, dateTimeContainer->lifetime());
	
	// Edit date section
	_wasEditedCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageWasEdited(),
		_data.wasEdited,
		st::defaultCheckbox));
	
	const auto editDateContainer = _content->add(object_ptr<Ui::RpWidget>(_content));
	editDateContainer->resize(editDateContainer->width(), st::defaultInputField.height);
	
	_editDateField = Ui::CreateChild<Ui::MaskedInputField>(
		editDateContainer,
		st::defaultInputField,
		tr::ayu_LocalMessageEditDate(),
		QString());
	_editDateField->setMask("99.99.9999");
	_editDateField->move(0, 0);
	
	_editTimeField = Ui::CreateChild<Ui::MaskedInputField>(
		editDateContainer,
		st::defaultInputField,
		tr::ayu_LocalMessageEditTime(),
		QString());
	_editTimeField->setMask("99:99:99");
	
	editDateContainer->widthValue() | rpl::start_with_next([=](int width) {
		const auto fieldWidth = (width - st::boxMediumSkip) / 2;
		_editDateField->resize(fieldWidth, st::defaultInputField.height);
		_editTimeField->resize(fieldWidth, st::defaultInputField.height);
		_editTimeField->move(fieldWidth + st::boxMediumSkip, 0);
	}, editDateContainer->lifetime());
	
	editDateContainer->setVisible(_data.wasEdited);
	
	_wasEditedCheck->checkedChanges() | rpl::start_with_next([=](bool checked) {
		_data.wasEdited = checked;
		editDateContainer->setVisible(checked);
	}, _wasEditedCheck->lifetime());
	
	updateDateTime();
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupFlagsSection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageFlags(),
		st::boxLabel));
	
	_silentCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageSilent(),
		_data.silent,
		st::defaultCheckbox));
	
	_pinnedCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessagePinned(),
		_data.pinned,
		st::defaultCheckbox));
	
	_noForwardsCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageNoForwards(),
		_data.noForwards,
		st::defaultCheckbox));
	
	_invertMediaCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageInvertMedia(),
		_data.invertMedia,
		st::defaultCheckbox));
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupStatsSection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageStats(),
		st::boxLabel));
	
	// Views
	_hasViewsCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageHasViews(),
		_data.hasViews,
		st::defaultCheckbox));
	
	_viewsField = _content->add(object_ptr<Ui::MaskedInputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageViews(),
		QString::number(_data.views)));
	_viewsField->setMask("999999999");
	_viewsField->setVisible(_data.hasViews);
	
	_hasViewsCheck->checkedChanges() | rpl::start_with_next([=](bool checked) {
		_data.hasViews = checked;
		_viewsField->setVisible(checked);
	}, _hasViewsCheck->lifetime());
	
	// Shares
	_hasSharesCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageHasShares(),
		_data.hasShares,
		st::defaultCheckbox));
	
	_sharesField = _content->add(object_ptr<Ui::MaskedInputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageShares(),
		QString::number(_data.shares)));
	_sharesField->setMask("999999999");
	_sharesField->setVisible(_data.hasShares);
	
	_hasSharesCheck->checkedChanges() | rpl::start_with_next([=](bool checked) {
		_data.hasShares = checked;
		_sharesField->setVisible(checked);
	}, _hasSharesCheck->lifetime());
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupReplySection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageReply(),
		st::boxLabel));
	
	_replyToField = _content->add(object_ptr<Ui::InputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageReplyTo(),
		QString()));
	
	_replyQuoteField = _content->add(object_ptr<Ui::InputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageReplyQuote(),
		QString()));
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupForwardSection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageForward(),
		st::boxLabel));
	
	_isForwardedCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageIsForwarded(),
		_data.isForwarded,
		st::defaultCheckbox));
	
	const auto forwardContent = _content->add(object_ptr<Ui::VerticalLayout>(_content));
	_forwardWrap = _content->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		_content,
		object_ptr<Ui::VerticalLayout>(_content)));
	
	auto forwardFields = _forwardWrap->entity();
	
	_forwardFromField = forwardFields->add(object_ptr<Ui::InputField>(
		forwardFields,
		st::defaultInputField,
		tr::ayu_LocalMessageForwardFrom(),
		QString()));
	
	_forwardFromNameField = forwardFields->add(object_ptr<Ui::InputField>(
		forwardFields,
		st::defaultInputField,
		tr::ayu_LocalMessageForwardFromName(),
		_data.forwardFromName));
	
	// Forward date and time
	const auto forwardDateContainer = forwardFields->add(object_ptr<Ui::RpWidget>(forwardFields));
	forwardDateContainer->resize(forwardDateContainer->width(), st::defaultInputField.height);
	
	_forwardDateField = Ui::CreateChild<Ui::MaskedInputField>(
		forwardDateContainer,
		st::defaultInputField,
		tr::ayu_LocalMessageForwardDate(),
		QString());
	_forwardDateField->setMask("99.99.9999");
	_forwardDateField->move(0, 0);
	
	_forwardTimeField = Ui::CreateChild<Ui::MaskedInputField>(
		forwardDateContainer,
		st::defaultInputField,
		tr::ayu_LocalMessageForwardTime(),
		QString());
	_forwardTimeField->setMask("99:99:99");
	
	forwardDateContainer->widthValue() | rpl::start_with_next([=](int width) {
		const auto fieldWidth = (width - st::boxMediumSkip) / 2;
		_forwardDateField->resize(fieldWidth, st::defaultInputField.height);
		_forwardTimeField->resize(fieldWidth, st::defaultInputField.height);
		_forwardTimeField->move(fieldWidth + st::boxMediumSkip, 0);
	}, forwardDateContainer->lifetime());
	
	_forwardPostAuthorField = forwardFields->add(object_ptr<Ui::InputField>(
		forwardFields,
		st::defaultInputField,
		tr::ayu_LocalMessageForwardPostAuthor(),
		_data.forwardPostAuthor));
	
	_forwardWrap->toggle(_data.isForwarded, anim::type::instant);
	
	_isForwardedCheck->checkedChanges() | rpl::start_with_next([=](bool checked) {
		_data.isForwarded = checked;
		_forwardWrap->toggle(checked, anim::type::normal);
	}, _isForwardedCheck->lifetime());
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupMediaSection() {
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageMedia(),
		st::boxLabel));
	
	_hasMediaCheck = _content->add(object_ptr<Ui::Checkbox>(
		_content,
		tr::ayu_LocalMessageHasMedia(),
		_data.hasMedia,
		st::defaultCheckbox));
	
	const auto mediaContent = _content->add(object_ptr<Ui::VerticalLayout>(_content));
	_mediaWrap = _content->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		_content,
		object_ptr<Ui::VerticalLayout>(_content)));
	
	auto mediaFields = _mediaWrap->entity();
	
	_mediaPathField = mediaFields->add(object_ptr<Ui::InputField>(
		mediaFields,
		st::defaultInputField,
		tr::ayu_LocalMessageMediaPath(),
		_data.mediaPath));
	
	_mediaCaptionField = mediaFields->add(object_ptr<Ui::InputField>(
		mediaFields,
		st::defaultInputField,
		tr::ayu_LocalMessageMediaCaption(),
		_data.mediaCaption));
	_mediaCaptionField->setMaxLength(kMaxCaptionLength);
	
	_mediaWrap->toggle(_data.hasMedia, anim::type::instant);
	
	_hasMediaCheck->checkedChanges() | rpl::start_with_next([=](bool checked) {
		_data.hasMedia = checked;
		_mediaWrap->toggle(checked, anim::type::normal);
	}, _hasMediaCheck->lifetime());
	
	// Advanced fields
	_content->add(object_ptr<Ui::FlatLabel>(
		_content,
		tr::ayu_LocalMessageAdvanced(),
		st::boxLabel));
	
	_starsPaidField = _content->add(object_ptr<Ui::MaskedInputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageStarsPaid(),
		QString::number(_data.starsPaid)));
	_starsPaidField->setMask("999999999");
	
	_effectIdField = _content->add(object_ptr<Ui::MaskedInputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageEffectId(),
		QString::number(_data.effectId)));
	_effectIdField->setMask("999999999999999999");
	
	_groupedIdField = _content->add(object_ptr<Ui::MaskedInputField>(
		_content,
		st::defaultInputField,
		tr::ayu_LocalMessageGroupedId(),
		QString::number(_data.groupedId)));
	_groupedIdField->setMask("999999999999999999");
	
	_content->add(object_ptr<Ui::FixedHeightWidget>(_content, st::boxMediumSkip));
}

void LocalMessageEditor::setupButtons() {
	_box->addButton(tr::lng_box_ok(), [=] {
		saveMessage();
		_box->closeBox();
	});
	
	_box->addButton(tr::lng_cancel(), [=] {
		_box->closeBox();
	});
	
	_box->setFocusCallback([=] {
		_textField->setFocusFast();
	});
}

void LocalMessageEditor::updateFromUser() {
	const auto text = _fromField->getLastText().trimmed();
	if (text.isEmpty()) {
		_data.fromId = _history->peer->id;
		if (const auto user = _history->peer->asUser()) {
			_fromUserpic->showUser(user);
		} else {
			_fromUserpic->showPeer(_history->peer);
		}
		return;
	}
	
	// Try to find user by username or name
	const auto &owner = _controller->session().data();
	if (const auto user = owner.userByUsername(text)) {
		_data.fromId = user->id;
		_fromUserpic->showUser(user);
	} else {
		// Search by name in chat participants
		if (const auto chat = _history->peer->asChat()) {
			for (const auto &participant : chat->participants) {
				if (const auto user = owner.user(participant.userId())) {
					if (user->name().toLower().contains(text.toLower())) {
						_data.fromId = user->id;
						_fromUserpic->showUser(user);
						return;
					}
				}
			}
		}
		// If not found, keep current peer
		_fromUserpic->showPeer(_history->peer);
	}
}

void LocalMessageEditor::updateDateTime() {
	const auto dateTime = QDateTime::fromSecsSinceEpoch(_data.date);
	_dateField->setText(dateTime.date().toString("dd.MM.yyyy"));
	_timeField->setText(dateTime.time().toString("hh:mm:ss"));
	
	if (_data.wasEdited && _data.editDate > 0) {
		const auto editDateTime = QDateTime::fromSecsSinceEpoch(_data.editDate);
		_editDateField->setText(editDateTime.date().toString("dd.MM.yyyy"));
		_editTimeField->setText(editDateTime.time().toString("hh:mm:ss"));
	}
}

void LocalMessageEditor::updatePreview() {
	// TODO: Add preview functionality
}

void LocalMessageEditor::saveMessage() {
	// Collect data from fields
	_data.text = _textField->getLastText();
	_data.postAuthor = _postAuthorField->getLastText();
	_data.serviceText = _serviceTextWrap->entity()->getLastText();
	
	// Parse date and time
	const auto dateText = _dateField->getLastText();
	const auto timeText = _timeField->getLastText();
	if (!dateText.isEmpty() && !timeText.isEmpty()) {
		const auto dateTime = QDateTime::fromString(
			dateText + " " + timeText,
			"dd.MM.yyyy hh:mm:ss");
		if (dateTime.isValid()) {
			_data.date = dateTime.toSecsSinceEpoch();
		}
	}
	
	// Parse edit date if applicable
	if (_data.wasEdited) {
		const auto editDateText = _editDateField->getLastText();
		const auto editTimeText = _editTimeField->getLastText();
		if (!editDateText.isEmpty() && !editTimeText.isEmpty()) {
			const auto editDateTime = QDateTime::fromString(
				editDateText + " " + editTimeText,
				"dd.MM.yyyy hh:mm:ss");
			if (editDateTime.isValid()) {
				_data.editDate = editDateTime.toSecsSinceEpoch();
			}
		}
	}
	
	// Collect flags
	_data.silent = _silentCheck->checked();
	_data.pinned = _pinnedCheck->checked();
	_data.noForwards = _noForwardsCheck->checked();
	_data.invertMedia = _invertMediaCheck->checked();
	
	// Collect stats
	if (_data.hasViews) {
		_data.views = _viewsField->getLastText().toInt();
	}
	if (_data.hasShares) {
		_data.shares = _sharesField->getLastText().toInt();
	}
	
	// Collect advanced fields
	_data.starsPaid = _starsPaidField->getLastText().toInt();
	_data.effectId = _effectIdField->getLastText().toULongLong();
	_data.groupedId = _groupedIdField->getLastText().toULongLong();
	
	// Create the message
	const auto localId = _controller->session().data().nextLocalMessageId();
	const auto fields = buildFields();
	
	HistoryItem* localItem = nullptr;
	
	if (_data.isService) {
		// Create service message
		// TODO: Implement service message creation
	} else {
		// Create regular message
		localItem = _history->addNewLocalMessage(
			HistoryItemCommonFields{
				.id = localId,
				.flags = buildFlags(),
				.from = _data.fromId,
				.date = _data.date,
				.postAuthor = _data.postAuthor,
				.groupedId = _data.groupedId,
				.effectId = _data.effectId,
			},
			TextWithEntities{ _data.text },
			MTP_messageMediaEmpty());
	}
	
	if (localItem) {
		// Add to local messages database
		AyuMessages::addLocalMessage(localItem);
	}
}

MessageFlags LocalMessageEditor::buildFlags() const {
	MessageFlags flags = MessageFlag::HasFromId | MessageFlag::Local;
	
	if (_data.silent) flags |= MessageFlag::Silent;
	if (_data.pinned) flags |= MessageFlag::Pinned;
	if (_data.noForwards) flags |= MessageFlag::NoForwards;
	if (_data.invertMedia) flags |= MessageFlag::InvertMedia;
	if (_data.hasViews) flags |= MessageFlag::HasViews;
	if (!_data.postAuthor.isEmpty()) flags |= MessageFlag::HasPostAuthor;
	if (_data.wasEdited) flags |= MessageFlag::HideEdited; // Will show as edited
	if (_data.isForwarded) {
		// TODO: Add forward flags
	}
	
	return flags;
}

HistoryItemCommonFields LocalMessageEditor::buildFields() const {
	return HistoryItemCommonFields{
		.id = 0, // Will be set by caller
		.flags = buildFlags(),
		.from = _data.fromId,
		.date = _data.date,
		.starsPaid = _data.starsPaid,
		.viaBotId = UserId(_data.viaBotId),
		.postAuthor = _data.postAuthor,
		.groupedId = _data.groupedId,
		.effectId = _data.effectId,
	};
}

} // namespace

void LocalMessageEditorBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		const LocalMessageData &initialData) {
	const auto editor = box->lifetime().make_state<LocalMessageEditor>(
		box,
		controller,
		history,
		initialData);
}

} // namespace AyuUi