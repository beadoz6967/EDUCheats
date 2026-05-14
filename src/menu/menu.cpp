#include "menu.hpp"
#include <Windows.h>
#include <cstdio>
#include <thread>
#include <chrono>

static void ClearConsole() {
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hCon == nullptr || hCon == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD count, written;
    if (!GetConsoleScreenBufferInfo(hCon, &csbi)) return;

    count = csbi.dwSize.X * csbi.dwSize.Y;
    COORD origin{ 0, 0 };
    FillConsoleOutputCharacterA(hCon, ' ', count, origin, &written);
    FillConsoleOutputAttribute(hCon, csbi.wAttributes, count, origin, &written);
    SetConsoleCursorPosition(hCon, origin);
}

static const char* BoolStr(bool v) { return v ? "ON " : "OFF"; }

void Menu::Print() const {
    ClearConsole();
    puts("  ___  ___  _   _  ___  _                _        ");
    puts(" | __||   \\| | | |/ __|| |_   ___  __ _ | |_  ___ ");
    puts(" | _| | |) | |_| | (__ | ' \\ / -_)/ _` ||  _|(_-< ");
    puts(" |___||___/ \\___/ \\___||_||_|\\___|\\__,_| \\__|/__/ ");
    puts("                                          v1.0\n");

    if (!m_visible) {
        puts("  [INSERT] Show menu    [END] Exit\n");
        return;
    }

    puts("  ----------------------------------------");
    printf("  [F1] Box ESP          [%s]\n", BoolStr(m_cfg.enabled));
    printf("  [F2] Name ESP         [%s]\n", BoolStr(m_cfg.nameESP));
    printf("  [F3] Health Bar       [%s]\n", BoolStr(m_cfg.healthBar));
    printf("  [F4] Box Color        [%s]\n", m_cfg.colorMode == 0 ? "TEAM" : "RED ");
    puts("  ----------------------------------------");
    puts("  [INSERT] Hide menu    [END] Exit\n");
}

void Menu::Run(std::atomic<bool>& running) {
    SetConsoleTitleA("EDUCheats");
    // Bring console to front so it's not buried under the game window
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        SetForegroundWindow(consoleWnd);
    }

    Print();

    // Track previous key states for edge detection
    bool prevInsert = false, prevF1 = false, prevF2 = false,
         prevF3    = false, prevF4 = false, prevEnd = false;

    auto pressed = [](int vk, bool& prev) -> bool {
        bool cur = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool edge = cur && !prev;
        prev = cur;
        return edge;
    };

    while (running) {
        if (pressed(VK_END, prevEnd)) {
            running = false;
            break;
        }

        if (pressed(VK_INSERT, prevInsert)) {
            m_visible = !m_visible;
            Print();
        }

        if (m_visible) {
            if (pressed(VK_F1, prevF1)) {
                m_cfg.enabled = !m_cfg.enabled.load();
                Print();
            }
            if (pressed(VK_F2, prevF2)) {
                m_cfg.nameESP = !m_cfg.nameESP.load();
                Print();
            }
            if (pressed(VK_F3, prevF3)) {
                m_cfg.healthBar = !m_cfg.healthBar.load();
                Print();
            }
            if (pressed(VK_F4, prevF4)) {
                m_cfg.colorMode = (m_cfg.colorMode == 0) ? 1 : 0;
                Print();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    FreeConsole();
}
