#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio::Cas {

enum class MathAstKind : std::uint8_t {
    Row = 0,
    TextRun = 1,
    MathVar = 2,
    MathOp = 3,
    Fraction = 4,
    Sqrt = 5,
    SupSub = 6,
    BigOp = 7,
    Accent = 8,
    Delimiter = 9,
    Matrix = 10,
    Space = 11
};

enum class MathFontRole : std::uint8_t {
    TextNormal = 0,
    TextBold = 1,
    MathItalic = 2,
    MathRoman = 3,
    MathBold = 4,
    MathSymbol = 5
};

struct MathAstNode;
using MathAstNodePtr = std::shared_ptr<MathAstNode>;

struct MathAstNode {
    MathAstKind kind = MathAstKind::Row;
    MathFontRole role = MathFontRole::MathItalic;
    std::string text;
    std::string closeDelim;
    std::string accentKind;
    std::string envName;
    bool hasBar = true;
    bool compact = false;
    bool bold = false;
    bool forceUpright = false;
    double spaceEm = 0.0;

    std::vector<MathAstNodePtr> children;
    MathAstNodePtr base;
    MathAstNodePtr sup;
    MathAstNodePtr sub;
    MathAstNodePtr num;
    MathAstNodePtr den;
    MathAstNodePtr radicand;
    MathAstNodePtr rootIdx;
    std::vector<std::vector<MathAstNodePtr>> grid;
};

struct LatexParseResult {
    bool ok = false;
    MathAstNodePtr root;
    std::string errorUtf8;
};

struct LatexValidationResult {
    bool valid = true;
    std::string errorUtf8;
};

struct RichLatexSpan {
    bool isMath = false;
    bool isAutoMath = false;
    bool isNewline = false;
    std::string rawText;
    MathAstNodePtr ast;
};

namespace detail_ltx {

inline std::string Trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline MathAstNodePtr MakeNode(MathAstKind k) {
    auto p = std::make_shared<MathAstNode>();
    p->kind = k;
    return p;
}

inline const std::unordered_map<std::string, std::string>& SymbolMap() {
    static const std::unordered_map<std::string, std::string> kMap = {
        { "alpha", "\xCE\xB1" }, { "beta", "\xCE\xB2" }, { "gamma", "\xCE\xB3" },
        { "delta", "\xCE\xB4" }, { "epsilon", "\xCE\xB5" }, { "varepsilon", "\xCE\xB5" },
        { "zeta", "\xCE\xB6" }, { "eta", "\xCE\xB7" }, { "theta", "\xCE\xB8" },
        { "vartheta", "\xCF\x91" }, { "iota", "\xCE\xB9" }, { "kappa", "\xCE\xBA" },
        { "lambda", "\xCE\xBB" }, { "mu", "\xCE\xBC" }, { "nu", "\xCE\xBD" },
        { "xi", "\xCE\xBE" }, { "pi", "\xCF\x80" }, { "rho", "\xCF\x81" },
        { "sigma", "\xCF\x83" }, { "tau", "\xCF\x84" }, { "upsilon", "\xCF\x85" },
        { "phi", "\xCF\x86" }, { "varphi", "\xCF\x86" }, { "chi", "\xCF\x87" },
        { "psi", "\xCF\x88" }, { "omega", "\xCF\x89" },
        { "Gamma", "\xCE\x93" }, { "Delta", "\xCE\x94" }, { "Theta", "\xCE\x98" },
        { "Lambda", "\xCE\x9B" }, { "Xi", "\xCE\x9E" }, { "Pi", "\xCE\xA0" },
        { "Sigma", "\xCE\xA3" }, { "Upsilon", "\xCE\xA5" }, { "Phi", "\xCE\xA6" },
        { "Psi", "\xCE\xA8" }, { "Omega", "\xCE\xA9" },
        { "cdot", "\xC2\xB7" }, { "times", "\xC3\x97" }, { "div", "\xC3\xB7" },
        { "pm", "\xC2\xB1" }, { "mp", "\xE2\x88\x93" }, { "leq", "\xE2\x89\xA4" },
        { "le", "\xE2\x89\xA4" }, { "geq", "\xE2\x89\xA5" }, { "ge", "\xE2\x89\xA5" },
        { "neq", "\xE2\x89\xA0" }, { "ne", "\xE2\x89\xA0" }, { "approx", "\xE2\x89\x88" },
        { "equiv", "\xE2\x89\xA1" }, { "infty", "\xE2\x88\x9E" }, { "partial", "\xE2\x88\x82" },
        { "nabla", "\xE2\x88\x87" }, { "angle", "\xE2\x88\xA0" }, { "degree", "\xC2\xB0" },
        { "to", "\xE2\x86\x92" }, { "rightarrow", "\xE2\x86\x92" }, { "leftarrow", "\xE2\x86\x90" },
        { "Rightarrow", "\xE2\x87\x92" }, { "leftrightarrow", "\xE2\x86\x94" },
        { "in", "\xE2\x88\x88" }, { "notin", "\xE2\x88\x89" }, { "subset", "\xE2\x8A\x82" },
        { "subseteq", "\xE2\x8A\x86" }, { "cup", "\xE2\x88\xAA" }, { "cap", "\xE2\x88\xA9" },
        { "forall", "\xE2\x88\x80" }, { "exists", "\xE2\x88\x83" }, { "dots", "\xE2\x80\xA6" },
        { "ldots", "\xE2\x80\xA6" }, { "cdots", "\xE2\x8B\xAF" }, { "propto", "\xE2\x88\x9D" },
        { "perp", "\xE2\x8A\xA5" }, { "parallel", "\xE2\x88\xA5" },
        { "hbar", "\xE2\x84\x8F" }, { "ell", "\xE2\x84\x93" },
        { "varpi", "\xCF\x96" }, { "varrho", "\xCF\xB1" }, { "varsigma", "\xCF\x82" },
        { "sim", "\xE2\x88\xBC" }, { "simeq", "\xE2\x89\x83" }, { "cong", "\xE2\x89\x85" },
        { "ll", "\xE2\x89\xAA" }, { "gg", "\xE2\x89\xAB" }, { "mid", "\xE2\x88\xA3" }
    };
    return kMap;
}

inline bool IsRomanFunctionCmd(const std::string& cmd) {
    static const char* kFns[] = {
        "sin", "cos", "tan", "csc", "sec", "cot",
        "asin", "acos", "atan", "arcsin", "arccos", "arctan",
        "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
        "ln", "log", "exp", "det", "tr", "rank", "dim",
        "min", "max", "sup", "inf", "arg", "Re", "Im", "conj", "gcd", "lcm", "deg", "rad"
    };
    for (const char* f : kFns) {
        if (cmd == f) return true;
    }
    return false;
}

inline bool IsBigOpCmd(const std::string& cmd, std::string& outGlyph) {
    if (cmd == "int") { outGlyph = "\xE2\x88\xAB"; return true; }
    if (cmd == "iint") { outGlyph = "\xE2\x88\xAC"; return true; }
    if (cmd == "iiint") { outGlyph = "\xE2\x88\xAD"; return true; }
    if (cmd == "oint") { outGlyph = "\xE2\x88\xAE"; return true; }
    if (cmd == "sum") { outGlyph = "\xE2\x88\x91"; return true; }
    if (cmd == "prod") { outGlyph = "\xE2\x88\x8F"; return true; }
    if (cmd == "bigcup") { outGlyph = "\xE2\x8B\x83"; return true; }
    if (cmd == "bigcap") { outGlyph = "\xE2\x8B\x82"; return true; }
    if (cmd == "lim") { outGlyph = "lim"; return true; }
    return false;
}

inline bool IsAccentCmd(const std::string& cmd) {
    return cmd == "vec" || cmd == "bar" || cmd == "hat" || cmd == "widehat" ||
           cmd == "overline" || cmd == "tilde" || cmd == "widetilde" ||
           cmd == "dot" || cmd == "ddot";
}

class LatexParserImpl {
public:
    explicit LatexParserImpl(const std::string& s) : s_(s) {}

    LatexParseResult Parse() {
        LatexParseResult res;
        SkipSpaces();
        auto root = ParseSequence("");
        if (!err_.empty()) {
            res.ok = false;
            res.errorUtf8 = err_;
            return res;
        }
        SkipSpaces();
        if (pos_ < s_.size()) {
            res.ok = false;
            res.errorUtf8 = "Token inesperado en LaTeX cerca de: " + s_.substr(pos_, (std::min<std::size_t>)(16, s_.size() - pos_));
            return res;
        }
        res.ok = true;
        res.root = root;
        return res;
    }

private:
    std::string s_;
    std::size_t pos_ = 0;
    std::string err_;
    int squareBracketDepth_ = 0;

