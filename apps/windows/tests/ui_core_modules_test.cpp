#include "operation_capture.h"
#include "lcd_indicators.h"
#include "latex_math_renderer.h"
#include "ui_theme.h"
#include "manual_catalog.h"
#include "pdf_viewer.h"
#include "knowledge_catalog.h"
#include "classwiz_keys_exact.h"
#include "gray_keys_exact.h"
#include "cw2_keys_exact.h"
#include "key_capture_rects.h"
#include "calcemu.h"
#include "calc_engine.h"
#include "../linux/engines/tests/u13_lcd_font_helper.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef CASIO_REPO_DIR
#define CASIO_REPO_DIR L"."
#endif

namespace {

int Fail(const char* what)
{
    std::cerr << "[FAIL] " << what << "\n";
    return 1;
}

std::filesystem::path ResolveTestRepoRoot()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    auto looksLikeRepo = [&](const fs::path& dir) -> bool {
        return !dir.empty() &&
               (fs::exists(dir / "assets" / "roms" / "fx991lax.rom", ec) ||
                fs::exists(dir / "roms" / "fx991lax.rom", ec)) &&
               (fs::exists(dir / "private_local" / "manuales" / "indice_paginas_pdf.tsv", ec) ||
                fs::exists(dir / "manuales" / "indice_paginas_pdf.tsv", ec));
    };

    wchar_t envBuf[32768]{};
    const DWORD envLen = GetEnvironmentVariableW(L"CASIO_UNIFIED_HOME", envBuf, 32768);
    if (envLen > 0 && envLen < 32768) {
        fs::path p(envBuf);
        if (looksLikeRepo(p)) return p;
    }

    wchar_t exeBuf[32768]{};
    const DWORD exeLen = GetModuleFileNameW(nullptr, exeBuf, 32768);
    if (exeLen > 0 && exeLen < 32768) {
        fs::path cur = fs::path(exeBuf).parent_path();
        for (int i = 0; i < 6 && !cur.empty(); ++i) {
            if (looksLikeRepo(cur)) return cur;
            fs::path parent = cur.parent_path();
            if (parent == cur) break;
            cur = parent;
        }
    }

    fs::path cwd = fs::current_path(ec);
    for (int i = 0; i < 4 && !cwd.empty(); ++i) {
        if (looksLikeRepo(cwd)) return cwd;
        fs::path parent = cwd.parent_path();
        if (parent == cwd) break;
        cwd = parent;
    }
    return fs::path(CASIO_REPO_DIR);
}

bool ContainsCaseInsensitive(const std::wstring& hay, const std::wstring& needle)
{
    if (needle.empty() || hay.size() < needle.size()) return false;
    std::wstring h = hay;
    std::wstring n = needle;
    for (wchar_t& ch : h) ch = static_cast<wchar_t>(towlower(ch));
    for (wchar_t& ch : n) ch = static_cast<wchar_t>(towlower(ch));
    return h.find(n) != std::wstring::npos;
}

int TestLcdIndicators()
{
    using namespace CasioLcdIndicators;

    if (GetIndicatorCount(0) != 20) return Fail("CW indicator count != 20");
    if (GetNativeStripWidth(0) != 192 || GetNativeStripHeight(0) != 8) return Fail("CW native strip dims");
    if (GetIndicatorCount(1) != 18) return Fail("ES indicator count != 18");
    if (GetNativeStripWidth(1) != 288 || GetNativeStripHeight(1) != 12) return Fail("ES native strip dims");

    for (int calcIdx = 0; calcIdx <= 1; ++calcIdx) {
        const int w = GetNativeStripWidth(calcIdx);
        const int h = GetNativeStripHeight(calcIdx);
        const int cnt = GetIndicatorCount(calcIdx);
        int prevEnd = 0;
        for (int i = 0; i < cnt; ++i) {
            const SpriteSpec* sp = GetIndicatorSpec(calcIdx, i);
            if (!sp) return Fail("Null SpriteSpec");
            if (sp->srcX < prevEnd || sp->srcX + sp->srcW > w) {
                return Fail("Indicator sprite overlap or out-of-bounds");
            }
            prevEnd = sp->srcX + sp->srcW;

            int setPixels = 0;
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < sp->srcW; ++x) {
                    if (GetIndicatorMaskPixel(calcIdx, i, x, y)) ++setPixels;
                }
            }
            if (setPixels == 0) return Fail("Empty glyph mask pixel count");
        }
    }

    auto verifyRender = [](int calcIdx, int scale, int activeByteOff, unsigned char activeMask, int sWidth) -> bool {
        const int dstW = GetNativeStripWidth(calcIdx) * scale;
        const int dstH = GetNativeStripHeight(calcIdx) * scale;

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = dstW;
        bmi.bmiHeader.biHeight = -dstH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HDC hdcScreen = GetDC(nullptr);
        HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);

        const COLORREF bg = RGB(210, 222, 198);
        const COLORREF fg = RGB(18, 28, 22);
        RECT rcFull{ 0, 0, dstW, dstH };
        HBRUSH brBg = CreateSolidBrush(bg);
        FillRect(hdcMem, &rcFull, brBg);
        DeleteObject(brBg);

        std::vector<unsigned char> raw(32, 0);
        raw[activeByteOff] = activeMask;
        bool ok = GlobalRenderer().Draw(hdcMem, nullptr, calcIdx, raw.data(), raw.size(), rcFull, bg, fg, 255);
        GdiFlush();

        const int stride = ((dstW * 3 + 3) / 4) * 4;
        std::vector<BYTE> readback(static_cast<size_t>(stride) * dstH);
        GetDIBits(hdcScreen, hBmp, 0, static_cast<UINT>(dstH), readback.data(), &bmi, DIB_RGB_COLORS);

        int fgInsideActive = 0;
        int fgOutsideActive = 0;
        const SpriteSpec* sp0 = GetIndicatorSpec(calcIdx, 0);
        const int actX0 = sp0->srcX * scale;
        const int actX1 = (sp0->srcX + sWidth) * scale;
        for (int y = 0; y < dstH; ++y) {
            const BYTE* row = readback.data() + static_cast<size_t>(y) * stride;
            for (int x = 0; x < dstW; ++x) {
                const BYTE b = row[x * 3 + 0];
                const BYTE g = row[x * 3 + 1];
                const BYTE r = row[x * 3 + 2];
                if (r < 50 && g < 50 && b < 50) {
                    if (x >= actX0 && x < actX1) ++fgInsideActive;
                    else ++fgOutsideActive;
                }
            }
        }

        SelectObject(hdcMem, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return ok && fgInsideActive > 0 && fgOutsideActive == 0;
    };

    if (!verifyRender(0, 2, 0x00, 0x01, 8)) return Fail("CW indicator pixel render check");
    if (!verifyRender(1, 1, 0x00, 0x10, 9)) return Fail("ES indicator pixel render check");

    std::cout << "[PASS] TestLcdIndicators\n";
    return 0;
}

