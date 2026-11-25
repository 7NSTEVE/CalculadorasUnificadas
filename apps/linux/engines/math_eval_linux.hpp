#pragma once

#include "unified_engines.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>

namespace CasioLinux::detail {

enum class AngleMode : int {
    Deg = 0,
    Rad = 1,
    Gra = 2
};

struct EvalError : std::runtime_error {
    bool isMathError;
    EvalError(bool mathErr, const char* msg)
        : std::runtime_error(msg), isMathError(mathErr) {}
};

inline std::string FormatDoubleCompact(double val, int precision = 10)
{
    if (std::isnan(val) || std::isinf(val)) return "Math ERROR";
    if (std::abs(val) < 1e-12) return "0";
    if (std::abs(val - std::round(val)) < 1e-10 && std::abs(val) < 1e12) {
        return std::to_string(static_cast<long long>(std::llround(val)));
    }
    std::ostringstream oss;
    oss << std::setprecision(precision) << val;
    return oss.str();
}

inline bool StartsWithCaseInsensitive(const std::string& s, size_t pos, const char* lit, size_t len)
{
    if (pos + len > s.size()) return false;
    for (size_t i = 0; i < len; ++i) {
        if (std::tolower(static_cast<unsigned char>(s[pos + i])) !=
            std::tolower(static_cast<unsigned char>(lit[i]))) {
            return false;
        }
    }
    return true;
}

inline std::string FormatAnsCompact(double val)
{
    if (!std::isfinite(val)) return "0";
    if (std::abs(val) < 1e-12) return "0";
    if (std::abs(val - std::round(val)) < 1e-10 && std::abs(val) < 1e12) {
        return std::to_string(static_cast<long long>(std::llround(val)));
    }
    std::ostringstream oss;
    oss << std::setprecision(15) << val;
    return oss.str();
}

inline bool IsSciExpAfterUtf8(const std::string& expr, size_t eIdx)
{
    if (eIdx + 1 >= expr.size()) return false;
    const unsigned char next = static_cast<unsigned char>(expr[eIdx + 1]);
    if (next >= '0' && next <= '9') return true;
    if ((next == '+' || next == '-') && eIdx + 2 < expr.size()) {
        const unsigned char d = static_cast<unsigned char>(expr[eIdx + 2]);
        return (d >= '0' && d <= '9');
    }
    if (next == 0xE2 && eIdx + 4 < expr.size() &&
        static_cast<unsigned char>(expr[eIdx + 2]) == 0x88 &&
        static_cast<unsigned char>(expr[eIdx + 3]) == 0x92) {
        const unsigned char d = static_cast<unsigned char>(expr[eIdx + 4]);
        return (d >= '0' && d <= '9');
    }
    return false;
}

inline std::string PreprocessMathUtf8(const std::string& expr, double lastAns)
{
    std::string raw;
    raw.reserve(expr.size() + 16);

    const std::string ansToken = "(" + FormatAnsCompact(lastAns) + ")";
    constexpr const char* kPiStr = "(3.14159265358979323846)";
    constexpr const char* kEulerStr = "(2.71828182845904523536)";

    for (size_t i = 0; i < expr.size();) {
        const unsigned char c = static_cast<unsigned char>(expr[i]);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }
        // UTF-8 × (U+00D7: C3 97)
        if (c == 0xC3 && i + 1 < expr.size() && static_cast<unsigned char>(expr[i + 1]) == 0x97) {
            raw.push_back('*');
            i += 2;
            continue;
        }
        // UTF-8 ÷ (U+00F7: C3 B7)
        if (c == 0xC3 && i + 1 < expr.size() && static_cast<unsigned char>(expr[i + 1]) == 0xB7) {
            raw.push_back('/');
            i += 2;
            continue;
        }
        // UTF-8 − (U+2212: E2 88 92)
        if (c == 0xE2 && i + 2 < expr.size() &&
            static_cast<unsigned char>(expr[i + 1]) == 0x88 &&
            static_cast<unsigned char>(expr[i + 2]) == 0x92) {
            raw.push_back('-');
            i += 3;
            continue;
        }
        // UTF-8 π (U+03C0: CF 80)
        if (c == 0xCF && i + 1 < expr.size() && static_cast<unsigned char>(expr[i + 1]) == 0x80) {
            raw += kPiStr;
            i += 2;
            continue;
        }
        // UTF-8 √ (U+221A: E2 88 9A)
        if (c == 0xE2 && i + 2 < expr.size() &&
            static_cast<unsigned char>(expr[i + 1]) == 0x88 &&
            static_cast<unsigned char>(expr[i + 2]) == 0x9A) {
            raw += "sqrt";
            i += 3;
            continue;
        }
        // Ans keyword
        if (StartsWithCaseInsensitive(expr, i, "ans", 3) &&
            (i == 0 || !std::isalpha(static_cast<unsigned char>(expr[i - 1]))) &&
            (i + 3 >= expr.size() || !std::isalpha(static_cast<unsigned char>(expr[i + 3])))) {
            raw += ansToken;
            i += 3;
            continue;
        }
        // pi keyword
        if (StartsWithCaseInsensitive(expr, i, "pi", 2) &&
            (i == 0 || !std::isalpha(static_cast<unsigned char>(expr[i - 1]))) &&
            (i + 2 >= expr.size() || !std::isalpha(static_cast<unsigned char>(expr[i + 2])))) {
            raw += kPiStr;
            i += 2;
            continue;
        }
        // Safe 'x' / 'X' as multiplication ONLY when NOT adjacent to alpha letters (preserves max, exp, etc.)
        if (c == 'x' || c == 'X') {
            const bool prevAlpha = (i > 0) && std::isalpha(static_cast<unsigned char>(expr[i - 1]));
            const bool nextAlpha = (i + 1 < expr.size()) && std::isalpha(static_cast<unsigned char>(expr[i + 1]));
            if (!prevAlpha && !nextAlpha) {
                raw.push_back('*');
                ++i;
                continue;
            }
        }
        // Standalone 'e' constant (not scientific notation after digit/dot and not identifier like "exp"/"ceil")
        if (c == 'e' || c == 'E') {
            const bool prevNum = !raw.empty() &&
                (std::isdigit(static_cast<unsigned char>(raw.back())) || raw.back() == '.');
            const bool isSciExpInput = prevNum && IsSciExpAfterUtf8(expr, i);
            const bool nextAlpha = (i + 1 < expr.size()) &&
                std::isalpha(static_cast<unsigned char>(expr[i + 1]));
            const bool prevAlpha = (i > 0) &&
                std::isalpha(static_cast<unsigned char>(expr[i - 1]));
            if (!isSciExpInput && !prevAlpha && !nextAlpha) {
                raw += kEulerStr;
                ++i;
                continue;
            }
        }

        raw.push_back(static_cast<char>(std::tolower(c)));
        ++i;
    }

