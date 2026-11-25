#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E  2.71828182845904523536
#endif

enum AngleMode { ANG_DEG = 0, ANG_RAD = 1, ANG_GRA = 2 };

struct CalcState {
    std::wstring expression;
    std::wstring resultStr;
    double       lastAns = 0.0;
    AngleMode    angleMode = ANG_DEG;
    bool         isShift = false;
    bool         isAlpha = false;
    bool         hasEvaluated = false;
    int          cursorPos = 0;

    // Variables A..F, X, Y, M
    double vars[9] = {0};

    void Reset() {
        expression.clear();
        resultStr.clear();
        hasEvaluated = false;
        isShift = false;
        isAlpha = false;
        cursorPos = 0;
    }
};

// ── Recursive descent scientific math evaluator (parity with math_eval_linux.hpp) ──
class MathEvaluator {
public:
    static bool Evaluate(const std::wstring& expr, double ans, AngleMode mode, double& outVal, std::wstring& outErr) {
        if (expr.empty()) {
            outVal = 0.0;
            return true;
        }
        std::wstring s = Preprocess(expr, ans);
        if (s.empty()) {
            outVal = 0.0;
            return true;
        }
        size_t pos = 0;
        try {
            outVal = ParseExpression(s, pos, mode);
            if (pos < s.length()) {
                outErr = L"Syntax ERROR";
                return false;
            }
            if (std::isnan(outVal) || std::isinf(outVal)) {
                outErr = L"Math ERROR";
                return false;
            }
            return true;
        } catch (const EvalErr& e) {
            outErr = e.isMath ? L"Math ERROR" : L"Syntax ERROR";
            return false;
        } catch (...) {
            outErr = L"Syntax ERROR";
            return false;
        }
    }

    static std::wstring FormatResult(double val) {
        if (std::isnan(val) || std::isinf(val)) return L"Math ERROR";
        if (std::abs(val) < 1e-12) return L"0";

        if (std::abs(val - std::round(val)) < 1e-10 && std::abs(val) < 1e12) {
            return std::to_wstring(static_cast<long long>(std::llround(val)));
        }

        std::wostringstream oss;
        oss << std::setprecision(10) << val;
        return oss.str();
    }

private:
    struct EvalErr {
        bool isMath;
    };

    static bool StartsWithCI(const std::wstring& s, size_t pos, const wchar_t* lit, size_t len) {
        if (pos + len > s.size()) return false;
        for (size_t i = 0; i < len; ++i) {
            if (std::towlower(s[pos + i]) != std::towlower(lit[i])) return false;
        }
        return true;
    }

    static std::wstring FormatCompactW(double val) {
        if (!std::isfinite(val)) return L"0";
        if (std::abs(val) < 1e-12) return L"0";
        if (std::abs(val - std::round(val)) < 1e-10 && std::abs(val) < 1e12) {
            return std::to_wstring(static_cast<long long>(std::llround(val)));
        }
        std::wostringstream oss;
        oss << std::setprecision(15) << val;
        return oss.str();
    }

    static bool IsSciExpAfter(const std::wstring& expr, size_t eIdx) {
        if (eIdx + 1 >= expr.size()) return false;
        const wchar_t next = expr[eIdx + 1];
        if (next >= L'0' && next <= L'9') return true;
        if ((next == L'+' || next == L'-' || next == L'\x2212') && eIdx + 2 < expr.size()) {
            const wchar_t d = expr[eIdx + 2];
            return (d >= L'0' && d <= L'9');
        }
        return false;
    }

