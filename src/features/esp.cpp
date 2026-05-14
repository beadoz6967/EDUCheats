#include "esp.hpp"
#include "../offsets.hpp"
#include <objbase.h>    // IUnknown — must precede gdiplus when WIN32_LEAN_AND_MEAN is set
#include <dwmapi.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

static constexpr wchar_t kOverlayClass[] = L"EDUCheats_Overlay";
static ESPOverlay* g_overlay = nullptr;

// 1 Source unit ≈ 1.905 cm. Conversion used for distance ESP display.
static constexpr float kUnitsToMeters = 0.01905f;

ESPOverlay::~ESPOverlay() {
    DestroyBackBuffer();
    if (m_penEnemy)      { DeleteObject(m_penEnemy);      m_penEnemy      = nullptr; }
    if (m_penTeam)       { DeleteObject(m_penTeam);       m_penTeam       = nullptr; }
    if (m_brushBlack)    { DeleteObject(m_brushBlack);    m_brushBlack    = nullptr; }
    if (m_brushHealthBg) { DeleteObject(m_brushHealthBg); m_brushHealthBg = nullptr; }
    DeleteCriticalSection(&m_dataLock);
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (g_overlay) {
            g_overlay->Paint(hdc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        // Suppress default erase — double-buffer fills its own background
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ESPOverlay::CreateOverlayWindow() {
    // Find the CS2 window for sizing and positioning
    HWND gameWnd = FindWindowW(nullptr, L"Counter-Strike 2");
    RECT gameRect{};
    if (gameWnd) {
        GetWindowRect(gameWnd, &gameRect);
        m_winW = gameRect.right  - gameRect.left;
        m_winH = gameRect.bottom - gameRect.top;
    } else {
        m_winW = GetSystemMetrics(SM_CXSCREEN);
        m_winH = GetSystemMetrics(SM_CYSCREEN);
        gameRect = { 0, 0, m_winW, m_winH };
    }

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = OverlayWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kOverlayClass;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kOverlayClass, L"",
        WS_POPUP,
        gameRect.left, gameRect.top, m_winW, m_winH,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!m_hwnd) {
        return;
    }

    // Black = transparent color key
    SetLayeredWindowAttributes(m_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    // Extend glass frame for clean compositing
    MARGINS margins{ -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void ESPOverlay::RefreshWindowBounds() {
    HWND gameWnd = FindWindowW(nullptr, L"Counter-Strike 2");
    if (!gameWnd) return;

    RECT r{};
    if (!GetWindowRect(gameWnd, &r)) return;

    int w = r.right  - r.left;
    int h = r.bottom - r.top;
    if (w == m_winW && h == m_winH) return;

    m_winW = w;
    m_winH = h;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, r.left, r.top, w, h, SWP_NOACTIVATE);

        // Rebuild back buffer immediately so the next frame paints to the
        // correct dimensions rather than the previous (stale) bitmap.
        HDC windowDC = GetDC(m_hwnd);
        if (windowDC) {
            EnsureBackBuffer(windowDC);
            ReleaseDC(m_hwnd, windowDC);
        }
    }
}

void ESPOverlay::DestroyBackBuffer() {
    if (m_memDC) {
        if (m_oldBmp) {
            SelectObject(m_memDC, m_oldBmp);
            m_oldBmp = nullptr;
        }
        if (m_memBmp) {
            DeleteObject(m_memBmp);
            m_memBmp = nullptr;
        }
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
    m_bufW = 0;
    m_bufH = 0;
}

void ESPOverlay::EnsureBackBuffer(HDC windowDC) {
    if (m_memDC && m_bufW == m_winW && m_bufH == m_winH) return;

    DestroyBackBuffer();

    m_memDC  = CreateCompatibleDC(windowDC);
    if (!m_memDC) return;

    m_memBmp = CreateCompatibleBitmap(windowDC, m_winW, m_winH);
    if (!m_memBmp) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
        return;
    }

    m_oldBmp = static_cast<HBITMAP>(SelectObject(m_memDC, m_memBmp));
    m_bufW   = m_winW;
    m_bufH   = m_winH;
}

void ESPOverlay::ReadViewMatrix() {
    m_mem.ReadBuffer(m_clientBase + offsets::dwViewMatrix,
                     &m_vMatrix, sizeof(ViewMatrix));
}

bool ESPOverlay::WorldToScreen(const Vector3& pos, Vector2& out) const {
    const float* m = &m_vMatrix.m[0][0];

    float w = m[12]*pos.x + m[13]*pos.y + m[14]*pos.z + m[15];
    if (w < 0.001f) return false;

    float x = m[0]*pos.x + m[1]*pos.y + m[2]*pos.z + m[3];
    float y = m[4]*pos.x + m[5]*pos.y + m[6]*pos.z + m[7];

    out.x = (m_winW / 2.f) + (x / w) * (m_winW / 2.f);
    out.y = (m_winH / 2.f) - (y / w) * (m_winH / 2.f);
    return true;
}

void ESPOverlay::UpdatePlayers(const PlayerESPData players[64], int count, int localTeam) {
    EnterCriticalSection(&m_dataLock);
    std::memcpy(m_players, players, sizeof(PlayerESPData) * count);
    m_playerCount = count;
    m_localTeam   = localTeam;
    LeaveCriticalSection(&m_dataLock);

    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ESPOverlay::DrawHealthBar(HDC hdc, const PlayerESPData& p,
                                int boxX, int boxY, int boxH) {
    int barW  = 4;
    int barX  = boxX - barW - 2;
    int barH  = boxH;
    int fillH = static_cast<int>(barH * (p.health / 100.f));

    RECT bgRect{ barX, boxY, barX + barW, boxY + barH };
    FillRect(hdc, &bgRect, m_brushHealthBg);

    // Health fill — green → yellow → red
    int r = static_cast<int>(255 * (1.f - p.health / 100.f));
    int g = static_cast<int>(255 * (p.health / 100.f));
    HBRUSH fillBrush = CreateSolidBrush(RGB(r, g, 0));
    RECT fillRect{ barX, boxY + barH - fillH, barX + barW, boxY + barH };
    FillRect(hdc, &fillRect, fillBrush);
    DeleteObject(fillBrush);
}

void ESPOverlay::DrawHpNumber(HDC hdc, const PlayerESPData& p,
                               int boxX, int boxY, int boxH) {
    // Right edge of the health bar; bar lives 2px + 4px left of boxX
    int barX = boxX - 4 - 2;
    int fillH = static_cast<int>(boxH * (p.health / 100.f));
    int topOfFill = boxY + boxH - fillH;

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", std::clamp(p.health, 0, 100));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutA(hdc, barX + 6, topOfFill - 1, buf, static_cast<int>(std::strlen(buf)));
}

void ESPOverlay::DrawName(HDC hdc, const PlayerESPData& p, int centerX, int topY) {
    if (p.name.empty()) return;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, p.name.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return;
    std::wstring wname(wlen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, p.name.c_str(), -1, wname.data(), wlen);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    SIZE sz{};
    GetTextExtentPoint32W(hdc, wname.c_str(), static_cast<int>(wname.size()), &sz);
    TextOutW(hdc, centerX - sz.cx / 2, topY - sz.cy - 2,
             wname.c_str(), static_cast<int>(wname.size()));
}

void ESPOverlay::DrawDistance(HDC hdc, const PlayerESPData& p, int centerX, int bottomY) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "[%.1fm]", p.distance);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));

    int len = static_cast<int>(std::strlen(buf));
    SIZE sz{};
    GetTextExtentPoint32A(hdc, buf, len, &sz);
    TextOutA(hdc, centerX - sz.cx / 2, bottomY + 2, buf, len);
}

