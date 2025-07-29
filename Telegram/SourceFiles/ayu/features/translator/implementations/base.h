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
#include <QtNetwork/QNetworkReply>

#include <functional>

#include "apiwrap.h"

namespace Main {
class Session;
}

namespace Ayu::Translator {

using CallbackSuccess = std::function<void(const std::vector<TextWithEntities> &)>;
using CallbackFail = std::function<void()>;
using CallbackCancel = std::function<void()>;

using MultiThreadCallbackSuccess = std::function<void(const TextWithEntities &)>;

QString randomDesktopUserAgent();

bool shouldWrapInHtml();

QString parseJsonPath(const QByteArray &body, const QString &jsonPath, bool *ok);

struct PassedData
{
	MTPflags<MTPmessages_translateText::Flags> flags;
	MTPInputPeer peer;
	MTPVector<MTPint> idList;
	MTPVector<MTPTextWithEntities> text;
	MTPstring toLang;
};

struct ParsedData
{
	std::vector<TextWithEntities> texts;
	QString fromLang;
	QString toLang;
};

struct StartTranslationArgs
{
	Main::Session *session;

	PassedData requestData;
	ParsedData parsedData;

	CallbackSuccess onSuccess;
	CallbackFail onFail;
};

struct ParsedDataSingle
{
	TextWithEntities text;
	QString fromLang;
	QString toLang;
};

struct MultiThreadArgs
{
	ParsedDataSingle parsedData;

	MultiThreadCallbackSuccess onSuccess;
	CallbackFail onFail;
};

class BaseTranslator : public QObject
{
	Q_OBJECT

public:
	explicit BaseTranslator(QObject *parent = nullptr)
		: QObject(parent) {
	}

	~BaseTranslator() override = default;

	[[nodiscard]] virtual QSet<QString> supportedLanguages() const { return {}; }

	[[nodiscard]] virtual CallbackCancel startTranslation(
		const StartTranslationArgs &args
	) = 0;
};

class MultiThreadTranslator : public BaseTranslator
{
	Q_OBJECT

public:
	explicit MultiThreadTranslator(QObject *parent = nullptr)
		: BaseTranslator(parent) {
	}

	~MultiThreadTranslator() override = default;

	[[nodiscard]] virtual int getConcurrencyLimit() const { return 1; }
	[[nodiscard]] virtual int getMaxRetries() const { return 3; }
	[[nodiscard]] virtual int getBaseWaitTimeMs() const { return 1000; }

	[[nodiscard]] CallbackCancel startTranslation(
		const StartTranslationArgs &args
	) override;

	[[nodiscard]] virtual QPointer<QNetworkReply> startSingleTranslation(
		const MultiThreadArgs &args
	) = 0;
};

}