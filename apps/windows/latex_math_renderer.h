#pragma once
// ============================================================================
// latex_math_renderer.h
// Motor Nativo Win32 GDI para Subconjunto LaTeX Matematico 2D (AST)
// Arquitectura en 3 fases desacopladas: Analizar (AST) -> Medir -> Dibujar.
// Soporta fracciones anidadas (\frac{a}{b}), radicales (\sqrt[n]{x}),
// parentesis adaptativos con superindices ((\frac{a}{b})^2), operadores grandes
// (\int_a^b, \sum, \lim), super/subindices, acentos (\vec, \bar, \hat),
// letras griegas, constantes fisicas y validacion sintactica (ValidateLatexRichText).
// ============================================================================

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cwctype>
#include <type_traits>

struct LatexNode {
    enum Type {
        NODE_SEQ = 0,
        NODE_TEXT,
        NODE_MATH_VAR,
        NODE_MATH_OP,
        NODE_FRAC,
        NODE_SQRT,
        NODE_BIGOP,
        NODE_SUPSUB,
        NODE_ACCENT,
        NODE_PAREN_GROUP,
        NODE_MATRIX
    };

    Type type = NODE_SEQ;
    std::wstring text;
    std::wstring closeText; // used by NODE_PAREN_GROUP, e.g. "(" / ")"
    bool bold = false;
    bool italic = false;
    bool forceUpright = false;
    std::vector<std::shared_ptr<LatexNode>> children;
    std::vector<std::vector<std::shared_ptr<LatexNode>>> rows;
    std::vector<int> colWidths;
    std::vector<int> rowAscents;
    std::vector<int> rowDescents;
    std::shared_ptr<LatexNode> num;   // frac num / sqrt radicand / base / inner group
    std::shared_ptr<LatexNode> den;   // frac den / sqrt index
    std::shared_ptr<LatexNode> sup;   // superscript / upper limit
    std::shared_ptr<LatexNode> sub;   // subscript / lower limit

    int width = 0;
    int height = 0;
    int ascent = 0;  // distance from top of box to baseline
};

struct LatexValidationResult {
    bool valid = true;
    std::wstring diagnostic = L"F\u00F3rmula 2D v\u00E1lida";
};

namespace LatexEngine {

template <typename T, typename U>
inline auto LMax(T a, U b) -> typename std::common_type<T, U>::type {
    using R = typename std::common_type<T, U>::type;
    return (static_cast<R>(a) > static_cast<R>(b)) ? static_cast<R>(a) : static_cast<R>(b);
}
template <typename T, typename U>
inline auto LMin(T a, U b) -> typename std::common_type<T, U>::type {
    using R = typename std::common_type<T, U>::type;
    return (static_cast<R>(a) < static_cast<R>(b)) ? static_cast<R>(a) : static_cast<R>(b);
}

inline HFONT CreateMathFont(int pixelHeight, bool italic, bool bold, bool mathSerif = true) {
    return CreateFontW(
        -LMax(8, pixelHeight), 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        italic ? TRUE : FALSE,
        FALSE, FALSE, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        mathSerif ? L"Cambria" : L"Segoe UI"
    );
}

inline std::wstring LookupLatexSymbol(const std::wstring& cmd, bool& outIsFunc, bool& outIsBigOp) {
    outIsFunc = false;
    outIsBigOp = false;

    if (cmd == L"int" || cmd == L"oint") { outIsBigOp = true; return cmd == L"oint" ? L"\x222E" : L"\x222B"; }
    if (cmd == L"sum")  { outIsBigOp = true; return L"\x2211"; }
    if (cmd == L"prod") { outIsBigOp = true; return L"\x220F"; }
    if (cmd == L"lim")  { outIsBigOp = true; outIsFunc = true; return L"lim"; }

    // Funciones matematicas rectas
    if (cmd == L"sin" || cmd == L"sen" || cmd == L"cos" || cmd == L"tan" || cmd == L"tg" ||
        cmd == L"sec" || cmd == L"csc" || cmd == L"cot" ||
        cmd == L"arcsin" || cmd == L"arccos" || cmd == L"arctan" ||
        cmd == L"arsinh" || cmd == L"arcosh" || cmd == L"artanh" ||
        cmd == L"sinh" || cmd == L"cosh" || cmd == L"tanh" ||
        cmd == L"coth" || cmd == L"sech" || cmd == L"csch" ||
        cmd == L"log" || cmd == L"ln" || cmd == L"lg" || cmd == L"exp" ||
        cmd == L"det" || cmd == L"dim" || cmd == L"max" || cmd == L"min" ||
        cmd == L"sup" || cmd == L"inf" || cmd == L"arg" || cmd == L"Arg" || cmd == L"deg" ||
        cmd == L"gcd" || cmd == L"lcm" || cmd == L"mod" || cmd == L"Pol" || cmd == L"Rec" ||
        cmd == L"Abs" || cmd == L"nPr" || cmd == L"nCr" || cmd == L"Rnd" ||
        cmd == L"Int" || cmd == L"Frac" || cmd == L"conj" || cmd == L"Conjg") {
        outIsFunc = true;
        return cmd;
    }

    // Alfabeto griego minusculas
    if (cmd == L"alpha")      return L"\x03B1";
    if (cmd == L"beta")       return L"\x03B2";
    if (cmd == L"gamma")      return L"\x03B3";
    if (cmd == L"delta")      return L"\x03B4";
    if (cmd == L"epsilon")    return L"\x03B5";
    if (cmd == L"varepsilon") return L"\x03B5";
    if (cmd == L"zeta")       return L"\x03B6";
    if (cmd == L"eta")        return L"\x03B7";
    if (cmd == L"theta")      return L"\x03B8";
    if (cmd == L"vartheta")   return L"\x03D1";
    if (cmd == L"iota")       return L"\x03B9";
    if (cmd == L"kappa")      return L"\x03BA";
    if (cmd == L"lambda")     return L"\x03BB";
    if (cmd == L"mu")         return L"\x03BC";
    if (cmd == L"nu")         return L"\x03BD";
    if (cmd == L"xi")         return L"\x03BE";
    if (cmd == L"pi")         return L"\x03C0";
    if (cmd == L"rho")        return L"\x03C1";
    if (cmd == L"sigma")      return L"\x03C3";
    if (cmd == L"tau")        return L"\x03C4";
    if (cmd == L"upsilon")    return L"\x03C5";
    if (cmd == L"phi")        return L"\x03D5";
    if (cmd == L"varphi")     return L"\x03C6";
    if (cmd == L"chi")        return L"\x03C7";
    if (cmd == L"psi")        return L"\x03C8";
    if (cmd == L"omega")      return L"\x03C9";

    // Alfabeto griego mayusculas
    if (cmd == L"Gamma")   return L"\x0393";
    if (cmd == L"Delta")   return L"\x0394";
    if (cmd == L"Theta")   return L"\x0398";
    if (cmd == L"Lambda")  return L"\x039B";
    if (cmd == L"Xi")      return L"\x039E";
    if (cmd == L"Pi")      return L"\x03A0";
    if (cmd == L"Sigma")   return L"\x03A3";
    if (cmd == L"Upsilon") return L"\x03A5";
    if (cmd == L"Phi")     return L"\x03A6";
    if (cmd == L"Psi")     return L"\x03A8";
    if (cmd == L"Omega")   return L"\x03A9";

    // Operadores matematicos y fisicos
    if (cmd == L"times")          return L" \u00D7 ";
    if (cmd == L"div")            return L" \u00F7 ";
    if (cmd == L"cdot")           return L" \u00B7 ";
    if (cmd == L"pm")             return L" \u00B1 ";
    if (cmd == L"mp")             return L" \x2213 ";
    if (cmd == L"le" || cmd == L"leq") return L" \x2264 ";
    if (cmd == L"ge" || cmd == L"geq") return L" \x2265 ";
    if (cmd == L"ne" || cmd == L"neq") return L" \x2260 ";
    if (cmd == L"approx")         return L" \x2248 ";
    if (cmd == L"equiv")          return L" \x2261 ";
    if (cmd == L"sim")            return L" \x223C ";
    if (cmd == L"propto")         return L" \x221D ";
    if (cmd == L"infty")          return L"\x221E";
    if (cmd == L"partial")        return L"\x2202";
    if (cmd == L"nabla")          return L"\x2207";
    if (cmd == L"forall")         return L"\x2200";
    if (cmd == L"exists")         return L"\x2203";
    if (cmd == L"in")             return L" \x2208 ";
    if (cmd == L"notin")          return L" \x2209 ";
    if (cmd == L"subset")         return L" \x2282 ";
    if (cmd == L"subseteq")       return L" \x2286 ";
    if (cmd == L"cup")            return L" \x222A ";
    if (cmd == L"cap")            return L" \x2229 ";
    if (cmd == L"emptyset")       return L"\x2205";
    if (cmd == L"rightarrow" || cmd == L"to" || cmd == L"longrightarrow") return L" \x2192 ";
    if (cmd == L"leftarrow" || cmd == L"hookleftarrow") return L" \x2190 ";
    if (cmd == L"uparrow")        return L"\x2191";
    if (cmd == L"downarrow")      return L"\x2193";
    if (cmd == L"leftrightarrow") return L" \x2194 ";
    if (cmd == L"Rightarrow" || cmd == L"implies") return L" \x21D2 ";
    if (cmd == L"Leftrightarrow" || cmd == L"iff" || cmd == L"Longleftrightarrow") return L" \x21D4 ";
    if (cmd == L"lfloor")         return L"\x230A";
    if (cmd == L"rfloor")         return L"\x230B";
    if (cmd == L"lceil")          return L"\x2308";
    if (cmd == L"rceil")          return L"\x2309";
    if (cmd == L"angle")          return L"\x2220";
    if (cmd == L"perp")           return L"\x22A5";
    if (cmd == L"parallel")       return L"\x2225";
    if (cmd == L"degree" || cmd == L"circ") return L"\u00B0";
    if (cmd == L"angstrom")       return L"\u00C5";
    if (cmd == L"ohm")            return L"\x03A9";
    if (cmd == L"prime")          return L"\x2032";
    if (cmd == L"Box" || cmd == L"square") return L"\x25A1";
    if (cmd == L"blacksquare")    return L"\x25A0";
    if (cmd == L"therefore")      return L" \x2234 ";
    if (cmd == L"because")        return L" \x2235 ";
    if (cmd == L"nexists")        return L"\x2204";
    if (cmd == L"cong")           return L" \x2245 ";
    if (cmd == L"simeq")          return L" \x2243 ";
    if (cmd == L"ll")             return L" \x226A ";
    if (cmd == L"gg")             return L" \x226B ";
    if (cmd == L"supset")         return L" \x2283 ";
    if (cmd == L"supseteq")       return L" \x2287 ";
    if (cmd == L"setminus")       return L" \x2216 ";
    if (cmd == L"oplus")          return L" \x2295 ";
    if (cmd == L"otimes")         return L" \x2297 ";
    if (cmd == L"bullet")         return L"\x2022";
    if (cmd == L"star")           return L"\x22C6";
    if (cmd == L"ast")            return L"\x2217";
    if (cmd == L"neg" || cmd == L"lnot") return L"\u00AC";
    if (cmd == L"wedge" || cmd == L"land") return L" \x2227 ";
    if (cmd == L"vee" || cmd == L"lor")    return L" \x2228 ";
    if (cmd == L"dagger")         return L"\x2020";
    if (cmd == L"ddagger")        return L"\x2021";
    if (cmd == L"hbar")           return L"\x210F";
    if (cmd == L"ell")            return L"\x2113";
    if (cmd == L"Re")             return L"\x211C";
    if (cmd == L"Im")             return L"\x2111";
    if (cmd == L"dots" || cmd == L"cdots" || cmd == L"ldots") return L"\x2026";
    if (cmd == L"langle") return L"\x27E8";
    if (cmd == L"rangle") return L"\x27E9";
    if (cmd == L"," || cmd == L";" || cmd == L":" || cmd == L" ") return L" ";
    if (cmd == L"!") return L"";
    if (cmd == L"quad" || cmd == L"qquad") return L"   ";
    if (cmd == L"left" || cmd == L"right" || cmd == L"big" || cmd == L"Big" || cmd == L"bigg" || cmd == L"Bigg") return L"";
    if (cmd == L"{" || cmd == L"}" || cmd == L"$" || cmd == L"%" || cmd == L"#" || cmd == L"&") return cmd;
    if (cmd == L"|") return L"\x2016";

    return L"";
}

inline bool IsKnownStructuralCommand(const std::wstring& cmd) {
    return cmd == L"frac" || cmd == L"dfrac" || cmd == L"tfrac" || cmd == L"binom" || cmd == L"sqrt" ||
           cmd == L"vec" || cmd == L"bar" || cmd == L"hat" || cmd == L"widehat" ||
           cmd == L"overline" || cmd == L"tilde" || cmd == L"widetilde" || cmd == L"dot" || cmd == L"ddot" ||
           cmd == L"text" || cmd == L"mathrm" || cmd == L"operatorname" ||
           cmd == L"mathbf" || cmd == L"boldsymbol" || cmd == L"mathbb" ||
           cmd == L"begin" || cmd == L"end";
}

inline bool IsSupportedLatexCommand(const std::wstring& cmd) {
    if (cmd.empty()) return false;
    if (IsKnownStructuralCommand(cmd)) return true;
    bool isF = false, isB = false;
    if (!LookupLatexSymbol(cmd, isF, isB).empty()) return true;
    return cmd == L"!" || cmd == L"left" || cmd == L"right" ||
           cmd == L"big" || cmd == L"Big" || cmd == L"bigg" || cmd == L"Bigg";
}

inline void SkipSpaces(const std::wstring& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == L' ' || s[pos] == L'\t')) ++pos;
}

