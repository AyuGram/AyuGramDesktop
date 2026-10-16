/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The vault: the profile folder is encrypted while the app is closed and opened with a password at launch.
// Every file under tdata becomes <name>.agv: AES-256-GCM in 64 KiB chunks, each chunk authenticated together
// with the file header, its index and a final-chunk flag, so truncation and reordering are detected.
// The key comes from the password with PBKDF2-HMAC-SHA512. tdata\vault.meta holds the salt and a verifier.
#include "storage/storage_vault.h"

#include "ayu/data/ayu_database.h"
#include "ayu/utils/ayu_cache_cleanup.h"
#include "base/random.h"
#include "logs.h"
#include "storage/storage_duress.h"
#include "storage/storage_secure_wipe.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>

#include <algorithm>
#include <array>
#include <cstring>

#ifdef Q_OS_WIN
#include "platform/win/vault_prompt_win.h"

#include <io.h>
#include <windows.h>
#endif // Q_OS_WIN

namespace Storage::Vault {
namespace {

constexpr auto kMetaMagic = "AYUVLT01";
constexpr auto kFileMagic = "AGV1";
constexpr auto kIterations = 600000;
constexpr auto kSaltSize = 16;
constexpr auto kKeySize = 32;
constexpr auto kNonceSize = 12;
constexpr auto kTagSize = 16;
constexpr auto kChunkSize = 64 * 1024;
constexpr auto kHeaderSize = 24;
constexpr auto kVerifier = "AYUG-VAULT-OK";
constexpr auto kMetaAad = "meta";
constexpr auto kOverwriteLimit = qint64(64) * 1024 * 1024;
constexpr auto kBusyStep = 250;
constexpr auto kBusyLimit = 20000;

using Key = std::array<unsigned char, kKeySize>;

struct State {
	Key key = {};
	bool hasKey = false;
	bool secondary = false;
#ifdef Q_OS_WIN
	HANDLE lock = INVALID_HANDLE_VALUE;
	HANDLE busy = INVALID_HANDLE_VALUE;
#endif // Q_OS_WIN
};

State &GlobalState() {
	static auto result = State();
	return result;
}

[[nodiscard]] QString TdataPath(const QString &workingDir) {
	auto base = QDir::cleanPath(workingDir);
	if (!base.endsWith('/')) {
		base += '/';
	}
	return base + QStringLiteral("tdata/");
}

[[nodiscard]] QString MetaPath(const QString &workingDir) {
	return TdataPath(workingDir) + QStringLiteral("vault.meta");
}

[[nodiscard]] bool IsOwnFile(const QString &name) {
	return name == QLatin1String("vault.meta")
		|| name == QLatin1String("vault.lock")
		|| name.endsWith(QLatin1String(".agv"))
		|| name.endsWith(QLatin1String(".agv.tmp"));
}

// Read before the vault opens or kept open while the app runs; none of them holds a secret.
[[nodiscard]] bool IsLeftPlain(const QString &name) {
	return name == QLatin1String("working")
		|| name == QLatin1String("usertag")
		|| name == QLatin1String("withdebug")
		|| name == QLatin1String("devversion")
		|| name.endsWith(QLatin1String(".ico"));
}

[[nodiscard]] bool FlushToDisk(QFile &file) {
	if (!file.flush()) {
		return false;
	}
#ifdef Q_OS_WIN
	const auto handle = _get_osfhandle(file.handle());
	return (handle != -1)
		&& FlushFileBuffers(reinterpret_cast<HANDLE>(handle));
#else // Q_OS_WIN
	return true;
#endif // !Q_OS_WIN
}

// Small files are overwritten first so the clear text does not linger in the freed clusters.
[[nodiscard]] bool RemovePlain(const QString &path, qint64 size) {
	return (size <= kOverwriteLimit)
		? SecureWipe::overwriteAndUnlinkFile(path)
		: QFile::remove(path);
}

void PutLE32(unsigned char *to, quint32 value) {
	for (auto i = 0; i != 4; ++i) {
		to[i] = (value >> (8 * i)) & 0xFF;
	}
}

void PutLE64(unsigned char *to, quint64 value) {
	for (auto i = 0; i != 8; ++i) {
		to[i] = (value >> (8 * i)) & 0xFF;
	}
}

[[nodiscard]] quint32 GetLE32(const unsigned char *from) {
	auto result = quint32();
	for (auto i = 0; i != 4; ++i) {
		result |= quint32(from[i]) << (8 * i);
	}
	return result;
}

[[nodiscard]] quint64 GetLE64(const unsigned char *from) {
	auto result = quint64();
	for (auto i = 0; i != 8; ++i) {
		result |= quint64(from[i]) << (8 * i);
	}
	return result;
}

[[nodiscard]] bool DeriveKey(
		const QByteArray &password,
		const unsigned char *salt,
		int iterations,
		Key &out) {
	return PKCS5_PBKDF2_HMAC(
		password.constData(),
		int(password.size()),
		salt,
		kSaltSize,
		iterations,
		EVP_sha512(),
		kKeySize,
		out.data()) == 1;
}

[[nodiscard]] bool Seal(
		const Key &key,
		const unsigned char *nonce,
		const unsigned char *aad,
		int aadSize,
		const unsigned char *plain,
		int size,
		unsigned char *cipher,
		unsigned char *tag) {
	const auto ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		return false;
	}
	auto length = 0;
	auto ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
		&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) == 1
		&& EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce) == 1
		&& EVP_EncryptUpdate(ctx, nullptr, &length, aad, aadSize) == 1
		&& (size == 0
			|| EVP_EncryptUpdate(ctx, cipher, &length, plain, size) == 1)
		&& EVP_EncryptFinal_ex(ctx, cipher + size, &length) == 1
		&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagSize, tag) == 1;
	EVP_CIPHER_CTX_free(ctx);
	return ok;
}

