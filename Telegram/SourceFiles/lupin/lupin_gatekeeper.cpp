#include "lupin/lupin_gatekeeper.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMessageAuthenticationCode>
#include <QtCore/QRandomGenerator>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtCore/QTimer>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#endif // Q_OS_WIN

namespace LupinGate {
namespace {

#ifdef Q_OS_WIN

// WHY: neither value should sit in the binary as a readable string;
// each is XOR-masked with its own key and assembled only when needed.
constexpr unsigned char kUrlKey[] = { 222, 234, 94, 28, 109, 211, 163, 103, 231, 17, 130, 179, 172, 90, 228, 104, 104, 206, 189, 205, 80, 170, 5, 214, 91, 153, 31, 118, 64, 246, 44, 204, 219, 81, 174, 177, 236, 235, 155, 118, 155, 68, 206, 52, 141, 18, 164, 135, 249, 159, 249, 148, 199, 12, 19, 105, 247, 168, 231, 159, 14, 173, 5, 163, 51, 51, 27, 73, 51, 116, 192, 235, 205, 146, 81, 4, 204, 240, 157, 86, 111, 147, 203, 59, 141, 12, 199, 251, 143, 186, 166, 79, 218, 229, 55, 157, 250, 47, 136, 135, 136, 48, 199, 170, 13, 192, 127, 181, 189, 150, 117, 250, 231, 7, 207, 134, 125, 139, 193, 168, 187, 239, 91, 189, 143, 205, 146, 166, 66, 68, 18, 182, 34, 243, 16, 232 };
constexpr unsigned char kUrlData[] = { 182, 234, 42, 28, 25, 211, 211, 103, 148, 17, 184, 179, 131, 90, 203, 104, 1, 206, 208, 205, 62, 170, 108, 214, 106, 153, 114, 118, 116, 246, 67, 204, 246, 81, 222, 177, 149, 235, 239, 118, 243, 68, 161, 52, 227, 18, 137, 135, 149, 159, 140, 148, 183, 12, 122, 105, 153, 168, 128, 159, 124, 173, 100, 163, 94, 51, 53, 73, 82, 116, 176, 235, 189, 146, 34, 4, 225, 240, 175, 86, 65, 147, 188, 59, 232, 12, 165, 251, 252, 186, 213, 79, 181, 229, 89, 157, 212, 47, 235, 135, 228, 48, 168, 170, 120, 192, 27, 181, 146, 150, 20, 250, 151, 7, 191, 134, 82, 139, 183, 168, 138, 239, 116, 189, 249, 205, 247, 166, 48, 68, 123, 182, 68, 243, 105, 232 };
constexpr unsigned char kSecretKey[] = { 159, 237, 137, 182, 94, 147, 150, 188, 43, 199, 212, 86, 99, 163, 203, 93, 236, 255, 79, 101, 255, 179, 53, 195, 169 };
constexpr unsigned char kSecretData[] = { 211, 184, 217, 255, 16, 190, 197, 245, 108, 137, 249, 111, 83, 155, 252, 112, 175, 183, 14, 43, 184, 246, 24, 142, 236 };

template <typename Char>
[[nodiscard]] std::basic_string<Char> Unmask(
		const unsigned char *key,
		const unsigned char *data,
		size_t count) {
	auto result = std::basic_string<Char>(count / sizeof(Char), Char(0));
	auto bytes = reinterpret_cast<unsigned char*>(result.data());
	for (auto i = size_t(0); i != count; ++i) {
		bytes[i] = data[i] ^ key[i];
	}
	return result;
}

[[nodiscard]] const std::wstring &AuthServerUrl() {
	static const auto url = Unmask<wchar_t>(kUrlKey, kUrlData, sizeof(kUrlData));
	return url;
}

[[nodiscard]] const std::string &ResponseSecret() {
	static const auto secret = Unmask<char>(kSecretKey, kSecretData, sizeof(kSecretData));
	return secret;
}
constexpr auto kCacheSalt = "LupinGram.auth.v1";
constexpr auto kOfflineGraceSecs = qint64(7 * 24 * 60 * 60);
constexpr auto kMaxResponseSize = 64 * 1024;
constexpr auto kPollInterval = 100;

enum class Result {
	Ok,
	Denied,
	NetworkError,
	BadResponse,
};

struct Reply {
	Result result = Result::NetworkError;
	QString staff;
};

struct Cache {
	QString key;
	QString staff;
	qint64 timestamp = 0;