inline std::shared_ptr<LatexNode> ParseMathSequence(const std::wstring& s, size_t& pos, wchar_t stopChar);

inline std::shared_ptr<LatexNode> ParseMathGroupOrAtom(const std::wstring& s, size_t& pos) {
    SkipSpaces(s, pos);
    if (pos >= s.size()) {
        auto empty = std::make_shared<LatexNode>();
        empty->type = LatexNode::NODE_TEXT;
        return empty;
    }

    if (s[pos] == L'{') {
        ++pos;
        return ParseMathSequence(s, pos, L'}');
    }

    // Balanced parentheses inside math mode: enables tall delimiting & ( \frac{a}{b} )^2
    if (s[pos] == L'(' || s[pos] == L'[') {
        wchar_t openCh = s[pos];
        wchar_t closeCh = (openCh == L'(') ? L')' : L']';
        // Only treat as delimited group if matching closer exists at balanced brace depth
        size_t scan = pos + 1;
        int braceDepth = 0;
        int parenDepth = 1;
        bool hasMatch = false;
        while (scan < s.size()) {
            if (s[scan] == L'\\' && scan + 1 < s.size()) { scan += 2; continue; }
            if (s[scan] == L'{') ++braceDepth;
            else if (s[scan] == L'}') { if (braceDepth == 0) break; --braceDepth; }
            else if (braceDepth == 0) {
                if (s[scan] == openCh) ++parenDepth;
                else if (s[scan] == closeCh) {
                    --parenDepth;
                    if (parenDepth == 0) { hasMatch = true; break; }
                }
            }
            ++scan;
        }
        if (hasMatch) {
            ++pos;
            auto grp = std::make_shared<LatexNode>();
            grp->type = LatexNode::NODE_PAREN_GROUP;
            grp->text = std::wstring(1, openCh);
            grp->closeText = std::wstring(1, closeCh);
            grp->num = ParseMathSequence(s, pos, closeCh);
            return grp;
        }
    }

    if (s[pos] == L'\\') {
        ++pos;
        if (pos >= s.size()) {
            auto n = std::make_shared<LatexNode>();
            n->type = LatexNode::NODE_TEXT;
            return n;
        }
        if (!iswalpha(s[pos])) {
            std::wstring sc(1, s[pos++]);
            bool isF = false, isB = false;
            std::wstring sym = LookupLatexSymbol(sc, isF, isB);
            auto n = std::make_shared<LatexNode>();
            n->type = LatexNode::NODE_MATH_OP;
            n->text = (sc == L"!") ? L"" : (sym.empty() ? sc : sym);
            return n;
        }
        size_t start = pos;
        while (pos < s.size() && iswalpha(s[pos])) ++pos;
        std::wstring cmd = s.substr(start, pos - start);

        auto readDelimiterToken = [&]() -> std::wstring {
            SkipSpaces(s, pos);
            if (pos >= s.size()) return L"";
            if (s[pos] == L'\\') {
                ++pos;
                if (pos >= s.size()) return L"";
                if (s[pos] == L'|') { ++pos; return L"\x2016"; }
                if (s[pos] == L'{' || s[pos] == L'}') return std::wstring(1, s[pos++]);
                if (!iswalpha(s[pos])) return std::wstring(1, s[pos++]);
                size_t dStart = pos;
                while (pos < s.size() && iswalpha(s[pos])) ++pos;
                std::wstring dCmd = s.substr(dStart, pos - dStart);
                if (dCmd == L"langle") return L"\x27E8";
                if (dCmd == L"rangle") return L"\x27E9";
                if (dCmd == L"lfloor") return L"\x230A";
                if (dCmd == L"rfloor") return L"\x230B";
                if (dCmd == L"lceil")  return L"\x2308";
                if (dCmd == L"rceil")  return L"\x2309";
                return L"|";
            }
            if (s[pos] == L'(' || s[pos] == L')' || s[pos] == L'[' || s[pos] == L']' || s[pos] == L'|' || s[pos] == L'.') {
                return std::wstring(1, s[pos++]);
            }
            return L"";
        };

        auto readBraceTextArg = [&]() -> std::wstring {
            SkipSpaces(s, pos);
            if (pos < s.size() && s[pos] == L'{') {
                ++pos;
                size_t st = pos;
                while (pos < s.size() && s[pos] != L'}') ++pos;
                std::wstring out = s.substr(st, pos - st);
                if (pos < s.size() && s[pos] == L'}') ++pos;
                return out;
            }
            return L"";
        };

        // Adaptive \left( ... \right) / \left[ ... \right] / \left. ... \right| delimiters
        if (cmd == L"left") {
            size_t savePos = pos;
            std::wstring openDelim = readDelimiterToken();
            if (!openDelim.empty()) {
                std::wstring defaultClose = L")";
                if (openDelim == L"[") defaultClose = L"]";
                else if (openDelim == L"|") defaultClose = L"|";
                else if (openDelim == L"\x2016") defaultClose = L"\x2016";
                else if (openDelim == L"{") defaultClose = L"}";
                else if (openDelim == L"\x27E8") defaultClose = L"\x27E9";
                else if (openDelim == L".") defaultClose = L".";

                auto grp = std::make_shared<LatexNode>();
                grp->type = LatexNode::NODE_PAREN_GROUP;
                grp->text = openDelim;
                grp->closeText = defaultClose;
                auto inner = std::make_shared<LatexNode>();
                inner->type = LatexNode::NODE_SEQ;
                // Parse until matching \right<delim>
                while (pos < s.size()) {
                    SkipSpaces(s, pos);
                    if (s.compare(pos, 6, L"\\right") == 0 && (pos + 6 >= s.size() || !iswalpha(s[pos + 6]))) {
                        pos += 6;
                        std::wstring parsedClose = readDelimiterToken();
                        if (!parsedClose.empty()) grp->closeText = parsedClose;
                        break;
                    }
                    auto subSeq = ParseMathSequence(s, pos, L'\\');
                    for (auto& c : subSeq->children) inner->children.push_back(c);
                    if (pos > 0 && pos <= s.size() && s[pos - 1] == L'\\') {
                        --pos;
                        if (s.compare(pos, 6, L"\\right") == 0 && (pos + 6 >= s.size() || !iswalpha(s[pos + 6]))) {
                            continue;
                        }
                        auto atom = ParseMathGroupOrAtom(s, pos);
                        std::shared_ptr<LatexNode> supN, subN;
                        for (int k = 0; k < 2; ++k) {
                            SkipSpaces(s, pos);
                            if (pos < s.size() && s[pos] == L'^') { ++pos; supN = ParseMathGroupOrAtom(s, pos); }
                            else if (pos < s.size() && s[pos] == L'_') { ++pos; subN = ParseMathGroupOrAtom(s, pos); }
                            else break;
                        }
                        if (supN || subN) {
                            if (atom->type == LatexNode::NODE_BIGOP) {
                                atom->sup = supN;
                                atom->sub = subN;
                                inner->children.push_back(atom);
                            } else {
                                auto ss = std::make_shared<LatexNode>();
                                ss->type = LatexNode::NODE_SUPSUB;
                                ss->num = atom;
                                ss->sup = supN;
                                ss->sub = subN;
                                inner->children.push_back(ss);
                            }
                        } else {
                            inner->children.push_back(atom);
                        }
                    } else {
                        break;
                    }
                }
                grp->num = inner;
                return grp;
            }
            pos = savePos;
        }

        if (cmd == L"begin") {
            std::wstring env = readBraceTextArg();
            auto mat = std::make_shared<LatexNode>();
            mat->type = LatexNode::NODE_MATRIX;
            mat->text = env;

            std::vector<std::wstring> rawCells;
            std::vector<std::vector<std::wstring>> rawRows;
            size_t cellStart = pos;
            int depthBrace = 0;
            int depthEnv = 0;
            auto pushCell = [&](size_t endIdx) {
                rawCells.push_back(s.substr(cellStart, endIdx - cellStart));
            };
            while (pos < s.size()) {
                if (s[pos] == L'{') { ++depthBrace; ++pos; continue; }
                if (s[pos] == L'}') { if (depthBrace > 0) --depthBrace; ++pos; continue; }
                if (depthBrace == 0 && s.compare(pos, 6, L"\\begin") == 0 && (pos + 6 >= s.size() || !iswalpha(s[pos + 6]))) {
                    ++depthEnv;
                    pos += 6;
                    continue;
                }
                if (depthBrace == 0 && s.compare(pos, 4, L"\\end") == 0 && (pos + 4 >= s.size() || !iswalpha(s[pos + 4]))) {
                    if (depthEnv == 0) {
                        pushCell(pos);
                        if (!rawCells.empty()) rawRows.push_back(std::move(rawCells));
                        pos += 4;
                        readBraceTextArg();
                        break;
                    }
                    --depthEnv;
                    pos += 4;
                    continue;
                }
                if (depthBrace == 0 && depthEnv == 0) {
                    if (s[pos] == L'&') {
                        pushCell(pos);
                        ++pos;
                        cellStart = pos;
                        continue;
                    }
                    if (s.compare(pos, 2, L"\\\\") == 0) {
                        pushCell(pos);
                        rawRows.push_back(std::move(rawCells));
                        rawCells.clear();
                        pos += 2;
                        cellStart = pos;
                        continue;
                    }
                }
                ++pos;
            }
            if (!rawCells.empty()) rawRows.push_back(std::move(rawCells));
            for (const auto& rr : rawRows) {
                std::vector<std::shared_ptr<LatexNode>> parsedRow;
                for (const auto& cStr : rr) {
                    size_t cp = 0;
                    parsedRow.push_back(ParseMathSequence(cStr, cp, 0));
                }
                mat->rows.push_back(std::move(parsedRow));
            }

            std::wstring oDel = L".", cDel = L".";
            if (env == L"pmatrix") { oDel = L"("; cDel = L")"; }
            else if (env == L"bmatrix") { oDel = L"["; cDel = L"]"; }
            else if (env == L"vmatrix") { oDel = L"|"; cDel = L"|"; }
            else if (env == L"Vmatrix") { oDel = L"\x2016"; cDel = L"\x2016"; }
            else if (env == L"Bmatrix") { oDel = L"{"; cDel = L"}"; }
            else if (env == L"cases")   { oDel = L"{"; cDel = L"."; }
            if (oDel != L"." || cDel != L".") {
                auto grp = std::make_shared<LatexNode>();
                grp->type = LatexNode::NODE_PAREN_GROUP;
                grp->text = oDel;
                grp->closeText = cDel;
                grp->num = mat;
                return grp;
            }
            return mat;
        }

        if (cmd == L"frac" || cmd == L"dfrac" || cmd == L"tfrac") {
            auto n = std::make_shared<LatexNode>();
            n->type = LatexNode::NODE_FRAC;
            n->num = ParseMathGroupOrAtom(s, pos);
            n->den = ParseMathGroupOrAtom(s, pos);
            return n;
        }
        if (cmd == L"binom") {
            auto mat = std::make_shared<LatexNode>();
            mat->type = LatexNode::NODE_MATRIX;
            mat->text = L"binom";
            mat->rows.push_back({ ParseMathGroupOrAtom(s, pos) });
            mat->rows.push_back({ ParseMathGroupOrAtom(s, pos) });
            auto grp = std::make_shared<LatexNode>();
            grp->type = LatexNode::NODE_PAREN_GROUP;
            grp->text = L"(";
            grp->closeText = L")";
            grp->num = mat;
            return grp;
        }
        if (cmd == L"sqrt") {
            auto n = std::make_shared<LatexNode>();
            n->type = LatexNode::NODE_SQRT;
            SkipSpaces(s, pos);
            if (pos < s.size() && s[pos] == L'[') {
                ++pos;
                n->den = ParseMathSequence(s, pos, L']');
            }
            n->num = ParseMathGroupOrAtom(s, pos);
            return n;
        }
        if (cmd == L"vec" || cmd == L"bar" || cmd == L"hat" || cmd == L"widehat" ||
            cmd == L"overline" || cmd == L"tilde" || cmd == L"widetilde" ||
            cmd == L"dot" || cmd == L"ddot") {
            auto n = std::make_shared<LatexNode>();
            n->type = LatexNode::NODE_ACCENT;
            n->text = cmd;
            n->num = ParseMathGroupOrAtom(s, pos);
            return n;
        }
        if (cmd == L"text" || cmd == L"mathrm" || cmd == L"operatorname") {
            auto grp = ParseMathGroupOrAtom(s, pos);
            grp->italic = false;
            grp->forceUpright = true;
            return grp;
        }
        if (cmd == L"mathbf" || cmd == L"boldsymbol") {
            auto grp = ParseMathGroupOrAtom(s, pos);
            grp->bold = true;
            return grp;
        }
        if (cmd == L"mathbb") {
            auto grp = ParseMathGroupOrAtom(s, pos);
            std::wstring ch = grp->text;
            if (!grp->children.empty()) ch = grp->children[0]->text;
            auto n = std::make_shared<LatexNode>();
            n->type = LatexNode::NODE_MATH_OP;
            if (ch == L"C") n->text = L"\x2102";
            else if (ch == L"R") n->text = L"\x211D";
            else if (ch == L"N") n->text = L"\x2115";
            else if (ch == L"Z") n->text = L"\x2124";
            else if (ch == L"Q") n->text = L"\x211A";
            else n->text = ch;
            return n;
        }

        bool isFunc = false, isBigOp = false;
        std::wstring sym = LookupLatexSymbol(cmd, isFunc, isBigOp);
        if (isBigOp) {
            auto n = std::make_shared<LatexNode>();
            n->type = LatexNode::NODE_BIGOP;
            n->text = sym;
            return n;
        }
        auto n = std::make_shared<LatexNode>();
        n->type = isFunc ? LatexNode::NODE_MATH_OP : LatexNode::NODE_MATH_VAR;
        n->italic = !isFunc && (
            cmd == L"alpha" || cmd == L"beta" || cmd == L"gamma" || cmd == L"delta" ||
            cmd == L"epsilon" || cmd == L"varepsilon" || cmd == L"zeta" || cmd == L"eta" ||
            cmd == L"theta" || cmd == L"vartheta" || cmd == L"iota" || cmd == L"kappa" ||
            cmd == L"lambda" || cmd == L"mu" || cmd == L"nu" || cmd == L"xi" ||
            cmd == L"pi" || cmd == L"rho" || cmd == L"sigma" || cmd == L"tau" ||
            cmd == L"upsilon" || cmd == L"phi" || cmd == L"varphi" || cmd == L"chi" ||
            cmd == L"psi" || cmd == L"omega");
        n->text = sym.empty() ? cmd : sym;
        return n;
    }

    wchar_t ch = s[pos++];
    auto n = std::make_shared<LatexNode>();
    if (iswalpha(ch)) {
        n->type = LatexNode::NODE_MATH_VAR;
        n->italic = true;
        n->text = std::wstring(1, ch);
    } else if (ch == L'+' || ch == L'-' || ch == L'=' || ch == L'<' || ch == L'>') {
        n->type = LatexNode::NODE_MATH_OP;
        n->text = std::wstring(L" ") + (ch == L'-' ? L"\x2212" : std::wstring(1, ch)) + L" ";
    } else {
        n->type = LatexNode::NODE_MATH_OP;
        n->text = std::wstring(1, ch);
    }
    return n;
}