int TestLatexSubsetEngine()
{
    const std::wstring validInputs[] = {
        L"Cuadr\u00E1tica: $x = \\frac{-b \\pm \\sqrt{b^{2} - 4ac}}{2a}$",
        L"Integral: $\\int_0^\\pi \\frac{\\sin(x)}{1+x^2}\\,dx$",
        L"Potencia anidada: $(\\frac{a}{b})^{2} = \\frac{a^{2}}{b^{2}}$",
        L"Serie: $\\sum_{k=1}^{n} k^{2} = \\frac{n(n+1)(2n+1)}{6}$",
        L"Derivada en punto: $\\left.\\frac{d}{dx}(x^{3})\\right|_{x=2} = 12$",
        L"Combinatoria y matriz: $\\binom{n}{k} + \\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}$",
        L"Acentos y plantilla: $\\tilde{x} + \\vec{v} + \\hat{\\theta} + \\frac{\\Box}{\\Box} + \\operatorname{nPr}(5,2)$",
        L"Griegas y operadores: $\\alpha + \\beta + \\gamma + \\phi + \\omega = \\Delta \\Sigma \\Omega$"
    };

    for (const auto& s : validInputs) {
        auto v = ValidateLatexRichText(s);
        if (!v.valid) return Fail("Valid LaTeX rejected");
    }

    const std::wstring invalidInputs[] = {
        L"Falta cierre $x+1",
        L"$\\frac{a}{2$",
        L"$x^{2}}$",
        L"$\\frac{a}$",
        L"$\\noexistecmd{x}$",
        L"$\\sqrt[3{x}$"
    };
    for (const auto& s : invalidInputs) {
        auto v = ValidateLatexRichText(s);
        if (v.valid) return Fail("Invalid LaTeX erroneously accepted");
    }

    const int W = 720;
    const int H = 260;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);

    RECT rcCard{ 0, 0, W, H };
    HBRUSH brWhite = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdcMem, &rcCard, brWhite);
    DeleteObject(brWhite);

    const std::wstring renderExpr =
        L"F\u00F3rmula: $(\\frac{a}{b})^{2}$  |  $x = \\frac{-b \\pm \\sqrt{b^{2} - 4ac}}{2a}$  |  $\\int_0^\\pi \\frac{\\sin(x)}{1+x^2}\\,dx$";
    const int measuredH = MeasureLatexRichText(hdcMem, renderExpr, W - 24, 16, false);
    RECT rcDraw{ 12, 12, W - 12, H - 12 };
    const int drawnH = DrawLatexRichText(hdcMem, renderExpr, rcDraw, RGB(20, 24, 32), RGB(10, 80, 175), 16, false);

    // Prueba de promocion matematica automatica, matrices, combinatoria y ajuste en 1 linea
    const std::wstring autoAndAdvExpr =
        L"Arcocoseno cos^-1(0.5) = 60\u00B0  |  Potencia x^2 + 10^x + x10^x  |  "
        L"$\\left.\\frac{d}{dx}(x^{3})\\right|_{x=2} = 12$  |  $\\binom{5}{2} = \\begin{pmatrix} 1 & 2 \\\\ 3 & 4 \\end{pmatrix}$  |  $\\frac{\\Box}{\\Box}$";
    const int advMeasuredH = MeasureLatexRichText(hdcMem, autoAndAdvExpr, W - 24, 15, false);
    RECT rcAdv{ 12, 120, W - 12, H - 36 };
    const int advDrawnH = DrawLatexRichText(hdcMem, autoAndAdvExpr, rcAdv, RGB(20, 24, 32), RGB(10, 80, 175), 15, false);
    RECT rcSingle{ 12, H - 34, 240, H - 6 };
    const int singleDrawnH = DrawLatexRichTextCentered(hdcMem, L"x^2 + \\frac{1}{2} + \\sqrt{x}", rcSingle, RGB(20, 24, 32), RGB(10, 80, 175), 14);
    GdiFlush();

    const int stride = ((W * 3 + 3) / 4) * 4;
    std::vector<BYTE> px(static_cast<size_t>(stride) * H);
    GetDIBits(hdcScreen, hBmp, 0, static_cast<UINT>(H), px.data(), &bmi, DIB_RGB_COLORS);

    int inkCount = 0;
    for (int y = 0; y < H; ++y) {
        const BYTE* row = px.data() + static_cast<size_t>(y) * stride;
        for (int x = 0; x < W; ++x) {
            if (row[x * 3 + 0] < 240 || row[x * 3 + 1] < 240 || row[x * 3 + 2] < 240) {
                ++inkCount;
            }
        }
    }

    SelectObject(hdcMem, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    if (measuredH <= 20 || drawnH <= 20 || measuredH != drawnH || measuredH > H) {
        return Fail("Latex measure/draw height mismatch or overflow");
    }
    if (advMeasuredH <= 20 || advDrawnH <= 20 || advMeasuredH != advDrawnH) {
        return Fail("Latex advanced/auto-math measure/draw height mismatch");
    }
    if (singleDrawnH <= 0 || singleDrawnH > 32) {
        return Fail("Latex single-line centered fit exceeded container height");
    }
    if (inkCount < 500) return Fail("Latex rendered insufficient ink pixels");

    std::cout << "[PASS] TestLatexSubsetEngine (height=" << drawnH << ", ink=" << inkCount << ")\n";
    return 0;
}

int TestOperationCaptureAndWritablePaths()
{
    namespace fs = std::filesystem;
    const std::wstring fallbackIni = ResolveWritableConfigIniPath(L"Q:\\__nonexistent_cu_ro_dir__");
    if (fallbackIni.empty() || !EndsWithCaseInsensitive(fallbackIni, L"config_unificada.ini")) {
        return Fail("Writable INI fallback path failed");
    }

    std::error_code ec;
    const fs::path tmpDir = fs::temp_directory_path(ec) / ("cu_ui_core_test_" + std::to_string(GetCurrentProcessId()));
    fs::create_directories(tmpDir, ec);
    if (ec) return Fail("Cannot create temp test dir");

    const std::wstring outSub = ResolveWritableOutputDir(L"guias_test", tmpDir.wstring());
    if (!IsDirectoryWritable(outSub)) return Fail("ResolveWritableOutputDir failed");

    const int W = 64, H = 48;
    const int stride = ((W * 3 + 3) / 4) * 4;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);
    if (!hBmp || !bits) return Fail("CreateDIBSection failed");

    BYTE* p = static_cast<BYTE*>(bits);
    for (int y = 0; y < H; ++y) {
        BYTE* row = p + static_cast<size_t>(y) * stride;
        for (int x = 0; x < W; ++x) {
            const bool dark = ((x + y) & 1) != 0;
            row[x * 3 + 0] = dark ? 28 : 215; // B
            row[x * 3 + 1] = dark ? 52 : 232; // G
            row[x * 3 + 2] = dark ? 19 : 204; // R
        }
    }

    const fs::path pngPath = fs::path(outSub) / "roundtrip_test.png";
    const fs::path bmpPath = fs::path(outSub) / "roundtrip_test.bmp";
    if (!SaveBitmapByExtension(pngPath.c_str(), hBmp, W, H)) {
        DeleteObject(hBmp);
        return Fail("SaveBitmapByExtension(.png) failed");
    }
    if (!SaveBitmapByExtension(bmpPath.c_str(), hBmp, W, H)) {
        DeleteObject(hBmp);
        return Fail("SaveBitmapByExtension(.bmp) failed");
    }
    DeleteObject(hBmp);

    bool pngOk = false;
    if (EnsureGdiplusInitialized()) {
        Gdiplus::Bitmap reloaded(pngPath.c_str());
        if (reloaded.GetLastStatus() == Gdiplus::Ok &&
            reloaded.GetWidth() == static_cast<UINT>(W) &&
            reloaded.GetHeight() == static_cast<UINT>(H)) {
            pngOk = true;
            for (int y = 0; y < H && pngOk; ++y) {
                for (int x = 0; x < W; ++x) {
                    Gdiplus::Color c;
                    reloaded.GetPixel(x, y, &c);
                    const bool dark = ((x + y) & 1) != 0;
                    const BYTE expR = dark ? 19 : 204;
                    const BYTE expG = dark ? 52 : 232;
                    const BYTE expB = dark ? 28 : 215;
                    if (c.GetR() != expR || c.GetG() != expG || c.GetB() != expB) {
                        pngOk = false;
                        break;
                    }
                }
            }
        }
    }

    fs::remove_all(tmpDir, ec);
    if (!pngOk) return Fail("Lossless PNG pixel roundtrip verification failed");

    std::cout << "[PASS] TestOperationCaptureAndWritablePaths\n";
    return 0;
}

