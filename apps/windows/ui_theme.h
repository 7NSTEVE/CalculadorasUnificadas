#pragma once
// ============================================================================
// ui_theme.h
// Sistema de diseno unificado para Calculadora Unificada (Win32 + GDI+)
// Paleta Graphite Studio, barra de titulo oscura DWM, controles vectoriales
// suavizados (GDI+ AntiAlias) y recorte alfa limpio para chasis CASIO.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include "operation_capture.h"
#include "ui_glass/translucent_tokens.hpp"
#include "ui_glass/translucent_math.hpp"
#include "ui_glass/vector_icons.hpp"
#include "ui_glass/gdi_translucent.hpp"

namespace CasioTheme {

// ── Tokens de Color: Modern Graphite Studio ────────────────────────────────
inline constexpr COLORREF CanvasBg      = RGB(11, 14, 20);
inline constexpr COLORREF StageBg       = RGB(14, 18, 26);
inline constexpr COLORREF Surface1      = RGB(20, 26, 37);
inline constexpr COLORREF Surface2      = RGB(28, 36, 51);
inline constexpr COLORREF Surface3      = RGB(37, 47, 67);
inline constexpr COLORREF InputBg       = RGB(17, 23, 34);
inline constexpr COLORREF BorderSubtle  = RGB(42, 53, 74);
inline constexpr COLORREF BorderStrong  = RGB(64, 80, 110);

inline constexpr COLORREF AccentBlue    = RGB(59, 130, 246);
inline constexpr COLORREF AccentHover   = RGB(96, 165, 250);
inline constexpr COLORREF AccentSoft    = RGB(24, 52, 92);
inline constexpr COLORREF AccentText    = RGB(147, 197, 253);

inline constexpr COLORREF SuccessGreen  = RGB(16, 185, 129);
inline constexpr COLORREF SuccessSoft   = RGB(16, 58, 46);
inline constexpr COLORREF SuccessText   = RGB(110, 231, 183);

inline constexpr COLORREF AmberWarm     = RGB(245, 158, 11);
inline constexpr COLORREF AmberSoft     = RGB(58, 40, 14);
inline constexpr COLORREF AmberText     = RGB(252, 211, 77);

inline constexpr COLORREF VioletAccent  = RGB(139, 92, 246);
inline constexpr COLORREF VioletSoft    = RGB(46, 33, 84);
inline constexpr COLORREF VioletText    = RGB(196, 181, 253);

inline constexpr COLORREF RoseRed       = RGB(244, 63, 94);
inline constexpr COLORREF RoseSoft      = RGB(64, 22, 34);
inline constexpr COLORREF RoseText      = RGB(253, 164, 175);

inline constexpr COLORREF TextPrimary   = RGB(241, 245, 249);
inline constexpr COLORREF TextSecondary = RGB(148, 163, 184);
inline constexpr COLORREF TextMuted     = RGB(100, 116, 139);

// ── Estado global del tema (guías de interfaz + macOS 27 Golden Gate material translúcido 2.0) ──
inline bool g_studioDarkTheme = false;
inline UiGlass::GlassSettings g_glassSettings{};

inline void SyncGlassSettingsFromLegacyFlag() noexcept {
    if (g_studioDarkTheme) {
        if (g_glassSettings.mode != UiGlass::AppearanceMode::Dark) {
            g_glassSettings.mode = UiGlass::AppearanceMode::Dark;
        }
    } else if (g_glassSettings.mode == UiGlass::AppearanceMode::Dark) {
        g_glassSettings.mode = UiGlass::AppearanceMode::Light;
    }
}

inline void SetStudioAppearanceMode(UiGlass::AppearanceMode mode) noexcept {
    g_glassSettings.mode = mode;
    g_glassSettings.increaseContrast = (mode == UiGlass::AppearanceMode::HighContrast);
    g_studioDarkTheme = (mode == UiGlass::AppearanceMode::Dark);
    UiGlass::SaveGlassSettings(g_glassSettings);
}

inline void SetStudioGlassOpacity(float opacity01, bool reduceTrans = false) noexcept {
    g_glassSettings.userOpacitySlider = (std::max)(0.0f, (std::min)(1.0f, opacity01));
    g_glassSettings.reduceTransparency = reduceTrans;
    UiGlass::SaveGlassSettings(g_glassSettings);
}

inline const UiGlass::GlassSettings& GetActiveGlassSettings() noexcept {
    SyncGlassSettingsFromLegacyFlag();
    return g_glassSettings;
}

inline UiGlass::GlassPalette ActiveGlassPalette() noexcept {
    return UiGlass::GetPalette(GetActiveGlassSettings());
}

namespace Light {
inline constexpr COLORREF CanvasBg      = RGB(245, 247, 250);
inline constexpr COLORREF StageBg       = RGB(239, 243, 248);
inline constexpr COLORREF Surface1      = RGB(255, 255, 255);
inline constexpr COLORREF Surface2      = RGB(247, 249, 252);
inline constexpr COLORREF Track         = RGB(231, 236, 243);
inline constexpr COLORREF BorderSubtle  = RGB(220, 226, 235);
inline constexpr COLORREF BorderStrong  = RGB(184, 196, 214);
inline constexpr COLORREF AccentBlue    = RGB(0, 122, 255);
inline constexpr COLORREF AccentHover   = RGB(12, 112, 232);
inline constexpr COLORREF AccentSoft    = RGB(232, 242, 255);
inline constexpr COLORREF AccentText    = RGB(0, 96, 208);
inline constexpr COLORREF SuccessGreen  = RGB(16, 163, 108);
inline constexpr COLORREF SuccessSoft   = RGB(234, 248, 242);
inline constexpr COLORREF SuccessText   = RGB(10, 118, 80);
inline constexpr COLORREF AmberWarm     = RGB(210, 118, 12);
inline constexpr COLORREF AmberSoft     = RGB(253, 244, 224);
inline constexpr COLORREF AmberText     = RGB(154, 82, 10);
inline constexpr COLORREF RoseRed       = RGB(218, 36, 76);
inline constexpr COLORREF RoseSoft      = RGB(254, 236, 240);
inline constexpr COLORREF RoseText      = RGB(180, 24, 62);
inline constexpr COLORREF TextPrimary   = RGB(20, 25, 34);
inline constexpr COLORREF TextSecondary = RGB(90, 102, 120);
inline constexpr COLORREF TextMuted     = RGB(138, 150, 168);
inline constexpr COLORREF InputBg       = RGB(255, 255, 255);
inline constexpr COLORREF Surface3      = RGB(232, 238, 247);
inline constexpr COLORREF VioletAccent  = RGB(109, 74, 218);
inline constexpr COLORREF VioletSoft    = RGB(240, 236, 255);
inline constexpr COLORREF VioletText    = RGB(91, 56, 194);
} // namespace Light

inline COLORREF StudioCanvasBg()      { return ActiveGlassPalette().windowCanvasTop.ToColorRef(); }
inline COLORREF StudioStageBg()       { return ActiveGlassPalette().windowCanvasBottom.ToColorRef(); }
inline COLORREF StudioSurface()       { return ActiveGlassPalette().contentCard.ToColorRef(); }
inline COLORREF StudioSurfaceAlt()    { return ActiveGlassPalette().contentCardElevated.ToColorRef(); }
inline COLORREF StudioSurface3()      { return ActiveGlassPalette().sidebarHover.ToColorRef(); }
inline COLORREF StudioInputBg()       { return ActiveGlassPalette().contentPaper.ToColorRef(); }
inline COLORREF StudioTrack()         { return ActiveGlassPalette().sidebarBg.ToColorRef(); }
inline COLORREF StudioBorder()        { return ActiveGlassPalette().borderSubtle.ToColorRef(); }
inline COLORREF StudioBorderStrong()  { return ActiveGlassPalette().borderStrong.ToColorRef(); }
inline COLORREF StudioAccent()        { return ActiveGlassPalette().accentBlue.ToColorRef(); }
inline COLORREF StudioAccentHover()   { return ActiveGlassPalette().accentBlueHover.ToColorRef(); }
inline COLORREF StudioAccentSoft()    { return ActiveGlassPalette().accentBlueSoft.ToColorRef(); }
inline COLORREF StudioAccentText()    { return ActiveGlassPalette().accentBlue.ToColorRef(); }
inline COLORREF StudioSuccess()       { return ActiveGlassPalette().accentGreen.ToColorRef(); }
inline COLORREF StudioSuccessSoft()   { return g_studioDarkTheme ? SuccessSoft   : Light::SuccessSoft; }
inline COLORREF StudioSuccessText()   { return g_studioDarkTheme ? SuccessText   : (ActiveGlassPalette().accentGreen.ToColorRef()); }
inline COLORREF StudioAmber()         { return ActiveGlassPalette().accentOrange.ToColorRef(); }
inline COLORREF StudioAmberSoft()     { return g_studioDarkTheme ? AmberSoft     : Light::AmberSoft; }
inline COLORREF StudioAmberText()     { return g_studioDarkTheme ? AmberText     : Light::AmberText; }
inline COLORREF StudioViolet()        { return ActiveGlassPalette().accentPurple.ToColorRef(); }
inline COLORREF StudioVioletSoft()    { return g_studioDarkTheme ? VioletSoft    : Light::VioletSoft; }
inline COLORREF StudioVioletText()    { return g_studioDarkTheme ? VioletText    : Light::VioletText; }
inline COLORREF StudioRose()          { return ActiveGlassPalette().accentRed.ToColorRef(); }
inline COLORREF StudioRoseSoft()      { return g_studioDarkTheme ? RoseSoft      : Light::RoseSoft; }
inline COLORREF StudioRoseText()      { return g_studioDarkTheme ? RoseText      : Light::RoseText; }
inline COLORREF StudioText()          { return ActiveGlassPalette().labelPrimary.ToColorRef(); }
inline COLORREF StudioTextSec()       { return ActiveGlassPalette().labelSecondary.ToColorRef(); }
inline COLORREF StudioTextMuted()     { return ActiveGlassPalette().labelTertiary.ToColorRef(); }

enum ButtonVariant {
    BTN_SECONDARY     = 0,
    BTN_PRIMARY       = 1,
    BTN_SUCCESS       = 2,
    BTN_VIOLET        = 3,
    BTN_AMBER         = 4,
    BTN_DANGER        = 5,
    BTN_GHOST         = 6,
    BTN_TOOLBAR_GHOST = 7
};

enum SidebarGlyphKind {
    GLYPH_NONE      = 0,
    GLYPH_COMPACT   = 1,
    GLYPH_PIP       = 2,
    GLYPH_PIN       = 3,
    GLYPH_ZOOM      = 4,
    GLYPH_INSPECTOR = 5,
    GLYPH_LAYERS    = 6,
    GLYPH_STUDIO    = 7,
    GLYPH_HELP      = 8,
    GLYPH_COPY      = 9,
    GLYPH_CAMERA    = 10,
    GLYPH_EXPORT    = 11,
    GLYPH_FOLDER    = 12,
    GLYPH_SAVE      = 13,
    GLYPH_UNDO      = 14,
    GLYPH_HIGHLIGHT = 15,
    GLYPH_RESTART   = 16,
    GLYPH_TRASH     = 17,
    GLYPH_SETTINGS  = 18,
    GLYPH_MENU      = 19,
    GLYPH_PLAYER    = 20
};

enum TransportKind {
    TR_SKIP_START = 0,
    TR_PREV       = 1,
    TR_PLAY       = 2,
    TR_PAUSE      = 3,
    TR_NEXT       = 4,
    TR_SKIP_END   = 5
};

inline HFONT CreateUiFont(int pxHeight, bool bold = false, bool italic = false) {
    return UiGlass::CreateHigUiFont(pxHeight, bold ? FW_SEMIBOLD : FW_NORMAL, italic);
}

inline Gdiplus::Color ToGpColor(COLORREF c, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(c), GetGValue(c), GetBValue(c));
}

