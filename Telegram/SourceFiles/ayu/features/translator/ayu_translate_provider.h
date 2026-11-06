// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "ayu/ayu_settings.h"
#include "translate_provider.h"

#include <memory>

namespace Main {
class Session;
} // namespace Main

namespace Ui {

[[nodiscard]] std::unique_ptr<TranslateProvider> CreateAyuTranslateProvider(
	not_null<Main::Session*> session,
	TranslationProvider provider);

} // namespace Ui