inline std::shared_ptr<LatexNode> ParseMathSequence(const std::wstring& s, size_t& pos, wchar_t stopChar) {
    auto seq = std::make_shared<LatexNode>();
    seq->type = LatexNode::NODE_SEQ;

    while (pos < s.size()) {
        if (stopChar != 0 && s[pos] == stopChar) {
            ++pos;
            break;
        }
        if (s[pos] == L' ' || s[pos] == L'\t') {
            ++pos;
            continue;
        }

        auto atom = ParseMathGroupOrAtom(s, pos);

        // Revisar si le siguen ^ o _ (superindice / subindice / limites de integral)
        std::shared_ptr<LatexNode> supNode = nullptr;
        std::shared_ptr<LatexNode> subNode = nullptr;
        for (int k = 0; k < 2; ++k) {
            SkipSpaces(s, pos);
            if (pos < s.size() && s[pos] == L'^') {
                ++pos;
                supNode = ParseMathGroupOrAtom(s, pos);
            } else if (pos < s.size() && s[pos] == L'_') {
                ++pos;
                subNode = ParseMathGroupOrAtom(s, pos);
            } else {
                break;
            }
        }

        if (supNode || subNode) {
            if (atom->type == LatexNode::NODE_BIGOP) {
                atom->sup = supNode;
                atom->sub = subNode;
                seq->children.push_back(atom);
            } else {
                auto ss = std::make_shared<LatexNode>();
                ss->type = LatexNode::NODE_SUPSUB;
                ss->num = atom;
                ss->sup = supNode;
                ss->sub = subNode;
                seq->children.push_back(ss);
            }
        } else {
            seq->children.push_back(atom);
        }
    }
    return seq;
}

// Shared layout calculations so MeasureNode and DrawNode never disagree.
struct SupSubMetrics {
    int baseW = 0;
    int baseAsc = 0;
    int baseDesc = 0;
    int scriptFont = 9;
    int supBaselineAboveMain = 0; // positive = above baselineY
    int subBaselineBelowMain = 0; // positive = below baselineY
    int totalW = 0;
    int ascent = 0;
    int height = 0;
};

inline SupSubMetrics ComputeSupSubMetrics(const std::shared_ptr<LatexNode>& node, int fontSize) {
    SupSubMetrics m{};
    m.scriptFont = LMax(9, (fontSize * 72) / 100);
    m.baseW = node->num ? node->num->width : 0;
    m.baseAsc = node->num ? node->num->ascent : (fontSize * 4) / 5;
    m.baseDesc = node->num ? LMax(0, node->num->height - m.baseAsc) : (fontSize / 5);

    int supAsc = node->sup ? node->sup->ascent : 0;
    int supDesc = node->sup ? LMax(0, node->sup->height - supAsc) : 0;
    int subAsc = node->sub ? node->sub->ascent : 0;
    int subDesc = node->sub ? LMax(0, node->sub->height - subAsc) : 0;

    // Position sup relative to actual top of base when base is tall (fractions, paren groups, roots)
    int baseAnchorSup = m.baseAsc - LMax(2, fontSize / 4);
    m.supBaselineAboveMain = node->sup ? LMax((fontSize * 48) / 100, baseAnchorSup) : 0;
    int baseAnchorSub = (m.baseDesc > fontSize / 3) ? (m.baseDesc - 1) : ((fontSize * 28) / 100);
    m.subBaselineBelowMain = node->sub ? LMax((fontSize * 28) / 100, baseAnchorSub) : 0;

    int supTopAbove = node->sup ? (m.supBaselineAboveMain + supAsc) : 0;
    int subBotBelow = node->sub ? (m.subBaselineBelowMain + subDesc) : 0;
    (void)supDesc;
    (void)subAsc;

    int scriptW = LMax(node->sup ? node->sup->width : 0, node->sub ? node->sub->width : 0);
    m.totalW = m.baseW + scriptW + 1;
    m.ascent = LMax(m.baseAsc, supTopAbove);
    int maxDesc = LMax(m.baseDesc, subBotBelow);
    m.height = m.ascent + LMax(2, maxDesc);
    return m;
}