int TestWin32ClipboardMultiFormat()
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"CuClipTestMsgWnd";
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(0, L"CuClipTestMsgWnd", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!hWnd) return Fail("CreateWindowExW HWND_MESSAGE failed");

    auto makeTestBitmap = []() -> HBITMAP {
        HDC hScr = GetDC(nullptr);
        HBITMAP h = CreateCompatibleBitmap(hScr, 24, 16);
        ReleaseDC(nullptr, hScr);
        return h;
    };

    // Attempt clipboard open cleanly (skip only if OS denies clipboard access in locked session)
    if (!OpenClipboard(hWnd)) {
        DestroyWindow(hWnd);
        std::cout << "[WARN] TestWin32ClipboardMultiFormat skipped (clipboard locked by another process)\n";
        return 0;
    }

    // 1. Both formats (CF_BITMAP + CF_UNICODETEXT) simultaneously
    EmptyClipboard();
    HBITMAP hBmp = makeTestBitmap();
    const wchar_t* sampleText = L"$x = \\frac{1}{2}$";
    const size_t cb = (wcslen(sampleText) + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
    void* pMem = GlobalLock(hMem);
    memcpy(pMem, sampleText, cb);
    GlobalUnlock(hMem);

    SetClipboardData(CF_BITMAP, hBmp);
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();

    const bool hasBmpBoth = IsClipboardFormatAvailable(CF_BITMAP) != FALSE;
    const bool hasTxtBoth = IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
    if (!hasBmpBoth || !hasTxtBoth) {
        DestroyWindow(hWnd);
        return Fail("Clipboard Both mode missing CF_BITMAP or CF_UNICODETEXT");
    }

    // 2. Solo Imagen (CF_BITMAP only)
    if (OpenClipboard(hWnd)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, makeTestBitmap());
        CloseClipboard();
    }
    if (!IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        DestroyWindow(hWnd);
        return Fail("Clipboard Image-only mode polluted with text");
    }

    // 3. Clean up clipboard
    if (OpenClipboard(hWnd)) {
        EmptyClipboard();
        CloseClipboard();
    }
    DestroyWindow(hWnd);
    std::cout << "[PASS] TestWin32ClipboardMultiFormat\n";
    return 0;
}

int TestCalcEmuRomModesAndSaveRestore()
{
    namespace fs = std::filesystem;
    const fs::path repo = ResolveTestRepoRoot();
    const fs::path cwRomPath = fs::exists(repo / "assets" / "roms" / "fx991lax.rom")
        ? (repo / "assets" / "roms" / "fx991lax.rom") : (repo / "roms" / "fx991lax.rom");
    const fs::path esRomPath = fs::exists(repo / "assets" / "roms" / "fx570vnplus.rom")
        ? (repo / "assets" / "roms" / "fx570vnplus.rom") : (repo / "roms" / "fx570vnplus.rom");
    if (!fs::exists(cwRomPath) || !fs::exists(esRomPath)) {
        return Fail("Required ROM files not found under CASIO_REPO_DIR/roms");
    }

    auto readRom = [](const fs::path& p) -> std::vector<unsigned char> {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        if (!f) return {};
        const auto sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<unsigned char> data(sz);
        if (!f.read(reinterpret_cast<char*>(data.data()), sz)) return {};
        return data;
    };

    // 1) ClassWiz fx-991LAX
    {
        auto cwRom = readRom(cwRomPath);
        if (cwRom.empty()) return Fail("Failed reading fx991lax.rom");
        CalcEmuModel cwModel{ CALCEMU_HW_CLASSWIZ, 0, 0x0003, 0x00 };
        CalcEmu* emu = calcemu_create(&cwModel, cwRom.data(), cwRom.size());
        if (!emu) return Fail("calcemu_create CW failed");

        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);
        if (calcemu_screen_mode(emu) != 5) {
            calcemu_destroy(emu);
            return Fail("CW initial screen_mode != 5");
        }

        // Verify SHIFT turns on S bit (0x00, mask 0x01)
        calcemu_tap_key(emu, 0x07, 400, 300);
        std::vector<unsigned char> raw(calcemu_screen_buffer_size(emu));
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if ((raw[0x00] & 0x01) == 0) {
            calcemu_destroy(emu);
            return Fail("CW SHIFT did not activate S indicator bit");
        }

        // Verify ALPHA turns on A bit (0x01, mask 0x01)
        calcemu_tap_key(emu, 0x17, 400, 300);
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if ((raw[0x01] & 0x01) == 0) {
            calcemu_destroy(emu);
            return Fail("CW ALPHA did not activate A indicator bit");
        }

        // Clear with AC and capture coherent state snapshot
        calcemu_tap_key(emu, 0x42, 400, 300);
        const size_t stSize = calcemu_save_state(emu, nullptr, 0);
        if (stSize == 0) {
            calcemu_destroy(emu);
            return Fail("CW calcemu_save_state size == 0");
        }
        std::vector<unsigned char> snapshot(stSize);
        if (calcemu_save_state(emu, snapshot.data(), snapshot.size()) != stSize) {
            calcemu_destroy(emu);
            return Fail("CW calcemu_save_state write mismatch");
        }
        std::vector<unsigned char> prePixels(calcemu_screen_width(emu) * calcemu_screen_rows(emu));
        calcemu_screen_pixels(emu, prePixels.data(), prePixels.size());

        // Mutate calculator state with 7 + 2 =
        calcemu_tap_key(emu, 0x02, 400, 300); // 7
        calcemu_tap_key(emu, 0x30, 400, 300); // +
        calcemu_tap_key(emu, 0x10, 400, 300); // 2
        calcemu_tap_key(emu, 0x60, 400, 300); // =
        std::vector<unsigned char> postPixels(prePixels.size());
        calcemu_screen_pixels(emu, postPixels.data(), postPixels.size());
        if (postPixels == prePixels) {
            calcemu_destroy(emu);
            return Fail("CW screen pixels did not change after 7+2=");
        }

        // Restore saved state and confirm byte-for-byte LCD reversion
        if (calcemu_load_state(emu, snapshot.data(), snapshot.size()) != 1) {
            calcemu_destroy(emu);
            return Fail("CW calcemu_load_state failed");
        }
        std::vector<unsigned char> restoredPixels(prePixels.size());
        calcemu_screen_pixels(emu, restoredPixels.data(), restoredPixels.size());
        if (restoredPixels != prePixels) {
            calcemu_destroy(emu);
            return Fail("CW calcemu_load_state did not restore exact LCD framebuffer");
        }

        // Mode change: MENU (0x47) -> 2 (0x10) = Complex -> verify 'i' indicator (0x0C, mask 0x01)
        calcemu_tap_key(emu, 0x47, 400, 300); // MENU
        calcemu_tap_key(emu, 0x10, 400, 300); // 2 (Complex)
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if ((raw[0x0C] & 0x01) == 0) {
            calcemu_destroy(emu);
            return Fail("CW MENU->2 did not activate Complex 'i' indicator bit");
        }
        calcemu_tap_key(emu, 0x47, 400, 300); // MENU
        calcemu_tap_key(emu, 0x00, 400, 300); // 1 (Calculate)

        // Verify SHIFT -> AC sequence in Casio PC Emulator ROM (real_hardware=0):
        // Clears SHIFT modifier and expression input, keeps Math/D indicators active (screen_mode==5) and accepts subsequent input.
        calcemu_tap_key(emu, 0x07, 400, 300); // SHIFT
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if ((raw[0x00] & 0x01) == 0) {
            calcemu_destroy(emu);
            return Fail("CW SHIFT before AC did not arm S bit");
        }
        calcemu_tap_key(emu, 0x42, 400, 300); // AC (OFF key on emulator ROM)
        calcemu_advance(emu, 1000);
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        std::vector<unsigned char> postOffPixels(prePixels.size());
        calcemu_screen_pixels(emu, postOffPixels.data(), postOffPixels.size());
        int offInk = 0;
        for (unsigned char v : postOffPixels) if (v) ++offInk;
        if (calcemu_screen_mode(emu) != 5 || (raw[0x00] & 0x01) != 0) {
            calcemu_destroy(emu);
            return Fail("CW SHIFT+AC unexpected mode or S modifier left armed");
        }
        // Check whether CW cursor blinks (blank screen except <=64 cursor pixels)
        if (offInk > 80) {
            std::cerr << "[DIAG] CW offInk=" << offInk << "\n";
            calcemu_destroy(emu);
            return Fail("CW SHIFT+AC left non-cursor expression ink on LCD");
        }
        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if (calcemu_screen_mode(emu) != 5 || (raw[0x06] & 0x01) == 0) {
            calcemu_destroy(emu);
            return Fail("CW ON did not recover active LCD with Degree indicator");
        }
        calcemu_destroy(emu);
    }

    // 2) ES PLUS fx-570VN PLUS
    {
        auto esRom = readRom(esRomPath);
        if (esRom.empty()) return Fail("Failed reading fx570vnplus.rom");
        CalcEmuModel esModel{ CALCEMU_HW_ES_PLUS, 0, 0x0001, 0x08 };
        CalcEmu* emu = calcemu_create(&esModel, esRom.data(), esRom.size());
        if (!emu) return Fail("calcemu_create ES failed");

        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);
        if (calcemu_screen_mode(emu) != 5) {
            calcemu_destroy(emu);
            return Fail("ES initial screen_mode != 5");
        }

        calcemu_tap_key(emu, 0x07, 400, 300); // SHIFT -> S bit (0x00, mask 0x10)
        std::vector<unsigned char> raw(calcemu_screen_buffer_size(emu));
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if ((raw[0x00] & 0x10) == 0) {
            calcemu_destroy(emu);
            return Fail("ES SHIFT did not activate S indicator bit");
        }

        calcemu_tap_key(emu, 0x17, 400, 300); // ALPHA -> A bit (0x00, mask 0x04)
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if ((raw[0x00] & 0x04) == 0) {
            calcemu_destroy(emu);
            return Fail("ES ALPHA did not activate A indicator bit");
        }

        calcemu_tap_key(emu, 0x47, 400, 300); // MODE
        calcemu_tap_key(emu, 0x10, 400, 300); // 2 (CMPLX -> 0x04, mask 0x80)
        calcemu_screen_buffer(emu, raw.data(), raw.size());
        if ((raw[0x04] & 0x80) == 0) {
            calcemu_destroy(emu);
            return Fail("ES MODE->2 did not activate CMPLX indicator bit");
        }

        // Verify SHIFT -> AC + ON on ES PLUS emulator ROM
        calcemu_tap_key(emu, 0x07, 400, 300); // SHIFT
        calcemu_tap_key(emu, 0x42, 400, 300); // AC
        calcemu_advance(emu, 1000);
        const int esAfterOffMode = calcemu_screen_mode(emu);
        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);
        if (calcemu_screen_mode(emu) != 5 || (esAfterOffMode != 0 && esAfterOffMode != 4 && esAfterOffMode != 5)) {
            calcemu_destroy(emu);
            return Fail("ES SHIFT+AC / ON recovery unexpected state");
        }
        calcemu_destroy(emu);
    }

    std::cout << "[PASS] TestCalcEmuRomModesAndSaveRestore\n";
    return 0;
}

