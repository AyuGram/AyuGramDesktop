// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <QtNetwork/QNetworkReply>
#include <QtXml/QDomDocument>

class AyuLanguage : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(AyuLanguage)

public:
	static AyuLanguage *currentInstance();
	static void init();
	static AyuLanguage *instance;

	void fetchLanguage(const QString &id, const QString &baseId);
	void applyLanguageJson(QJsonDocument doc);

public Q_SLOTS:
	void fetchFinished();
	void fetchError(QNetworkReply::NetworkError e);

private:
	AyuLanguage();
	~AyuLanguage() override = default;

	QNetworkAccessManager networkManager;
	QNetworkReply *_chkReply = nullptr;
	bool needFallback = false;
};
