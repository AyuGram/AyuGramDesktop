/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// Packs a file into a password-protected .7z with 7-Zip once it has been saved with Save As.
#pragma once

class DocumentData;

namespace AyuFeatures::Zip {

// The 7-Zip command line program, or an empty string when it is not installed.
[[nodiscard]] QString FindSevenZip();

// Whether saved files get packed: the setting is on and a password is set.
[[nodiscard]] bool Enabled();

// Call right after DocumentData::save to a user-chosen path. Waits for the download, then packs it.
void ArchiveAfterSave(not_null<DocumentData*> document, const QString &path);

// Packs an existing file now; calls done(archivePath, error) on the main thread (error is empty on success).
void Archive(
	const QString &file,
	Fn<void(QString archive, QString error)> done);

} // namespace AyuFeatures::Zip
