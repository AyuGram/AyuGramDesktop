// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#ifdef Q_OS_WIN

#include "windows_utils.h"

#include "base/platform/win/base_windows_winrt.h"
#include "platform/win/windows_app_user_model_id.h"

#include <ShlObj_core.h>
#include <propvarutil.h>

void processIcon(QString shortcut, QString iconPath) {
	if (!QFile::exists(shortcut)) {
		return;
	}

	IShellLink *pShellLink = NULL;
	IPersistFile *pPersistFile = NULL;

	HRESULT hr = CoCreateInstance(CLSID_ShellLink,
								  NULL,
								  CLSCTX_INPROC_SERVER,
								  IID_IShellLink,
								  (void**) &pShellLink);
	if (SUCCEEDED(hr)) {
		hr = pShellLink->QueryInterface(IID_IPersistFile, (void**) &pPersistFile);
		if (SUCCEEDED(hr)) {
			WCHAR wszShortcutPath[MAX_PATH];
			shortcut.toWCharArray(wszShortcutPath);
			wszShortcutPath[shortcut.length()] = '\0';

			if (SUCCEEDED(pPersistFile->Load(wszShortcutPath, STGM_READWRITE))) {
				pShellLink->SetIconLocation(iconPath.toStdWString().c_str(), 0);
				pPersistFile->Save(wszShortcutPath, TRUE);
			}

			pPersistFile->Release();
		}

		pShellLink->Release();
	}
}

void processLegacy(const QString &appdata, const QString &iconPath) {
	auto shortcut = appdata + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/AyuGram Desktop.lnk";
	if (!QFile::exists(shortcut)) {
		shortcut = appdata + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/AyuGram.lnk";
	}
	if (!QFile::exists(shortcut)) {
		return;
	}

	processIcon(shortcut, iconPath);
}

void processNewPinned(const QString &iconPath) {
	if (!SUCCEEDED(CoInitialize(0))) {
		return;
	}
	const auto coGuard = gsl::finally([]
	{
		CoUninitialize();
	});

	const auto path = Platform::AppUserModelId::PinnedIconsPath();
	const auto native = QDir::toNativeSeparators(path).toStdWString();

	const auto srcid = Platform::AppUserModelId::MyExecutablePathId();
	if (!srcid) {
		return;
	}

	WIN32_FIND_DATA findData;
	HANDLE findHandle = FindFirstFileEx(
		(native + L"*").c_str(),
		FindExInfoStandard,
		&findData,
		FindExSearchNameMatch,
		0,
		0);
	if (findHandle == INVALID_HANDLE_VALUE) {
		return;
	}

	do {
		std::wstring fname = native + findData.cFileName;
		const auto filePath = QString::fromStdWString(fname);
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}

		DWORD attributes = GetFileAttributes(fname.c_str());
		if (attributes >= 0xFFFFFFF) {
			continue; // file does not exist
		}

		auto shellLink = base::WinRT::TryCreateInstance<IShellLink>(
			CLSID_ShellLink);
		if (!shellLink) {
			continue;
		}

		auto persistFile = shellLink.try_as<IPersistFile>();
		if (!persistFile) {
			continue;
		}

		auto hr = persistFile->Load(fname.c_str(), STGM_READWRITE);
		if (!SUCCEEDED(hr)) continue;

		WCHAR dst[MAX_PATH] = {0};
		hr = shellLink->GetPath(dst, MAX_PATH, nullptr, 0);
		if (!SUCCEEDED(hr)) continue;

		if (Platform::AppUserModelId::GetUniqueFileId(dst) == srcid) {
			auto propertyStore = shellLink.try_as<IPropertyStore>();
			if (!propertyStore) {
				continue;
			}

			processIcon(filePath, iconPath);
		}
	} while (FindNextFile(findHandle, &findData));
	DWORD errorCode = GetLastError();
	if (errorCode && errorCode != ERROR_NO_MORE_FILES) {
		return;
	}
	FindClose(findHandle);
}

void processNewShortcuts(const QString &iconPath) {
	const auto path = Platform::AppUserModelId::systemShortcutPath();
	if (path.isEmpty()) {
		return;
	}

	const auto shortcut = path + u"AyuGram Desktop/AyuGram.lnk"_q;
	const auto native = QDir::toNativeSeparators(path).toStdWString();

	DWORD attributes = GetFileAttributes(native.c_str());
	if (attributes >= 0xFFFFFFF) {
		return; // file does not exist
	}

	const auto normalizedPath = QString::fromStdWString(native);
	processIcon(normalizedPath, iconPath);
}

void reloadAppIconFromTaskBar() {
	QString appdata = QDir::fromNativeSeparators(qgetenv("APPDATA"));
	QString iconPath = appdata + "/AyuGram.ico";

	processNewPinned(iconPath);
	processNewShortcuts(iconPath);
	processLegacy(appdata, iconPath);

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

#endif
