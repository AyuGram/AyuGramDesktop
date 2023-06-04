#include "ayu/boxes/edit_edited_mark.h"
#include "ayu/boxes/edit_deleted_mark.h"
#include "ayu/ayu_settings.h"
#include "ayu/settings/settings_ayu.h"

#include "lang_auto.h"
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
        return tr::ayu_AyuPreferences();
    }

    Ayu::Ayu(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent) {
        setupContent(controller);
    }

    void Ayu::SetupAyuGramSettings(not_null<Ui::VerticalLayout *> container) {
        auto settings = &AyuSettings::getInstance();

        AddSkip(container);
        AddSubsectionTitle(container, tr::ayu_GhostEssentialsHeader());

        AddButton(
                container,
                tr::ayu_SendReadPackets(),
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
                tr::ayu_SendOnlinePackets(),
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
                tr::ayu_SendUploadProgress(),
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
                tr::ayu_SendOfflinePacketAfterOnline(),
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
                tr::ayu_UseScheduledMessages(),
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

        AddDivider(container);
        AddSkip(container);

        AddSubsectionTitle(container, tr::ayu_SpyEssentialsHeader());

        AddButton(
                container,
                tr::ayu_KeepDeletedMessages(),
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
                tr::ayu_KeepMessagesHistory(),
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

        AddDivider(container);
        AddSkip(container);

        AddSubsectionTitle(container, tr::ayu_CustomizationHeader());

        auto btn = AddButtonWithLabel(
                container,
                tr::ayu_DeletedMarkText(),
                AyuSettings::get_deletedMarkReactive().value(),
                st::settingsButtonNoIcon
        );
        btn->addClickHandler([=]() {
            auto box = Box<EditDeletedMarkBox>();
            Ui::show(std::move(box));
        });

        auto btn2 = AddButtonWithLabel(
                container,
                rpl::single(QString("Edited mark")),
                AyuSettings::get_editedMarkReactive().value(),
                st::settingsButtonNoIcon
        );
        btn2->addClickHandler([=]() {
            auto box = Box<EditEditedMarkBox>();
            Ui::show(std::move(box));
        });

        AddDividerText(container, tr::ayu_SettingsWatermark());
    }

    void Ayu::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupAyuGramSettings(content);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

