// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu_translator.h"

#include <optional>
#include <QtCore/QCryptographicHash>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>

#include "api/api_text_entities.h"
#include "ayu/ayu_settings.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "implementations/google.h"
#include "implementations/telegram.h"
#include "implementations/yandex.h"
#include "main/main_session.h"

// todo: expose available languages from current translator and use in `ChooseTranslateToBox`

namespace Ayu::Translator {

TranslateManager::Builder::Builder(
	TranslateManager &manager,
	Main::Session &session,
	const MTPflags<MTPmessages_translateText::Flags> &flags,
	const MTPInputPeer &peer,
	const MTPVector<MTPint> &id,
	const MTPVector<MTPTextWithEntities> &text,
	const MTPstring &to_lang
)
	: _manager(&manager)
	  , _session(&session)
	  , _flags(flags)
	  , _peer(peer)
	  , _idList(id)
	  , _text(text)
	  , _toLang(to_lang) {
}

TranslateManager::Builder &TranslateManager::Builder::done(std::function < void(const Result &) > cb) {
	_done = std::move(cb);
	return *this;
}

TranslateManager::Builder &TranslateManager::Builder::fail(std::function < void(const MTP::Error &) > cb) {
	_fail = std::move(cb);
	return *this;
}

TranslateManager::Builder &TranslateManager::Builder::fail(std::function<void()> cb) {
	_fail = [cb = std::move(cb)](const MTP::Error &)
	{
		cb();
	};
	return *this;
}

mtpRequestId TranslateManager::Builder::send() {
	return _manager->performTranslation(*this);
}

void TranslateManager::Builder::cancel() {
	if (_id) {
		_manager->cancel(_id);
		_id = 0;
	}
}

TranslateManager::Builder TranslateManager::request(
	Main::Session &session,
	const MTPflags<MTPmessages_translateText::Flags> &flags,
	const MTPInputPeer &peer,
	const MTPVector<MTPint> &id,
	const MTPVector<MTPTextWithEntities> &text,
	const MTPstring &to_lang
) {
	return Builder(*this, session, flags, peer, id, text, to_lang);
}

mtpRequestId TranslateManager::performTranslation(Builder &req) {
	const auto id = _nextId++;
	_pending.emplace(id,
					 Pending{
						 .done = std::move(req._done),
						 .fail = std::move(req._fail),
						 .cancel = nullptr,
					 });
	req._id = id;

	std::vector<TextWithEntities> texts;
	std::vector<QString> cacheKeys;
	std::vector<TextWithEntities> resultTexts;
	std::vector<int> uncachedIndices;
	std::vector<TextWithEntities> uncachedTexts;

	const auto toLang = qs(req._toLang);
	const auto fromLang = QStringLiteral("auto");

	if (!req.texts().v.isEmpty()) {
		for (int i = 0; i < req.texts().v.size(); ++i) {
			const auto text = qs(req.texts().v[i].data().vtext());
			const auto entities = Api::EntitiesFromMTP(req.session(), req.texts().v[i].data().ventities().v);
			const auto textWithEntities = TextWithEntities{
				.text = text,
				.entities = entities
			};
			texts.push_back(textWithEntities);

			// todo: entities are not considered in cache key
			const auto key = generateCacheKey(text, fromLang, toLang);
			cacheKeys.push_back(key);

			if (const auto cached = getFromCache(key)) {
				resultTexts.push_back(cached->translatedText);
			} else {
				resultTexts.push_back({});
				uncachedIndices.push_back(i);
				uncachedTexts.push_back(textWithEntities);
			}
		}
	} else if (!req.ids().v.isEmpty()) {
		if (const auto peerData = Data::PeerFromInputMTP(&req.session()->data(), req.peer())) {
			for (int i = 0; i < req.ids().v.size(); ++i) {
				const auto msgId = req.ids().v[i].v;
				if (const auto message = req.session()->data().message(peerData->id, msgId)) {
					const auto textWithEntities = message->originalText();
					texts.push_back(textWithEntities);

					const auto key = generateMessageCacheKey(peerData->id, msgId, fromLang, toLang);
					cacheKeys.push_back(key);

					if (const auto cached = getFromCache(key)) {
						resultTexts.push_back(cached->translatedText);
					} else {
						resultTexts.push_back({});
						uncachedIndices.push_back(i);
						uncachedTexts.push_back(textWithEntities);
					}
				} else {
					// todo: ??
					texts.push_back({});
					cacheKeys.push_back(QString());
					resultTexts.push_back({});
				}
			}
		}
	}

	if (texts.empty() || toLang.isEmpty()) {
		triggerFail(id);
		return id;
	}

	if (uncachedTexts.empty()) {
		auto vec = QVector<MTPTextWithEntities>();
		for (const auto &translatedText : resultTexts) {
			vec.push_back(MTP_textWithEntities(
				MTP_string(translatedText.text),
				Api::EntitiesToMTP(req.session(), translatedText.entities)));
		}
		const auto result = MTP_messages_translateResult(MTP_vector<MTPTextWithEntities>(vec));
		triggerDone(id, result);
		return id;
	}

	CallbackSuccess onSuccess = [this, id, resultTexts = std::move(resultTexts), cacheKeys = std::move(cacheKeys),
			uncachedIndices = std::move(uncachedIndices), texts = std::move(texts),
			fromLang, toLang, &req](const std::vector<TextWithEntities> &translated) mutable
	{
		for (size_t i = 0; i < translated.size() && i < uncachedIndices.size(); ++i) {
			const auto index = uncachedIndices[i];
			resultTexts[index] = translated[i];

			const auto &key = cacheKeys[index];
			if (!key.isEmpty()) {
				insertToCache(key,
							  CacheEntry{
								  .originalText = texts[index],
								  .translatedText = translated[i],
								  .fromLang = fromLang,
								  .toLang = toLang
							  });
			}
		}

		auto vec = QVector<MTPTextWithEntities>();
		for (const auto &translatedText : resultTexts) {
			vec.push_back(MTP_textWithEntities(
				MTP_string(translatedText.text),
				Api::EntitiesToMTP(req.session(), translatedText.entities)));
		}
		const auto result = MTP_messages_translateResult(MTP_vector<MTPTextWithEntities>(vec));
		triggerDone(id, result);
	};

	CallbackFail onFail = [this, id]
	{
		triggerFail(id);
	};

	const auto args = StartTranslationArgs{
		.session = req.session(),
		.requestData = {
			.flags = req.flags(),
			.peer = req.peer(),
			.idList = req.ids(),
			.text = req.texts(),
			.toLang = req.toLang(),
		},
		.parsedData = {
			.texts = uncachedTexts,
			.fromLang = fromLang,
			.toLang = toLang,
		},
		.onSuccess = std::move(onSuccess),
		.onFail = std::move(onFail),
	};

	if (const auto it = _pending.find(id); it != _pending.end()) {
		const auto &settings = AyuSettings::getInstance();
		if (settings.translationProvider == "telegram") {
			it->second.cancel = TelegramTranslator::instance().startTranslation(args);
		} else if (settings.translationProvider == "yandex") {
			it->second.cancel = YandexTranslator::instance().startTranslation(args);
		} else {
			it->second.cancel = GoogleTranslator::instance().startTranslation(args);
		}
	}

	return id;
}

bool TranslateManager::cancel(mtpRequestId requestId) {
	const auto it = _pending.find(requestId);
	if (it == _pending.end()) return false;
	if (it->second.cancel) {
		it->second.cancel();
	}
	_pending.erase(it);
	return true;
}

bool TranslateManager::triggerDone(mtpRequestId id, const Result &result) {
	const auto it = _pending.find(id);
	if (it == _pending.end()) return false;
	auto cb = std::move(it->second.done);
	_pending.erase(it);
	if (cb) cb(result);
	return true;
}

bool TranslateManager::triggerFail(mtpRequestId id) {
	const auto it = _pending.find(id);
	if (it == _pending.end()) return false;
	auto cb = std::move(it->second.fail);
	_pending.erase(it);
	if (cb) cb(MTP::Error(MTP::Error::MTPLocal("RESPONSE_PARSE_FAILED", "Error parse failed.")));
	return true;
}

void TranslateManager::resetCache() {
	_cacheList.clear();
	_cacheMap.clear();

	// todo: remove all running requests
}

TranslateManager *TranslateManager::currentInstance() {
	return instance;
}

TranslateManager *TranslateManager::instance = nullptr;

void TranslateManager::init() {
	if (!instance) instance = new TranslateManager;
}

QString TranslateManager::generateCacheKey(const QString &text, const QString &fromLang, const QString &toLang) const {
	const auto textHash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1).toHex();
	return QStringLiteral("%1_%2_%3").arg(QString::fromLatin1(textHash), fromLang, toLang);
}

