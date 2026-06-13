/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_history_view_media.h"

#include "iv/markdown/iv_markdown_article.h"
#include "base/unixtime.h"
#include "iv/markdown/iv_markdown_media_block.h"

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtWidgets/QApplication>
#include <array>
#include <unordered_set>

#include "settings.h"
#include "ui/image/image_location.h"
#include "data/data_types.h"
#include "data/data_document.h"
#include "data/data_file_click_handler.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/admin_log/history_admin_log_item.h"
#include "history/history_location_manager.h"
#include "history/view/history_view_cursor_state.h"
#include "history/history_view_top_toast.h"
#include "history/view/history_view_element.h"
#include "history/view/history_view_message.h"
#include "history/view/media/history_view_media.h"
#include "ui/basic_click_handlers.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"

#include "rpl/filter.h"
#include "rpl/lifetime.h"

#include <utility>

namespace Iv::Markdown {
namespace {

class IvHistoryViewDelegate final : public HistoryView::SimpleElementDelegate {
public:
	IvHistoryViewDelegate(
		not_null<Window::SessionController*> controller,
		not_null<::Data::Session*> session,
		Fn<void()> update);

	HistoryView::Context elementContext() override;

	HistoryView::ElementChatMode elementChatMode() override;

	bool elementAnimationsPaused() override;

	void elementOpenPhoto(
			not_null<PhotoData*> photo,
			FullMsgId context) override;

	void elementOpenDocument(
			not_null<DocumentData*> document,
			FullMsgId context,
			bool showInMediaView = false) override;

	void elementCancelUpload(const FullMsgId &context) override;

	void elementShowTooltip(
			const TextWithEntities &text,
			Fn<void()> hiddenCallback) override;

private:
	const not_null<::Data::Session*> _session;
	HistoryView::InfoTooltip _tooltip;
};

IvHistoryViewDelegate::IvHistoryViewDelegate(
	not_null<Window::SessionController*> controller,
	not_null<::Data::Session*> session,
	Fn<void()> update)
: HistoryView::SimpleElementDelegate(controller, std::move(update))
, _session(session) {
}

HistoryView::Context IvHistoryViewDelegate::elementContext() {
	return HistoryView::Context::TTLViewer;
}

HistoryView::ElementChatMode IvHistoryViewDelegate::elementChatMode() {
	return HistoryView::ElementChatMode::Default;
}

bool IvHistoryViewDelegate::elementAnimationsPaused() {
	return false;
}

void IvHistoryViewDelegate::elementOpenPhoto(
		not_null<PhotoData*> photo,
		FullMsgId context) {
	controller()->openPhoto(photo, { .id = context });
}

void IvHistoryViewDelegate::elementOpenDocument(
		not_null<DocumentData*> document,
		FullMsgId context,
		bool showInMediaView) {
	controller()->openDocument(
		document,
		showInMediaView,
		{ .id = context });
}

void IvHistoryViewDelegate::elementCancelUpload(const FullMsgId &context) {
	if (const auto item = _session->message(context)) {
		controller()->cancelUploadLayer(item);
	}
}

void IvHistoryViewDelegate::elementShowTooltip(
		const TextWithEntities &text,
		Fn<void()> hiddenCallback) {
	const auto widget = QApplication::activeWindow();
	if (!widget) {
		return;
	}
	_tooltip.show(
		not_null{ widget },
		&_session->session(),
		text,
		std::move(hiddenCallback));
}

struct IvHistoryViewHit {
	ClickHandlerPtr link;
	MediaActivation activation;
	bool supported = true;
};

[[nodiscard]] MediaActivation ExternalActivation(QString url) {
	auto result = MediaActivation();
	if (!url.isEmpty()) {
		result.kind = MediaActivationKind::ExternalUrl;
		result.url = std::move(url);
	}
	return result;
}

[[nodiscard]] not_null<HistoryItem*> CreateIvHostMessage(
		not_null<History*> history,
		QString pageUrl) {
	const auto item = history->addNewLocalMessage({
		.id = history->nextNonHistoryEntryId(),
		.flags = (MessageFlag::FakeHistoryItem
			| MessageFlag::Local
			| MessageFlag::HideDisplayDate),
		.date = base::unixtime::now(),
	}, TextWithEntities(), MTP_messageMediaEmpty());
	item->setMediaForInstantView(std::move(pageUrl));
	return item;
}

class IvHistoryViewBlock final : public MediaBlock {
public:
	explicit IvHistoryViewBlock(IvHistoryViewMediaDescriptor descriptor);
	~IvHistoryViewBlock();

