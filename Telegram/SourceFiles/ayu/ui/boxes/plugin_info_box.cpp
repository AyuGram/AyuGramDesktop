// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/plugin_info_box.h"

#include "apiwrap.h"
#include "core/file_utilities.h"
#include "core/ui_integration.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers_set.h"
#include "lang/lang_keys.h"
#include "chat_helpers/stickers_lottie.h"
#include "history/view/media/history_view_sticker_player.h"
#include "main/main_session.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_giveaway.h"
#include "styles/style_layers.h"
#include "styles/style_premium.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/vertical_list.h"
#include "ui/image/image_prepare.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "window/window_session_controller.h"

#include <QPainterPath>
#include <QRegularExpression>


namespace Ui {
namespace {

QString ExtractField(const QByteArray &data, const QString &key) {
	const auto re = QRegularExpression(
		u"__%1__\\s*=\\s*(?:\"((?:\\\\.|[^\"\\\\])*)\"|'((?:\\\\.|[^'\\\\])*)')"_q.arg(key));
	const auto match = re.match(QString::fromUtf8(data));
	if (!match.hasMatch()) {
		return {};
	}
	auto result = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
	result.replace(u"\\n"_q, u"\n"_q);
	result.replace(u"\\t"_q, u"\t"_q);
	result.replace(u"\\\""_q, u"\""_q);
	result.replace(u"\\'"_q, u"'"_q);
	result.replace(u"\\\\"_q, u"\\"_q);
	return result;
}

void FillPluginInfoBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	const QString &pluginPath,
	PluginMetadata metadata,
	DocumentData *stickerDoc) {
	box->setStyle(st::giveawayGiftCodeBox);
	box->setNoContentMargin(true);
	box->setWidth(st::aboutWidth);
	box->verticalLayout()->resizeToWidth(box->width());

	box->addTopButton(st::pluginShowInFolder,
					  [=]
					  {
						  File::ShowInFolder(pluginPath);
					  });

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	if (stickerDoc) {
		const auto stickerSize = st::maxStickerSize / 3;
		const auto radius = stickerSize / 5;
		const auto widget = box->verticalLayout()->add(
			object_ptr<Ui::RpWidget>(box->verticalLayout()));
		widget->resize(Size(stickerSize));

		const auto media = stickerDoc->createMediaView();
		media->checkStickerLarge();
		const auto sticker = stickerDoc->sticker();

		const auto player = widget->lifetime().make_state<
			std::unique_ptr<HistoryView::StickerPlayer>>();

		const auto createPlayer = [=] {
			if (sticker->isLottie()) {
				*player = std::make_unique<HistoryView::LottiePlayer>(
					ChatHelpers::LottiePlayerFromDocument(
						media.get(),
						ChatHelpers::StickerLottieSize::StickerSet,
						Size(stickerSize),
						Lottie::Quality::High));
			} else if (sticker->isWebm()) {
				*player = std::make_unique<HistoryView::WebmPlayer>(
					media->owner()->location(),
					media->bytes(),
					Size(stickerSize));
			} else {
				*player = std::make_unique<HistoryView::StaticStickerPlayer>(
					media->owner()->location(),
					media->bytes(),
					Size(stickerSize));
			}
			(*player)->setRepaintCallback([=] { widget->update(); });
		};

		if (media->loaded()) {
			createPlayer();
		} else {
			rpl::single() | rpl::then(
				stickerDoc->owner().session().downloaderTaskFinished()
			) | rpl::filter([=] {
				return media->loaded();
			}) | rpl::take(1) | rpl::on_next([=] {
				createPlayer();
			}, widget->lifetime());
		}

		widget->paintRequest(
		) | rpl::on_next([=] {
			if (!*player || !(*player)->ready()) {
				return;
			}
			const auto frame = (*player)->frame(
				Size(stickerSize),
				QColor(0, 0, 0, 0),
				false,
				crl::now(),
				false);
			if (frame.image.isNull()) {
				return;
			}
			auto p = QPainter(widget);
			PainterHighQualityEnabler hq(p);
			const auto x = (widget->width() - stickerSize) / 2;
			QPainterPath path;
			path.addRoundedRect(
				QRectF(x, 0, stickerSize, stickerSize),
				radius,
				radius);
			p.setClipPath(path);
			p.drawImage(
				QRect(x, 0, stickerSize, stickerSize),
				frame.image);
			(*player)->markFrameShown();
		}, widget->lifetime());

		Ui::AddSkip(box->verticalLayout());
		Ui::AddSkip(box->verticalLayout());
	}

	box->verticalLayout()->add(
		object_ptr<Ui::FlatLabel>(
			box->verticalLayout(),
			rpl::single(metadata.name),
			st::boxTitle),
		st::boxRowPadding,
		style::al_top);

	{
		const auto versionLabel = box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				st::pluginVersionAuthorLabel),
			st::boxRowPadding,
			style::al_justify);

