// Drakeygram Desktop
// Fully independent fork. No external tracking.

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

	void loadCachedLanguage();
	void saveCachedLanguage(const QByteArray &json, const QString &langId);
	[[nodiscard]] QString getCacheDir() const;
	[[nodiscard]] QString getCachePath(const QString &langId) const;

	QNetworkAccessManager networkManager;
	QNetworkReply *_chkReply = nullptr;
	bool needFallback = false;
	QString _currentLangId;
};
