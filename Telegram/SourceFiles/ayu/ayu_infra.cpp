// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#include "ayu_infra.h"

#include "ayu_lang.h"
#include "ayu_worker.h"
#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "lang/lang_instance.h"

namespace AyuInfra {

void initLang() {
	QString langPackBaseId = Lang::GetInstance().baseId();
	QString langPackId = Lang::GetInstance().id();
	if (langPackId.isEmpty()) {
		LOG(("Lang ID not found! Re-use old language pack..."));
		return;
	}
	CustomLangPack::initInstance();
	CustomLangPack::currentInstance()->fetchCustomLangPack(langPackId, langPackBaseId);
}

void initFonts() {
	auto settings = &AyuSettings::getInstance();

	AyuFonts::setMainFont(settings->mainFont);
	AyuFonts::setMonoFont(settings->monoFont);
}

void initDatabase() {
	AyuDatabase::initialize();
}

void initWorker() {
	AyuWorker::initialize();
}

void init() {
	initLang();
	initDatabase();
	initFonts();
	initWorker();
}

}