		const auto versionPrefix = tr::ayu_PluginVersion(tr::now)
			+ u" "_q
			+ metadata.version;

		if (metadata.author.isEmpty()) {
			versionLabel->setText(
				versionPrefix
				+ u" \u2022 "_q
				+ tr::ayu_PluginNoAuthor(tr::now));
		} else {
			auto text = TextWithEntities{
				versionPrefix + u" \u2022 "_q,
			};
			static const auto usernameRe = QRegularExpression(
				u"@([A-Za-z0-9_]+)"_q);
			const auto &author = metadata.author;
			auto it = usernameRe.globalMatch(author);
			auto lastEnd = 0;
			while (it.hasNext()) {
				const auto match = it.next();
				if (match.capturedStart() > lastEnd) {
					text.append(TextWithEntities{
						author.mid(lastEnd,
								   match.capturedStart() - lastEnd)
					});
				}
				text.append(Ui::Text::Link(
					match.captured(0),
					controller->session().createInternalLinkFull(
						match.captured(1))));
				lastEnd = match.capturedEnd();
			}
			if (lastEnd < author.size()) {
				text.append(TextWithEntities{
					author.mid(lastEnd)
				});
			}
			if (lastEnd == 0) {
				text.append(TextWithEntities{author});
			}
			versionLabel->setMarkedText(
				text,
				Core::TextContext({
					.session = &controller->session(),
				}));
		}
	}

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	{
		const auto hasDescription = !metadata.description.isEmpty();
		auto descText = hasDescription
			? TextWithEntities{metadata.description}
			: TextWithEntities{tr::ayu_PluginNoDescription(tr::now)};
		if (hasDescription) {
			TextUtilities::ParseEntities(
				descText,
				TextParseLinks | TextParseMentions | TextParseMarkdown);
		}

		const auto availableWidth = box->width()
			- st::boxRowPadding.left()
			- st::boxRowPadding.right();

		const auto container = box->verticalLayout()->add(
			object_ptr<Ui::RpWidget>(box->verticalLayout()),
			st::boxRowPadding);

		const auto scroll = Ui::CreateChild<Ui::ScrollArea>(
			container,
			st::boxScroll);
		const auto descLabel = scroll->setOwnedWidget(
			object_ptr<Ui::FlatLabel>(scroll, st::boxLabel));
		descLabel->setMarkedText(
			descText,
			Core::TextContext({
				.session = &controller->session(),
			}));
		descLabel->resizeToWidth(availableWidth);

		const auto needsScroll =
			descLabel->height() > st::maxPluginDescriptionHeight;
		const auto scrollPad = needsScroll
								   ? (st::boxScroll.width + st::lineWidth)
								   : 0;
		if (needsScroll) {
			descLabel->resizeToWidth(availableWidth - scrollPad);
		}

		const auto containerHeight = std::min(
			descLabel->height(),
			st::maxPluginDescriptionHeight);
		container->resize(availableWidth, containerHeight);

		container->sizeValue(
		) | rpl::on_next([=](QSize size)
						 {
							 scroll->setGeometry(0, 0, size.width(), size.height());
							 descLabel->resizeToWidth(size.width() - scrollPad);
						 },
						 container->lifetime());

		Ui::AddSkip(box->verticalLayout());
	}

	if (!metadata.requirements.isEmpty()) {
		const auto chipsContainer = box->verticalLayout()->add(
			object_ptr<Ui::RpWidget>(box->verticalLayout()),
			st::boxRowPadding);

		auto chips = std::vector<Ui::RoundButton*>();
		for (const auto &dep : metadata.requirements) {
			const auto chip = Ui::CreateChild<Ui::RoundButton>(
				chipsContainer,
				rpl::single(dep),
				st::defaultTableSmallButton);
			chip->setTextTransform(
				Ui::RoundButtonTextTransform::NoTransform);
			chip->setClickedCallback([dep] {
				File::OpenUrl(
					u"https://pypi.org/project/"_q + dep + u"/"_q);
			});
			chips.push_back(chip);
		}

		const auto chipsPtr = chipsContainer->lifetime().make_state<
			std::vector<Ui::RoundButton*>>(std::move(chips));

		chipsContainer->sizeValue(
		) | rpl::on_next([=](QSize size)
						 {
							 const auto skip = st::pluginChipSkip;
							 auto x = 0;
							 auto y = 0;
							 auto widthLeft = size.width();
							 auto firstRow = true;
							 for (const auto chip : *chipsPtr) {
								 const auto chipWidth = chip->width();
								 const auto chipHeight = chip->height();
								 if (!firstRow && chipWidth > widthLeft) {
									 x = 0;
									 y += chipHeight + skip;
									 widthLeft = size.width();
								 }
								 firstRow = false;
								 chip->moveToLeft(x, y);
								 x += chipWidth + skip;
								 widthLeft = size.width() - x;
							 }
							 if (!chipsPtr->empty()) {
								 const auto lastChip = chipsPtr->back();
								 chipsContainer->resize(
									 size.width(),
									 lastChip->y() + lastChip->height());
							 }
						 },
						 chipsContainer->lifetime());

		Ui::AddSkip(box->verticalLayout());
	}

	Ui::AddSkip(box->verticalLayout());

	box->verticalLayout()->add(
		object_ptr<Ui::FlatLabel>(
			box->verticalLayout(),
			tr::ayu_PluginsNotAvailable(),
			st::boxDividerLabel),
		st::boxRowPadding);

	Ui::AddSkip(box->verticalLayout());

	const auto closeButton = box->addButton(
		tr::lng_close(),
		[=] { box->closeBox(); });
	const auto buttonWidth = box->width()
		- rect::m::sum::h(st::starrefFooterBox.buttonPadding);
	closeButton->widthValue() | rpl::filter([=]
	{
		return (closeButton->widthNoMargins() != buttonWidth);
	}) | rpl::on_next([=]
					  {
						  closeButton->resizeToWidth(buttonWidth);
					  },
					  closeButton->lifetime());
}

}

