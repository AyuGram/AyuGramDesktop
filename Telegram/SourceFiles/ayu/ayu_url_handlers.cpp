// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu/ayu_url_handlers.h"

#include "base/qthelp_url.h"

#include "lang_auto.h"
#include "ui/boxes/confirm_box.h"
#include "ayu/utils/telegram_helpers.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "data/data_session.h"
#include "data/data_user.h"

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

}