// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ayu_lang.h"

#include "qjsondocument.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_instance.h"
#include "storage/localstorage.h"

#include <QDir>
#include <QFile>

// hard-coded languages
std::map<QString, QString> langMapping = {
	{"pt-br", "pt"},
	{"zh-hans-beta", "zh-hans"},
	{"zh-hant-beta", "zh-hant"},
	{"zh-hans-raw", "zh-hans"},
	{"zh-hant-raw", "zh-hant"},
};

constexpr auto postfixes = {
	"zero",
	"one",
	"two",
	"few",
	"many",
	"other"
};

AyuLanguage *AyuLanguage::instance = nullptr;

AyuLanguage::AyuLanguage() = default;

void AyuLanguage::init() {
	if (!instance) {
		instance = new AyuLanguage;
		Lang::GetInstance().updated(
		) | rpl::on_next([] {
			const auto id = Lang::GetInstance().id();
			const auto baseId = Lang::GetInstance().baseId();
			if (!id.isEmpty() && instance) {
				instance->loadCachedLanguage();
				instance->fetchLanguage(id, baseId);
			}
		}, instance->_lifetime);
	}
	instance->loadCachedLanguage();
}

AyuLanguage *AyuLanguage::currentInstance() {
	return instance;
}

QString AyuLanguage::getCacheDir() const {
	return cWorkingDir() + u"tdata/ayu/languages/"_q;
}

QString AyuLanguage::getCachePath(const QString &langId) const {
	return getCacheDir() + langId + u".json"_q;
}

void AyuLanguage::loadCachedLanguage() {
	const auto langPackId = Lang::GetInstance().id();
	const auto langPackBaseId = Lang::GetInstance().baseId();
	auto finalLangPackId = langMapping.contains(langPackId) ? langMapping[langPackId] : langPackId;

	if (finalLangPackId.isEmpty()) {
		finalLangPackId = langPackBaseId;
	}
	if (finalLangPackId.isEmpty()) {
		LOG(("AyuGram Language: loadCachedLanguage empty finalLangPackId"));
		return;
	}

	const auto cachePath = getCachePath(finalLangPackId);
	QFile file(cachePath);
	if (!file.exists()) {
		const auto basePath = getCachePath(langPackBaseId);
		LOG(("AyuGram Language cache %1 does not exist, checking base: %2").arg(cachePath, basePath));
		if (!QFile::exists(basePath)) {
			return;
		}
		file.setFileName(basePath);
	}

	if (file.open(QIODevice::ReadOnly)) {
		const auto data = file.readAll();
		file.close();

		QJsonParseError error{};
		const auto doc = QJsonDocument::fromJson(data, &error);
		if (error.error == QJsonParseError::NoError) {
			LOG(("Loading cached AyuGram language: %1 (%2 bytes, %3 keys)").arg(finalLangPackId).arg(data.size()).arg(doc.object().keys().size()));
			applyLanguageJson(doc);
		} else {
			LOG(("Failed parsing cached AyuGram language %1 (%2 bytes): %3").arg(finalLangPackId).arg(data.size()).arg(error.errorString()));
		}
	}
}

void AyuLanguage::saveCachedLanguage(const QByteArray &json, const QString &langId) {
	const auto cacheDir = getCacheDir();
	QDir().mkpath(cacheDir);

	const auto cachePath = getCachePath(langId);
	QFile file(cachePath);
	if (file.open(QIODevice::WriteOnly)) {
		file.write(json);
		file.close();
		LOG(("Cached AyuGram language: %1 (%2 bytes) at %3").arg(langId).arg(json.size()).arg(cachePath));
	} else {
		LOG(("Failed to open cache file for writing: %1").arg(cachePath));
	}
}

void AyuLanguage::fetchLanguage(const QString &id, const QString &baseId) {
	if (_chkReply) {
		LOG(("Aborting previous pending AyuGram language request"));
		_chkReply->disconnect();
		_chkReply->abort();
		_chkReply = nullptr;
	}
	needFallback = false;

	auto finalLangPackId = langMapping.contains(id) ? langMapping[id] : id;
	_currentLangId = finalLangPackId.isEmpty() ? baseId : finalLangPackId;

	if (Core::App().settings().proxy().isEnabled()) {
		const auto proxy = Core::App().settings().proxy().selected();
		if (proxy.type == MTP::ProxyData::Type::Socks5 || proxy.type == MTP::ProxyData::Type::Http) {
			const auto networkProxy = ToNetworkProxy(ToDirectIpProxy(Core::App().settings().proxy().selected()));
			networkManager.setProxy(networkProxy);
		}
	}

	QUrl url;
	const auto targetLangId = (!finalLangPackId.isEmpty() && !needFallback)
		? finalLangPackId
		: (needFallback ? baseId : finalLangPackId);

	url.setUrl(qsl("https://raw.githubusercontent.com/PH4N7OMx/Languages/main/values/langs/%1/Shared.json").arg(
		targetLangId));

	LOG(("AyuGram Language fetchLanguage: requested id='%1', baseId='%2', finalLangPackId='%3', currentLangId='%4', targetLangId='%5', url='%6'").arg(id, baseId, finalLangPackId, _currentLangId, targetLangId, url.toString()));

	QNetworkRequest req(url);
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	_chkReply = networkManager.get(req);
	connect(_chkReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(fetchError(QNetworkReply::NetworkError)));
	connect(_chkReply, SIGNAL(finished()), this, SLOT(fetchFinished()));
}

