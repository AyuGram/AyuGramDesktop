/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/sticker_creator_box.h"

#include "api/api_stickers_creator.h"
#include "chat_helpers/compose/compose_show.h"
#include "chat_helpers/emoji_picker_overlay.h"
#include "core/file_utilities.h"
#include "editor/editor_layer_widget.h"
#include "editor/photo_editor.h"
#include "editor/photo_editor_common.h"
#include "editor/scene/scene.h"
#include "editor/scene/scene_item_image.h"
#include "info/channel_statistics/boosts/giveaway/boost_badge.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/emoji_config.h"
#include "ui/image/image.h"
#include "ui/image/image_prepare.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/layer_widget.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_editor.h"
#include "styles/style_layers.h"

#include <QtCore/QBuffer>
#include <QtGui/QImageReader>

namespace {

constexpr auto kStickerSide = 512;
constexpr auto kPreviewSide = 256;
constexpr auto kWebpQuality = 95;
constexpr auto kMaxEmojis = 7;

[[nodiscard]] QImage LoadImageFromFile(const QString &path) {
	auto reader = QImageReader(path);
	reader.setAutoTransform(true);
	auto image = reader.read();
	if (image.format() != QImage::Format_ARGB32_Premultiplied
		&& image.format() != QImage::Format_ARGB32) {
		image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	}
	return image;
}

class PreviewWidget final : public Ui::RpWidget {
public:
	PreviewWidget(QWidget *parent, QImage image)
	: RpWidget(parent)
	, _image(std::move(image)) {
		resize(kPreviewSide, kPreviewSide);
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		auto hq = PainterHighQualityEnabler(p);
		const auto target = QRect(0, 0, width(), height());
		p.drawImage(target, _image);
	}

private:
	const QImage _image;

};

void OpenPhotoEditorForSticker(
		std::shared_ptr<ChatHelpers::Show> show,
		QImage image,
		Fn<void(QImage&&)> onDone) {
	if (image.isNull()) {
		show->showToast(tr::lng_stickers_create_open_failed(tr::now));
		return;
	}
	const auto sessionController = show->resolveWindow();
	if (!sessionController) {
		show->showToast(tr::lng_stickers_create_open_failed(tr::now));
		return;
	}
	const auto windowController = &sessionController->window();
	const auto parentWidget = sessionController->widget();

	if (image.width() <= 0
		|| image.height() <= 0
		|| (image.width() > 10 * image.height())
		|| (image.height() > 10 * image.width())) {
		show->showToast(tr::lng_stickers_create_open_failed(tr::now));
		return;
	}

	auto canvas = QImage(
		kStickerSide,
		kStickerSide,
		QImage::Format_ARGB32_Premultiplied);
	canvas.fill(Qt::transparent);
	const auto baseImage = std::make_shared<Image>(std::move(canvas));

	auto scene = std::make_shared<Editor::Scene>(
		QRectF(0, 0, kStickerSide, kStickerSide));

	const auto userPixmap = QPixmap::fromImage(std::move(image));
	const auto userSize = userPixmap.size();
	const auto fitted = userSize.scaled(
		QSize(kStickerSide, kStickerSide),
		Qt::KeepAspectRatio);
	const auto handle = st::photoEditorItemHandleSize;
	const auto itemSize = (userSize.width() >= userSize.height())
		? int((fitted.height() + handle)
			* userSize.width() / float64(userSize.height()))
		: (fitted.width() + handle);
	auto itemData = Editor::ItemBase::Data{
		.initialZoom = 1.0,
		.zPtr = scene->lastZ(),
		.size = itemSize,
		.x = kStickerSide / 2,
		.y = kStickerSide / 2,
		.imageSize = userSize,
	};
	auto imageItem = std::make_shared<Editor::ItemImage>(
		QPixmap(userPixmap),
		std::move(itemData));
	scene->addItem(std::move(imageItem));

	auto modifications = Editor::PhotoModifications{
		.crop = QRect(0, 0, kStickerSide, kStickerSide),
		.paint = std::move(scene),
	};

	auto editor = base::make_unique_q<Editor::PhotoEditor>(
		parentWidget,
		windowController,
		baseImage,
		std::move(modifications),
		Editor::EditorData{
			.exactSize = QSize(kStickerSide, kStickerSide),
			.cropType = Editor::EditorData::CropType::RoundedRect,
			.cropMode = Editor::EditorData::CropMode::Mask,
			.keepAspectRatio = true,
			.fixedCrop = true,
		});
	const auto raw = editor.get();

	auto applyModifications = [=, done = std::move(onDone)](
			const Editor::PhotoModifications &mods) mutable {
		auto result = Editor::ImageModified(baseImage->original(), mods);
		if (result.size() != QSize(kStickerSide, kStickerSide)) {
			result = result.scaled(
				kStickerSide,
				kStickerSide,
				Qt::IgnoreAspectRatio,
				Qt::SmoothTransformation);
		}
		Editor::ApplyShapeMask(result, mods);
		done(std::move(result));
	};

	auto layer = std::make_unique<Editor::LayerWidget>(
		parentWidget,
		std::move(editor));
	Editor::InitEditorLayer(layer.get(), raw, std::move(applyModifications));
	windowController->showLayer(
		std::move(layer),
		Ui::LayerOption::KeepOther);
}

[[nodiscard]] QByteArray EncodeWebp(QImage image) {
	if (image.size() != QSize(kStickerSide, kStickerSide)) {
		image = image.scaled(
			kStickerSide,
			kStickerSide,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
	}
	if (image.format() != QImage::Format_ARGB32) {
		image = image.convertToFormat(QImage::Format_ARGB32);
	}
	auto bytes = QByteArray();
	auto buffer = QBuffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	image.save(&buffer, "WEBP", kWebpQuality);
	return bytes;
}

} // namespace

namespace Api {

void CreateStickerBox(
		not_null<Ui::GenericBox*> box,
		std::shared_ptr<ChatHelpers::Show> show,
		StickerSetIdentifier set,
		QImage image,
		Fn<void(MTPmessages_StickerSet)> done) {
	struct State {
		rpl::variable<bool> uploading = false;
		std::unique_ptr<StickerUpload> upload;
		QPointer<Ui::RoundButton> addButton;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto session = &show->session();

	box->setTitle(tr::lng_stickers_create_image_title());

	const auto inner = box->verticalLayout();

	auto pickerDescriptor = ChatHelpers::EmojiPickerOverlayDescriptor{
		.aboutText = tr::lng_stickers_create_emoji_about(tr::now),
		.maxSelected = kMaxEmojis,
		.allowExpand = true,
	};
	const auto metrics = ChatHelpers::EmojiPickerOverlay::EstimateMetrics(
		pickerDescriptor.aboutText);
	const auto pickerCollapsed = metrics.collapsedHeight;
	const auto pickerTotalExpanded = metrics.totalExpandedHeight;
	const auto shadowExt = metrics.shadowExtent;

	constexpr auto kStickerOverlap = 24;
	const auto stickerTop = shadowExt.top()
		+ pickerCollapsed
		- kStickerOverlap;
	const auto holderHeight = std::max(
		stickerTop + kPreviewSide,
		pickerTotalExpanded);

	const auto previewHolder = inner->add(
		object_ptr<Ui::RpWidget>(inner),
		QMargins(0, 0, 0, 0),
		style::al_top);
	previewHolder->resize(st::boxWideWidth, holderHeight);
	const auto preview = Ui::CreateChild<PreviewWidget>(
		previewHolder,
		image);

	const auto picker = Ui::CreateChild<ChatHelpers::EmojiPickerOverlay>(
		previewHolder,
		std::move(pickerDescriptor));

	auto layoutOverlay = [=] {
		const auto bubbleW = std::min(
			previewHolder->width()
				- 2 * st::boxRowPadding.left()
				- shadowExt.left() - shadowExt.right(),
			int(kPreviewSide * 1.1));
		const auto totalW = bubbleW + shadowExt.left() + shadowExt.right();
		const auto x = (previewHolder->width() - totalW) / 2;
		picker->setGeometry(x, 0, totalW, pickerTotalExpanded);
		picker->raise();
	};

	previewHolder->widthValue(
	) | rpl::on_next([=](int width) {
		preview->move((width - kPreviewSide) / 2, stickerTop);
		layoutOverlay();
	}, preview->lifetime());

	Ui::AddSkip(inner);

	const auto startUpload = [=, set = std::move(set), done = std::move(done)](
			) mutable {
		if (state->uploading.current()) {
			return;
		}
		auto emoji = QString();
		for (const auto one : picker->selected()) {
			emoji.append(one->text());
		}
		if (emoji.isEmpty()) {
			show->showToast(
				tr::lng_stickers_create_emoji_required(tr::now));
			return;
		}
		const auto bytes = EncodeWebp(image);
		if (bytes.isEmpty()) {
			show->showToast(
				tr::lng_stickers_create_upload_failed(tr::now));
			return;
		}

		const auto lockedWidth = state->addButton
			? state->addButton->width()
			: 0;
		state->uploading = true;
		if (state->addButton && lockedWidth > 0) {
			state->addButton->resizeToWidth(lockedWidth);
		}
		state->upload = std::make_unique<StickerUpload>(
			session,
			set,
			bytes,
			emoji);

		const auto doneCallback = done;
		state->upload->start(
			crl::guard(box, [=](MTPmessages_StickerSet result) {
				state->upload = nullptr;
				state->uploading = false;
				show->showToast(tr::lng_stickers_create_added(tr::now));
				if (doneCallback) {
					doneCallback(result);
				}
				box->closeBox();
			}),
			crl::guard(box, [=](QString err) {
				state->upload = nullptr;
				state->uploading = false;
				show->showToast(err.isEmpty()
					? tr::lng_stickers_create_upload_failed(tr::now)
					: err);
			}));
	};

	const auto addButton = box->addButton(
		rpl::conditional(
			state->uploading.value(),
			rpl::single(QString()),
			tr::lng_box_done()),
		startUpload);
	state->addButton = addButton;
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });

