// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023
#ifdef Q_OS_WIN

#include "windows_utils.h"

#include <ShlObj_core.h>

void reloadAppIconFromTaskBar() {
	QString appdata = QDir::fromNativeSeparators(qgetenv("APPDATA"));
	QString ayugramIconPath = appdata + "/AyuGram.ico";

	QString shortcut = appdata + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/AyuGram Desktop.lnk";
	if (!QFile::exists(shortcut)) {
		shortcut = appdata + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/AyuGram.lnk";
	}

	if (QFile::exists(shortcut)) {
		IShellLink *pShellLink = NULL;
		IPersistFile *pPersistFile = NULL;

		HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
		                              (void **) &pShellLink);
		if (SUCCEEDED(hr)) {
			hr = pShellLink->QueryInterface(IID_IPersistFile, (void **) &pPersistFile);
			if (SUCCEEDED(hr)) {
				WCHAR wszShortcutPath[MAX_PATH];
				shortcut.toWCharArray(wszShortcutPath);
				wszShortcutPath[shortcut.length()] = '\0';

				if (SUCCEEDED(pPersistFile->Load(wszShortcutPath, STGM_READWRITE))) {
					pShellLink->SetIconLocation(ayugramIconPath.toStdWString().c_str(), 0);
					pPersistFile->Save(wszShortcutPath, TRUE);
				}

				pPersistFile->Release();
			}

			pShellLink->Release();
		}

		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
	}
}

#endif