void ESPOverlay::DrawCornerBox(HDC hdc, const PlayerESPData& /*p*/,
                                int x, int y, int w, int h) {
    int cornerLen = h / 4;
    if (cornerLen < 3) cornerLen = 3;

    const int x2 = x + w;
    const int y2 = y + h;

    auto drawLine = [&](int x1, int y1, int x2_, int y2_) {
        MoveToEx(hdc, x1, y1, nullptr);
        LineTo  (hdc, x2_, y2_);
    };

    // Top-left
    drawLine(x, y, x + cornerLen, y);
    drawLine(x, y, x,             y + cornerLen);

    // Top-right
    drawLine(x2, y, x2 - cornerLen, y);
    drawLine(x2, y, x2,             y + cornerLen);

    // Bottom-left
    drawLine(x, y2, x + cornerLen, y2);
    drawLine(x, y2, x,             y2 - cornerLen);

    // Bottom-right
    drawLine(x2, y2, x2 - cornerLen, y2);
    drawLine(x2, y2, x2,             y2 - cornerLen);
}

void ESPOverlay::RenderFrame(HDC hdc, int /*localTeam*/) {
    // Fill back buffer black to preserve color-key transparency
    RECT rc{ 0, 0, m_winW, m_winH };
    FillRect(hdc, &rc, m_brushBlack);

    if (!m_cfg.enabled) return;

    ReadViewMatrix();

    EnterCriticalSection(&m_dataLock);
    for (int i = 0; i < m_playerCount; ++i) {
        const PlayerESPData& p = m_players[i];
        if (!p.alive) continue;

        Vector2 feet{}, head{};
        if (!WorldToScreen(p.origin,  feet)) continue;
        if (!WorldToScreen(p.headPos, head)) continue;

        int boxH = static_cast<int>(feet.y - head.y);
        if (boxH < 5) continue;

        int boxW = boxH / 2;
        int x    = static_cast<int>(feet.x) - boxW / 2;
        int y    = static_cast<int>(head.y);

        HPEN   pen      = (m_cfg.colorMode == 0 && !p.isEnemy) ? m_penTeam : m_penEnemy;
        HPEN   oldPen   = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));

        DrawCornerBox(hdc, p, x, y, boxW, boxH);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);

        if (m_cfg.healthBar)   DrawHealthBar(hdc, p, x, y, boxH);
        if (m_cfg.hpNumbers)   DrawHpNumber (hdc, p, x, y, boxH);
        if (m_cfg.nameESP)     DrawName     (hdc, p, x + boxW / 2, y);
        if (m_cfg.distanceESP) DrawDistance (hdc, p, x + boxW / 2, y + boxH);
    }
    LeaveCriticalSection(&m_dataLock);
}

