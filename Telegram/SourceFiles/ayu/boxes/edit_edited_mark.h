#pragma once

#include "boxes/abstract_box.h"
#include "base/timer.h"
#include "mtproto/sender.h"

class EditEditedMarkBox : public Ui::BoxContent {
public:
    EditEditedMarkBox(QWidget*);

protected:
    void setInnerFocus() override;
    void prepare() override;

    void resizeEvent(QResizeEvent *e) override;

private:
    void submit();
    void save();

    object_ptr<Ui::InputField> _text;
};