// ── Escalado uniforme de interfaz (Per-Monitor DPI + 4K al 100%) ───────────
inline int   g_uiScaleMode    = 0;    // 0 = Auto, 100, 125, 150, 175, 200
inline float g_uiScale        = 1.0f; // Factor efectivo (1.0f .. 2.0f)
inline float g_canvasUiScale  = 1.0f; // Factor efectivo usado en CanvasWnd

inline float ClampAndSnapUiScale(float raw) {
    if (raw < 1.0f) raw = 1.0f;
    if (raw > 2.0f) raw = 2.0f;
    int pct = static_cast<int>(std::lround((raw * 100.0f) / 25.0f)) * 25;
    if (pct < 100) pct = 100;
    if (pct > 200) pct = 200;
    return static_cast<float>(pct) / 100.0f;
}

inline float ComputeRecommendedUiScale(HWND hwnd = nullptr) {
    if (g_uiScaleMode >= 100 && g_uiScaleMode <= 200) {
        return ClampAndSnapUiScale(static_cast<float>(g_uiScaleMode) / 100.0f);
    }
    float dpiScale = 1.0f;
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (hUser) {
        using PfnGetDpiForWindow = UINT(WINAPI*)(HWND);
        using PfnGetDpiForSystem = UINT(WINAPI*)();
        if (hwnd && IsWindow(hwnd)) {
            auto fnW = reinterpret_cast<PfnGetDpiForWindow>(GetProcAddress(hUser, "GetDpiForWindow"));
            if (fnW) {
                UINT dpi = fnW(hwnd);
                if (dpi >= 96) dpiScale = static_cast<float>(dpi) / 96.0f;
            }
        } else {
            auto fnS = reinterpret_cast<PfnGetDpiForSystem>(GetProcAddress(hUser, "GetDpiForSystem"));
            if (fnS) {
                UINT dpi = fnS();
                if (dpi >= 96) dpiScale = static_cast<float>(dpi) / 96.0f;
            }
        }
    }
    if (dpiScale > 1.05f) {
        return ClampAndSnapUiScale(dpiScale);
    }
    HMONITOR hMon = (hwnd && IsWindow(hwnd))
        ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    int workH = GetSystemMetrics(SM_CYSCREEN);
    int workW = GetSystemMetrics(SM_CXSCREEN);
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        workW = mi.rcWork.right - mi.rcWork.left;
        workH = mi.rcWork.bottom - mi.rcWork.top;
    }
    if (hwnd && IsWindow(hwnd)) {
        RECT wrc{};
        if (GetClientRect(hwnd, &wrc)) {
            int cw = wrc.right - wrc.left;
            int ch = wrc.bottom - wrc.top;
            if (cw > 200 && ch > 200 && (cw < 1800 || ch < 1050)) {
                float winScale = (std::min)(static_cast<float>(cw) / 1320.0f, static_cast<float>(ch) / 760.0f);
                if (winScale < 1.15f) return 1.0f;
            }
        }
    }
    float autoByRes = (std::min)(static_cast<float>(workH) / 1080.0f, static_cast<float>(workW) / 1920.0f);
    if (autoByRes >= 1.85f) return 2.0f;
    if (autoByRes >= 1.60f) return 1.75f;
    if (autoByRes >= 1.38f) return 1.50f;
    if (autoByRes >= 1.18f) return 1.25f;
    return 1.0f;
}

inline float ComputeDialogUiScale(HWND hwnd, int logicalW, int logicalH) {
    float target = ComputeRecommendedUiScale(nullptr);
    if (hwnd && IsWindow(hwnd) && !(g_uiScaleMode >= 100 && g_uiScaleMode <= 200)) {
        HMODULE hUser = GetModuleHandleW(L"user32.dll");
        if (hUser) {
            using PfnGetDpiForWindow = UINT(WINAPI*)(HWND);
            auto fnW = reinterpret_cast<PfnGetDpiForWindow>(GetProcAddress(hUser, "GetDpiForWindow"));
            if (fnW) {
                UINT dpi = fnW(hwnd);
                if (dpi >= 105) {
                    target = (std::max)(target, ClampAndSnapUiScale(static_cast<float>(dpi) / 96.0f));
                }
            }
        }
    }
    HMONITOR hMon = (hwnd && IsWindow(hwnd))
        ? MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    int workW = GetSystemMetrics(SM_CXSCREEN);
    int workH = GetSystemMetrics(SM_CYSCREEN);
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        workW = mi.rcWork.right - mi.rcWork.left;
        workH = mi.rcWork.bottom - mi.rcWork.top;
    }
    const float maxByW = (workW > 120)
        ? (static_cast<float>(workW) * 0.94f / static_cast<float>((std::max)(1, logicalW + 24)))
        : 1.0f;
    const float maxByH = (workH > 120)
        ? (static_cast<float>(workH) * 0.94f / static_cast<float>((std::max)(1, logicalH + 56)))
        : 1.0f;
    const float maxSafe = (std::min)(maxByW, maxByH);
    if (target > maxSafe) {
        target = std::floor((std::max)(1.0f, maxSafe) * 4.0f + 0.001f) / 4.0f;
    }
    return ClampAndSnapUiScale(target);
}

inline int UiToDevice(int logicalVal, float scale = g_uiScale) {
    return static_cast<int>(std::lround(static_cast<float>(logicalVal) * scale));
}

inline int DeviceToLogical(int deviceVal, float scale = g_uiScale) {
    if (scale <= 0.01f) return deviceVal;
    return static_cast<int>(std::lround(static_cast<float>(deviceVal) / scale));
}

inline RECT DeviceRectToLogical(const RECT& r, float scale = g_uiScale) {
    return RECT{
        DeviceToLogical(r.left, scale),
        DeviceToLogical(r.top, scale),
        DeviceToLogical(r.right, scale),
        DeviceToLogical(r.bottom, scale)
    };
}

inline void SyncGraphicsTransform(HDC hdc, Gdiplus::Graphics& g) {
    // Gdiplus::Graphics(hdc) already inherits the HDC's GM_ADVANCED WorldTransform
    // internally when created from an HDC; calling g.SetTransform with XFORM again
    // doubles the scale factor (e.g., 2x -> 4x). Keep as a no-op hook.
    (void)hdc;
    (void)g;
}

struct UiScaleScope {
    HDC hdc = nullptr;
    int savedDc = 0;
    bool active = false;

    UiScaleScope(HDC dc, float scale, float offsetX = 0.0f, float offsetY = 0.0f)
        : hdc(dc) {
        if (!hdc) return;
        if (std::fabs(scale - 1.0f) < 0.001f && std::fabs(offsetX) < 0.001f && std::fabs(offsetY) < 0.001f) {
            return;
        }
        savedDc = SaveDC(hdc);
        SetGraphicsMode(hdc, GM_ADVANCED);
        XFORM xf{};
        xf.eM11 = scale;
        xf.eM22 = scale;
        xf.eDx  = offsetX;
        xf.eDy  = offsetY;
        SetWorldTransform(hdc, &xf);
        active = true;
    }
    ~UiScaleScope() {
        if (active && hdc && savedDc) {
            RestoreDC(hdc, savedDc);
        }
    }
    UiScaleScope(const UiScaleScope&) = delete;
    UiScaleScope& operator=(const UiScaleScope&) = delete;
};

