#pragma once

// Included exclusively by doc_studio/math/engineering_cas.cpp

namespace DocStudio::Cas {

namespace detail_sym {

inline std::string Trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::string FmtNum(double v, int prec = 12) {
    if (!std::isfinite(v)) return "NaN";
    if (std::abs(v) < 1e-13) return "0";
    if (std::abs(v - std::round(v)) < 1e-12 && std::abs(v) < 1e13) {
        return std::to_string(static_cast<long long>(std::llround(v)));
    }
    std::ostringstream oss;
    oss.precision(prec);
    oss << v;
    return oss.str();
}

inline bool IsNum(const MathJsonNode& n, double& out) {
    if (n.kind == MathJsonKind::Number) { out = n.realVal; return true; }
    if (n.kind == MathJsonKind::Complex && std::abs(n.imagVal) < 1e-13) { out = n.realVal; return true; }
    return false;
}

inline bool IsZero(const MathJsonNode& n) {
    double v = 0.0;
    return IsNum(n, v) && std::abs(v) < 1e-13;
}

inline bool IsOne(const MathJsonNode& n) {
    double v = 0.0;
    return IsNum(n, v) && std::abs(v - 1.0) < 1e-13;
}

inline bool IsComplexOrNum(const MathJsonNode& n, std::complex<double>& out) {
    if (n.kind == MathJsonKind::Number) { out = { n.realVal, 0.0 }; return true; }
    if (n.kind == MathJsonKind::Complex) { out = { n.realVal, n.imagVal }; return true; }
    return false;
}

inline MathJsonNode MakeNumOrComplex(std::complex<double> z) {
    if (std::abs(z.imag()) < 1e-13) return MathJsonNode::MakeNumber(std::abs(z.real()) < 1e-13 ? 0.0 : z.real());
    return MathJsonNode::MakeComplex(std::abs(z.real()) < 1e-13 ? 0.0 : z.real(), z.imag());
}

inline std::string CanonicalFnHead(const std::string& raw) {
    if (raw == "sin") return "Sin";
    if (raw == "cos") return "Cos";
    if (raw == "tan") return "Tan";
    if (raw == "asin") return "Asin";
    if (raw == "acos") return "Acos";
    if (raw == "atan") return "Atan";
    if (raw == "sinh") return "Sinh";
    if (raw == "cosh") return "Cosh";
    if (raw == "tanh") return "Tanh";
    if (raw == "asinh") return "Asinh";
    if (raw == "acosh") return "Acosh";
    if (raw == "atanh") return "Atanh";
    if (raw == "sqrt") return "Sqrt";
    if (raw == "exp") return "Exp";
    if (raw == "ln") return "Ln";
    if (raw == "log10" || raw == "log") return "Log10";
    if (raw == "abs") return "Abs";
    if (raw == "arg") return "Arg";
    if (raw == "phase") return "Phase";
    if (raw == "re") return "Re";
    if (raw == "im") return "Im";
    if (raw == "conj") return "Conj";
    if (raw == "floor") return "Floor";
    if (raw == "ceil") return "Ceil";
    if (raw == "round") return "Round";
    if (raw == "rect") return "Rect";
    if (raw == "polar") return "Polar";
    if (raw == "rad") return "Rad";
    if (raw == "deg") return "Deg";
    if (raw == "sind") return "Sind";
    if (raw == "cosd") return "Cosd";
    if (raw == "tand") return "Tand";
    if (raw == "atan2") return "Atan2";
    if (raw == "pow") return "Pow";
    if (raw == "min") return "Min";
    if (raw == "max") return "Max";
    return raw;
}

inline std::string HeadToCasFn(const std::string& head) {
    if (head == "Sin") return "sin";
    if (head == "Cos") return "cos";
    if (head == "Tan") return "tan";
    if (head == "Asin") return "asin";
    if (head == "Acos") return "acos";
    if (head == "Atan") return "atan";
    if (head == "Sinh") return "sinh";
    if (head == "Cosh") return "cosh";
    if (head == "Tanh") return "tanh";
    if (head == "Asinh") return "asinh";
    if (head == "Acosh") return "acosh";
    if (head == "Atanh") return "atanh";
    if (head == "Sqrt") return "sqrt";
    if (head == "Exp") return "exp";
    if (head == "Ln") return "ln";
    if (head == "Log10") return "log10";
    if (head == "Abs") return "abs";
    if (head == "Arg") return "arg";
    if (head == "Phase") return "phase";
    if (head == "Re") return "re";
    if (head == "Im") return "im";
    if (head == "Conj") return "conj";
    if (head == "Floor") return "floor";
    if (head == "Ceil") return "ceil";
    if (head == "Round") return "round";
    if (head == "Rect") return "rect";
    if (head == "Polar") return "polar";
    if (head == "Rad") return "rad";
    if (head == "Deg") return "deg";
    if (head == "Sind") return "sind";
    if (head == "Cosd") return "cosd";
    if (head == "Tand") return "tand";
    if (head == "Atan2") return "atan2";
    if (head == "Pow") return "pow";
    if (head == "Min") return "min";
    if (head == "Max") return "max";
    return head;
}

enum class AstTokKind {
    End, Number, Ident, Plus, Minus, Star, Slash, Caret, LParen, RParen, Comma, Equal, Angle, Pipe, Parallel
};

struct AstTok {
    AstTokKind kind = AstTokKind::End;
    std::string text;
    double numVal = 0.0;
    bool imagSuffix = false;
    bool isHyphenUnitSep = false;
    char engSuffix = 0;
};

class AstExprParser {
public:
    explicit AstExprParser(const std::string& src, const std::unordered_set<std::string>* boundVars = nullptr)
        : src_(src), boundVars_(boundVars) {
        Tokenize();
    }

    bool Parse(MathJsonNode& out, std::string& outErr) {
        if (tokErr_) { outErr = tokErrMsg_; return false; }
        if (tokens_.empty() || tokens_[0].kind == AstTokKind::End) {
            outErr = "Expresion vacia";
            return false;
        }
        if (!ParseEquationOrConversion(out, outErr)) return false;
        if (Peek().kind != AstTokKind::End) {
            outErr = "Token inesperado: " + Peek().text;
            return false;
        }
        if (hasHyphenSep_) {
            const MathJsonNode unitAst = std::move(out);
            for (auto& tok : tokens_) {
                if (tok.isHyphenUnitSep) {
                    tok.kind = AstTokKind::Minus;
                    tok.text = "-";
                    tok.isHyphenUnitSep = false;
                }
            }
            pos_ = 0;
            MathJsonNode minusAst;
            std::string minusErr;
            if (ParseEquationOrConversion(minusAst, minusErr) && Peek().kind == AstTokKind::End) {
                std::vector<MathJsonNode> args;
                args.reserve(2 + hyphenPairSyms_.size());
                args.push_back(unitAst);
                args.push_back(std::move(minusAst));
                for (const auto& sym : hyphenPairSyms_) {
                    args.push_back(MathJsonNode::MakeSymbol(sym));
                }
                out = MathJsonNode::MakeFunction("HyphenChoice", std::move(args));
            } else {
                out = unitAst;
            }
        }
        return true;
    }

private:
    std::string src_;
    const std::unordered_set<std::string>* boundVars_ = nullptr;
    std::vector<AstTok> tokens_;
    std::size_t pos_ = 0;
    bool tokErr_ = false;
    std::string tokErrMsg_;
    bool hasHyphenSep_ = false;
    std::vector<std::string> hyphenPairSyms_;

    const AstTok& Peek(std::size_t k = 0) const {
        static const AstTok kEnd{};
        return (pos_ + k < tokens_.size()) ? tokens_[pos_ + k] : kEnd;
    }
    AstTok Advance() {
        AstTok t = Peek();
        if (pos_ < tokens_.size()) ++pos_;
        return t;
    }
    static bool IsIdentStart(unsigned char c) { return std::isalpha(c) != 0 || c == '_' || c >= 0x80; }
    static bool IsIdentCont(unsigned char c) { return std::isalnum(c) != 0 || c == '_' || c == '%' || c >= 0x80; }