void AyuLanguage::fetchFinished() {
	if (!_chkReply) return;

	QString langPackBaseId = Lang::GetInstance().baseId();
	QString langPackId = Lang::GetInstance().id();
	auto statusCode = _chkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	LOG(("AyuGram Language fetchFinished: statusCode=%1, currentLangId='%2'").arg(statusCode).arg(_currentLangId));

	if (statusCode == 404 && !langPackId.isEmpty() && !langPackBaseId.isEmpty() && !needFallback) {
		LOG(("AyuGram Language not found (404)! Fallback to main language: %1...").arg(langPackBaseId));
		needFallback = true;
		_chkReply->disconnect();
		fetchLanguage("", langPackBaseId);
	} else {
		const auto result = _chkReply->readAll().trimmed();
		QJsonParseError error{};
		const auto doc = QJsonDocument::fromJson(result, &error);
		if (error.error == QJsonParseError::NoError) {
			LOG(("Fetched AyuGram language successfully: %1 (%2 bytes, %3 keys)").arg(_currentLangId).arg(result.size()).arg(doc.object().keys().size()));
			saveCachedLanguage(result, _currentLangId);
			applyLanguageJson(doc);
		} else {
			LOG(("Incorrect language JSON File for %1 (status %2, size %3 bytes): %4").arg(_currentLangId).arg(statusCode).arg(result.size()).arg(error.errorString()));
		}

		_chkReply = nullptr;
	}
}

void AyuLanguage::fetchError(QNetworkReply::NetworkError e) {
	LOG(("AyuGram Language Network error: %1 for currentLangId='%2'").arg(e).arg(_currentLangId));

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
		auto val = json.value(brokenKey).toString().replace(qsl("&amp;"), qsl("&"));

		if (key.endsWith("_Android")) {
			continue;
		}

		for (const auto &postfix : postfixes) {
			if (key.endsWith(qsl("_") + postfix)) {
				key = key.replace(qsl("_") + postfix, qsl("#") + postfix);
				break;
			}
		}

		if (key.endsWith("_PC")) {
			key = key.replace("_PC", "");
		}

		if (val.contains(qsl("%1$d")) && !val.contains(qsl("%2$d"))) {
			val = val.replace(qsl("%1$d"), qsl("{count}"));
		} else if (val.contains(qsl("%1$d")) && val.contains(qsl("%2$d"))) {
			val = val.replace(qsl("%1$d"), qsl("{item1}")).replace(qsl("%2$d"), qsl("{item2}"));
		} else if (val.contains(qsl("%1$s")) && !val.contains(qsl("%2$s"))) {
			val = val.replace(qsl("%1$s"), qsl("{item}"));
		} else if (val.contains(qsl("%1$s")) && val.contains(qsl("%2$s"))) {
			val = val.replace(qsl("%1$s"), qsl("{item1}")).replace(qsl("%2$s"), qsl("{item2}"));
		}

		Lang::GetInstance().resetValue(key.toUtf8());
		Lang::GetInstance().applyValue(key.toUtf8(), val.toUtf8());

		if (brokenKey == u"KeepDeletedMessages"_q) {
			Lang::GetInstance().applyValue("ayu_SaveDeletedMessages", val.toUtf8());
		} else if (brokenKey == u"KeepMessagesHistory"_q) {
			Lang::GetInstance().applyValue("ayu_SaveMessagesHistory", val.toUtf8());
		} else if (brokenKey == u"SaveForBots"_q) {
			Lang::GetInstance().applyValue("ayu_MessageSavingSaveForBots", val.toUtf8());
		} else if (brokenKey == u"MarkReadAfterSend"_q) {
			Lang::GetInstance().applyValue("ayu_MarkReadAfterAction", val.toUtf8());
		} else if (brokenKey == u"GhostMode"_q) {
			Lang::GetInstance().applyValue("ayu_GhostModeToggle", val.toUtf8());
		} else if (brokenKey == u"GhostModeToggle"_q) {
			Lang::GetInstance().applyValue("ayu_GhostMode", val.toUtf8());
		} else if (brokenKey == u"LocalTelegramPremium"_q) {
			Lang::GetInstance().applyValue("ayu_LocalPremium", val.toUtf8());
		} else if (brokenKey == u"LocalPremium"_q) {
			Lang::GetInstance().applyValue("ayu_LocalTelegramPremium", val.toUtf8());
		}
	}
	Lang::GetInstance().updatePluralRules();
}
