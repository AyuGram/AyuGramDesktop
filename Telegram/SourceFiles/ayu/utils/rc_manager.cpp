// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#include "rc_manager.h"

#include <QJsonArray>
#include <qjsondocument.h>
#include <QTimer>

#include "base/unixtime.h"

std::unordered_set<ID> default_developers = {
	963080346, 1282540315, 1374434073, 168769611,
	1773117711, 5330087923, 666154369, 139303278,
	668557709, 1348136086, 6288255532, 7453676178,
	// -------------------------------------------
	778327202, 238292700, 1795176335, 6247153446,
	1752394339, 7745305003, 1183312839, 497855299,
	623054735
};

std::unordered_set<ID> default_channels = {
	1233768168, 1524581881, 1571726392, 1632728092,
	1172503281, 1877362358, 1905581924, 1794457129,
	1434550607, 1947958814, 1815864846, 2130395384,
	1976430343, 1754537498, 1725670701,
};

void RCManager::start() {
	DEBUG_LOG(("RCManager: starting"));
	_manager = std::make_unique<QNetworkAccessManager>();

	makeRequest();

	_timer = new QTimer(this);
	connect(_timer, &QTimer::timeout, this, &RCManager::makeRequest);
	_timer->start(60 * 60 * 1000); // 1 hour
}

void RCManager::makeRequest() {
	if (!_manager) {
		return;
	}

	LOG(("RCManager: requesting map"));

	clearSentRequest();

	const auto request = QNetworkRequest(QUrl("https://update.ayugram.one/rc/current/desktop"));
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

void RCManager::gotResponse() {
	if (!_reply) {
		return;
	}

	cSetLastUpdateCheck(base::unixtime::now());
	const auto response = _reply->readAll();
	clearSentRequest();

	if (!handleResponse(response)) {
		LOG(("RCManager: Error bad map size: %1").arg(response.size()));
		gotFailure(QNetworkReply::UnknownContentError);
	}
}

bool RCManager::handleResponse(const QByteArray &response) {
	return applyResponse(response);
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
	const auto channels = root.value("channels").toArray();
	const auto supporters = root.value("supporters").toArray();

	_developers.clear();
	_channels.clear();
	_supporters.clear();

	for (const auto &developer : developers) {
		if (const auto id = developer.toVariant().toLongLong()) {
			_developers.insert(id);
		}
	}

	for (const auto &channel : channels) {
		if (const auto id = channel.toVariant().toLongLong()) {
			_channels.insert(id);
		}
	}

	for (const auto &supporter : supporters) {
		if (const auto id = supporter.toVariant().toLongLong()) {
			_supporters.insert(id);
		}
	}

	initialized = true;

	LOG(("RCManager: Loaded %1 developers, %2 channels"
	).arg(_developers.size()).arg(_channels.size()));

	return true;
}

void RCManager::gotFailure(QNetworkReply::NetworkError e) {
	LOG(("RCManager: Error %1").arg(e));
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
