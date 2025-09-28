// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/ui/boxes/import_filters_box.h"

#include "qr/qr_generate.h"
#include "styles/style_boxes.h"
#include "styles/style_intro.h"
#include "styles/style_layers.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/controls/invite_link_label.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtSvg/QSvgRenderer>

#include "lang_auto.h"
#include "ayu/features/filters/filters_utils.h"
#include "styles/style_giveaway.h"
#include "styles/style_settings.h"
#include "ui/toast/toast.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/slide_wrap.h"

namespace Ui {

void FillImportFiltersBox(not_null<Ui::GenericBox*> box, bool import) {
	box->setStyle(st::giveawayGiftCodeBox);
	box->setNoContentMargin(true);
	box->setTitle(import ? tr::ayu_FiltersMenuImport() : tr::ayu_FiltersMenuExport());
	box->verticalLayout()->resizeToWidth(box->width());

	const auto container = box->verticalLayout();

	const auto skip = st::settingsSendTypeSkip;
	auto wrap = object_ptr<Ui::VerticalLayout>(container);
	const auto inner = wrap.data();
	container->add(
		object_ptr<Ui::OverrideMargins>(
			container,
			std::move(wrap),
			QMargins(0, skip, 0, skip)));

	Ui::InputField *importURLField = nullptr;
	Ui::SlideWrap<Ui::VerticalLayout> *importURLWrap = nullptr;

	const auto intoURL = std::make_shared<RadioenumGroup<bool>>(false);
	const auto addOption = [&](bool value, const QString &text)
	{
		inner->add(
			object_ptr<Ui::Radioenum<bool>>(
				inner,
				intoURL,
				value,
				text,
				st::settingsSendType),
			st::settingsSendTypePadding);

		if (import && value) {
			const auto clipboardText = QGuiApplication::clipboard()->text().trimmed();
			const auto prefill = clipboardText.startsWith("http") ? clipboardText : QString();

			importURLWrap = inner->add(
				object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
					inner,
					object_ptr<Ui::VerticalLayout>(inner),
					st::giveawayGiftCodeBox.buttonPadding
				)
			);
			importURLField = importURLWrap->entity()->add(
				object_ptr<Ui::InputField>(
					container,
					st::defaultInputField,
					rpl::single(QString("URL")),
					prefill
				)
			);
			importURLWrap->hide(anim::type::instant);
		}
	};
	addOption(false, import ? tr::ayu_FiltersImportClipboard(tr::now) : tr::ayu_FiltersExportClipboard(tr::now));
	addOption(true, import ? tr::ayu_FiltersImportURL(tr::now) : tr::ayu_FiltersExportURL(tr::now));

	intoURL->setChangedCallback([=](bool value)
	{
		if (import) {
			importURLWrap->toggle(value, anim::type::normal);
		}
	});

	const auto actionButton = box->addButton(
		import ? tr::ayu_FiltersMenuImport() : tr::ayu_FiltersMenuExport(),
		[=]
		{
			const auto isURL = intoURL.get()->current();

			if (import) {
				if (isURL) {
					FilterUtils::getInstance().importFromLink(
						importURLField->getLastText().trimmed());
				} else {
					FilterUtils::getInstance().importFromJson(
						QGuiApplication::clipboard()->text().toUtf8());
				}
			} else {
				if (isURL) {
					FilterUtils::getInstance().publishFilters();
				} else {
					const auto data = FilterUtils::getInstance().exportFilters();
					QGuiApplication::clipboard()->setText(data);

					Toast::Show(tr::lng_text_copied(tr::now));
				}
			}
			box->closeBox();
		});
	const auto buttonWidth = box->width()
		- rect::m::sum::h(st::giveawayGiftCodeBox.buttonPadding);
	actionButton->widthValue() | rpl::filter([=]
	{
		return (actionButton->widthNoMargins() != buttonWidth);
	}) | rpl::start_with_next([=]
							  {
								  actionButton->resizeToWidth(buttonWidth);
							  },
							  actionButton->lifetime());

	box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });
}

} // namespace Ui
