#pragma once
// ============================================================================
// ui_glass/translucent_tokens.hpp
// guías de diseño de interfaces + macOS 27 Golden Gate ("material translúcido 2.0")
// Semantic color palettes (Light default / Dark / High Contrast), continuous
// UserOpacitySlider (0.0..1.0), Windows OS accessibility auto-detection,
// concentric corner geometry, and DWM Windows 11 frame attributes.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace UiGlass {

enum class AppearanceMode {
    Light = 0,       // guías de interfaz Light default
    Dark = 1,        // guías de interfaz Dark (base + elevated)
    HighContrast = 2 // WCAG AAA High Contrast (1px crisp borders + opaque)
};

struct Rgba8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr COLORREF ToColorRef() const noexcept { return RGB(r, g, b); }
    constexpr float Rf() const noexcept { return static_cast<float>(r) / 255.0f; }
    constexpr float Gf() const noexcept { return static_cast<float>(g) / 255.0f; }
    constexpr float Bf() const noexcept { return static_cast<float>(b) / 255.0f; }
    constexpr float Af() const noexcept { return static_cast<float>(a) / 255.0f; }
};

struct GlassSettings {
    AppearanceMode mode = AppearanceMode::Light;
    float userOpacitySlider = 0.36f; // macOS 27 Golden Gate Balanced Mode default
    bool reduceTransparency = false;
    bool increaseContrast = false;
    bool reduceMotion = false;

    bool IsDark() const noexcept { return mode == AppearanceMode::Dark; }
    bool IsHighContrast() const noexcept { return mode == AppearanceMode::HighContrast || increaseContrast; }
    float EffectiveOpacity() const noexcept {
        if (reduceTransparency) return 0.94f;
        return std::clamp(userOpacitySlider, 0.0f, 1.0f);
    }
};

struct GlassPalette {
    // Content / Canvas layer (NEVER material translúcido!)
    Rgba8 windowCanvasTop{};
    Rgba8 windowCanvasBottom{};
    Rgba8 contentPaper{};
    Rgba8 contentCard{};
    Rgba8 contentCardElevated{};
    Rgba8 sidebarBg{};
    Rgba8 sidebarHover{};
    Rgba8 sidebarSelected{};
    Rgba8 inspectorBg{};
    Rgba8 separator{};
    Rgba8 borderSubtle{};
    Rgba8 borderStrong{};

    // Typography hierarchy (WCAG AA/AAA)
    Rgba8 labelPrimary{};
    Rgba8 labelSecondary{};
    Rgba8 labelTertiary{};

    // guías de interfaz System Semantic Accents (for colored sidebar icons & single prominent CTA)
    Rgba8 accentBlue{};
    Rgba8 accentBlueHover{};
    Rgba8 accentBlueSoft{};
    Rgba8 accentIndigo{};
    Rgba8 accentPurple{};
    Rgba8 accentGreen{};
    Rgba8 accentTeal{};
    Rgba8 accentOrange{};
    Rgba8 accentRed{};

    // Aqua 2.0 vitreous window/badge highlights
    Rgba8 glassTintNeutral{};
    Rgba8 glassPillSelected{};
    Rgba8 glassPillHover{};
    Rgba8 glassRimLight{};
};

