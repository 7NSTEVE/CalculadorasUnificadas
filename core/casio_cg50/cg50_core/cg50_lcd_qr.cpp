#include "cg50_lcd_qr.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cg50 {
namespace {

// GF(256) with primitive polynomial x^8 + x^4 + x^3 + x^2 + 1 (0x11D)
struct Gf256 {
    std::array<std::uint8_t, 512> exp{};
    std::array<std::uint8_t, 256> log{};

    Gf256()
    {
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            exp[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(x);
            exp[static_cast<std::size_t>(i + 255)] = static_cast<std::uint8_t>(x);
            log[static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(i);
            x <<= 1;
            if ((x & 0x100) != 0) x ^= 0x11D;
        }
        exp[510] = exp[0];
        exp[511] = exp[1];
    }

    std::uint8_t Mul(const std::uint8_t a, const std::uint8_t b) const noexcept
    {
        if (a == 0u || b == 0u) return 0u;
        return exp[static_cast<std::size_t>(log[a]) + static_cast<std::size_t>(log[b])];
    }

    std::uint8_t Inv(const std::uint8_t a) const noexcept
    {
        if (a == 0u) return 0u;
        return exp[255u - log[a]];
    }
};

const Gf256& GetGf()
{
    static const Gf256 gf;
    return gf;
}

// ISO-18004 Version 1..10 RS block geometry: {totalCodewords, ecPerBlock, g1Blocks, g1Data, g2Blocks, g2Data}
// Indexed as [version-1][eccLevel], where eccLevel: 0=L, 1=M, 2=Q, 3=H
struct RsSpec {
    int totalCodewords;
    int ecPerBlock;
    int g1Blocks;
    int g1Data;
    int g2Blocks;
    int g2Data;
};

constexpr RsSpec kRsTable[10][4] = {
    // V1 (21x21, 26 codewords)
    {{26, 7, 1, 19, 0, 0}, {26, 10, 1, 16, 0, 0}, {26, 13, 1, 13, 0, 0}, {26, 17, 1, 9, 0, 0}},
    // V2 (25x25, 44 codewords)
    {{44, 10, 1, 34, 0, 0}, {44, 16, 1, 28, 0, 0}, {44, 22, 1, 22, 0, 0}, {44, 28, 1, 16, 0, 0}},
    // V3 (29x29, 70 codewords)
    {{70, 15, 1, 55, 0, 0}, {70, 26, 1, 44, 0, 0}, {70, 18, 2, 17, 0, 0}, {70, 22, 2, 13, 0, 0}},
    // V4 (33x33, 100 codewords)
    {{100, 20, 1, 80, 0, 0}, {100, 18, 2, 32, 0, 0}, {100, 26, 2, 24, 0, 0}, {100, 16, 4, 9, 0, 0}},
    // V5 (37x37, 134 codewords)
    {{134, 26, 1, 108, 0, 0}, {134, 24, 2, 43, 0, 0}, {134, 18, 2, 15, 2, 16}, {134, 22, 2, 11, 2, 12}},
    // V6 (41x41, 172 codewords)
    {{172, 18, 2, 68, 0, 0}, {172, 16, 4, 27, 0, 0}, {172, 24, 4, 19, 0, 0}, {172, 28, 4, 15, 0, 0}},
    // V7 (45x45, 196 codewords)
    {{196, 20, 2, 78, 0, 0}, {196, 18, 4, 31, 0, 0}, {196, 18, 2, 14, 4, 15}, {196, 26, 4, 13, 1, 14}},
    // V8 (49x49, 242 codewords)
    {{242, 24, 2, 97, 0, 0}, {242, 22, 2, 38, 2, 39}, {242, 22, 4, 18, 2, 19}, {242, 26, 4, 14, 2, 15}},
    // V9 (53x53, 292 codewords)
    {{292, 30, 2, 116, 0, 0}, {292, 22, 3, 36, 2, 37}, {292, 20, 4, 16, 4, 17}, {292, 24, 4, 12, 4, 13}},
    // V10 (57x57, 346 codewords)
    {{346, 18, 2, 68, 2, 69}, {346, 26, 4, 43, 1, 44}, {346, 24, 6, 19, 2, 20}, {346, 28, 6, 15, 2, 16}},
};

// Alignment center coordinates for V1..V10
std::vector<int> AlignmentCoords(const int version)
{
    switch (version) {
    case 1:  return {};
    case 2:  return {6, 18};
    case 3:  return {6, 22};
    case 4:  return {6, 26};
    case 5:  return {6, 30};
    case 6:  return {6, 34};
    case 7:  return {6, 22, 38};
    case 8:  return {6, 24, 42};
    case 9:  return {6, 26, 46};
    case 10: return {6, 28, 50};
    default: return {};
    }
}

int PopCount16(std::uint16_t v) noexcept
{
    int c = 0;
    while (v) { v &= static_cast<std::uint16_t>(v - 1u); ++c; }
    return c;
}

// Compute BCH(15,5) format word for 5-bit data (2-bit eccFormatBits << 3 | 3-bit mask)
std::uint16_t EncodeFormatBits(const int data5) noexcept
{
    int d = data5 << 10;
    for (int i = 4; i >= 0; --i) {
        if ((d & (1 << (i + 10))) != 0) d ^= (0x537 << i);
    }
    return static_cast<std::uint16_t>(((data5 << 10) | d) ^ 0x5412);
}

bool DecodeFormatBits(const std::uint16_t raw15, int& outEccLevel, int& outMask) noexcept
{
    int bestDist = 16;
    int bestData = -1;
    for (int d = 0; d < 32; ++d) {
        const std::uint16_t code = EncodeFormatBits(d);
        const int dist = PopCount16(static_cast<std::uint16_t>(raw15 ^ code));
        if (dist < bestDist) {
            bestDist = dist;
            bestData = d;
        }
    }
    if (bestDist > 3 || bestData < 0) return false;
    // ISO-18004 format bits -> eccLevel: 01=L(0), 00=M(1), 11=Q(2), 10=H(3)
    const int fEcc = (bestData >> 3) & 3;
    switch (fEcc) {
    case 1: outEccLevel = 0; break; // L
    case 0: outEccLevel = 1; break; // M
    case 3: outEccLevel = 2; break; // Q
    case 2: outEccLevel = 3; break; // H
    default: return false;
    }
    outMask = bestData & 7;
    return true;
}

bool ApplyMask(const int mask, const int r, const int c) noexcept
{
    switch (mask) {
    case 0: return ((r + c) % 2) == 0;
    case 1: return (r % 2) == 0;
    case 2: return (c % 3) == 0;
    case 3: return ((r + c) % 3) == 0;
    case 4: return (((r / 2) + (c / 3)) % 2) == 0;
    case 5: return (((r * c) % 2) + ((r * c) % 3)) == 0;
    case 6: return ((((r * c) % 2) + ((r * c) % 3)) % 2) == 0;
    case 7: return ((((r + c) % 2) + ((r * c) % 3)) % 2) == 0;
    default: return false;
    }
}

// Verify or correct a single RS block (data + ec) using Berlekamp-Massey + Chien + Forney
bool RsCorrectBlock(std::vector<std::uint8_t>& block, const int ecCount)
{
    const auto& gf = GetGf();
    const int n = static_cast<int>(block.size());
    std::vector<std::uint8_t> syn(static_cast<std::size_t>(ecCount), 0u);
    bool anyError = false;
    for (int i = 0; i < ecCount; ++i) {
        std::uint8_t val = 0u;
        const std::uint8_t alpha = gf.exp[static_cast<std::size_t>(i)];
        for (int j = 0; j < n; ++j) {
            val = static_cast<std::uint8_t>(gf.Mul(val, alpha) ^ block[static_cast<std::size_t>(j)]);
        }
        syn[static_cast<std::size_t>(i)] = val;
        if (val != 0u) anyError = true;
    }
    if (!anyError) return true;

    // Berlekamp-Massey
    std::vector<std::uint8_t> errLoc = {1u};
    std::vector<std::uint8_t> oldLoc = {1u};
    for (int i = 0; i < ecCount; ++i) {
        std::uint8_t delta = syn[static_cast<std::size_t>(i)];
        for (std::size_t j = 1; j < errLoc.size(); ++j) {
            delta ^= gf.Mul(errLoc[j], syn[static_cast<std::size_t>(i) - j]);
        }
        oldLoc.insert(oldLoc.begin(), 0u);
        if (delta != 0u) {
            if (oldLoc.size() > errLoc.size()) {
                std::vector<std::uint8_t> newLoc(oldLoc.size(), 0u);
                for (std::size_t j = 0; j < oldLoc.size(); ++j) newLoc[j] = gf.Mul(oldLoc[j], delta);
                const std::uint8_t invD = gf.Inv(delta);
                oldLoc.resize(errLoc.size());
                for (std::size_t j = 0; j < errLoc.size(); ++j) oldLoc[j] = gf.Mul(errLoc[j], invD);
                errLoc = std::move(newLoc);
            }
            if (errLoc.size() < oldLoc.size()) errLoc.resize(oldLoc.size(), 0u);
            for (std::size_t j = 0; j < oldLoc.size(); ++j) {
                errLoc[j] ^= gf.Mul(delta, oldLoc[j]);
            }
        }
    }
    const int numErrors = static_cast<int>(errLoc.size()) - 1;
    if (numErrors <= 0 || numErrors * 2 > ecCount) return false;

    // Chien search
    std::vector<int> errPos;
    for (int i = 0; i < n; ++i) {
        std::uint8_t sum = 0u;
        const std::uint8_t xInv = gf.exp[static_cast<std::size_t>(255 - i)];
        std::uint8_t p = 1u;
        for (std::size_t j = 0; j < errLoc.size(); ++j) {
            sum ^= gf.Mul(errLoc[j], p);
            p = gf.Mul(p, xInv);
        }
        if (sum == 0u) errPos.push_back(n - 1 - i);
    }
    if (static_cast<int>(errPos.size()) != numErrors) return false;

    // Error evaluator Omega = (S * errLoc) mod x^ecCount
    std::vector<std::uint8_t> omega(static_cast<std::size_t>(ecCount), 0u);
    for (int i = 0; i < ecCount; ++i) {
        for (std::size_t j = 0; j < errLoc.size() && i + static_cast<int>(j) < ecCount; ++j) {
            omega[static_cast<std::size_t>(i) + j] ^= gf.Mul(syn[static_cast<std::size_t>(i)], errLoc[j]);
        }
    }
    for (int pos : errPos) {
        const int pow = n - 1 - pos;
        const std::uint8_t x = gf.exp[static_cast<std::size_t>(pow)];
        const std::uint8_t xInv = gf.Inv(x);
        std::uint8_t num = 0u, p = 1u;
        for (int j = 0; j < ecCount; ++j) {
            num ^= gf.Mul(omega[static_cast<std::size_t>(j)], p);
            p = gf.Mul(p, xInv);
        }
        std::uint8_t den = 0u;
        p = 1u;
        for (std::size_t j = 1; j < errLoc.size(); j += 2) {
            den ^= gf.Mul(errLoc[j], p);
            p = gf.Mul(p, gf.Mul(xInv, xInv));
        }
        if (den == 0u) return false;
        block[static_cast<std::size_t>(pos)] ^= gf.Mul(num, gf.Inv(den));
    }
    return true;
}

bool DecodeDataBitStream(const std::vector<std::uint8_t>& dataBytes, const int version, std::string& outText)
{
    outText.clear();
    std::size_t bitPos = 0u;
    const std::size_t totalBits = dataBytes.size() * 8u;
    auto readBits = [&](const int count, int& outVal) -> bool {
        if (bitPos + static_cast<std::size_t>(count) > totalBits) return false;
        int v = 0;
        for (int i = 0; i < count; ++i) {
            const std::uint8_t b = dataBytes[bitPos >> 3];
            const int bit = (b >> (7u - (bitPos & 7u))) & 1;
            v = (v << 1) | bit;
            ++bitPos;
        }
        outVal = v;
        return true;
    };

    static constexpr const char* kAlphaTable = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

    while (bitPos + 4u <= totalBits) {
        int mode = 0;
        if (!readBits(4, mode)) break;
        if (mode == 0) break; // Terminator

        if (mode == 1) { // Numeric
            const int cntBits = (version <= 9) ? 10 : 12;
            int cnt = 0;
            if (!readBits(cntBits, cnt)) return false;
            while (cnt >= 3) {
                int v = 0;
                if (!readBits(10, v) || v >= 1000) return false;
                outText.push_back(static_cast<char>('0' + (v / 100)));
                outText.push_back(static_cast<char>('0' + ((v / 10) % 10)));
                outText.push_back(static_cast<char>('0' + (v % 10)));
                cnt -= 3;
            }
            if (cnt == 2) {
                int v = 0;
                if (!readBits(7, v) || v >= 100) return false;
                outText.push_back(static_cast<char>('0' + (v / 10)));
                outText.push_back(static_cast<char>('0' + (v % 10)));
            } else if (cnt == 1) {
                int v = 0;
                if (!readBits(4, v) || v >= 10) return false;
                outText.push_back(static_cast<char>('0' + v));
            }
        } else if (mode == 2) { // Alphanumeric
            const int cntBits = (version <= 9) ? 9 : 11;
            int cnt = 0;
            if (!readBits(cntBits, cnt)) return false;
            while (cnt >= 2) {
                int v = 0;
                if (!readBits(11, v) || v >= 45 * 45) return false;
                outText.push_back(kAlphaTable[v / 45]);
                outText.push_back(kAlphaTable[v % 45]);
                cnt -= 2;
            }
            if (cnt == 1) {
                int v = 0;
                if (!readBits(6, v) || v >= 45) return false;
                outText.push_back(kAlphaTable[v]);
            }
        } else if (mode == 4) { // Byte (UTF-8 / ASCII URLs)
            const int cntBits = (version <= 9) ? 8 : 16;
            int cnt = 0;
            if (!readBits(cntBits, cnt)) return false;
            for (int i = 0; i < cnt; ++i) {
                int b = 0;
                if (!readBits(8, b)) return false;
                outText.push_back(static_cast<char>(b));
            }
        } else if (mode == 7) { // ECI
            int eci = 0;
            if (!readBits(8, eci)) return false;
            if ((eci & 0x80) != 0) {
                int extra = 0;
                if (!readBits(8, extra)) return false;
            }
        } else {
            return false;
        }
    }
    return !outText.empty();
}

bool DecodeMatrix(const std::vector<std::uint8_t>& grid, const int dim, std::string& outText)
{
    if (dim < 21 || dim > 57 || ((dim - 17) % 4) != 0) return false;
    const int version = (dim - 17) / 4;
    if (version < 1 || version > 10) return false;

    auto at = [&](const int r, const int c) noexcept -> int {
        return grid[static_cast<std::size_t>(r) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(c)] ? 1 : 0;
    };

    // Read Format Info #1 (around top-left finder)
    std::uint16_t fmt1 = 0u;
    constexpr int f1R[15] = {8,8,8,8,8,8,8,8,7,5,4,3,2,1,0};
    constexpr int f1C[15] = {0,1,2,3,4,5,7,8,8,8,8,8,8,8,8};
    for (int i = 0; i < 15; ++i) fmt1 = static_cast<std::uint16_t>((fmt1 << 1) | at(f1R[i], f1C[i]));

    int eccLevel = 0, mask = 0;
    if (!DecodeFormatBits(fmt1, eccLevel, mask)) {
        // Fallback: Format Info #2 (bottom-left + top-right)
        std::uint16_t fmt2 = 0u;
        for (int i = 0; i < 7; ++i) fmt2 = static_cast<std::uint16_t>((fmt2 << 1) | at(dim - 1 - i, 8));
        for (int i = 0; i < 8; ++i) fmt2 = static_cast<std::uint16_t>((fmt2 << 1) | at(8, dim - 8 + i));
        if (!DecodeFormatBits(fmt2, eccLevel, mask)) return false;
    }

    // Build function-module reservation map
    std::vector<std::uint8_t> isFunc(static_cast<std::size_t>(dim * dim), 0u);
    auto markRect = [&](const int r0, const int c0, const int h, const int w) {
        for (int r = r0; r < r0 + h; ++r) {
            for (int c = c0; c < c0 + w; ++c) {
                if (r >= 0 && r < dim && c >= 0 && c < dim) {
                    isFunc[static_cast<std::size_t>(r) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(c)] = 1u;
                }
            }
        }
    };
    markRect(0, 0, 9, 9);               // Top-left finder + separator + format
    markRect(0, dim - 8, 9, 8);         // Top-right finder + separator + format
    markRect(dim - 8, 0, 8, 9);         // Bottom-left finder + separator + format + dark module
    for (int i = 0; i < dim; ++i) {
        isFunc[static_cast<std::size_t>(6) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(i)] = 1u;
        isFunc[static_cast<std::size_t>(i) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(6)] = 1u;
    }
    const auto align = AlignmentCoords(version);
    for (int ar : align) {
        for (int ac : align) {
            if ((ar <= 8 && ac <= 8) || (ar <= 8 && ac >= dim - 9) || (ar >= dim - 9 && ac <= 8)) {
                continue;
            }
            markRect(ar - 2, ac - 2, 5, 5);
        }
    }
    if (version >= 7) {
        markRect(0, dim - 11, 6, 3);
        markRect(dim - 11, 0, 3, 6);
    }

    // Read raw codewords in ISO-18004 upward/downward 2-column zigzag order
    const RsSpec& spec = kRsTable[version - 1][eccLevel];
    std::vector<std::uint8_t> rawCodewords;
    rawCodewords.reserve(static_cast<std::size_t>(spec.totalCodewords));
    std::uint8_t curByte = 0u;
    int curBits = 0;
    bool upward = true;
    for (int col = dim - 1; col >= 1; col -= 2) {
        if (col == 6) col = 5;
        for (int step = 0; step < dim; ++step) {
            const int r = upward ? (dim - 1 - step) : step;
            for (int dx = 0; dx < 2; ++dx) {
                const int c = col - dx;
                if (isFunc[static_cast<std::size_t>(r) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(c)]) {
                    continue;
                }
                int bit = at(r, c);
                if (ApplyMask(mask, r, c)) bit ^= 1;
                curByte = static_cast<std::uint8_t>((curByte << 1) | bit);
                if (++curBits == 8) {
                    rawCodewords.push_back(curByte);
                    curByte = 0u;
                    curBits = 0;
                    if (static_cast<int>(rawCodewords.size()) == spec.totalCodewords) break;
                }
            }
            if (static_cast<int>(rawCodewords.size()) == spec.totalCodewords) break;
        }
        upward = !upward;
        if (static_cast<int>(rawCodewords.size()) == spec.totalCodewords) break;
    }
    if (static_cast<int>(rawCodewords.size()) != spec.totalCodewords) return false;

    // De-interleave RS blocks
    const int numBlocks = spec.g1Blocks + spec.g2Blocks;
    std::vector<std::vector<std::uint8_t>> blocks(static_cast<std::size_t>(numBlocks));
    std::vector<int> dataLens(static_cast<std::size_t>(numBlocks), 0);
    int maxData = 0;
    for (int b = 0; b < numBlocks; ++b) {
        dataLens[static_cast<std::size_t>(b)] = (b < spec.g1Blocks) ? spec.g1Data : spec.g2Data;
        maxData = std::max(maxData, dataLens[static_cast<std::size_t>(b)]);
        blocks[static_cast<std::size_t>(b)].reserve(static_cast<std::size_t>(dataLens[static_cast<std::size_t>(b)] + spec.ecPerBlock));
    }

    std::size_t idx = 0u;
    for (int i = 0; i < maxData; ++i) {
        for (int b = 0; b < numBlocks; ++b) {
            if (i < dataLens[static_cast<std::size_t>(b)]) {
                blocks[static_cast<std::size_t>(b)].push_back(rawCodewords[idx++]);
            }
        }
    }
    for (int i = 0; i < spec.ecPerBlock; ++i) {
        for (int b = 0; b < numBlocks; ++b) {
            blocks[static_cast<std::size_t>(b)].push_back(rawCodewords[idx++]);
        }
    }

    std::vector<std::uint8_t> dataStream;
    for (int b = 0; b < numBlocks; ++b) {
        if (!RsCorrectBlock(blocks[static_cast<std::size_t>(b)], spec.ecPerBlock)) return false;
        dataStream.insert(dataStream.end(),
                          blocks[static_cast<std::size_t>(b)].begin(),
                          blocks[static_cast<std::size_t>(b)].begin() + dataLens[static_cast<std::size_t>(b)]);
    }
    return DecodeDataBitStream(dataStream, version, outText);
}

} // namespace

bool DecodeLcdQrRgba(const std::uint8_t* rgba,
                     const int width,
                     const int height,
                     std::string& outText)
{
    outText.clear();
    if (!rgba || width < 40 || height < 40) return false;

    // Binarize digital LCD: dark pixels (luma < 128) = 1
    std::vector<std::uint8_t> bin(static_cast<std::size_t>(width * height), 0u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t off = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4u;
            const int luma = (static_cast<int>(rgba[off]) * 30 +
                              static_cast<int>(rgba[off + 1u]) * 59 +
                              static_cast<int>(rgba[off + 2u]) * 11) / 100;
            bin[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = (luma < 128) ? 1u : 0u;
        }
    }

    // Locate digital 7x7 finder pattern (exact 1:1:3:1:1 horizontal & vertical dark/light run ratio)
    struct FinderHit { int x0; int y0; int mod; };
    std::vector<FinderHit> finders;

    auto isDark = [&](const int x, const int y) noexcept -> bool {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return bin[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] != 0u;
    };

    auto verifyFinderAt = [&](const int fx, const int fy, const int m) noexcept -> bool {
        if (fx < 0 || fy < 0 || fx + 7 * m > width || fy + 7 * m > height) return false;
        // Sample center pixel of every 7x7 module of the finder pattern
        for (int r = 0; r < 7; ++r) {
            for (int c = 0; c < 7; ++c) {
                const bool expectDark = (r == 0 || r == 6 || c == 0 || c == 6 ||
                                         (r >= 2 && r <= 4 && c >= 2 && c <= 4));
                const int px = fx + c * m + m / 2;
                const int py = fy + r * m + m / 2;
                if (isDark(px, py) != expectDark) return false;
            }
        }
        return true;
    };

    // On a 384x216 Casio CG50 LCD, QR module size is typically 2..6 pixels (max 216 / 21 = 10)
    for (int m = 2; m <= 9; ++m) {
        const int fSize = 7 * m;
        for (int y = 0; y <= height - fSize; ++y) {
            // Quick scan along the center row of a potential finder (row 3*m + m/2)
            const int cy = y + 3 * m + m / 2;
            for (int x = 0; x <= width - fSize; ++x) {
                // Fast reject on top-left corner + center pixel + light ring pixel
                if (!isDark(x + m / 2, y + m / 2)) continue;
                if (!isDark(x + 3 * m + m / 2, cy)) continue;
                if (isDark(x + m + m / 2, cy)) continue;
                if (verifyFinderAt(x, y, m)) {
                    // Check if a matching top-right and bottom-left finder exist for any valid V1..V10 dim
                    for (int ver = 1; ver <= 10; ++ver) {
                        const int dim = 17 + 4 * ver;
                        const int qrPx = dim * m;
                        if (x + qrPx > width || y + qrPx > height) break;
                        const int trX = x + (dim - 7) * m;
                        const int blY = y + (dim - 7) * m;
                        if (verifyFinderAt(trX, y, m) && verifyFinderAt(x, blY, m)) {
                            // Sample full dim x dim module grid
                            std::vector<std::uint8_t> grid(static_cast<std::size_t>(dim * dim), 0u);
                            for (int gr = 0; gr < dim; ++gr) {
                                for (int gc = 0; gc < dim; ++gc) {
                                    const int sx = x + gc * m + m / 2;
                                    const int sy = y + gr * m + m / 2;
                                    grid[static_cast<std::size_t>(gr) * static_cast<std::size_t>(dim) + static_cast<std::size_t>(gc)] =
                                        isDark(sx, sy) ? 1u : 0u;
                                }
                            }
                            if (DecodeMatrix(grid, dim, outText)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

} // namespace cg50