    static std::wstring Preprocess(const std::wstring& expr, double ans) {
        std::wstring raw;
        raw.reserve(expr.size() + 16);
        const std::wstring ansToken = L"(" + FormatCompactW(ans) + L")";
        constexpr const wchar_t* kPiStr = L"(3.14159265358979323846)";
        constexpr const wchar_t* kEulerStr = L"(2.71828182845904523536)";

        for (size_t i = 0; i < expr.size();) {
            const wchar_t c = expr[i];
            if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
                ++i;
                continue;
            }
            if (c == L'\x00D7') { raw.push_back(L'*'); ++i; continue; }
            if (c == L'\x00F7') { raw.push_back(L'/'); ++i; continue; }
            if (c == L'\x2212') { raw.push_back(L'-'); ++i; continue; }
            if (c == L'\x03C0') { raw += kPiStr; ++i; continue; }
            if (c == L'\x221A') { raw += L"sqrt"; ++i; continue; }

            // Ans keyword
            if (StartsWithCI(expr, i, L"ans", 3) &&
                (i == 0 || !std::iswalpha(expr[i - 1])) &&
                (i + 3 >= expr.size() || !std::iswalpha(expr[i + 3]))) {
                raw += ansToken;
                i += 3;
                continue;
            }
            // pi keyword
            if (StartsWithCI(expr, i, L"pi", 2) &&
                (i == 0 || !std::iswalpha(expr[i - 1])) &&
                (i + 2 >= expr.size() || !std::iswalpha(expr[i + 2]))) {
                raw += kPiStr;
                i += 2;
                continue;
            }
            // Safe 'x' / 'X' as multiplication only when NOT adjacent to alpha letters (preserves max, exp, etc.)
            if (c == L'x' || c == L'X') {
                const bool prevAlpha = (i > 0) && std::iswalpha(expr[i - 1]);
                const bool nextAlpha = (i + 1 < expr.size()) && std::iswalpha(expr[i + 1]);
                if (!prevAlpha && !nextAlpha) {
                    raw.push_back(L'*');
                    ++i;
                    continue;
                }
            }
            // Standalone 'e' constant (not scientific notation and not identifier like exp/ceil)
            if (c == L'e' || c == L'E') {
                const bool prevNum = !raw.empty() && ((raw.back() >= L'0' && raw.back() <= L'9') || raw.back() == L'.');
                const bool isSciExpInput = prevNum && IsSciExpAfter(expr, i);
                const bool prevAlpha = (i > 0) && std::iswalpha(expr[i - 1]);
                const bool nextAlpha = (i + 1 < expr.size()) && std::iswalpha(expr[i + 1]);
                if (!isSciExpInput && !prevAlpha && !nextAlpha) {
                    raw += kEulerStr;
                    ++i;
                    continue;
                }
            }

            raw.push_back(static_cast<wchar_t>(std::towlower(c)));
            ++i;
        }

