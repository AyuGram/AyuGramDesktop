#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <locale>
#include <codecvt>

#ifdef _WIN32

#include <windows.h>
#include <tlhelp32.h>

#else
#include <dirent.h>
#include <unistd.h>
#endif

// A function to check if a process is running by its name
// Bing AI generated
bool is_process_running(const std::string &name) {
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
    } while (Process32Next(snapshot, &entry));
    // No match found
    CloseHandle(snapshot);
    return false;
#else
    // Open the /proc directory
    DIR* dir = opendir("/proc");
    if (dir == nullptr) {
        std::cerr << "Failed to open /proc\n";
        return false;
    }
    // Read the subdirectories
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Check if the subdirectory is a number (pid)
        std::string pid = entry->d_name;
        if (std::all_of(pid.begin(), pid.end(), isdigit)) {
            // Read the /proc/pid/cmdline file
            std::string cmdline_file = "/proc/" + pid + "/cmdline";
            std::ifstream file(cmdline_file);
            if (file.is_open()) {
                // Get the first word of the file (process name)
                std::string process_name;
                std::getline(file, process_name, '\0');
                // Remove the path if present
                size_t pos = process_name.rfind('/');
                if (pos != std::string::npos) {
                    process_name = process_name.substr(pos + 1);
                }
                // Compare the names
                if (name == process_name) {
                    // Found a match
                    closedir(dir);
                    return true;
                }
            }
        }
    }
    // No match found
    closedir(dir);
    return false;
#endif
}