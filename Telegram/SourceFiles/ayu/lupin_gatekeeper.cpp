#include "ayu/lupin_gatekeeper.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif // Q_OS_WIN

namespace Lupin {
namespace {

constexpr auto kAuthServerUrl = L"https://api.lupingram.local/v1/verify";

enum class Result {
	Ok,
	Denied,
	NetworkError,
};

QString GlobalStaff;

[[nodiscard]] QString AuthPath() {
	return qEnvironmentVariable("APPDATA") + u"/LupinGram/auth.dat"_q;
}

[[nodiscard]] QJsonObject LoadCache() {
	auto file = QFile(AuthPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return {};
	}
	return QJsonDocument::fromJson(file.readAll()).object();
}

void SaveCache(const QString &key, const QString &staff) {
	QDir().mkpath(QFileInfo(AuthPath()).absolutePath());
	auto file = QFile(AuthPath());
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(QJsonObject{
			{ u"key"_q, key },
			{ u"staff"_q, staff },
		}).toJson(QJsonDocument::Compact));
	}
}

#ifdef Q_OS_WIN

[[nodiscard]] QString ComputeHwid() {
	auto guid = std::wstring(128, L'\0');
	auto size = DWORD(guid.size() * sizeof(wchar_t));
	RegGetValueW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Cryptography",
		L"MachineGuid",
		RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
		nullptr,
		guid.data(),
		&size);
	auto serial = DWORD(0);
	GetVolumeInformationW(
		L"C:\\",
		nullptr,
		0,
		&serial,
		nullptr,
		nullptr,
		nullptr,
		0);
	const auto raw = QString::fromWCharArray(guid.c_str())
		+ u"|"_q
		+ QString::number(serial, 16);
	return QString::fromLatin1(QCryptographicHash::hash(
		raw.toUtf8(),
		QCryptographicHash::Sha256).toHex());
}

[[nodiscard]] bool Post(const QByteArray &body, QByteArray &response) {
	auto host = std::wstring(256, L'\0');
	auto path = std::wstring(512, L'\0');
	auto parts = URL_COMPONENTS();
	parts.dwStructSize = sizeof(parts);
	parts.lpszHostName = host.data();
	parts.dwHostNameLength = DWORD(host.size());
	parts.lpszUrlPath = path.data();
	parts.dwUrlPathLength = DWORD(path.size());
	if (!WinHttpCrackUrl(kAuthServerUrl, 0, 0, &parts)) {
		return false;
	}
	const auto session = WinHttpOpen(
		L"LupinGram",
		WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);
	if (!session) {
		return false;
	}
	WinHttpSetTimeouts(session, 8000, 8000, 8000, 8000);
	auto ok = false;
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
			(LPVOID)body.constData(),
			DWORD(body.size()),
			DWORD(body.size()),
			0)
		&& WinHttpReceiveResponse(request, nullptr)) {
		ok = true;
		auto available = DWORD(0);
		while (WinHttpQueryDataAvailable(request, &available) && available) {
			auto chunk = QByteArray(int(available), Qt::Uninitialized);
			auto read = DWORD(0);
			if (!WinHttpReadData(request, chunk.data(), available, &read)
				|| !read) {
				break;
			}
			response.append(chunk.constData(), int(read));
		}
	}
	if (request) WinHttpCloseHandle(request);
	if (connect) WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);
	return ok;
}

#else // Q_OS_WIN

[[nodiscard]] QString ComputeHwid() {
	return {};
}

[[nodiscard]] bool Post(const QByteArray &, QByteArray &) {
	return false;
}

#endif // Q_OS_WIN

[[nodiscard]] Result Verify(const QString &key, QString &staff) {
	const auto body = QJsonDocument(QJsonObject{
		{ u"invite_key"_q, key },
		{ u"hwid"_q, ComputeHwid() },
	}).toJson(QJsonDocument::Compact);
	auto response = QByteArray();
	if (!Post(body, response)) {
		return Result::NetworkError;
	}
	const auto object = QJsonDocument::fromJson(response).object();
	if (object.value(u"status"_q).toString() == u"ok"_q) {
		staff = object.value(u"issued_by"_q).toString();
		return staff.isEmpty() ? Result::Denied : Result::Ok;
	}
	return Result::Denied;
}

[[nodiscard]] bool ShowDialog() {
	auto dialog = QDialog();
	dialog.setWindowTitle(u"LUPINGRAM // ACCESS RESTRICTED"_q);
	dialog.setFixedSize(420, 220);
	dialog.setStyleSheet(
		u"QDialog{background:#050805;}"
		"QLabel{color:#00FF41;font-family:Consolas;font-size:14px;}"
		"QLineEdit{background:#0A0F0A;color:#00FF41;border:1px solid #003B00;"
		"padding:8px;font-family:Consolas;font-size:15px;}"
		"QPushButton{background:#00CC33;color:#050805;border:0;padding:10px;"
		"font-family:Consolas;font-weight:bold;}"
		"QPushButton:hover{background:#00FF41;}"_q);
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
	const auto submit = [&] {
		const auto key = input->text().trimmed().toUpper();
		if (!QRegularExpression(u"^[A-Z0-9]{1,8}-[A-Z0-9]{4}-[A-Z0-9]{4}$"_q).match(key).hasMatch()) {
			status->setText(u"INVALID KEY FORMAT"_q);
			return;
		}
		auto staff = QString();
		switch (Verify(key, staff)) {
		case Result::Ok:
			GlobalStaff = staff;
			SaveCache(key, staff);
			dialog.accept();
			break;
		case Result::Denied:
			status->setText(u"ACCESS DENIED"_q);
			break;
		case Result::NetworkError:
			status->setText(u"NETWORK ERROR"_q);
			break;
		}
	};
	QObject::connect(button, &QPushButton::clicked, submit);
	QObject::connect(input, &QLineEdit::returnPressed, submit);
	return dialog.exec() == QDialog::Accepted;
}

} // namespace

QString StaffName() {
	return GlobalStaff;
}

bool RunGatekeeper() {
	const auto cache = LoadCache();
	const auto key = cache.value(u"key"_q).toString();
	if (!key.isEmpty()) {
		auto staff = QString();
		switch (Verify(key, staff)) {
		case Result::Ok:
			GlobalStaff = staff;
			SaveCache(key, staff);
			return true;
		case Result::NetworkError:
			GlobalStaff = cache.value(u"staff"_q).toString();
			return !GlobalStaff.isEmpty();
		case Result::Denied:
			QFile::remove(AuthPath());
			break;
		}
	}
	return ShowDialog();
}

} // namespace Lupin
