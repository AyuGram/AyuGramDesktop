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
#include <QSaveFile>
#include <QRegularExpression>
#include <QtNetwork/QNetworkProxy>
#include <algorithm>

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

AyuLanguage::AyuLanguage() {
	Lang::GetInstance().idChanges() | rpl::on_next([=] {
		syncLanguage();
	}, _lifetime);
	Lang::GetInstance().updated() | rpl::on_next([=] {
		syncLanguage();
	}, _lifetime);
}

void AyuLanguage::init() {
	if (!instance) instance = new AyuLanguage;
	instance->syncLanguage();
}

AyuLanguage *AyuLanguage::currentInstance() {
	return instance;
}

namespace {

QString NormalizeLanguage(QString id) {
	id = id.toLower();
	if (langMapping.contains(id)) id = langMapping[id];
	static const auto valid = QRegularExpression(u"^[a-z0-9_-]+$"_q);
	return valid.match(id).hasMatch() ? id : QString();
}

bool ValidLanguage(const QJsonDocument &doc) {
	if (!doc.isObject() || doc.object().isEmpty()) return false;
	const auto object = doc.object();
	return std::all_of(object.begin(), object.end(), [](const QJsonValue &value) {
		return value.isString();
	});
}

} // namespace

void AyuLanguage::syncLanguage() {
	if (_applying) return;
	const auto id = Lang::GetInstance().isCustom()
		? QString()
		: NormalizeLanguage(Lang::GetInstance().id().isEmpty()
			? u"en"_q : Lang::GetInstance().id());
	const auto baseId = NormalizeLanguage(Lang::GetInstance().baseId());
	if (id == _currentLangId && baseId == _baseLangId) {
		if (!_document.isNull()) {
			_applying = true;
			applyLanguageJson(_document);
			_applying = false;
		}
		return;
	}
	if (_chkReply) {
		const auto reply = _chkReply;
		_chkReply = nullptr;
		reply->abort();
	}
	_currentLangId = id;
	_baseLangId = baseId;
	_document = QJsonDocument();
	if (id.isEmpty() || id == u"en"_q) return;
	loadCachedLanguage();
	fetchLanguage(id);
}

QString AyuLanguage::getCacheDir() const {
	return cWorkingDir() + u"tdata/ayu/languages/"_q;
}

QString AyuLanguage::getCachePath(const QString &langId) const {
	return getCacheDir() + langId + u".json"_q;
}

void AyuLanguage::loadCachedLanguage() {
	for (const auto &id : { _currentLangId, _baseLangId }) {
		if (id.isEmpty()) continue;
		QFile file(getCachePath(id));
		if (!file.open(QIODevice::ReadOnly)) continue;
		const auto doc = QJsonDocument::fromJson(file.readAll());
		if (!ValidLanguage(doc)) continue;
		_document = doc;
		LOG(("Loading AyuGram language: %1").arg(id));
		applyLanguageJson(doc);
		return;
	}
}

void AyuLanguage::saveCachedLanguage(const QByteArray &json, const QString &langId) {
	QDir().mkpath(getCacheDir());
	QSaveFile file(getCachePath(langId));
	if (file.open(QIODevice::WriteOnly)
		&& file.write(json) == json.size()
		&& file.commit()) {
		LOG(("Cached AyuGram language: %1").arg(langId));
	}
}

void AyuLanguage::fetchLanguage(const QString &id, bool mirror) {
	networkManager.setProxy(QNetworkProxy::DefaultProxy);
	if (Core::App().settings().proxy().isEnabled()) {
		const auto proxy = Core::App().settings().proxy().selected();
		if (proxy.type == MTP::ProxyData::Type::Socks5
			|| proxy.type == MTP::ProxyData::Type::Http) {
			networkManager.setProxy(ToNetworkProxy(ToDirectIpProxy(proxy)));
		}
	}
	const auto url = (mirror
		? u"https://raw.githubusercontent.com/AyuGram/Languages/l10n_main/values/langs/%1/Shared.json"_q
		: u"https://cdn.jsdelivr.net/gh/AyuGram/Languages@l10n_main/values/langs/%1/Shared.json"_q).arg(id);
	auto request = QNetworkRequest(QUrl(url));
	request.setTransferTimeout(10000);
	const auto reply = networkManager.get(request);
	_chkReply = reply;
	connect(reply, &QNetworkReply::finished, this, [=] {
		reply->deleteLater();
		if (_chkReply != reply) return;
		_chkReply = nullptr;
		const auto data = reply->readAll();
		const auto doc = QJsonDocument::fromJson(data);
		if (reply->error() == QNetworkReply::NoError && ValidLanguage(doc)) {
			saveCachedLanguage(data, id);
			_document = doc;
			applyLanguageJson(doc);
		} else if (!mirror) {
			fetchLanguage(id, true);
		} else if (!_baseLangId.isEmpty() && id != _baseLangId) {
			fetchLanguage(_baseLangId);
		} else {
			LOG(("AyuGram language unavailable: %1").arg(id));
		}
	});
}

void AyuLanguage::applyLanguageJson(QJsonDocument doc) {
	const auto json = doc.object();
	for (const QString &brokenKey : json.keys()) {
		auto key = qsl("ayu_") + brokenKey;
		auto val = json.value(brokenKey).toString().replace(qsl("&amp;"), qsl("&"));

		if (val.isEmpty() || key.endsWith("_Android")) {
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
	if (!_applying) {
		_applying = true;
		Lang::GetInstance().notifyUpdated();
		_applying = false;
	}
}
