// ============================================================================
// fx991cw_host.cpp
// Native 64-bit in-process fx-991CW (ClassWiz II) host powered directly by
// certified core/casio_cw2 v47 (nX-U16/100, 192x63 4-level grayscale, 20-icon
// indicator strip, 24832-byte persistent NVRAM).
//
// Zero external casioemu.exe subprocess, zero SDL2.dll dependency.
// ============================================================================

#include "fx991cw_host.h"
#include "casio_cw2.h"
#include "cw2_keys_exact.h"
#include "operation_capture.h"

#include <windowsx.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int kCw2SkinW = 992;
constexpr int kCw2SkinH = 2082;
constexpr int kCw2LcdLeft = 112;
constexpr int kCw2LcdTop = 318;
constexpr int kCw2LcdRight = 880;
constexpr int kCw2LcdBottom = 610;
constexpr UINT_PTR kCw2TimerId = 0x5191;

// Authentic 4-level ClassWiz II grayscale palette (BGRA32: 0xAARRGGBB)
constexpr std::uint32_t kCw2GrayPalette[4] = {
    0xFFE4EBE4u, // level 0: off (soft sage LCD glass)
    0xFFA4B3A8u, // level 1: light gray
    0xFF5A6D72u, // level 2: mid-dark slate
    0xFF14243Au  // level 3: crisp dark ink
};

struct Cw2IndicatorLabel {
    int bit;
    const wchar_t* text;
};

constexpr Cw2IndicatorLabel kCw2Indicators[] = {
    {  0, L"S" },
    {  1, L"Math" },
    {  2, L"D" },
    {  3, L"R" },
    {  4, L"G" },
    {  5, L"FIX" },
    {  6, L"SCI" },
    {  7, L"f(x)" },
    {  8, L"E" },
    {  9, L"i" },
    { 10, L"\u2220" },
    { 11, L"\u25BC" },
    { 12, L"VRFY" },
    { 13, L"g(x)" },
    { 14, L"\u25C0" },
    { 15, L"\u25BC" },
    { 16, L"\u25B2" },
    { 17, L"\u25B6" },
    { 18, L"\u23F8" },
    { 19, L"\u2600" },
};

struct Cw2AssetPaths {
    fs::path romPath;
    fs::path defaultRamPath;
    fs::path skinPath;
    fs::path liveRamPath;
    bool valid = false;
};

struct NativeCw2Globals {
    Fx991CwState state = Fx991CwState::Stopped;
    std::wstring lastError;
    HWND hChild = nullptr;
    CasioCw2Core* core = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> skinBmp;
    std::vector<std::uint8_t> grayBuf;
    std::vector<std::uint32_t> lcdBgra;
    int lcdW = 192;
    int lcdH = 63;
    std::uint32_t indicators = 0;
    int pressedKeyIdx = -1;
    bool mouseCaptured = false;
    RECT lastRect{ 0, 0, 0, 0 };
    bool lastVisible = false;
    bool hasAppliedRect = false;
    Cw2AssetPaths paths;
};

NativeCw2Globals g_cw2;

fs::path GetModuleDir()
{
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (len == 0) return fs::current_path();
        if (len < buf.size() - 1) {
            buf.resize(len);
            return fs::path(buf).parent_path();
        }
        buf.resize(buf.size() * 2);
    }
}

bool ReadFileBytes(const fs::path& p, std::vector<std::uint8_t>& out)
{
    std::error_code ec;
    if (!fs::is_regular_file(p, ec) || ec) return false;
    const auto sz = fs::file_size(p, ec);
    if (ec || sz == 0 || sz > (32ull << 20)) return false;
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.resize(static_cast<std::size_t>(sz));
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return in.good() || static_cast<std::size_t>(in.gcount()) == out.size();
}

bool WriteFileBytesAtomic(const fs::path& p, const std::uint8_t* data, std::size_t size)
{
    if (!data || size == 0 || p.empty()) return false;
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    const fs::path tmp = p.wstring() + L".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        out.flush();
        if (!out.good()) return false;
    }
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

