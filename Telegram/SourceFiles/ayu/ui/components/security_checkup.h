/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// A short list of the protections this install has on, with a bar that fills as they are switched on.
#pragma once

#include "base/object_ptr.h"
#include "base/timer.h"
#include "ui/effects/animation_value.h"
#include "ui/rp_widget.h"

class SecurityCheckup final : public Ui::RpWidget {
public:
	explicit SecurityCheckup(QWidget *parent);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	struct Row {
		QString label;
		bool on = false;

		friend inline bool operator==(const Row &, const Row &) = default;
	};

	void refresh();
	[[nodiscard]] int doneCount() const;

	std::vector<Row> _rows;
	base::Timer _timer;
	object_ptr<Ui::RpWidget> _icon = { nullptr };
	Fn<void(anim::repeat)> _animate;

};
