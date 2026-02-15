// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/local_api/ayu_local_api.h"

#include "ayu/ayu_settings.h"
#include "ayu/local_api/ayu_local_api_server.h"

#include "logs.h"

#include <memory>

namespace AyuLocalApi {
namespace {

std::unique_ptr<AyuLocalApiServer> Server;

} // namespace

void init() {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.localApiEnabled) {
		return;
	}

	if (!Server) {
		Server = std::make_unique<AyuLocalApiServer>();
	}

	const auto ok = Server->start(settings.localApiPort, settings.localApiKey);
	if (ok) {
		LOG((u"AyuLocalApi: listening on 127.0.0.1:%1"_q
			.arg(settings.localApiPort)));
	} else {
		LOG((u"AyuLocalApi: failed to listen on 127.0.0.1:%1"_q
			.arg(settings.localApiPort)));
	}
}

} // namespace AyuLocalApi
