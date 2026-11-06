// Drakeygram Desktop
// Fully independent fork. No external tracking.

#include "ayu/utils/rc_manager.h"

#include <QTimer>

// Completely local. No network requests. No AyuGram / Extera endpoints.

std::unordered_set<ID> default_developers = {};
std::unordered_set<ID> default_channels = {};

void RCManager::start() {
	DEBUG_LOG(("RCManager: local-only mode (network disabled)"));
	// Intentionally no network manager, no timer, no requests.
	// All remote config / developer lists / badges are disabled.
	initialized = true;
}

void RCManager::makeRequest() {
	// no-op
}

void RCManager::sendRequest() {
	// no-op
}

bool RCManager::tryRetryWithExteraFallback() {
	return false;
}

void RCManager::gotResponse() {
	// no-op
}

bool RCManager::handleResponse(const QByteArray &response) {
	return false;
}

bool RCManager::applyResponse(const QByteArray &response) {
	return false;
}

void RCManager::gotFailure(QNetworkReply::NetworkError e) {
	// no-op
}

void RCManager::clearSentRequest() {
	// no-op
}

RCManager::~RCManager() {
	_manager = nullptr;
}
