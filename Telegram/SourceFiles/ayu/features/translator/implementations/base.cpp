// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include "base/random.h"

#include <cmath>
#include <memory>
#include <vector>

#include "./base.h"

namespace Ayu::Translator {

std::vector<QString> desktopUserAgents = {
	// zen
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:142.0) Gecko/20100101 Firefox/142.0",
	// cent
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/118.0.0.0 Safari/537.36",
	// orion mac
	"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Safari/605.1.15",
	// chrome mac
	"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/139.0.0.0 Safari/537.36"
};

QString randomDesktopUserAgent() {
	return desktopUserAgents[base::RandomIndex(static_cast<int>(desktopUserAgents.size()))];
}

bool shouldWrapInHtml() {
	// todo: make an option
	return true;
}

QString parseJsonPath(const QByteArray &body, const QString &jsonPath, bool *ok) {
	if (ok) *ok = false;
	if (body.isEmpty()) {
		return {};
	}

	QJsonParseError err{};
	const auto doc = QJsonDocument::fromJson(body, &err);
	if (err.error != QJsonParseError::NoError || doc.isNull()) {
		return {};
	}

	QJsonValue current;
	if (doc.isObject()) {
		current = doc.object();
	} else if (doc.isArray()) {
		current = doc.array();
	} else {
		return {};
	}

	auto indexInto = [](const QJsonValue &val, int idx, bool *okPtr) -> QJsonValue
	{
		if (!val.isArray()) {
			if (okPtr) *okPtr = false;
			return QJsonValue{};
		}
		const auto arr = val.toArray();
		if (idx < 0 || idx >= arr.size()) {
			if (okPtr) *okPtr = false;
			return QJsonValue{};
		}
		if (okPtr) *okPtr = true;
		return arr.at(idx);
	};

	const auto parts = jsonPath.split('.', Qt::SkipEmptyParts);
	for (const auto &partRaw : parts) {
		QString part = partRaw;

		int pos = 0;
		if (!part.isEmpty() && part[0] != '[') {
			const int bracket = part.indexOf('[');
			QString key = (bracket >= 0) ? part.left(bracket) : part;
			pos = key.size();

			if (!key.isEmpty()) {
				if (!current.isObject()) {
					return {};
				}
				current = current.toObject().value(key);
				if (current.isUndefined() || current.isNull()) {
					return {};
				}
			}
		}

		while (pos < part.size() && part[pos] == '[') {
			const int end = part.indexOf(']', pos + 1);
			if (end < 0) return {};
			const auto idxStr = part.mid(pos + 1, end - pos - 1);
			bool okIndex = false;
			int idx = idxStr.toInt(&okIndex);
			if (!okIndex) return {};

			bool okStep = false;
			current = indexInto(current, idx, &okStep);
			if (!okStep) return {};
			pos = end + 1;
		}
	}

	QString result;
	if (current.isString()) {
		result = current.toString();
	} else if (current.isArray()) {
		const auto arr = current.toArray();
		result.reserve(256);
		for (const auto &v : arr) {
			if (v.isObject()) {
				const auto o = v.toObject();
				if (o.contains("trans")) {
					result += o.value("trans").toString();
				} else if (o.contains("text")) {
					result += o.value("text").toString();
				}
			} else if (v.isString()) {
				result += v.toString();
			}
		}
	} else if (current.isObject()) {
		const auto o = current.toObject();
		if (o.contains("trans")) {
			result = o.value("trans").toString();
		} else if (o.contains("text")) {
			result = o.value("text").toString();
		}
	}

	if (ok) *ok = !result.isNull();
	return result;
}