[[nodiscard]] bool Open(
		const Key &key,
		const unsigned char *nonce,
		const unsigned char *aad,
		int aadSize,
		const unsigned char *cipher,
		int size,
		const unsigned char *tag,
		unsigned char *plain) {
	const auto ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		return false;
	}
	auto length = 0;
	auto ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
		&& EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceSize, nullptr) == 1
		&& EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce) == 1
		&& EVP_DecryptUpdate(ctx, nullptr, &length, aad, aadSize) == 1
		&& (size == 0
			|| EVP_DecryptUpdate(ctx, plain, &length, cipher, size) == 1)
		&& EVP_CIPHER_CTX_ctrl(
			ctx,
			EVP_CTRL_GCM_SET_TAG,
			kTagSize,
			const_cast<unsigned char*>(tag)) == 1
		&& EVP_DecryptFinal_ex(ctx, plain + size, &length) == 1;
	EVP_CIPHER_CTX_free(ctx);
	return ok;
}

struct Meta {
	int iterations = 0;
	std::array<unsigned char, kSaltSize> salt = {};
	std::array<unsigned char, kNonceSize> nonce = {};
	std::array<unsigned char, 13> cipher = {};
	std::array<unsigned char, kTagSize> tag = {};
};

constexpr auto kMetaSize = 8 + 4 + kSaltSize + kNonceSize + 13 + kTagSize;

[[nodiscard]] bool ReadMeta(const QString &workingDir, Meta &out) {
	auto file = QFile(MetaPath(workingDir));
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	const auto raw = file.readAll();
	if (raw.size() != kMetaSize
		|| std::memcmp(raw.constData(), kMetaMagic, 8) != 0) {
		return false;
	}
	const auto bytes = reinterpret_cast<const unsigned char*>(raw.constData());
	auto at = 8;
	out.iterations = int(GetLE32(bytes + at));
	at += 4;
	std::memcpy(out.salt.data(), bytes + at, kSaltSize);
	at += kSaltSize;
	std::memcpy(out.nonce.data(), bytes + at, kNonceSize);
	at += kNonceSize;
	std::memcpy(out.cipher.data(), bytes + at, 13);
	at += 13;
	std::memcpy(out.tag.data(), bytes + at, kTagSize);
	return out.iterations >= 100000 && out.iterations <= 100000000;
}

