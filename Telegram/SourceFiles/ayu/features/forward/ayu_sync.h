// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "apiwrap.h"
#include "base/random.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "history/history_item.h"
#include "storage/file_download.h"
#include "storage/file_upload.h"
#include "storage/storage_account.h"
#include "ui/chat/attach/attach_prepare.h"

namespace AyuSync {

QString pathForSave(not_null<Main::Session*> session);
QString filePath(not_null<Main::Session*> session, const Data::Media *media);
void loadDocuments(not_null<Main::Session*> session, const std::vector<not_null<HistoryItem*>> &items);
bool isMediaDownloadable(Data::Media *media);
void sendMessageSync(not_null<Main::Session*> session, Api::MessageToSend &message);

void sendDocumentSync(not_null<Main::Session*> session,
					  Ui::PreparedGroup &group,
					  SendMediaType type,
					  TextWithTags &&caption,
					  const Api::SendAction &action);

void sendStickerSync(not_null<Main::Session*> session,
					 Api::MessageToSend &message,
					 not_null<DocumentData*> document);
void waitForMsgSync(not_null<Main::Session*> session, const Api::SendAction &action);
void loadPhotoSync(not_null<Main::Session*> session, const std::pair<not_null<PhotoData*>, FullMsgId> &photos);
void loadDocumentSync(not_null<Main::Session*> session, DocumentData *data, not_null<HistoryItem*> item);
void forwardMessagesSync(not_null<Main::Session*> session,
						 const std::vector<not_null<HistoryItem*>> &items,
						 const ApiWrap::SendAction &action,
						 Data::ForwardOptions options);
void sendVoiceSync(not_null<Main::Session*> session,
				   const QByteArray &data,
				   int64_t duration,
				   bool video,
				   const Api::SendAction &action);
}
