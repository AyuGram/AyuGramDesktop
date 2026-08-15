// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#pragma once

#include <functional>

namespace Ayu::STT {

void DownloadWithProgress(
	const QString &url,
	const QString &destPath,
	const QString &sha256Expected,
	const std::function<void(int percent)>& onProgress,
	const std::function<void(bool ok)> &onDone);

} // namespace Ayu::STT