Cw2AssetPaths ResolveCw2Assets()
{
    Cw2AssetPaths out;
    std::vector<fs::path> roots;
    const fs::path exeDir = GetModuleDir();
    fs::path cur = exeDir;
    for (int i = 0; i < 6 && !cur.empty(); ++i) {
        roots.push_back(cur);
        if (cur == cur.parent_path()) break;
        cur = cur.parent_path();
    }
    roots.push_back(fs::current_path());
#ifdef CASIO_MONOREPO_ROOT
    roots.emplace_back(CASIO_MONOREPO_ROOT);
#endif

    std::error_code ec;
    for (const auto& r : roots) {
        if (out.romPath.empty()) {
            const fs::path c1 = r / L"assets" / L"roms" / L"fx991cw_rom.bin";
            const fs::path c2 = r / L"roms" / L"fx991cw_rom.bin";
            const fs::path c3 = r / L"private_local" / L"official_casio_runtime" / L"fx991cw" / L"models" / L"fx991cw_emu" / L"rom.bin";
            if (fs::is_regular_file(c1, ec) && !ec) out.romPath = c1;
            else if (fs::is_regular_file(c2, ec) && !ec) out.romPath = c2;
            else if (fs::is_regular_file(c3, ec) && !ec) out.romPath = c3;
            ec.clear();
        }
        if (out.defaultRamPath.empty()) {
            const fs::path r1 = r / L"assets" / L"roms" / L"fx991cw_default_en.ram";
            const fs::path r2 = r / L"roms" / L"fx991cw_default_en.ram";
            const fs::path r3 = r / L"private_local" / L"official_casio_runtime" / L"fx991cw" / L"models" / L"fx991cw_emu" / L"default_en.ram";
            if (fs::is_regular_file(r1, ec) && !ec) out.defaultRamPath = r1;
            else if (fs::is_regular_file(r2, ec) && !ec) out.defaultRamPath = r2;
            else if (fs::is_regular_file(r3, ec) && !ec) out.defaultRamPath = r3;
            ec.clear();
        }
        if (out.skinPath.empty()) {
            const fs::path s1 = r / L"assets" / L"skins" / L"caratula_cw2.png";
            const fs::path s2 = r / L"skins" / L"caratula_cw2.png";
            const fs::path s3 = r / L"private_local" / L"official_casio_runtime" / L"fx991cw" / L"models" / L"fx991cw_emu" / L"interface.png";
            if (fs::is_regular_file(s1, ec) && !ec) out.skinPath = s1;
            else if (fs::is_regular_file(s2, ec) && !ec) out.skinPath = s2;
            else if (fs::is_regular_file(s3, ec) && !ec) out.skinPath = s3;
            ec.clear();
        }
    }

    const std::wstring stateDir = ResolveWritableOutputDir(L"cw2_state");
    if (!stateDir.empty()) {
        out.liveRamPath = fs::path(stateDir) / L"fx991cw_live.ram";
    }
    out.valid = !out.romPath.empty();
    return out;
}

void PersistLiveRam()
{
    if (!g_cw2.core || g_cw2.paths.liveRamPath.empty()) return;
    const std::size_t ramSz = casio_cw2_core_ram_size(g_cw2.core);
    if (ramSz == 0) return;
    std::vector<std::uint8_t> buf(ramSz);
    const std::size_t got = casio_cw2_core_export_ram(g_cw2.core, buf.data(), buf.size());
    if (got == ramSz) {
        (void)WriteFileBytesAtomic(g_cw2.paths.liveRamPath, buf.data(), buf.size());
    }
}

void RefreshLcdPixels()
{
    if (!g_cw2.core) return;
    g_cw2.lcdW = casio_cw2_core_screen_width(g_cw2.core);
    g_cw2.lcdH = casio_cw2_core_screen_height(g_cw2.core);
    if (g_cw2.lcdW <= 0 || g_cw2.lcdH <= 0) return;

    const std::size_t count = static_cast<std::size_t>(g_cw2.lcdW) * static_cast<std::size_t>(g_cw2.lcdH);
    g_cw2.grayBuf.resize(count);
    g_cw2.lcdBgra.resize(count);
    casio_cw2_core_copy_gray_levels(g_cw2.core, g_cw2.grayBuf.data(), g_cw2.grayBuf.size());
    g_cw2.indicators = casio_cw2_core_indicators(g_cw2.core);

    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t lv = g_cw2.grayBuf[i] & 0x03u;
        g_cw2.lcdBgra[i] = kCw2GrayPalette[lv];
    }
}

