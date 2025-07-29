// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "./base.h"

namespace Ayu::Translator {

class TelegramTranslator final : public BaseTranslator
{
	Q_OBJECT

public:
	static TelegramTranslator &instance();

	[[nodiscard]] QSet<QString> supportedLanguages() const override { return {}; }

	[[nodiscard]] CallbackCancel startTranslation(
		const StartTranslationArgs &args
	) override;

private:
	explicit TelegramTranslator(QObject *parent = nullptr);
};

}