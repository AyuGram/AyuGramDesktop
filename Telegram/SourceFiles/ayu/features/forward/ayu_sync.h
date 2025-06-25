#pragma once
#include "data/data_media_types.h"
#include "data/data_session.h"
#include <base/random.h>
#include <data/data_histories.h>
#include <data/data_peer.h>
#include <history/history_item.h>

#include "apiwrap.h"
#include "base/unixtime.h"
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "storage/file_download_mtproto.h"
#include "storage/file_upload.h"
#include "api/api_peer_photo.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "storage/localimageloader.h"
#include "storage/storage_media_prepare.h"
#include "ui/chat/attach/attach_prepare.h"
#include "storage/file_download.h"
#include "storage/storage_account.h"


namespace AyuSync {

QString pathForSave(not_null<Main::Session *> session);
QString filePath(not_null<Main::Session *> session, const Data::Media *media);
void loadDocuments(not_null<Main::Session *> session, const std::vector<not_null<HistoryItem *> > &items);
bool isMediaDownloadable(Data::Media *media);
void sendMessageSync(not_null<Main::Session *> session, Api::MessageToSend &message);

void sendDocumentSync(not_null<Main::Session *> session,
	Ui::PreparedList &&list,
	SendMediaType type,
	TextWithTags &&caption,
					  const std::shared_ptr<SendingAlbum> &album,
	const Api::SendAction &action);

void sendGifOrStickSync(not_null<Main::Session *> session,
						Api::MessageToSend &message,
						not_null<DocumentData *> document);
void waitForMsgSync(not_null<Main::Session *> session, const Api::SendAction &action);
void loadPhotoSync(not_null<Main::Session *> session, const std::pair<not_null<PhotoData *>, FullMsgId> &photos);
void loadDocumentSync(not_null<Main::Session *> session, DocumentData *data, not_null<HistoryItem *> item);
void forwardMessagesSync(not_null<Main::Session *> session, const std::vector<not_null<HistoryItem *> > &items, const ApiWrap::SendAction &action);

}