	[[nodiscard]] bool usableOffline() const {
		const auto age = QDateTime::currentSecsSinceEpoch() - timestamp;
		return !key.isEmpty()
			&& !staff.isEmpty()
			&& age >= 0
			&& age <= kOfflineGraceSecs;
	}
};

struct Pending {
	std::mutex mutex;
	bool done = false;
	Reply reply;
};

QString GlobalStaff;

[[nodiscard]] QString AuthPath() {
	return qEnvironmentVariable("APPDATA") + u"/LupinGram/auth.dat"_q;
}

[[nodiscard]] QByteArray Dpapi(const QByteArray &input, bool protect) {
	auto in = DATA_BLOB{
		DWORD(input.size()),
		reinterpret_cast<BYTE*>(const_cast<char*>(input.constData())),
	};
	auto entropy = DATA_BLOB{
		DWORD(qstrlen(kCacheSalt)),
		reinterpret_cast<BYTE*>(const_cast<char*>(kCacheSalt)),
	};
	auto out = DATA_BLOB();
	const auto ok = protect
		? CryptProtectData(
			&in,
			L"LupinGram",
			&entropy,
			nullptr,
			nullptr,
			CRYPTPROTECT_UI_FORBIDDEN,
			&out)
		: CryptUnprotectData(
			&in,
			nullptr,
			&entropy,
			nullptr,
			nullptr,
			CRYPTPROTECT_UI_FORBIDDEN,
			&out);
	if (!ok) {
		return {};
	}
	auto result = QByteArray(
		reinterpret_cast<const char*>(out.pbData),
		int(out.cbData));
	LocalFree(out.pbData);
	return result;
}

[[nodiscard]] QByteArray CacheSignature(
		const QString &key,
		const QString &staff,
		qint64 timestamp,
		const QString &hwid) {
	const auto raw = QString::fromLatin1(kCacheSalt)
		+ u"|"_q + key
		+ u"|"_q + staff
		+ u"|"_q + QString::number(timestamp)
		+ u"|"_q + hwid;
	return QCryptographicHash::hash(
		raw.toUtf8(),
		QCryptographicHash::Sha256).toHex();
}

[[nodiscard]] Cache LoadCache(const QString &hwid) {
	auto file = QFile(AuthPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	const auto plain = Dpapi(file.readAll(), false);
	const auto object = QJsonDocument::fromJson(plain).object();
	auto result = Cache{
		.key = object.value(u"key"_q).toString(),
		.staff = object.value(u"staff"_q).toString(),
		.timestamp = qint64(object.value(u"ts"_q).toDouble()),
	};
	const auto signature = object.value(u"sig"_q).toString().toLatin1();
	if (signature != CacheSignature(
			result.key,
			result.staff,
			result.timestamp,
			hwid)) {
		return {};
	}
	return result;
}

void RemoveCache() {
	QFile::remove(AuthPath());
}

void SaveCache(const QString &key, const QString &staff, const QString &hwid) {
	const auto timestamp = QDateTime::currentSecsSinceEpoch();
	const auto json = QJsonDocument(QJsonObject{
		{ u"key"_q, key },
		{ u"staff"_q, staff },
		{ u"ts"_q, double(timestamp) },
		{ u"sig"_q, QString::fromLatin1(
			CacheSignature(key, staff, timestamp, hwid)) },
	}).toJson(QJsonDocument::Compact);
	const auto protectedData = Dpapi(json, true);
	if (protectedData.isEmpty()) {
		return;
	}
	QDir().mkpath(QFileInfo(AuthPath()).absolutePath());
	auto file = QSaveFile(AuthPath());
	if (file.open(QIODevice::WriteOnly)) {
		file.write(protectedData);
		file.commit();
	}
}

[[nodiscard]] QString ComputeHwid() {
	auto guid = std::wstring(128, L'\0');
	auto size = DWORD(guid.size() * sizeof(wchar_t));
	const auto status = RegGetValueW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Cryptography",
		L"MachineGuid",
		RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
		nullptr,
		guid.data(),
		&size);
	if (status != ERROR_SUCCESS || size < sizeof(wchar_t)) {
		return {};
	}
	guid.resize(size / sizeof(wchar_t) - 1);
	auto serial = DWORD(0);
	if (!GetVolumeInformationW(
			L"C:\\",
			nullptr,
			0,
			&serial,
			nullptr,
			nullptr,
			nullptr,
			0)) {
		serial = 0;
	}
	const auto raw = QString::fromStdWString(guid)
		+ u"|"_q
		+ QString::number(serial, 16);
	return QString::fromLatin1(QCryptographicHash::hash(
		raw.toUtf8(),
		QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] int Post(const QByteArray &body, QByteArray &response) {
	auto host = std::wstring(256, L'\0');
	auto path = std::wstring(512, L'\0');
	auto parts = URL_COMPONENTS();
	parts.dwStructSize = sizeof(parts);
	parts.lpszHostName = host.data();
	parts.dwHostNameLength = DWORD(host.size());
	parts.lpszUrlPath = path.data();
	parts.dwUrlPathLength = DWORD(path.size());
	if (!WinHttpCrackUrl(AuthServerUrl().c_str(), 0, 0, &parts)) {
		return 0;
	}
	const auto session = WinHttpOpen(
		L"LupinGram",
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);
	if (!session) {
		return 0;
	}
	WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);
	auto code = DWORD(0);
	const auto connect = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
	const auto request = connect ? WinHttpOpenRequest(
		connect,
		L"POST",
		path.c_str(),
		nullptr,
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		(parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0)
		: nullptr;
	if (request
		&& WinHttpSendRequest(
			request,
			L"Content-Type: application/json\r\n",
			DWORD(-1),
			const_cast<char*>(body.constData()),
			DWORD(body.size()),
			DWORD(body.size()),
			0)
		&& WinHttpReceiveResponse(request, nullptr)) {
		auto size = DWORD(sizeof(code));
		if (!WinHttpQueryHeaders(
				request,
				WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX,
				&code,
				&size,
				WINHTTP_NO_HEADER_INDEX)) {
			code = 0;
		}
		auto available = DWORD(0);
		while (WinHttpQueryDataAvailable(request, &available)
			&& available
			&& response.size() < kMaxResponseSize) {
			auto chunk = QByteArray(int(available), Qt::Uninitialized);
			auto read = DWORD(0);
			if (!WinHttpReadData(request, chunk.data(), available, &read)
				|| !read) {
				break;
			}
			response.append(chunk.constData(), int(read));
		}
	}
	if (request) {
		WinHttpCloseHandle(request);
	}
	if (connect) {
		WinHttpCloseHandle(connect);
	}
	WinHttpCloseHandle(session);
	return int(code);
}

[[nodiscard]] Reply Verify(const QString &key, const QString &hwid) {
	const auto nonce = QString::number(
		QRandomGenerator::system()->generate64(),
		16);
	const auto body = QJsonDocument(QJsonObject{
		{ u"invite_key"_q, key },
		{ u"hwid"_q, hwid },
		{ u"nonce"_q, nonce },
	}).toJson(QJsonDocument::Compact);
	auto response = QByteArray();
	if (Post(body, response) != 200) {
		return { Result::NetworkError };
	}
	const auto document = QJsonDocument::fromJson(response);
	if (!document.isObject()) {
		return { Result::NetworkError };
	}
	const auto object = document.object();
	const auto status = object.value(u"status"_q).toString();
	if (status == u"banned"_q
		|| status == u"invalid"_q
		|| status == u"expired"_q) {
		return { Result::Denied };
	} else if (status != u"ok"_q) {
		return { Result::BadResponse };
	}
	const auto staff = object.value(u"issued_by"_q).toString();
	const auto signedText = u"ok|"_q + staff + u"|"_q + key + u"|"_q + hwid
		+ u"|"_q + nonce;
	const auto expected = QMessageAuthenticationCode::hash(
		signedText.toUtf8(),
		QByteArray::fromStdString(ResponseSecret()),
		QCryptographicHash::Sha256).toHex();
	if (staff.isEmpty()
		|| object.value(u"nonce"_q).toString() != nonce
		|| object.value(u"sig"_q).toString().toLatin1() != expected) {
		return { Result::BadResponse };
	}
	return { Result::Ok, staff };
}

[[nodiscard]] bool ShowDialog(const Cache &cache, const QString &hwid) {
	auto dialog = QDialog();
	dialog.setWindowTitle(u"LUPINGRAM // ACCESS RESTRICTED"_q);
	dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
	dialog.setStyleSheet(
		u"QDialog{background:#050805;}"
		"QLabel{color:#00FF41;font-family:Consolas;font-size:1.1em;}"
		"QLineEdit{background:#0A0F0A;color:#00FF41;border:1px solid #003B00;"
		"padding:0.5em;font-family:Consolas;font-size:1.2em;}"
		"QLineEdit:disabled{color:#008F11;}"
		"QPushButton{background:#00CC33;color:#050805;border:0;padding:0.7em;"
		"font-family:Consolas;font-weight:bold;}"
		"QPushButton:hover{background:#00FF41;}"
		"QPushButton:disabled{background:#003B00;color:#008F11;}"_q);
	dialog.setMinimumWidth(dialog.fontMetrics().horizontalAdvance(
		QString(44, QChar('M'))));

	auto layout = new QVBoxLayout(&dialog);
	auto title = new QLabel(u"LUPINGRAM // ACCESS RESTRICTED"_q, &dialog);
	auto input = new QLineEdit(&dialog);
	input->setPlaceholderText(u"LPN-XXXX-XXXX"_q);
	auto status = new QLabel(&dialog);
	auto button = new QPushButton(u"VERIFY ACCESS"_q, &dialog);
	layout->addWidget(title);
	layout->addWidget(input);
	layout->addWidget(status);
	layout->addWidget(button);

	auto pending = std::shared_ptr<Pending>();
	auto pendingKey = QString();
	auto pendingFromCache = false;
	auto failures = 0;
	auto lockedUntil = qint64(0);

	const auto refresh = [&] {
		const auto busy = (pending != nullptr);
		const auto left = lockedUntil - QDateTime::currentSecsSinceEpoch();
		input->setEnabled(!busy);
		button->setEnabled(!busy && left <= 0);
		if (busy) {
			button->setText(u"CHECKING…"_q);
		} else if (left > 0) {
			button->setText(u"WAIT %1 s"_q.arg(left));
		} else {
			button->setText(u"VERIFY ACCESS"_q);
		}
	};
	const auto start = [&](const QString &key, bool fromCache) {
		pendingKey = key;
		pendingFromCache = fromCache;
		pending = std::make_shared<Pending>();
		status->clear();
		refresh();
		std::thread([state = pending, key, hwid] {
			auto reply = Verify(key, hwid);
			const auto lock = std::scoped_lock(state->mutex);
			state->reply = std::move(reply);
			state->done = true;
		}).detach();
	};
	const auto handle = [&](const Reply &reply) {
		switch (reply.result) {
		case Result::Ok:
			GlobalStaff = reply.staff;
			SaveCache(pendingKey, reply.staff, hwid);
			dialog.accept();
			return;
		case Result::Denied:
			if (pendingFromCache) {
				RemoveCache();
			}
			status->setText(u"ACCESS DENIED"_q);
			++failures;
			break;
		case Result::NetworkError:
			if (pendingFromCache && cache.usableOffline()) {
				GlobalStaff = cache.staff;
				dialog.accept();
				return;
			}
			status->setText(u"NETWORK ERROR"_q);
			break;
		case Result::BadResponse:
			status->setText(u"BAD SERVER RESPONSE"_q);
			break;
		}
		lockedUntil = (failures > 0)
			? QDateTime::currentSecsSinceEpoch() + std::min(failures * 3, 30)
			: 0;
	};

	auto timer = new QTimer(&dialog);
	timer->setInterval(kPollInterval);
	QObject::connect(timer, &QTimer::timeout, [&] {
		if (pending) {
			auto reply = std::optional<Reply>();
			{
				const auto lock = std::scoped_lock(pending->mutex);
				if (pending->done) {
					reply = pending->reply;
				}
			}
			if (reply) {
				pending = nullptr;
				handle(*reply);
			}
		}
		refresh();
	});
	timer->start();

	const auto submit = [&] {
		if (pending || lockedUntil > QDateTime::currentSecsSinceEpoch()) {
			return;
		}
		const auto key = input->text().trimmed().toUpper();
		if (!QRegularExpression(
				u"^[A-Z0-9]{1,8}-[A-Z0-9]{4}-[A-Z0-9]{4}$"_q
			).match(key).hasMatch()) {
			status->setText(u"INVALID KEY FORMAT"_q);
			return;
		}
		start(key, false);
	};
	QObject::connect(button, &QPushButton::clicked, submit);
	QObject::connect(input, &QLineEdit::returnPressed, submit);

	if (!cache.key.isEmpty()) {
		start(cache.key, true);
	}
	refresh();
	return dialog.exec() == QDialog::Accepted;
}

#endif // Q_OS_WIN

} // namespace

QString StaffName() {
#ifdef Q_OS_WIN
	return GlobalStaff;
#else // Q_OS_WIN
	return {};
#endif // Q_OS_WIN
}

bool RunGatekeeper() {
#ifdef Q_OS_WIN
	const auto hwid = ComputeHwid();
	if (hwid.isEmpty()) {
		QMessageBox::critical(
			nullptr,
			u"LUPINGRAM"_q,
			u"HWID ERROR: cannot read machine identity."_q);
		return false;
	}
	return ShowDialog(LoadCache(hwid), hwid);
#else // Q_OS_WIN
	return true;
#endif // Q_OS_WIN
}

} // namespace LupinGate
