// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once

#include "history/history_item.h"
#include "ui/layers/box_content.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/vertical_layout.h"

namespace AyuUi
{

class MessageHistoryBox : public Ui::BoxContent
{
public:
	MessageHistoryBox(QWidget *, HistoryItem *item);

protected:
	void prepare() override;

	void resizeEvent(QResizeEvent *e) override;

private:
	void setupControls();

	void addEditedMessagesToLayout(HistoryItem *item);

	object_ptr<Ui::VerticalLayout> _content;
	const base::unique_qptr<Ui::ScrollArea> _scroll;

	rpl::event_stream<int> _contentHeight;
};

}
