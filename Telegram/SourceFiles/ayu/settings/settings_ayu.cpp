#include "ayu/boxes/edit_edited_mark.h"
#include "ayu/boxes/edit_deleted_mark.h"
#include "ayu/ayu_settings.h"
#include "ayu/settings/settings_ayu.h"

#include "mainwindow.h"
#include "settings/settings_common.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "boxes/connection_box.h"
#include "ui/boxes/confirm_box.h"
#include "platform/platform_specific.h"
#include "window/window_session_controller.h"
#include "lang/lang_instance.h"
#include "core/application.h"
#include "storage/localstorage.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "styles/style_settings.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"

namespace Settings {

    rpl::producer<QString> Ayu::title() {
        return rpl::single(QString("AyuGram Settings"));
    }

    Ayu::Ayu(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent) {
        setupContent(controller);
    }

    void Ayu::SetupAyuGramSettings(not_null<Ui::VerticalLayout *> container,
                                   not_null<Window::SessionController *> controller) {
        auto settings = &AyuSettings::getInstance();

        AddSubsectionTitle(container, rpl::single(QString("General")));

        AddButton(
                container,
                rpl::single(QString("Send read packets")),
                st::settingsButtonNoIcon
        )->toggleOn(
                rpl::single(settings->sendReadPackets)
        )->toggledValue(
        ) | rpl::filter([=](bool enabled) {
            return (enabled != settings->sendReadPackets);
        }) | rpl::start_with_next([=](bool enabled) {
            settings->set_sendReadPackets(enabled);
            Local::writeSettings();
        }, container->lifetime());

        AddButton(
                container,
                rpl::single(QString("Send online packets")),
                st::settingsButtonNoIcon
        )->toggleOn(
                rpl::single(settings->sendOnlinePackets)
        )->toggledValue(
        ) | rpl::filter([=](bool enabled) {
            return (enabled != settings->sendOnlinePackets);
        }) | rpl::start_with_next([=](bool enabled) {
            settings->set_sendOnlinePackets(enabled);
            Local::writeSettings();
        }, container->lifetime());

        AddButton(
                container,
                rpl::single(QString("Send offline packet after online")),
                st::settingsButtonNoIcon
        )->toggleOn(
                rpl::single(settings->sendOfflinePacketAfterOnline)
        )->toggledValue(
        ) | rpl::filter([=](bool enabled) {
            return (enabled != settings->sendOfflinePacketAfterOnline);
        }) | rpl::start_with_next([=](bool enabled) {
            settings->set_sendOfflinePacketAfterOnline(enabled);
            Local::writeSettings();
        }, container->lifetime());

        AddButton(
                container,
                rpl::single(QString("Send typing & upload progress")),
                st::settingsButtonNoIcon
        )->toggleOn(
                rpl::single(settings->sendUploadProgress)
        )->toggledValue(
        ) | rpl::filter([=](bool enabled) {
            return (enabled != settings->sendUploadProgress);
        }) | rpl::start_with_next([=](bool enabled) {
            settings->set_sendUploadProgress(enabled);
            Local::writeSettings();
        }, container->lifetime());

        AddButton(
                container,
                rpl::single(QString("Use scheduled messages to keep offline")),
                st::settingsButtonNoIcon
        )->toggleOn(
                rpl::single(settings->useScheduledMessages)
        )->toggledValue(
        ) | rpl::filter([=](bool enabled) {
            return (enabled != settings->useScheduledMessages);
        }) | rpl::start_with_next([=](bool enabled) {
            settings->set_useScheduledMessages(enabled);
            Local::writeSettings();
        }, container->lifetime());

        AddButton(
                container,
                rpl::single(QString("Keep deleted messages")),
                st::settingsButtonNoIcon
        )->toggleOn(
                rpl::single(settings->keepDeletedMessages)
        )->toggledValue(
        ) | rpl::filter([=](bool enabled) {
            return (enabled != settings->keepDeletedMessages);
        }) | rpl::start_with_next([=](bool enabled) {
            settings->set_keepDeletedMessages(enabled);
            Local::writeSettings();
        }, container->lifetime());

        AddButton(
                container,
                rpl::single(QString("Keep messages' history")),
                st::settingsButtonNoIcon
        )->toggleOn(
                rpl::single(settings->keepMessagesHistory)
        )->toggledValue(
        ) | rpl::filter([=](bool enabled) {
            return (enabled != settings->keepMessagesHistory);
        }) | rpl::start_with_next([=](bool enabled) {
            settings->set_keepMessagesHistory(enabled);
            Local::writeSettings();
        }, container->lifetime());

        auto currentDeletedMark = lifetime().make_state<rpl::variable<QString>>();

        auto btn = AddButtonWithLabel(
                container,
                rpl::single(QString("Deleted Mark")),
                currentDeletedMark->changes(),
                st::settingsButtonNoIcon
        );
        btn->addClickHandler([=]() {
            auto box = Box<EditDeletedMarkBox>();
            box->boxClosing() | rpl::start_with_next([=]() {
                *currentDeletedMark = settings->deletedMark;
            }, container->lifetime());

            Ui::show(std::move(box));
        });
        *currentDeletedMark = settings->deletedMark;

        auto currentEditedMark = lifetime().make_state<rpl::variable<QString>>();

        auto btn2 = AddButtonWithLabel(
                container,
                rpl::single(QString("Edited Mark")),
                currentEditedMark->changes(),
                st::settingsButtonNoIcon
        );
        btn2->addClickHandler([=]() {
            auto box = Box<EditEditedMarkBox>();
            box->boxClosing() | rpl::start_with_next([=]() {
                *currentEditedMark = settings->editedMark;
            }, container->lifetime());

            Ui::show(std::move(box));
        });
        *currentEditedMark = settings->editedMark;

        AddDividerText(container, rpl::single(QString("AyuGram developed and maintained by Radolyn Labs")));
    }

    void Ayu::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupAyuGramSettings(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

