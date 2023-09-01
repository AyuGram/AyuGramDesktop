// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "ayu_assets.h"
#include "ayu/ayu_settings.h"

QString LAST_LOADED_NAME;
QImage LAST_LOADED;
QImage LAST_LOADED_NO_MARGIN;

void loadIcons()
{
	auto settings = &AyuSettings::getInstance();
	if (LAST_LOADED_NAME != settings->appIcon)
	{
		LAST_LOADED_NAME = settings->appIcon;
		if (settings->appIcon == AyuSettings::DEFAULT_ICON)
		{
			LAST_LOADED = logo();
			LAST_LOADED_NO_MARGIN = logoNoMargin();
		}
		else if (settings->appIcon == AyuSettings::ALT_ICON)
		{
			LAST_LOADED = logoAlt();
			LAST_LOADED_NO_MARGIN = logoAltNoMargin();
		}
		else if (settings->appIcon == AyuSettings::NOTHING_ICON)
		{
			LAST_LOADED = logoNothing();
			LAST_LOADED_NO_MARGIN = logoNothingNoMargin();
		}
		else
		{
			LAST_LOADED = logo();
			LAST_LOADED_NO_MARGIN = logoNoMargin();
		}
	}
}

QImage logo()
{
	return QImage(qsl(":/gui/art/logo_256.png"));
}

QImage logoNoMargin()
{
	return QImage(qsl(":/gui/art/logo_256_no_margin.png"));
}

QImage logoAlt()
{
	return QImage(qsl(":/gui/art/ayu/alt/logo256.png"));
}

QImage logoAltNoMargin()
{
	return QImage(qsl(":/gui/art/ayu/alt/logo256_no_margin.png"));
}

QImage logoNothing()
{
	return QImage(qsl(":/gui/art/ayu/nothing/logo256.png"));
}

QImage logoNothingNoMargin()
{
	return QImage(qsl(":/gui/art/ayu/nothing/logo256_no_margin.png"));
}

QString currentAppLogoName()
{
	return LAST_LOADED_NAME;
}

QImage currentAppLogo()
{
	loadIcons();
	return LAST_LOADED;
}

QImage currentAppLogoNoMargin()
{
	loadIcons();
	return LAST_LOADED_NO_MARGIN;
}
