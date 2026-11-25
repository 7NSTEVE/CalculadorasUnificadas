#pragma once

// ============================================================================
// math_box_typesetter.hpp
// Shared Cross-Platform 2D Natural Textbook Mathematical Box Typesetter
// & Graphical Casio Keycap Renderer built on DocStudio::Cas::MathAstNode.
//
// Zero OS headers: renders identically through IMathDrawBackend on:
//   - Windows Direct2D + DirectWrite (Cambria Math / Segoe UI Variable)
//   - Linux Cairo + PangoCairo (STIX Two Math / DejaVu Serif / Inter)
//   - Vector PDF Writer (doc_studio/pdf/pdf_writer.cpp)
//
// Guarantees:
//   - True 2D stacked fractions (\frac, top-level rational H(s))
//   - Scaled & vertically shifted superscripts/subscripts (0.70x)
//   - Vector square & nth roots (\sqrt) with vinculum bar
//   - Stretchy delimiters ( ), [ ], { }, | |, < > and aligned matrices
//   - Big operators (\int, \sum, \prod, \lim) with above/below limits
//   - Accents (\vec, \hat, \bar, \dot, \tilde, \overline)
//   - Italic math variables vs upright functions & SI units
//   - Unicode Greek & physics symbols (Omega -> Ω, cdot -> ·, angle -> ∠, ...)
//   - Scientific notation formatted as a × 10^{n} (zero raw 8.29e-05)
//   - Graphical rounded Casio keycap badges (zero raw [x10^x] bracket dumps)
// ============================================================================

#include "../core/doc_types.hpp"
#include "../math/latex_mathml_ast.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::MathTypeset {

enum class FontStyleRole : std::uint8_t {
    UiRegular = 0,
    UiSemiBold = 1,
    MathItalic = 2,
    MathRoman = 3,
    MathBold = 4,
    MathSymbol = 5,
    KeycapLabel = 6
};

struct TextMetrics2D {
    double width = 0.0;
    double ascent = 0.0;
    double descent = 0.0;
};

struct IMathDrawBackend {
    virtual ~IMathDrawBackend() = default;

    virtual TextMetrics2D MeasureText(
        const std::string& utf8,
        double fontSizePx,
        FontStyleRole role) = 0;

    virtual void DrawTextRun(
        double x,
        double baselineY,
        const std::string& utf8,
        double fontSizePx,
        FontStyleRole role,
        DocColor color) = 0;

    virtual void DrawLine(
        double x0, double y0,
        double x1, double y1,
        double thickness,
        DocColor color) = 0;

    virtual void DrawRoundedPill(
        double x, double y, double w, double h,
        double radius,
        DocColor fill,
        DocColor stroke,
        double strokeWidth) = 0;
};

struct BoxMetrics {
    double width = 0.0;
    double ascent = 0.0;
    double descent = 0.0;

    double Height() const noexcept { return ascent + descent; }
};

namespace detail {

inline FontStyleRole ToBackendRole(Cas::MathFontRole r, bool forceUpright, bool bold) noexcept {
    if (bold) return FontStyleRole::MathBold;
    if (forceUpright) return FontStyleRole::MathRoman;
    switch (r) {
        case Cas::MathFontRole::TextNormal: return FontStyleRole::UiRegular;
        case Cas::MathFontRole::TextBold:   return FontStyleRole::UiSemiBold;
        case Cas::MathFontRole::MathItalic: return FontStyleRole::MathItalic;
        case Cas::MathFontRole::MathRoman:  return FontStyleRole::MathRoman;
        case Cas::MathFontRole::MathBold:   return FontStyleRole::MathBold;
        case Cas::MathFontRole::MathSymbol: return FontStyleRole::MathSymbol;
    }
    return FontStyleRole::MathRoman;
}

inline bool LooksLikeScientificOrUnitText(const std::string& s) {
    if (s.find('\\') != std::string::npos ||
        s.find('^') != std::string::npos ||
        s.find('_') != std::string::npos ||
        s.find('$') != std::string::npos) {
        return true;
    }
    // Check e-notation like 8.29e-05 or 1.2e+3
    for (std::size_t i = 1; i + 2 < s.size(); ++i) {
        if ((s[i] == 'e' || s[i] == 'E') &&
            std::isdigit(static_cast<unsigned char>(s[i - 1])) &&
            (s[i + 1] == '+' || s[i + 1] == '-' || std::isdigit(static_cast<unsigned char>(s[i + 1])))) {
            return true;
        }
    }
    return false;
}

// Convert raw formula/result text (which may be LaTeX, CAS expression, or mixed text with
// scientific notation / Greek names / Ohm / underscores / powers) into clean parseable LaTeX.
inline std::string NormalizeLineToRichLatex(const std::string& rawLine, bool isMathLine) {
    if (rawLine.empty()) return {};

    // Already contains explicit $...$ math delimiters
    if (rawLine.find('$') != std::string::npos) {
        return rawLine;
    }

    // Already contains LaTeX commands (\frac, \sqrt, \Omega, \text, etc.)
    if (rawLine.find('\\') != std::string::npos) {
        return "$" + rawLine + "$";
    }

    // Always keep natural prose prefixes before ": " in upright UI text (e.g.
    // "Superficie 3D CVD: z = sin(x^2 + y^2)", "Osciloscopio Nodo 2 [0..35 ms]: Vmax=...")
    const auto colonPos = rawLine.find(": ");
    if (colonPos != std::string::npos && colonPos > 0) {
        const std::string prefix = rawLine.substr(0, colonPos + 2);
        const std::string rhs = rawLine.substr(colonPos + 2);
        if (prefix.find('=') == std::string::npos && prefix.find('\\') == std::string::npos) {
            return prefix + NormalizeLineToRichLatex(rhs, isMathLine);
        }
    }

    // Multi-metric comma-separated readings (e.g. "Vmax=9.198 V, Vmin=-1.69e-53 V, Vrms=4.556 V")
    if (rawLine.find(", ") != std::string::npos && rawLine.find('=') != std::string::npos) {
        // Only split if each segment looks like "Name=Value" and we aren't inside parentheses
        int depth = 0;
        std::vector<std::string> segs;
        std::size_t st = 0;
        for (std::size_t k = 0; k < rawLine.size(); ++k) {
            if (rawLine[k] == '(' || rawLine[k] == '[') ++depth;
            else if (rawLine[k] == ')' || rawLine[k] == ']') --depth;
            else if (depth == 0 && rawLine[k] == ',' && k + 1 < rawLine.size() && rawLine[k + 1] == ' ') {
                segs.push_back(rawLine.substr(st, k - st));
                st = k + 2;
                ++k;
            }
        }
        segs.push_back(rawLine.substr(st));
        if (segs.size() >= 2) {
            bool allAssign = true;
            for (const auto& sg : segs) {
                if (sg.find('=') == std::string::npos) { allAssign = false; break; }
            }
            if (allAssign) {
                auto prettifyTelemetrySym = [](std::string seg) {
                    auto rep = [&](const char* from, const char* to) {
                        const std::size_t flen = std::char_traits<char>::length(from);
                        if (seg.size() >= flen && seg.compare(0, flen, from) == 0) {
                            seg.replace(0, flen, to);
                        }
                    };
                    rep("Vmax=", "V_max=");
                    rep("Vmin=", "V_min=");
                    rep("Vrms=", "V_rms=");
                    rep("Vdc=",  "V_dc=");
                    rep("Imax=", "I_max=");
                    rep("Imin=", "I_min=");
                    rep("Irms=", "I_rms=");
                    rep("Idc=",  "I_dc=");
                    return seg;
                };
                std::string out;
                for (std::size_t i = 0; i < segs.size(); ++i) {
                    if (i > 0) out += "  \xC2\xB7  ";
                    out += "$" + Cas::ConvertCasExprToLatex(prettifyTelemetrySym(segs[i])) + "$";
                }
                return out;
            }
        }
    }

    if (isMathLine) {
        // Handle chained #varsub equations separated by " = " cleanly
        std::vector<std::string> parts;
        std::size_t pos = 0;
        while (pos <= rawLine.size()) {
            const auto eq = rawLine.find(" = ", pos);
            if (eq == std::string::npos) {
                parts.push_back(rawLine.substr(pos));
                break;
            }
            parts.push_back(rawLine.substr(pos, eq - pos));
            pos = eq + 3;
        }
        std::string combined;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) combined += " = ";
            combined += Cas::ConvertCasExprToLatex(parts[i]);
        }
        return "$" + combined + "$";
    }

    if (LooksLikeScientificOrUnitText(rawLine) ||
        rawLine.find('=') != std::string::npos) {
        // Split on " | " so multi-metric lines stay cleanly spaced
        if (rawLine.find(" | ") != std::string::npos) {
            std::string out;
            std::size_t pos = 0;
            while (pos <= rawLine.size()) {
                const auto bar = rawLine.find(" | ", pos);
                std::string seg = (bar == std::string::npos) ? rawLine.substr(pos) : rawLine.substr(pos, bar - pos);
                if (!out.empty()) out += "   \xC2\xB7   ";
                out += "$" + Cas::ConvertCasExprToLatex(seg) + "$";
                if (bar == std::string::npos) break;
                pos = bar + 3;
            }
            return out;
        }
        return "$" + Cas::ConvertCasExprToLatex(rawLine) + "$";
    }

    return rawLine;
}