inline void ApplyDwmDarkTitleBar(HWND hwnd, bool dark) {
    UiGlass::ApplyGoldenGateWindowFrame(hwnd, dark);
    if (hwnd && IsWindow(hwnd) && IsWindowVisible(hwnd)) {
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

inline void EnableThemeScrollbar(HWND hwnd, bool dark) {
    if (!hwnd || !IsWindow(hwnd)) return;
    HMODULE hUx = LoadLibraryW(L"uxtheme.dll");
    if (!hUx) return;
    using PfnSetWindowTheme = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    auto fn = reinterpret_cast<PfnSetWindowTheme>(GetProcAddress(hUx, "SetWindowTheme"));
    if (fn) {
        fn(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    FreeLibrary(hUx);
}

inline void EnableDarkScrollbar(HWND hwnd) {
    EnableThemeScrollbar(hwnd, g_studioDarkTheme);
}

inline HBRUSH GetDarkInputBrush() {
    static HBRUSH s_br = CreateSolidBrush(InputBg);
    return s_br;
}

inline HBRUSH GetLightInputBrush() {
    static HBRUSH s_br = CreateSolidBrush(Light::InputBg);
    return s_br;
}

inline HBRUSH GetStudioInputBrush() {
    return g_studioDarkTheme ? GetDarkInputBrush() : GetLightInputBrush();
}

inline HBRUSH GetSurface1Brush() {
    static HBRUSH s_br = CreateSolidBrush(Surface1);
    return s_br;
}

inline HBRUSH GetStudioSurfaceBrush() {
    static HBRUSH s_brLight = CreateSolidBrush(Light::Surface1);
    return g_studioDarkTheme ? GetSurface1Brush() : s_brLight;
}

inline LRESULT HandleStudioEditCtlColor(WPARAM wParam) {
    HDC hdc = reinterpret_cast<HDC>(wParam);
    SetTextColor(hdc, StudioText());
    SetBkColor(hdc, StudioInputBg());
    return reinterpret_cast<LRESULT>(GetStudioInputBrush());
}

inline LRESULT HandleDarkEditCtlColor(WPARAM wParam) {
    return HandleStudioEditCtlColor(wParam);
}

inline void AddRoundRectPath(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float r) {
    float maxR = (std::min)(w, h) * 0.48f;
    float rr = (std::max)(0.5f, (std::min)(r, maxR));
    float d = rr * 2.0f;
    path.Reset();
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

inline void FillRoundRectAA(
    HDC hdc, const RECT& rc, float radius,
    COLORREF fill, COLORREF border = CLR_INVALID, float borderW = 1.0f,
    BYTE fillAlpha = 255, BYTE borderAlpha = 255)
{
    int iw = rc.right - rc.left;
    int ih = rc.bottom - rc.top;
    if (iw <= 1 || ih <= 1 || !EnsureGdiplusInitialized()) return;

    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    float fx = static_cast<float>(rc.left) + 0.5f;
    float fy = static_cast<float>(rc.top) + 0.5f;
    float fw = static_cast<float>(iw) - 1.0f;
    float fh = static_cast<float>(ih) - 1.0f;
    if (fw <= 1.0f || fh <= 1.0f) return;

    Gdiplus::GraphicsPath path;
    AddRoundRectPath(path, fx, fy, fw, fh, radius);

    if (fill != CLR_INVALID && fillAlpha > 0) {
        Gdiplus::SolidBrush br(ToGpColor(fill, fillAlpha));
        g.FillPath(&br, &path);
    }
    if (border != CLR_INVALID && borderW > 0.0f && borderAlpha > 0) {
        Gdiplus::Pen pen(ToGpColor(border, borderAlpha), borderW);
        g.DrawPath(&pen, &path);
    }
}

inline void DrawSoftShadow(HDC hdc, const RECT& rc, float radius, int layers = 4) {
    if (!EnsureGdiplusInitialized()) return;
    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for (int i = layers; i >= 1; --i) {
        RECT r = rc;
        InflateRect(&r, i, i);
        OffsetRect(&r, 0, i / 2 + 1);
        float fw = static_cast<float>(r.right - r.left);
        float fh = static_cast<float>(r.bottom - r.top);
        if (fw <= 2.0f || fh <= 2.0f) continue;
        Gdiplus::GraphicsPath path;
        AddRoundRectPath(path, static_cast<float>(r.left), static_cast<float>(r.top), fw, fh, radius + i);
        BYTE a = static_cast<BYTE>((std::max)(6, 36 - i * 7));
        Gdiplus::SolidBrush br(Gdiplus::Color(a, 0, 0, 0));
        g.FillPath(&br, &path);
    }
}

inline void DrawInputChrome(HDC hdc, const RECT& editRc, bool focused = false) {
    RECT box = editRc;
    InflateRect(&box, 6, 4);
    FillRoundRectAA(hdc, box, 7.0f, StudioInputBg(), focused ? StudioAccent() : StudioBorderStrong(), focused ? 1.5f : 1.0f);
}

inline void DrawModernPill(
    HDC hdc, const RECT& rc, COLORREF fill, COLORREF border, COLORREF textCol,
    const std::wstring& txt, HFONT hFont, float radius = 7.0f)
{
    FillRoundRectAA(hdc, rc, radius, fill, border, 1.0f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textCol);
    HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
    RECT tx = rc;
    int pad = ((rc.right - rc.left) < 38) ? 2 : 6;
    tx.left += pad;
    tx.right -= pad;
    DrawTextW(hdc, txt.c_str(), -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (oldF) SelectObject(hdc, oldF);
}

inline void DrawTrashIconGdi(HDC hdc, const RECT& rc, COLORREF strokeColor, float strokeW = 1.45f) {
    if (!EnsureGdiplusInitialized()) return;
    const int iw = rc.right - rc.left;
    const int ih = rc.bottom - rc.top;
    if (iw < 10 || ih < 10) return;

    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    const float cx = (rc.left + rc.right) * 0.5f;
    const float cy = (rc.top + rc.bottom) * 0.5f;
    const float sc = (std::min)(static_cast<float>(iw), static_cast<float>(ih)) / 24.0f;
    const float u = (std::max)(0.75f, (std::min)(1.35f, sc));

    Gdiplus::Pen pen(ToGpColor(strokeColor, 240), strokeW);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);

    // Handle on lid
    g.DrawLine(&pen, cx - 2.2f * u, cy - 4.0f * u, cx - 1.8f * u, cy - 5.6f * u);
    g.DrawLine(&pen, cx - 1.8f * u, cy - 5.6f * u, cx + 1.8f * u, cy - 5.6f * u);
    g.DrawLine(&pen, cx + 1.8f * u, cy - 5.6f * u, cx + 2.2f * u, cy - 4.0f * u);

    // Top lid bar
    g.DrawLine(&pen, cx - 5.4f * u, cy - 3.6f * u, cx + 5.4f * u, cy - 3.6f * u);

    // Tapered basket outline
    Gdiplus::PointF body[4] = {
        { cx - 4.1f * u, cy - 3.5f * u },
        { cx - 3.3f * u, cy + 5.6f * u },
        { cx + 3.3f * u, cy + 5.6f * u },
        { cx + 4.1f * u, cy - 3.5f * u }
    };
    g.DrawLines(&pen, body, 4);

    // Inner vertical slats
    g.DrawLine(&pen, cx - 1.45f * u, cy - 1.1f * u, cx - 1.15f * u, cy + 3.6f * u);
    g.DrawLine(&pen, cx + 1.45f * u, cy - 1.1f * u, cx + 1.15f * u, cy + 3.6f * u);
}

inline bool IsDirectionalKeyId(int model, int keyId) {
    if (model == 0) return (keyId >= 2 && keyId <= 5);
    if (model == 1) return (keyId == 2 || keyId == 5 || keyId == 6 || keyId == 7);
    if (model == 2) return (keyId >= 10 && keyId <= 13);
    if (model == 3) return (keyId == 2 || keyId == 6 || keyId == 7 || keyId == 8 || keyId == 10);
    return false;
}

inline void DrawDirectionalKeyHighlightBitmap(
    HBITMAP hBmp, int bmpW, int bmpH,
    int boundsL, int boundsT,
    int model, int keyId,
    int keyCx, int keyCy, int keyW, int keyH)
{
    if (!hBmp || bmpW <= 4 || bmpH <= 4 || !IsDirectionalKeyId(model, keyId)) return;
    if (!EnsureGdiplusInitialized()) return;

    HDC memDc = CreateCompatibleDC(nullptr);
    if (!memDc) return;
    HGDIOBJ oldBmp = SelectObject(memDc, hBmp);
    if (oldBmp) {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

        const float rw = (std::max)(8.0f, static_cast<float>(keyW));
        const float rh = (std::max)(8.0f, static_cast<float>(keyH));
        const float pad = (model == 3) ? 4.0f : 2.0f;
        float lx = static_cast<float>(keyCx - boundsL) - rw * 0.5f + pad * 0.5f;
        float ly = static_cast<float>(keyCy - boundsT) - rh * 0.5f + pad * 0.5f;
        float lw = (std::max)(6.0f, rw - pad);
        float lh = (std::max)(6.0f, rh - pad);

        const float margin = (std::max)(2.0f, static_cast<float>(bmpW) * 0.025f);
        if (lx < margin) { lw -= (margin - lx); lx = margin; }
        if (ly < margin) { lh -= (margin - ly); ly = margin; }
        if (lx + lw > bmpW - margin) lw = static_cast<float>(bmpW) - margin - lx;
        if (ly + lh > bmpH - margin) lh = static_cast<float>(bmpH) - margin - ly;

        if (lw > 4.0f && lh > 4.0f) {
            const float haloW  = (std::max)(2.8f, static_cast<float>(bmpW) * 0.052f);
            const float ringW  = (std::max)(1.8f, static_cast<float>(bmpW) * 0.027f);
            const float innerW = (std::max)(1.0f, static_cast<float>(bmpW) * 0.011f);

            // Translucent sky-blue fill + glowing outer halo + crisp hover ring (matches fx-991CW hover style)
            Gdiplus::SolidBrush brFill(Gdiplus::Color(86, 56, 152, 255));
            Gdiplus::Pen penHalo(Gdiplus::Color(118, 76, 172, 255), haloW);
            Gdiplus::Pen penRing(Gdiplus::Color(245, 96, 188, 255), ringW);
            Gdiplus::Pen penInner(Gdiplus::Color(200, 224, 244, 255), innerW);

            g.FillEllipse(&brFill, lx, ly, lw, lh);
            g.DrawEllipse(&penHalo, lx, ly, lw, lh);
            g.DrawEllipse(&penRing, lx, ly, lw, lh);
            if (lw > 10.0f && lh > 10.0f) {
                g.DrawEllipse(&penInner, lx + ringW * 0.45f, ly + ringW * 0.45f, lw - ringW * 0.9f, lh - ringW * 0.9f);
            }
        }
        GdiFlush();
        SelectObject(memDc, oldBmp);
    }
    DeleteDC(memDc);
}

inline void DrawModernButton(
    HDC hdc, const RECT& rc, const std::wstring& txt, HFONT hFont,
    int variant = BTN_SECONDARY, bool active = false, bool hovered = false)
{
    COLORREF fill = Surface2;
    COLORREF border = BorderSubtle;
    COLORREF fg = TextPrimary;

    switch (variant) {
    case BTN_PRIMARY:
        fill   = hovered ? AccentHover : AccentBlue;
        border = hovered ? AccentText : AccentBlue;
        fg     = RGB(255, 255, 255);
        break;
    case BTN_SUCCESS:
        fill   = active ? RGB(20, 120, 88) : SuccessSoft;
        border = active ? SuccessGreen : RGB(30, 118, 88);
        fg     = active ? RGB(255, 255, 255) : SuccessText;
        break;
    case BTN_VIOLET:
        fill   = active ? RGB(92, 62, 168) : VioletSoft;
        border = active ? VioletAccent : RGB(96, 74, 162);
        fg     = active ? RGB(255, 255, 255) : VioletText;
        break;
    case BTN_AMBER:
        fill   = active ? RGB(154, 96, 16) : AmberSoft;
        border = active ? AmberWarm : RGB(142, 98, 28);
        fg     = active ? RGB(255, 255, 255) : AmberText;
        break;
    case BTN_DANGER:
        fill   = active ? RGB(176, 38, 64) : RoseSoft;
        border = active ? RoseRed : RGB(146, 52, 74);
        fg     = active ? RGB(255, 255, 255) : RoseText;
        break;
    case BTN_GHOST:
        fill   = active ? Surface2 : (hovered ? Surface2 : Surface1);
        border = active ? AccentBlue : (hovered ? BorderStrong : BorderSubtle);
        fg     = active ? AccentText : (hovered ? TextPrimary : TextSecondary);
        break;
    case BTN_TOOLBAR_GHOST:
        fill   = active ? AccentSoft : (hovered ? Surface2 : CLR_INVALID);
        border = active ? AccentBlue : (hovered ? BorderSubtle : CLR_INVALID);
        fg     = active ? AccentText : (hovered ? TextPrimary : TextSecondary);
        break;
    default: // BTN_SECONDARY
        fill   = active ? AccentSoft : (hovered ? Surface3 : Surface2);
        border = active ? AccentBlue : (hovered ? BorderStrong : BorderSubtle);
        fg     = active ? RGB(240, 248, 255) : TextPrimary;
        break;
    }
    if (fill == CLR_INVALID && border == CLR_INVALID) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fg);
        HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
        RECT tx = rc;
        int pad = ((rc.right - rc.left) < 38) ? 2 : 6;
        tx.left += pad;
        tx.right -= pad;
        DrawTextW(hdc, txt.c_str(), -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (oldF) SelectObject(hdc, oldF);
    } else {
        DrawModernPill(hdc, rc, fill, border, fg, txt, hFont, 7.0f);
    }
}

inline void DrawSegmentItem(
    HDC hdc, const RECT& rc, const std::wstring& txt, HFONT hFont,
    bool selected, bool hovered, bool emphasizeAccent = false)
{
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    if (selected) {
        const COLORREF fill = emphasizeAccent ? AccentBlue : Surface3;
        const COLORREF border = emphasizeAccent ? AccentHover : BorderStrong;
        const COLORREF fg = RGB(255, 255, 255);
        DrawModernPill(hdc, rc, fill, border, fg, txt, hFont, 7.0f);
    } else if (hovered) {
        DrawModernPill(hdc, rc, RGB(34, 42, 58), CLR_INVALID, TextPrimary, txt, hFont, 7.0f);
    } else {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, TextSecondary);
        HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
        RECT tx = rc;
        tx.left += 4;
        tx.right -= 4;
        DrawTextW(hdc, txt.c_str(), -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (oldF) SelectObject(hdc, oldF);
    }
}

// Render a true optically refracted material translúcido 2.0 floating capsule over hdc
// safely handling active GM_ADVANCED WorldTransforms (UiScaleScope).
inline void DrawFloatingGlassCapsule(
    HDC hdc, const RECT& logicalRc, float uiScale = 1.0f,
    float logicalCornerRadius = -1.0f, bool prominent = false)
{
    if (!hdc || logicalRc.right <= logicalRc.left + 4 || logicalRc.bottom <= logicalRc.top + 4) return;
    if (!EnsureGdiplusInitialized()) return;

    const int saved = SaveDC(hdc);
    XFORM xf{};
    const bool hasXf = (GetGraphicsMode(hdc) == GM_ADVANCED) && GetWorldTransform(hdc, &xf);
    RECT physRc = logicalRc;
    float effScale = (uiScale > 0.05f) ? uiScale : 1.0f;
    if (hasXf) {
        physRc.left   = static_cast<LONG>(std::lround(logicalRc.left   * xf.eM11 + xf.eDx));
        physRc.top    = static_cast<LONG>(std::lround(logicalRc.top    * xf.eM22 + xf.eDy));
        physRc.right  = static_cast<LONG>(std::lround(logicalRc.right  * xf.eM11 + xf.eDx));
        physRc.bottom = static_cast<LONG>(std::lround(logicalRc.bottom * xf.eM22 + xf.eDy));
        effScale = (xf.eM11 > 0.05f) ? xf.eM11 : effScale;
        ModifyWorldTransform(hdc, nullptr, MWT_IDENTITY);
    }

    const UiGlass::GlassSettings& gs = GetActiveGlassSettings();
    const UiGlass::GlassPalette pal = UiGlass::GetPalette(gs);
    const float physR = (logicalCornerRadius < 0.0f) ? -1.0f : (logicalCornerRadius * effScale);
    if (prominent) {
        UiGlass::DrawTranslucentCapsuleOnHdc(hdc, physRc, gs, physR, pal.accentBlue, 0.66f);
    } else {
        UiGlass::DrawTranslucentCapsuleOnHdc(hdc, physRc, gs, physR);
    }
    RestoreDC(hdc, saved);
}

// guías de interfaz macOS 27 Golden Gate atmospheric header band with subtle tonal wave
// and ScrollEdgeEffectStyle.soft transition (never harsh opaque dark slab)
inline void DrawTranslucentHeaderStrip(HDC hdc, const RECT& rcHeader, bool withBottomDivider = true)
{
    if (!hdc || rcHeader.right <= rcHeader.left || rcHeader.bottom <= rcHeader.top) return;
    if (!EnsureGdiplusInitialized()) return;
    const UiGlass::GlassSettings& gs = GetActiveGlassSettings();
    const UiGlass::GlassPalette pal = UiGlass::GetPalette(gs);
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float x = static_cast<float>(rcHeader.left);
    const float y = static_cast<float>(rcHeader.top);
    const float w = static_cast<float>(rcHeader.right - rcHeader.left);
    const float h = static_cast<float>(rcHeader.bottom - rcHeader.top);

    Gdiplus::LinearGradientBrush base(
        Gdiplus::PointF(x, y), Gdiplus::PointF(x, y + h),
        UiGlass::ToGdip(pal.windowCanvasTop), UiGlass::ToGdip(pal.windowCanvasBottom));
    g.FillRectangle(&base, x, y, w, h);

    if (!gs.IsHighContrast()) {
        Gdiplus::LinearGradientBrush wave(
            Gdiplus::PointF(x, y), Gdiplus::PointF(x + w, y + h),
            UiGlass::ToGdip(pal.accentBlue, gs.IsDark() ? 24 : 18),
            UiGlass::ToGdip(pal.accentIndigo, gs.IsDark() ? 18 : 13));
        g.FillRectangle(&wave, x, y, w, h);
    }

    const int fadeH = (std::min)(14, static_cast<int>(h / 3));
    if (fadeH >= 4) {
        UiGlass::DrawScrollEdgeSoftFade(g, RECT{ rcHeader.left, rcHeader.bottom - fadeH, rcHeader.right, rcHeader.bottom }, gs, false);
    }
    if (withBottomDivider) {
        Gdiplus::Pen sep(UiGlass::ToGdip(pal.separator), 1.0f);
        g.DrawLine(&sep, x, y + h - 1.0f, x + w, y + h - 1.0f);
    }
}

inline void DrawStudioAtmosphericBackdrop(HDC hdc, const RECT& rc)
{
    if (rc.right <= rc.left || rc.bottom <= rc.top || !EnsureGdiplusInitialized()) return;
    const UiGlass::GlassSettings& gs = GetActiveGlassSettings();
    const UiGlass::GlassPalette pal = UiGlass::GetPalette(gs);
    Gdiplus::Graphics g(hdc);
    const Gdiplus::RectF r(static_cast<float>(rc.left), static_cast<float>(rc.top),
        static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top));

    Gdiplus::LinearGradientBrush base(r,
        UiGlass::ToGdip(pal.windowCanvasTop),
        UiGlass::ToGdip(pal.windowCanvasBottom),
        Gdiplus::LinearGradientModeVertical);
    g.FillRectangle(&base, r);

    // Subtle guías de interfaz atmospheric radial aura behind top floating glass bar & bottom dock
    // so optical Snell bevel refraction catches genuine chromatic nuance without distracting from hardware
    if (!gs.IsHighContrast()) {
        Gdiplus::LinearGradientBrush topAura(
            Gdiplus::PointF(r.X, r.Y),
            Gdiplus::PointF(r.X + r.Width, r.Y + (std::min)(r.Height, 120.0f)),
            UiGlass::ToGdip(pal.accentBlue, gs.IsDark() ? 22 : 16),
            UiGlass::ToGdip(pal.accentIndigo, gs.IsDark() ? 14 : 10));
        g.FillRectangle(&topAura, r.X, r.Y, r.Width, (std::min)(r.Height, 76.0f));
    }
}

inline void DrawStudioButton(
    HDC hdc, const RECT& rc, const std::wstring& txt, HFONT hFont,
    int variant = BTN_SECONDARY, bool active = false, bool hovered = false, float radius = 7.0f)
{
    const UiGlass::GlassSettings& gs = GetActiveGlassSettings();
    const UiGlass::GlassPalette pal = UiGlass::GetPalette(gs);
    const bool hc = gs.IsHighContrast();
    if (!g_studioDarkTheme) {
        COLORREF fill = hc ? pal.contentCard.ToColorRef() : Light::Surface1;
        COLORREF border = hc ? pal.borderStrong.ToColorRef() : Light::BorderSubtle;
        COLORREF fg = hc ? pal.labelPrimary.ToColorRef() : Light::TextPrimary;
        switch (variant) {
        case BTN_PRIMARY:
            fill   = hovered ? Light::AccentHover : pal.accentBlue.ToColorRef();
            border = hc ? pal.borderStrong.ToColorRef() : fill;
            fg     = RGB(255, 255, 255);
            break;
        case BTN_SUCCESS:
            fill   = active ? Light::SuccessGreen : Light::SuccessSoft;
            border = hc ? pal.borderStrong.ToColorRef() : (active ? Light::SuccessGreen : RGB(174, 224, 202));
            fg     = active ? RGB(255, 255, 255) : Light::SuccessText;
            break;
        case BTN_VIOLET:
            fill   = active ? Light::VioletAccent : Light::VioletSoft;
            border = hc ? pal.borderStrong.ToColorRef() : (active ? Light::VioletAccent : RGB(205, 194, 246));
            fg     = active ? RGB(255, 255, 255) : Light::VioletText;
            break;
        case BTN_AMBER:
            fill   = active ? Light::AmberWarm : Light::AmberSoft;
            border = hc ? pal.borderStrong.ToColorRef() : (active ? Light::AmberWarm : RGB(236, 208, 158));
            fg     = active ? RGB(255, 255, 255) : Light::AmberText;
            break;
        case BTN_DANGER:
            fill   = active ? Light::RoseRed : Light::RoseSoft;
            border = hc ? pal.borderStrong.ToColorRef() : (active ? Light::RoseRed : RGB(240, 190, 204));
            fg     = active ? RGB(255, 255, 255) : Light::RoseText;
            break;
        case BTN_TOOLBAR_GHOST:
            fill   = active ? pal.accentBlueSoft.ToColorRef() : (hovered ? pal.glassPillHover.ToColorRef() : CLR_INVALID);
            border = active ? pal.accentBlue.ToColorRef() : (hovered ? (hc ? pal.borderStrong.ToColorRef() : pal.borderSubtle.ToColorRef()) : CLR_INVALID);
            fg     = active ? pal.accentBlue.ToColorRef() : (hovered ? pal.labelPrimary.ToColorRef() : (hc ? pal.labelPrimary.ToColorRef() : pal.labelSecondary.ToColorRef()));
            break;
        default: // BTN_SECONDARY / BTN_GHOST
            fill   = active ? pal.accentBlueSoft.ToColorRef() : (hovered ? pal.contentCardElevated.ToColorRef() : pal.contentCard.ToColorRef());
            border = active ? pal.accentBlue.ToColorRef() : (hc ? pal.borderStrong.ToColorRef() : (hovered ? Light::BorderStrong : Light::BorderSubtle));
            fg     = active ? pal.accentBlue.ToColorRef() : pal.labelPrimary.ToColorRef();
            break;
        }
        if (fill == CLR_INVALID && border == CLR_INVALID) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, fg);
            HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
            RECT tx = rc;
            int pad = ((rc.right - rc.left) < 38) ? 2 : 6;
            tx.left += pad;
            tx.right -= pad;
            DrawTextW(hdc, txt.c_str(), -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (oldF) SelectObject(hdc, oldF);
        } else {
            DrawModernPill(hdc, rc, fill, border, fg, txt, hFont, radius);
        }
        return;
    }
    COLORREF fill = pal.contentCardElevated.ToColorRef();
    COLORREF border = hc ? pal.borderStrong.ToColorRef() : pal.borderSubtle.ToColorRef();
    COLORREF fg = pal.labelPrimary.ToColorRef();
    switch (variant) {
    case BTN_PRIMARY:
        fill   = hovered ? pal.accentBlueHover.ToColorRef() : pal.accentBlue.ToColorRef();
        border = hc ? pal.borderStrong.ToColorRef() : fill;
        fg     = RGB(255, 255, 255);
        break;
    case BTN_SUCCESS:
        fill   = active ? RGB(20, 128, 88) : SuccessSoft;
        border = hc ? pal.borderStrong.ToColorRef() : (active ? SuccessGreen : RGB(44, 118, 92));
        fg     = active ? RGB(255, 255, 255) : SuccessText;
        break;
    case BTN_VIOLET:
        fill   = active ? RGB(104, 72, 206) : VioletSoft;
        border = hc ? pal.borderStrong.ToColorRef() : (active ? VioletAccent : RGB(102, 82, 168));
        fg     = active ? RGB(255, 255, 255) : VioletText;
        break;
    case BTN_AMBER:
        fill   = active ? RGB(154, 96, 16) : AmberSoft;
        border = hc ? pal.borderStrong.ToColorRef() : (active ? AmberWarm : RGB(142, 98, 28));
        fg     = active ? RGB(255, 255, 255) : AmberText;
        break;
    case BTN_DANGER:
        fill   = active ? RGB(176, 38, 64) : RoseSoft;
        border = hc ? pal.borderStrong.ToColorRef() : (active ? RoseRed : RGB(146, 52, 74));
        fg     = active ? RGB(255, 255, 255) : RoseText;
        break;
    case BTN_TOOLBAR_GHOST:
        fill   = active ? pal.glassPillSelected.ToColorRef() : (hovered ? pal.glassPillHover.ToColorRef() : CLR_INVALID);
        border = active ? pal.accentBlue.ToColorRef() : (hovered ? (hc ? pal.borderStrong.ToColorRef() : pal.borderSubtle.ToColorRef()) : CLR_INVALID);
        fg     = active ? RGB(122, 198, 255) : (hovered ? pal.labelPrimary.ToColorRef() : pal.labelSecondary.ToColorRef());
        break;
    default: // BTN_SECONDARY / BTN_GHOST
        fill   = active ? pal.accentBlueSoft.ToColorRef() : (hovered ? pal.glassPillHover.ToColorRef() : pal.contentCardElevated.ToColorRef());
        border = active ? pal.accentBlue.ToColorRef() : (hc ? pal.borderStrong.ToColorRef() : (hovered ? pal.borderStrong.ToColorRef() : pal.borderSubtle.ToColorRef()));
        fg     = active ? RGB(122, 198, 255) : pal.labelPrimary.ToColorRef();
        break;
    }
    if (fill == CLR_INVALID && border == CLR_INVALID) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fg);
        HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
        RECT tx = rc;
        int pad = ((rc.right - rc.left) < 38) ? 2 : 6;
        tx.left += pad;
        tx.right -= pad;
        DrawTextW(hdc, txt.c_str(), -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (oldF) SelectObject(hdc, oldF);
    } else {
        DrawModernPill(hdc, rc, fill, border, fg, txt, hFont, radius);
    }
}

inline void DrawStudioSegmentItem(
    HDC hdc, const RECT& rc, const std::wstring& txt, HFONT hFont,
    bool selected, bool hovered, bool emphasizeAccent = false)
{
    if (rc.right <= rc.left || rc.bottom <= rc.top) return;
    const UiGlass::GlassSettings& gs = GetActiveGlassSettings();
    const UiGlass::GlassPalette pal = UiGlass::GetPalette(gs);
    const float h = static_cast<float>(rc.bottom - rc.top);
    const float innerR = (std::max)(6.0f, h * 0.45f); // concentric pill radius inside outer capsule

    if (selected) {
        const COLORREF fill = pal.glassPillSelected.ToColorRef();
        const COLORREF border = gs.IsHighContrast()
            ? pal.borderStrong.ToColorRef()
            : (emphasizeAccent ? pal.accentBlue.ToColorRef() : pal.borderSubtle.ToColorRef());
        // WCAG AAA legibility (>8.5:1) for emphasized active pill ink in Dark Mode
        const COLORREF darkAccentFg = RGB(122, 198, 255);
        const COLORREF fg = emphasizeAccent
            ? (gs.IsDark() ? darkAccentFg : pal.accentBlue.ToColorRef())
            : pal.labelPrimary.ToColorRef();
        DrawModernPill(hdc, rc, fill, border, fg, txt, hFont, innerR);
    } else if (hovered) {
        DrawModernPill(hdc, rc, pal.glassPillHover.ToColorRef(), CLR_INVALID, pal.labelPrimary.ToColorRef(), txt, hFont, innerR);
    } else {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, pal.labelSecondary.ToColorRef());
        HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
        RECT tx = rc;
        tx.left += 4;
        tx.right -= 4;
        DrawTextW(hdc, txt.c_str(), -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (oldF) SelectObject(hdc, oldF);
    }
}

inline void DrawCalculatorAmbientHalo(HDC hdc, const RECT& vp, bool active, float radius = 18.0f, bool withPedestal = false, float uiScale = 1.0f)
{
    if (!EnsureGdiplusInitialized()) return;
    const int vw = vp.right - vp.left;
    const int vh = vp.bottom - vp.top;
    if (vw <= 10 || vh <= 10) return;

    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    if (withPedestal) {
        const int padX = (std::max)(8, static_cast<int>(std::lround(11.0f * uiScale)));
        const int padTop = (std::max)(26, static_cast<int>(std::lround(34.0f * uiScale)));
        const int padBot = (std::max)(8, static_cast<int>(std::lround(11.0f * uiScale)));
        RECT rCard = { vp.left - padX, vp.top - padTop, vp.right + padX, vp.bottom + padBot };
        const float cr = (std::max)(11.0f * uiScale, (std::min)(16.0f * uiScale, radius));

        for (int i = 2; i >= 1; --i) {
            RECT rs = rCard;
            InflateRect(&rs, i, i);
            OffsetRect(&rs, 0, i * 2);
            Gdiplus::GraphicsPath ps;
            AddRoundRectPath(ps, static_cast<float>(rs.left), static_cast<float>(rs.top),
                static_cast<float>(rs.right - rs.left), static_cast<float>(rs.bottom - rs.top), cr + i);
            const BYTE a = !g_studioDarkTheme
                ? static_cast<BYTE>(active ? (14 - i * 4) : (9 - i * 3))
                : static_cast<BYTE>(22 - i * 6);
            Gdiplus::SolidBrush sb(!g_studioDarkTheme ? Gdiplus::Color(a, 15, 23, 42) : Gdiplus::Color(a, 0, 0, 0));
            g.FillPath(&sb, &ps);
        }

        const UiGlass::GlassSettings& gs = GetActiveGlassSettings();
        const UiGlass::GlassPalette pal = UiGlass::GetPalette(gs);
        Gdiplus::GraphicsPath pCard;
        AddRoundRectPath(pCard, static_cast<float>(rCard.left), static_cast<float>(rCard.top),
            static_cast<float>(rCard.right - rCard.left), static_cast<float>(rCard.bottom - rCard.top), cr);
        if (!g_studioDarkTheme) {
            Gdiplus::SolidBrush fillBr(active ? UiGlass::ToGdip(pal.contentCard, 255) : UiGlass::ToGdip(pal.contentCard, 244));
            g.FillPath(&fillBr, &pCard);
            const Gdiplus::Color bdCol = active
                ? UiGlass::ToGdip(pal.accentBlue, 240)
                : (gs.IsHighContrast() ? UiGlass::ToGdip(pal.borderStrong, 255) : UiGlass::ToGdip(pal.borderSubtle, 255));
            Gdiplus::Pen borderPen(bdCol, (active || gs.IsHighContrast()) ? (1.6f * uiScale) : (1.0f * uiScale));
            g.DrawPath(&borderPen, &pCard);
        } else {
            Gdiplus::SolidBrush fillBr(active ? Gdiplus::Color(255, 27, 33, 46) : Gdiplus::Color(245, 23, 27, 36));
            g.FillPath(&fillBr, &pCard);
            Gdiplus::Pen borderPen(
                active ? Gdiplus::Color(225, 59, 130, 246) : Gdiplus::Color(255, 42, 51, 69),
                active ? (1.5f * uiScale) : (1.0f * uiScale));
            g.DrawPath(&borderPen, &pCard);
        }
        return;
    }

    // Subtle guías de interfaz grounded elliptical contact shadow strictly beneath the solo chassis
    // (avoids painting a rectangular tinted box behind transparent rounded skin corners)
    const float shW = static_cast<float>(vw) * 0.84f;
    const float shH = (std::max)(12.0f * uiScale, static_cast<float>(vh) * 0.032f);
    const float shX = static_cast<float>(vp.left) + (static_cast<float>(vw) - shW) * 0.5f;
    const float shY = static_cast<float>(vp.bottom) - shH * 0.45f;
    for (int i = 3; i >= 1; --i) {
        const float expandX = static_cast<float>(i) * 5.0f * uiScale;
        const float expandY = static_cast<float>(i) * 2.2f * uiScale;
        const BYTE a = !g_studioDarkTheme
            ? static_cast<BYTE>((std::max)(4, 18 - i * 4))
            : static_cast<BYTE>((std::max)(6, 30 - i * 7));
        Gdiplus::SolidBrush br(!g_studioDarkTheme ? Gdiplus::Color(a, 15, 23, 42) : Gdiplus::Color(a, 0, 0, 0));
        g.FillEllipse(&br, shX - expandX, shY - expandY * 0.35f, shW + expandX * 2.0f, shH + expandY * 1.4f);
    }
}

inline void DrawSidebarVectorGlyph(HDC hdc, const RECT& rc, int kind, COLORREF col) {
    if (kind == GLYPH_NONE || !EnsureGdiplusInitialized()) return;
    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    float cx = (rc.left + rc.right) * 0.5f;
    float cy = (rc.top + rc.bottom) * 0.5f;
    float u = (std::min)(static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top)) * 0.34f;
    if (u < 3.5f) u = 3.5f;
    Gdiplus::Pen pen(ToGpColor(col, 235), 1.35f);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush br(ToGpColor(col, 235));

    switch (kind) {
    case GLYPH_COMPACT: {
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, cx - u * 0.72f, cy - u, u * 1.44f, u * 2.0f, 1.8f);
        g.DrawPath(&pen, &p);
        g.DrawLine(&pen, cx - u * 0.4f, cy - u * 0.42f, cx + u * 0.4f, cy - u * 0.42f);
        break;
    }
    case GLYPH_PIP: {
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, cx - u, cy - u * 0.85f, u * 1.4f, u * 1.25f, 1.5f);
        g.DrawPath(&pen, &p);
        Gdiplus::GraphicsPath p2;
        AddRoundRectPath(p2, cx - u * 0.1f, cy - u * 0.1f, u * 1.1f, u * 0.95f, 1.4f);
        g.FillPath(&br, &p2);
        break;
    }
    case GLYPH_PIN:
        g.DrawLine(&pen, cx - u * 0.55f, cy - u * 0.7f, cx + u * 0.55f, cy - u * 0.7f);
        g.DrawLine(&pen, cx, cy - u * 0.7f, cx, cy + u * 0.95f);
        g.DrawLine(&pen, cx - u * 0.8f, cy + u * 0.15f, cx + u * 0.8f, cy + u * 0.15f);
        break;
    case GLYPH_ZOOM:
        g.DrawEllipse(&pen, cx - u * 0.9f, cy - u * 0.9f, u * 1.35f, u * 1.35f);
        g.DrawLine(&pen, cx + u * 0.25f, cy + u * 0.25f, cx + u * 0.95f, cy + u * 0.95f);
        break;
    case GLYPH_INSPECTOR: {
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, cx - u, cy - u * 0.8f, u * 2.0f, u * 1.6f, 1.8f);
        g.DrawPath(&pen, &p);
        g.DrawLine(&pen, cx + u * 0.3f, cy - u * 0.8f, cx + u * 0.3f, cy + u * 0.8f);
        break;
    }
    case GLYPH_LAYERS: {
        Gdiplus::PointF top[4] = { { cx, cy - u }, { cx + u, cy - u * 0.35f }, { cx, cy + u * 0.3f }, { cx - u, cy - u * 0.35f } };
        g.DrawPolygon(&pen, top, 4);
        g.DrawLine(&pen, cx - u * 0.9f, cy + u * 0.32f, cx, cy + u * 0.85f);
        g.DrawLine(&pen, cx, cy + u * 0.85f, cx + u * 0.9f, cy + u * 0.32f);
        break;
    }
    case GLYPH_STUDIO: {
        Gdiplus::PointF star[4] = { { cx, cy - u }, { cx + u * 0.28f, cy }, { cx, cy + u }, { cx - u * 0.28f, cy } };
        Gdiplus::PointF starH[4] = { { cx - u, cy }, { cx, cy - u * 0.28f }, { cx + u, cy }, { cx, cy + u * 0.28f } };
        g.DrawPolygon(&pen, star, 4);
        g.DrawPolygon(&pen, starH, 4);
        break;
    }
    case GLYPH_HELP:
        g.DrawEllipse(&pen, cx - u * 0.95f, cy - u * 0.95f, u * 1.9f, u * 1.9f);
        g.DrawArc(&pen, cx - u * 0.38f, cy - u * 0.58f, u * 0.76f, u * 0.65f, 190.0f, 210.0f);
        g.DrawLine(&pen, cx, cy + u * 0.05f, cx, cy + u * 0.32f);
        g.FillEllipse(&br, cx - 0.9f, cy + u * 0.52f, 1.8f, 1.8f);
        break;
    case GLYPH_COPY: {
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, cx - u * 0.3f, cy - u * 0.85f, u * 1.15f, u * 1.35f, 1.4f);
        g.DrawPath(&pen, &p);
        g.DrawLine(&pen, cx - u * 0.75f, cy - u * 0.35f, cx - u * 0.75f, cy + u * 0.85f);
        g.DrawLine(&pen, cx - u * 0.75f, cy + u * 0.85f, cx + u * 0.35f, cy + u * 0.85f);
        break;
    }
    case GLYPH_CAMERA: {
        Gdiplus::GraphicsPath p;
        AddRoundRectPath(p, cx - u, cy - u * 0.6f, u * 2.0f, u * 1.4f, 1.8f);
        g.DrawPath(&pen, &p);
        g.DrawEllipse(&pen, cx - u * 0.38f, cy - u * 0.25f, u * 0.76f, u * 0.76f);
        break;
    }
    case GLYPH_EXPORT:
        g.DrawLine(&pen, cx, cy + u * 0.25f, cx, cy - u * 0.9f);
        g.DrawLine(&pen, cx - u * 0.45f, cy - u * 0.45f, cx, cy - u * 0.9f);
        g.DrawLine(&pen, cx + u * 0.45f, cy - u * 0.45f, cx, cy - u * 0.9f);
        g.DrawLine(&pen, cx - u * 0.85f, cy + u * 0.1f, cx - u * 0.85f, cy + u * 0.85f);
        g.DrawLine(&pen, cx - u * 0.85f, cy + u * 0.85f, cx + u * 0.85f, cy + u * 0.85f);
        g.DrawLine(&pen, cx + u * 0.85f, cy + u * 0.85f, cx + u * 0.85f, cy + u * 0.1f);
        break;
    case GLYPH_FOLDER:
        g.DrawLine(&pen, cx - u, cy - u * 0.7f, cx - u * 0.2f, cy - u * 0.7f);
        g.DrawLine(&pen, cx - u * 0.2f, cy - u * 0.7f, cx, cy - u * 0.38f);
        g.DrawLine(&pen, cx, cy - u * 0.38f, cx + u, cy - u * 0.38f);
        g.DrawLine(&pen, cx + u, cy - u * 0.38f, cx + u, cy + u * 0.75f);
        g.DrawLine(&pen, cx + u, cy + u * 0.75f, cx - u, cy + u * 0.75f);
        g.DrawLine(&pen, cx - u, cy + u * 0.75f, cx - u, cy - u * 0.7f);
        break;
    case GLYPH_SAVE:
        g.DrawLine(&pen, cx, cy - u * 0.85f, cx, cy + u * 0.3f);
        g.DrawLine(&pen, cx - u * 0.45f, cy - u * 0.15f, cx, cy + u * 0.3f);
        g.DrawLine(&pen, cx + u * 0.45f, cy - u * 0.15f, cx, cy + u * 0.3f);
        g.DrawLine(&pen, cx - u * 0.85f, cy + u * 0.75f, cx + u * 0.85f, cy + u * 0.75f);
        break;
    case GLYPH_UNDO:
        g.DrawArc(&pen, cx - u * 0.75f, cy - u * 0.55f, u * 1.55f, u * 1.35f, 180.0f, 220.0f);
        g.DrawLine(&pen, cx - u * 0.85f, cy - u * 0.5f, cx - u * 0.35f, cy - u * 0.5f);
        g.DrawLine(&pen, cx - u * 0.85f, cy - u * 0.5f, cx - u * 0.85f, cy);
        break;
    case GLYPH_HIGHLIGHT:
        g.DrawEllipse(&pen, cx - u * 0.9f, cy - u * 0.9f, u * 1.8f, u * 1.8f);
        g.FillEllipse(&br, cx - u * 0.32f, cy - u * 0.32f, u * 0.64f, u * 0.64f);
        break;
    case GLYPH_RESTART:
        g.DrawArc(&pen, cx - u * 0.85f, cy - u * 0.85f, u * 1.7f, u * 1.7f, -45.0f, 285.0f);
        g.DrawLine(&pen, cx + u * 0.35f, cy - u * 0.85f, cx + u * 0.85f, cy - u * 0.85f);
        g.DrawLine(&pen, cx + u * 0.85f, cy - u * 0.85f, cx + u * 0.85f, cy - u * 0.35f);
        break;
    case GLYPH_TRASH:
        g.DrawLine(&pen, cx - u * 0.9f, cy - u * 0.55f, cx + u * 0.9f, cy - u * 0.55f);
        g.DrawLine(&pen, cx - u * 0.65f, cy - u * 0.55f, cx - u * 0.5f, cy + u * 0.85f);
        g.DrawLine(&pen, cx - u * 0.5f, cy + u * 0.85f, cx + u * 0.5f, cy + u * 0.85f);
        g.DrawLine(&pen, cx + u * 0.5f, cy + u * 0.85f, cx + u * 0.65f, cy - u * 0.55f);
        break;
    case GLYPH_SETTINGS:
        g.DrawLine(&pen, cx - u, cy - u * 0.42f, cx + u, cy - u * 0.42f);
        g.FillEllipse(&br, cx - u * 0.55f, cy - u * 0.68f, u * 0.52f, u * 0.52f);
        g.DrawLine(&pen, cx - u, cy + u * 0.42f, cx + u, cy + u * 0.42f);
        g.FillEllipse(&br, cx + u * 0.1f, cy + u * 0.16f, u * 0.52f, u * 0.52f);
        break;
    case GLYPH_MENU:
        g.DrawLine(&pen, cx - u * 0.8f, cy - u * 0.55f, cx + u * 0.8f, cy - u * 0.55f);
        g.DrawLine(&pen, cx - u * 0.8f, cy, cx + u * 0.8f, cy);
        g.DrawLine(&pen, cx - u * 0.8f, cy + u * 0.55f, cx + u * 0.8f, cy + u * 0.55f);
        break;
    case GLYPH_PLAYER: {
        Gdiplus::PointF pts[3] = { { cx - u * 0.5f, cy - u * 0.7f }, { cx - u * 0.5f, cy + u * 0.7f }, { cx + u * 0.75f, cy } };
        g.DrawPolygon(&pen, pts, 3);
        break;
    }
    }
}

