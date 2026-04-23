// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ui/text/text_entity.h"

#include <QtCore/QString>

namespace Ayu::Translator::Html {

[[nodiscard]] QString entitiesToHtml(const TextWithEntities &text);
[[nodiscard]] TextWithEntities htmlToEntities(const QString &text);

}