[[nodiscard]] bool WriteMeta(const QString &workingDir, const Meta &meta) {
	auto raw = QByteArray(kMetaSize, 0);
	auto bytes = reinterpret_cast<unsigned char*>(raw.data());
	std::memcpy(bytes, kMetaMagic, 8);
	auto at = 8;
	PutLE32(bytes + at, quint32(meta.iterations));
	at += 4;
	std::memcpy(bytes + at, meta.salt.data(), kSaltSize);
	at += kSaltSize;
	std::memcpy(bytes + at, meta.nonce.data(), kNonceSize);
	at += kNonceSize;
	std::memcpy(bytes + at, meta.cipher.data(), 13);
	at += 13;
	std::memcpy(bytes + at, meta.tag.data(), kTagSize);

	QDir().mkpath(TdataPath(workingDir));
	auto file = QSaveFile(MetaPath(workingDir));
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	file.write(raw);
	return file.commit();
}

[[nodiscard]] bool AcquireLock(const QString &workingDir) {
#ifdef Q_OS_WIN
	auto &state = GlobalState();
	if (state.lock != INVALID_HANDLE_VALUE) {
		return true;
	}
	QDir().mkpath(TdataPath(workingDir));
	const auto path = QDir::toNativeSeparators(
		TdataPath(workingDir) + QStringLiteral("vault.lock"));
	state.lock = CreateFileW(
		path.toStdWString().c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_HIDDEN | FILE_FLAG_DELETE_ON_CLOSE,
		nullptr);
	return state.lock != INVALID_HANDLE_VALUE;
#else // Q_OS_WIN
	Q_UNUSED(workingDir);
	return false;
#endif // !Q_OS_WIN
}

void ReleaseLock() {
#ifdef Q_OS_WIN
	auto &state = GlobalState();
	if (state.lock != INVALID_HANDLE_VALUE) {
		CloseHandle(state.lock);
		state.lock = INVALID_HANDLE_VALUE;
	}
#endif // Q_OS_WIN
}

// Exists while the profile is being opened or closed, so a second launch waits instead of racing the seal.
[[nodiscard]] bool EnterBusy(const QString &workingDir) {
#ifdef Q_OS_WIN
	auto &state = GlobalState();
	if (state.busy != INVALID_HANDLE_VALUE) {
		return true;
	}
	const auto hash = QCryptographicHash::hash(
		QDir::cleanPath(workingDir).toLower().toUtf8(),
		QCryptographicHash::Sha1).toHex();
	const auto name = (QStringLiteral("Local\\AyuVaultBusy-")
		+ QString::fromLatin1(hash)).toStdWString();
	const auto handle = CreateMutexW(nullptr, FALSE, name.c_str());
	if (!handle) {
		return true;
	} else if (GetLastError() == ERROR_ALREADY_EXISTS) {
		CloseHandle(handle);
		return false;
	}
	state.busy = handle;
#else // Q_OS_WIN
	Q_UNUSED(workingDir);
#endif // !Q_OS_WIN
	return true;
}

void LeaveBusy() {
#ifdef Q_OS_WIN
	auto &state = GlobalState();
	if (state.busy != INVALID_HANDLE_VALUE) {
		CloseHandle(state.busy);
		state.busy = INVALID_HANDLE_VALUE;
	}
#endif // Q_OS_WIN
}

[[nodiscard]] QStringList ListFiles(const QString &tdata, bool sealed) {
	auto result = QStringList();
	auto it = QDirIterator(
		tdata,
		QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks,
		QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const auto path = it.next();
		const auto name = QFileInfo(path).fileName();
		if (sealed) {
			if (name.endsWith(QLatin1String(".agv"))
				&& name != QLatin1String("vault.meta")) {
				result.push_back(path);
			}
		} else if (!IsOwnFile(name) && !IsLeftPlain(name)) {
			result.push_back(path);
		}
	}
	return result;
}

