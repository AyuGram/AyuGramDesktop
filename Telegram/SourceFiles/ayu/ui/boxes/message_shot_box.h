// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/features/message_shot/message_shot.h"
#include "ui/layers/box_content.h"

class MessageShotBox : public Ui::BoxContent
{
public:
	MessageShotBox(QWidget *parent, AyuFeatures::MessageShot::ShotConfig config);

	bool tookShot() const {
		return _tookShot;
	}

protected:
	void prepare() override;

private:
	void setupContent();

	AyuFeatures::MessageShot::ShotConfig _config;
	std::shared_ptr<style::palette> _selectedPalette;

	bool _tookShot = false;
};
