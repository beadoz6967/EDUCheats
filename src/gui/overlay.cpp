#include "overlay.hpp"
#include "esp_render.hpp"
#include "menu_ui.hpp"
#include "../theme.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#include <MinHook.h>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ---------------------------------------------------------------------------
// File-scope state — all accessed on CS2's render thread (hkPresent) or
// protected by s_lock for the scan-thread-facing PushPlayers call.
// ---------------------------------------------------------------------------
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);

static PresentFn s_oPresent = nullptr;
static WNDPROC   s_oWndProc = nullptr;
static bool      s_hooked   = false;

// Snapshot written by scan thread, read by hkPresent
static std::mutex     s_lock;
static PlayerESPData  s_players[64]{};
static int            s_playerCount = 0;
static int            s_localTeam   = 0;
static ViewMatrix     s_view{};
static bool           s_menuVisible = false;

// Config refs (set during Install, valid until Uninstall)
static ESPConfig*    s_esp = nullptr;
static AimbotConfig* s_ab  = nullptr;
static GameState*    s_gs  = nullptr;
static Config*       s_cfg = nullptr;

// DX11 — we do NOT own device/ctx (game owns them); we DO own the RTV
static ID3D11Device*           s_device = nullptr;
static ID3D11DeviceContext*    s_ctx    = nullptr;
static ID3D11RenderTargetView* s_rtv    = nullptr;
static HWND                    s_hwnd   = nullptr;
static int                     s_winW   = 0;
static int                     s_winH   = 0;

// ---------------------------------------------------------------------------

static void RebuildRtv(IDXGISwapChain* pChain) {
    if (s_rtv) { s_rtv->Release(); s_rtv = nullptr; }

    ID3D11Texture2D* back = nullptr;
    if (FAILED(pChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))))
        return;

    D3D11_TEXTURE2D_DESC td{};
    back->GetDesc(&td);
    s_winW = static_cast<int>(td.Width);
    s_winH = static_cast<int>(td.Height);

    s_device->CreateRenderTargetView(back, nullptr, &s_rtv);
    back->Release();
}

static LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_INSERT)
        s_menuVisible = !s_menuVisible;

    // Forward all messages to ImGui so it can process input when menu is open.
    // Events still reach CS2 via CallWindowProcW so game input is unaffected.
    ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);

    // Release RTV before CS2 calls ResizeBuffers; hkPresent rebuilds it next frame
    if (msg == WM_SIZE && wp != SIZE_MINIMIZED) {
        if (s_rtv) { s_rtv->Release(); s_rtv = nullptr; }
    }

    return CallWindowProcW(s_oWndProc, hwnd, msg, wp, lp);
}

