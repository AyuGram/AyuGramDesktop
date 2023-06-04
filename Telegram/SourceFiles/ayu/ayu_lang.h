#pragma once

#include <QtNetwork/QNetworkReply>
#include <QtXml/QDomDocument>

class CustomLangPack : public QObject {
Q_OBJECT
    Q_DISABLE_COPY(CustomLangPack)

public:
    static CustomLangPack *currentInstance();
    static void initInstance();
    static CustomLangPack *instance;

    void fetchCustomLangPack(const QString& langPackId, const QString& langPackBaseId);
    void loadDefaultLangFile();
    void parseLangFile(QJsonDocument str);

public Q_SLOTS:
    void fetchFinished();
    void fetchError(QNetworkReply::NetworkError e);

private:
    CustomLangPack();
    ~CustomLangPack() = default;

    QNetworkAccessManager networkManager;
    QNetworkReply *_chkReply = nullptr;
    bool needFallback = false;
};