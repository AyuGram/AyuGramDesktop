// Drakeygram Desktop
// Fully independent fork. No external tracking.

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
	if (!instance) instance = new AyuLanguage;
	instance->loadCachedLanguage();
}

AyuLanguage *AyuLanguage::currentInstance() {
	return instance;
}

QString AyuLanguage::getCacheDir() const {
	return cWorkingDir() + u"tdata/drakey/languages/"_q;
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
		return;
	}

	const auto cachePath = getCachePath(finalLangPackId);
	QFile file(cachePath);
	if (!file.exists()) {
		const auto basePath = getCachePath(langPackBaseId);
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
			LOG(("Loading cached Drakeygram language: %1").arg(finalLangPackId));
			applyLanguageJson(doc);
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
		LOG(("Cached Drakeygram language: %1").arg(langId));
	}
}

void AyuLanguage::fetchLanguage(const QString &id, const QString &baseId) {
	// NETWORK FETCH DISABLED.
	// Previously pulled from AyuGram CDN / jsDelivr.
	// Now only uses local cache if present. No remote requests.
	auto finalLangPackId = langMapping.contains(id) ? langMapping[id] : id;
	_currentLangId = finalLangPackId.isEmpty() ? baseId : finalLangPackId;

	// Intentionally do nothing network-related.
	// Language strings will come from built-in / local cache only.
	LOG(("Drakeygram: remote language fetch disabled"));
}

void AyuLanguage::fetchFinished() {
	// no-op - network disabled
	_chkReply = nullptr;
}

void AyuLanguage::fetchError(QNetworkReply::NetworkError e) {
	// no-op - network disabled
	_chkReply = nullptr;
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
			val = val.replace(qsl("%1$d"), qsl("{count1}")).replace(qsl("%2$d"), qsl("{count2}"));
		} else if (val.contains(qsl("%1$s")) && !val.contains(qsl("%2$s"))) {
			val = val.replace(qsl("%1$s"), qsl("{item}"));
		} else if (val.contains(qsl("%1$s")) && val.contains(qsl("%2$s"))) {
			val = val.replace(qsl("%1$s"), qsl("{item1}")).replace(qsl("%2$s"), qsl("{item2}"));
		}

		Lang::GetInstance().resetValue(key.toUtf8());
		Lang::GetInstance().applyValue(key.toUtf8(), val.toUtf8());
	}
	Lang::GetInstance().updatePluralRules();
}
