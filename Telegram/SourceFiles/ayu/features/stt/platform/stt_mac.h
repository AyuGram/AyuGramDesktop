// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#pragma once

#include <functional>
#include <QtCore/QString>

namespace Ayu::STT::Mac {

void transcribeFile(const QString &filePath, const QString &language, std::function<void(QString)> callback);
void requestSpeechPermission();

} // namespace Ayu::STT::Mac