inline std::string PrettyUnicodeMathText(const std::string& raw) {
    std::string out = raw;
    auto repl = [&](const std::string& a, const std::string& b) {
        std::size_t p = 0;
        while ((p = out.find(a, p)) != std::string::npos) {
            out.replace(p, a.size(), b);
            p += b.size();
        }
    };
    repl("-", "\xE2\x88\x92"); // Unicode minus for math ops when standalone
    return out;
}

inline std::string NormalizeMathOpGlyph(const std::string& op) {
    if (op == "-") return "\xE2\x88\x92";
    if (op == "*") return "\xC2\xB7";
    if (op == "<=") return "\xE2\x89\xA4";
    if (op == ">=") return "\xE2\x89\xA5";
    if (op == "!=") return "\xE2\x89\xA0";
    if (op == "->") return "\xE2\x86\x92";
    return op;
}

inline double OpHorizontalPad(const std::string& op, double fontSize) noexcept {
    if (op == "," || op == ";" || op == ":") return fontSize * 0.12;
    if (op == "(" || op == ")" || op == "[" || op == "]") return fontSize * 0.04;
    if (op == "=" || op == "+" || op == "-" || op == "\xE2\x88\x92" ||
        op == "\xC2\xB1" || op == "\xE2\x89\xA4" || op == "\xE2\x89\xA5" ||
        op == "\xE2\x89\xA0" || op == "\xE2\x89\x88" || op == "\xE2\x86\x92") {
        return fontSize * 0.22;
    }
    if (op == "\xC2\xB7" || op == "\xC3\x97" || op == "\xC3\xB7") {
        return fontSize * 0.16;
    }
    return fontSize * 0.10;
}

inline void DrawStretchyDelimiter(
    IMathDrawBackend& bk,
    const std::string& delim,
    double x,
    double topY,
    double bottomY,
    double width,
    double fontSize,
    DocColor color)
{
    if (delim.empty() || delim == ".") return;
    const double h = (std::max)(fontSize, bottomY - topY);
    const double midY = 0.5 * (topY + bottomY);
    const double stroke = (std::max)(1.0, fontSize * 0.075);

    // For normal single-line heights, draw native glyph centered cleanly
    if (h <= fontSize * 1.45) {
        const auto m = bk.MeasureText(delim, fontSize, FontStyleRole::MathSymbol);
        const double baseY = midY + (m.ascent - m.descent) * 0.5;
        bk.DrawTextRun(x + (width - m.width) * 0.5, baseY, delim, fontSize, FontStyleRole::MathSymbol, color);
        return;
    }

    const double pad = (std::max)(1.5, stroke * 1.2);
    const double cx = x + width * 0.5;
    const double xL = x + pad;
    const double xR = x + width - pad;

    if (delim == "[" || delim == "]") {
        const double stemX = (delim == "[") ? xL + stroke * 0.5 : xR - stroke * 0.5;
        const double tickEnd = (delim == "[") ? xR : xL;
        bk.DrawLine(stemX, topY + pad, stemX, bottomY - pad, stroke, color);
        bk.DrawLine(stemX, topY + pad, tickEnd, topY + pad, stroke, color);
        bk.DrawLine(stemX, bottomY - pad, tickEnd, bottomY - pad, stroke, color);
        return;
    }
    if (delim == "|" || delim == "\xE2\x88\xA3") {
        bk.DrawLine(cx, topY + pad, cx, bottomY - pad, stroke, color);
        return;
    }
    if (delim == "||" || delim == "\xE2\x80\x96") {
        bk.DrawLine(cx - stroke * 1.4, topY + pad, cx - stroke * 1.4, bottomY - pad, stroke, color);
        bk.DrawLine(cx + stroke * 1.4, topY + pad, cx + stroke * 1.4, bottomY - pad, stroke, color);
        return;
    }
    if (delim == "(" || delim == ")") {
        const bool isLeft = (delim == "(");
        const double edgeX = isLeft ? xR : xL;
        const double bowX = isLeft ? xL : xR;
        const int segs = 10;
        double px = edgeX;
        double py = topY + pad;
        for (int i = 1; i <= segs; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(segs);
            const double ny = (topY + pad) + t * (h - 2.0 * pad);
            const double arch = std::sin(t * 3.14159265358979323846);
            const double nx = edgeX + (bowX - edgeX) * arch;
            bk.DrawLine(px, py, nx, ny, stroke, color);
            px = nx;
            py = ny;
        }
        return;
    }
    if (delim == "{" || delim == "}") {
        const bool isLeft = (delim == "{");
        const double outerX = isLeft ? xR : xL;
        const double innerX = isLeft ? xL : xR;
        const double stemX = 0.5 * (outerX + innerX);
        const double notch = (std::min)(6.0, h * 0.14);
        bk.DrawLine(outerX, topY + pad, stemX, topY + pad + notch, stroke, color);
        bk.DrawLine(stemX, topY + pad + notch, stemX, midY - notch * 0.8, stroke, color);
        bk.DrawLine(stemX, midY - notch * 0.8, innerX, midY, stroke, color);
        bk.DrawLine(innerX, midY, stemX, midY + notch * 0.8, stroke, color);
        bk.DrawLine(stemX, midY + notch * 0.8, stemX, bottomY - pad - notch, stroke, color);
        bk.DrawLine(stemX, bottomY - pad - notch, outerX, bottomY - pad, stroke, color);
        return;
    }
    if (delim == "<" || delim == "\xE2\x9F\xA8" || delim == ">" || delim == "\xE2\x9F\xA9") {
        const bool isLeft = (delim == "<" || delim == "\xE2\x9F\xA8");
        const double tipX = isLeft ? xL : xR;
        const double openX = isLeft ? xR : xL;
        bk.DrawLine(openX, topY + pad, tipX, midY, stroke, color);
        bk.DrawLine(tipX, midY, openX, bottomY - pad, stroke, color);
        return;
    }

    // Fallback: draw glyph centered vertically
    const auto m = bk.MeasureText(delim, fontSize, FontStyleRole::MathSymbol);
    bk.DrawTextRun(x + (width - m.width) * 0.5, midY + (m.ascent - m.descent) * 0.5,
                   delim, fontSize, FontStyleRole::MathSymbol, color);
}

} // namespace detail