	[[nodiscard]] uint64 stableId() const override;

	[[nodiscard]] bool supported() const;

	[[nodiscard]] int resizeGetHeight(int width) override;

	void setGeometry(QRect geometry) override;

	[[nodiscard]] QRect geometry() const override;

	[[nodiscard]] int firstLineBaseline() const override;

	void paint(
		Painter &p,
		const MarkdownArticlePaintContext &context) const override;

	[[nodiscard]] ClickHandlerPtr linkAt(QPoint point) const override;

	[[nodiscard]] MediaActivation activationAt(QPoint point) const override;

	[[nodiscard]] MediaBlockSelectionData selectionData() const override;

	[[nodiscard]] bool hasHeavyPart() const override;

	void unloadHeavyPart() override;

	void hideSpoilers() override;

private:
	[[nodiscard]] IvHistoryViewHit resolveHit(QPoint point) const;

	[[nodiscard]] IvHistoryViewHit resolveLocalHit(QPoint point) const;

	[[nodiscard]] IvHistoryViewHit classifyState(
			const HistoryView::TextState &state) const;

	[[nodiscard]] IvHistoryViewHit classifyHandler(
			const ClickHandlerPtr &handler) const;

	[[nodiscard]] bool probeSupport();

	[[nodiscard]] bool supportsHitClassification();

	void hostUpdated() override;

