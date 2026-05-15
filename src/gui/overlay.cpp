#include "overlay.hpp"
#include "esp_render.hpp"
#include "menu_ui.hpp"
#include "../theme.hpp"
#include "../offsets.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

// ImGui Win32 backend exports its own WndProc handler — forward to it for
// keyboard/mouse before our own logic runs.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static constexpr wchar_t kOverlayClass[] = L"EDUCheats_DX11";
static Overlay* g_overlay = nullptr;

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_overlay && g_overlay->m_device && wp != SIZE_MINIMIZED) {
            g_overlay->DestroyRenderTarget();
            auto* sc = static_cast<IDXGISwapChain*>(g_overlay->m_swapchain);
            sc->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            g_overlay->CreateRenderTarget();
            g_overlay->m_winW = LOWORD(lp);
            g_overlay->m_winH = HIWORD(lp);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

Overlay::Overlay(const Memory& mem, uintptr_t clientBase,
                 ESPConfig& cfg, AimbotConfig& ab, GameState& state, Config& persist)
    : m_mem(mem), m_clientBase(clientBase),
      m_cfg(cfg), m_ab(ab), m_state(state), m_persist(persist) {}

Overlay::~Overlay() {
    if (m_device) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        DestroyDevice();
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        UnregisterClassW(kOverlayClass, GetModuleHandleW(nullptr));
    }
}

bool Overlay::CreateOverlayWindow() {
    // Find CS2 by both class and title for robustness.
    // CS2 uses class "SDL_app" — title alone fails on some configurations.
    HWND gameWnd = FindWindowW(L"SDL_app", L"Counter-Strike 2");
    if (!gameWnd) gameWnd = FindWindowW(nullptr, L"Counter-Strike 2");

    RECT gameRect{};
    if (gameWnd) {
        GetWindowRect(gameWnd, &gameRect);
        m_winW = gameRect.right  - gameRect.left;
        m_winH = gameRect.bottom - gameRect.top;
        printf("[Overlay] CS2 window found at %dx%d\n", m_winW, m_winH);
    } else {
        m_winW = GetSystemMetrics(SM_CXSCREEN);
        m_winH = GetSystemMetrics(SM_CYSCREEN);
        gameRect = { 0, 0, m_winW, m_winH };
        printf("[Overlay] CS2 window not found — falling back to virtual screen %dx%d\n", m_winW, m_winH);
    }

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = OverlayWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kOverlayClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        printf("[Overlay] RegisterClassExW failed (0x%lX)\n", GetLastError());
        return false;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kOverlayClass, L"EDUCheats",
        WS_POPUP,
        gameRect.left, gameRect.top, m_winW, m_winH,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!m_hwnd) {
        printf("[Overlay] CreateWindowExW failed (0x%lX)\n", GetLastError());
        return false;
    }

    // Alpha-channel layering — DX11 clears to {0,0,0,0} and DWM composites
    // properly. Avoids color-key issues that broke the previous GDI overlay.
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    // Extend DWM glass into the whole client area so the transparent
    // backdrop survives compositing.
    MARGINS margins{ -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

bool Overlay::CreateDeviceAndSwapchain() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount       = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow      = m_hwnd;
    sd.SampleDesc.Count  = 1;
    sd.SampleDesc.Quality= 0;
    sd.Windowed          = TRUE;
    sd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL achieved{};

    ID3D11Device*        dev   = nullptr;
    ID3D11DeviceContext* ctx   = nullptr;
    IDXGISwapChain*      swap  = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        &sd, &swap, &dev, &achieved, &ctx);

    if (FAILED(hr)) {
        // Fallback to WARP renderer for systems without a usable GPU driver
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
            &sd, &swap, &dev, &achieved, &ctx);
    }

    if (FAILED(hr)) {
        printf("[Overlay] D3D11CreateDeviceAndSwapChain failed (0x%lX)\n", hr);
        return false;
    }

    m_device     = dev;
    m_deviceCtx  = ctx;
    m_swapchain  = swap;
    CreateRenderTarget();
    return true;
}

void Overlay::CreateRenderTarget() {
    auto* swap = static_cast<IDXGISwapChain*>(m_swapchain);
    auto* dev  = static_cast<ID3D11Device*>(m_device);

    ID3D11Texture2D* back = nullptr;
    if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return;

    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(dev->CreateRenderTargetView(back, nullptr, &rtv))) {
        back->Release();
        printf("[Overlay] CreateRenderTargetView failed\n");
        return;
    }
    back->Release();
    m_renderTargetView = rtv;
}

void Overlay::DestroyRenderTarget() {
    if (m_renderTargetView) {
        static_cast<ID3D11RenderTargetView*>(m_renderTargetView)->Release();
        m_renderTargetView = nullptr;
    }
}

