// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#pragma once

#include "ayu/features/stt/ggml_backend_detector.h"

#include <functional>
#include <QtCore/QString>

namespace Ayu::STT {

class GgmlBackendManager {
public:
	static GgmlBackendManager &instance();

	[[nodiscard]] static QString backendDirectory(GgmlBackendKind kind);

	[[nodiscard]] static QString loadedModulePath(GgmlBackendKind kind);
	[[nodiscard]] static bool isDownloaded(GgmlBackendKind kind);

	static void download(
		GgmlBackendKind kind,
		const std::function<void(int percent)> &onProgress,
		const std::function<void(bool ok)> &onDone);

	static void remove(GgmlBackendKind kind);

private:
	GgmlBackendManager() = default;
};

} // namespace Ayu::STT