PluginMetadata ParsePluginMetadata(const QByteArray &data) {
	auto metadata = PluginMetadata();

	metadata.id = ExtractField(data, u"id"_q);
	metadata.name = ExtractField(data, u"name"_q).left(100);
	metadata.description = ExtractField(data, u"description"_q).left(8000);
	metadata.author = ExtractField(data, u"author"_q).left(200);
	metadata.version = ExtractField(data, u"version"_q).left(32);
	metadata.icon = ExtractField(data, u"icon"_q);
	metadata.minVersion = ExtractField(data, u"min_version"_q);

	const auto reqString = ExtractField(data, u"requirements"_q);
	if (!reqString.isEmpty()) {
		for (const auto &part : reqString.split(',')) {
			const auto trimmed = part.trimmed();
			if (!trimmed.isEmpty()) {
				metadata.requirements.append(trimmed);
			}
		}
	}

	if (metadata.version.isEmpty()) {
		metadata.version = u"1.0"_q;
	}

	static const auto idRegex = QRegularExpression(
		u"^[a-zA-Z][a-zA-Z0-9_-]{1,31}$"_q);
	if (metadata.id.isEmpty()
		|| !idRegex.match(metadata.id).hasMatch()) {
		return {};
	}

	if (metadata.name.isEmpty()) {
		return {};
	}

	return metadata;
}

void ShowPluginInfoBox(
	not_null<Window::SessionController*> controller,
	const QString &pluginPath,
	PluginMetadata metadata) {
	const auto showBox = [controller, pluginPath](
		PluginMetadata md,
		DocumentData *stickerDoc)
	{
		controller->show(Box(
			FillPluginInfoBox,
			controller,
			pluginPath,
			std::move(md),
			stickerDoc));
	};

	if (metadata.icon.isEmpty()) {
		showBox(std::move(metadata), nullptr);
		return;
	}

	const auto parts = metadata.icon.split('/');
	if (parts.size() != 2) {
		showBox(std::move(metadata), nullptr);
		return;
	}

	const auto shortName = parts[0];
	const auto index = parts[1].toInt();
	const auto session = &controller->session();
	const auto shared = std::make_shared<PluginMetadata>(
		std::move(metadata));

	session->api().request(MTPmessages_GetStickerSet(
		Data::InputStickerSet({
			.shortName = shortName,
		}),
		MTP_int(0)
	)).done([=](const MTPmessages_StickerSet &result)
	{
		auto doc = (DocumentData*) nullptr;
		result.match([&](const MTPDmessages_stickerSet &data)
					 {
						 const auto &v = data.vdocuments().v;
						 if (index >= 0 && index < v.size()) {
							 doc = session->data().processDocument(v[index]);
						 }
					 },
					 [](const MTPDmessages_stickerSetNotModified &)
					 {
					 });

		if (!doc) {
			showBox(std::move(*shared), nullptr);
			return;
		}

		const auto media = doc->createMediaView();
		media->checkStickerLarge();

		if (media->loaded()) {
			showBox(std::move(*shared), doc);
			return;
		}

		rpl::single() | rpl::then(
			session->downloaderTaskFinished()
		) | rpl::filter([=] {
			return media->loaded();
		}) | rpl::take(1) | rpl::on_next([=] {
			showBox(std::move(*shared), doc);
		}, session->lifetime());
		media->automaticLoad(doc->stickerSetOrigin(), nullptr);
	}).fail([=](const MTP::Error &)
	{
		showBox(std::move(*shared), nullptr);
	}).send();
}

}