inline void DrawToggleSwitch(HDC hdc, const RECT& trackRc, bool checked, COLORREF onColor = AccentBlue, bool lightSurface = false) {
    int tw = trackRc.right - trackRc.left;
    int th = trackRc.bottom - trackRc.top;
    if (tw < 20 || th < 12 || !EnsureGdiplusInitialized()) return;

    float r = static_cast<float>(th) * 0.5f;
    COLORREF trFill = checked ? onColor : (lightSurface ? RGB(203, 213, 225) : RGB(44, 54, 74));
    COLORREF trBord = checked ? onColor : (lightSurface ? RGB(180, 194, 212) : BorderStrong);
    FillRoundRectAA(hdc, trackRc, r, trFill, trBord, 1.0f);

    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    float pad = 2.5f;
    float d = static_cast<float>(th) - pad * 2.0f;
    float kx = checked
        ? (static_cast<float>(trackRc.right) - pad - d)
        : (static_cast<float>(trackRc.left) + pad);
    float ky = static_cast<float>(trackRc.top) + pad;
    UiGlass::DrawAquaVitreousOrb(g, kx + d * 0.5f, ky + d * 0.5f, d * 0.5f,
        Gdiplus::Color(255, 255, 255, 255), false, !lightSurface);
}

inline void DrawRadioCircle(HDC hdc, int cx, int cy, int r, bool checked, COLORREF accent = AccentBlue) {
    if (!EnsureGdiplusInitialized()) return;
    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    float fr = static_cast<float>(r);
    Gdiplus::Pen ring(ToGpColor(checked ? accent : StudioBorderStrong(), 255), 1.8f);
    g.DrawEllipse(&ring, cx - fr, cy - fr, fr * 2.0f, fr * 2.0f);
    if (checked) {
        Gdiplus::SolidBrush dot(ToGpColor(accent, 255));
        float ir = fr * 0.52f;
        g.FillEllipse(&dot, cx - ir, cy - ir, ir * 2.0f, ir * 2.0f);
    }
}

