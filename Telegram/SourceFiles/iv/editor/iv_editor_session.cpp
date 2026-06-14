/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/editor/iv_editor_session.h"

#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <crl/crl_async.h>
#include <crl/crl_on_main.h>

#include "api/api_sending.h"
#include "api/api_editing.h"
#include "apiwrap.h"
#include "base/weak_ptr.h"
#include "chat_helpers/compose/compose_show.h"
#include "core/application.h"
#include "core/shortcuts.h"
#include "data/data_document.h"
#include "data/data_location.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "iv/iv_cached_media.h"
#include "iv/editor/iv_editor_box.h"
#include "iv/editor/iv_editor_state.h"
#include "iv/editor/iv_editor_widget.h"
#include "iv/iv_instance.h"
#include "iv/iv_rich_message_serializer.h"
#include "lang/lang_keys.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "menu/menu_send.h"
#include "settings/sections/settings_premium.h"
#include "storage/file_upload.h"
#include "storage/localimageloader.h"
#include "storage/storage_account.h"
#include "storage/storage_media_prepare.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/controls/location_picker.h"
#include "ui/rp_widget.h"
#include "ui/widgets/separate_panel.h"
#include "window/window_session_controller.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "styles/style_boxes.h"

namespace Iv::Editor {
namespace {

using PreparedFile = Ui::PreparedFile;
using PreparedFileType = Ui::PreparedFile::Type;
using PreparedList = Ui::PreparedList;

enum class AttachmentState : uchar {
	Uploading,
	Finalizing,
	Ready,
	Failed,
};

struct PreparedDocumentInfo {
	QSize dimensions;
	QString title;
	QString performer;
	QString fileName;
	int duration = 0;
	bool audio = false;
	bool animation = false;
	bool video = false;
};

struct AttachmentMeta {
	PreparedFileType type = PreparedFileType::None;
	RichPage::BlockKind blockKind = RichPage::BlockKind::Unsupported;
	QString caption;
	QString displayName;
	QSize dimensions;
	QString audioTitle;
	QString audioPerformer;
	QString audioFileName;
	int audioDuration = 0;
	bool spoiler = false;
	bool autoplay = false;
	bool loop = false;
};

class PrepareAttachmentTask final : public Task {
public:
	PrepareAttachmentTask(
		FileLoadTask::Args &&args,
		Fn<void(std::shared_ptr<FilePrepareResult>)> done)
	: _task(std::move(args))
	, _done(std::move(done)) {
	}

	void process() override {
		_task.process({ .generateGoodThumbnail = false });
	}