static HRESULT STDMETHODCALLTYPE hkPresent(IDXGISwapChain* pChain, UINT si, UINT fl) {
    // One-time init: pull device/ctx from the game's own swapchain
    static bool inited = false;
    if (!inited) {
        if (FAILED(pChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&s_device))))
            return s_oPresent(pChain, si, fl);

        s_device->GetImmediateContext(&s_ctx);

        RebuildRtv(pChain);

        DXGI_SWAP_CHAIN_DESC desc{};
        pChain->GetDesc(&desc);
        s_hwnd = desc.OutputWindow;

        s_oWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hkWndProc)));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        // Load Segoe UI at DPI-aware size
        UINT dpi = GetDpiForWindow(s_hwnd);
        if (dpi < 72) dpi = 96;
        float sz = floorf(16.f * static_cast<float>(dpi) / 96.f);
        char fontPath[MAX_PATH];
        ExpandEnvironmentStringsA("%SystemRoot%\\Fonts\\segoeui.ttf", fontPath, MAX_PATH);
        ImFontConfig fc;
        fc.OversampleH = 3;
        fc.OversampleV = 1;
        if (!io.Fonts->AddFontFromFileTTF(fontPath, sz, &fc))
            io.Fonts->AddFontDefault();

        theme::ApplyEducanetStyle();
        ImGui_ImplWin32_Init(s_hwnd);
        ImGui_ImplDX11_Init(s_device, s_ctx);

        inited = true;
    }

    // RTV becomes stale after ResizeBuffers — rebuild before rendering
    if (!s_rtv) {
        RebuildRtv(pChain);
        if (!s_rtv) return s_oPresent(pChain, si, fl); // skip frame, no target
    }

    // Grab snapshot outside the render hot path
    PlayerESPData snap[64]{};
    int snapCount = 0, snapTeam = 0;
    ViewMatrix snapView{};
    {
        std::lock_guard<std::mutex> lk(s_lock);
        snapCount = s_playerCount;
        snapTeam  = s_localTeam;
        snapView  = s_view;
        for (int i = 0; i < snapCount; ++i)
            snap[i] = s_players[i];
    }

    s_ctx->OMSetRenderTargets(1, &s_rtv, nullptr);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (s_esp && s_esp->enabled.load())
        esp_render::DrawAll(snap, snapCount, snapTeam, snapView, s_winW, s_winH, *s_esp);

    if (s_menuVisible && s_esp && s_ab && s_gs && s_cfg)
        menu_ui::Draw(*s_esp, *s_ab, *s_gs, *s_cfg, s_menuVisible);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return s_oPresent(pChain, si, fl);
}

// Returns the address of IDXGISwapChain::Present by creating a throwaway
// DX11 device + swapchain and reading vtable slot 8.
static void* GetPresentAddr() {
    HWND dummy = CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 8, 8,
                                 nullptr, nullptr, nullptr, nullptr);
    if (!dummy) return nullptr;

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount      = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow     = dummy;
    scd.SampleDesc.Count = 1;
    scd.Windowed         = TRUE;

    ID3D11Device*    dev{};
    IDXGISwapChain*  sc{};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &scd, &sc, &dev, nullptr, nullptr);

    if (FAILED(hr)) {
        // WARP fallback for headless/CI environments
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &scd, &sc, &dev, nullptr, nullptr);
    }

    if (FAILED(hr)) { DestroyWindow(dummy); return nullptr; }

    void* present = (*reinterpret_cast<void***>(sc))[8];
    sc->Release();
    dev->Release();
    DestroyWindow(dummy);
    return present;
}

// ---------------------------------------------------------------------------
// Overlay public methods
// ---------------------------------------------------------------------------

Overlay g_overlay;

void Overlay::Install(ESPConfig& esp, AimbotConfig& ab, GameState& gs, Config& cfg) {
    s_esp = &esp;
    s_ab  = &ab;
    s_gs  = &gs;
    s_cfg = &cfg;

    void* presentAddr = GetPresentAddr();
    if (!presentAddr) return;

    MH_Initialize();
    if (MH_CreateHook(presentAddr, reinterpret_cast<void*>(hkPresent),
                      reinterpret_cast<void**>(&s_oPresent)) == MH_OK) {
        MH_EnableHook(presentAddr);
        s_hooked = true;
    }
}

void Overlay::Uninstall() {
    if (!s_hooked) return;

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    s_hooked = false;

    // Restore CS2's WndProc before tearing down ImGui
    if (s_hwnd && s_oWndProc) {
        SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_oWndProc));
        s_oWndProc = nullptr;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (s_rtv) { s_rtv->Release(); s_rtv = nullptr; }
    // Release the AddRef from GetImmediateContext; device is game-owned, not ours
    if (s_ctx) { s_ctx->Release(); s_ctx = nullptr; }
    s_device = nullptr;
}

void Overlay::PushPlayers(const PlayerESPData* players, int count,
                          int localTeam, const ViewMatrix& view) {
    std::lock_guard<std::mutex> lk(s_lock);
    s_playerCount = count;
    s_localTeam   = localTeam;
    s_view        = view;
    for (int i = 0; i < count; ++i)
        s_players[i] = players[i];
}

bool Overlay::IsRunning() const {
    return s_hooked;
}