	{
		using namespace Info::Statistics;
		const auto loadingAnimation = InfiniteRadialAnimationWidget(
			addButton,
			addButton->height() / 2,
			&st::editStickerSetNameLoading);
		AddChildToWidgetCenter(addButton, loadingAnimation);
		loadingAnimation->showOn(state->uploading.value());
	}

	box->setWidth(st::boxWideWidth);

	box->boxClosing(
	) | rpl::on_next([=] {
		state->upload = nullptr;
	}, box->lifetime());
}

void OpenCreateStickerFlow(
		std::shared_ptr<ChatHelpers::Show> show,
		StickerSetIdentifier set,
		Fn<void(MTPmessages_StickerSet)> done) {
	const auto parent = QPointer<QWidget>(show->toastParent());

	const auto onChosen = [=, set = std::move(set), done = std::move(done)](
			FileDialog::OpenResult &&result) mutable {
		if (result.paths.isEmpty() && result.remoteContent.isEmpty()) {
			return;
		}
		const auto path = result.paths.isEmpty()
			? QString()
			: result.paths.front();
		auto image = path.isEmpty()
			? QImage::fromData(result.remoteContent)
			: LoadImageFromFile(path);
		OpenPhotoEditorForSticker(
			show,
			std::move(image),
			[=, set = std::move(set), done = std::move(done)](
					QImage &&prepared) mutable {
				show->showBox(Box(
					CreateStickerBox,
					show,
					std::move(set),
					std::move(prepared),
					std::move(done)));
			});
	};

	FileDialog::GetOpenPath(
		parent,
		tr::lng_stickers_create_choose_image(tr::now),
		FileDialog::ImagesFilter(),
		std::move(onChosen));
}

} // namespace Api
