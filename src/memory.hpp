#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>
#include <cstdint>
#include <optional>
#include <string>

class Memory {
public:
    Memory() = default;
    ~Memory() { if (m_handle) CloseHandle(m_handle); }

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    bool Attach(const std::string& processName);
    bool IsAttached() const { return m_handle != nullptr && m_pid != 0; }
    uintptr_t GetModuleBase(const std::string& moduleName) const;

    template<typename T>
    T Read(uintptr_t address) const {
        T value{};
        ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), nullptr);
        return value;
    }

    bool ReadBuffer(uintptr_t address, void* buffer, size_t size) const {
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(m_handle, reinterpret_cast<LPCVOID>(address),
                                 buffer, size, &bytesRead) && bytesRead == size;
    }

    std::string ReadString(uintptr_t address, size_t maxLen = 128) const;

    DWORD GetPID() const { return m_pid; }

private:
    HANDLE m_handle = nullptr;
    DWORD  m_pid    = 0;

    DWORD FindPID(const std::string& processName) const;
};