int TestKnowledgeCatalogCompletenessAndJson()
{
    namespace fs = std::filesystem;
    const auto& cat = CasioKnowledge::GetCatalog();
    if (cat.keys.size() != 150) return Fail("Catalog key count != 150");
    if (cat.indicators.size() != 38) return Fail("Catalog indicator count != 38");
    if (cat.procedures.size() != 14) return Fail("Catalog procedure count != 14");
    if (cat.diagnostics.size() != 10) return Fail("Catalog diagnostics count != 10");
    if (cat.errors.size() != 6) return Fail("Catalog errors count != 6");
    if (cat.tips.size() != 16) return Fail("Catalog tips count != 16");
    if (cat.setupProfiles.size() != 5) return Fail("Catalog setupProfiles count != 5");
    if (cat.checklist.size() != 8) return Fail("Catalog checklist count != 8");
    if (cat.comparisons.size() != 10) return Fail("Catalog comparisons count != 10");

    for (int m = 0; m < 3; ++m) {
        for (int k = 0; k < 50; ++k) {
            const auto* rec = CasioKnowledge::FindKeyRecord(m, k);
            if (!rec) return Fail("Missing key record in catalog");
            if (rec->keyName.empty() || rec->pdfFile.empty() || rec->pdfPage < 1) {
                return Fail("Invalid metadata in catalog key record");
            }
            if (!rec->normal.available || rec->normal.label.empty() || rec->normal.summary.empty()) {
                return Fail("Missing normal layer in catalog key record");
            }
            const CasioKnowledge::KeyLayerInfo* layers[3] = { &rec->normal, &rec->shift, &rec->alpha };
            for (const auto* lyr : layers) {
                if (!lyr->available) continue;
                if (!lyr->latexFormula.empty()) {
                    auto vf = ValidateLatexRichText(lyr->latexFormula);
                    if (!vf.valid) {
                        std::wcerr << L"[FAIL] Invalid latexFormula for calc=" << m << L" key=" << k << L": " << lyr->latexFormula << L"\n";
                        return 1;
                    }
                }
                if (!lyr->latexExample.empty()) {
                    auto ve = ValidateLatexRichText(lyr->latexExample);
                    if (!ve.valid) {
                        std::wcerr << L"[FAIL] Invalid latexExample for calc=" << m << L" key=" << k << L": " << lyr->latexExample << L"\n";
                        return 1;
                    }
                }
            }
        }
    }

    for (int i = 0; i < 20; ++i) {
        const auto* doc = CasioKnowledge::FindIndicatorDoc(0, i);
        if (!doc || doc->title.empty() || doc->meaning.empty()) return Fail("Missing CW indicator doc");
    }
    for (int i = 0; i < 18; ++i) {
        const auto* doc = CasioKnowledge::FindIndicatorDoc(1, i);
        if (!doc || doc->title.empty() || doc->meaning.empty()) return Fail("Missing ES indicator doc");
    }

    // HitTestLcdIndicator check (S sprite 0 at x=0..8 on CW width 192 -> dst x=4 in 384 strip)
    RECT cwIndRect{ 0, 0, 384, 16 };
    std::vector<unsigned char> mockRaw(32, 0);
    mockRaw[0x00] = 0x01; // S lit on CW
    int hitSp = -1;
    bool hitLit = false;
    if (!CasioKnowledge::HitTestLcdIndicator(0, 4, 8, cwIndRect, mockRaw.data(), mockRaw.size(), hitSp, hitLit) ||
        hitSp != 0 || !hitLit) {
        return Fail("HitTestLcdIndicator failed to detect active CW S indicator");
    }
    mockRaw[0x00] = 0x00;
    if (!CasioKnowledge::HitTestLcdIndicator(0, 4, 8, cwIndRect, mockRaw.data(), mockRaw.size(), hitSp, hitLit) ||
        hitSp != 0 || hitLit) {
        return Fail("HitTestLcdIndicator failed to report unlit CW S indicator");
    }

    // Search synonym expansion & scoring check ("decimales" -> matches FIX/SD)
    const auto syns = CasioKnowledge::ExpandQueryWithSynonyms(L"decimales");
    bool hasFixSyn = false;
    for (const auto& s : syns) {
        if (s == L"fix") { hasFixSyn = true; break; }
    }
    if (!hasFixSyn) return Fail("ExpandQueryWithSynonyms failed for 'decimales'");
    if (CasioKnowledge::ScoreKnowledgeMatch(L"Formato FIX redondear cifras", L"decimales") <= 0) {
        return Fail("ScoreKnowledgeMatch failed synonym hit for 'decimales'");
    }

    for (const auto& proc : cat.procedures) {
        if (!proc.latexFormula.empty() && !ValidateLatexRichText(proc.latexFormula).valid) {
            std::wcerr << L"[FAIL] Invalid LaTeX formula in procedure: " << proc.id << L"\n";
            return 1;
        }
        if (!proc.goalSummary.empty() && !ValidateLatexRichText(proc.goalSummary).valid) {
            std::wcerr << L"[FAIL] Invalid LaTeX goalSummary in procedure: " << proc.id << L"\n";
            return 1;
        }
        bool anySupported = false;
        for (int m = 0; m < 3; ++m) {
            if (proc.perModel[m].supported) anySupported = true;
            if (proc.perModel[m].supported && m < 2 && proc.perModel[m].keys.empty()) {
                return Fail("Supported CW/ES procedure has empty key sequence");
            }
        }
        if (!anySupported) return Fail("Procedure supports no calculator model");
    }
    for (const auto& diag : cat.diagnostics) {
        if (!diag.latexExplanation.empty() && !ValidateLatexRichText(diag.latexExplanation).valid) {
            std::wcerr << L"[FAIL] Invalid LaTeX in diagnostic: " << diag.id << L"\n";
            return 1;
        }
        if (!diag.whyItHappens.empty() && !ValidateLatexRichText(diag.whyItHappens).valid) {
            std::wcerr << L"[FAIL] Invalid LaTeX whyItHappens in diagnostic: " << diag.id << L"\n";
            return 1;
        }
    }
    for (const auto& tip : cat.tips) {
        if (!tip.latexDetail.empty() && !ValidateLatexRichText(tip.latexDetail).valid) {
            std::wcerr << L"[FAIL] Invalid LaTeX in tip: " << tip.id << L"\n";
            return 1;
        }
        if (!tip.summary.empty() && !ValidateLatexRichText(tip.summary).valid) {
            std::wcerr << L"[FAIL] Invalid LaTeX summary in tip: " << tip.id << L"\n";
            return 1;
        }
    }

    // Export canonical deterministic catalog JSON into repo/catalog/casio_knowledge_catalog.json
    const fs::path repo = ResolveTestRepoRoot();
    const fs::path jsonPath = fs::exists(repo / "assets" / "catalog")
        ? (repo / "assets" / "catalog" / "casio_knowledge_catalog.json")
        : (repo / "catalog" / "casio_knowledge_catalog.json");
    if (!CasioKnowledge::ExportCatalogToJson(jsonPath.wstring())) {
        return Fail("ExportCatalogToJson failed");
    }
    std::ifstream jf(jsonPath, std::ios::binary | std::ios::ate);
    if (!jf) return Fail("Cannot reopen exported casio_knowledge_catalog.json");
    const auto jsz = static_cast<size_t>(jf.tellg());
    if (jsz < 30000) return Fail("Exported casio_knowledge_catalog.json unexpectedly small");
    jf.seekg(0);
    std::string jstr(jsz, '\0');
    jf.read(jstr.data(), jsz);
    if (jstr.find("\"schemaVersion\": \"1.0.0\"") == std::string::npos ||
        jstr.find("\"keys\": 150") == std::string::npos ||
        jstr.find("\"indicators\": 38") == std::string::npos ||
        jstr.find("\"procedures\": 14") == std::string::npos) {
        return Fail("Exported casio_knowledge_catalog.json missing expected summary fields");
    }

    std::cout << "[PASS] TestKnowledgeCatalogCompletenessAndJson\n";
    return 0;
}

