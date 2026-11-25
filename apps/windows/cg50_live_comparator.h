#pragma once
// ============================================================================
// cg50_live_comparator.h
// Ventana opcional de comparación en vivo:
//   Núcleo Oficial Casio (DLLs originales vía host 32-bit) vs.
//   Núcleo Propio Independiente (SH7305 FULL_SILICON v47, cero DLLs Casio).
//
// Diseño alineado con guías de interfaz + material translúcido y los 18 anti-patrones:
//   - Sin emojis, sin estética neón, sin tarjetas anidadas.
//   - Paleta sincronizada con CasioTheme (Porcelana Claro / Grafito Oscuro).
//   - PerMonitorV2 DPI scaling, 3 controles esenciales, métricas sobrias.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "cg50_backend.h"
#include "operation_capture.h"
#include "ui_theme.h"

inline HWND g_hCg50CompareWnd = nullptr;
inline bool g_cg50CompareShowDiff = true;
inline Cg50Backend::EngineMode g_cg50PrevSingleMode = Cg50Backend::EngineMode::NativeIndependent;
inline RECT g_cmpBtnResetRc{ 0, 0, 0, 0 };
inline RECT g_cmpBtnDiffRc{ 0, 0, 0, 0 };
inline RECT g_cmpBtnSaveRc{ 0, 0, 0, 0 };
inline int  g_cmpHoverBtn = -1;
inline std::wstring g_cmpLastSavedMsg;