// Forward declarations for recursive measure + draw
inline BoxMetrics MeasureAst(IMathDrawBackend& bk, const Cas::MathAstNodePtr& node, double fontSize);
inline void DrawAst(IMathDrawBackend& bk, const Cas::MathAstNodePtr& node, double x, double baselineY,
                    double fontSize, DocColor color);

inline BoxMetrics MeasureAst(IMathDrawBackend& bk, const Cas::MathAstNodePtr& node, double fontSize) {
    if (!node) return { 0.0, fontSize * 0.75, fontSize * 0.25 };
    const double clampedFs = (std::max)(5.5, fontSize);

    switch (node->kind) {
    case Cas::MathAstKind::Space:
        return { node->spaceEm * clampedFs, clampedFs * 0.7, clampedFs * 0.2 };

    case Cas::MathAstKind::TextRun:
    case Cas::MathAstKind::MathVar: {
        const auto role = detail::ToBackendRole(node->role, node->forceUpright, node->bold);
        const auto tm = bk.MeasureText(node->text, clampedFs, role);
        return { tm.width, (std::max)(tm.ascent, clampedFs * 0.65), (std::max)(tm.descent, clampedFs * 0.20) };
    }

    case Cas::MathAstKind::MathOp: {
        const std::string glyph = detail::NormalizeMathOpGlyph(node->text);
        const double pad = detail::OpHorizontalPad(glyph, clampedFs);
        const auto role = detail::ToBackendRole(node->role, true, node->bold);
        const auto tm = bk.MeasureText(glyph, clampedFs, role);
        return { tm.width + 2.0 * pad, (std::max)(tm.ascent, clampedFs * 0.65), (std::max)(tm.descent, clampedFs * 0.20) };
    }

    case Cas::MathAstKind::Row: {
        BoxMetrics m{ 0.0, clampedFs * 0.68, clampedFs * 0.22 };
        for (const auto& ch : node->children) {
            const auto cm = MeasureAst(bk, ch, clampedFs);
            m.width += cm.width;
            m.ascent = (std::max)(m.ascent, cm.ascent);
            m.descent = (std::max)(m.descent, cm.descent);
        }
        return m;
    }

    case Cas::MathAstKind::Fraction: {
        const double subFs = (std::max)(6.0, clampedFs * (node->compact ? 0.72 : 0.84));
        const auto nm = MeasureAst(bk, node->num, subFs);
        const auto dm = MeasureAst(bk, node->den, subFs);
        const double axisH = clampedFs * 0.28;
        const double gap = (std::max)(2.0, clampedFs * 0.14);
        const double barPad = (std::max)(3.0, clampedFs * 0.18);
        const double w = (std::max)(nm.width, dm.width) + 2.0 * barPad;
        const double asc = axisH + gap + nm.Height();
        const double desc = (std::max)(clampedFs * 0.25, dm.Height() + gap - axisH);
        return { w, asc, desc };
    }

    case Cas::MathAstKind::Sqrt: {
        const auto rm = MeasureAst(bk, node->radicand, clampedFs);
        const double hookW = clampedFs * 0.68;
        const double topPad = (std::max)(2.2, clampedFs * 0.18);
        const double rightPad = clampedFs * 0.16;
        double idxExtraW = 0.0;
        double idxTop = 0.0;
        if (node->rootIdx) {
            const auto im = MeasureAst(bk, node->rootIdx, clampedFs * 0.58);
            idxExtraW = (std::max)(0.0, im.width - hookW * 0.35);
            idxTop = im.Height() * 0.65;
        }
        return {
            idxExtraW + hookW + rm.width + rightPad,
            (std::max)(rm.ascent + topPad, idxTop + clampedFs * 0.30),
            rm.descent + clampedFs * 0.06
        };
    }

    case Cas::MathAstKind::SupSub: {
        const auto bm = MeasureAst(bk, node->base, clampedFs);
        const double scriptFs = (std::max)(5.5, clampedFs * 0.70);
        BoxMetrics sm{}, sb{};
        if (node->sup) sm = MeasureAst(bk, node->sup, scriptFs);
        if (node->sub) sb = MeasureAst(bk, node->sub, scriptFs);

        const double supShift = (std::max)(clampedFs * 0.42, bm.ascent * 0.62);
        const double subShift = (std::max)(clampedFs * 0.24, bm.descent * 0.75 + scriptFs * 0.16);
        const double scriptW = (std::max)(sm.width, sb.width);
        const double gapX = (scriptW > 0.0) ? (clampedFs * 0.05) : 0.0;

        double asc = bm.ascent;
        double desc = bm.descent;
        if (node->sup) asc = (std::max)(asc, supShift + sm.ascent);
        if (node->sub) desc = (std::max)(desc, subShift + sb.descent);
        return { bm.width + gapX + scriptW, asc, desc };
    }

    case Cas::MathAstKind::BigOp: {
        const bool isLim = (node->text == "lim");
        const double opFs = isLim ? clampedFs : (clampedFs * 1.38);
        const auto opM = bk.MeasureText(node->text, opFs, FontStyleRole::MathSymbol);
        const double limFs = (std::max)(5.5, clampedFs * 0.66);
        BoxMetrics supM{}, subM{};
        if (node->sup) supM = MeasureAst(bk, node->sup, limFs);
        if (node->sub) subM = MeasureAst(bk, node->sub, limFs);
        const double w = (std::max)({ opM.width, supM.width, subM.width }) + clampedFs * 0.26;
        const double vGap = (std::max)(1.5, clampedFs * 0.10);
        const double asc = opM.ascent + (node->sup ? (vGap + supM.Height()) : 0.0);
        const double desc = opM.descent + (node->sub ? (vGap + subM.Height()) : 0.0);
        return { w, asc, desc };
    }

    case Cas::MathAstKind::Accent: {
        const auto bm = MeasureAst(bk, node->base, clampedFs);
        const double accentH = clampedFs * 0.34;
        return { (std::max)(bm.width, clampedFs * 0.50), bm.ascent + accentH, bm.descent };
    }

    case Cas::MathAstKind::Delimiter: {
        const auto& inner = node->base ? node->base : (!node->children.empty() ? node->children.front() : Cas::MathAstNodePtr{});
        const auto bm = MeasureAst(bk, inner, clampedFs);
        const double dW = clampedFs * 0.42;
        const double lW = (!node->text.empty() && node->text != ".") ? dW : 0.0;
        const double rW = (!node->closeDelim.empty() && node->closeDelim != ".") ? dW : 0.0;
        return { lW + bm.width + rW + clampedFs * 0.08, bm.ascent + clampedFs * 0.08, bm.descent + clampedFs * 0.08 };
    }

    case Cas::MathAstKind::Matrix: {
        const double cellFs = (std::max)(6.0, clampedFs * 0.86);
        const std::size_t rows = node->grid.size();
        std::size_t cols = 0;
        for (const auto& r : node->grid) cols = (std::max)(cols, r.size());
        if (rows == 0 || cols == 0) return { clampedFs, clampedFs * 0.7, clampedFs * 0.2 };

        std::vector<double> colW(cols, 0.0);
        std::vector<double> rowH(rows, 0.0);
        for (std::size_t r = 0; r < rows; ++r) {
            double maxA = cellFs * 0.65, maxD = cellFs * 0.22;
            for (std::size_t c = 0; c < node->grid[r].size(); ++c) {
                const auto cm = MeasureAst(bk, node->grid[r][c], cellFs);
                colW[c] = (std::max)(colW[c], cm.width);
                maxA = (std::max)(maxA, cm.ascent);
                maxD = (std::max)(maxD, cm.descent);
            }
            rowH[r] = maxA + maxD;
        }
        const double colGap = clampedFs * 0.62;
        const double rowGap = clampedFs * 0.24;
        double totalW = 0.0;
        for (std::size_t c = 0; c < cols; ++c) {
            if (c > 0) totalW += colGap;
            totalW += colW[c];
        }
        double totalH = 0.0;
        for (std::size_t r = 0; r < rows; ++r) {
            if (r > 0) totalH += rowGap;
            totalH += rowH[r];
        }
        const bool hasDelims = (node->envName != "matrix" && node->envName != "aligned" && node->envName != "cases");
        const double delimW = hasDelims ? (clampedFs * 0.45) : (node->envName == "cases" ? clampedFs * 0.50 : 0.0);
        const double axisH = clampedFs * 0.28;
        const double halfH = totalH * 0.5;
        return {
            totalW + (hasDelims ? 2.0 * delimW + clampedFs * 0.20 : delimW + clampedFs * 0.14),
            halfH + axisH,
            (std::max)(clampedFs * 0.25, halfH - axisH)
        };
    }
    }
    return { 0.0, clampedFs * 0.7, clampedFs * 0.2 };
}