int TestCatalogSequencesOnRealRoms()
{
    namespace fs = std::filesystem;
    const fs::path repo = ResolveTestRepoRoot();
    const fs::path cwRomPath = fs::exists(repo / "assets" / "roms" / "fx991lax.rom")
        ? (repo / "assets" / "roms" / "fx991lax.rom") : (repo / "roms" / "fx991lax.rom");
    const fs::path esRomPath = fs::exists(repo / "assets" / "roms" / "fx570vnplus.rom")
        ? (repo / "assets" / "roms" / "fx570vnplus.rom") : (repo / "roms" / "fx570vnplus.rom");
    auto readRom = [](const fs::path& p) -> std::vector<unsigned char> {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        if (!f) return {};
        const auto sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<unsigned char> data(sz);
        if (!f.read(reinterpret_cast<char*>(data.data()), sz)) return {};
        return data;
    };

    const auto& cat = CasioKnowledge::GetCatalog();

    // Run curated catalog procedure sequences on ClassWiz & ES PLUS ROMs
    auto findProc = [&](const wchar_t* id) -> const CasioKnowledge::ProcedureEntry* {
        for (const auto& p : cat.procedures) if (p.id == id) return &p;
        return nullptr;
    };
    const auto* procIntegral = findProc(L"proc_def_integral");
    const auto* procSto = findProc(L"proc_store_vars");
    if (!procIntegral || !procSto) return Fail("Required test procedures missing from catalog");

    // 1) ClassWiz ROM sequence verification (Integral + STO)
    {
        auto cwRom = readRom(cwRomPath);
        if (cwRom.empty()) return Fail("Failed reading cw rom for catalog sequence test");
        CalcEmuModel cwModel{ CALCEMU_HW_CLASSWIZ, 0, 0x0003, 0x00 };
        CalcEmu* emu = calcemu_create(&cwModel, cwRom.data(), cwRom.size());
        if (!emu) return Fail("calcemu_create CW failed in catalog seq test");
        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);

        std::vector<unsigned char> basePixels(calcemu_screen_width(emu) * calcemu_screen_rows(emu));
        calcemu_screen_pixels(emu, basePixels.data(), basePixels.size());

        for (int keyId : procIntegral->perModel[0].keys) {
            if (keyId < 0 || keyId >= NUM_CW_KEYS) { calcemu_destroy(emu); return Fail("Invalid CW keyId in catalog proc"); }
            const unsigned short code = g_classwizExactKeys[keyId].matrixCode;
            if (code == 0xFF) calcemu_power_on(emu);
            else calcemu_tap_key(emu, static_cast<unsigned char>(code), 400, 300);
        }
        calcemu_advance(emu, 1000);
        if (calcemu_screen_mode(emu) != 5) { calcemu_destroy(emu); return Fail("CW exited active LCD during integral catalog seq"); }
        std::vector<unsigned char> postIntPixels(basePixels.size());
        calcemu_screen_pixels(emu, postIntPixels.data(), postIntPixels.size());
        if (postIntPixels == basePixels) { calcemu_destroy(emu); return Fail("CW LCD did not update after integral catalog seq"); }

        calcemu_tap_key(emu, 0x42, 400, 300); // AC
        for (int keyId : procSto->perModel[0].keys) {
            if (keyId < 0 || keyId >= NUM_CW_KEYS) { calcemu_destroy(emu); return Fail("Invalid CW keyId in STO proc"); }
            calcemu_tap_key(emu, static_cast<unsigned char>(g_classwizExactKeys[keyId].matrixCode), 400, 300);
        }
        if (calcemu_screen_mode(emu) != 5) { calcemu_destroy(emu); return Fail("CW exited active LCD during STO proc"); }
        calcemu_destroy(emu);
    }

    // 2) ES PLUS ROM sequence verification (Integral + STO)
    {
        auto esRom = readRom(esRomPath);
        if (esRom.empty()) return Fail("Failed reading ES rom for catalog sequence test");
        CalcEmuModel esModel{ CALCEMU_HW_ES_PLUS, 0, 0x0001, 0x08 };
        CalcEmu* emu = calcemu_create(&esModel, esRom.data(), esRom.size());
        if (!emu) return Fail("calcemu_create ES failed in catalog seq test");
        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);

        std::vector<unsigned char> basePixels(calcemu_screen_width(emu) * calcemu_screen_rows(emu));
        calcemu_screen_pixels(emu, basePixels.data(), basePixels.size());

        for (int keyId : procIntegral->perModel[1].keys) {
            if (keyId < 0 || keyId >= NUM_GRAY_KEYS) { calcemu_destroy(emu); return Fail("Invalid ES keyId in catalog proc"); }
            const unsigned short code = g_grayExactKeys[keyId].matrixCode;
            if (code == 0xFF) calcemu_power_on(emu);
            else calcemu_tap_key(emu, static_cast<unsigned char>(code), 400, 300);
        }
        calcemu_advance(emu, 1000);
        if (calcemu_screen_mode(emu) != 5) { calcemu_destroy(emu); return Fail("ES exited active LCD during integral catalog seq"); }
        std::vector<unsigned char> postIntPixels(basePixels.size());
        calcemu_screen_pixels(emu, postIntPixels.data(), postIntPixels.size());
        if (postIntPixels == basePixels) { calcemu_destroy(emu); return Fail("ES LCD did not update after integral catalog seq"); }

        calcemu_tap_key(emu, 0x42, 400, 300); // AC
        for (int keyId : procSto->perModel[1].keys) {
            if (keyId < 0 || keyId >= NUM_GRAY_KEYS) { calcemu_destroy(emu); return Fail("Invalid ES keyId in STO proc"); }
            calcemu_tap_key(emu, static_cast<unsigned char>(g_grayExactKeys[keyId].matrixCode), 400, 300);
        }
        if (calcemu_screen_mode(emu) != 5) { calcemu_destroy(emu); return Fail("ES exited active LCD during STO proc"); }
        calcemu_destroy(emu);
    }

    std::cout << "[PASS] TestCatalogSequencesOnRealRoms\n";
    return 0;
}