static inline void DrawBgra32Lcd(
    HDC hdc,
    const std::uint32_t* pixels,
    int srcW,
    int srcH,
    const RECT& dst)
{
    if (!pixels || srcW <= 0 || srcH <= 0) return;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = srcW;
    bmi.bmiHeader.biHeight = -srcH; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(
        hdc,
        dst.left, dst.top, dst.right - dst.left, dst.bottom - dst.top,
        0, 0, srcW, srcH,
        pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

static inline void SaveLiveCompareSnapshot(Cg50Backend* cg50)
{
    if (!cg50) return;
    std::vector<std::uint32_t> diffBuf;
    const auto m = cg50->ComputeLiveComparison(&diffBuf);
    const auto offFb = cg50->OfficialFramebuffer();
    const auto natFb = cg50->NativeFramebuffer();

    const std::wstring outDir = ResolveWritableOutputDir(L"compare_captures");
    if (outDir.empty()) return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t stamp[64]{};
    swprintf_s(stamp, L"%04u%02u%02u_%02u%02u%02u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    const std::wstring pngPath = outDir + L"\\cg50_compare_" + stamp + L".png";
    const std::wstring jsonPath = outDir + L"\\cg50_compare_" + stamp + L".json";

    // Build crisp 3-panel composite bitmap (Official | Native | Diff)
    const int panelW = 384;
    const int panelH = 216;
    const int pad = 16;
    const int headerH = 34;
    const int footerH = 32;
    const int panels = (!diffBuf.empty()) ? 3 : 2;
    const int totalW = pad + panels * (panelW + pad);
    const int totalH = headerH + panelH + footerH + pad;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, totalW, totalH);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);

    HBRUSH bgBr = CreateSolidBrush(CasioTheme::StudioCanvasBg());
    RECT fullRc{ 0, 0, totalW, totalH };
    FillRect(hdcMem, &fullRc, bgBr);
    DeleteObject(bgBr);

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, CasioTheme::StudioText());
    HFONT hF = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldF = SelectObject(hdcMem, hF);

    const wchar_t* titles[3] = {
        L"Oficial (DLLs Casio)",
        L"N\u00FAcleo propio (SH7305 v47)",
        L"Mapa de diferencias"
    };
    const std::uint32_t* bufs[3] = {
        offFb.pixels,
        natFb.pixels,
        diffBuf.empty() ? nullptr : diffBuf.data()
    };

    for (int i = 0; i < panels; ++i) {
        const int x = pad + i * (panelW + pad);
        RECT tr{ x, 6, x + panelW, headerH };
        DrawTextW(hdcMem, titles[i], -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT pr{ x, headerH, x + panelW, headerH + panelH };
        if (bufs[i]) {
            DrawBgra32Lcd(hdcMem, bufs[i], panelW, panelH, pr);
        } else {
            HBRUSH emptyBr = CreateSolidBrush(CasioTheme::StudioSurface());
            FillRect(hdcMem, &pr, emptyBr);
            DeleteObject(emptyBr);
            DrawTextW(hdcMem, L"Sin se\u00F1al", -1, &pr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    wchar_t foot[256]{};
    swprintf_s(foot,
        L"Coincidencia: %.2f%%  |  P\u00EDxeles distintos: %u / %u  |  Delta m\u00E1x: %u  |  Oficial: %.2f ms  |  Propio: %.2f ms",
        m.matchPercent, m.diffPixels, m.totalPixels, m.maxChannelDelta, m.officialStepMs, m.nativeStepMs);
    RECT fr{ pad, headerH + panelH + 6, totalW - pad, totalH - 4 };
    DrawTextW(hdcMem, foot, -1, &fr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, oldF);
    DeleteObject(hF);
    GdiFlush();
    const bool pngOk = SaveBitmapToPngFile(pngPath.c_str(), hBmp);
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    // Save JSON telemetry alongside PNG
    std::ofstream jf(std::filesystem::path(jsonPath), std::ios::binary | std::ios::trunc);
    if (jf) {
        jf << "{\n"
           << "  \"official_ready\": " << (m.officialReady ? "true" : "false") << ",\n"
           << "  \"native_ready\": " << (m.nativeReady ? "true" : "false") << ",\n"
           << "  \"width\": " << m.width << ",\n"
           << "  \"height\": " << m.height << ",\n"
           << "  \"exact_match_pixels\": " << m.exactMatchPixels << ",\n"
           << "  \"diff_pixels\": " << m.diffPixels << ",\n"
           << "  \"match_percent\": " << std::fixed << std::setprecision(4) << m.matchPercent << ",\n"
           << "  \"max_channel_delta\": " << m.maxChannelDelta << ",\n"
           << "  \"mean_abs_error\": " << std::fixed << std::setprecision(4) << m.meanAbsError << ",\n"
           << "  \"official_step_ms\": " << std::fixed << std::setprecision(3) << m.officialStepMs << ",\n"
           << "  \"native_step_ms\": " << std::fixed << std::setprecision(3) << m.nativeStepMs << ",\n"
           << "  \"sync_steps\": " << m.syncSteps << ",\n"
           << "  \"sync_keys_dispatched\": " << m.syncKeysDispatched << "\n"
           << "}\n";
    }

    g_cmpLastSavedMsg = pngOk ? (L"Captura guardada en " + outDir) : L"No se pudo guardar la captura";
    if (g_hCg50CompareWnd && IsWindow(g_hCg50CompareWnd)) {
        InvalidateRect(g_hCg50CompareWnd, nullptr, FALSE);
    }
}

static inline void DrawCleanButton(
    HDC hdc,
    const RECT& rc,
    const wchar_t* label,
    bool active,
    bool hovered,
    HFONT hFont)
{
    const COLORREF bg = active
        ? CasioTheme::StudioAccentSoft()
        : (hovered ? CasioTheme::StudioSurfaceAlt() : CasioTheme::StudioSurface());
    const COLORREF border = active
        ? CasioTheme::StudioAccent()
        : (hovered ? CasioTheme::StudioBorderStrong() : CasioTheme::StudioBorder());
    const COLORREF fg = active ? CasioTheme::StudioAccentText() : CasioTheme::StudioText();

    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH hBr = CreateSolidBrush(bg);
    HGDIOBJ oldP = SelectObject(hdc, hPen);
    HGDIOBJ oldB = SelectObject(hdc, hBr);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(hdc, oldP);
    SelectObject(hdc, oldB);
    DeleteObject(hPen);
    DeleteObject(hBr);

    HGDIOBJ oldF = SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    RECT tr = rc;
    DrawTextW(hdc, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldF);
}

inline LRESULT CALLBACK Cg50LiveCompareWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* cg50 = reinterpret_cast<Cg50Backend*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);
        return 0;
    }
    case WM_DPICHANGED: {
        const RECT* prc = reinterpret_cast<const RECT*>(lParam);
        if (prc) {
            SetWindowPos(hWnd, nullptr, prc->left, prc->top,
                prc->right - prc->left, prc->bottom - prc->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int hit = -1;
        if (PtInRect(&g_cmpBtnResetRc, pt)) hit = 0;
        else if (PtInRect(&g_cmpBtnDiffRc, pt)) hit = 1;
        else if (PtInRect(&g_cmpBtnSaveRc, pt)) hit = 2;
        if (hit != g_cmpHoverBtn) {
            g_cmpHoverBtn = hit;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_cmpBtnResetRc, pt) && cg50) {
            cg50->Reset();
            g_cmpLastSavedMsg = L"Ambos motores reiniciados en sincron\u00EDa.";
            InvalidateRect(hWnd, nullptr, FALSE);
        } else if (PtInRect(&g_cmpBtnDiffRc, pt)) {
            g_cg50CompareShowDiff = !g_cg50CompareShowDiff;
            InvalidateRect(hWnd, nullptr, TRUE);
        } else if (PtInRect(&g_cmpBtnSaveRc, pt) && cg50) {
            SaveLiveCompareSnapshot(cg50);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdcPaint = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        const int W = (std::max)(1, static_cast<int>(rc.right - rc.left));
        const int H = (std::max)(1, static_cast<int>(rc.bottom - rc.top));

        HDC hdc = CreateCompatibleDC(hdcPaint);
        HBITMAP hMem = CreateCompatibleBitmap(hdcPaint, W, H);
        HGDIOBJ oldBmp = SelectObject(hdc, hMem);

        HBRUSH bgBr = CreateSolidBrush(CasioTheme::StudioCanvasBg());
        FillRect(hdc, &rc, bgBr);
        DeleteObject(bgBr);

        const float sc = CasioTheme::g_uiScale;
        auto S = [sc](int v) { return CasioTheme::UiToDevice(v, sc); };

        HFONT hTitleF = CreateFontW(-S(15), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT hBodyF = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT hSmallF = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        const int pad = S(16);
        const int topBarH = S(48);
        const int footerH = S(44);

        // Top bar title + 3 minimalist guías de interfaz actions
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CasioTheme::StudioText());
        HGDIOBJ oldF = SelectObject(hdc, hTitleF);
        RECT titleRc{ pad, S(10), W / 2, topBarH };
        DrawTextW(hdc, L"Comparador fx-CG50: Oficial vs. Propio", -1, &titleRc,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const int btnH = S(30);
        const int btnY = (topBarH - btnH) / 2;
        const int gap = S(8);
        const int wSave = S(128);
        const int wDiff = S(148);
        const int wReset = S(124);
        g_cmpBtnSaveRc  = { W - pad - wSave, btnY, W - pad, btnY + btnH };
        g_cmpBtnDiffRc  = { g_cmpBtnSaveRc.left - gap - wDiff, btnY, g_cmpBtnSaveRc.left - gap, btnY + btnH };
        g_cmpBtnResetRc = { g_cmpBtnDiffRc.left - gap - wReset, btnY, g_cmpBtnDiffRc.left - gap, btnY + btnH };

        DrawCleanButton(hdc, g_cmpBtnResetRc, L"Reiniciar ambos", false, g_cmpHoverBtn == 0, hBodyF);
        DrawCleanButton(hdc, g_cmpBtnDiffRc,  L"Mapa de diferencias", g_cg50CompareShowDiff, g_cmpHoverBtn == 1, hBodyF);
        DrawCleanButton(hdc, g_cmpBtnSaveRc,  L"Guardar captura", false, g_cmpHoverBtn == 2, hBodyF);

        // Hairline divider
        HPEN divPen = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
        HGDIOBJ oldP = SelectObject(hdc, divPen);
        MoveToEx(hdc, 0, topBarH, nullptr);
        LineTo(hdc, W, topBarH);
        MoveToEx(hdc, 0, H - footerH, nullptr);
        LineTo(hdc, W, H - footerH);
        SelectObject(hdc, oldP);
        DeleteObject(divPen);

        // Compute live comparison & fetch frames
        std::vector<std::uint32_t> diffBuf;
        Cg50Backend::LiveCompareMetrics m{};
        Cg50Backend::FramebufferView offFb{}, natFb{};
        if (cg50) {
            m = cg50->ComputeLiveComparison(g_cg50CompareShowDiff ? &diffBuf : nullptr);
            offFb = cg50->OfficialFramebuffer();
            natFb = cg50->NativeFramebuffer();
        }

        const int panels = g_cg50CompareShowDiff ? 3 : 2;
        const int stageTop = topBarH + pad;
        const int stageH = (std::max)(S(120), H - footerH - stageTop - pad);
        const int labelH = S(24);
        const int colGap = S(16);
        const int colW = (std::max)(S(180), (W - 2 * pad - (panels - 1) * colGap) / panels);
        const int availLcdH = (std::max)(S(80), stageH - labelH - S(8));

        // Aspect-locked 384:216 LCD dimensions
        int lcdW = colW;
        int lcdH = (lcdW * 216) / 384;
        if (lcdH > availLcdH) {
            lcdH = availLcdH;
            lcdW = (lcdH * 384) / 216;
        }

        const wchar_t* labels[3] = {
            L"Oficial (DLLs Casio)",
            L"N\u00FAcleo propio (SH7305 v47)",
            L"Mapa de diferencias"
        };
        const std::uint32_t* buffers[3] = {
            offFb.pixels,
            natFb.pixels,
            (g_cg50CompareShowDiff && !diffBuf.empty()) ? diffBuf.data() : nullptr
        };
        const std::wstring fallbacks[3] = {
            m.officialDiag.empty() ? L"Runtime oficial Casio no disponible" : m.officialDiag,
            m.nativeDiag.empty() ? L"N\u00FAcleo propio sin firmware" : m.nativeDiag,
            L"Requiere ambos motores activos"
        };

        for (int i = 0; i < panels; ++i) {
            const int cx = pad + i * (colW + colGap);
            const int xLcd = cx + (colW - lcdW) / 2;
            const int yLcd = stageTop + labelH + (availLcdH - lcdH) / 2;

            SelectObject(hdc, hBodyF);
            SetTextColor(hdc, CasioTheme::StudioTextSec());
            RECT lblRc{ xLcd, stageTop, xLcd + lcdW, stageTop + labelH };
            DrawTextW(hdc, labels[i], -1, &lblRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            RECT lcdRc{ xLcd, yLcd, xLcd + lcdW, yLcd + lcdH };
            if (buffers[i]) {
                DrawBgra32Lcd(hdc, buffers[i], 384, 216, lcdRc);
            } else {
                HBRUSH surfBr = CreateSolidBrush(CasioTheme::StudioSurface());
                FillRect(hdc, &lcdRc, surfBr);
                DeleteObject(surfBr);
                SelectObject(hdc, hSmallF);
                SetTextColor(hdc, CasioTheme::StudioTextMuted());
                RECT padRc = lcdRc;
                InflateRect(&padRc, -S(12), -S(12));
                DrawTextW(hdc, fallbacks[i].c_str(), -1, &padRc,
                    DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            }

            // Crisp hairline border around LCD
            HPEN bPen = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorderStrong());
            HGDIOBJ op = SelectObject(hdc, bPen);
            HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, lcdRc.left - 1, lcdRc.top - 1, lcdRc.right + 1, lcdRc.bottom + 1);
            SelectObject(hdc, op);
            SelectObject(hdc, ob);
            DeleteObject(bPen);
        }

        // Sober single-line metrics footer
        SelectObject(hdc, hSmallF);
        SetTextColor(hdc, CasioTheme::StudioTextSec());
        wchar_t statusLine[384]{};
        if (m.officialReady && m.nativeReady) {
            swprintf_s(statusLine,
                L"Coincidencia: %.2f%%   \u00B7   P\u00EDxeles distintos: %u / %u   \u00B7   \u0394m\u00E1x: %u   \u00B7   Oficial: %.2f ms   \u00B7   Propio: %.2f ms   \u00B7   Teclas: %llu",
                m.matchPercent, m.diffPixels, m.totalPixels, m.maxChannelDelta,
                m.officialStepMs, m.nativeStepMs,
                static_cast<unsigned long long>(m.syncKeysDispatched));
        } else {
            swprintf_s(statusLine,
                L"Estado: Oficial=%ls  \u00B7  Propio=%ls  \u00B7  Pulse teclas en la fx-CG50 principal para comparar en vivo.",
                m.officialReady ? L"Listo" : L"No disponible",
                m.nativeReady ? L"Listo" : L"No disponible");
        }
        std::wstring footerText(statusLine);
        if (!g_cmpLastSavedMsg.empty()) {
            footerText += L"   \u00B7   " + g_cmpLastSavedMsg;
        }
        RECT footRc{ pad, H - footerH, W - pad, H };
        DrawTextW(hdc, footerText.c_str(), -1, &footRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(hdc, oldF);
        DeleteObject(hTitleF);
        DeleteObject(hBodyF);
        DeleteObject(hSmallF);

        BitBlt(hdcPaint, 0, 0, W, H, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, oldBmp);
        DeleteObject(hMem);
        DeleteDC(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CLOSE: {
        if (cg50 && cg50->GetEngineMode() == Cg50Backend::EngineMode::CompareLive) {
            cg50->SetEngineMode(g_cg50PrevSingleMode);
        }
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_DESTROY:
        g_hCg50CompareWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

inline void OpenOrToggleCg50LiveComparator(HWND hOwner, HINSTANCE hInst, Cg50Backend* cg50)
{
    if (!cg50) return;

    if (g_hCg50CompareWnd && IsWindow(g_hCg50CompareWnd)) {
        SendMessageW(g_hCg50CompareWnd, WM_CLOSE, 0, 0);
        return;
    }

    if (cg50->GetEngineMode() != Cg50Backend::EngineMode::CompareLive) {
        g_cg50PrevSingleMode = cg50->GetEngineMode();
    }
    cg50->SetEngineMode(Cg50Backend::EngineMode::CompareLive);
    g_cmpLastSavedMsg.clear();

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = Cg50LiveCompareWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CasioCg50LiveCompareWnd";
        RegisterClassExW(&wc);
        s_registered = true;
    }

    const int w = CasioTheme::UiToDevice(1160);
    const int h = CasioTheme::UiToDevice(420);
    g_hCg50CompareWnd = CreateWindowExW(
        0,
        L"CasioCg50LiveCompareWnd",
        L"Comparador en vivo fx-CG50 (Oficial vs. Propio)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        hOwner, nullptr, hInst, cg50);
}

inline void RefreshCg50LiveComparator()
{
    if (g_hCg50CompareWnd && IsWindow(g_hCg50CompareWnd)) {
        InvalidateRect(g_hCg50CompareWnd, nullptr, FALSE);
    }
}