	const uint64 _stableId = 0;
	const IvHistoryViewMediaKind _kind = IvHistoryViewMediaKind::Map;
	const QString _copyText;
	const QSize _layoutHint;
	const std::shared_ptr<PhotoRuntime> _photoRuntime;
	const std::shared_ptr<DocumentRuntime> _documentRuntime;
	const base::flat_map<
		uint64,
		std::shared_ptr<PhotoRuntime>> _groupedPhotoRuntimes;
	const base::flat_map<
		uint64,
		std::shared_ptr<DocumentRuntime>> _groupedDocumentRuntimes;
	const std::shared_ptr<IvHistoryViewMediaHost> _host;
	const std::vector<std::shared_ptr<void>> _keepAlive;
	std::unique_ptr<HistoryView::Media> _media;
	QRect _geometry;
	int _requestedWidth = 0;
	bool _supported = false;
	MediaBlockHost *_registeredBridgeHost = nullptr;
};

IvHistoryViewBlock::IvHistoryViewBlock(
	IvHistoryViewMediaDescriptor descriptor)
: _stableId(descriptor.stableId)
, _kind(descriptor.kind)
, _copyText(std::move(descriptor.copyText))
, _layoutHint(descriptor.layoutHint)
, _photoRuntime(std::move(descriptor.photo))
, _documentRuntime(std::move(descriptor.document))
, _groupedPhotoRuntimes(std::move(descriptor.groupedPhotos))
, _groupedDocumentRuntimes(std::move(descriptor.groupedDocuments))
, _host(std::move(descriptor.host))
, _keepAlive(std::move(descriptor.keepAlive)) {
	if (descriptor.mediaFactory) {
		_media = descriptor.mediaFactory(_host->view());
	}
	if (_media) {
		_media->initDimensions();
	}
	_supported = _media && probeSupport();
}

IvHistoryViewBlock::~IvHistoryViewBlock() {
	if (const auto registered = _registeredBridgeHost) {
		_registeredBridgeHost = nullptr;
		_host->unregisterViewRequestBridge(registered);
	}
}

uint64 IvHistoryViewBlock::stableId() const {
	return _stableId;
}

bool IvHistoryViewBlock::supported() const {
	return _supported;
}

int IvHistoryViewBlock::resizeGetHeight(int width) {
	if (!_media) {
		return 0;
	}
	_requestedWidth = std::max(width, 1);
	return _media->resizeGetHeight(_requestedWidth);
}

void IvHistoryViewBlock::setGeometry(QRect geometry) {
	if (!_media) {
		_geometry = geometry;
		return;
	}
	const auto width = std::max(geometry.width(), 1);
	if (_requestedWidth != width) {
		_requestedWidth = width;
		_media->resizeGetHeight(_requestedWidth);
	}
	_geometry = QRect(geometry.topLeft(), _media->currentSize());
}

QRect IvHistoryViewBlock::geometry() const {
	return _geometry;
}

int IvHistoryViewBlock::firstLineBaseline() const {
	return _geometry.y();
}

void IvHistoryViewBlock::paint(
		Painter &p,
		const MarkdownArticlePaintContext &context) const {
	if (!_media || _geometry.isEmpty()) {
		return;
	}
	const auto visible = context.clip.intersected(_geometry);
	if (visible.isEmpty()) {
		return;
	}
	p.save();
	p.translate(_geometry.topLeft());
	auto local = context.translated(-_geometry.topLeft());
	local.clip = visible.translated(-_geometry.topLeft());
	_media->draw(p, local);
	p.restore();
}

ClickHandlerPtr IvHistoryViewBlock::linkAt(QPoint point) const {
	return resolveHit(point).link;
}

MediaActivation IvHistoryViewBlock::activationAt(QPoint point) const {
	return resolveHit(point).activation;
}

MediaBlockSelectionData IvHistoryViewBlock::selectionData() const {
	return {
		.copyText = _copyText,
	};
}

bool IvHistoryViewBlock::hasHeavyPart() const {
	return _media && _media->hasHeavyPart();
}

void IvHistoryViewBlock::unloadHeavyPart() {
	const auto had = hasHeavyPart();
	if (_media) {
		_media->unloadHeavyPart();
	}
	if (had) {
		_host->view()->checkHeavyPart();
	}
}

void IvHistoryViewBlock::hideSpoilers() {
	if (_media) {
		_media->hideSpoilers();
	}
}

IvHistoryViewHit IvHistoryViewBlock::resolveHit(QPoint point) const {
	auto result = IvHistoryViewHit();
	if (!_supported || !_media || !_geometry.contains(point)) {
		return result;
	}
	return resolveLocalHit(point - _geometry.topLeft());
}

IvHistoryViewHit IvHistoryViewBlock::resolveLocalHit(QPoint point) const {
	auto result = IvHistoryViewHit();
	if (!_media) {
		return result;
	}
	const auto state = _media->textState(
		point,
		HistoryView::StateRequest{
			.flags = Ui::Text::StateRequest::Flag::LookupLink
				| Ui::Text::StateRequest::Flag::LookupCustomTooltip,
		});
	return classifyState(state);
}

IvHistoryViewHit IvHistoryViewBlock::classifyState(
		const HistoryView::TextState &state) const {
	return classifyHandler(state.link);
}

IvHistoryViewHit IvHistoryViewBlock::classifyHandler(
		const ClickHandlerPtr &handler) const {
	auto result = IvHistoryViewHit();
	if (!handler) {
		return result;
	}
	if (_kind == IvHistoryViewMediaKind::Photo) {
		if (std::dynamic_pointer_cast<LambdaClickHandler>(handler)) {
			result.link = handler;
			return result;
		}
		if (std::dynamic_pointer_cast<PhotoSaveClickHandler>(handler)
			|| std::dynamic_pointer_cast<PhotoCancelClickHandler>(handler)) {
			result.link = handler;
			return result;
		}
		if (std::dynamic_pointer_cast<PhotoOpenClickHandler>(handler)
			&& _photoRuntime) {
			result.activation.kind = MediaActivationKind::Photo;
			result.activation.photo = _photoRuntime;
			return result;
		}
		result.supported = false;
		return result;
	}
	if (std::dynamic_pointer_cast<VoiceSeekClickHandler>(handler)
		|| std::dynamic_pointer_cast<LambdaClickHandler>(handler)
		|| std::dynamic_pointer_cast<PhotoSaveClickHandler>(handler)
		|| std::dynamic_pointer_cast<PhotoCancelClickHandler>(handler)
		|| std::dynamic_pointer_cast<DocumentSaveClickHandler>(handler)
		|| std::dynamic_pointer_cast<DocumentCancelClickHandler>(handler)
		|| std::dynamic_pointer_cast<DocumentOpenWithClickHandler>(handler)) {
		result.link = handler;
		return result;
	}
	if (_kind == IvHistoryViewMediaKind::Audio
		&& std::dynamic_pointer_cast<DocumentOpenClickHandler>(handler)) {
		result.link = handler;
		return result;
	}
	if (_kind == IvHistoryViewMediaKind::GroupedMedia) {
		if (const auto photoOpen
			= std::dynamic_pointer_cast<PhotoOpenClickHandler>(handler)) {
			const auto i = _groupedPhotoRuntimes.find(photoOpen->photo()->id);
			if (i != end(_groupedPhotoRuntimes)) {
				result.activation.kind = MediaActivationKind::Photo;
				result.activation.photo = i->second;
				return result;
			}
		} else if (const auto documentOpen
			= std::dynamic_pointer_cast<DocumentOpenClickHandler>(handler)) {
			const auto i = _groupedDocumentRuntimes.find(
				documentOpen->document()->id);
			if (i != end(_groupedDocumentRuntimes)) {
				result.activation.kind = MediaActivationKind::Document;
				result.activation.document = i->second;
				return result;
			}
		}
		result.supported = false;
		return result;
	}
	if (std::dynamic_pointer_cast<PhotoOpenClickHandler>(handler)
		&& _photoRuntime) {
		result.activation.kind = MediaActivationKind::Photo;
		result.activation.photo = _photoRuntime;
		return result;
	}
	if (std::dynamic_pointer_cast<DocumentOpenClickHandler>(handler)
		&& _documentRuntime) {
		result.activation.kind = MediaActivationKind::Document;
		result.activation.document = _documentRuntime;
		return result;
	}
	if (std::dynamic_pointer_cast<LocationClickHandler>(handler)) {
		result.activation = ExternalActivation(handler->url());
		return result;
	}
	if (std::dynamic_pointer_cast<UrlClickHandler>(handler)
		|| !handler->url().isEmpty()) {
		result.activation = ExternalActivation(handler->url());
		return result;
	}
	result.supported = false;
	return result;
}

bool IvHistoryViewBlock::probeSupport() {
	if (!_media) {
		return false;
	}
	switch (_kind) {
	case IvHistoryViewMediaKind::Photo:
		return supportsHitClassification();
	case IvHistoryViewMediaKind::Document:
		return supportsHitClassification();
	case IvHistoryViewMediaKind::GroupedMedia:
		return supportsHitClassification();
	case IvHistoryViewMediaKind::Map:
	case IvHistoryViewMediaKind::Audio:
		return true;
	}
	return false;
}

bool IvHistoryViewBlock::supportsHitClassification() {
	const auto width = std::max(_layoutHint.width(), 1);
	_media->resizeGetHeight(width);
	const auto size = _media->currentSize();
	if (size.isEmpty()) {
		return false;
	}
	const auto right = std::max(size.width() - 1, 0);
	const auto bottom = std::max(size.height() - 1, 0);
	const auto points = std::array{
		QPoint(size.width() / 2, size.height() / 2),
		QPoint(0, 0),
		QPoint(right, 0),
		QPoint(0, bottom),
		QPoint(right, bottom),
	};
	for (const auto &point : points) {
		if (!resolveLocalHit(point).supported) {
			return false;
		}
	}
	return true;
}

void IvHistoryViewBlock::hostUpdated() {
	const auto current = host();
	if (_registeredBridgeHost == current) {
		return;
	}
	if (_registeredBridgeHost) {
		_host->unregisterViewRequestBridge(_registeredBridgeHost);
	}
	_registeredBridgeHost = current;
	if (_registeredBridgeHost) {
		_host->registerViewRequestBridge(_registeredBridgeHost);
	}
}

} // namespace

struct IvHistoryViewMediaHost::State {
	State(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		QString pageUrl);
	State(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item);
	explicit State(not_null<HistoryView::Element*> view);

