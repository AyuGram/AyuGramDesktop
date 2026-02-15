// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "ayu/local_api/ayu_local_api_server.h"

#include <algorithm>
#include <memory>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpSocket>

#include "core/application.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"

#include "logs.h"

namespace {

[[nodiscard]] Main::Session *ResolveSessionByUserId(qint64 userId) {
	if (!Core::App().someSessionExists()) {
		return nullptr;
	}
	if (userId == 0) {
		return &Core::App().domain().active().session();
	}
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (const auto session = account->maybeSession()) {
			if (session->userId().bare == userId) {
				return session;
			}
		}
	}
	return nullptr;
}

[[nodiscard]] PeerData *PeerFromNumericId(
		Main::Session *session,
		qint64 numericPeer) {
	if (!session || numericPeer == 0) {
		return nullptr;
	}

	// Preserve peer type hints from common conventions:
	// - positive: user id
	// - negative: chat id
	// - -100xxxxxxxxxx: channel id (Telethon/Bot API style)
	if (numericPeer > 0) {
		return session->data().userLoaded(UserId(BareId(numericPeer)));
	}

	const auto absValue = qAbs(numericPeer);
	if (absValue >= 1000000000000LL) {
		const auto bare = absValue - 1000000000000LL;
		return session->data().channelLoaded(ChannelId(BareId(bare)));
	}
	return session->data().chatLoaded(ChatId(BareId(absValue)));
}

} // namespace

AyuLocalApiServer::AyuLocalApiServer(QObject *parent)
: QObject(parent) {
	connect(&_server, &QTcpServer::newConnection, this, [=] {
		onNewConnection();
	});
}

bool AyuLocalApiServer::start(int port, const QString &apiKey) {
	_apiKey = apiKey;
	if (_server.isListening()) {
		return true;
	}
	if (port <= 0 || port > 65535) {
		return false;
	}
	return _server.listen(QHostAddress::LocalHost, quint16(port));
}

void AyuLocalApiServer::stop() {
	for (auto i = _connections.begin(); i != _connections.end(); ++i) {
		if (i.key()) {
			i.key()->disconnectFromHost();
		}
	}
	_connections.clear();
	_downloads.clear();
	_server.close();
}

void AyuLocalApiServer::onNewConnection() {
	while (_server.hasPendingConnections()) {
		auto socket = _server.nextPendingConnection();
		if (!socket) {
			continue;
		}
		_connections.insert(socket, ConnectionState());
		connect(socket, &QTcpSocket::readyRead, this, [=] {
			onReadyRead(socket);
		});
		connect(socket, &QTcpSocket::disconnected, this, [=] {
			onDisconnected(socket);
		});
	}
}

void AyuLocalApiServer::onReadyRead(QTcpSocket *socket) {
	if (!socket) {
		return;
	}
	const auto it = _connections.find(socket);
	if (it == _connections.end()) {
		return;
	}
	auto &state = it.value();
	state.buffer.append(socket->readAll());

	ParsedRequest parsed;
	if (!tryParseRequest(socket, state, parsed)) {
		return;
	}

	dispatch(socket, parsed);
}

void AyuLocalApiServer::onDisconnected(QTcpSocket *socket) {
	_connections.remove(socket);
	if (socket) {
		socket->deleteLater();
	}
}