inline void DrawDotIndicator(HDC hdc, int cx, int cy, int r, COLORREF col) {
    if (!EnsureGdiplusInitialized()) return;
    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush br(ToGpColor(col, 255));
    float fr = static_cast<float>(r);
    g.FillEllipse(&br, cx - fr, cy - fr, fr * 2.0f, fr * 2.0f);
}

inline void DrawTransportGlyph(HDC hdc, const RECT& rc, int kind, COLORREF col) {
    if (!EnsureGdiplusInitialized()) return;
    Gdiplus::Graphics g(hdc);
    SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush br(ToGpColor(col, 255));

    float cx = (rc.left + rc.right) * 0.5f;
    float cy = (rc.top + rc.bottom) * 0.5f;
    float s = (std::min)(static_cast<float>(rc.right - rc.left), static_cast<float>(rc.bottom - rc.top)) * 0.27f;

    if (kind == TR_PLAY) {
        Gdiplus::PointF pts[3] = {
            { cx - s * 0.65f, cy - s },
            { cx - s * 0.65f, cy + s },
            { cx + s * 0.95f, cy }
        };
        g.FillPolygon(&br, pts, 3);
    } else if (kind == TR_PAUSE) {
        float bw = (std::max)(2.0f, s * 0.45f);
        g.FillRectangle(&br, cx - bw * 1.35f, cy - s * 0.85f, bw, s * 1.7f);
        g.FillRectangle(&br, cx + bw * 0.35f, cy - s * 0.85f, bw, s * 1.7f);
    } else if (kind == TR_PREV) {
        Gdiplus::PointF pts[3] = {
            { cx + s * 0.65f, cy - s * 0.85f },
            { cx + s * 0.65f, cy + s * 0.85f },
            { cx - s * 0.75f, cy }
        };
        g.FillPolygon(&br, pts, 3);
    } else if (kind == TR_NEXT) {
        Gdiplus::PointF pts[3] = {
            { cx - s * 0.65f, cy - s * 0.85f },
            { cx - s * 0.65f, cy + s * 0.85f },
            { cx + s * 0.75f, cy }
        };
        g.FillPolygon(&br, pts, 3);
    } else if (kind == TR_SKIP_START) {
        float bw = (std::max)(2.0f, s * 0.35f);
        g.FillRectangle(&br, cx - s * 0.9f, cy - s * 0.85f, bw, s * 1.7f);
        Gdiplus::PointF pts[3] = {
            { cx + s * 0.9f, cy - s * 0.85f },
            { cx + s * 0.9f, cy + s * 0.85f },
            { cx - s * 0.35f, cy }
        };
        g.FillPolygon(&br, pts, 3);
    } else if (kind == TR_SKIP_END) {
        float bw = (std::max)(2.0f, s * 0.35f);
        g.FillRectangle(&br, cx + s * 0.55f, cy - s * 0.85f, bw, s * 1.7f);
        Gdiplus::PointF pts[3] = {
            { cx - s * 0.9f, cy - s * 0.85f },
            { cx - s * 0.9f, cy + s * 0.85f },
            { cx + s * 0.35f, cy }
        };
        g.FillPolygon(&br, pts, 3);
    }
}

