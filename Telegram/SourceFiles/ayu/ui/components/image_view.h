// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#pragma once


#include "ui/rp_widget.h"
#include "ui/effects/animations.h"

class ImageView : public Ui::RpWidget
{
public:
	ImageView(QWidget *parent);

	void setImage(const QImage& image);
	QImage getImage() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

private:
	QImage image;
	QImage prevImage;

	Ui::Animations::Simple animation;

};