int TestNeutralUiStringsAndZeroEmojis()
{
    namespace fs = std::filesystem;
    const fs::path repo = ResolveTestRepoRoot();
    const char* files[] = {
        "apps/windows/main.cpp",
        "apps/windows/fx991cw_host.h",
        "apps/windows/fx991cw_host.cpp",
        "apps/windows/ui_theme.h",
        "apps/windows/settings_and_exporter.h",
        "apps/windows/smart_help_view.h",
        "apps/windows/key_history_view.h",
        "apps/windows/popup_display.h",
        "apps/windows/lcd_indicators.h",
        "apps/windows/operation_capture.h",
        "apps/windows/latex_math_renderer.h",
        "apps/windows/manual_catalog.h",
        "apps/windows/pdf_viewer.h",
        "apps/windows/knowledge_catalog.h",
        "apps/windows/learning_studio.h",
        "studio/app/main_studio.cpp",
        "assets/catalog/casio_knowledge_catalog.json"
    };
    const wchar_t* bannedTerms[] = {
        L"alumno",
        L"profesor",
        L"did\u00E1ctic",
        L"didactic",
        L"guias_alumnos",
        L"alum.",
        L"Cach\u00E9 0/256 MiB",
        L"Apariencia \u00B7 material translúcido 2.0",
        L"nX-U8 + firmware embebido",
        L"LaTeX v\u00E1lido",
        L"Sintaxis LaTeX"
    };
    const char* dpiRequiredFiles[] = {
        "apps/windows/main.cpp",
        "apps/windows/cg50_live_comparator.h",
        "apps/windows/settings_and_exporter.h",
        "apps/windows/learning_studio.h",
        "apps/windows/smart_help_view.h",
        "apps/windows/pdf_viewer.h",
        "apps/windows/popup_display.h",
        "apps/windows/key_history_view.h",
        "studio/app/main_studio.cpp"
    };
    for (const char* drel : dpiRequiredFiles) {
        std::ifstream df(repo / drel, std::ios::binary);
        if (!df) return Fail("Cannot open window module for WM_DPICHANGED audit");
        std::string dbytes((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
        if (dbytes.find("case WM_DPICHANGED:") == std::string::npos) {
            std::cerr << "[FAIL] Missing case WM_DPICHANGED: in " << drel << "\n";
            return 1;
        }
    }

    for (const char* rel : files) {
        const fs::path p = repo / rel;
        std::ifstream f(p, std::ios::binary);
        if (!f) return Fail("Cannot open source file for policy audit");
        std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        if (wlen <= 0) continue;
        std::wstring ws(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), ws.data(), wlen);

        for (const wchar_t* term : bannedTerms) {
            if (ContainsCaseInsensitive(ws, term)) {
                std::cerr << "[FAIL] Banned term in " << rel << "\n";
                return 1;
            }
        }
        for (size_t i = 0; i < ws.size(); ++i) {
            const wchar_t c = ws[i];
            if (c == 0x2699 || c == 0x25B6 || c == 0x25AC || c == 0x2197 || c == 0x25A6) {
                std::cerr << "[FAIL] Disallowed decorative icon char in " << rel << "\n";
                return 1;
            }
            if (c >= 0xD83C && c <= 0xDBFF) { // UTF-16 high surrogate (all emoji SMP blocks U+1F000+)
                std::cerr << "[FAIL] Surrogate emoji found in " << rel << "\n";
                return 1;
            }
        }
    }

    std::cout << "[PASS] TestNeutralUiStringsAndZeroEmojis\n";
    return 0;
}

int TestUiScalingAndTransformAlignment()
{
    if (CasioTheme::UiToDevice(256, 2.0f) != 512) return Fail("UiToDevice 2.0x mismatch");
    if (CasioTheme::DeviceToLogical(512, 2.0f) != 256) return Fail("DeviceToLogical 2.0x mismatch");
    const RECT dLog = CasioTheme::DeviceRectToLogical(RECT{ 20, 40, 520, 140 }, 2.0f);
    if (dLog.left != 10 || dLog.top != 20 || dLog.right != 260 || dLog.bottom != 70) {
        return Fail("DeviceRectToLogical 2.0x mismatch");
    }
    if (CasioTheme::ClampAndSnapUiScale(0.5f) < 1.0f) return Fail("ClampAndSnapUiScale lower bound");
    if (CasioTheme::ClampAndSnapUiScale(5.0f) > 2.5f) return Fail("ClampAndSnapUiScale upper bound");

    const int bmpW = 420;
    const int bmpH = 180;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmpW;
    bmi.bmiHeader.biHeight = -bmpH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp || !bits) {
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
        return Fail("Failed to allocate DIB for UiScaleScope test");
    }
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);

    const COLORREF bgCol = RGB(18, 22, 30);
    RECT rcFull{ 0, 0, bmpW, bmpH };
    HBRUSH brBg = CreateSolidBrush(bgCol);
    FillRect(hdcMem, &rcFull, brBg);
    DeleteObject(brBg);

    {
        CasioTheme::UiScaleScope scope(hdcMem, 2.0f);
        const RECT boxLog{ 10, 10, 180, 60 }; // Physical with 2x transform -> {20, 20, 360, 120}
        CasioTheme::FillRoundRectAA(hdcMem, boxLog, 8.0f, RGB(37, 99, 235), RGB(96, 165, 250), 1.0f);
        // Place formula inside logical {95, 22, 176, 56} -> physical {190, 44, 352, 112}
        DrawLatexRichText(hdcMem, L"$x^{2}+\\frac{1}{2}$", RECT{ 95, 22, 176, 56 },
                          RGB(255, 255, 255), RGB(255, 255, 255), 14, false);
    }

    // Ensure world transform is cleanly restored back to identity after scope exit
    XFORM xf{};
    if (!GetWorldTransform(hdcMem, &xf) ||
        std::fabs(xf.eM11 - 1.0f) > 0.01f || std::fabs(xf.eM22 - 1.0f) > 0.01f ||
        std::fabs(xf.eDx) > 0.01f || std::fabs(xf.eDy) > 0.01f) {
        SelectObject(hdcMem, oldBmp);
        DeleteDC(hdcMem);
        DeleteObject(hBmp);
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
        return Fail("UiScaleScope did not restore identity XFORM");
    }

    GdiFlush();
    const int stride = ((bmpW * 3 + 3) / 4) * 4;
    const auto* rowBytes = static_cast<const unsigned char*>(bits);
    auto pixelAt = [&](int x, int y) -> COLORREF {
        const unsigned char* p = rowBytes + y * stride + x * 3;
        return RGB(p[2], p[1], p[0]);
    };

    // (220, 80) and (350, 110) are outside unscaled {10..180, 10..60} but inside 2x {20..360, 20..120}
    const COLORREF insideScaled = pixelAt(220, 80);
    const COLORREF cornerInside = pixelAt(350, 110);
    if (insideScaled == bgCol || cornerInside == bgCol) {
        std::cerr << "[DIAG] insideScaled=(" << (int)GetRValue(insideScaled) << "," << (int)GetGValue(insideScaled) << "," << (int)GetBValue(insideScaled) << ")"
                  << " cornerInside=(" << (int)GetRValue(cornerInside) << "," << (int)GetGValue(cornerInside) << "," << (int)GetBValue(cornerInside) << ")\n";
        SelectObject(hdcMem, oldBmp);
        DeleteDC(hdcMem);
        DeleteObject(hBmp);
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
        return Fail("FillRoundRectAA did not scale into exact 2x GDI+ physical bounds");
    }
    // Check that LaTeX white pixels rendered strictly inside 2x region (> 176 in X and > 56 in Y)
    int whiteScaledPixels = 0;
    for (int py = 58; py < 112; ++py) {
        for (int px = 190; px < 350; ++px) {
            const COLORREF c = pixelAt(px, py);
            if (GetRValue(c) > 200 && GetGValue(c) > 200 && GetBValue(c) > 200) {
                ++whiteScaledPixels;
            }
        }
    }
    if (whiteScaledPixels < 8) {
        SelectObject(hdcMem, oldBmp);
        DeleteDC(hdcMem);
        DeleteObject(hBmp);
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
        return Fail("DrawLatexRichText did not scale white glyphs into 2x region");
    }

    // Outside scaled box (365, 80), (200, 125), (395, 160) must remain exact background color
    const COLORREF outRight = pixelAt(365, 80);
    const COLORREF outBot   = pixelAt(200, 125);
    const COLORREF outFar   = pixelAt(395, 160);
    if (outRight != bgCol || outBot != bgCol || outFar != bgCol) {
        std::cerr << "[DIAG] outRight=(" << (int)GetRValue(outRight) << "," << (int)GetGValue(outRight) << "," << (int)GetBValue(outRight) << ")"
                  << " outBot=(" << (int)GetRValue(outBot) << "," << (int)GetGValue(outBot) << "," << (int)GetBValue(outBot) << ")"
                  << " outFar=(" << (int)GetRValue(outFar) << "," << (int)GetGValue(outFar) << "," << (int)GetBValue(outFar) << ")\n";
        SelectObject(hdcMem, oldBmp);
        DeleteDC(hdcMem);
        DeleteObject(hBmp);
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
        return Fail("Scaled draw bled past 2x bounds");
    }

    SelectObject(hdcMem, oldBmp);
    DeleteDC(hdcMem);
    DeleteObject(hBmp);
    if (hdcScreen) ReleaseDC(nullptr, hdcScreen);

    const int prevMode = CasioTheme::g_uiScaleMode;
    CasioTheme::g_uiScaleMode = 100;
    const float forced100 = CasioTheme::ComputeDialogUiScale(nullptr, 720, 646);
    CasioTheme::g_uiScaleMode = 200;
    const float clampedHuge = CasioTheme::ComputeDialogUiScale(nullptr, 50000, 50000);
    CasioTheme::g_uiScaleMode = prevMode;
    if (std::abs(forced100 - 1.0f) > 0.01f || std::abs(clampedHuge - 1.0f) > 0.01f) {
        return Fail("ComputeDialogUiScale bounds check failed");
    }

    // Synthetic WM_DPICHANGED (144 -> 192 -> 96 DPI) injection test on live Native PDF Viewer window
    {
        const auto samplePdf = (std::filesystem::exists(ResolveTestRepoRoot() / "private_local" / "manuales")
            ? (ResolveTestRepoRoot() / "private_local" / "manuales" / g_officialManuals[0].fileName)
            : (ResolveTestRepoRoot() / "manuales" / g_officialManuals[0].fileName)).wstring();
        HWND hPdf = CreateNativePdfViewerWindow(samplePdf, 1, nullptr, L"", nullptr, false, 0, 16ull * 1024ull * 1024ull, false);
        if (!hPdf || !IsWindow(hPdf)) {
            return Fail("Failed to create offscreen NativePdfViewer window for WM_DPICHANGED test");
        }
        struct DpiStep { UINT dpi; RECT rc; };
        const DpiStep steps[3] = {
            { 144u, RECT{ 80, 60, 1280, 920 } },
            { 192u, RECT{ 80, 60, 1440, 1040 } },
            { 96u,  RECT{ 80, 60, 1100, 840 } }
        };
        for (const auto& s : steps) {
            SendMessageW(hPdf, WM_DPICHANGED, MAKEWPARAM(s.dpi, s.dpi), reinterpret_cast<LPARAM>(&s.rc));
            RECT actual{};
            GetWindowRect(hPdf, &actual);
            if ((actual.right - actual.left) != (s.rc.right - s.rc.left) ||
                (actual.bottom - actual.top) != (s.rc.bottom - s.rc.top)) {
                DestroyWindow(hPdf);
                return Fail("PdfViewerWndProc WM_DPICHANGED did not resize window to suggested RECT");
            }
        }
        DestroyWindow(hPdf);
    }

    std::cout << "[PASS] TestUiScalingAndTransformAlignment\n";
    return 0;
}

