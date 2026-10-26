// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/mcp/mcp_server.h"

#include "ayu/ayu_settings.h"
#include "ayu/mcp/mcp_auth.h"
#include "ayu/mcp/mcp_protocol.h"
#include "base/flat_map.h"
#include "lang_auto.h"
#include "ui/toast/toast.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtNetwork/QTcpSocket>

namespace AyuMcp {

struct HttpRequest {
	QByteArray method;
	QByteArray path;
	base::flat_map<QByteArray, QByteArray> headers;
	QByteArray body;

	[[nodiscard]] QByteArray header(const QByteArray &name) const {
		const auto i = headers.find(name);
		return (i != end(headers)) ? i->second : QByteArray();
	}
};

namespace {

constexpr auto kPort = quint16(9527);
constexpr auto kMaxHeaderSize = 16 * 1024;
constexpr auto kMaxBodySize = 1024 * 1024;
constexpr auto kReadTimeoutMs = 10000;

constexpr auto kStatusOk = 200;
constexpr auto kStatusAccepted = 202;
constexpr auto kStatusBadRequest = 400;
constexpr auto kStatusUnauthorized = 401;
constexpr auto kStatusForbidden = 403;
constexpr auto kStatusNotFound = 404;
constexpr auto kStatusMethodNotAllowed = 405;
constexpr auto kStatusLengthRequired = 411;
constexpr auto kStatusTooLarge = 413;
constexpr auto kStatusHeadersTooLarge = 431;

std::unique_ptr<Server> GlobalServer;

struct Incoming {
	QByteArray data;
	bool complete = false;
};

struct Parsed {
	std::optional<HttpRequest> request;
	int failure = 0;
};

[[nodiscard]] Parsed parse(const QByteArray &data) {
	const auto separator = QByteArray("\r\n\r\n");
	const auto headerEnd = data.indexOf(separator);
	if (headerEnd < 0 || headerEnd > kMaxHeaderSize) {
		return { .failure = (data.size() > kMaxHeaderSize)
			? kStatusHeadersTooLarge
			: 0 };
	}
	const auto lines = data.left(headerEnd).split('\n');
	const auto requestLine = lines.front().trimmed().split(' ');
	if (requestLine.size() != 3) {
		return { .failure = kStatusBadRequest };
	}
	auto request = HttpRequest{
		.method = requestLine[0],
		.path = requestLine[1].split('?').front(),
	};
	for (auto i = 1; i < lines.size(); ++i) {
		const auto colon = lines[i].indexOf(':');
		if (colon > 0) {
			request.headers.emplace(
				lines[i].left(colon).trimmed().toLower(),
				lines[i].mid(colon + 1).trimmed());
		}
	}
	if (!request.header("transfer-encoding").isEmpty()) {
		return { .failure = kStatusLengthRequired };
	}
	auto valid = true;
	const auto lengthHeader = request.header("content-length");
	const auto length = lengthHeader.isEmpty()
		? 0
		: lengthHeader.toLongLong(&valid);
	if (!valid || length < 0) {
		return { .failure = kStatusBadRequest };
	} else if (length > kMaxBodySize) {
		return { .failure = kStatusTooLarge };
	}
	const auto bodyStart = headerEnd + separator.size();
	if (data.size() < bodyStart + length) {
		return {};
	}
	request.body = data.mid(bodyStart, length);
	return { .request = std::move(request) };
}

[[nodiscard]] QByteArray reason(int status) {
	switch (status) {
	case kStatusOk: return "OK";
	case kStatusAccepted: return "Accepted";
	case kStatusBadRequest: return "Bad Request";
	case kStatusUnauthorized: return "Unauthorized";
	case kStatusForbidden: return "Forbidden";
	case kStatusNotFound: return "Not Found";
	case kStatusMethodNotAllowed: return "Method Not Allowed";
	case kStatusLengthRequired: return "Length Required";
	case kStatusTooLarge: return "Content Too Large";
	case kStatusHeadersTooLarge: return "Request Header Fields Too Large";
	}
	return "Error";
}

void send(
		not_null<QTcpSocket*> socket,
		int status,
		const QByteArray &body = QByteArray(),
		const QByteArray &extraHeaders = QByteArray()) {
	auto head = "HTTP/1.1 "
		+ QByteArray::number(status)
		+ ' '
		+ reason(status)
		+ "\r\n";
	if (!body.isEmpty()) {
		head += "Content-Type: application/json\r\n";
	}
	head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
		+ "Cache-Control: no-store\r\n"
		+ "Connection: close\r\n"
		+ extraHeaders
		+ "\r\n";
	socket->write(head + body);
	socket->disconnectFromHost();
}

} // namespace

Server::Server() {
	connect(&_server, &QTcpServer::newConnection, this, [=] { accept(); });
}

bool Server::start(quint16 port) {
	return _server.listen(QHostAddress::LocalHost, port);
}

void Server::accept() {
	while (const auto socket = _server.nextPendingConnection()) {
		const auto incoming = std::make_shared<Incoming>();
		connect(socket, &QTcpSocket::readyRead, this, [=] {
			if (incoming->complete) {
				return;
			}
			incoming->data += socket->readAll();
			auto parsed = parse(incoming->data);
			if (parsed.failure) {
				incoming->complete = true;
				send(socket, parsed.failure);
			} else if (parsed.request) {
				incoming->complete = true;
				handle(socket, *parsed.request);
			}
		});
		connect(
			socket,
			&QTcpSocket::disconnected,
			socket,
			&QObject::deleteLater);
		QTimer::singleShot(kReadTimeoutMs, socket, [=] {
			if (!incoming->complete) {
				socket->abort();
			}
		});
	}
}

void Server::handle(
		not_null<QTcpSocket*> socket,
		const HttpRequest &request) {
	if (!hostAllowed(request.header("host"), _server.serverPort())
		|| !request.header("origin").isEmpty()) {
		send(socket, kStatusForbidden);
		return;
	} else if (request.path != "/mcp") {
		send(socket, kStatusNotFound);
		return;
	} else if (request.method != "POST") {
		send(socket, kStatusMethodNotAllowed, {}, "Allow: POST\r\n");
		return;
	} else if (!authorized(request.header("authorization"))) {
		send(socket, kStatusUnauthorized, {}, "WWW-Authenticate: Bearer\r\n");
		return;
	}
	const auto weak = QPointer<QTcpSocket>(socket.get());
	handleMessage(request.body, {
		.protocolVersion = request.header("mcp-protocol-version"),
		.method = request.header("mcp-method"),
		.name = request.header("mcp-name"),
	}, [=] {
		return weak && (weak->state() == QAbstractSocket::ConnectedState);
	}, [=](Response response) {
		if (weak) {
			send(weak.data(), response.status, response.body);
		}
	});
}

void applySettings(bool interactive) {
	if (!AyuSettings::getInstance().mcpEnabled()) {
		GlobalServer = nullptr;
		return;
	} else if (GlobalServer) {
		return;
	}
	auto server = std::make_unique<Server>();
	if (server->start(kPort)) {
		GlobalServer = std::move(server);
		return;
	}
	LOG(("AyuGram MCP: could not listen on 127.0.0.1:%1.").arg(kPort));
	if (interactive) {
		Ui::Toast::Show(tr::ayu_McpPortTaken(
			tr::now,
			lt_port,
			QString::number(kPort)));
	}
}

QString clientConfig() {
	const auto server = QJsonObject{
		{ u"type"_q, u"http"_q },
		{ u"url"_q, u"http://127.0.0.1:%1/mcp"_q.arg(kPort) },
		{
			u"headers"_q,
			QJsonObject{ { u"Authorization"_q, u"Bearer "_q + token() } },
		},
	};
	const auto config = QJsonObject{
		{ u"mcpServers"_q, QJsonObject{ { u"ayugram"_q, server } } },
	};
	return QString::fromUtf8(
		QJsonDocument(config).toJson(QJsonDocument::Indented));
}

} // namespace AyuMcp
