#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace CasioLinux::Indicators {

struct SpriteSpec {
    int rawByteOffset;
    unsigned char bitMask;
    int srcX;
    int srcW;
};

inline constexpr int kCwStripW = 192;
inline constexpr int kCwStripH = 8;
inline constexpr int kEsStripW = 288;
inline constexpr int kEsStripH = 12;

// 20 ClassWiz indicators (0xF800..0xF816, hw_id == 4) on exact 192 x 8 native grid.
inline constexpr SpriteSpec kClassWizSprites[20] = {
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
inline constexpr SpriteSpec kEsPlusSprites[18] = {
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

// Bitmask bits 0..11 -> sprite indices
inline constexpr int kCwBitToSprite[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16 };
inline constexpr int kEsBitToSprite[12] = { 0, 1, 2, 3, 14, 9, 10, 11, 12, 13, 15, 16 };

namespace detail {

inline const char* const kCwGlyph_S[8] = {
    ".######.",
    "###...##",
    "##.#####",
    "###..###",
    "#####.##",
    "##...###",
    ".######.",
    "........"
};
inline const char* const kCwGlyph_A[8] = {
    ".######.",
    "###..###",
    "##.##.##",
    "##....##",
    "##.##.##",
    "##.##.##",
    ".######.",
    "........"
};
inline const char* const kCwGlyph_M[8] = {
    ".....",
    "#...#",
    "##.##",
    "#.#.#",
    "#...#",
    "#...#",
    ".....",
    "....."
};
inline const char* const kCwGlyph_STO[8] = {
    "...........",
    ".##.###.###",
    "#....#..#.#",
    ".#...#..#.#",
    "..#..#..#.#",
    "##...#..###",
    "...........",
    "..........."
};
inline const char* const kCwGlyph_Math[8] = {
    ".....########",
    "....#........",
    "....#..#####.",
    "...#...#...#.",
    "#..#...#...#.",
    ".#.#...#...#.",
    "..#....#####.",
    "............."
};
inline const char* const kCwGlyph_D[8] = {
    ".######.",
    "##...###",
    "##.##.##",
    "##.##.##",
    "##.##.##",
    "##...###",
    ".######.",
    "........"
};
inline const char* const kCwGlyph_R[8] = {
    ".######.",
    "##...###",
    "##.##.##",
    "##...###",
    "##.#.###",
    "##.##.##",
    ".######.",
    "........"
};
inline const char* const kCwGlyph_G[8] = {
    ".######.",
    "###..###",
    "##.#####",
    "##.##.##",
    "##.##.##",
    "###..###",
    ".######.",
    "........"
};
inline const char* const kCwGlyph_FIX[8] = {
    "...........",
    "###.###.#.#",
    "#....#..#.#",
    "##...#...#.",
    "#....#..#.#",
    "#...###.#.#",
    "...........",
    "..........."
};
inline const char* const kCwGlyph_SCI[8] = {
    "...........",
    ".##..##.###",
    "#...#....#.",
    ".#..#....#.",
    "..#.#....#.",
    "##...##.###",
    "...........",
    "..........."
};
inline const char* const kCwGlyph_E[8] = {
    "....",
    "####",
    "#...",
    "###.",
    "#...",
    "####",
    "....",
    "...."
};
inline const char* const kCwGlyph_i[8] = {
    ".#.",
    "...",
    ".#.",
    ".#.",
    ".#.",
    ".#.",
    "...",
    "..."
};
inline const char* const kCwGlyph_Angle[8] = {
    "......",
    "....#.",
    "...#..",
    "..#.#.",
    ".#..#.",
    "######",
    "......",
    "......"
};
inline const char* const kCwGlyph_DownInd[8] = {
    ".......",
    "#######",
    ".#####.",
    "..###..",
    "...#...",
    ".......",
    ".......",
    "......."
};
inline const char* const kCwGlyph_Left[8] = {
    ".....",
    "...#.",
    "..##.",
    ".###.",
    "..##.",
    "...#.",
    ".....",
    "....."
};
inline const char* const kCwGlyph_Down[8] = {
    ".......",
    "#######",
    ".#####.",
    "..###..",
    "...#...",
    ".......",
    ".......",
    "......."
};
inline const char* const kCwGlyph_Up[8] = {
    ".......",
    "...#...",
    "..###..",
    ".#####.",
    "#######",
    ".......",
    ".......",
    "......."
};
inline const char* const kCwGlyph_Right[8] = {
    ".....",
    ".#...",
    ".##..",
    ".###.",
    ".##..",
    ".#...",
    ".....",
    "....."
};
inline const char* const kCwGlyph_Pause[8] = {
    ".......",
    ".##.##.",
    ".##.##.",
    ".##.##.",
    ".##.##.",
    ".##.##.",
    ".......",
    "......."
};
inline const char* const kCwGlyph_Sun[8] = {
    "...#...",
    "#..#..#",
    "..###..",
    "##.#.##",
    "..###..",
    "#..#..#",
    "...#...",
    "......."
};

inline const char* const* const kCwGlyphPtrs[20] = {
    kCwGlyph_S, kCwGlyph_A, kCwGlyph_M, kCwGlyph_STO, kCwGlyph_Math,
    kCwGlyph_D, kCwGlyph_R, kCwGlyph_G, kCwGlyph_FIX, kCwGlyph_SCI,
    kCwGlyph_E, kCwGlyph_i, kCwGlyph_Angle, kCwGlyph_DownInd, kCwGlyph_Left,
    kCwGlyph_Down, kCwGlyph_Up, kCwGlyph_Right, kCwGlyph_Pause, kCwGlyph_Sun
};

inline const char* const kEsGlyph_S[12] = {
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
inline const char* const kEsGlyph_A[12] = {
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
inline const char* const kEsGlyph_M[12] = {
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
inline const char* const kEsGlyph_STO[12] = {
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
inline const char* const kEsGlyph_RCL[12] = {
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
inline const char* const kEsGlyph_STAT[12] = {
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
inline const char* const kEsGlyph_CMPLX[12] = {
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
inline const char* const kEsGlyph_MAT[12] = {
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
inline const char* const kEsGlyph_VCT[12] = {
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
inline const char* const kEsGlyph_D[12] = {
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
inline const char* const kEsGlyph_R[12] = {
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
inline const char* const kEsGlyph_G[12] = {
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
inline const char* const kEsGlyph_FIX[12] = {
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
inline const char* const kEsGlyph_SCI[12] = {
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
inline const char* const kEsGlyph_Math[12] = {
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
inline const char* const kEsGlyph_Down[12] = {
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
inline const char* const kEsGlyph_Up[12] = {
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
inline const char* const kEsGlyph_Disp[12] = {
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

inline const char* const* const kEsGlyphPtrs[18] = {
    kEsGlyph_S, kEsGlyph_A, kEsGlyph_M, kEsGlyph_STO, kEsGlyph_RCL, kEsGlyph_STAT,
    kEsGlyph_CMPLX, kEsGlyph_MAT, kEsGlyph_VCT, kEsGlyph_D, kEsGlyph_R, kEsGlyph_G,
    kEsGlyph_FIX, kEsGlyph_SCI, kEsGlyph_Math, kEsGlyph_Down, kEsGlyph_Up, kEsGlyph_Disp
};

inline void StampSprite(
    int calcIdx,
    int spriteIndex,
    int stripW,
    int stripH,
    std::uint8_t fgR, std::uint8_t fgG, std::uint8_t fgB,
    std::vector<std::uint8_t>& outRgba)
{
    const SpriteSpec* sp = (calcIdx == 0)
        ? (spriteIndex >= 0 && spriteIndex < 20 ? &kClassWizSprites[spriteIndex] : nullptr)
        : (spriteIndex >= 0 && spriteIndex < 18 ? &kEsPlusSprites[spriteIndex] : nullptr);
    if (!sp) return;
    const char* const* glyph = (calcIdx == 0)
        ? kCwGlyphPtrs[spriteIndex]
        : kEsGlyphPtrs[spriteIndex];
    for (int ly = 0; ly < stripH; ++ly) {
        const char* row = glyph[ly];
        if (!row) continue;
        const size_t len = std::strlen(row);
        for (int lx = 0; lx < sp->srcW && static_cast<size_t>(lx) < len; ++lx) {
            if (row[lx] == '#') {
                const int gx = sp->srcX + lx;
                if (gx < 0 || gx >= stripW) continue;
                const size_t idx = (static_cast<size_t>(ly) * stripW + gx) * 4u;
                outRgba[idx + 0] = fgR;
                outRgba[idx + 1] = fgG;
                outRgba[idx + 2] = fgB;
                outRgba[idx + 3] = 255u;
            }
        }
    }
}

} // namespace detail

inline void RenderStripFromMask(
    int calcIdx,
    std::uint32_t indicatorsMask,
    std::uint8_t fgR, std::uint8_t fgG, std::uint8_t fgB,
    std::uint8_t bgR, std::uint8_t bgG, std::uint8_t bgB,
    std::vector<std::uint8_t>& outRgba,
    int& outW,
    int& outH)
{
    if (calcIdx != 0 && calcIdx != 1) {
        outW = 0;
        outH = 0;
        outRgba.clear();
        return;
    }
    outW = (calcIdx == 0) ? kCwStripW : kEsStripW;
    outH = (calcIdx == 0) ? kCwStripH : kEsStripH;
    const size_t pxCount = static_cast<size_t>(outW) * static_cast<size_t>(outH);
    outRgba.resize(pxCount * 4u);
    for (size_t i = 0; i < pxCount; ++i) {
        outRgba[i * 4u + 0] = bgR;
        outRgba[i * 4u + 1] = bgG;
        outRgba[i * 4u + 2] = bgB;
        outRgba[i * 4u + 3] = 255u;
    }

    const int* map = (calcIdx == 0) ? kCwBitToSprite : kEsBitToSprite;
    for (int bit = 0; bit < 12; ++bit) {
        if ((indicatorsMask & (1u << bit)) != 0u) {
            detail::StampSprite(calcIdx, map[bit], outW, outH, fgR, fgG, fgB, outRgba);
        }
    }
}

} // namespace CasioLinux::Indicators
