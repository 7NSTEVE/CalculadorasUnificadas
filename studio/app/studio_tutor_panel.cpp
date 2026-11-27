#include "studio_tutor_panel.hpp"
#include "../core/utf_conv.hpp"
#include "../tutor/tutor_whiteboard_protocol.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include "../pdf/winrt_adapters.hpp"

namespace DocStudio::Tutor {

using nlohmann::json;

#ifdef _WIN32
namespace {

constexpr wchar_t kTutorWndClass[] = L"CuadernU_AudiovisualTutorWnd";
constexpr std::uint64_t kTutorHostId = 990001ull;
constexpr UINT kWmTutorSidecarReady = WM_APP + 101;
// Mientras arranca el motor, la ventana se repinta cada segundo para mostrar cuánto lleva
constexpr UINT_PTR kTimerArranque = 1;

/** DPI del monitor de la ventana (o del sistema); 96 = 100 %. Carga dinámica como en el resto de CuadernU. */
UINT DpiDeVentana(HWND h) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    using GetDpiForSystemFn = UINT(WINAPI*)();
    static const auto pGetDpiWnd = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    static const auto pGetDpiSys = reinterpret_cast<GetDpiForSystemFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForSystem"));
    UINT dpi = 0;
    if (pGetDpiWnd && h && IsWindow(h)) dpi = pGetDpiWnd(h);
    else if (pGetDpiSys) dpi = pGetDpiSys();
    return dpi >= 96 ? dpi : 96;
}

std::string RgbaHex(UiGlass::Rgba8 c) {
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
    return buf;
}

std::string BuildThemeMessageJson(const UiGlass::GlassSettings& s) {
    const auto pal = UiGlass::GetPalette(s);
    const char* modeStr = (s.mode == UiGlass::AppearanceMode::Dark)
        ? "dark"
        : (s.mode == UiGlass::AppearanceMode::HighContrast ? "high-contrast" : "light");
    json j = {
        {"type", "HOST_THEME_SYNC"},
        {"mode", modeStr},
        {"reduceTransparency", s.reduceTransparency},
        {"reduceMotion", s.reduceMotion},
        {"tokens", {
            {"paper", RgbaHex(pal.contentPaper)},
            {"canvasTop", RgbaHex(pal.windowCanvasTop)},
            {"card", RgbaHex(pal.contentCard)},
            {"cardElevated", RgbaHex(pal.contentCardElevated)},
            {"borderSubtle", RgbaHex(pal.borderSubtle)},
            {"labelPrimary", RgbaHex(pal.labelPrimary)},
            {"labelSecondary", RgbaHex(pal.labelSecondary)},
            {"accentBlue", RgbaHex(pal.accentBlue)},
            {"accentGreen", RgbaHex(pal.accentGreen)},
            {"accentOrange", RgbaHex(pal.accentOrange)},
            {"accentRed", RgbaHex(pal.accentRed)},
            {"accentIndigo", RgbaHex(pal.accentIndigo)}
        }}
    };
    return j.dump();
}

} // namespace
#endif

struct StudioTutorPanel::Impl {
#ifdef _WIN32
    HWND hwnd = nullptr;
    HWND ownerHwnd = nullptr;
    UiGlass::GlassSettings glass{};
    WebView2HtmlHost webHost;
    TutorSidecarBridge sidecar;
    BakeCallbackFn onBake;
    StatusCallbackFn onStatus;
    bool webReady = false;
    bool webActivating = false;
    bool startupFailed = false;
    SidecarLaunchConfig pendingCfg{};
    std::thread startupThread;
    std::atomic<bool> startupCancel{false};
    std::atomic<bool> starting{false};
    std::mutex startupMtx;
    std::string statusBannerUtf8 = "Abriendo la Pizarra del profesor\xE2\x80\xA6";
    std::chrono::steady_clock::time_point inicioArranque{};

    void JoinStartupThread() {
        startupCancel.store(true, std::memory_order_relaxed);
        if (startupThread.joinable()) {
            startupThread.join();
        }
        starting.store(false, std::memory_order_relaxed);
    }

    void PaintLoadingBanner(HWND wnd) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(wnd, &ps);
        RECT rc{};
        GetClientRect(wnd, &rc);
        const auto pal = UiGlass::GetPalette(glass);
        HBRUSH br = CreateSolidBrush(RGB(pal.contentPaper.r, pal.contentPaper.g, pal.contentPaper.b));
        FillRect(hdc, &rc, br);
        DeleteObject(br);