CallbackCancel MultiThreadTranslator::startTranslation(const StartTranslationArgs &args) {
	const auto &texts = args.parsedData.texts;
	const auto &fromLang = args.parsedData.fromLang;
	const auto &toLang = args.parsedData.toLang;
	if (texts.empty() || toLang.trimmed().isEmpty()) {
		if (args.onFail) args.onFail();
		return []
		{
		};
	}

	struct BatchState
	{
		MultiThreadTranslator *self = nullptr;
		std::vector<TextWithEntities> inputs;
		QString from;
		QString to;
		CallbackSuccess onSuccess;
		CallbackFail onFail;

		std::vector<TextWithEntities> results;
		std::vector<QPointer<QNetworkReply>> replies;
		std::vector<int> retryCount;
		std::vector<QPointer<QTimer>> retryTimers;
		int total = 0;
		int nextIndex = 0;
		int inProgress = 0;
		bool finished = false;

		std::function<void()> pump;
		std::function<void(int)> tryTranslateIndex;

		void cancelAll() const {
			for (auto &r : replies) {
				if (r && r->isRunning()) {
					r->abort();
				}
			}
			for (auto &timer : retryTimers) {
				if (timer) {
					timer->stop();
				}
			}
		}
	};

	auto state = std::make_shared<BatchState>();
	state->self = this;
	state->inputs = texts;
	state->from = fromLang;
	state->to = toLang;
	state->onSuccess = args.onSuccess;
	state->onFail = args.onFail;
	state->total = static_cast<int>(texts.size());
	state->results.resize(state->total);
	state->replies.resize(state->total);
	state->retryCount.resize(state->total, 0);
	state->retryTimers.resize(state->total);

	const auto maxConcurrent = getConcurrencyLimit();
	const auto maxRetries = getMaxRetries();
	const auto baseWaitTime = getBaseWaitTimeMs();

	auto finishFail = [state]()
	{
		if (state->finished) return;
		state->finished = true;
		state->cancelAll();
		if (state->onFail) state->onFail();
	};

	auto finishSuccess = [state]()
	{
		if (state->finished) return;
		state->finished = true;
		if (state->onSuccess) state->onSuccess(state->results);
	};

	state->tryTranslateIndex = [state, finishFail, finishSuccess, maxRetries, baseWaitTime](int i) mutable
	{
		if (state->finished) return;

		MultiThreadArgs singleArgs;
		singleArgs.parsedData.text = state->inputs[i];
		singleArgs.parsedData.fromLang = state->from;
		singleArgs.parsedData.toLang = state->to;
		singleArgs.onSuccess = [state, i, finishSuccess](const TextWithEntities &translated) mutable
		{
			if (state->finished) return;
			state->results[i] = translated;
			state->replies[i] = nullptr;
			state->inProgress--;
			if (state->nextIndex >= state->total && state->inProgress == 0) {
				finishSuccess();
				return;
			}
			if (!state->finished && state->pump) {
				state->pump();
			}
		};
		singleArgs.onFail = [state, i, finishFail, maxRetries, baseWaitTime]() mutable
		{
			if (state->finished) return;

			state->replies[i] = nullptr;
			state->retryCount[i]++;

			if (state->retryCount[i] >= maxRetries) {
				finishFail();
				return;
			}

			const int delayMs = static_cast<int>(baseWaitTime * std::pow(2.0, state->retryCount[i] - 1));

			auto timer = new QTimer(state->self);
			state->retryTimers[i] = timer;
			timer->setSingleShot(true);

			QObject::connect(timer,
							 &QTimer::timeout,
							 [state, i, timer]() mutable
							 {
								 if (state->finished) return;
								 timer->deleteLater();
								 state->retryTimers[i] = nullptr;
								 state->tryTranslateIndex(i);
							 });

			timer->start(delayMs);
		};

		const auto r = state->self->startSingleTranslation(singleArgs);
		state->replies[i] = r;
		if (!r && !state->finished) {
			singleArgs.onFail();
		}
	};

	state->pump = [state, maxConcurrent]() mutable
	{
		if (state->finished) return;
		while (!state->finished && state->inProgress < maxConcurrent && state->nextIndex < state->total) {
			const int i = state->nextIndex++;
			state->inProgress++;
			state->tryTranslateIndex(i);
		}
	};

	state->pump();

	return [state, finishFail]() mutable
	{
		finishFail();
	};
}

}