    // Insert implicit multiplication (e.g. "2(3)", "(2)(3)", "2sin(30)", ")sin(30)")
    std::string out;
    out.reserve(raw.size() + 8);
    for (size_t i = 0; i < raw.size(); ++i) {
        const char cur = raw[i];
        if (!out.empty()) {
            const char prev = out.back();
            const bool prevEndValue =
                std::isdigit(static_cast<unsigned char>(prev)) ||
                prev == ')' || prev == '!' || prev == '%';
            const bool isSciExp = (cur == 'e') &&
                (std::isdigit(static_cast<unsigned char>(prev)) || prev == '.') &&
                (i + 1 < raw.size()) &&
                (std::isdigit(static_cast<unsigned char>(raw[i + 1])) || raw[i + 1] == '+' || raw[i + 1] == '-');
            const bool isLog2Call = (cur == '(' && out.size() >= 4 &&
                out.compare(out.size() - 4, 4, "log2") == 0);
            const bool curStartFactor =
                (cur == '(' && !isLog2Call) ||
                (std::isalpha(static_cast<unsigned char>(cur)) && !isSciExp);
            if (prevEndValue && curStartFactor) {
                out.push_back('*');
            }
        }
        out.push_back(cur);
    }
    return out;
}

class MathParser {
public:
    MathParser(const std::string& s, AngleMode mode)
        : s_(s), mode_(mode) {}

    double ParseAll()
    {
        double val = ParseExpression();
        if (pos_ < s_.size()) {
            throw EvalError(false, "Syntax ERROR");
        }
        if (std::isnan(val) || std::isinf(val)) {
            throw EvalError(true, "Math ERROR");
        }
        return val;
    }

private:
    const std::string& s_;
    AngleMode mode_;
    size_t pos_ = 0;