bool AyuLocalApiServer::tryParseRequest(
		QTcpSocket *socket,
		ConnectionState &state,
		ParsedRequest &out) {
	Q_UNUSED(socket);
	const auto headerEnd = state.buffer.indexOf("\r\n\r\n");
	if (headerEnd < 0) {
		return false;
	}
	const auto headerBytes = state.buffer.left(headerEnd);
	const auto headerLines = headerBytes.split('\n');
	if (headerLines.isEmpty()) {
		return false;
	}

	const auto requestLine = QByteArray(headerLines[0]).trimmed();
	const auto parts = requestLine.split(' ');
	if (parts.size() < 2) {
		return false;
	}
	out.method = QString::fromLatin1(parts[0]).trimmed().toUpper();
	const auto rawTarget = parts[1].trimmed();
	// Construct a full URL for parsing.
	const auto fullUrl = QByteArray("http://local") + rawTarget;
	out.url = QUrl::fromEncoded(fullUrl);

	out.headers.clear();
	state.contentLength = 0;
	for (int i = 1; i < headerLines.size(); ++i) {
		const auto line = QByteArray(headerLines[i]).trimmed();
		if (line.isEmpty()) {
			continue;
		}
		const auto colon = line.indexOf(':');
		if (colon <= 0) {
			continue;
		}
		const auto name = QString::fromLatin1(line.left(colon)).trimmed();
		const auto value = QString::fromLatin1(line.mid(colon + 1)).trimmed();
		out.headers.insert(name.toLower(), value);
		if (name.compare("Content-Length", Qt::CaseInsensitive) == 0) {
			state.contentLength = value.toInt();
		}
	}

	const auto bodyStart = headerEnd + 4;
	if (state.contentLength < 0) {
		state.contentLength = 0;
	}
	if (state.buffer.size() < bodyStart + state.contentLength) {
		return false;
	}
	out.body = state.buffer.mid(bodyStart, state.contentLength);

	// Consume this request from the buffer. We close connection anyway.
	state.buffer = state.buffer.mid(bodyStart + state.contentLength);
	return true;
}

void AyuLocalApiServer::dispatch(QTcpSocket *socket, const ParsedRequest &request) {
	if (!socket) {
		return;
	}
	if (!checkApiKey(request.headers)) {
		replyError(socket, 401, "unauthorized");
		return;
	}

	const auto path = request.url.path();
	if (request.method == "GET" && path == "/health") {
		handleHealth(socket);
		return;
	}
	if (request.method == "POST" && path == "/load-messages") {
		handleLoadMessages(socket, request);
		return;
	}
	if (request.method == "POST" && path == "/download") {
		handleDownload(socket, request);
		return;
	}
	if (request.method == "GET" && path == "/progress") {
		handleProgress(socket, request);
		return;
	}

	replyError(socket, 404, "not_found");
}

void AyuLocalApiServer::handleHealth(QTcpSocket *socket) {
	QJsonObject obj;
	obj["ok"] = true;
	obj["listen"] = "127.0.0.1";
	obj["api"] = "AyuLocalApi";
	replyJson(socket, 200, obj);
}

void AyuLocalApiServer::handleLoadMessages(QTcpSocket *socket, const ParsedRequest &request) {
	QJsonParseError err;
	const auto doc = QJsonDocument::fromJson(request.body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		replyError(socket, 400, "bad_json");
		return;
	}
	const auto obj = doc.object();
	const auto sessionUserId = jsonInt64(obj, "session", 0);
	const auto peerNumeric = jsonInt64(obj, "peer", 0);
	const auto msgId = jsonInt64(obj, "msg", 0);
	if (!peerNumeric || !msgId) {
		replyError(socket, 400, "missing_peer_or_msg");
		return;
	}

	auto session = ResolveSessionByUserId(sessionUserId);
	if (!session) {
		replyError(socket, 400, "session_not_found");
		return;
	}

	auto socketPtr = QPointer<QTcpSocket>(socket);
	auto attempts = std::make_shared<int>(0);
	auto timer = new QTimer(this);
	timer->setInterval(50);
	timer->setSingleShot(false);

	// Trigger dialogs load, in case the peer is not loaded yet.
	session->api().requestDialogs();
	session->api().requestMoreDialogsIfNeeded();

	connect(timer, &QTimer::timeout, this, [=] {
		if (!socketPtr) {
			timer->deleteLater();
			return;
		}
		(*attempts)++;
		const auto peer = PeerFromNumericId(session, peerNumeric);
		if (!peer) {
			if (*attempts >= 200) { // ~10s
				timer->stop();
				timer->deleteLater();
				replyError(socketPtr, 404, "peer_not_loaded");
			}
			return;
		}

		timer->stop();
		timer->deleteLater();

		// Request message load into cache.
		session->api().requestMessageData(peer, MsgId(msgId), [=] {
			if (!socketPtr) {
				return;
			}
			const auto item = session->data().message(peer->id, MsgId(msgId));
			QJsonObject out;
			out["success"] = (item != nullptr);
			out["peer"] = QString::number(peerNumeric);
			out["msg"] = QString::number(msgId);
			replyJson(socketPtr, (item ? 200 : 404), out);
		});
	});

	timer->start();
}

