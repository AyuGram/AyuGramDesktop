// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include <codecvt>
#include <iostream>
#include <locale>
#include <string>

#ifdef _WIN32

#include <tlhelp32.h>
#include <windows.h>

#endif

// A function to check if a process is running by its name
// Bing AI generated
inline bool isProcessRunning(const std::string &name)
{
#ifdef _WIN32
	// Create a snapshot of all processes
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		std::cerr << "Failed to create snapshot\n";
		return false;
	}
	// Iterate over the processes and compare the names
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(entry);
	if (!Process32First(snapshot, &entry)) {
		std::cerr << "Failed to get first process\n";
		CloseHandle(snapshot);
		return false;
	}
	do {
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::string entry_name = converter.to_bytes(entry.szExeFile);
		if (name == entry_name) {
			// Found a match
			CloseHandle(snapshot);
			return true;
		}
	}
	while (Process32Next(snapshot, &entry));
	// No match found
	CloseHandle(snapshot);
	return false;
#else
	throw std::logic_error("not implemented");
#endif
}

// Copilot generated
void killProcess(const std::string &name)
{
#ifdef _WIN32
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		std::cerr << "Failed to create snapshot\n";
		return;
	}
	// Iterate over the processes and compare the names
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(entry);
	if (!Process32First(snapshot, &entry)) {
		std::cerr << "Failed to get first process\n";
		CloseHandle(snapshot);
		return;
	}
	do {
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::string entry_name = converter.to_bytes(entry.szExeFile);
		if (name == entry_name) {
			// Found a match
			HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
			if (hProcess != nullptr) {
				TerminateProcess(hProcess, 9);
				CloseHandle(hProcess);
			}
			CloseHandle(snapshot);
			return;
		}
	}
	while (Process32Next(snapshot, &entry));
	// No match found
	CloseHandle(snapshot);
#else
	throw std::logic_error("not implemented");
#endif
}
