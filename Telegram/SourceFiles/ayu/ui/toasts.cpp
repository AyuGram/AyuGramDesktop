#include "ayu/ui/toasts.h"

#include "styles/style_chat.h"
#include "ui/widgets/buttons.h"

#include <memory>

namespace Ayu::Ui {

void ShowToastWithAction(
		::Ui::Toast::Config &&config,
		const QString &buttonText,
		Fn<void()> callback) {
	const auto st = std::make_shared<style::Toast>(*config.st);
	st->padding.setRight(st::historyPremiumViewSet.style.font->width(buttonText)
		- st::historyPremiumViewSet.width);
	config.st = st.get();
	config.acceptinput = true;

	const auto weak = ::Ui::Toast::Show(std::move(config));
	const auto strong = weak.get();
	if (!strong) {
		return;
	}
	const auto widget = strong->widget();
	widget->lifetime().add([st] {});
	const auto hideToast = [weak] {
		if (const auto strong = weak.get()) {
			strong->hideAnimated();
		}
	};

	const auto clickableBackground = ::Ui::CreateChild<::Ui::AbstractButton>(
		widget.get());
	clickableBackground->setPointerCursor(false);
	clickableBackground->setAcceptBoth();
	clickableBackground->show();
	clickableBackground->addClickHandler([=](Qt::MouseButton button) {
		if (button == Qt::RightButton) {
			hideToast();
		}
	});

	const auto button = ::Ui::CreateChild<::Ui::RoundButton>(
		widget.get(),
		rpl::single(buttonText),
		st::historyPremiumViewSet);
	button->show();
	rpl::combine(
		widget->sizeValue(),
		button->sizeValue()
	) | rpl::on_next([=](QSize outer, QSize inner) {
		button->moveToRight(
			0,
			(outer.height() - inner.height()) / 2,
			outer.width());
		clickableBackground->resize(outer);
	}, widget->lifetime());

	button->setClickedCallback([=] {
		callback();
		hideToast();
	});
}

}
