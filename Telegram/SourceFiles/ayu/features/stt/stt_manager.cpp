// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#include "ayu/features/stt/stt_manager.h"

#include "ayu/ayu_settings.h"

#include <QtCore/QFile>
#include <QtCore/QStandardPaths>

#if defined(Q_OS_MAC)
#include "ayu/features/stt/platform/stt_mac.h"
#endif

#if defined(HAVE_WHISPER)
#include "ayu/features/stt/whisper_service.h"
#endif

namespace Ayu::STT {

namespace {

constexpr std::array<const char*, 3> kModelFileNames = {
	"ggml-tiny.bin",
	"ggml-base.bin",
	"ggml-small.bin",
};

constexpr std::array<const char*, 3> kModelUrls = {
	"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin",
	"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
	"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin",
};

} // namespace

STTManager &STTManager::instance() {
	static STTManager self;
	return self;
}

QString STTManager::modelsDirectory() {
	const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	return base + u"/whisper_models"_q;
}

QString STTManager::modelPath(const int modelType) {
	if (modelType < 0 || modelType >= static_cast<int>(kModelFileNames.size())) {
		return {};
	}
	return modelsDirectory() + u"/"_q + kModelFileNames[modelType];
}

QString STTManager::modelUrl(const int modelType) {
	if (modelType < 0 || modelType >= static_cast<int>(kModelUrls.size())) {
		return {};
	}
	return QString::fromUtf8(kModelUrls[modelType]);
}

bool STTManager::modelExists(const int modelType) {
	return QFile::exists(modelPath(modelType));
}

void STTManager::requestPermission() {
#if defined(Q_OS_MAC)
	if (AyuSettings::getInstance().sttEngine() == STTEngine::AppleSpeech) {
		Mac::requestSpeechPermission();
	}
#endif
}

void STTManager::transcribe(const QString &filePath, std::function<void(QString)> callback) {
	const auto &settings = AyuSettings::getInstance();

#if defined(Q_OS_MAC)
	if (settings.sttEngine() == STTEngine::AppleSpeech) {
		Mac::transcribeFile(filePath, settings.sttLanguage(), std::move(callback));
		return;
	}
#endif

#if defined(HAVE_WHISPER)
	const auto modelType = static_cast<int>(settings.whisperModelType());
	const auto path = modelPath(modelType);
	const auto language = settings.sttLanguage();

	WhisperService::instance().transcribeOnDemand(
		filePath,
		path,
		language,
		std::move(callback));
	return;
#endif

	// No engine available for the selected configuration: report failure
	// instead of silently dropping the callback (which would hang the spinner).
	callback(QString());
}

} // namespace Ayu::STT
