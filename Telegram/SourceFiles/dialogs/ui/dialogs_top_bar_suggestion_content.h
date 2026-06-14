/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/buttons.h"
#include "ui/widgets/shadow.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"

namespace Ui {
class DynamicImage;
class IconButton;
} // namespace Ui

namespace Ui::Text {
struct MarkedContext;
} // namespace Ui::Text

namespace Data {
struct UnreviewedAuth;
} // namespace Data

namespace Dialogs {

class UnconfirmedAuthWrap : public Ui::SlideWrap<Ui::VerticalLayout> {
public:
	UnconfirmedAuthWrap(
		not_null<Ui::RpWidget*> parent,
		object_ptr<Ui::VerticalLayout> &&child);

	[[nodiscard]] const Ui::BoxShadow &shadow() const {
		return _shadow;
	}

	[[nodiscard]] rpl::producer<int> desiredHeightValue() const override;

	void setCollapseProgress(rpl::producer<float64> progress);
	void prepareCollapseSnapshot();

protected:
	int resizeGetHeight(int newWidth) override;

private:
	void releaseCollapseSnapshot();

	float64 _collapseProgress = 0.;
	QPixmap _collapseSnapshot;
	Ui::BoxShadow _shadow;

};

not_null<UnconfirmedAuthWrap*> CreateUnconfirmedAuthContent(
		not_null<Ui::RpWidget*> parent,
		rpl::producer<std::vector<Data::UnreviewedAuth>> list,
		Fn<void(bool)> callback,
		rpl::producer<float64> collapseProgress);

class TopBarSuggestionContent : public Ui::RippleButton {
public:
	enum class RightIcon {
		None,
		Close,
		Arrow,
	};

	TopBarSuggestionContent(
		not_null<Ui::RpWidget*> parent,
		Fn<bool()> emojiPaused = nullptr);

	void setContent(
		TextWithEntities title,
		TextWithEntities description,
		std::optional<Ui::Text::MarkedContext> context = std::nullopt,
		std::optional<QColor> descriptionColorOverride = std::nullopt);

	void setHideCallback(Fn<void()>);
	void setRightIcon(RightIcon);
	void setRightButton(
		rpl::producer<TextWithEntities> text,
		Fn<void()> callback);
	void setLeadingWidget(Ui::RpWidget *widget);
	void setCollapseProgress(rpl::producer<float64> progress);
	void prepareCollapseSnapshot();

	[[nodiscard]] const style::TextStyle &contentTitleSt() const;

protected:
	void paintEvent(QPaintEvent *) override;
	int resizeGetHeight(int newWidth) override;

private:
	void draw(QPainter &p);
	void releaseCollapseSnapshot();

	const style::TextStyle &_titleSt;
	const style::TextStyle &_contentTitleSt;
	const style::TextStyle &_contentTextSt;

	Ui::Text::String _contentTitle;
	Ui::Text::String _contentText;
	float64 _collapseProgress = 0.;
	QPixmap _collapseSnapshot;
	std::optional<QColor> _descriptionColorOverride;

	Ui::BoxShadow _shadow;

	base::unique_qptr<Ui::IconButton> _rightHide;
	base::unique_qptr<Ui::IconButton> _rightArrow;
	base::unique_qptr<Ui::RoundButton> _rightButton;
	QPointer<Ui::RpWidget> _leadingWidget;
	rpl::lifetime _leadingWidgetLifetime;
	Fn<void()> _hideCallback;
	Fn<bool()> _emojiPaused;

	int _leftPadding = 0;

	RightIcon _rightIcon = RightIcon::None;

	std::shared_ptr<Ui::DynamicImage> _rightPhoto;
	QImage _rightPhotoImage;

};

} // namespace Dialogs