    void SkipSpaces() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\r' || s_[pos_] == '\n')) {
            ++pos_;
        }
    }

    bool StartsWith(const char* lit) const {
        const std::size_t n = std::char_traits<char>::length(lit);
        return s_.compare(pos_, n, lit) == 0;
    }

    std::string ReadCommandName() {
        // assumes s_[pos_] == '\\'
        ++pos_;
        if (pos_ >= s_.size()) return "";
        if (!std::isalpha(static_cast<unsigned char>(s_[pos_]))) {
            std::string sym(1, s_[pos_++]);
            return sym;
        }
        const std::size_t st = pos_;
        while (pos_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        return s_.substr(st, pos_ - st);
    }

    std::string ReadBracedRaw() {
        SkipSpaces();
        if (pos_ >= s_.size() || s_[pos_] != '{') return "";
        ++pos_;
        int depth = 1;
        const std::size_t st = pos_;
        while (pos_ < s_.size() && depth > 0) {
            if (s_[pos_] == '\\' && pos_ + 1 < s_.size()) {
                pos_ += 2;
                continue;
            }
            if (s_[pos_] == '{') ++depth;
            else if (s_[pos_] == '}') --depth;
            if (depth > 0) ++pos_;
        }
        if (depth != 0) {
            err_ = "Llave '{' sin cerrar";
            return "";
        }
        const std::string inner = s_.substr(st, pos_ - st);
        ++pos_; // skip '}'
        return inner;
    }

    MathAstNodePtr ParseRequiredArg(bool forceUpright = false, bool preserveSpaces = false) {
        SkipSpaces();
        if (pos_ >= s_.size()) {
            err_ = "Falta argumento en comando LaTeX";
            return MakeNode(MathAstKind::Row);
        }
        if (s_[pos_] == '{') {
            ++pos_;
            auto seq = ParseSequence("}", forceUpright, preserveSpaces);
            if (!err_.empty()) return seq;
            if (pos_ >= s_.size() || s_[pos_] != '}') {
                err_ = "Falta '}' de cierre";
                return seq;
            }
            ++pos_;
            return seq;
        }
        return ParseSingleAtom(forceUpright);
    }

    std::string ReadDelimToken() {
        SkipSpaces();
        if (pos_ >= s_.size()) return "";
        if (s_[pos_] == '\\') {
            const std::size_t save = pos_;
            const std::string cmd = ReadCommandName();
            if (cmd == "{" || cmd == "}") return cmd;
            if (cmd == "langle") return "\xE2\x9F\xA8";
            if (cmd == "rangle") return "\xE2\x9F\xA9";
            if (cmd == "lvert" || cmd == "rvert") return "|";
            if (cmd == "lVert" || cmd == "rVert" || cmd == "|") return "\xE2\x80\x96";
            if (cmd == "lfloor") return "\xE2\x8C\x8A";
            if (cmd == "rfloor") return "\xE2\x8C\x8B";
            if (cmd == "lceil") return "\xE2\x8C\x88";
            if (cmd == "rceil") return "\xE2\x8C\x89";
            pos_ = save;
            return "";
        }
        const unsigned char uc = static_cast<unsigned char>(s_[pos_]);
        if (uc == '.') {
            ++pos_;
            return "";
        }
        std::size_t len = 1;
        if ((uc & 0xE0) == 0xC0) len = 2;
        else if ((uc & 0xF0) == 0xE0) len = 3;
        else if ((uc & 0xF8) == 0xF0) len = 4;
        len = (std::min)(len, s_.size() - pos_);
        const std::string tok = s_.substr(pos_, len);
        pos_ += len;
        return tok;
    }

    MathAstNodePtr ParseSingleAtom(bool forceUpright = false) {
        SkipSpaces();
        if (pos_ >= s_.size()) return MakeNode(MathAstKind::Row);

        const char ch = s_[pos_];
        if (ch == '{') {
            ++pos_;
            auto seq = ParseSequence("}", forceUpright);
            if (!err_.empty()) return seq;
            if (pos_ >= s_.size() || s_[pos_] != '}') {
                err_ = "Falta '}' de cierre";
                return seq;
            }
            ++pos_;
            return seq;
        }

        if (ch == '\\') {
            const std::size_t save = pos_;
            const std::string cmd = ReadCommandName();
            if (cmd.empty()) {
                err_ = "Comando LaTeX vacio";
                return MakeNode(MathAstKind::Row);
            }
            if (cmd == "," || cmd == ";" || cmd == ":" || cmd == "!" || cmd == "quad" || cmd == "qquad") {
                auto sp = MakeNode(MathAstKind::Space);
                if (cmd == "!") sp->spaceEm = -0.17;
                else if (cmd == ",") sp->spaceEm = 0.17;
                else if (cmd == ":") sp->spaceEm = 0.22;
                else if (cmd == ";") sp->spaceEm = 0.28;
                else if (cmd == "quad") sp->spaceEm = 1.0;
                else sp->spaceEm = 2.0;
                return sp;
            }
            if (cmd == "frac" || cmd == "dfrac" || cmd == "tfrac" || cmd == "binom") {
                auto fn = MakeNode(MathAstKind::Fraction);
                fn->hasBar = (cmd != "binom");
                fn->compact = (cmd == "tfrac");
                fn->num = ParseRequiredArg(forceUpright);
                if (!err_.empty()) return fn;
                fn->den = ParseRequiredArg(forceUpright);
                return fn;
            }
            if (cmd == "sqrt") {
                auto sq = MakeNode(MathAstKind::Sqrt);
                SkipSpaces();
                if (pos_ < s_.size() && s_[pos_] == '[') {
                    ++pos_;
                    sq->rootIdx = ParseSequence("]", forceUpright);
                    if (!err_.empty()) return sq;
                    if (pos_ >= s_.size() || s_[pos_] != ']') {
                        err_ = "Falta ']' en \\sqrt[n]";
                        return sq;
                    }
                    ++pos_;
                }
                sq->radicand = ParseRequiredArg(forceUpright);
                return sq;
            }
            if (cmd == "text" || cmd == "textrm" || cmd == "mathrm" || cmd == "operatorname") {
                const bool preserveTextSpaces = (cmd == "text" || cmd == "textrm");
                auto inner = ParseRequiredArg(true, preserveTextSpaces);
                inner->forceUpright = true;
                return inner;
            }
            if (cmd == "mathbf" || cmd == "boldsymbol" || cmd == "mathbb") {
                auto inner = ParseRequiredArg(cmd == "mathbf");
                inner->bold = true;
                return inner;
            }
            if (IsAccentCmd(cmd)) {
                auto ac = MakeNode(MathAstKind::Accent);
                ac->accentKind = cmd;
                ac->base = ParseRequiredArg(forceUpright);
                return ac;
            }
            std::string bigGlyph;
            if (IsBigOpCmd(cmd, bigGlyph)) {
                auto bo = MakeNode(MathAstKind::BigOp);
                bo->text = bigGlyph;
                bo->role = (cmd == "lim") ? MathFontRole::MathRoman : MathFontRole::MathSymbol;
                bo->forceUpright = (cmd == "lim");
                return bo;
            }
            if (cmd == "left") {
                const std::string openD = ReadDelimToken();
                auto dl = MakeNode(MathAstKind::Delimiter);
                dl->text = openD;
                auto inner = ParseSequence("\\right", forceUpright);
                if (!err_.empty()) return dl;
                SkipSpaces();
                if (!StartsWith("\\right")) {
                    err_ = "Falta \\right para emparejar \\left";
                    return dl;
                }
                pos_ += 6; // skip \right
                dl->closeDelim = ReadDelimToken();
                dl->children.push_back(inner);
                return dl;
            }
            if (cmd == "right") {
                pos_ = save;
                err_ = "Comando \\right sin \\left previo";
                return MakeNode(MathAstKind::Row);
            }
            if (cmd == "begin") {
                const std::string env = ReadBracedRaw();
                if (!err_.empty()) return MakeNode(MathAstKind::Row);
                if (env.empty()) {
                    err_ = "Falta nombre de entorno en \\begin{}";
                    return MakeNode(MathAstKind::Row);
                }
                auto mat = MakeNode(MathAstKind::Matrix);
                mat->envName = env;
                if (env == "pmatrix") { mat->text = "("; mat->closeDelim = ")"; }
                else if (env == "bmatrix") { mat->text = "["; mat->closeDelim = "]"; }
                else if (env == "Bmatrix") { mat->text = "{"; mat->closeDelim = "}"; }
                else if (env == "vmatrix") { mat->text = "|"; mat->closeDelim = "|"; }
                else if (env == "Vmatrix") { mat->text = "\xE2\x80\x96"; mat->closeDelim = "\xE2\x80\x96"; }
                else if (env == "cases") { mat->text = "{"; mat->closeDelim = ""; }
                else { mat->text = ""; mat->closeDelim = ""; }

                std::vector<MathAstNodePtr> curRow;
                while (pos_ < s_.size() && err_.empty()) {
                    auto cell = ParseSequence("\\end|&|\\\\", forceUpright);
                    if (!err_.empty()) return mat;
                    curRow.push_back(cell);
                    SkipSpaces();
                    if (StartsWith("\\\\")) {
                        pos_ += 2;
                        mat->grid.push_back(std::move(curRow));
                        curRow.clear();
                        continue;
                    }
                    if (pos_ < s_.size() && s_[pos_] == '&') {
                        ++pos_;
                        continue;
                    }
                    if (StartsWith("\\end")) {
                        pos_ += 4;
                        const std::string endEnv = ReadBracedRaw();
                        if (endEnv != env) {
                            err_ = "\\end{" + endEnv + "} no coincide con \\begin{" + env + "}";
                            return mat;
                        }
                        mat->grid.push_back(std::move(curRow));
                        return mat;
                    }
                    err_ = "Falta \\end{" + env + "}";
                    return mat;
                }
                err_ = "Falta \\end{" + env + "}";
                return mat;
            }
            if (IsRomanFunctionCmd(cmd)) {
                auto fn = MakeNode(MathAstKind::MathVar);
                fn->text = cmd;
                fn->role = MathFontRole::MathRoman;
                fn->forceUpright = true;
                return fn;
            }
            if (cmd == "circ") {
                auto op = MakeNode(MathAstKind::MathOp);
                op->text = "\xC2\xB0";
                op->role = MathFontRole::MathSymbol;
                op->forceUpright = forceUpright;
                return op;
            }
            if (cmd == "ohm" || cmd == "Ohm") {
                auto op = MakeNode(MathAstKind::MathOp);
                op->text = "\xCE\xA9";
                op->role = MathFontRole::MathSymbol;
                op->forceUpright = forceUpright;
                return op;
            }
            const auto& symMap = SymbolMap();
            auto it = symMap.find(cmd);
            if (it != symMap.end()) {
                auto op = MakeNode(MathAstKind::MathOp);
                op->text = it->second;
                op->role = MathFontRole::MathSymbol;
                op->forceUpright = forceUpright;
                return op;
            }
            if (cmd == "{" || cmd == "}" || cmd == "%" || cmd == "$" || cmd == "&" || cmd == "#") {
                auto op = MakeNode(MathAstKind::MathOp);
                op->text = cmd;
                op->role = MathFontRole::MathRoman;
                op->forceUpright = forceUpright;
                return op;
            }
            auto tx = MakeNode(MathAstKind::MathVar);
            tx->text = cmd;
            tx->forceUpright = forceUpright;
            tx->role = forceUpright ? MathFontRole::TextNormal : MathFontRole::MathRoman;
            return tx;
        }

        // Plain character or number run
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            const std::size_t st = pos_;
            while (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '.')) {
                ++pos_;
            }
            auto num = MakeNode(MathAstKind::TextRun);
            num->text = s_.substr(st, pos_ - st);
            num->role = MathFontRole::MathRoman;
            num->forceUpright = true;
            return num;
        }

        if (ch == '+' || ch == '-' || ch == '=' || ch == '<' || ch == '>' || ch == ',' ||
            ch == ';' || ch == ':' || ch == '/' || ch == '*' || ch == '!' || ch == '(' ||
            ch == ')' || ch == '[' || ch == ']' || ch == '|') {
            if (ch == '[') ++squareBracketDepth_;
            else if (ch == ']' && squareBracketDepth_ > 0) --squareBracketDepth_;
            auto op = MakeNode(MathAstKind::MathOp);
            op->text = std::string(1, ch);
            op->role = MathFontRole::MathRoman;
            ++pos_;
            return op;
        }

        // Single UTF-8 character or ASCII letter
        std::size_t len = 1;
        const unsigned char uc = static_cast<unsigned char>(ch);
        if ((uc & 0xE0) == 0xC0) len = 2;
        else if ((uc & 0xF0) == 0xE0) len = 3;
        else if ((uc & 0xF8) == 0xF0) len = 4;
        len = (std::min)(len, s_.size() - pos_);
        const std::string glyph = s_.substr(pos_, len);
        pos_ += len;

        auto v = MakeNode(MathAstKind::MathVar);
        v->text = glyph;
        v->forceUpright = forceUpright;
        v->role = forceUpright ? MathFontRole::TextNormal : MathFontRole::MathItalic;
        return v;
    }

    bool HitStopToken(const std::string& stopSpec) const {
        if (pos_ >= s_.size()) return true;
        if (stopSpec.empty()) return false;
        if (stopSpec == "}" && s_[pos_] == '}') return true;
        if (stopSpec == "]" && s_[pos_] == ']') return true;
        if (stopSpec == "\\right" && StartsWith("\\right")) return true;
        if (stopSpec == "\\end|&|\\\\") {
            if (StartsWith("\\end") || StartsWith("\\\\") || s_[pos_] == '&') return true;
        }
        return false;
    }

    MathAstNodePtr ParseSequence(const std::string& stopSpec, bool forceUpright = false, bool preserveSpaces = false) {
        auto row = MakeNode(MathAstKind::Row);
        row->forceUpright = forceUpright;
        if (forceUpright) row->role = MathFontRole::MathRoman;

        while (err_.empty()) {
            const std::size_t wsStart = pos_;
            SkipSpaces();
            if (HitStopToken(stopSpec)) break;
            if (stopSpec == "}" && (StartsWith("\\right") || StartsWith("\\end"))) break;
            if (stopSpec == "\\right" && s_[pos_] == '}') break;
            if (preserveSpaces && pos_ > wsStart && !row->children.empty()) {
                auto sp = MakeNode(MathAstKind::TextRun);
                sp->text = " ";
                sp->role = MathFontRole::TextNormal;
                sp->forceUpright = true;
                row->children.push_back(std::move(sp));
            }

            if (s_[pos_] == '}' || (s_[pos_] == ']' && squareBracketDepth_ == 0)) {
                err_ = std::string("Delimitador de cierre inesperado '") + s_[pos_] + "'";
                break;
            }

            if (s_[pos_] == '^' || s_[pos_] == '_') {
                // Orphan ^ or _ -> attach to empty base
                auto base = MakeNode(MathAstKind::Row);
                auto ss = ParseSupSubTail(base, forceUpright);
                row->children.push_back(ss);
                continue;
            }

            auto atom = ParseSingleAtom(forceUpright);
            if (!err_.empty()) break;

            if (!preserveSpaces) {
                SkipSpaces();
            } else {
                std::size_t look = pos_;
                while (look < s_.size() && (s_[look] == ' ' || s_[look] == '\t' || s_[look] == '\r' || s_[look] == '\n')) ++look;
                if (look < s_.size() && (s_[look] == '^' || s_[look] == '_')) {
                    pos_ = look;
                }
            }
            if (pos_ < s_.size() && (s_[pos_] == '^' || s_[pos_] == '_')) {
                atom = ParseSupSubTail(atom, forceUpright);
            }
            row->children.push_back(std::move(atom));
        }

        if (row->children.size() == 1 && row->kind == MathAstKind::Row && !forceUpright) {
            return row->children[0];
        }
        return row;
    }

    MathAstNodePtr ParseSupSubTail(const MathAstNodePtr& base, bool forceUpright) {
        MathAstNodePtr supNode;
        MathAstNodePtr subNode;
        while (pos_ < s_.size() && (s_[pos_] == '^' || s_[pos_] == '_')) {
            const char mark = s_[pos_++];
            auto arg = ParseRequiredArg(forceUpright);
            if (!err_.empty()) break;
            if (mark == '^') supNode = arg;
            else subNode = arg;
            SkipSpaces();
        }
        if (base && base->kind == MathAstKind::BigOp) {
            if (supNode) base->sup = supNode;
            if (subNode) base->sub = subNode;
            return base;
        }
        auto ss = MakeNode(MathAstKind::SupSub);
        ss->base = base;
        ss->sup = supNode;
        ss->sub = subNode;
        return ss;
    }
};

