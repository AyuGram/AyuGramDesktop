// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/mcp/mcp_auth.h"

#include "settings.h"

#include <QtCore/QFile>
#include <QtCore/QRandomGenerator>

namespace AyuMcp {
namespace {

constexpr auto kTokenWords = 4;

QString GlobalToken;

// WHY: not in ayu_settings.json, which users paste into bug reports,
// and leaking the token leaks every chat.
[[nodiscard]] QString tokenPath() {
	return cWorkingDir() + u"tdata/ayu_mcp_token"_q;
}

[[nodiscard]] QString generate() {
	auto bytes = QByteArray();
	for (auto i = 0; i != kTokenWords; ++i) {
		const auto value = QRandomGenerator::system()->generate64();
		bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
	}
	return u"ayu_"_q + QString::fromLatin1(bytes.toBase64(
		QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

void store(const QString &value) {
	auto file = QFile(tokenPath());
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		LOG(("AyuGram MCP: could not write the token file."));
		return;
	}
	file.write(value.toUtf8());
	file.close();
	file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

[[nodiscard]] QString load() {
	auto file = QFile(tokenPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return QString();
	}
	return QString::fromUtf8(file.readAll()).trimmed();
}

[[nodiscard]] bool sameToken(const QByteArray &a, const QByteArray &b) {
	if (a.isEmpty() || a.size() != b.size()) {
		return false;
	}
	auto diff = 0;
	for (auto i = 0; i != a.size(); ++i) {
		diff |= (a[i] ^ b[i]); // constant time, so it can't be guessed bytewise
	}
	return (diff == 0);
}

} // namespace

QString token() {
	if (GlobalToken.isEmpty()) {
		GlobalToken = load();
	}
	if (GlobalToken.isEmpty()) {
		GlobalToken = generate();
		store(GlobalToken);
	}
	return GlobalToken;
}

bool authorized(const QByteArray &authorizationHeader) {
	const auto expected = token().toUtf8();
	const auto prefix = QByteArray("bearer ");
	if (!authorizationHeader.toLower().startsWith(prefix)) {
		return false;
	}
	const auto provided = authorizationHeader.mid(prefix.size()).trimmed();
	return sameToken(expected, provided);
}

bool hostAllowed(const QByteArray &hostHeader, quint16 port) {
	const auto suffix = ':' + QByteArray::number(port);
	const auto name = hostHeader.endsWith(suffix)
		? hostHeader.left(hostHeader.size() - suffix.size())
		: hostHeader;
	return (name == "127.0.0.1")
		|| (name == "localhost")
		|| (name == "[::1]");
}

} // namespace AyuMcp
