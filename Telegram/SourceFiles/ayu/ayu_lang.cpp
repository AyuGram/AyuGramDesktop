// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
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

AyuLanguage *AyuLanguage::instance = nullptr;

AyuLanguage::AyuLanguage() = default;

void AyuLanguage::init() {
	if (!instance) instance = new AyuLanguage;
}

AyuLanguage *AyuLanguage::currentInstance() {
	return instance;
}

void AyuLanguage::fetchLanguage(const QString &id, const QString &baseId) {
	auto finalLangPackId = langMapping.contains(id) ? langMapping[id] : id;

	if (Core::App().settings().proxy().isEnabled()) {
		const auto proxy = Core::App().settings().proxy().selected();
		if (proxy.type == MTP::ProxyData::Type::Socks5 || proxy.type == MTP::ProxyData::Type::Http) {
			const auto networkProxy = ToNetworkProxy(ToDirectIpProxy(Core::App().settings().proxy().selected()));
			networkManager.setProxy(networkProxy);
		}
	}

	// using `jsdelivr` since China (...and maybe other?) users have some problems with GitHub
	// https://crowdin.com/project/ayugram/discussions/6
	QUrl url;
	if (!finalLangPackId.isEmpty() && !baseId.isEmpty() && !needFallback) {
		url.setUrl(qsl("https://cdn.jsdelivr.net/gh/AyuGram/Languages@l10n_main/values/langs/%1/Shared.json").arg(
			finalLangPackId));
	} else {
		url.setUrl(qsl("https://cdn.jsdelivr.net/gh/AyuGram/Languages@l10n_main/values/langs/%1/Shared.json").arg(
			needFallback ? baseId : finalLangPackId));
	}
	_chkReply = networkManager.get(QNetworkRequest(url));
	connect(_chkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(fetchError(QNetworkReply::NetworkError)));
	connect(_chkReply, SIGNAL(finished()), this, SLOT(fetchFinished()));
}

void AyuLanguage::fetchFinished() {
	if (!_chkReply) return;

	QString langPackBaseId = Lang::GetInstance().baseId();
	QString langPackId = Lang::GetInstance().id();
	auto statusCode = _chkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

	if (statusCode == 404 && !langPackId.isEmpty() && !langPackBaseId.isEmpty() && !needFallback) {
		LOG(("AyuGram Language not found! Fallback to main language: %1...").arg(langPackBaseId));
		needFallback = true;
		_chkReply->disconnect();
		fetchLanguage("", langPackBaseId);
	} else {
		const auto result = _chkReply->readAll().trimmed();
		QJsonParseError error{};
		const auto doc = QJsonDocument::fromJson(result, &error);
		if (error.error == QJsonParseError::NoError) {
			applyLanguageJson(doc);
		} else {
			LOG(("Incorrect language JSON File."));
		}

		_chkReply = nullptr;
	}
}

void AyuLanguage::fetchError(QNetworkReply::NetworkError e) {
	LOG(("Network error: %1").arg(e));

	if (e == QNetworkReply::NetworkError::ContentNotFoundError) {
		const auto baseId = Lang::GetInstance().baseId();
		const auto id = Lang::GetInstance().id();

		if (!id.isEmpty() && !baseId.isEmpty() && !needFallback) {
			LOG(("AyuGram Language not found! Fallback to main language: %1...").arg(baseId));
			needFallback = true;
			_chkReply->disconnect();
			fetchLanguage("", baseId);
		} else {
			LOG(("AyuGram Language not found!"));
			_chkReply = nullptr;
		}
	}
}

void AyuLanguage::applyLanguageJson(QJsonDocument doc) {
	const auto json = doc.object();
	for (const QString &brokenKey : json.keys()) {
		auto key = qsl("ayu_") + brokenKey;
		const auto val = json.value(brokenKey).toString().replace(qsl("&amp;"), qsl("&")).toUtf8();

		if (key.endsWith("_zero") || key.endsWith("_two") || key.endsWith("_few") || key.endsWith("_many")) {
			continue;
		}

		if (key.endsWith("_one")) {
			key = key.replace("_one", "#one");
		} else if (key.endsWith("_other")) {
			key = key.replace("_other", "#other");
		}

		Lang::GetInstance().resetValue(key.toUtf8());
		Lang::GetInstance().applyValue(key.toUtf8(), val);
	}
	Lang::GetInstance().updatePluralRules();
}