inline std::string ReverseLookupLatexSymbol(const std::string& utf8) {
    if (utf8 == "\xC2\xB7") return "\\cdot ";
    if (utf8 == "\xC3\x97") return "\\times ";
    if (utf8 == "\xC3\xB7") return "\\div ";
    if (utf8 == "\xC2\xB1") return "\\pm ";
    if (utf8 == "\xE2\x88\x93") return "\\mp ";
    if (utf8 == "\xE2\x89\xA4") return "\\leq ";
    if (utf8 == "\xE2\x89\xA5") return "\\geq ";
    if (utf8 == "\xE2\x89\xA0") return "\\neq ";
    if (utf8 == "\xE2\x89\x88") return "\\approx ";
    if (utf8 == "\xE2\x88\x9E") return "\\infty ";
    if (utf8 == "\xE2\x88\x82") return "\\partial ";
    if (utf8 == "\xE2\x88\x87") return "\\nabla ";
    if (utf8 == "\xE2\x88\xA0") return "\\angle ";
    if (utf8 == "\xE2\x86\x92") return "\\to ";
    if (utf8 == "\xE2\x87\x92") return "\\Rightarrow ";
    for (const auto& kv : SymbolMap()) {
        if (kv.second == utf8) return "\\" + kv.first + " ";
    }
    return utf8;
}

inline std::string XmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out.push_back(c);
    }
    return out;
}

inline std::string DecodeMathMlEntities(const std::string& raw) {
    static const std::unordered_map<std::string, std::string> kEnt = {
        { "amp", "&" }, { "lt", "<" }, { "gt", ">" }, { "quot", "\"" }, { "apos", "'" },
        { "nbsp", " " }, { "pi", "\xCF\x80" }, { "alpha", "\xCE\xB1" }, { "beta", "\xCE\xB2" },
        { "gamma", "\xCE\xB3" }, { "delta", "\xCE\xB4" }, { "epsilon", "\xCE\xB5" },
        { "theta", "\xCE\xB8" }, { "lambda", "\xCE\xBB" }, { "mu", "\xCE\xBC" },
        { "sigma", "\xCF\x83" }, { "phi", "\xCF\x86" }, { "omega", "\xCF\x89" },
        { "Omega", "\xCE\xA9" }, { "Delta", "\xCE\x94" }, { "Sigma", "\xCE\xA3" },
        { "sum", "\xE2\x88\x91" }, { "prod", "\xE2\x88\x8F" }, { "int", "\xE2\x88\xAB" },
        { "infin", "\xE2\x88\x9E" }, { "middot", "\xC2\xB7" }, { "times", "\xC3\x97" },
        { "divide", "\xC3\xB7" }, { "plusmn", "\xC2\xB1" }, { "minus", "-" },
        { "le", "\xE2\x89\xA4" }, { "ge", "\xE2\x89\xA5" }, { "ne", "\xE2\x89\xA0" },
        { "asymp", "\xE2\x89\x88" }, { "part", "\xE2\x88\x82" }, { "nabla", "\xE2\x88\x87" },
        { "ang", "\xE2\x88\xA0" }, { "radic", "\xE2\x88\x9A" }, { "rarr", "\xE2\x86\x92" },
        { "ApplyFunction", "" }, { "InvisibleTimes", "" }
    };
    std::string out;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '&') {
            const auto semi = raw.find(';', i + 1);
            if (semi != std::string::npos) {
                const std::string name = raw.substr(i + 1, semi - i - 1);
                if (!name.empty() && name[0] == '#') {
                    std::uint32_t cp = 0;
                    try {
                        cp = (name.size() > 1 && (name[1] == 'x' || name[1] == 'X'))
                            ? static_cast<std::uint32_t>(std::stoul(name.substr(2), nullptr, 16))
                            : static_cast<std::uint32_t>(std::stoul(name.substr(1), nullptr, 10));
                    } catch (...) { cp = 0; }
                    if (cp > 0 && cp < 0x80) out.push_back(static_cast<char>(cp));
                    else if (cp < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else if (cp < 0x10000) {
                        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    i = semi;
                    continue;
                }
                auto it = kEnt.find(name);
                if (it != kEnt.end()) {
                    out += it->second;
                    i = semi;
                    continue;
                }
            }
        }
        out.push_back(raw[i]);
    }
    return out;
}

class MathMlParserImpl {
public:
    explicit MathMlParserImpl(const std::string& xml) : s_(xml) {}

    LatexParseResult Parse() {
        LatexParseResult res;
        SkipWsAndComments();
        if (pos_ >= s_.size()) {
            res.errorUtf8 = "MathML vacio";
            return res;
        }
        auto node = ParseElement();
        if (!err_.empty()) {
            res.errorUtf8 = err_;
            return res;
        }
        if (!node) {
            res.errorUtf8 = "MathML invalido";
            return res;
        }
        res.ok = true;
        res.root = node;
        return res;
    }

private:
    std::string s_;
    std::size_t pos_ = 0;
    std::string err_;
    int squareBracketDepth_ = 0;

    void SkipWsAndComments() {
        while (pos_ < s_.size()) {
            while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
            if (s_.compare(pos_, 4, "<!--") == 0) {
                const auto end = s_.find("-->", pos_ + 4);
                pos_ = (end == std::string::npos) ? s_.size() : (end + 3);
                continue;
            }
            if (s_.compare(pos_, 2, "<?") == 0) {
                const auto end = s_.find("?>", pos_ + 2);
                pos_ = (end == std::string::npos) ? s_.size() : (end + 2);
                continue;
            }
            break;
        }
    }

    static std::string ExtractAttr(const std::string& attrStr, const std::string& key) {
        const std::string pat1 = key + "=\"";
        const std::string pat2 = key + "='";
        auto p = attrStr.find(pat1);
        char q = '"';
        if (p == std::string::npos) {
            p = attrStr.find(pat2);
            q = '\'';
        }
        if (p == std::string::npos) return "";
        p += key.size() + 2;
        const auto e = attrStr.find(q, p);
        return (e == std::string::npos) ? "" : attrStr.substr(p, e - p);
    }

    static std::string StripTagPrefix(const std::string& tag) {
        const auto colon = tag.find(':');
        return (colon == std::string::npos) ? tag : tag.substr(colon + 1);
    }

    std::string ReadInnerTextUntilClose(const std::string& rawTag) {
        const std::size_t st = pos_;
        const std::string closePat = "</" + rawTag;
        const auto end = s_.find(closePat, pos_);
        if (end == std::string::npos) {
            err_ = "Etiqueta MathML sin cerrar: <" + rawTag + ">";
            return "";
        }
        const std::string text = DecodeMathMlEntities(s_.substr(st, end - st));
        const auto gt = s_.find('>', end);
        if (gt == std::string::npos) {
            err_ = "Cierre XML malformado";
            return "";
        }
        pos_ = gt + 1;
        return Trim(text);
    }

