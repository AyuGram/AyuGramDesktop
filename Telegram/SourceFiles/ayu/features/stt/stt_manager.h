// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <functional>
#include <QtCore/QString>

namespace Ayu::STT {

class STTManager {
public:
	static STTManager &instance();
	void transcribe(const QString &filePath, std::function<void(QString)> callback);
	static void requestPermission();
	[[nodiscard]] static QString modelPath(int modelType);
	[[nodiscard]] static QString modelUrl(int modelType);
	[[nodiscard]] static bool modelExists(int modelType);
	[[nodiscard]] static QString modelsDirectory();

private:
	STTManager() = default;
};

} // namespace Ayu::STT
