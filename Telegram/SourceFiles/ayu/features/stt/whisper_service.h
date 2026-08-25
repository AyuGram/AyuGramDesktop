// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#pragma once

#include "base/timer.h"

#include <functional>
#include <vector>
#include <QtCore/QMutex>
#include <QtCore/QString>

struct whisper_context;

namespace Ayu::STT {

class WhisperService {
public:
	static WhisperService &instance();

	void transcribeOnDemand(
		const QString &filePath,
		const QString &modelPath,
		const QString &language,
		std::function<void(QString)> callback);
	static std::vector<float> decodeAudioToPcm(const QString &filePath);
	void scheduleFreeContext();
	void freeContext(bool unloadBackend = false);
	bool isQuitPrevent();

private:
	WhisperService();

	QMutex _ctxMutex;
	whisper_context *_cachedCtx = nullptr; // guarded by _ctxMutex
	QString _cachedModelPath; // guarded by _ctxMutex
	QString _loadedGgmlBackendPath; // guarded by _ctxMutex
	void *_loadedGgmlBackendReg = nullptr; // guarded by _ctxMutex
	base::Timer _idleTimer; // main thread only
	bool _freeInFlight = false; // main thread only
};

} // namespace Ayu::STT
