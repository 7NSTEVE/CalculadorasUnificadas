#include "latex_cairo.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace CasioLatexCairo {

namespace {

enum class FontRole {
    TextNormal,
    TextBold,
    MathItalic,
    MathRoman,
    MathBold,
    MathSymbol
};

struct BoxMetrics {
    double width = 0.0;
    double ascent = 0.0;
    double descent = 0.0;
    double height() const noexcept { return ascent + descent; }
};

struct MathNode;
using MathNodePtr = std::shared_ptr<MathNode>;

enum class NodeKind {
    Row,
    TextRun,
    Fraction,
    Sqrt,
    SupSub,
    BigOp,
    Accent,
    Delimiter,
    Matrix,
    Space
};

struct MathNode {
    NodeKind kind = NodeKind::Row;
    std::string text;
    FontRole role = FontRole::MathRoman;
    bool isOperator = false;
    bool hasBar = true; // Fraction vs \binom
    std::string leftDelim = "(";
    std::string rightDelim = ")";
    std::string accentKind; // vec, bar, hat, tilde, dot, ddot, overline
    std::string bigOpSym;
    std::string envName;
    bool compactFraction = false;
    double spaceEm = 0.0;

    std::vector<MathNodePtr> children;
    MathNodePtr base;
    MathNodePtr sup;
    MathNodePtr sub;
    MathNodePtr num;
    MathNodePtr den;
    MathNodePtr radicand;
    MathNodePtr rootIdx;
    std::vector<std::vector<MathNodePtr>> grid;

    BoxMetrics m{};
    double childScale = 1.0;
    double axisYOffset = 0.0; // relative to baseline (upwards negative in screen coords)
};

const std::unordered_map<std::string, std::string>& LatexSymbolMap() {
    static const std::unordered_map<std::string, std::string> kMap = {
        {"alpha", "\xCE\xB1"}, {"beta", "\xCE\xB2"}, {"gamma", "\xCE\xB3"}, {"delta", "\xCE\xB4"},
        {"epsilon", "\xCE\xB5"}, {"varepsilon", "\xCE\xB5"}, {"zeta", "\xCE\xB6"}, {"eta", "\xCE\xB7"},
        {"theta", "\xCE\xB8"}, {"vartheta", "\xCF\x91"}, {"iota", "\xCE\xB9"}, {"kappa", "\xCE\xBA"},
        {"lambda", "\xCE\xBB"}, {"mu", "\xCE\xBC"}, {"nu", "\xCE\xBD"}, {"xi", "\xCE\xBE"},
        {"pi", "\xCF\x80"}, {"varpi", "\xCF\x96"}, {"rho", "\xCF\x81"}, {"sigma", "\xCF\x83"},
        {"varsigma", "\xCF\x82"}, {"tau", "\xCF\x84"}, {"upsilon", "\xCF\x85"}, {"phi", "\xCF\x86"},
        {"varphi", "\xCF\x95"}, {"chi", "\xCF\x87"}, {"psi", "\xCF\x88"}, {"omega", "\xCF\x89"},
        {"Gamma", "\xCE\x93"}, {"Delta", "\xCE\x94"}, {"Theta", "\xCE\x98"}, {"Lambda", "\xCE\x9B"},
        {"Xi", "\xCE\x9E"}, {"Pi", "\xCE\xA0"}, {"Sigma", "\xCE\xA3"}, {"Upsilon", "\xCE\xA5"},
        {"Phi", "\xCE\xA6"}, {"Psi", "\xCE\xA8"}, {"Omega", "\xCE\xA9"},
        {"times", "\xC3\x97"}, {"div", "\xC3\xB7"}, {"pm", "\xC2\xB1"}, {"mp", "\xE2\x88\x93"},
        {"cdot", "\xC2\xB7"}, {"ast", "*"}, {"star", "\xE2\x8B\x86"},
        {"leq", "\xE2\x89\xA4"}, {"le", "\xE2\x89\xA4"}, {"geq", "\xE2\x89\xA5"}, {"ge", "\xE2\x89\xA5"},
        {"neq", "\xE2\x89\xA0"}, {"ne", "\xE2\x89\xA0"}, {"approx", "\xE2\x89\x88"},
        {"equiv", "\xE2\x89\xA1"}, {"sim", "\xE2\x88\xBC"}, {"propto", "\xE2\x88\x9D"},
        {"infty", "\xE2\x88\x9E"}, {"partial", "\xE2\x88\x82"}, {"nabla", "\xE2\x88\x87"},
        {"forall", "\xE2\x88\x80"}, {"exists", "\xE2\x88\x83"}, {"nexists", "\xE2\x88\x84"},
        {"in", "\xE2\x88\x88"}, {"notin", "\xE2\x88\x89"}, {"ni", "\xE2\x88\x8B"},
        {"subset", "\xE2\x8A\x82"}, {"subseteq", "\xE2\x8A\x86"}, {"supset", "\xE2\x8A\x83"},
        {"supseteq", "\xE2\x8A\x87"}, {"cup", "\xE2\x88\xAA"}, {"cap", "\xE2\x88\xA9"},
        {"emptyset", "\xE2\x88\x85"}, {"varnothing", "\xE2\x88\x85"},
        {"to", "\xE2\x86\x92"}, {"rightarrow", "\xE2\x86\x92"}, {"leftarrow", "\xE2\x86\x90"},
        {"leftrightarrow", "\xE2\x86\x94"}, {"Rightarrow", "\xE2\x87\x92"},
        {"Leftarrow", "\xE2\x87\x90"}, {"Leftrightarrow", "\xE2\x87\x94"}, {"mapsto", "\xE2\x86\xA6"},
        {"therefore", "\xE2\x88\xB4"}, {"because", "\xE2\x88\xB5"}, {"angle", "\xE2\x88\xA0"},
        {"perp", "\xE2\x8A\xA5"}, {"parallel", "\xE2\x88\xA5"}, {"triangle", "\xE2\x96\xB3"},
        {"circ", "\xE2\x88\x98"}, {"deg", "\xC2\xB0"}, {"degree", "\xC2\xB0"},
        {"textdegree", "\xC2\xB0"}, {"bullet", "\xE2\x80\xA2"}, {"dots", "\xE2\x80\xA6"},
        {"ldots", "\xE2\x80\xA6"}, {"cdots", "\xE2\x8B\xAF"}, {"vdots", "\xE2\x8B\xAE"},
        {"prime", "\xE2\x80\xB2"}, {"Re", "\xE2\x84\x9C"}, {"Im", "\xE2\x84\x91"},
        {"aleph", "\xE2\x84\xB5"}, {"hbar", "\xE2\x84\x8F"}, {"ell", "\xE2\x84\x93"},
        {"neg", "\xC2\xAC"}, {"lnot", "\xC2\xAC"}, {"land", "\xE2\x88\xA7"},
        {"wedge", "\xE2\x88\xA7"}, {"lor", "\xE2\x88\xA8"}, {"vee", "\xE2\x88\xA8"},
        {"oplus", "\xE2\x8A\x95"}, {"otimes", "\xE2\x8A\x97"},
        {"langle", "\xE2\x9F\xA8"}, {"rangle", "\xE2\x9F\xA9"},
        {"lceil", "\xE2\x8C\x88"}, {"rceil", "\xE2\x8C\x89"},
        {"lfloor", "\xE2\x8C\x8A"}, {"rfloor", "\xE2\x8C\x8B"},
        {"|", "\xE2\x80\x96"}, {"%", "%"}, {"$", "$"}, {"#", "#"},
        {"&", "&"}, {"_", "_"}, {"{", "{"}, {"}", "}"}
    };
    return kMap;
}

bool IsNamedRomanFunction(const std::string& cmd) {
    static const char* const kFuncs[] = {
        "sin", "cos", "tan", "csc", "sec", "cot",
        "arcsin", "arccos", "arctan", "sinh", "cosh", "tanh",
        "ln", "log", "lg", "exp", "det", "gcd", "lcm",
        "min", "max", "sup", "inf", "arg", "deg", "dim",
        "ker", "mod", "Pr"
    };
    for (const char* f : kFuncs) {
        if (cmd == f) return true;
    }
    return false;
}

bool IsBigOpCommand(const std::string& cmd, std::string& outSym) {
    if (cmd == "int") { outSym = "\xE2\x88\xAB"; return true; }
    if (cmd == "iint") { outSym = "\xE2\x88\xAC"; return true; }
    if (cmd == "iiint") { outSym = "\xE2\x88\xAD"; return true; }
    if (cmd == "oint") { outSym = "\xE2\x88\xAE"; return true; }
    if (cmd == "sum") { outSym = "\xE2\x88\x91"; return true; }
    if (cmd == "prod") { outSym = "\xE2\x88\x8F"; return true; }
    if (cmd == "bigcup") { outSym = "\xE2\x8B\x83"; return true; }
    if (cmd == "bigcap") { outSym = "\xE2\x8B\x82"; return true; }
    if (cmd == "lim") { outSym = "lim"; return true; }
    return false;
}

std::string MapBlackboardBold(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        switch (c) {
        case 'R': out += "\xE2\x84\x9D"; break;
        case 'N': out += "\xE2\x84\x95"; break;
        case 'Z': out += "\xE2\x84\xA4"; break;
        case 'Q': out += "\xE2\x84\x9A"; break;
        case 'C': out += "\xE2\x84\x82"; break;
        default: out.push_back(static_cast<char>(c)); break;
        }
    }
    return out;
}