    void Tokenize() {
        std::size_t i = 0;
        bool afterPipe = false;
        while (i < src_.size()) {
            const unsigned char ch = static_cast<unsigned char>(src_[i]);
            if (std::isspace(ch)) { ++i; continue; }
            if (i + 3 <= src_.size() &&
                static_cast<unsigned char>(src_[i]) == 0xE2 &&
                static_cast<unsigned char>(src_[i + 1]) == 0x88 &&
                static_cast<unsigned char>(src_[i + 2]) == 0xA0) {
                tokens_.push_back({ AstTokKind::Angle, "\xE2\x88\xA0", 0.0, false });
                i += 3;
                continue;
            }
            if (i + 2 <= src_.size() &&
                static_cast<unsigned char>(src_[i]) == 0xC2 &&
                static_cast<unsigned char>(src_[i + 1]) == 0xB0) {
                tokens_.push_back({ AstTokKind::Ident, "deg", 0.0, false });
                i += 2;
                continue;
            }
            if (std::isdigit(ch) || (ch == '.' && i + 1 < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i + 1])))) {
                const std::size_t st = i;
                while (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) ++i;
                if (i < src_.size() && src_[i] == '.') {
                    ++i;
                    while (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) ++i;
                }
                if (i < src_.size() && (src_[i] == 'e' || src_[i] == 'E')) {
                    const std::size_t save = i++;
                    if (i < src_.size() && (src_[i] == '+' || src_[i] == '-')) ++i;
                    if (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) {
                        while (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) ++i;
                    } else {
                        i = save;
                    }
                }
                double v = 0.0;
                try { v = std::stod(src_.substr(st, i - st)); }
                catch (...) { tokErr_ = true; tokErrMsg_ = "Numero invalido"; return; }
                bool isImag = false;
                char engSuf = 0;
                if (i < src_.size() && (src_[i] == 'j' || src_[i] == 'i') &&
                    (i + 1 >= src_.size() || !IsIdentCont(static_cast<unsigned char>(src_[i + 1])))) {
                    isImag = true;
                    ++i;
                } else if (i < src_.size() && (i + 1 >= src_.size() || !IsIdentCont(static_cast<unsigned char>(src_[i + 1])))) {
                    const char p = src_[i];
                    if ((p == 'p' || p == 'n' || p == 'u' || p == 'k' || p == 'M' || p == 'G') &&
                        (!boundVars_ || boundVars_->find(std::string(1, p)) == boundVars_->end())) {
                        engSuf = p;
                        ++i;
                    }
                }
                tokens_.push_back({ AstTokKind::Number, src_.substr(st, i - st), v, isImag, false, engSuf });
                continue;
            }
            if (ch == '%') {
                tokens_.push_back({ AstTokKind::Ident, "%", 0.0, false });
                ++i;
                continue;
            }
            if (IsIdentStart(ch)) {
                const std::size_t st = i;
                while (i < src_.size() && IsIdentCont(static_cast<unsigned char>(src_[i]))) {
                    if (i + 3 <= src_.size() &&
                        static_cast<unsigned char>(src_[i]) == 0xE2 &&
                        static_cast<unsigned char>(src_[i + 1]) == 0x88 &&
                        static_cast<unsigned char>(src_[i + 2]) == 0xA0) break;
                    if (i + 2 <= src_.size() &&
                        static_cast<unsigned char>(src_[i]) == 0xC2 &&
                        static_cast<unsigned char>(src_[i + 1]) == 0xB0) break;
                    ++i;
                }
                bool prevUnitContext = false;
                if (!tokens_.empty()) {
                    std::size_t ctxIdx = tokens_.size() - 1;
                    while (ctxIdx > 0 && tokens_[ctxIdx].kind == AstTokKind::LParen) {
                        --ctxIdx;
                    }
                    const AstTokKind ck = tokens_[ctxIdx].kind;
                    prevUnitContext = (ck == AstTokKind::Number ||
                                       ck == AstTokKind::RParen ||
                                       ck == AstTokKind::Star ||
                                       ck == AstTokKind::Slash);
                }
                tokens_.push_back({ AstTokKind::Ident, src_.substr(st, i - st), 0.0, false, false, 0 });
                // FIND-07 + R2-C1-02 + R3-05: Treat lhs-rhs as hyphenated compound unit in unit factor context
                // (after number, ')', '*', '/', or '(' preceded by those) when in the engineering hyphenated pair allowlist
                // and neither side is a bound variable, while preserving deferred HyphenChoice when boundVars is empty.
                while (!afterPipe && prevUnitContext && i < src_.size() && src_[i] == '-' && i + 1 < src_.size() &&
                       IsIdentStart(static_cast<unsigned char>(src_[i + 1]))) {
                    const std::string& lhsWord = tokens_.back().text;
                    if (boundVars_ && boundVars_->find(lhsWord) != boundVars_->end()) break;
                    UnitDim d1{}, d2{};
                    double s1 = 1.0, o1 = 0.0, s2 = 1.0, o2 = 0.0;
                    if (!TryParseUnitExpression(lhsWord, d1, s1, o1) || o1 != 0.0) break;
                    std::size_t j = i + 1;
                    while (j < src_.size() && IsIdentCont(static_cast<unsigned char>(src_[j]))) ++j;
                    const std::string rhsWord = src_.substr(i + 1, j - (i + 1));
                    if (boundVars_ && boundVars_->find(rhsWord) != boundVars_->end()) break;
                    if (!IsHyphenatedCompoundUnitPair(lhsWord, rhsWord)) break;
                    if (!TryParseUnitExpression(rhsWord, d2, s2, o2) || o2 != 0.0 || d1 == d2) break;
                    hasHyphenSep_ = true;
                    if (std::find(hyphenPairSyms_.begin(), hyphenPairSyms_.end(), lhsWord) == hyphenPairSyms_.end()) {
                        hyphenPairSyms_.push_back(lhsWord);
                    }
                    if (std::find(hyphenPairSyms_.begin(), hyphenPairSyms_.end(), rhsWord) == hyphenPairSyms_.end()) {
                        hyphenPairSyms_.push_back(rhsWord);
                    }
                    tokens_.push_back({ AstTokKind::Star, "*", 0.0, false, true, 0 });
                    tokens_.push_back({ AstTokKind::Ident, rhsWord, 0.0, false, false, 0 });
                    i = j;
                }
                continue;
            }
            if (src_[i] == '|' && i + 1 < src_.size() && src_[i + 1] == '|') {
                tokens_.push_back({ AstTokKind::Parallel, "||", 0.0, false });
                i += 2;
                continue;
            }
            switch (src_[i]) {
            case '+': tokens_.push_back({ AstTokKind::Plus, "+", 0.0, false }); break;
            case '-': tokens_.push_back({ AstTokKind::Minus, "-", 0.0, false }); break;
            case '*': tokens_.push_back({ AstTokKind::Star, "*", 0.0, false }); break;
            case '/': tokens_.push_back({ AstTokKind::Slash, "/", 0.0, false }); break;
            case '^': tokens_.push_back({ AstTokKind::Caret, "^", 0.0, false }); break;
            case '(': tokens_.push_back({ AstTokKind::LParen, "(", 0.0, false }); break;
            case ')': tokens_.push_back({ AstTokKind::RParen, ")", 0.0, false }); break;
            case ',': tokens_.push_back({ AstTokKind::Comma, ",", 0.0, false }); break;
            case '=': tokens_.push_back({ AstTokKind::Equal, "=", 0.0, false }); break;
            case '@': tokens_.push_back({ AstTokKind::Angle, "@", 0.0, false }); break;
            case '<': tokens_.push_back({ AstTokKind::Angle, "<", 0.0, false }); break;
            case '|':
                tokens_.push_back({ AstTokKind::Pipe, "|", 0.0, false, false });
                afterPipe = true;
                break;
            default:
                tokErr_ = true;
                tokErrMsg_ = std::string("Caracter no soportado: ") + src_[i];
                return;
            }
            ++i;
        }
        tokens_.push_back({ AstTokKind::End, "", 0.0, false });
    }

    bool ParseEquationOrConversion(MathJsonNode& out, std::string& outErr) {
        MathJsonNode lhs;
        if (!ParseConversion(lhs, outErr)) return false;
        if (Peek().kind == AstTokKind::Equal) {
            Advance();
            MathJsonNode rhs;
            if (!ParseConversion(rhs, outErr)) return false;
            out = MathJsonNode::MakeFunction("Equal", { std::move(lhs), std::move(rhs) });
            return true;
        }
        out = std::move(lhs);
        return true;
    }

    bool ParseConversion(MathJsonNode& out, std::string& outErr) {
        if (!ParseAddSub(out, outErr)) return false;
        if (Peek().kind == AstTokKind::Pipe) {
            Advance();
            std::string unitTarget;
            while (Peek().kind != AstTokKind::End && Peek().kind != AstTokKind::Equal) {
                unitTarget += Advance().text;
            }
            unitTarget = Trim(unitTarget);
            UnitDim d{};
            double sc = 1.0, off = 0.0;
            if (!TryParseUnitExpression(unitTarget, d, sc, off, &outErr)) return false;
            out = MathJsonNode::MakeFunction("ConvertUnit", { std::move(out), MathJsonNode::MakeString(unitTarget) });
        }
        return true;
    }

    bool ParseAddSub(MathJsonNode& out, std::string& outErr) {
        if (!ParseParallel(out, outErr)) return false;
        while (Peek().kind == AstTokKind::Plus || Peek().kind == AstTokKind::Minus) {
            const AstTokKind op = Advance().kind;
            MathJsonNode rhs;
            if (!ParseParallel(rhs, outErr)) return false;
            if (op == AstTokKind::Plus) {
                out = MathJsonNode::MakeFunction("Add", { std::move(out), std::move(rhs) });
            } else {
                out = MathJsonNode::MakeFunction("Subtract", { std::move(out), std::move(rhs) });
            }
        }
        return true;
    }

    bool ParseParallel(MathJsonNode& out, std::string& outErr) {
        if (!ParseMulDiv(out, outErr)) return false;
        while (Peek().kind == AstTokKind::Parallel) {
            Advance();
            MathJsonNode rhs;
            if (!ParseMulDiv(rhs, outErr)) return false;
            out = MathJsonNode::MakeFunction("Parallel", { std::move(out), std::move(rhs) });
        }
        return true;
    }

    bool ShouldImplicitMultiplyNext() const {
        const AstTokKind k = Peek().kind;
        return k == AstTokKind::Ident || k == AstTokKind::LParen;
    }

    static bool IsPureUnitIdent(const std::string& tok) {
        if (tok == "pi" || tok == "e" || tok == "i" || tok == "j" ||
            tok == "g0" || tok == "c0" || tok == "eps0" || tok == "mu0") {
            return false;
        }
        UnitDim d{};
        double s = 1.0, o = 0.0;
        return TryParseUnitExpression(tok, d, s, o);
    }

    bool IsUnboundUnitIdent(const std::string& tok) const {
        if (boundVars_ && boundVars_->find(tok) != boundVars_->end()) return false;
        return IsPureUnitIdent(tok);
    }

    bool ParseMulDiv(MathJsonNode& out, std::string& outErr) {
        if (!ParsePower(out, outErr)) return false;
        while (true) {
            const AstTokKind k = Peek().kind;
            if (k == AstTokKind::Star || k == AstTokKind::Slash) {
                Advance();
                const bool denomCanBindUnits = (k == AstTokKind::Slash) &&
                    (Peek().kind == AstTokKind::Number ||
                     (Peek().kind == AstTokKind::Ident && Peek(1).kind != AstTokKind::LParen && IsUnboundUnitIdent(Peek().text)));
                MathJsonNode rhs;
                if (!ParsePower(rhs, outErr)) return false;
                if (denomCanBindUnits) {
                    while (true) {
                        if (Peek().kind == AstTokKind::Ident && Peek(1).kind != AstTokKind::LParen &&
                            IsUnboundUnitIdent(Peek().text)) {
                            MathJsonNode unitFactor;
                            if (!ParsePower(unitFactor, outErr)) return false;
                            rhs = MathJsonNode::MakeFunction("Multiply", { std::move(rhs), std::move(unitFactor) });
                        } else if (Peek().kind == AstTokKind::Star && Peek().isHyphenUnitSep &&
                                   Peek(1).kind == AstTokKind::Ident && Peek(2).kind != AstTokKind::LParen &&
                                   IsUnboundUnitIdent(Peek(1).text)) {
                            Advance();
                            MathJsonNode unitFactor;
                            if (!ParsePower(unitFactor, outErr)) return false;
                            rhs = MathJsonNode::MakeFunction("Multiply", { std::move(rhs), std::move(unitFactor) });
                        } else {
                            break;
                        }
                    }
                }
                if (k == AstTokKind::Star) {
                    out = MathJsonNode::MakeFunction("Multiply", { std::move(out), std::move(rhs) });
                } else {
                    out = MathJsonNode::MakeFunction("Divide", { std::move(out), std::move(rhs) });
                }
            } else if (ShouldImplicitMultiplyNext()) {
                MathJsonNode rhs;
                if (!ParsePower(rhs, outErr)) return false;
                // Check if (num * unit) -> keep as UnitQuantity if rhs is a pure unit symbol not in math constants
                out = MathJsonNode::MakeFunction("Multiply", { std::move(out), std::move(rhs) });
            } else {
                break;
            }
        }
        return true;
    }

    bool ParsePower(MathJsonNode& out, std::string& outErr) {
        if (!ParsePhasor(out, outErr)) return false;
        if (Peek().kind == AstTokKind::Caret) {
            Advance();
            MathJsonNode expNode;
            if (!ParsePower(expNode, outErr)) return false;
            out = MathJsonNode::MakeFunction("Power", { std::move(out), std::move(expNode) });
        }
        return true;
    }

    bool ParsePhasor(MathJsonNode& out, std::string& outErr) {
        if (!ParseUnary(out, outErr)) return false;
        if (Peek().kind == AstTokKind::Angle) {
            Advance();
            MathJsonNode ang;
            if (!ParseUnary(ang, outErr)) return false;
            out = MathJsonNode::MakeFunction("Phasor", { std::move(out), std::move(ang) });
        }
        return true;
    }

    bool ParseUnary(MathJsonNode& out, std::string& outErr) {
        if (Peek().kind == AstTokKind::Plus) {
            Advance();
            return ParseUnary(out, outErr);
        }
        if (Peek().kind == AstTokKind::Minus) {
            Advance();
            MathJsonNode inner;
            if (!ParseUnary(inner, outErr)) return false;
            if (inner.kind == MathJsonKind::Number) {
                out = MathJsonNode::MakeNumber(-inner.realVal);
            } else if (inner.kind == MathJsonKind::Complex) {
                out = MathJsonNode::MakeComplex(-inner.realVal, -inner.imagVal);
            } else {
                out = MathJsonNode::MakeFunction("Negate", { std::move(inner) });
            }
            return true;
        }
        return ParsePrimary(out, outErr);
    }

    bool ParsePrimary(MathJsonNode& out, std::string& outErr) {
        const AstTok t = Peek();
        if (t.kind == AstTokKind::LParen) {
            Advance();
            if (!ParseAddSub(out, outErr)) return false;
            if (Peek().kind != AstTokKind::RParen) {
                outErr = "Falta parentesis de cierre ')'";
                return false;
            }
            Advance();
            return true;
        }
        if (t.kind == AstTokKind::Number) {
            Advance();
            if (t.engSuffix != 0) {
                out = MathJsonNode::MakeFunction("EngSuffix", {
                    MathJsonNode::MakeNumber(t.numVal),
                    MathJsonNode::MakeSymbol(std::string(1, t.engSuffix))
                });
                return true;
            }
            out = t.imagSuffix ? MathJsonNode::MakeComplex(0.0, t.numVal) : MathJsonNode::MakeNumber(t.numVal);
            return true;
        }
        if (t.kind == AstTokKind::Ident) {
            Advance();
            if (Peek().kind == AstTokKind::LParen) {
                Advance();
                std::vector<MathJsonNode> args;
                if (Peek().kind != AstTokKind::RParen) {
                    while (true) {
                        MathJsonNode arg;
                        if (!ParseAddSub(arg, outErr)) return false;
                        args.push_back(std::move(arg));
                        if (Peek().kind == AstTokKind::Comma) { Advance(); continue; }
                        break;
                    }
                }
                if (Peek().kind != AstTokKind::RParen) {
                    outErr = "Falta ')' en funcion " + t.text;
                    return false;
                }
                Advance();
                out = MathJsonNode::MakeFunction(CanonicalFnHead(t.text), std::move(args));
                return true;
            }
            out = MathJsonNode::MakeSymbol(t.text);
            return true;
        }
        outErr = "Se esperaba valor, variable o parentesis cerca de: " + t.text;
        return false;
    }
};