QString TranslateManager::generateMessageCacheKey(PeerId peerId,
												  MsgId msgId,
												  const QString &fromLang,
												  const QString &toLang) const {
	return QStringLiteral("%1_%2_%3_%4").arg(peerId.value).arg(msgId.bare).arg(fromLang, toLang);
}

void TranslateManager::insertToCache(const QString &key, const CacheEntry &entry) {
	if (const auto it = _cacheMap.find(key); it != _cacheMap.end()) {
		_cacheList.erase(it->second);
		_cacheMap.erase(it);
	}

	_cacheList.emplace_front(key, entry);
	_cacheMap[key] = _cacheList.begin();

	if (_cacheList.size() > MAX_CACHE_SIZE) {
		removeLeastRecentlyUsed();
	}
}

std::optional<TranslateManager::CacheEntry> TranslateManager::getFromCache(const QString &key) {
	const auto it = _cacheMap.find(key);
	if (it == _cacheMap.end()) {
		return std::nullopt;
	}

	auto entry = it->second->second;
	_cacheList.erase(it->second);
	_cacheList.emplace_front(key, entry);
	_cacheMap[key] = _cacheList.begin();

	return entry;
}

void TranslateManager::removeLeastRecentlyUsed() {
	if (_cacheList.empty()) return;

	const auto &lru = _cacheList.back();
	_cacheMap.erase(lru.first);
	_cacheList.pop_back();
}

}