    MathAstNodePtr ParseElement() {
        SkipWsAndComments();
        if (pos_ >= s_.size() || s_[pos_] != '<') {
            err_ = "Se esperaba '<' en MathML";
            return nullptr;
        }
        if (pos_ + 1 < s_.size() && s_[pos_ + 1] == '/') {
            err_ = "Etiqueta de cierre inesperada";
            return nullptr;
        }
        ++pos_;
        const std::size_t nameSt = pos_;
        while (pos_ < s_.size() && !std::isspace(static_cast<unsigned char>(s_[pos_])) && s_[pos_] != '>' && s_[pos_] != '/') {
            ++pos_;
        }
        const std::string rawTag = s_.substr(nameSt, pos_ - nameSt);
        const std::string tag = StripTagPrefix(rawTag);
        const std::size_t attrSt = pos_;
        bool selfClosing = false;
        while (pos_ < s_.size() && s_[pos_] != '>') {
            if (s_[pos_] == '/' && pos_ + 1 < s_.size() && s_[pos_ + 1] == '>') {
                selfClosing = true;
                break;
            }
            ++pos_;
        }
        if (pos_ >= s_.size()) {
            err_ = "Etiqueta sin '>'";
            return nullptr;
        }
        const std::string attrStr = s_.substr(attrSt, pos_ - attrSt);
        pos_ += selfClosing ? 2 : 1;

        if (tag == "mspace") {
            if (!selfClosing) ReadInnerTextUntilClose(rawTag);
            auto sp = MakeNode(MathAstKind::Space);
            sp->spaceEm = 0.25;
            return sp;
        }

        if (tag == "mi" || tag == "mn" || tag == "mo" || tag == "mtext" || tag == "ms") {
            const std::string txt = selfClosing ? std::string() : ReadInnerTextUntilClose(rawTag);
            if (!err_.empty()) return nullptr;
            if (tag == "mn") {
                auto n = MakeNode(MathAstKind::TextRun);
                n->text = txt;
                n->role = MathFontRole::MathRoman;
                return n;
            }
            if (tag == "mtext") {
                auto n = MakeNode(MathAstKind::TextRun);
                n->text = txt;
                n->role = MathFontRole::TextNormal;
                n->forceUpright = true;
                return n;
            }
            if (tag == "mo") {
                auto n = MakeNode(MathAstKind::MathOp);
                n->text = txt;
                n->role = MathFontRole::MathSymbol;
                return n;
            }
            // <mi>
            auto n = MakeNode(MathAstKind::MathVar);
            n->text = txt;
            const std::string mv = ExtractAttr(attrStr, "mathvariant");
            if (mv == "normal" || IsRomanFunctionCmd(txt)) {
                n->role = MathFontRole::MathRoman;
                n->forceUpright = true;
            } else if (mv == "bold") {
                n->role = MathFontRole::MathBold;
                n->bold = true;
            } else {
                n->role = MathFontRole::MathItalic;
            }
            return n;
        }

        std::vector<MathAstNodePtr> kids;
        if (!selfClosing) {
            while (err_.empty()) {
                SkipWsAndComments();
                if (pos_ >= s_.size()) {
                    err_ = "Falta etiqueta de cierre </" + rawTag + ">";
                    return nullptr;
                }
                if (s_.compare(pos_, 2, "</") == 0) {
                    const auto gt = s_.find('>', pos_ + 2);
                    if (gt == std::string::npos) { err_ = "Cierre malformado"; return nullptr; }
                    const std::string endName = Trim(s_.substr(pos_ + 2, gt - (pos_ + 2)));
                    pos_ = gt + 1;
                    if (StripTagPrefix(endName) != tag) {
                        err_ = "Cierre </" + endName + "> no coincide con <" + rawTag + ">";
                        return nullptr;
                    }
                    break;
                }
                if (s_[pos_] == '<') {
                    auto child = ParseElement();
                    if (!err_.empty()) return nullptr;
                    if (child) kids.push_back(std::move(child));
                } else {
                    const std::size_t tst = pos_;
                    while (pos_ < s_.size() && s_[pos_] != '<') ++pos_;
                    const std::string t = Trim(DecodeMathMlEntities(s_.substr(tst, pos_ - tst)));
                    if (!t.empty()) {
                        auto tr = MakeNode(MathAstKind::TextRun);
                        tr->text = t;
                        tr->role = MathFontRole::TextNormal;
                        kids.push_back(std::move(tr));
                    }
                }
            }
        }

        auto wrapRow = [&](std::vector<MathAstNodePtr> v) -> MathAstNodePtr {
            if (v.size() == 1) return v[0];
            auto r = MakeNode(MathAstKind::Row);
            r->children = std::move(v);
            return r;
        };

        if (tag == "math" || tag == "mrow" || tag == "mstyle" || tag == "semantics" || tag == "mpadded") {
            auto applyFenceToMatrix = [](const MathAstNodePtr& mat, const std::string& op, const std::string& cl) -> bool {
                if (!mat || mat->kind != MathAstKind::Matrix) return false;
                if (op == "(" && cl == ")") { mat->envName = "pmatrix"; mat->text = "("; mat->closeDelim = ")"; return true; }
                if (op == "[" && cl == "]") { mat->envName = "bmatrix"; mat->text = "["; mat->closeDelim = "]"; return true; }
                if (op == "{" && cl == "}") { mat->envName = "Bmatrix"; mat->text = "{"; mat->closeDelim = "}"; return true; }
                if (op == "|" && cl == "|") { mat->envName = "vmatrix"; mat->text = "|"; mat->closeDelim = "|"; return true; }
                if (op == "\xE2\x80\x96" && cl == "\xE2\x80\x96") { mat->envName = "Vmatrix"; mat->text = op; mat->closeDelim = cl; return true; }
                return false;
            };
            std::vector<MathAstNodePtr> collapsed;
            collapsed.reserve(kids.size());
            for (std::size_t idx = 0; idx < kids.size(); ++idx) {
                if (idx + 2 < kids.size() &&
                    kids[idx] && kids[idx]->kind == MathAstKind::MathOp &&
                    kids[idx + 1] && kids[idx + 1]->kind == MathAstKind::Matrix &&
                    kids[idx + 2] && kids[idx + 2]->kind == MathAstKind::MathOp &&
                    applyFenceToMatrix(kids[idx + 1], kids[idx]->text, kids[idx + 2]->text)) {
                    collapsed.push_back(kids[idx + 1]);
                    idx += 2;
                    continue;
                }
                if (idx + 1 < kids.size() &&
                    kids[idx] && kids[idx]->kind == MathAstKind::MathOp && kids[idx]->text == "{" &&
                    kids[idx + 1] && kids[idx + 1]->kind == MathAstKind::Matrix &&
                    (idx + 2 >= kids.size() || !kids[idx + 2] || kids[idx + 2]->kind != MathAstKind::MathOp || kids[idx + 2]->text != "}")) {
                    kids[idx + 1]->envName = "cases";
                    kids[idx + 1]->text = "{";
                    kids[idx + 1]->closeDelim.clear();
                    collapsed.push_back(kids[idx + 1]);
                    idx += 1;
                    continue;
                }
                collapsed.push_back(kids[idx]);
            }
            return wrapRow(std::move(collapsed));
        }
        if (tag == "mfrac") {
            auto f = MakeNode(MathAstKind::Fraction);
            const std::string lt = ExtractAttr(attrStr, "linethickness");
            f->hasBar = (lt != "0" && lt != "0px" && lt != "0pt");
            if (kids.size() >= 1) f->num = kids[0];
            if (kids.size() >= 2) f->den = kids[1];
            return f;
        }
        if (tag == "msqrt") {
            auto sq = MakeNode(MathAstKind::Sqrt);
            sq->radicand = wrapRow(std::move(kids));
            return sq;
        }
        if (tag == "mroot") {
            auto sq = MakeNode(MathAstKind::Sqrt);
            if (kids.size() >= 1) sq->radicand = kids[0];
            if (kids.size() >= 2) sq->rootIdx = kids[1];
            return sq;
        }
        if (tag == "msup" || tag == "msub" || tag == "msubsup" ||
            tag == "mover" || tag == "munder" || tag == "munderover") {
            if (kids.size() >= 1 && kids[0] &&
                (kids[0]->text == "\xE2\x88\x91" || kids[0]->text == "\xE2\x88\x8F" ||
                 kids[0]->text == "\xE2\x88\xAB" || kids[0]->text == "\xE2\x88\xAE" ||
                 kids[0]->text == "lim")) {
                auto bo = MakeNode(MathAstKind::BigOp);
                bo->text = kids[0]->text;
                bo->role = MathFontRole::MathSymbol;
                if (tag == "msup" || tag == "mover") {
                    if (kids.size() >= 2) bo->sup = kids[1];
                } else if (tag == "msub" || tag == "munder") {
                    if (kids.size() >= 2) bo->sub = kids[1];
                } else {
                    if (kids.size() >= 2) bo->sub = kids[1];
                    if (kids.size() >= 3) bo->sup = kids[2];
                }
                return bo;
            }
            if (tag == "mover" && kids.size() >= 2 && kids[1]) {
                const std::string& acc = kids[1]->text;
                if (acc == "\xE2\x86\x92" || acc == "^" || acc == "\xC2\xAF" || acc == "~" || acc == "." || acc == "..") {
                    auto ac = MakeNode(MathAstKind::Accent);
                    ac->base = kids[0];
                    if (acc == "\xE2\x86\x92") ac->accentKind = "vec";
                    else if (acc == "^") ac->accentKind = "hat";
                    else if (acc == "~") ac->accentKind = "tilde";
                    else if (acc == ".") ac->accentKind = "dot";
                    else if (acc == "..") ac->accentKind = "ddot";
                    else ac->accentKind = "bar";
                    return ac;
                }
            }
            auto ss = MakeNode(MathAstKind::SupSub);
            if (kids.size() >= 1) ss->base = kids[0];
            if (tag == "msup" || tag == "mover") {
                if (kids.size() >= 2) ss->sup = kids[1];
            } else if (tag == "msub" || tag == "munder") {
                if (kids.size() >= 2) ss->sub = kids[1];
            } else {
                if (kids.size() >= 2) ss->sub = kids[1];
                if (kids.size() >= 3) ss->sup = kids[2];
            }
            return ss;
        }
        if (tag == "mfenced") {
            const std::string op = ExtractAttr(attrStr, "open");
            const std::string cl = ExtractAttr(attrStr, "close");
            const std::string openTok = op.empty() ? "(" : op;
            const std::string closeTok = cl.empty() ? ")" : cl;
            if (kids.size() == 1 && kids[0] && kids[0]->kind == MathAstKind::Matrix) {
                auto& mat = kids[0];
                if (openTok == "(" && closeTok == ")") { mat->envName = "pmatrix"; mat->text = "("; mat->closeDelim = ")"; }
                else if (openTok == "[" && closeTok == "]") { mat->envName = "bmatrix"; mat->text = "["; mat->closeDelim = "]"; }
                else if (openTok == "{" && closeTok == "}") { mat->envName = "Bmatrix"; mat->text = "{"; mat->closeDelim = "}"; }
                else if (openTok == "|" && closeTok == "|") { mat->envName = "vmatrix"; mat->text = "|"; mat->closeDelim = "|"; }
                return mat;
            }
            auto dl = MakeNode(MathAstKind::Delimiter);
            dl->text = openTok;
            dl->closeDelim = closeTok;
            dl->children = std::move(kids);
            return dl;
        }
        if (tag == "mtable") {
            auto mat = MakeNode(MathAstKind::Matrix);
            mat->envName = "bmatrix";
            mat->text = "[";
            mat->closeDelim = "]";
            for (const auto& r : kids) {
                if (r && r->kind == MathAstKind::Matrix && !r->grid.empty()) {
                    mat->grid.push_back(r->grid[0]);
                } else if (r) {
                    mat->grid.push_back({ r });
                }
            }
            return mat;
        }
        if (tag == "mtr" || tag == "mlabeledtr") {
            auto rowWrap = MakeNode(MathAstKind::Matrix);
            rowWrap->grid.push_back(std::move(kids));
            return rowWrap;
        }
        if (tag == "mtd") {
            return wrapRow(std::move(kids));
        }
        return wrapRow(std::move(kids));
    }
};

} // namespace detail_ltx

inline LatexParseResult ParseLatexToAst(const std::string& latexUtf8) {
    std::string trimmed = detail_ltx::Trim(latexUtf8);
    if (trimmed.size() >= 4 && trimmed.substr(0, 2) == "$$" && trimmed.substr(trimmed.size() - 2) == "$$") {
        trimmed = trimmed.substr(2, trimmed.size() - 4);
    } else if (trimmed.size() >= 2 && trimmed.front() == '$' && trimmed.back() == '$') {
        trimmed = trimmed.substr(1, trimmed.size() - 2);
    } else if (trimmed.size() >= 4 && trimmed.substr(0, 2) == "\\[" && trimmed.substr(trimmed.size() - 2) == "\\]") {
        trimmed = trimmed.substr(2, trimmed.size() - 4);
    } else if (trimmed.size() >= 4 && trimmed.substr(0, 2) == "\\(" && trimmed.substr(trimmed.size() - 2) == "\\)") {
        trimmed = trimmed.substr(2, trimmed.size() - 4);
    }
    detail_ltx::LatexParserImpl parser(trimmed);
    return parser.Parse();
}

inline LatexValidationResult ValidateLatexUtf8(const std::string& rawUtf8) {
    LatexValidationResult out;
    int braceDepth = 0;
    for (std::size_t i = 0; i < rawUtf8.size(); ++i) {
        if (rawUtf8[i] == '\\' && i + 1 < rawUtf8.size()) {
            ++i;
            continue;
        }
        if (rawUtf8[i] == '{') ++braceDepth;
        else if (rawUtf8[i] == '}') {
            --braceDepth;
            if (braceDepth < 0) {
                out.valid = false;
                out.errorUtf8 = "Llave '}' sin apertura";
                return out;
            }
        }
    }
    if (braceDepth != 0) {
        out.valid = false;
        out.errorUtf8 = "Llave '{' sin cerrar";
        return out;
    }

    // Strip $...$ / $$...$$ checks
    std::size_t dollarCount = 0;
    std::size_t i = 0;
    while (i < rawUtf8.size()) {
        if (rawUtf8[i] == '\\') { i += 2; continue; }
        if (rawUtf8[i] == '$') {
            if (i + 1 < rawUtf8.size() && rawUtf8[i + 1] == '$') { i += 2; dollarCount += 2; }
            else { ++i; ++dollarCount; }
            continue;
        }
        ++i;
    }
    if ((dollarCount % 2) != 0) {
        out.valid = false;
        out.errorUtf8 = "Delimitador '$' sin cerrar";
        return out;
    }

    const auto parsed = ParseLatexToAst(rawUtf8);
    if (!parsed.ok) {
        out.valid = false;
        out.errorUtf8 = parsed.errorUtf8;
    }
    return out;
}