        // Insert implicit multiplication
        std::wstring out;
        out.reserve(raw.size() + 8);
        for (size_t i = 0; i < raw.size(); ++i) {
            const wchar_t cur = raw[i];
            if (!out.empty()) {
                const wchar_t prev = out.back();
                const bool prevEndValue =
                    (prev >= L'0' && prev <= L'9') || prev == L')' || prev == L'!' || prev == L'%';
                const bool isSciExp = (cur == L'e') &&
                    ((prev >= L'0' && prev <= L'9') || prev == L'.') &&
                    (i + 1 < raw.size()) &&
                    ((raw[i + 1] >= L'0' && raw[i + 1] <= L'9') || raw[i + 1] == L'+' || raw[i + 1] == L'-');
                const bool isLog2Call = (cur == L'(' && out.size() >= 4 &&
                    out.compare(out.size() - 4, 4, L"log2") == 0);
                const bool curStartFactor =
                    (cur == L'(' && !isLog2Call) || (std::iswalpha(cur) && !isSciExp);
                if (prevEndValue && curStartFactor) {
                    out.push_back(L'*');
                }
            }
            out.push_back(cur);
        }
        return out;
    }

    static double ParseExpression(const std::wstring& s, size_t& pos, AngleMode mode) {
        double val = ParseTerm(s, pos, mode);
        while (pos < s.length()) {
            if (s[pos] == L'+') { pos++; val += ParseTerm(s, pos, mode); }
            else if (s[pos] == L'-') { pos++; val -= ParseTerm(s, pos, mode); }
            else break;
        }
        return val;
    }

    static double ParseTerm(const std::wstring& s, size_t& pos, AngleMode mode) {
        double val = ParseUnary(s, pos, mode);
        while (pos < s.length()) {
            if (s[pos] == L'*') { pos++; val *= ParseUnary(s, pos, mode); }
            else if (s[pos] == L'/') {
                pos++;
                double denom = ParseUnary(s, pos, mode);
                if (std::abs(denom) < 1e-15) throw EvalErr{ true };
                val /= denom;
            } else break;
        }
        return val;
    }

    static double ParseUnary(const std::wstring& s, size_t& pos, AngleMode mode) {
        if (pos < s.length() && s[pos] == L'-') { pos++; return -ParseUnary(s, pos, mode); }
        if (pos < s.length() && s[pos] == L'+') { pos++; return ParseUnary(s, pos, mode); }
        return ParsePower(s, pos, mode);
    }

    static double ParsePower(const std::wstring& s, size_t& pos, AngleMode mode) {
        double val = ParsePostfix(s, pos, mode);
        if (pos < s.length() && s[pos] == L'^') {
            pos++;
            const double exponent = ParseUnary(s, pos, mode);
            const double res = std::pow(val, exponent);
            if (std::isnan(res) || std::isinf(res)) throw EvalErr{ true };
            return res;
        }
        return val;
    }

    static double ParsePostfix(const std::wstring& s, size_t& pos, AngleMode mode) {
        double val = ParsePrimary(s, pos, mode);
        while (pos < s.length()) {
            if (s[pos] == L'!') {
                pos++;
                val = Factorial(val);
            } else if (s[pos] == L'%') {
                pos++;
                val /= 100.0;
            } else break;
        }
        return val;
    }

    static double ParseSignedPrimary(const std::wstring& s, size_t& pos, AngleMode mode) {
        if (pos < s.length() && s[pos] == L'-') { pos++; return -ParseSignedPrimary(s, pos, mode); }
        if (pos < s.length() && s[pos] == L'+') { pos++; return ParseSignedPrimary(s, pos, mode); }
        return ParsePrimary(s, pos, mode);
    }

    static double ParsePrimary(const std::wstring& s, size_t& pos, AngleMode mode) {
        if (pos >= s.length()) throw EvalErr{ false };

        if (s[pos] == L'(') {
            pos++;
            if (pos < s.length() && s[pos] == L')') throw EvalErr{ false };
            double val = ParseExpression(s, pos, mode);
            if (pos < s.length()) {
                if (s[pos] != L')') throw EvalErr{ false };
                pos++;
            }
            return val;
        }

        // Functions (ordered longest-first to avoid prefix collisions like sinh vs sin)
        static const struct { const wchar_t* name; size_t len; int arity; } kFuncs[] = {
            { L"asinh", 5, 1 }, { L"acosh", 5, 1 }, { L"atanh", 5, 1 },
            { L"floor", 5, 1 }, { L"round", 5, 1 }, { L"trunc", 5, 1 },
            { L"asin",  4, 1 }, { L"acos",  4, 1 }, { L"atan",  4, 1 },
            { L"sinh",  4, 1 }, { L"cosh",  4, 1 }, { L"tanh",  4, 1 },
            { L"sqrt",  4, 1 }, { L"cbrt",  4, 1 }, { L"log2",  4, 1 },
            { L"ceil",  4, 1 }, { L"frac",  4, 1 },
            { L"sin",   3, 1 }, { L"cos",   3, 1 }, { L"tan",   3, 1 },
            { L"log",   3, 1 }, { L"exp",   3, 1 }, { L"abs",   3, 1 },
            { L"sgn",   3, 1 }, { L"deg",   3, 1 }, { L"rad",   3, 1 },
            { L"min",   3, 2 }, { L"max",   3, 2 }, { L"npr",   3, 2 },
            { L"ncr",   3, 2 }, { L"gcd",   3, 2 }, { L"lcm",   3, 2 },
            { L"mod",   3, 2 },
            { L"ln",    2, 1 }
        };

        for (const auto& f : kFuncs) {
            if (s.compare(pos, f.len, f.name) == 0 &&
                (pos + f.len >= s.length() || !std::iswalpha(s[pos + f.len]))) {
                pos += f.len;
                if (f.arity == 2) {
                    if (pos >= s.length() || s[pos] != L'(') throw EvalErr{ false };
                    pos++;
                    double a = ParseExpression(s, pos, mode);
                    if (pos >= s.length() || s[pos] != L',') throw EvalErr{ false };
                    pos++;
                    double b = ParseExpression(s, pos, mode);
                    if (pos >= s.length() || s[pos] != L')') throw EvalErr{ false };
                    pos++;
                    return ApplyFunc2(f.name, a, b);
                }
                const double arg = ParseSignedPrimary(s, pos, mode);
                return ApplyFunc(f.name, arg, mode);
            }
        }

        // Numbers (including scientific notation 1.5e-3)
        const size_t start = pos;
        bool hasDigit = false;
        bool hasDot = false;
        while (pos < s.length()) {
            const wchar_t c = s[pos];
            if (c >= L'0' && c <= L'9') {
                hasDigit = true;
                pos++;
            } else if (c == L'.' && !hasDot) {
                hasDot = true;
                pos++;
            } else break;
        }
        if (!hasDigit || start == pos) throw EvalErr{ false };
        if (pos < s.length() && s[pos] == L'e') {
            size_t expPos = pos + 1;
            if (expPos < s.length() && (s[expPos] == L'+' || s[expPos] == L'-')) expPos++;
            const size_t dStart = expPos;
            while (expPos < s.length() && s[expPos] >= L'0' && s[expPos] <= L'9') expPos++;
            if (expPos > dStart) pos = expPos;
        }

        try {
            size_t idx = 0;
            const std::wstring numStr = s.substr(start, pos - start);
            const double val = std::stod(numStr, &idx);
            if (idx != numStr.size()) throw EvalErr{ false };
            if (std::isnan(val) || std::isinf(val)) throw EvalErr{ true };
            return val;
        } catch (const EvalErr&) {
            throw;
        } catch (...) {
            throw EvalErr{ true };
        }
    }

    static double ToRad(double arg, AngleMode mode) {
        if (mode == ANG_DEG) return arg * M_PI / 180.0;
        if (mode == ANG_GRA) return arg * M_PI / 200.0;
        return arg;
    }

    static double ToAngle(double rad, AngleMode mode) {
        if (mode == ANG_DEG) return rad * 180.0 / M_PI;
        if (mode == ANG_GRA) return rad * 200.0 / M_PI;
        return rad;
    }

    static double ApplyFunc(const std::wstring& name, double arg, AngleMode mode) {
        if (name == L"sqrt") { if (arg < 0.0) throw EvalErr{ true }; return std::sqrt(arg); }
        if (name == L"cbrt") return std::cbrt(arg);
        if (name == L"ln") { if (arg <= 0.0) throw EvalErr{ true }; return std::log(arg); }
        if (name == L"log") { if (arg <= 0.0) throw EvalErr{ true }; return std::log10(arg); }
        if (name == L"log2") { if (arg <= 0.0) throw EvalErr{ true }; return std::log2(arg); }
        if (name == L"exp") return std::exp(arg);
        if (name == L"abs") return std::abs(arg);
        if (name == L"floor") return std::floor(arg);
        if (name == L"ceil") return std::ceil(arg);
        if (name == L"round") return std::round(arg);
        if (name == L"trunc") return std::trunc(arg);
        if (name == L"frac") return arg - std::trunc(arg);
        if (name == L"sgn") return (arg > 0.0) ? 1.0 : ((arg < 0.0) ? -1.0 : 0.0);
        if (name == L"deg") return arg * 180.0 / M_PI;
        if (name == L"rad") return arg * M_PI / 180.0;
        if (name == L"sinh") return std::sinh(arg);
        if (name == L"cosh") return std::cosh(arg);
        if (name == L"tanh") return std::tanh(arg);
        if (name == L"asinh") return std::asinh(arg);
        if (name == L"acosh") { if (arg < 1.0) throw EvalErr{ true }; return std::acosh(arg); }
        if (name == L"atanh") { if (arg <= -1.0 || arg >= 1.0) throw EvalErr{ true }; return std::atanh(arg); }

        const double rad = ToRad(arg, mode);
        if (name == L"sin") {
            double v = std::sin(rad);
            if (std::abs(v) < 1e-14) v = 0.0;
            return v;
        }
        if (name == L"cos") {
            double v = std::cos(rad);
            if (std::abs(v) < 1e-14) v = 0.0;
            return v;
        }
        if (name == L"tan") {
            if (std::abs(std::cos(rad)) < 1e-13) throw EvalErr{ true };
            double v = std::tan(rad);
            if (std::abs(v) < 1e-14) v = 0.0;
            return v;
        }
        if (name == L"asin") { if (arg < -1.0 || arg > 1.0) throw EvalErr{ true }; return ToAngle(std::asin(arg), mode); }
        if (name == L"acos") { if (arg < -1.0 || arg > 1.0) throw EvalErr{ true }; return ToAngle(std::acos(arg), mode); }
        if (name == L"atan") return ToAngle(std::atan(arg), mode);
        throw EvalErr{ false };
    }

    static double ApplyFunc2(const std::wstring& name, double a, double b) {
        if (name == L"min") return (std::min)(a, b);
        if (name == L"max") return (std::max)(a, b);
        if (name == L"mod") {
            if (std::abs(b) < 1e-15) throw EvalErr{ true };
            return std::fmod(a, b);
        }
        if (name == L"gcd" || name == L"lcm") {
            if (std::floor(a) != a || std::floor(b) != b || std::abs(a) > 1e15 || std::abs(b) > 1e15) throw EvalErr{ true };
            const long long ia = std::llround(std::abs(a));
            const long long ib = std::llround(std::abs(b));
            if (name == L"gcd") return static_cast<double>(std::gcd(ia, ib));
            if (ia == 0 || ib == 0) return 0.0;
            return static_cast<double>(std::lcm(ia, ib));
        }
        if (name == L"npr" || name == L"ncr") {
            if (a < 0.0 || b < 0.0 || b > a || a > 170.0 || std::floor(a) != a || std::floor(b) != b) throw EvalErr{ true };
            const int n = static_cast<int>(a);
            const int r = static_cast<int>(b);
            if (name == L"npr") {
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
        throw EvalErr{ false };
    }

    static double Factorial(double val) {
        if (val < 0.0 || val > 170.0 || std::floor(val) != val) throw EvalErr{ true };
        double res = 1.0;
        for (int i = 2; i <= static_cast<int>(val); ++i) res *= i;
        return res;
    }
};