inline json MathJsonNodeToJsonObj(const MathJsonNode& n) {
    switch (n.kind) {
    case MathJsonKind::Number:
        return json(n.realVal);
    case MathJsonKind::Complex:
        return json::array({ "Complex", n.realVal, n.imagVal });
    case MathJsonKind::Symbol:
        return json(n.text);
    case MathJsonKind::String:
        return json::object({ { "str", n.text } });
    case MathJsonKind::Function: {
        json arr = json::array();
        arr.push_back(n.text);
        for (const auto& a : n.args) arr.push_back(MathJsonNodeToJsonObj(a));
        return arr;
    }
    }
    return json(0);
}

inline bool JsonObjToMathJsonNode(const json& j, MathJsonNode& out, std::string* outErr) {
    if (j.is_number()) {
        out = MathJsonNode::MakeNumber(j.get<double>());
        return true;
    }
    if (j.is_string()) {
        const std::string s = j.get<std::string>();
        if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') {
            out = MathJsonNode::MakeString(s.substr(1, s.size() - 2));
        } else {
            out = MathJsonNode::MakeSymbol(s);
        }
        return true;
    }
    if (j.is_object()) {
        if (j.contains("str") && j["str"].is_string()) {
            out = MathJsonNode::MakeString(j["str"].get<std::string>());
            return true;
        }
        if (j.contains("num") && j["num"].is_string()) {
            try {
                out = MathJsonNode::MakeNumber(std::stod(j["num"].get<std::string>()));
                return true;
            } catch (...) {}
        }
        if (j.contains("fn") && j["fn"].is_array() && !j["fn"].empty()) {
            return JsonObjToMathJsonNode(j["fn"], out, outErr);
        }
        if (outErr) *outErr = "Objeto MathJSON no reconocido";
        return false;
    }
    if (j.is_array()) {
        if (j.empty() || !j[0].is_string()) {
            if (outErr) *outErr = "Expresion MathJSON debe iniciar con nombre de funcion";
            return false;
        }
        const std::string head = j[0].get<std::string>();
        if (head == "Complex" && j.size() == 3 && j[1].is_number() && j[2].is_number()) {
            out = MathJsonNode::MakeComplex(j[1].get<double>(), j[2].get<double>());
            return true;
        }
        std::vector<MathJsonNode> args;
        args.reserve(j.size() - 1);
        for (std::size_t i = 1; i < j.size(); ++i) {
            MathJsonNode child;
            if (!JsonObjToMathJsonNode(j[i], child, outErr)) return false;
            args.push_back(std::move(child));
        }
        out = MathJsonNode::MakeFunction(head, std::move(args));
        return true;
    }
    if (outErr) *outErr = "Tipo JSON invalido en MathJSON";
    return false;
}

inline int NodePrecedence(const MathJsonNode& n) {
    if (n.kind != MathJsonKind::Function) return 100;
    if (n.text == "Equal") return 5;
    if (n.text == "ConvertUnit") return 8;
    if (n.text == "Add" || n.text == "Subtract") return 10;
    if (n.text == "Parallel") return 15;
    if (n.text == "Multiply" || n.text == "Divide" || n.text == "UnitQuantity") return 20;
    if (n.text == "Negate") return 25;
    if (n.text == "Phasor") return 30;
    if (n.text == "Power") return 40;
    return 90;
}