int TestFx991CwKeysAndCaptureRects()
{
    if (NUM_CW2_KEYS != 48) {
        return Fail("Expected 48 fx-991CW keys in g_cw2ExactKeys");
    }
    for (int i = 0; i < NUM_CW2_KEYS; ++i) {
        const auto& k = g_cw2ExactKeys[i];
        if (k.id != i || !k.name || k.name[0] == L'\0') {
            return Fail("Invalid ID/name in g_cw2ExactKeys");
        }
        if (k.cx < 40 || k.cx > 952 || k.cy < 720 || k.cy > 2020 || k.w < 88 || k.h < 88) {
            return Fail("Out-of-bounds fx-991CW 992x2082 key geometry");
        }
        const auto spec = CalculatorKeyCaptureSpec(3, i, k.cx, k.cy, k.w, k.h, 992, 2082);
        const int cw = spec.bounds.right - spec.bounds.left;
        const int ch = spec.bounds.bottom - spec.bounds.top;
        if (spec.bounds.left < 0 || spec.bounds.top < 0 ||
            spec.bounds.right > 992 || spec.bounds.bottom > 2082 ||
            cw < 90 || ch < 90) {
            return Fail("Invalid CalculatorKeyCaptureSpec bounds for fx-991CW");
        }
    }

    // Verify official 992x2140 interface.png and calibrated 992x2082 model.lua
    const std::filesystem::path repoRoot = ResolveTestRepoRoot();
    const auto cw2ModelDir = std::filesystem::exists(repoRoot / "private_local" / "official_casio_runtime" / "fx991cw" / "models" / "fx991cw_emu")
        ? (repoRoot / "private_local" / "official_casio_runtime" / "fx991cw" / "models" / "fx991cw_emu")
        : (repoRoot / "runtime" / "fx991cw" / "models" / "fx991cw_emu");
    const auto ifacePath = cw2ModelDir / "interface.png";
    const auto luaPath = cw2ModelDir / "model.lua";
    if (!EnsureGdiplusInitialized()) return Fail("Gdiplus init failed");
    {
        Gdiplus::Bitmap bmp(ifacePath.c_str());
        if (bmp.GetLastStatus() != Gdiplus::Ok || bmp.GetWidth() != 992 || bmp.GetHeight() != 2140) {
            return Fail("Official fx-991CW interface.png missing or not 992x2140");
        }
    }
    std::ifstream luaStream(luaPath);
    if (!luaStream.is_open()) return Fail("Missing runtime/fx991cw/models/fx991cw_emu/model.lua");
    std::string luaText((std::istreambuf_iterator<char>(luaStream)), std::istreambuf_iterator<char>());
    if (luaText.find("rsd_interface = {0, 0, 992, 2082, 0, 0}") == std::string::npos ||
        luaText.find("rsd_pixel = { 112, 358,  4,  4,  112, 358}") == std::string::npos) {
        return Fail("fx-991CW model.lua not aligned to 992x2082 official geometry");
    }

    std::cout << "[PASS] TestFx991CwKeysAndCaptureRects\n";
    return 0;
}

int TestNativePdfViewerAll16Manuals()
{
    namespace fs = std::filesystem;
    const fs::path manualsDir = fs::exists(ResolveTestRepoRoot() / "private_local" / "manuales")
        ? (ResolveTestRepoRoot() / "private_local" / "manuales")
        : (ResolveTestRepoRoot() / "manuales");
    if (!fs::exists(manualsDir)) return Fail("Missing manuales/ directory");

    constexpr size_t total = sizeof(g_officialManuals) / sizeof(g_officialManuals[0]);
    static_assert(total == 16, "Expected 16 official PDF manuals in manual_catalog.h");

    int maskCover = 0;
    for (size_t i = 0; i < total; ++i) {
        const auto& m = g_officialManuals[i];
        maskCover |= m.calcMask;

        const fs::path pdfPath = manualsDir / m.fileName;
        if (!fs::exists(pdfPath)) {
            std::cerr << "[FAIL] Missing PDF manual: " << pdfPath.string() << "\n";
            return 1;
        }
        const uintmax_t fsz = fs::file_size(pdfPath);
        if (fsz < 10240) {
            std::cerr << "[FAIL] PDF file too small: " << pdfPath.string() << "\n";
            return 1;
        }

        int pageCount = 0;
        int w = 0;
        int h = 0;
        int ink = 0;
        const HRESULT hr = PdfNativeProbeRenderPageToDIB(
            pdfPath.wstring(), 1, 720, &pageCount, &w, &h, &ink);
        if (FAILED(hr) || pageCount < 1 || w < 400 || h < 250 || ink < 400) {
            std::cerr << "[FAIL] Native WinRT PDF render failed for " << pdfPath.string()
                      << " hr=0x" << std::hex << hr << std::dec
                      << " pages=" << pageCount << " size=" << w << "x" << h
                      << " ink=" << ink << "\n";
            return 1;
        }

        int midPage = 0;
        int midInk = 0;
        if (pageCount >= 10) {
            midPage = (pageCount >= 20) ? 10 : (pageCount / 2);
            int mw = 0, mh = 0, mpc = 0;
            const HRESULT hrMid = PdfNativeProbeRenderPageToDIB(
                pdfPath.wstring(), midPage, 720, &mpc, &mw, &mh, &midInk);
            if (FAILED(hrMid) || mw < 400 || mh < 250 || midInk < 400) {
                std::cerr << "[FAIL] Native WinRT PDF inner page " << midPage
                          << " render failed for " << pdfPath.string()
                          << " hr=0x" << std::hex << hrMid << std::dec << "\n";
                return 1;
            }
        }

        std::string narrowName = pdfPath.filename().string();
        std::cout << "  [PDF] " << narrowName << ": pages=" << pageCount
                  << ", p1=" << w << "x" << h << ", ink1=" << ink;
        if (midPage > 0) {
            std::cout << ", p" << midPage << "_ink=" << midInk;
        }
        std::cout << "\n";
    }

    if ((maskCover & 0x0F) != 0x0F) {
        return Fail("PDF catalog does not cover all 4 calculator models");
    }
    if (!fs::exists(manualsDir / "indice_paginas_pdf.tsv")) {
        return Fail("Missing manuales/indice_paginas_pdf.tsv");
    }

    std::cout << "[PASS] TestNativePdfViewerAll16Manuals (" << total << "/16 PDFs verified)\n";
    return 0;
}