class LatexParser {
public:
    explicit LatexParser(const std::string& src) : src_(src) {}

    MathNodePtr ParseRow(const char* stopAt = nullptr) {
        auto row = std::make_shared<MathNode>();
        row->kind = NodeKind::Row;
        while (pos_ < src_.size()) {
            if (stopAt && MatchesAhead(stopAt)) break;
            char ch = src_[pos_];
            if (ch == '}' || ch == ']') break;
            if (ch == '&') break;
            if (ch == '\\' && MatchesAhead("\\\\")) break;
            if (ch == '\\' && MatchesAhead("\\end{")) break;
            if (ch == '\\' && MatchesAhead("\\right")) break;

            MathNodePtr atom = ParseAtomWithScripts();
            if (atom) {
                row->children.push_back(atom);
            } else {
                // Safety advance to guarantee termination on malformed characters
                if (pos_ < src_.size()) pos_++;
            }
        }
        return row;
    }

private:
    const std::string& src_;
    size_t pos_ = 0;

    bool MatchesAhead(const char* lit) const {
        size_t len = std::strlen(lit);
        return pos_ + len <= src_.size() && src_.compare(pos_, len, lit) == 0;
    }

    void SkipWhitespace() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }

    std::string ReadBracedGroupRaw() {
        SkipWhitespace();
        if (pos_ >= src_.size()) return {};
        if (src_[pos_] != '{') {
            std::string single(1, src_[pos_++]);
            return single;
        }
        ++pos_;
        int depth = 1;
        size_t start = pos_;
        while (pos_ < src_.size() && depth > 0) {
            if (src_[pos_] == '\\' && pos_ + 1 < src_.size()) {
                pos_ += 2;
                continue;
            }
            if (src_[pos_] == '{') depth++;
            else if (src_[pos_] == '}') {
                depth--;
                if (depth == 0) {
                    std::string out = src_.substr(start, pos_ - start);
                    ++pos_;
                    return out;
                }
            }
            ++pos_;
        }
        return src_.substr(start, pos_ - start);
    }

    MathNodePtr ParseBracedArg() {
        SkipWhitespace();
        if (pos_ >= src_.size()) return std::make_shared<MathNode>();
        if (src_[pos_] == '{') {
            ++pos_;
            auto node = ParseRow();
            if (pos_ < src_.size() && src_[pos_] == '}') ++pos_;
            return node;
        }
        return ParseSingleAtom(false);
    }

    MathNodePtr ParseOptionalBracketArg() {
        SkipWhitespace();
        if (pos_ < src_.size() && src_[pos_] == '[') {
            ++pos_;
            auto node = ParseRow();
            if (pos_ < src_.size() && src_[pos_] == ']') ++pos_;
            return node;
        }
        return nullptr;
    }

    std::string ReadCommandName() {
        if (pos_ >= src_.size() || src_[pos_] != '\\') return {};
        ++pos_;
        if (pos_ >= src_.size()) return {};
        if (!std::isalpha(static_cast<unsigned char>(src_[pos_]))) {
            return std::string(1, src_[pos_++]);
        }
        size_t start = pos_;
        while (pos_ < src_.size() && std::isalpha(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
        return src_.substr(start, pos_ - start);
    }

    std::string ReadDelimToken() {
        SkipWhitespace();
        if (pos_ >= src_.size()) return ".";
        if (src_[pos_] == '\\') {
            std::string cmd = ReadCommandName();
            if (cmd == "{" || cmd == "}") return cmd;
            if (cmd == "|") return "\xE2\x80\x96";
            auto it = LatexSymbolMap().find(cmd);
            if (it != LatexSymbolMap().end()) return it->second;
            return "(";
        }
        char c = src_[pos_++];
        if (c == '.') return "";
        return std::string(1, c);
    }

    MathNodePtr ParseSingleAtom(bool allowScripts) {
        (void)allowScripts;
        SkipWhitespace();
        if (pos_ >= src_.size()) return nullptr;
        char c = src_[pos_];

        if (c == '{') {
            ++pos_;
            auto row = ParseRow();
            if (pos_ < src_.size() && src_[pos_] == '}') ++pos_;
            return row;
        }

        if (c == '\\') {
            std::string cmd = ReadCommandName();
            if (cmd.empty()) return nullptr;
            if (cmd == "," || cmd == ":") {
                auto sp = std::make_shared<MathNode>();
                sp->kind = NodeKind::Space;
                sp->spaceEm = 0.22;
                return sp;
            }
            if (cmd == ";" || cmd == " ") {
                auto sp = std::make_shared<MathNode>();
                sp->kind = NodeKind::Space;
                sp->spaceEm = 0.35;
                return sp;
            }
            if (cmd == "!") {
                auto sp = std::make_shared<MathNode>();
                sp->kind = NodeKind::Space;
                sp->spaceEm = -0.10;
                return sp;
            }
            if (cmd == "quad") {
                auto sp = std::make_shared<MathNode>();
                sp->kind = NodeKind::Space;
                sp->spaceEm = 0.9;
                return sp;
            }
            if (cmd == "qquad") {
                auto sp = std::make_shared<MathNode>();
                sp->kind = NodeKind::Space;
                sp->spaceEm = 1.6;
                return sp;
            }
            if (cmd == "frac" || cmd == "dfrac" || cmd == "tfrac" || cmd == "binom") {
                auto f = std::make_shared<MathNode>();
                f->kind = NodeKind::Fraction;
                f->hasBar = (cmd != "binom");
                f->compactFraction = (cmd == "tfrac");
                f->num = ParseBracedArg();
                f->den = ParseBracedArg();
                if (!f->hasBar) {
                    auto d = std::make_shared<MathNode>();
                    d->kind = NodeKind::Delimiter;
                    d->leftDelim = "(";
                    d->rightDelim = ")";
                    d->base = f;
                    return d;
                }
                return f;
            }
            if (cmd == "sqrt") {
                auto sq = std::make_shared<MathNode>();
                sq->kind = NodeKind::Sqrt;
                sq->rootIdx = ParseOptionalBracketArg();
                sq->radicand = ParseBracedArg();
                return sq;
            }
            std::string bigSym;
            if (IsBigOpCommand(cmd, bigSym)) {
                auto op = std::make_shared<MathNode>();
                op->kind = NodeKind::BigOp;
                op->bigOpSym = bigSym;
                // Consume _ and ^ limits in either order
                for (int k = 0; k < 2; ++k) {
                    SkipWhitespace();
                    if (pos_ < src_.size() && src_[pos_] == '_' && !op->sub) {
                        ++pos_;
                        op->sub = ParseBracedArg();
                    } else if (pos_ < src_.size() && src_[pos_] == '^' && !op->sup) {
                        ++pos_;
                        op->sup = ParseBracedArg();
                    }
                }
                return op;
            }
            if (cmd == "vec" || cmd == "overrightarrow" || cmd == "bar" || cmd == "overline" ||
                cmd == "hat" || cmd == "widehat" || cmd == "tilde" || cmd == "dot" || cmd == "ddot") {
                auto ac = std::make_shared<MathNode>();
                ac->kind = NodeKind::Accent;
                ac->accentKind = cmd;
                ac->base = ParseBracedArg();
                return ac;
            }
            if (cmd == "text" || cmd == "textrm" || cmd == "mathrm" || cmd == "operatorname") {
                std::string raw = ReadBracedGroupRaw();
                auto t = std::make_shared<MathNode>();
                t->kind = NodeKind::TextRun;
                t->text = raw;
                t->role = (cmd == "text" || cmd == "textrm") ? FontRole::TextNormal : FontRole::MathRoman;
                return t;
            }
            if (cmd == "textbf" || cmd == "mathbf") {
                std::string raw = ReadBracedGroupRaw();
                auto t = std::make_shared<MathNode>();
                t->kind = NodeKind::TextRun;
                t->text = raw;
                t->role = FontRole::MathBold;
                return t;
            }
            if (cmd == "mathit") {
                std::string raw = ReadBracedGroupRaw();
                auto t = std::make_shared<MathNode>();
                t->kind = NodeKind::TextRun;
                t->text = raw;
                t->role = FontRole::MathItalic;
                return t;
            }
            if (cmd == "mathbb") {
                std::string raw = ReadBracedGroupRaw();
                auto t = std::make_shared<MathNode>();
                t->kind = NodeKind::TextRun;
                t->text = MapBlackboardBold(raw);
                t->role = FontRole::MathRoman;
                return t;
            }
            if (cmd == "left") {
                auto d = std::make_shared<MathNode>();
                d->kind = NodeKind::Delimiter;
                d->leftDelim = ReadDelimToken();
                d->base = ParseRow();
                if (MatchesAhead("\\right")) {
                    pos_ += 6;
                    d->rightDelim = ReadDelimToken();
                } else {
                    d->rightDelim.clear();
                }
                return d;
            }
            if (cmd == "begin") {
                std::string env = ReadBracedGroupRaw();
                auto m = std::make_shared<MathNode>();
                m->kind = NodeKind::Matrix;
                m->envName = env;
                if (env == "pmatrix") { m->leftDelim = "("; m->rightDelim = ")"; }
                else if (env == "bmatrix") { m->leftDelim = "["; m->rightDelim = "]"; }
                else if (env == "Bmatrix") { m->leftDelim = "{"; m->rightDelim = "}"; }
                else if (env == "vmatrix") { m->leftDelim = "|"; m->rightDelim = "|"; }
                else if (env == "cases") { m->leftDelim = "{"; m->rightDelim = ""; }
                else { m->leftDelim = ""; m->rightDelim = ""; }

                while (pos_ < src_.size()) {
                    std::vector<MathNodePtr> rowCells;
                    while (pos_ < src_.size()) {
                        rowCells.push_back(ParseRow());
                        SkipWhitespace();
                        if (pos_ < src_.size() && src_[pos_] == '&') {
                            ++pos_;
                            continue;
                        }
                        break;
                    }
                    m->grid.push_back(std::move(rowCells));
                    SkipWhitespace();
                    if (MatchesAhead("\\\\")) {
                        pos_ += 2;
                        continue;
                    }
                    if (MatchesAhead("\\end{")) {
                        pos_ += 5;
                        while (pos_ < src_.size() && src_[pos_] != '}') ++pos_;
                        if (pos_ < src_.size()) ++pos_;
                        break;
                    }
                    break;
                }
                if (!m->leftDelim.empty() || !m->rightDelim.empty()) {
                    auto d = std::make_shared<MathNode>();
                    d->kind = NodeKind::Delimiter;
                    d->leftDelim = m->leftDelim;
                    d->rightDelim = m->rightDelim;
                    d->base = m;
                    return d;
                }
                return m;
            }

            if (IsNamedRomanFunction(cmd)) {
                auto t = std::make_shared<MathNode>();
                t->kind = NodeKind::TextRun;
                t->text = cmd;
                t->role = FontRole::MathRoman;
                return t;
            }

            auto it = LatexSymbolMap().find(cmd);
            auto t = std::make_shared<MathNode>();
            t->kind = NodeKind::TextRun;
            if (it != LatexSymbolMap().end()) {
                t->text = it->second;
            } else {
                t->text = cmd;
            }
            t->role = FontRole::MathRoman;
            return t;
        }

        // Single character / digit / letter / operator token
        auto t = std::make_shared<MathNode>();
        t->kind = NodeKind::TextRun;
        if (std::isalpha(static_cast<unsigned char>(c))) {
            t->text = std::string(1, c);
            t->role = FontRole::MathItalic;
            ++pos_;
            return t;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t start = pos_;
            while (pos_ < src_.size() &&
                   (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.')) {
                ++pos_;
            }
            t->text = src_.substr(start, pos_ - start);
            t->role = FontRole::MathRoman;
            return t;
        }
        if (c == '-' && pos_ + 1 < src_.size()) {
            t->text = "\xE2\x88\x92"; // Unicode minus −
            t->role = FontRole::MathRoman;
            t->isOperator = true;
            ++pos_;
            return t;
        }
        if (c == '+' || c == '=' || c == '<' || c == '>' || c == '*' || c == '/' || c == ',' || c == ';') {
            if (c == '+' || c == '=' || c == '<' || c == '>') {
                t->text = std::string(" ") + c + " ";
            } else if (c == '*') {
                t->text = "\xC2\xB7";
            } else {
                t->text = std::string(1, c);
            }
            t->role = FontRole::MathRoman;
            t->isOperator = true;
            ++pos_;
            return t;
        }

        // UTF-8 multi-byte character or punctuation
        unsigned char uc = static_cast<unsigned char>(src_[pos_]);
        size_t len = 1;
        if ((uc & 0xE0) == 0xC0) len = 2;
        else if ((uc & 0xF0) == 0xE0) len = 3;
        else if ((uc & 0xF8) == 0xF0) len = 4;
        len = std::min(len, src_.size() - pos_);
        t->text = src_.substr(pos_, len);
        t->role = FontRole::MathRoman;
        pos_ += len;
        return t;
    }

    MathNodePtr ParseAtomWithScripts() {
        SkipWhitespace();
        if (pos_ >= src_.size()) return nullptr;

        MathNodePtr base;
        if (src_[pos_] == '^' || src_[pos_] == '_') {
            // Standalone leading script (e.g. ^2 in inline auto-math)
            base = std::make_shared<MathNode>();
            base->kind = NodeKind::Space;
            base->spaceEm = 0.02;
        } else {
            base = ParseSingleAtom(true);
        }
        if (!base) return nullptr;

        MathNodePtr sub, sup;
        for (int k = 0; k < 2; ++k) {
            SkipWhitespace();
            if (pos_ < src_.size() && src_[pos_] == '_' && !sub) {
                ++pos_;
                sub = ParseBracedArg();
            } else if (pos_ < src_.size() && src_[pos_] == '^' && !sup) {
                ++pos_;
                sup = ParseBracedArg();
            }
        }
        if (!sub && !sup) return base;

        auto ss = std::make_shared<MathNode>();
        ss->kind = NodeKind::SupSub;
        ss->base = base;
        ss->sub = sub;
        ss->sup = sup;
        return ss;
    }
};

struct PangoDescGuard {
    PangoFontDescription* desc = nullptr;
    explicit PangoDescGuard(FontRole role, double fontPx, bool globalBold) {
        desc = pango_font_description_new();
        switch (role) {
        case FontRole::MathItalic:
            pango_font_description_set_family(desc, "DejaVu Serif, Liberation Serif, Noto Serif, serif");
            pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
            pango_font_description_set_weight(desc, globalBold ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
            break;
        case FontRole::MathRoman:
            pango_font_description_set_family(desc, "DejaVu Sans, Noto Sans, Liberation Sans, sans-serif");
            pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
            pango_font_description_set_weight(desc, globalBold ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
            break;
        case FontRole::MathBold:
        case FontRole::TextBold:
            pango_font_description_set_family(desc, "DejaVu Sans, Noto Sans, Liberation Sans, sans-serif");
            pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
            pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
            break;
        case FontRole::MathSymbol:
            pango_font_description_set_family(desc, "DejaVu Serif, DejaVu Sans, FreeSerif, serif");
            pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
            pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
            break;
        case FontRole::TextNormal:
        default:
            pango_font_description_set_family(desc, "Sans");
            pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
            pango_font_description_set_weight(desc, globalBold ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL);
            break;
        }
        pango_font_description_set_absolute_size(desc, std::max(5.0, fontPx) * PANGO_SCALE);
    }
    ~PangoDescGuard() {
        if (desc) pango_font_description_free(desc);
    }
};

BoxMetrics MeasureString(cairo_t* cr, const std::string& text, FontRole role, double fontPx, bool globalBold) {
    if (text.empty()) return {0.0, fontPx * 0.78, fontPx * 0.22};
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoDescGuard fg(role, fontPx, globalBold);
    pango_layout_set_font_description(layout, fg.desc);
    pango_layout_set_text(layout, text.c_str(), static_cast<int>(text.size()));
    int pw = 0, ph = 0;
    pango_layout_get_pixel_size(layout, &pw, &ph);
    double base = static_cast<double>(pango_layout_get_baseline(layout)) / PANGO_SCALE;
    if (base <= 0.0) base = ph * 0.78;
    double desc = std::max(1.0, static_cast<double>(ph) - base);
    g_object_unref(layout);
    return {static_cast<double>(pw), base, desc};
}

void DrawStringAtBaseline(cairo_t* cr, const std::string& text, FontRole role, double fontPx, bool globalBold,
                          double x, double baselineY, CasioThemeLinux::RgbaColor color) {
    if (text.empty()) return;
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoDescGuard fg(role, fontPx, globalBold);
    pango_layout_set_font_description(layout, fg.desc);
    pango_layout_set_text(layout, text.c_str(), static_cast<int>(text.size()));
    double base = static_cast<double>(pango_layout_get_baseline(layout)) / PANGO_SCALE;
    CasioThemeLinux::CairoSetSource(cr, color);
    cairo_move_to(cr, x, baselineY - base);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

BoxMetrics MeasureNode(cairo_t* cr, const MathNodePtr& node, double fontPx, bool globalBold) {
    if (!node) return {0.0, fontPx * 0.75, fontPx * 0.25};
    node->childScale = 1.0;

    switch (node->kind) {
    case NodeKind::Space: {
        node->m = {std::max(0.0, node->spaceEm * fontPx), fontPx * 0.75, fontPx * 0.22};
        return node->m;
    }
    case NodeKind::TextRun: {
        node->m = MeasureString(cr, node->text, node->role, fontPx, globalBold);
        return node->m;
    }
    case NodeKind::Row: {
        double w = 0.0, asc = fontPx * 0.72, desc = fontPx * 0.22;
        for (const auto& ch : node->children) {
            BoxMetrics cm = MeasureNode(cr, ch, fontPx, globalBold);
            w += cm.width;
            asc = std::max(asc, cm.ascent);
            desc = std::max(desc, cm.descent);
        }
        node->m = {w, asc, desc};
        return node->m;
    }
    case NodeKind::Fraction: {
        const double subScale = node->compactFraction ? 0.74 : 0.84;
        const double subFont = std::max(8.0, fontPx * subScale);
        node->childScale = subScale;
        BoxMetrics nm = MeasureNode(cr, node->num, subFont, globalBold);
        BoxMetrics dm = MeasureNode(cr, node->den, subFont, globalBold);
        const double padX = std::max(3.0, fontPx * 0.20);
        const double gapY = std::max(2.0, fontPx * 0.14);
        const double axisH = fontPx * 0.28; // math axis height above baseline
        double w = std::max(nm.width, dm.width) + padX * 2.0;
        double asc = axisH + gapY + nm.height() + 1.0;
        double desc = std::max(fontPx * 0.25, gapY + dm.height() - axisH + 1.0);
        node->m = {w, asc, desc};
        return node->m;
    }
    case NodeKind::Sqrt: {
        BoxMetrics rm = MeasureNode(cr, node->radicand, fontPx, globalBold);
        const double padTop = std::max(3.0, fontPx * 0.20);
        const double hookW = std::max(8.0, fontPx * 0.56);
        double idxW = 0.0;
        double idxH = 0.0;
        if (node->rootIdx) {
            BoxMetrics im = MeasureNode(cr, node->rootIdx, std::max(7.5, fontPx * 0.58), globalBold);
            idxW = im.width + 1.5;
            idxH = im.height();
        }
        double totalHookW = std::max(hookW, idxW + hookW * 0.52);
        double asc = std::max(rm.ascent + padTop, idxH + rm.ascent * 0.35);
        double desc = std::max(rm.descent, fontPx * 0.22) + 1.5;
        node->m = {totalHookW + rm.width + 3.0, asc, desc};
        return node->m;
    }
    case NodeKind::SupSub: {
        BoxMetrics bm = MeasureNode(cr, node->base, fontPx, globalBold);
        const double scrFont = std::max(7.5, fontPx * 0.68);
        BoxMetrics supM = node->sup ? MeasureNode(cr, node->sup, scrFont, globalBold) : BoxMetrics{};
        BoxMetrics subM = node->sub ? MeasureNode(cr, node->sub, scrFont, globalBold) : BoxMetrics{};
        const double scrW = std::max(supM.width, subM.width);
        const double supLift = bm.ascent * 0.42;
        const double subDrop = std::max(fontPx * 0.20, bm.descent * 0.55);
        double asc = bm.ascent;
        double desc = bm.descent;
        if (node->sup) asc = std::max(asc, supLift + supM.height() * 0.85);
        if (node->sub) desc = std::max(desc, subDrop + subM.height() * 0.55);
        node->m = {bm.width + scrW + 1.0, asc, desc};
        return node->m;
    }
    case NodeKind::BigOp: {
        const double opScale = (node->bigOpSym == "lim") ? 1.0 : 1.48;
        BoxMetrics om = MeasureString(cr, node->bigOpSym, FontRole::MathSymbol, fontPx * opScale, globalBold);
        const double limFont = std::max(7.0, fontPx * 0.64);
        BoxMetrics subM = node->sub ? MeasureNode(cr, node->sub, limFont, globalBold) : BoxMetrics{};
        BoxMetrics supM = node->sup ? MeasureNode(cr, node->sup, limFont, globalBold) : BoxMetrics{};
        double w = std::max(om.width, std::max(subM.width, supM.width)) + 4.0;
        double asc = om.ascent + (node->sup ? (supM.height() + 2.0) : 0.0);
        double desc = om.descent + (node->sub ? (subM.height() + 2.0) : 0.0);
        node->m = {w, asc, desc};
        return node->m;
    }
    case NodeKind::Accent: {
        BoxMetrics bm = MeasureNode(cr, node->base, fontPx, globalBold);
        const double extra = std::max(4.0, fontPx * 0.30);
        node->m = {bm.width + 2.0, bm.ascent + extra, bm.descent};
        return node->m;
    }
    case NodeKind::Delimiter: {
        BoxMetrics bm = MeasureNode(cr, node->base, fontPx, globalBold);
        double dw = std::max(5.0, fontPx * 0.36);
        double lW = node->leftDelim.empty() ? 0.0 : dw;
        double rW = node->rightDelim.empty() ? 0.0 : dw;
        node->m = {lW + bm.width + rW + 2.0, bm.ascent + 2.0, bm.descent + 2.0};
        return node->m;
    }
    case NodeKind::Matrix: {
        const size_t nRows = node->grid.size();
        size_t nCols = 0;
        for (const auto& r : node->grid) nCols = std::max(nCols, r.size());
        std::vector<double> colW(nCols, 0.0);
        double totalH = 0.0;
        const double rowGap = std::max(4.0, fontPx * 0.30);
        const double colGap = std::max(8.0, fontPx * 0.60);
        for (size_t r = 0; r < nRows; ++r) {
            double rowH = fontPx;
            for (size_t c = 0; c < node->grid[r].size(); ++c) {
                BoxMetrics cm = MeasureNode(cr, node->grid[r][c], fontPx * 0.90, globalBold);
                colW[c] = std::max(colW[c], cm.width);
                rowH = std::max(rowH, cm.height());
            }
            totalH += rowH + (r + 1 < nRows ? rowGap : 0.0);
        }
        double totalW = 4.0;
        for (size_t c = 0; c < nCols; ++c) {
            totalW += colW[c] + (c + 1 < nCols ? colGap : 0.0);
        }
        const double axisH = fontPx * 0.28;
        node->m = {totalW, totalH * 0.5 + axisH, std::max(2.0, totalH * 0.5 - axisH)};
        return node->m;
    }
    }
    return {0.0, fontPx * 0.75, fontPx * 0.25};
}

void DrawDelimiterGlyph(cairo_t* cr, const std::string& delim,
                        double x, double topY, double botY, double w,
                        CasioThemeLinux::RgbaColor color) {
    if (delim.empty() || w <= 0.5 || botY <= topY) return;
    CasioThemeLinux::CairoSetSource(cr, color);
    cairo_set_line_width(cr, std::clamp(w * 0.16, 1.1, 2.2));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    const double h = botY - topY;
    const double midY = 0.5 * (topY + botY);
    if (delim == "(" || delim == ")") {
        const bool isLeft = (delim == "(");
        const double xEdge = isLeft ? (x + w * 0.76) : (x + w * 0.24);
        const double xPeak = isLeft ? (x + w * 0.20) : (x + w * 0.80);
        cairo_move_to(cr, xEdge, topY + 1.0);
        cairo_curve_to(cr, xPeak, topY + h * 0.25, xPeak, topY + h * 0.75, xEdge, botY - 1.0);
        cairo_stroke(cr);
    } else if (delim == "[" || delim == "]") {
        const bool isLeft = (delim == "[");
        const double xVert = isLeft ? (x + w * 0.34) : (x + w * 0.66);
        const double xTip = isLeft ? (x + w * 0.80) : (x + w * 0.20);
        cairo_move_to(cr, xTip, topY + 1.0);
        cairo_line_to(cr, xVert, topY + 1.0);
        cairo_line_to(cr, xVert, botY - 1.0);
        cairo_line_to(cr, xTip, botY - 1.0);
        cairo_stroke(cr);
    } else if (delim == "{" || delim == "}") {
        const bool isLeft = (delim == "{");
        const double xStem = x + w * 0.50;
        const double xTip = isLeft ? (x + w * 0.80) : (x + w * 0.20);
        const double xPoint = isLeft ? (x + w * 0.18) : (x + w * 0.82);
        cairo_move_to(cr, xTip, topY + 1.0);
        cairo_curve_to(cr, xStem, topY + 1.0, xStem, midY - h * 0.12, xPoint, midY);
        cairo_curve_to(cr, xStem, midY + h * 0.12, xStem, botY - 1.0, xTip, botY - 1.0);
        cairo_stroke(cr);
    } else if (delim == "|") {
        const double cx = x + w * 0.50;
        cairo_move_to(cr, cx, topY + 1.0);
        cairo_line_to(cr, cx, botY - 1.0);
        cairo_stroke(cr);
    } else if (delim == "\xE2\x80\x96") {
        cairo_move_to(cr, x + w * 0.35, topY + 1.0);
        cairo_line_to(cr, x + w * 0.35, botY - 1.0);
        cairo_move_to(cr, x + w * 0.65, topY + 1.0);
        cairo_line_to(cr, x + w * 0.65, botY - 1.0);
        cairo_stroke(cr);
    } else {
        const bool isLeft = (delim == "\xE2\x9F\xA8");
        const double xEdge = isLeft ? (x + w * 0.78) : (x + w * 0.22);
        const double xMid = isLeft ? (x + w * 0.22) : (x + w * 0.78);
        cairo_move_to(cr, xEdge, topY + 1.0);
        cairo_line_to(cr, xMid, midY);
        cairo_line_to(cr, xEdge, botY - 1.0);
        cairo_stroke(cr);
    }
}

void DrawNode(cairo_t* cr, const MathNodePtr& node, double x, double baselineY,
              double fontPx, CasioThemeLinux::RgbaColor color, bool globalBold) {
    if (!node) return;
    switch (node->kind) {
    case NodeKind::Space:
        return;
    case NodeKind::TextRun:
        DrawStringAtBaseline(cr, node->text, node->role, fontPx, globalBold, x, baselineY, color);
        return;
    case NodeKind::Row: {
        double cx = x;
        for (const auto& ch : node->children) {
            DrawNode(cr, ch, cx, baselineY, fontPx, color, globalBold);
            cx += ch->m.width;
        }
        return;
    }
    case NodeKind::Fraction: {
        const double subScale = node->compactFraction ? 0.74 : 0.84;
        const double subFont = std::max(8.0, fontPx * subScale);
        const double padX = std::max(3.0, fontPx * 0.20);
        const double gapY = std::max(2.0, fontPx * 0.14);
        const double axisY = baselineY - fontPx * 0.28;
        const double numW = node->num->m.width;
        const double denW = node->den->m.width;
        const double contentW = std::max(numW, denW);

        double numBaseline = axisY - gapY - node->num->m.descent;
        double denBaseline = axisY + gapY + node->den->m.ascent;
        double numX = x + padX + 0.5 * (contentW - numW);
        double denX = x + padX + 0.5 * (contentW - denW);

        DrawNode(cr, node->num, numX, numBaseline, subFont, color, globalBold);
        DrawNode(cr, node->den, denX, denBaseline, subFont, color, globalBold);

        if (node->hasBar) {
            CasioThemeLinux::CairoSetSource(cr, color);
            cairo_set_line_width(cr, std::clamp(fontPx * 0.08, 1.0, 2.0));
            cairo_move_to(cr, x + 2.0, axisY);
            cairo_line_to(cr, x + node->m.width - 2.0, axisY);
            cairo_stroke(cr);
        }
        return;
    }
    case NodeKind::Sqrt: {
        const double hookW = std::max(8.0, fontPx * 0.56);
        double idxW = 0.0;
        if (node->rootIdx) idxW = node->rootIdx->m.width + 1.5;
        double totalHookW = std::max(hookW, idxW + hookW * 0.52);

        const double topBarY = baselineY - node->m.ascent + 2.0;
        const double botTipY = baselineY + node->m.descent - 1.5;
        const double radX = x + totalHookW;
        DrawNode(cr, node->radicand, radX, baselineY, fontPx, color, globalBold);

        if (node->rootIdx) {
            const double idxFont = std::max(7.5, fontPx * 0.58);
            DrawNode(cr, node->rootIdx, x + 1.0, topBarY + node->rootIdx->m.ascent + 1.0,
                     idxFont, color, globalBold);
        }

        CasioThemeLinux::CairoSetSource(cr, color);
        cairo_set_line_width(cr, std::clamp(fontPx * 0.085, 1.1, 2.1));
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        const double hookStartX = x + totalHookW - hookW;
        const double midY = 0.55 * (topBarY + botTipY);
        cairo_move_to(cr, hookStartX + 1.0, midY);
        cairo_line_to(cr, hookStartX + hookW * 0.30, midY - 1.5);
        cairo_line_to(cr, hookStartX + hookW * 0.58, botTipY);
        cairo_line_to(cr, radX - 1.5, topBarY);
        cairo_line_to(cr, x + node->m.width - 1.0, topBarY);
        cairo_stroke(cr);
        return;
    }
    case NodeKind::SupSub: {
        DrawNode(cr, node->base, x, baselineY, fontPx, color, globalBold);
        const double scrFont = std::max(7.5, fontPx * 0.68);
        const double sx = x + node->base->m.width + 0.8;
        if (node->sup) {
            const double supBaseline = baselineY - node->base->m.ascent * 0.44;
            DrawNode(cr, node->sup, sx, supBaseline, scrFont, color, globalBold);
        }
        if (node->sub) {
            const double subDrop = std::max(fontPx * 0.22, node->base->m.descent * 0.55);
            DrawNode(cr, node->sub, sx, baselineY + subDrop, scrFont, color, globalBold);
        }
        return;
    }
    case NodeKind::BigOp: {
        const double opScale = (node->bigOpSym == "lim") ? 1.0 : 1.48;
        BoxMetrics om = MeasureString(cr, node->bigOpSym, FontRole::MathSymbol, fontPx * opScale, globalBold);
        const double ox = x + 0.5 * (node->m.width - om.width);
        DrawStringAtBaseline(cr, node->bigOpSym, FontRole::MathSymbol, fontPx * opScale, globalBold,
                             ox, baselineY + om.descent * 0.15, color);
        const double limFont = std::max(7.0, fontPx * 0.64);
        if (node->sup) {
            double sx = x + 0.5 * (node->m.width - node->sup->m.width);
            double sBase = baselineY - om.ascent - 2.0 - node->sup->m.descent;
            DrawNode(cr, node->sup, sx, sBase, limFont, color, globalBold);
        }
        if (node->sub) {
            double sx = x + 0.5 * (node->m.width - node->sub->m.width);
            double sBase = baselineY + om.descent + 2.0 + node->sub->m.ascent;
            DrawNode(cr, node->sub, sx, sBase, limFont, color, globalBold);
        }
        return;
    }
    case NodeKind::Accent: {
        const double bx = x + 1.0;
        DrawNode(cr, node->base, bx, baselineY, fontPx, color, globalBold);
        const double topY = baselineY - node->base->m.ascent - 2.5;
        const double x0 = bx + 1.0;
        const double x1 = bx + std::max(4.0, node->base->m.width - 1.0);
        const double mx = 0.5 * (x0 + x1);
        CasioThemeLinux::CairoSetSource(cr, color);
        cairo_set_line_width(cr, std::clamp(fontPx * 0.08, 1.0, 1.8));
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        if (node->accentKind == "bar" || node->accentKind == "overline") {
            cairo_move_to(cr, x0, topY);
            cairo_line_to(cr, x1, topY);
            cairo_stroke(cr);
        } else if (node->accentKind == "vec" || node->accentKind == "overrightarrow") {
            cairo_move_to(cr, x0, topY);
            cairo_line_to(cr, x1, topY);
            cairo_line_to(cr, x1 - 3.2, topY - 2.2);
            cairo_move_to(cr, x1, topY);
            cairo_line_to(cr, x1 - 3.2, topY + 2.2);
            cairo_stroke(cr);
        } else if (node->accentKind == "hat" || node->accentKind == "widehat") {
            cairo_move_to(cr, x0, topY + 1.5);
            cairo_line_to(cr, mx, topY - 2.2);
            cairo_line_to(cr, x1, topY + 1.5);
            cairo_stroke(cr);
        } else if (node->accentKind == "tilde") {
            cairo_move_to(cr, x0, topY);
            cairo_curve_to(cr, (x0 + mx) * 0.5, topY - 3.0, (mx + x1) * 0.5, topY + 3.0, x1, topY);
            cairo_stroke(cr);
        } else if (node->accentKind == "dot") {
            cairo_arc(cr, mx, topY, 1.4, 0.0, 6.28318);
            cairo_fill(cr);
        } else if (node->accentKind == "ddot") {
            cairo_arc(cr, mx - 2.4, topY, 1.3, 0.0, 6.28318);
            cairo_arc(cr, mx + 2.4, topY, 1.3, 0.0, 6.28318);
            cairo_fill(cr);
        }
        return;
    }
    case NodeKind::Delimiter: {
        const double dw = std::max(5.0, fontPx * 0.36);
        const double lW = node->leftDelim.empty() ? 0.0 : dw;
        const double topY = baselineY - node->base->m.ascent;
        const double botY = baselineY + node->base->m.descent;
        if (lW > 0.0) DrawDelimiterGlyph(cr, node->leftDelim, x, topY, botY, lW, color);
        DrawNode(cr, node->base, x + lW + 1.0, baselineY, fontPx, color, globalBold);
        if (!node->rightDelim.empty()) {
            DrawDelimiterGlyph(cr, node->rightDelim, x + lW + 2.0 + node->base->m.width,
                               topY, botY, dw, color);
        }
        return;
    }
    case NodeKind::Matrix: {
        const size_t nRows = node->grid.size();
        size_t nCols = 0;
        for (const auto& r : node->grid) nCols = std::max(nCols, r.size());
        std::vector<double> colW(nCols, 0.0);
        std::vector<double> rowAsc(nRows, fontPx * 0.72);
        std::vector<double> rowDesc(nRows, fontPx * 0.24);
        const double rowGap = std::max(4.0, fontPx * 0.30);
        const double colGap = std::max(8.0, fontPx * 0.60);
        for (size_t r = 0; r < nRows; ++r) {
            for (size_t c = 0; c < node->grid[r].size(); ++c) {
                const auto& cm = node->grid[r][c]->m;
                colW[c] = std::max(colW[c], cm.width);
                rowAsc[r] = std::max(rowAsc[r], cm.ascent);
                rowDesc[r] = std::max(rowDesc[r], cm.descent);
            }
        }
        double cy = baselineY - node->m.ascent + 2.0;
        for (size_t r = 0; r < nRows; ++r) {
            double rBase = cy + rowAsc[r];
            double cx = x + 2.0;
            for (size_t c = 0; c < node->grid[r].size(); ++c) {
                const auto& cell = node->grid[r][c];
                double cellX = cx + 0.5 * (colW[c] - cell->m.width);
                DrawNode(cr, cell, cellX, rBase, fontPx * 0.90, color, globalBold);
                cx += colW[c] + colGap;
            }
            cy += rowAsc[r] + rowDesc[r] + rowGap;
        }
        return;
    }
    }
}

struct InlineToken {
    bool isMath = false;
    std::string text;
    MathNodePtr ast;
    BoxMetrics m{};
};

bool LooksLikeAutoMathToken(const std::string& w) {
    if (w.size() < 2) return false;
    if (w.find('\\') != std::string::npos) return true;
    const bool hasSupSub = (w.find('^') != std::string::npos) || (w.find('_') != std::string::npos);
    if (!hasSupSub) return false;
    bool hasAlnum = false;
    for (unsigned char c : w) {
        if (std::isalnum(c)) { hasAlnum = true; break; }
    }
    return hasAlnum;
}

std::vector<InlineToken> TokenizeMixedText(const std::string& input, bool mathDefault) {
    std::vector<InlineToken> tokens;
    if (mathDefault && input.find('$') == std::string::npos) {
        LatexParser parser(input);
        InlineToken tk;
        tk.isMath = true;
        tk.text = input;
        tk.ast = parser.ParseRow();
        tokens.push_back(std::move(tk));
        return tokens;
    }

    size_t i = 0;
    auto pushPlainWords = [&](const std::string& seg) {
        size_t p = 0;
        while (p < seg.size()) {
            if (seg[p] == '\n') {
                tokens.push_back({false, "\n", nullptr, {}});
                ++p;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(seg[p]))) {
                size_t start = p;
                while (p < seg.size() && seg[p] != '\n' && std::isspace(static_cast<unsigned char>(seg[p]))) ++p;
                tokens.push_back({false, seg.substr(start, p - start), nullptr, {}});
                continue;
            }
            size_t start = p;
            while (p < seg.size() && !std::isspace(static_cast<unsigned char>(seg[p]))) ++p;
            std::string word = seg.substr(start, p - start);
            if (LooksLikeAutoMathToken(word)) {
                // Separate trailing punctuation (e.g. "x^2," -> math "x^2" + plain ",")
                std::string trail;
                while (word.size() > 2 &&
                       (word.back() == '.' || word.back() == ',' || word.back() == ';' || word.back() == ':' || word.back() == ')')) {
                    if (word.back() == ')' && word.find('(') != std::string::npos) break;
                    trail.insert(trail.begin(), word.back());
                    word.pop_back();
                }
                LatexParser parser(word);
                tokens.push_back({true, word, parser.ParseRow(), {}});
                if (!trail.empty()) tokens.push_back({false, trail, nullptr, {}});
            } else {
                tokens.push_back({false, word, nullptr, {}});
            }
        }
    };

    while (i < input.size()) {
        size_t dollar = input.find('$', i);
        if (dollar == std::string::npos) {
            pushPlainWords(input.substr(i));
            break;
        }
        if (dollar > i) {
            pushPlainWords(input.substr(i, dollar - i));
        }
        bool isDouble = (dollar + 1 < input.size() && input[dollar + 1] == '$');
        size_t start = dollar + (isDouble ? 2 : 1);
        size_t close = input.find(isDouble ? "$$" : "$", start);
        if (close == std::string::npos) {
            pushPlainWords(input.substr(dollar));
            break;
        }
        std::string mathSrc = input.substr(start, close - start);
        LatexParser parser(mathSrc);
        tokens.push_back({true, mathSrc, parser.ParseRow(), {}});
        i = close + (isDouble ? 2 : 1);
    }
    return tokens;
}

struct RenderedLine {
    std::vector<InlineToken> items;
    double width = 0.0;
    double ascent = 0.0;
    double descent = 0.0;
};

std::vector<RenderedLine> LayoutTokens(cairo_t* cr, const std::string& text, double fontPx,
                                       double maxWidth, bool bold, bool mathDefault) {
    auto tokens = TokenizeMixedText(text, mathDefault);
    for (auto& tk : tokens) {
        if (tk.text == "\n") continue;
        if (tk.isMath && tk.ast) {
            tk.m = MeasureNode(cr, tk.ast, fontPx, bold);
        } else {
            tk.m = MeasureString(cr, tk.text, bold ? FontRole::TextBold : FontRole::TextNormal, fontPx, bold);
        }
    }

    std::vector<RenderedLine> lines;
    RenderedLine cur;
    const double baseAsc = fontPx * 0.78;
    const double baseDesc = fontPx * 0.24;
    cur.ascent = baseAsc;
    cur.descent = baseDesc;

    auto flushLine = [&]() {
        while (!cur.items.empty() && !cur.items.back().isMath &&
               std::all_of(cur.items.back().text.begin(), cur.items.back().text.end(),
                           [](unsigned char c) { return std::isspace(c) != 0; })) {
            cur.width -= cur.items.back().m.width;
            cur.items.pop_back();
        }
        cur.width = std::max(0.0, cur.width);
        lines.push_back(std::move(cur));
        cur = RenderedLine{};
        cur.ascent = baseAsc;
        cur.descent = baseDesc;
    };

    for (auto& tk : tokens) {
        if (tk.text == "\n") {
            flushLine();
            continue;
        }
        const bool isSpaceOnly = !tk.isMath &&
            std::all_of(tk.text.begin(), tk.text.end(), [](unsigned char c) { return std::isspace(c) != 0; });
        if (cur.items.empty() && isSpaceOnly) continue;

        if (maxWidth > 0.0 && !cur.items.empty() && !isSpaceOnly &&
            (cur.width + tk.m.width > maxWidth)) {
            flushLine();
        }
        cur.width += tk.m.width;
        cur.ascent = std::max(cur.ascent, tk.m.ascent);
        cur.descent = std::max(cur.descent, tk.m.descent);
        cur.items.push_back(std::move(tk));
    }
    if (!cur.items.empty() || lines.empty()) {
        flushLine();
    }
    return lines;
}

} // namespace

ValidationResult ValidateRichLatexInput(const std::string& raw) {
    int dollarCount = 0;
    int braceDepth = 0;
    int leftCount = 0;
    int rightCount = 0;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\') {
            if (raw.compare(i, 5, "\\left") == 0) leftCount++;
            else if (raw.compare(i, 6, "\\right") == 0) rightCount++;
            ++i;
            continue;
        }
        if (raw[i] == '$') dollarCount++;
        else if (raw[i] == '{') braceDepth++;
        else if (raw[i] == '}') {
            braceDepth--;
            if (braceDepth < 0) return {false, "Llave '}' sin apertura correspondiente"};
        }
    }
    if ((dollarCount % 2) != 0) return {false, "Delimitador '$' matematico sin cerrar"};
    if (braceDepth != 0) return {false, "Llave '{' sin cerrar en expresion LaTeX"};
    if (leftCount != rightCount) return {false, "Delimitador \\left / \\right desbalanceado"};
    return {true, "Sintaxis LaTeX valida"};
}

MeasureExtent MeasureLatexRichText(cairo_t* cr, const std::string& text, double fontPx,
                                   double maxWidth, bool bold, bool mathDefault) {
    if (!cr) return {};
    auto lines = LayoutTokens(cr, text, fontPx, maxWidth, bold, mathDefault);
    double w = 0.0, h = 0.0;
    const double lineGap = std::max(3.0, fontPx * 0.24);
    for (size_t i = 0; i < lines.size(); ++i) {
        w = std::max(w, lines[i].width);
        h += lines[i].ascent + lines[i].descent;
        if (i + 1 < lines.size()) h += lineGap;
    }
    return {w, h};
}

MeasureExtent DrawLatexRichText(cairo_t* cr, const std::string& text,
                                double x, double y, double maxWidth, double maxHeight,
                                double fontPx, CasioThemeLinux::RgbaColor color,
                                bool bold, bool mathDefault) {
    if (!cr) return {};
    auto lines = LayoutTokens(cr, text, fontPx, maxWidth, bold, mathDefault);
    const double lineGap = std::max(3.0, fontPx * 0.24);

    cairo_save(cr);
    if (maxWidth > 0.0 && maxHeight > 0.0) {
        cairo_rectangle(cr, x - 2.0, y - 2.0, maxWidth + 4.0, maxHeight + 4.0);
        cairo_clip(cr);
    }

    double curY = y;
    double maxDrawnW = 0.0;
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& ln = lines[i];
        const double lineH = ln.ascent + ln.descent;
        if (maxHeight > 0.0 && (curY - y) + lineH > maxHeight + 2.0 && i > 0) break;
        const double baselineY = curY + ln.ascent;
        double curX = x;
        for (const auto& tk : ln.items) {
            if (tk.isMath && tk.ast) {
                DrawNode(cr, tk.ast, curX, baselineY, fontPx, color, bold);
            } else {
                DrawStringAtBaseline(cr, tk.text, bold ? FontRole::TextBold : FontRole::TextNormal,
                                     fontPx, bold, curX, baselineY, color);
            }
            curX += tk.m.width;
        }
        maxDrawnW = std::max(maxDrawnW, ln.width);
        curY += lineH + (i + 1 < lines.size() ? lineGap : 0.0);
    }
    cairo_restore(cr);
    return {maxDrawnW, curY - y};
}

void DrawLatexRichTextCentered(cairo_t* cr, const std::string& text,
                               double x, double y, double w, double h,
                               double fontPx, CasioThemeLinux::RgbaColor color,
                               bool bold, bool mathDefault) {
    if (!cr || w <= 2.0 || h <= 2.0) return;
    double tryFont = fontPx;
    MeasureExtent ex{};
    for (int iter = 0; iter < 5; ++iter) {
        ex = MeasureLatexRichText(cr, text, tryFont, 0.0, bold, mathDefault);
        if (ex.width <= w - 4.0 && ex.height <= h - 2.0) break;
        tryFont *= 0.86;
        if (tryFont < 8.5) break;
    }
    auto lines = LayoutTokens(cr, text, tryFont, 0.0, bold, mathDefault);
    if (lines.empty()) return;
    const auto& ln = lines[0];
    double drawX = x + 0.5 * std::max(0.0, w - ln.width);
    double drawY = y + 0.5 * std::max(0.0, h - (ln.ascent + ln.descent));
    DrawLatexRichText(cr, text, drawX, drawY, w, h, tryFont, color, bold, mathDefault);
}

} // namespace CasioLatexCairo