inline std::string EmitExprRec(const MathJsonNode& n, int parentPrec = 0) {
    if (n.kind == MathJsonKind::Number) {
        const std::string s = FmtNum(n.realVal, 10);
        return (n.realVal < 0.0 && parentPrec > 10) ? ("(" + s + ")") : s;
    }
    if (n.kind == MathJsonKind::Complex) {
        std::ostringstream oss;
        if (std::abs(n.realVal) < 1e-13) {
            oss << FmtNum(n.imagVal, 10) << "j";
            return (n.imagVal < 0.0 && parentPrec > 10) ? ("(" + oss.str() + ")") : oss.str();
        }
        oss << FmtNum(n.realVal, 10) << (n.imagVal >= 0.0 ? " + " : " - ") << FmtNum(std::abs(n.imagVal), 10) << "j";
        return (parentPrec > 10) ? ("(" + oss.str() + ")") : oss.str();
    }
    if (n.kind == MathJsonKind::Symbol) return n.text;
    if (n.kind == MathJsonKind::String) return n.text;

    const int myPrec = NodePrecedence(n);
    auto wrap = [&](const std::string& body) {
        return (myPrec < parentPrec) ? ("(" + body + ")") : body;
    };

    if (n.text == "Equal" && n.args.size() == 2) {
        return EmitExprRec(n.args[0], 0) + " = " + EmitExprRec(n.args[1], 0);
    }
    if (n.text == "ConvertUnit" && n.args.size() == 2) {
        return EmitExprRec(n.args[0], 9) + " | " + EmitExprRec(n.args[1], 0);
    }
    if (n.text == "UnitQuantity" && n.args.size() == 2) {
        return wrap(EmitExprRec(n.args[0], 21) + " " + EmitExprRec(n.args[1], 0));
    }
    if (n.text == "EngSuffix" && n.args.size() == 2 && n.args[0].kind == MathJsonKind::Number && n.args[1].kind == MathJsonKind::Symbol) {
        return FmtNum(n.args[0].realVal, 10) + n.args[1].text;
    }
    if (n.text == "HyphenChoice" && !n.args.empty()) {
        return EmitExprRec(n.args[0], parentPrec);
    }
    if (n.text == "Negate" && n.args.size() == 1) {
        return wrap("-" + EmitExprRec(n.args[0], 26));
    }
    if (n.text == "Add" && !n.args.empty()) {
        std::string out;
        for (std::size_t i = 0; i < n.args.size(); ++i) {
            std::string term = EmitExprRec(n.args[i], 10);
            if (i == 0) {
                out = term;
            } else if (!term.empty() && term[0] == '-') {
                out += " - " + term.substr(1);
            } else {
                out += " + " + term;
            }
        }
        return wrap(out);
    }
    if (n.text == "Subtract" && n.args.size() == 2) {
        return wrap(EmitExprRec(n.args[0], 10) + " - " + EmitExprRec(n.args[1], 11));
    }
    if (n.text == "Parallel" && n.args.size() == 2) {
        return wrap(EmitExprRec(n.args[0], 15) + " || " + EmitExprRec(n.args[1], 16));
    }
    if (n.text == "Multiply" && !n.args.empty()) {
        std::string out;
        for (std::size_t i = 0; i < n.args.size(); ++i) {
            if (i > 0) out += "*";
            out += EmitExprRec(n.args[i], 20);
        }
        return wrap(out);
    }
    if (n.text == "Divide" && n.args.size() == 2) {
        return wrap(EmitExprRec(n.args[0], 20) + "/" + EmitExprRec(n.args[1], 21));
    }
    if (n.text == "Power" && n.args.size() == 2) {
        return wrap(EmitExprRec(n.args[0], 41) + "^" + EmitExprRec(n.args[1], 40));
    }
    if (n.text == "Phasor" && n.args.size() == 2) {
        return wrap(EmitExprRec(n.args[0], 31) + " @ " + EmitExprRec(n.args[1], 31));
    }
    std::string out = HeadToCasFn(n.text) + "(";
    for (std::size_t i = 0; i < n.args.size(); ++i) {
        if (i > 0) out += ", ";
        out += EmitExprRec(n.args[i], 0);
    }
    out += ")";
    return out;
}

inline MathJsonNode FlattenAndFoldOnce(const MathJsonNode& node, int maxExpandTerms, bool allowExpand);

struct MonomialKey {
    std::complex<double> coeff{ 1.0, 0.0 };
    std::vector<std::pair<std::string, MathJsonNode>> factors; // canonical sorted non-numeric factors
    std::string factorSignature;
};

inline std::string NodeKey(const MathJsonNode& n) {
    if (n.kind == MathJsonKind::Number) return "N(" + FmtNum(n.realVal, 12) + ")";
    if (n.kind == MathJsonKind::Complex) return "C(" + FmtNum(n.realVal, 12) + "," + FmtNum(n.imagVal, 12) + ")";
    if (n.kind == MathJsonKind::Symbol) return "S(" + n.text + ")";
    if (n.kind == MathJsonKind::String) return "T(" + n.text + ")";
    std::string out = "F(" + n.text;
    for (const auto& a : n.args) out += "," + NodeKey(a);
    return out + ")";
}

inline MathJsonNode BuildProductFromCoeffAndFactors(
    std::complex<double> coeff,
    const std::vector<MathJsonNode>& factors)
{
    if (std::abs(coeff) < 1e-13) return MathJsonNode::MakeNumber(0.0);
    std::vector<MathJsonNode> items;
    if (std::abs(coeff - std::complex<double>(1.0, 0.0)) > 1e-12 || factors.empty()) {
        items.push_back(MakeNumOrComplex(coeff));
    }
    for (const auto& f : factors) items.push_back(f);
    if (items.size() == 1) return items[0];
    return MathJsonNode::MakeFunction("Multiply", std::move(items));
}

inline MathJsonNode DistributeTwoAdds(const MathJsonNode& a, const MathJsonNode& b, int maxExpandTerms) {
    const std::vector<MathJsonNode> termsA = (a.kind == MathJsonKind::Function && a.text == "Add") ? a.args : std::vector<MathJsonNode>{ a };
    const std::vector<MathJsonNode> termsB = (b.kind == MathJsonKind::Function && b.text == "Add") ? b.args : std::vector<MathJsonNode>{ b };
    if (static_cast<int>(termsA.size() * termsB.size()) > maxExpandTerms) {
        return MathJsonNode::MakeFunction("Multiply", { a, b });
    }
    std::vector<MathJsonNode> sumTerms;
    sumTerms.reserve(termsA.size() * termsB.size());
    for (const auto& ta : termsA) {
        for (const auto& tb : termsB) {
            sumTerms.push_back(FlattenAndFoldOnce(MathJsonNode::MakeFunction("Multiply", { ta, tb }), maxExpandTerms, true));
        }
    }
    return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Add", std::move(sumTerms)), maxExpandTerms, true);
}

