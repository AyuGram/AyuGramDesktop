// Drakeygram Desktop
// Fully independent fork. No external tracking.

#include "ayu/ayu_url_handlers.h"

#include "lang_auto.h"
#include "mainwindow.h"
#include "ayu/ui/settings/settings_main.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/qthelp_url.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "ui/boxes/confirm_box.h"
#include "ui/boxes/donate_info_box.h"
#include "window/window_controller.h"

#include <QDesktopServices>

namespace AyuUrlHandlers {

bool ResolveUser(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context) {
	if (!controller) {
		return false;
	}
	const auto params = url_parse_params(
		match->captured(1),
		qthelp::UrlParamNameTransform::ToLower);
	const auto userId = params.value(qsl("id")).toLongLong();
	if (!userId) {
		return false;
	}
	const auto peer = controller->session().data().peerLoaded(static_cast<PeerId>(userId));
	if (peer != nullptr) {
		controller->showPeerInfo(peer);
		return true;
	}

	searchUserById(
		userId,
		&controller->session(),
		[=](const QString &title, PeerData *data)
		{
			if (data) {
				controller->showPeerInfo(data);
				return;
			}

			Core::App().hideMediaView();
			Ui::show(Ui::MakeInformBox(tr::ayu_UserNotFoundMessage()));
		}
	);

	return true;
}

bool ResolveChat(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context) {
	if (!controller) {
		return false;
	}
	const auto params = url_parse_params(
		match->captured(1),
		qthelp::UrlParamNameTransform::ToLower);
	const auto chatId = params.value(qsl("id")).toLongLong();
	if (!chatId) {
		return false;
	}
	const auto peer = controller->session().data().peerLoaded(static_cast<PeerId>(chatId));
	if (peer != nullptr) {
		controller->showPeerHistory(peer);
		return true;
	}

	searchChatById(
		chatId,
		&controller->session(),
		[=](const QString &title, PeerData *data)
		{
			if (data) {
				controller->showPeerHistory(data);
				return;
			}

			Core::App().hideMediaView();
			Ui::show(Ui::MakeInformBox(tr::ayu_UserNotFoundMessage()));
		}
	);

	return true;
}

bool HandleAyu(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context) {
	if (!controller) {
		return false;
	}

	try {
		const auto section = match->captured(1).mid(1).toLower();
		const auto type = [&]() -> std::optional<::Settings::Type>
		{
			if (section == u"settings"_q || section == u"preferences"_q || section == u"prefs"_q) {
				return ::Settings::AyuMain::Id();
			}
			return std::nullopt;
		}();

		if (type.has_value()) {
			controller->showSettings(*type);
			controller->window().activate();
		} else {
			controller->showToast(QString(":3"), 500);
		}
	} catch (...) {
	}

	return true;
}

bool HandleSupport(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context) {
	if (!controller) {
		return false;
	}
	// Donate/support box neutralized - no external AyuGram links
	controller->showToast(QString("Drakeygram"), 500);
	return true;
}

struct ResolvedSetting {
	QString controlId;
	::Settings::Type section = ::Settings::AyuMain::Id();
};

[[nodiscard]] ResolvedSetting ResolveSetting(
		const QString &controlId,
		not_null<::Main::Session*> session) {
	const auto &registry = ::Settings::Builder::SearchRegistry::Instance();
	const auto entries = registry.collectAll(session);
	for (const auto &entry : entries) {
		if (!entry.section) {
			continue;
		}
		if (entry.id == controlId) {
			return {
				.controlId = entry.id,
				.section = entry.section,
			};
		}
		if (entry.altIds.contains(controlId)) {
			return {
				.controlId = entry.id,
				.section = entry.section,
			};
		}
	}
	return { .controlId = controlId };
}

bool HandleAyuSettings(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context) {
	if (!controller) {
		return false;
	}

	const auto params = url_parse_params(
		match->captured(1),
		qthelp::UrlParamNameTransform::ToLower);
	const auto settingName = params.value(u"s"_q);

	if (settingName.isEmpty()) {
		controller->showSettings(::Settings::AyuMain::Id());
	} else {
		const auto resolved = ResolveSetting(
			u"ayu/"_q + settingName,
			&controller->session());
		controller->window().setHighlightControlId(resolved.controlId);
		controller->showSettings(resolved.section);
	}
	controller->window().activate();
	return true;
}

bool TryHandleSpotify(const QString &url) {
	if (!url.contains("spotify.com")) {
		return false;
	}

	using namespace qthelp;
	constexpr auto matchOptions = RegExOption::CaseInsensitive;
	const auto match = regex_match(
		u"^(https?:\\/\\/)?([a-zA-Z0-9_]+)\\.spotify\\.com\\/(?<type>track|album|artist|user|playlist)\\/(?<identifier>[a-zA-Z0-9_\\/]+?)((\\?si=.+)?)$"_q,
		url,
		matchOptions);
	if (match) {
		const auto type = match->captured("type").toLower();
		const auto identifier = match->captured("identifier").replace("/", ":");
		const auto res = QString("spotify:%1:%2").arg(type).arg(identifier);
		return QDesktopServices::openUrl(QUrl(res));
	}

	return false;
}

bool IsWebUrl(const QUrl &url) {
	const auto scheme = url.scheme().toLower();
	return (scheme == u"http"_q) || (scheme == u"https"_q);
}

bool IsUnsafeExternalUrl(const QString &url) {
	static const auto kBlocked = base::flat_set<QString>{
		u"file"_q,
		u"smb"_q,
		u"nfs"_q,
		u"afp"_q,
		u"cifs"_q,
		u"dav"_q,
		u"davs"_q,
		u"webdav"_q,
		u"search"_q,
		u"search-ms"_q,
		u"shell"_q,
		u"javascript"_q,
		u"vbscript"_q,
		u"jar"_q,
		u"jnlp"_q,
		u"jnlps"_q,
		u"mk"_q,
		u"its"_q,
		u"mhtml"_q,
		u"hcp"_q,
		u"res"_q,
	};
	const auto scheme = QUrl(url).scheme().toLower();
	return (scheme.size() < 2)
		|| kBlocked.contains(scheme)
		|| scheme.startsWith(u"ms-"_q);
}

}
