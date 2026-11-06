// Drakeygram Desktop
// Fully independent fork. No external tracking.

#pragma once

#include "ayu/data/entities.h"

#include <QtNetwork/QNetworkReply>

extern std::unordered_set<ID> default_developers;
extern std::unordered_set<ID> default_channels;

struct CustomBadge
{
	EmojiStatusId emojiStatusId;
	QString text;
};

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
		return default_developers;
	}

	[[nodiscard]] const std::unordered_set<ID> &channels() const {
		return default_channels;
	}

	[[nodiscard]] const std::unordered_set<ID> &supporters() const {
		return _supporters;
	}

	[[nodiscard]] const std::unordered_set<ID> &supporterChannels() const {
		return _supporterChannels;
	}

	[[nodiscard]] const std::unordered_map<ID, CustomBadge> &supporterCustomBadges() const {
		return _customBadges;
	}

	[[nodiscard]] QString donateUsername() const {
		return QString();
	}

	[[nodiscard]] QString donateAmountUsd() const {
		return QString();
	}

	[[nodiscard]] QString donateAmountTon() const {
		return QString();
	}

	[[nodiscard]] QString donateAmountRub() const {
		return QString();
	}

private:
	RCManager() = default;
	~RCManager();

	void makeRequest();
	void sendRequest();
	bool tryRetryWithExteraFallback();

	void gotResponse();
	void gotFailure(QNetworkReply::NetworkError e);
	void clearSentRequest();
	bool handleResponse(const QByteArray &response);
	bool applyResponse(const QByteArray &response);

	bool initialized = false;

	std::unordered_set<ID> _developers = {};
	std::unordered_set<ID> _officialChannels = {};
	std::unordered_set<ID> _supporters = {};
	std::unordered_set<ID> _supporterChannels = {};
	std::unordered_map<ID, CustomBadge> _customBadges = {};

	// Donate fields intentionally empty - no AyuGram links
	QString _donateUsername;
	QString _donateAmountUsd;
	QString _donateAmountTon;
	QString _donateAmountRub;

	QTimer* _timer = nullptr;

	std::unique_ptr<QNetworkAccessManager> _manager = nullptr;
	QNetworkReply *_reply = nullptr;
	bool _useExteraFallback = false;
	bool _retryAttempted = false;
};
