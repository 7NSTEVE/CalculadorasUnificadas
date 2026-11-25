#pragma once
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <algorithm>
#include "fx991cw_host.h"

static HWND g_hPopupDisplayWnd = nullptr;
static bool g_popupShowGrid = false;
static int  g_captureZoomScale = 2; // 1 = x1, 2 = x2, 3 = x3
static std::wstring g_lastToolbarToast;
static ULONGLONG g_lastToolbarToastMs = 0;

// Implemented in settings_and_exporter.h (respects Solo Imagen / Solo Texto / Ambas)
static bool CopyBitmapAndTextByCurrentMode(HWND hwndOwner, int calcIdx, HBITMAP hBmp, bool updateStatus);

static void ShowToolbarToast(const std::wstring& msg)
{
    g_lastToolbarToast = msg;
    g_lastToolbarToastMs = GetTickCount64();
    SyncCompactToastOverlay(true);
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    if (g_hPopupDisplayWnd && IsWindow(g_hPopupDisplayWnd))
        InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
}

static RECT FitLcdAspectRect(const RECT& area, int calcIdx, int marginX, int marginY)
{
    int aw = (std::max)(16, (int)(area.right - area.left) - marginX * 2);
    int ah = (std::max)(16, (int)(area.bottom - area.top) - marginY * 2);
    int refW = (calcIdx == 3) ? 205 : ((calcIdx == 2) ? 384 : ((calcIdx == 1) ? 288 : 384));
    int refH = (calcIdx == 3) ?  87 : ((calcIdx == 2) ? 216 : ((calcIdx == 1) ? 111 : 150));

    int drawW = aw;
    int drawH = MulDiv(aw, refH, refW);
    if (drawH > ah) {
        drawH = ah;
        drawW = MulDiv(ah, refW, refH);
    }
    drawW = (std::max)(16, drawW);
    drawH = (std::max)(16, drawH);
    int left = area.left + ((area.right - area.left) - drawW) / 2;
    int top  = area.top  + ((area.bottom - area.top) - drawH) / 2;
    return RECT{ left, top, left + drawW, top + drawH };
}