[[nodiscard]] bool SealFile(const Key &key, const QString &path) {
	auto in = QFile(path);
	if (!in.open(QIODevice::ReadOnly)) {
		return false;
	}
	const auto size = in.size();
	const auto temp = path + QStringLiteral(".agv.tmp");
	auto out = QFile(temp);
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	const auto fail = [&] {
		out.close();
		QFile::remove(temp);
		return false;
	};

	unsigned char header[kHeaderSize] = {};
	std::memcpy(header, kFileMagic, 4);
	PutLE32(header + 4, kChunkSize);
	PutLE64(header + 8, quint64(size));
	base::RandomFill(header + 16, 8);
	if (out.write(reinterpret_cast<const char*>(header), kHeaderSize) != kHeaderSize) {
		return fail();
	}

	auto plain = QByteArray(kChunkSize, 0);
	auto cipher = QByteArray(kChunkSize, 0);
	auto remaining = qint64(size);
	auto counter = quint32(0);
	do {
		const auto count = std::min<qint64>(kChunkSize, remaining);
		if (in.read(plain.data(), count) != count) {
			return fail();
		}
		unsigned char nonce[kNonceSize] = {};
		std::memcpy(nonce, header + 16, 8);
		for (auto i = 0; i != 4; ++i) {
			nonce[8 + i] = (counter >> (8 * (3 - i))) & 0xFF;
		}
		unsigned char aad[kHeaderSize + 5] = {};
		std::memcpy(aad, header, kHeaderSize);
		PutLE32(aad + kHeaderSize, counter);
		aad[kHeaderSize + 4] = (remaining - count == 0) ? 1 : 0;
		unsigned char tag[kTagSize] = {};
		if (!Seal(
				key,
				nonce,
				aad,
				sizeof(aad),
				reinterpret_cast<const unsigned char*>(plain.constData()),
				int(count),
				reinterpret_cast<unsigned char*>(cipher.data()),
				tag)) {
			return fail();
		}
		if (out.write(cipher.constData(), count) != count
			|| out.write(reinterpret_cast<const char*>(tag), kTagSize) != kTagSize) {
			return fail();
		}
		remaining -= count;
		++counter;
	} while (remaining > 0);

	if (!in.atEnd() || in.size() != size) {
		return fail();
	}
	if (!FlushToDisk(out)) {
		return fail();
	}
	out.close();
	in.close();

	const auto target = path + QStringLiteral(".agv");
	QFile::remove(target);
	if (!QFile::rename(temp, target)) {
		QFile::remove(temp);
		return false;
	}
	// A plain file that cannot be removed stays next to the sealed copy; the next close tries again.
	return RemovePlain(path, size);
}

[[nodiscard]] bool UnsealFile(const Key &key, const QString &sealedPath) {
	const auto target = sealedPath.left(sealedPath.size() - 3);
	if (QFileInfo::exists(target)) {
		// Both exist only after an interrupted close: the plain file is the newer one.
		return QFile::remove(sealedPath);
	}
	auto in = QFile(sealedPath);
	if (!in.open(QIODevice::ReadOnly)) {
		return false;
	}
	unsigned char header[kHeaderSize] = {};
	if (in.read(reinterpret_cast<char*>(header), kHeaderSize) != kHeaderSize
		|| std::memcmp(header, kFileMagic, 4) != 0) {
		return false;
	}
	const auto chunk = qint64(GetLE32(header + 4));
	const auto size = qint64(GetLE64(header + 8));
	if (chunk <= 0 || chunk > 1024 * 1024 || size < 0) {
		return false;
	}
	const auto count = std::max<qint64>(1, (size + chunk - 1) / chunk);
	if (in.size() != kHeaderSize + size + count * kTagSize) {
		return false;
	}

	const auto temp = target + QStringLiteral(".agv.tmp");
	auto out = QFile(temp);
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	const auto fail = [&] {
		out.close();
		QFile::remove(temp);
		return false;
	};
	auto cipher = QByteArray(int(chunk), 0);
	auto plain = QByteArray(int(chunk), 0);
	auto remaining = size;
	for (auto counter = quint32(0); counter != quint32(count); ++counter) {
		const auto n = std::min<qint64>(chunk, remaining);
		unsigned char tag[kTagSize] = {};
		if (in.read(cipher.data(), n) != n
			|| in.read(reinterpret_cast<char*>(tag), kTagSize) != kTagSize) {
			return fail();
		}
		unsigned char nonce[kNonceSize] = {};
		std::memcpy(nonce, header + 16, 8);
		for (auto i = 0; i != 4; ++i) {
			nonce[8 + i] = (counter >> (8 * (3 - i))) & 0xFF;
		}
		unsigned char aad[kHeaderSize + 5] = {};
		std::memcpy(aad, header, kHeaderSize);
		PutLE32(aad + kHeaderSize, counter);
		aad[kHeaderSize + 4] = (remaining - n == 0) ? 1 : 0;
		if (!Open(
				key,
				nonce,
				aad,
				sizeof(aad),
				reinterpret_cast<const unsigned char*>(cipher.constData()),
				int(n),
				tag,
				reinterpret_cast<unsigned char*>(plain.data()))) {
			return fail();
		}
		if (out.write(plain.constData(), n) != n) {
			return fail();
		}
		remaining -= n;
	}
	if (!FlushToDisk(out)) {
		return fail();
	}
	out.close();
	in.close();
	if (!QFile::rename(temp, target)) {
		QFile::remove(temp);
		return false;
	}
	return QFile::remove(sealedPath);
}

} // namespace