inline GlassPalette GetPalette(const GlassSettings& s) noexcept {
    GlassPalette p{};
    if (s.mode == AppearanceMode::HighContrast) {
        p.windowCanvasTop     = { 255, 255, 255, 255 };
        p.windowCanvasBottom  = { 248, 249, 251, 255 };
        p.contentPaper        = { 255, 255, 255, 255 };
        p.contentCard         = { 255, 255, 255, 255 };
        p.contentCardElevated = { 245, 247, 250, 255 };
        p.sidebarBg           = { 247, 248, 250, 255 };
        p.sidebarHover        = { 232, 238, 248, 255 };
        p.sidebarSelected     = { 214, 230, 255, 255 };
        p.inspectorBg         = { 248, 249, 252, 255 };
        p.separator           = { 28,  34,  44,  255 };
        p.borderSubtle        = { 44,  52,  66,  255 };
        p.borderStrong        = { 15,  20,  28,  255 };
        p.labelPrimary        = { 0,   0,   0,   255 };
        p.labelSecondary      = { 24,  30,  40,  255 };
        p.labelTertiary       = { 38,  46,  58,  255 };
        p.accentBlue          = { 0,   54,  196, 255 };
        p.accentBlueHover     = { 0,   42,  166, 255 };
        p.accentBlueSoft      = { 226, 238, 255, 255 };
        p.accentIndigo        = { 46,  42,  150, 255 };
        p.accentPurple        = { 84,  22,  142, 255 };
        p.accentGreen         = { 10,  86,  40,  255 };
        p.accentTeal          = { 0,   82,  112, 255 };
        p.accentOrange        = { 148, 56,  0,   255 };
        p.accentRed           = { 166, 12,  36,  255 };
        p.glassTintNeutral    = { 255, 255, 255, 250 };
        p.glassPillSelected   = { 216, 232, 255, 255 };
        p.glassPillHover      = { 234, 240, 248, 255 };
        p.glassRimLight       = { 15,  20,  28,  255 };
        return p;
    }

    if (s.mode == AppearanceMode::Dark) {
        // guías de interfaz Dark Mode (macOS 27 Golden Gate calibrated slate, base + elevated)
        p.windowCanvasTop     = { 20,  22,  28,  255 };
        p.windowCanvasBottom  = { 15,  17,  22,  255 };
        p.contentPaper        = { 28,  31,  39,  255 };
        p.contentCard         = { 30,  34,  43,  255 };
        p.contentCardElevated = { 40,  45,  56,  255 };
        p.sidebarBg           = { 24,  27,  34,  255 };
        p.sidebarHover        = { 38,  43,  54,  255 };
        p.sidebarSelected     = { 18,  52,  96,  255 };
        p.inspectorBg         = { 24,  27,  34,  255 };
        p.separator           = { 54,  60,  74,  255 };
        p.borderSubtle        = { 56,  63,  78,  255 };
        p.borderStrong        = { 86,  96,  118, 255 };
        p.labelPrimary        = { 245, 247, 251, 255 };
        p.labelSecondary      = { 176, 186, 204, 255 };
        p.labelTertiary       = { 146, 158, 178, 255 };
        p.accentBlue          = { 36,  148, 255, 255 };
        p.accentBlueHover     = { 68,  166, 255, 255 };
        p.accentBlueSoft      = { 22,  52,  94,  255 };
        p.accentIndigo        = { 126, 124, 248, 255 };
        p.accentPurple        = { 216, 146, 255, 255 };
        p.accentGreen         = { 72,  224, 114, 255 };
        p.accentTeal          = { 100, 210, 255, 255 };
        p.accentOrange        = { 255, 159, 10,  255 };
        p.accentRed           = { 255, 69,  58,  255 };
        p.glassTintNeutral    = { 36,  41,  52,  218 };
        p.glassPillSelected   = { 52,  60,  76,  238 };
        p.glassPillHover      = { 48,  54,  68,  210 };
        p.glassRimLight       = { 255, 255, 255, 68  };
        return p;
    }

    // guías de interfaz Light Default (macOS 27 Golden Gate airy clarity)
    p.windowCanvasTop     = { 243, 246, 251, 255 };
    p.windowCanvasBottom  = { 235, 240, 247, 255 };
    p.contentPaper        = { 255, 255, 255, 255 };
    p.contentCard         = { 255, 255, 255, 255 };
    p.contentCardElevated = { 248, 250, 253, 255 };
    p.sidebarBg           = { 244, 246, 250, 255 };
    p.sidebarHover        = { 233, 238, 246, 255 };
    p.sidebarSelected     = { 222, 236, 255, 255 };
    p.inspectorBg         = { 247, 249, 252, 255 };
    p.separator           = { 218, 224, 234, 255 };
    p.borderSubtle        = { 214, 221, 232, 255 };
    p.borderStrong        = { 174, 186, 206, 255 };
    p.labelPrimary        = { 22,  26,  34,  255 };
    p.labelSecondary      = { 76,  86,  102, 255 };
    p.labelTertiary       = { 88,  98,  114, 255 };
    p.accentBlue          = { 0,   102, 224, 255 };
    p.accentBlueHover     = { 0,   88,  200, 255 };
    p.accentBlueSoft      = { 228, 240, 255, 255 };
    p.accentIndigo        = { 76,  72,  196, 255 };
    p.accentPurple        = { 132, 48,  184, 255 };
    p.accentGreen         = { 20,  122, 54,  255 };
    p.accentTeal          = { 0,   158, 198, 255 };
    p.accentOrange        = { 255, 149, 0,   255 };
    p.accentRed           = { 255, 59,  48,  255 };
    p.glassTintNeutral    = { 252, 253, 255, 215 };
    p.glassPillSelected   = { 255, 255, 255, 245 };
    p.glassPillHover      = { 236, 242, 250, 210 };
    p.glassRimLight       = { 255, 255, 255, 220 };
    return p;
}

