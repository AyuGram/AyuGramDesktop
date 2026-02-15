// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QtNetwork/QTcpServer>

class QTcpSocket;

class AyuLocalApiServer final : public QObject {
public:
	explicit AyuLocalApiServer(QObject *parent = nullptr);

	[[nodiscard]] bool start(int port, const QString &apiKey);
	void stop();

	[[nodiscard]] static qint64 normalizePeerId(qint64 peer);
	[[nodiscard]] static QString normalizePath(const QString &path);

private:
	struct ConnectionState {
		QByteArray buffer;
		bool parsedHeaders = false;
		int contentLength = 0;
	};

	struct DownloadState {
		int id = 0;
		qint64 sessionUserId = 0; // 0 means active
		qint64 peer = 0;
		qint64 msg = 0;
		QString path;
		quint64 documentId = 0;
		qint64 ready = 0;
		qint64 total = 0;
		QString state = "unknown";
		QString error;
		QElapsedTimer created;
	};

	struct ParsedRequest {
		QString method;
		QUrl url;
		QHash<QString, QString> headers;
		QByteArray body;
	};

	void onNewConnection();
	void onReadyRead(QTcpSocket *socket);
	void onDisconnected(QTcpSocket *socket);

	[[nodiscard]] bool tryParseRequest(
		QTcpSocket *socket,
		ConnectionState &state,
		ParsedRequest &out);

	void dispatch(QTcpSocket *socket, const ParsedRequest &request);

	// Handlers.
	void handleHealth(QTcpSocket *socket);
	void handleLoadMessages(QTcpSocket *socket, const ParsedRequest &request);
	void handleDownload(QTcpSocket *socket, const ParsedRequest &request);
	void handleProgress(QTcpSocket *socket, const ParsedRequest &request);

	// Response helpers.
	void replyJson(QTcpSocket *socket, int code, const QByteArray &json);
	void replyJson(QTcpSocket *socket, int code, const QJsonObject &obj);
	void replyError(QTcpSocket *socket, int code, const QString &error);

	[[nodiscard]] bool checkApiKey(
		const QHash<QString, QString> &headers) const;
	[[nodiscard]] QString headerValue(
		const QHash<QString, QString> &headers,
		const QString &name) const;

	[[nodiscard]] static qint64 jsonInt64(
		const QJsonObject &obj,
		const char *key,
		qint64 def = 0);
	[[nodiscard]] static QString jsonString(
		const QJsonObject &obj,
		const char *key);

	QTcpServer _server;
	QHash<QTcpSocket*, ConnectionState> _connections;
	QHash<int, DownloadState> _downloads;
	int _nextDownloadId = 1;
	QString _apiKey;
};
