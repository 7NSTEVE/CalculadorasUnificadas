#include "studio_tutor_panel.hpp"
#include "../core/utf_conv.hpp"
#include "../tutor/tutor_whiteboard_protocol.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>

namespace DocStudio::Tutor {

using nlohmann::json;

#ifdef _WIN32
namespace {

constexpr wchar_t kTutorWndClass[] = L"CuadernU_AudiovisualTutorWnd";
constexpr std::uint64_t kTutorHostId = 990001ull;

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
            case WM_SIZE: {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                if (wp == SIZE_MINIMIZED) {
                    self->webHost.SuspendAll();
                } else {
                    self->webHost.SyncBlockBounds(kTutorHostId, rc, true);
                }
                return 0;
            }
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                self->webReady = false;
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
    std::string* outErr)
{
    impl_->glass = glassSettings;
    impl_->ownerHwnd = ownerHwnd;
    impl_->onBake = std::move(onBakeToDocument);
    impl_->onStatus = std::move(onStatusUpdate);

    if (impl_->hwnd && IsWindow(impl_->hwnd)) {
        ShowWindow(impl_->hwnd, SW_RESTORE);
        SetForegroundWindow(impl_->hwnd);
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

    std::string sidecarErr;
    if (!impl_->sidecar.Start(cfg, optionalVault, &sidecarErr)) {
        if (outErr) *outErr = sidecarErr;
        return false;
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

    const int w = 1320;
    const int h = 860;
    impl_->hwnd = CreateWindowExW(
        0,
        kTutorWndClass,
        L"CuadernU \u00B7 Pizarra Tutor Audiovisual",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        w,
        h,
        ownerHwnd,
        nullptr,
        hInst,
        impl_);

    if (!impl_->hwnd) {
        impl_->sidecar.Stop();
        if (outErr) *outErr = "No se pudo crear la ventana del Tutor Audiovisual.";
        return false;
    }

    UiGlass::ApplyWindowsNativeMicaBackdrop(impl_->hwnd, glassSettings.IsDark(), glassSettings.reduceTransparency);
    ShowWindow(impl_->hwnd, SW_SHOW);
    UpdateWindow(impl_->hwnd);

    RECT rc{};
    GetClientRect(impl_->hwnd, &rc);
    const auto ep = impl_->sidecar.Endpoint();
    const std::string query =
        "port=" + std::to_string(ep.port) +
        "&token=" + ep.sessionTokenHex +
        "&host=cuadernu_win32";

    impl_->webHost.ActivateTutorHost(
        impl_->hwnd,
        kTutorHostId,
        rc,
        cfg.assetsWebDir,
        query,
        [this](const std::string& jsonMsg) { impl_->HandleWebMessage(jsonMsg); },
        [this](std::uint64_t, bool ok) {
            impl_->webReady = ok;
            if (ok) {
                impl_->webHost.PostMessageJson(kTutorHostId, BuildThemeMessageJson(impl_->glass));
                if (impl_->onStatus) {
                    impl_->onStatus("Pizarra Tutor Audiovisual lista");
                }
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

void StudioTutorPanel::Close() {
    if (impl_->hwnd && IsWindow(impl_->hwnd)) {
        DestroyWindow(impl_->hwnd);
    } else {
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