struct BigOpMetrics {
    bool isLim = false;
    int opFont = 15;
    int limFont = 10;
    SIZE opSize{ 0, 0 };
    TEXTMETRICW opTm{};
    int opTopAboveBase = 0;
    int limOffsetX = 0;
    int supBaselineAboveMain = 0;
    int subBaselineBelowMain = 0;
    int width = 0;
    int ascent = 0;
    int height = 0;
};

inline BigOpMetrics ComputeBigOpMetrics(HDC hdc, const std::shared_ptr<LatexNode>& node, int fontSize) {
    BigOpMetrics bm{};
    bm.isLim = (node->text == L"lim");
    bm.opFont = bm.isLim ? fontSize : (fontSize * 145) / 100;
    bm.limFont = LMax(9, (fontSize * 70) / 100);

    HFONT hFont = CreateMathFont(bm.opFont, false, false, true);
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    GetTextExtentPoint32W(hdc, node->text.c_str(), (int)node->text.size(), &bm.opSize);
    GetTextMetricsW(hdc, &bm.opTm);
    SelectObject(hdc, hOld);
    DeleteObject(hFont);

    if (bm.isLim) {
        int subW = node->sub ? node->sub->width : 0;
        bm.width = LMax((int)bm.opSize.cx, subW) + 4;
        bm.ascent = bm.opTm.tmAscent;
        int opDesc = LMax(0, (int)bm.opSize.cy - bm.opTm.tmAscent);
        int subH = node->sub ? node->sub->height : 0;
        bm.subBaselineBelowMain = opDesc + (node->sub ? node->sub->ascent : 0);
        bm.height = bm.ascent + LMax(opDesc, opDesc + subH);
    } else {
        bm.opTopAboveBase = (bm.opTm.tmAscent * 85) / 100;
        int opBotBelowBase = LMax(0, (int)bm.opSize.cy - bm.opTopAboveBase);
        bm.limOffsetX = bm.opSize.cx + 2;

        int supAsc = node->sup ? node->sup->ascent : 0;
        int supDesc = node->sup ? LMax(0, node->sup->height - supAsc) : 0;
        int subAsc = node->sub ? node->sub->ascent : 0;
        int subDesc = node->sub ? LMax(0, node->sub->height - subAsc) : 0;

        // Upper limit sits cleanly beside the top hook; never extends out of box
        bm.supBaselineAboveMain = node->sup ? (bm.opTopAboveBase + 2 - supAsc) : 0;
        int supTopAbove = node->sup ? LMax(bm.opTopAboveBase, bm.supBaselineAboveMain + supAsc) : bm.opTopAboveBase;
        (void)supDesc;

        // Lower limit sits beside bottom tail
        bm.subBaselineBelowMain = node->sub ? LMax(opBotBelowBase - 2, subAsc - 2) : 0;
        int subBotBelow = node->sub ? LMax(opBotBelowBase, bm.subBaselineBelowMain + subDesc) : opBotBelowBase;

        int limW = LMax(node->sup ? node->sup->width : 0, node->sub ? node->sub->width : 0);
        bm.width = bm.opSize.cx + limW + ((limW > 0) ? 4 : 1);
        bm.ascent = supTopAbove + 1;
        bm.height = bm.ascent + subBotBelow + 1;
    }
    return bm;
}

inline void MeasureNode(HDC hdc, const std::shared_ptr<LatexNode>& node, int fontSize, bool inMath) {
    if (!node) return;

    if (node->type == LatexNode::NODE_SEQ) {
        int totalW = 0;
        int maxAscent = 0;
        int maxDescent = 0;
        for (auto& ch : node->children) {
            if (node->bold) ch->bold = true;
            if (node->forceUpright) {
                ch->italic = false;
                ch->forceUpright = true;
            }
            MeasureNode(hdc, ch, fontSize, inMath);
            totalW += ch->width;
            maxAscent = LMax(maxAscent, ch->ascent);
            maxDescent = LMax(maxDescent, ch->height - ch->ascent);
        }
        node->width = totalW;
        node->ascent = LMax(fontSize, maxAscent);
        node->height = node->ascent + LMax(4, maxDescent);
        return;
    }

    if (node->type == LatexNode::NODE_MATRIX) {
        int cellFont = LMax(9, (fontSize * 85) / 100);
        int colGap = 10;
        int rowGap = (node->text == L"binom") ? 3 : 5;
        size_t numCols = 0;
        for (const auto& r : node->rows) {
            if (r.size() > numCols) numCols = r.size();
        }
        node->colWidths.assign(numCols, 6);
        node->rowAscents.assign(node->rows.size(), (cellFont * 4) / 5);
        node->rowDescents.assign(node->rows.size(), cellFont / 4);

        for (size_t r = 0; r < node->rows.size(); ++r) {
            for (size_t c = 0; c < node->rows[r].size(); ++c) {
                auto& cell = node->rows[r][c];
                if (!cell) continue;
                if (node->bold) cell->bold = true;
                MeasureNode(hdc, cell, cellFont, true);
                node->colWidths[c] = LMax(node->colWidths[c], cell->width);
                node->rowAscents[r] = LMax(node->rowAscents[r], cell->ascent);
                node->rowDescents[r] = LMax(node->rowDescents[r], LMax(2, cell->height - cell->ascent));
            }
        }
        int totalW = 4;
        for (size_t c = 0; c < numCols; ++c) {
            totalW += node->colWidths[c];
            if (c + 1 < numCols) totalW += colGap;
        }
        int totalH = 0;
        for (size_t r = 0; r < node->rows.size(); ++r) {
            totalH += node->rowAscents[r] + node->rowDescents[r];
            if (r + 1 < node->rows.size()) totalH += rowGap;
        }
        totalH = LMax(totalH, fontSize);
        node->width = totalW;
        node->ascent = totalH / 2 + fontSize / 3;
        node->height = totalH;
        return;
    }

    if (node->type == LatexNode::NODE_PAREN_GROUP) {
        MeasureNode(hdc, node->num, fontSize, true);
        int innerH = node->num ? node->num->height : fontSize;
        int innerAsc = node->num ? node->num->ascent : (fontSize * 4) / 5;
        int innerW = node->num ? node->num->width : 4;
        bool tall = (innerH > (fontSize * 210) / 100);
        auto measureDel = [&](const std::wstring& d) -> int {
            if (d.empty() || d == L".") return 1;
            if (tall) return LMax(7, (fontSize * 55) / 100);
            HFONT hFont = CreateMathFont(fontSize, false, node->bold, true);
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            SIZE sz{};
            GetTextExtentPoint32W(hdc, d.c_str(), (int)d.size(), &sz);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
            return sz.cx + 1;
        };
        int leftW = measureDel(node->text);
        int rightW = measureDel(node->closeText);
        node->width = leftW + innerW + rightW;
        node->ascent = innerAsc + (tall ? 2 : 0);
        node->height = innerH + (tall ? 4 : 0);
        return;
    }

    if (node->type == LatexNode::NODE_TEXT || node->type == LatexNode::NODE_MATH_VAR || node->type == LatexNode::NODE_MATH_OP) {
        if (node->text.empty()) {
            node->width = 0;
            node->height = fontSize;
            node->ascent = (fontSize * 4) / 5;
            return;
        }
        bool useSerif = inMath || node->type == LatexNode::NODE_MATH_VAR || node->type == LatexNode::NODE_MATH_OP;
        bool useItalic = node->italic && !node->forceUpright;
        HFONT hFont = CreateMathFont(fontSize, useItalic, node->bold, useSerif);
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);
        SIZE sz = { 0, 0 };
        GetTextExtentPoint32W(hdc, node->text.c_str(), (int)node->text.size(), &sz);
        TEXTMETRICW tm = {};
        GetTextMetricsW(hdc, &tm);
        SelectObject(hdc, hOld);
        DeleteObject(hFont);

        node->width = sz.cx + (useItalic ? 1 : 0);
        node->height = sz.cy;
        node->ascent = tm.tmAscent;
        return;
    }

    if (node->type == LatexNode::NODE_FRAC) {
        int subFont = LMax(9, (fontSize * 75) / 100);
        MeasureNode(hdc, node->num, subFont, true);
        MeasureNode(hdc, node->den, subFont, true);
        int w = LMax(node->num ? node->num->width : 6, node->den ? node->den->width : 6) + 8;
        int numH = node->num ? node->num->height : subFont;
        int denH = node->den ? node->den->height : subFont;
        int mathAxis = fontSize / 3;
        node->width = w + 4;
        node->ascent = numH + 2 + mathAxis;
        node->height = node->ascent + LMax(2, denH + 2 - mathAxis);
        return;
    }

    if (node->type == LatexNode::NODE_SQRT) {
        MeasureNode(hdc, node->num, fontSize, true);
        int idxW = 0;
        if (node->den) {
            MeasureNode(hdc, node->den, LMax(9, (fontSize * 65) / 100), true);
            idxW = LMax(0, node->den->width - 3);
        }
        int radW = LMax(10, (fontSize * 3) / 5);
        int innerH = node->num ? node->num->height : fontSize;
        int innerAsc = node->num ? node->num->ascent : (fontSize * 4) / 5;
        node->width = idxW + radW + (node->num ? node->num->width : 6) + 7;
        node->ascent = innerAsc + 5;
        node->height = innerH + 6;
        return;
    }

    if (node->type == LatexNode::NODE_BIGOP) {
        int limFont = LMax(9, (fontSize * 70) / 100);
        if (node->sup) MeasureNode(hdc, node->sup, limFont, true);
        if (node->sub) MeasureNode(hdc, node->sub, limFont, true);
        BigOpMetrics bm = ComputeBigOpMetrics(hdc, node, fontSize);
        node->width = bm.width;
        node->ascent = bm.ascent;
        node->height = bm.height;
        return;
    }

    if (node->type == LatexNode::NODE_SUPSUB) {
        MeasureNode(hdc, node->num, fontSize, true);
        int scriptFont = LMax(9, (fontSize * 72) / 100);
        if (node->sup) MeasureNode(hdc, node->sup, scriptFont, true);
        if (node->sub) MeasureNode(hdc, node->sub, scriptFont, true);
        SupSubMetrics m = ComputeSupSubMetrics(node, fontSize);
        node->width = m.totalW;
        node->ascent = m.ascent;
        node->height = m.height;
        return;
    }

    if (node->type == LatexNode::NODE_ACCENT) {
        MeasureNode(hdc, node->num, fontSize, true);
        node->width = (node->num ? node->num->width : 8) + 2;
        node->ascent = (node->num ? node->num->ascent : fontSize) + 5;
        node->height = (node->num ? node->num->height : fontSize) + 5;
        return;
    }
}

