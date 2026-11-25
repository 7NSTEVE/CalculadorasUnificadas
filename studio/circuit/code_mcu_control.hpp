#pragma once

#include "circuit_models.hpp"
#include "circuit_schematic.hpp"
#include "circuit_symbolic.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::Circuit {

// -------- 1. CodeBlock Syntax Highlighting Tokenizer (F-255) --------

enum class CodeLanguage : std::uint8_t {
    C = 0,
    Cpp = 1,
    Python = 2,
    ArduinoIno = 3
};

enum class CodeTokenKind : std::uint8_t {
    Plain = 0,
    Keyword,
    Type,
    Preprocessor,
    Number,
    String,
    Comment,
    Identifier,
    Operator,
    Punctuation,
    McuApi
};

struct CodeToken {
    std::uint32_t line = 0;
    std::uint32_t col = 0;
    std::uint32_t byteOffset = 0;
    std::uint32_t byteLength = 0;
    CodeTokenKind kind = CodeTokenKind::Plain;
};

struct CodeHighlightResult {
    CodeLanguage language = CodeLanguage::Cpp;
    std::uint32_t lineCount = 0;
    std::vector<CodeToken> tokens;
};

inline CodeHighlightResult TokenizeCodeBlock(const std::string& sourceUtf8, CodeLanguage lang) {
    CodeHighlightResult res;
    res.language = lang;
    res.lineCount = sourceUtf8.empty() ? 0u : 1u;

    static const std::set<std::string> kCppKeywords = {
        "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
        "return", "struct", "class", "enum", "namespace", "using", "const", "constexpr",
        "static", "volatile", "inline", "true", "false", "nullptr", "sizeof", "public", "private"
    };
    static const std::set<std::string> kCppTypes = {
        "void", "int", "long", "short", "char", "unsigned", "signed", "float", "double",
        "bool", "uint8_t", "uint16_t", "uint32_t", "int32_t", "size_t", "String", "auto"
    };
    static const std::set<std::string> kPyKeywords = {
        "def", "class", "if", "elif", "else", "for", "while", "break", "continue", "return",
        "import", "from", "as", "pass", "True", "False", "None", "and", "or", "not", "in", "is", "with"
    };
    static const std::set<std::string> kMcuApis = {
        "setup", "loop", "pinMode", "digitalWrite", "digitalRead", "analogWrite", "analogRead",
        "millis", "micros", "delay", "delayMicroseconds", "Serial", "println", "print",
        "ledcSetup", "ledcAttachPin", "ledcWrite", "OUTPUT", "INPUT", "INPUT_PULLUP", "HIGH", "LOW"
    };

    const std::size_t n = sourceUtf8.size();
    std::size_t i = 0;
    std::uint32_t line = 0;
    std::uint32_t col = 0;
    bool lineStart = true;

    auto pushTok = [&](std::size_t start, std::size_t len, std::uint32_t tLine, std::uint32_t tCol, CodeTokenKind k) {
        if (len == 0) return;
        res.tokens.push_back({ tLine, tCol, static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(len), k });
    };

    while (i < n) {
        const char ch = sourceUtf8[i];
        if (ch == '\n') {
            ++line;
            ++res.lineCount;
            col = 0;
            lineStart = true;
            ++i;
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            ++col;
            ++i;
            continue;
        }

        // Comments
        if (lang != CodeLanguage::Python && i + 1 < n && ch == '/' && sourceUtf8[i + 1] == '/') {
            const std::size_t start = i;
            const std::uint32_t sLine = line, sCol = col;
            while (i < n && sourceUtf8[i] != '\n') { ++i; ++col; }
            pushTok(start, i - start, sLine, sCol, CodeTokenKind::Comment);
            lineStart = false;
            continue;
        }
        if (lang != CodeLanguage::Python && i + 1 < n && ch == '/' && sourceUtf8[i + 1] == '*') {
            const std::size_t start = i;
            const std::uint32_t sLine = line, sCol = col;
            i += 2; col += 2;
            while (i + 1 < n && !(sourceUtf8[i] == '*' && sourceUtf8[i + 1] == '/')) {
                if (sourceUtf8[i] == '\n') { ++line; ++res.lineCount; col = 0; }
                else ++col;
                ++i;
            }
            if (i + 1 < n) { i += 2; col += 2; }
            pushTok(start, i - start, sLine, sCol, CodeTokenKind::Comment);
            lineStart = false;
            continue;
        }
        if (lang == CodeLanguage::Python && ch == '#') {
            const std::size_t start = i;
            const std::uint32_t sLine = line, sCol = col;
            while (i < n && sourceUtf8[i] != '\n') { ++i; ++col; }
            pushTok(start, i - start, sLine, sCol, CodeTokenKind::Comment);
            lineStart = false;
            continue;
        }

        // C/C++/Arduino Preprocessor
        if (lang != CodeLanguage::Python && lineStart && ch == '#') {
            const std::size_t start = i;
            const std::uint32_t sLine = line, sCol = col;
            while (i < n && sourceUtf8[i] != '\n') { ++i; ++col; }
            pushTok(start, i - start, sLine, sCol, CodeTokenKind::Preprocessor);
            lineStart = false;
            continue;
        }

        lineStart = false;

        // Strings & char literals
        if (ch == '"' || ch == '\'') {
            const char quote = ch;
            const std::size_t start = i;
            const std::uint32_t sLine = line, sCol = col;
            ++i; ++col;
            while (i < n) {
                if (sourceUtf8[i] == '\\' && i + 1 < n) {
                    i += 2; col += 2; continue;
                }
                if (sourceUtf8[i] == quote) {
                    ++i; ++col; break;
                }
                if (sourceUtf8[i] == '\n') break;
                ++i; ++col;
            }
            pushTok(start, i - start, sLine, sCol, CodeTokenKind::String);
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            const std::size_t start = i;
            const std::uint32_t sCol = col;
            while (i < n && (std::isalnum(static_cast<unsigned char>(sourceUtf8[i])) ||
                             sourceUtf8[i] == '.' || sourceUtf8[i] == '_')) {
                ++i; ++col;
            }
            pushTok(start, i - start, line, sCol, CodeTokenKind::Number);
            continue;
        }

        // Identifiers / Keywords / Types / MCU APIs
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            const std::size_t start = i;
            const std::uint32_t sCol = col;
            while (i < n && (std::isalnum(static_cast<unsigned char>(sourceUtf8[i])) || sourceUtf8[i] == '_')) {
                ++i; ++col;
            }
            const std::string word = sourceUtf8.substr(start, i - start);
            CodeTokenKind tk = CodeTokenKind::Identifier;
            if (lang == CodeLanguage::Python) {
                if (kPyKeywords.count(word)) tk = CodeTokenKind::Keyword;
                else if (kMcuApis.count(word)) tk = CodeTokenKind::McuApi;
            } else {
                if (kCppKeywords.count(word)) tk = CodeTokenKind::Keyword;
                else if (kCppTypes.count(word)) tk = CodeTokenKind::Type;
                else if (kMcuApis.count(word)) tk = CodeTokenKind::McuApi;
            }
            pushTok(start, i - start, line, sCol, tk);
            continue;
        }

        // Operators vs Punctuation
        const std::size_t start = i;
        const std::uint32_t sCol = col;
        ++i; ++col;
        if (std::string("+-*/%=<>!&|^~?:").find(ch) != std::string::npos) {
            if (i < n && std::string("=+-&|<>").find(sourceUtf8[i]) != std::string::npos) {
                ++i; ++col;
            }
            pushTok(start, i - start, line, sCol, CodeTokenKind::Operator);
        } else {
            pushTok(start, 1, line, sCol, CodeTokenKind::Punctuation);
        }
    }

    return res;
}

// -------- 2. Deterministic MCU (Arduino / ESP32 / RP2040 / ATtiny85) Co-Simulation --------

enum class McuBoardKind : std::uint8_t {
    ArduinoUno = 0,
    Esp32 = 1,
    Rp2040 = 2,
    Attiny85 = 3
};

enum class McuPinMode : std::uint8_t {
    Input = 0,
    Output = 1,
    InputPullup = 2,
    PwmOut = 3,
    AnalogIn = 4
};

struct McuPinBinding {
    std::string mcuPinName; // "D13", "A0", "GPIO2", "GP15", "PB0"
    int circuitNode = 0;    // MNA node > 0
    McuPinMode mode = McuPinMode::Output;
    double lastDrivenV = 0.0;
    double lastSampledV = 0.0;
    double pwmDuty01 = 0.0;
    double pwmFreqHz = 490.0;
};

struct McuBoardProfile {
    McuBoardKind kind = McuBoardKind::ArduinoUno;
    double vddV = 5.0;
    double gpioRonOhm = 25.0;
    int adcBits = 10;
    double vRefV = 5.0;
};

inline McuBoardProfile GetMcuBoardProfile(McuBoardKind board) noexcept {
    switch (board) {
    case McuBoardKind::ArduinoUno: return { board, 5.0, 25.0, 10, 5.0 };
    case McuBoardKind::Esp32:      return { board, 3.3, 30.0, 12, 3.3 };
    case McuBoardKind::Rp2040:     return { board, 3.3, 25.0, 12, 3.3 };
    case McuBoardKind::Attiny85:   return { board, 5.0, 35.0, 10, 5.0 };
    }
    return { board, 5.0, 25.0, 10, 5.0 };
}

inline bool IsValidPinForBoard(McuBoardKind board, const std::string& pinRaw) {
    const std::string p = ToUpperAscii(pinRaw);
    auto parseNumAfter = [&](const char* prefix, int lo, int hi) -> bool {
        const std::string pref(prefix);
        if (p.rfind(pref, 0) != 0) return false;
        const std::string tail = p.substr(pref.size());
        if (tail.empty()) return false;
        for (char c : tail) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        const int v = std::stoi(tail);
        return v >= lo && v <= hi;
    };
    switch (board) {
    case McuBoardKind::ArduinoUno:
        return parseNumAfter("D", 0, 13) || parseNumAfter("A", 0, 5) || parseNumAfter("", 0, 19);
    case McuBoardKind::Esp32:
        return parseNumAfter("GPIO", 0, 39) || parseNumAfter("D", 0, 39) || parseNumAfter("", 0, 39);
    case McuBoardKind::Rp2040:
        return parseNumAfter("GP", 0, 29) || parseNumAfter("D", 0, 29) || parseNumAfter("", 0, 29);
    case McuBoardKind::Attiny85:
        return parseNumAfter("PB", 0, 5) || parseNumAfter("D", 0, 5) || parseNumAfter("A", 0, 3) || parseNumAfter("", 0, 5);
    }
    return false;
}

struct McuCoSimResult {
    bool ok = false;
    TransientWaveformResult circuitWaveforms;
    std::map<std::string, std::vector<double>> pinVoltages;
    std::vector<std::string> serialLog;
    std::string errorUtf8;
};