int HitTestCw2ClientPoint(int x, int y, int clientW, int clientH)
{
    if (clientW <= 0 || clientH <= 0) return -1;
    const double sx = (static_cast<double>(x) * kCw2SkinW) / static_cast<double>(clientW);
    const double sy = (static_cast<double>(y) * kCw2SkinH) / static_cast<double>(clientH);
    int best = -1;
    double bestDist2 = 1e18;
    for (int i = 0; i < NUM_CW2_KEYS; ++i) {
        const auto& k = g_cw2ExactKeys[i];
        const double hw = k.w * 0.56;
        const double hh = k.h * 0.56;
        if (sx >= k.cx - hw && sx <= k.cx + hw && sy >= k.cy - hh && sy <= k.cy + hh) {
            const double dx = sx - k.cx;
            const double dy = sy - k.cy;
            const double d2 = dx * dx + dy * dy;
            if (d2 < bestDist2) {
                bestDist2 = d2;
                best = i;
            }
        }
    }
    return best;
}

std::uint8_t MapVkToCw2Matrix(WPARAM vk)
{
    switch (vk) {
    case '0': case VK_NUMPAD0: return 0x64;
    case '1': case VK_NUMPAD1: return 0x00;
    case '2': case VK_NUMPAD2: return 0x10;
    case '3': case VK_NUMPAD3: return 0x20;
    case '4': case VK_NUMPAD4: return 0x01;
    case '5': case VK_NUMPAD5: return 0x11;
    case '6': case VK_NUMPAD6: return 0x21;
    case '7': case VK_NUMPAD7: return 0x02;
    case '8': case VK_NUMPAD8: return 0x12;
    case '9': case VK_NUMPAD9: return 0x22;
    case VK_ADD: case VK_OEM_PLUS: return 0x30;       // +
    case VK_SUBTRACT: case VK_OEM_MINUS: return 0x40; // -
    case VK_MULTIPLY: return 0x31;                    // x
    case VK_DIVIDE: case VK_OEM_2: return 0x41;       // /
    case VK_DECIMAL: case VK_OEM_PERIOD: return 0x63; // .
    case VK_RETURN: return 0x60;                      // EXE
    case VK_BACK: case VK_DELETE: return 0x32;        // DEL
    case VK_ESCAPE: return 0x42;                      // AC
    case VK_UP: return 0x37;
    case VK_DOWN: return 0x35;
    case VK_LEFT: return 0x26;
    case VK_RIGHT: return 0x46;
    case VK_HOME: return 0x17;                        // HOME
    case VK_SHIFT: return 0x05;                       // SHIFT
    case 'S': return 0x13;                            // SIN
    case 'C': return 0x23;                            // COS
    case 'T': return 0x33;                            // TAN
    case 'X': return 0x04;                            // x
    case 'L': return 0x54;                            // LOG
    case 'F': return 0x61;                            // FORMAT
    case 'A': return 0x03;                            // ANS
    case 'R': return 0x24;                            // SQRT
    case 'P': return 0x34;                            // POWER
    case VK_OEM_4: return 0x43;                       // (
    case VK_OEM_6: return 0x53;                       // )
    default: return 0x00u; // 0x00 is key '1', so use 0xFE sentinel below
    }
}

bool TryMapVkToCw2(WPARAM vk, std::uint8_t& outCode)
{
    if (vk == '1' || vk == VK_NUMPAD1) {
        outCode = 0x00;
        return true;
    }
    const std::uint8_t c = MapVkToCw2Matrix(vk);
    if (c == 0x00u) return false;
    outCode = c;
    return true;
}

