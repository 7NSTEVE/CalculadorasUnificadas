#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

// Self-authored pixel-exact LCD status indicator renderer.
// Renders original 1-bit pixel glyph patterns on native integer grids:
//   - ClassWiz (calcIdx == 0): 192 x 8 px native strip (scaled 2x -> 384 x 16 px)
//   - ES / VN PLUS (calcIdx == 1): 288 x 12 px native strip (scaled 1x -> 288 x 12 px)
// Paired with nX-U8 0xF800 hardware indicator register offsets.
namespace CasioLcdIndicators {

struct SpriteSpec {
    int rawByteOffset;
    unsigned char bitMask;
    int srcX;
    int srcW;
};

struct GlyphPattern {
    int rows;
    const char* const* lines;
};

static constexpr int kCwStripW = 192;
static constexpr int kCwStripH = 8;
static constexpr int kEsStripW = 288;
static constexpr int kEsStripH = 12;

// 20 ClassWiz indicators (0xF800..0xF816, hw_id == 4) on exact 192 x 8 native grid.
static constexpr SpriteSpec kClassWizSprites[20] = {
    { 0x00, 0x01,   1,  8 }, //  0: S (SHIFT)
    { 0x01, 0x01,  11,  8 }, //  1: A (ALPHA)
    { 0x02, 0x01,  21,  5 }, //  2: M (Memory)
    { 0x03, 0x01,  28, 11 }, //  3: STO
    { 0x05, 0x01,  41, 13 }, //  4: Math (sqrt/box)
    { 0x06, 0x01,  57,  8 }, //  5: D (Degrees)
    { 0x07, 0x01,  67,  8 }, //  6: R (Radians)
    { 0x08, 0x01,  77,  8 }, //  7: G (Gradians)
    { 0x09, 0x01,  87, 11 }, //  8: FIX
    { 0x0A, 0x01, 100, 11 }, //  9: SCI
    { 0x0B, 0x01, 113,  4 }, // 10: E (Engineering / Sci exp)
    { 0x0C, 0x01, 119,  3 }, // 11: i (Complex)
    { 0x0D, 0x01, 124,  6 }, // 12: Angle symbol
    { 0x0F, 0x01, 132,  7 }, // 13: Down indicator
    { 0x10, 0x01, 142,  5 }, // 14: Left scroll arrow
    { 0x11, 0x01, 149,  7 }, // 15: Down scroll arrow
    { 0x12, 0x01, 158,  7 }, // 16: Up scroll arrow
    { 0x13, 0x01, 167,  5 }, // 17: Right scroll arrow
    { 0x15, 0x01, 174,  7 }, // 18: Pause
    { 0x16, 0x01, 183,  7 }, // 19: Backlight / solar indicator
};

// 18 Natural ES / VN PLUS indicators (0xF800..0xF80B, hw_id == 3) on exact 288 x 12 native grid.
static constexpr SpriteSpec kEsPlusSprites[18] = {
    { 0x00, 0x10,   2,  9 }, //  0: S (SHIFT)
    { 0x00, 0x04,  13,  9 }, //  1: A (ALPHA)
    { 0x01, 0x10,  24,  7 }, //  2: M (Memory)
    { 0x01, 0x02,  33, 14 }, //  3: STO
    { 0x02, 0x40,  49, 14 }, //  4: RCL
    { 0x03, 0x40,  66, 20 }, //  5: STAT
    { 0x04, 0x80,  89, 25 }, //  6: CMPLX
    { 0x05, 0x40, 117, 17 }, //  7: MAT
    { 0x05, 0x01, 137, 15 }, //  8: VCT
    { 0x07, 0x20, 156,  9 }, //  9: D (Degrees)
    { 0x07, 0x02, 167,  9 }, // 10: R (Radians)
    { 0x08, 0x10, 178,  9 }, // 11: G (Gradians)
    { 0x08, 0x01, 190, 14 }, // 12: FIX
    { 0x09, 0x20, 207, 14 }, // 13: SCI
    { 0x0A, 0x40, 224, 20 }, // 14: Math
    { 0x0A, 0x08, 247,  7 }, // 15: Down scroll arrow
    { 0x0B, 0x80, 256,  7 }, // 16: Up scroll arrow
    { 0x0B, 0x10, 266, 19 }, // 17: Disp
};

namespace detail {

// --- ClassWiz 192 x 8 pixel glyphs ---
static const char* const kCwGlyph_S[8] = {
    ".######.",
    "###...##",
    "##.#####",
    "###..###",
    "#####.##",
    "##...###",
    ".######.",
    "........"
};
static const char* const kCwGlyph_A[8] = {
    ".######.",
    "###..###",
    "##.##.##",
    "##....##",
    "##.##.##",
    "##.##.##",
    ".######.",
    "........"
};
static const char* const kCwGlyph_M[8] = {
    ".....",
    "#...#",
    "##.##",
    "#.#.#",
    "#...#",
    "#...#",
    ".....",
    "....."
};
static const char* const kCwGlyph_STO[8] = {
    "...........",
    ".##.###.###",
    "#....#..#.#",
    ".#...#..#.#",
    "..#..#..#.#",
    "##...#..###",
    "...........",
    "..........."
};
static const char* const kCwGlyph_Math[8] = {
    ".....########",
    "....#........",
    "....#..#####.",
    "...#...#...#.",
    "#..#...#...#.",
    ".#.#...#...#.",
    "..#....#####.",
    "............."
};
static const char* const kCwGlyph_D[8] = {
    ".######.",
    "##...###",
    "##.##.##",
    "##.##.##",
    "##.##.##",
    "##...###",
    ".######.",
    "........"
};
static const char* const kCwGlyph_R[8] = {
    ".######.",
    "##...###",
    "##.##.##",
    "##...###",
    "##.#.###",
    "##.##.##",
    ".######.",
    "........"
};
static const char* const kCwGlyph_G[8] = {
    ".######.",
    "###..###",
    "##.#####",
    "##.##.##",
    "##.##.##",
    "###..###",
    ".######.",
    "........"
};
static const char* const kCwGlyph_FIX[8] = {
    "...........",
    "###.###.#.#",
    "#....#..#.#",
    "##...#...#.",
    "#....#..#.#",
    "#...###.#.#",
    "...........",
    "..........."
};
static const char* const kCwGlyph_SCI[8] = {
    "...........",
    ".##..##.###",
    "#...#....#.",
    ".#..#....#.",
    "..#.#....#.",
    "##...##.###",
    "...........",
    "..........."
};
static const char* const kCwGlyph_E[8] = {
    "....",
    "####",
    "#...",
    "###.",
    "#...",
    "####",
    "....",
    "...."
};
static const char* const kCwGlyph_i[8] = {
    ".#.",
    "...",
    ".#.",
    ".#.",
    ".#.",
    ".#.",
    "...",
    "..."
};
static const char* const kCwGlyph_Angle[8] = {
    "......",
    "....#.",
    "...#..",
    "..#.#.",
    ".#..#.",
    "######",
    "......",
    "......"
};
static const char* const kCwGlyph_DownInd[8] = {
    ".......",
    "#######",
    ".#####.",
    "..###..",
    "...#...",
    ".......",
    ".......",
    "......."
};
static const char* const kCwGlyph_Left[8] = {
    ".....",
    "...#.",
    "..##.",
    ".###.",
    "..##.",
    "...#.",
    ".....",
    "....."
};
static const char* const kCwGlyph_Down[8] = {
    ".......",
    "#######",
    ".#####.",
    "..###..",
    "...#...",
    ".......",
    ".......",
    "......."
};
static const char* const kCwGlyph_Up[8] = {
    ".......",
    "...#...",
    "..###..",
    ".#####.",
    "#######",
    ".......",
    ".......",
    "......."
};
static const char* const kCwGlyph_Right[8] = {
    ".....",
    ".#...",
    ".##..",
    ".###.",
    ".##..",
    ".#...",
    ".....",
    "....."
};
static const char* const kCwGlyph_Pause[8] = {
    ".......",
    ".##.##.",
    ".##.##.",
    ".##.##.",
    ".##.##.",
    ".##.##.",
    ".......",
    "......."
};
static const char* const kCwGlyph_Sun[8] = {
    "...#...",
    "#..#..#",
    "..###..",
    "##.#.##",
    "..###..",
    "#..#..#",
    "...#...",
    "......."
};

static const char* const* const kCwGlyphPtrs[20] = {
    kCwGlyph_S, kCwGlyph_A, kCwGlyph_M, kCwGlyph_STO, kCwGlyph_Math,
    kCwGlyph_D, kCwGlyph_R, kCwGlyph_G, kCwGlyph_FIX, kCwGlyph_SCI,
    kCwGlyph_E, kCwGlyph_i, kCwGlyph_Angle, kCwGlyph_DownInd, kCwGlyph_Left,
    kCwGlyph_Down, kCwGlyph_Up, kCwGlyph_Right, kCwGlyph_Pause, kCwGlyph_Sun
};

// --- Natural ES / VN PLUS 288 x 12 pixel glyphs ---
static const char* const kEsGlyph_S[12] = {
    ".#######.",
    "#########",
    "###....##",
    "##.######",
    "##.######",
    "###...###",
    "######.##",
    "######.##",
    "##....###",
    "#########",
    ".#######.",
    "........."
};
static const char* const kEsGlyph_A[12] = {
    ".#######.",
    "#########",
    "###...###",
    "##.###.##",
    "##.###.##",
    "##.....##",
    "##.###.##",
    "##.###.##",
    "##.###.##",
    "#########",
    ".#######.",
    "........."
};
static const char* const kEsGlyph_M[12] = {
    ".......",
    ".......",
    "#.....#",
    "##...##",
    "#.#.#.#",
    "#..#..#",
    "#.....#",
    "#.....#",
    "#.....#",
    ".......",
    ".......",
    "......."
};
static const char* const kEsGlyph_STO[12] = {
    "..............",
    "..............",
    ".###.####..##.",
    "#.....#...#..#",
    ".##...#...#..#",
    "...#..#...#..#",
    "...#..#...#..#",
    "###...#....##.",
    "..............",
    "..............",
    "..............",
    ".............."
};
static const char* const kEsGlyph_RCL[12] = {
    "..............",
    "..............",
    "###...##..#...",
    "#..#.#..#.#...",
    "###..#....#...",
    "#.#..#....#...",
    "#..#.#..#.#...",
    "#..#..##..####",
    "..............",
    "..............",
    "..............",
    ".............."
};
static const char* const kEsGlyph_STAT[12] = {
    "....................",
    "....................",
    ".###.#####..##.#####",
    "#......#...#..#..#..",
    ".##....#...####..#..",
    "...#...#...#..#..#..",
    "###....#...#..#..#..",
    "....................",
    "....................",
    "....................",
    "....................",
    "...................."
};
static const char* const kEsGlyph_CMPLX[12] = {
    ".........................",
    ".........................",
    ".###.#...#.###..#...#...#",
    "#....##.##.#..#.#....#.#.",
    "#....#.#.#.###..#.....#..",
    "#....#...#.#....#....#.#.",
    ".###.#...#.#....####.#..#",
    ".........................",
    ".........................",
    ".........................",
    ".........................",
    "........................."
};
static const char* const kEsGlyph_MAT[12] = {
    ".................",
    ".................",
    "#...#..###..#####",
    "##.##.#...#...#..",
    "#.#.#.#####...#..",
    "#...#.#...#...#..",
    "#...#.#...#...#..",
    ".................",
    ".................",
    ".................",
    ".................",
    "................."
};
static const char* const kEsGlyph_VCT[12] = {
    "...............",
    "...............",
    "#...#..##.#####",
    "#...#.#.....#..",
    "#...#.#.....#..",
    ".#.#..#.....#..",
    "..#....##...#..",
    "...............",
    "...............",
    "...............",
    "...............",
    "..............."
};
static const char* const kEsGlyph_D[12] = {
    ".#######.",
    "#########",
    "##....###",
    "##.###.##",
    "##.###.##",
    "##.###.##",
    "##.###.##",
    "##.###.##",
    "##....###",
    "#########",
    ".#######.",
    "........."
};
static const char* const kEsGlyph_R[12] = {
    ".#######.",
    "#########",
    "##....###",
    "##.###.##",
    "##.###.##",
    "##....###",
    "##.##.###",
    "##.###.##",
    "##.###.##",
    "#########",
    ".#######.",
    "........."
};
static const char* const kEsGlyph_G[12] = {
    ".#######.",
    "#########",
    "###...###",
    "##.######",
    "##.######",
    "##.##..##",
    "##.###.##",
    "##.###.##",
    "###...###",
    "#########",
    ".#######.",
    "........."
};
static const char* const kEsGlyph_FIX[12] = {
    "..............",
    "..............",
    "####.####.#..#",
    "#.....#...#..#",
    "###...#....##.",
    "#.....#...#..#",
    "#....####.#..#",
    "..............",
    "..............",
    "..............",
    "..............",
    ".............."
};
static const char* const kEsGlyph_SCI[12] = {
    "..............",
    "..............",
    ".###..###.####",
    "#....#.....#..",
    ".##..#.....#..",
    "...#.#.....#..",
    "###...###.####",
    "..............",
    "..............",
    "..............",
    "..............",
    ".............."
};
static const char* const kEsGlyph_Math[12] = {
    "....................",
    "....................",
    "#...#..........#....",
    "##.##.......#..#....",
    "#.#.#..##..###.###..",
    "#...#.#..#..#..#..#.",
    "#...#.#..#..#..#..#.",
    "#...#..##.#.#..#..#.",
    "....................",
    "....................",
    "....................",
    "...................."
};
static const char* const kEsGlyph_Down[12] = {
    ".......",
    ".......",
    "#######",
    ".#####.",
    "..###..",
    "...#...",
    ".......",
    ".......",
    ".......",
    ".......",
    ".......",
    "......."
};
static const char* const kEsGlyph_Up[12] = {
    ".......",
    ".......",
    "...#...",
    "..###..",
    ".#####.",
    "#######",
    ".......",
    ".......",
    ".......",
    ".......",
    ".......",
    "......."
};
static const char* const kEsGlyph_Disp[12] = {
    "...................",
    "...................",
    "###...#............",
    "#..#......###..###.",
    "#..#..#..#.....#..#",
    "#..#..#...##...#..#",
    "#..#..#.....#..###.",
    "###...#..###...#...",
    "...............#...",
    "...................",
    "...................",
    "..................."
};

static const char* const* const kEsGlyphPtrs[18] = {
    kEsGlyph_S, kEsGlyph_A, kEsGlyph_M, kEsGlyph_STO, kEsGlyph_RCL, kEsGlyph_STAT,
    kEsGlyph_CMPLX, kEsGlyph_MAT, kEsGlyph_VCT, kEsGlyph_D, kEsGlyph_R, kEsGlyph_G,
    kEsGlyph_FIX, kEsGlyph_SCI, kEsGlyph_Math, kEsGlyph_Down, kEsGlyph_Up, kEsGlyph_Disp
};

} // namespace detail

inline int GetIndicatorCount(int calcIdx) {
    if (calcIdx == 0) return 20;
    if (calcIdx == 1) return 18;
    return 0;
}

inline int GetNativeStripWidth(int calcIdx) {
    return (calcIdx == 0) ? kCwStripW : ((calcIdx == 1) ? kEsStripW : 0);
}

inline int GetNativeStripHeight(int calcIdx) {
    return (calcIdx == 0) ? kCwStripH : ((calcIdx == 1) ? kEsStripH : 0);
}

inline const SpriteSpec* GetIndicatorSpec(int calcIdx, int spriteIndex) {
    if (calcIdx == 0 && spriteIndex >= 0 && spriteIndex < 20) return &kClassWizSprites[spriteIndex];
    if (calcIdx == 1 && spriteIndex >= 0 && spriteIndex < 18) return &kEsPlusSprites[spriteIndex];
    return nullptr;
}

inline bool GetIndicatorMaskPixel(int calcIdx, int spriteIndex, int localX, int localY) {
    const SpriteSpec* sp = GetIndicatorSpec(calcIdx, spriteIndex);
    if (!sp || localX < 0 || localX >= sp->srcW) return false;
    const int h = GetNativeStripHeight(calcIdx);
    if (localY < 0 || localY >= h) return false;
    const char* row = (calcIdx == 0)
        ? detail::kCwGlyphPtrs[spriteIndex][localY]
        : detail::kEsGlyphPtrs[spriteIndex][localY];
    if (!row) return false;
    const size_t rowLen = std::strlen(row);
    if (static_cast<size_t>(localX) >= rowLen) return false;
    return row[localX] == '#';
}

struct CachedStrip {
    bool     loaded = false;
    int      w = 0;
    int      h = 0;
    std::vector<unsigned char> maskAlpha; // 0 = background, 255 = foreground ink
    HBITMAP  colorBmp = nullptr;
    COLORREF cachedBg = CLR_INVALID;
    COLORREF cachedFg = CLR_INVALID;
    int      cachedInkScale = -1;
};

class Renderer {
public:
    ~Renderer() { Release(); }

