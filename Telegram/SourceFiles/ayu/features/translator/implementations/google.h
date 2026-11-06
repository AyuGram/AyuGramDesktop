// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "ayu/features/translator/implementations/base.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>

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