inline void DrawTallDelimiter(HDC hdc, const std::wstring& sym, int x, int topY, int botY, int parenW, COLORREF color) {
    if (sym.empty() || sym == L".") return;
    HPEN hPen = CreatePen(PS_SOLID, 1, color);
    HPEN hOld = (HPEN)SelectObject(hdc, hPen);
    int h = LMax(8, botY - topY);
    if (sym == L"(" || sym == L")") {
        bool left = (sym == L"(");
        int xOuter = left ? (x + parenW - 1) : (x + 1);
        int xCurve = left ? (x + 1) : (x + parenW - 2);
        POINT pts[4] = {
            { xOuter, topY + 1 },
            { xCurve, topY + h / 6 },
            { xCurve, botY - h / 6 },
            { xOuter, botY - 1 }
        };
        PolyBezier(hdc, pts, 4);
    } else if (sym == L"[" || sym == L"]") {
        bool left = (sym == L"[");
        int xVert = left ? (x + 2) : (x + parenW - 3);
        int xTick = left ? (x + parenW - 2) : (x + 2);
        MoveToEx(hdc, xTick, topY + 1, nullptr);
        LineTo(hdc, xVert, topY + 1);
        LineTo(hdc, xVert, botY - 1);
        LineTo(hdc, xTick, botY - 1);
    } else if (sym == L"{" || sym == L"}") {
        bool left = (sym == L"{");
        int midY = topY + h / 2;
        int xOut = left ? (x + parenW - 2) : (x + 2);
        int xMid = left ? (x + parenW / 2) : (x + parenW / 2);
        int xTip = left ? (x + 1) : (x + parenW - 2);
        MoveToEx(hdc, xOut, topY + 1, nullptr);
        LineTo(hdc, xMid, topY + 3);
        LineTo(hdc, xMid, midY - 2);
        LineTo(hdc, xTip, midY);
        LineTo(hdc, xMid, midY + 2);
        LineTo(hdc, xMid, botY - 3);
        LineTo(hdc, xOut, botY - 1);
    } else if (sym == L"\x27E8" || sym == L"\x27E9") {
        bool left = (sym == L"\x27E8");
        int midY = topY + h / 2;
        int xOuter = left ? (x + parenW - 2) : (x + 2);
        int xTip = left ? (x + 1) : (x + parenW - 2);
        MoveToEx(hdc, xOuter, topY + 1, nullptr);
        LineTo(hdc, xTip, midY);
        LineTo(hdc, xOuter, botY - 1);
    } else if (sym == L"\x2016") {
        int cx = x + parenW / 2;
        MoveToEx(hdc, cx - 1, topY + 1, nullptr);
        LineTo(hdc, cx - 1, botY - 1);
        MoveToEx(hdc, cx + 2, topY + 1, nullptr);
        LineTo(hdc, cx + 2, botY - 1);
    } else {
        int cx = x + parenW / 2;
        MoveToEx(hdc, cx, topY + 1, nullptr);
        LineTo(hdc, cx, botY - 1);
    }
    SelectObject(hdc, hOld);
    DeleteObject(hPen);
}

inline void DrawNode(HDC hdc, const std::shared_ptr<LatexNode>& node, int x, int baselineY, int fontSize, COLORREF color, bool inMath) {
    if (!node) return;

    if (node->type == LatexNode::NODE_SEQ) {
        int curX = x;
        for (auto& ch : node->children) {
            DrawNode(hdc, ch, curX, baselineY, fontSize, color, inMath);
            curX += ch->width;
        }
        return;
    }

    if (node->type == LatexNode::NODE_MATRIX) {
        int cellFont = LMax(9, (fontSize * 85) / 100);
        int colGap = 10;
        int rowGap = (node->text == L"binom") ? 3 : 5;
        int curY = baselineY - node->ascent;
        for (size_t r = 0; r < node->rows.size(); ++r) {
            int rowBaseY = curY + node->rowAscents[r];
            int curX = x + 2;
            for (size_t c = 0; c < node->rows[r].size(); ++c) {
                int cw = (c < node->colWidths.size()) ? node->colWidths[c] : 8;
                auto& cell = node->rows[r][c];
                if (cell) {
                    int offX = (cw - cell->width) / 2;
                    DrawNode(hdc, cell, curX + LMax(0, offX), rowBaseY, cellFont, color, true);
                }
                curX += cw + colGap;
            }
            curY += node->rowAscents[r] + node->rowDescents[r] + rowGap;
        }
        return;
    }

    if (node->type == LatexNode::NODE_PAREN_GROUP) {
        int innerH = node->num ? node->num->height : fontSize;
        int innerW = node->num ? node->num->width : 4;
        bool tall = (innerH > (fontSize * 210) / 100);
        if (tall) {
            int leftW = (node->text.empty() || node->text == L".") ? 1 : LMax(7, (fontSize * 55) / 100);
            int rightW = (node->closeText.empty() || node->closeText == L".") ? 1 : LMax(7, (fontSize * 55) / 100);
            int topY = baselineY - node->ascent + 1;
            int botY = baselineY + (node->height - node->ascent) - 1;
            DrawTallDelimiter(hdc, node->text, x, topY, botY, leftW, color);
            if (node->num) {
                DrawNode(hdc, node->num, x + leftW, baselineY, fontSize, color, true);
            }
            DrawTallDelimiter(hdc, node->closeText, x + leftW + innerW, topY, botY, rightW, color);
        } else {
            auto drawDel = [&](const std::wstring& d, int dx) -> int {
                if (d.empty() || d == L".") return 1;
                HFONT hFont = CreateMathFont(fontSize, false, node->bold, true);
                HFONT hOld = (HFONT)SelectObject(hdc, hFont);
                SIZE sz{};
                GetTextExtentPoint32W(hdc, d.c_str(), (int)d.size(), &sz);
                TEXTMETRICW tm{};
                GetTextMetricsW(hdc, &tm);
                SetTextColor(hdc, color);
                TextOutW(hdc, dx, baselineY - tm.tmAscent, d.c_str(), (int)d.size());
                SelectObject(hdc, hOld);
                DeleteObject(hFont);
                return sz.cx + 1;
            };
            int leftW = drawDel(node->text, x);
            if (node->num) {
                DrawNode(hdc, node->num, x + leftW, baselineY, fontSize, color, true);
            }
            drawDel(node->closeText, x + leftW + innerW);
        }
        return;
    }

    if (node->type == LatexNode::NODE_TEXT || node->type == LatexNode::NODE_MATH_VAR || node->type == LatexNode::NODE_MATH_OP) {
        if (node->text.empty()) return;
        bool useSerif = inMath || node->type == LatexNode::NODE_MATH_VAR || node->type == LatexNode::NODE_MATH_OP;
        bool useItalic = node->italic && !node->forceUpright;
        HFONT hFont = CreateMathFont(fontSize, useItalic, node->bold, useSerif);
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);
        SetTextColor(hdc, color);
        TextOutW(hdc, x, baselineY - node->ascent, node->text.c_str(), (int)node->text.size());
        SelectObject(hdc, hOld);
        DeleteObject(hFont);
        return;
    }

    if (node->type == LatexNode::NODE_FRAC) {
        int subFont = LMax(9, (fontSize * 75) / 100);
        int barW = node->width - 4;
        int barX = x + 2;
        int barY = baselineY - (fontSize / 3);

        if (node->num) {
            int numX = barX + (barW - node->num->width) / 2;
            int numDesc = node->num->height - node->num->ascent;
            int numBaseY = barY - 2 - numDesc;
            DrawNode(hdc, node->num, numX, numBaseY, subFont, color, true);
        }
        HPEN hPen = CreatePen(PS_SOLID, 1, color);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, barX, barY, nullptr);
        LineTo(hdc, barX + barW, barY);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        if (node->den) {
            int denX = barX + (barW - node->den->width) / 2;
            int denBaseY = barY + 2 + node->den->ascent;
            DrawNode(hdc, node->den, denX, denBaseY, subFont, color, true);
        }
        return;
    }

    if (node->type == LatexNode::NODE_SQRT) {
        int idxW = 0;
        if (node->den) {
            idxW = LMax(0, node->den->width - 3);
            int idxFont = LMax(9, (fontSize * 65) / 100);
            DrawNode(hdc, node->den, x, baselineY - (fontSize / 2), idxFont, color, true);
        }
        int radW = LMax(10, (fontSize * 3) / 5);
        int radX = x + idxW;
        int topY = baselineY - node->ascent + 2;
        int botY = baselineY + (node->height - node->ascent) - 1;
        int midY = topY + (botY - topY) * 55 / 100;
        int contentW = node->num ? node->num->width : 6;

        HPEN hPen = CreatePen(PS_SOLID, 1, color);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, radX + 1, midY + 2, nullptr);
        LineTo(hdc, radX + 3, midY);
        LineTo(hdc, radX + radW / 2, botY);
        LineTo(hdc, radX + radW - 1, topY);
        LineTo(hdc, radX + radW + contentW + 5, topY);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        if (node->num) {
            DrawNode(hdc, node->num, radX + radW + 3, baselineY, fontSize, color, true);
        }
        return;
    }

    if (node->type == LatexNode::NODE_BIGOP) {
        BigOpMetrics bm = ComputeBigOpMetrics(hdc, node, fontSize);
        HFONT hFont = CreateMathFont(bm.opFont, false, false, true);
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);
        SetTextColor(hdc, color);

        if (bm.isLim) {
            int opX = x + (node->width - bm.opSize.cx) / 2;
            TextOutW(hdc, opX, baselineY - bm.opTm.tmAscent, node->text.c_str(), (int)node->text.size());
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
            if (node->sub) {
                int subX = x + (node->width - node->sub->width) / 2;
                int subBase = baselineY + bm.subBaselineBelowMain;
                DrawNode(hdc, node->sub, subX, subBase, bm.limFont, color, true);
            }
        } else {
            int opTopY = baselineY - bm.opTopAboveBase;
            TextOutW(hdc, x, opTopY, node->text.c_str(), (int)node->text.size());
            SelectObject(hdc, hOld);
            DeleteObject(hFont);

            int limX = x + bm.limOffsetX;
            if (node->sup) {
                int supBase = baselineY - bm.supBaselineAboveMain;
                DrawNode(hdc, node->sup, limX, supBase, bm.limFont, color, true);
            }
            if (node->sub) {
                int subBase = baselineY + bm.subBaselineBelowMain;
                DrawNode(hdc, node->sub, limX, subBase, bm.limFont, color, true);
            }
        }
        return;
    }

    if (node->type == LatexNode::NODE_SUPSUB) {
        SupSubMetrics m = ComputeSupSubMetrics(node, fontSize);
        if (node->num) {
            DrawNode(hdc, node->num, x, baselineY, fontSize, color, true);
        }
        int scriptX = x + m.baseW;
        if (node->sup) {
            int supBase = baselineY - m.supBaselineAboveMain;
            DrawNode(hdc, node->sup, scriptX, supBase, m.scriptFont, color, true);
        }
        if (node->sub) {
            int subBase = baselineY + m.subBaselineBelowMain;
            DrawNode(hdc, node->sub, scriptX, subBase, m.scriptFont, color, true);
        }
        return;
    }

    if (node->type == LatexNode::NODE_ACCENT) {
        if (node->num) {
            DrawNode(hdc, node->num, x + 1, baselineY, fontSize, color, true);
            int topY = baselineY - node->num->ascent - 2;
            int x1 = x + 1;
            int x2 = x + 1 + LMax(4, node->num->width);
            int midX = (x1 + x2) / 2;
            HPEN hPen = CreatePen(PS_SOLID, 1, color);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            if (node->text == L"hat" || node->text == L"widehat") {
                MoveToEx(hdc, x1 + 1, topY + 2, nullptr);
                LineTo(hdc, midX, topY - 2);
                LineTo(hdc, x2 - 1, topY + 2);
            } else if (node->text == L"tilde" || node->text == L"widetilde") {
                POINT pts[4] = {
                    { x1, topY + 1 },
                    { x1 + (x2 - x1) / 3, topY - 3 },
                    { x1 + (2 * (x2 - x1)) / 3, topY + 3 },
                    { x2, topY - 1 }
                };
                PolyBezier(hdc, pts, 4);
            } else if (node->text == L"dot") {
                HBRUSH br = CreateSolidBrush(color);
                RECT dotR = { midX - 1, topY - 1, midX + 2, topY + 2 };
                FillRect(hdc, &dotR, br);
                DeleteObject(br);
            } else if (node->text == L"ddot") {
                HBRUSH br = CreateSolidBrush(color);
                RECT d1 = { midX - 3, topY - 1, midX - 1, topY + 2 };
                RECT d2 = { midX + 1, topY - 1, midX + 3, topY + 2 };
                FillRect(hdc, &d1, br);
                FillRect(hdc, &d2, br);
                DeleteObject(br);
            } else {
                MoveToEx(hdc, x1, topY, nullptr);
                LineTo(hdc, x2, topY);
                if (node->text == L"vec") {
                    MoveToEx(hdc, x2 - 3, topY - 2, nullptr);
                    LineTo(hdc, x2, topY);
                    MoveToEx(hdc, x2 - 3, topY + 2, nullptr);
                    LineTo(hdc, x2, topY);
                }
            }
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
        }
        return;
    }
}