	void finish() override {
		_done(_task.peekResult());
	}

private:
	FileLoadTask _task;
	Fn<void(std::shared_ptr<FilePrepareResult>)> _done;

};

[[nodiscard]] Ui::LocationPickerConfig ResolveMapsConfig(
		not_null<Main::Session*> session) {
	const auto &appConfig = session->appConfig();
	auto map = appConfig.get<base::flat_map<QString, QString>>(
		u"tdesktop_config_map"_q,
		base::flat_map<QString, QString>());
	return {
		.mapsToken = map[u"maps"_q],
		.geoToken = map[u"geo"_q],
	};
}

[[nodiscard]] QString PreparedFileName(const PreparedFile &file) {
	return file.displayName.isEmpty()
		? QFileInfo(file.path).fileName()
		: file.displayName;
}

[[nodiscard]] bool AcceptedPreparedFileType(PreparedFileType type) {
	return (type == PreparedFileType::Photo)
		|| (type == PreparedFileType::Video)
		|| (type == PreparedFileType::Music);
}

[[nodiscard]] bool CanUseRichMessages(not_null<Main::Session*> session) {
	return session->premium();
}

void ShowRichMessagesPremiumToast(
		not_null<Window::SessionController*> controller) {
	Settings::ShowPremiumPromoToast(
		controller->uiShow(),
		tr::lng_article_premium_required(
			tr::now,
			lt_link,
			tr::link(tr::bold(
				tr::lng_article_premium_required_link(tr::now))),
			tr::marked),
		u"rich_message"_q);
}

[[nodiscard]] bool IsRichMessageMediaKind(RichPage::BlockKind kind) {
	switch (kind) {
	case RichPage::BlockKind::Photo:
	case RichPage::BlockKind::Video:
	case RichPage::BlockKind::Audio:
		return true;
	default:
		return false;
	}
}

void CountRichPageMedia(
		const std::vector<RichPage::Block> &blocks,
		int *result) {
	for (const auto &block : blocks) {
		if (IsRichMessageMediaKind(block.kind)) {
			++(*result);
		}
		CountRichPageMedia(block.blocks, result);
		for (const auto &item : block.listItems) {
			CountRichPageMedia(item.blocks, result);
		}
		for (const auto &item : block.mediaItems) {
			if (IsRichMessageMediaKind(item.kind)) {
				++(*result);
			}
		}
	}
}

[[nodiscard]] int CountRichPageMedia(const RichPage &page) {
	auto result = 0;
	CountRichPageMedia(page.blocks, &result);
	return result;
}

template <typename Container>
[[nodiscard]] int CountAcceptedPreparedFiles(const Container &files) {
	auto result = 0;
	for (const auto &file : files) {
		if (AcceptedPreparedFileType(file.type)) {
			++result;
		}
	}
	return result;
}

[[nodiscard]] int CountAcceptedPreparedFiles(const PreparedList &list) {
	return CountAcceptedPreparedFiles(list.files)
		+ CountAcceptedPreparedFiles(list.filesToProcess);
}

[[nodiscard]] RichPage::RichText ToRichText(QString text) {
	auto result = RichPage::RichText();
	result.text.text = std::move(text);
	return result;
}

[[nodiscard]] RichPage::BlockKind BlockKindForPreparedType(
		PreparedFileType type) {
	switch (type) {
	case PreparedFileType::Photo:
		return RichPage::BlockKind::Photo;
	case PreparedFileType::Video:
		return RichPage::BlockKind::Video;
	case PreparedFileType::Music:
		return RichPage::BlockKind::Audio;
	default:
		return RichPage::BlockKind::Unsupported;
	}
}

[[nodiscard]] PreparedDocumentInfo DocumentInfoFromPrepared(
		const MTPDocument &document);

[[nodiscard]] RichPage::BlockKind BlockKindForPreparedResult(
		const FilePrepareResult &prepared) {
	if (prepared.type == SendMediaType::Photo) {
		return RichPage::BlockKind::Photo;
	}
	if (prepared.type != SendMediaType::File) {
		return RichPage::BlockKind::Unsupported;
	}
	const auto info = DocumentInfoFromPrepared(prepared.document);
	if (info.video) {
		return RichPage::BlockKind::Video;
	}
	if (info.audio) {
		return RichPage::BlockKind::Audio;
	}
	return RichPage::BlockKind::Unsupported;
}

[[nodiscard]] QSize PhotoSizeFromPrepared(const MTPPhoto &photo) {
	auto result = QSize();
	photo.match([](const MTPDphotoEmpty &) {
	}, [&](const MTPDphoto &data) {
		const auto assign = [&](const QString &type, int width, int height) {
			if (result.isEmpty() && (type == u"x"_q || type == u"w"_q)) {
				result = QSize(width, height);
			}
			if (type == u"y"_q) {
				result = QSize(width, height);
			}
		};
		for (const auto &size : data.vsizes().v) {
			size.match([](const MTPDphotoSizeEmpty &) {
			}, [&](const MTPDphotoSize &row) {
				assign(qs(row.vtype()), row.vw().v, row.vh().v);
			}, [&](const MTPDphotoCachedSize &row) {
				assign(qs(row.vtype()), row.vw().v, row.vh().v);
			}, [&](const MTPDphotoStrippedSize &) {
			}, [&](const MTPDphotoSizeProgressive &row) {
				assign(qs(row.vtype()), row.vw().v, row.vh().v);
			}, [&](const MTPDphotoPathSize &) {
			});
		}
	});
	return result;
}

[[nodiscard]] PreparedDocumentInfo DocumentInfoFromPrepared(
		const MTPDocument &document) {
	auto result = PreparedDocumentInfo();
	document.match([](const MTPDdocumentEmpty &) {
	}, [&](const MTPDdocument &data) {
		const auto assign = [&](int width, int height, bool force) {
			if (width <= 0 || height <= 0) {
				return;
			}
			if (force || result.dimensions.isEmpty()) {
				result.dimensions = QSize(width, height);
			}
		};
		for (const auto &attribute : data.vattributes().v) {
			attribute.match([&](const MTPDdocumentAttributeAudio &row) {
				result.audio = true;
				result.duration = row.vduration().v;
				result.title = qs(row.vtitle().value_or_empty());
				result.performer = qs(row.vperformer().value_or_empty());
			}, [&](const MTPDdocumentAttributeFilename &row) {
				result.fileName = qs(row.vfile_name());
			}, [&](const MTPDdocumentAttributeImageSize &row) {
				assign(row.vw().v, row.vh().v, false);
			}, [&](const MTPDdocumentAttributeAnimated &) {
				result.animation = true;
			}, [&](const MTPDdocumentAttributeVideo &row) {
				result.video = true;
				assign(row.vw().v, row.vh().v, true);
			}, [&](const auto &) {
			});
		}
	});
	return result;
}

[[nodiscard]] QVector<MTPDocumentAttribute> DocumentAttributesFromPrepared(
		const FilePrepareResult &prepared) {
	auto result = QVector<MTPDocumentAttribute>();
	prepared.document.match([&](const MTPDdocument &data) {
		result = data.vattributes().v;
	}, [](const auto &) {
	});
	return result;
}

[[nodiscard]] QVector<MTPInputDocument> ToInputDocumentVector(
		const std::vector<MTPInputDocument> &stickers) {
	auto result = QVector<MTPInputDocument>();
	result.reserve(int(stickers.size()));
	for (const auto &sticker : stickers) {
		result.push_back(sticker);
	}
	return result;
}

[[nodiscard]] AttachmentMeta BuildAttachmentMeta(const PreparedFile &file) {
	auto result = AttachmentMeta{
		.type = file.type,
		.blockKind = BlockKindForPreparedType(file.type),
		.caption = file.caption.text,
		.displayName = PreparedFileName(file),
		.dimensions = !file.shownDimensions.isEmpty()
			? file.shownDimensions
			: file.originalDimensions,
		.spoiler = file.spoiler,
	};
	if (!file.information) {
		result.audioFileName = result.displayName;
		return result;
	}
	if (const auto song = std::get_if<Ui::PreparedFileInformation::Song>(
			&file.information->media)) {
		result.audioTitle = song->title;
		result.audioPerformer = song->performer;
		result.audioDuration = int(song->duration / 1000);
		result.audioFileName = result.displayName;
	} else if (const auto video = std::get_if<Ui::PreparedFileInformation::Video>(
			&file.information->media)) {
		result.autoplay = video->isGifv;
		result.loop = video->isGifv;
	}
	return result;
}

[[nodiscard]] std::unique_ptr<FileLoadTask> BuildVideoCoverTask(
		not_null<Main::Session*> session,
		PeerId peer,
		std::unique_ptr<PreparedFile> file) {
	if (!file) {
		return nullptr;
	}
	return std::make_unique<FileLoadTask>(FileLoadTask::Args{
		.session = session,
		.filepath = file->path,
		.content = std::move(file->content),
		.information = std::move(file->information),
		.videoCover = nullptr,
		.type = SendMediaType::Photo,
		.to = FileLoadTo(
			peer,
			Api::SendOptions(),
			FullReplyTo(),
			MsgId()),
		.caption = TextWithTags(),
		.spoiler = false,
		.album = nullptr,
		.forceFile = false,
		.sendLargePhotos = false,
		.idOverride = 0,
		.displayName = file->displayName,
	});
}

[[nodiscard]] FileLoadTask::Args BuildPrepareTaskArgs(
		not_null<Main::Session*> session,
		PeerId peer,
		PreparedFile file) {
	const auto sendType = (file.type == PreparedFileType::Photo)
		? SendMediaType::Photo
		: SendMediaType::File;
	return {
		.session = session,
		.filepath = file.path,
		.content = std::move(file.content),
		.information = std::move(file.information),
		.videoCover = BuildVideoCoverTask(
			session,
			peer,
			std::move(file.videoCover)),
		.type = sendType,
		.to = FileLoadTo(
			peer,
			Api::SendOptions(),
			FullReplyTo(),
			MsgId()),
		.caption = TextWithTags(),
		.spoiler = file.spoiler,
		.album = nullptr,
		.forceFile = false,
		.sendLargePhotos = file.sendLargePhotos,
		.idOverride = 0,
		.displayName = file.displayName,
	};
}

class ArticleSession final
	: public std::enable_shared_from_this<ArticleSession>
	, public base::has_weak_ptr {
public:
	static void ShowCompose(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		Api::SendAction action,
		Fn<SendMenu::Details()> sendMenuDetails) {
		auto session = std::shared_ptr<ArticleSession>(new ArticleSession(
			controller,
			peer,
			Mode::Compose,
			FullMsgId(peer->id, controller->session().data().nextLocalMessageId()),
			std::make_shared<RichPage>(),
			std::move(action),
			std::move(sendMenuDetails),
			std::nullopt));
		session->showWindow();
	}

	static void ShowEdit(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item,
		std::shared_ptr<const RichPage> richPage) {
		if (!richPage || !CanEditRichPage(richPage)) {
			controller->showToast(tr::lng_edit_error(tr::now));
			return;
		}
		auto session = std::shared_ptr<ArticleSession>(new ArticleSession(
			controller,
			item->history()->peer,
			Mode::Edit,
			item->fullId(),
			std::make_shared<RichPage>(*richPage),
			std::nullopt,
			nullptr,
			EditedItemSnapshot{
				.item = item,
				.inlinePage = item->richPage(),
				.summary = item->originalText(),
				.fullPage = item->fullRichPage(),
			}));
		session->showWindow();
	}

	~ArticleSession() {
		_submitDeferred = false;
		for (const auto &attachment : _attachments) {
			_session->uploader().cancel(attachment.uploadId);
		}
	}

private:
	enum class Mode {
		Compose,
		Edit,
	};

	struct AttachmentRecord {
		FullMsgId uploadId;
		PreparedFileType type = PreparedFileType::None;
		RichPage::BlockKind blockKind = RichPage::BlockKind::Unsupported;
		uint64 localMediaId = 0;
		AttachmentState state = AttachmentState::Uploading;
		float64 progress = 0.;
		QString caption;
		QString filename;
		QString filemime;
		QVector<MTPDocumentAttribute> attributes;
		bool forceFile = false;
		QString audioTitle;
		QString audioPerformer;
		QString audioFileName;
		int audioDuration = 0;
		QSize dimensions;
		bool spoiler = false;
		bool autoplay = false;
		bool loop = false;
		std::vector<State::BlockPath> blockLocators;
		MTPInputPhoto inputPhoto;
		MTPInputDocument inputDocument;
		uint64 serverMediaId = 0;
		uint64 accessHash = 0;
		QByteArray fileReference;
		PhotoData *serverPhoto = nullptr;
		DocumentData *serverDocument = nullptr;
	};

	struct QueuedPrepare {
		QPointer<Widget> editor;
		PreparedFile file;
		uint64 batchId = 0;
	};

	struct EditedItemSnapshot {
		not_null<HistoryItem*> item;
		std::shared_ptr<const RichPage> inlinePage;
		TextWithEntities summary;
		std::shared_ptr<const RichPage> fullPage;
	};

	ArticleSession(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		Mode mode,
		FullMsgId articleId,
		std::shared_ptr<RichPage> page,
		std::optional<Api::SendAction> action,
		Fn<SendMenu::Details()> sendMenuDetails,
		std::optional<EditedItemSnapshot> edited)
	: _controller(controller)
	, _session(&controller->session())
	, _show(controller->uiShow())
	, _peer(peer)
	, _mode(mode)
	, _articleId(articleId)
	, _composeAction(std::move(action))
	, _sendMenuDetails(std::move(sendMenuDetails))
	, _edited(std::move(edited))
	, _page(page ? std::move(page) : std::make_shared<RichPage>())
	, _runtime(CreateMessageMediaRuntime(
		_session,
		_articleId,
		[](QString) {
		},
		[](QString) {
		}))
	, _showLimitToast([controller](RichMessageLimitError error) {
		switch (error) {
		case RichMessageLimitError::Length:
			controller->showToast(tr::lng_article_limit_length(tr::now));
			return;
		case RichMessageLimitError::Blocks:
			controller->showToast(tr::lng_article_limit_blocks(tr::now));
			return;
		case RichMessageLimitError::Depth:
			controller->showToast(tr::lng_article_limit_depth(tr::now));
			return;
		case RichMessageLimitError::Media:
			controller->showToast(tr::lng_article_limit_media(tr::now));
			return;
		case RichMessageLimitError::TableColumns:
			controller->showToast(tr::lng_article_limit_columns(tr::now));
			return;
		}
		controller->showToast(tr::lng_edit_error(tr::now));
	})
	, _limits(ResolveRichMessageLimits(_session))
	, _state(std::make_shared<State>(_page, _runtime, _limits))
	, _submitOptions(_composeAction ? _composeAction->options : Api::SendOptions()) {
		subscribeToUploader();
	}

	[[nodiscard]] bool submitRequested() {
		if (_submittedPage || _submitApiRequested) {
			return false;
		}
		if (!CanUseRichMessages(_session)) {
			ShowRichMessagesPremiumToast(_controller);
			return false;
		}
		if (hasPendingPreparation()) {
			_submitDeferred = true;
			return false;
		}
		if (hasVisibleFailedAttachments()) {
			showAttachmentFailedToast();
			return false;
		}
		auto page = std::shared_ptr<const RichPage>(
			std::make_shared<RichPage>(_state->richPage()));
		if (const auto error = ValidateRichMessage(*page, _limits)) {
			showRichMessageLimitToast(*error);
			return false;
		}
		if (!applySubmittedLocalState(page)) {
			_controller->showToast(tr::lng_edit_error(tr::now));
			return false;
		}
		_submitDeferred = false;
		_submittedPage = std::move(page);
		_backgroundHold = shared_from_this();
		maybeContinueSubmittedRequest();
		return true;
	}

	[[nodiscard]] bool cancelRequested() {
		_submitDeferred = false;
		return true;
	}

	[[nodiscard]] HistoryItem *currentSubmittedItem() const {
		return _session->data().message(_articleId);
	}

	[[nodiscard]] HistoryItem *ensureComposeLocalItem() {
		if (const auto item = currentSubmittedItem()) {
			return item;
		}
		if (!_composeAction) {
			return nullptr;
		}
		auto action = *_composeAction;
		const auto history = action.history;
		const auto peer = history->peer;
		auto flags = NewMessageFlags(peer);
		if (action.replyTo) {
			flags |= MessageFlag::HasReplyInfo;
		}
		Api::FillMessagePostFlags(action, peer, flags);
		if (action.options.scheduled) {
			flags |= MessageFlag::IsOrWasScheduled;
		}
		if (action.options.shortcutId) {
			flags |= MessageFlag::ShortcutMessage;
		}
		const auto starsPaid = std::min(
			peer->starsPerMessageChecked(),
			action.options.starsApproved);
		return history->addNewLocalMessage({
			.id = _articleId.msg,
			.flags = flags,
			.from = NewMessageFromId(action),
			.replyTo = action.replyTo,
			.date = NewMessageDate(action.options),
			.scheduleRepeatPeriod = action.options.scheduleRepeatPeriod,
			.shortcutId = action.options.shortcutId,
			.starsPaid = starsPaid,
			.postAuthor = NewMessagePostAuthor(action),
			.effectId = action.options.effectId,
			.suggest = HistoryMessageSuggestInfo(action.options),
		}, TextWithEntities(), MTP_messageMediaEmpty());
	}

	[[nodiscard]] bool keepsInlineRichPage() const {
		return (_mode == Mode::Edit)
			&& _edited
			&& _edited->inlinePage
			&& _edited->inlinePage->part;
	}

	[[nodiscard]] bool applySubmittedLocalState(
			const std::shared_ptr<const RichPage> &page) {
		const auto item = (_mode == Mode::Compose)
			? ensureComposeLocalItem()
			: currentSubmittedItem();
		if (!item) {
			return false;
		}
		if (keepsInlineRichPage()) {
			item->setFullRichPage(page);
			return true;
		}
		item->applyLocalRichPage(page);
		return true;
	}

	void restoreEditedItem() {
		if (!_edited) {
			return;
		}
		if (const auto item = currentSubmittedItem()) {
			if (keepsInlineRichPage()) {
				if (_edited->fullPage) {
					item->setFullRichPage(_edited->fullPage);
				} else {
					item->clearFullRichPage();
				}
				return;
			}
			item->applyLocalRichPage(_edited->inlinePage, _edited->summary);
			if (_edited->fullPage) {
				item->setFullRichPage(_edited->fullPage);
			}
		}
	}

	void finishSubmittedWork() {
		_submitApiRequested = false;
		_submittedPage = nullptr;
		_backgroundHold = nullptr;
	}

	void failSubmittedWork(bool showToast) {
		if (showToast) {
			showAttachmentFailedToast();
		}
		if (_mode == Mode::Edit) {
			restoreEditedItem();
		} else if (const auto item = currentSubmittedItem()) {
			item->sendFailed();
		}
		finishSubmittedWork();
	}

	[[nodiscard]] bool pageContainsAttachment(
			const std::vector<RichPage::Block> &blocks,
			const AttachmentRecord &attachment) const {
		for (const auto &block : blocks) {
			if (blockMatchesAttachment(block, attachment)
				|| pageContainsAttachment(block.blocks, attachment)) {
				return true;
			}
			for (const auto &item : block.listItems) {
				if (pageContainsAttachment(item.blocks, attachment)) {
					return true;
				}
			}
		}
		return false;
	}

	[[nodiscard]] bool submittedPageContainsAttachment(
			const AttachmentRecord &attachment) const {
		return _submittedPage
			&& pageContainsAttachment(_submittedPage->blocks, attachment);
	}

	[[nodiscard]] bool hasFailedSubmittedAttachments() const {
		for (const auto &attachment : _attachments) {
			if (attachment.state == AttachmentState::Failed
				&& submittedPageContainsAttachment(attachment)) {
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] bool submittedAttachmentsReady() const {
		for (const auto &attachment : _attachments) {
			if (submittedPageContainsAttachment(attachment)
				&& attachment.state != AttachmentState::Ready) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] const AttachmentRecord *attachmentForBlock(
			const RichPage::Block &block) const {
		for (const auto &attachment : _attachments) {
			if (blockMatchesAttachment(block, attachment)) {
				return &attachment;
			}
		}
		return nullptr;
	}

	[[nodiscard]] bool patchReadyAttachmentBlock(
			RichPage::Block &block,
			const AttachmentRecord &attachment) const {
		if (attachment.state != AttachmentState::Ready) {
			return false;
		}
		if (attachment.blockKind == RichPage::BlockKind::Photo) {
			if (!attachment.serverPhoto || !attachment.serverMediaId) {
				return false;
			}
			block.photoId = attachment.serverMediaId;
			block.photo = attachment.serverPhoto;
		} else {
			if (!attachment.serverDocument || !attachment.serverMediaId) {
				return false;
			}
			block.documentId = attachment.serverMediaId;
			block.document = attachment.serverDocument;
		}
		return true;
	}

	[[nodiscard]] bool patchSubmittedBlocks(
			std::vector<RichPage::Block> &blocks) const {
		for (auto &block : blocks) {
			if (const auto attachment = attachmentForBlock(block)) {
				if (!patchReadyAttachmentBlock(block, *attachment)) {
					return false;
				}
			}
			if (!patchSubmittedBlocks(block.blocks)) {
				return false;
			}
			for (auto &item : block.listItems) {
				if (!patchSubmittedBlocks(item.blocks)) {
					return false;
				}
			}
		}
		return true;
	}

	[[nodiscard]] std::optional<MTPInputRichMessage> serializeSubmittedPage() const {
		if (!_submittedPage) {
			return std::nullopt;
		}
		auto page = RichPage(*_submittedPage);
		return patchSubmittedBlocks(page.blocks)
			? SerializeInputRichMessage(_session, page)
			: std::optional<MTPInputRichMessage>();
	}

	void maybeContinueSubmittedRequest() {
		if (!_submittedPage || _submitApiRequested) {
			return;
		}
		if (hasFailedSubmittedAttachments()) {
			failSubmittedWork(false);
			return;
		}
		if (!submittedAttachmentsReady()) {
			return;
		}
		const auto richMessage = serializeSubmittedPage();
		if (!richMessage) {
			failSubmittedWork(true);
			return;
		}
		const auto item = currentSubmittedItem();
		if (!item) {
			finishSubmittedWork();
			return;
		}
		_submitApiRequested = true;
		if (_mode == Mode::Compose) {
			auto action = *_composeAction;
			action.options = _submitOptions;
			_session->api().sendRichMessage(item, *richMessage, std::move(action));
			finishSubmittedWork();
			return;
		}
		Api::EditRichMessage(
			not_null{ item },
			[weak = base::make_weak(this)] {
				if (const auto session = weak.get()) {
					return session->serializeSubmittedPage();
				}
				return std::optional<MTPInputRichMessage>();
			},
			_submitOptions,
			[weak = base::make_weak(this)](mtpRequestId) {
				if (const auto session = weak.get()) {
					session->finishSubmittedWork();
				}
			},
			[weak = base::make_weak(this)](const QString &error, mtpRequestId) {
				if (const auto session = weak.get()) {
					session->restoreEditedItem();
					session->_controller->showToast(error.isEmpty()
						? tr::lng_edit_error(tr::now)
						: error);
					session->finishSubmittedWork();
				}
			});
	}

	void requestSubmit(Api::SendOptions options) {
		_submitOptions = std::move(options);
		if (_composeAction) {
			_composeAction->options = _submitOptions;
		}
		if (hasPendingPreparation()) {
			_submitDeferred = true;
			return;
		}
		if (hasVisibleFailedAttachments()) {
			showAttachmentFailedToast();
			return;
		}
		simulateSubmitClick();
	}

	void setupSubmitButton(not_null<Ui::RpWidget*> button) {
		_submitButton = button;
		if (_mode != Mode::Compose || !_sendMenuDetails) {
			return;
		}
		const auto weak = base::make_weak(this);
		const auto submit = [weak](Api::SendOptions options) {
			if (const auto session = weak.get()) {
				session->requestSubmit(std::move(options));
			}
		};
		SendMenu::SetupMenuAndShortcuts(
			button,
			_controller->uiShow(),
			[weak] {
				if (const auto session = weak.get()) {
					return session->_sendMenuDetails
						? session->_sendMenuDetails()
						: SendMenu::Details();
				}
				return SendMenu::Details();
			},
			SendMenu::DefaultCallback(_controller->uiShow(), submit));
	}

	void requestMedia(not_null<Widget*> editor, QPointer<QWidget> parent) {
		if (!parent) {
			return;
		}
		_editor = editor;
		const auto weak = base::make_weak(this);
		const auto editorPointer = QPointer<Widget>(editor.get());
		FileDialog::GetOpenPath(
			std::move(parent),
			tr::lng_choose_file(tr::now),
			FileDialog::PhotoVideoAudioFilesFilter(),
			[weak, editorPointer](FileDialog::OpenResult &&result) mutable {
				if (const auto session = weak.get()) {
					session->handleMediaDialogResult(
						editorPointer,
						std::move(result));
				}
			});
	}

	void requestMap(
			not_null<Widget*> editor,
			QPointer<QWidget> parent,
			rpl::producer<> closeRequests) {
		if (!parent) {
			return;
		}
		_editor = editor;
		const auto config = ResolveMapsConfig(_session);
		if (!Ui::LocationPicker::Available(config)) {
			return;
		}
		const auto weak = base::make_weak(this);
		const auto editorPointer = QPointer<Widget>(editor.get());
		Ui::LocationPicker::Show({
			.parent = static_cast<Ui::RpWidget*>(parent.data()),
			.config = config,
			.chooseLabel = tr::lng_maps_point_send(),
			.session = _session,
			.callback = [weak, editorPointer](::Data::InputVenue venue) {
				if (const auto session = weak.get()) {
					session->applyMapSelection(editorPointer, std::move(venue));
				}
			},
			.quit = [] { Shortcuts::Launch(Shortcuts::Command::Quit); },
			.storageId = _session->local().resolveStorageIdBots(),
			.closeRequests = std::move(closeRequests),
		});
	}

	void showWindow() {
		_backgroundHold = shared_from_this();
		auto descriptor = ShowWindowDescriptor{
			.session = _session,
			.show = _show,
			.peer = _peer,
			.state = _state,
			.submitType = (_mode == Mode::Compose)
				? ShowWindowDescriptor::SubmitType::Send
				: ShowWindowDescriptor::SubmitType::Save,
			.customEmojiPaused = [show = _show] {
				return show->paused(ChatHelpers::PauseReason::Layer);
			},
			.cancelled = [session = shared_from_this()] {
				return session->cancelRequested();
			},
			.confirmed = [session = shared_from_this()] {
				return session->submitRequested();
			},
			.setupSubmitButton = [session = shared_from_this()](
					not_null<Ui::RpWidget*> button) {
				session->setupSubmitButton(button);
			},
			.requestMedia = [session = shared_from_this()](
					not_null<Widget*> editor,
					QPointer<QWidget> parent) {
				session->requestMedia(editor, std::move(parent));
			},
			.requestMap = [session = shared_from_this()](
					not_null<Widget*> editor,
					QPointer<QWidget> parent,
					rpl::producer<> closeRequests) {
				session->requestMap(
					editor,
					std::move(parent),
					std::move(closeRequests));
			},
			.closed = [session = shared_from_this()] {
				session->windowClosed();
			},
			.showLimitToast = _showLimitToast,
		};
		_windowHost = ShowWindow(std::move(descriptor));
	}

	void windowClosed() {
		_editor = nullptr;
		_submitButton = nullptr;
		_windowHost = nullptr;
		if (!_submittedPage && !_submitApiRequested) {
			_backgroundHold = nullptr;
		}
	}

	void handleMediaDialogResult(
		QPointer<Widget> editor,
		FileDialog::OpenResult &&result) {
		auto showError = [=](tr::phrase<> phrase) {
			_controller->showToast(phrase(tr::now));
		};
		auto list = Storage::PreparedFileFromFilesDialog(
			std::move(result),
			[](const PreparedList &) {
				return true;
			},
			showError,
			st::sendMediaPreviewSize,
			_session->premium());
		if (!list) {
			return;
		}
		applyPreparedList(editor, std::move(*list), ++_prepareBatchId);
	}

	void applyPreparedList(
		QPointer<Widget> editor,
		PreparedList list,
		uint64 batchId) {
		if (const auto accepted = CountAcceptedPreparedFiles(list);
			accepted && exceedsMediaLimitWith(accepted)) {
			showRichMessageLimitToast(RichMessageLimitError::Media);
			return;
		}
		for (auto &file : list.files) {
			applyPreparedFile(editor, std::move(file), batchId);
		}
		for (auto &file : list.filesToProcess) {
			_prepareQueue.push_back({
				.editor = editor,
				.file = std::move(file),
				.batchId = batchId,
			});
		}
		enqueueNextPrepare();
	}

	void enqueueNextPrepare() {
		if (_preparing) {
			return;
		}
		while (!_prepareQueue.empty()
			&& _prepareQueue.front().file.information) {
			auto queued = std::move(_prepareQueue.front());
			_prepareQueue.pop_front();
			applyPreparedFile(
				queued.editor,
				std::move(queued.file),
				queued.batchId);
		}
		if (_prepareQueue.empty()) {
			maybeContinueDeferredSubmit();
			return;
		}
		auto queued = std::move(_prepareQueue.front());
		_prepareQueue.pop_front();
		const auto weak = base::make_weak(this);
		_preparing = true;
		_preparingFileType = queued.file.type;
		const auto sideLimit = PhotoSideLimit();
		crl::async([weak, queued = std::move(queued), sideLimit]() mutable {
			Storage::PrepareDetails(
				queued.file,
				st::sendMediaPreviewSize,
				sideLimit);
			crl::on_main([weak, queued = std::move(queued)]() mutable {
				if (const auto session = weak.get()) {
					session->preparedAsyncFile(std::move(queued));
				}
			});
		});
	}

	void preparedAsyncFile(QueuedPrepare queued) {
		_preparing = false;
		_preparingFileType = PreparedFileType::None;
		applyPreparedFile(
			queued.editor,
			std::move(queued.file),
			queued.batchId);
		enqueueNextPrepare();
	}

	void applyPreparedFile(
		QPointer<Widget> editor,
		PreparedFile file,
		uint64 batchId) {
		if (!AcceptedPreparedFileType(file.type)) {
			showUnsupportedMediaToast(batchId);
			return;
		}
		if (exceedsMediaLimitWith(1)) {
			showRichMessageLimitToast(RichMessageLimitError::Media);
			return;
		}
		prepareAttachment(editor, std::move(file), batchId);
	}

	void prepareAttachment(
		QPointer<Widget> editor,
		PreparedFile file,
		uint64 batchId) {
		const auto meta = BuildAttachmentMeta(file);
		const auto weak = base::make_weak(this);
		++_pendingAttachmentPrepareCount;
		_attachmentPrepareQueue.addTask(
			std::make_unique<PrepareAttachmentTask>(
				BuildPrepareTaskArgs(_session, _peer->id, std::move(file)),
				[weak, editor, meta, batchId](
						std::shared_ptr<FilePrepareResult> prepared) mutable {
					if (const auto session = weak.get()) {
						session->attachmentPrepared(
							editor,
							std::move(meta),
							std::move(prepared),
							batchId);
					}
				}));
	}

	void attachmentPrepared(
		QPointer<Widget> editor,
		AttachmentMeta meta,
		std::shared_ptr<FilePrepareResult> prepared,
		uint64 batchId) {
		_pendingAttachmentPrepareCount = std::max(
			_pendingAttachmentPrepareCount - 1,
			0);
		if (!prepared) {
			showAttachmentFailedToast();
			maybeContinueDeferredSubmit();
			return;
		}
		if (!editor) {
			maybeContinueDeferredSubmit();
			return;
		}
		if (meta.blockKind != BlockKindForPreparedResult(*prepared)) {
			showUnsupportedMediaToast(batchId);
			maybeContinueDeferredSubmit();
			return;
		}
		startAttachmentUpload(editor, std::move(meta), std::move(prepared));
		maybeContinueDeferredSubmit();
	}

	void startAttachmentUpload(
		QPointer<Widget> editor,
		AttachmentMeta meta,
		std::shared_ptr<FilePrepareResult> prepared) {
		if (!editor || !prepared) {
			return;
		}
		if (exceedsMediaLimitWith(1)) {
			showRichMessageLimitToast(RichMessageLimitError::Media);
			return;
		}
		_editor = editor;
		const auto uploadId = FullMsgId(
			_peer->id,
			_session->data().nextLocalMessageId());
		auto record = AttachmentRecord{
			.uploadId = uploadId,
			.type = meta.type,
			.blockKind = meta.blockKind,
			.localMediaId = prepared->id,
			.state = AttachmentState::Uploading,
			.caption = meta.caption,
			.filename = prepared->filename,
			.filemime = prepared->filemime,
			.attributes = DocumentAttributesFromPrepared(*prepared),
			.forceFile = prepared->forceFile,
			.audioTitle = meta.audioTitle,
			.audioPerformer = meta.audioPerformer,
			.audioFileName = meta.audioFileName.isEmpty()
				? meta.displayName
				: meta.audioFileName,
			.audioDuration = meta.audioDuration,
			.dimensions = meta.dimensions,
			.spoiler = meta.spoiler,
			.autoplay = meta.autoplay,
			.loop = meta.loop,
		};
		if (record.blockKind == RichPage::BlockKind::Photo) {
			const auto size = PhotoSizeFromPrepared(prepared->photo);
			if (!size.isEmpty()) {
				record.dimensions = size;
			}
		} else {
			const auto info = DocumentInfoFromPrepared(prepared->document);
			if (!info.dimensions.isEmpty()) {
				record.dimensions = info.dimensions;
			}
			if (record.blockKind == RichPage::BlockKind::Audio) {
				if (record.audioTitle.isEmpty()) {
					record.audioTitle = info.title;
				}
				if (record.audioPerformer.isEmpty()) {
					record.audioPerformer = info.performer;
				}
				if (record.audioFileName.isEmpty()) {
					record.audioFileName = !info.fileName.isEmpty()
						? info.fileName
						: prepared->filename;
				}
				if (!record.audioDuration) {
					record.audioDuration = info.duration;
				}
			} else {
				record.autoplay = record.autoplay || info.animation;
				record.loop = record.loop || info.animation;
			}
		}

		_attachments.push_back(std::move(record));
		auto &stored = _attachments.back();
		editor->insertPreparedBlock(makeAttachmentBlock(stored));
		refreshAttachmentLocators(_state->richPage(), stored);
		if (stored.blockLocators.empty()) {
			_attachments.pop_back();
			return;
		}
		_session->uploader().upload(uploadId, prepared);
		updateAttachmentProgress(stored);
		requestEditorUpdate();
	}

	void applyMapSelection(
		QPointer<Widget> editor,
		::Data::InputVenue venue) {
		if (!editor) {
			return;
		}
		_editor = editor;
		editor->insertPreparedBlock(makeMapBlock(std::move(venue)));
	}

	[[nodiscard]] RichPage::Block makeAttachmentBlock(
			const AttachmentRecord &attachment) const {
		auto block = RichPage::Block();
		block.kind = attachment.blockKind;
		block.caption = ToRichText(attachment.caption);
		if (attachment.blockKind == RichPage::BlockKind::Photo) {
			block.photoId = attachment.localMediaId;
			block.width = attachment.dimensions.width();
			block.height = attachment.dimensions.height();
			block.spoiler = attachment.spoiler;
		} else if (attachment.blockKind == RichPage::BlockKind::Video) {
			block.documentId = attachment.localMediaId;
			block.width = attachment.dimensions.width();
			block.height = attachment.dimensions.height();
			block.spoiler = attachment.spoiler;
			block.autoplay = attachment.autoplay;
			block.loop = attachment.loop;
		} else if (attachment.blockKind == RichPage::BlockKind::Audio) {
			block.documentId = attachment.localMediaId;
			block.audioTitle = attachment.audioTitle;
			block.audioPerformer = attachment.audioPerformer;
			block.audioFileName = attachment.audioFileName;
			block.audioDuration = attachment.audioDuration;
		}
		return block;
	}

	[[nodiscard]] RichPage::Block makeMapBlock(::Data::InputVenue venue) const {
		const auto point = ::Data::LocationPoint(
			venue.lat,
			venue.lon,
			::Data::LocationPoint::NoAccessHash);
		const auto preview = ::Data::ComputeLocation(point);
		auto caption = QString();
		if (!venue.title.isEmpty() && !venue.address.isEmpty()) {
			caption = venue.title + u"\n"_q + venue.address;
		} else {
			caption = !venue.title.isEmpty() ? venue.title : venue.address;
		}
		auto block = RichPage::Block();
		block.kind = RichPage::BlockKind::Map;
		block.latitude = venue.lat;
		block.longitude = venue.lon;
		block.accessHash = point.accessHash();
		block.width = preview.width;
		block.height = preview.height;
		block.zoom = preview.zoom;
		block.caption = ToRichText(std::move(caption));
		return block;
	}

	void subscribeToUploader() {
		_session->uploader().photoReady(
		) | rpl::on_next([=](const Storage::UploadedMedia &data) {
			if (const auto attachment = findAttachment(data.fullId)) {
				finalizeUploadedPhoto(*attachment, data);
			}
		}, _lifetime);
		_session->uploader().documentReady(
		) | rpl::on_next([=](const Storage::UploadedMedia &data) {
			if (const auto attachment = findAttachment(data.fullId)) {
				finalizeUploadedDocument(*attachment, data);
			}
		}, _lifetime);
		_session->uploader().photoProgress(
		) | rpl::on_next([=](const FullMsgId &id) {
			if (const auto attachment = findAttachment(id)) {
				updateAttachmentProgress(*attachment);
			}
		}, _lifetime);
		_session->uploader().documentProgress(
		) | rpl::on_next([=](const FullMsgId &id) {
			if (const auto attachment = findAttachment(id)) {
				updateAttachmentProgress(*attachment);
			}
		}, _lifetime);
		_session->uploader().photoFailed(
		) | rpl::on_next([=](const FullMsgId &id) {
			markAttachmentFailed(id);
		}, _lifetime);
		_session->uploader().documentFailed(
		) | rpl::on_next([=](const FullMsgId &id) {
			markAttachmentFailed(id);
		}, _lifetime);
	}

	void finalizeUploadedPhoto(
		AttachmentRecord &attachment,
		const Storage::UploadedMedia &data) {
		using Flag = MTPDinputMediaUploadedPhoto::Flag;
		attachment.state = AttachmentState::Finalizing;
		auto flags = MTPDinputMediaUploadedPhoto::Flags();
		const auto stickers = ToInputDocumentVector(data.info.attachedStickers);
		if (attachment.spoiler) {
			flags |= Flag::f_spoiler;
		}
		if (!stickers.isEmpty()) {
			flags |= Flag::f_stickers;
		}
		_session->api().request(MTPmessages_UploadMedia(
			MTP_flags(0),
			MTPstring(),
			_peer->input(),
			MTP_inputMediaUploadedPhoto(
				MTP_flags(flags),
				data.info.file,
				MTP_vector<MTPInputDocument>(std::move(stickers)),
				MTP_int(0),
				MTPInputDocument())
		)).done([weak = base::make_weak(this), uploadId = attachment.uploadId](
				const MTPMessageMedia &result) {
			if (const auto session = weak.get()) {
				session->applyUploadedPhotoResult(uploadId, result);
			}
		}).fail([weak = base::make_weak(this), uploadId = attachment.uploadId](
				const MTP::Error &) {
			if (const auto session = weak.get()) {
				session->markAttachmentFailed(uploadId);
			}
		}).send();
	}

	void finalizeUploadedDocument(
		AttachmentRecord &attachment,
		const Storage::UploadedMedia &data) {
		using Flag = MTPDinputMediaUploadedDocument::Flag;
		attachment.state = AttachmentState::Finalizing;
		auto flags = MTPDinputMediaUploadedDocument::Flags();
		if (attachment.forceFile) {
			flags |= Flag::f_force_file;
		}
		const auto stickers = ToInputDocumentVector(data.info.attachedStickers);
		if (data.info.thumb) {
			flags |= Flag::f_thumb;
		}
		if (attachment.spoiler) {
			flags |= Flag::f_spoiler;
		}
		if (!stickers.isEmpty()) {
			flags |= Flag::f_stickers;
		}
		if (data.info.videoCover) {
			flags |= Flag::f_video_cover;
		}
		auto attributes = !attachment.attributes.isEmpty()
			? attachment.attributes
			: QVector<MTPDocumentAttribute>(
				1,
				MTP_documentAttributeFilename(MTP_string(attachment.filename)));
		_session->api().request(MTPmessages_UploadMedia(
			MTP_flags(0),
			MTPstring(),
			_peer->input(),
			MTP_inputMediaUploadedDocument(
				MTP_flags(flags),
				data.info.file,
				data.info.thumb.value_or(MTPInputFile()),
				MTP_string(attachment.filemime),
				MTP_vector<MTPDocumentAttribute>(std::move(attributes)),
				MTP_vector<MTPInputDocument>(std::move(stickers)),
				data.info.videoCover.value_or(MTPInputPhoto()),
				MTP_int(0),
				MTP_int(0))
		)).done([weak = base::make_weak(this), uploadId = attachment.uploadId](
				const MTPMessageMedia &result) {
			if (const auto session = weak.get()) {
				session->applyUploadedDocumentResult(uploadId, result);
			}
		}).fail([weak = base::make_weak(this), uploadId = attachment.uploadId](
				const MTP::Error &) {
			if (const auto session = weak.get()) {
				session->markAttachmentFailed(uploadId);
			}
		}).send();
	}

	void applyUploadedPhotoResult(
		FullMsgId uploadId,
		const MTPMessageMedia &result) {
		const auto attachment = findAttachment(uploadId);
		if (!attachment) {
			return;
		}
		auto ok = false;
		result.match([&](const MTPDmessageMediaPhoto &media) {
			const auto photo = media.vphoto();
			if (!photo || photo->type() != mtpc_photo) {
				return;
			}
			const auto &fields = photo->c_photo();
			attachment->state = AttachmentState::Ready;
			attachment->serverMediaId = fields.vid().v;
			attachment->accessHash = fields.vaccess_hash().v;
			attachment->fileReference = fields.vfile_reference().v;
			attachment->serverPhoto = _session->data().processPhoto(*photo);
			attachment->inputPhoto = MTP_inputPhoto(
				fields.vid(),
				fields.vaccess_hash(),
				fields.vfile_reference());
			ok = true;
		}, [&](const auto &) {
		});
		if (!ok) {
			markAttachmentFailed(uploadId);
			return;
		}
		attachment->progress = 1.;
		if (_editor) {
			auto patched = true;
			_editor->applyExternalRichPageMutation([&](RichPage &page) {
				const auto result = patchVisibleAttachmentBlocks(
					page,
					*attachment);
				patched = patched && result;
				return result;
			});
			if (!patched) {
				requestEditorUpdate();
			}
		} else if (!patchVisibleAttachmentBlocks(
			*visibleRichPage(),
			*attachment)) {
			requestEditorUpdate();
		} else {
			_state->resyncAfterExternalRichPageMutation();
			requestEditorUpdate();
		}
		maybeContinueSubmittedRequest();
	}

	void applyUploadedDocumentResult(
		FullMsgId uploadId,
		const MTPMessageMedia &result) {
		const auto attachment = findAttachment(uploadId);
		if (!attachment) {
			return;
		}
		auto ok = false;
		result.match([&](const MTPDmessageMediaDocument &media) {
			const auto document = media.vdocument();
			if (!document || document->type() != mtpc_document) {
				return;
			}
			const auto &fields = document->c_document();
			attachment->state = AttachmentState::Ready;
			attachment->serverMediaId = fields.vid().v;
			attachment->accessHash = fields.vaccess_hash().v;
			attachment->fileReference = fields.vfile_reference().v;
			attachment->serverDocument = _session->data().processDocument(*document);
			attachment->inputDocument = MTP_inputDocument(
				fields.vid(),
				fields.vaccess_hash(),
				fields.vfile_reference());
			ok = true;
		}, [&](const auto &) {
		});
		if (!ok) {
			markAttachmentFailed(uploadId);
			return;
		}
		attachment->progress = 1.;
		if (_editor) {
			auto patched = true;
			_editor->applyExternalRichPageMutation([&](RichPage &page) {
				const auto result = patchVisibleAttachmentBlocks(
					page,
					*attachment);
				patched = patched && result;
				return result;
			});
			if (!patched) {
				requestEditorUpdate();
			}
		} else if (!patchVisibleAttachmentBlocks(
			*visibleRichPage(),
			*attachment)) {
			requestEditorUpdate();
		} else {
			_state->resyncAfterExternalRichPageMutation();
			requestEditorUpdate();
		}
		maybeContinueSubmittedRequest();
	}

	void markAttachmentFailed(FullMsgId uploadId) {
		if (const auto attachment = findAttachment(uploadId)) {
			attachment->state = AttachmentState::Failed;
			updateAttachmentProgress(*attachment);
			showAttachmentFailedToast();
			requestEditorUpdate();
			maybeContinueSubmittedRequest();
		}
	}

	void updateAttachmentProgress(AttachmentRecord &attachment) {
		if (attachment.state == AttachmentState::Ready) {
			attachment.progress = 1.;
		} else if ((attachment.state == AttachmentState::Uploading)
			|| (attachment.state == AttachmentState::Finalizing)) {
			if (attachment.blockKind == RichPage::BlockKind::Photo) {
				attachment.progress = _session->data().photo(
					attachment.localMediaId)->progress();
			} else {
				attachment.progress = _session->data().document(
					attachment.localMediaId)->progress();
			}
		}
		requestEditorUpdate();
	}

	void requestEditorUpdate() {
		if (_editor) {
			_editor->update();
		}
	}

	[[nodiscard]] AttachmentRecord *findAttachment(FullMsgId uploadId) {
		for (auto &attachment : _attachments) {
			if (attachment.uploadId == uploadId) {
				return &attachment;
			}
		}
		return nullptr;
	}

	[[nodiscard]] bool blockMatchesAttachment(
		const RichPage::Block &block,
		const AttachmentRecord &attachment) const {
		const auto mediaIdMatches = [&](uint64 id) {
			return id
				&& ((id == attachment.localMediaId)
					|| (attachment.serverMediaId
						&& (id == attachment.serverMediaId)));
		};
		switch (attachment.blockKind) {
		case RichPage::BlockKind::Photo:
			return (block.kind == RichPage::BlockKind::Photo)
				&& mediaIdMatches(block.photoId);
		case RichPage::BlockKind::Video:
			return (block.kind == RichPage::BlockKind::Video)
				&& mediaIdMatches(block.documentId);
		case RichPage::BlockKind::Audio:
			return (block.kind == RichPage::BlockKind::Audio)
				&& mediaIdMatches(block.documentId);
		default:
			return false;
		}
	}

	void collectBlockLocators(
		const std::vector<RichPage::Block> &blocks,
		const State::BlockContainerPath &container,
		const AttachmentRecord &attachment,
		std::vector<State::BlockPath> &result) const {
		for (auto i = 0, count = int(blocks.size()); i != count; ++i) {
			const auto path = State::BlockPath{
				.container = container,
				.index = i,
			};
			const auto &block = blocks[i];
			if (blockMatchesAttachment(block, attachment)) {
				result.push_back(path);
			}
			if (!block.blocks.empty()) {
				auto child = container;
				child.steps.push_back({
					.kind = State::BlockContainerKind::BlockChildren,
					.blockIndex = i,
				});
				collectBlockLocators(
					block.blocks,
					child,
					attachment,
					result);
			}
			for (auto itemIndex = 0, items = int(block.listItems.size());
				itemIndex != items;
				++itemIndex) {
				const auto &itemBlocks = block.listItems[itemIndex].blocks;
				if (itemBlocks.empty()) {
					continue;
				}
				auto child = container;
				child.steps.push_back({
					.kind = State::BlockContainerKind::ListItemChildren,
					.blockIndex = i,
					.listItemIndex = itemIndex,
				});
				collectBlockLocators(
					itemBlocks,
					child,
					attachment,
					result);
			}
		}
	}

	void refreshAttachmentLocators(
		const RichPage &page,
		AttachmentRecord &attachment) {
		auto locators = std::vector<State::BlockPath>();
		collectBlockLocators(
			page.blocks,
			State::BlockContainerPath(),
			attachment,
			locators);
		attachment.blockLocators = std::move(locators);
	}

	[[nodiscard]] RichPage *visibleRichPage() const {
		return &const_cast<RichPage&>(_state->richPage());
	}

	[[nodiscard]] std::vector<RichPage::Block> *visibleBlockContainer(
		RichPage &page,
		const State::BlockContainerPath &path) const {
		auto result = &page.blocks;
		for (const auto &step : path.steps) {
			switch (step.kind) {
			case State::BlockContainerKind::Root:
				break;
			case State::BlockContainerKind::BlockChildren:
				if (step.blockIndex < 0 || step.blockIndex >= int(result->size())) {
					return nullptr;
				}
				result = &(*result)[step.blockIndex].blocks;
				break;
			case State::BlockContainerKind::ListItemChildren: {
				if (step.blockIndex < 0 || step.blockIndex >= int(result->size())) {
					return nullptr;
				}
				auto &block = (*result)[step.blockIndex];
				if (step.listItemIndex < 0
					|| step.listItemIndex >= int(block.listItems.size())) {
					return nullptr;
				}
				result = &block.listItems[step.listItemIndex].blocks;
			} break;
			}
		}
		return result;
	}

	[[nodiscard]] RichPage::Block *visibleBlock(
		RichPage &page,
		const State::BlockPath &path) const {
		const auto blocks = visibleBlockContainer(page, path.container);
		if (!blocks || path.index < 0 || path.index >= int(blocks->size())) {
			return nullptr;
		}
		return &(*blocks)[path.index];
	}

	[[nodiscard]] bool patchVisibleAttachmentBlocks(
		RichPage &page,
		AttachmentRecord &attachment) {
		refreshAttachmentLocators(page, attachment);
		for (const auto &locator : attachment.blockLocators) {
			const auto block = visibleBlock(page, locator);
			if (!block || !blockMatchesAttachment(*block, attachment)) {
				continue;
			}
			if (!patchReadyAttachmentBlock(*block, attachment)) {
				return false;
			}
		}
		refreshAttachmentLocators(page, attachment);
		return true;
	}

	[[nodiscard]] int pendingAttachmentPlaceholders() const {
		auto result = _pendingAttachmentPrepareCount;
		if (AcceptedPreparedFileType(_preparingFileType)) {
			++result;
		}
		for (const auto &queued : _prepareQueue) {
			if (AcceptedPreparedFileType(queued.file.type)) {
				++result;
			}
		}
		return result;
	}

	[[nodiscard]] bool exceedsMediaLimitWith(int additionalMedia) const {
		return (CountRichPageMedia(_state->richPage())
			+ pendingAttachmentPlaceholders()
			+ additionalMedia) > _limits.maxMedia;
	}

	[[nodiscard]] bool hasVisibleAttachmentBlock(AttachmentRecord &attachment) {
		refreshAttachmentLocators(_state->richPage(), attachment);
		return !attachment.blockLocators.empty();
	}

	[[nodiscard]] bool hasVisibleFailedAttachments() {
		for (auto &attachment : _attachments) {
			if (attachment.state == AttachmentState::Failed
				&& hasVisibleAttachmentBlock(attachment)) {
				return true;
			}
		}
		return false;
	}

	void showAttachmentFailedToast() {
		_controller->showToast(tr::lng_attach_failed(tr::now));
	}

	void showRichMessageLimitToast(RichMessageLimitError error) const {
		_showLimitToast(error);
	}

	void showUnsupportedMediaToast(uint64 batchId) {
		if (_rejectedToastBatchId == batchId) {
			return;
		}
		_rejectedToastBatchId = batchId;
		_controller->showToast(tr::lng_iv_editor_media_invalid_file(tr::now));
	}

	[[nodiscard]] bool hasPendingPreparation() const {
		return _preparing
			|| !_prepareQueue.empty()
			|| (_pendingAttachmentPrepareCount > 0);
	}

	void maybeContinueDeferredSubmit() {
		if (!_submitDeferred || hasPendingPreparation()) {
			return;
		}
		_submitDeferred = false;
		simulateSubmitClick();
	}

	void simulateSubmitClick() {
		if (!_submitButton) {
			return;
		}
		const auto post = [button = _submitButton](QEvent::Type type) {
			if (!button) {
				return;
			}
			QApplication::postEvent(
				button,
				new QMouseEvent(
					type,
					QPointF(0, 0),
					Qt::LeftButton,
					Qt::LeftButton,
					Qt::NoModifier));
		};
		post(QEvent::MouseButtonPress);
		post(QEvent::MouseButtonRelease);
	}

	const not_null<Window::SessionController*> _controller;
	const not_null<Main::Session*> _session;
	const std::shared_ptr<ChatHelpers::Show> _show;
	const not_null<PeerData*> _peer;
	const Mode _mode;
	const FullMsgId _articleId;
	std::optional<Api::SendAction> _composeAction;
	const Fn<SendMenu::Details()> _sendMenuDetails;
	const std::optional<EditedItemSnapshot> _edited;
	const std::shared_ptr<RichPage> _page;
	const std::shared_ptr<Markdown::MediaRuntime> _runtime;
	const Fn<void(RichMessageLimitError)> _showLimitToast;
	const RichMessageLimits _limits;
	const std::shared_ptr<State> _state;
	Api::SendOptions _submitOptions;
	QPointer<Ui::RpWidget> _submitButton;
	QPointer<Widget> _editor;
	std::unique_ptr<WindowHost> _windowHost;
	std::shared_ptr<ArticleSession> _backgroundHold;
	std::shared_ptr<const RichPage> _submittedPage;
	std::vector<AttachmentRecord> _attachments;
	std::deque<QueuedPrepare> _prepareQueue;
	TaskQueue _attachmentPrepareQueue;
	rpl::lifetime _lifetime;
	uint64 _prepareBatchId = 0;
	uint64 _rejectedToastBatchId = 0;
	int _pendingAttachmentPrepareCount = 0;
	bool _preparing = false;
	PreparedFileType _preparingFileType = PreparedFileType::None;
	bool _submitDeferred = false;
	bool _submitApiRequested = false;

};

} // namespace

void ShowComposeBox(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		Api::SendAction action,
		Fn<SendMenu::Details()> sendMenuDetails) {
	if (!CanUseRichMessages(&controller->session())) {
		ShowRichMessagesPremiumToast(controller);
		return;
	}
	ArticleSession::ShowCompose(
		controller,
		peer,
		std::move(action),
		std::move(sendMenuDetails));
}

void ShowEditBox(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	if (!CanUseRichMessages(&controller->session())) {
		ShowRichMessagesPremiumToast(controller);
		return;
	}
	const auto weak = base::make_weak(controller);
	const auto itemId = item->fullId();
	Core::App().iv().resolveRichMessage(controller, item, [=](
			std::shared_ptr<const RichPage> page) {
		const auto strong = weak.get();
		const auto current = strong
			? strong->session().data().message(itemId)
			: nullptr;
		if (!strong || !current) {
			return;
		}
		if (!page || !CanEditRichPage(page)) {
			strong->showToast(tr::lng_edit_error(tr::now));
			return;
		}
		ArticleSession::ShowEdit(
			not_null{ strong },
			not_null{ current },
			std::move(page));
	});
}

} // namespace Iv::Editor
