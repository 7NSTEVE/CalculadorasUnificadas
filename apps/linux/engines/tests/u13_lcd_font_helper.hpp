#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace U13Matrix {

struct U13KeyToken {
    std::uint8_t nxCode;   // CalcEmu nX-U8 matrix code
    const char* cw2Key;    // Key name in BuildCw2Keys()
};

struct U13Case {
    const char* id;
    const char* expr;
    const char* expectedDot; // VN+ / CW2 / CAS
    std::vector<U13KeyToken> keys;
};

inline std::string ExpectedForLax(const std::string& expectedDot)
{
    std::string out = expectedDot;
    for (char& ch : out) {
        if (ch == '.') ch = ',';
    }
    return out;
}

inline const std::vector<U13Case>& Get18Cases()
{
    static const std::vector<U13Case> cases = {
        {"suma", "1+2", "3",
         {{0x00, "1"}, {0x30, "MAS"}, {0x10, "2"}}},
        {"producto", "9*8", "72",
         {{0x22, "9"}, {0x31, "POR"}, {0x12, "8"}}},
        {"division_exacta", "100/4", "25",
         {{0x00, "1"}, {0x64, "0"}, {0x64, "0"}, {0x41, "ENTRE"}, {0x01, "4"}}},
        {"division_larga", "1/7", "0.1428571429",
         {{0x00, "1"}, {0x41, "ENTRE"}, {0x02, "7"}}},
        {"decimales", "1.5*4", "6",
         {{0x00, "1"}, {0x63, "PUNTO"}, {0x11, "5"}, {0x31, "POR"}, {0x01, "4"}}},
        {"potencia", "2^10", "1024",
         {{0x10, "2"}, {0x35, "POTENCIA"}, {0x00, "1"}, {0x64, "0"}}},
        {"cuadrado", "12^2", "144",
         {{0x00, "1"}, {0x10, "2"}, {0x25, "CUADRADO"}}},
        {"raiz_exacta", "sqrt(9)", "3",
         {{0x15, "RAIZ"}, {0x22, "9"}}},
        {"raiz_irracional", "sqrt(2)", "1.414213562",
         {{0x15, "RAIZ"}, {0x10, "2"}}},
        {"seno30", "sin(30)", "0.5",
         {{0x34, "SIN"}, {0x20, "3"}, {0x64, "0"}}},
        {"coseno60", "cos(60)", "0.5",
         {{0x44, "COS"}, {0x21, "6"}, {0x64, "0"}}},
        {"tangente45", "tan(45)", "1",
         {{0x54, "TAN"}, {0x01, "4"}, {0x11, "5"}}},
        {"logaritmo", "log(100)", "2",
         {{0x45, "LOG_B"}, {0x00, "1"}, {0x64, "0"}, {0x64, "0"}}},
        {"notacion_cient", "2*10^3+1", "2001",
         {{0x10, "2"}, {0x62, "EXP"}, {0x20, "3"}, {0x30, "MAS"}, {0x00, "1"}}},
        {"precedencia", "2+3*4", "14",
         {{0x10, "2"}, {0x30, "MAS"}, {0x20, "3"}, {0x31, "POR"}, {0x01, "4"}}},
        {"parentesis", "(2+3)*4", "20",
         {{0x23, "PAREN_ABRE"}, {0x10, "2"}, {0x30, "MAS"}, {0x20, "3"}, {0x33, "PAREN_CIERRA"}, {0x31, "POR"}, {0x01, "4"}}},
        {"negativo", "-5+2", "-3",
         {{0x04, "MENOS"}, {0x11, "5"}, {0x30, "MAS"}, {0x10, "2"}}},
        {"cadena_ans", "100/3", "33.33333333",
         {{0x00, "1"}, {0x64, "0"}, {0x64, "0"}, {0x41, "ENTRE"}, {0x20, "3"}}}
    };
    return cases;
}

