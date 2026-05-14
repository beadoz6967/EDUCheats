#include "memory.hpp"
#include <algorithm>

DWORD Memory::FindPID(const std::string& processName) const {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    DWORD pid = 0;
    if (Process32First(snap, &entry)) {
        do {
            if (processName == entry.szExeFile) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snap, &entry));
    }

    CloseHandle(snap);
    return pid;
}

bool Memory::Attach(const std::string& processName) {
    m_pid = FindPID(processName);
    if (!m_pid) return false;

    m_handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, m_pid);
    return m_handle != nullptr;
}

uintptr_t Memory::GetModuleBase(const std::string& moduleName) const {
    if (!m_handle) return 0;

    HMODULE modules[1024]{};
    DWORD needed = 0;

    if (!EnumProcessModules(m_handle, modules, sizeof(modules), &needed))
        return 0;

    const DWORD count = needed / sizeof(HMODULE);
    char name[MAX_PATH]{};

    for (DWORD i = 0; i < count; ++i) {
        if (GetModuleFileNameExA(m_handle, modules[i], name, MAX_PATH)) {
            // Compare just the filename portion
            std::string fullPath(name);
            auto slash = fullPath.find_last_of("\\/");
            std::string filename = (slash != std::string::npos) ? fullPath.substr(slash + 1) : fullPath;

            // Case-insensitive compare
            std::string a = filename, b = moduleName;
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);

            if (a == b)
                return reinterpret_cast<uintptr_t>(modules[i]);
        }
    }

    return 0;
}

std::string Memory::ReadString(uintptr_t address, size_t maxLen) const {
    std::string result(maxLen, '\0');
    SIZE_T bytesRead = 0;
    ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address),
                      result.data(), maxLen, &bytesRead);
    result.resize(strnlen(result.c_str(), maxLen));
    return result;
}