namespace Detail {

struct MiniMcuVm {
    struct Stmt {
        enum class Kind { PinMode, DigitalWrite, AnalogWrite, Delay, SerialPrint, VarAssign, IfElse, ForLoop, WhileLoop };
        Kind kind = Kind::Delay;
        std::string target;
        std::string arg1;
        std::string arg2;
        std::string condLhs;
        std::string condOp;
        std::string condRhs;
        std::vector<Stmt> initStmts;
        std::vector<Stmt> stepStmts;
        std::vector<Stmt> thenBody;
        std::vector<Stmt> elseBody;
    };

    struct LoopCursor {
        bool initialized = false;
        std::size_t bodyPc = 0;
        int itersThisTick = 0;
    };

    std::vector<Stmt> setupStmts;
    std::vector<Stmt> loopStmts;
    std::map<std::string, double> vars;
    std::map<std::string, std::string> pinAliases;
    std::map<std::string, McuPinBinding*> pinMap;
    std::map<const Stmt*, LoopCursor> loopCursors;
    McuBoardProfile profile{};
    double simTimeMs = 0.0;
    double sleepUntilMs = 0.0;
    std::size_t loopPc = 0;
    bool setupDone = false;
    std::vector<std::string> serialLog;

    static std::string Trim(std::string s) {
        const auto a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        const auto b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    static std::string StripComments(const std::string& code) {
        std::string out;
        out.reserve(code.size());
        for (std::size_t i = 0; i < code.size(); ++i) {
            if (i + 1 < code.size() && code[i] == '/' && code[i + 1] == '/') {
                while (i < code.size() && code[i] != '\n') ++i;
                out += '\n';
            } else if (i + 1 < code.size() && code[i] == '/' && code[i + 1] == '*') {
                i += 2;
                while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/')) ++i;
                ++i;
            } else {
                out += code[i];
            }
        }
        return out;
    }

    static std::string CanonicalPinKey(std::string p) {
        p = Trim(std::move(p));
        if (p.size() >= 2 && p.front() == '"' && p.back() == '"') p = p.substr(1, p.size() - 2);
        return ToUpperAscii(p);
    }

    std::string ResolvePinKey(const std::string& raw) const {
        const std::string t = Trim(raw);
        const std::string direct = CanonicalPinKey(t);
        if (pinMap.count(direct)) return direct;
        auto ait = pinAliases.find(t);
        if (ait != pinAliases.end() && pinMap.count(ait->second)) return ait->second;
        if (vars.count(t) || (!t.empty() && std::isdigit(static_cast<unsigned char>(t[0])))) {
            const long long num = std::llround(EvalExpr(t));
            const std::string numStr = std::to_string(num);
            for (const std::string& cand : { numStr, "D" + numStr, "GPIO" + numStr, "GP" + numStr, "PB" + numStr }) {
                if (pinMap.count(cand)) return cand;
            }
        }
        return direct;
    }

    static bool IsBinaryOpAt(const std::string& s, std::size_t idx) noexcept {
        if (idx == 0) return false;
        const char op = s[idx];
        std::size_t prev = idx;
        while (prev > 0 && std::isspace(static_cast<unsigned char>(s[prev - 1]))) --prev;
        if (prev == 0) return false;
        const char prevCh = s[prev - 1];
        if (op == '+' || op == '-') {
            if ((prevCh == 'e' || prevCh == 'E') && prev >= 2 &&
                (std::isdigit(static_cast<unsigned char>(s[prev - 2])) || s[prev - 2] == '.')) {
                return false; // scientific notation exponent sign (e.g. 1e-3, 1E+6)
            }
            if (prevCh == '+' || prevCh == '-' || prevCh == '*' || prevCh == '/' || prevCh == '(') {
                return false; // unary sign (e.g. 10 * -5)
            }
        }
        return true;
    }

    double EvalExpr(const std::string& exprRaw) const {
        const std::string s = Trim(exprRaw);
        if (s.empty()) return 0.0;

        // Strip balanced outer parentheses only when the opening '(' matches the final ')'
        if (s.size() >= 2 && s.front() == '(' && s.back() == ')') {
            int depth = 0;
            bool wrapsAll = true;
            for (std::size_t k = 0; k + 1 < s.size(); ++k) {
                if (s[k] == '(') ++depth;
                else if (s[k] == ')') --depth;
                if (depth == 0) { wrapsAll = false; break; }
            }
            if (wrapsAll) return EvalExpr(s.substr(1, s.size() - 2));
        }

        const std::string up = ToUpperAscii(s);
        if (up == "HIGH" || up == "TRUE") return 1.0;
        if (up == "LOW" || up == "FALSE") return 0.0;
        if (up == "OUTPUT") return 1.0;
        if (up == "INPUT") return 0.0;
        if (up == "INPUT_PULLUP") return 2.0;
        if (s == "millis()") return simTimeMs;

        // Binary arithmetic respecting precedence: first scan '+' / '-' right-to-left, then '*' / '/'
        for (int pass = 0; pass < 2; ++pass) {
            int depth = 0;
            for (std::size_t i = s.size(); i > 0; --i) {
                const std::size_t idx = i - 1;
                const char c = s[idx];
                if (c == ')') ++depth;
                else if (c == '(') --depth;
                else if (depth == 0) {
                    const bool match = (pass == 0) ? (c == '+' || c == '-') : (c == '*' || c == '/');
                    if (match && IsBinaryOpAt(s, idx)) {
                        const double l = EvalExpr(s.substr(0, idx));
                        const double r = EvalExpr(s.substr(idx + 1));
                        if (c == '+') return l + r;
                        if (c == '-') return l - r;
                        if (c == '*') return l * r;
                        if (c == '/') return (std::abs(r) > 1e-12) ? (l / r) : 0.0;
                    }
                }
            }
        }

        // Unary +/-
        if (s[0] == '-') return -EvalExpr(s.substr(1));
        if (s[0] == '+') return EvalExpr(s.substr(1));

        // analogRead(pin) / digitalRead(pin)
        if (s.rfind("analogRead(", 0) == 0 && s.back() == ')') {
            const std::string pKey = ResolvePinKey(s.substr(11, s.size() - 12));
            auto it = pinMap.find(pKey);
            if (it != pinMap.end()) {
                const double maxAdc = static_cast<double>((1 << profile.adcBits) - 1);
                return std::clamp(std::round((it->second->lastSampledV / profile.vRefV) * maxAdc), 0.0, maxAdc);
            }
            return 0.0;
        }
        if (s.rfind("digitalRead(", 0) == 0 && s.back() == ')') {
            const std::string pKey = ResolvePinKey(s.substr(12, s.size() - 13));
            auto it = pinMap.find(pKey);
            if (it != pinMap.end()) {
                return (it->second->lastSampledV >= 0.5 * profile.vddV) ? 1.0 : 0.0;
            }
            return 0.0;
        }

        auto vit = vars.find(s);
        if (vit != vars.end()) return vit->second;

        try {
            std::size_t idx = 0;
            const double val = std::stod(s, &idx);
            if (idx == s.size()) return val;
        } catch (...) {}
        return 0.0;
    }

    bool EvalCond(const std::string& lhs, const std::string& op, const std::string& rhs) const {
        const double a = EvalExpr(lhs);
        const double b = EvalExpr(rhs);
        if (op == ">") return a > b;
        if (op == "<") return a < b;
        if (op == ">=") return a >= b;
        if (op == "<=") return a <= b;
        if (op == "==") return std::abs(a - b) < 1e-6;
        if (op == "!=") return std::abs(a - b) >= 1e-6;
        return a != 0.0;
    }

    static void ParseCondition(const std::string& cond, std::string& outLhs, std::string& outOp, std::string& outRhs) {
        outLhs.clear(); outOp.clear(); outRhs.clear();
        for (const char* op : { ">=", "<=", "==", "!=", ">", "<" }) {
            const auto opPos = cond.find(op);
            if (opPos != std::string::npos) {
                outLhs = Trim(cond.substr(0, opPos));
                outOp = op;
                outRhs = Trim(cond.substr(opPos + std::string(op).size()));
                return;
            }
        }
        outLhs = Trim(cond);
    }

    static bool MatchesKeyword(const std::string& block, std::size_t pos, const char* kw) noexcept {
        const std::size_t len = std::strlen(kw);
        if (block.compare(pos, len, kw) != 0) return false;
        if (pos > 0 && (std::isalnum(static_cast<unsigned char>(block[pos - 1])) || block[pos - 1] == '_')) return false;
        if (pos + len < block.size() && (std::isalnum(static_cast<unsigned char>(block[pos + len])) || block[pos + len] == '_')) return false;
        return true;
    }

    static bool ExtractParenAndBrace(
        const std::string& block,
        std::size_t kwEnd,
        std::string& outParen,
        std::string& outBody,
        std::size_t& outAfterBrace)
    {
        const std::size_t n = block.size();
        const std::size_t lp = block.find('(', kwEnd);
        if (lp == std::string::npos) return false;
        int depth = 1;
        std::size_t rp = lp + 1;
        while (rp < n && depth > 0) {
            if (block[rp] == '(') ++depth;
            else if (block[rp] == ')') --depth;
            ++rp;
        }
        if (depth != 0) return false;
        outParen = Trim(block.substr(lp + 1, rp - lp - 2));

        std::size_t bodyStart = rp;
        while (bodyStart < n && std::isspace(static_cast<unsigned char>(block[bodyStart]))) ++bodyStart;
        if (bodyStart < n && block[bodyStart] == '{') {
            int bDepth = 1;
            std::size_t rb = bodyStart + 1;
            while (rb < n && bDepth > 0) {
                if (block[rb] == '{') ++bDepth;
                else if (block[rb] == '}') --bDepth;
                ++rb;
            }
            outBody = block.substr(bodyStart + 1, rb - bodyStart - 2);
            outAfterBrace = rb;
            return true;
        }
        const std::size_t semi = block.find(';', bodyStart);
        if (semi == std::string::npos) return false;
        outBody = block.substr(bodyStart, semi - bodyStart + 1);
        outAfterBrace = semi + 1;
        return true;
    }

    static std::vector<Stmt> ParseBlock(const std::string& block) {
        std::vector<Stmt> out;
        std::size_t i = 0;
        const std::size_t n = block.size();
        while (i < n) {
            while (i < n && (std::isspace(static_cast<unsigned char>(block[i])) || block[i] == ';')) ++i;
            if (i >= n) break;

            if (MatchesKeyword(block, i, "if")) {
                std::string cond, thenText;
                std::size_t rb = i;
                if (!ExtractParenAndBrace(block, i + 2, cond, thenText, rb)) break;

                Stmt st;
                st.kind = Stmt::Kind::IfElse;
                ParseCondition(cond, st.condLhs, st.condOp, st.condRhs);
                st.thenBody = ParseBlock(thenText);

                std::size_t after = rb;
                while (after < n && std::isspace(static_cast<unsigned char>(block[after]))) ++after;
                if (MatchesKeyword(block, after, "else")) {
                    std::size_t p = after + 4;
                    while (p < n && std::isspace(static_cast<unsigned char>(block[p]))) ++p;
                    if (MatchesKeyword(block, p, "if")) {
                        // Parse `else if (...) { ... } [else ...]` recursively as a single statement in elseBody
                        // Find end of this entire else-if chain by parsing block starting at p and taking the first stmt?
                        // Even cleaner: parse block.substr(p) for the rest of the chain or extract balanced chain!
                        // Wait: parsing block.substr(p) would also consume statements AFTER the if/else chain unless we track end offset.
                        // So let's loop over else-if chain or use a recursive lambda with cursor!
                        auto parseIfChain = [&](auto& self, std::size_t ifPos, std::size_t& endPos) -> Stmt {
                            Stmt node;
                            node.kind = Stmt::Kind::IfElse;
                            std::string cnd, bdy;
                            std::size_t rPos = ifPos;
                            if (!ExtractParenAndBrace(block, ifPos + 2, cnd, bdy, rPos)) {
                                endPos = n;
                                return node;
                            }
                            ParseCondition(cnd, node.condLhs, node.condOp, node.condRhs);
                            node.thenBody = ParseBlock(bdy);
                            endPos = rPos;
                            std::size_t nx = rPos;
                            while (nx < n && std::isspace(static_cast<unsigned char>(block[nx]))) ++nx;
                            if (MatchesKeyword(block, nx, "else")) {
                                std::size_t ep = nx + 4;
                                while (ep < n && std::isspace(static_cast<unsigned char>(block[ep]))) ++ep;
                                if (MatchesKeyword(block, ep, "if")) {
                                    std::size_t subEnd = ep;
                                    node.elseBody.push_back(self(self, ep, subEnd));
                                    endPos = subEnd;
                                } else if (ep < n && block[ep] == '{') {
                                    int ebDepth = 1;
                                    std::size_t erb = ep + 1;
                                    while (erb < n && ebDepth > 0) {
                                        if (block[erb] == '{') ++ebDepth;
                                        else if (block[erb] == '}') --ebDepth;
                                        ++erb;
                                    }
                                    node.elseBody = ParseBlock(block.substr(ep + 1, erb - ep - 2));
                                    endPos = erb;
                                } else {
                                    const std::size_t semi = block.find(';', ep);
                                    if (semi != std::string::npos) {
                                        node.elseBody = ParseBlock(block.substr(ep, semi - ep + 1));
                                        endPos = semi + 1;
                                    }
                                }
                            }
                            return node;
                        };
                        std::size_t chainEnd = p;
                        st.elseBody.push_back(parseIfChain(parseIfChain, p, chainEnd));
                        i = chainEnd;
                    } else if (p < n && block[p] == '{') {
                        int ebDepth = 1;
                        std::size_t erb = p + 1;
                        while (erb < n && ebDepth > 0) {
                            if (block[erb] == '{') ++ebDepth;
                            else if (block[erb] == '}') --ebDepth;
                            ++erb;
                        }
                        st.elseBody = ParseBlock(block.substr(p + 1, erb - p - 2));
                        i = erb;
                    } else {
                        const std::size_t semi = block.find(';', p);
                        if (semi != std::string::npos) {
                            st.elseBody = ParseBlock(block.substr(p, semi - p + 1));
                            i = semi + 1;
                        } else {
                            i = rb;
                        }
                    }
                } else {
                    i = rb;
                }
                out.push_back(std::move(st));
                continue;
            }

            if (MatchesKeyword(block, i, "for")) {
                std::string header, bodyText;
                std::size_t rb = i;
                if (!ExtractParenAndBrace(block, i + 3, header, bodyText, rb)) break;
                i = rb;

                const std::size_t s1 = header.find(';');
                const std::size_t s2 = (s1 != std::string::npos) ? header.find(';', s1 + 1) : std::string::npos;
                Stmt st;
                st.kind = Stmt::Kind::ForLoop;
                if (s1 != std::string::npos && s2 != std::string::npos) {
                    const std::string initStr = Trim(header.substr(0, s1));
                    const std::string condStr = Trim(header.substr(s1 + 1, s2 - s1 - 1));
                    const std::string stepStr = Trim(header.substr(s2 + 1));
                    if (!initStr.empty()) st.initStmts = ParseBlock(initStr + ";");
                    if (!condStr.empty()) ParseCondition(condStr, st.condLhs, st.condOp, st.condRhs);
                    if (!stepStr.empty()) st.stepStmts = ParseBlock(stepStr + ";");
                }
                st.thenBody = ParseBlock(bodyText);
                out.push_back(std::move(st));
                continue;
            }

            if (MatchesKeyword(block, i, "while")) {
                std::string condStr, bodyText;
                std::size_t rb = i;
                if (!ExtractParenAndBrace(block, i + 5, condStr, bodyText, rb)) break;
                i = rb;

                Stmt st;
                st.kind = Stmt::Kind::WhileLoop;
                ParseCondition(condStr, st.condLhs, st.condOp, st.condRhs);
                st.thenBody = ParseBlock(bodyText);
                out.push_back(std::move(st));
                continue;
            }

            const std::size_t semi = block.find(';', i);
            if (semi == std::string::npos) break;
            const std::string line = Trim(block.substr(i, semi - i));
            i = semi + 1;
            if (line.empty()) continue;

            auto parseCallArgs = [](const std::string& call, std::string& fn, std::string& a1, std::string& a2) -> bool {
                const auto lp = call.find('(');
                const auto rp = call.rfind(')');
                if (lp == std::string::npos || rp == std::string::npos || rp <= lp) return false;
                fn = Trim(call.substr(0, lp));
                const std::string inner = call.substr(lp + 1, rp - lp - 1);
                int depth = 0;
                std::size_t comma = std::string::npos;
                for (std::size_t k = 0; k < inner.size(); ++k) {
                    if (inner[k] == '(') ++depth;
                    else if (inner[k] == ')') --depth;
                    else if (depth == 0 && inner[k] == ',') { comma = k; break; }
                }
                if (comma == std::string::npos) {
                    a1 = Trim(inner);
                    a2.clear();
                } else {
                    a1 = Trim(inner.substr(0, comma));
                    a2 = Trim(inner.substr(comma + 1));
                }
                return true;
            };

            // Increment / decrement shorthands: i++, ++i, i--, --i, x += y, x -= y
            if (line.size() >= 3 && line.compare(line.size() - 2, 2, "++") == 0) {
                const std::string var = Trim(line.substr(0, line.size() - 2));
                Stmt st; st.kind = Stmt::Kind::VarAssign; st.target = var; st.arg1 = var + " + 1";
                out.push_back(std::move(st));
                continue;
            }
            if (line.size() >= 3 && line.compare(0, 2, "++") == 0) {
                const std::string var = Trim(line.substr(2));
                Stmt st; st.kind = Stmt::Kind::VarAssign; st.target = var; st.arg1 = var + " + 1";
                out.push_back(std::move(st));
                continue;
            }
            if (line.size() >= 3 && line.compare(line.size() - 2, 2, "--") == 0) {
                const std::string var = Trim(line.substr(0, line.size() - 2));
                Stmt st; st.kind = Stmt::Kind::VarAssign; st.target = var; st.arg1 = var + " - 1";
                out.push_back(std::move(st));
                continue;
            }
            if (line.size() >= 3 && line.compare(0, 2, "--") == 0) {
                const std::string var = Trim(line.substr(2));
                Stmt st; st.kind = Stmt::Kind::VarAssign; st.target = var; st.arg1 = var + " - 1";
                out.push_back(std::move(st));
                continue;
            }

            std::string fn, a1, a2;
            const auto eqPos = line.find('=');
            if (eqPos != std::string::npos && (line.find('(') == std::string::npos || eqPos < line.find('('))) {
                const bool isPlusEq = (eqPos > 0 && line[eqPos - 1] == '+');
                const bool isMinusEq = (eqPos > 0 && line[eqPos - 1] == '-');
                std::string lhs = Trim(line.substr(0, (isPlusEq || isMinusEq) ? (eqPos - 1) : eqPos));
                const std::string rhs = Trim(line.substr(eqPos + 1));
                for (const char* kw : { "const ", "int ", "float ", "double ", "bool ", "uint8_t ", "uint16_t ", "uint32_t " }) {
                    const std::string p(kw);
                    if (lhs.rfind(p, 0) == 0) lhs = Trim(lhs.substr(p.size()));
                }
                Stmt st;
                st.kind = Stmt::Kind::VarAssign;
                st.target = lhs;
                if (isPlusEq) st.arg1 = lhs + " + (" + rhs + ")";
                else if (isMinusEq) st.arg1 = lhs + " - (" + rhs + ")";
                else st.arg1 = rhs;
                out.push_back(std::move(st));
            } else if (parseCallArgs(line, fn, a1, a2)) {
                Stmt st;
                if (fn == "pinMode") { st.kind = Stmt::Kind::PinMode; st.arg1 = a1; st.arg2 = a2; out.push_back(std::move(st)); }
                else if (fn == "digitalWrite") { st.kind = Stmt::Kind::DigitalWrite; st.arg1 = a1; st.arg2 = a2; out.push_back(std::move(st)); }
                else if (fn == "analogWrite" || fn == "ledcWrite") { st.kind = Stmt::Kind::AnalogWrite; st.arg1 = a1; st.arg2 = a2; out.push_back(std::move(st)); }
                else if (fn == "delay") { st.kind = Stmt::Kind::Delay; st.arg1 = a1; out.push_back(std::move(st)); }
                else if (fn == "Serial.println" || fn == "Serial.print") { st.kind = Stmt::Kind::SerialPrint; st.arg1 = a1; out.push_back(std::move(st)); }
            }
        }
        return out;
    }

    static std::string ExtractFunctionBody(const std::string& code, const std::string& fnName) {
        const auto pos = code.find(fnName);
        if (pos == std::string::npos) return "";
        const auto lb = code.find('{', pos);
        if (lb == std::string::npos) return "";
        int depth = 1;
        std::size_t rb = lb + 1;
        while (rb < code.size() && depth > 0) {
            if (code[rb] == '{') ++depth;
            else if (code[rb] == '}') --depth;
            ++rb;
        }
        return code.substr(lb + 1, rb - lb - 2);
    }

    bool Compile(const std::string& rawCode, McuBoardKind board, std::vector<McuPinBinding>& bindings) {
        profile = GetMcuBoardProfile(board);
        pinMap.clear();
        pinAliases.clear();
        loopCursors.clear();
        for (auto& b : bindings) {
            pinMap[CanonicalPinKey(b.mcuPinName)] = &b;
        }
        const std::string clean = StripComments(rawCode);
        // Parse global const/int declarations before setup()
        const std::size_t firstFn = (std::min)(clean.find("void setup"), clean.find("void loop"));
        if (firstFn != std::string::npos && firstFn > 0) {
            const auto globals = ParseBlock(clean.substr(0, firstFn));
            for (const auto& st : globals) {
                if (st.kind == Stmt::Kind::VarAssign) {
                    const std::string canonRhs = CanonicalPinKey(st.arg1);
                    if (pinMap.count(canonRhs)) pinAliases[st.target] = canonRhs;
                    vars[st.target] = EvalExpr(st.arg1);
                }
            }
        }
        setupStmts = ParseBlock(ExtractFunctionBody(clean, "setup"));
        loopStmts = ParseBlock(ExtractFunctionBody(clean, "loop"));
        return !setupStmts.empty() || !loopStmts.empty();
    }

    void ExecStmt(const Stmt& st, bool& yieldedDelay) {
        switch (st.kind) {
        case Stmt::Kind::VarAssign: {
            const std::string canonRhs = CanonicalPinKey(st.arg1);
            if (pinMap.count(canonRhs)) pinAliases[st.target] = canonRhs;
            vars[st.target] = EvalExpr(st.arg1);
            break;
        }
        case Stmt::Kind::PinMode: {
            const std::string pKey = ResolvePinKey(st.arg1);
            auto it = pinMap.find(pKey);
            if (it != pinMap.end()) {
                const int m = static_cast<int>(std::llround(EvalExpr(st.arg2)));
                if (m == 1) it->second->mode = McuPinMode::Output;
                else if (m == 2) it->second->mode = McuPinMode::InputPullup;
                else it->second->mode = McuPinMode::Input;
            }
            break;
        }
        case Stmt::Kind::DigitalWrite: {
            const std::string pKey = ResolvePinKey(st.arg1);
            auto it = pinMap.find(pKey);
            if (it != pinMap.end()) {
                const bool high = EvalExpr(st.arg2) >= 0.5;
                it->second->mode = McuPinMode::Output;
                it->second->lastDrivenV = high ? profile.vddV : 0.0;
                it->second->pwmDuty01 = high ? 1.0 : 0.0;
            }
            break;
        }
        case Stmt::Kind::AnalogWrite: {
            const std::string pKey = ResolvePinKey(st.arg1);
            auto it = pinMap.find(pKey);
            if (it != pinMap.end()) {
                const double val = std::clamp(EvalExpr(st.arg2), 0.0, 255.0);
                it->second->mode = McuPinMode::PwmOut;
                it->second->pwmDuty01 = val / 255.0;
                it->second->lastDrivenV = profile.vddV * it->second->pwmDuty01;
            }
            break;
        }
        case Stmt::Kind::Delay: {
            const double ms = (std::max)(0.0, EvalExpr(st.arg1));
            sleepUntilMs = simTimeMs + ms;
            yieldedDelay = true;
            break;
        }
        case Stmt::Kind::SerialPrint: {
            std::ostringstream oss;
            if (st.arg1.size() >= 2 && st.arg1.front() == '"' && st.arg1.back() == '"') {
                oss << st.arg1.substr(1, st.arg1.size() - 2);
            } else {
                oss << EvalExpr(st.arg1);
            }
            if (serialLog.size() < 256) serialLog.push_back(oss.str());
            break;
        }
        case Stmt::Kind::IfElse: {
            const auto& body = EvalCond(st.condLhs, st.condOp, st.condRhs) ? st.thenBody : st.elseBody;
            for (const auto& sub : body) {
                ExecStmt(sub, yieldedDelay);
                if (yieldedDelay) break;
            }
            break;
        }
        case Stmt::Kind::ForLoop:
        case Stmt::Kind::WhileLoop: {
            LoopCursor& cur = loopCursors[&st];
            if (!cur.initialized) {
                bool dummy = false;
                for (const auto& ist : st.initStmts) ExecStmt(ist, dummy);
                cur.initialized = true;
                cur.bodyPc = 0;
            }
            cur.itersThisTick = 0;
            constexpr int kMaxLoopItersPerStep = 256;
            while (cur.itersThisTick < kMaxLoopItersPerStep) {
                if (cur.bodyPc == 0) {
                    const bool condOk = (st.condLhs.empty() && st.condOp.empty())
                        ? true
                        : EvalCond(st.condLhs, st.condOp, st.condRhs);
                    if (!condOk) {
                        cur.initialized = false;
                        break;
                    }
                }
                if (st.thenBody.empty()) {
                    bool dummy = false;
                    for (const auto& sst : st.stepStmts) ExecStmt(sst, dummy);
                    ++cur.itersThisTick;
                    continue;
                }
                while (cur.bodyPc < st.thenBody.size()) {
                    ExecStmt(st.thenBody[cur.bodyPc++], yieldedDelay);
                    if (yieldedDelay) return;
                }
                cur.bodyPc = 0;
                bool dummy = false;
                for (const auto& sst : st.stepStmts) ExecStmt(sst, dummy);
                ++cur.itersThisTick;
            }
            break;
        }
        }
    }

    void RunSetup() {
        if (setupDone) return;
        bool dummy = false;
        for (const auto& st : setupStmts) ExecStmt(st, dummy);
        setupDone = true;
        sleepUntilMs = 0.0;
    }

    void StepToTimeMs(double targetMs) {
        simTimeMs = targetMs;
        if (!setupDone) RunSetup();
        if (loopStmts.empty()) return;
        int guard = 0;
        while (simTimeMs >= sleepUntilMs && guard++ < 64) {
            if (loopPc >= loopStmts.size()) loopPc = 0;
            bool yielded = false;
            const std::size_t curIdx = loopPc;
            ExecStmt(loopStmts[curIdx], yielded);
            const bool insidePausedLoop = yielded &&
                (loopStmts[curIdx].kind == Stmt::Kind::ForLoop || loopStmts[curIdx].kind == Stmt::Kind::WhileLoop) &&
                loopCursors.count(&loopStmts[curIdx]) && loopCursors[&loopStmts[curIdx]].initialized;
            if (!insidePausedLoop) {
                ++loopPc;
            }
            if (yielded) break;
            if (loopPc >= loopStmts.size()) {
                loopPc = 0;
                break; // completed 1 full loop iteration at this time step
            }
        }
    }
};

} // namespace Detail

inline McuCoSimResult RunMcuCircuitCoSimulation(
    const std::vector<CircuitComponent>& circuit,
    const std::string& mcuCodeUtf8,
    McuBoardKind board,
    const std::vector<McuPinBinding>& bindingsIn,
    double durationSec,
    std::size_t numSteps = 400)
{
    McuCoSimResult res;
    if (circuit.empty() || durationSec <= 0.0 || numSteps < 2) {
        res.errorUtf8 = "Parametros invalidos para co-simulacion MCU.";
        return res;
    }

    std::vector<McuPinBinding> bindings = bindingsIn;
    for (const auto& b : bindings) {
        if (!IsValidPinForBoard(board, b.mcuPinName) || b.circuitNode <= 0) {
            res.errorUtf8 = "Pin MCU invalido o nodo desconectado: " + b.mcuPinName;
            return res;
        }
    }

    Detail::MiniMcuVm vm;
    if (!vm.Compile(mcuCodeUtf8, board, bindings)) {
        res.errorUtf8 = "No se encontro setup()/loop() valido en el codigo MCU.";
        return res;
    }

    // Execute setup() BEFORE building augmented GPIO drivers so pinMode(..., OUTPUT) in setup takes effect!
    vm.RunSetup();

    // Also scan loopStmts to detect any pins driven dynamically during loop()
    std::set<std::string> dynamicDrivenKeys;
    auto scanDriven = [&](auto& self, const std::vector<Detail::MiniMcuVm::Stmt>& stmts) -> void {
        for (const auto& st : stmts) {
            if (st.kind == Detail::MiniMcuVm::Stmt::Kind::DigitalWrite ||
                st.kind == Detail::MiniMcuVm::Stmt::Kind::AnalogWrite ||
                st.kind == Detail::MiniMcuVm::Stmt::Kind::PinMode) {
                dynamicDrivenKeys.insert(vm.ResolvePinKey(st.arg1));
            }
            self(self, st.thenBody);
            self(self, st.elseBody);
        }
    };
    scanDriven(scanDriven, vm.loopStmts);

    // Build augmented circuit with Thevenin GPIO drivers for Output/PwmOut/InputPullup/dynamically-driven pins
    int maxLoadNode = 0;
    for (const auto& c : circuit) {
        maxLoadNode = (std::max)(maxLoadNode, (std::max)(c.nodePos, (std::max)(c.nodeNeg, c.nodeOut)));
        maxLoadNode = (std::max)(maxLoadNode, (std::max)(c.nodeCtrlPos, c.nodeCtrlNeg));
        for (int en : c.extraNodes) maxLoadNode = (std::max)(maxLoadNode, en);
    }
    for (const auto& b : bindings) maxLoadNode = (std::max)(maxLoadNode, b.circuitNode);
    const int origMaxLoadNode = maxLoadNode;

    std::vector<CircuitComponent> augCircuit = circuit;
    std::map<std::string, std::size_t> pinSourceIdx;
    for (const auto& b : bindings) {
        const std::string cKey = Detail::MiniMcuVm::CanonicalPinKey(b.mcuPinName);
        const bool isDriven = (b.mode == McuPinMode::Output || b.mode == McuPinMode::PwmOut ||
                               b.mode == McuPinMode::InputPullup || dynamicDrivenKeys.count(cKey) > 0);
        if (isDriven) {
            const int auxNode = ++maxLoadNode;
            CircuitComponent vs;
            vs.kind = ComponentKind::VoltageSource;
            vs.name = "V_MCU_" + b.mcuPinName;
            vs.nodePos = auxNode;
            vs.nodeNeg = 0;
            vs.value = (b.mode == McuPinMode::InputPullup) ? vm.profile.vddV : b.lastDrivenV;
            pinSourceIdx[b.mcuPinName] = augCircuit.size();
            augCircuit.push_back(vs);

            CircuitComponent rOut;
            rOut.kind = ComponentKind::Resistor;
            rOut.name = "R_MCU_" + b.mcuPinName;
            rOut.nodePos = auxNode;
            rOut.nodeNeg = b.circuitNode;
            rOut.value = (b.mode == McuPinMode::InputPullup) ? 30000.0
                : ((b.mode == McuPinMode::Output || b.mode == McuPinMode::PwmOut) ? vm.profile.gpioRonOhm : 1e9);
            augCircuit.push_back(rOut);
        }
    }

    const double dt = durationSec / static_cast<double>(numSteps);
    CircuitTransientSession sess;
    std::string sessErr;
    if (!sess.Initialize(augCircuit, dt, numSteps + 16, false, &sessErr)) {
        res.errorUtf8 = sessErr;
        return res;
    }

    res.circuitWaveforms.probedNode = bindings.empty() ? 1 : bindings.front().circuitNode;
    res.circuitWaveforms.durationSec = durationSec;
    res.circuitWaveforms.dtSec = dt;

    OscilloscopeFrame frame{};
    for (std::size_t step = 0; step <= numSteps; ++step) {
        const double t = static_cast<double>(step) * dt;
        vm.StepToTimeMs(t * 1000.0);

        for (const auto& b : bindings) {
            double driveV = (b.mode == McuPinMode::InputPullup) ? vm.profile.vddV : b.lastDrivenV;
            if (b.mode == McuPinMode::PwmOut) {
                const double cyc = b.pwmFreqHz * t;
                const double frac = cyc - std::floor(cyc);
                driveV = (frac < b.pwmDuty01) ? vm.profile.vddV : 0.0;
            }
            if (pinSourceIdx.count(b.mcuPinName)) {
                sess.SetComponentValue("V_MCU_" + b.mcuPinName, driveV);
                const double rPin = (b.mode == McuPinMode::InputPullup) ? 30000.0
                    : ((b.mode == McuPinMode::Output || b.mode == McuPinMode::PwmOut) ? vm.profile.gpioRonOhm : 1e9);
                sess.SetComponentValue("R_MCU_" + b.mcuPinName, rPin);
            }
        }

        if (!sess.StepFrame(dt, frame, &sessErr)) {
            res.errorUtf8 = sessErr;
            return res;
        }

        res.circuitWaveforms.timeSec.push_back(t);
        for (const auto& ch : frame.channelRing) {
            if (!ch.second.empty() && ch.first.size() > 3 && ch.first[0] == 'V' && ch.first[1] == '(') {
                const int nId = std::stoi(ch.first.substr(2, ch.first.size() - 3));
                if (nId <= origMaxLoadNode) {
                    res.circuitWaveforms.nodeWaveforms[nId].push_back(ch.second.back());
                }
            }
        }
        for (auto& b : bindings) {
            const std::string chKey = "V(" + std::to_string(b.circuitNode) + ")";
            double vNode = 0.0;
            auto itCh = frame.channelRing.find(chKey);
            if (itCh != frame.channelRing.end() && !itCh->second.empty()) vNode = itCh->second.back();
            b.lastSampledV = vNode;
            res.pinVoltages[b.mcuPinName].push_back(vNode);
        }
    }

    const auto& wf = res.circuitWaveforms.nodeWaveforms[res.circuitWaveforms.probedNode];
    if (!wf.empty()) {
        res.circuitWaveforms.vMax = *std::max_element(wf.begin(), wf.end());
        res.circuitWaveforms.vMin = *std::min_element(wf.begin(), wf.end());
        double sum = 0.0, sumSq = 0.0;
        for (double v : wf) { sum += v; sumSq += v * v; }
        res.circuitWaveforms.vDcAvg = sum / static_cast<double>(wf.size());
        res.circuitWaveforms.vRms = std::sqrt(sumSq / static_cast<double>(wf.size()));
    }
    res.serialLog = std::move(vm.serialLog);
    res.circuitWaveforms.ok = true;
    res.ok = true;
    return res;
}

// -------- 3. Control Block Diagrams (Mason / SFG / Routh / RK4 Step / Bode) (F-256) --------

enum class ControlNodeKind : std::uint8_t {
    Input = 0,
    Output = 1,
    Gain = 2,
    TransferFunction = 3,
    Integrator = 4,
    Differentiator = 5,
    Pid = 6,
    SumJunction = 7,
    BranchPoint = 8,
    Saturation = 9
};

struct ControlBlockNode {
    std::uint64_t id = 0;
    ControlNodeKind kind = ControlNodeKind::TransferFunction;
    std::string label;
    Vec2 pos{};
    double width = 96.0;
    double height = 52.0;
    RationalPoly tf{ { { 1.0 } }, { { 1.0 } } };
    double gainK = 1.0;
    double kp = 1.0;
    double ki = 0.0;
    double kd = 0.0;
    double satMin = -1e9;
    double satMax = 1e9;
};

struct ControlEdge {
    std::uint64_t id = 0;
    std::uint64_t fromId = 0;
    std::uint64_t toId = 0;
    int sign = 1; // +1 or -1 at SumJunction
    std::vector<Vec2> waypoints;
};

struct ControlBlockDiagram {
    std::string title = "ControlSystem";
    std::vector<ControlBlockNode> nodes;
    std::vector<ControlEdge> edges;
};

struct ControlSystemAnalysis {
    bool ok = false;
    RationalPoly closedLoopTf;
    std::string closedLoopUtf8;
    std::string closedLoopLatex;
    std::string masonFormulaLatex;
    std::vector<std::string> forwardPathsDesc;
    std::vector<std::string> feedbackLoopsDesc;
    std::vector<std::complex<double>> poles;
    std::vector<std::complex<double>> zeros;
    bool isStableRouth = false;
    std::vector<BodePoint> bode;
    std::vector<double> stepTimeSec;
    std::vector<double> stepOutput;
    std::vector<double> impulseOutput;
    std::string errorUtf8;
};

inline RationalPoly GetControlNodeTransferFunction(const ControlBlockNode& node) {
    switch (node.kind) {
    case ControlNodeKind::Input:
    case ControlNodeKind::Output:
    case ControlNodeKind::SumJunction:
    case ControlNodeKind::BranchPoint:
    case ControlNodeKind::Saturation:
        return { Poly1D{ { 1.0 } }, Poly1D{ { 1.0 } } };
    case ControlNodeKind::Gain:
        return { Poly1D{ { node.gainK } }, Poly1D{ { 1.0 } } };
    case ControlNodeKind::Integrator:
        return { Poly1D{ { 1.0 } }, Poly1D{ { 0.0, 1.0 } } };
    case ControlNodeKind::Differentiator:
        return { Poly1D{ { 0.0, 1.0 } }, Poly1D{ { 1.0 } } };
    case ControlNodeKind::Pid: {
        RationalPoly r;
        r.num = Poly1D{ { node.ki, node.kp, node.kd } };
        r.den = (std::abs(node.ki) > 1e-12) ? Poly1D{ { 0.0, 1.0 } } : Poly1D{ { 1.0 } };
        r.Normalize(true);
        return r;
    }
    case ControlNodeKind::TransferFunction: {
        RationalPoly r = node.tf;
        r.Normalize(true);
        return r;
    }
    }
    return { Poly1D{ { 1.0 } }, Poly1D{ { 1.0 } } };
}

inline ControlSystemAnalysis AnalyzeControlBlockDiagram(
    const ControlBlockDiagram& diag,
    double stepDurationSec = 5.0,
    std::size_t stepSamples = 200,
    double bodeFStartHz = 0.01,
    double bodeFEndHz = 1000.0,
    std::size_t bodePoints = 41)
{
    ControlSystemAnalysis res;
    const std::size_t N = diag.nodes.size();
    if (N == 0) {
        res.errorUtf8 = "Diagrama de bloques vacio.";
        return res;
    }

    std::map<std::uint64_t, std::size_t> idToIdx;
    std::size_t inIdx = N, outIdx = N;
    for (std::size_t i = 0; i < N; ++i) {
        idToIdx[diag.nodes[i].id] = i;
        if (diag.nodes[i].kind == ControlNodeKind::Input) inIdx = i;
        if (diag.nodes[i].kind == ControlNodeKind::Output) outIdx = i;
    }
    if (inIdx >= N || outIdx >= N) {
        res.errorUtf8 = "El diagrama requiere un nodo Input y un nodo Output.";
        return res;
    }

    // Build adjacency list & SFG system (I - G(s)) * X(s) = e_in
    std::vector<std::vector<std::pair<std::size_t, RationalPoly>>> adj(N);
    std::vector<std::vector<RationalPoly>> A(N, std::vector<RationalPoly>(N, { Poly1D{ { 0.0 } }, Poly1D{ { 1.0 } } }));
    std::vector<RationalPoly> rhs(N, { Poly1D{ { 0.0 } }, Poly1D{ { 1.0 } } });
    for (std::size_t i = 0; i < N; ++i) A[i][i] = { Poly1D{ { 1.0 } }, Poly1D{ { 1.0 } } };
    rhs[inIdx] = { Poly1D{ { 1.0 } }, Poly1D{ { 1.0 } } };

    for (const auto& e : diag.edges) {
        if (!idToIdx.count(e.fromId) || !idToIdx.count(e.toId)) continue;
        const std::size_t u = idToIdx[e.fromId];
        const std::size_t v = idToIdx[e.toId];
        RationalPoly gain = GetControlNodeTransferFunction(diag.nodes[v]);
        if (e.sign < 0) {
            for (double& c : gain.num.coeffs) c = -c;
        }
        adj[u].push_back({ v, gain });
        A[v][u] = A[v][u] - gain;
    }

    // Enumerate simple forward paths and feedback loops for Mason diagnostic breakdown
    {
        std::vector<bool> vis(N, false);
        std::vector<std::size_t> path;
        auto dfsPath = [&](auto& self, std::size_t cur) -> void {
            if (cur == outIdx) {
                std::ostringstream oss;
                for (std::size_t k = 0; k < path.size(); ++k) {
                    if (k > 0) oss << " -> ";
                    oss << diag.nodes[path[k]].label;
                }
                res.forwardPathsDesc.push_back(oss.str());
                return;
            }
            if (res.forwardPathsDesc.size() >= 16) return;
            for (const auto& edge : adj[cur]) {
                if (!vis[edge.first]) {
                    vis[edge.first] = true;
                    path.push_back(edge.first);
                    self(self, edge.first);
                    path.pop_back();
                    vis[edge.first] = false;
                }
            }
        };
        vis[inIdx] = true;
        path.push_back(inIdx);
        dfsPath(dfsPath, inIdx);

        std::set<std::vector<std::size_t>> uniqueCycles;
        for (std::size_t start = 0; start < N; ++start) {
            std::vector<bool> cvis(N, false);
            std::vector<std::size_t> cpath{ start };
            auto dfsCycle = [&](auto& self, std::size_t cur) -> void {
                if (res.feedbackLoopsDesc.size() >= 16) return;
                for (const auto& edge : adj[cur]) {
                    if (edge.first == start && cpath.size() >= 2) {
                        std::vector<std::size_t> canon = cpath;
                        auto minIt = std::min_element(canon.begin(), canon.end());
                        std::rotate(canon.begin(), minIt, canon.end());
                        if (uniqueCycles.insert(canon).second) {
                            std::ostringstream oss;
                            for (std::size_t k = 0; k < canon.size(); ++k) {
                                if (k > 0) oss << " -> ";
                                oss << diag.nodes[canon[k]].label;
                            }
                            oss << " -> " << diag.nodes[canon[0]].label;
                            res.feedbackLoopsDesc.push_back(oss.str());
                        }
                    } else if (!cvis[edge.first] && edge.first > start) {
                        cvis[edge.first] = true;
                        cpath.push_back(edge.first);
                        self(self, edge.first);
                        cpath.pop_back();
                        cvis[edge.first] = false;
                    }
                }
            };
            cvis[start] = true;
            dfsCycle(dfsCycle, start);
        }
    }

    // Algebraic SFG Gaussian elimination over RationalPoly
    auto isZeroRat = [](const RationalPoly& rp) {
        return rp.num.Degree() == 0 && std::abs(rp.num.coeffs[0]) < 1e-12;
    };
    for (std::size_t col = 0; col < N; ++col) {
        std::size_t piv = col;
        while (piv < N && isZeroRat(A[piv][col])) ++piv;
        if (piv >= N) {
            res.errorUtf8 = "Lazo algebraico singular en diagrama de bloques.";
            return res;
        }
        if (piv != col) {
            std::swap(A[col], A[piv]);
            std::swap(rhs[col], rhs[piv]);
        }
        const RationalPoly diagVal = A[col][col];
        for (std::size_t c = col; c < N; ++c) A[col][c] = A[col][c] / diagVal;
        rhs[col] = rhs[col] / diagVal;
        for (std::size_t r = 0; r < N; ++r) {
            if (r == col || isZeroRat(A[r][col])) continue;
            const RationalPoly f = A[r][col];
            for (std::size_t c = col; c < N; ++c) A[r][c] = A[r][c] - (f * A[col][c]);
            rhs[r] = rhs[r] - (f * rhs[col]);
        }
    }

    res.closedLoopTf = rhs[outIdx];
    res.closedLoopTf.Normalize(true);
    res.closedLoopUtf8 = "H_cl(s) = " + res.closedLoopTf.FormatUtf8("s");
    res.closedLoopLatex = "H_{\\text{cl}}(s) = " + res.closedLoopTf.FormatLatex("s");
    res.masonFormulaLatex = "H_{\\text{cl}}(s) = \\frac{\\sum_k P_k \\Delta_k}{\\Delta} = " +
                            res.closedLoopTf.FormatLatex("s");

    res.zeros = FindPolynomialRootsAberth(res.closedLoopTf.num);
    res.poles = FindPolynomialRootsAberth(res.closedLoopTf.den);
    res.isStableRouth = ComputeRouthHurwitzStable(res.closedLoopTf.den);

    // Bode sweep of H_cl(s)
    if (bodePoints >= 2 && bodeFStartHz > 0.0 && bodeFEndHz > bodeFStartHz) {
        const double l0 = std::log10(bodeFStartHz);
        const double l1 = std::log10(bodeFEndHz);
        for (std::size_t i = 0; i < bodePoints; ++i) {
            const double f = std::pow(10.0, l0 + (static_cast<double>(i) / static_cast<double>(bodePoints - 1)) * (l1 - l0));
            const std::complex<double> H = res.closedLoopTf.Eval({ 0.0, 2.0 * kPi * f });
            const double db = 20.0 * std::log10((std::max)(1e-15, std::abs(H)));
            const double ph = std::arg(H) * (180.0 / kPi);
            res.bode.push_back({ f, db, ph });
        }
    }

    // RK4 Time-Domain Step & Impulse Response via Controllable Canonical Realization
    // (or per-node state-space + piecewise-linear algebraic graph RK4 when Saturation is present)
    if (stepDurationSec > 0.0 && stepSamples >= 2) {
        const double dt = stepDurationSec / static_cast<double>(stepSamples);
        res.stepTimeSec.reserve(stepSamples + 1);
        res.stepOutput.reserve(stepSamples + 1);
        res.impulseOutput.reserve(stepSamples + 1);

        bool hasSat = false;
        for (const auto& nd : diag.nodes) {
            if (nd.kind == ControlNodeKind::Saturation) {
                hasSat = true;
                break;
            }
        }

        if (hasSat) {
            std::vector<std::vector<double>> mIn(N, std::vector<double>(N, 0.0));
            for (const auto& e : diag.edges) {
                if (!idToIdx.count(e.fromId) || !idToIdx.count(e.toId)) continue;
                const std::size_t uIdx = idToIdx[e.fromId];
                const std::size_t vIdx = idToIdx[e.toId];
                mIn[vIdx][uIdx] += (e.sign < 0) ? -1.0 : 1.0;
            }

            const double tauD = (std::max)(2.0 * dt, 1e-4);

            struct NodeBlockSim {
                bool isSat = false;
                bool satActive = false;
                double satMin = -1e9;
                double satMax = 1e9;
                bool isPid = false;
                bool hasPidInt = false;
                bool hasPidDeriv = false;
                double kp = 0.0;
                double ki = 0.0;
                double kd = 0.0;
                std::size_t stateOffset = 0;
                std::size_t stateDim = 0;
                double dFeed = 0.0;
                std::vector<double> a;
                std::vector<double> c;
            };

            std::vector<NodeBlockSim> blocks(N);
            std::size_t totalStates = 0;
            for (std::size_t i = 0; i < N; ++i) {
                const auto& nd = diag.nodes[i];
                auto& blk = blocks[i];
                blk.stateOffset = totalStates;
                switch (nd.kind) {
                case ControlNodeKind::Input:
                case ControlNodeKind::Output:
                case ControlNodeKind::SumJunction:
                case ControlNodeKind::BranchPoint:
                    blk.dFeed = 1.0;
                    break;
                case ControlNodeKind::Gain:
                    blk.dFeed = nd.gainK;
                    break;
                case ControlNodeKind::Saturation:
                    blk.isSat = true;
                    blk.satActive = (nd.satMin <= nd.satMax);
                    blk.satMin = nd.satMin;
                    blk.satMax = nd.satMax;
                    blk.dFeed = 1.0;
                    break;
                case ControlNodeKind::Integrator:
                    blk.stateDim = 1;
                    blk.a = { 0.0 };
                    blk.c = { 1.0 };
                    blk.dFeed = 0.0;
                    break;
                case ControlNodeKind::Differentiator:
                    blk.stateDim = 1;
                    blk.a = { 1.0 / tauD };
                    blk.dFeed = 1.0 / tauD;
                    blk.c = { -1.0 / (tauD * tauD) };
                    break;
                case ControlNodeKind::Pid:
                    blk.isPid = true;
                    blk.kp = nd.kp;
                    blk.ki = nd.ki;
                    blk.kd = nd.kd;
                    blk.hasPidInt = (std::abs(nd.ki) > 1e-12);
                    blk.hasPidDeriv = (std::abs(nd.kd) > 1e-12);
                    blk.stateDim = (blk.hasPidInt ? 1u : 0u) + (blk.hasPidDeriv ? 1u : 0u);
                    blk.dFeed = nd.kp + (blk.hasPidDeriv ? (nd.kd / tauD) : 0.0);
                    break;
                case ControlNodeKind::TransferFunction: {
                    RationalPoly rp = GetControlNodeTransferFunction(nd);
                    while (rp.num.Degree() > rp.den.Degree()) {
                        rp.den = rp.den * Poly1D{ { 1.0, tauD } };
                    }
                    rp.Normalize(false);
                    const int degDNode = rp.den.Degree();
                    const int degNNode = rp.num.Degree();
                    if (degDNode == 0) {
                        blk.dFeed = (std::abs(rp.den.coeffs[0]) > 1e-12) ? (rp.num.coeffs[0] / rp.den.coeffs[0]) : 0.0;
                    } else {
                        const double leadNode = rp.den.coeffs[static_cast<std::size_t>(degDNode)];
                        blk.stateDim = static_cast<std::size_t>(degDNode);
                        blk.a.assign(blk.stateDim, 0.0);
                        blk.c.assign(blk.stateDim, 0.0);
                        std::vector<double> bNode(blk.stateDim + 1, 0.0);
                        for (int j = 0; j < degDNode; ++j) {
                            blk.a[static_cast<std::size_t>(j)] = rp.den.coeffs[static_cast<std::size_t>(j)] / leadNode;
                        }
                        for (int j = 0; j <= degNNode && j <= degDNode; ++j) {
                            bNode[static_cast<std::size_t>(j)] = rp.num.coeffs[static_cast<std::size_t>(j)] / leadNode;
                        }
                        blk.dFeed = bNode[blk.stateDim];
                        for (std::size_t j = 0; j < blk.stateDim; ++j) {
                            blk.c[j] = bNode[j] - blk.dFeed * blk.a[j];
                        }
                    }
                    break;
                }
                }
                totalStates += blk.stateDim;
            }

            auto solveRealLinear = [](std::vector<std::vector<double>>& mat, std::vector<double>& rhsVec) -> bool {
                const std::size_t sz = rhsVec.size();
                for (std::size_t col = 0; col < sz; ++col) {
                    std::size_t piv = col;
                    double best = std::abs(mat[col][col]);
                    for (std::size_t r = col + 1; r < sz; ++r) {
                        const double v = std::abs(mat[r][col]);
                        if (v > best) {
                            best = v;
                            piv = r;
                        }
                    }
                    if (best < 1e-12 || !std::isfinite(best)) return false;
                    if (piv != col) {
                        std::swap(mat[col], mat[piv]);
                        std::swap(rhsVec[col], rhsVec[piv]);
                    }
                    const double inv = 1.0 / mat[col][col];
                    for (std::size_t c = col; c < sz; ++c) mat[col][c] *= inv;
                    rhsVec[col] *= inv;
                    for (std::size_t r = 0; r < sz; ++r) {
                        if (r == col) continue;
                        const double f = mat[r][col];
                        if (f == 0.0) continue;
                        for (std::size_t c = col; c < sz; ++c) mat[r][c] -= f * mat[col][c];
                        rhsVec[r] -= f * rhsVec[col];
                    }
                }
                return true;
            };

            auto solveAlgebraic = [&](const std::vector<double>& x, double uExt, std::vector<int>& satModes,
                                      std::vector<double>& uOut, std::vector<double>& yOut) -> bool {
                std::vector<double> yState(N, 0.0);
                for (std::size_t i = 0; i < N; ++i) {
                    const auto& blk = blocks[i];
                    if (blk.isPid) {
                        double ys = 0.0;
                        if (blk.hasPidInt) ys += blk.ki * x[blk.stateOffset];
                        if (blk.hasPidDeriv) {
                            const std::size_t dOff = blk.stateOffset + (blk.hasPidInt ? 1u : 0u);
                            ys -= (blk.kd / (tauD * tauD)) * x[dOff];
                        }
                        yState[i] = ys;
                    } else if (blk.stateDim > 0) {
                        double ys = 0.0;
                        for (std::size_t j = 0; j < blk.stateDim; ++j) {
                            ys += blk.c[j] * x[blk.stateOffset + j];
                        }
                        yState[i] = ys;
                    }
                }

                uOut.assign(N, 0.0);
                yOut.assign(N, 0.0);
                std::vector<std::size_t> satIndices;
                for (std::size_t i = 0; i < N; ++i) {
                    if (blocks[i].isSat && blocks[i].satActive) satIndices.push_back(i);
                }
                std::set<std::vector<int>> seenModes;
                seenModes.insert(satModes);
                std::vector<bool> flippedInSearch(N, false);
                bool converged = false;
                for (int iter = 0; iter < 64; ++iter) {
                    std::vector<std::vector<double>> mMat(N, std::vector<double>(N, 0.0));
                    std::vector<double> rVec(N, 0.0);
                    for (std::size_t r = 0; r < N; ++r) {
                        const auto& blk = blocks[r];
                        double dEff = blk.dFeed;
                        double yOff = yState[r];
                        if (blk.isSat && blk.satActive) {
                            if (satModes[r] < 0) {
                                dEff = 0.0;
                                yOff = blk.satMin;
                            } else if (satModes[r] > 0) {
                                dEff = 0.0;
                                yOff = blk.satMax;
                            } else {
                                dEff = 1.0;
                                yOff = 0.0;
                            }
                        }
                        rVec[r] = yOff + (r == inIdx ? dEff * uExt : 0.0);
                        for (std::size_t col = 0; col < N; ++col) {
                            mMat[r][col] = (r == col ? 1.0 : 0.0) - dEff * mIn[r][col];
                        }
                    }
                    if (!solveRealLinear(mMat, rVec)) {
                        res.errorUtf8 = "Lazo algebraico singular en diagrama de bloques.";
                        return false;
                    }
                    yOut = std::move(rVec);
                    for (std::size_t i = 0; i < N; ++i) {
                        double ui = (i == inIdx) ? uExt : 0.0;
                        for (std::size_t p = 0; p < N; ++p) ui += mIn[i][p] * yOut[p];
                        uOut[i] = ui;
                    }
                    bool changed = false;
                    std::vector<int> nextModes = satModes;
                    for (std::size_t i : satIndices) {
                        const int nm = (uOut[i] < blocks[i].satMin) ? -1 : ((uOut[i] > blocks[i].satMax) ? 1 : 0);
                        if (nm != satModes[i]) {
                            nextModes[i] = nm;
                            flippedInSearch[i] = true;
                            changed = true;
                        }
                    }
                    if (!changed) {
                        converged = true;
                        break;
                    }
                    if (seenModes.count(nextModes) == 0) {
                        satModes = std::move(nextModes);
                        seenModes.insert(satModes);
                        continue;
                    }

                    // Mode cycle detected (e.g., +1 <-> -1 skipping linear mode 0 in high-gain algebraic loops):
                    // test linear mode (0) on cycling Saturation blocks before any fallback.
                    std::vector<std::size_t> cyclingIndices;
                    for (std::size_t i : satIndices) {
                        if (flippedInSearch[i]) cyclingIndices.push_back(i);
                    }
                    if (cyclingIndices.empty()) cyclingIndices = satIndices;

                    bool foundUnvisited = false;
                    std::vector<int> cand = satModes;
                    for (std::size_t i : cyclingIndices) cand[i] = 0;
                    if (seenModes.count(cand) == 0) {
                        satModes = std::move(cand);
                        seenModes.insert(satModes);
                        foundUnvisited = true;
                    }
                    if (!foundUnvisited) {
                        for (std::size_t i : cyclingIndices) {
                            std::vector<int> single = satModes;
                            single[i] = 0;
                            if (seenModes.count(single) == 0) {
                                satModes = std::move(single);
                                seenModes.insert(satModes);
                                foundUnvisited = true;
                                break;
                            }
                            std::vector<int> singleNext = nextModes;
                            singleNext[i] = 0;
                            if (seenModes.count(singleNext) == 0) {
                                satModes = std::move(singleNext);
                                seenModes.insert(satModes);
                                foundUnvisited = true;
                                break;
                            }
                        }
                    }
                    if (!foundUnvisited && cyclingIndices.size() <= 4) {
                        const std::size_t kCnt = cyclingIndices.size();
                        std::size_t totalCombos = 1;
                        for (std::size_t k = 0; k < kCnt; ++k) totalCombos *= 3;
                        const int modeOrder[3] = { 0, 1, -1 };
                        for (std::size_t code = 0; code < totalCombos; ++code) {
                            std::vector<int> combo = satModes;
                            std::size_t tmpCode = code;
                            for (std::size_t k = 0; k < kCnt; ++k) {
                                combo[cyclingIndices[k]] = modeOrder[tmpCode % 3];
                                tmpCode /= 3;
                            }
                            if (seenModes.count(combo) == 0) {
                                satModes = std::move(combo);
                                seenModes.insert(satModes);
                                foundUnvisited = true;
                                break;
                            }
                        }
                    }
                    if (!foundUnvisited) break;
                }
                if (!converged) {
                    for (int iter = 0; iter < 32; ++iter) {
                        std::vector<double> yNext(N, 0.0);
                        for (std::size_t i = 0; i < N; ++i) {
                            double ui = (i == inIdx) ? uExt : 0.0;
                            for (std::size_t p = 0; p < N; ++p) ui += mIn[i][p] * yOut[p];
                            if (blocks[i].isSat && blocks[i].satActive) {
                                yNext[i] = std::clamp(ui, blocks[i].satMin, blocks[i].satMax);
                            } else {
                                yNext[i] = yState[i] + blocks[i].dFeed * ui;
                            }
                        }
                        for (std::size_t i = 0; i < N; ++i) {
                            yOut[i] = 0.5 * yOut[i] + 0.5 * yNext[i];
                        }
                    }
                    for (std::size_t i = 0; i < N; ++i) {
                        double ui = (i == inIdx) ? uExt : 0.0;
                        for (std::size_t p = 0; p < N; ++p) ui += mIn[i][p] * yOut[p];
                        uOut[i] = ui;
                        if (blocks[i].isSat && blocks[i].satActive) {
                            yOut[i] = std::clamp(ui, blocks[i].satMin, blocks[i].satMax);
                        }
                    }
                }
                return true;
            };

            auto evalDeriv = [&](const std::vector<double>& x, double uExt, std::vector<int>& satModes,
                                 std::vector<double>& dxOut, double& yScalarOut) -> bool {
                std::vector<double> uVec;
                std::vector<double> yVec;
                if (!solveAlgebraic(x, uExt, satModes, uVec, yVec)) return false;
                yScalarOut = yVec[outIdx];
                dxOut.assign(totalStates, 0.0);
                for (std::size_t i = 0; i < N; ++i) {
                    const auto& blk = blocks[i];
                    if (blk.stateDim == 0) continue;
                    if (blk.isPid) {
                        if (blk.hasPidInt) dxOut[blk.stateOffset] = uVec[i];
                        if (blk.hasPidDeriv) {
                            const std::size_t dOff = blk.stateOffset + (blk.hasPidInt ? 1u : 0u);
                            dxOut[dOff] = uVec[i] - x[dOff] / tauD;
                        }
                    } else {
                        for (std::size_t j = 0; j + 1 < blk.stateDim; ++j) {
                            dxOut[blk.stateOffset + j] = x[blk.stateOffset + j + 1];
                        }
                        double last = uVec[i];
                        for (std::size_t j = 0; j < blk.stateDim; ++j) {
                            last -= blk.a[j] * x[blk.stateOffset + j];
                        }
                        dxOut[blk.stateOffset + blk.stateDim - 1] = last;
                    }
                }
                return true;
            };

            auto rk4StepGraph = [&](std::vector<double>& x, double uExt, std::vector<int>& satModes) -> bool {
                if (totalStates == 0) return true;
                std::vector<double> k1, k2, k3, k4;
                double yTmp = 0.0;
                if (!evalDeriv(x, uExt, satModes, k1, yTmp)) return false;
                std::vector<double> tmp(totalStates, 0.0);
                for (std::size_t i = 0; i < totalStates; ++i) tmp[i] = x[i] + 0.5 * dt * k1[i];
                if (!evalDeriv(tmp, uExt, satModes, k2, yTmp)) return false;
                for (std::size_t i = 0; i < totalStates; ++i) tmp[i] = x[i] + 0.5 * dt * k2[i];
                if (!evalDeriv(tmp, uExt, satModes, k3, yTmp)) return false;
                for (std::size_t i = 0; i < totalStates; ++i) tmp[i] = x[i] + dt * k3[i];
                if (!evalDeriv(tmp, uExt, satModes, k4, yTmp)) return false;
                for (std::size_t i = 0; i < totalStates; ++i) {
                    x[i] += (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
                }
                return true;
            };

            std::vector<double> xStep(totalStates, 0.0);
            std::vector<double> xImp(totalStates, 0.0);
            std::vector<int> satModesStep(N, 0);
            std::vector<int> satModesImp(N, 0);
            std::vector<double> dxDummy;
            for (std::size_t k = 0; k <= stepSamples; ++k) {
                const double t = static_cast<double>(k) * dt;
                double yStep = 0.0;
                double yImp = 0.0;
                if (!evalDeriv(xStep, 1.0, satModesStep, dxDummy, yStep)) return res;
                const double uImp = (k == 0) ? (1.0 / dt) : 0.0;
                if (!evalDeriv(xImp, uImp, satModesImp, dxDummy, yImp)) return res;
                res.stepTimeSec.push_back(t);
                res.stepOutput.push_back(yStep);
                res.impulseOutput.push_back(yImp);
                if (k == stepSamples) break;
                if (!rk4StepGraph(xStep, 1.0, satModesStep)) return res;
                if (!rk4StepGraph(xImp, uImp, satModesImp)) return res;
            }
        } else {
            Poly1D num = res.closedLoopTf.num;
            Poly1D den = res.closedLoopTf.den;
            const int degD = den.Degree();
            const int degN = num.Degree();

            if (degD == 0 || degN > degD) {
                const double dc = (std::abs(den.coeffs[0]) > 1e-12) ? (num.coeffs[0] / den.coeffs[0]) : 0.0;
                for (std::size_t i = 0; i <= stepSamples; ++i) {
                    res.stepTimeSec.push_back(static_cast<double>(i) * dt);
                    res.stepOutput.push_back(dc);
                    res.impulseOutput.push_back(i == 0 ? dc / dt : 0.0);
                }
            } else {
                const double lead = den.coeffs[static_cast<std::size_t>(degD)];
                std::vector<double> a(static_cast<std::size_t>(degD), 0.0);
                std::vector<double> b(static_cast<std::size_t>(degD + 1), 0.0);
                for (int i = 0; i < degD; ++i) {
                    a[static_cast<std::size_t>(i)] = den.coeffs[static_cast<std::size_t>(i)] / lead;
                }
                for (int i = 0; i <= degN; ++i) {
                    b[static_cast<std::size_t>(i)] = num.coeffs[static_cast<std::size_t>(i)] / lead;
                }
                const double b0 = b[static_cast<std::size_t>(degD)]; // direct feedthrough D
                std::vector<double> cVec(static_cast<std::size_t>(degD), 0.0);
                for (int i = 0; i < degD; ++i) {
                    cVec[static_cast<std::size_t>(i)] = b[static_cast<std::size_t>(i)] - b0 * a[static_cast<std::size_t>(i)];
                }

                auto deriv = [&](const std::vector<double>& x, double u) -> std::vector<double> {
                    std::vector<double> dx(static_cast<std::size_t>(degD), 0.0);
                    for (int i = 0; i < degD - 1; ++i) {
                        dx[static_cast<std::size_t>(i)] = x[static_cast<std::size_t>(i + 1)];
                    }
                    double last = u;
                    for (int i = 0; i < degD; ++i) {
                        last -= a[static_cast<std::size_t>(i)] * x[static_cast<std::size_t>(i)];
                    }
                    dx[static_cast<std::size_t>(degD - 1)] = last;
                    return dx;
                };
                auto evalY = [&](const std::vector<double>& x, double u) -> double {
                    double y = b0 * u;
                    for (int i = 0; i < degD; ++i) y += cVec[static_cast<std::size_t>(i)] * x[static_cast<std::size_t>(i)];
                    return y;
                };

                std::vector<double> xStep(static_cast<std::size_t>(degD), 0.0);
                std::vector<double> xImp(static_cast<std::size_t>(degD), 0.0);
                xImp[static_cast<std::size_t>(degD - 1)] = 1.0; // impulse delta(t) initial velocity

                for (std::size_t k = 0; k <= stepSamples; ++k) {
                    const double t = static_cast<double>(k) * dt;
                    res.stepTimeSec.push_back(t);
                    res.stepOutput.push_back(evalY(xStep, 1.0));
                    res.impulseOutput.push_back(evalY(xImp, 0.0));
                    if (k == stepSamples) break;

                    auto rk4Step = [&](std::vector<double>& x, double u) {
                        const auto k1 = deriv(x, u);
                        std::vector<double> tmp(x.size());
                        for (std::size_t i = 0; i < x.size(); ++i) tmp[i] = x[i] + 0.5 * dt * k1[i];
                        const auto k2 = deriv(tmp, u);
                        for (std::size_t i = 0; i < x.size(); ++i) tmp[i] = x[i] + 0.5 * dt * k2[i];
                        const auto k3 = deriv(tmp, u);
                        for (std::size_t i = 0; i < x.size(); ++i) tmp[i] = x[i] + dt * k3[i];
                        const auto k4 = deriv(tmp, u);
                        for (std::size_t i = 0; i < x.size(); ++i) {
                            x[i] += (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
                        }
                    };
                    rk4Step(xStep, 1.0);
                    rk4Step(xImp, 0.0);
                }
            }
        }
    }

    res.ok = true;
    return res;
}

// -------- 4. Engineering Flowchart Graph Model & SVG Exporter (F-256) --------

enum class FlowNodeKind : std::uint8_t {
    StartEnd = 0,
    Process = 1,
    Decision = 2,
    InputOutput = 3,
    Subroutine = 4,
    Connector = 5
};

struct FlowNode {
    std::uint64_t id = 0;
    FlowNodeKind kind = FlowNodeKind::Process;
    std::string textUtf8;
    Vec2 pos{};
    double width = 130.0;
    double height = 56.0;
};

struct FlowEdge {
    std::uint64_t id = 0;
    std::uint64_t fromId = 0;
    std::uint64_t toId = 0;
    std::string branchLabel;
    std::vector<Vec2> waypoints;
};

struct FlowchartDiagram {
    std::string title = "Flowchart";
    std::vector<FlowNode> nodes;
    std::vector<FlowEdge> edges;
};

struct FlowchartDiagnostic {
    bool isError = false;
    std::uint64_t objectId = 0;
    std::string messageUtf8;
};

inline std::vector<FlowchartDiagnostic> ValidateFlowchart(const FlowchartDiagram& fc) {
    std::vector<FlowchartDiagnostic> diags;
    if (fc.nodes.empty()) {
        diags.push_back({ true, 0, "Diagrama de flujo vacio." });
        return diags;
    }

    std::map<std::uint64_t, std::size_t> idMap;
    std::vector<std::uint64_t> starts;
    for (std::size_t i = 0; i < fc.nodes.size(); ++i) {
        idMap[fc.nodes[i].id] = i;
        if (fc.nodes[i].kind == FlowNodeKind::StartEnd) starts.push_back(fc.nodes[i].id);
    }
    if (starts.empty()) {
        diags.push_back({ true, 0, "Falta nodo StartEnd de inicio/fin." });
    }

    std::map<std::uint64_t, std::vector<std::uint64_t>> outAdj;
    std::map<std::uint64_t, std::vector<std::uint64_t>> revAdj;
    for (const auto& e : fc.edges) {
        if (idMap.count(e.fromId) && idMap.count(e.toId)) {
            outAdj[e.fromId].push_back(e.toId);
            revAdj[e.toId].push_back(e.fromId);
        }
    }

    for (const auto& nd : fc.nodes) {
        const std::size_t outDeg = outAdj[nd.id].size();
        if (nd.kind == FlowNodeKind::Decision && outDeg < 2) {
            diags.push_back({ true, nd.id, "Nodo Decision '" + nd.textUtf8 + "' requiere al menos 2 ramas de salida." });
        }
        if (nd.kind != FlowNodeKind::StartEnd && outDeg == 0) {
            diags.push_back({ false, nd.id, "Nodo sin salida (dead-end): " + nd.textUtf8 });
        }
    }

    std::set<std::uint64_t> visited;
    if (!starts.empty()) {
        std::vector<std::uint64_t> q{ starts.front() };
        visited.insert(starts.front());
        while (!q.empty()) {
            const std::uint64_t cur = q.back();
            q.pop_back();
            for (std::uint64_t nxt : outAdj[cur]) {
                if (visited.insert(nxt).second) q.push_back(nxt);
            }
        }
        for (const auto& nd : fc.nodes) {
            if (!visited.count(nd.id)) {
                diags.push_back({ false, nd.id, "Nodo inalcanzable desde inicio: " + nd.textUtf8 });
            }
        }
    }

    // Reverse BFS from terminal StartEnd nodes to detect infinite cycles with no exit to End
    std::set<std::uint64_t> canReachEnd;
    std::vector<std::uint64_t> revQ;
    for (const auto& nd : fc.nodes) {
        if (nd.kind == FlowNodeKind::StartEnd && outAdj[nd.id].empty()) {
            canReachEnd.insert(nd.id);
            revQ.push_back(nd.id);
        }
    }
    while (!revQ.empty()) {
        const std::uint64_t cur = revQ.back();
        revQ.pop_back();
        for (std::uint64_t prev : revAdj[cur]) {
            if (canReachEnd.insert(prev).second) revQ.push_back(prev);
        }
    }
    for (const auto& nd : fc.nodes) {
        const bool isReachable = starts.empty() || visited.count(nd.id) > 0;
        if (isReachable && !outAdj[nd.id].empty() && !canReachEnd.count(nd.id)) {
            diags.push_back({ true, nd.id, "Ciclo infinito sin salida a nodo Fin (StartEnd): " + nd.textUtf8 });
        }
    }
    return diags;
}

namespace Detail {

inline std::string EscapeXmlFlow(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return out;
}

} // namespace Detail

inline std::string ExportFlowchartToSvg(const FlowchartDiagram& fc) {
    double minX = 0.0, minY = 0.0, maxX = 480.0, maxY = 320.0;
    if (!fc.nodes.empty()) {
        minX = 1e9; minY = 1e9; maxX = -1e9; maxY = -1e9;
        for (const auto& n : fc.nodes) {
            minX = (std::min)(minX, n.pos.x - n.width);
            maxX = (std::max)(maxX, n.pos.x + n.width);
            minY = (std::min)(minY, n.pos.y - n.height);
            maxY = (std::max)(maxY, n.pos.y + n.height);
        }
    }
    const double w = (std::max)(360.0, maxX - minX);
    const double h = (std::max)(240.0, maxY - minY);

    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"" << minX << " " << minY << " " << w << " " << h << "\">\n";
    svg << "<defs><marker id=\"fc-arrow\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"6\" markerHeight=\"6\" orient=\"auto\">"
        << "<path d=\"M 0 1 L 10 5 L 0 9 z\" fill=\"#334155\"/></marker></defs>\n";
    svg << "<rect x=\"" << minX << "\" y=\"" << minY << "\" width=\"" << w << "\" height=\"" << h << "\" fill=\"#f8fafc\"/>\n";

    std::map<std::uint64_t, const FlowNode*> nodeMap;
    for (const auto& n : fc.nodes) nodeMap[n.id] = &n;

    for (const auto& e : fc.edges) {
        auto itA = nodeMap.find(e.fromId);
        auto itB = nodeMap.find(e.toId);
        if (itA == nodeMap.end() || itB == nodeMap.end()) continue;
        const Vec2 a = itA->second->pos;
        const Vec2 b = itB->second->pos;
        svg << "<polyline points=\"" << a.x << "," << a.y << " ";
        for (const auto& wp : e.waypoints) svg << wp.x << "," << wp.y << " ";
        svg << b.x << "," << b.y << "\" fill=\"none\" stroke=\"#334155\" stroke-width=\"1.8\" marker-end=\"url(#fc-arrow)\"/>\n";
        if (!e.branchLabel.empty()) {
            svg << "<text x=\"" << (0.5 * (a.x + b.x) + 6.0) << "\" y=\"" << (0.5 * (a.y + b.y) - 4.0)
                << "\" font-family=\"Segoe UI\" font-size=\"11\" fill=\"#0f172a\">"
                << Detail::EscapeXmlFlow(e.branchLabel) << "</text>\n";
        }
    }

    for (const auto& n : fc.nodes) {
        const double x0 = n.pos.x - 0.5 * n.width;
        const double y0 = n.pos.y - 0.5 * n.height;
        if (n.kind == FlowNodeKind::StartEnd) {
            svg << "<rect x=\"" << x0 << "\" y=\"" << y0 << "\" width=\"" << n.width << "\" height=\"" << n.height
                << "\" rx=\"" << (0.5 * n.height) << "\" fill=\"#dbeafe\" stroke=\"#1d4ed8\" stroke-width=\"1.8\"/>\n";
        } else if (n.kind == FlowNodeKind::Decision) {
            svg << "<polygon points=\"" << n.pos.x << "," << y0 << " " << (x0 + n.width) << "," << n.pos.y << " "
                << n.pos.x << "," << (y0 + n.height) << " " << x0 << "," << n.pos.y
                << "\" fill=\"#fef3c7\" stroke=\"#b45309\" stroke-width=\"1.8\"/>\n";
        } else if (n.kind == FlowNodeKind::InputOutput) {
            const double sk = 14.0;
            svg << "<polygon points=\"" << (x0 + sk) << "," << y0 << " " << (x0 + n.width) << "," << y0 << " "
                << (x0 + n.width - sk) << "," << (y0 + n.height) << " " << x0 << "," << (y0 + n.height)
                << "\" fill=\"#dcfce7\" stroke=\"#15803d\" stroke-width=\"1.8\"/>\n";
        } else if (n.kind == FlowNodeKind::Connector) {
            svg << "<circle cx=\"" << n.pos.x << "\" cy=\"" << n.pos.y << "\" r=\"" << (0.35 * n.height)
                << "\" fill=\"#f1f5f9\" stroke=\"#475569\" stroke-width=\"1.8\"/>\n";
        } else {
            svg << "<rect x=\"" << x0 << "\" y=\"" << y0 << "\" width=\"" << n.width << "\" height=\"" << n.height
                << "\" rx=\"4\" fill=\"#eff6ff\" stroke=\"#2563eb\" stroke-width=\"1.8\"/>\n";
        }
        svg << "<text x=\"" << n.pos.x << "\" y=\"" << (n.pos.y + 4.0)
            << "\" text-anchor=\"middle\" font-family=\"Segoe UI\" font-size=\"12\" fill=\"#0f172a\">"
            << Detail::EscapeXmlFlow(n.textUtf8) << "</text>\n";
    }
    svg << "</svg>\n";
    return svg.str();
}

} // namespace DocStudio::Circuit
