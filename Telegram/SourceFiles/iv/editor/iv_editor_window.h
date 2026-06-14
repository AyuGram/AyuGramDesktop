/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "ui/widgets/rp_window.h"

#include <rpl/event_stream.h>

namespace Iv::Editor {

class Window final : public Ui::RpWindow {
public:
	explicit Window(QWidget *parent = nullptr);

	[[nodiscard]] rpl::producer<> imeCompositionStarts() const;
	void imeCompositionStartReceived();

protected:
#ifdef Q_OS_WIN
	bool nativeEvent(
		const QByteArray &eventType,
		void *message,
		native_event_filter_result *result) override;
#elif defined Q_OS_MAC // Q_OS_WIN
	bool nativeEvent(
		const QByteArray &eventType,
		void *message,
		qintptr *result) override;
#endif // Q_OS_WIN || Q_OS_MAC

private:
	rpl::event_stream<> _imeCompositionStartReceived;

};

} // namespace Iv::Editor