inline void DrawKeyBadge(HDC hdc, const RECT& rightAnchor, const std::wstring& keys, HFONT hFont, bool active = false) {
    if (keys.empty()) return;
    HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
    SIZE sz{};
    GetTextExtentPoint32W(hdc, keys.c_str(), static_cast<int>(keys.size()), &sz);
    int w = sz.cx + 10;
    int availW = (std::max)(0, static_cast<int>(rightAnchor.right - rightAnchor.left));
    if (w > availW) w = availW;
    int h = (std::min)(static_cast<int>(rightAnchor.bottom - rightAnchor.top), 18);
    int cy = (rightAnchor.top + rightAnchor.bottom) / 2;
    RECT rc{ rightAnchor.right - w, cy - h / 2, rightAnchor.right, cy + (h - h / 2) };
    FillRoundRectAA(hdc, rc, 4.0f,
        active ? AccentSoft : RGB(17, 23, 35),
        active ? AccentBlue : BorderSubtle, 1.0f);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, active ? AccentText : TextMuted);
    DrawTextW(hdc, keys.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (oldF) SelectObject(hdc, oldF);
}

// Convierte el bitmap del chasis CASIO a DIB 32bpp con canal alfa premultiplicado,
// eliminando por inundacion desde las 4 esquinas el recuadro gris uniforme exterior
// sin tocar los botones ni el interior blanco/gris de la calculadora.
inline HBITMAP CreateTransparentSkinAlphaBitmap(HBITMAP hSrcBmp) {
    if (!hSrcBmp) return nullptr;
    BITMAP bm{};
    if (GetObjectW(hSrcBmp, sizeof(bm), &bm) == 0 || bm.bmWidth <= 8 || bm.bmHeight <= 8) return nullptr;
    const int w = bm.bmWidth;
    const int h = (bm.bmHeight < 0) ? -bm.bmHeight : bm.bmHeight;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> src(static_cast<size_t>(w) * h * 4);
    HDC hdcScreen = GetDC(nullptr);
    int rows = GetDIBits(hdcScreen, hSrcBmp, 0, static_cast<UINT>(h), src.data(), &bmi, DIB_RGB_COLORS);
    if (rows <= 0) {
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    auto pixAt = [&](int x, int y, int ch) -> int {
        return src[(static_cast<size_t>(y) * w + x) * 4 + ch];
    };
    const int sB = pixAt(2, 2, 0);
    const int sG = pixAt(2, 2, 1);
    const int sR = pixAt(2, 2, 2);

    std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
    std::deque<int> q;
    const int seeds[4][2] = { {0, 0}, {w - 1, 0}, {0, h - 1}, {w - 1, h - 1} };
    for (const auto& sd : seeds) {
        int sx = sd[0], sy = sd[1];
        int idx = sy * w + sx;
        if (!mask[idx]) {
            mask[idx] = 1;
            q.push_back(idx);
        }
    }

    const int dirs[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    while (!q.empty()) {
        int cur = q.front();
        q.pop_front();
        int x = cur % w;
        int y = cur / w;
        for (const auto& d : dirs) {
            int nx = x + d[0];
            int ny = y + d[1];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            size_t nIdx = static_cast<size_t>(ny) * w + nx;
            if (mask[nIdx]) continue;
            int db = std::abs(pixAt(nx, ny, 0) - sB);
            int dg = std::abs(pixAt(nx, ny, 1) - sG);
            int dr = std::abs(pixAt(nx, ny, 2) - sR);
            if (db <= 14 && dg <= 14 && dr <= 14) {
                mask[nIdx] = 1;
                q.push_back(static_cast<int>(nIdx));
            }
        }
    }

    void* dstBits = nullptr;
    HBITMAP hAlpha = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &dstBits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);
    if (!hAlpha || !dstBits) return nullptr;

    uint8_t* dst = static_cast<uint8_t*>(dstBits);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t pIdx = static_cast<size_t>(y) * w + x;
            size_t o = pIdx * 4;
            if (mask[pIdx]) {
                dst[o + 0] = 0;
                dst[o + 1] = 0;
                dst[o + 2] = 0;
                dst[o + 3] = 0;
            } else {
                // Suavizado 1px en el borde con el fondo eliminado
                int adjZero = 0;
                for (const auto& d : dirs) {
                    int nx = x + d[0], ny = y + d[1];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h && mask[static_cast<size_t>(ny) * w + nx]) {
                        adjZero++;
                    }
                }
                if (adjZero > 0) {
                    const BYTE a = (adjZero >= 2) ? 140 : 205;
                    dst[o + 0] = static_cast<uint8_t>((src[o + 0] * a) / 255);
                    dst[o + 1] = static_cast<uint8_t>((src[o + 1] * a) / 255);
                    dst[o + 2] = static_cast<uint8_t>((src[o + 2] * a) / 255);
                    dst[o + 3] = a;
                } else {
                    dst[o + 0] = src[o + 0];
                    dst[o + 1] = src[o + 1];
                    dst[o + 2] = src[o + 2];
                    dst[o + 3] = 255;
                }
            }
        }
    }
    return hAlpha;
}