inline void DrawAst(
    IMathDrawBackend& bk,
    const Cas::MathAstNodePtr& node,
    double x,
    double baselineY,
    double fontSize,
    DocColor color)
{
    if (!node) return;
    const double clampedFs = (std::max)(5.5, fontSize);
    const double stroke = (std::max)(0.9, clampedFs * 0.068);

    switch (node->kind) {
    case Cas::MathAstKind::Space:
        return;

    case Cas::MathAstKind::TextRun:
    case Cas::MathAstKind::MathVar: {
        const auto role = detail::ToBackendRole(node->role, node->forceUpright, node->bold);
        bk.DrawTextRun(x, baselineY, node->text, clampedFs, role, color);
        return;
    }

    case Cas::MathAstKind::MathOp: {
        const std::string glyph = detail::NormalizeMathOpGlyph(node->text);
        const double pad = detail::OpHorizontalPad(glyph, clampedFs);
        const auto role = detail::ToBackendRole(node->role, true, node->bold);
        bk.DrawTextRun(x + pad, baselineY, glyph, clampedFs, role, color);
        return;
    }

    case Cas::MathAstKind::Row: {
        double curX = x;
        for (const auto& ch : node->children) {
            const auto cm = MeasureAst(bk, ch, clampedFs);
            DrawAst(bk, ch, curX, baselineY, clampedFs, color);
            curX += cm.width;
        }
        return;
    }

    case Cas::MathAstKind::Fraction: {
        const double subFs = (std::max)(6.0, clampedFs * (node->compact ? 0.72 : 0.84));
        const auto nm = MeasureAst(bk, node->num, subFs);
        const auto dm = MeasureAst(bk, node->den, subFs);
        const double axisY = baselineY - clampedFs * 0.28;
        const double gap = (std::max)(2.0, clampedFs * 0.14);
        const double barPad = (std::max)(3.0, clampedFs * 0.18);
        const double w = (std::max)(nm.width, dm.width) + 2.0 * barPad;

        const double numX = x + (w - nm.width) * 0.5;
        const double numBaseY = axisY - gap - nm.descent;
        DrawAst(bk, node->num, numX, numBaseY, subFs, color);

        if (node->hasBar) {
            bk.DrawLine(x + barPad * 0.35, axisY, x + w - barPad * 0.35, axisY, stroke, color);
        }

        const double denX = x + (w - dm.width) * 0.5;
        const double denBaseY = axisY + gap + dm.ascent;
        DrawAst(bk, node->den, denX, denBaseY, subFs, color);
        return;
    }

    case Cas::MathAstKind::Sqrt: {
        const auto rm = MeasureAst(bk, node->radicand, clampedFs);
        const double hookW = clampedFs * 0.68;
        const double topPad = (std::max)(2.2, clampedFs * 0.18);
        const double rightPad = clampedFs * 0.16;
        double idxExtraW = 0.0;
        if (node->rootIdx) {
            const auto im = MeasureAst(bk, node->rootIdx, clampedFs * 0.58);
            idxExtraW = (std::max)(0.0, im.width - hookW * 0.35);
            DrawAst(bk, node->rootIdx, x, baselineY - clampedFs * 0.48, clampedFs * 0.58, color);
        }

        const double radX = x + idxExtraW + hookW;
        const double topY = baselineY - rm.ascent - topPad * 0.75;
        const double botY = baselineY + rm.descent * 0.65;
        const double kneeX = x + idxExtraW + hookW * 0.15;
        const double kneeY = baselineY - clampedFs * 0.18;
        const double troughX = x + idxExtraW + hookW * 0.48;

        // Draw crisp vector radical check + vinculum bar
        bk.DrawLine(x + idxExtraW, kneeY + clampedFs * 0.05, kneeX, kneeY, stroke, color);
        bk.DrawLine(kneeX, kneeY, troughX, botY, stroke * 1.25, color);
        bk.DrawLine(troughX, botY, radX - clampedFs * 0.06, topY, stroke, color);
        bk.DrawLine(radX - clampedFs * 0.06, topY, radX + rm.width + rightPad * 0.5, topY, stroke, color);

        DrawAst(bk, node->radicand, radX, baselineY, clampedFs, color);
        return;
    }

    case Cas::MathAstKind::SupSub: {
        const auto bm = MeasureAst(bk, node->base, clampedFs);
        DrawAst(bk, node->base, x, baselineY, clampedFs, color);
        const double scriptFs = (std::max)(5.5, clampedFs * 0.70);
        const double sx = x + bm.width + clampedFs * 0.05;
        if (node->sup) {
            const double supShift = (std::max)(clampedFs * 0.42, bm.ascent * 0.62);
            DrawAst(bk, node->sup, sx, baselineY - supShift, scriptFs, color);
        }
        if (node->sub) {
            const double subShift = (std::max)(clampedFs * 0.24, bm.descent * 0.75 + scriptFs * 0.16);
            DrawAst(bk, node->sub, sx, baselineY + subShift, scriptFs, color);
        }
        return;
    }

    case Cas::MathAstKind::BigOp: {
        const bool isLim = (node->text == "lim");
        const double opFs = isLim ? clampedFs : (clampedFs * 1.38);
        const auto opM = bk.MeasureText(node->text, opFs, FontStyleRole::MathSymbol);
        const double limFs = (std::max)(5.5, clampedFs * 0.66);
        BoxMetrics supM{}, subM{};
        if (node->sup) supM = MeasureAst(bk, node->sup, limFs);
        if (node->sub) subM = MeasureAst(bk, node->sub, limFs);
        const double w = (std::max)({ opM.width, supM.width, subM.width }) + clampedFs * 0.26;
        const double vGap = (std::max)(1.5, clampedFs * 0.10);

        bk.DrawTextRun(x + (w - opM.width) * 0.5, baselineY, node->text, opFs, FontStyleRole::MathSymbol, color);
        if (node->sup) {
            const double supBase = baselineY - opM.ascent - vGap - supM.descent;
            DrawAst(bk, node->sup, x + (w - supM.width) * 0.5, supBase, limFs, color);
        }
        if (node->sub) {
            const double subBase = baselineY + opM.descent + vGap + subM.ascent;
            DrawAst(bk, node->sub, x + (w - subM.width) * 0.5, subBase, limFs, color);
        }
        return;
    }

    case Cas::MathAstKind::Accent: {
        const auto bm = MeasureAst(bk, node->base, clampedFs);
        const double w = (std::max)(bm.width, clampedFs * 0.50);
        const double bx = x + (w - bm.width) * 0.5;
        DrawAst(bk, node->base, bx, baselineY, clampedFs, color);

        const double ay = baselineY - bm.ascent - clampedFs * 0.14;
        const double x0 = x + clampedFs * 0.05;
        const double x1 = x + w - clampedFs * 0.05;
        const double xm = 0.5 * (x0 + x1);

        if (node->accentKind == "bar" || node->accentKind == "overline") {
            bk.DrawLine(x0, ay, x1, ay, stroke, color);
        } else if (node->accentKind == "vec") {
            bk.DrawLine(x0, ay, x1, ay, stroke, color);
            const double ah = (std::max)(2.0, clampedFs * 0.16);
            bk.DrawLine(x1 - ah, ay - ah * 0.6, x1, ay, stroke, color);
            bk.DrawLine(x1 - ah, ay + ah * 0.6, x1, ay, stroke, color);
        } else if (node->accentKind == "hat") {
            const double hh = (std::max)(2.2, clampedFs * 0.18);
            bk.DrawLine(x0, ay + hh * 0.4, xm, ay - hh * 0.6, stroke, color);
            bk.DrawLine(xm, ay - hh * 0.6, x1, ay + hh * 0.4, stroke, color);
        } else if (node->accentKind == "dot") {
            bk.DrawTextRun(xm - clampedFs * 0.14, ay + clampedFs * 0.12, "\xCB\x99", clampedFs * 0.9, FontStyleRole::MathSymbol, color);
        } else if (node->accentKind == "ddot") {
            bk.DrawTextRun(xm - clampedFs * 0.24, ay + clampedFs * 0.12, "\xC2\xA8", clampedFs * 0.9, FontStyleRole::MathSymbol, color);
        } else {
            bk.DrawTextRun(xm - clampedFs * 0.22, ay + clampedFs * 0.18, "~", clampedFs * 0.85, FontStyleRole::MathSymbol, color);
        }
        return;
    }

    case Cas::MathAstKind::Delimiter: {
        const auto& inner = node->base ? node->base : (!node->children.empty() ? node->children.front() : Cas::MathAstNodePtr{});
        const auto bm = MeasureAst(bk, inner, clampedFs);
        const double dW = clampedFs * 0.42;
        const double lW = (!node->text.empty() && node->text != ".") ? dW : 0.0;
        const double rW = (!node->closeDelim.empty() && node->closeDelim != ".") ? dW : 0.0;
        const double topY = baselineY - bm.ascent - clampedFs * 0.06;
        const double botY = baselineY + bm.descent + clampedFs * 0.06;

        if (lW > 0.0) {
            detail::DrawStretchyDelimiter(bk, node->text, x, topY, botY, lW, clampedFs, color);
        }
        DrawAst(bk, inner, x + lW + clampedFs * 0.04, baselineY, clampedFs, color);
        if (rW > 0.0) {
            detail::DrawStretchyDelimiter(bk, node->closeDelim, x + lW + bm.width + clampedFs * 0.08, topY, botY, rW, clampedFs, color);
        }
        return;
    }

    case Cas::MathAstKind::Matrix: {
        const double cellFs = (std::max)(6.0, clampedFs * 0.86);
        const std::size_t rows = node->grid.size();
        std::size_t cols = 0;
        for (const auto& r : node->grid) cols = (std::max)(cols, r.size());
        if (rows == 0 || cols == 0) return;

        std::vector<double> colW(cols, 0.0);
        std::vector<double> rowAsc(rows, cellFs * 0.65);
        std::vector<double> rowDesc(rows, cellFs * 0.22);
        for (std::size_t r = 0; r < rows; ++r) {
            for (std::size_t c = 0; c < node->grid[r].size(); ++c) {
                const auto cm = MeasureAst(bk, node->grid[r][c], cellFs);
                colW[c] = (std::max)(colW[c], cm.width);
                rowAsc[r] = (std::max)(rowAsc[r], cm.ascent);
                rowDesc[r] = (std::max)(rowDesc[r], cm.descent);
            }
        }
        const double colGap = clampedFs * 0.62;
        const double rowGap = clampedFs * 0.24;
        double totalW = 0.0;
        for (std::size_t c = 0; c < cols; ++c) {
            if (c > 0) totalW += colGap;
            totalW += colW[c];
        }
        double totalH = 0.0;
        for (std::size_t r = 0; r < rows; ++r) {
            if (r > 0) totalH += rowGap;
            totalH += (rowAsc[r] + rowDesc[r]);
        }

        std::string openD, closeD;
        if (node->envName == "pmatrix") { openD = "("; closeD = ")"; }
        else if (node->envName == "bmatrix") { openD = "["; closeD = "]"; }
        else if (node->envName == "Bmatrix") { openD = "{"; closeD = "}"; }
        else if (node->envName == "vmatrix") { openD = "|"; closeD = "|"; }
        else if (node->envName == "Vmatrix") { openD = "||"; closeD = "||"; }
        else if (node->envName == "cases") { openD = "{"; closeD = ""; }

        const double dW = (!openD.empty()) ? (clampedFs * 0.45) : 0.0;
        const double axisY = baselineY - clampedFs * 0.28;
        const double topY = axisY - totalH * 0.5;
        const double botY = axisY + totalH * 0.5;

        if (!openD.empty()) {
            detail::DrawStretchyDelimiter(bk, openD, x, topY - 2.0, botY + 2.0, dW, clampedFs, color);
        }
        const double gridX0 = x + dW + (!openD.empty() ? clampedFs * 0.10 : 0.0);
        double curY = topY;
        for (std::size_t r = 0; r < rows; ++r) {
            const double rowBaseY = curY + rowAsc[r];
            double curX = gridX0;
            for (std::size_t c = 0; c < node->grid[r].size(); ++c) {
                const auto cm = MeasureAst(bk, node->grid[r][c], cellFs);
                const double cellX = (node->envName == "cases") ? curX : (curX + (colW[c] - cm.width) * 0.5);
                DrawAst(bk, node->grid[r][c], cellX, rowBaseY, cellFs, color);
                curX += colW[c] + colGap;
            }
            curY += rowAsc[r] + rowDesc[r] + rowGap;
        }
        if (!closeD.empty()) {
            detail::DrawStretchyDelimiter(bk, closeD, gridX0 + totalW + clampedFs * 0.08, topY - 2.0, botY + 2.0, dW, clampedFs, color);
        }
        return;
    }
    }
}

