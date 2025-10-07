// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "google.h"

#include <memory>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QUrl>
#include <QtGui/QTextDocument>

#include "ayu/features/translator/html_parser.h"

namespace Ayu::Translator {

namespace {

constexpr auto kGoogleTranslateUrl = "https://translate-pa.googleapis.com/v1/translateHtml";
constexpr auto kGoogleContentType = "application/json+protobuf";
constexpr auto kGoogleDefaultApiKey = "AIzaSyATBXajvzQLTDHEQbcpq0Ihe0vWDHmO520";

QString decodeHtmlEntities(const QString &text) {
	QTextDocument doc;
	doc.setHtml(text);
	return doc.toPlainText();
}

QStringList collectStrings(const QJsonValue &value) {
	QStringList result;
	if (value.isString()) {
		result.push_back(value.toString());
		return result;
	}
	if (value.isArray()) {
		const auto arr = value.toArray();
		for (const auto &item : arr) {
			result.append(collectStrings(item));
		}
		return result;
	}
	if (value.isObject()) {
		const auto obj = value.toObject();
		if (obj.contains(QStringLiteral("text"))) {
			result.append(collectStrings(obj.value(QStringLiteral("text"))));
		}
		if (obj.contains(QStringLiteral("trans"))) {
			result.append(collectStrings(obj.value(QStringLiteral("trans"))));
		}
	}
	return result;
}

} // namespace

GoogleTranslator &GoogleTranslator::instance() {
	static GoogleTranslator inst;
	return inst;
}

GoogleTranslator::GoogleTranslator(QObject *parent)
	: MultiThreadTranslator(parent) {
}

QPointer<QNetworkReply> GoogleTranslator::startSingleTranslation(
	const MultiThreadArgs &args
) {
	const auto &text = args.parsedData.text;
	const auto &fromLang = args.parsedData.fromLang;
	const auto &toLang = args.parsedData.toLang;
	const auto onSuccess = args.onSuccess;
	const auto onFail = args.onFail;

	if (text.empty() || toLang.isEmpty()) {
		if (onFail) onFail();
		return nullptr;
	}

	const auto from = fromLang.trimmed().isEmpty() ? QStringLiteral("auto") : fromLang.trimmed();
	const auto to = toLang.trimmed();

	const auto preparedText = shouldWrapInHtml() ? Html::entitiesToHtml(text) : text.text;
	QJsonArray requestRoot;
	QJsonArray requestPayload;
	QJsonArray requestText;
	requestText.append(preparedText);
	requestPayload.append(requestText);
	requestPayload.append(from);
	requestPayload.append(to);
	requestRoot.append(requestPayload);
	requestRoot.append(QStringLiteral("wt_lib"));
	const auto body = QJsonDocument(requestRoot).toJson(QJsonDocument::Compact);

	QNetworkRequest req(QUrl(QString::fromLatin1(kGoogleTranslateUrl)));
	const auto userAgent = randomDesktopUserAgent();
	req.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
	req.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(kGoogleContentType));
	req.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
	req.setRawHeader(QByteArrayLiteral("X-Goog-Api-Key"), QByteArray(kGoogleDefaultApiKey));

	QPointer<QNetworkReply> reply = _nam.post(req, body);

	auto timer = new QTimer(reply);
	timer->setSingleShot(true);
	timer->setInterval(15000);
	QObject::connect(timer,
					 &QTimer::timeout,
					 reply,
					 [reply]
					 {
						 if (!reply) return;
						 if (reply->isRunning()) reply->abort();
					 });
	timer->start();

	QObject::connect(reply,
					 &QNetworkReply::finished,
					 reply,
					 [reply, onSuccess = onSuccess, onFail = onFail, timer]
					 {
						 if (!reply) return;
						 timer->stop();
						 const auto guard = std::unique_ptr<QNetworkReply, void(*)(QNetworkReply *)>(
							 reply,
							 [](QNetworkReply *r) { r->deleteLater(); });
						 if (reply->error() != QNetworkReply::NoError) {
							 if (onFail) onFail();
							 return;
						 }
						 const auto body = reply->readAll();
						 QJsonParseError parseError{};
						 const auto doc = QJsonDocument::fromJson(body, &parseError);
						 if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
							 if (onFail) onFail();
							 return;
						 }
						 const auto root = doc.array();
						 if (root.isEmpty()) {
						 	 if (onFail) onFail();
						 	 return;
						 }
						 const auto translatedItems = collectStrings(root.at(0));
						 const auto textOutCombined = translatedItems.join(QStringLiteral(" "));
						 if (textOutCombined.trimmed().isEmpty()) {
						 	 if (onFail) onFail();
						 	 return;
						 }
						 const auto decodedText = decodeHtmlEntities(textOutCombined);
						 if (onSuccess) onSuccess(shouldWrapInHtml()
						 			  ? Html::htmlToEntities(decodedText)
						 			  : TextWithEntities{decodedText});
					 });

	return reply;
}

}