    static constexpr double kPi = 3.14159265358979323846;

    static double Factorial(double val)
    {
        if (val < 0.0 || val > 170.0 || std::floor(val) != val) {
            throw EvalError(true, "Math ERROR");
        }
        double res = 1.0;
        const int n = static_cast<int>(val);
        for (int i = 2; i <= n; ++i) res *= static_cast<double>(i);
        return res;
    }

    double ToRad(double arg) const
    {
        if (mode_ == AngleMode::Deg) return arg * kPi / 180.0;
        if (mode_ == AngleMode::Gra) return arg * kPi / 200.0;
        return arg;
    }

    double FromRad(double rad) const
    {
        if (mode_ == AngleMode::Deg) return rad * 180.0 / kPi;
        if (mode_ == AngleMode::Gra) return rad * 200.0 / kPi;
        return rad;
    }

    double ApplyFunc(const char* name, double arg) const
    {
        const std::string fn(name);
        if (fn == "sqrt") { if (arg < 0.0) throw EvalError(true, "Math ERROR"); return std::sqrt(arg); }
        if (fn == "cbrt") return std::cbrt(arg);
        if (fn == "ln") { if (arg <= 0.0) throw EvalError(true, "Math ERROR"); return std::log(arg); }
        if (fn == "log") { if (arg <= 0.0) throw EvalError(true, "Math ERROR"); return std::log10(arg); }
        if (fn == "log2") { if (arg <= 0.0) throw EvalError(true, "Math ERROR"); return std::log2(arg); }
        if (fn == "exp") return std::exp(arg);
        if (fn == "abs") return std::abs(arg);
        if (fn == "floor") return std::floor(arg);
        if (fn == "ceil") return std::ceil(arg);
        if (fn == "round") return std::round(arg);
        if (fn == "trunc") return std::trunc(arg);
        if (fn == "frac") return arg - std::trunc(arg);
        if (fn == "sgn") return (arg > 0.0) ? 1.0 : ((arg < 0.0) ? -1.0 : 0.0);
        if (fn == "deg") return arg * 180.0 / kPi;
        if (fn == "rad") return arg * kPi / 180.0;
        if (fn == "sinh") return std::sinh(arg);
        if (fn == "cosh") return std::cosh(arg);
        if (fn == "tanh") return std::tanh(arg);
        if (fn == "asinh") return std::asinh(arg);
        if (fn == "acosh") { if (arg < 1.0) throw EvalError(true, "Math ERROR"); return std::acosh(arg); }
        if (fn == "atanh") { if (arg <= -1.0 || arg >= 1.0) throw EvalError(true, "Math ERROR"); return std::atanh(arg); }
        if (fn == "sin") {
            double v = std::sin(ToRad(arg));
            if (std::abs(v) < 1e-14) v = 0.0;
            return v;
        }
        if (fn == "cos") {
            double v = std::cos(ToRad(arg));
            if (std::abs(v) < 1e-14) v = 0.0;
            return v;
        }
        if (fn == "tan") {
            const double r = ToRad(arg);
            if (std::abs(std::cos(r)) < 1e-13) throw EvalError(true, "Math ERROR");
            double v = std::tan(r);
            if (std::abs(v) < 1e-14) v = 0.0;
            return v;
        }
        if (fn == "asin") { if (arg < -1.0 || arg > 1.0) throw EvalError(true, "Math ERROR"); return FromRad(std::asin(arg)); }
        if (fn == "acos") { if (arg < -1.0 || arg > 1.0) throw EvalError(true, "Math ERROR"); return FromRad(std::acos(arg)); }
        if (fn == "atan") return FromRad(std::atan(arg));
        throw EvalError(false, "Syntax ERROR");
    }