void AyuLocalApiServer::handleDownload(QTcpSocket *socket, const ParsedRequest &request) {
	QJsonParseError err;
	const auto doc = QJsonDocument::fromJson(request.body, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject()) {
		replyError(socket, 400, "bad_json");
		return;
	}
	const auto obj = doc.object();
	const auto sessionUserId = jsonInt64(obj, "session", 0);
	const auto peerNumeric = jsonInt64(obj, "peer", 0);
	const auto msgId = jsonInt64(obj, "msg", 0);
	const auto rawPath = jsonString(obj, "path");
	const auto path = normalizePath(rawPath);
	if (!peerNumeric || !msgId || path.isEmpty()) {
		replyError(socket, 400, "missing_fields");
		return;
	}

	auto session = ResolveSessionByUserId(sessionUserId);
	if (!session) {
		replyError(socket, 400, "session_not_found");
		return;
	}

	// Ensure destination directory exists.
	const auto fi = QFileInfo(path);
	if (!fi.absoluteDir().exists()) {
		QDir().mkpath(fi.absolutePath());
	}

	auto socketPtr = QPointer<QTcpSocket>(socket);
	auto attempts = std::make_shared<int>(0);
	auto timer = new QTimer(this);
	timer->setInterval(50);
	timer->setSingleShot(false);

	session->api().requestDialogs();
	session->api().requestMoreDialogsIfNeeded();

	connect(timer, &QTimer::timeout, this, [=] {
		if (!socketPtr) {
			timer->deleteLater();
			return;
		}
		(*attempts)++;
		const auto peer = PeerFromNumericId(session, peerNumeric);
		if (!peer) {
			if (*attempts >= 200) { // ~10s
				timer->stop();
				timer->deleteLater();
				replyError(socketPtr, 404, "peer_not_loaded");
			}
			return;
		}
		timer->stop();
		timer->deleteLater();

		// Ensure message is loaded.
		session->api().requestMessageData(peer, MsgId(msgId), [=] {
			if (!socketPtr) {
				return;
			}
			const auto item = session->data().message(peer->id, MsgId(msgId));
			if (!item) {
				replyError(socketPtr, 404, "message_not_loaded");
				return;
			}
			const auto media = item->media();
			if (!media) {
				replyError(socketPtr, 400, "no_media");
				return;
			}
			const auto document = media->document();
			if (!document) {
				replyError(socketPtr, 400, "no_document");
				return;
			}

			const auto requestId = _nextDownloadId++;
			DownloadState st;
			st.id = requestId;
			st.sessionUserId = sessionUserId;
			st.peer = peerNumeric;
			st.msg = msgId;
			st.path = path;
			st.documentId = document->id;
			st.total = document->size;
			st.created.start();
			_downloads.insert(requestId, st);

			// Start download.
			document->save(Data::FileOrigin(item->fullId()), path, LoadFromCloudOrLocal, false);

			QJsonObject out;
			out["id"] = requestId;
			replyJson(socketPtr, 200, out);
		});
	});

	timer->start();
}

