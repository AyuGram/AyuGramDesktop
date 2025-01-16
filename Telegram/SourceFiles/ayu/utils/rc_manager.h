// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#pragma once

#include <QtNetwork/QNetworkReply>

#include "ayu/data/entities.h"

extern std::unordered_set<ID> default_developers;
extern std::unordered_set<ID> default_channels;

class RCManager final : public QObject
{
	Q_OBJECT
public:
	static RCManager &getInstance() {
		static RCManager instance;
		return instance;
	}

	RCManager(const RCManager &) = delete;
	RCManager &operator=(const RCManager &) = delete;
	RCManager(RCManager &&) = delete;
	RCManager &operator=(RCManager &&) = delete;

	void start();

	[[nodiscard]] const std::unordered_set<ID> &developers() const {
		if (!initialized) {
			return default_developers;
		}
		return _developers;
	}

	[[nodiscard]] const std::unordered_set<ID> &channels() const {
		if (!initialized) {
			return default_channels;
		}
		return _channels;
	}

	[[nodiscard]] const std::unordered_set<ID> &supporters() const {
		return _supporters;
	}

private:
	RCManager() = default;
	~RCManager();

	void makeRequest();

	void gotResponse();
	void gotFailure(QNetworkReply::NetworkError e);
	void clearSentRequest();
	bool handleResponse(const QByteArray &response);
	bool applyResponse(const QByteArray &response);

	bool initialized = false;

	std::unordered_set<ID> _developers = {};
	std::unordered_set<ID> _channels = {};
	std::unordered_set<ID> _supporters = {};

	QTimer* _timer = nullptr;

	std::unique_ptr<QNetworkAccessManager> _manager = nullptr;
	QNetworkReply *_reply = nullptr;

};
