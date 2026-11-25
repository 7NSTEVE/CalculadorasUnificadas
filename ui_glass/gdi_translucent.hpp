#pragma once
// ============================================================================
// ui_glass/gdi_translucent.hpp
// Win32 HDC + GDI+ bridge for the physically based material translúcido 2.0 compositor
// Captures real backdrop pixels behind floating controls, executes Dual Kawase
// blur + spectral Snell RGB bevel lensing + Schlick Fresnel + opposing @1/@2
// specular rims + UserOpacitySlider + HighContrast 1px border, and blits back.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include "translucent_math.hpp"
#include "translucent_tokens.hpp"
#include "vector_icons.hpp"

#include <algorithm>
#include <cstring>
#include <string>

namespace UiGlass {

inline Gdiplus::Color ToGdip(Rgba8 c, std::uint8_t alphaOverride = 255) noexcept {
    return Gdiplus::Color(alphaOverride == 255 ? c.a : alphaOverride, c.r, c.g, c.b);
}

inline void AddRoundPath(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    const float rr = (std::max)(0.5f, (std::min)(r, (std::min)(w, h) * 0.49f));
    const float d = rr * 2.0f;
    p.Reset();
    p.AddArc(x, y, d, d, 180.0f, 90.0f);
    p.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    p.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    p.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    p.CloseFigure();
}

// Render a true optically refracted material translúcido 2.0 floating capsule over an existing HDC backdrop.
// Pass cornerRadius < 0 for a full pill capsule (r = height / 2).
inline void DrawTranslucentCapsuleOnHdc(
    HDC hdcBackdrop,
    const RECT& rc,
    const GlassSettings& settings,
    float cornerRadius = -1.0f,
    Rgba8 tint = { 255, 255, 255, 255 },
    float tintProminence = 0.0f,
    float* outMeanBackdropLuma = nullptr)
{
    if (!hdcBackdrop) return;
    const int boxW = rc.right - rc.left;
    const int boxH = rc.bottom - rc.top;
    if (boxW <= 4 || boxH <= 4) return;

    const int padX = 8;
    const int padY = 10;
    const int capX = rc.left - padX;
    const int capY = rc.top - padY;
    const int bufW = boxW + padX * 2;
    const int bufH = boxH + padY * 2;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bufW;
    bmi.bmiHeader.biHeight = -bufH; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcCap = CreateCompatibleDC(hdcBackdrop);
    if (!hdcCap) return;
    HBITMAP hDib = CreateDIBSection(hdcBackdrop, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hDib || !bits) {
        if (hDib) DeleteObject(hDib);
        DeleteDC(hdcCap);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(hdcCap, hDib);
    // Pre-fill capture buffer with theme canvas color so out-of-bounds margins (e.g. capY < 0)
    // never introduce uninitialized black pixels into Dual Kawase blur passes.
    {
        const GlassPalette capPal = GetPalette(settings);
        HBRUSH brPad = CreateSolidBrush(capPal.windowCanvasTop.ToColorRef());
        RECT rcCapAll{ 0, 0, bufW, bufH };
        FillRect(hdcCap, &rcCapAll, brPad);
        DeleteObject(brPad);
    }
    // Capture live pixels already drawn behind the floating capsule (clamped safely to [0, backdropSize])
    const int srcX = (std::max)(0, capX);
    const int srcY = (std::max)(0, capY);
    const int dstX = srcX - capX;
    const int dstY = srcY - capY;
    int copyW = bufW - dstX;
    int copyH = bufH - dstY;
    if (HBITMAP hSrcBmp = static_cast<HBITMAP>(GetCurrentObject(hdcBackdrop, OBJ_BITMAP))) {
        BITMAP bm{};
        if (GetObject(hSrcBmp, sizeof(bm), &bm) == sizeof(bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            copyW = (std::min)(copyW, (std::max)(0, static_cast<int>(bm.bmWidth) - srcX));
            copyH = (std::min)(copyH, (std::max)(0, static_cast<int>(bm.bmHeight) - srcY));
        }
    }
    if (copyW > 0 && copyH > 0) {
        BitBlt(hdcCap, dstX, dstY, copyW, copyH, hdcBackdrop, srcX, srcY, SRCCOPY);
    }
    GdiFlush();

    OpticalGlassParams op{};
    op.cornerRadius = (cornerRadius < 0.0f) ? (static_cast<float>(boxH) * 0.5f) : cornerRadius;
    op.bevelWidth = (std::min)(18.0f, static_cast<float>((std::min)(boxW, boxH)) * 0.44f);
    op.bevelHeight = (std::min)(10.0f, op.bevelWidth * 0.55f);
    op.baseThickness = 14.0f;
    op.userOpacitySlider = settings.EffectiveOpacity();
    op.tintR = tint.Rf();
    op.tintG = tint.Gf();
    op.tintB = tint.Bf();
    op.tintProminence = std::clamp(tintProminence, 0.0f, 0.78f);
    op.darkMode = settings.IsDark();
    op.highContrastBorder = settings.IsHighContrast();
    op.reduceTransparency = settings.reduceTransparency;
    op.blurPasses = (boxW * boxH > 120000) ? 1 : 2;

    CompositeTranslucentCapsuleBgra32(
        static_cast<std::uint8_t*>(bits),
        bufW, bufH, padX, padY, op, outMeanBackdropLuma);

    // Blit optically refracted material translúcido 2.0 + soft shadow back onto hdcBackdrop
    if (copyW > 0 && copyH > 0) {
        BitBlt(hdcBackdrop, srcX, srcY, copyW, copyH, hdcCap, dstX, dstY, SRCCOPY);
    }

    SelectObject(hdcCap, oldBmp);
    DeleteObject(hDib);
    DeleteDC(hdcCap);
}

// macOS 27 Golden Gate ("Aqua 2.0") vitreous glossy orb for ink wells & window badges
inline void DrawAquaVitreousOrb(
    Gdiplus::Graphics& g, float cx, float cy, float radius,
    Gdiplus::Color baseCol, bool selected = false, bool dark = false)
{
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float d = radius * 2.0f;
    // Soft outer selection ring
    if (selected) {
        Gdiplus::Pen ring(dark ? Gdiplus::Color(240, 10, 132, 255) : Gdiplus::Color(240, 0, 122, 255), 2.0f);
        g.DrawEllipse(&ring, cx - radius - 2.4f, cy - radius - 2.4f, d + 4.8f, d + 4.8f);
    }
    // Base translucent vitreous body
    Gdiplus::SolidBrush body(baseCol);
    g.FillEllipse(&body, cx - radius, cy - radius, d, d);
    // Internal bottom bounce light (Aqua 2.0 jewel glow)
    Gdiplus::LinearGradientBrush glow(
        Gdiplus::PointF(cx, cy - radius), Gdiplus::PointF(cx, cy + radius),
        Gdiplus::Color(150, 255, 255, 255), Gdiplus::Color(40, 255, 255, 255));
    g.FillEllipse(&glow, cx - radius * 0.62f, cy - radius * 0.78f, radius * 1.24f, radius * 0.68f);
    // Crisp specular edge rim
    Gdiplus::Pen rim(Gdiplus::Color(dark ? 95 : 75, 0, 0, 0), 0.9f);
    g.DrawEllipse(&rim, cx - radius, cy - radius, d, d);
}

// guías de interfaz ScrollEdgeEffectStyle.soft atmospheric top/bottom veil
inline void DrawScrollEdgeSoftFade(
    Gdiplus::Graphics& g, const RECT& rc, const GlassSettings& settings, bool fromTop = true)
{
    const float w = static_cast<float>(rc.right - rc.left);
    const float h = static_cast<float>(rc.bottom - rc.top);
    if (w <= 1.0f || h <= 1.0f) return;
    const GlassPalette pal = GetPalette(settings);
    const std::uint8_t topA = settings.IsHighContrast() ? 235 : 190;
    const Gdiplus::Color cSolid(topA, pal.windowCanvasTop.r, pal.windowCanvasTop.g, pal.windowCanvasTop.b);
    const Gdiplus::Color cClear(0, pal.windowCanvasTop.r, pal.windowCanvasTop.g, pal.windowCanvasTop.b);
    Gdiplus::LinearGradientBrush br(
        Gdiplus::PointF(static_cast<float>(rc.left), static_cast<float>(rc.top)),
        Gdiplus::PointF(static_cast<float>(rc.left), static_cast<float>(rc.bottom)),
        fromTop ? cSolid : cClear,
        fromTop ? cClear : cSolid);
    g.FillRectangle(&br, static_cast<float>(rc.left), static_cast<float>(rc.top), w, h);
}

// Styled graphical CASIO keycap pill (replaces raw "[SHIFT]" text dumps with authentic tactile keycap)
inline void DrawCasioKeycapPillGdi(
    HDC hdc, const RECT& rc, const std::wstring& rawLabel, HFONT hFont, bool darkMode)
{
    const int iw = rc.right - rc.left;
    const int ih = rc.bottom - rc.top;
    if (iw < 10 || ih < 10) return;

    std::wstring lbl = rawLabel;
    // Strip surrounding raw brackets if present
    if (lbl.size() >= 2 && lbl.front() == L'[' && lbl.back() == L']') {
        lbl = lbl.substr(1, lbl.size() - 2);
    }

    // Semantic Casio hardware key coloring
    std::wstring up = lbl;
    for (wchar_t& ch : up) if (ch >= L'a' && ch <= L'z') ch = static_cast<wchar_t>(ch - L'a' + L'A');

    Gdiplus::Color topCol(255, 52, 58, 70);
    Gdiplus::Color botCol(255, 32, 37, 46);
    Gdiplus::Color rimCol(255, 86, 94, 112);
    COLORREF txtCol = RGB(248, 250, 252);

    if (up == L"SHIFT") {
        topCol = Gdiplus::Color(255, 246, 196, 68);
        botCol = Gdiplus::Color(255, 212, 152, 24);
        rimCol = Gdiplus::Color(255, 176, 122, 14);
        txtCol = RGB(26, 20, 8);
    } else if (up == L"ALPHA") {
        topCol = Gdiplus::Color(255, 232, 82, 128);
        botCol = Gdiplus::Color(255, 194, 46, 92);
        rimCol = Gdiplus::Color(255, 154, 32, 70);
        txtCol = RGB(255, 255, 255);
    } else if (up == L"AC" || up == L"DEL" || up == L"ON") {
        topCol = Gdiplus::Color(255, 238, 128, 42);
        botCol = Gdiplus::Color(255, 204, 92, 18);
        rimCol = Gdiplus::Color(255, 168, 72, 12);
        txtCol = RGB(255, 255, 255);
    } else if (up == L"EXE" || up == L"=" || up == L"OK") {
        topCol = Gdiplus::Color(255, 24, 134, 255);
        botCol = Gdiplus::Color(255, 0, 96, 216);
        rimCol = Gdiplus::Color(255, 0, 76, 182);
        txtCol = RGB(255, 255, 255);
    } else if (!darkMode && up.size() == 1 && (up[0] >= L'0' && up[0] <= L'9')) {
        topCol = Gdiplus::Color(255, 255, 255, 255);
        botCol = Gdiplus::Color(255, 235, 240, 248);
        rimCol = Gdiplus::Color(255, 176, 188, 206);
        txtCol = RGB(18, 24, 34);
    }

    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const float x = static_cast<float>(rc.left) + 0.5f;
    const float y = static_cast<float>(rc.top) + 0.5f;
    const float w = static_cast<float>(iw) - 1.0f;
    const float h = static_cast<float>(ih) - 1.0f;
    const float r = (std::min)(5.5f, h * 0.34f);

    // Subtle 1px drop shadow
    Gdiplus::GraphicsPath shPath;
    AddRoundPath(shPath, x, y + 1.2f, w, h, r);
    Gdiplus::SolidBrush shBr(Gdiplus::Color(darkMode ? 85 : 42, 0, 0, 0));
    g.FillPath(&shBr, &shPath);

    // Keycap body with subtle vertical tactile gradient
    Gdiplus::GraphicsPath bodyPath;
    AddRoundPath(bodyPath, x, y, w, h, r);
    Gdiplus::LinearGradientBrush grad(
        Gdiplus::PointF(x, y), Gdiplus::PointF(x, y + h), topCol, botCol);
    g.FillPath(&grad, &bodyPath);

    // Top specular edge highlight (Aqua 2.0 keycap sheen)
    Gdiplus::Pen hiPen(Gdiplus::Color(115, 255, 255, 255), 0.9f);
    g.DrawLine(&hiPen, x + r, y + 1.0f, x + w - r, y + 1.0f);

    Gdiplus::Pen rimPen(rimCol, 1.0f);
    g.DrawPath(&rimPen, &bodyPath);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, txtCol);
    HGDIOBJ oldF = hFont ? SelectObject(hdc, hFont) : nullptr;
    RECT tx = rc;
    tx.left += 3;
    tx.right -= 3;
    DrawTextW(hdc, lbl.c_str(), static_cast<int>(lbl.size()), &tx,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (oldF) SelectObject(hdc, oldF);
}

} // namespace UiGlass
