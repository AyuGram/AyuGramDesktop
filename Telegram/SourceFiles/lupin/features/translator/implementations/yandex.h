// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "lupin/features/translator/implementations/base.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>

namespace Lupin::Translator {

class YandexTranslator final : public MultiThreadTranslator
{
	Q_OBJECT

public:
	static YandexTranslator &instance();

	[[nodiscard]] QSet<QString> supportedLanguages() const override;

	[[nodiscard]] QPointer<QNetworkReply> startSingleTranslation(
		const MultiThreadArgs &args
	) override;

private:
	explicit YandexTranslator(QObject *parent = nullptr);

	QNetworkAccessManager _nam;
	QString _uuid;
};

}