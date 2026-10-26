// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>

namespace Main {
class Session;
} // namespace Main

namespace AyuMcp {

enum class Tier {
	Read,
	Network,
	Write,
};

struct ToolResult {
	QString text;
	QJsonValue structured;
	bool isError = false;
};

[[nodiscard]] ToolResult toolError(const QString &message);
[[nodiscard]] QJsonObject parseSchema(const QByteArray &json);

struct CallContext {
	not_null<Main::Session*> session;
	Fn<bool()> alive;
};

struct ToolHints {
	bool readOnly = true;
	bool destructive = false;
	bool idempotent = true;
	bool openWorld = false;
};

using ToolHandler = Fn<void(
	const CallContext &context,
	const QJsonObject &arguments,
	Fn<void(ToolResult)> done)>;

struct ToolSpec {
	QString name;
	QString title;
	QString description;
	QJsonObject inputSchema;
	QJsonObject outputSchema;
	ToolHints hints;
	Tier tier = Tier::Read;
	ToolHandler handler;
};

void registerTool(ToolSpec spec);

[[nodiscard]] bool tierEnabled(Tier tier);
[[nodiscard]] QJsonArray listTools();
[[nodiscard]] bool hasTool(const QString &name);

void callTool(
	const QString &name,
	const QJsonObject &arguments,
	Fn<bool()> alive,
	Fn<void(ToolResult)> done);

} // namespace AyuMcp
