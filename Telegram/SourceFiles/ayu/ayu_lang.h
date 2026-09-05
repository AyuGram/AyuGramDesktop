// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <QtNetwork/QNetworkReply>
#include <QtXml/QDomDocument>
#include <QJsonDocument>
#include "rpl/lifetime.h"

class AyuLanguage : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(AyuLanguage)

public:
	static AyuLanguage *currentInstance();
	static void init();
	static AyuLanguage *instance;

	void applyLanguageJson(QJsonDocument doc);

private:
	AyuLanguage();
	~AyuLanguage() override = default;

	void loadCachedLanguage();
	void syncLanguage();
	void fetchLanguage(const QString &id, bool mirror = false);
	void saveCachedLanguage(const QByteArray &json, const QString &langId);
	[[nodiscard]] QString getCacheDir() const;
	[[nodiscard]] QString getCachePath(const QString &langId) const;

	QNetworkAccessManager networkManager;
	QNetworkReply *_chkReply = nullptr;
	QString _currentLangId;
	QString _baseLangId;
	QJsonDocument _document;
	bool _applying = false;
	rpl::lifetime _lifetime;
};