    void Release() {
        for (auto& s : strips_) {
            if (s.colorBmp) {
                DeleteObject(s.colorBmp);
                s.colorBmp = nullptr;
            }
            s.maskAlpha.clear();
            s.loaded = false;
            s.cachedBg = CLR_INVALID;
            s.cachedFg = CLR_INVALID;
            s.cachedInkScale = -1;
        }
    }

    // Draws all active status indicators for `calcIdx` (0=ClassWiz, 1=ES/VN PLUS)
    // inside `indRect` using exact integer-aligned pixel glyphs (COLORONCOLOR).
    // `hInst` is optional (kept in signature for backwards API compatibility).
    bool Draw(HDC hdc, HINSTANCE hInst, int calcIdx,
              const unsigned char* rawBuf, size_t rawSize,
              const RECT& indRect, COLORREF bgColor, COLORREF fgColor,
              int inkScale255 = 255) {
        (void)hInst;
        if (calcIdx < 0 || calcIdx > 1 || !rawBuf || rawSize == 0) return false;
        const int dstW = indRect.right - indRect.left;
        const int dstH = indRect.bottom - indRect.top;
        if (dstW <= 0 || dstH <= 0) return false;

        CachedStrip& strip = strips_[calcIdx];
        if (!strip.loaded) {
            if (!BuildNativeStrip(calcIdx, strip)) {
                return false;
            }
        }
        if (!EnsureColorizedStrip(hdc, strip, bgColor, fgColor, inkScale255)) {
            return false;
        }

        const SpriteSpec* specs = (calcIdx == 0) ? kClassWizSprites : kEsPlusSprites;
        const int count = GetIndicatorCount(calcIdx);

        HDC memDC = CreateCompatibleDC(hdc);
        if (!memDC) return false;
        HGDIOBJ oldBmp = SelectObject(memDC, strip.colorBmp);

        const int prevMode = SetStretchBltMode(hdc, COLORONCOLOR);

        for (int i = 0; i < count; ++i) {
            const SpriteSpec& sp = specs[i];
            if (static_cast<size_t>(sp.rawByteOffset) >= rawSize) continue;
            if ((rawBuf[sp.rawByteOffset] & sp.bitMask) == 0) continue;

            const int dx0 = indRect.left + MulDiv(sp.srcX, dstW, strip.w);
            const int dx1 = indRect.left + MulDiv(sp.srcX + sp.srcW, dstW, strip.w);
            const int dw = (std::max)(1, dx1 - dx0);
            StretchBlt(
                hdc, dx0, indRect.top, dw, dstH,
                memDC, sp.srcX, 0, sp.srcW, strip.h,
                SRCCOPY
            );
        }

        SetStretchBltMode(hdc, prevMode);
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        return true;
    }

private:
    CachedStrip strips_[2]{};

