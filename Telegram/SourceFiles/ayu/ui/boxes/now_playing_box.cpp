// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/now_playing_box.h"

#include "base/weak_ptr.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/utils/now_playing_client.h"
#include "ayu/utils/telegram_helpers.h"
#include "lang/lang_text_entity.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/vertical_list.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "main/main_session.h"

namespace Ui {

void SetupNowPlayingBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::ayu_ScrobblingService());

	const auto content = box->verticalLayout();

	Ui::AddSubsectionTitle(
		content,
		tr::ayu_SelectServiceTitle(),
		{ 0, st::settingsPrivacySkipTop, 0, 0 });

	const auto initialService = AyuSettings::getInstance().nowPlayingService();
	const auto group = std::make_shared<Ui::RadioenumGroup<NowPlayingServiceType>>(
		initialService);

	const auto addOption = [&](NowPlayingServiceType type, const QString &text) {
		content->add(
			object_ptr<Ui::Radioenum<NowPlayingServiceType>>(
				content,
				group,
				type,
				text,
				st::settingsPrivacyOption),
			st::settingsSendTypePadding + style::margins(
				-st::lineWidth,
				st::settingsPrivacySkipTop,
				0,
				0));
	};

	addOption(NowPlayingServiceType::None, tr::ayu_ScrobblingNone(tr::now));
	addOption(NowPlayingServiceType::LastFm, tr::ayu_ScrobblingLastFm(tr::now));
	addOption(NowPlayingServiceType::StatsFm, tr::ayu_ScrobblingStatsFm(tr::now));

	Ui::AddSkip(content);
	Ui::AddDividerText(content, tr::ayu_SelectServiceInfo());

	const auto slideWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)));

	const auto inner = slideWrap->entity();

	const auto usernameField = inner->add(
		object_ptr<Ui::InputField>(
			inner,
			st::defaultInputField,
			tr::ayu_EnterUsername(),
			AyuSettings::getInstance().nowPlayingUsername()),
		st::boxRowPadding + style::margins(0, st::settingsPrivacySkipTop, 0, st::settingsPrivacySkipTop));

	const auto linkProducer = group->value() | rpl::map([=](NowPlayingServiceType service) {
		const auto url = (service == NowPlayingServiceType::StatsFm)
			? u"https://stats.fm"_q
			: u"https://www.last.fm"_q;
		const auto link = Ui::Text::Link(tr::ayu_EnterUsernameOfficialWebsite(tr::now), url);
		return tr::ayu_EnterUsernameInfo(
			tr::now,
			lt_link,
			link,
			tr::marked);
	});

	Ui::AddSkip(inner);
	Ui::AddDividerText(inner, std::move(linkProducer));

	if (initialService == NowPlayingServiceType::None) {
		slideWrap->hide(anim::type::instant);
	}

	group->setChangedCallback([=](NowPlayingServiceType service) {
		const auto visible = (service != NowPlayingServiceType::None);
		slideWrap->toggle(visible, anim::type::normal);
		if (visible) {
			usernameField->setFocus();
		}
	});

	const auto userId = controller->session().userId().bare;
	Ayu::NowPlaying::FetchProfile(userId, crl::guard(box, [=](NowPlayingServiceType service, const QString &username) {
		if (group->current() == initialService) {
			group->setValue(service);
			if (usernameField->getLastText().trimmed().isEmpty()
				|| usernameField->getLastText() == AyuSettings::getInstance().nowPlayingUsername()) {
				usernameField->setText(username);
			}
		}
	}));

	const auto save = [=] {
		const auto selectedService = group->current();
		const auto username = usernameField->getLastText().trimmed();

		if (selectedService != NowPlayingServiceType::None && username.isEmpty()) {
			usernameField->showError();
			return;
		}

		const auto session = &controller->session();
		updateNowPlayingService(
			session,
			selectedService,
			username,
			crl::guard(box, [=, weakController = base::make_weak(controller)](bool success) {
				const auto strongController = weakController.get();
				if (success) {
					AyuSettings::getInstance().setNowPlayingService(selectedService);
					AyuSettings::getInstance().setNowPlayingUsername(
						(selectedService == NowPlayingServiceType::None)
							? QString()
							: username);
					if (strongController) {
						strongController->showToast(tr::ayu_ScrobblingSavedToast(tr::now));
					}
					box->closeBox();
				} else {
					if (strongController) {
						strongController->showToast(tr::ayu_ScrobblingErrorToast(tr::now));
					}
				}
			}));
	};

	usernameField->submits() | rpl::on_next(save, usernameField->lifetime());

	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace Ui
