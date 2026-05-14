#include "menu.hpp"
#include <Windows.h>
#include <cstdio>
#include <thread>
#include <chrono>

static void ClearConsole() {
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD count, written;
    GetConsoleScreenBufferInfo(hCon, &csbi);
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
    // Allocate a console for the menu
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    SetConsoleTitleA("EDUCheats");

    HANDLE hCon = GetStdHandle(STD_INPUT_HANDLE);

    Print();

    while (running) {
        // Non-blocking key poll at ~20Hz
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            running = false;
            break;
        }

        if (GetAsyncKeyState(VK_INSERT) & 1) {
            m_visible = !m_visible;
            Print();
        }

        if (m_visible) {
            if (GetAsyncKeyState(VK_F1) & 1) {
                m_cfg.enabled = !m_cfg.enabled.load();
                Print();
            }
            if (GetAsyncKeyState(VK_F2) & 1) {
                m_cfg.nameESP = !m_cfg.nameESP.load();
                Print();
            }
            if (GetAsyncKeyState(VK_F3) & 1) {
                m_cfg.healthBar = !m_cfg.healthBar.load();
                Print();
            }
            if (GetAsyncKeyState(VK_F4) & 1) {
                m_cfg.colorMode = (m_cfg.colorMode == 0) ? 1 : 0;
                Print();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    FreeConsole();
}
