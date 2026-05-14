#include "esp.hpp"
#include "../offsets.hpp"
#include <objbase.h>    // IUnknown — must precede gdiplus when WIN32_LEAN_AND_MEAN is set
#include <dwmapi.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

static constexpr wchar_t kOverlayClass[] = L"EDUCheats_Overlay";
static ESPOverlay* g_overlay = nullptr;

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
    if (!RegisterClassExW(&wc)) {
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

    // Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(40, 40, 40));
    RECT bgRect{ barX, boxY, barX + barW, boxY + barH };
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    // Health fill — green → yellow → red
    int r = static_cast<int>(255 * (1.f - p.health / 100.f));
    int g = static_cast<int>(255 * (p.health / 100.f));
    HBRUSH fillBrush = CreateSolidBrush(RGB(r, g, 0));
    RECT fillRect{ barX, boxY + barH - fillH, barX + barW, boxY + barH };
    FillRect(hdc, &fillRect, fillBrush);
    DeleteObject(fillBrush);
}

void ESPOverlay::DrawName(HDC hdc, const PlayerESPData& p, int centerX, int topY) {
    if (p.name.empty()) return;

    std::wstring wname(p.name.begin(), p.name.end());
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    SIZE sz{};
    GetTextExtentPoint32W(hdc, wname.c_str(), static_cast<int>(wname.size()), &sz);
    TextOutW(hdc, centerX - sz.cx / 2, topY - sz.cy - 2,
             wname.c_str(), static_cast<int>(wname.size()));
}

void ESPOverlay::DrawBoxESP(HDC hdc, const PlayerESPData& p,
                             int /*winW*/, int /*winH*/) {
    Vector2 feet{}, head{};
    if (!WorldToScreen(p.origin, feet))  return;
    if (!WorldToScreen(p.headPos, head)) return;

    int boxH = static_cast<int>(feet.y - head.y);
    if (boxH < 5) return;

    int boxW = boxH / 2;
    int x    = static_cast<int>(feet.x) - boxW / 2;
    int y    = static_cast<int>(head.y);

    COLORREF color;
    if (m_cfg.colorMode == 0) {
        color = p.isEnemy ? RGB(255, 60, 60) : RGB(60, 150, 255);
    } else {
        color = RGB(255, 60, 60);
    }

    HPEN pen   = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));

    Rectangle(hdc, x, y, x + boxW, y + boxH);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);

    if (m_cfg.healthBar) DrawHealthBar(hdc, p, x, y, boxH);
    if (m_cfg.nameESP)   DrawName(hdc, p, x + boxW / 2, y);
}

void ESPOverlay::RenderFrame(HDC hdc, int localTeam) {
    if (!m_cfg.enabled) return;

    // Clear to black (transparent via color key)
    RECT rc{ 0, 0, m_winW, m_winH };
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &rc, black);
    DeleteObject(black);

    ReadViewMatrix();

    EnterCriticalSection(&m_dataLock);
    for (int i = 0; i < m_playerCount; ++i) {
        const PlayerESPData& p = m_players[i];
        if (!p.alive) continue;
        DrawBoxESP(hdc, p, m_winW, m_winH);
    }
    LeaveCriticalSection(&m_dataLock);
}

void ESPOverlay::Paint(HDC hdc) {
    RenderFrame(hdc, m_localTeam);
}

void ESPOverlay::Stop() {
    if (m_hwnd) PostMessageW(m_hwnd, WM_DESTROY, 0, 0);
}

void ESPOverlay::Run(int localTeam) {
    InitializeCriticalSection(&m_dataLock);
    m_localTeam = localTeam;
    g_overlay   = this;

    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    CreateOverlayWindow();

    if (!m_hwnd) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        DeleteCriticalSection(&m_dataLock);
        g_overlay = nullptr;
        return;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    DeleteCriticalSection(&m_dataLock);
}
