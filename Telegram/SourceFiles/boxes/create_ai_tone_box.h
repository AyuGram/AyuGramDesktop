/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Data {
struct AiComposeTone;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class AbstractButton;
class GenericBox;
class Show;
class VerticalLayout;
} // namespace Ui

not_null<Ui::AbstractButton*> AddAiToneIconPreview(
	not_null<Ui::VerticalLayout*> container,
	not_null<Main::Session*> session,
	rpl::producer<DocumentId> emojiIdValue,
	Fn<void(DocumentId)> emojiIdChosen = nullptr);

void CreateAiToneBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session,
	Fn<void(Data::AiComposeTone)> saved = nullptr);

void EditAiToneBox(
	not_null<Ui::GenericBox*> box,
	not_null<Main::Session*> session,
	const Data::AiComposeTone &tone,
	Fn<void(Data::AiComposeTone)> saved = nullptr);

void ConfirmDeleteAiTone(
	std::shared_ptr<Ui::Show> show,
	not_null<Main::Session*> session,
	const Data::AiComposeTone &tone,
	Fn<void()> done = nullptr);

void ShowAiComposeToneLimitError(
	std::shared_ptr<Ui::Show> show,
	not_null<Main::Session*> session);