void Overlay::DestroyDevice() {
    DestroyRenderTarget();
    if (m_swapchain)  { static_cast<IDXGISwapChain*>(m_swapchain)->Release();      m_swapchain  = nullptr; }
    if (m_deviceCtx)  { static_cast<ID3D11DeviceContext*>(m_deviceCtx)->Release(); m_deviceCtx  = nullptr; }
    if (m_device)     { static_cast<ID3D11Device*>(m_device)->Release();           m_device     = nullptr; }
}

void Overlay::RefreshGameWindowBounds() {
    HWND gameWnd = FindWindowW(L"SDL_app", L"Counter-Strike 2");
    if (!gameWnd) gameWnd = FindWindowW(nullptr, L"Counter-Strike 2");
    if (!gameWnd) return;

    RECT r{};
    if (!GetWindowRect(gameWnd, &r)) return;

    int w = r.right  - r.left;
    int h = r.bottom - r.top;
    if (w == m_winW && h == m_winH && m_hwnd) return;

    m_winW = w;
    m_winH = h;
    if (m_hwnd)
        SetWindowPos(m_hwnd, HWND_TOPMOST, r.left, r.top, w, h, SWP_NOACTIVATE);
}

void Overlay::ApplyClickThrough(bool clickThrough) {
    if (!m_hwnd) return;
    if (m_clickThrough == clickThrough) return;

    LONG_PTR style = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if (clickThrough) {
        style |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    } else {
        style &= ~(LONG_PTR)(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }
    SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, style);
    if (!clickThrough) SetForegroundWindow(m_hwnd);
    m_clickThrough = clickThrough;
}

void Overlay::ToggleMenu() {
    m_menuVisible = !m_menuVisible;
    ApplyClickThrough(!m_menuVisible);
}

void Overlay::PushPlayers(const PlayerESPData players[64], int count,
                           int localTeam, const ViewMatrix& view) {
    std::lock_guard<std::mutex> lk(m_lock);
    for (int i = 0; i < count; ++i) m_players[i] = players[i];
    m_playerCount = count;
    m_localTeam   = localTeam;
    m_view        = view;
}

void Overlay::RenderFrame() {
    auto* ctx  = static_cast<ID3D11DeviceContext*>(m_deviceCtx);
    auto* swap = static_cast<IDXGISwapChain*>(m_swapchain);
    auto* rtv  = static_cast<ID3D11RenderTargetView*>(m_renderTargetView);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Copy snapshot under lock — render without holding it so PushPlayers never blocks for a full frame
    PlayerESPData snapPlayers[64];
    int snapCount, snapTeam;
    ViewMatrix snapView;
    {
        std::lock_guard<std::mutex> lk(m_lock);
        for (int i = 0; i < m_playerCount; ++i) snapPlayers[i] = m_players[i];
        snapCount = m_playerCount;
        snapTeam  = m_localTeam;
        snapView  = m_view;
    }
    if (m_cfg.enabled.load()) {
        esp_render::DrawAll(snapPlayers, snapCount, snapTeam,
                            snapView, m_winW, m_winH, m_cfg);
    }

    if (m_menuVisible) {
        menu_ui::Draw(m_cfg, m_ab, m_state, m_persist, m_menuVisible);
    }

    ImGui::Render();

    float clear[4]{ 0.f, 0.f, 0.f, 0.f }; // fully transparent
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->ClearRenderTargetView(rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present with syncInterval=0 to avoid VSync wait and reduce input latency.
    // This makes the overlay update as soon as frames are ready (may cause tearing).
    swap->Present(0, 0);
}

void Overlay::Run(std::atomic<bool>& running) {
    g_overlay = this;

    if (!CreateOverlayWindow())       { g_overlay = nullptr; return; }
    if (!CreateDeviceAndSwapchain())  { g_overlay = nullptr; return; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Replace the blurry 13px bitmap default with Segoe UI at DPI-aware size
    {
        UINT dpi = GetDpiForWindow(m_hwnd);
        if (dpi < 72) dpi = 96;
        float sz = floorf(16.f * static_cast<float>(dpi) / 96.f);
        ImFontConfig fc;
        fc.OversampleH = 3;
        fc.OversampleV = 1;
        char path[MAX_PATH];
        ExpandEnvironmentStringsA("%SystemRoot%\\Fonts\\segoeui.ttf", path, MAX_PATH);
        if (!io.Fonts->AddFontFromFileTTF(path, sz, &fc))
            io.Fonts->AddFontDefault();
    }

    theme::ApplyEducanetStyle();

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(m_device),
                        static_cast<ID3D11DeviceContext*>(m_deviceCtx));

    printf("[Overlay] DX11 + ImGui initialized — entering render loop\n");

    MSG msg{};
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;

        // INSERT toggles menu — polled here so it works regardless of focus
        bool curIns = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (curIns && !m_prevInsert) ToggleMenu();
        m_prevInsert = curIns;

        if (GetAsyncKeyState(VK_END) & 0x8000) {
            running = false;
            break;
        }

        RefreshGameWindowBounds();
        RenderFrame();
    }

    printf("[Overlay] Render loop exited\n");
}