inline MathJsonNode FlattenAndFoldOnce(const MathJsonNode& node, int maxExpandTerms, bool allowExpand) {
    if (node.kind != MathJsonKind::Function) {
        if (node.kind == MathJsonKind::Complex && std::abs(node.imagVal) < 1e-13) {
            return MathJsonNode::MakeNumber(node.realVal);
        }
        return node;
    }

    std::vector<MathJsonNode> args;
    args.reserve(node.args.size());
    for (const auto& a : node.args) {
        args.push_back(FlattenAndFoldOnce(a, maxExpandTerms, allowExpand));
    }

    const std::string& h = node.text;
    if (h == "HyphenChoice" && !args.empty()) {
        return args[0];
    }
    if (h == "EngSuffix" && args.size() == 2) {
        return MathJsonNode::MakeFunction(h, std::move(args));
    }
    if (h == "Equal" && args.size() == 2) {
        return MathJsonNode::MakeFunction("Equal", std::move(args));
    }
    if (h == "Negate" && args.size() == 1) {
        std::complex<double> z{};
        if (IsComplexOrNum(args[0], z)) return MakeNumOrComplex(-z);
        return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Multiply", { MathJsonNode::MakeNumber(-1.0), args[0] }), maxExpandTerms, allowExpand);
    }
    if (h == "Subtract" && args.size() == 2) {
        auto negB = MathJsonNode::MakeFunction("Multiply", { MathJsonNode::MakeNumber(-1.0), args[1] });
        return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Add", { args[0], negB }), maxExpandTerms, allowExpand);
    }
    if (h == "Divide" && args.size() == 2) {
        std::complex<double> numZ{}, denZ{};
        if (IsComplexOrNum(args[0], numZ) && IsComplexOrNum(args[1], denZ) && std::abs(denZ) > 1e-15) {
            return MakeNumOrComplex(numZ / denZ);
        }
        if (IsComplexOrNum(args[1], denZ) && std::abs(denZ) > 1e-15) {
            if (std::abs(denZ - std::complex<double>(1.0, 0.0)) < 1e-13) return args[0];
            return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Multiply", { MakeNumOrComplex(1.0 / denZ), args[0] }), maxExpandTerms, allowExpand);
        }
        if (NodeKey(args[0]) == NodeKey(args[1]) && !IsZero(args[1])) {
            return MathJsonNode::MakeNumber(1.0);
        }
        // a / b -> a * b^(-1) when helpful for canonicalization, OR keep Divide if non-numeric denom
        auto inv = FlattenAndFoldOnce(MathJsonNode::MakeFunction("Power", { args[1], MathJsonNode::MakeNumber(-1.0) }), maxExpandTerms, allowExpand);
        return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Multiply", { args[0], inv }), maxExpandTerms, allowExpand);
    }
    if (h == "Power" && args.size() == 2) {
        double p = 0.0;
        if (IsZero(args[1])) return MathJsonNode::MakeNumber(1.0);
        if (IsOne(args[1])) return args[0];
        if (IsOne(args[0])) return MathJsonNode::MakeNumber(1.0);
        std::complex<double> bz{}, ez{};
        if (IsComplexOrNum(args[0], bz) && IsComplexOrNum(args[1], ez)) {
            if (std::abs(bz) > 1e-15 || ez.real() > 0.0) {
                return MakeNumOrComplex(std::pow(bz, ez));
            }
        }
        if (args[0].kind == MathJsonKind::Function && args[0].text == "Power" && args[0].args.size() == 2) {
            double p1 = 0.0, p2 = 0.0;
            if (IsNum(args[0].args[1], p1) && IsNum(args[1], p2) &&
                std::abs(p1 - std::round(p1)) < 1e-11 && std::abs(p2 - std::round(p2)) < 1e-11) {
                return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Power", { args[0].args[0], MathJsonNode::MakeNumber(p1 * p2) }), maxExpandTerms, allowExpand);
            }
        }
        if (args[0].kind == MathJsonKind::Function && args[0].text == "Multiply" &&
            IsNum(args[1], p) && std::abs(p - std::round(p)) < 1e-11) {
            std::vector<MathJsonNode> pw;
            for (const auto& f : args[0].args) {
                pw.push_back(MathJsonNode::MakeFunction("Power", { f, args[1] }));
            }
            return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Multiply", std::move(pw)), maxExpandTerms, allowExpand);
        }
        if (allowExpand && args[0].kind == MathJsonKind::Function && args[0].text == "Add" &&
            IsNum(args[1], p) && p >= 2.0 && p <= 6.0 && std::abs(p - std::round(p)) < 1e-11) {
            const int ip = static_cast<int>(std::llround(p));
            if (static_cast<int>(std::pow(static_cast<double>(args[0].args.size()), ip)) <= maxExpandTerms) {
                MathJsonNode acc = args[0];
                for (int k = 1; k < ip; ++k) {
                    acc = DistributeTwoAdds(acc, args[0], maxExpandTerms);
                }
                return acc;
            }
        }
        return MathJsonNode::MakeFunction("Power", std::move(args));
    }

    if (h == "Multiply") {
        std::complex<double> numCoeff(1.0, 0.0);
        std::vector<MathJsonNode> factors;
        for (const auto& a : args) {
            if (a.kind == MathJsonKind::Function && a.text == "Multiply") {
                for (const auto& sub : a.args) {
                    std::complex<double> z{};
                    if (IsComplexOrNum(sub, z)) numCoeff *= z;
                    else factors.push_back(sub);
                }
            } else {
                std::complex<double> z{};
                if (IsComplexOrNum(a, z)) numCoeff *= z;
                else factors.push_back(a);
            }
        }
        if (std::abs(numCoeff) < 1e-13) return MathJsonNode::MakeNumber(0.0);

        // Combine identical base powers: x * x -> x^2, x^2 * x^-1 -> x
        struct PowGroup { MathJsonNode base; double expNum = 0.0; bool isNumExp = true; MathJsonNode rawExp; };
        std::vector<PowGroup> groups;
        for (const auto& f : factors) {
            MathJsonNode base = f;
            double eNum = 1.0;
            bool numExp = true;
            MathJsonNode rawExp = MathJsonNode::MakeNumber(1.0);
            if (f.kind == MathJsonKind::Function && f.text == "Power" && f.args.size() == 2) {
                base = f.args[0];
                rawExp = f.args[1];
                numExp = IsNum(f.args[1], eNum);
            }
            const std::string bKey = NodeKey(base);
            bool merged = false;
            for (auto& g : groups) {
                if (g.isNumExp && numExp && NodeKey(g.base) == bKey) {
                    g.expNum += eNum;
                    merged = true;
                    break;
                }
            }
            if (!merged) groups.push_back({ base, eNum, numExp, rawExp });
        }

        std::vector<MathJsonNode> mergedFactors;
        for (const auto& g : groups) {
            if (g.isNumExp) {
                if (std::abs(g.expNum) < 1e-13) continue;
                if (std::abs(g.expNum - 1.0) < 1e-13) mergedFactors.push_back(g.base);
                else mergedFactors.push_back(MathJsonNode::MakeFunction("Power", { g.base, MathJsonNode::MakeNumber(g.expNum) }));
            } else {
                mergedFactors.push_back(MathJsonNode::MakeFunction("Power", { g.base, g.rawExp }));
            }
        }

        // Distributive expansion if an Add factor is present and term count <= maxExpandTerms
        if (allowExpand) {
            for (std::size_t idx = 0; idx < mergedFactors.size(); ++idx) {
                if (mergedFactors[idx].kind == MathJsonKind::Function && mergedFactors[idx].text == "Add") {
                    const auto& sumTerms = mergedFactors[idx].args;
                    if (static_cast<int>(sumTerms.size()) <= maxExpandTerms) {
                        std::vector<MathJsonNode> rest;
                        rest.push_back(MakeNumOrComplex(numCoeff));
                        for (std::size_t j = 0; j < mergedFactors.size(); ++j) {
                            if (j != idx) rest.push_back(mergedFactors[j]);
                        }
                        std::vector<MathJsonNode> expanded;
                        for (const auto& st : sumTerms) {
                            std::vector<MathJsonNode> termFactors = rest;
                            termFactors.push_back(st);
                            expanded.push_back(MathJsonNode::MakeFunction("Multiply", std::move(termFactors)));
                        }
                        return FlattenAndFoldOnce(MathJsonNode::MakeFunction("Add", std::move(expanded)), maxExpandTerms, true);
                    }
                }
            }
        }

        std::sort(mergedFactors.begin(), mergedFactors.end(), [](const MathJsonNode& a, const MathJsonNode& b) {
            return NodeKey(a) < NodeKey(b);
        });
        return BuildProductFromCoeffAndFactors(numCoeff, mergedFactors);
    }

    if (h == "Add") {
        std::vector<MathJsonNode> flat;
        for (const auto& a : args) {
            if (a.kind == MathJsonKind::Function && a.text == "Add") {
                for (const auto& sub : a.args) flat.push_back(sub);
            } else {
                flat.push_back(a);
            }
        }

        std::complex<double> constSum(0.0, 0.0);
        struct TermGroup {
            std::complex<double> coeff{ 0.0, 0.0 };
            std::vector<MathJsonNode> factors;
            std::string key;
        };
        std::vector<TermGroup> groups;

        for (const auto& t : flat) {
            std::complex<double> z{};
            if (IsComplexOrNum(t, z)) {
                constSum += z;
                continue;
            }
            std::complex<double> tc(1.0, 0.0);
            std::vector<MathJsonNode> tf;
            if (t.kind == MathJsonKind::Function && t.text == "Multiply") {
                for (const auto& f : t.args) {
                    if (IsComplexOrNum(f, z)) tc *= z;
                    else tf.push_back(f);
                }
            } else {
                tf.push_back(t);
            }
            std::sort(tf.begin(), tf.end(), [](const MathJsonNode& a, const MathJsonNode& b) {
                return NodeKey(a) < NodeKey(b);
            });
            std::string key;
            for (const auto& f : tf) key += NodeKey(f) + "|";
            bool found = false;
            for (auto& g : groups) {
                if (g.key == key) {
                    g.coeff += tc;
                    found = true;
                    break;
                }
            }
            if (!found) groups.push_back({ tc, std::move(tf), key });
        }

        // Trig Pythagorean identity: c*Sin(u)^2 + c*Cos(u)^2 -> c
        for (std::size_t i = 0; i < groups.size(); ++i) {
            if (std::abs(groups[i].coeff) < 1e-13) continue;
            auto isTrigSq = [](const std::vector<MathJsonNode>& fs, const char* fnName, MathJsonNode& outArg) -> bool {
                double p = 0.0;
                if (fs.size() != 1) return false;
                const auto& f = fs[0];
                if (f.kind == MathJsonKind::Function && f.text == "Power" && f.args.size() == 2 &&
                    IsNum(f.args[1], p) && std::abs(p - 2.0) < 1e-12 &&
                    f.args[0].kind == MathJsonKind::Function && f.args[0].text == fnName && f.args[0].args.size() == 1) {
                    outArg = f.args[0].args[0];
                    return true;
                }
                return false;
            };
            MathJsonNode uSin;
            if (isTrigSq(groups[i].factors, "Sin", uSin)) {
                const std::string uKey = NodeKey(uSin);
                for (std::size_t j = 0; j < groups.size(); ++j) {
                    if (i == j || std::abs(groups[j].coeff) < 1e-13) continue;
                    MathJsonNode uCos;
                    if (isTrigSq(groups[j].factors, "Cos", uCos) && NodeKey(uCos) == uKey &&
                        std::abs(groups[i].coeff - groups[j].coeff) < 1e-12) {
                        constSum += groups[i].coeff;
                        groups[i].coeff = { 0.0, 0.0 };
                        groups[j].coeff = { 0.0, 0.0 };
                        break;
                    }
                }
            }
        }

        std::vector<MathJsonNode> finalTerms;
        if (std::abs(constSum) > 1e-13) {
            finalTerms.push_back(MakeNumOrComplex(constSum));
        }
        std::sort(groups.begin(), groups.end(), [](const TermGroup& a, const TermGroup& b) {
            return a.key < b.key;
        });
        for (const auto& g : groups) {
            if (std::abs(g.coeff) < 1e-13) continue;
            finalTerms.push_back(BuildProductFromCoeffAndFactors(g.coeff, g.factors));
        }
        if (finalTerms.empty()) return MathJsonNode::MakeNumber(0.0);
        if (finalTerms.size() == 1) return finalTerms[0];
        return MathJsonNode::MakeFunction("Add", std::move(finalTerms));
    }

    // Trig parity & constant folding for unary functions
    if (args.size() == 1) {
        if (h == "Sin" || h == "Cos" || h == "Tan") {
            double c0 = 0.0;
            if (args[0].kind == MathJsonKind::Function && args[0].text == "Multiply" &&
                !args[0].args.empty() && IsNum(args[0].args[0], c0) && c0 < 0.0) {
                std::vector<MathJsonNode> posArgs = args[0].args;
                posArgs[0] = MathJsonNode::MakeNumber(-c0);
                auto posInner = FlattenAndFoldOnce(MathJsonNode::MakeFunction("Multiply", std::move(posArgs)), maxExpandTerms, allowExpand);
                auto fNode = MathJsonNode::MakeFunction(h, { posInner });
                if (h == "Cos") return fNode;
                return MathJsonNode::MakeFunction("Multiply", { MathJsonNode::MakeNumber(-1.0), fNode });
            }
        }
        if (h == "Exp" && IsZero(args[0])) return MathJsonNode::MakeNumber(1.0);
        if (h == "Ln" && IsOne(args[0])) return MathJsonNode::MakeNumber(0.0);
        if (h == "Sqrt") {
            double v = 0.0;
            if (IsNum(args[0], v) && v >= 0.0) {
                const double r = std::sqrt(v);
                if (std::abs(r - std::round(r)) < 1e-12) return MathJsonNode::MakeNumber(std::round(r));
            }
        }
    }

    return MathJsonNode::MakeFunction(h, std::move(args));
}

