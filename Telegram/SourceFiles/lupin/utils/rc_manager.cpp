// This is the source code of LupinGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "lupin/utils/rc_manager.h"

#include <QJsonArray>
#include <qjsondocument.h>
#include <QTimer>

namespace {

constexpr auto kPrimaryUrl = "https://update.lupingram.invalid/rc/current/desktop2";
constexpr auto kExteraUrl = "https://update.lupingram.invalid/rc/compact";
constexpr auto kFetchTimeout = 15 * 1000;

}

std::unordered_set<ID> default_developers = {};

std::unordered_set<ID> default_channels = {};

void RCManager::start() {
	DEBUG_LOG(("RCManager: starting"));
	// LupinGram: the remote badge map is not used, nothing is fetched.
}

void RCManager::makeRequest() {
	_retryAttempted = false;
	sendRequest();
}

void RCManager::sendRequest() {
	if (!_manager) {
		return;
	}

	const auto url = QString::fromLatin1(_useExteraFallback ? kExteraUrl : kPrimaryUrl);
	LOG(("RCManager: requesting map"));

	clearSentRequest();

	auto request = QNetworkRequest(QUrl(url));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setTransferTimeout(kFetchTimeout);
	_reply = _manager->get(request);
	connect(_reply,
			&QNetworkReply::finished,
			[=]
			{
				gotResponse();
			});
	connect(_reply,
			&QNetworkReply::errorOccurred,
			[=](auto e)
			{
				gotFailure(e);
			});
}

bool RCManager::tryRetryWithExteraFallback() {
	if (_retryAttempted || _useExteraFallback) {
		return false;
	}
	LOG(("RCManager: switching to extera fallback endpoint"));
	_useExteraFallback = true;
	_retryAttempted = true;
	sendRequest();
	return true;
}

void RCManager::gotResponse() {
	if (!_reply) {
		return;
	}

	const auto response = _reply->readAll();
	clearSentRequest();

	if (!handleResponse(response)) {
		LOG(("RCManager: Error bad map size: %1").arg(response.size()));
		gotFailure(QNetworkReply::UnknownContentError);
	}
}

bool RCManager::handleResponse(const QByteArray &response) {
	try {
		return applyResponse(response);
	} catch (...) {
		LOG(("RCManager: Failed to apply response"));
		return false;
	}
}

bool RCManager::applyResponse(const QByteArray &response) {
	auto error = QJsonParseError{0, QJsonParseError::NoError};
	const auto document = QJsonDocument::fromJson(response, &error);
	if (error.error != QJsonParseError::NoError) {
		LOG(("RCManager: Failed to parse JSON, error: %1"
		).arg(error.errorString()));
		return false;
	}
	if (!document.isObject()) {
		LOG(("RCManager: not an object received in JSON"));
		return false;
	}
	const auto root = document.object();

	const auto developers = root.value("developers").toArray();
	const auto officialChannels = root.value("officialChannels").toArray();
	const auto supporters = root.value("supporters").toArray();
	const auto supporterChannels = root.value("supporterChannels").toArray();
	const auto customBadges = root.value("customBadges").toArray();

	_developers.clear();
	_officialChannels.clear();
	_supporters.clear();
	_supporterChannels.clear();
	_customBadges.clear();

	for (const auto &developer : developers) {
		if (const auto id = developer.toVariant().toLongLong()) {
			_developers.insert(id);
		}
	}

	for (const auto &channel : officialChannels) {
		if (const auto id = channel.toVariant().toLongLong()) {
			_officialChannels.insert(id);
		}
	}

	for (const auto &supporter : supporters) {
		if (const auto id = supporter.toVariant().toLongLong()) {
			_supporters.insert(id);
		}
	}

	for (const auto &channel : supporterChannels) {
		if (const auto id = channel.toVariant().toLongLong()) {
			_supporterChannels.insert(id);
		}
	}

	for (const auto &badge : customBadges) {
		if (!badge.isObject()) {
			continue;
		}
		const auto obj = badge.toObject();
		const auto id = obj.value("id").toVariant().toLongLong();
		if (!id) {
			continue;
		}
		const auto badgeObj = obj.value("badge");
		if (!badgeObj.isObject()) {
			continue;
		}
		const auto badgeData = badgeObj.toObject();
		CustomBadge customBadge;
		if (const auto emojiStatusId = badgeData.value("documentId").toVariant().toLongLong()) {
			customBadge.emojiStatusId = EmojiStatusId(emojiStatusId);
		} else {
			continue;
		}
		if (const auto text = badgeData.value("text").toString(); !text.isEmpty()) {
			customBadge.text = text;
		}
		_customBadges[id] = customBadge;
	}

	if (const auto donateUsername = root.value("donateUsername"); donateUsername.isString()) {
		if (const auto value = donateUsername.toString(); !value.isEmpty()) {
			_donateUsername = value;
		}
	}
	if (const auto donateAmountUsd = root.value("donateAmountUsd"); donateAmountUsd.isString()) {
		if (const auto value = donateAmountUsd.toString(); !value.isEmpty()) {
			_donateAmountUsd = value;
		}
	}
	if (const auto donateAmountTon = root.value("donateAmountTon"); donateAmountTon.isString()) {
		if (const auto value = donateAmountTon.toString(); !value.isEmpty()) {
			_donateAmountTon = value;
		}
	}
	if (const auto donateAmountRub = root.value("donateAmountRub"); donateAmountRub.isString()) {
		if (const auto value = donateAmountRub.toString(); !value.isEmpty()) {
			_donateAmountRub = value;
		}
	}

	initialized = true;

	LOG(("RCManager: Loaded %1 developers, %2 official channels"
	).arg(_developers.size()).arg(_officialChannels.size()));

	return true;
}

void RCManager::gotFailure(QNetworkReply::NetworkError e) {
	LOG(("RCManager: Error %1").arg(e));
	if (tryRetryWithExteraFallback()) {
		LOG(("RCManager: retrying request with extera fallback endpoint"));
		return;
	}
	LOG(("RCManager: no retry left for failed request"));
	if (const auto reply = base::take(_reply)) {
		reply->deleteLater();
	}
}

void RCManager::clearSentRequest() {
	const auto reply = base::take(_reply);
	if (!reply) {
		return;
	}
	disconnect(reply, &QNetworkReply::finished, nullptr, nullptr);
	disconnect(reply, &QNetworkReply::errorOccurred, nullptr, nullptr);
	reply->abort();
	reply->deleteLater();
}

RCManager::~RCManager() {
	clearSentRequest();
	_manager = nullptr;
}
