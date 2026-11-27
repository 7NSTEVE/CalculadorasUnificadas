#pragma once

#include "../core/camera.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/document_model.hpp"
#include "../render/d2d_renderer.hpp"
#include "../../ui_glass/gdi_translucent.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace DocStudio {

// Dedicated Secondary Projector / Classroom Window (F-199, F-209):
// Projects clean canvas/PDF output with presenterMode=true (hiding private teacher layers)
// and supports true snapshot Freeze Projector so the instructor can prepare next steps privately.
class SecondaryProjectorWindow {
public:
    ~SecondaryProjectorWindow() { Close(); }

    bool IsOpen() const noexcept { return hwnd_ != nullptr && IsWindow(hwnd_); }
    HWND Hwnd() const noexcept { return hwnd_; }
    bool IsFrozen() const noexcept { return frozen_; }
    void SetFrozen(bool f) {
        if (f && !frozen_ && doc_ && cam_) {
            std::vector<std::uint8_t> snap;
            if (CudocStorage::SerializeToBytes(*doc_, snap, nullptr) &&
                CudocStorage::DeserializeFromBytes(snap.data(), snap.size(), frozenDoc_, nullptr)) {
                frozenCam_ = *cam_;
                hasFrozenSnapshot_ = true;
            }
        }
        frozen_ = f;
        if (IsOpen()) {
            SetWindowTextW(hwnd_, frozen_
                ? L"CuadernU \u2014 Proyector [CONGELADO]"
                : L"CuadernU \u2014 Proyector (Vista Limpia)");
            if (!frozen_) RefreshNow();
        }
    }

    bool Open(HINSTANCE hInst, HWND owner,
              const DocumentModel* doc,
              const Camera2D* cam,
              PageRenderCache* cache,
              const ViewportOverlayState* baseOverlay,
              bool backgroundStealth = false)
    {
        doc_ = doc;
        cam_ = cam;
        cache_ = cache;
        if (baseOverlay) {
            overlay_ = *baseOverlay;
            SanitizeOverlay();
        }
        if (IsOpen()) {
            UiGlass::ApplyGoldenGateWindowFrame(hwnd_, overlay_.darkTheme);
            if (!backgroundStealth) {
                ShowWindow(hwnd_, SW_SHOWNORMAL);
                SetForegroundWindow(hwnd_);
            }
            RefreshNow();
            return true;
        }

        static bool registered = false;
        if (!registered) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = WndProc;
            wc.hInstance = hInst;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = L"CuadernUProjectorWndClass";
            RegisterClassExW(&wc);
            registered = true;
        }

        // Open on secondary monitor when available, else clean 1280x720 window
        int wx = backgroundStealth ? 64 : CW_USEDEFAULT;
        int wy = backgroundStealth ? 64 : CW_USEDEFAULT;
        int ww = 1280, wh = 720;
        if (!backgroundStealth) {
            struct MonPick { RECT primary{}; RECT secondary{}; bool hasSecondary = false; } mp{};
            EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT lprc, LPARAM lp) -> BOOL {
                auto* p = reinterpret_cast<MonPick*>(lp);
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(hMon, &mi)) {
                    if (mi.dwFlags & MONITORINFOF_PRIMARY) p->primary = *lprc;
                    else if (!p->hasSecondary) { p->secondary = *lprc; p->hasSecondary = true; }
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&mp));
            if (mp.hasSecondary) {
                wx = mp.secondary.left;
                wy = mp.secondary.top;
                ww = mp.secondary.right - mp.secondary.left;
                wh = mp.secondary.bottom - mp.secondary.top;
            }
        }

        const DWORD exStyle = backgroundStealth ? (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE) : 0u;
        hwnd_ = CreateWindowExW(
            exStyle, L"CuadernUProjectorWndClass", L"CuadernU \u2014 Proyector (Vista Limpia)",
            WS_OVERLAPPEDWINDOW, wx, wy, ww, wh, backgroundStealth ? nullptr : owner, nullptr, hInst, this);
        if (!hwnd_) return false;
        UiGlass::ApplyGoldenGateWindowFrame(hwnd_, overlay_.darkTheme);
        if (backgroundStealth) {
            if (HMODULE hDwm = LoadLibraryW(L"dwmapi.dll")) {
                using DwmSetAttrFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
                if (auto fn = reinterpret_cast<DwmSetAttrFn>(GetProcAddress(hDwm, "DwmSetWindowAttribute"))) {
                    BOOL cloak = TRUE;
                    fn(hwnd_, 13 /*DWMWA_CLOAK*/, &cloak, sizeof(cloak));
                }
                FreeLibrary(hDwm);
            }
            ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        } else {
            ShowWindow(hwnd_, SW_SHOWNORMAL);
        }
        UpdateWindow(hwnd_);
        return true;
    }

    void SyncFromMain(
        const DocumentModel* doc,
        const Camera2D* cam,
        PageRenderCache* cache,
        const ViewportOverlayState& mainOverlay)
    {
        if (!IsOpen() || frozen_) return;
        doc_ = doc;
        cam_ = cam;
        cache_ = cache;
        const bool darkChanged = (overlay_.darkTheme != mainOverlay.darkTheme);
        overlay_ = mainOverlay;
        SanitizeOverlay();
        if (darkChanged) UiGlass::ApplyGoldenGateWindowFrame(hwnd_, overlay_.darkTheme);
        RefreshNow();
    }

    void RefreshNow() {
        if (IsOpen()) InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void Close() {
        if (hwnd_) {
            HWND h = hwnd_;
            hwnd_ = nullptr;
            if (IsWindow(h)) DestroyWindow(h);
        }
        hasFrozenSnapshot_ = false;
        frozen_ = false;
        renderer_.DiscardDeviceResources();
    }

private:
    void SanitizeOverlay() noexcept {
        overlay_.presenterMode = true;
        overlay_.showSelectionHandles = false;
        overlay_.floatingOverlayBgra = nullptr;
        overlay_.searchMatches = nullptr;
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        SecondaryProjectorWindow* self = reinterpret_cast<SecondaryProjectorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<SecondaryProjectorWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }
        if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);
        switch (msg) {
        case WM_SIZE:
            self->renderer_.Resize(LOWORD(lParam), HIWORD(lParam));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            if (suggested) {
                SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                    suggested->right - suggested->left, suggested->bottom - suggested->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const DocumentModel* activeDoc = (self->frozen_ && self->hasFrozenSnapshot_) ? &self->frozenDoc_ : self->doc_;
            const Camera2D* activeCam = (self->frozen_ && self->hasFrozenSnapshot_) ? &self->frozenCam_ : self->cam_;
            if (activeDoc && activeCam && rc.right > 0 && rc.bottom > 0) {
                self->renderer_.AttachHwnd(hwnd, rc.right, rc.bottom);
                self->renderer_.RenderViewport(
                    *activeDoc, *activeCam, rc.right, rc.bottom,
                    self->cache_, 1, nullptr, &self->overlay_);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == 'F') {
                self->SetFrozen(!self->frozen_);
                return 0;
            }
            break;
        case WM_DESTROY:
            self->renderer_.DiscardDeviceResources();
            self->hwnd_ = nullptr;
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    HWND hwnd_ = nullptr;
    bool frozen_ = false;
    bool hasFrozenSnapshot_ = false;
    DocumentModel frozenDoc_{};
    Camera2D frozenCam_{};
    const DocumentModel* doc_ = nullptr;
    const Camera2D* cam_ = nullptr;
    PageRenderCache* cache_ = nullptr;
    ViewportOverlayState overlay_{};
    D2DRenderer renderer_;
};

} // namespace DocStudio
#endif
