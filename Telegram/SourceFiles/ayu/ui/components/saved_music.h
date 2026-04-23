// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ui/widgets/buttons.h"

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace Data {
class DocumentMedia;
}

namespace Info::Profile {

struct MusicButtonData;

struct ResultCover
{
	QPixmap pix;
	QColor bg;
	bool noCover;
};

class AyuMusicButton final : public Ui::RippleButton
{
public:
	AyuMusicButton(QWidget *parent, MusicButtonData data, std::optional<QColor> overrideBg, Fn<void()> handler);
	~AyuMusicButton();

	void updateData(MusicButtonData data);

	rpl::producer<> onReady() const {
		return _onReady.events();
	}

private:
	void downloadAndMakeCover(FullMsgId msgId);
	void makeCover();

	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;

	std::unique_ptr<Ui::FlatLabel> _performer;
	std::unique_ptr<Ui::FlatLabel> _title;
	std::shared_ptr<Data::DocumentMedia> _mediaView;
	std::optional<ResultCover> _currentCover;
	rpl::event_stream<> _onReady;

	QString _performerText;
	QString _titleText;

	std::optional<QColor> _overrideBg;

};

} // namespace Info::Profile