bool Supported() {
#ifdef Q_OS_WIN
	return true;
#else // Q_OS_WIN
	return false;
#endif // !Q_OS_WIN
}

bool IsEnabled(const QString &workingDir) {
	return QFileInfo::exists(MetaPath(workingDir));
}

bool UnlockWithPassword(
		const QString &workingDir,
		const QByteArray &utf8Password) {
	auto meta = Meta();
	if (utf8Password.isEmpty() || !ReadMeta(workingDir, meta)) {
		return false;
	}
	auto key = Key();
	if (!DeriveKey(utf8Password, meta.salt.data(), meta.iterations, key)) {
		return false;
	}
	unsigned char plain[13] = {};
	const auto ok = Open(
		key,
		meta.nonce.data(),
		reinterpret_cast<const unsigned char*>(kMetaAad),
		int(std::strlen(kMetaAad)),
		meta.cipher.data(),
		13,
		meta.tag.data(),
		plain)
		&& std::memcmp(plain, kVerifier, 13) == 0;
	if (!ok) {
		OPENSSL_cleanse(key.data(), key.size());
		return false;
	}
	auto &state = GlobalState();
	state.key = key;
	state.hasKey = true;
	OPENSSL_cleanse(key.data(), key.size());
	return true;
}

bool Enable(const QString &workingDir, const QByteArray &utf8Password) {
	if (!Supported()
		|| utf8Password.isEmpty()
		|| IsEnabled(workingDir)
		|| !AcquireLock(workingDir)) {
		return false;
	}
	auto meta = Meta();
	meta.iterations = kIterations;
	base::RandomFill(meta.salt.data(), kSaltSize);
	base::RandomFill(meta.nonce.data(), kNonceSize);
	auto key = Key();
	if (!DeriveKey(utf8Password, meta.salt.data(), meta.iterations, key)
		|| !Seal(
			key,
			meta.nonce.data(),
			reinterpret_cast<const unsigned char*>(kMetaAad),
			int(std::strlen(kMetaAad)),
			reinterpret_cast<const unsigned char*>(kVerifier),
			13,
			meta.cipher.data(),
			meta.tag.data())
		|| !WriteMeta(workingDir, meta)) {
		OPENSSL_cleanse(key.data(), key.size());
		ReleaseLock();
		return false;
	}
	auto &state = GlobalState();
	state.key = key;
	state.hasKey = true;
	OPENSSL_cleanse(key.data(), key.size());
	return true;
}

bool Disable(const QString &workingDir, const QByteArray &utf8Password) {
	if (!UnlockWithPassword(workingDir, utf8Password)
		|| !ListFiles(TdataPath(workingDir), true).isEmpty()
		|| !QFile::remove(MetaPath(workingDir))) {
		return false;
	}
	ForgetKey();
	ReleaseLock();
	return true;
}

void ForgetKey() {
	auto &state = GlobalState();
	OPENSSL_cleanse(state.key.data(), state.key.size());
	state.hasKey = false;
}

Result UnsealAll(const QString &workingDir) {
	auto result = Result();
	const auto &state = GlobalState();
	if (!state.hasKey) {
		result.ok = false;
		return result;
	}
	for (const auto &path : ListFiles(TdataPath(workingDir), true)) {
		if (UnsealFile(state.key, path)) {
			++result.files;
		} else {
			if (!result.skipped) {
				result.firstFailed = path;
			}
			++result.skipped;
		}
	}
	return result;
}