    double ApplyFunc2(const char* name, double a, double b) const
    {
        const std::string fn(name);
        if (fn == "min") return (std::min)(a, b);
        if (fn == "max") return (std::max)(a, b);
        if (fn == "mod") {
            if (std::abs(b) < 1e-15) throw EvalError(true, "Math ERROR");
            return std::fmod(a, b);
        }
        if (fn == "gcd" || fn == "lcm") {
            if (std::floor(a) != a || std::floor(b) != b || std::abs(a) > 1e15 || std::abs(b) > 1e15) {
                throw EvalError(true, "Math ERROR");
            }
            const long long ia = std::llround(std::abs(a));
            const long long ib = std::llround(std::abs(b));
            if (fn == "gcd") return static_cast<double>(std::gcd(ia, ib));
            if (ia == 0 || ib == 0) return 0.0;
            return static_cast<double>(std::lcm(ia, ib));
        }
        if (fn == "npr" || fn == "ncr") {
            if (a < 0.0 || b < 0.0 || b > a || a > 170.0 || std::floor(a) != a || std::floor(b) != b) {
                throw EvalError(true, "Math ERROR");
            }
            const int n = static_cast<int>(a);
            const int r = static_cast<int>(b);
            if (fn == "npr") {
                double res = 1.0;
                for (int i = 0; i < r; ++i) res *= static_cast<double>(n - i);
                return res;
            }
            const int k = (std::min)(r, n - r);
            double res = 1.0;
            for (int i = 1; i <= k; ++i) {
                res = (res * static_cast<double>(n - k + i)) / static_cast<double>(i);
            }
            return res;
        }
        throw EvalError(false, "Syntax ERROR");
    }

    double ParseExpression()
    {
        double val = ParseTerm();
        while (pos_ < s_.size()) {
            if (s_[pos_] == '+') {
                ++pos_;
                val += ParseTerm();
            } else if (s_[pos_] == '-') {
                ++pos_;
                val -= ParseTerm();
            } else {
                break;
            }
        }
        return val;
    }

    double ParseTerm()
    {
        double val = ParseUnary();
        while (pos_ < s_.size()) {
            if (s_[pos_] == '*') {
                ++pos_;
                val *= ParseUnary();
            } else if (s_[pos_] == '/') {
                ++pos_;
                const double denom = ParseUnary();
                if (std::abs(denom) < 1e-15) {
                    throw EvalError(true, "Math ERROR");
                }
                val /= denom;
            } else {
                break;
            }
        }
        return val;
    }

    double ParseUnary()
    {
        if (pos_ < s_.size() && s_[pos_] == '-') {
            ++pos_;
            return -ParseUnary();
        }
        if (pos_ < s_.size() && s_[pos_] == '+') {
            ++pos_;
            return ParseUnary();
        }
        return ParsePower();
    }

    double ParsePower()
    {
        double val = ParsePostfix();
        if (pos_ < s_.size() && s_[pos_] == '^') {
            ++pos_;
            const double exponent = ParseUnary();
            const double res = std::pow(val, exponent);
            if (std::isnan(res) || std::isinf(res)) {
                throw EvalError(true, "Math ERROR");
            }
            return res;
        }
        return val;
    }

    double ParsePostfix()
    {
        double val = ParsePrimary();
        while (pos_ < s_.size()) {
            if (s_[pos_] == '!') {
                ++pos_;
                val = Factorial(val);
            } else if (s_[pos_] == '%') {
                ++pos_;
                val /= 100.0;
            } else {
                break;
            }
        }
        return val;
    }

    double ParseSignedPrimary()
    {
        if (pos_ < s_.size() && s_[pos_] == '-') {
            ++pos_;
            return -ParseSignedPrimary();
        }
        if (pos_ < s_.size() && s_[pos_] == '+') {
            ++pos_;
            return ParseSignedPrimary();
        }
        return ParsePrimary();
    }