inline std::string EmitLatexFromAst(const MathAstNodePtr& node) {
    if (!node) return "";
    auto wrapBraced = [](const MathAstNodePtr& child) -> std::string {
        const std::string s = EmitLatexFromAst(child);
        if (s.size() == 1 && std::isalnum(static_cast<unsigned char>(s[0]))) return s;
        return "{" + s + "}";
    };
    switch (node->kind) {
    case MathAstKind::Row: {
        std::string out;
        for (std::size_t i = 0; i < node->children.size(); ++i) {
            const std::string part = EmitLatexFromAst(node->children[i]);
            if (!out.empty() && !part.empty() &&
                std::isalpha(static_cast<unsigned char>(out.back())) &&
                std::isalpha(static_cast<unsigned char>(part.front()))) {
                out.push_back(' ');
            }
            out += part;
        }
        if (node->forceUpright) return "\\mathrm{" + out + "}";
        if (node->bold) return "\\mathbf{" + out + "}";
        return out;
    }
    case MathAstKind::TextRun:
        if (node->text == " ") return " ";
        if (node->forceUpright) return "\\text{" + node->text + "}";
        return node->text;
    case MathAstKind::MathVar:
        if (node->forceUpright && detail_ltx::IsRomanFunctionCmd(node->text)) {
            return "\\" + node->text;
        }
        if (node->forceUpright) return "\\mathrm{" + node->text + "}";
        if (node->bold) return "\\mathbf{" + node->text + "}";
        return detail_ltx::ReverseLookupLatexSymbol(node->text);
    case MathAstKind::MathOp:
        return detail_ltx::ReverseLookupLatexSymbol(node->text);
    case MathAstKind::Space:
        if (node->spaceEm < 0.0) return "\\!";
        if (node->spaceEm >= 1.5) return "\\qquad ";
        if (node->spaceEm >= 0.8) return "\\quad ";
        return "\\,";
    case MathAstKind::Fraction: {
        const std::string cmd = !node->hasBar ? "\\binom" : (node->compact ? "\\tfrac" : "\\frac");
        return cmd + "{" + EmitLatexFromAst(node->num) + "}{" + EmitLatexFromAst(node->den) + "}";
    }
    case MathAstKind::Sqrt:
        if (node->rootIdx) {
            return "\\sqrt[" + EmitLatexFromAst(node->rootIdx) + "]{" + EmitLatexFromAst(node->radicand) + "}";
        }
        return "\\sqrt{" + EmitLatexFromAst(node->radicand) + "}";
    case MathAstKind::SupSub: {
        std::string b = EmitLatexFromAst(node->base);
        if (node->base && node->base->kind == MathAstKind::Row && node->base->children.size() > 1) {
            b = "{" + b + "}";
        }
        std::string out = b;
        if (node->sub) out += "_" + wrapBraced(node->sub);
        if (node->sup) out += "^" + wrapBraced(node->sup);
        return out;
    }
    case MathAstKind::BigOp: {
        std::string cmd = "\\int";
        if (node->text == "\xE2\x88\x91") cmd = "\\sum";
        else if (node->text == "\xE2\x88\x8F") cmd = "\\prod";
        else if (node->text == "\xE2\x88\xAC") cmd = "\\iint";
        else if (node->text == "\xE2\x88\xAD") cmd = "\\iiint";
        else if (node->text == "\xE2\x88\xAE") cmd = "\\oint";
        else if (node->text == "\xE2\x8B\x83") cmd = "\\bigcup";
        else if (node->text == "\xE2\x8B\x82") cmd = "\\bigcap";
        else if (node->text == "lim") cmd = "\\lim";
        std::string out = cmd;
        if (node->sub) out += "_{" + EmitLatexFromAst(node->sub) + "}";
        if (node->sup) out += "^{" + EmitLatexFromAst(node->sup) + "}";
        return out + " ";
    }
    case MathAstKind::Accent:
        return "\\" + (node->accentKind.empty() ? std::string("vec") : node->accentKind) +
               "{" + EmitLatexFromAst(node->base) + "}";
    case MathAstKind::Delimiter: {
        auto emitDelim = [](const std::string& d) -> std::string {
            if (d.empty()) return ".";
            if (d == "\xE2\x80\x96") return "\\|";
            if (d == "\xE2\x9F\xA8") return "\\langle ";
            if (d == "\xE2\x9F\xA9") return "\\rangle ";
            if (d == "\xE2\x8C\x8A") return "\\lfloor ";
            if (d == "\xE2\x8C\x8B") return "\\rfloor ";
            if (d == "\xE2\x8C\x88") return "\\lceil ";
            if (d == "\xE2\x8C\x89") return "\\rceil ";
            if (d == "{" || d == "}") return "\\" + d;
            return d;
        };
        const std::string op = emitDelim(node->text);
        const std::string cl = emitDelim(node->closeDelim);
        std::string body;
        for (const auto& c : node->children) body += EmitLatexFromAst(c);
        return "\\left" + op + body + "\\right" + cl;
    }
    case MathAstKind::Matrix: {
        const std::string env = node->envName.empty() ? std::string("bmatrix") : node->envName;
        std::ostringstream oss;
        oss << "\\begin{" << env << "} ";
        for (std::size_t r = 0; r < node->grid.size(); ++r) {
            if (r > 0) oss << " \\\\ ";
            for (std::size_t c = 0; c < node->grid[r].size(); ++c) {
                if (c > 0) oss << " & ";
                oss << EmitLatexFromAst(node->grid[r][c]);
            }
        }
        oss << " \\end{" << env << "}";
        return oss.str();
    }
    }
    return "";
}

inline std::string EmitMathMLInner(const MathAstNodePtr& node) {
    if (!node) return "<mrow/>";
    switch (node->kind) {
    case MathAstKind::Row: {
        std::string out = "<mrow>";
        for (const auto& c : node->children) out += EmitMathMLInner(c);
        out += "</mrow>";
        return out;
    }
    case MathAstKind::TextRun:
        if (node->forceUpright || node->role == MathFontRole::TextNormal || node->role == MathFontRole::TextBold) {
            return "<mtext>" + detail_ltx::XmlEscape(node->text) + "</mtext>";
        }
        return "<mn>" + detail_ltx::XmlEscape(node->text) + "</mn>";
    case MathAstKind::MathVar: {
        std::string attr;
        if (node->bold) attr = " mathvariant=\"bold\"";
        else if (node->forceUpright || node->role == MathFontRole::MathRoman) attr = " mathvariant=\"normal\"";
        return "<mi" + attr + ">" + detail_ltx::XmlEscape(node->text) + "</mi>";
    }
    case MathAstKind::MathOp:
        return "<mo>" + detail_ltx::XmlEscape(node->text) + "</mo>";
    case MathAstKind::Space:
        return "<mspace width=\"0.25em\"/>";
    case MathAstKind::Fraction:
        return std::string("<mfrac") + (node->hasBar ? "" : " linethickness=\"0\"") + ">" +
               EmitMathMLInner(node->num) + EmitMathMLInner(node->den) + "</mfrac>";
    case MathAstKind::Sqrt:
        if (node->rootIdx) {
            return "<mroot>" + EmitMathMLInner(node->radicand) + EmitMathMLInner(node->rootIdx) + "</mroot>";
        }
        return "<msqrt>" + EmitMathMLInner(node->radicand) + "</msqrt>";
    case MathAstKind::SupSub: {
        const std::string b = EmitMathMLInner(node->base);
        if (node->sub && node->sup) {
            return "<msubsup>" + b + EmitMathMLInner(node->sub) + EmitMathMLInner(node->sup) + "</msubsup>";
        }
        if (node->sup) {
            return "<msup>" + b + EmitMathMLInner(node->sup) + "</msup>";
        }
        if (node->sub) {
            return "<msub>" + b + EmitMathMLInner(node->sub) + "</msub>";
        }
        return b;
    }
    case MathAstKind::BigOp: {
        const std::string op = "<mo>" + detail_ltx::XmlEscape(node->text) + "</mo>";
        if (node->sub && node->sup) {
            return "<munderover>" + op + EmitMathMLInner(node->sub) + EmitMathMLInner(node->sup) + "</munderover>";
        }
        if (node->sub) return "<munder>" + op + EmitMathMLInner(node->sub) + "</munder>";
        if (node->sup) return "<mover>" + op + EmitMathMLInner(node->sup) + "</mover>";
        return op;
    }
    case MathAstKind::Accent: {
        std::string glyph = "\xC2\xAF";
        if (node->accentKind == "vec") glyph = "\xE2\x86\x92";
        else if (node->accentKind == "hat" || node->accentKind == "widehat") glyph = "^";
        else if (node->accentKind == "tilde" || node->accentKind == "widetilde") glyph = "~";
        else if (node->accentKind == "dot") glyph = ".";
        else if (node->accentKind == "ddot") glyph = "..";
        return "<mover accent=\"true\">" + EmitMathMLInner(node->base) + "<mo>" + detail_ltx::XmlEscape(glyph) + "</mo></mover>";
    }
    case MathAstKind::Delimiter: {
        std::string out = "<mrow>";
        if (!node->text.empty()) out += "<mo stretchy=\"true\">" + detail_ltx::XmlEscape(node->text) + "</mo>";
        for (const auto& c : node->children) out += EmitMathMLInner(c);
        if (!node->closeDelim.empty()) out += "<mo stretchy=\"true\">" + detail_ltx::XmlEscape(node->closeDelim) + "</mo>";
        out += "</mrow>";
        return out;
    }
    case MathAstKind::Matrix: {
        std::ostringstream oss;
        const bool hasParens = !node->text.empty() || !node->closeDelim.empty();
        if (hasParens) {
            oss << "<mrow>";
            if (!node->text.empty()) oss << "<mo stretchy=\"true\">" << detail_ltx::XmlEscape(node->text) << "</mo>";
        }
        oss << "<mtable>";
        for (const auto& row : node->grid) {
            oss << "<mtr>";
            for (const auto& cell : row) {
                oss << "<mtd>" << EmitMathMLInner(cell) << "</mtd>";
            }
            oss << "</mtr>";
        }
        oss << "</mtable>";
        if (hasParens) {
            if (!node->closeDelim.empty()) oss << "<mo stretchy=\"true\">" << detail_ltx::XmlEscape(node->closeDelim) << "</mo>";
            oss << "</mrow>";
        }
        return oss.str();
    }
    }
    return "<mrow/>";
}

inline std::string EmitMathMLFromAst(const MathAstNodePtr& node, bool displayBlock = false) {
    return std::string("<math xmlns=\"http://www.w3.org/1998/Math/MathML\" display=\"") +
           (displayBlock ? "block" : "inline") + "\">" + EmitMathMLInner(node) + "</math>";
}

inline LatexParseResult ParseMathMLToAst(const std::string& mathmlXmlUtf8) {
    detail_ltx::MathMlParserImpl parser(mathmlXmlUtf8);
    return parser.Parse();
}

inline std::string LatexToMathML(const std::string& latexUtf8, bool displayBlock = false, std::string* outErr = nullptr) {
    const auto parsed = ParseLatexToAst(latexUtf8);
    if (!parsed.ok) {
        if (outErr) *outErr = parsed.errorUtf8;
        return "";
    }
    return EmitMathMLFromAst(parsed.root, displayBlock);
}

