// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "apiwrap.h"
#include "base/flat_map.h"
#include "base/random.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "history/history_item.h"
#include "storage/file_download.h"
#include "storage/file_upload.h"
#include "storage/storage_account.h"
#include "ui/chat/attach/attach_prepare.h"

namespace Iv {
struct RichPage;
} // namespace Iv

namespace AyuSync {

using DocumentPaths = base::flat_map<not_null<DocumentData*>, QString>;

struct UploadedFile
{
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
};

QString pathForSave(not_null<Main::Session*> session);
QString documentFileName(not_null<DocumentData*> document);
QString filePath(not_null<Main::Session*> session, not_null<PhotoData*> photo);
qint64 fileSize(const QString &path);
[[nodiscard]] DocumentPaths loadDocuments(
	not_null<Main::Session*> session,
	const std::vector<not_null<HistoryItem*>> &items,
	const Fn<bool()> &cancelled);
void sendMessageSync(not_null<Main::Session*> session, Api::MessageToSend &&message);

void sendDocumentSync(not_null<Main::Session*> session,
					  Ui::PreparedGroup &group,
					  SendMediaType type,
					  TextWithTags &&caption,
					  const Api::SendAction &action);

void sendStickerSync(not_null<Main::Session*> session,
					 Api::MessageToSend &&message,
					 not_null<DocumentData*> document);
void waitForMsgSync(not_null<Main::Session*> session, const Api::SendAction &action);
void loadPhotoSync(
	not_null<Main::Session*> session,
	not_null<PhotoData*> photo,
	Data::FileOrigin origin,
	const Fn<bool()> &cancelled);
[[nodiscard]] QString loadDocumentSync(
	not_null<Main::Session*> session,
	not_null<DocumentData*> document,
	Data::FileOrigin origin,
	const Fn<bool()> &cancelled);
void forwardMessagesSync(not_null<Main::Session*> session,
						 const std::vector<not_null<HistoryItem*>> &items,
						 const ApiWrap::SendAction &action,
						 Data::ForwardOptions options);
void sendVoiceSync(not_null<Main::Session*> session,
				   const QByteArray &data,
				   int64_t duration,
				   bool video,
				   Api::MessageToSend &&message);

UploadedFile uploadFileSync(not_null<Main::Session*> session,
							not_null<PeerData*> peer,
							const QString &path,
							SendMediaType type,
							bool forceFile,
							const QString &displayName = {});

std::shared_ptr<const Iv::RichPage> loadFullRichPageSync(
	not_null<Main::Session*> session,
	FullMsgId itemId);

bool sendRichMessageSync(not_null<Main::Session*> session,
						 const MTPInputRichMessage &richMessage,
						 const Api::SendAction &action);
} // namespace AyuSync