void PaintCw2LcdIntoRect(HDC hdc, const RECT& lcdRc)
{
    const int lw = (std::max)(1, static_cast<int>(lcdRc.right - lcdRc.left));
    const int lh = (std::max)(1, static_cast<int>(lcdRc.bottom - lcdRc.top));

    HBRUSH lcdBg = CreateSolidBrush(RGB(0xE4, 0xEB, 0xE4));
    FillRect(hdc, &lcdRc, lcdBg);
    DeleteObject(lcdBg);

    // Top indicator strip (16% of LCD height)
    const int stripH = (std::max)(12, (lh * 16) / 100);
    RECT stripRc{ lcdRc.left + 4, lcdRc.top + 1, lcdRc.right - 4, lcdRc.top + stripH };
    const int nInd = static_cast<int>(sizeof(kCw2Indicators) / sizeof(kCw2Indicators[0]));
    const int fontPx = (std::max)(9, stripH - 3);
    HFONT hIndFont = CreateFontW(-fontPx, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldF = SelectObject(hdc, hIndFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0x14, 0x24, 0x3A));

    const int slotW = (std::max)(1, static_cast<int>(stripRc.right - stripRc.left) / nInd);
    for (int i = 0; i < nInd; ++i) {
        if ((g_cw2.indicators & (1u << kCw2Indicators[i].bit)) == 0u) continue;
        RECT r{ stripRc.left + i * slotW, stripRc.top, stripRc.left + (i + 1) * slotW + 8, stripRc.bottom };
        DrawTextW(hdc, kCw2Indicators[i].text, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, oldF);
    DeleteObject(hIndFont);

    // Subtle hairline separator under indicators
    HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(0xC2, 0xCE, 0xC4));
    HGDIOBJ oldP = SelectObject(hdc, sepPen);
    MoveToEx(hdc, lcdRc.left + 3, lcdRc.top + stripH, nullptr);
    LineTo(hdc, lcdRc.right - 3, lcdRc.top + stripH);
    SelectObject(hdc, oldP);
    DeleteObject(sepPen);

    // 192x63 4-level grayscale dot-matrix
    if (!g_cw2.lcdBgra.empty() && g_cw2.lcdW > 0 && g_cw2.lcdH > 0) {
        const int padX = (std::max)(2, lw / 48);
        const int dstX = lcdRc.left + padX;
        const int dstY = lcdRc.top + stripH + 2;
        const int dstW = (std::max)(1, lw - 2 * padX);
        const int dstH = (std::max)(1, lh - stripH - 4);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = g_cw2.lcdW;
        bmi.bmiHeader.biHeight = -g_cw2.lcdH; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(hdc, COLORONCOLOR);
        StretchDIBits(
            hdc,
            dstX, dstY, dstW, dstH,
            0, 0, g_cw2.lcdW, g_cw2.lcdH,
            g_cw2.lcdBgra.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
    }
}

void PaintCw2FullIntoDc(HDC hdc, int w, int h)
{
    RECT fullRc{ 0, 0, w, h };
    HBRUSH bgBr = CreateSolidBrush(RGB(0xF5, 0xF5, 0xF7));
    FillRect(hdc, &fullRc, bgBr);
    DeleteObject(bgBr);

    if (g_cw2.skinBmp && g_cw2.skinBmp->GetLastStatus() == Gdiplus::Ok) {
        Gdiplus::Graphics gr(hdc);
        gr.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        gr.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        gr.DrawImage(g_cw2.skinBmp.get(), Gdiplus::Rect(0, 0, w, h));
    } else {
        // Minimal clean fallback casing when PNG is absent
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(0x40, 0x44, 0x4D));
        HBRUSH br = CreateSolidBrush(RGB(0x28, 0x2C, 0x34));
        HGDIOBJ op = SelectObject(hdc, pen);
        HGDIOBJ ob = SelectObject(hdc, br);
        RoundRect(hdc, 2, 2, w - 2, h - 2, 24, 24);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(pen);
        DeleteObject(br);
    }

    RECT lcdRc{
        MulDiv(w, kCw2LcdLeft, kCw2SkinW),
        MulDiv(h, kCw2LcdTop, kCw2SkinH),
        MulDiv(w, kCw2LcdRight, kCw2SkinW),
        MulDiv(h, kCw2LcdBottom, kCw2SkinH)
    };
    PaintCw2LcdIntoRect(hdc, lcdRc);

    if (g_cw2.pressedKeyIdx >= 0 && g_cw2.pressedKeyIdx < NUM_CW2_KEYS) {
        const auto& k = g_cw2ExactKeys[g_cw2.pressedKeyIdx];
        const int x0 = MulDiv(w, k.cx - k.w / 2, kCw2SkinW);
        const int y0 = MulDiv(h, k.cy - k.h / 2, kCw2SkinH);
        const int x1 = MulDiv(w, k.cx + k.w / 2, kCw2SkinW);
        const int y1 = MulDiv(h, k.cy + k.h / 2, kCw2SkinH);
        Gdiplus::Graphics gr(hdc);
        gr.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush pressBr(Gdiplus::Color(55, 0, 113, 227));
        Gdiplus::Pen pressPen(Gdiplus::Color(180, 0, 113, 227), 2.0f);
        gr.FillEllipse(&pressBr, x0, y0, (std::max)(4, x1 - x0), (std::max)(4, y1 - y0));
        gr.DrawEllipse(&pressPen, x0, y0, (std::max)(4, x1 - x0), (std::max)(4, y1 - y0));
    }
}

