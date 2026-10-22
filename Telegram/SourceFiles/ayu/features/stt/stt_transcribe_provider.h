// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026

#pragma once

#include <functional>
#include <QtCore/QString>

class HistoryItem;

namespace Ayu::STT {

[[nodiscard]] bool ShouldTranscribeLocally(not_null<HistoryItem*> item);
void RequestLocalTranscribe(
	not_null<HistoryItem*> item,
	const std::function<void(QString)>& done);

} // namespace Ayu::STT