// ============================================================================
// Rich Math Line Renderer (Mixes UI text and 2D MathAstNode spans cleanly,
// auto-scaling down if wider than maxWidth so nothing clips or leaks raw LaTeX)
// ============================================================================
inline double DrawRichMathLine(
    IMathDrawBackend& bk,
    const std::string& rawLine,
    double x,
    double topY,
    double maxWidth,
    double fontSize,
    DocColor color,
    bool forceMathLine = false)
{
    if (rawLine.empty()) return fontSize * 1.15;
    const std::string norm = detail::NormalizeLineToRichLatex(rawLine, forceMathLine);
    const auto spans = Cas::TokenizeRichLatexUtf8(norm);

    auto measureSpans = [&](double fs) -> BoxMetrics {
        BoxMetrics total{ 0.0, fs * 0.78, fs * 0.24 };
        for (const auto& sp : spans) {
            if (sp.isNewline) continue;
            if (sp.isMath && sp.ast) {
                const auto bm = MeasureAst(bk, sp.ast, fs);
                total.width += bm.width;
                total.ascent = (std::max)(total.ascent, bm.ascent);
                total.descent = (std::max)(total.descent, bm.descent);
            } else {
                const auto tm = bk.MeasureText(sp.rawText, fs, FontStyleRole::UiRegular);
                total.width += tm.width;
                total.ascent = (std::max)(total.ascent, tm.ascent);
                total.descent = (std::max)(total.descent, tm.descent);
            }
        }
        return total;
    };

    double effFs = (std::max)(6.5, fontSize);
    BoxMetrics m = measureSpans(effFs);
    if (maxWidth > 24.0 && m.width > maxWidth) {
        const double shrink = std::clamp(maxWidth / m.width, 0.58, 1.0);
        effFs = (std::max)(6.5, effFs * shrink);
        m = measureSpans(effFs);
    }

    const double baselineY = topY + m.ascent;
    double curX = x;
    for (const auto& sp : spans) {
        if (sp.isNewline) continue;
        if (sp.isMath && sp.ast) {
            const auto bm = MeasureAst(bk, sp.ast, effFs);
            DrawAst(bk, sp.ast, curX, baselineY, effFs, color);
            curX += bm.width;
        } else {
            const auto tm = bk.MeasureText(sp.rawText, effFs, FontStyleRole::UiRegular);
            bk.DrawTextRun(curX, baselineY, sp.rawText, effFs, FontStyleRole::UiRegular, color);
            curX += tm.width;
        }
    }
    return m.Height() + effFs * 0.24;
}

