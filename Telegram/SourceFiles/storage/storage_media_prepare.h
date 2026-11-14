/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "core/file_utilities.h"

namespace tr {
template <typename ...>
struct phrase;
} // namespace tr

namespace Ui {
struct PreparedFileInformation;
struct PreparedFile;
struct PreparedList;
struct PreparedBundle;
class SendFilesWay;
enum class AlbumType;
} // namespace Ui

namespace Editor {
struct PhotoModifications;
} // namespace Editor

namespace Storage {

enum class MimeDataState {
	None,
	Files,
	FilesArchive,
	FilesArchiveOnly,
	PhotoFiles,
	PhotoFilesArchive,
	MediaFiles,
	MediaFilesArchive,
	//PremiumFile,
	Image,
	Media,
	Folder,
	FolderArchiveOnly,
};

[[nodiscard]] std::optional<Ui::PreparedList> PreparedFileFromFilesDialog(
	FileDialog::OpenResult &&result,
	Fn<bool(const Ui::PreparedList&)> checkResult,
	Fn<void(tr::phrase<>)> errorCallback,
	int previewWidth,
	bool premium);
[[nodiscard]] MimeDataState ComputeMimeDataState(const QMimeData *data);
[[nodiscard]] bool ValidatePhotoEditorMediaDragData(
	not_null<const QMimeData*> data,
	bool withVideo);

struct PhotoEditorMedia {
	QImage image;
	QString videoPath;
	QByteArray videoContent;
	crl::time videoDuration = 0;

	[[nodiscard]] bool video() const {
		return !videoPath.isEmpty() || !videoContent.isEmpty();
	}
	[[nodiscard]] explicit operator bool() const {
		return !image.isNull();
	}
};
[[nodiscard]] PhotoEditorMedia ReadPhotoEditorMedia(
	const QString &path,
	const QByteArray &content);
[[nodiscard]] bool ValidateEditMediaDragData(
	not_null<const QMimeData*> data,
	Ui::AlbumType albumType);
[[nodiscard]] Ui::PreparedList PrepareMediaList(
	const QList<QUrl> &files,
	int previewWidth,
	bool premium,
	Fn<void(const Ui::PreparedList &)> errorCallback = nullptr);
[[nodiscard]] Ui::PreparedList PrepareMediaList(
	const QStringList &files,
	int previewWidth,
	bool premium,
	Fn<void(const Ui::PreparedList &)> errorCallback = nullptr);
[[nodiscard]] Ui::PreparedList PrepareMediaFromImage(
	QImage &&image,
	QByteArray &&content,
	int previewWidth);
void PrepareDetails(Ui::PreparedFile &file, int previewWidth, int sideLimit);

// Paste hints: MIME types a copier may offer on the clipboard next to the
// files or the image it copies, to say what it means by them. Only their
// presence counts, the payload is ignored. They apply to a plain paste
// only, never to drops, "send as" choices or edits.
//
// kStickerHintMimeType: the files are stickers. When every pasted file is
// one Telegram sends as a sticker, "send as file" is preselected (an
// uncompressed WebP goes out as a sticker).
// kNoDialogHintMimeType: send the paste at once, the way the send files
// box would preselect it, without the box. Not honoured when another hint
// on the paste can't be, so the box shows what will be sent instead.
inline constexpr auto kStickerHintMimeType = "application/x-sticker";
inline constexpr auto kNoDialogHintMimeType = "application/x-no-dialog";

struct PasteHints {
	bool sticker = false;
	bool noDialog = false;
};
[[nodiscard]] PasteHints ReadPasteHints(not_null<const QMimeData*> data);

// Whether the file goes out as a sticker when it is sent uncompressed.
[[nodiscard]] bool SendsAsSticker(const Ui::PreparedFile &file);

// Applies the hints to a list prepared from a pasted file list or image.
// Returns true when the list should be sent at once, without the box.
[[nodiscard]] bool ApplyPasteHints(Ui::PreparedList &list, PasteHints hints);

// The bundle the send files box would send for the list with the way it
// preselects (starting from the user's saved way) and no caption.
[[nodiscard]] std::shared_ptr<Ui::PreparedBundle> PrepareBundleWithoutBox(
	Ui::PreparedList &&list,
	Ui::SendFilesWay way,
	bool slowmode);

struct VideoDetails {
	QSize originalDimensions;
	QSize shownDimensions;
	QImage preview;
};

[[nodiscard]] VideoDetails ComputeVideoDetails(
	const QImage &thumbnail,
	const Editor::PhotoModifications &geometry,
	int previewWidth,
	int sideLimit);
void ApplyVideoDetails(Ui::PreparedFile &file, VideoDetails &&details);
void UpdateVideoDetails(
	Ui::PreparedFile &file,
	int previewWidth,
	int sideLimit);
void UpdateImageDetails(
	Ui::PreparedFile &file,
	int previewWidth,
	int sideLimit);

bool ApplyModifications(Ui::PreparedList &list, bool composeAnimated = false);

} // namespace Storage