inline std::string MathMLToLatex(const std::string& mathmlUtf8, std::string* outErr = nullptr) {
    const auto parsed = ParseMathMLToAst(mathmlUtf8);
    if (!parsed.ok) {
        if (outErr) *outErr = parsed.errorUtf8;
        return "";
    }
    return EmitLatexFromAst(parsed.root);
}

inline std::string TryPromoteAutoMathTokenUtf8(const std::string& tok) {
    if (tok.empty()) return "";
    // Promote tokens with Unicode math operators, superscripts, or clear a/b fraction form
    std::string out = tok;
    bool changed = false;
    if (out.find("\xE2\x88\x9A") != std::string::npos) { // √
        changed = true;
        std::size_t p = 0;
        while ((p = out.find("\xE2\x88\x9A", p)) != std::string::npos) {
            out.replace(p, 3, "\\sqrt{");
            const std::size_t argSt = p + 6;
            std::size_t argEnd = argSt;
            while (argEnd < out.size() && (std::isalnum(static_cast<unsigned char>(out[argEnd])) || out[argEnd] == '_')) {
                ++argEnd;
            }
            out.insert(argEnd, "}");
            p = argEnd + 1;
        }
    }
    if (out.find("\xC2\xB2") != std::string::npos) { // ²
        changed = true;
        std::size_t p = 0;
        while ((p = out.find("\xC2\xB2", p)) != std::string::npos) {
            out.replace(p, 2, "^{2}");
            p += 4;
        }
    }
    if (out.find("\xC2\xB3") != std::string::npos) { // ³
        changed = true;
        std::size_t p = 0;
        while ((p = out.find("\xC2\xB3", p)) != std::string::npos) {
            out.replace(p, 2, "^{3}");
            p += 4;
        }
    }
    return changed ? out : "";
}

inline std::vector<RichLatexSpan> TokenizeRichLatexUtf8(const std::string& textUtf8, bool mathDefault = false) {
    std::vector<RichLatexSpan> spans;
    if (mathDefault) {
        RichLatexSpan sp;
        sp.isMath = true;
        sp.rawText = textUtf8;
        sp.ast = ParseLatexToAst(textUtf8).root;
        spans.push_back(std::move(sp));
        return spans;
    }
    std::size_t i = 0;
    std::string plainBuf;
    auto flushPlain = [&]() {
        if (plainBuf.empty()) return;
        // Scan space-delimited tokens for auto-math promotion
        std::istringstream iss(plainBuf);
        std::string word;
        std::string curText;
        while (iss >> word) {
            const std::string promoted = TryPromoteAutoMathTokenUtf8(word);
            if (!promoted.empty()) {
                if (!curText.empty()) {
                    spans.push_back({ false, false, false, curText + " ", nullptr });
                    curText.clear();
                }
                RichLatexSpan msp;
                msp.isMath = true;
                msp.isAutoMath = true;
                msp.rawText = promoted;
                msp.ast = ParseLatexToAst(promoted).root;
                spans.push_back(std::move(msp));
            } else {
                if (!curText.empty()) curText.push_back(' ');
                curText += word;
            }
        }
        if (!curText.empty()) {
            spans.push_back({ false, false, false, curText, nullptr });
        }
        plainBuf.clear();
    };

    while (i < textUtf8.size()) {
        if (textUtf8[i] == '\n') {
            flushPlain();
            spans.push_back({ false, false, true, "\n", nullptr });
            ++i;
            continue;
        }
        if (textUtf8[i] == '$') {
            const bool isDouble = (i + 1 < textUtf8.size() && textUtf8[i + 1] == '$');
            const std::size_t delimLen = isDouble ? 2 : 1;
            const auto close = textUtf8.find(isDouble ? "$$" : "$", i + delimLen);
            if (close != std::string::npos) {
                flushPlain();
                const std::string body = textUtf8.substr(i + delimLen, close - (i + delimLen));
                RichLatexSpan msp;
                msp.isMath = true;
                msp.rawText = body;
                msp.ast = ParseLatexToAst(body).root;
                spans.push_back(std::move(msp));
                i = close + delimLen;
                continue;
            }
        }
        if (textUtf8.compare(i, 2, "\\(") == 0 || textUtf8.compare(i, 2, "\\[") == 0) {
            const std::string closeTok = (textUtf8[i + 1] == '(') ? "\\)" : "\\]";
            const auto close = textUtf8.find(closeTok, i + 2);
            if (close != std::string::npos) {
                flushPlain();
                const std::string body = textUtf8.substr(i + 2, close - (i + 2));
                RichLatexSpan msp;
                msp.isMath = true;
                msp.rawText = body;
                msp.ast = ParseLatexToAst(body).root;
                spans.push_back(std::move(msp));
                i = close + 2;
                continue;
            }
        }
        plainBuf.push_back(textUtf8[i++]);
    }
    flushPlain();
    return spans;
}

inline bool ConvertLatexToCasExpr(
    const std::string& latexUtf8,
    std::string& outCasExprUtf8,
    std::string* outErr = nullptr)
{
    std::string s = detail_ltx::Trim(latexUtf8);
    if (s.empty()) {
        if (outErr) *outErr = "Expresion LaTeX vacia";
        return false;
    }
    if (s.size() >= 4 && s.substr(0, 2) == "$$" && s.substr(s.size() - 2) == "$$") s = s.substr(2, s.size() - 4);
    else if (s.size() >= 2 && s.front() == '$' && s.back() == '$') s = s.substr(1, s.size() - 2);
    else if (s.size() >= 4 && s.substr(0, 2) == "\\[" && s.substr(s.size() - 2) == "\\]") s = s.substr(2, s.size() - 4);
    else if (s.size() >= 4 && s.substr(0, 2) == "\\(" && s.substr(s.size() - 2) == "\\)") s = s.substr(2, s.size() - 4);

    // Quick balanced check via ParseLatexToAst (also handles non-LaTeX fallback)
    std::size_t pos = 0;
    auto skipWs = [&]() {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    };
    auto readBrace = [&](std::string& out) -> bool {
        skipWs();
        if (pos >= s.size() || s[pos] != '{') return false;
        ++pos;
        int depth = 1;
        const std::size_t st = pos;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '\\' && pos + 1 < s.size()) { pos += 2; continue; }
            if (s[pos] == '{') ++depth;
            else if (s[pos] == '}') --depth;
            if (depth > 0) ++pos;
        }
        if (depth != 0) return false;
        out = s.substr(st, pos - st);
        ++pos;
        return true;
    };
    auto readBracket = [&](std::string& out) -> bool {
        skipWs();
        if (pos >= s.size() || s[pos] != '[') return false;
        ++pos;
        int depth = 1;
        const std::size_t st = pos;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '[') ++depth;
            else if (s[pos] == ']') --depth;
            if (depth > 0) ++pos;
        }
        if (depth != 0) return false;
        out = s.substr(st, pos - st);
        ++pos;
        return true;
    };

    std::string out;
    auto maybeSpace = [&]() {
        if (!out.empty() && out.back() != ' ' && out.back() != '(' && out.back() != '+' &&
            out.back() != '-' && out.back() != '*' && out.back() != '/' && out.back() != '^') {
            out.push_back(' ');
        }
    };

    while (pos < s.size()) {
        const char ch = s[pos];
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++pos;
            continue;
        }
        if (ch == '\\') {
            ++pos;
            if (pos >= s.size()) break;
            if (s[pos] == ',' || s[pos] == ';' || s[pos] == ':' || s[pos] == '!') {
                ++pos;
                maybeSpace();
                continue;
            }
            if (s[pos] == '{' || s[pos] == '}' || s[pos] == '|') {
                out.push_back(s[pos++]);
                continue;
            }
            const std::size_t st = pos;
            while (pos < s.size() && std::isalpha(static_cast<unsigned char>(s[pos]))) ++pos;
            const std::string cmd = s.substr(st, pos - st);
            if (cmd == "frac" || cmd == "dfrac" || cmd == "tfrac") {
                std::string a, b, ca, cb;
                if (!readBrace(a) || !readBrace(b) ||
                    !ConvertLatexToCasExpr(a, ca, outErr) || !ConvertLatexToCasExpr(b, cb, outErr)) {
                    if (outErr && outErr->empty()) *outErr = "Argumentos invalidos en \\frac";
                    return false;
                }
                maybeSpace();
                out += "((" + ca + ")/(" + cb + "))";
                continue;
            }
            if (cmd == "sqrt") {
                std::string nStr, xStr, cx;
                const bool hasRoot = readBracket(nStr);
                if (!readBrace(xStr) || !ConvertLatexToCasExpr(xStr, cx, outErr)) {
                    if (outErr && outErr->empty()) *outErr = "Argumento invalido en \\sqrt";
                    return false;
                }
                maybeSpace();
                if (hasRoot) {
                    std::string cn;
                    if (!ConvertLatexToCasExpr(nStr, cn, outErr)) return false;
                    out += "((" + cx + ")^(1/(" + cn + ")))";
                } else {
                    out += "sqrt(" + cx + ")";
                }
                continue;
            }
            if (cmd == "mathrm" || cmd == "text" || cmd == "textrm" || cmd == "operatorname") {
                std::string inner;
                if (!readBrace(inner)) return false;
                // Convert \Omega inside unit text
                std::string norm;
                for (std::size_t k = 0; k < inner.size();) {
                    if (inner.compare(k, 6, "\\Omega") == 0) { norm += "Ohm"; k += 6; }
                    else if (inner.compare(k, 3, "\\mu") == 0) { norm += "u"; k += 3; }
                    else norm.push_back(inner[k++]);
                }
                maybeSpace();
                out += detail_ltx::Trim(norm);
                continue;
            }
            if (cmd == "left") {
                skipWs();
                if (pos < s.size() && s[pos] == '|') {
                    // Find matching \right|
                    ++pos;
                    const auto rpos = s.find("\\right|", pos);
                    if (rpos != std::string::npos) {
                        std::string innerCas;
                        if (!ConvertLatexToCasExpr(s.substr(pos, rpos - pos), innerCas, outErr)) return false;
                        out += "abs(" + innerCas + ")";
                        pos = rpos + 7;
                        continue;
                    }
                    out.push_back('(');
                    continue;
                }
                if (pos < s.size() && (s[pos] == '(' || s[pos] == '[')) {
                    out.push_back('(');
                    ++pos;
                }
                continue;
            }
            if (cmd == "right") {
                skipWs();
                if (pos < s.size() && (s[pos] == ')' || s[pos] == ']' || s[pos] == '|')) {
                    out.push_back(')');
                    ++pos;
                }
                continue;
            }
            if (cmd == "begin") {
                std::string env;
                if (!readBrace(env)) return false;
                const std::string endTag = "\\end{" + env + "}";
                const auto endPos = s.find(endTag, pos);
                if (endPos == std::string::npos) {
                    if (outErr) *outErr = "Falta " + endTag;
                    return false;
                }
                const std::string body = s.substr(pos, endPos - pos);
                pos = endPos + endTag.size();
                // Split rows by "\\" and cols by "&"
                out += "[";
                std::size_t rp = 0;
                bool firstRow = true;
                while (rp <= body.size()) {
                    const auto nextRow = body.find("\\\\", rp);
                    const std::string rowStr = (nextRow == std::string::npos) ? body.substr(rp) : body.substr(rp, nextRow - rp);
                    if (!detail_ltx::Trim(rowStr).empty()) {
                        if (!firstRow) out += ",";
                        firstRow = false;
                        out += "[";
                        std::size_t cp = 0;
                        bool firstCol = true;
                        while (cp <= rowStr.size()) {
                            const auto amp = rowStr.find('&', cp);
                            const std::string cell = (amp == std::string::npos) ? rowStr.substr(cp) : rowStr.substr(cp, amp - cp);
                            std::string cCas;
                            if (!ConvertLatexToCasExpr(cell, cCas, outErr)) return false;
                            if (!firstCol) out += ",";
                            firstCol = false;
                            out += cCas;
                            if (amp == std::string::npos) break;
                            cp = amp + 1;
                        }
                        out += "]";
                    }
                    if (nextRow == std::string::npos) break;
                    rp = nextRow + 2;
                }
                out += "]";
                continue;
            }
            if (cmd == "cdot" || cmd == "times") { out += " * "; continue; }
            if (cmd == "div") { out += " / "; continue; }
            if (cmd == "mid") { out += " | "; continue; }
            if (cmd == "angle") { out += " @ "; continue; }
            if (cmd == "pi") { maybeSpace(); out += "pi"; continue; }
            if (cmd == "Omega") { out += "Ohm"; continue; }
            if (cmd == "mu") { out += "u"; continue; }
            if (cmd == "log") { maybeSpace(); out += "log10"; continue; }
            if (detail_ltx::IsRomanFunctionCmd(cmd)) {
                maybeSpace();
                out += cmd;
                continue;
            }
            const auto& symMap = detail_ltx::SymbolMap();
            auto sit = symMap.find(cmd);
            if (sit != symMap.end()) {
                maybeSpace();
                out += cmd;
                continue;
            }
            maybeSpace();
            out += cmd;
            continue;
        }
        if (ch == '{') {
            std::string inner, cInner;
            if (!readBrace(inner) || !ConvertLatexToCasExpr(inner, cInner, outErr)) return false;
            out += "(" + cInner + ")";
            continue;
        }
        if (ch == '^') {
            ++pos;
            skipWs();
            if (pos < s.size() && s[pos] == '{') {
                std::string inner, cInner;
                if (!readBrace(inner) || !ConvertLatexToCasExpr(inner, cInner, outErr)) return false;
                out += "^(" + cInner + ")";
            } else {
                out.push_back('^');
            }
            continue;
        }
        if (ch == '_') {
            ++pos;
            skipWs();
            if (pos < s.size() && s[pos] == '{') {
                std::string inner;
                if (!readBrace(inner)) return false;
                out += "_" + detail_ltx::Trim(inner);
            } else {
                out.push_back('_');
            }
            continue;
        }
        if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '=' || ch == '|' || ch == '@') {
            out += std::string(" ") + ch + " ";
            ++pos;
            continue;
        }
        out.push_back(ch);
        ++pos;
    }

    outCasExprUtf8 = detail_ltx::Trim(out);
    return !outCasExprUtf8.empty();
}