	const not_null<::Data::Session*> session;
	const QString pageUrl;
	const std::unique_ptr<IvHistoryViewDelegate> delegate;
	const not_null<HistoryItem*> item;
	AdminLog::OwnedItem owned;
	std::unique_ptr<HistoryView::Element> realView;
	HistoryView::Element *view = nullptr;
	bool needsViewRequestBridge = true;
	MediaBlockHost *bridgeHost = nullptr;
	int bridgeHostReferences = 0;
	rpl::lifetime bridgeLifetime;
};

IvHistoryViewMediaHost::State::State(
	not_null<Window::SessionController*> controller,
	not_null<History*> history,
	QString pageUrl)
: session(&history->owner())
, pageUrl(std::move(pageUrl))
, delegate(std::make_unique<IvHistoryViewDelegate>(
	controller,
	session,
	[=] {
		if (view) {
			view->repaint();
		}
	}))
, item(CreateIvHostMessage(history, this->pageUrl))
, owned(delegate.get(), item)
, view(static_cast<HistoryView::Message*>(owned.get())) {
	static_cast<HistoryView::Message*>(view)->setInstantViewMediaRuntime(
		this->pageUrl);
}

IvHistoryViewMediaHost::State::State(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item)
: session(&item->history()->owner())
, delegate(std::make_unique<IvHistoryViewDelegate>(
	controller,
	session,
	[=] {
		if (view) {
			view->repaint();
		}
	}))
, item(item)
, realView(this->item->createView(delegate.get()))
, view(static_cast<HistoryView::Message*>(realView.get())) {
	static_cast<HistoryView::Message*>(view)->setInstantViewMediaRuntime(
		this->pageUrl);
}

IvHistoryViewMediaHost::State::State(
	not_null<HistoryView::Element*> view)
: session(&view->history()->owner())
, item(view->data())
, view(view.get())
, needsViewRequestBridge(false) {
}

IvHistoryViewMediaHost::IvHistoryViewMediaHost(
	not_null<Window::SessionController*> controller,
	not_null<History*> history,
	QString pageUrl)
: _state(std::make_unique<State>(
	controller,
	history,
	std::move(pageUrl))) {
}

IvHistoryViewMediaHost::IvHistoryViewMediaHost(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item)
: _state(std::make_unique<State>(controller, item)) {
}

IvHistoryViewMediaHost::IvHistoryViewMediaHost(
	not_null<HistoryView::Element*> view)
: _state(std::make_unique<State>(view)) {
}

IvHistoryViewMediaHost::~IvHistoryViewMediaHost() = default;

not_null<::Data::Session*> IvHistoryViewMediaHost::session() const {
	return _state->session;
}

not_null<HistoryItem*> IvHistoryViewMediaHost::item() const {
	return _state->item;
}

not_null<HistoryView::Element*> IvHistoryViewMediaHost::view() const {
	return not_null<HistoryView::Element*>{ _state->view };
}

const QString &IvHistoryViewMediaHost::pageUrl() const {
	return _state->pageUrl;
}

bool IvHistoryViewMediaHost::needsViewRequestBridge() const {
	return _state->needsViewRequestBridge;
}

void IvHistoryViewMediaHost::registerViewRequestBridge(MediaBlockHost *host) {
	if (!host || !_state->needsViewRequestBridge) {
		return;
	}
	if (_state->bridgeHost == host) {
		++_state->bridgeHostReferences;
		return;
	}
	_state->bridgeLifetime.destroy();
	_state->bridgeHost = host;
	_state->bridgeHostReferences = 1;
	_state->session->viewRepaintRequest(
	) | rpl::filter([=](::Data::RequestViewRepaint data) {
		return (data.view == _state->view);
	}) | rpl::on_next([=](::Data::RequestViewRepaint) {
		if (_state->bridgeHost) {
			_state->bridgeHost->requestRepaint(QRect());
		}
	}, _state->bridgeLifetime);
	_state->session->viewResizeRequest(
	) | rpl::filter([=](not_null<HistoryView::Element*> view) {
		return (view == _state->view);
	}) | rpl::on_next([=](not_null<HistoryView::Element*>) {
		if (_state->bridgeHost) {
			_state->bridgeHost->requestRelayout(QRect());
		}
	}, _state->bridgeLifetime);
}

void IvHistoryViewMediaHost::unregisterViewRequestBridge(MediaBlockHost *host) {
	if (!host
		|| !_state->needsViewRequestBridge
		|| _state->bridgeHost != host) {
		return;
	}
	--_state->bridgeHostReferences;
	if (_state->bridgeHostReferences > 0) {
		return;
	}
	_state->bridgeHostReferences = 0;
	_state->bridgeHost = nullptr;
	_state->bridgeLifetime.destroy();
}

void IvHistoryViewMediaHost::registerPhoto(not_null<PhotoData*> photo) const {
	_state->item->addPhotoForInstantView(photo);
}

void IvHistoryViewMediaHost::registerDocument(
		not_null<DocumentData*> document) const {
	_state->item->addDocumentForInstantView(document);
}

IvHistoryViewMediaBlockFactory::IvHistoryViewMediaBlockFactory(
	base::weak_ptr<Window::SessionController> controller,
	PhotoFactory createPhoto,
	VideoFactory createVideo,
	AudioFactory createAudio,
	MapFactory createMap,
	GroupedMediaFactory createGroupedMedia)
: _controller(std::move(controller))
, _createPhoto(std::move(createPhoto))
, _createVideo(std::move(createVideo))
, _createAudio(std::move(createAudio))
, _createMap(std::move(createMap))
, _createGroupedMedia(std::move(createGroupedMedia)) {
}

std::shared_ptr<MediaBlock> IvHistoryViewMediaBlockFactory::createPhoto(
		const PreparedPhotoBlockData &prepared) const {
	return create(prepared, _createPhoto);
}

std::shared_ptr<MediaBlock> IvHistoryViewMediaBlockFactory::createVideo(
		const PreparedVideoBlockData &prepared) const {
	return create(prepared, _createVideo);
}

std::shared_ptr<MediaBlock> IvHistoryViewMediaBlockFactory::createAudio(
		const PreparedAudioBlockData &prepared) const {
	return create(prepared, _createAudio);
}

std::shared_ptr<MediaBlock> IvHistoryViewMediaBlockFactory::createMap(
		const PreparedMapBlockData &prepared) const {
	return create(prepared, _createMap);
}

auto IvHistoryViewMediaBlockFactory::createGroupedMedia(
	const PreparedGroupedMediaBlockData &prepared) const
-> std::shared_ptr<MediaBlock> {
	return create(prepared, _createGroupedMedia);
}

std::shared_ptr<MediaBlock> CreateIvHistoryViewMediaBlock(
		IvHistoryViewMediaDescriptor descriptor) {
	if (!descriptor.host) {
		return nullptr;
	}
	if (!descriptor.mediaFactory) {
		return nullptr;
	}
	const auto block = std::make_shared<IvHistoryViewBlock>(
		std::move(descriptor));
	return block->supported() ? block : nullptr;
}

} // namespace Iv::Markdown
