// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/mcp/mcp_registry.h"

#include "core/application.h"
#include "main/main_account.h"
#include "main/main_domain.h"

#include <QtCore/QJsonDocument>

#include <cmath>

namespace AyuMcp {
namespace {

[[nodiscard]] Main::Session *activeSession() {
	if (!Core::IsAppLaunched() || !Core::App().domain().started()) {
		return nullptr;
	}
	return Core::App().domain().active().maybeSession();
}

[[nodiscard]] std::vector<ToolSpec> &tools() {
	static auto result = std::vector<ToolSpec>();
	return result;
}

[[nodiscard]] const ToolSpec *findTool(const QString &name) {
	const auto &list = tools();
	const auto i = ranges::find(list, name, &ToolSpec::name);
	return (i != end(list) && tierEnabled(i->tier)) ? &*i : nullptr;
}

[[nodiscard]] QJsonObject annotationsJson(const ToolSpec &spec) {
	return QJsonObject{
		{ u"title"_q, spec.title },
		{ u"readOnlyHint"_q, spec.hints.readOnly },
		{ u"destructiveHint"_q, spec.hints.destructive },
		{ u"idempotentHint"_q, spec.hints.idempotent },
		{ u"openWorldHint"_q, spec.hints.openWorld },
	};
}

[[nodiscard]] bool typeMatches(const QJsonValue &value, const QString &type) {
	if (type == u"string"_q) {
		return value.isString();
	} else if (type == u"boolean"_q) {
		return value.isBool();
	} else if (type == u"number"_q) {
		return value.isDouble();
	} else if (type == u"integer"_q) {
		return value.isDouble()
			&& (value.toDouble() == std::floor(value.toDouble()));
	} else if (type == u"object"_q) {
		return value.isObject();
	} else if (type == u"array"_q) {
		return value.isArray();
	}
	return true;
}

[[nodiscard]] QString validateArgument(
		const QString &name,
		const QJsonValue &value,
		const QJsonObject &property) {
	if (property.isEmpty()) {
		return u"Unknown argument '%1'."_q.arg(name);
	}
	const auto type = property.value(u"type"_q).toString();
	if (!typeMatches(value, type)) {
		return u"Argument '%1' must be of type %2."_q.arg(name, type);
	}
	const auto minimum = property.value(u"minimum"_q);
	const auto maximum = property.value(u"maximum"_q);
	if ((!minimum.isUndefined() && value.toDouble() < minimum.toDouble())
		|| (!maximum.isUndefined() && value.toDouble() > maximum.toDouble())) {
		return u"Argument '%1' is out of range."_q.arg(name);
	}
	const auto allowed = property.value(u"enum"_q);
	if (allowed.isArray() && !allowed.toArray().contains(value)) {
		return u"Argument '%1' has an unsupported value."_q.arg(name);
	}
	return QString();
}

[[nodiscard]] QString validate(
		const QJsonObject &schema,
		const QJsonObject &arguments) {
	for (const auto &required : schema.value(u"required"_q).toArray()) {
		if (!arguments.contains(required.toString())) {
			return u"Missing required argument '%1'."_q.arg(
				required.toString());
		}
	}
	const auto properties = schema.value(u"properties"_q).toObject();
	for (const auto &name : arguments.keys()) {
		const auto error = validateArgument(
			name,
			arguments.value(name),
			properties.value(name).toObject());
		if (!error.isEmpty()) {
			return error;
		}
	}
	return QString();
}

} // namespace

ToolResult toolError(const QString &message) {
	return { .text = message, .isError = true };
}

QJsonObject parseSchema(const QByteArray &json) {
	const auto document = QJsonDocument::fromJson(json);
	Expects(document.isObject());

	return document.object();
}

void registerTool(ToolSpec spec) {
	Expects(!ranges::contains(tools(), spec.name, &ToolSpec::name));

	tools().push_back(std::move(spec));
}

bool tierEnabled(Tier tier) {
	return (tier == Tier::Read);
}

QJsonArray listTools() {
	auto result = QJsonArray();
	for (const auto &spec : tools()) {
		if (!tierEnabled(spec.tier)) {
			continue;
		}
		auto tool = QJsonObject{
			{ u"name"_q, spec.name },
			{ u"title"_q, spec.title },
			{ u"description"_q, spec.description },
			{ u"inputSchema"_q, spec.inputSchema },
			{ u"annotations"_q, annotationsJson(spec) },
		};
		if (!spec.outputSchema.isEmpty()) {
			tool.insert(u"outputSchema"_q, spec.outputSchema);
		}
		result.append(tool);
	}
	return result;
}

bool hasTool(const QString &name) {
	return (findTool(name) != nullptr);
}

void callTool(
		const QString &name,
		const QJsonObject &arguments,
		Fn<bool()> alive,
		Fn<void(ToolResult)> done) {
	const auto spec = findTool(name);
	if (!spec) {
		done(toolError(u"Unknown tool '%1'."_q.arg(name)));
		return;
	}
	const auto session = activeSession();
	if (!session) {
		done(toolError(u"No Telegram account is logged in."_q));
		return;
	}
	const auto error = validate(spec->inputSchema, arguments);
	if (!error.isEmpty()) {
		done(toolError(error));
		return;
	}
	spec->handler(
		{ .session = session, .alive = std::move(alive) },
		arguments,
		std::move(done));
}

} // namespace AyuMcp