LRESULT CALLBACK Cw2NativeHostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TIMER:
        if (wParam == kCw2TimerId && g_cw2.core) {
            casio_cw2_core_advance(g_cw2.core, 33);
            if (casio_cw2_core_take_redraw_flag(g_cw2.core)) {
                RefreshLcdPixels();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        break;
    case WM_LBUTTONDOWN: {
        SetFocus(hWnd);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        const int idx = HitTestCw2ClientPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), rc.right - rc.left, rc.bottom - rc.top);
        if (idx >= 0 && g_cw2.core) {
            g_cw2.pressedKeyIdx = idx;
            g_cw2.mouseCaptured = true;
            SetCapture(hWnd);
            const std::uint8_t code = static_cast<std::uint8_t>(g_cw2ExactKeys[idx].matrixCode & 0xFFu);
            if (code == 0xFFu) {
                casio_cw2_core_power_on(g_cw2.core);
            } else {
                casio_cw2_core_key_down(g_cw2.core, code);
            }
            casio_cw2_core_advance(g_cw2.core, 45);
            RefreshLcdPixels();
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_cw2.mouseCaptured) {
            ReleaseCapture();
            g_cw2.mouseCaptured = false;
        }
        if (g_cw2.pressedKeyIdx >= 0 && g_cw2.core) {
            casio_cw2_core_key_up(g_cw2.core);
            casio_cw2_core_advance(g_cw2.core, 55);
            g_cw2.pressedKeyIdx = -1;
            RefreshLcdPixels();
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (g_cw2.mouseCaptured && g_cw2.core) {
            g_cw2.mouseCaptured = false;
            g_cw2.pressedKeyIdx = -1;
            casio_cw2_core_key_up(g_cw2.core);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if (Fx991CwHost::ForwardKey(wParam, true)) return 0;
        break;
    case WM_KEYUP:
        if (Fx991CwHost::ForwardKey(wParam, false)) return 0;
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdcPaint = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
        const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
        HDC hdcMem = CreateCompatibleDC(hdcPaint);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcPaint, w, h);
        HGDIOBJ old = SelectObject(hdcMem, hBmp);
        PaintCw2FullIntoDc(hdcMem, w, h);
        BitBlt(hdcPaint, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, old);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool StartNativeCw2Internal(HWND hParent, const RECT& viewportRect, bool visible, bool resetRam)
{
    g_cw2.paths = ResolveCw2Assets();
    if (!g_cw2.paths.valid) {
        g_cw2.state = Fx991CwState::NotFound;
        g_cw2.lastError = L"No se encontr\u00F3 assets/roms/fx991cw_rom.bin para el motor nativo fx-991CW v47.";
        return false;
    }

    std::vector<std::uint8_t> romBytes, ramBytes;
    if (!ReadFileBytes(g_cw2.paths.romPath, romBytes)) {
        g_cw2.state = Fx991CwState::Failed;
        g_cw2.lastError = L"Error leyendo fx991cw_rom.bin.";
        return false;
    }
    if (!resetRam && !g_cw2.paths.liveRamPath.empty()) {
        (void)ReadFileBytes(g_cw2.paths.liveRamPath, ramBytes);
    }
    if (ramBytes.empty() && !g_cw2.paths.defaultRamPath.empty()) {
        (void)ReadFileBytes(g_cw2.paths.defaultRamPath, ramBytes);
    }

    if (g_cw2.core) {
        casio_cw2_core_destroy(g_cw2.core);
        g_cw2.core = nullptr;
    }
    g_cw2.core = casio_cw2_core_create(
        romBytes.data(), romBytes.size(),
        ramBytes.empty() ? nullptr : ramBytes.data(), ramBytes.size());
    if (!g_cw2.core) {
        g_cw2.state = Fx991CwState::Failed;
        g_cw2.lastError = L"Fallo al inicializar core/casio_cw2 v47.";
        return false;
    }

    casio_cw2_core_power_on(g_cw2.core);
    casio_cw2_core_advance(g_cw2.core, 420);
    RefreshLcdPixels();

    if (!g_cw2.skinBmp && !g_cw2.paths.skinPath.empty()) {
        auto bmp = std::make_unique<Gdiplus::Bitmap>(g_cw2.paths.skinPath.c_str());
        if (bmp && bmp->GetLastStatus() == Gdiplus::Ok) {
            g_cw2.skinBmp = std::move(bmp);
        }
    }

    if (hParent && IsWindow(hParent)) {
        static bool s_reg = false;
        if (!s_reg) {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = Cw2NativeHostWndProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
            wc.lpszClassName = L"CasioCw2NativeHostWnd";
            RegisterClassExW(&wc);
            s_reg = true;
        }
        if (!g_cw2.hChild || !IsWindow(g_cw2.hChild)) {
            const int w = (std::max)(1, static_cast<int>(viewportRect.right - viewportRect.left));
            const int h = (std::max)(1, static_cast<int>(viewportRect.bottom - viewportRect.top));
            g_cw2.hChild = CreateWindowExW(
                0,
                L"CasioCw2NativeHostWnd",
                L"fx-991CW Nativo",
                WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | (visible ? WS_VISIBLE : 0),
                viewportRect.left, viewportRect.top, w, h,
                hParent, nullptr, GetModuleHandleW(nullptr), nullptr);
        }
        if (g_cw2.hChild) {
            SetTimer(g_cw2.hChild, kCw2TimerId, 33, nullptr);
            Fx991CwHost::SyncLayout(hParent, viewportRect, visible);
        }
    }

    g_cw2.state = Fx991CwState::Ready;
    g_cw2.lastError.clear();
    return true;
}

} // namespace

bool Fx991CwHost::IsRuntimeAvailable()
{
    return ResolveCw2Assets().valid;
}

Fx991CwState Fx991CwHost::GetState()
{
    return g_cw2.state;
}

std::wstring Fx991CwHost::GetLastError()
{
    return g_cw2.lastError;
}

HWND Fx991CwHost::GetChildHwnd()
{
    return (g_cw2.hChild && IsWindow(g_cw2.hChild)) ? g_cw2.hChild : nullptr;
}

bool Fx991CwHost::EnsureLaunched(HWND hParent, const RECT& viewportRect, bool visible)
{
    if (g_cw2.state == Fx991CwState::Ready && g_cw2.core) {
        SyncLayout(hParent, viewportRect, visible);
        return true;
    }
    return StartNativeCw2Internal(hParent, viewportRect, visible, false);
}

void Fx991CwHost::PollAttach(HWND hParent, const RECT& viewportRect, bool visible)
{
    if (visible && g_cw2.state != Fx991CwState::Ready && g_cw2.state != Fx991CwState::Failed) {
        (void)EnsureLaunched(hParent, viewportRect, visible);
        return;
    }
    if (g_cw2.core && g_cw2.hChild && IsWindow(g_cw2.hChild)) {
        SyncLayout(hParent, viewportRect, visible);
    }
}

void Fx991CwHost::SyncLayout(HWND hParent, const RECT& viewportRect, bool visible)
{
    if (!g_cw2.hChild || !IsWindow(g_cw2.hChild)) {
        if (visible && g_cw2.core && hParent && IsWindow(hParent)) {
            (void)StartNativeCw2Internal(hParent, viewportRect, visible, false);
        }
        return;
    }
    const int w = (std::max)(1, static_cast<int>(viewportRect.right - viewportRect.left));
    const int h = (std::max)(1, static_cast<int>(viewportRect.bottom - viewportRect.top));
    const bool rectChanged = !g_cw2.hasAppliedRect ||
        g_cw2.lastRect.left != viewportRect.left ||
        g_cw2.lastRect.top != viewportRect.top ||
        g_cw2.lastRect.right != viewportRect.right ||
        g_cw2.lastRect.bottom != viewportRect.bottom;
    const bool visChanged = !g_cw2.hasAppliedRect || g_cw2.lastVisible != visible;

    if (rectChanged) {
        MoveWindow(g_cw2.hChild, viewportRect.left, viewportRect.top, w, h, visible ? TRUE : FALSE);
        g_cw2.lastRect = viewportRect;
    }
    if (visChanged) {
        ShowWindow(g_cw2.hChild, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        g_cw2.lastVisible = visible;
    }
    g_cw2.hasAppliedRect = true;
}

bool Fx991CwHost::ForwardKey(WPARAM vk, bool down)
{
    if (!g_cw2.core) return false;
    std::uint8_t code = 0;
    if (!TryMapVkToCw2(vk, code)) return false;
    if (down) {
        casio_cw2_core_key_down(g_cw2.core, code);
        casio_cw2_core_advance(g_cw2.core, 45);
    } else {
        casio_cw2_core_key_up(g_cw2.core);
        casio_cw2_core_advance(g_cw2.core, 55);
    }
    RefreshLcdPixels();
    if (g_cw2.hChild && IsWindow(g_cw2.hChild)) {
        InvalidateRect(g_cw2.hChild, nullptr, FALSE);
    }
    return true;
}

bool Fx991CwHost::ClickNormalized(double normX, double normY)
{
    if (!g_cw2.core || !std::isfinite(normX) || !std::isfinite(normY)) return false;
    const double cx = std::clamp(normX, 0.0, 1.0) * kCw2SkinW;
    const double cy = std::clamp(normY, 0.0, 1.0) * kCw2SkinH;
    const int idx = HitTestCw2ClientPoint(
        static_cast<int>(std::lround(cx)),
        static_cast<int>(std::lround(cy)),
        kCw2SkinW, kCw2SkinH);
    if (idx < 0) return false;

    const std::uint8_t code = static_cast<std::uint8_t>(g_cw2ExactKeys[idx].matrixCode & 0xFFu);
    if (code == 0xFFu) {
        casio_cw2_core_power_on(g_cw2.core);
        casio_cw2_core_advance(g_cw2.core, 120);
    } else {
        casio_cw2_core_tap_key(g_cw2.core, code, 60, 80);
    }
    RefreshLcdPixels();
    if (g_cw2.hChild && IsWindow(g_cw2.hChild)) {
        InvalidateRect(g_cw2.hChild, nullptr, FALSE);
    }
    return true;
}

bool Fx991CwHost::IsChildFocused()
{
    HWND hFocus = GetFocus();
    return hFocus != nullptr && hFocus == GetChildHwnd();
}

HBITMAP Fx991CwHost::CaptureBitmap(bool lcdOnly, int& outW, int& outH)
{
    outW = 0;
    outH = 0;
    if (!g_cw2.core) return nullptr;
    RefreshLcdPixels();

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) return nullptr;

    int w = 0, h = 0;
    if (lcdOnly) {
        w = 384;
        h = 152;
    } else if (g_cw2.hChild && IsWindow(g_cw2.hChild)) {
        RECT rc{};
        GetClientRect(g_cw2.hChild, &rc);
        w = (std::max)(240, static_cast<int>(rc.right - rc.left));
        h = (std::max)(480, static_cast<int>(rc.bottom - rc.top));
    } else {
        w = 496;
        h = 1041;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, w, h);
    if (!hdcMem || !hBmp) {
        if (hdcMem) DeleteDC(hdcMem);
        if (hBmp) DeleteObject(hBmp);
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HGDIOBJ old = SelectObject(hdcMem, hBmp);
    if (lcdOnly) {
        RECT rc{ 0, 0, w, h };
        PaintCw2LcdIntoRect(hdcMem, rc);
    } else {
        PaintCw2FullIntoDc(hdcMem, w, h);
    }
    SelectObject(hdcMem, old);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    outW = w;
    outH = h;
    return hBmp;
}

bool Fx991CwHost::Restart(HWND hParent, const RECT& viewportRect, bool visible, bool resetRam)
{
    if (!resetRam) {
        PersistLiveRam();
    } else if (!g_cw2.paths.liveRamPath.empty()) {
        std::error_code ec;
        fs::remove(g_cw2.paths.liveRamPath, ec);
    }
    return StartNativeCw2Internal(hParent, viewportRect, visible, resetRam);
}

void Fx991CwHost::Shutdown()
{
    PersistLiveRam();
    if (g_cw2.hChild && IsWindow(g_cw2.hChild)) {
        KillTimer(g_cw2.hChild, kCw2TimerId);
        DestroyWindow(g_cw2.hChild);
    }
    g_cw2.hChild = nullptr;
    if (g_cw2.core) {
        casio_cw2_core_destroy(g_cw2.core);
        g_cw2.core = nullptr;
    }
    g_cw2.skinBmp.reset();
    g_cw2.grayBuf.clear();
    g_cw2.lcdBgra.clear();
    g_cw2.pressedKeyIdx = -1;
    g_cw2.mouseCaptured = false;
    g_cw2.hasAppliedRect = false;
    g_cw2.lastVisible = false;
    g_cw2.state = IsRuntimeAvailable() ? Fx991CwState::Stopped : Fx991CwState::NotFound;
}