struct InlineSpan {
    bool isMath = false;
    bool isAutoMath = false;
    bool isNewline = false;
    std::wstring rawText;
    std::shared_ptr<LatexNode> node;
};

inline LatexValidationResult ValidateMathSegment(const std::wstring& s) {
    LatexValidationResult res{};
    size_t i = 0;
    SkipSpaces(s, i);
    if (i >= s.size()) {
        res.valid = false;
        res.diagnostic = L"Bloque matem\u00E1tico '$$' vac\u00EDo.";
        return res;
    }

    int braceDepth = 0;
    int bracketDepth = 0;

    auto consumeArgOrFail = [&](size_t& p, const wchar_t* ownerCmd) -> bool {
        SkipSpaces(s, p);
        if (p >= s.size() || s[p] == L'}' || s[p] == L'^' || s[p] == L'_') {
            res.valid = false;
            res.diagnostic = std::wstring(L"Falta argumento obligatorio para '") + ownerCmd + L"'.";
            return false;
        }
        if (s[p] == L'{') {
            int d = 1;
            ++p;
            bool nonSpaceInside = false;
            while (p < s.size() && d > 0) {
                if (s[p] == L'\\' && p + 1 < s.size()) {
                    nonSpaceInside = true;
                    p += 2;
                    continue;
                }
                if (s[p] == L'{') ++d;
                else if (s[p] == L'}') --d;
                else if (s[p] != L' ' && s[p] != L'\t') nonSpaceInside = true;
                ++p;
            }
            if (d != 0) {
                res.valid = false;
                res.diagnostic = std::wstring(L"Llave '{' sin cerrar en '") + ownerCmd + L"'.";
                return false;
            }
            if (!nonSpaceInside) {
                res.valid = false;
                res.diagnostic = std::wstring(L"Argumento vac\u00EDo '{}' en '") + ownerCmd + L"'.";
                return false;
            }
            return true;
        }
        if (s[p] == L'\\') {
            ++p;
            if (p >= s.size()) {
                res.valid = false;
                res.diagnostic = std::wstring(L"Barra '\\' incompleta en '") + ownerCmd + L"'.";
                return false;
            }
            if (!iswalpha(s[p])) { ++p; return true; }
            size_t st = p;
            while (p < s.size() && iswalpha(s[p])) ++p;
            std::wstring subCmd = s.substr(st, p - st);
            if (!IsSupportedLatexCommand(subCmd)) {
                res.valid = false;
                res.diagnostic = L"Comando LaTeX no soportado en el subconjunto 2D: \\" + subCmd;
                return false;
            }
            return true;
        }
        ++p;
        return true;
    };

    while (i < s.size()) {
        wchar_t ch = s[i];
        if (ch == L'{') { ++braceDepth; ++i; continue; }
        if (ch == L'}') {
            if (braceDepth == 0) {
                res.valid = false;
                res.diagnostic = L"Llave de cierre '}' inesperada sin apertura '{'.";
                return res;
            }
            --braceDepth;
            ++i;
            continue;
        }
        if (ch == L'[') { ++bracketDepth; ++i; continue; }
        if (ch == L']') {
            if (bracketDepth > 0) --bracketDepth;
            ++i;
            continue;
        }
        if (ch == L'^' || ch == L'_') {
            wchar_t opStr[2] = { ch, 0 };
            size_t probe = i + 1;
            if (!consumeArgOrFail(probe, opStr)) return res;
            ++i;
            continue;
        }
        if (ch == L'\\') {
            ++i;
            if (i >= s.size()) {
                res.valid = false;
                res.diagnostic = L"Barra invertida '\\' suelta al final del bloque matem\u00E1tico.";
                return res;
            }
            if (!iswalpha(s[i])) {
                wchar_t sym = s[i++];
                bool isF = false, isB = false;
                std::wstring esc(1, sym);
                if (sym != L'\\' && esc != L"," && esc != L";" && esc != L":" && esc != L" " &&
                    esc != L"!" && esc != L"{" && esc != L"}" && esc != L"$" &&
                    esc != L"%" && esc != L"|" && LookupLatexSymbol(esc, isF, isB).empty()) {
                    res.valid = false;
                    res.diagnostic = std::wstring(L"Secuencia de escape desconocida: \\") + esc;
                    return res;
                }
                continue;
            }
            size_t st = i;
            while (i < s.size() && iswalpha(s[i])) ++i;
            std::wstring cmd = s.substr(st, i - st);
            if (!IsSupportedLatexCommand(cmd)) {
                res.valid = false;
                res.diagnostic = L"Comando matem\u00E1tico no soportado en el subconjunto 2D: \\" + cmd;
                return res;
            }
            if (cmd == L"frac" || cmd == L"dfrac" || cmd == L"tfrac" || cmd == L"binom") {
                size_t probe = i;
                std::wstring label = L"\\" + cmd;
                if (!consumeArgOrFail(probe, label.c_str())) return res;
                if (!consumeArgOrFail(probe, label.c_str())) return res;
            } else if (cmd == L"sqrt") {
                size_t probe = i;
                SkipSpaces(s, probe);
                if (probe < s.size() && s[probe] == L'[') {
                    ++probe;
                    while (probe < s.size() && s[probe] != L']') ++probe;
                    if (probe >= s.size()) {
                        res.valid = false;
                        res.diagnostic = L"Corchete '[' de \u00EDndice de \\sqrt sin cerrar ']'.";
                        return res;
                    }
                    ++probe;
                }
                if (!consumeArgOrFail(probe, L"\\sqrt")) return res;
            } else if (cmd == L"vec" || cmd == L"bar" || cmd == L"hat" || cmd == L"widehat" ||
                       cmd == L"overline" || cmd == L"tilde" || cmd == L"widetilde" ||
                       cmd == L"dot" || cmd == L"ddot" ||
                       cmd == L"text" || cmd == L"mathrm" || cmd == L"operatorname" ||
                       cmd == L"mathbf" || cmd == L"boldsymbol" || cmd == L"mathbb" ||
                       cmd == L"begin" || cmd == L"end") {
                size_t probe = i;
                std::wstring label = L"\\" + cmd;
                if (!consumeArgOrFail(probe, label.c_str())) return res;
            }
            continue;
        }
        ++i;
    }

    if (braceDepth != 0) {
        res.valid = false;
        res.diagnostic = L"Falta cerrar " + std::to_wstring(braceDepth) + L" llave(s) '}' en la f\u00F3rmula.";
        return res;
    }
    return res;
}

inline LatexValidationResult ValidateRichLatexInput(const std::wstring& input) {
    LatexValidationResult ok{};
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == L'$') {
            ++i;
            size_t end = input.find(L'$', i);
            if (end == std::wstring::npos) {
                ok.valid = false;
                ok.diagnostic = L"F\u00F3rmula: falta delimitador de cierre '$'.";
                return ok;
            }
            std::wstring seg = input.substr(i, end - i);
            LatexValidationResult segRes = ValidateMathSegment(seg);
            if (!segRes.valid) return segRes;
            i = end + 1;
            continue;
        }
        ++i;
    }
    return ok;
}

inline std::wstring MapUnicodeSupChar(wchar_t ch) {
    switch (ch) {
        case 0x2070: return L"0"; case 0x00B9: return L"1"; case 0x00B2: return L"2"; case 0x00B3: return L"3";
        case 0x2074: return L"4"; case 0x2075: return L"5"; case 0x2076: return L"6"; case 0x2077: return L"7";
        case 0x2078: return L"8"; case 0x2079: return L"9"; case 0x207A: return L"+"; case 0x207B: return L"-";
        case 0x207F: return L"n"; case 0x02E3: return L"x"; case 0x02B8: return L"y"; case 0x2071: return L"i";
        default: return L"";
    }
}

inline std::wstring MapUnicodeSubChar(wchar_t ch) {
    if (ch >= 0x2080 && ch <= 0x2089) return std::wstring(1, static_cast<wchar_t>(L'0' + (ch - 0x2080)));
    switch (ch) {
        case 0x208A: return L"+"; case 0x208B: return L"-";
        case 0x2099: return L"n"; case 0x2093: return L"x";
        case 0x1D62: return L"i"; case 0x2C7C: return L"j";
        default: return L"";
    }
}

