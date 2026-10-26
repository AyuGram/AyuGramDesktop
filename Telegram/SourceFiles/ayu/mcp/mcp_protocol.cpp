// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/mcp/mcp_protocol.h"

#include "ayu/mcp/mcp_registry.h"
#include "base/flat_map.h"
#include "core/version.h"

#include <QtCore/QJsonDocument>

namespace AyuMcp {
namespace {

constexpr auto kModernVersion = "2026-07-28";
constexpr auto kLegacyVersions = std::array{
	"2025-11-25",
	"2025-06-18",
	"2025-03-26",
};

constexpr auto kParseError = -32700;
constexpr auto kInvalidRequest = -32600;
constexpr auto kMethodNotFound = -32601;
constexpr auto kInvalidParams = -32602;
constexpr auto kHeaderMismatch = -32020;
constexpr auto kUnsupportedVersion = -32022;

constexpr auto kStatusOk = 200;
constexpr auto kStatusAccepted = 202;
constexpr auto kStatusBadRequest = 400;
constexpr auto kStatusNotFound = 404;

struct Call {
	QJsonValue id;
	QString method;
	QJsonObject params;
	bool modern = false;
};

struct Error {
	int code = 0;
	QString message;
	QJsonValue data;
	int status = kStatusOk;
};

using Outcome = std::variant<QJsonObject, Error>;
using Handler = Fn<void(const Call &call, Fn<void(Outcome)> done)>;

struct Method {
	Handler handler;
	bool legacyOnly = false;
};

[[nodiscard]] QString versionKey() {
	return u"io.modelcontextprotocol/protocolVersion"_q;
}

[[nodiscard]] QJsonArray supportedVersions() {
	auto result = QJsonArray{ QString::fromLatin1(kModernVersion) };
	for (const auto version : kLegacyVersions) {
		result.append(QString::fromLatin1(version));
	}
	return result;
}

[[nodiscard]] QJsonObject serverInfo() {
	return QJsonObject{
		{ u"name"_q, u"ayugram"_q },
		{ u"title"_q, u"AyuGram Desktop"_q },
		{ u"version"_q, QString::fromLatin1(AppVersionStr) },
	};
}

[[nodiscard]] QJsonObject capabilities() {
	return QJsonObject{
		{ u"tools"_q, QJsonObject{ { u"listChanged"_q, false } } },
	};
}

[[nodiscard]] QString instructions() {
	return u"Reads chats of the active AyuGram account. Only data "
		"already stored by this client is available."_q;
}

[[nodiscard]] QByteArray serialize(const QJsonObject &object) {
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

[[nodiscard]] Response errorResponse(const QJsonValue &id, const Error &e) {
	auto error = QJsonObject{
		{ u"code"_q, e.code },
		{ u"message"_q, e.message },
	};
	if (!e.data.isNull() && !e.data.isUndefined()) {
		error.insert(u"data"_q, e.data);
	}
	return {
		.status = e.status,
		.body = serialize({
			{ u"jsonrpc"_q, u"2.0"_q },
			{ u"id"_q, id },
			{ u"error"_q, error },
		}),
	};
}

[[nodiscard]] Response resultResponse(const Call &call, QJsonObject result) {
	result.insert(u"resultType"_q, u"complete"_q);
	if (call.modern) {
		auto meta = result.value(u"_meta"_q).toObject();
		meta.insert(u"io.modelcontextprotocol/serverInfo"_q, serverInfo());
		result.insert(u"_meta"_q, meta);
	}
	return {
		.body = serialize({
			{ u"jsonrpc"_q, u"2.0"_q },
			{ u"id"_q, call.id },
			{ u"result"_q, result },
		}),
	};
}

[[nodiscard]] QString decodeHeader(const QByteArray &value) {
	const auto prefix = QByteArray("=?base64?");
	const auto suffix = QByteArray("?=");
	if (value.startsWith(prefix) && value.endsWith(suffix)) {
		const auto encoded = value.mid(
			prefix.size(),
			value.size() - prefix.size() - suffix.size());
		return QString::fromUtf8(QByteArray::fromBase64(encoded));
	}
	return QString::fromLatin1(value);
}

[[nodiscard]] std::optional<Error> checkModern(
		const Call &call,
		const RequestHeaders &headers) {
	const auto version = call.params.value(u"_meta"_q).toObject().value(
		versionKey()).toString();
	if (version != QString::fromLatin1(kModernVersion)) {
		return Error{
			.code = kUnsupportedVersion,
			.message = u"Unsupported protocol version."_q,
			.data = QJsonObject{
				{ u"supported"_q, supportedVersions() },
				{ u"requested"_q, version },
			},
			.status = kStatusBadRequest,
		};
	}
	const auto mismatch = [](const QString &header) {
		return Error{
			.code = kHeaderMismatch,
			.message = u"Header %1 does not match the body."_q.arg(header),
			.status = kStatusBadRequest,
		};
	};
	if (QString::fromLatin1(headers.protocolVersion) != version) {
		return mismatch(u"MCP-Protocol-Version"_q);
	} else if (QString::fromLatin1(headers.method) != call.method) {
		return mismatch(u"Mcp-Method"_q);
	} else if (call.method == u"tools/call"_q
		&& decodeHeader(headers.name)
			!= call.params.value(u"name"_q).toString()) {
		return mismatch(u"Mcp-Name"_q);
	}
	return std::nullopt;
}

void initialize(const Call &call, Fn<void(Outcome)> done) {
	const auto requested = call.params.value(
		u"protocolVersion"_q).toString().toLatin1();
	const auto known = ranges::contains(kLegacyVersions, requested);
	done(QJsonObject{
		{
			u"protocolVersion"_q,
			(known
				? QString::fromLatin1(requested)
				: QString::fromLatin1(kLegacyVersions[0])),
		},
		{ u"capabilities"_q, capabilities() },
		{ u"serverInfo"_q, serverInfo() },
		{ u"instructions"_q, instructions() },
	});
}

void ping(const Call &, Fn<void(Outcome)> done) {
	done(QJsonObject());
}

void discover(const Call &, Fn<void(Outcome)> done) {
	done(QJsonObject{
		{ u"supportedVersions"_q, supportedVersions() },
		{ u"capabilities"_q, capabilities() },
		{ u"instructions"_q, instructions() },
		{ u"ttlMs"_q, 0 },
		{ u"cacheScope"_q, u"private"_q },
	});
}

void toolsList(const Call &, Fn<void(Outcome)> done) {
	done(QJsonObject{
		{ u"tools"_q, listTools() },
		{ u"ttlMs"_q, 0 },
		{ u"cacheScope"_q, u"private"_q },
	});
}

[[nodiscard]] QJsonObject toolResultJson(const ToolResult &result) {
	auto json = QJsonObject{
		{
			u"content"_q,
			QJsonArray{ QJsonObject{
				{ u"type"_q, u"text"_q },
				{ u"text"_q, result.text },
			} },
		},
		{ u"isError"_q, result.isError },
	};
	if (!result.structured.isUndefined() && !result.structured.isNull()) {
		json.insert(u"structuredContent"_q, result.structured);
	}
	return json;
}

void toolsCall(const Call &call, Fn<void(Outcome)> done, Fn<bool()> alive) {
	const auto name = call.params.value(u"name"_q).toString();
	if (!hasTool(name)) {
		done(Error{
			.code = kInvalidParams,
			.message = u"Unknown tool: %1"_q.arg(name),
		});
		return;
	}
	callTool(
		name,
		call.params.value(u"arguments"_q).toObject(),
		std::move(alive),
		[=](ToolResult result) { done(toolResultJson(result)); });
}

[[nodiscard]] base::flat_map<QString, Method> methods(Fn<bool()> alive) {
	auto result = base::flat_map<QString, Method>();
	result.emplace(u"initialize"_q, Method{ initialize, true });
	result.emplace(u"ping"_q, Method{ ping, true });
	result.emplace(u"server/discover"_q, Method{ discover });
	result.emplace(u"tools/list"_q, Method{ toolsList });
	result.emplace(u"tools/call"_q, Method{ [=](
			const Call &call,
			Fn<void(Outcome)> done) {
		toolsCall(call, std::move(done), alive);
	} });
	return result;
}

} // namespace

void handleMessage(
		const QByteArray &body,
		const RequestHeaders &headers,
		Fn<bool()> alive,
		Fn<void(Response)> done) {
	const auto null = QJsonValue(QJsonValue::Null);
	auto parseError = QJsonParseError();
	const auto document = QJsonDocument::fromJson(body, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		done(errorResponse(null, {
			.code = kParseError,
			.message = u"Parse error."_q,
			.status = kStatusBadRequest,
		}));
		return;
	} else if (!document.isObject()) {
		done(errorResponse(null, {
			.code = kInvalidRequest,
			.message = u"Expected a single JSON-RPC message."_q,
			.status = kStatusBadRequest,
		}));
		return;
	}
	const auto message = document.object();
	if (!message.contains(u"method"_q) || !message.contains(u"id"_q)) {
		done({ .status = kStatusAccepted });
		return;
	}
	auto call = Call{
		.id = message.value(u"id"_q),
		.method = message.value(u"method"_q).toString(),
		.params = message.value(u"params"_q).toObject(),
	};
	call.modern = call.params.value(u"_meta"_q).toObject().contains(
		versionKey());
	if (call.modern) {
		if (const auto error = checkModern(call, headers)) {
			done(errorResponse(call.id, *error));
			return;
		}
	}
	const auto table = methods(std::move(alive));
	const auto i = table.find(call.method);
	if (i == end(table) || (call.modern && i->second.legacyOnly)) {
		done(errorResponse(call.id, {
			.code = kMethodNotFound,
			.message = u"Method not found: %1"_q.arg(call.method),
			.status = call.modern ? kStatusNotFound : kStatusOk,
		}));
		return;
	}
	i->second.handler(call, [=](Outcome outcome) {
		if (const auto error = std::get_if<Error>(&outcome)) {
			done(errorResponse(call.id, *error));
		} else {
			done(resultResponse(call, std::get<QJsonObject>(outcome)));
		}
	});
}

} // namespace AyuMcp