void ESPOverlay::Paint(HDC hdc) {
    EnsureBackBuffer(hdc);

    if (!m_memDC) {
        // Back-buffer creation failed — paint direct to window as fallback
        RenderFrame(hdc, m_localTeam);
        return;
    }

    RefreshWindowBounds();

    RenderFrame(m_memDC, m_localTeam);

    BitBlt(hdc, 0, 0, m_winW, m_winH, m_memDC, 0, 0, SRCCOPY);
}

void ESPOverlay::Stop() {
    if (m_hwnd) PostMessageW(m_hwnd, WM_DESTROY, 0, 0);
}

void ESPOverlay::Run(int localTeam) {
    m_localTeam = localTeam;
    g_overlay   = this;

    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    CreateOverlayWindow();

    if (!m_hwnd) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        g_overlay = nullptr;
        return;
    }

    m_penEnemy      = CreatePen(PS_SOLID, 1, RGB(255, 60, 60));
    m_penTeam       = CreatePen(PS_SOLID, 1, RGB(60, 150, 255));
    m_brushBlack    = CreateSolidBrush(RGB(0, 0, 0));
    m_brushHealthBg = CreateSolidBrush(RGB(40, 40, 40));

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_overlay = nullptr;
    Gdiplus::GdiplusShutdown(gdiplusToken);
}
