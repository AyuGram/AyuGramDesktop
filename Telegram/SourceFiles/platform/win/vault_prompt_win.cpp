/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
// The password window of the vault, shown at launch before Qt exists.
#include "platform/win/vault_prompt_win.h"

#include <windows.h>

#include <QtCore/QDir>

#include <vector>

namespace Platform::VaultPrompt {
namespace {

constexpr auto kClassName = L"AyuVaultPrompt";
constexpr auto kEditId = 101;
constexpr auto kStatusId = 102;
constexpr auto kPasswordLimit = 512;

const auto kNight = RGB(11, 9, 7);
const auto kField = RGB(29, 24, 17);
const auto kSand = RGB(243, 236, 221);
const auto kGold = RGB(226, 184, 90);
const auto kError = RGB(236, 90, 82);

struct Context {
	const std::function<bool(const QByteArray &)> *check = nullptr;
	HWND edit = nullptr;
	HWND status = nullptr;
	HWND unlock = nullptr;
	HBRUSH night = nullptr;
	HBRUSH field = nullptr;
	HFONT font = nullptr;
	HFONT title = nullptr;
	int dpi = 96;
	bool done = false;
	bool opened = false;
};

[[nodiscard]] int Scale(const Context &context, int value) {
	return MulDiv(value, context.dpi, 96);
}

void TryUnlock(HWND window, Context &context) {
	auto buffer = std::vector<wchar_t>(kPasswordLimit + 1, 0);
	GetWindowTextW(context.edit, buffer.data(), kPasswordLimit);
	auto password = QString::fromWCharArray(buffer.data()).toUtf8();
	SecureZeroMemory(buffer.data(), buffer.size() * sizeof(wchar_t));
	if (password.isEmpty()) {
		return;
	}
	EnableWindow(context.unlock, FALSE);
	SetWindowTextW(context.status, L"Checking...");
	UpdateWindow(window);
	const auto previous = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
	const auto ok = (*context.check)(password);
	SetCursor(previous);
	password.fill('\0');
	if (ok) {
		context.opened = true;
		context.done = true;
		return;
	}
	EnableWindow(context.unlock, TRUE);
	SetWindowTextW(context.edit, L"");
	SetWindowTextW(context.status, L"That is not the password.");
	SetFocus(context.edit);
}

void PaintWordmark(HWND window, Context &context) {
	auto paint = PAINTSTRUCT();
	const auto dc = BeginPaint(window, &paint);
	SetBkMode(dc, TRANSPARENT);
	const auto old = SelectObject(dc, context.title);
	const auto top = Scale(context, 18);
	auto x = Scale(context, 24);
	auto size = SIZE();
	SetTextColor(dc, kSand);
	TextOutW(dc, x, top, L"PRIVATE", 7);
	GetTextExtentPoint32W(dc, L"PRIVATE ", 8, &size);
	x += size.cx;
	SetTextColor(dc, kGold);
	TextOutW(dc, x, top, L"EDITION", 7);
	SelectObject(dc, old);
	EndPaint(window, &paint);
}

LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	auto *context = reinterpret_cast<Context*>(
		GetWindowLongPtrW(window, GWLP_USERDATA));
	switch (message) {
	case WM_NCCREATE: {
		const auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
		SetWindowLongPtrW(
			window,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(create->lpCreateParams));
		return DefWindowProcW(window, message, wParam, lParam);
	}
	case WM_COMMAND:
		if (context && LOWORD(wParam) == IDOK) {
			TryUnlock(window, *context);
			return 0;
		} else if (context && LOWORD(wParam) == IDCANCEL) {
			context->done = true;
			return 0;
		}
		break;
	case WM_CLOSE:
		if (context) {
			context->done = true;
		}
		return 0;
	case WM_PAINT:
		if (context) {
			PaintWordmark(window, *context);
			return 0;
		}
		break;
	case WM_ERASEBKGND: {
		RECT rect;
		GetClientRect(window, &rect);
		FillRect(reinterpret_cast<HDC>(wParam), &rect, context->night);
		return 1;
	}
	case WM_CTLCOLORSTATIC: {
		const auto control = reinterpret_cast<HWND>(lParam);
		const auto id = GetDlgCtrlID(control);
		SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
		SetTextColor(
			reinterpret_cast<HDC>(wParam),
			(id == kStatusId) ? kError : kSand);
		return reinterpret_cast<LRESULT>(context->night);
	}
	case WM_CTLCOLOREDIT:
		SetBkColor(reinterpret_cast<HDC>(wParam), kField);
		SetTextColor(reinterpret_cast<HDC>(wParam), kSand);
		return reinterpret_cast<LRESULT>(context->field);
	}
	return DefWindowProcW(window, message, wParam, lParam);
}

void EnableDarkFrame(HWND window) {
	if (const auto dwm = LoadLibraryW(L"dwmapi.dll")) {
		using Set = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
		if (const auto set = reinterpret_cast<Set>(
				GetProcAddress(dwm, "DwmSetWindowAttribute"))) {
			const auto on = BOOL(TRUE);
			set(window, 20, &on, sizeof(on));
		}
		FreeLibrary(dwm);
	}
}

void EnableDarkControl(HWND control) {
	if (const auto theme = LoadLibraryW(L"uxtheme.dll")) {
		using Set = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
		if (const auto set = reinterpret_cast<Set>(
				GetProcAddress(theme, "SetWindowTheme"))) {
			set(control, L"DarkMode_Explorer", nullptr);
		}
		FreeLibrary(theme);
	}
}

HWND Child(
		HWND parent,
		Context &context,
		const wchar_t *type,
		const wchar_t *text,
		DWORD style,
		int id,
		int x,
		int y,
		int w,
		int h,
		HFONT font) {
	const auto result = CreateWindowExW(
		0,
		type,
		text,
		WS_CHILD | WS_VISIBLE | style,
		Scale(context, x),
		Scale(context, y),
		Scale(context, w),
		Scale(context, h),
		parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		GetModuleHandleW(nullptr),
		nullptr);
	SendMessageW(result, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	return result;
}

} // namespace

bool Ask(
		const QString &folder,
		const std::function<bool(const QByteArray &)> &check) {
	auto context = Context();
	context.check = &check;
	const auto screen = GetDC(nullptr);
	context.dpi = GetDeviceCaps(screen, LOGPIXELSY);
	ReleaseDC(nullptr, screen);
	context.night = CreateSolidBrush(kNight);
	context.field = CreateSolidBrush(kField);
	context.font = CreateFontW(
		-MulDiv(10, context.dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0,
		DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
	context.title = CreateFontW(
		-MulDiv(16, context.dpi, 72), 0, 0, 0, FW_LIGHT, 0, 0, 0,
		DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

	auto windowClass = WNDCLASSW();
	windowClass.lpfnWndProc = Proc;
	windowClass.hInstance = GetModuleHandleW(nullptr);
	windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClass.hbrBackground = context.night;
	windowClass.lpszClassName = kClassName;
	RegisterClassW(&windowClass);

	const auto style = DWORD(WS_POPUP | WS_CAPTION | WS_SYSMENU);
	const auto exStyle = DWORD(WS_EX_TOPMOST | WS_EX_APPWINDOW);
	auto rect = RECT{ 0, 0, Scale(context, 420), Scale(context, 214) };
	AdjustWindowRectEx(&rect, style, FALSE, exStyle);
	const auto width = rect.right - rect.left;
	const auto height = rect.bottom - rect.top;
	const auto window = CreateWindowExW(
		exStyle,
		kClassName,
		L"AyuGram vault",
		style,
		(GetSystemMetrics(SM_CXSCREEN) - width) / 2,
		(GetSystemMetrics(SM_CYSCREEN) - height) / 2,
		width,
		height,
		nullptr,
		nullptr,
		GetModuleHandleW(nullptr),
		&context);
	if (!window) {
		return false;
	}
	EnableDarkFrame(window);

	const auto name = QDir::toNativeSeparators(folder).toStdWString();
	Child(
		window, context, L"STATIC",
		L"Enter the vault password to open your chats.", SS_LEFT,
		-1, 24, 58, 372, 20, context.font);
	Child(
		window, context, L"STATIC", name.c_str(), SS_LEFT | SS_PATHELLIPSIS,
		-1, 24, 78, 372, 18, context.font);
	context.edit = Child(
		window, context, L"EDIT", L"",
		ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER,
		kEditId, 24, 108, 372, 26, context.font);
	SendMessageW(context.edit, EM_LIMITTEXT, kPasswordLimit - 1, 0);
	context.status = Child(
		window, context, L"STATIC", L"", SS_LEFT,
		kStatusId, 24, 138, 372, 18, context.font);
	context.unlock = Child(
		window, context, L"BUTTON", L"Unlock",
		BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK, 212, 168, 88, 30, context.font);
	const auto quit = Child(
		window, context, L"BUTTON", L"Quit",
		BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL, 308, 168, 88, 30, context.font);
	EnableDarkControl(context.unlock);
	EnableDarkControl(quit);

	ShowWindow(window, SW_SHOW);
	SetForegroundWindow(window);
	SetFocus(context.edit);

	auto message = MSG();
	while (!context.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
		if (!IsDialogMessageW(window, &message)) {
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}
	DestroyWindow(window);
	UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
	DeleteObject(context.night);
	DeleteObject(context.field);
	DeleteObject(context.font);
	DeleteObject(context.title);
	return context.opened;
}

} // namespace Platform::VaultPrompt
