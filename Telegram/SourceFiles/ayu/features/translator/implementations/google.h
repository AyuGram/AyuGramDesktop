// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>

#include "./base.h"

namespace Ayu::Translator {

class GoogleTranslator final : public MultiThreadTranslator
{
	Q_OBJECT

public:
	static GoogleTranslator &instance();

	// all languages
	[[nodiscard]] QSet<QString> supportedLanguages() const override { return {}; }

	[[nodiscard]] QPointer<QNetworkReply> startSingleTranslation(
		const MultiThreadArgs &args
	) override;

private:
	explicit GoogleTranslator(QObject *parent = nullptr);

	QNetworkAccessManager _nam;
};

}