        // Mientras arranca: un mensaje sereno con los segundos, para no pensar que se colgó.
        // Si falló: el aviso con qué hacer. La letra se escala al DPI del monitor (antes 18 px físicos).
        std::string linea1Utf8;
        std::string linea2Utf8;
        if (starting.load(std::memory_order_relaxed)) {
            const auto seg = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - inicioArranque).count();
            linea1Utf8 = "Preparando la Pizarra del profesor\xE2\x80\xA6 " + std::to_string(seg) + " s";
            linea2Utf8 = "Suele tardar unos segundos; si el equipo est\xC3\xA1 ocupado, hasta un minuto.";
        } else {
            std::lock_guard<std::mutex> lk(startupMtx);
            linea1Utf8 = statusBannerUtf8;
        }
        const int dpi = static_cast<int>(DpiDeVentana(wnd));
        auto fuente = [dpi](int px, int peso) {
            return CreateFontW(
                -MulDiv(px, dpi, 96), 0, 0, 0, peso, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        };
        HFONT f1 = fuente(22, FW_SEMIBOLD);
        HFONT f2 = fuente(15, FW_NORMAL);
        HFONT oldFont = f1 ? static_cast<HFONT>(SelectObject(hdc, f1)) : nullptr;
        SetBkMode(hdc, TRANSPARENT);
        const int medio = (rc.top + rc.bottom) / 2;
        const std::wstring l1 = Utf8ToWide(linea1Utf8);
        RECT r1 = rc;
        r1.bottom = medio;
        SetTextColor(hdc, RGB(pal.labelPrimary.r, pal.labelPrimary.g, pal.labelPrimary.b));
        DrawTextW(hdc, l1.c_str(), -1, &r1, DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (!linea2Utf8.empty()) {
            if (f2) SelectObject(hdc, f2);
            const std::wstring l2 = Utf8ToWide(linea2Utf8);
            RECT r2 = rc;
            r2.top = medio + MulDiv(10, dpi, 96);
            SetTextColor(hdc, RGB(pal.labelSecondary.r, pal.labelSecondary.g, pal.labelSecondary.b));
            DrawTextW(hdc, l2.c_str(), -1, &r2, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        if (oldFont) SelectObject(hdc, oldFont);
        if (f1) DeleteObject(f1);
        if (f2) DeleteObject(f2);
        EndPaint(wnd, &ps);
    }

    void ActivateWebAfterSidecarReady(bool ok) {
        if (startupThread.joinable()) {
            startupThread.join();
        }
        starting.store(false, std::memory_order_relaxed);
        if (!hwnd || !IsWindow(hwnd)) return;
        KillTimer(hwnd, kTimerArranque);

        if (!ok) {
            startupFailed = true;
            webActivating = false;
            std::string errMsg;
            {
                std::lock_guard<std::mutex> lk(startupMtx);
                errMsg = statusBannerUtf8;
            }
            InvalidateRect(hwnd, nullptr, TRUE);
            if (onStatus) onStatus(errMsg);
            return;
        }

        webActivating = true;
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const auto ep = sidecar.Endpoint();
        const std::string query =
            "port=" + std::to_string(ep.port) +
            "&token=" + ep.sessionTokenHex +
            "&host=cuadernu_win32";

        webHost.ActivateTutorHost(
            hwnd,
            kTutorHostId,
            rc,
            pendingCfg.assetsWebDir,
            query,
            [this](const std::string& jsonMsg) { HandleWebMessage(jsonMsg); },
            [this](std::uint64_t, bool readyOk) {
                webActivating = false;
                webReady = readyOk;
                if (!readyOk) {
                    startupFailed = true;
                } else {
                    webHost.PostMessageJson(kTutorHostId, BuildThemeMessageJson(glass));
                    if (onStatus) {
                        onStatus("Pizarra Tutor Audiovisual lista");
                    }
                }
            });
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = reinterpret_cast<Impl*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

        switch (msg) {
            case WM_PAINT:
                if (!self->webReady) {
                    self->PaintLoadingBanner(hwnd);
                    return 0;
                }
                break;
            case kWmTutorSidecarReady:
                self->ActivateWebAfterSidecarReady(wp != 0);
                return 0;
            case WM_TIMER:
                if (wp == kTimerArranque) {
                    if (self->starting.load(std::memory_order_relaxed)) {
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        KillTimer(hwnd, kTimerArranque);
                    }
                    return 0;
                }
                break;
            case WM_SIZE: {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                if (wp == SIZE_MINIMIZED) {
                    self->webHost.SuspendAll();
                } else {
                    self->webHost.SyncBlockBounds(kTutorHostId, rc, true);
                    if (!self->webReady) InvalidateRect(hwnd, nullptr, TRUE);
                }
                return 0;
            }
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                self->webReady = false;
                self->JoinStartupThread();
                self->webHost.Shutdown();
                self->sidecar.Stop();
                self->hwnd = nullptr;
                return 0;
            default:
                break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    void HandleWebMessage(const std::string& rawJson) {
        try {
            const auto msg = json::parse(rawJson);
            const std::string kind = msg.value("type", "");
            if (kind == "WEB_BAKE_SESSION" && onBake) {
                const std::string beatsJson = msg.contains("beats") ? msg["beats"].dump() : "[]";
                const auto rep = onBake(beatsJson);
                json ack = {
                    {"type", "HOST_BAKE_RESULT"},
                    {"ok", rep.ok},
                    {"bakedCount", rep.bakedObjectsCount},
                    {"summary", rep.summaryUtf8}
                };
                webHost.PostMessageJson(kTutorHostId, ack.dump());
                if (onStatus) onStatus(rep.summaryUtf8);
            } else if (kind == "WEB_STATUS_NOTIFY" && onStatus) {
                onStatus(msg.value("message", ""));
            }
        } catch (...) {}
    }
#endif
};

StudioTutorPanel::StudioTutorPanel() : impl_(new Impl()) {}

StudioTutorPanel::~StudioTutorPanel() {
#ifdef _WIN32
    Close();
#endif
    delete impl_;
    impl_ = nullptr;
}

#ifdef _WIN32
bool StudioTutorPanel::OpenOrFocus(
    HWND ownerHwnd,
    const UiGlass::GlassSettings& glassSettings,
    const AiProviderVault* optionalVault,
    BakeCallbackFn onBakeToDocument,
    StatusCallbackFn onStatusUpdate,
    std::string* outErr,
    bool backgroundStealth)
{
    impl_->glass = glassSettings;
    impl_->ownerHwnd = ownerHwnd;
    impl_->onBake = std::move(onBakeToDocument);
    impl_->onStatus = std::move(onStatusUpdate);

    if (impl_->hwnd && IsWindow(impl_->hwnd)) {
        if (!backgroundStealth) {
            // Solo restaurar si estaba minimizada: SW_RESTORE sacaría a una ventana maximizada de ese estado
            if (IsIconic(impl_->hwnd)) ShowWindow(impl_->hwnd, SW_RESTORE);
            SetForegroundWindow(impl_->hwnd);
        }
        SyncAppearance(glassSettings);
        return true;
    }

    if (!WebView2HtmlHost::IsWebView2RuntimeInstalled()) {
        if (outErr) *outErr = "Microsoft Edge WebView2 Runtime no est\xC3\xA1 instalado en este equipo.";
        return false;
    }

    auto cfg = TutorSidecarBridge::DiscoverDefaultConfig();
    if (cfg.assetsWebDir.empty() || !std::filesystem::is_directory(cfg.assetsWebDir)) {
        if (outErr) *outErr = "No se encontr\xC3\xB3 la carpeta assets/cuadernu/tutor_web.";
        return false;
    }

    const SidecarVaultSnapshot vaultSnap = SidecarVaultSnapshot::Capture(optionalVault);
    impl_->JoinStartupThread();
    impl_->pendingCfg = cfg;
    impl_->startupFailed = false;
    impl_->webActivating = false;
    impl_->webReady = false;
    impl_->startupCancel.store(false, std::memory_order_relaxed);
    impl_->starting.store(true, std::memory_order_relaxed);
    impl_->inicioArranque = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(impl_->startupMtx);
        impl_->statusBannerUtf8 = "Abriendo la Pizarra del profesor\xE2\x80\xA6";
    }

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    if (!GetClassInfoExW(hInst, kTutorWndClass, &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = Impl::WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kTutorWndClass;
        RegisterClassExW(&wc);
    }

    // El proceso es PerMonitorV2: CreateWindowExW recibe píxeles físicos. 1320×860 sin escalar
    // eran ~660×430 en un 4K al 200 % y la pizarra quedaba sin espacio. Se escala por DPI y se
    // limita al área de trabajo del monitor; además la ventana abre maximizada (es el lugar de trabajo).
    const UINT dpi = DpiDeVentana(ownerHwnd);
    int w = MulDiv(1320, static_cast<int>(dpi), 96);
    int h = MulDiv(860, static_cast<int>(dpi), 96);
    {
        HMONITOR mon = MonitorFromWindow(ownerHwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (mon && GetMonitorInfoW(mon, &mi)) {
            const int maxW = (mi.rcWork.right - mi.rcWork.left) * 9 / 10;
            const int maxH = (mi.rcWork.bottom - mi.rcWork.top) * 9 / 10;
            if (maxW > 0 && w > maxW) w = maxW;
            if (maxH > 0 && h > maxH) h = maxH;
        }
    }
    const DWORD exStyle = backgroundStealth ? (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE) : 0u;
    impl_->hwnd = CreateWindowExW(
        exStyle,
        kTutorWndClass,
        L"CuadernU \u00B7 Pizarra Tutor Audiovisual",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        backgroundStealth ? 64 : CW_USEDEFAULT,
        backgroundStealth ? 64 : CW_USEDEFAULT,
        w,
        h,
        backgroundStealth ? nullptr : ownerHwnd,
        nullptr,
        hInst,
        impl_);

    if (!impl_->hwnd) {
        impl_->starting.store(false, std::memory_order_relaxed);
        if (outErr) *outErr = "No se pudo crear la ventana del Tutor Audiovisual.";
        return false;
    }

    UiGlass::ApplyWindowsNativeMicaBackdrop(impl_->hwnd, glassSettings.IsDark(), glassSettings.reduceTransparency);
    if (backgroundStealth) {
        if (HMODULE hDwm = LoadLibraryW(L"dwmapi.dll")) {
            using DwmSetAttrFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
            if (auto fn = reinterpret_cast<DwmSetAttrFn>(GetProcAddress(hDwm, "DwmSetWindowAttribute"))) {
                BOOL cloak = TRUE;
                fn(impl_->hwnd, 13 /*DWMWA_CLOAK*/, &cloak, sizeof(cloak));
            }
            FreeLibrary(hDwm);
        }
        ShowWindow(impl_->hwnd, SW_SHOWNOACTIVATE);
    } else {
        ShowWindow(impl_->hwnd, SW_SHOWMAXIMIZED);
    }
    UpdateWindow(impl_->hwnd);
    SetTimer(impl_->hwnd, kTimerArranque, 1000, nullptr);

    if (impl_->onStatus) {
        impl_->onStatus("Iniciando motor del Tutor Audiovisual...");
    }

    // Arrancar Python en un hilo de fondo para no congelar nunca el hilo UI de CuadernU.
    HWND targetHwnd = impl_->hwnd;
    Impl* self = impl_;
    impl_->startupThread = std::thread([self, targetHwnd, cfg, vaultSnap]() {
        std::string sidecarErr;
        const bool ok = self->sidecar.StartWithSnapshot(cfg, vaultSnap, &self->startupCancel, &sidecarErr);
        if (!ok) {
            std::lock_guard<std::mutex> lk(self->startupMtx);
            self->statusBannerUtf8 = sidecarErr.empty()
                ? "No se pudo iniciar el motor local del Tutor Audiovisual."
                : sidecarErr;
        }
        if (!self->startupCancel.load(std::memory_order_relaxed) && IsWindow(targetHwnd)) {
            PostMessageW(targetHwnd, kWmTutorSidecarReady, ok ? 1 : 0, 0);
        }
    });

    return true;
}

void StudioTutorPanel::SyncAppearance(const UiGlass::GlassSettings& glassSettings) {
    impl_->glass = glassSettings;
    if (impl_->hwnd && IsWindow(impl_->hwnd)) {
        UiGlass::ApplyWindowsNativeMicaBackdrop(impl_->hwnd, glassSettings.IsDark(), glassSettings.reduceTransparency);
    }
    if (impl_->webReady) {
        impl_->webHost.PostMessageJson(kTutorHostId, BuildThemeMessageJson(glassSettings));
    }
}

void StudioTutorPanel::SendActionToWeb(const std::string& actionTypeUtf8, const std::string& payloadJsonUtf8) {
    if (!impl_->webReady) return;
    json j = {{"type", actionTypeUtf8}};
    try {
        j["payload"] = json::parse(payloadJsonUtf8.empty() ? "{}" : payloadJsonUtf8);
    } catch (...) {
        j["payload"] = json::object();
    }
    impl_->webHost.PostMessageJson(kTutorHostId, j.dump());
}

bool StudioTutorPanel::IsWebReady() const noexcept {
    return impl_ && impl_->webReady;
}

bool StudioTutorPanel::CaptureWebViewPng(const std::wstring& outPathW, int timeoutMs) {
    if (!impl_ || !impl_->hwnd || !IsWindow(impl_->hwnd) || outPathW.empty()) return false;

    auto pumpOnce = []() {
        MSG m{};
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    };

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(250, timeoutMs));
    while ((!impl_->webReady || !impl_->webHost.IsNavigationCompleted(kTutorHostId)) &&
           !impl_->startupFailed &&
           std::chrono::steady_clock::now() < deadline &&
           IsWindow(impl_->hwnd)) {
        pumpOnce();
        if (!impl_->starting.load(std::memory_order_relaxed) && !impl_->webActivating && !impl_->webReady) {
            // Sidecar finished without starting WebView2; give one more message pump for kWmTutorSidecarReady
            pumpOnce();
            if (!impl_->starting.load(std::memory_order_relaxed) && !impl_->webActivating && !impl_->webReady && impl_->startupFailed) {
                break;
            }
        }
        Sleep(20);
    }

    std::vector<std::uint8_t> capturedPng;
    if (impl_->webReady) {
        auto doCapture = [&](int settleMs) {
            const auto settleEnd = std::chrono::steady_clock::now() + std::chrono::milliseconds(settleMs);
            while (std::chrono::steady_clock::now() < settleEnd && IsWindow(impl_->hwnd)) {
                pumpOnce();
                Sleep(20);
            }
            capturedPng.clear();
            bool done = false;
            impl_->webHost.CaptureSnapshot(kTutorHostId, [&](std::uint64_t, std::vector<std::uint8_t> bytes) {
                capturedPng = std::move(bytes);
                done = true;
            });
            const auto capDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);
            while (!done && std::chrono::steady_clock::now() < capDeadline && IsWindow(impl_->hwnd)) {
                pumpOnce();
                Sleep(15);
            }
        };

        // Allow Chromium WebView2 time to complete DOM layout, stylesheet paint & WebSocket connect
        doCapture(1800);
        // If first capture caught a pre-paint solid blank frame (<28 KB for a large viewport), settle and retry once
        if (capturedPng.size() < 28000u && IsWindow(impl_->hwnd)) {
            doCapture(1500);
        }
    }

    if (capturedPng.empty()) {
        // Fallback: capture Win32 client area (e.g. loading banner or GDI host) via PrintWindow
        RECT rc{};
        GetClientRect(impl_->hwnd, &rc);
        const int w = std::max(1, static_cast<int>(rc.right - rc.left));
        const int h = std::max(1, static_cast<int>(rc.bottom - rc.top));
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP hbmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (hbmp && bits) {
            HGDIOBJ old = SelectObject(hdcMem, hbmp);
            PrintWindow(impl_->hwnd, hdcMem, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
            RasterImageBgra32 img;
            img.width = w;
            img.height = h;
            img.pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
            std::memcpy(img.pixels.data(), bits, img.pixels.size());
            for (std::size_t i = 3; i < img.pixels.size(); i += 4) img.pixels[i] = 255u;
            WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, capturedPng);
            SelectObject(hdcMem, old);
            DeleteObject(hbmp);
        }
        if (hdcMem) DeleteDC(hdcMem);
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
    }

    if (capturedPng.empty()) return false;
    std::error_code ec;
    const std::filesystem::path p(outPathW);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs.write(reinterpret_cast<const char*>(capturedPng.data()), static_cast<std::streamsize>(capturedPng.size()));
    return ofs.good();
}

void StudioTutorPanel::Close() {
    if (impl_->hwnd && IsWindow(impl_->hwnd)) {
        DestroyWindow(impl_->hwnd);
    } else {
        impl_->JoinStartupThread();
        impl_->webHost.Shutdown();
        impl_->sidecar.Stop();
    }
}

bool StudioTutorPanel::IsOpen() const noexcept {
    return impl_->hwnd != nullptr && IsWindow(impl_->hwnd) != FALSE;
}

HWND StudioTutorPanel::Hwnd() const noexcept {
    return impl_->hwnd;
}
#endif

} // namespace DocStudio::Tutor
