/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <rpl/producer.h>

#include <QtCore/QPointer>
#include <QtCore/QString>

#include <memory>

class PeerData;
class QWidget;

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class RpWidget;
} // namespace Ui

namespace Iv {
enum class RichMessageLimitError : unsigned char;
} // namespace Iv

namespace Iv::Editor {

class State;
class Widget;

struct ShowWindowDescriptor {
	enum class SubmitType {
		Send,
		Save,
	};

	not_null<Main::Session*> session;
	std::shared_ptr<ChatHelpers::Show> show;
	not_null<PeerData*> peer;
	std::shared_ptr<State> state;
	QString submitLabel;
	SubmitType submitType = SubmitType::Send;
	Fn<bool()> customEmojiPaused;
	Fn<bool()> cancelled;
	Fn<bool()> confirmed;
	Fn<void(not_null<Ui::RpWidget*>)> setupSubmitButton;
	Fn<void(not_null<Widget*>, QPointer<QWidget>)> requestMedia;
	Fn<void(not_null<Widget*>, QPointer<QWidget>, rpl::producer<>)> requestMap;
	Fn<void()> closed;
	Fn<void(RichMessageLimitError)> showLimitToast;
};

class WindowHost final {
public:
	~WindowHost();

private:
	friend std::unique_ptr<WindowHost> ShowWindow(
		ShowWindowDescriptor descriptor);

	explicit WindowHost(ShowWindowDescriptor descriptor);

	struct Impl;
	std::unique_ptr<Impl> _impl;

};

std::unique_ptr<WindowHost> ShowWindow(ShowWindowDescriptor descriptor);

} // namespace Iv::Editor
