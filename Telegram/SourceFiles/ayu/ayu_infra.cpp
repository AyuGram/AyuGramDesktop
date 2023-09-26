// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_infra.h"

#include "ayu_lang.h"
#include "ayu/ayu_lottie.h"
#include "ayu/ui/ayu_lottie.h"
#include "ayu/database/ayu_database.h"
#include "lang/lang_instance.h"
#include "ayu/ayu_settings.h"

namespace AyuInfra
{

void initLang()
{
	QString langPackBaseId = Lang::GetInstance().baseId();
	QString langPackId = Lang::GetInstance().id();
	if (langPackId.isEmpty()) {
		LOG(("Lang ID not found! Re-use old language pack..."));
		return;
	}
	CustomLangPack::initInstance();
	CustomLangPack::currentInstance()->fetchCustomLangPack(langPackId, langPackBaseId);
}

void initLottie()
{
	AyuUi::setLottieImpl(std::make_shared<AyuUi::AyuLottieImpl>());
}

void initDatabase()
{
	AyuDatabase::initialize();
}

void initFonts()
{
	auto ayuSettings = AyuSettings::getInstance();

	AyuFonts::setCommonFont(ayuSettings.commonFont);
	AyuFonts::setMonoFont(ayuSettings.monoFont);
}

void init()
{
	initLang();
	initLottie();
	initFonts();
	initDatabase();
}

}