namespace detail_ltx {

inline std::string FormatScientificNotationLatex(const std::string& src) {
    std::string out;
    out.reserve(src.size() + 16);
    std::size_t i = 0;
    while (i < src.size()) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        const bool prevOk = (i == 0) ||
            (!std::isalnum(static_cast<unsigned char>(src[i - 1])) && src[i - 1] != '_' && src[i - 1] != '.');
        if (prevOk && (std::isdigit(c) || (c == '.' && i + 1 < src.size() && std::isdigit(static_cast<unsigned char>(src[i + 1]))))) {
            std::size_t j = i;
            while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
            if (j < src.size() && src[j] == '.') {
                ++j;
                while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
            }
            if (j < src.size() && (src[j] == 'e' || src[j] == 'E')) {
                const std::size_t ePos = j++;
                char sign = 0;
                if (j < src.size() && (src[j] == '+' || src[j] == '-')) {
                    sign = src[j++];
                }
                const std::size_t dSt = j;
                while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
                const bool nextOk = (j >= src.size()) || (!std::isalnum(static_cast<unsigned char>(src[j])) && src[j] != '_');
                if (j > dSt && nextOk) {
                    const std::string mant = src.substr(i, ePos - i);
                    std::size_t nz = dSt;
                    while (nz + 1 < j && src[nz] == '0') ++nz;
                    const std::string expDigits = src.substr(nz, j - nz);
                    out += mant + " \\times 10^{" + (sign == '-' ? std::string("-") : std::string()) + expDigits + "}";
                    i = j;
                    continue;
                }
            }
        }
        out.push_back(src[i++]);
    }
    return out;
}

inline bool IsKnownUnitTokenLatex(const std::string& rawTok) {
    if (rawTok.empty()) return false;
    std::string tok = rawTok;
    auto repl = [&](const std::string& from, const std::string& to) {
        std::size_t p = 0;
        while ((p = tok.find(from, p)) != std::string::npos) {
            tok.replace(p, from.size(), to);
            p += to.size();
        }
    };
    repl("\xCE\xA9", "Ohm");
    repl("\xE2\x84\xA6", "Ohm");
    repl("\xC2\xB5", "u");
    repl("\xCE\xBC", "u");

    static const char* kBaseUnits[] = {
        "rad", "deg", "gra", "m", "in", "ft", "yd", "mi", "Angstrom", "ha", "acre",
        "g", "lb", "oz", "ton", "slug", "s", "min", "h", "hr", "Hz", "rpm", "mph",
        "N", "dyn", "lbf", "kip", "Nm", "J", "erg", "cal", "BTU", "eV", "Wh", "W", "hp",
        "L", "l", "gal", "Pa", "bar", "psi", "ksi", "psf", "ksf", "atm", "torr",
        "A", "Ah", "V", "Ohm", "ohm", "S", "F", "H", "C", "Wb", "T", "K",
        "degR", "degC", "degF", "mol", "cd", "lm", "lx"
    };
    auto isBase = [&](const std::string& s) {
        for (const char* u : kBaseUnits) {
            if (s == u) return true;
        }
        return false;
    };
    if (isBase(tok)) return true;
    if (tok.size() >= 2) {
        const char p = tok[0];
        if (p == 'p' || p == 'n' || p == 'u' || p == 'm' || p == 'c' || p == 'k' || p == 'M' || p == 'G' || p == 'T') {
            const std::string base = tok.substr(1);
            if (base != "degC" && base != "degF" && base != "degR" &&
                base != "min" && base != "hr" && base != "in" && base != "ft" && base != "mph" &&
                isBase(base)) {
                return true;
            }
        }
    }
    return false;
}

inline std::string FormatUnitLatexText(const std::string& rawUnit) {
    std::string u = rawUnit;
    auto repl = [&](const std::string& from, const std::string& to) {
        std::size_t p = 0;
        while ((p = u.find(from, p)) != std::string::npos) {
            u.replace(p, from.size(), to);
            p += to.size();
        }
    };
    repl("\xCE\xA9", "\\Omega");
    repl("\xE2\x84\xA6", "\\Omega");
    repl("Ohm", "\\Omega");
    repl("ohm", "\\Omega");
    return "\\text{" + u + "}";
}

inline bool TrySplitNumberAndUnit(
    const std::string& s,
    std::string& outNum,
    std::string& outUnitBase,
    std::string& outUnitExp)
{
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    const std::size_t dStart = i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    if (i == dStart) return false;
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        const std::size_t sv = i++;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        const std::size_t ed = i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i == ed) i = sv;
    }
    if (i >= s.size() || !std::isspace(static_cast<unsigned char>(s[i]))) return false;
    outNum = s.substr(0, i);
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i >= s.size()) return false;
    const std::string rest = Trim(s.substr(i));
    const auto caret = rest.find('^');
    if (caret != std::string::npos) {
        outUnitBase = rest.substr(0, caret);
        outUnitExp = rest.substr(caret + 1);
        if (outUnitExp.empty()) return false;
        for (std::size_t k = 0; k < outUnitExp.size(); ++k) {
            if (k == 0 && outUnitExp[k] == '-') continue;
            if (!std::isdigit(static_cast<unsigned char>(outUnitExp[k]))) return false;
        }
    } else {
        outUnitBase = rest;
        outUnitExp.clear();
    }
    return IsKnownUnitTokenLatex(outUnitBase);
}

} // namespace detail_ltx