// ============================================================================
// Graphical Casio Keycap Pill Strip Renderer (Zero raw [x10^x] bracket text!)
// Parses sequences like "[HOME] Calculate [OK] 4 . 7 [x10^x] 3 [x] 2 [(-)] 3 [EXE]"
// into tactile rounded graphical Casio keycaps with color-coded hardware accents.
// ============================================================================
struct CasioKeycapToken {
    std::string label;
    bool isArrow = false;
    DocColor fill = DocColor::RGBA(241, 245, 249, 255);
    DocColor stroke = DocColor::RGBA(148, 163, 184, 255);
    DocColor text = DocColor::RGBA(15, 23, 42, 255);
};

inline std::vector<CasioKeycapToken> ParseCasioKeycaps(const std::string& seqUtf8, bool darkSurface = false) {
    std::vector<CasioKeycapToken> out;
    auto pushTok = [&](const std::string& raw, bool bracketed) {
        std::string s = raw;
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        if (s.empty()) return;

        if (s == "->" || s == "\xE2\x86\x92") {
            CasioKeycapToken t;
            t.label = "\xE2\x86\x92";
            t.isArrow = true;
            t.text = darkSurface ? DocColor::RGBA(148, 163, 184, 255) : DocColor::RGBA(100, 116, 139, 255);
            out.push_back(t);
            return;
        }

        CasioKeycapToken k;
        std::string up = s;
        for (char& ch : up) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

        const DocColor opFill   = darkSurface ? DocColor::RGBA(51, 65, 85, 255)   : DocColor::RGBA(226, 232, 240, 255);
        const DocColor opStroke = darkSurface ? DocColor::RGBA(100, 116, 139, 255) : DocColor::RGBA(148, 163, 184, 255);
        const DocColor opText   = darkSurface ? DocColor::RGBA(241, 245, 249, 255) : DocColor::RGBA(15, 23, 42, 255);
        const DocColor numFill  = darkSurface ? DocColor::RGBA(36, 46, 62, 255)   : DocColor::RGBA(255, 255, 255, 255);

        if (up == "SHIFT") {
            k.label = "SHIFT";
            k.fill = darkSurface ? DocColor::RGBA(217, 152, 24, 255) : DocColor::RGBA(254, 243, 199, 255);
            k.stroke = darkSurface ? DocColor::RGBA(251, 191, 36, 255) : DocColor::RGBA(217, 119, 6, 255);
            k.text = darkSurface ? DocColor::RGBA(24, 16, 4, 255) : DocColor::RGBA(146, 64, 14, 255);
        } else if (up == "ALPHA") {
            k.label = "ALPHA";
            k.fill = darkSurface ? DocColor::RGBA(210, 56, 102, 255) : DocColor::RGBA(254, 226, 226, 255);
            k.stroke = darkSurface ? DocColor::RGBA(248, 113, 113, 255) : DocColor::RGBA(220, 38, 38, 255);
            k.text = darkSurface ? DocColor::RGBA(255, 255, 255, 255) : DocColor::RGBA(153, 27, 27, 255);
        } else if (up == "EXE" || up == "OK" || up == "=") {
            k.label = (up == "=") ? "=" : up;
            k.fill = DocColor::RGBA(37, 99, 235, 255);
            k.stroke = darkSurface ? DocColor::RGBA(96, 165, 250, 255) : DocColor::RGBA(29, 78, 216, 255);
            k.text = DocColor::RGBA(255, 255, 255, 255);
        } else if (up == "AC" || up == "DEL" || up == "ON") {
            k.label = up;
            k.fill = DocColor::RGBA(234, 88, 12, 255);
            k.stroke = darkSurface ? DocColor::RGBA(251, 146, 60, 255) : DocColor::RGBA(194, 65, 12, 255);
            k.text = DocColor::RGBA(255, 255, 255, 255);
        } else if (up == "HOME" || up == "MENU" || up == "MODE" || up == "OPTN" || up == "CATALOG" || up == "TOOLS" || up == "FORMAT") {
            k.label = up;
            k.fill = DocColor::RGBA(30, 41, 59, 255);
            k.stroke = darkSurface ? DocColor::RGBA(100, 116, 139, 255) : DocColor::RGBA(15, 23, 42, 255);
            k.text = DocColor::RGBA(248, 250, 252, 255);
        } else if (up == "X10^X" || up == "EXP") {
            k.label = "\xC3\x97" "10\xCB\xA3"; // ×10ˣ
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else if (up == "X" || s == "*") {
            k.label = "\xC3\x97"; // ×
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else if (s == "/") {
            k.label = "\xC3\xB7"; // ÷
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else if (s == "(-)" || s == "-") {
            k.label = (s == "(-)") ? "(\xE2\x88\x92)" : "\xE2\x88\x92";
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else if (up == "RIGHT") {
            k.label = "\xE2\x96\xB6";
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else if (up == "LEFT") {
            k.label = "\xE2\x97\x80";
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else if (up == "DOWN") {
            k.label = "\xE2\x96\xBC";
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else if (up == "UP") {
            k.label = "\xE2\x96\xB2";
            k.fill = opFill;
            k.stroke = opStroke;
            k.text = opText;
        } else {
            k.label = s;
            k.fill = bracketed ? opFill : numFill;
            k.stroke = opStroke;
            k.text = opText;
        }
        out.push_back(std::move(k));
    };

    std::size_t i = 0;
    while (i < seqUtf8.size()) {
        while (i < seqUtf8.size() && (std::isspace(static_cast<unsigned char>(seqUtf8[i])) || seqUtf8[i] == ',')) {
            ++i;
        }
        if (i >= seqUtf8.size()) break;
        if (seqUtf8[i] == '[') {
            const auto close = seqUtf8.find(']', i + 1);
            if (close != std::string::npos) {
                pushTok(seqUtf8.substr(i + 1, close - i - 1), true);
                i = close + 1;
                continue;
            }
        }
        const std::size_t start = i;
        while (i < seqUtf8.size() &&
               !std::isspace(static_cast<unsigned char>(seqUtf8[i])) &&
               seqUtf8[i] != ',' && seqUtf8[i] != '[') {
            ++i;
        }
        pushTok(seqUtf8.substr(start, i - start), false);
    }
    return out;
}

inline double DrawCasioKeycapStrip(
    IMathDrawBackend& bk,
    const std::string& keySeqUtf8,
    double x,
    double topY,
    double maxWidth,
    double maxHeight,
    double fontSize,
    bool darkSurface = false)
{
    const auto caps = ParseCasioKeycaps(keySeqUtf8, darkSurface);
    if (caps.empty()) return 0.0;

    const double fs = (std::max)(6.5, fontSize * 0.84);
    const double padX = (std::max)(3.5, fs * 0.44);
    const double pillH = (std::max)(14.0, fs * 1.56);
    const double gapX = (std::max)(3.0, fs * 0.30);
    const double gapY = (std::max)(3.0, fs * 0.32);
    const double radius = (std::min)(5.0, pillH * 0.28);

    double curX = x;
    double curY = topY;
    for (const auto& cap : caps) {
        const auto tm = bk.MeasureText(cap.label, fs, FontStyleRole::KeycapLabel);
        const double itemW = cap.isArrow ? (tm.width + 2.0) : (std::max)(pillH * 0.92, tm.width + 2.0 * padX);
        if (curX > x && (curX + itemW > x + maxWidth)) {
            curX = x;
            curY += pillH + gapY;
            if (maxHeight > 0.0 && (curY + pillH - topY) > maxHeight) {
                break;
            }
        }
        if (cap.isArrow) {
            const double baseY = curY + (pillH + tm.ascent - tm.descent) * 0.5;
            bk.DrawTextRun(curX + 1.0, baseY, cap.label, fs, FontStyleRole::UiSemiBold, cap.text);
        } else {
            bk.DrawRoundedPill(curX, curY, itemW, pillH, radius, cap.fill, cap.stroke, 1.0);
            const double baseY = curY + (pillH + tm.ascent - tm.descent) * 0.5;
            bk.DrawTextRun(curX + (itemW - tm.width) * 0.5, baseY, cap.label, fs, FontStyleRole::KeycapLabel, cap.text);
        }
        curX += itemW + gapX;
    }
    return (curY + pillH - topY) + gapY;
}

inline bool TextContainsMathMarkup(const std::string& s) {
    if (s.find('$') != std::string::npos) return true;
    if (s.find("\\frac") != std::string::npos || s.find("\\sqrt") != std::string::npos ||
        s.find("\\Omega") != std::string::npos || s.find("\\theta") != std::string::npos ||
        s.find("\\pi") != std::string::npos || s.find("\\cdot") != std::string::npos ||
        s.find("\\int") != std::string::npos || s.find("\\sum") != std::string::npos ||
        s.find("\\angle") != std::string::npos || s.find("\\text{") != std::string::npos) {
        return true;
    }
    return false;
}

// Unified 2D textbook card content renderer shared across Windows D2D, Linux Cairo, and PDF export.
inline void DrawStemCardContent(
    IMathDrawBackend& bk,
    const AnnotationObject& obj,
    const DocRect& sr,
    double zoom,
    DocColor titleColor,
    DocColor bodyColor,
    const std::string& headerFallbackUtf8,
    const std::string& bodyFallbackUtf8,
    bool darkSurface = false)
{
    const double pad = (std::max)(4.0, 8.0 * zoom);
    const double titleFs = (std::max)(8.5, 11.5 * zoom);
    const double bodyFs = (std::max)(8.5, 12.2 * zoom);
    const double contentW = (std::max)(24.0, sr.w - 2.0 * pad);
    double curY = sr.y + pad * 0.75;
    const double maxY = sr.y + sr.h - pad * 0.6;

    // 1. Header row (2D typeset for MathBlock / PlotBlock formulas, clean UI semibold otherwise)
    const bool headerIsFormula = (obj.kind == ObjectKind::MathBlock || obj.kind == ObjectKind::PlotBlock);
    if (obj.kind == ObjectKind::MathBlock) {
        std::string hdr = obj.formulaExprUtf8.empty() ? headerFallbackUtf8 : obj.formulaExprUtf8;
        curY += DrawRichMathLine(bk, hdr, sr.x + pad, curY, contentW, titleFs, titleColor, true);
    } else if (obj.kind == ObjectKind::PlotBlock) {
        std::string hdr = !obj.captionUtf8.empty() ? obj.captionUtf8
                        : (!obj.formulaExprUtf8.empty() ? obj.formulaExprUtf8 : headerFallbackUtf8);
        curY += DrawRichMathLine(bk, hdr, sr.x + pad, curY, contentW, titleFs, titleColor, false);
    } else {
        const auto tm = bk.MeasureText(headerFallbackUtf8, titleFs, FontStyleRole::UiSemiBold);
        bk.DrawTextRun(sr.x + pad, curY + tm.ascent, headerFallbackUtf8, titleFs, FontStyleRole::UiSemiBold, titleColor);
        curY += tm.ascent + tm.descent + pad * 0.55;
    }
    (void)headerIsFormula;

    // Subtle hairline separator under header (except PlotBlock where plot frame sits right below)
    if (obj.kind != ObjectKind::PlotBlock && curY < maxY - 6.0) {
        DocColor sep = titleColor;
        sep.a = darkSurface ? 72 : 45;
        bk.DrawLine(sr.x + pad, curY, sr.x + sr.w - pad, curY, (std::max)(0.75, 0.85 * zoom), sep);
        curY += pad * 0.55;
    }

    if (obj.kind == ObjectKind::PlotBlock) {
        return; // Plot frame & curves are drawn by caller below header
    }

    // 2. CalcSessionCard: graphical Casio keycaps + 2D formula/result (reserving right space if LCD PNG present)
    if (obj.kind == ObjectKind::CalcSessionCard) {
        double textW = contentW;
        if (!obj.pngBytes.empty()) {
            const double imgW = (std::min)(sr.w * 0.42, 130.0 * zoom);
            if (imgW > 18.0) textW = (std::max)(36.0, contentW - imgW - pad);
        }
        if (!obj.calcKeySeqUtf8.empty() && curY < maxY - 12.0) {
            curY += DrawCasioKeycapStrip(bk, obj.calcKeySeqUtf8, sr.x + pad, curY,
                                         textW, (std::max)(16.0, (maxY - curY) * 0.52), bodyFs * 0.90, darkSurface);
        }
        if (!obj.calcLatexUtf8.empty() && curY < maxY - 10.0) {
            curY += DrawRichMathLine(bk, obj.calcLatexUtf8, sr.x + pad, curY, textW, bodyFs, bodyColor, true);
        }
        const std::string resLine = !obj.evaluatedResultUtf8.empty() ? ("= " + obj.evaluatedResultUtf8) : obj.textUtf8;
        if (!resLine.empty() && curY < maxY - 8.0) {
            curY += DrawRichMathLine(bk, resLine, sr.x + pad, curY, textW, bodyFs * 1.05, titleColor, true);
        }
        return;
    }

    // 3. Body lines (MathBlock / CircuitBlock / StudyTape) rendered in 2D textbook notation
    const bool forceMathBody = (obj.kind == ObjectKind::MathBlock);
    std::istringstream iss(bodyFallbackUtf8);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (curY >= maxY - 4.0) break;
        if (line.empty()) {
            curY += bodyFs * 0.5;
            continue;
        }
        // Check-My-Work status badge pill
        if (obj.kind == ObjectKind::MathBlock && line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            const bool isErr = (line.find("ERR") != std::string::npos || line.find("DIFF") != std::string::npos);
            const DocColor pillFill = darkSurface
                ? (isErr ? DocColor::RGBA(96, 26, 36, 240) : DocColor::RGBA(20, 74, 48, 240))
                : (isErr ? DocColor::RGBA(254, 226, 226, 255) : DocColor::RGBA(220, 252, 231, 255));
            const DocColor pillBorder = isErr ? DocColor::RGBA(239, 68, 68, 255) : DocColor::RGBA(34, 197, 94, 255);
            const DocColor pillText = darkSurface
                ? (isErr ? DocColor::RGBA(254, 202, 202, 255) : DocColor::RGBA(187, 247, 208, 255))
                : (isErr ? DocColor::RGBA(153, 27, 27, 255) : DocColor::RGBA(21, 128, 61, 255));
            const std::string badge = line.substr(1, line.size() - 2);
            const double badgeFs = (std::max)(7.5, bodyFs * 0.80);
            const auto tm = bk.MeasureText(badge, badgeFs, FontStyleRole::UiSemiBold);
            const double pillH = tm.ascent + tm.descent + 5.0;
            const double pillW = (std::min)(contentW, tm.width + 12.0);
            bk.DrawRoundedPill(sr.x + pad, curY, pillW, pillH, pillH * 0.5, pillFill, pillBorder, 1.0);
            bk.DrawTextRun(sr.x + pad + 6.0, curY + 2.5 + tm.ascent, badge, badgeFs, FontStyleRole::UiSemiBold, pillText);
            curY += pillH + 4.0;
            continue;
        }
        // Split long circuit oscilloscope telemetry lines ("Prefix: Vmax=..., Vmin=..., Vrms=..., Vdc=...")
        // onto two crisp readable lines instead of shrinking everything to micro-text.
        if (obj.kind == ObjectKind::CircuitBlock) {
            const auto colon = line.find(": ");
            if (colon != std::string::npos && colon > 0 && line.find(", ", colon) != std::string::npos &&
                line.find('=', colon) != std::string::npos && line.size() > 54) {
                const std::string prefix = line.substr(0, colon);
                const std::string metrics = line.substr(colon + 2);
                const double lblFs = (std::max)(8.0, bodyFs * 0.88);
                const auto tm = bk.MeasureText(prefix, lblFs, FontStyleRole::UiSemiBold);
                bk.DrawTextRun(sr.x + pad, curY + tm.ascent, prefix, lblFs, FontStyleRole::UiSemiBold, titleColor);
                curY += tm.ascent + tm.descent + 3.0;
                if (curY < maxY - 4.0) {
                    curY += DrawRichMathLine(bk, metrics, sr.x + pad, curY, contentW, bodyFs * 0.94, bodyColor, false);
                }
                continue;
            }
        }
        const bool isCircuitFormula = (obj.kind == ObjectKind::CircuitBlock);
        curY += DrawRichMathLine(bk, line, sr.x + pad, curY, contentW, bodyFs, bodyColor, forceMathBody || isCircuitFormula);
    }
}

} // namespace DocStudio::MathTypeset