inline MathJsonNode RunCanonicalLoop(const MathJsonNode& ast, int maxExpandTerms, bool allowExpand) {
    MathJsonNode cur = ast;
    for (int pass = 0; pass < 8; ++pass) {
        MathJsonNode next = FlattenAndFoldOnce(cur, maxExpandTerms, allowExpand);
        if (NodeKey(next) == NodeKey(cur)) break;
        cur = std::move(next);
    }
    return cur;
}

inline bool DiffRec(const MathJsonNode& n, const std::string& v, MathJsonNode& out, std::string& err) {
    if (n.kind == MathJsonKind::Number || n.kind == MathJsonKind::Complex || n.kind == MathJsonKind::String) {
        out = MathJsonNode::MakeNumber(0.0);
        return true;
    }
    if (n.kind == MathJsonKind::Symbol) {
        out = MathJsonNode::MakeNumber(n.text == v ? 1.0 : 0.0);
        return true;
    }
    const std::string& h = n.text;
    if (h == "EngSuffix" && n.args.size() == 2) {
        if (n.args[1].kind == MathJsonKind::Symbol && n.args[1].text == v) {
            out = n.args[0];
        } else {
            out = MathJsonNode::MakeNumber(0.0);
        }
        return true;
    }
    if (h == "Negate" && n.args.size() == 1) {
        MathJsonNode d;
        if (!DiffRec(n.args[0], v, d, err)) return false;
        out = MathJsonNode::MakeFunction("Negate", { std::move(d) });
        return true;
    }
    if (h == "Add") {
        std::vector<MathJsonNode> ds;
        for (const auto& a : n.args) {
            MathJsonNode d;
            if (!DiffRec(a, v, d, err)) return false;
            ds.push_back(std::move(d));
        }
        out = MathJsonNode::MakeFunction("Add", std::move(ds));
        return true;
    }
    if (h == "Subtract" && n.args.size() == 2) {
        MathJsonNode da, db;
        if (!DiffRec(n.args[0], v, da, err) || !DiffRec(n.args[1], v, db, err)) return false;
        out = MathJsonNode::MakeFunction("Subtract", { std::move(da), std::move(db) });
        return true;
    }
    if (h == "Multiply" && !n.args.empty()) {
        if (n.args.size() == 1) return DiffRec(n.args[0], v, out, err);
        // Product rule over args[0] * rest
        MathJsonNode rest = (n.args.size() == 2)
            ? n.args[1]
            : MathJsonNode::MakeFunction("Multiply", std::vector<MathJsonNode>(n.args.begin() + 1, n.args.end()));
        MathJsonNode da, dr;
        if (!DiffRec(n.args[0], v, da, err) || !DiffRec(rest, v, dr, err)) return false;
        out = MathJsonNode::MakeFunction("Add", {
            MathJsonNode::MakeFunction("Multiply", { std::move(da), rest }),
            MathJsonNode::MakeFunction("Multiply", { n.args[0], std::move(dr) })
        });
        return true;
    }
    if (h == "Divide" && n.args.size() == 2) {
        const auto& f = n.args[0];
        const auto& g = n.args[1];
        MathJsonNode df, dg;
        if (!DiffRec(f, v, df, err) || !DiffRec(g, v, dg, err)) return false;
        auto num = MathJsonNode::MakeFunction("Subtract", {
            MathJsonNode::MakeFunction("Multiply", { std::move(df), g }),
            MathJsonNode::MakeFunction("Multiply", { f, std::move(dg) })
        });
        auto den = MathJsonNode::MakeFunction("Power", { g, MathJsonNode::MakeNumber(2.0) });
        out = MathJsonNode::MakeFunction("Divide", { std::move(num), std::move(den) });
        return true;
    }
    if (h == "Power" && n.args.size() == 2) {
        const auto& u = n.args[0];
        const auto& p = n.args[1];
        MathJsonNode du, dp;
        if (!DiffRec(u, v, du, err) || !DiffRec(p, v, dp, err)) return false;
        if (IsZero(dp)) {
            // d(u^c)/dx = c * u^(c-1) * du
            auto pm1 = MathJsonNode::MakeFunction("Subtract", { p, MathJsonNode::MakeNumber(1.0) });
            out = MathJsonNode::MakeFunction("Multiply", { p, MathJsonNode::MakeFunction("Power", { u, pm1 }), du });
            return true;
        }
        // General u^p: u^p * (dp * ln(u) + p * du / u)
        auto term1 = MathJsonNode::MakeFunction("Multiply", { dp, MathJsonNode::MakeFunction("Ln", { u }) });
        auto term2 = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeFunction("Multiply", { p, du }), u });
        out = MathJsonNode::MakeFunction("Multiply", { n, MathJsonNode::MakeFunction("Add", { term1, term2 }) });
        return true;
    }
    if (n.args.size() == 1) {
        const auto& u = n.args[0];
        MathJsonNode du;
        if (!DiffRec(u, v, du, err)) return false;
        MathJsonNode df;
        if (h == "Sin") df = MathJsonNode::MakeFunction("Cos", { u });
        else if (h == "Cos") df = MathJsonNode::MakeFunction("Negate", { MathJsonNode::MakeFunction("Sin", { u }) });
        else if (h == "Tan") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Power", { MathJsonNode::MakeFunction("Cos", { u }), MathJsonNode::MakeNumber(2.0) }) });
        else if (h == "Sinh") df = MathJsonNode::MakeFunction("Cosh", { u });
        else if (h == "Cosh") df = MathJsonNode::MakeFunction("Sinh", { u });
        else if (h == "Tanh") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Power", { MathJsonNode::MakeFunction("Cosh", { u }), MathJsonNode::MakeNumber(2.0) }) });
        else if (h == "Exp") df = MathJsonNode::MakeFunction("Exp", { u });
        else if (h == "Ln") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(1.0), u });
        else if (h == "Log10") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Multiply", { u, MathJsonNode::MakeFunction("Ln", { MathJsonNode::MakeNumber(10.0) }) }) });
        else if (h == "Sqrt") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Multiply", { MathJsonNode::MakeNumber(2.0), MathJsonNode::MakeFunction("Sqrt", { u }) }) });
        else if (h == "Asin") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Sqrt", { MathJsonNode::MakeFunction("Subtract", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Power", { u, MathJsonNode::MakeNumber(2.0) }) }) }) });
        else if (h == "Acos") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(-1.0), MathJsonNode::MakeFunction("Sqrt", { MathJsonNode::MakeFunction("Subtract", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Power", { u, MathJsonNode::MakeNumber(2.0) }) }) }) });
        else if (h == "Atan") df = MathJsonNode::MakeFunction("Divide", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Add", { MathJsonNode::MakeNumber(1.0), MathJsonNode::MakeFunction("Power", { u, MathJsonNode::MakeNumber(2.0) }) }) });
        else {
            err = "Derivada simbolica no soportada para funcion: " + h;
            return false;
        }
        out = MathJsonNode::MakeFunction("Multiply", { std::move(df), std::move(du) });
        return true;
    }
    err = "Nodo no derivable: " + h;
    return false;
}

inline void CollectFreeSymsRec(
    const MathJsonNode& n,
    const SymbolEnv& boundEnv,
    std::unordered_set<std::string>& seen,
    std::vector<std::string>& out)
{
    static const std::unordered_set<std::string> kBuiltins{
        "pi", "e", "i", "j", "g0", "c0", "eps0", "mu0"
    };
    if (n.kind == MathJsonKind::Symbol) {
        if (kBuiltins.find(n.text) != kBuiltins.end()) return;
        if (boundEnv.find(n.text) != boundEnv.end()) return;
        UnitDim d{}; double sc = 1.0, off = 0.0;
        if (TryParseUnitExpression(n.text, d, sc, off)) return;
        if (seen.insert(n.text).second) out.push_back(n.text);
        return;
    }
    if (n.kind == MathJsonKind::Function) {
        if (n.text == "EngSuffix") return;
        if (n.text == "HyphenChoice" && !n.args.empty()) {
            CollectFreeSymsRec(n.args[0], boundEnv, seen, out);
            return;
        }
        const std::size_t limit = (n.text == "ConvertUnit" && n.args.size() == 2) ? 1 : n.args.size();
        for (std::size_t i = 0; i < limit; ++i) {
            CollectFreeSymsRec(n.args[i], boundEnv, seen, out);
        }
    }
}

} // namespace detail_sym

bool ParseMathJson(const std::string& mathJsonUtf8, MathJsonNode& outAst, std::string* outErr) {
    try {
        const json j = json::parse(mathJsonUtf8);
        return detail_sym::JsonObjToMathJsonNode(j, outAst, outErr);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = std::string("JSON invalido: ") + ex.what();
        return false;
    }
}

std::string ToMathJsonString(const MathJsonNode& ast, bool pretty) {
    const json j = detail_sym::MathJsonNodeToJsonObj(ast);
    return pretty ? j.dump(2) : j.dump();
}

bool ValidateMathJsonAst(const MathJsonNode& ast, std::vector<std::string>& outErrors) {
    outErrors.clear();
    auto check = [&](auto& self, const MathJsonNode& n, bool allowString) -> void {
        if (n.kind == MathJsonKind::Number) {
            if (!std::isfinite(n.realVal)) outErrors.push_back("Numero no finito en AST");
            return;
        }
        if (n.kind == MathJsonKind::Complex) {
            if (!std::isfinite(n.realVal) || !std::isfinite(n.imagVal)) outErrors.push_back("Numero complejo no finito en AST");
            return;
        }
        if (n.kind == MathJsonKind::Symbol) {
            if (detail_sym::Trim(n.text).empty()) outErrors.push_back("Simbolo vacio en AST");
            return;
        }
        if (n.kind == MathJsonKind::String) {
            if (!allowString) outErrors.push_back("Nodo String no permitido fuera de argumento de unidad");
            return;
        }

        if (n.text.empty()) {
            outErrors.push_back("Funcion sin nombre en AST");
            return;
        }
        static const std::unordered_set<std::string> kUnaryOps = {
            "Negate", "Sin", "Cos", "Tan", "Asin", "Acos", "Atan",
            "Sinh", "Cosh", "Tanh", "Asinh", "Acosh", "Atanh",
            "Sqrt", "Exp", "Ln", "Log10", "Abs", "Arg", "Phase",
            "Re", "Im", "Conj", "Floor", "Ceil", "Round",
            "Rad", "Deg", "Sind", "Cosd", "Tand"
        };
        static const std::unordered_set<std::string> kOneOrTwoOps = {
            "Rect", "Polar"
        };
        static const std::unordered_set<std::string> kBinaryOps = {
            "Subtract", "Divide", "Power", "Equal", "Phasor", "Parallel",
            "UnitQuantity", "ConvertUnit", "Atan2", "Pow", "Min", "Max", "Complex", "EngSuffix"
        };
        static const std::unordered_set<std::string> kVariadicOps = {
            "Add", "Multiply"
        };
        const std::size_t ar = n.args.size();
        if (kUnaryOps.find(n.text) != kUnaryOps.end()) {
            if (ar != 1) outErrors.push_back(n.text + " requiere exactamente 1 argumento");
        } else if (kOneOrTwoOps.find(n.text) != kOneOrTwoOps.end()) {
            if (ar != 1 && ar != 2) outErrors.push_back(n.text + " requiere 1 o 2 argumentos");
        } else if (kBinaryOps.find(n.text) != kBinaryOps.end()) {
            if (ar != 2) outErrors.push_back(n.text + " requiere exactamente 2 argumentos");
        } else if (kVariadicOps.find(n.text) != kVariadicOps.end()) {
            if (ar == 0) outErrors.push_back(n.text + " requiere al menos 1 argumento");
        } else if (n.text == "HyphenChoice") {
            if (ar < 3) outErrors.push_back("HyphenChoice requiere al menos 3 argumentos");
            for (std::size_t idx = 2; idx < ar; ++idx) {
                if (n.args[idx].kind != MathJsonKind::Symbol) {
                    outErrors.push_back("HyphenChoice requiere simbolos en argumentos de variable");
                    break;
                }
            }
        } else {
            outErrors.push_back("Operador o funcion MathJSON no permitido: " + n.text);
        }
        if (n.text == "ConvertUnit" && ar == 2) {
            UnitDim d{}; double s = 1.0, o = 0.0;
            std::string uErr;
            if (n.args[1].kind != MathJsonKind::String || !TryParseUnitExpression(n.args[1].text, d, s, o, &uErr)) {
                outErrors.push_back("Unidad destino invalida en ConvertUnit");
            }
        }
        if (n.text == "UnitQuantity" && ar == 2) {
            UnitDim d{}; double s = 1.0, o = 0.0;
            std::string uErr;
            if ((n.args[1].kind != MathJsonKind::String && n.args[1].kind != MathJsonKind::Symbol) ||
                !TryParseUnitExpression(n.args[1].text, d, s, o, &uErr)) {
                outErrors.push_back("Unidad invalida en UnitQuantity");
            }
        }
        if (n.text == "Complex" && ar == 2) {
            if (n.args[0].kind != MathJsonKind::Number || !std::isfinite(n.args[0].realVal) ||
                n.args[1].kind != MathJsonKind::Number || !std::isfinite(n.args[1].realVal)) {
                outErrors.push_back("Complex requiere dos argumentos numericos finitos");
            }
        }
        if (n.text == "EngSuffix" && ar == 2) {
            static const std::string kValidSuf = "pnu kMG";
            if (n.args[0].kind != MathJsonKind::Number || !std::isfinite(n.args[0].realVal) ||
                n.args[1].kind != MathJsonKind::Symbol || n.args[1].text.size() != 1 ||
                n.args[1].text[0] == ' ' || kValidSuf.find(n.args[1].text[0]) == std::string::npos) {
                outErrors.push_back("EngSuffix invalido en AST");
            }
        }
        for (std::size_t idx = 0; idx < n.args.size(); ++idx) {
            const bool childAllowStr = (idx == 1 && ar == 2 && (n.text == "ConvertUnit" || n.text == "UnitQuantity"));
            self(self, n.args[idx], childAllowStr);
        }
    };
    check(check, ast, false);
    return outErrors.empty();
}

bool ParseExpressionToMathJson(
    const std::string& exprOrLatexUtf8,
    MathJsonNode& outAst,
    const std::unordered_set<std::string>& boundVars,
    std::string* outErr)
{
    std::string s = detail_sym::Trim(exprOrLatexUtf8);
    if (s.empty()) {
        if (outErr) *outErr = "Expresion vacia";
        return false;
    }
    if (s.find('\\') != std::string::npos || s.find('$') != std::string::npos) {
        std::string casExpr;
        if (ConvertLatexToCasExpr(s, casExpr, outErr)) {
            s = casExpr;
        } else {
            return false;
        }
    }
    detail_sym::AstExprParser parser(s, &boundVars);
    std::string err;
    if (!parser.Parse(outAst, err)) {
        if (outErr) *outErr = err;
        return false;
    }
    return true;
}

bool ParseExpressionToMathJson(
    const std::string& exprOrLatexUtf8,
    MathJsonNode& outAst,
    std::string* outErr)
{
    static const std::unordered_set<std::string> kEmpty;
    return ParseExpressionToMathJson(exprOrLatexUtf8, outAst, kEmpty, outErr);
}

std::string MathJsonToExpression(const MathJsonNode& ast) {
    return detail_sym::EmitExprRec(ast, 0);
}

std::string MathJsonToLatex(const MathJsonNode& ast) {
    return ConvertCasExprToLatex(MathJsonToExpression(ast));
}

std::vector<std::string> ExtractFreeSymbols(
    const MathJsonNode& ast,
    const SymbolEnv& boundEnv)
{
    std::unordered_set<std::string> seen;
    std::vector<std::string> out;
    detail_sym::CollectFreeSymsRec(ast, boundEnv, seen, out);
    return out;
}

MathJsonNode CanonicalizeMathJson(const MathJsonNode& ast, int maxExpandTerms) {
    return detail_sym::RunCanonicalLoop(ast, maxExpandTerms, true);
}

MathJsonNode SimplifySymbolicAst(const MathJsonNode& ast) {
    return detail_sym::RunCanonicalLoop(ast, 64, false);
}

MathJsonNode ExpandSymbolicAst(const MathJsonNode& ast, int maxExpandTerms) {
    return detail_sym::RunCanonicalLoop(ast, maxExpandTerms, true);
}

namespace detail_sym {

inline MathJsonNode NormalizeAstForDiffVar(const MathJsonNode& n, const std::string& v) {
    if (n.kind != MathJsonKind::Function) return n;
    if (n.text == "HyphenChoice" && n.args.size() >= 3) {
        bool hasVar = false;
        for (std::size_t idx = 2; idx < n.args.size(); ++idx) {
            if (n.args[idx].kind == MathJsonKind::Symbol && n.args[idx].text == v) {
                hasVar = true;
                break;
            }
        }
        return NormalizeAstForDiffVar(hasVar ? n.args[1] : n.args[0], v);
    }
    if (n.text == "EngSuffix" && n.args.size() == 2 &&
        n.args[1].kind == MathJsonKind::Symbol && n.args[1].text == v) {
        return MathJsonNode::MakeFunction("Multiply", { n.args[0], MathJsonNode::MakeSymbol(v) });
    }
    if (n.text == "Power" && n.args.size() == 2 &&
        n.args[0].kind == MathJsonKind::Function && n.args[0].text == "EngSuffix" &&
        n.args[0].args.size() == 2 &&
        n.args[0].args[1].kind == MathJsonKind::Symbol && n.args[0].args[1].text == v) {
        return MathJsonNode::MakeFunction("Multiply", {
            n.args[0].args[0],
            MathJsonNode::MakeFunction("Power", { MathJsonNode::MakeSymbol(v), NormalizeAstForDiffVar(n.args[1], v) })
        });
    }
    MathJsonNode copy = n;
    for (auto& a : copy.args) a = NormalizeAstForDiffVar(a, v);
    return copy;
}

} // namespace detail_sym

bool DifferentiateAst(
    const MathJsonNode& ast,
    const std::string& varName,
    MathJsonNode& outDeriv,
    std::string* outErr)
{
    std::string err;
    const std::string targetVar = varName.empty() ? "x" : varName;
    const MathJsonNode normAst = detail_sym::NormalizeAstForDiffVar(ast, targetVar);
    MathJsonNode raw;
    if (!detail_sym::DiffRec(normAst, targetVar, raw, err)) {
        if (outErr) *outErr = err;
        return false;
    }
    outDeriv = SimplifySymbolicAst(raw);
    return true;
}

SymbolicOpResult SimplifySymbolicExpression(const std::string& exprUtf8) {
    SymbolicOpResult res;
    MathJsonNode ast;
    if (!ParseExpressionToMathJson(exprUtf8, ast, &res.errorUtf8)) return res;
    res.ast = SimplifySymbolicAst(ast);
    res.expressionUtf8 = MathJsonToExpression(res.ast);
    res.latexUtf8 = MathJsonToLatex(res.ast);
    res.ok = true;
    return res;
}

SymbolicOpResult ExpandSymbolicExpression(const std::string& exprUtf8) {
    SymbolicOpResult res;
    MathJsonNode ast;
    if (!ParseExpressionToMathJson(exprUtf8, ast, &res.errorUtf8)) return res;
    res.ast = ExpandSymbolicAst(ast, 128);
    res.expressionUtf8 = MathJsonToExpression(res.ast);
    res.latexUtf8 = MathJsonToLatex(res.ast);
    res.ok = true;
    return res;
}

SymbolicOpResult DifferentiateExpression(const std::string& exprUtf8, const std::string& varName) {
    SymbolicOpResult res;
    MathJsonNode ast;
    const std::string v = varName.empty() ? "x" : varName;
    if (!ParseExpressionToMathJson(exprUtf8, ast, std::unordered_set<std::string>{ v }, &res.errorUtf8)) return res;
    if (!DifferentiateAst(ast, v, res.ast, &res.errorUtf8)) return res;
    res.expressionUtf8 = MathJsonToExpression(res.ast);
    res.latexUtf8 = MathJsonToLatex(res.ast);
    res.ok = true;
    return res;
}

AlgebraicSolveResult SolveAlgebraicEquation(
    const std::string& eqUtf8,
    const std::string& varName,
    const SymbolEnv& env)
{
    AlgebraicSolveResult res;
    const std::string v = varName.empty() ? "x" : varName;
    res.varNameUtf8 = v;

    std::string lhsStr = eqUtf8;
    std::string rhsStr = "0";
    const auto eqPos = eqUtf8.find('=');
    if (eqPos != std::string::npos) {
        lhsStr = detail_sym::Trim(eqUtf8.substr(0, eqPos));
        rhsStr = detail_sym::Trim(eqUtf8.substr(eqPos + 1));
    }
    if (lhsStr.empty() || rhsStr.empty()) {
        res.errorUtf8 = "Ecuacion invalida o vacia";
        return res;
    }

    const std::string fExpr = "(" + lhsStr + ") - (" + rhsStr + ")";
    std::unordered_set<std::string> boundVars{ v };
    for (const auto& kv : env) boundVars.insert(kv.first);
    MathJsonNode fAst;
    if (!ParseExpressionToMathJson(fExpr, fAst, boundVars, &res.errorUtf8)) return res;
    const MathJsonNode canon = CanonicalizeMathJson(fAst, 128);
    const std::string canonStr = MathJsonToExpression(canon);

    res.steps.push_back({ 1, "Forma estandar f(" + v + ") = 0", canonStr + " = 0", ConvertCasExprToLatex(canonStr) + " = 0" });

    auto evalAt = [&](std::complex<double> xVal, std::complex<double>& outVal) -> bool {
        SymbolEnv work = env;
        work[v] = QuantityValue{ xVal, {}, "", 1.0, 0.0, false };
        CasEvalResult ev{};
        if (EvaluateAst(canon, work, ev) && ev.ok) {
            outVal = ev.result.value;
            return true;
        }
        ev = EvaluateExpression(fExpr, work);
        if (ev.ok) { outVal = ev.result.value; return true; }
        return false;
    };

    // Detect exact polynomial degree <= 2 via finite-difference probing on f(x) at 0, 1, -1, 2, -2
    std::complex<double> f0{}, f1{}, fm1{}, f2{}, fm2{};
    if (evalAt(0.0, f0) && evalAt(1.0, f1) && evalAt(-1.0, fm1) && evalAt(2.0, f2) && evalAt(-2.0, fm2)) {
        const auto c = f0;
        const auto a = 0.5 * (f1 + fm1) - c;
        const auto b = 0.5 * (f1 - fm1);
        const auto err2 = std::abs((4.0 * a + 2.0 * b + c) - f2) + std::abs((4.0 * a - 2.0 * b + c) - fm2);
        if (err2 < 1e-8) {
            if (std::abs(a) < 1e-11) {
                if (std::abs(b) < 1e-11) {
                    res.errorUtf8 = (std::abs(c) < 1e-11)
                        ? "Identidad (infinitas soluciones)"
                        : "Ecuacion inconsistente (sin solucion)";
                    return res;
                }
                const auto xSol = -c / b;
                res.solvedExact = true;
                res.solutions = { xSol };
                res.steps.push_back({ 2, "Ecuacion lineal a*" + v + " + b = 0",
                    detail_sym::FmtNum(b.real(), 6) + "*" + v + " + " + detail_sym::FmtNum(c.real(), 6) + " = 0",
                    detail_sym::FmtNum(b.real(), 6) + v + " = " + detail_sym::FmtNum(-c.real(), 6) });
                const std::string solStr = QuantityValue{ xSol, {}, "", 1.0, 0.0, false }.FormatUtf8(6);
                res.solutionStringsUtf8.push_back(solStr);
                res.steps.push_back({ 3, "Despeje exacto", v + " = " + solStr, v + " = " + ConvertCasExprToLatex(solStr) });
                std::complex<double> check{};
                evalAt(xSol, check);
                res.maxResidual = std::abs(check);
                res.summaryUtf8 = v + " = " + solStr;
                res.ok = true;
                return res;
            }
            // Quadratic a*x^2 + b*x + c = 0
            const auto disc = b * b - 4.0 * a * c;
            const auto sqrtDisc = std::sqrt(disc);
            const auto x1 = (-b + sqrtDisc) / (2.0 * a);
            const auto x2 = (-b - sqrtDisc) / (2.0 * a);
            res.solvedExact = true;
            res.solutions = (std::abs(x1 - x2) < 1e-11) ? std::vector<std::complex<double>>{ x1 } : std::vector<std::complex<double>>{ x1, x2 };
            res.steps.push_back({ 2, "Discriminante cuadratico Delta = b^2 - 4ac",
                "Delta = " + QuantityValue{ disc, {}, "", 1.0, 0.0, false }.FormatUtf8(6),
                "\\Delta = " + ConvertCasExprToLatex(QuantityValue{ disc, {}, "", 1.0, 0.0, false }.FormatUtf8(6)) });
            for (const auto& sVal : res.solutions) {
                res.solutionStringsUtf8.push_back(QuantityValue{ sVal, {}, "", 1.0, 0.0, false }.FormatUtf8(6));
            }
            std::string solSummary;
            for (std::size_t i = 0; i < res.solutionStringsUtf8.size(); ++i) {
                if (i > 0) solSummary += "; ";
                solSummary += v + "_" + std::to_string(i + 1) + " = " + res.solutionStringsUtf8[i];
            }
            res.steps.push_back({ 3, "Raices por formula general", solSummary, ConvertCasExprToLatex(solSummary) });
            std::complex<double> c1{}, c2{};
            evalAt(x1, c1);
            evalAt(x2, c2);
            res.maxResidual = (std::max)(std::abs(c1), std::abs(c2));
            res.summaryUtf8 = solSummary;
            res.ok = true;
            return res;
        }
    }

    // Nonlinear safeguarded Newton-Raphson multi-seed fallback
    static constexpr double kSeeds[5] = { 1.0, 0.25, -1.0, 2.5, 0.0 };
    for (double seed : kSeeds) {
        std::complex<double> x(seed, 0.0);
        bool converged = false;
        for (int it = 0; it < 60; ++it) {
            std::complex<double> fx{};
            if (!evalAt(x, fx)) break;
            if (std::abs(fx) < 1e-11) { converged = true; break; }
            const double h = (std::max)(1e-7, std::abs(x) * 1e-6);
            std::complex<double> fp{}, fm{};
            if (!evalAt(x + h, fp) || !evalAt(x - h, fm)) break;
            const auto df = (fp - fm) / (2.0 * h);
            if (std::abs(df) < 1e-14) break;
            const auto next = x - fx / df;
            if (std::abs(next - x) < 1e-11 * (1.0 + std::abs(x))) {
                x = next;
                converged = true;
                break;
            }
            x = next;
        }
        if (converged) {
            std::complex<double> fx{};
            if (evalAt(x, fx) && std::abs(fx) < 1e-7) {
                res.solutions = { x };
                const std::string solStr = QuantityValue{ x, {}, "", 1.0, 0.0, false }.FormatUtf8(6);
                res.solutionStringsUtf8 = { solStr };
                res.steps.push_back({ 2, "Iteracion Newton-Raphson convergente", v + " = " + solStr, v + " \\approx " + ConvertCasExprToLatex(solStr) });
                res.maxResidual = std::abs(fx);
                res.summaryUtf8 = v + " = " + solStr;
                res.ok = true;
                return res;
            }
        }
    }

    res.errorUtf8 = "No se encontro raiz convergente para " + eqUtf8;
    return res;
}

} // namespace DocStudio::Cas
