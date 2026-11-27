#pragma once
// ============================================================================
// studio/app/studio_tec_digital_views.hpp
// Native guías de interfaz + material translúcido 2.0 Software Vector Views for TEC Digital
// (0% GPU, 0% idle CPU, zero background timers, PerMonitorV2 4K,
// Porcelain Light / Graphite Dark / High Contrast synchronized).
// ============================================================================

#ifdef _WIN32

#include "../core/utf_conv.hpp"
#include "../integrations/tec_digital/tec_academic_store.hpp"
#include "../../ui_glass/hig_dialog_theme.hpp"
#include "../../ui_glass/translucent_tokens.hpp"

#include <gdiplus.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace DocStudio::TecDigitalUi::Views {

namespace td = academic::tec_digital;

// ----------------------------------------------------------------------------
// Shared guías de interfaz Micro-Components (Software Vector GDI+)
// ----------------------------------------------------------------------------

inline void FillSolidRectDc(HDC hdc, int w, int h, UiGlass::Rgba8 c) noexcept {
    if (!hdc || w <= 0 || h <= 0) return;
    RECT rc{ 0, 0, w, h };
    HBRUSH br = CreateSolidBrush(RGB(c.r, c.g, c.b));
    if (br) {
        FillRect(hdc, &rc, br);
        DeleteObject(br);
    }
}

inline void DrawSurface(Gdiplus::Graphics& g, float x, float y, float w, float h, float r,
                        UiGlass::Rgba8 fill, UiGlass::Rgba8 border, float borderW) {
    if (w <= 2.0f || h <= 2.0f) return;
    Gdiplus::GraphicsPath path;
    UiGlass::AddRoundPath(path, x, y, w, h, r);
    Gdiplus::SolidBrush br(UiGlass::ToGdip(fill));
    g.FillPath(&br, &path);
    if (borderW > 0.0f && border.a > 0) {
        Gdiplus::Pen pen(UiGlass::ToGdip(border), borderW);
        g.DrawPath(&pen, &path);
    }
}

inline UiGlass::Rgba8 SoftTint(UiGlass::Rgba8 accent, bool dark) noexcept {
    if (dark) {
        return UiGlass::Rgba8{
            static_cast<BYTE>(28 + (accent.r / 5)),
            static_cast<BYTE>(30 + (accent.g / 5)),
            static_cast<BYTE>(34 + (accent.b / 5)),
            255
        };
    }
    return UiGlass::Rgba8{
        static_cast<BYTE>(255 - ((255 - accent.r) / 7)),
        static_cast<BYTE>(255 - ((255 - accent.g) / 7)),
        static_cast<BYTE>(255 - ((255 - accent.b) / 7)),
        255
    };
}

inline UiGlass::Rgba8 DeptAccent(const std::string& dept, const std::string& type,
                                 const UiGlass::GlassPalette& pal) noexcept {
    if (type == "CATEDRA") return pal.accentIndigo;
    if (type == "ESCUELA") return pal.accentTeal;
    if (type == "PROGRAMA") return pal.accentPurple;
    if (type != "CLASE") return pal.accentOrange;
    std::string d;
    for (char c : dept) d.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    if (d == "E" || d == "EL" || d == "MI" || d == "EMT") return pal.accentBlue;
    if (d == "FI" || d == "QU" || d == "BI") return pal.accentIndigo;
    if (d == "MA" || d == "EM") return pal.accentPurple;
    if (d == "CA" || d == "IC" || d == "CE" || d == "CES") return pal.accentTeal;
    return pal.accentBlue;
}

inline std::wstring SpaceTypeBadgeLabel(const std::string& type) {
    if (type == "CLASE") return L"Curso";
    if (type == "CATEDRA") return L"Cátedra";
    if (type == "ESCUELA") return L"Escuela";
    if (type == "SUBGRUPO") return L"Subgrupo";
    if (type == "PROGRAMA") return L"Programa";
    return L"Comunidad";
}