inline std::wstring MapUnicodeMathSymbol(wchar_t ch, bool& outChanged) {
    switch (ch) {
        case 0x03B1: outChanged = true; return L"\\alpha ";
        case 0x03B2: outChanged = true; return L"\\beta ";
        case 0x03B3: outChanged = true; return L"\\gamma ";
        case 0x03B4: outChanged = true; return L"\\delta ";
        case 0x03B5: outChanged = true; return L"\\epsilon ";
        case 0x03B6: outChanged = true; return L"\\zeta ";
        case 0x03B7: outChanged = true; return L"\\eta ";
        case 0x03B8: outChanged = true; return L"\\theta ";
        case 0x03D1: outChanged = true; return L"\\vartheta ";
        case 0x03B9: outChanged = true; return L"\\iota ";
        case 0x03BA: outChanged = true; return L"\\kappa ";
        case 0x03BB: outChanged = true; return L"\\lambda ";
        case 0x03BC: outChanged = true; return L"\\mu ";
        case 0x03BD: outChanged = true; return L"\\nu ";
        case 0x03BE: outChanged = true; return L"\\xi ";
        case 0x03C0: outChanged = true; return L"\\pi ";
        case 0x03C1: outChanged = true; return L"\\rho ";
        case 0x03C3: outChanged = true; return L"\\sigma ";
        case 0x03C4: outChanged = true; return L"\\tau ";
        case 0x03C5: outChanged = true; return L"\\upsilon ";
        case 0x03C6: outChanged = true; return L"\\varphi ";
        case 0x03D5: outChanged = true; return L"\\phi ";
        case 0x03C7: outChanged = true; return L"\\chi ";
        case 0x03C8: outChanged = true; return L"\\psi ";
        case 0x03C9: outChanged = true; return L"\\omega ";
        case 0x0393: outChanged = true; return L"\\Gamma ";
        case 0x0394: outChanged = true; return L"\\Delta ";
        case 0x0398: outChanged = true; return L"\\Theta ";
        case 0x039B: outChanged = true; return L"\\Lambda ";
        case 0x039E: outChanged = true; return L"\\Xi ";
        case 0x03A0: outChanged = true; return L"\\Pi ";
        case 0x03A3: outChanged = true; return L"\\Sigma ";
        case 0x03A6: outChanged = true; return L"\\Phi ";
        case 0x03A8: outChanged = true; return L"\\Psi ";
        case 0x03A9: outChanged = true; return L"\\Omega ";
        case 0x00D7: outChanged = true; return L"\\times ";
        case 0x00F7: outChanged = true; return L"\\div ";
        case 0x00B1: outChanged = true; return L"\\pm ";
        case 0x2213: outChanged = true; return L"\\mp ";
        case 0x2264: outChanged = true; return L"\\le ";
        case 0x2265: outChanged = true; return L"\\ge ";
        case 0x2260: outChanged = true; return L"\\ne ";
        case 0x2248: outChanged = true; return L"\\approx ";
        case 0x2261: outChanged = true; return L"\\equiv ";
        case 0x221E: outChanged = true; return L"\\infty ";
        case 0x2202: outChanged = true; return L"\\partial ";
        case 0x2207: outChanged = true; return L"\\nabla ";
        case 0x222B: outChanged = true; return L"\\int ";
        case 0x222E: outChanged = true; return L"\\oint ";
        case 0x2211: outChanged = true; return L"\\sum ";
        case 0x220F: outChanged = true; return L"\\prod ";
        case 0x2220: outChanged = true; return L"\\angle ";
        case 0x00B0: outChanged = true; return L"\\degree ";
        case 0x2032: outChanged = true; return L"\\prime ";
        case 0x2033: outChanged = true; return L"\\prime\\prime ";
        case 0x25A1: outChanged = true; return L"\\Box ";
        case 0x25A0: outChanged = true; return L"\\blacksquare ";
        case 0x2208: outChanged = true; return L"\\in ";
        case 0x2209: outChanged = true; return L"\\notin ";
        case 0x221D: outChanged = true; return L"\\propto ";
        case 0x2229: outChanged = true; return L"\\cap ";
        case 0x222A: outChanged = true; return L"\\cup ";
        case 0x2205: outChanged = true; return L"\\emptyset ";
        case 0x2212: outChanged = true; return L"-";
        default: return L"";
    }
}

inline bool TryPromoteAutoMathToken(const std::wstring& core, std::wstring& outLatex) {
    if (core.empty()) return false;
    // Do not promote URLs, paths, or key brackets like [SHIFT], ->
    if (core.find(L"://") != std::wstring::npos || core.find(L"\\\\") != std::wstring::npos) return false;
    if (core == L"->" || core == L"<-" || core == L"\x2192" || core == L"\x2190") return false;
    if (core.size() >= 2 && core.front() == L'[' && core.back() == L']') return false;

    bool hasMathSignal = false;
    std::wstring s;
    s.reserve(core.size() * 2);

    // Handle leading cube-root or square-root prefix (e.g. ³√(x), ³√x, √x, √(a+b), √□)
    auto wrapRootArg = [&](size_t startIdx) -> std::wstring {
        if (startIdx >= core.size()) return L"\\Box";
        std::wstring rem = core.substr(startIdx);
        if (rem.size() >= 2 && rem.front() == L'(' && rem.back() == L')') {
            rem = rem.substr(1, rem.size() - 2);
        }
        if (rem.empty() || rem == L"\x25A1") return L"\\Box";
        std::wstring rec;
        if (TryPromoteAutoMathToken(rem, rec)) return rec;
        return rem;
    };

    if (core.size() >= 2 && core[0] == 0x00B3 && core[1] == 0x221A) {
        outLatex = L"\\sqrt[3]{" + wrapRootArg(2) + L"}";
        return true;
    }
    if (core[0] == 0x221A) {
        outLatex = L"\\sqrt{" + wrapRootArg(1) + L"}";
        return true;
    }

    // Translate Unicode superscripts/subscripts & Greek/math symbols into LaTeX
    for (size_t i = 0; i < core.size(); ) {
        std::wstring supRun;
        while (i < core.size()) {
            std::wstring m = MapUnicodeSupChar(core[i]);
            if (m.empty()) break;
            supRun += m;
            ++i;
        }
        if (!supRun.empty()) {
            hasMathSignal = true;
            s += L"^{" + supRun + L"}";
            continue;
        }
        std::wstring subRun;
        while (i < core.size()) {
            std::wstring m = MapUnicodeSubChar(core[i]);
            if (m.empty()) break;
            subRun += m;
            ++i;
        }
        if (!subRun.empty()) {
            hasMathSignal = true;
            s += L"_{" + subRun + L"}";
            continue;
        }
        if (core[i] == 0x221A) {
            hasMathSignal = true;
            ++i;
            s += L"\\sqrt";
            if (i < core.size() && core[i] == L'(') {
                s += L"{";
                ++i;
                int d = 1;
                while (i < core.size() && d > 0) {
                    if (core[i] == L'(') ++d;
                    else if (core[i] == L')') { --d; if (d == 0) { ++i; break; } }
                    bool c = false;
                    std::wstring u = MapUnicodeMathSymbol(core[i], c);
                    s += u.empty() ? std::wstring(1, core[i]) : u;
                    ++i;
                }
                s += L"}";
            } else if (i < core.size()) {
                bool c = false;
                std::wstring u = MapUnicodeMathSymbol(core[i], c);
                s += L"{" + (u.empty() ? std::wstring(1, core[i]) : u) + L"}";
                ++i;
            } else {
                s += L"{\\Box}";
            }
            continue;
        }
        bool symChanged = false;
        std::wstring mapped = MapUnicodeMathSymbol(core[i], symChanged);
        if (symChanged) {
            hasMathSignal = true;
            s += mapped;
            ++i;
            continue;
        }
        s += core[i++];
    }

    // Rewrite CASIO key notation "x10^..." -> "\times 10^..."
    if (s.compare(0, 4, L"x10^") == 0) {
        hasMathSignal = true;
        s = L"\\times 10" + s.substr(3);
    }

    // Detect ASCII '^' (always math when non-empty lhs & rhs) and safe math '_'
    size_t caretPos = s.find(L'^');
    if (caretPos != std::wstring::npos && caretPos > 0 && caretPos + 1 < s.size()) {
        hasMathSignal = true;
    }
    size_t underPos = s.find(L'_');
    if (underPos != std::wstring::npos && underPos > 0 && underPos + 1 < s.size()) {
        bool allowSub = hasMathSignal;
        if (!allowSub && s.find(L'.') == std::wstring::npos) {
            std::wstring lhs = s.substr(0, underPos);
            std::wstring rhs = s.substr(underPos + 1);
            if ((lhs.size() <= 3 && !iswdigit(lhs[0])) || lhs == L"log") {
                if (rhs.size() <= 4 && rhs.find(L'_') == std::wstring::npos) {
                    allowSub = true;
                }
            }
        }
        if (allowSub) hasMathSignal = true;
    }

    // Normalize unbraced multi-char ^... or _... scripts (e.g. cos^-1 -> cos^{-1}, x_12 -> x_{12})
    if (hasMathSignal) {
        std::wstring braced;
        braced.reserve(s.size() + 8);
        for (size_t i = 0; i < s.size(); ++i) {
            if ((s[i] == L'^' || s[i] == L'_') && i + 1 < s.size()) {
                wchar_t op = s[i];
                if (s[i + 1] == L'{') {
                    braced += op;
                    continue;
                }
                if (s[i + 1] == L'(') {
                    size_t j = i + 2;
                    int d = 1;
                    while (j < s.size() && d > 0) {
                        if (s[j] == L'(') ++d;
                        else if (s[j] == L')') --d;
                        ++j;
                    }
                    if (d == 0) {
                        braced += op;
                        braced += L'{';
                        braced += s.substr(i + 2, j - i - 3);
                        braced += L'}';
                        i = j - 1;
                        continue;
                    }
                }
                size_t j = i + 1;
                if (s[j] == L'-' || s[j] == L'+') ++j;
                while (j < s.size() && (iswalnum(s[j]) || s[j] == L',')) ++j;
                if (j > i + 1) {
                    braced += op;
                    braced += L'{';
                    braced += s.substr(i + 1, j - (i + 1));
                    braced += L'}';
                    i = j - 1;
                    continue;
                }
            }
            braced += s[i];
        }
        s = std::move(braced);
    }

    // Detect clean simple math/derivative fractions a/b (never UI labels like D/R/G or ES/EN)
    size_t slashPos = s.find(L'/');
    if (slashPos != std::wstring::npos && slashPos > 0 && slashPos + 1 < s.size() &&
        s.find(L'/', slashPos + 1) == std::wstring::npos) {
        auto trimFrac = [](std::wstring v) {
            while (!v.empty() && v.back() == L' ') v.pop_back();
            while (!v.empty() && v.front() == L' ') v.erase(v.begin());
            return v;
        };
        std::wstring lhs = trimFrac(s.substr(0, slashPos));
        std::wstring rhs = trimFrac(s.substr(slashPos + 1));
        auto isFracAtom = [](const std::wstring& part) -> bool {
            if (part.empty() || part.size() > 14) return false;
            if (part == L"\\pi" || part == L"\\theta" || part == L"\\alpha" || part == L"\\beta") return true;
            if (part.find(L'\\') != std::wstring::npos) return false;
            int upperCount = 0;
            for (wchar_t c : part) {
                if (c >= L'A' && c <= L'Z') ++upperCount;
                else if (!(iswdigit(c) || (c >= L'a' && c <= L'z') || c == L'.')) return false;
            }
            if (upperCount >= 2) return false;
            if (upperCount == 1 && part.size() > 2) return false;
            return true;
        };
        bool isDerivOp = (lhs == L"d" && (rhs == L"dx" || rhs == L"dt" || rhs == L"dy")) ||
                         (lhs == L"dy" && rhs == L"dx") ||
                         (lhs == L"\\partial" && rhs == L"\\partial x");
        if (isDerivOp || (isFracAtom(lhs) && isFracAtom(rhs) &&
            (lhs[0] == L'\\' || std::all_of(lhs.begin(), lhs.end(), iswdigit) || lhs.size() <= 2) &&
            (rhs[0] == L'\\' || std::all_of(rhs.begin(), rhs.end(), iswdigit) || rhs.size() <= 2))) {
            hasMathSignal = true;
            s = L"\\frac{" + lhs + L"}{" + rhs + L"}";
        }
    }

    // Promote upright math function names when followed by '(', '^', '_'
    static const wchar_t* kFuncs[] = {
        L"arcsin", L"arccos", L"arctan", L"arsinh", L"arcosh", L"artanh",
        L"sinh", L"cosh", L"tanh", L"sin", L"cos", L"tan", L"sec", L"csc", L"cot",
        L"log", L"ln", L"exp", L"lim", L"det", L"Abs", L"nPr", L"nCr"
    };
    for (const wchar_t* fn : kFuncs) {
        size_t flen = wcslen(fn);
        size_t pos = 0;
        while ((pos = s.find(fn, pos)) != std::wstring::npos) {
            bool leftOk = (pos == 0 || (!iswalpha(s[pos - 1]) && s[pos - 1] != L'\\'));
            size_t after = pos + flen;
            bool rightTrigger = (after < s.size() && (s[after] == L'(' || s[after] == L'^' || s[after] == L'_'));
            if (leftOk && rightTrigger) {
                hasMathSignal = true;
                s.insert(pos, L"\\");
                pos = after + 1;
            } else {
                pos = after;
            }
        }
    }

    // Standalone |x|
    if (!hasMathSignal && s.size() >= 3 && s.front() == L'|' && s.back() == L'|' && s.find(L'|', 1) == s.size() - 1) {
        hasMathSignal = true;
    }

    if (!hasMathSignal) return false;
    outLatex = std::move(s);
    return true;
}