inline const std::unordered_map<std::string, std::string>& GetLcdFontMap()
{
    static const std::unordered_map<std::string, std::string> font = {
        // ES PLUS 9px digits & operators
        {".#.\n##.\n.#.\n.#.\n.#.\n.#.\n.#.\n.#.\n###", "1"},
        {".###.\n#...#\n#...#\n....#\n...#.\n..#..\n.#...\n#....\n#####", "2"},
        {".###.\n#...#\n#...#\n....#\n..##.\n....#\n#...#\n#...#\n.###.", "3"},
        {"...#.\n..##.\n..##.\n.#.#.\n.#.#.\n#..#.\n#####\n...#.\n...#.", "4"},
        {"#####\n#....\n#....\n####.\n#...#\n....#\n....#\n#...#\n.###.", "5"},
        {"..##.\n.#...\n#....\n#....\n####.\n#...#\n#...#\n#...#\n.###.", "6"},
        {"#####\n#...#\n....#\n...#.\n...#.\n...#.\n..#..\n..#..\n..#..", "7"},
        {".###.\n#...#\n#...#\n#...#\n.###.\n#...#\n#...#\n#...#\n.###.", "8"},
        {".###.\n#...#\n#...#\n#...#\n.####\n....#\n....#\n...#.\n.##..", "9"},
        {".###.\n#...#\n#...#\n#...#\n#...#\n#...#\n#...#\n#...#\n.###.", "0"},
        {"..#..\n..#..\n#####\n..#..\n..#..", "+"},
        {"..#..\n.....\n#####\n.....\n..#..", "/"},

        // ClassWiz 12px digits
        {"..##..\n####..\n..##..\n..##..\n..##..\n..##..\n..##..\n..##..\n..##..\n..##..\n..##..\n######", "1"},
        {"..#####..\n.##...##.\n##.....##\n##.....##\n.......##\n......##.\n....###..\n...##....\n..##.....\n.##.....#\n#########\n#########", "2"},
        {".######..\n###...##.\n##.....##\n.......##\n......##.\n..#####..\n......##.\n.......##\n.......##\n##.....##\n###...##.\n.######..", "3"},
        {".....##..\n....###..\n...#.##..\n...#.##..\n..#..##..\n..#..##..\n.#...##..\n.#...##..\n#########\n.....##..\n.....##..\n....####.", "4"},
        {".########\n.#######.\n.#.......\n.#.......\n.######..\n##....##.\n#......##\n.......##\n.......##\n##.....##\n###...##.\n.######..", "5"},
        {"..######.\n.##...###\n##.....##\n##.......\n##.####..\n###...##.\n##.....##\n##.....##\n##.....##\n##.....##\n.##...##.\n..#####..", "6"},
        {"#########\n#########\n#.......#\n#.....##.\n.....##..\n.....##..\n....##...\n....##...\n...##....\n...##....\n...##....\n...##....", "7"},
        {"..#####..\n.##...##.\n##.....##\n##.....##\n###....##\n.#######.\n.#######.\n##....###\n##.....##\n##.....##\n.##...##.\n..#####..", "8"},
        {"..#####..\n.##...##.\n##.....##\n##.....##\n##.....##\n##.....##\n.##...###\n..####.##\n.......##\n##.....##\n###...##.\n.######..", "9"},
        {"..#####..\n.##...##.\n.#.....#.\n##.....##\n##.....##\n##.....##\n##.....##\n##.....##\n##.....##\n.#.....#.\n.##...##.\n..#####..", "0"},

        // Punctuation & shared glyphs
        {"##\n##", "."},
        {"###", "-"},
        {"###\n###\n###\n.##\n##.", ","},
        {"######\n######", "-"},
        {"#####", "-"},
        {"##########\n##########", "-"},
        {"##\n##\n..\n##\n##", ":"},
        {"###\n###\n###\n...\n...\n...\n###\n###\n###", ":"},

        // ES PLUS 6px font (input / menus)
        {".#.\n##.\n.#.\n.#.\n.#.\n###", "1"},
        {".###.\n#...#\n...#.\n..#..\n.#...\n#####", "2"},
        {".###.\n#...#\n..##.\n....#\n#...#\n.###.", "3"},
        {"...#.\n..##.\n.#.#.\n#..#.\n#####\n...#.", "4"},
        {"#####\n#....\n####.\n....#\n#...#\n.###.", "5"},
        {"..##.\n.#...\n####.\n#...#\n#...#\n.###.", "6"},
        {"#####\n....#\n...#.\n...#.\n..#..\n..#..", "7"},
        {".###.\n#...#\n.###.\n#...#\n#...#\n.###.", "8"},
        {".###.\n#...#\n#....\n#....\n#...#\n.###.", "C"},
        {".###.\n#...#\n#...#\n#...#\n#...#\n.###.", "O"},
        {"#...#\n##.##\n#.#.#\n#.#.#\n#...#\n#...#", "M"},
        {"####.\n#...#\n#...#\n####.\n#....\n#....", "P"},
        {"#....\n#....\n#....\n#....\n#....\n#####", "L"},
        {"#...#\n.#.#.\n..#..\n.#.#.\n#...#\n#...#", "X"},
        {".###.\n#...#\n.##..\n...#.\n#...#\n.###.", "S"},
        {"#####\n..#..\n..#..\n..#..\n..#..\n..#..", "T"},
        {"..#..\n.#.#.\n#...#\n#####\n#...#\n#...#", "A"},
        {"####.\n#...#\n####.\n#...#\n#...#\n####.", "B"},
        {"#####\n#....\n####.\n#....\n#....\n#####", "E"},
        {"#...#\n##..#\n#.#.#\n#..##\n#...#\n#...#", "N"},
        {".###.\n#...#\n#...#\n#.#.#\n#..#.\n.##.#", "Q"},
        {"####.\n#...#\n#...#\n####.\n#..#.\n#...#", "R"},
        {"###\n.#.\n.#.\n.#.\n.#.\n###", "I"},
        {"#...#\n#...#\n.#.#.\n.#.#.\n..#..\n..#..", "V"}
    };
    return font;
}