static HBITMAP ScaleBitmapHalftone(HBITMAP hSrc, int srcW, int srcH, int dstW, int dstH, bool drawGridFrame)
{
    if (!hSrc || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcDst = CreateCompatibleDC(hdcScreen);
    HBITMAP hDst = CreateCompatibleBitmap(hdcScreen, dstW, dstH);
    HGDIOBJ oldSrc = SelectObject(hdcSrc, hSrc);
    HGDIOBJ oldDst = SelectObject(hdcDst, hDst);
    SetStretchBltMode(hdcDst, HALFTONE);
    SetBrushOrgEx(hdcDst, 0, 0, nullptr);
    StretchBlt(hdcDst, 0, 0, dstW, dstH, hdcSrc, 0, 0, srcW, srcH, SRCCOPY);
    if (drawGridFrame) {
        RECT rc{ 0, 0, dstW, dstH };
        HBRUSH brFrame = CreateSolidBrush(RGB(60, 70, 85));
        FrameRect(hdcDst, &rc, brFrame);
        DeleteObject(brFrame);
    }
    SelectObject(hdcSrc, oldSrc);
    SelectObject(hdcDst, oldDst);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ReleaseDC(nullptr, hdcScreen);
    return hDst;
}

// Copies the active calculator's LCD to the Windows Clipboard and saves a lossless PNG in ./capturas/
static bool CopyAndSaveActiveLcd(int calcIdx, int zoomScale)
{
    if (calcIdx < 0 || calcIdx > 3) calcIdx = 0;
    if (zoomScale < 1) zoomScale = 1;
    if (zoomScale > 3) zoomScale = 3;

    int outW = 0;
    int outH = 0;
    HBITMAP hBmp = nullptr;

    if (calcIdx == 3) {
        int rawW = 0, rawH = 0;
        HBITMAP hRaw = Fx991CwHost::CaptureBitmap(true, rawW, rawH);
        if (hRaw && rawW > 0 && rawH > 0) {
            outW = rawW * zoomScale;
            outH = rawH * zoomScale;
            hBmp = ScaleBitmapHalftone(hRaw, rawW, rawH, outW, outH, g_popupShowGrid);
            DeleteObject(hRaw);
        }
        if (!hBmp) {
            ShowToolbarToast(L"fx-991CW aun no esta listo para capturar.");
            return false;
        }
    } else {
        int baseW = (calcIdx == 2) ? 384 : ((calcIdx == 1) ? 288 : 192);
        int baseH = (calcIdx == 2) ? 216 : ((calcIdx == 1) ? 111 : 75);
        outW = baseW * zoomScale;
        outH = baseH * zoomScale;

        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        hBmp = CreateCompatibleBitmap(hdcScreen, outW, outH);
        HGDIOBJ hOld = SelectObject(hdcMem, hBmp);

        RECT rcLcd = { 0, 0, outW, outH };
        if (calcIdx == 2) {
            DrawCg50Screen(hdcMem, rcLcd);
        } else {
            DrawLcdDisplay(hdcMem, calcIdx, rcLcd);
        }

        if (g_popupShowGrid) {
            HBRUSH brFrame = CreateSolidBrush(RGB(60, 70, 85));
            FrameRect(hdcMem, &rcLcd, brFrame);
            DeleteObject(brFrame);
        }

        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    // 1. Save lossless PNG (portable dir if writable, else %LOCALAPPDATA%\CalculadoraUnificada\capturas)
    std::wstring capDir = GetCapturesDirectory();

    SYSTEMTIME st{};
    GetLocalTime(&st);
    const wchar_t* shortNames[4] = { L"fx570LAX", L"fx570VN", L"fxCG50", L"fx991CW" };
    wchar_t fileName[128]{};
    wsprintfW(fileName, L"LCD_%s_x%d_%04d%02d%02d_%02d%02d%02d_%03d.png",
              shortNames[calcIdx], zoomScale,
              st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    std::wstring fullPath = capDir + L"\\" + fileName;
    SaveBitmapByExtension(fullPath.c_str(), hBmp, outW, outH);

    // 2. Copy using configured clipboard mode (Solo Imagen / Solo Texto / Ambas)
    bool clipOk = CopyBitmapAndTextByCurrentMode(g_hMainWnd, calcIdx, hBmp, false);
    DeleteObject(hBmp);

    std::wstring msg = L"LCD copiado al portapapeles y guardado en PNG (x" + std::to_wstring(zoomScale) + L")";
    ShowToolbarToast(msg);
    return clipOk;
}

// Draw crisp vector camera icon inside `rc`
static void DrawCameraIconGdi(HDC hdc, const RECT& rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // Camera body
    RoundRect(hdc, cx - 8, cy - 4, cx + 8, cy + 6, 3, 3);
    // Top viewfinder bump
    MoveToEx(hdc, cx - 4, cy - 4, nullptr);
    LineTo(hdc, cx - 2, cy - 7);
    LineTo(hdc, cx + 2, cy - 7);
    LineTo(hdc, cx + 4, cy - 4);
    // Lens circle
    Ellipse(hdc, cx - 3, cy - 2, cx + 3, cy + 4);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

// Draw crisp vector grid/frame icon inside `rc`
static void DrawGridIconGdi(HDC hdc, const RECT& rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    Rectangle(hdc, cx - 7, cy - 5, cx + 7, cy + 5);
    MoveToEx(hdc, cx - 2, cy - 5, nullptr); LineTo(hdc, cx - 2, cy + 5);
    MoveToEx(hdc, cx + 2, cy - 5, nullptr); LineTo(hdc, cx + 2, cy + 5);
    MoveToEx(hdc, cx - 7, cy, nullptr);     LineTo(hdc, cx + 7, cy);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

// Draw crisp vector Pop-up Display icon inside `rc`
static void DrawPopupIconGdi(HDC hdc, const RECT& rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    Rectangle(hdc, cx - 7, cy - 5, cx + 7, cy + 5);
    // Diagonal arrow inside box pointing top-right
    MoveToEx(hdc, cx - 3, cy + 2, nullptr);
    LineTo(hdc, cx + 3, cy - 2);
    MoveToEx(hdc, cx, cy - 2, nullptr);
    LineTo(hdc, cx + 3, cy - 2);
    LineTo(hdc, cx + 3, cy + 1);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

// Draw crisp vector Separate/Compact View icon [|O|] inside `rc`
static void DrawCompactViewIconGdi(HDC hdc, const RECT& rc, COLORREF color)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // Center phone/calculator body
    RoundRect(hdc, cx - 4, cy - 6, cx + 4, cy + 6, 2, 2);
    // Side brackets
    MoveToEx(hdc, cx - 7, cy - 5, nullptr); LineTo(hdc, cx - 7, cy + 5);
    MoveToEx(hdc, cx + 7, cy - 5, nullptr); LineTo(hdc, cx + 7, cy + 5);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}

static LRESULT CALLBACK PopupDisplayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static constexpr int POPUP_BAR_H = 42;
    static const RECT kRcToolsCap  = { 8, 5, 156, 37 };
    static const RECT kRcModelsCap = { 164, 5, 434, 37 };
    static const RECT kRcCam       = { 12, 8, 40, 34 };
    static const RECT kRcGrid      = { 44, 8, 72, 34 };
    static const RECT kRcZoom      = { 76, 8, 152, 34 };
    static const RECT kRcModels[4] = {
        { 168, 8, 240, 34 },
        { 242, 8, 294, 34 },
        { 296, 8, 356, 34 },
        { 358, 8, 430, 34 }
    };

    switch (msg) {
    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE:
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hWnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = LOWORD(lParam);
        int my = HIWORD(lParam);

        if (PtInRect(&kRcCam, POINT{ mx, my })) {
            CopyAndSaveActiveLcd(g_activeCalcIdx, g_captureZoomScale);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&kRcGrid, POINT{ mx, my })) {
            g_popupShowGrid = !g_popupShowGrid;
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&kRcZoom, POINT{ mx, my })) {
            g_captureZoomScale = (g_captureZoomScale % 3) + 1;
            int baseW = (g_activeCalcIdx == 3) ? 209 : ((g_activeCalcIdx == 2) ? 384 : ((g_activeCalcIdx == 1) ? 288 : 192));
            int baseH = (g_activeCalcIdx == 3) ?  97 : ((g_activeCalcIdx == 2) ? 216 : ((g_activeCalcIdx == 1) ? 111 : 75));
            RECT rcWnd{};
            GetWindowRect(hWnd, &rcWnd);
            SetWindowPos(hWnd, nullptr, rcWnd.left, rcWnd.top,
                         max(448, baseW * g_captureZoomScale / 2 + 220),
                         max(210, baseH * g_captureZoomScale / 2 + 125),
                         SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        for (int i = 0; i < 4; ++i) {
            if (PtInRect(&kRcModels[i], POINT{ mx, my })) {
                g_activeCalcIdx = i;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }
        // Clic directo sobre un indicador LCD superior para explicarlo
        if (my >= POPUP_BAR_H) {
            RECT rcClient{}; GetClientRect(hWnd, &rcClient);
            RECT rcLcd = { 0, POPUP_BAR_H, rcClient.right, rcClient.bottom };
            int idx = (g_activeCalcIdx >= 0 && g_activeCalcIdx < 4) ? g_activeCalcIdx : 0;
            if (idx < 2 && g_nativeCalcs[idx].emu) {
                RECT fit = FitLcdAspectRect(rcLcd, idx, 8, 6);
                RECT indRect{}, dotRect{};
                ComputeLcdIndAndDotRects(idx, fit, indRect, dotRect);
                const size_t rawSz = calcemu_screen_buffer_size(g_nativeCalcs[idx].emu);
                std::vector<unsigned char> rawBuf(rawSz);
                bool gotRaw = (rawSz > 0 && calcemu_screen_buffer(g_nativeCalcs[idx].emu, rawBuf.data(), rawBuf.size()) == rawSz);
                int spriteIdx = -1; bool isLit = false;
                if (CasioKnowledge::HitTestLcdIndicator(idx, mx, my, indRect, gotRaw ? rawBuf.data() : nullptr, rawBuf.size(), spriteIdx, isLit)) {
                    OpenExplorerForIndicator(idx, spriteIdx, isLit, true);
                }
            }
        }
        return 0;
    }

    case WM_RBUTTONUP: {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        if (my < POPUP_BAR_H) return 0;
        RECT rcClient{}; GetClientRect(hWnd, &rcClient);
        RECT rcLcd = { 0, POPUP_BAR_H, rcClient.right, rcClient.bottom };
        int idx = (g_activeCalcIdx >= 0 && g_activeCalcIdx < 4) ? g_activeCalcIdx : 0;
        int spriteIdx = -1; bool isLit = false;
        if (idx < 2 && g_nativeCalcs[idx].emu) {
            RECT fit = FitLcdAspectRect(rcLcd, idx, 8, 6);
            RECT indRect{}, dotRect{};
            ComputeLcdIndAndDotRects(idx, fit, indRect, dotRect);
            const size_t rawSz = calcemu_screen_buffer_size(g_nativeCalcs[idx].emu);
            std::vector<unsigned char> rawBuf(rawSz);
            bool gotRaw = (rawSz > 0 && calcemu_screen_buffer(g_nativeCalcs[idx].emu, rawBuf.data(), rawBuf.size()) == rawSz);
            CasioKnowledge::HitTestLcdIndicator(idx, mx, my, indRect, gotRaw ? rawBuf.data() : nullptr, rawBuf.size(), spriteIdx, isLit);
        }
        ShowCalculatorContextMenu(hWnd, mx, my, idx, -1, spriteIdx, isLit, true, -1);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdcWin = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        if (rc.right <= 0 || rc.bottom <= 0) {
            EndPaint(hWnd, &ps);
            return 0;
        }

        HDC hdc = CreateCompatibleDC(hdcWin);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcWin, rc.right, rc.bottom);
        HGDIOBJ hOldBmp = SelectObject(hdc, hBmp);

        // guías de interfaz material translúcido 2.0 floating toolbar strip (Light / Dark / High Contrast)
        RECT rcBar = { 0, 0, rc.right, POPUP_BAR_H };
        CasioTheme::DrawTranslucentHeaderStrip(hdc, rcBar, true);
        CasioTheme::DrawFloatingGlassCapsule(hdc, kRcToolsCap, 1.0f, -1.0f, false);
        CasioTheme::DrawFloatingGlassCapsule(hdc, kRcModelsCap, 1.0f, -1.0f, false);

        // Camera capture button
        DrawCameraIconGdi(hdc, kRcCam, CasioTheme::StudioTextSec());

        // Grid / Frame toggle button
        if (g_popupShowGrid) {
            CasioTheme::FillRoundRectAA(hdc, kRcGrid, 12.0f, CasioTheme::StudioAccentSoft(), CasioTheme::StudioAccent(), 1.0f);
        }
        DrawGridIconGdi(hdc, kRcGrid, g_popupShowGrid ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextSec());

        // Zoom badge [x1 / x2 / x3]
        std::wstring zoomTxt = L"Zoom x" + std::to_wstring(g_captureZoomScale);
        CasioTheme::DrawStudioButton(hdc, kRcZoom, zoomTxt, g_fontSmall, CasioTheme::BTN_TOOLBAR_GHOST, false, false, 12.0f);

        // Model selector segmented pills inside floating material translúcido capsule
        const wchar_t* pillLabels[4] = { L"Blanca", L"Gris", L"CG50", L"991CW" };
        for (int i = 0; i < 4; ++i) {
            bool active = (g_activeCalcIdx == i);
            CasioTheme::DrawStudioSegmentItem(hdc, kRcModels[i], pillLabels[i], g_fontSmall, active, false, true);
        }

        // Main magnified LCD display area (aspect-preserving, never squished)
        RECT rcLcd = { 0, POPUP_BAR_H, rc.right, rc.bottom };
        int idx = (g_activeCalcIdx >= 0 && g_activeCalcIdx < 4) ? g_activeCalcIdx : 0;
        if (idx == 3) {
            HBRUSH brCwBg = CreateSolidBrush(RGB(213, 226, 213));
            FillRect(hdc, &rcLcd, brCwBg);
            DeleteObject(brCwBg);
            int cwW = 0, cwH = 0;
            HBITMAP hCwLcd = Fx991CwHost::CaptureBitmap(true, cwW, cwH);
            if (hCwLcd && cwW > 0 && cwH > 0) {
                RECT fitCw = FitLcdAspectRect(rcLcd, idx, 8, 6);
                HDC hdcSrc = CreateCompatibleDC(hdc);
                HGDIOBJ oldSrc = SelectObject(hdcSrc, hCwLcd);
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, nullptr);
                StretchBlt(hdc, fitCw.left, fitCw.top, fitCw.right - fitCw.left, fitCw.bottom - fitCw.top,
                           hdcSrc, 0, 0, cwW, cwH, SRCCOPY);
                SelectObject(hdcSrc, oldSrc);
                DeleteDC(hdcSrc);
                DeleteObject(hCwLcd);
            } else {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(40, 52, 68));
                HFONT oldF = g_fontNormal ? (HFONT)SelectObject(hdc, g_fontNormal) : nullptr;
                DrawTextW(hdc, L"fx-991CW (CasioEmuX) iniciando...", -1, &rcLcd,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (oldF) SelectObject(hdc, oldF);
            }
        } else if (idx == 2) {
            HBRUSH brCgBg = CreateSolidBrush(RGB(15, 20, 30));
            FillRect(hdc, &rcLcd, brCgBg);
            DeleteObject(brCgBg);
            RECT fitCg = FitLcdAspectRect(rcLcd, idx, 0, 0);
            DrawCg50Screen(hdc, fitCg);
        } else {
            HBRUSH brLcdBg = CreateSolidBrush(g_nativeCalcs[idx].lcdBgColor);
            FillRect(hdc, &rcLcd, brLcdBg);
            DeleteObject(brLcdBg);
            RECT fit = FitLcdAspectRect(rcLcd, idx, 8, 6);
            DrawLcdDisplay(hdc, idx, fit);
        }

        // Optional pixel grid / capture frame overlay
        if (g_popupShowGrid) {
            RECT rcFrame = rcLcd;
            InflateRect(&rcFrame, -2, -2);
            HBRUSH brF = CreateSolidBrush(CasioTheme::StudioAccent());
            FrameRect(hdc, &rcFrame, brF);
            DeleteObject(brF);
        }

        BitBlt(hdcWin, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        if (g_hMainWnd) EnumChildWindows(g_hMainWnd, [](HWND c, LPARAM) -> BOOL { InvalidateRect(c, nullptr, FALSE); return TRUE; }, 0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void EnsurePopupDisplayRegistered()
{
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PopupDisplayWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"CasioPopupDisplayWnd";
    RegisterClassExW(&wc);
    registered = true;
}

static void TogglePopupDisplayWindow()
{
    EnsurePopupDisplayRegistered();
    if (g_hPopupDisplayWnd && IsWindow(g_hPopupDisplayWnd)) {
        if (IsWindowVisible(g_hPopupDisplayWnd)) {
            ShowWindow(g_hPopupDisplayWnd, SW_HIDE);
        } else {
            ShowWindow(g_hPopupDisplayWnd, SW_SHOWNOACTIVATE);
            InvalidateRect(g_hPopupDisplayWnd, nullptr, TRUE);
        }
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        return;
    }

    RECT rcMain{};
    GetWindowRect(g_hMainWnd, &rcMain);
    int popW = 490;
    int popH = 265;
    int popX = rcMain.right + 12;
    int popY = rcMain.top + 8;
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    if (popX + popW > scrW) {
        popX = max(20, scrW - popW - 20);
    }

    g_hPopupDisplayWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"CasioPopupDisplayWnd",
        L"Pop-up Display",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE,
        popX, popY, popW, popH,
        g_hMainWnd, nullptr, g_hInst, nullptr);

    if (g_hPopupDisplayWnd) {
        CasioTheme::ApplyDwmDarkTitleBar(g_hPopupDisplayWnd, CasioTheme::g_studioDarkTheme);
        ShowWindow(g_hPopupDisplayWnd, SW_SHOWNOACTIVATE);
        UpdateWindow(g_hPopupDisplayWnd);
    }
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
}