inline std::vector<InlineSpan> TokenizeRichLatexText(const std::wstring& input) {
    std::vector<InlineSpan> spans;
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == L'\r') {
            ++i;
            continue;
        }
        if (input[i] == L'\n') {
            InlineSpan nl;
            nl.isNewline = true;
            spans.push_back(nl);
            ++i;
            continue;
        }
        if (input[i] == L'$') {
            ++i;
            size_t end = input.find(L'$', i);
            std::wstring mathSrc = (end == std::wstring::npos) ? input.substr(i) : input.substr(i, end - i);
            i = (end == std::wstring::npos) ? input.size() : (end + 1);
            size_t pos = 0;
            InlineSpan sp;
            sp.isMath = true;
            sp.rawText = mathSrc;
            sp.node = ParseMathSequence(mathSrc, pos, 0);
            spans.push_back(sp);
            continue;
        }

        // Texto normal palabra por palabra para permitir ajuste de linea (word-wrap)
        size_t start = i;
        while (i < input.size() && input[i] != L'$' && input[i] != L'\n' && input[i] != L'\r' && input[i] != L' ') {
            ++i;
        }
        size_t coreEnd = i;
        while (i < input.size() && input[i] == L' ') {
            ++i;
        }
        std::wstring word = input.substr(start, i - start);
        std::wstring core = input.substr(start, coreEnd - start);
        std::wstring trailWs = input.substr(coreEnd, i - coreEnd);
        std::wstring trailPunct;
        while (!core.empty() && (core.back() == L',' || core.back() == L';' || core.back() == L':')) {
            trailPunct.insert(trailPunct.begin(), core.back());
            core.pop_back();
        }
        if (core.size() >= 2 && core.back() == L'.' && core[core.size() - 2] != L'.') {
            trailPunct.insert(trailPunct.begin(), L'.');
            core.pop_back();
        }

        std::wstring autoLatex;
        if (!core.empty() && TryPromoteAutoMathToken(core, autoLatex)) {
            size_t pos = 0;
            InlineSpan msp;
            msp.isMath = false;
            msp.isAutoMath = true;
            msp.rawText = autoLatex;
            msp.node = ParseMathSequence(autoLatex, pos, 0);
            spans.push_back(msp);

            std::wstring tail = trailPunct + trailWs;
            if (!tail.empty()) {
                InlineSpan tsp;
                tsp.isMath = false;
                tsp.rawText = tail;
                tsp.node = std::make_shared<LatexNode>();
                tsp.node->type = LatexNode::NODE_TEXT;
                tsp.node->text = tail;
                spans.push_back(tsp);
            }
        } else if (!word.empty()) {
            InlineSpan sp;
            sp.isMath = false;
            sp.rawText = word;
            sp.node = std::make_shared<LatexNode>();
            sp.node->type = LatexNode::NODE_TEXT;
            sp.node->text = word;
            spans.push_back(sp);
        }
    }
    return spans;
}

inline int RenderOrMeasureLatexRichText(HDC hdc, const std::wstring& text, RECT rc, COLORREF textColor, COLORREF mathAccentColor, int baseFontSize, bool singleLine, bool draw, bool hCenter = false) {
    if (text.empty()) return 0;
    int maxW = LMax(40, (int)(rc.right - rc.left));
    int availH = (int)(rc.bottom - rc.top);
    auto spans = TokenizeRichLatexText(text);

    struct LineItem {
        InlineSpan span;
        int xOffset = 0;
    };
    struct LineLayout {
        std::vector<LineItem> items;
        int maxAscent = 0;
        int maxDescent = 0;
        int totalWidth = 0;
        int height = 0;
    };

    auto layoutWithFont = [&](int fSize) -> std::vector<LineLayout> {
        for (auto& sp : spans) {
            if (!sp.isNewline && sp.node) {
                MeasureNode(hdc, sp.node, fSize, sp.isMath || sp.isAutoMath);
            }
        }
        std::vector<LineLayout> lines;
        LineLayout curLine;
        int curX = 0;
        auto flushLine = [&]() {
            if (curLine.items.empty()) {
                curLine.maxAscent = (fSize * 4) / 5;
                curLine.maxDescent = fSize / 3;
                curLine.height = fSize + 4;
                curLine.totalWidth = 0;
            } else {
                curLine.height = curLine.maxAscent + curLine.maxDescent + 4;
                curLine.totalWidth = curX;
            }
            lines.push_back(curLine);
            curLine = LineLayout();
            curX = 0;
        };
        for (auto& sp : spans) {
            if (sp.isNewline) {
                if (singleLine) continue;
                flushLine();
                continue;
            }
            if (!sp.node) continue;
            int w = sp.node->width;
            if (!singleLine && curX > 0 && curX + w > maxW) {
                flushLine();
            }
            LineItem li;
            li.span = sp;
            li.xOffset = curX;
            curLine.items.push_back(li);
            curLine.maxAscent = LMax(curLine.maxAscent, sp.node->ascent);
            curLine.maxDescent = LMax(curLine.maxDescent, sp.node->height - sp.node->ascent);
            curX += w;
        }
        if (!curLine.items.empty() || lines.empty()) {
            flushLine();
        }
        return lines;
    };

    int useFont = baseFontSize;
    auto lines = layoutWithFont(useFont);
    if (singleLine && availH > 0 && availH < 500 && !lines.empty()) {
        while (useFont > 10 && (lines[0].maxAscent + lines[0].maxDescent > availH || (hCenter && lines[0].totalWidth > maxW))) {
            --useFont;
            lines = layoutWithFont(useFont);
        }
    }

    int savedDc = 0;
    if (draw) {
        savedDc = SaveDC(hdc);
        IntersectClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
    }

    int curY = rc.top;
    if (draw && singleLine && !lines.empty()) {
        int inkH = lines[0].maxAscent + lines[0].maxDescent;
        curY = rc.top + (availH - inkH) / 2;
    }
    SetBkMode(hdc, TRANSPARENT);
    for (auto& ln : lines) {
        int baselineY = curY + ln.maxAscent;
        int lineStartX = rc.left;
        if (hCenter && ln.totalWidth < maxW) {
            lineStartX = rc.left + (maxW - ln.totalWidth) / 2;
        }
        if (draw && curY < rc.bottom) {
            for (auto& it : ln.items) {
                COLORREF col = it.span.isMath ? mathAccentColor : textColor;
                DrawNode(hdc, it.span.node, lineStartX + it.xOffset, baselineY, useFont, col, it.span.isMath || it.span.isAutoMath);
            }
        }
        curY += ln.height;
        if (singleLine) break;
    }
    if (draw && savedDc) {
        RestoreDC(hdc, savedDc);
    }
    return curY - rc.top;
}

} // namespace LatexEngine

inline LatexValidationResult ValidateLatexRichText(const std::wstring& text) {
    return LatexEngine::ValidateRichLatexInput(text);
}

inline int DrawLatexRichText(HDC hdc, const std::wstring& text, RECT rc, COLORREF textColor, COLORREF mathAccentColor, int baseFontSize = 15, bool singleLine = false) {
    return LatexEngine::RenderOrMeasureLatexRichText(hdc, text, rc, textColor, mathAccentColor, baseFontSize, singleLine, true, false);
}

inline int DrawLatexRichTextCentered(HDC hdc, const std::wstring& text, RECT rc, COLORREF textColor, COLORREF mathAccentColor, int baseFontSize = 14) {
    return LatexEngine::RenderOrMeasureLatexRichText(hdc, text, rc, textColor, mathAccentColor, baseFontSize, true, true, true);
}

inline int MeasureLatexRichText(HDC hdc, const std::wstring& text, int maxWidth, int baseFontSize = 15, bool singleLine = false) {
    RECT rc = { 0, 0, maxWidth, 10000 };
    return LatexEngine::RenderOrMeasureLatexRichText(hdc, text, rc, RGB(0, 0, 0), RGB(0, 0, 0), baseFontSize, singleLine, false, false);
}
