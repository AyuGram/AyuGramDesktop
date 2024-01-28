// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu/ayu_url_handlers.h"

#include "base/qthelp_url.h"

#include "lang_auto.h"
#include "mainwindow.h"
#include "ui/boxes/confirm_box.h"
#include "ayu/utils/telegram_helpers.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "data/data_session.h"
#include "data/data_user.h"

#include <QDesktopServices>

namespace AyuUrlHandlers
{

bool ResolveUser(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
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

	searchById(userId, &controller->session(), false, [=](const QString &title, UserData *data)
	{
		if (data) {
			controller->showPeerInfo(data);
			return;
		}

		Core::App().hideMediaView();
		Ui::show(Ui::MakeInformBox(tr::ayu_UserNotFoundMessage()));
	});

	return true;
}

bool HandleAyu(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
	if (!controller) {
		return false;
	}
	controller->showToast(QString(":3"), 500);
	return true;
}

bool TryHandleSpotify(const QString& url)
{
	if (!url.contains("spotify.com")) {
		return false;
	}

	// docs on their url scheme
	// https://www.iana.org/assignments/uri-schemes/prov/spotify

	using namespace qthelp;
	const auto matchOptions = RegExOption::CaseInsensitive;
	// https://regex101.com/r/l4Ogzf/2
	const auto match = regex_match(
		u"^(https?:\\/\\/)?([a-zA-Z0-9_]+)\\.spotify\\.com\\/(?<type>track|album|artist|user|playlist)\\/(?<identifier>[a-zA-Z0-9_\\/]+?)((\\?si=.+)?)$"_q,
		url,
		matchOptions);
	if (match) {
		const auto type = match->captured("type").toLower();
		const auto identifier = match->captured("identifier").replace("/", ":");

		// '/' -> ':' for links like:
		// https://open.spotify.com/user/1185903410/playlist/6YAnJeVC7tgOiocOG23Dd
		// so it'll look like
		// spotify:user:1185903410:playlist:6YAnJeVC7tgOiocOG23Dd

		const auto res = QString("spotify:%1:%2").arg(type).arg(identifier);

		if (!res.isEmpty()) {
			return QDesktopServices::openUrl(QUrl(res));
		}
	}

	return false;
}

}