Result SealAll(const QString &workingDir, bool keepCaches) {
	auto result = Result();
	const auto &state = GlobalState();
	if (!state.hasKey) {
		result.ok = false;
		return result;
	}
	if (!keepCaches) {
		const auto removed = Ayu::CacheCleanup::run(workingDir, {
			.clearCache = true,
		});
		DEBUG_LOG(("Vault: caches removed=%1 skipped=%2"
			).arg(removed.removed
			).arg(removed.skipped));
	}
	for (const auto &path : ListFiles(TdataPath(workingDir), false)) {
		if (SealFile(state.key, path)) {
			++result.files;
		} else {
			++result.skipped;
		}
	}
	return result;
}

bool OpenAtStart(const QString &workingDir) {
	if (!IsEnabled(workingDir)) {
		return true;
	}
#ifdef Q_OS_WIN
	for (auto waited = 0;; waited += kBusyStep) {
		if (EnterBusy(workingDir)) {
			if (AcquireLock(workingDir)) {
				break;
			}
			// A running copy owns the profile and takes this launch over.
			LeaveBusy();
			GlobalState().secondary = true;
			return true;
		} else if (waited >= kBusyLimit) {
			return false;
		}
		// Another copy is opening or closing the profile: wait, never touch the files.
		Sleep(kBusyStep);
	}
	const auto opened = Platform::VaultPrompt::Ask(
		workingDir,
		[&](const QByteArray &password) {
			if (UnlockWithPassword(workingDir, password)) {
				Duress::ResetWrongPasswords();
				return true;
			}
			// An unreadable vault.meta is not a wrong password.
			auto meta = Meta();
			if (!password.isEmpty() && ReadMeta(workingDir, meta)) {
				Duress::RegisterWrongPassword();
			}
			return false;
		});
	if (!opened) {
		ReleaseLock();
		LeaveBusy();
		return false;
	}
	const auto result = UnsealAll(workingDir);
	LOG(("Vault: opened %1 files, %2 skipped").arg(result.files).arg(result.skipped));
	if (result.skipped > 0) {
		ForgetKey();
		ReleaseLock();
		LeaveBusy();
		const auto text = QStringLiteral(
			"%1 files could not be decrypted, so the app did not start. "
			"Close other programs that use the profile folder and start it again.\n\nFirst file: %2"
		).arg(result.skipped).arg(QFileInfo(result.firstFailed).fileName()).toStdWString();
		MessageBoxW(
			nullptr,
			text.c_str(),
			L"AyuGram vault",
			MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
		return false;
	}
	LeaveBusy();
	return true;
#else // Q_OS_WIN
	LOG(("Vault: this platform cannot open an encrypted profile."));
	return false;
#endif // !Q_OS_WIN
}

void CloseAtExit(
		const QString &workingDir,
		const std::function<bool()> &keepCaches) {
	const auto &state = GlobalState();
	if (state.secondary || !state.hasKey || !IsEnabled(workingDir)) {
		ForgetKey();
		ReleaseLock();
		return;
	}
#ifdef Q_OS_WIN
	// A launch racing this close waits on the marker; it is only ever held for a moment elsewhere.
	for (auto tries = 0; tries != 100 && !EnterBusy(workingDir); ++tries) {
		Sleep(50);
	}
#endif // Q_OS_WIN
	AyuDatabase::shutdown();
	const auto result = SealAll(workingDir, keepCaches());
	LOG(("Vault: closed %1 files, %2 skipped").arg(result.files).arg(result.skipped));
#ifdef Q_OS_WIN
	if (result.skipped > 0) {
		const auto text = QStringLiteral(
			"%1 files could not be encrypted and stay readable on disk. "
			"Close other programs that use the profile folder and quit the app again."
		).arg(result.skipped).toStdWString();
		MessageBoxW(
			nullptr,
			text.c_str(),
			L"AyuGram vault",
			MB_OK | MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
	}
#endif // Q_OS_WIN
	ForgetKey();
	ReleaseLock();
	LeaveBusy();
}

} // namespace Storage::Vault