// Detect Windows OS accessibility preferences (High Contrast, Reduce Transparency)
inline void DetectSystemAccessibility(GlassSettings& s) noexcept {
    HIGHCONTRASTW hc{};
    hc.cbSize = sizeof(hc);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) &&
        (hc.dwFlags & HCF_HIGHCONTRASTON) != 0) {
        s.mode = AppearanceMode::HighContrast;
        s.increaseContrast = true;
        s.reduceTransparency = true;
    }

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD enableTrans = 1;
        DWORD sz = sizeof(enableTrans);
        if (RegQueryValueExW(hKey, L"EnableTransparency", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(&enableTrans), &sz) == ERROR_SUCCESS) {
            if (enableTrans == 0) s.reduceTransparency = true;
        }
        RegCloseKey(hKey);
    }

    BOOL clientAnim = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &clientAnim, 0) && !clientAnim) {
        s.reduceMotion = true;
    }
}

// Apply Windows 11 native DWM Mica backdrop (executed inside dwm.exe -> 0% app GPU / 0% app CPU)
// plus uniform rounded window corners and dark/light DWM title bar.
inline void ApplyWindowsNativeMicaBackdrop(HWND hwnd, bool dark, bool reduceTransparency = false) noexcept {
    if (!hwnd || !IsWindow(hwnd)) return;
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (!hDwm) return;
    using PfnDwmSetAttr = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto fn = reinterpret_cast<PfnDwmSetAttr>(GetProcAddress(hDwm, "DwmSetWindowAttribute"));
    if (fn) {
        BOOL darkVal = dark ? TRUE : FALSE;
        if (FAILED(fn(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkVal, sizeof(darkVal)))) {
            fn(hwnd, 19, &darkVal, sizeof(darkVal));
        }
        // DWMWA_WINDOW_CORNER_PREFERENCE (33) = DWMWCP_ROUND (2)
        DWORD cornerPref = 2;
        fn(hwnd, 33, &cornerPref, sizeof(cornerPref));
        // DWMWA_SYSTEMBACKDROP_TYPE (38): 2 = DWMSBT_MAINWINDOW (Mica), 1 = DWMSBT_NONE
        DWORD backdrop = reduceTransparency ? 1u : 2u;
        fn(hwnd, 38, &backdrop, sizeof(backdrop));
    }
    FreeLibrary(hDwm);
}

// Apply macOS 27 Golden Gate uniform rounded window corners + dark/light DWM title bar
inline void ApplyGoldenGateWindowFrame(HWND hwnd, bool dark, bool reduceTransparency = false) noexcept {
    ApplyWindowsNativeMicaBackdrop(hwnd, dark, reduceTransparency);
}

inline std::wstring glassSettingsIniOverride;

inline std::wstring GetGlassSettingsIniPath() {
    if (!glassSettingsIniOverride.empty()) return glassSettingsIniOverride;
    wchar_t buf[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH - 48) {
        std::wstring dir = std::wstring(buf) + L"\\CalculadoraUnificada";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\translucent_settings.ini";
    }
    return L"translucent_settings.ini";
}

inline void SaveGlassSettings(const GlassSettings& s) {
    const std::wstring path = GetGlassSettingsIniPath();
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return;
    ofs << "mode=" << static_cast<int>(s.mode) << "\n";
    ofs << "opacity=" << std::clamp(s.userOpacitySlider, 0.0f, 1.0f) << "\n";
    ofs << "reduce_transparency=" << (s.reduceTransparency ? 1 : 0) << "\n";
    ofs << "increase_contrast=" << (s.increaseContrast ? 1 : 0) << "\n";
}

inline void LoadGlassSettings(GlassSettings& s) {
    DetectSystemAccessibility(s);
    const std::wstring path = GetGlassSettingsIniPath();
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;
    std::string line;
    while (std::getline(ifs, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);
        try {
            if (k == "mode") {
                const int m = std::stoi(v);
                if (m >= 0 && m <= 2) s.mode = static_cast<AppearanceMode>(m);
            } else if (k == "opacity") {
                s.userOpacitySlider = std::clamp(std::stof(v), 0.0f, 1.0f);
            } else if (k == "reduce_transparency") {
                s.reduceTransparency = (std::stoi(v) != 0);
            } else if (k == "increase_contrast") {
                s.increaseContrast = (std::stoi(v) != 0);
            }
        } catch (...) {
        }
    }
}

// HIG font selector: prefers Segoe UI Variable Text/Display, falls back to Segoe UI
inline const wchar_t* PreferredUiFontFamily() noexcept {
    return L"Segoe UI Variable Text";
}

inline HFONT CreateHigUiFont(int pxHeight, int weight = FW_NORMAL, bool italic = false) noexcept {
    HFONT f = CreateFontW(
        -pxHeight, 0, 0, 0, weight,
        italic ? TRUE : FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable Text");
    if (!f) {
        f = CreateFontW(
            -pxHeight, 0, 0, 0, weight,
            italic ? TRUE : FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    }
    return f;
}

} // namespace UiGlass