inline std::string ConvertCasExprToLatex(const std::string& casExprUtf8) {
    std::string s = detail_ltx::Trim(casExprUtf8);
    if (s.empty()) return "";

    // Matrix literal [[a,b],[c,d]]
    if (s.size() >= 4 && s.front() == '[' && s.back() == ']' && s.find("[[") != std::string::npos) {
        std::ostringstream oss;
        oss << "\\begin{bmatrix} ";
        std::size_t i = 1;
        bool firstRow = true;
        while (i + 1 < s.size()) {
            while (i + 1 < s.size() && (s[i] == ',' || s[i] == ';' || std::isspace(static_cast<unsigned char>(s[i])))) ++i;
            if (i + 1 >= s.size() || s[i] != '[') break;
            ++i;
            if (!firstRow) oss << " \\\\ ";
            firstRow = false;
            bool firstCol = true;
            while (i < s.size() && s[i] != ']') {
                const std::size_t st = i;
                while (i < s.size() && s[i] != ',' && s[i] != ']') ++i;
                const std::string cell = detail_ltx::Trim(s.substr(st, i - st));
                if (!firstCol) oss << " & ";
                firstCol = false;
                oss << ConvertCasExprToLatex(cell);
                if (i < s.size() && s[i] == ',') ++i;
            }
            if (i < s.size() && s[i] == ']') ++i;
        }
        oss << " \\end{bmatrix}";
        return oss.str();
    }

    // Handle "expr | unit"
    const auto pipePos = s.find('|');
    if (pipePos != std::string::npos && (pipePos + 1 >= s.size() || s[pipePos + 1] != '|')) {
        const std::string lhs = ConvertCasExprToLatex(s.substr(0, pipePos));
        std::string rhs = detail_ltx::Trim(s.substr(pipePos + 1));
        std::size_t p = 0;
        while ((p = rhs.find("Ohm", p)) != std::string::npos) { rhs.replace(p, 3, "\\Omega "); p += 7; }
        p = 0;
        while ((p = rhs.find("\xCE\xA9", p)) != std::string::npos) { rhs.replace(p, 2, "\\Omega "); p += 7; }
        return lhs + " \\mid \\mathrm{" + rhs + "}";
    }

    // Handle assignment "lhs = rhs"
    const auto eqPos = s.find('=');
    if (eqPos != std::string::npos) {
        return ConvertCasExprToLatex(s.substr(0, eqPos)) + " = " + ConvertCasExprToLatex(s.substr(eqPos + 1));
    }

    // Recursive simple infix -> LaTeX converter with balanced parens & \frac for top-level '/'
    auto stripOuterParens = [](std::string t) {
        t = detail_ltx::Trim(t);
        while (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
            int d = 0;
            bool match = true;
            for (std::size_t k = 0; k + 1 < t.size(); ++k) {
                if (t[k] == '(') ++d;
                else if (t[k] == ')') --d;
                if (d == 0) { match = false; break; }
            }
            if (!match) break;
            t = detail_ltx::Trim(t.substr(1, t.size() - 2));
        }
        return t;
    };
    auto hasTopLevelAddSubOrUnaryNeg = [&](const std::string& raw, bool includeLeadingUnaryMinus) {
        const std::string u = stripOuterParens(raw);
        if (u.empty()) return false;
        if (includeLeadingUnaryMinus && u[0] == '-') return true;
        int d = 0;
        for (std::size_t k = 0; k < u.size(); ++k) {
            if (u[k] == '(') ++d;
            else if (u[k] == ')') --d;
            else if (d == 0 && k > 0 && (u[k] == '+' || u[k] == '-')) {
                if ((u[k - 1] == 'e' || u[k - 1] == 'E') && k >= 2 && std::isdigit(static_cast<unsigned char>(u[k - 2]))) {
                    continue;
                }
                std::size_t prev = k - 1;
                while (prev > 0 && std::isspace(static_cast<unsigned char>(u[prev]))) --prev;
                if (u[prev] == '*' || u[prev] == '/' || u[prev] == '^' || u[prev] == '+' || u[prev] == '-' || u[prev] == '(') {
                    continue;
                }
                return true;
            }
        }
        return false;
    };
    auto needsBaseParens = [&](const std::string& raw) {
        const std::string u = stripOuterParens(raw);
        if (u.empty()) return false;
        if (u[0] == '-' || u[0] == '+') return true;
        int d = 0;
        for (std::size_t k = 0; k < u.size(); ++k) {
            if (u[k] == '(') ++d;
            else if (u[k] == ')') --d;
            else if (d == 0) {
                if (u[k] == '+' || u[k] == '-') {
                    if (k > 0 && (u[k - 1] == 'e' || u[k - 1] == 'E') && k >= 2 && std::isdigit(static_cast<unsigned char>(u[k - 2]))) {
                        continue;
                    }
                    return true;
                }
                if (u[k] == '*' || u[k] == '/' || u[k] == '^' || u[k] == ' ' || u[k] == '|' || u[k] == '@') {
                    return true;
                }
            }
        }
        return false;
    };

    s = stripOuterParens(s);

    // Split on top-level '+' / '-'
    int depth = 0;
    for (std::size_t i = s.size(); i-- > 0;) {
        if (s[i] == ')') ++depth;
        else if (s[i] == '(') --depth;
        else if (depth == 0 && i > 0 && (s[i] == '+' || s[i] == '-')) {
            // Skip scientific notation e+ / e-
            if ((s[i - 1] == 'e' || s[i - 1] == 'E') && i >= 2 && std::isdigit(static_cast<unsigned char>(s[i - 2]))) {
                continue;
            }
            // Skip unary after operator
            std::size_t prev = i - 1;
            while (prev > 0 && std::isspace(static_cast<unsigned char>(s[prev]))) --prev;
            if (s[prev] == '*' || s[prev] == '/' || s[prev] == '^' || s[prev] == '+' || s[prev] == '-' || s[prev] == '(') {
                continue;
            }
            const std::string l = detail_ltx::Trim(s.substr(0, i));
            const std::string r = detail_ltx::Trim(s.substr(i + 1));
            if (!l.empty() && !r.empty()) {
                std::string lr = ConvertCasExprToLatex(r);
                const std::string ur = stripOuterParens(r);
                if (s[i] == '-' && hasTopLevelAddSubOrUnaryNeg(r, true)) {
                    lr = "\\left(" + lr + "\\right)";
                } else if (s[i] == '+' && !r.empty() && r[0] == '(' && !ur.empty() && ur[0] == '-') {
                    lr = "\\left(" + lr + "\\right)";
                }
                return ConvertCasExprToLatex(l) + (s[i] == '+' ? " + " : " - ") + lr;
            }
        }
    }

    // Split on top-level '*' / '/' right-to-left (equal precedence, left-associative)
    depth = 0;
    for (std::size_t i = s.size(); i-- > 0;) {
        if (s[i] == ')') ++depth;
        else if (s[i] == '(') --depth;
        else if (depth == 0 && (s[i] == '/' || s[i] == '*')) {
            const std::string l = detail_ltx::Trim(s.substr(0, i));
            const std::string r = detail_ltx::Trim(s.substr(i + 1));
            if (!l.empty() && !r.empty()) {
                if (s[i] == '/') {
                    return "\\frac{" + ConvertCasExprToLatex(l) + "}{" + ConvertCasExprToLatex(r) + "}";
                }
                std::string ll = ConvertCasExprToLatex(l);
                std::string lr = ConvertCasExprToLatex(r);
                if (hasTopLevelAddSubOrUnaryNeg(l, false)) ll = "\\left(" + ll + "\\right)";
                if (hasTopLevelAddSubOrUnaryNeg(r, true)) lr = "\\left(" + lr + "\\right)";
                return ll + " \\cdot " + lr;
            }
        }
    }

    // Handle "<number> <unit>^<exp>" (e.g. "100 cm^2") without wrapping in \left(...\right)
    {
        std::string numP, uBase, uExp;
        if (detail_ltx::TrySplitNumberAndUnit(s, numP, uBase, uExp)) {
            std::string out = detail_ltx::FormatScientificNotationLatex(numP) + "\\," + detail_ltx::FormatUnitLatexText(uBase);
            if (!uExp.empty()) out += "^{" + uExp + "}";
            return out;
        }
    }

    // Handle leading unary minus (lower precedence than '^', e.g. -x^2 -> -x^{2}, -(a+b) -> -\left(a+b\right), -(a+b)^2 -> -\left(a+b\right)^{2})
    if (s.size() > 1 && s[0] == '-') {
        if (s.size() > 3 && s[1] == '(' && s.back() == ')') {
            int d = 0;
            bool match = true;
            for (std::size_t k = 1; k + 1 < s.size(); ++k) {
                if (s[k] == '(') ++d;
                else if (s[k] == ')') --d;
                if (d == 0) { match = false; break; }
            }
            if (match) {
                return "-\\left(" + ConvertCasExprToLatex(s.substr(2, s.size() - 3)) + "\\right)";
            }
        }
        return "-" + ConvertCasExprToLatex(s.substr(1));
    }

    // Split on top-level '^'
    depth = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') --depth;
        else if (depth == 0 && s[i] == '^') {
            const std::string l = detail_ltx::Trim(s.substr(0, i));
            const std::string r = detail_ltx::Trim(s.substr(i + 1));
            if (!l.empty() && !r.empty()) {
                std::string bl = ConvertCasExprToLatex(l);
                if (needsBaseParens(l)) {
                    bl = "\\left(" + bl + "\\right)";
                }
                return bl + "^{" + ConvertCasExprToLatex(r) + "}";
            }
        }
    }

    // Function call f(args...)
    const auto lp = s.find('(');
    if (lp != std::string::npos && lp > 0 && s.back() == ')') {
        const std::string fn = detail_ltx::Trim(s.substr(0, lp));
        bool isIdent = true;
        for (char c : fn) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') { isIdent = false; break; }
        }
        if (isIdent) {
            const std::string arg = s.substr(lp + 1, s.size() - lp - 2);
            std::string lArg;
            {
                int ad = 0;
                std::size_t st = 0;
                bool firstArg = true;
                for (std::size_t k = 0; k <= arg.size(); ++k) {
                    if (k == arg.size() || (ad == 0 && arg[k] == ',')) {
                        const std::string part = detail_ltx::Trim(arg.substr(st, k - st));
                        if (!firstArg) lArg += ", ";
                        firstArg = false;
                        lArg += ConvertCasExprToLatex(part);
                        st = k + 1;
                    } else if (arg[k] == '(') ++ad;
                    else if (arg[k] == ')') --ad;
                }
            }
            if (fn == "sqrt") return "\\sqrt{" + lArg + "}";
            if (fn == "abs") return "\\left|" + lArg + "\\right|";
            if (fn == "exp") return "e^{" + lArg + "}";
            if (fn == "log10") return "\\log_{10}\\left(" + lArg + "\\right)";
            if (detail_ltx::IsRomanFunctionCmd(fn)) {
                return "\\" + fn + "\\left(" + lArg + "\\right)";
            }
            const auto fnUs = fn.find('_');
            if (fnUs != std::string::npos && fnUs > 0 && fnUs + 1 < fn.size()) {
                const std::string baseFn = fn.substr(0, fnUs);
                const std::string subFn = fn.substr(fnUs + 1);
                if (detail_ltx::IsRomanFunctionCmd(baseFn)) {
                    return "\\" + baseFn + "_{" + subFn + "}\\left(" + lArg + "\\right)";
                }
                if (baseFn.size() == 1) {
                    return baseFn + "_{" + subFn + "}\\left(" + lArg + "\\right)";
                }
                return "\\operatorname{" + baseFn + "}_{" + subFn + "}\\left(" + lArg + "\\right)";
            }
            if (fn.size() == 1) {
                return fn + "\\left(" + lArg + "\\right)";
            }
            return "\\operatorname{" + fn + "}\\left(" + lArg + "\\right)";
        }
    }

    // Token replacements for scientific notation, symbols/units/UTF-8, subscripts
    std::string out = detail_ltx::FormatScientificNotationLatex(s);
    auto replAll = [&](const std::string& from, const std::string& to) {
        std::size_t p = 0;
        while ((p = out.find(from, p)) != std::string::npos) {
            out.replace(p, from.size(), to);
            p += to.size();
        }
    };
    replAll("\xE2\x88\xA0", "\\angle ");
    replAll("\xC2\xB0", "^{\\circ}");
    replAll("\xCE\xA9", "\\Omega ");
    replAll("Ohm", "\\Omega ");
    if (out == "pi") return "\\pi";
    const auto us = out.find('_');
    if (us != std::string::npos && us > 0 && us + 1 < out.size()) {
        std::size_t subEnd = us + 1;
        while (subEnd < out.size() && std::isalnum(static_cast<unsigned char>(out[subEnd]))) {
            ++subEnd;
        }
        if (subEnd > us + 1) {
            const std::string tail = out.substr(subEnd);
            return out.substr(0, us) + "_{" + out.substr(us + 1, subEnd - (us + 1)) + "}" +
                   (tail.empty() ? std::string() : ConvertCasExprToLatex(tail));
        }
    }
    return out;
}

inline std::string FormatVectorLatex(
    const std::vector<double>& comps,
    const std::string& unitUtf8 = "",
    bool columnMatrix = true)
{
    auto fmtNum = [](double v) {
        if (std::abs(v) < 1e-12) return std::string("0");
        std::ostringstream oss;
        oss.precision(6);
        oss << v;
        return oss.str();
    };
    std::ostringstream oss;
    if (columnMatrix) {
        oss << "\\begin{bmatrix} ";
        for (std::size_t i = 0; i < comps.size(); ++i) {
            if (i > 0) oss << " \\\\ ";
            oss << fmtNum(comps[i]);
        }
        oss << " \\end{bmatrix}";
    } else {
        oss << "\\left\\langle ";
        for (std::size_t i = 0; i < comps.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << fmtNum(comps[i]);
        }
        oss << " \\right\\rangle";
    }
    if (!unitUtf8.empty()) oss << "\\,\\mathrm{" << unitUtf8 << "}";
    return oss.str();
}

inline std::string FormatVectorMathML(
    const std::vector<double>& comps,
    const std::string& unitUtf8 = "",
    bool columnMatrix = true)
{
    return LatexToMathML(FormatVectorLatex(comps, unitUtf8, columnMatrix), false);
}

inline std::string FormatVarSubLatex2D(
    const std::string& varNameUtf8,
    const std::string& symRhsUtf8,
    const std::string& varSubUtf8,
    const std::string& resultUtf8,
    int mathSubModeInt /* 0=VarSub, 1=NoSub, 2=NoVar */)
{
    const std::string lVar = varNameUtf8.empty() ? std::string() : ConvertCasExprToLatex(varNameUtf8);
    const std::string lSym = ConvertCasExprToLatex(symRhsUtf8);
    const std::string lSub = ConvertCasExprToLatex(varSubUtf8);
    const std::string lRes = ConvertCasExprToLatex(resultUtf8);

    std::string body;
    if (mathSubModeInt == 1) { // NoSub
        body = lVar.empty() ? (lSym + " = " + lRes) : (lVar + " = " + lSym + " = " + lRes);
    } else if (mathSubModeInt == 2) { // NoVar
        body = lRes;
    } else { // VarSub
        if (!lVar.empty() && symRhsUtf8 != varSubUtf8 && !lSub.empty()) {
            body = lVar + " = " + lSym + " = " + lSub + " = " + lRes;
        } else if (!lVar.empty()) {
            body = lVar + " = " + lRes;
        } else if (!lSub.empty()) {
            body = lSub + " = " + lRes;
        } else {
            body = lRes;
        }
    }
    return "$" + body + "$";
}

} // namespace DocStudio::Cas
