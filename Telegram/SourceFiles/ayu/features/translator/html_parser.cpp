// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "html_parser.h"

namespace Ayu::Translator::Html {

// yandex messes up HTML badly, so formatting removed for now

QString entitiesToHtml(const TextWithEntities &text) {
	return text.text;
}

TextWithEntities htmlToEntities(const QString &text) {
	TextWithEntities result = {.text = text};

	// links parsing doesn't work actually as it's not even accounted in ParseEntities
	// todo: find a way to parse links
	TextUtilities::ApplyServerCleaning(result);
	TextUtilities::ParseEntities(result, TextParseLinks | TextParseMentions | TextParseHashtags | TextParseBotCommands);
	TextUtilities::Trim(result);

	return result;
}

}