inline bool BuildSkinPargbBuffer(HBITMAP hSrcBmp, int w, int h, std::vector<uint8_t>& outPargb) {
    outPargb.clear();
    if (!hSrcBmp || w <= 8 || h <= 8) return false;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uint8_t> src(static_cast<size_t>(w) * h * 4);
    HDC hdcScreen = GetDC(nullptr);
    int rows = GetDIBits(hdcScreen, hSrcBmp, 0, static_cast<UINT>(h), src.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdcScreen);
    if (rows <= 0) return false;

    auto pixAt = [&](int x, int y, int ch) -> int {
        return src[(static_cast<size_t>(y) * w + x) * 4 + ch];
    };
    const int sB = pixAt(2, 2, 0), sG = pixAt(2, 2, 1), sR = pixAt(2, 2, 2);

    std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
    std::deque<int> q;
    const int seeds[4][2] = { {0, 0}, {w - 1, 0}, {0, h - 1}, {w - 1, h - 1} };
    for (const auto& sd : seeds) {
        int idx = sd[1] * w + sd[0];
        if (!mask[idx]) { mask[idx] = 1; q.push_back(idx); }
    }
    const int dirs[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    while (!q.empty()) {
        int cur = q.front(); q.pop_front();
        int x = cur % w, y = cur / w;
        for (const auto& d : dirs) {
            int nx = x + d[0], ny = y + d[1];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            size_t nIdx = static_cast<size_t>(ny) * w + nx;
            if (mask[nIdx]) continue;
            if (std::abs(pixAt(nx, ny, 0) - sB) <= 14 &&
                std::abs(pixAt(nx, ny, 1) - sG) <= 14 &&
                std::abs(pixAt(nx, ny, 2) - sR) <= 14) {
                mask[nIdx] = 1;
                q.push_back(static_cast<int>(nIdx));
            }
        }
    }

    outPargb.resize(static_cast<size_t>(w) * h * 4, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t pIdx = static_cast<size_t>(y) * w + x;
            size_t o = pIdx * 4;
            if (mask[pIdx]) continue;
            int adjZero = 0;
            for (const auto& d : dirs) {
                int nx = x + d[0], ny = y + d[1];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h && mask[static_cast<size_t>(ny) * w + nx]) adjZero++;
            }
            if (adjZero > 0) {
                const BYTE a = (adjZero >= 2) ? 140 : 205;
                outPargb[o + 0] = static_cast<uint8_t>((src[o + 0] * a) / 255);
                outPargb[o + 1] = static_cast<uint8_t>((src[o + 1] * a) / 255);
                outPargb[o + 2] = static_cast<uint8_t>((src[o + 2] * a) / 255);
                outPargb[o + 3] = a;
            } else {
                outPargb[o + 0] = src[o + 0];
                outPargb[o + 1] = src[o + 1];
                outPargb[o + 2] = src[o + 2];
                outPargb[o + 3] = 255;
            }
        }
    }
    return true;
}