    static bool BuildNativeStrip(int calcIdx, CachedStrip& out) {
        const int w = GetNativeStripWidth(calcIdx);
        const int h = GetNativeStripHeight(calcIdx);
        const int count = GetIndicatorCount(calcIdx);
        if (w <= 0 || h <= 0 || count <= 0) return false;

        out.w = w;
        out.h = h;
        out.maskAlpha.assign(static_cast<size_t>(w) * h, 0);

        for (int i = 0; i < count; ++i) {
            const SpriteSpec* sp = GetIndicatorSpec(calcIdx, i);
            if (!sp) continue;
            for (int ly = 0; ly < h; ++ly) {
                for (int lx = 0; lx < sp->srcW; ++lx) {
                    const int gx = sp->srcX + lx;
                    if (gx < 0 || gx >= w) continue;
                    if (GetIndicatorMaskPixel(calcIdx, i, lx, ly)) {
                        out.maskAlpha[static_cast<size_t>(ly) * w + gx] = 255;
                    }
                }
            }
        }
        out.loaded = true;
        return true;
    }

    static bool EnsureColorizedStrip(HDC refHdc, CachedStrip& strip, COLORREF bgColor, COLORREF fgColor, int inkScale255) {
        inkScale255 = (std::max)(0, (std::min)(255, inkScale255));
        if (strip.colorBmp && strip.cachedBg == bgColor && strip.cachedFg == fgColor && strip.cachedInkScale == inkScale255) {
            return true;
        }
        if (strip.colorBmp) {
            DeleteObject(strip.colorBmp);
            strip.colorBmp = nullptr;
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = strip.w;
        bmi.bmiHeader.biHeight = -strip.h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hBmp = CreateDIBSection(refHdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (!hBmp || !pBits) {
            if (hBmp) DeleteObject(hBmp);
            return false;
        }

        const int fgR = GetRValue(fgColor), fgG = GetGValue(fgColor), fgB = GetBValue(fgColor);
        const int bgR = GetRValue(bgColor), bgG = GetGValue(bgColor), bgB = GetBValue(bgColor);

        std::uint32_t* dst = static_cast<std::uint32_t*>(pBits);
        const size_t total = static_cast<size_t>(strip.w) * strip.h;
        for (size_t i = 0; i < total; ++i) {
            const int a = (static_cast<int>(strip.maskAlpha[i]) * inkScale255) / 255;
            const int r = (bgR * (255 - a) + fgR * a) / 255;
            const int g = (bgG * (255 - a) + fgG * a) / 255;
            const int b = (bgB * (255 - a) + fgB * a) / 255;
            dst[i] = static_cast<std::uint32_t>((r << 16) | (g << 8) | b);
        }

        strip.colorBmp = hBmp;
        strip.cachedBg = bgColor;
        strip.cachedFg = fgColor;
        strip.cachedInkScale = inkScale255;
        return true;
    }
};

inline Renderer& GlobalRenderer() {
    static Renderer s_renderer;
    return s_renderer;
}

} // namespace CasioLcdIndicators