int TestU13EighteenCaseRomDifferentialMatrix()
{
    namespace fs = std::filesystem;
    const fs::path repo = ResolveTestRepoRoot();
    const fs::path cwRomPath = fs::exists(repo / "assets" / "roms" / "fx991lax.rom")
        ? (repo / "assets" / "roms" / "fx991lax.rom") : (repo / "roms" / "fx991lax.rom");
    const fs::path esRomPath = fs::exists(repo / "assets" / "roms" / "fx570vnplus.rom")
        ? (repo / "assets" / "roms" / "fx570vnplus.rom") : (repo / "roms" / "fx570vnplus.rom");
    auto readRom = [](const fs::path& p) -> std::vector<unsigned char> {
        std::ifstream f(p, std::ios::binary | std::ios::ate);
        if (!f) return {};
        const auto sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<unsigned char> data(sz);
        if (!f.read(reinterpret_cast<char*>(data.data()), sz)) return {};
        return data;
    };

    const auto& cases = U13Matrix::Get18Cases();
    if (cases.size() != 18) return Fail("U-13 matrix case count != 18");

    // 1) Blanca fx-570LA X (LineIO, comma decimal)
    {
        auto rom = readRom(cwRomPath);
        if (rom.empty()) return Fail("U-13 failed reading fx991lax.rom");
        CalcEmuModel m{ CALCEMU_HW_CLASSWIZ, 0, 0x0003, 0x00 };
        CalcEmu* emu = calcemu_create(&m, rom.data(), rom.size());
        if (!emu) return Fail("U-13 calcemu_create CW failed");
        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);
        // LineIO setup: SHIFT (0x07) -> MENU (0x47) -> 1 (0x00) -> 3 (0x20)
        calcemu_tap_key(emu, 0x07, 400, 1500);
        calcemu_tap_key(emu, 0x47, 400, 1500);
        calcemu_tap_key(emu, 0x00, 400, 1500);
        calcemu_tap_key(emu, 0x20, 400, 2000);

        const int w = calcemu_screen_width(emu);
        const int h = calcemu_screen_rows(emu);
        std::vector<std::uint8_t> px(static_cast<size_t>(w) * h);
        for (const auto& c : cases) {
            calcemu_tap_key(emu, 0x42, 400, 2500); // AC
            for (const auto& kt : c.keys) {
                calcemu_tap_key(emu, kt.nxCode, 400, 1500);
            }
            calcemu_tap_key(emu, 0x60, 400, 2500); // =
            calcemu_screen_pixels(emu, px.data(), px.size());
            const auto lines = U13Matrix::DecodeLcdLines(px, w, h);
            const std::string got = lines.empty() ? std::string() : lines.back();
            const std::string exp = U13Matrix::ExpectedForLax(c.expectedDot);
            if (got != exp) {
                std::cerr << "[FAIL] U-13 Win LAX [" << c.id << "] exp=" << exp << " got=" << got << "\n";
                calcemu_destroy(emu);
                return 1;
            }
        }
        calcemu_destroy(emu);
    }

    // 2) Gris fx-570VN PLUS (LineIO, dot decimal)
    {
        auto rom = readRom(esRomPath);
        if (rom.empty()) return Fail("U-13 failed reading fx570vnplus.rom");
        CalcEmuModel m{ CALCEMU_HW_ES_PLUS, 0, 0x0001, 0x08 };
        CalcEmu* emu = calcemu_create(&m, rom.data(), rom.size());
        if (!emu) return Fail("U-13 calcemu_create ES failed");
        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);
        // LineIO setup: SHIFT (0x07) -> MODE (0x47) -> 2 (0x10)
        calcemu_tap_key(emu, 0x07, 400, 1500);
        calcemu_tap_key(emu, 0x47, 400, 1500);
        calcemu_tap_key(emu, 0x10, 400, 2000);

        const int w = calcemu_screen_width(emu);
        const int h = calcemu_screen_rows(emu);
        std::vector<std::uint8_t> px(static_cast<size_t>(w) * h);
        for (const auto& c : cases) {
            calcemu_tap_key(emu, 0x42, 400, 2500); // AC
            for (const auto& kt : c.keys) {
                calcemu_tap_key(emu, kt.nxCode, 400, 1500);
            }
            calcemu_tap_key(emu, 0x60, 400, 2500); // =
            calcemu_screen_pixels(emu, px.data(), px.size());
            const auto lines = U13Matrix::DecodeLcdLines(px, w, h);
            const std::string got = lines.empty() ? std::string() : lines.back();
            if (got != c.expectedDot) {
                std::cerr << "[FAIL] U-13 Win VN+ [" << c.id << "] exp=" << c.expectedDot << " got=" << got << "\n";
                calcemu_destroy(emu);
                return 1;
            }
        }
        calcemu_destroy(emu);
    }

    // 3) Differential check against MathEvaluator (calc_engine.h)
    for (const auto& c : cases) {
        std::wstring wExpr(c.expr, c.expr + std::strlen(c.expr));
        double outVal = 0.0;
        std::wstring err;
        if (!MathEvaluator::Evaluate(wExpr, 0.0, ANG_DEG, outVal, err)) {
            std::cerr << "[FAIL] U-13 MathEvaluator error on " << c.expr << "\n";
            return 1;
        }
        const std::wstring wFmt = MathEvaluator::FormatResult(outVal);
        const std::wstring wExp(c.expectedDot, c.expectedDot + std::strlen(c.expectedDot));
        if (wFmt != wExp) {
            std::wcerr << L"[FAIL] U-13 MathEvaluator mismatch on " << wExpr
                       << L": exp=" << wExp << L" got=" << wFmt << L"\n";
            return 1;
        }
    }

    std::cout << "[PASS] TestU13EighteenCaseRomDifferentialMatrix (18/18 LAX + 18/18 VN+ + 18/18 MathEvaluator)\n";
    return 0;
}

} // namespace

int main()
{
    if (TestLcdIndicators() != 0) return 1;
    if (TestLatexSubsetEngine() != 0) return 1;
    if (TestOperationCaptureAndWritablePaths() != 0) return 1;
    if (TestWin32ClipboardMultiFormat() != 0) return 1;
    if (TestCalcEmuRomModesAndSaveRestore() != 0) return 1;
    if (TestKnowledgeCatalogCompletenessAndJson() != 0) return 1;
    if (TestCatalogSequencesOnRealRoms() != 0) return 1;
    if (TestNeutralUiStringsAndZeroEmojis() != 0) return 1;
    if (TestUiScalingAndTransformAlignment() != 0) return 1;
    if (TestFx991CwKeysAndCaptureRects() != 0) return 1;
    if (TestNativePdfViewerAll16Manuals() != 0) return 1;
    if (TestU13EighteenCaseRomDifferentialMatrix() != 0) return 1;
    std::cout << "ALL UiCoreModules TESTS PASSED (12/12)\n";
    return 0;
}
