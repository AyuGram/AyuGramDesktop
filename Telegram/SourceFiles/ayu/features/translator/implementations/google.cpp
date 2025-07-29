// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "google.h"

#include <memory>
#include <QtCore/QJsonArray>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include "ayu/features/translator/html_parser.h"

namespace Ayu::Translator {

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

	QUrl url(QStringLiteral("https://translate.googleapis.com/translate_a/single"));
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("dj"), QStringLiteral("1"));
	query.addQueryItem(QStringLiteral("q"), shouldWrapInHtml() ? Html::entitiesToHtml(text) : text.text);
	query.addQueryItem(QStringLiteral("sl"), from);
	query.addQueryItem(QStringLiteral("tl"), to);
	query.addQueryItem(QStringLiteral("ie"), QStringLiteral("UTF-8"));
	query.addQueryItem(QStringLiteral("oe"), QStringLiteral("UTF-8"));
	query.addQueryItem(QStringLiteral("client"), QStringLiteral("at"));
	query.addQueryItem(QStringLiteral("dt"), QStringLiteral("t"));
	query.addQueryItem(QStringLiteral("otf"), QStringLiteral("2"));
	url.setQuery(query);

	QNetworkRequest req(url);
	const auto userAgent = randomDesktopUserAgent();
	req.setHeader(QNetworkRequest::UserAgentHeader, userAgent);

	QPointer<QNetworkReply> reply = _nam.get(req);

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
						 bool ok = false;
						 const auto textOut = parseJsonPath(body, QStringLiteral("sentences"), &ok);
						 if (!ok) {
							 if (onFail) onFail();
							 return;
						 }
						 if (onSuccess) onSuccess(shouldWrapInHtml()
													  ? Html::htmlToEntities(textOut)
													  : TextWithEntities{textOut});
					 });

	return reply;
}

}