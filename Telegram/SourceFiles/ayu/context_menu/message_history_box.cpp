#include <styles/style_layers.h>
#include <styles/style_boxes.h>
#include <ui/effects/scroll_content_shadow.h>
#include <styles/style_settings.h>
#include "message_history_box.h"
#include "settings/settings_common.h"
#include "ayu/ayu_settings.h"
#include "ayu/database/ayu_database.h"
#include "history/history.h"

using namespace Settings;

namespace AyuUi {
    MessageHistoryBox::MessageHistoryBox(QWidget *, HistoryItem *item)
            : _content(this), _scroll(base::make_unique_q<Ui::ScrollArea>(this, st::boxScroll)) {
        setupControls();
        addEditedMessagesToLayout(item);
    }

    void MessageHistoryBox::setupControls() {
        _content.create(this);

        _content->resizeToWidth(st::boxWideWidth);
        _content->moveToLeft(0, 0);

        _content->heightValue(
        ) | rpl::start_to_stream(_contentHeight, _content->lifetime());

        _scroll->setOwnedWidget(
                object_ptr<Ui::RpWidget>::fromRaw(_content));
    }

    void MessageHistoryBox::resizeEvent(QResizeEvent *e) {
        _scroll->resize(width(), height() - st::boxPhotoPadding.top() - st::boxPadding.bottom());
        _scroll->move(0, st::boxPadding.top());

        if (_content) {
            _content->resize(_scroll->width(), _content->height());
        }
    }

    void MessageHistoryBox::prepare() {
        setTitle(tr::ayu_EditsHistoryTitle());

//        setDimensionsToContent(st::boxWideWidth, _content);
        setDimensions(st::boxWideWidth, 900);
        Ui::SetupShadowsToScrollContent(this, _scroll, _contentHeight.events());
    }

    void MessageHistoryBox::addEditedMessagesToLayout(HistoryItem *item) {
        auto messages = AyuDatabase::getEditedMessages(item);
        if (messages.empty()) {
            return;
        }

        for (const auto &message: messages) {
            AddSkip(_content);
            AddDividerText(_content, rpl::single(QString::fromStdString(message.text)));
            AddSkip(_content);
        }
    }
}