inline float DrawPill(Gdiplus::Graphics& g, float x, float y, const std::wstring& text,
                      float dpiScale, UiGlass::Rgba8 fill, UiGlass::Rgba8 fg,
                      bool hc = false, float maxW = 240.0f) {
    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font f(&ff, 11.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::RectF meas{};
    g.MeasureString(text.c_str(), -1, &f, Gdiplus::PointF(0, 0), &meas);
    const float pw = std::min(maxW * dpiScale, meas.Width + 16.0f * dpiScale);
    const float ph = 21.0f * dpiScale;
    DrawSurface(g, x, y, pw, ph, ph * 0.5f, fill, hc ? fg : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.2f : 0.0f);
    Gdiplus::SolidBrush tbr(UiGlass::ToGdip(fg));
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    g.DrawString(text.c_str(), -1, &f, Gdiplus::RectF(x + 4.0f, y, pw - 8.0f, ph), &sf, &tbr);
    return pw;
}

inline void DrawProgressBar(Gdiplus::Graphics& g, float x, float y, float w, float h,
                            float ratio01, UiGlass::Rgba8 track, UiGlass::Rgba8 fill) {
    if (w <= 4.0f || h <= 2.0f) return;
    const float r = h * 0.5f;
    DrawSurface(g, x, y, w, h, r, track, { 0, 0, 0, 0 }, 0.0f);
    const float clamped = std::clamp(ratio01, 0.0f, 1.0f);
    const float fw = std::max(clamped > 0.005f ? h : 0.0f, w * clamped);
    if (fw > 1.0f) {
        DrawSurface(g, x, y, fw, h, r, fill, { 0, 0, 0, 0 }, 0.0f);
    }
}

struct SegmentedHit {
    RECT rc{};
    int index = 0;
};

inline void DrawSegmentedBar(Gdiplus::Graphics& g, float x, float y, float w, float h,
                             const std::vector<std::wstring>& labels, int selectedIdx, int hoverIdx,
                             float dpiScale, const UiGlass::GlassPalette& pal, bool hc,
                             std::vector<SegmentedHit>& outHits) {
    outHits.clear();
    if (labels.empty() || w <= 20.0f) return;
    const float radius = 8.0f * dpiScale;
    DrawSurface(g, x, y, w, h, radius, pal.sidebarBg,
                hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 0.9f);
    const float pad = 3.0f * dpiScale;
    const float segW = (w - pad * 2.0f) / static_cast<float>(labels.size());
    const float segH = h - pad * 2.0f;

    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font fReg(&ff, 12.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font fBold(&ff, 12.2f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    for (size_t i = 0; i < labels.size(); ++i) {
        const float sx = x + pad + static_cast<float>(i) * segW;
        const float sy = y + pad;
        const bool sel = (static_cast<int>(i) == selectedIdx);
        const bool hov = (static_cast<int>(i) == hoverIdx && !sel);
        if (sel) {
            DrawSurface(g, sx, sy, segW, segH, 6.0f * dpiScale, pal.contentCardElevated,
                        hc ? pal.accentBlue : pal.borderSubtle, hc ? 1.5f : 0.8f);
        } else if (hov) {
            DrawSurface(g, sx, sy, segW, segH, 6.0f * dpiScale, pal.sidebarHover, { 0, 0, 0, 0 }, 0.0f);
        }
        Gdiplus::SolidBrush tbr(UiGlass::ToGdip(sel ? pal.labelPrimary : pal.labelSecondary));
        g.DrawString(labels[i].c_str(), -1, sel ? &fBold : &fReg,
                     Gdiplus::RectF(sx + 4.0f, sy, std::max(8.0f, segW - 8.0f), segH), &sf, &tbr);
        SegmentedHit hit{};
        hit.rc = { static_cast<LONG>(sx), static_cast<LONG>(sy),
                   static_cast<LONG>(sx + segW), static_cast<LONG>(sy + segH) };
        hit.index = static_cast<int>(i);
        outHits.push_back(hit);
    }
}

inline void DrawKpiTile(Gdiplus::Graphics& g, float x, float y, float w, float h,
                        const std::wstring& title, const std::wstring& bigVal, const std::wstring& sub,
                        UiGlass::IconId icon, UiGlass::Rgba8 accent,
                        float dpiScale, const UiGlass::GlassPalette& pal, bool dark, bool hc) {
    DrawSurface(g, x, y, w, h, 10.0f * dpiScale, pal.contentCardElevated,
                hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 0.9f);

    // Subtle squircle icon chip at top-right
    const float icSz = 26.0f * dpiScale;
    const float icX = x + w - icSz - 12.0f * dpiScale;
    const float icY = y + 11.0f * dpiScale;
    DrawSurface(g, icX, icY, icSz, icSz, 7.0f * dpiScale, SoftTint(accent, dark),
                hc ? accent : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.2f : 0.0f);
    UiGlass::DrawVectorIconGdiPlus(g, icon, icX + icSz * 0.5f, icY + icSz * 0.5f,
                                   13.5f * dpiScale, UiGlass::ToGdip(accent), 1.55f * dpiScale);

    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font fLbl(&ff, 11.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font fVal(&ff, 21.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::Font fSub(&ff, 11.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
    Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
    Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
    Gdiplus::StringFormat sf;
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    const float padX = 13.0f * dpiScale;
    g.DrawString(title.c_str(), -1, &fLbl,
                 Gdiplus::RectF(x + padX, y + 11.0f * dpiScale, std::max(20.0f, w - icSz - 28.0f * dpiScale), 16.0f * dpiScale),
                 &sf, &brSec);
    g.DrawString(bigVal.c_str(), -1, &fVal,
                 Gdiplus::RectF(x + padX, y + 30.0f * dpiScale, std::max(20.0f, w - 26.0f * dpiScale), 28.0f * dpiScale),
                 &sf, &brPrim);
    if (!sub.empty()) {
        g.DrawString(sub.c_str(), -1, &fSub,
                     Gdiplus::RectF(x + padX, y + h - 22.0f * dpiScale, std::max(20.0f, w - 26.0f * dpiScale), 16.0f * dpiScale),
                     &sf, &brTert);
    }
}

inline void DrawScrollbarThumb(Gdiplus::Graphics& g, int w, int h, int scrollY, int maxScrollY,
                               float dpiScale, const UiGlass::GlassPalette& pal, float topY = 0.0f) {
    const float regionH = static_cast<float>(h) - topY;
    if (maxScrollY <= 0 || regionH <= 40.0f) return;
    const float totalH = regionH + static_cast<float>(maxScrollY);
    const float barH = std::max(28.0f * dpiScale, (regionH * regionH) / totalH);
    const float trackH = std::max(1.0f, regionH - barH - 12.0f * dpiScale);
    const float barY = topY + 6.0f * dpiScale + (static_cast<float>(scrollY) / static_cast<float>(maxScrollY)) * trackH;
    DrawSurface(g, static_cast<float>(w) - 7.5f * dpiScale, barY, 4.0f * dpiScale, barH, 2.0f * dpiScale,
                UiGlass::Rgba8{ pal.labelTertiary.r, pal.labelTertiary.g, pal.labelTertiary.b, 125 },
                { 0, 0, 0, 0 }, 0.0f);
}

inline std::wstring FileExtBadgeLabel(const std::string& filename, UiGlass::Rgba8& outColor,
                                      const UiGlass::GlassPalette& pal) {
    std::string ext;
    const size_t dot = filename.rfind('.');
    if (dot != std::string::npos && dot + 1 < filename.size()) {
        ext = filename.substr(dot + 1);
        for (char& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (ext == "PDF") { outColor = pal.accentRed; return L"PDF"; }
    if (ext == "DOC" || ext == "DOCX") { outColor = pal.accentBlue; return L"DOC"; }
    if (ext == "XLS" || ext == "XLSX" || ext == "CSV") { outColor = pal.accentGreen; return L"XLS"; }
    if (ext == "PPT" || ext == "PPTX") { outColor = pal.accentOrange; return L"PPT"; }
    if (ext == "ZIP" || ext == "RAR" || ext == "7Z") { outColor = pal.accentPurple; return L"ZIP"; }
    if (ext == "PNG" || ext == "JPG" || ext == "JPEG") { outColor = pal.accentTeal; return L"IMG"; }
    if (ext.size() >= 2 && ext.size() <= 4) { outColor = pal.accentIndigo; return Utf8ToWide(ext); }
    outColor = pal.labelSecondary;
    return L"DOC";
}

// ----------------------------------------------------------------------------
// GDI+ PNG Capture Helper for Visual Audit CLI (--tec-digital-preview)
// ----------------------------------------------------------------------------

inline bool GetPngEncoderClsid(CLSID* outClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;
    std::vector<BYTE> buf(size);
    auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    if (Gdiplus::GetImageEncoders(num, size, info) != Gdiplus::Ok) return false;
    for (UINT i = 0; i < num; ++i) {
        if (info[i].MimeType && std::wcscmp(info[i].MimeType, L"image/png") == 0) {
            *outClsid = info[i].Clsid;
            return true;
        }
    }
    return false;
}

inline bool SaveWindowScreenshotPngW(HWND hwnd, const wchar_t* outPngPathW) {
    if (!hwnd || !IsWindow(hwnd) || !outPngPathW || !outPngPathW[0]) return false;
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    const int w = std::max<int>(1, rc.right - rc.left);
    const int h = std::max<int>(1, rc.bottom - rc.top);

    HDC scrDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(scrDc);
    HBITMAP bmp = CreateCompatibleBitmap(scrDc, w, h);
    HGDIOBJ old = SelectObject(memDc, bmp);

    BOOL ok = PrintWindow(hwnd, memDc, PW_RENDERFULLCONTENT);
    if (!ok) {
        ok = PrintWindow(hwnd, memDc, 0);
    }

    bool saved = false;
    CLSID pngClsid{};
    if (ok && GetPngEncoderClsid(&pngClsid)) {
        Gdiplus::Bitmap gBmp(bmp, nullptr);
        saved = (gBmp.Save(outPngPathW, &pngClsid, nullptr) == Gdiplus::Ok);
    }

    SelectObject(memDc, old);
    DeleteObject(bmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, scrDc);
    return saved;
}

}  // namespace DocStudio::TecDigitalUi::Views

#endif  // _WIN32
