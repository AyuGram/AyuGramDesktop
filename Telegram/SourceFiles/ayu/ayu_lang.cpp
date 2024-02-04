// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_lang.h"

#include "qjsondocument.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_instance.h"

// hard-coded languages
std::map<QString, QString> langMapping = {
	{"pt-br", "pt"},
	{"zh-hans-beta", "zh-hans"},
	{"zh-hant-beta", "zh-hant"},
	{"zh-hans-raw", "zh-hans"},
	{"zh-hant-raw", "zh-hant"},
};

CustomLangPack *CustomLangPack::instance = nullptr;

CustomLangPack::CustomLangPack() = default;

void CustomLangPack::initInstance() {
	if (!instance) instance = new CustomLangPack;
}

CustomLangPack *CustomLangPack::currentInstance() {
	return instance;
}

void CustomLangPack::fetchCustomLangPack(const QString &langPackId, const QString &langPackBaseId) {
	LOG(("Current Language pack ID: %1, Base ID: %2").arg(langPackId, langPackBaseId));

	auto finalLangPackId = langMapping.contains(langPackId) ? langMapping[langPackId] : langPackId;

	const auto proxy = Core::App().settings().proxy().isEnabled()
						   ? Core::App().settings().proxy().selected()
						   : MTP::ProxyData();
	if (proxy.type == MTP::ProxyData::Type::Socks5 || proxy.type == MTP::ProxyData::Type::Http) {
		QNetworkProxy LocaleProxy = ToNetworkProxy(ToDirectIpProxy(proxy));
		networkManager.setProxy(LocaleProxy);
	}

	// using `jsdelivr` since China (...and maybe other?) users have some problems with GitHub
	// https://crowdin.com/project/ayugram/discussions/6
	QUrl url;
	if (!finalLangPackId.isEmpty() && !langPackBaseId.isEmpty() && !needFallback) {
		url.setUrl(qsl("https://cdn.jsdelivr.net/gh/AyuGram/Languages@l10n_main/values/langs/%1/Shared.json").arg(
			finalLangPackId));
	} else {
		url.setUrl(qsl("https://cdn.jsdelivr.net/gh/AyuGram/Languages@l10n_main/values/langs/%1/Shared.json").arg(
			needFallback ? langPackBaseId : finalLangPackId));
	}
	_chkReply = networkManager.get(QNetworkRequest(url));
	connect(_chkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(fetchError(QNetworkReply::NetworkError)));
	connect(_chkReply, SIGNAL(finished()), this, SLOT(fetchFinished()));
	LOG(("Fetching %1 lang pack...").arg(
		needFallback ? (langPackBaseId.isEmpty() ? finalLangPackId : langPackBaseId) : finalLangPackId));
}

void CustomLangPack::fetchFinished() {
	if (!_chkReply) return;

	QString langPackBaseId = Lang::GetInstance().baseId();
	QString langPackId = Lang::GetInstance().id();
	auto statusCode = _chkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	if (statusCode == 404 && !langPackId.isEmpty() && !langPackBaseId.isEmpty() && !needFallback) {
		LOG(("AyuGram Language pack not found! Fallback to main language: %1...").arg(langPackBaseId));
		needFallback = true;
		_chkReply->disconnect();
		fetchCustomLangPack("", langPackBaseId);
	} else {
		QByteArray result = _chkReply->readAll().trimmed();
		QJsonParseError error{};
		QJsonDocument str = QJsonDocument::fromJson(result, &error);
		if (error.error == QJsonParseError::NoError) {
			parseLangFile(str);
		} else {
			LOG(("Incorrect JSON File. Fallback to default language: English..."));
			loadDefaultLangFile();
		}

		_chkReply = nullptr;
	}
}

void CustomLangPack::fetchError(QNetworkReply::NetworkError e) {
	LOG(("Network error: %1").arg(e));

	if (e == QNetworkReply::NetworkError::ContentNotFoundError) {
		QString langPackBaseId = Lang::GetInstance().baseId();
		QString langPackId = Lang::GetInstance().id();

		if (!langPackId.isEmpty() && !langPackBaseId.isEmpty() && !needFallback) {
			LOG(("AyuGram Language pack not found! Fallback to main language: %1...").arg(langPackBaseId));
			needFallback = true;
			_chkReply->disconnect();
			fetchCustomLangPack("", langPackBaseId);
		} else {
			LOG(("AyuGram Language pack not found! Fallback to default language: English..."));
			loadDefaultLangFile();
			_chkReply = nullptr;
		}
	}
}

void CustomLangPack::loadDefaultLangFile() {
	QFile file(":/localization/en.json");
	if (file.open(QIODevice::ReadOnly)) {
		QJsonDocument str = QJsonDocument::fromJson(file.readAll());
		QJsonObject json = str.object();
		for (const QString &key : json.keys()) {
			Lang::GetInstance().applyValue(key.toUtf8(), json.value(key).toString().toUtf8());
		}
		Lang::GetInstance().updatePluralRules();
		file.close();
	}
}

void CustomLangPack::parseLangFile(QJsonDocument str) {
	QJsonObject json = str.object();
	for (const QString &brokenKey : json.keys()) {
		auto key = qsl("ayu_") + brokenKey;
		auto val = json.value(brokenKey).toString().replace(qsl("&amp;"), qsl("&")).toUtf8();

		Lang::GetInstance().resetValue(key.toUtf8());
		Lang::GetInstance().applyValue(key.toUtf8(), val);
		if (key.contains("#other")) {
			Lang::GetInstance().resetValue(key.toUtf8().replace("#other", "#few"));
			Lang::GetInstance().resetValue(key.toUtf8().replace("#other", "#few"));
			Lang::GetInstance().applyValue(key.toUtf8().replace("#other", "#few"), val);
			Lang::GetInstance().applyValue(key.toUtf8().replace("#other", "#many"), val);
		}
	}
	Lang::GetInstance().updatePluralRules();
}
