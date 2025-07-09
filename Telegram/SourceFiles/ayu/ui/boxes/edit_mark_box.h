// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "base/timer.h"
#include "boxes/abstract_box.h"

namespace Ui {
class InputField;
}

class EditMarkBox : public Ui::BoxContent
{
public:
	EditMarkBox(QWidget *, rpl::producer<QString> title, const QString& currentValue, QString  defaultValue, const Fn<void(const QString&)> &saveCallback);

protected:
	void setInnerFocus() override;
	void prepare() override;
	void resizeEvent(QResizeEvent *e) override;

private:
	void submit();
	void save();

	rpl::producer<QString> _title;
	QString _defaultValue;
	Fn<void(const QString&)> _saveCallback;

	object_ptr<Ui::InputField> _text;
};
