// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "yandex.h"

#include <memory>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "ayu/features/translator/html_parser.h"

namespace Ayu::Translator {

YandexTranslator &YandexTranslator::instance() {
	static YandexTranslator inst;
	return inst;
}

YandexTranslator::YandexTranslator(QObject *parent)
	: MultiThreadTranslator(parent)
	  , _uuid(QUuid::createUuid().toString(QUuid::WithoutBraces).replace("-", "")) {
}

QSet<QString> YandexTranslator::supportedLanguages() const {
	static const QSet<QString> languages = {
		QStringLiteral("az"), QStringLiteral("sq"), QStringLiteral("am"), QStringLiteral("en"),
		QStringLiteral("ar"), QStringLiteral("hy"), QStringLiteral("af"), QStringLiteral("eu"),
		QStringLiteral("ba"), QStringLiteral("be"), QStringLiteral("bn"), QStringLiteral("my"),
		QStringLiteral("bg"), QStringLiteral("bs"), QStringLiteral("cv"), QStringLiteral("cy"),
		QStringLiteral("hu"), QStringLiteral("vi"), QStringLiteral("ht"), QStringLiteral("gl"),
		QStringLiteral("nl"), QStringLiteral("mrj"), QStringLiteral("el"), QStringLiteral("ka"),
		QStringLiteral("gu"), QStringLiteral("da"), QStringLiteral("he"), QStringLiteral("yi"),
		QStringLiteral("id"), QStringLiteral("ga"), QStringLiteral("it"), QStringLiteral("is"),
		QStringLiteral("es"), QStringLiteral("kk"), QStringLiteral("kn"), QStringLiteral("ca"),
		QStringLiteral("ky"), QStringLiteral("zh"), QStringLiteral("ko"), QStringLiteral("xh"),
		QStringLiteral("km"), QStringLiteral("lo"), QStringLiteral("la"), QStringLiteral("lv"),
		QStringLiteral("lt"), QStringLiteral("lb"), QStringLiteral("mg"), QStringLiteral("ms"),
		QStringLiteral("ml"), QStringLiteral("mt"), QStringLiteral("mk"), QStringLiteral("mi"),
		QStringLiteral("mr"), QStringLiteral("mhr"), QStringLiteral("mn"), QStringLiteral("de"),
		QStringLiteral("ne"), QStringLiteral("no"), QStringLiteral("pa"), QStringLiteral("pap"),
		QStringLiteral("fa"), QStringLiteral("pl"), QStringLiteral("pt"), QStringLiteral("ro"),
		QStringLiteral("ru"), QStringLiteral("ceb"), QStringLiteral("sr"), QStringLiteral("si"),
		QStringLiteral("sk"), QStringLiteral("sl"), QStringLiteral("sw"), QStringLiteral("su"),
		QStringLiteral("tg"), QStringLiteral("th"), QStringLiteral("tl"), QStringLiteral("ta"),
		QStringLiteral("tt"), QStringLiteral("te"), QStringLiteral("tr"), QStringLiteral("udm"),
		QStringLiteral("uz"), QStringLiteral("uk"), QStringLiteral("ur"), QStringLiteral("fi"),
		QStringLiteral("fr"), QStringLiteral("hi"), QStringLiteral("hr"), QStringLiteral("cs"),
		QStringLiteral("sv"), QStringLiteral("gd"), QStringLiteral("et"), QStringLiteral("eo"),
		QStringLiteral("jv"), QStringLiteral("ja")
	};
	return languages;
}

QPointer<QNetworkReply> YandexTranslator::startSingleTranslation(
	const MultiThreadArgs &args
) {
	const auto &text = args.parsedData.text;
	// const auto &fromLang = args.parsedData.fromLang;
	const auto &toLang = args.parsedData.toLang;
	const auto onSuccess = args.onSuccess;
	const auto onFail = args.onFail;

	if (text.empty() || toLang.isEmpty()) {
		if (onFail) onFail();
		return nullptr;
	}

	const auto to = toLang.trimmed();

	QUrl url(QStringLiteral("https://translate.yandex.net/api/v1/tr.json/translate"));
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("srv"), QStringLiteral("android"));
	query.addQueryItem(QStringLiteral("id"), _uuid + QStringLiteral("-0-0"));
	url.setQuery(query);

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::UserAgentHeader,
				  QStringLiteral("ru.yandex.translate/21.15.4.21402814 (Xiaomi Redmi K20 Pro; Android 11)"));
	req.setHeader(QNetworkRequest::ContentTypeHeader,
				  QStringLiteral("application/x-www-form-urlencoded"));

	QUrlQuery postData;
	postData.addQueryItem(QStringLiteral("lang"), to);
	postData.addQueryItem(QStringLiteral("text"), shouldWrapInHtml() ? Html::entitiesToHtml(text) : text.text);
	const auto postDataEncoded = postData.toString(QUrl::FullyEncoded).toUtf8();

	QPointer<QNetworkReply> reply = _nam.post(req, postDataEncoded);

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
						 const auto translatedText = parseJsonPath(body, QStringLiteral("text"), &ok);
						 if (!ok) {
							 if (onFail) onFail();
							 return;
						 }
						 if (onSuccess) onSuccess(shouldWrapInHtml()
													  ? Html::htmlToEntities(translatedText)
													  : TextWithEntities{translatedText});
					 });

	return reply;
}

}