    double ParsePrimary()
    {
        if (pos_ >= s_.size()) {
            throw EvalError(false, "Syntax ERROR");
        }

        if (s_[pos_] == '(') {
            ++pos_;
            if (pos_ < s_.size() && s_[pos_] == ')') {
                throw EvalError(false, "Syntax ERROR");
            }
            double val = ParseExpression();
            if (pos_ < s_.size()) {
                if (s_[pos_] != ')') {
                    throw EvalError(false, "Syntax ERROR");
                }
                ++pos_;
            }
            return val;
        }

        static constexpr struct { const char* name; size_t len; int arity; } kFuncs[] = {
            { "asinh", 5, 1 }, { "acosh", 5, 1 }, { "atanh", 5, 1 },
            { "floor", 5, 1 }, { "round", 5, 1 }, { "trunc", 5, 1 },
            { "asin",  4, 1 }, { "acos",  4, 1 }, { "atan",  4, 1 },
            { "sinh",  4, 1 }, { "cosh",  4, 1 }, { "tanh",  4, 1 },
            { "sqrt",  4, 1 }, { "cbrt",  4, 1 }, { "log2",  4, 1 },
            { "ceil",  4, 1 }, { "frac",  4, 1 },
            { "sin",   3, 1 }, { "cos",   3, 1 }, { "tan",   3, 1 },
            { "log",   3, 1 }, { "exp",   3, 1 }, { "abs",   3, 1 },
            { "sgn",   3, 1 }, { "deg",   3, 1 }, { "rad",   3, 1 },
            { "min",   3, 2 }, { "max",   3, 2 }, { "npr",   3, 2 },
            { "ncr",   3, 2 }, { "gcd",   3, 2 }, { "lcm",   3, 2 },
            { "mod",   3, 2 },
            { "ln",    2, 1 }
        };

        for (const auto& f : kFuncs) {
            if (s_.compare(pos_, f.len, f.name) == 0 &&
                (pos_ + f.len >= s_.size() || !std::isalpha(static_cast<unsigned char>(s_[pos_ + f.len])))) {
                pos_ += f.len;
                if (f.arity == 2) {
                    if (pos_ >= s_.size() || s_[pos_] != '(') throw EvalError(false, "Syntax ERROR");
                    ++pos_;
                    const double a = ParseExpression();
                    if (pos_ >= s_.size() || s_[pos_] != ',') throw EvalError(false, "Syntax ERROR");
                    ++pos_;
                    const double b = ParseExpression();
                    if (pos_ >= s_.size() || s_[pos_] != ')') throw EvalError(false, "Syntax ERROR");
                    ++pos_;
                    return ApplyFunc2(f.name, a, b);
                }
                const double arg = ParseSignedPrimary();
                return ApplyFunc(f.name, arg);
            }
        }

        const size_t start = pos_;
        bool hasDigit = false;
        bool hasDot = false;
        while (pos_ < s_.size()) {
            const char c = s_[pos_];
            if (c >= '0' && c <= '9') {
                hasDigit = true;
                ++pos_;
            } else if (c == '.' && !hasDot) {
                hasDot = true;
                ++pos_;
            } else {
                break;
            }
        }
        if (!hasDigit || start == pos_) {
            throw EvalError(false, "Syntax ERROR");
        }
        if (pos_ < s_.size() && s_[pos_] == 'e') {
            size_t expPos = pos_ + 1;
            if (expPos < s_.size() && (s_[expPos] == '+' || s_[expPos] == '-')) {
                ++expPos;
            }
            const size_t digitStart = expPos;
            while (expPos < s_.size() && s_[expPos] >= '0' && s_[expPos] <= '9') {
                ++expPos;
            }
            if (expPos > digitStart) {
                pos_ = expPos;
            }
        }

        try {
            size_t idx = 0;
            const std::string numStr = s_.substr(start, pos_ - start);
            const double val = std::stod(numStr, &idx);
            if (idx != numStr.size()) throw EvalError(false, "Syntax ERROR");
            if (std::isnan(val) || std::isinf(val)) throw EvalError(true, "Math ERROR");
            return val;
        } catch (const EvalError&) {
            throw;
        } catch (...) {
            throw EvalError(true, "Math ERROR");
        }
    }
};

inline EvalOutput EvaluateWithAngleMode(const std::string& expr, AngleMode mode, double lastAns)
{
    EvalOutput out;
    const std::string s = PreprocessMathUtf8(expr, lastAns);
    if (s.empty()) {
        out.ok = true;
        out.isError = false;
        out.value = 0.0;
        out.formatted = "0";
        return out;
    }

    try {
        MathParser parser(s, mode);
        const double val = parser.ParseAll();
        out.ok = true;
        out.isError = false;
        out.value = val;
        out.formatted = FormatDoubleCompact(val, 10);
        return out;
    } catch (const EvalError& err) {
        out.ok = false;
        out.isError = true;
        out.value = 0.0;
        out.formatted = err.what();
        out.errorMessage = err.what();
        return out;
    } catch (...) {
        out.ok = false;
        out.isError = true;
        out.value = 0.0;
        out.formatted = "Syntax ERROR";
        out.errorMessage = "Syntax ERROR";
        return out;
    }
}

} // namespace CasioLinux::detail
