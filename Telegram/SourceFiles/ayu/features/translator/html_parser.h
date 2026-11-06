// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "ui/text/text_entity.h"

#include <QtCore/QString>

namespace Ayu::Translator::Html {

[[nodiscard]] QString entitiesToHtml(const TextWithEntities &text);
[[nodiscard]] TextWithEntities htmlToEntities(const QString &text);

}