void AyuLocalApiServer::handleProgress(QTcpSocket *socket, const ParsedRequest &request) {
	const auto q = QUrlQuery(request.url);
	const auto id = q.queryItemValue("id").toInt();
	if (id <= 0) {
		replyError(socket, 400, "missing_id");
		return;
	}
	if (!_downloads.contains(id)) {
		replyError(socket, 404, "unknown_id");
		return;
	}
	auto st = _downloads.value(id);

	auto session = ResolveSessionByUserId(st.sessionUserId);
	if (!session) {
		replyError(socket, 400, "session_not_found");
		return;
	}

	qint64 fileSize = 0;
	if (!st.path.isEmpty()) {
		const auto fi = QFileInfo(st.path);
		if (fi.exists()) {
			fileSize = fi.size();
		}
	}

	const auto document = session->data().document(DocumentId(st.documentId));

	qint64 ready = fileSize;
	qint64 total = 0;
	QString state = "unknown";

	if (document) {
		ready = std::max<qint64>(ready, document->loadOffset());
		total = document->size;
		if (document->loading()) {
			state = "downloading";
		} else if (document->status == FileDownloadFailed) {
			state = "failed";
		} else if (fileSize > 0 && (total <= 0 || fileSize >= total)) {
			// Historically the old API returned "unknown" after completion.
			state = "unknown";
			ready = (total > 0) ? total : ready;
		} else {
			state = "unknown";
		}
	} else {
		// No document pointer available, fall back to file stats.
		state = (fileSize > 0) ? "unknown" : "unknown";
		ready = fileSize;
		total = st.total;
	}

	st.ready = ready;
	st.total = total;
	st.state = state;
	_downloads.insert(id, st);

	QJsonObject out;
	out["state"] = state;
	out["ready"] = double(ready);
	out["total"] = double(total);
	replyJson(socket, 200, out);
}

void AyuLocalApiServer::replyJson(QTcpSocket *socket, int code, const QJsonObject &obj) {
	const auto json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
	replyJson(socket, code, json);
}

void AyuLocalApiServer::replyJson(QTcpSocket *socket, int code, const QByteArray &json) {
	if (!socket) {
		return;
	}
	QByteArray status = "200 OK";
	switch (code) {
	case 200: status = "200 OK"; break;
	case 400: status = "400 Bad Request"; break;
	case 401: status = "401 Unauthorized"; break;
	case 404: status = "404 Not Found"; break;
	case 500: status = "500 Internal Server Error"; break;
	default: status = QByteArray::number(code) + " OK"; break;
	}
	QByteArray reply;
	reply.reserve(256 + json.size());
	reply += "HTTP/1.1 ";
	reply += status;
	reply += "\r\n";
	reply += "Content-Type: application/json\r\n";
	reply += "Connection: close\r\n";
	reply += "Content-Length: ";
	reply += QByteArray::number(json.size());
	reply += "\r\n\r\n";
	reply += json;
	socket->write(reply);
	socket->flush();
	socket->disconnectFromHost();
}

void AyuLocalApiServer::replyError(QTcpSocket *socket, int code, const QString &error) {
	QJsonObject obj;
	obj["error"] = error;
	replyJson(socket, code, obj);
}

bool AyuLocalApiServer::checkApiKey(const QHash<QString, QString> &headers) const {
	if (_apiKey.isEmpty()) {
		return true;
	}
	const auto provided = headerValue(headers, "x-api-key");
	return (provided == _apiKey);
}

QString AyuLocalApiServer::headerValue(
		const QHash<QString, QString> &headers,
		const QString &name) const {
	const auto it = headers.find(name.toLower());
	return (it != headers.end()) ? it.value() : QString();
}

qint64 AyuLocalApiServer::jsonInt64(
		const QJsonObject &obj,
		const char *key,
		qint64 def) {
	const auto v = obj.value(QLatin1String(key));
	if (v.isDouble()) {
		return qint64(v.toDouble(def));
	}
	if (v.isString()) {
		bool ok = false;
		const auto n = v.toString().toLongLong(&ok);
		return ok ? n : def;
	}
	return def;
}

QString AyuLocalApiServer::jsonString(const QJsonObject &obj, const char *key) {
	const auto v = obj.value(QLatin1String(key));
	return v.isString() ? v.toString() : QString();
}

qint64 AyuLocalApiServer::normalizePeerId(qint64 peer) {
	if (peer == 0) {
		return 0;
	}
	if (peer > 0) {
		return peer;
	}
	const auto absValue = qAbs(peer);
	// Telethon / Bot API style channel id: -1001234567890
	if (absValue >= 1000000000000LL) {
		return absValue - 1000000000000LL;
	}
	return absValue;
}

QString AyuLocalApiServer::normalizePath(const QString &path) {
	if (path.trimmed().isEmpty()) {
		return QString();
	}
	QFileInfo fi(path);
	if (!fi.isAbsolute()) {
		return QString();
	}
	return QDir::toNativeSeparators(fi.absoluteFilePath());
}