inline HBITMAP EnsureScaledSkinBitmap(
    int srcW, int srcH, const std::vector<uint8_t>& srcPargb,
    int dstW, int dstH, HBITMAP& cachedBmp, int& cachedW, int& cachedH)
{
    if (dstW <= 0 || dstH <= 0 || srcW <= 0 || srcH <= 0 || srcPargb.empty()) return nullptr;
    if (cachedBmp && cachedW == dstW && cachedH == dstH) return cachedBmp;
    if (cachedBmp) {
        DeleteObject(cachedBmp);
        cachedBmp = nullptr;
    }
    cachedW = 0;
    cachedH = 0;
    if (!EnsureGdiplusInitialized()) return nullptr;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dstW;
    bmi.bmiHeader.biHeight = -dstH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hOut = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hOut || !bits) {
        ReleaseDC(nullptr, hdcScreen);
        if (hOut) DeleteObject(hOut);
        return nullptr;
    }
    std::memset(bits, 0, static_cast<size_t>(dstW) * dstH * 4);

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HGDIOBJ oldB = SelectObject(hdcMem, hOut);
    {
        Gdiplus::Bitmap srcBmp(srcW, srcH, srcW * 4, PixelFormat32bppPARGB,
            const_cast<BYTE*>(srcPargb.data()));
        Gdiplus::Graphics g(hdcMem);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.DrawImage(&srcBmp, 0, 0, dstW, dstH);
    }
    SelectObject(hdcMem, oldB);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    cachedBmp = hOut;
    cachedW = dstW;
    cachedH = dstH;
    return cachedBmp;
}

} // namespace CasioTheme
