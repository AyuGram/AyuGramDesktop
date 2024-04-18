// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#pragma once

#include "ui/rp_widget.h"
#include "ui/effects/animations.h"

class IconPicker : public Ui::RpWidget
{
public:
	IconPicker(QWidget *parent);

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

	Ui::Animations::Simple animation;
	QString wasSelected;
};