inline std::vector<std::string> DecodeLcdLines(
    const std::vector<std::uint8_t>& pixels,
    int width,
    int height)
{
    std::vector<std::string> lines;
    if (width <= 0 || height <= 0 || pixels.size() < static_cast<size_t>(width) * height) {
        return lines;
    }

    auto pxAt = [&](int x, int y) -> bool {
        return pixels[static_cast<size_t>(y) * width + x] != 0;
    };

    // 1. text_bands
    struct Band { int top; int bottom; };
    std::vector<Band> bands;
    int bandStart = -1;
    for (int y = 0; y < height; ++y) {
        bool hasInk = false;
        for (int x = 0; x < width; ++x) {
            if (pxAt(x, y)) { hasInk = true; break; }
        }
        if (hasInk && bandStart < 0) bandStart = y;
        else if (!hasInk && bandStart >= 0) {
            bands.push_back({bandStart, y});
            bandStart = -1;
        }
    }
    if (bandStart >= 0) bands.push_back({bandStart, height});

    const auto& font = GetLcdFontMap();

    // 2. segment_glyphs + decode_band
    for (const auto& b : bands) {
        struct Glyph { int left; int right; std::string key; };
        std::vector<Glyph> glyphs;
        std::vector<bool> cols(static_cast<size_t>(width), false);
        for (int x = 0; x < width; ++x) {
            for (int y = b.top; y < b.bottom; ++y) {
                if (pxAt(x, y)) { cols[static_cast<size_t>(x)] = true; break; }
            }
        }
        int gStart = -1;
        auto pushGlyph = [&](int left, int right) {
            std::vector<std::string> rows;
            for (int y = b.top; y < b.bottom; ++y) {
                std::string r;
                r.reserve(static_cast<size_t>(right - left));
                bool any = false;
                for (int x = left; x < right; ++x) {
                    const bool on = pxAt(x, y);
                    r.push_back(on ? '#' : '.');
                    if (on) any = true;
                }
                if (any || !rows.empty()) rows.push_back(std::move(r));
            }
            while (!rows.empty() && rows.back().find('#') == std::string::npos) {
                rows.pop_back();
            }
            std::string key;
            for (size_t i = 0; i < rows.size(); ++i) {
                if (i > 0) key.push_back('\n');
                key += rows[i];
            }
            glyphs.push_back({left, right, std::move(key)});
        };

        for (int x = 0; x < width; ++x) {
            if (cols[static_cast<size_t>(x)] && gStart < 0) gStart = x;
            else if (!cols[static_cast<size_t>(x)] && gStart >= 0) {
                pushGlyph(gStart, x);
                gStart = -1;
            }
        }
        if (gStart >= 0) pushGlyph(gStart, width);

        std::vector<int> widths;
        widths.reserve(glyphs.size());
        for (const auto& g : glyphs) widths.push_back(g.right - g.left);
        std::sort(widths.begin(), widths.end());
        const int typical = widths.empty() ? 5 : widths[widths.size() / 2];
        const int spaceGap = (std::max)(4, static_cast<int>(typical * 1.2));

        std::string out;
        int prevRight = -1;
        for (const auto& g : glyphs) {
            if (prevRight >= 0 && g.left - prevRight >= spaceGap) {
                out.push_back(' ');
            }
            auto it = font.find(g.key);
            out += (it != font.end()) ? it->second : "?";
            prevRight = g.right;
        }
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back() == ' ') out.pop_back();
        lines.push_back(std::move(out));
    }
    return lines;
}

inline std::vector<std::uint8_t> ExtractMonoFromRgba(
    const std::vector<std::uint8_t>& rgba,
    int width,
    int height)
{
    std::vector<std::uint8_t> mono(static_cast<size_t>(width) * height, 0);
    if (rgba.size() < static_cast<size_t>(width) * height * 4u) return mono;
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        const std::uint8_t r = rgba[i * 4u + 0];
        const std::uint8_t g = rgba[i * 4u + 1];
        const std::uint8_t b = rgba[i * 4u + 2];
        // LCD foreground pixels are dark (< 128); background is bright (~205-230)
        mono[i] = (static_cast<int>(r) + g + b < 384) ? 1u : 0u;
    }
    return mono;
}

} // namespace U13Matrix
