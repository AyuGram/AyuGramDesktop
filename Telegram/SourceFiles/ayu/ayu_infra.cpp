// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2024
#include "ayu_infra.h"

#include "ayu/ayu_lang.h"
#include "ayu/ayu_worker.h"
#include "ayu/ayu_fonts.h"
#include "ayu/ayu_settings.h"
#include "ayu/data/ayu_database.h"
#include "lang/lang_instance.h"
#include "utils/taptic_engine/taptic_engine.h"

namespace AyuInfra {

void initLang() {
	QString id = Lang::GetInstance().id();
	QString baseId = Lang::GetInstance().baseId();
	if (id.isEmpty()) {
		LOG(("Language ID not found!"));
		return;
	}
	AyuLanguage::init();
	AyuLanguage::currentInstance()->fetchLanguage(id, baseId);
}

void initFonts() {
	auto settings = &AyuSettings::getInstance();

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

	TapticEngine::init();
}

}
