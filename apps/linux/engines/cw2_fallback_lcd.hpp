#pragma once

#include "key_tables_linux.hpp"
#include "math_eval_linux.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace CasioLinux::detail {

inline constexpr int kCw2LcdW = 192;
inline constexpr int kCw2LcdH = 64;

// Standard 5x7 column-major bitmap font for ASCII 0x20 (' ') .. 0x7E ('~').
// Each byte encodes 7 vertical pixels (bit 0 = top row, bit 6 = bottom row).
inline const std::array<std::uint8_t, 5>& Font5x7Glyph(char ch)
{
    static constexpr std::array<std::array<std::uint8_t, 5>, 95> kFont = {{
        {{0x00, 0x00, 0x00, 0x00, 0x00}}, // ' ' 0x20
        {{0x00, 0x00, 0x5F, 0x00, 0x00}}, // '!'
        {{0x00, 0x07, 0x00, 0x07, 0x00}}, // '"'
        {{0x14, 0x7F, 0x14, 0x7F, 0x14}}, // '#'
        {{0x24, 0x2A, 0x7F, 0x2A, 0x12}}, // '$'
        {{0x23, 0x13, 0x08, 0x64, 0x62}}, // '%'
        {{0x36, 0x49, 0x55, 0x22, 0x50}}, // '&'
        {{0x00, 0x05, 0x03, 0x00, 0x00}}, // '''
        {{0x00, 0x1C, 0x22, 0x41, 0x00}}, // '('
        {{0x00, 0x41, 0x22, 0x1C, 0x00}}, // ')'
        {{0x14, 0x08, 0x3E, 0x08, 0x14}}, // '*'
        {{0x08, 0x08, 0x3E, 0x08, 0x08}}, // '+'
        {{0x00, 0x50, 0x30, 0x00, 0x00}}, // ','
        {{0x08, 0x08, 0x08, 0x08, 0x08}}, // '-'
        {{0x00, 0x60, 0x60, 0x00, 0x00}}, // '.'
        {{0x20, 0x10, 0x08, 0x04, 0x02}}, // '/'
        {{0x3E, 0x51, 0x49, 0x45, 0x3E}}, // '0'
        {{0x00, 0x42, 0x7F, 0x40, 0x00}}, // '1'
        {{0x42, 0x61, 0x51, 0x49, 0x46}}, // '2'
        {{0x21, 0x41, 0x45, 0x4B, 0x31}}, // '3'
        {{0x18, 0x14, 0x12, 0x7F, 0x10}}, // '4'
        {{0x27, 0x45, 0x45, 0x45, 0x39}}, // '5'
        {{0x3C, 0x4A, 0x49, 0x49, 0x30}}, // '6'
        {{0x01, 0x71, 0x09, 0x05, 0x03}}, // '7'
        {{0x36, 0x49, 0x49, 0x49, 0x36}}, // '8'
        {{0x06, 0x49, 0x49, 0x29, 0x1E}}, // '9'
        {{0x00, 0x36, 0x36, 0x00, 0x00}}, // ':'
        {{0x00, 0x56, 0x36, 0x00, 0x00}}, // ';'
        {{0x08, 0x14, 0x22, 0x41, 0x00}}, // '<'
        {{0x14, 0x14, 0x14, 0x14, 0x14}}, // '='
        {{0x00, 0x41, 0x22, 0x14, 0x08}}, // '>'
        {{0x02, 0x01, 0x51, 0x09, 0x06}}, // '?'
        {{0x32, 0x49, 0x79, 0x41, 0x3E}}, // '@'
        {{0x7E, 0x11, 0x11, 0x11, 0x7E}}, // 'A'
        {{0x7F, 0x49, 0x49, 0x49, 0x36}}, // 'B'
        {{0x3E, 0x41, 0x41, 0x41, 0x22}}, // 'C'
        {{0x7F, 0x41, 0x41, 0x22, 0x1C}}, // 'D'
        {{0x7F, 0x49, 0x49, 0x49, 0x41}}, // 'E'
        {{0x7F, 0x09, 0x09, 0x09, 0x01}}, // 'F'
        {{0x3E, 0x41, 0x49, 0x49, 0x7A}}, // 'G'
        {{0x7F, 0x08, 0x08, 0x08, 0x7F}}, // 'H'
        {{0x00, 0x41, 0x7F, 0x41, 0x00}}, // 'I'
        {{0x20, 0x40, 0x41, 0x3F, 0x01}}, // 'J'
        {{0x7F, 0x08, 0x14, 0x22, 0x41}}, // 'K'
        {{0x7F, 0x40, 0x40, 0x40, 0x40}}, // 'L'
        {{0x7F, 0x02, 0x0C, 0x02, 0x7F}}, // 'M'
        {{0x7F, 0x04, 0x08, 0x10, 0x7F}}, // 'N'
        {{0x3E, 0x41, 0x41, 0x41, 0x3E}}, // 'O'
        {{0x7F, 0x09, 0x09, 0x09, 0x06}}, // 'P'
        {{0x3E, 0x41, 0x51, 0x21, 0x5E}}, // 'Q'
        {{0x7F, 0x09, 0x19, 0x29, 0x46}}, // 'R'
        {{0x46, 0x49, 0x49, 0x49, 0x31}}, // 'S'
        {{0x01, 0x01, 0x7F, 0x01, 0x01}}, // 'T'
        {{0x3F, 0x40, 0x40, 0x40, 0x3F}}, // 'U'
        {{0x1F, 0x20, 0x40, 0x20, 0x1F}}, // 'V'
        {{0x3F, 0x40, 0x38, 0x40, 0x3F}}, // 'W'
        {{0x63, 0x14, 0x08, 0x14, 0x63}}, // 'X'
        {{0x07, 0x08, 0x70, 0x08, 0x07}}, // 'Y'
        {{0x61, 0x51, 0x49, 0x45, 0x43}}, // 'Z'
        {{0x00, 0x7F, 0x41, 0x41, 0x00}}, // '['
        {{0x02, 0x04, 0x08, 0x10, 0x20}}, // '\'
        {{0x00, 0x41, 0x41, 0x7F, 0x00}}, // ']'
        {{0x04, 0x02, 0x01, 0x02, 0x04}}, // '^'
        {{0x40, 0x40, 0x40, 0x40, 0x40}}, // '_'
        {{0x00, 0x01, 0x02, 0x04, 0x00}}, // '`'
        {{0x20, 0x54, 0x54, 0x54, 0x78}}, // 'a'
        {{0x7F, 0x48, 0x44, 0x44, 0x38}}, // 'b'
        {{0x38, 0x44, 0x44, 0x44, 0x20}}, // 'c'
        {{0x38, 0x44, 0x44, 0x48, 0x7F}}, // 'd'
        {{0x38, 0x54, 0x54, 0x54, 0x18}}, // 'e'
        {{0x08, 0x7E, 0x09, 0x01, 0x02}}, // 'f'
        {{0x0C, 0x52, 0x52, 0x52, 0x3E}}, // 'g'
        {{0x7F, 0x08, 0x04, 0x04, 0x78}}, // 'h'
        {{0x00, 0x44, 0x7D, 0x40, 0x00}}, // 'i'
        {{0x20, 0x40, 0x44, 0x3D, 0x00}}, // 'j'
        {{0x7F, 0x10, 0x28, 0x44, 0x00}}, // 'k'
        {{0x00, 0x41, 0x7F, 0x40, 0x00}}, // 'l'
        {{0x7C, 0x04, 0x18, 0x04, 0x78}}, // 'm'
        {{0x7C, 0x08, 0x04, 0x04, 0x78}}, // 'n'
        {{0x38, 0x44, 0x44, 0x44, 0x38}}, // 'o'
        {{0x7C, 0x14, 0x14, 0x14, 0x08}}, // 'p'
        {{0x08, 0x14, 0x14, 0x18, 0x7C}}, // 'q'
        {{0x7C, 0x08, 0x04, 0x04, 0x08}}, // 'r'
        {{0x48, 0x54, 0x54, 0x54, 0x20}}, // 's'
        {{0x04, 0x3F, 0x44, 0x40, 0x20}}, // 't'
        {{0x3C, 0x40, 0x40, 0x20, 0x7C}}, // 'u'
        {{0x1C, 0x20, 0x40, 0x20, 0x1C}}, // 'v'
        {{0x3C, 0x40, 0x30, 0x40, 0x3C}}, // 'w'
        {{0x44, 0x28, 0x10, 0x28, 0x44}}, // 'x'
        {{0x0C, 0x50, 0x50, 0x50, 0x3C}}, // 'y'
        {{0x44, 0x64, 0x54, 0x4C, 0x44}}, // 'z'
        {{0x00, 0x08, 0x36, 0x41, 0x00}}, // '{'
        {{0x00, 0x00, 0x7F, 0x00, 0x00}}, // '|'
        {{0x00, 0x41, 0x36, 0x08, 0x00}}, // '}'
        {{0x10, 0x08, 0x08, 0x10, 0x08}}, // '~'
    }};

    const unsigned char uc = static_cast<unsigned char>(ch);
    if (uc < 0x20 || uc > 0x7E) {
        return kFont[0];
    }
    return kFont[uc - 0x20];
}

class ComplexMathParser {
public:
    ComplexMathParser(const std::string& expr, AngleMode mode, std::complex<double> ans)
        : mode_(mode), ans_(ans)
    {
        s_ = Preprocess(expr);
    }

    std::complex<double> ParseAll()
    {
        std::complex<double> val = ParseExpr();
        if (pos_ < s_.size()) {
            throw EvalError(false, "Syntax ERROR");
        }
        if (!std::isfinite(val.real()) || !std::isfinite(val.imag())) {
            throw EvalError(true, "Math ERROR");
        }
        return val;
    }

private:
    std::string s_;
    AngleMode mode_;
    std::complex<double> ans_;
    size_t pos_ = 0;

    static constexpr double kPi = 3.14159265358979323846;

    double AngleToRad(double a) const
    {
        if (mode_ == AngleMode::Deg) return a * kPi / 180.0;
        if (mode_ == AngleMode::Gra) return a * kPi / 200.0;
        return a;
    }

    double RadToAngle(double r) const
    {
        if (mode_ == AngleMode::Deg) return r * 180.0 / kPi;
        if (mode_ == AngleMode::Gra) return r * 200.0 / kPi;
        return r;
    }

    static std::string Preprocess(const std::string& expr)
    {
        std::string raw;
        raw.reserve(expr.size() + 8);
        for (size_t i = 0; i < expr.size();) {
            const unsigned char c = static_cast<unsigned char>(expr[i]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++i;
                continue;
            }
            if (c == 0xC3 && i + 1 < expr.size() && static_cast<unsigned char>(expr[i + 1]) == 0x97) {
                raw.push_back('*');
                i += 2;
                continue;
            }
            if (c == 0xC3 && i + 1 < expr.size() && static_cast<unsigned char>(expr[i + 1]) == 0xB7) {
                raw.push_back('/');
                i += 2;
                continue;
            }
            if (c == 0xE2 && i + 2 < expr.size() &&
                static_cast<unsigned char>(expr[i + 1]) == 0x88 &&
                static_cast<unsigned char>(expr[i + 2]) == 0x92) {
                raw.push_back('-');
                i += 3;
                continue;
            }
            // ∠ (E2 88 A0) -> '@'
            if (c == 0xE2 && i + 2 < expr.size() &&
                static_cast<unsigned char>(expr[i + 1]) == 0x88 &&
                static_cast<unsigned char>(expr[i + 2]) == 0xA0) {
                raw.push_back('@');
                i += 3;
                continue;
            }
            // ° (C2 B0) -> skip
            if (c == 0xC2 && i + 1 < expr.size() && static_cast<unsigned char>(expr[i + 1]) == 0xB0) {
                i += 2;
                continue;
            }
            // π (CF 80) -> "pi"
            if (c == 0xCF && i + 1 < expr.size() && static_cast<unsigned char>(expr[i + 1]) == 0x80) {
                raw += "pi";
                i += 2;
                continue;
            }
            // √ (E2 88 9A) -> "sqrt"
            if (c == 0xE2 && i + 2 < expr.size() &&
                static_cast<unsigned char>(expr[i + 1]) == 0x88 &&
                static_cast<unsigned char>(expr[i + 2]) == 0x9A) {
                raw += "sqrt";
                i += 3;
                continue;
            }
            raw.push_back(static_cast<char>(std::tolower(c)));
            ++i;
        }
        return raw;
    }

    std::complex<double> ParseExpr()
    {
        auto v = ParseTerm();
        while (pos_ < s_.size()) {
            if (s_[pos_] == '+') { ++pos_; v += ParseTerm(); }
            else if (s_[pos_] == '-') { ++pos_; v -= ParseTerm(); }
            else break;
        }
        return v;
    }

    std::complex<double> ParseTerm()
    {
        auto v = ParsePhasor();
        while (pos_ < s_.size()) {
            if (s_[pos_] == '*') { ++pos_; v *= ParsePhasor(); }
            else if (s_[pos_] == '/') {
                ++pos_;
                auto d = ParsePhasor();
                if (std::abs(d) < 1e-15) throw EvalError(true, "Math ERROR");
                v /= d;
            } else break;
        }
        return v;
    }

    std::complex<double> ParsePhasor()
    {
        auto mag = ParseUnary();
        if (pos_ < s_.size() && s_[pos_] == '@') {
            ++pos_;
            auto ang = ParseUnary();
            if (std::abs(mag.imag()) > 1e-12 || std::abs(ang.imag()) > 1e-12) {
                throw EvalError(false, "Syntax ERROR");
            }
            const double rad = AngleToRad(ang.real());
            return std::polar(mag.real(), rad);
        }
        return mag;
    }

    static std::complex<double> CleanSignedZero(std::complex<double> z) noexcept
    {
        if (z.real() == 0.0) z.real(0.0);
        if (z.imag() == 0.0) z.imag(0.0);
        return z;
    }

    std::complex<double> ParseUnary()
    {
        if (pos_ < s_.size() && s_[pos_] == '-') { ++pos_; return CleanSignedZero(-ParseUnary()); }
        if (pos_ < s_.size() && s_[pos_] == '+') { ++pos_; return ParseUnary(); }
        return ParseImplicitMul();
    }

    std::complex<double> ParsePowerExponent()
    {
        if (pos_ < s_.size() && s_[pos_] == '-') { ++pos_; return CleanSignedZero(-ParsePowerExponent()); }
        if (pos_ < s_.size() && s_[pos_] == '+') { ++pos_; return ParsePowerExponent(); }
        return ParsePower();
    }

    std::complex<double> ParseImplicitMul()
    {
        auto v = ParsePower();
        // Implicit multiplication with 'i', 'j', '(', or function/constant identifier (e.g., 4i, 2(3), 2abs(3+4i))
        // Exponentiation ('^') binds tighter than implicit multiplication: 2i^2 == 2*(i^2) == -2.
        while (pos_ < s_.size()) {
            if ((s_[pos_] == 'i' || s_[pos_] == 'j') &&
                (pos_ + 1 >= s_.size() || !std::isalpha(static_cast<unsigned char>(s_[pos_ + 1])))) {
                v *= ParsePower();
            } else if (s_[pos_] == '(' || std::isalpha(static_cast<unsigned char>(s_[pos_]))) {
                v *= ParsePower();
            } else {
                break;
            }
        }
        return v;
    }

    std::complex<double> ParsePower()
    {
        auto base = ParsePrimary();
        if (pos_ < s_.size() && s_[pos_] == '^') {
            ++pos_;
            auto ex = ParsePowerExponent();
            if (std::abs(base) < 1e-15 && ex.real() <= 0.0) {
                throw EvalError(true, "Math ERROR");
            }
            auto res = std::pow(CleanSignedZero(base), CleanSignedZero(ex));
            if (!std::isfinite(res.real()) || !std::isfinite(res.imag())) {
                throw EvalError(true, "Math ERROR");
            }
            return CleanSignedZero(res);
        }
        return base;
    }

    std::complex<double> ParsePrimary()
    {
        if (pos_ >= s_.size()) throw EvalError(false, "Syntax ERROR");
        if (s_[pos_] == '(') {
            ++pos_;
            if (pos_ < s_.size() && s_[pos_] == ')') throw EvalError(false, "Syntax ERROR");
            auto v = ParseExpr();
            if (pos_ >= s_.size() || s_[pos_] != ')') throw EvalError(false, "Syntax ERROR");
            ++pos_;
            return v;
        }

        if (std::isalpha(static_cast<unsigned char>(s_[pos_]))) {
            const size_t st = pos_;
            while (pos_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[pos_]))) ++pos_;
            const std::string ident = s_.substr(st, pos_ - st);
            if (ident == "i" || ident == "j") return std::complex<double>(0.0, 1.0);
            if (ident == "pi") return std::complex<double>(kPi, 0.0);
            if (ident == "e") return std::complex<double>(2.71828182845904523536, 0.0);
            if (ident == "ans") return ans_;

            auto arg = (pos_ < s_.size() && s_[pos_] == '(') ? ParsePrimary() : ParseUnary();
            arg = CleanSignedZero(arg);
            if (ident == "abs")  return std::complex<double>(std::abs(arg), 0.0);
            if (ident == "arg")  return std::complex<double>(RadToAngle(std::arg(arg)), 0.0);
            if (ident == "conj") return CleanSignedZero(std::conj(arg));
            if (ident == "real") return std::complex<double>(arg.real() == 0.0 ? 0.0 : arg.real(), 0.0);
            if (ident == "imag") return std::complex<double>(arg.imag() == 0.0 ? 0.0 : arg.imag(), 0.0);
            if (ident == "sqrt") return CleanSignedZero(std::sqrt(arg));
            if (ident == "exp")  return CleanSignedZero(std::exp(arg));
            if (ident == "ln")   {
                if (std::abs(arg) < 1e-15) throw EvalError(true, "Math ERROR");
                return CleanSignedZero(std::log(arg));
            }
            if (ident == "log")  {
                if (std::abs(arg) < 1e-15) throw EvalError(true, "Math ERROR");
                return CleanSignedZero(std::log10(arg));
            }
            const double toRad = AngleToRad(1.0);
            const double fromRad = RadToAngle(1.0);
            if (ident == "sin")  return CleanSignedZero(std::sin(arg * toRad));
            if (ident == "cos")  return CleanSignedZero(std::cos(arg * toRad));
            if (ident == "tan")  {
                const std::complex<double> radArg = arg * toRad;
                if (std::abs(std::cos(radArg)) < 1e-13) throw EvalError(true, "Math ERROR");
                return CleanSignedZero(std::tan(radArg));
            }
            if (ident == "asin") return CleanSignedZero(std::asin(arg) * fromRad);
            if (ident == "acos") return CleanSignedZero(std::acos(arg) * fromRad);
            if (ident == "atan") {
                if (std::abs(arg.real()) < 1e-12 && std::abs(std::abs(arg.imag()) - 1.0) < 1e-12) {
                    throw EvalError(true, "Math ERROR");
                }
                return CleanSignedZero(std::atan(arg) * fromRad);
            }
            throw EvalError(false, "Syntax ERROR");
        }

        const size_t start = pos_;
        bool hasDigit = false;
        bool hasDot = false;
        while (pos_ < s_.size()) {
            const char c = s_[pos_];
            if (c >= '0' && c <= '9') { hasDigit = true; ++pos_; }
            else if (c == '.' && !hasDot) { hasDot = true; ++pos_; }
            else break;
        }
        if (!hasDigit) throw EvalError(false, "Syntax ERROR");
        if (pos_ < s_.size() && s_[pos_] == 'e') {
            size_t ep = pos_ + 1;
            if (ep < s_.size() && (s_[ep] == '+' || s_[ep] == '-')) ++ep;
            size_t ds = ep;
            while (ep < s_.size() && s_[ep] >= '0' && s_[ep] <= '9') ++ep;
            if (ep > ds) pos_ = ep;
        }
        try {
            size_t idx = 0;
            const std::string num = s_.substr(start, pos_ - start);
            double v = std::stod(num, &idx);
            if (idx != num.size()) throw EvalError(false, "Syntax ERROR");
            return std::complex<double>(v, 0.0);
        } catch (const EvalError&) {
            throw;
        } catch (...) {
            throw EvalError(true, "Math ERROR");
        }
    }
};

class Cw2FallbackEngine {
public:
    enum class DisplayFormat : int {
        Standard = 0,
        Scientific = 1,
        Fixed2 = 2
    };

    enum class AppMode : int {
        Calculate = 0,
        HomeMenu = 1,
        Complex = 2,
        Matrix = 3,
        Table = 4,
        Equation = 5,
        SettingsMenu = 6,
        CatalogMenu = 7,
        VariableMenu = 8
    };

    enum class MatrixSubState : int {
        Menu = 0,
        EditDim = 1,
        EditGrid = 2,
        ShowResult = 3
    };

    enum class TableSubState : int {
        EditExpr = 0,
        EditStart = 1,
        EditEnd = 2,
        EditStep = 3,
        ViewRows = 4
    };

    enum class EqnSubState : int {
        Menu = 0,
        EnterCoeffs = 1,
        ShowResult = 2
    };

    static constexpr std::array<char, 9> kVarNames = { 'A', 'B', 'C', 'D', 'E', 'F', 'X', 'Y', 'M' };

    Cw2FallbackEngine()
    {
        Reset();
    }

    void Reset()
    {
        mode_ = AppMode::Calculate;
        prevMode_ = AppMode::Calculate;
        homeIndex_ = 0;
        menuCursor_ = 0;

        expression_.clear();
        lastExpr_.clear();
        resultStr_ = "0";
        lastAns_ = 0.0;
        lastValue_ = 0.0;
        lastComplex_ = { 0.0, 0.0 };
        angleMode_ = AngleMode::Deg;
        format_ = DisplayFormat::Standard;
        complexPolar_ = false;
        shift_ = false;
        alpha_ = false;
        stoPending_ = false;
        hasEvaluated_ = false;
        isError_ = false;
        cursorPos_ = 0;
        cursorBlinkTicks_ = 0;
        vars_.fill(0.0);

        matSub_ = MatrixSubState::Menu;
        matEditingIdx_ = 0;
        matDim_[0] = 2; matDim_[1] = 2;
        matData_[0] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0 };
        matData_[1] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0 };
        matRow_ = 0; matCol_ = 0;
        matMenuIdx_ = 0;

        tableSub_ = TableSubState::EditExpr;
        tableExpr_ = "x^2";
        tableStart_ = 1.0;
        tableEnd_ = 5.0;
        tableStep_ = 1.0;
        tableScroll_ = 0;
        tableRows_.clear();

        eqnSub_ = EqnSubState::Menu;
        eqnKind_ = 0; // 0: simul 2-var, 1: simul 3-var, 2: poly deg 2
        eqnMenuIdx_ = 0;
        eqnCoeffIdx_ = 0;
        eqnCoeffs_.fill(0.0);

        dirty_ = true;
    }

    void StepMs(std::uint32_t ms)
    {
        if (ms == 0) return;
        const std::uint32_t prevPhase = (cursorBlinkTicks_ / 400u) & 1u;
        cursorBlinkTicks_ += ms;
        const std::uint32_t nextPhase = (cursorBlinkTicks_ / 400u) & 1u;
        if (prevPhase != nextPhase) {
            dirty_ = true;
        }
    }

    bool HandleMatrixCode(std::uint16_t matrixCode)
    {
        const auto& keys = BuildCw2Keys();
        for (const auto& k : keys) {
            if (k.matrixCode == matrixCode) {
                return HandleKeyId(k.id);
            }
        }
        return false;
    }

    bool HandleKeyId(int keyId)
    {
        const auto& keys = BuildCw2Keys();
        if (keyId < 0 || keyId >= static_cast<int>(keys.size())) {
            return false;
        }
        const std::string& name = keys[static_cast<size_t>(keyId)].name;

        // Global keys active across all modes
        if (name == "ON") {
            expression_.clear();
            resultStr_ = "0";
            hasEvaluated_ = false;
            isError_ = false;
            shift_ = false;
            alpha_ = false;
            stoPending_ = false;
            cursorPos_ = 0;
            mode_ = AppMode::Calculate;
            prevMode_ = AppMode::Calculate;
            dirty_ = true;
            return true;
        }
        if (name == "HOME") {
            if (mode_ != AppMode::HomeMenu && mode_ != AppMode::SettingsMenu &&
                mode_ != AppMode::CatalogMenu && mode_ != AppMode::VariableMenu) {
                prevMode_ = mode_;
            }
            mode_ = AppMode::HomeMenu;
            homeIndex_ = 0;
            if (prevMode_ == AppMode::Complex) homeIndex_ = 1;
            else if (prevMode_ == AppMode::Equation) homeIndex_ = 2;
            else if (prevMode_ == AppMode::Table) homeIndex_ = 3;
            else if (prevMode_ == AppMode::Matrix) homeIndex_ = 4;
            isError_ = false;
            shift_ = false;
            alpha_ = false;
            dirty_ = true;
            return true;
        }
        if (name == "SHIFT") {
            shift_ = !shift_;
            dirty_ = true;
            return true;
        }
        if (name == "TOOLS" && mode_ != AppMode::VariableMenu) {
            if (shift_) {
                stoPending_ = !stoPending_;
                shift_ = false;
                dirty_ = true;
                return true;
            }
            CycleAngleMode();
            return true;
        }
        if (name == "SETTINGS") {
            if (mode_ == AppMode::SettingsMenu) {
                CycleAngleMode();
                return true;
            }
            if (mode_ != AppMode::HomeMenu && mode_ != AppMode::CatalogMenu && mode_ != AppMode::VariableMenu) {
                prevMode_ = mode_;
            }
            mode_ = AppMode::SettingsMenu;
            menuCursor_ = 0;
            shift_ = false;
            dirty_ = true;
            return true;
        }

        // Dispatch by active mode
        switch (mode_) {
        case AppMode::HomeMenu:     return HandleHomeMenuKey(name);
        case AppMode::SettingsMenu: return HandleSettingsMenuKey(name);
        case AppMode::CatalogMenu:  return HandleCatalogMenuKey(name);
        case AppMode::VariableMenu: return HandleVariableMenuKey(name);
        case AppMode::Matrix:       return HandleMatrixKey(name);
        case AppMode::Table:        return HandleTableKey(name);
        case AppMode::Equation:     return HandleEquationKey(name);
        case AppMode::Complex:
        case AppMode::Calculate:
            return HandleCalcOrComplexKey(name);
        }
        return false;
    }

    AppMode Mode() const noexcept { return mode_; }

    std::string ModeName() const
    {
        switch (mode_) {
        case AppMode::Calculate:    return "CALC";
        case AppMode::HomeMenu:     return "HOME";
        case AppMode::Complex:      return "CMPLX";
        case AppMode::Matrix:       return "MAT";
        case AppMode::Table:        return "TBL";
        case AppMode::Equation:     return "EQN";
        case AppMode::SettingsMenu: return "SET";
        case AppMode::CatalogMenu:  return "CAT";
        case AppMode::VariableMenu: return "VAR";
        }
        return "CALC";
    }

    void SetVariable(char varName, double val)
    {
        for (size_t i = 0; i < kVarNames.size(); ++i) {
            if (kVarNames[i] == varName) {
                vars_[i] = val;
                dirty_ = true;
                return;
            }
        }
    }

    double GetVariable(char varName) const noexcept
    {
        for (size_t i = 0; i < kVarNames.size(); ++i) {
            if (kVarNames[i] == varName) return vars_[i];
        }
        return 0.0;
    }

    // Direct programmatic helpers for testing and API consumers
    std::string EvalComplexExpr(const std::string& expr)
    {
        mode_ = AppMode::Complex;
        expression_ = expr;
        cursorPos_ = expression_.size();
        EvaluateCurrent();
        return resultStr_;
    }

    void SetMatrixA(int dim, const std::vector<double>& rowMajor)
    {
        const int d = (dim == 3) ? 3 : 2;
        matDim_[0] = d;
        matData_[0].fill(0.0);
        for (int i = 0; i < d * d && i < static_cast<int>(rowMajor.size()); ++i) {
            matData_[0][static_cast<size_t>(i)] = rowMajor[static_cast<size_t>(i)];
        }
        dirty_ = true;
    }

    void SetMatrixB(int dim, const std::vector<double>& rowMajor)
    {
        const int d = (dim == 3) ? 3 : 2;
        matDim_[1] = d;
        matData_[1].fill(0.0);
        for (int i = 0; i < d * d && i < static_cast<int>(rowMajor.size()); ++i) {
            matData_[1][static_cast<size_t>(i)] = rowMajor[static_cast<size_t>(i)];
        }
        dirty_ = true;
    }

    std::string EvaluateMatrixOp(const std::string& opName)
    {
        mode_ = AppMode::Matrix;
        matSub_ = MatrixSubState::ShowResult;
        expression_ = opName;
        ExecuteMatrixOperation(opName);
        dirty_ = true;
        return resultStr_;
    }

    std::vector<std::pair<double, std::string>> ComputeTable(
        const std::string& fxExpr,
        double start,
        double end,
        double step)
    {
        mode_ = AppMode::Table;
        tableExpr_ = fxExpr;
        tableStart_ = start;
        tableEnd_ = end;
        tableStep_ = step;
        GenerateTableRows();
        tableSub_ = TableSubState::ViewRows;
        dirty_ = true;
        return tableRows_;
    }

    std::string SolveSimultaneous(int nVars, const std::vector<double>& coeffs)
    {
        mode_ = AppMode::Equation;
        eqnKind_ = (nVars == 3) ? 1 : 0;
        eqnSub_ = EqnSubState::ShowResult;
        eqnCoeffs_.fill(0.0);
        for (size_t i = 0; i < coeffs.size() && i < eqnCoeffs_.size(); ++i) {
            eqnCoeffs_[i] = coeffs[i];
        }
        SolveCurrentEquation();
        dirty_ = true;
        return resultStr_;
    }

    std::string SolveQuadratic(double a, double b, double c)
    {
        mode_ = AppMode::Equation;
        eqnKind_ = 2;
        eqnSub_ = EqnSubState::ShowResult;
        eqnCoeffs_.fill(0.0);
        eqnCoeffs_[0] = a;
        eqnCoeffs_[1] = b;
        eqnCoeffs_[2] = c;
        SolveCurrentEquation();
        dirty_ = true;
        return resultStr_;
    }

    bool RenderRgba(std::vector<std::uint8_t>& outRgba, std::uint32_t* outIndicators, bool forceCopy)
    {
        if (outIndicators) {
            *outIndicators = ComputeIndicatorsMask();
        }
        if (!forceCopy && !dirty_) {
            return false;
        }
        dirty_ = false;

        constexpr std::uint8_t kBgR = 207, kBgG = 230, kBgB = 219;
        constexpr std::uint8_t kFgR = 20,  kFgG = 36,  kFgB = 58;

        outRgba.resize(static_cast<size_t>(kCw2LcdW) * kCw2LcdH * 4u);
        for (size_t i = 0; i < static_cast<size_t>(kCw2LcdW) * kCw2LcdH; ++i) {
            outRgba[i * 4u + 0] = kBgR;
            outRgba[i * 4u + 1] = kBgG;
            outRgba[i * 4u + 2] = kBgB;
            outRgba[i * 4u + 3] = 255u;
        }

        auto setPx = [&](int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
            if (x < 0 || x >= kCw2LcdW || y < 0 || y >= kCw2LcdH) return;
            const size_t idx = (static_cast<size_t>(y) * kCw2LcdW + static_cast<size_t>(x)) * 4u;
            outRgba[idx + 0] = r;
            outRgba[idx + 1] = g;
            outRgba[idx + 2] = b;
            outRgba[idx + 3] = 255u;
        };

        auto fillRect = [&](int x0, int y0, int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
            for (int y = y0; y < y0 + h; ++y) {
                for (int x = x0; x < x0 + w; ++x) {
                    setPx(x, y, r, g, b);
                }
            }
        };

        auto drawChar = [&](int x0, int y0, char ch, int scale, bool inverted) {
            const auto& cols = Font5x7Glyph(ch);
            for (int cx = 0; cx < 5; ++cx) {
                const std::uint8_t col = cols[static_cast<size_t>(cx)];
                for (int cy = 0; cy < 7; ++cy) {
                    const bool on = (col & (1u << cy)) != 0u;
                    if (on ^ inverted) {
                        fillRect(x0 + cx * scale, y0 + cy * scale, scale, scale, kFgR, kFgG, kFgB);
                    }
                }
            }
        };

        auto drawText = [&](int x0, int y0, const std::string& text, int scale, bool inverted) {
            int x = x0;
            for (char ch : text) {
                if (x + 5 * scale > kCw2LcdW - 1) break;
                drawChar(x, y0, ch, scale, inverted);
                x += 6 * scale;
            }
        };

        auto drawBadge = [&](int x0, const std::string& lbl) -> int {
            const int bw = static_cast<int>(lbl.size()) * 6 + 3;
            fillRect(x0, 1, bw, 9, kFgR, kFgG, kFgB);
            const auto& bg = outRgba;
            (void)bg;
            int cx0 = x0 + 2;
            for (char ch : lbl) {
                const auto& gc = Font5x7Glyph(ch);
                for (int cx = 0; cx < 5; ++cx) {
                    for (int cy = 0; cy < 7; ++cy) {
                        const bool on = (gc[static_cast<size_t>(cx)] & (1u << cy)) != 0u;
                        setPx(cx0 + cx, 2 + cy,
                              on ? kBgR : kFgR,
                              on ? kBgG : kFgG,
                              on ? kBgB : kFgB);
                    }
                }
                cx0 += 6;
            }
            return x0 + bw + 2;
        };

        // Top status bar (y = 0..10)
        int bx = 2;
        if (shift_) bx = drawBadge(bx, "S");
        if (alpha_) bx = drawBadge(bx, "A");
        if (std::abs(vars_[8]) > 1e-12) bx = drawBadge(bx, "M");
        if (stoPending_) bx = drawBadge(bx, "STO");

        const char angCh = (angleMode_ == AngleMode::Deg) ? 'D'
                         : (angleMode_ == AngleMode::Rad ? 'R' : 'G');
        bx = drawBadge(bx, std::string(1, angCh));
        if (format_ == DisplayFormat::Scientific) {
            drawText(bx, 2, "SCI", 1, false);
        } else if (format_ == DisplayFormat::Fixed2) {
            drawText(bx, 2, "FIX", 1, false);
        }

        const std::string modeTag = ModeName();
        const int modeW = static_cast<int>(modeTag.size()) * 6;
        drawText(kCw2LcdW - modeW - 3, 2, modeTag, 1, false);

        // Divider line
        fillRect(2, 11, kCw2LcdW - 4, 1, kFgR, kFgG, kFgB);

        if (mode_ == AppMode::HomeMenu) {
            static constexpr const char* kItems[5] = {
                "1:Calculate", "2:Complex", "3:Equation", "4:Table", "5:Matrix"
            };
            const int pageStart = (homeIndex_ >= 3) ? 2 : 0;
            for (int r = 0; r < 3 && pageStart + r < 5; ++r) {
                const int idx = pageStart + r;
                const int y = 15 + r * 14;
                if (idx == homeIndex_) {
                    fillRect(4, y - 1, kCw2LcdW - 8, 11, kFgR, kFgG, kFgB);
                    drawText(8, y + 1, kItems[idx], 1, true);
                } else {
                    drawText(8, y + 1, kItems[idx], 1, false);
                }
            }
            return true;
        }

        if (mode_ == AppMode::SettingsMenu) {
            static constexpr const char* kSetItems[6] = {
                "1:Angle Deg", "2:Angle Rad", "3:Angle Gra",
                "4:Fmt Norm",  "5:Fmt Fix2",  "6:Fmt Sci"
            };
            const int start = (menuCursor_ >= 3) ? 3 : 0;
            for (int r = 0; r < 3 && start + r < 6; ++r) {
                const int idx = start + r;
                const int y = 15 + r * 14;
                if (idx == menuCursor_) {
                    fillRect(4, y - 1, kCw2LcdW - 8, 11, kFgR, kFgG, kFgB);
                    drawText(8, y + 1, kSetItems[idx], 1, true);
                } else {
                    drawText(8, y + 1, kSetItems[idx], 1, false);
                }
            }
            return true;
        }

        if (mode_ == AppMode::CatalogMenu) {
            static constexpr const char* kCatItems[8] = {
                "1:sqrt(", "2:ln(", "3:log(", "4:exp(",
                "5:abs(",  "6:asin(", "7:acos(", "8:atan("
            };
            const int start = std::max(0, std::min(menuCursor_ - 1, 5));
            for (int r = 0; r < 3 && start + r < 8; ++r) {
                const int idx = start + r;
                const int y = 15 + r * 14;
                if (idx == menuCursor_) {
                    fillRect(4, y - 1, kCw2LcdW - 8, 11, kFgR, kFgG, kFgB);
                    drawText(8, y + 1, kCatItems[idx], 1, true);
                } else {
                    drawText(8, y + 1, kCatItems[idx], 1, false);
                }
            }
            return true;
        }

        if (mode_ == AppMode::VariableMenu) {
            const int start = std::max(0, std::min(menuCursor_ - 1, 6));
            for (int r = 0; r < 3 && start + r < 9; ++r) {
                const int idx = start + r;
                const int y = 15 + r * 14;
                std::string row = std::to_string(idx + 1) + ":" + kVarNames[static_cast<size_t>(idx)] +
                    "=" + FormatDoubleCompact(vars_[static_cast<size_t>(idx)], 6);
                if (idx == menuCursor_) {
                    fillRect(4, y - 1, kCw2LcdW - 8, 11, kFgR, kFgG, kFgB);
                    drawText(8, y + 1, row, 1, true);
                } else {
                    drawText(8, y + 1, row, 1, false);
                }
            }
            return true;
        }

        if (mode_ == AppMode::Table && tableSub_ == TableSubState::ViewRows) {
            drawText(5, 14, "x       | f(x)", 1, false);
            fillRect(4, 23, kCw2LcdW - 8, 1, kFgR, kFgG, kFgB);
            for (int r = 0; r < 3; ++r) {
                const size_t idx = static_cast<size_t>(tableScroll_ + r);
                if (idx >= tableRows_.size()) break;
                const std::string xs = FormatDoubleCompact(tableRows_[idx].first, 6);
                std::string line = xs;
                while (line.size() < 8) line.push_back(' ');
                line += "| " + tableRows_[idx].second;
                drawText(5, 26 + r * 11, line, 1, false);
            }
            return true;
        }

        if (mode_ == AppMode::Matrix && matSub_ == MatrixSubState::Menu) {
            static constexpr const char* kMatItems[9] = {
                "1:Edit MatA", "2:Edit MatB", "3:MatA+MatB",
                "4:MatA-MatB", "5:MatA*MatB", "6:det(MatA)",
                "7:inv(MatA)", "8:tr(MatA)",  "9:Trn(MatA)"
            };
            const int start = std::max(0, std::min(matMenuIdx_ - 1, 6));
            for (int r = 0; r < 3 && start + r < 9; ++r) {
                const int idx = start + r;
                const int y = 15 + r * 14;
                if (idx == matMenuIdx_) {
                    fillRect(4, y - 1, kCw2LcdW - 8, 11, kFgR, kFgG, kFgB);
                    drawText(8, y + 1, kMatItems[idx], 1, true);
                } else {
                    drawText(8, y + 1, kMatItems[idx], 1, false);
                }
            }
            return true;
        }

        if (mode_ == AppMode::Matrix && matSub_ == MatrixSubState::EditGrid) {
            const int d = matDim_[matEditingIdx_];
            const std::string hdr = std::string(matEditingIdx_ == 0 ? "MatA " : "MatB ") +
                std::to_string(d) + "x" + std::to_string(d) + " [" +
                std::to_string(matRow_ + 1) + "," + std::to_string(matCol_ + 1) + "]";
            drawText(5, 15, hdr, 1, false);
            const double val = matData_[matEditingIdx_][static_cast<size_t>(matRow_ * d + matCol_)];
            const std::string editStr = expression_.empty() ? FormatDoubleCompact(val, 8) : expression_;
            drawText(5, 32, editStr, 2, false);
            return true;
        }

        if (mode_ == AppMode::Equation && eqnSub_ == EqnSubState::Menu) {
            static constexpr const char* kEqnItems[3] = {
                "1:Simul 2-var", "2:Simul 3-var", "3:Poly ax^2+bx+c"
            };
            for (int r = 0; r < 3; ++r) {
                const int y = 15 + r * 14;
                if (r == eqnMenuIdx_) {
                    fillRect(4, y - 1, kCw2LcdW - 8, 11, kFgR, kFgG, kFgB);
                    drawText(8, y + 1, kEqnItems[r], 1, true);
                } else {
                    drawText(8, y + 1, kEqnItems[r], 1, false);
                }
            }
            return true;
        }

        // Standard Expression + Result layout (Calculate, Complex, Table edit, Matrix result, Equation input/result)
        std::string topLine = expression_;
        if (mode_ == AppMode::Table) {
            if (tableSub_ == TableSubState::EditExpr) topLine = "f(x)=" + expression_;
            else if (tableSub_ == TableSubState::EditStart) topLine = "Start=" + expression_;
            else if (tableSub_ == TableSubState::EditEnd) topLine = "End=" + expression_;
            else if (tableSub_ == TableSubState::EditStep) topLine = "Step=" + expression_;
        } else if (mode_ == AppMode::Equation && eqnSub_ == EqnSubState::EnterCoeffs) {
            topLine = "Coeff #" + std::to_string(eqnCoeffIdx_ + 1) + ": " + expression_;
        }

        const std::string dispExpr = NormalizeForAsciiLcd(topLine);
        constexpr size_t kMaxExprChars = 30;
        size_t viewStart = 0;
        const size_t asciiCursor = std::min(cursorPos_, dispExpr.size());
        if (dispExpr.size() > kMaxExprChars) {
            if (asciiCursor > kMaxExprChars - 2) {
                viewStart = asciiCursor - (kMaxExprChars - 2);
            }
            if (viewStart + kMaxExprChars > dispExpr.size()) {
                viewStart = dispExpr.size() - kMaxExprChars;
            }
        }
        const std::string visibleExpr = dispExpr.substr(viewStart, kMaxExprChars);
        drawText(5, 16, visibleExpr, 1, false);

        if ((mode_ == AppMode::Calculate || mode_ == AppMode::Complex) &&
            !hasEvaluated_ && ((cursorBlinkTicks_ / 400u) & 1u) == 0u) {
            const int cx = 5 + static_cast<int>(asciiCursor - viewStart) * 6;
            fillRect(cx - 1, 15, 1, 9, kFgR, kFgG, kFgB);
        }

        const std::string dispRes = NormalizeForAsciiLcd(resultStr_);
        if (dispRes.size() <= 15) {
            const int totalW = static_cast<int>(dispRes.size()) * 12;
            const int rx = std::max(4, kCw2LcdW - 4 - totalW);
            drawText(rx, 43, dispRes, 2, false);
        } else {
            const std::string trimmed = dispRes.substr(0, 30);
            const int totalW = static_cast<int>(trimmed.size()) * 6;
            const int rx = std::max(4, kCw2LcdW - 4 - totalW);
            drawText(rx, 47, trimmed, 1, false);
        }

        return true;
    }

    std::uint32_t ComputeIndicatorsMask() const noexcept
    {
        std::uint32_t m = 0;
        if (shift_) m |= (1u << 0);
        if (alpha_) m |= (1u << 1);
        if (std::abs(vars_[8]) > 1e-12) m |= (1u << 2);
        if (stoPending_) m |= (1u << 3);
        m |= (1u << 4); // Math mode
        if (angleMode_ == AngleMode::Deg) m |= (1u << 5);
        else if (angleMode_ == AngleMode::Rad) m |= (1u << 6);
        else m |= (1u << 7);
        if (format_ == DisplayFormat::Fixed2) m |= (1u << 8);
        if (format_ == DisplayFormat::Scientific) m |= (1u << 9);

        bool canDown = false;
        bool canUp = false;
        if (mode_ == AppMode::Table && tableSub_ == TableSubState::ViewRows) {
            canUp = tableScroll_ > 0;
            canDown = (tableScroll_ + 3) < static_cast<int>(tableRows_.size());
        } else if (mode_ == AppMode::HomeMenu) {
            canUp = homeIndex_ > 0;
            canDown = homeIndex_ < 4;
        } else if (mode_ == AppMode::Matrix && matSub_ == MatrixSubState::Menu) {
            canUp = matMenuIdx_ > 0;
            canDown = matMenuIdx_ < 8;
        } else if (mode_ == AppMode::SettingsMenu) {
            canUp = menuCursor_ > 0;
            canDown = menuCursor_ < 5;
        } else if (mode_ == AppMode::CatalogMenu) {
            canUp = menuCursor_ > 0;
            canDown = menuCursor_ < 7;
        } else if (mode_ == AppMode::VariableMenu) {
            canUp = menuCursor_ > 0;
            canDown = menuCursor_ < 8;
        }
        if (canDown) m |= (1u << 10);
        if (canUp) m |= (1u << 11);
        return m;
    }

    const std::string& Expression() const noexcept { return expression_; }
    const std::string& Result() const noexcept { return resultStr_; }

    std::vector<std::uint8_t> ExportState() const
    {
        std::vector<std::uint8_t> out;
        auto pushU32 = [&](std::uint32_t v) {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
        };
        auto pushU64 = [&](std::uint64_t v) {
            for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFFu));
        };
        auto pushDouble = [&](double d) {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &d, sizeof(bits));
            pushU64(bits);
        };
        auto pushStr = [&](const std::string& s) {
            pushU32(static_cast<std::uint32_t>(s.size()));
            out.insert(out.end(), s.begin(), s.end());
        };

        out.insert(out.end(), {'C', 'W', '2', 'F'});
        pushU32(3);
        pushU32(static_cast<std::uint32_t>(angleMode_));
        pushU32(static_cast<std::uint32_t>(format_));
        out.push_back(shift_ ? 1u : 0u);
        out.push_back(hasEvaluated_ ? 1u : 0u);
        out.push_back(isError_ ? 1u : 0u);
        pushU32(static_cast<std::uint32_t>(cursorPos_));
        pushDouble(lastAns_);
        pushDouble(lastValue_);
        pushStr(expression_);
        pushStr(lastExpr_);
        pushStr(resultStr_);

        // v2 extensions
        pushU32(static_cast<std::uint32_t>(mode_));
        pushU32(static_cast<std::uint32_t>(prevMode_));
        out.push_back(alpha_ ? 1u : 0u);
        out.push_back(stoPending_ ? 1u : 0u);
        out.push_back(complexPolar_ ? 1u : 0u);
        pushDouble(lastComplex_.real());
        pushDouble(lastComplex_.imag());
        for (double v : vars_) pushDouble(v);
        for (int m = 0; m < 2; ++m) {
            pushU32(static_cast<std::uint32_t>(matDim_[m]));
            for (double v : matData_[m]) pushDouble(v);
        }
        pushStr(tableExpr_);
        pushDouble(tableStart_);
        pushDouble(tableEnd_);
        pushDouble(tableStep_);
        pushU32(static_cast<std::uint32_t>(tableScroll_));
        pushU32(static_cast<std::uint32_t>(eqnKind_));

        // v3 substate & cursor extensions
        pushU32(static_cast<std::uint32_t>(homeIndex_));
        pushU32(static_cast<std::uint32_t>(menuCursor_));
        pushU32(cursorBlinkTicks_);
        pushU32(static_cast<std::uint32_t>(matSub_));
        pushU32(static_cast<std::uint32_t>(matEditingIdx_));
        pushU32(static_cast<std::uint32_t>(matRow_));
        pushU32(static_cast<std::uint32_t>(matCol_));
        pushU32(static_cast<std::uint32_t>(matMenuIdx_));
        pushU32(static_cast<std::uint32_t>(tableSub_));
        pushU32(static_cast<std::uint32_t>(eqnSub_));
        pushU32(static_cast<std::uint32_t>(eqnMenuIdx_));
        pushU32(static_cast<std::uint32_t>(eqnCoeffIdx_));
        for (double v : eqnCoeffs_) pushDouble(v);
        return out;
    }

    bool ImportState(const std::vector<std::uint8_t>& buf)
    {
        size_t pos = 0;
        auto readU32 = [&](std::uint32_t& v) -> bool {
            if (pos + 4 > buf.size()) return false;
            v = 0;
            for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(buf[pos++]) << (i * 8);
            return true;
        };
        auto readU64 = [&](std::uint64_t& v) -> bool {
            if (pos + 8 > buf.size()) return false;
            v = 0;
            for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(buf[pos++]) << (i * 8);
            return true;
        };
        auto readDouble = [&](double& d) -> bool {
            std::uint64_t bits = 0;
            if (!readU64(bits)) return false;
            std::memcpy(&d, &bits, sizeof(d));
            return true;
        };
        auto readStr = [&](std::string& s) -> bool {
            std::uint32_t len = 0;
            if (!readU32(len) || pos + len > buf.size()) return false;
            s.assign(reinterpret_cast<const char*>(buf.data() + pos), len);
            pos += len;
            return true;
        };

        if (buf.size() < 8 || std::memcmp(buf.data(), "CW2F", 4) != 0) return false;
        pos = 4;
        std::uint32_t ver = 0, ang = 0, fmt = 0, cur = 0;
        if (!readU32(ver) || (ver != 1 && ver != 2 && ver != 3)) return false;
        if (!readU32(ang) || !readU32(fmt)) return false;
        if (pos + 3 > buf.size()) return false;
        const bool sh = buf[pos++] != 0;
        const bool ev = buf[pos++] != 0;
        const bool er = buf[pos++] != 0;
        if (!readU32(cur)) return false;
        double lAns = 0.0, lVal = 0.0;
        if (!readDouble(lAns) || !readDouble(lVal)) return false;
        std::string expr, lexpr, res;
        if (!readStr(expr) || !readStr(lexpr) || !readStr(res)) return false;

        if (ver == 1) {
            if (pos != buf.size()) return false;
            Reset();
            angleMode_ = (ang <= 2) ? static_cast<AngleMode>(ang) : AngleMode::Deg;
            format_ = (fmt <= 2) ? static_cast<DisplayFormat>(fmt) : DisplayFormat::Standard;
            shift_ = sh;
            hasEvaluated_ = ev;
            isError_ = er;
            cursorPos_ = std::min<size_t>(cur, expr.size());
            lastAns_ = lAns;
            lastValue_ = lVal;
            expression_ = std::move(expr);
            lastExpr_ = std::move(lexpr);
            resultStr_ = std::move(res);
            dirty_ = true;
            return true;
        }

        // ver == 2 or ver == 3
        std::uint32_t md = 0, pmd = 0;
        if (!readU32(md) || !readU32(pmd)) return false;
        if (pos + 3 > buf.size()) return false;
        const bool al = buf[pos++] != 0;
        const bool st = buf[pos++] != 0;
        const bool cp = buf[pos++] != 0;
        double cr = 0.0, ci = 0.0;
        if (!readDouble(cr) || !readDouble(ci)) return false;
        std::array<double, 9> newVars{};
        for (double& v : newVars) {
            if (!readDouble(v)) return false;
        }
        std::array<int, 2> newMatDim{ 2, 2 };
        std::array<std::array<double, 9>, 2> newMatData{};
        for (int m = 0; m < 2; ++m) {
            std::uint32_t d = 2;
            if (!readU32(d)) return false;
            newMatDim[m] = (d == 3) ? 3 : 2;
            for (double& v : newMatData[m]) {
                if (!readDouble(v)) return false;
            }
        }
        std::string tblExpr;
        double tStart = 1.0, tEnd = 5.0, tStep = 1.0;
        std::uint32_t tScroll = 0, eKind = 0;
        if (!readStr(tblExpr) || !readDouble(tStart) || !readDouble(tEnd) || !readDouble(tStep) ||
            !readU32(tScroll) || !readU32(eKind)) {
            return false;
        }

        std::uint32_t hIdx = 0, mCur = 0, blinkTicks = 0;
        std::uint32_t mSub = 0, mEditIdx = 0, mRow = 0, mCol = 0, mMenuIdx = 0;
        std::uint32_t tSub = 0, eSub = 0, eMenuIdx = 0, eCoeffIdx = 0;
        std::array<double, 12> newEqnCoeffs{};

        if (ver == 3) {
            if (!readU32(hIdx) || !readU32(mCur) || !readU32(blinkTicks) ||
                !readU32(mSub) || !readU32(mEditIdx) || !readU32(mRow) || !readU32(mCol) || !readU32(mMenuIdx) ||
                !readU32(tSub) || !readU32(eSub) || !readU32(eMenuIdx) || !readU32(eCoeffIdx)) {
                return false;
            }
            for (double& v : newEqnCoeffs) {
                if (!readDouble(v)) return false;
            }
        }
        if (pos != buf.size()) return false;

        // Clear all mode-specific substates before restoring snapshot fields (prevents cross-mode substate leakage).
        Reset();

        angleMode_ = (ang <= 2) ? static_cast<AngleMode>(ang) : AngleMode::Deg;
        format_ = (fmt <= 2) ? static_cast<DisplayFormat>(fmt) : DisplayFormat::Standard;
        shift_ = sh;
        hasEvaluated_ = ev;
        isError_ = er;
        cursorPos_ = std::min<size_t>(cur, expr.size());
        lastAns_ = lAns;
        lastValue_ = lVal;
        expression_ = std::move(expr);
        lastExpr_ = std::move(lexpr);
        resultStr_ = std::move(res);
        mode_ = (md <= 8) ? static_cast<AppMode>(md) : AppMode::Calculate;
        prevMode_ = (pmd <= 8) ? static_cast<AppMode>(pmd) : AppMode::Calculate;
        alpha_ = al;
        stoPending_ = st;
        complexPolar_ = cp;
        lastComplex_ = { cr, ci };
        vars_ = newVars;
        matDim_ = newMatDim;
        matData_ = newMatData;
        tableExpr_ = std::move(tblExpr);
        tableStart_ = tStart;
        tableEnd_ = tEnd;
        tableStep_ = tStep;
        tableScroll_ = static_cast<int>(tScroll);
        eqnKind_ = (eKind <= 2) ? static_cast<int>(eKind) : 0;

        if (ver == 3) {
            homeIndex_ = (hIdx <= 4) ? static_cast<int>(hIdx) : 0;
            menuCursor_ = static_cast<int>(mCur);
            cursorBlinkTicks_ = blinkTicks;
            matSub_ = (mSub <= 3) ? static_cast<MatrixSubState>(mSub) : MatrixSubState::Menu;
            matEditingIdx_ = (mEditIdx <= 1) ? static_cast<int>(mEditIdx) : 0;
            const int d = matDim_[matEditingIdx_];
            matRow_ = (static_cast<int>(mRow) < d) ? static_cast<int>(mRow) : 0;
            matCol_ = (static_cast<int>(mCol) < d) ? static_cast<int>(mCol) : 0;
            matMenuIdx_ = (mMenuIdx <= 8) ? static_cast<int>(mMenuIdx) : 0;
            tableSub_ = (tSub <= 4) ? static_cast<TableSubState>(tSub) : TableSubState::EditExpr;
            eqnSub_ = (eSub <= 2) ? static_cast<EqnSubState>(eSub) : EqnSubState::Menu;
            eqnMenuIdx_ = (eMenuIdx <= 2) ? static_cast<int>(eMenuIdx) : 0;
            eqnCoeffIdx_ = (eCoeffIdx < 12) ? static_cast<int>(eCoeffIdx) : 0;
            eqnCoeffs_ = newEqnCoeffs;
        } else {
            // Backward-compatible v2 inference so importing v2 snapshots renders expected results
            if (mode_ == AppMode::Table) {
                tableSub_ = TableSubState::ViewRows;
            } else if (mode_ == AppMode::Equation && (hasEvaluated_ || isError_)) {
                eqnSub_ = EqnSubState::ShowResult;
            } else if (mode_ == AppMode::Matrix && (hasEvaluated_ || isError_)) {
                matSub_ = MatrixSubState::ShowResult;
            }
        }

        if (mode_ == AppMode::Table && tableSub_ == TableSubState::ViewRows) {
            const int savedScroll = tableScroll_;
            const std::string savedResult = std::move(resultStr_);
            GenerateTableRows();
            tableSub_ = TableSubState::ViewRows;
            const int maxScroll = (tableRows_.size() > 3)
                ? static_cast<int>(tableRows_.size() - 3)
                : 0;
            tableScroll_ = (savedScroll < 0) ? 0 : ((savedScroll > maxScroll) ? maxScroll : savedScroll);
            if (!savedResult.empty()) {
                resultStr_ = savedResult;
            }
        }
        dirty_ = true;
        return true;
    }

private:
    AppMode mode_ = AppMode::Calculate;
    AppMode prevMode_ = AppMode::Calculate;
    int homeIndex_ = 0;
    int menuCursor_ = 0;

    std::string expression_;
    std::string lastExpr_;
    std::string resultStr_ = "0";
    double lastAns_ = 0.0;
    double lastValue_ = 0.0;
    std::complex<double> lastComplex_{ 0.0, 0.0 };
    AngleMode angleMode_ = AngleMode::Deg;
    DisplayFormat format_ = DisplayFormat::Standard;
    bool complexPolar_ = false;
    bool shift_ = false;
    bool alpha_ = false;
    bool stoPending_ = false;
    bool hasEvaluated_ = false;
    bool isError_ = false;
    size_t cursorPos_ = 0;
    std::uint32_t cursorBlinkTicks_ = 0;
    std::array<double, 9> vars_{};

    // Matrix state
    MatrixSubState matSub_ = MatrixSubState::Menu;
    int matEditingIdx_ = 0;
    std::array<int, 2> matDim_{ 2, 2 };
    std::array<std::array<double, 9>, 2> matData_{};
    int matRow_ = 0;
    int matCol_ = 0;
    int matMenuIdx_ = 0;

    // Table state
    TableSubState tableSub_ = TableSubState::EditExpr;
    std::string tableExpr_ = "x^2";
    double tableStart_ = 1.0;
    double tableEnd_ = 5.0;
    double tableStep_ = 1.0;
    int tableScroll_ = 0;
    std::vector<std::pair<double, std::string>> tableRows_;

    // Equation state
    EqnSubState eqnSub_ = EqnSubState::Menu;
    int eqnKind_ = 0;
    int eqnMenuIdx_ = 0;
    int eqnCoeffIdx_ = 0;
    std::array<double, 12> eqnCoeffs_{};

    bool dirty_ = true;

    void CycleAngleMode()
    {
        if (angleMode_ == AngleMode::Deg) angleMode_ = AngleMode::Rad;
        else if (angleMode_ == AngleMode::Rad) angleMode_ = AngleMode::Gra;
        else angleMode_ = AngleMode::Deg;
        shift_ = false;
        dirty_ = true;
    }

    void EnterAppMode(AppMode m)
    {
        mode_ = m;
        prevMode_ = m;
        shift_ = false;
        alpha_ = false;
        stoPending_ = false;
        isError_ = false;
        hasEvaluated_ = false;
        expression_.clear();
        cursorPos_ = 0;
        if (m == AppMode::Matrix) {
            matSub_ = MatrixSubState::Menu;
            matMenuIdx_ = 0;
        } else if (m == AppMode::Table) {
            tableSub_ = TableSubState::EditExpr;
            expression_ = tableExpr_;
            cursorPos_ = expression_.size();
        } else if (m == AppMode::Equation) {
            eqnSub_ = EqnSubState::Menu;
            eqnMenuIdx_ = 0;
            eqnCoeffIdx_ = 0;
        }
        dirty_ = true;
    }

    bool HandleHomeMenuKey(const std::string& name)
    {
        if (name == "IZQUIERDA" || name == "ARRIBA") {
            homeIndex_ = (homeIndex_ + 4) % 5;
            dirty_ = true;
            return true;
        }
        if (name == "DERECHA" || name == "ABAJO") {
            homeIndex_ = (homeIndex_ + 1) % 5;
            dirty_ = true;
            return true;
        }
        if (name == "ATRAS" || name == "AC") {
            mode_ = prevMode_;
            dirty_ = true;
            return true;
        }
        int chosen = -1;
        if (name == "OK" || name == "EXE") chosen = homeIndex_;
        else if (name.size() == 1 && name[0] >= '1' && name[0] <= '5') chosen = name[0] - '1';

        if (chosen >= 0 && chosen <= 4) {
            static constexpr AppMode kModes[5] = {
                AppMode::Calculate, AppMode::Complex, AppMode::Equation, AppMode::Table, AppMode::Matrix
            };
            EnterAppMode(kModes[chosen]);
            return true;
        }
        return false;
    }

    bool HandleSettingsMenuKey(const std::string& name)
    {
        if (name == "ARRIBA") { menuCursor_ = (menuCursor_ + 5) % 6; dirty_ = true; return true; }
        if (name == "ABAJO")  { menuCursor_ = (menuCursor_ + 1) % 6; dirty_ = true; return true; }
        if (name == "ATRAS" || name == "AC") { mode_ = prevMode_; dirty_ = true; return true; }
        int chosen = -1;
        if (name == "OK" || name == "EXE") chosen = menuCursor_;
        else if (name.size() == 1 && name[0] >= '1' && name[0] <= '6') chosen = name[0] - '1';
        if (chosen >= 0 && chosen <= 5) {
            if (chosen == 0) angleMode_ = AngleMode::Deg;
            else if (chosen == 1) angleMode_ = AngleMode::Rad;
            else if (chosen == 2) angleMode_ = AngleMode::Gra;
            else if (chosen == 3) format_ = DisplayFormat::Standard;
            else if (chosen == 4) format_ = DisplayFormat::Fixed2;
            else if (chosen == 5) format_ = DisplayFormat::Scientific;
            mode_ = prevMode_;
            if (hasEvaluated_ && !isError_ && mode_ == AppMode::Calculate) {
                resultStr_ = FormatForDisplay(lastValue_);
            }
            dirty_ = true;
            return true;
        }
        return false;
    }

    bool HandleCatalogMenuKey(const std::string& name)
    {
        static constexpr const char* kCatTokens[8] = {
            "sqrt(", "ln(", "log(", "exp(", "abs(", "asin(", "acos(", "atan("
        };
        if (name == "ARRIBA") { menuCursor_ = (menuCursor_ + 7) % 8; dirty_ = true; return true; }
        if (name == "ABAJO")  { menuCursor_ = (menuCursor_ + 1) % 8; dirty_ = true; return true; }
        if (name == "ATRAS" || name == "AC") { mode_ = prevMode_; dirty_ = true; return true; }
        int chosen = -1;
        if (name == "OK" || name == "EXE") chosen = menuCursor_;
        else if (name.size() == 1 && name[0] >= '1' && name[0] <= '8') chosen = name[0] - '1';
        if (chosen >= 0 && chosen <= 7) {
            mode_ = prevMode_;
            InsertToken(kCatTokens[chosen], false);
            return true;
        }
        return false;
    }

    bool HandleVariableMenuKey(const std::string& name)
    {
        if (name == "ARRIBA") { menuCursor_ = (menuCursor_ + 8) % 9; dirty_ = true; return true; }
        if (name == "ABAJO")  { menuCursor_ = (menuCursor_ + 1) % 9; dirty_ = true; return true; }
        if (name == "FORMAT" || name == "TOOLS") { stoPending_ = !stoPending_; dirty_ = true; return true; }
        if (name == "ATRAS" || name == "AC") { stoPending_ = false; mode_ = prevMode_; dirty_ = true; return true; }
        int chosen = -1;
        if (name == "OK" || name == "EXE") chosen = menuCursor_;
        else if (name.size() == 1 && name[0] >= '1' && name[0] <= '9') chosen = name[0] - '1';
        if (chosen >= 0 && chosen <= 8) {
            mode_ = prevMode_;
            if (stoPending_) {
                vars_[static_cast<size_t>(chosen)] = lastValue_;
                stoPending_ = false;
                resultStr_ = std::string(1, kVarNames[static_cast<size_t>(chosen)]) + "<-" + FormatForDisplay(lastValue_);
                hasEvaluated_ = true;
                dirty_ = true;
            } else {
                InsertToken(std::string(1, kVarNames[static_cast<size_t>(chosen)]), false);
            }
            return true;
        }
        return false;
    }

    bool HandleMatrixKey(const std::string& name)
    {
        if (matSub_ == MatrixSubState::Menu) {
            if (name == "ARRIBA") { matMenuIdx_ = (matMenuIdx_ + 8) % 9; dirty_ = true; return true; }
            if (name == "ABAJO")  { matMenuIdx_ = (matMenuIdx_ + 1) % 9; dirty_ = true; return true; }
            if (name == "ATRAS" || name == "AC") { EnterAppMode(AppMode::Calculate); return true; }
            int chosen = -1;
            if (name == "OK" || name == "EXE") chosen = matMenuIdx_;
            else if (name.size() == 1 && name[0] >= '1' && name[0] <= '9') chosen = name[0] - '1';
            if (chosen < 0 || chosen > 8) return false;
            if (chosen == 0 || chosen == 1) {
                matEditingIdx_ = chosen;
                matSub_ = MatrixSubState::EditDim;
                dirty_ = true;
                return true;
            }
            static constexpr const char* kOps[7] = {
                "MatA+MatB", "MatA-MatB", "MatA*MatB", "det(MatA)", "inv(MatA)", "tr(MatA)", "transpose(MatA)"
            };
            EvaluateMatrixOp(kOps[chosen - 2]);
            return true;
        }

        if (matSub_ == MatrixSubState::EditDim) {
            if (name == "1" || name == "OK" || name == "EXE") {
                matDim_[matEditingIdx_] = (name == "2") ? 3 : 2;
                matRow_ = 0; matCol_ = 0;
                expression_.clear();
                matSub_ = MatrixSubState::EditGrid;
                dirty_ = true;
                return true;
            }
            if (name == "2") {
                matDim_[matEditingIdx_] = 3;
                matRow_ = 0; matCol_ = 0;
                expression_.clear();
                matSub_ = MatrixSubState::EditGrid;
                dirty_ = true;
                return true;
            }
            if (name == "ATRAS" || name == "AC") {
                matSub_ = MatrixSubState::Menu;
                dirty_ = true;
                return true;
            }
            return false;
        }

        if (matSub_ == MatrixSubState::EditGrid) {
            const int d = matDim_[matEditingIdx_];
            if (name == "ATRAS" || name == "AC" || name == "CATALOG") {
                expression_.clear();
                matSub_ = MatrixSubState::Menu;
                dirty_ = true;
                return true;
            }
            if (name == "IZQUIERDA") { matCol_ = (matCol_ + d - 1) % d; expression_.clear(); dirty_ = true; return true; }
            if (name == "DERECHA")   { matCol_ = (matCol_ + 1) % d; expression_.clear(); dirty_ = true; return true; }
            if (name == "ARRIBA")    { matRow_ = (matRow_ + d - 1) % d; expression_.clear(); dirty_ = true; return true; }
            if (name == "ABAJO")     { matRow_ = (matRow_ + 1) % d; expression_.clear(); dirty_ = true; return true; }
            if (name == "DEL" && !expression_.empty()) { expression_.pop_back(); dirty_ = true; return true; }
            if (name == "MENOS" && expression_.empty()) { expression_ = "-"; dirty_ = true; return true; }
            if (name == "PUNTO" && expression_.find('.') == std::string::npos) { expression_.push_back('.'); dirty_ = true; return true; }
            if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') { expression_.push_back(name[0]); dirty_ = true; return true; }
            if (name == "EXE" || name == "OK") {
                if (!expression_.empty()) {
                    const auto ev = EvaluateWithAngleMode(expression_, angleMode_, lastAns_);
                    if (ev.ok && !ev.isError) {
                        matData_[matEditingIdx_][static_cast<size_t>(matRow_ * d + matCol_)] = ev.value;
                    }
                    expression_.clear();
                }
                ++matCol_;
                if (matCol_ >= d) {
                    matCol_ = 0;
                    ++matRow_;
                    if (matRow_ >= d) matRow_ = 0;
                }
                dirty_ = true;
                return true;
            }
            return false;
        }

        // MatrixSubState::ShowResult
        if (name == "ATRAS" || name == "AC") {
            matSub_ = MatrixSubState::Menu;
            dirty_ = true;
            return true;
        }
        return false;
    }

    bool HandleTableKey(const std::string& name)
    {
        if (tableSub_ == TableSubState::ViewRows) {
            if (name == "ARRIBA" || name == "RE_PAG") {
                if (tableScroll_ > 0) --tableScroll_;
                dirty_ = true;
                return true;
            }
            if (name == "ABAJO" || name == "AV_PAG") {
                if (tableScroll_ + 3 < static_cast<int>(tableRows_.size())) ++tableScroll_;
                dirty_ = true;
                return true;
            }
            if (name == "ATRAS" || name == "AC") {
                tableSub_ = TableSubState::EditExpr;
                expression_ = tableExpr_;
                cursorPos_ = expression_.size();
                hasEvaluated_ = false;
                dirty_ = true;
                return true;
            }
            return false;
        }

        if (name == "AC") {
            expression_.clear();
            cursorPos_ = 0;
            hasEvaluated_ = false;
            dirty_ = true;
            return true;
        }
        if (name == "EXE" || name == "OK") {
            if (tableSub_ == TableSubState::EditExpr) {
                if (!expression_.empty()) tableExpr_ = expression_;
                tableSub_ = TableSubState::EditStart;
                expression_ = FormatDoubleCompact(tableStart_, 8);
                cursorPos_ = expression_.size();
                hasEvaluated_ = false;
            } else if (tableSub_ == TableSubState::EditStart) {
                const auto ev = EvaluateWithAngleMode(expression_, angleMode_, lastAns_);
                if (ev.ok && !ev.isError) tableStart_ = ev.value;
                tableSub_ = TableSubState::EditEnd;
                expression_ = FormatDoubleCompact(tableEnd_, 8);
                cursorPos_ = expression_.size();
                hasEvaluated_ = false;
            } else if (tableSub_ == TableSubState::EditEnd) {
                const auto ev = EvaluateWithAngleMode(expression_, angleMode_, lastAns_);
                if (ev.ok && !ev.isError) tableEnd_ = ev.value;
                tableSub_ = TableSubState::EditStep;
                expression_ = FormatDoubleCompact(tableStep_, 8);
                cursorPos_ = expression_.size();
                hasEvaluated_ = false;
            } else if (tableSub_ == TableSubState::EditStep) {
                const auto ev = EvaluateWithAngleMode(expression_, angleMode_, lastAns_);
                if (ev.ok && !ev.isError) tableStep_ = ev.value;
                GenerateTableRows();
                tableSub_ = TableSubState::ViewRows;
            }
            dirty_ = true;
            return true;
        }
        return HandleCalcOrComplexKey(name);
    }

    bool HandleEquationKey(const std::string& name)
    {
        if (eqnSub_ == EqnSubState::Menu) {
            if (name == "ARRIBA") { eqnMenuIdx_ = (eqnMenuIdx_ + 2) % 3; dirty_ = true; return true; }
            if (name == "ABAJO")  { eqnMenuIdx_ = (eqnMenuIdx_ + 1) % 3; dirty_ = true; return true; }
            if (name == "ATRAS" || name == "AC") { EnterAppMode(AppMode::Calculate); return true; }
            int chosen = -1;
            if (name == "OK" || name == "EXE") chosen = eqnMenuIdx_;
            else if (name.size() == 1 && name[0] >= '1' && name[0] <= '3') chosen = name[0] - '1';
            if (chosen >= 0 && chosen <= 2) {
                eqnKind_ = chosen;
                eqnCoeffIdx_ = 0;
                eqnCoeffs_.fill(0.0);
                eqnSub_ = EqnSubState::EnterCoeffs;
                expression_.clear();
                resultStr_ = "0";
                dirty_ = true;
                return true;
            }
            return false;
        }
        if (eqnSub_ == EqnSubState::EnterCoeffs) {
            if (name == "ATRAS" || name == "AC") {
                eqnSub_ = EqnSubState::Menu;
                expression_.clear();
                dirty_ = true;
                return true;
            }
            if (name == "DEL" && !expression_.empty()) { expression_.pop_back(); dirty_ = true; return true; }
            if (name == "MENOS" && expression_.empty()) { expression_ = "-"; dirty_ = true; return true; }
            if (name == "PUNTO" && expression_.find('.') == std::string::npos) { expression_.push_back('.'); dirty_ = true; return true; }
            if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') { expression_.push_back(name[0]); dirty_ = true; return true; }
            if (name == "EXE" || name == "OK") {
                double val = 0.0;
                if (!expression_.empty()) {
                    const auto ev = EvaluateWithAngleMode(expression_, angleMode_, lastAns_);
                    if (ev.ok && !ev.isError) val = ev.value;
                }
                eqnCoeffs_[static_cast<size_t>(eqnCoeffIdx_)] = val;
                expression_.clear();
                ++eqnCoeffIdx_;
                const int total = (eqnKind_ == 0) ? 6 : (eqnKind_ == 1 ? 12 : 3);
                if (eqnCoeffIdx_ >= total) {
                    eqnSub_ = EqnSubState::ShowResult;
                    SolveCurrentEquation();
                }
                dirty_ = true;
                return true;
            }
            return false;
        }
        // ShowResult
        if (name == "ATRAS" || name == "AC") {
            eqnSub_ = EqnSubState::Menu;
            dirty_ = true;
            return true;
        }
        return false;
    }

    void InsertToken(const std::string& tok, bool isOperator)
    {
        if (isError_) {
            expression_.clear();
            cursorPos_ = 0;
            isError_ = false;
        }
        if (hasEvaluated_) {
            if (isOperator) {
                expression_ = "Ans";
                cursorPos_ = expression_.size();
            } else {
                expression_.clear();
                cursorPos_ = 0;
            }
            hasEvaluated_ = false;
        }
        if (cursorPos_ > expression_.size()) cursorPos_ = expression_.size();
        expression_.insert(cursorPos_, tok);
        cursorPos_ += tok.size();
        shift_ = false;
        alpha_ = false;
        dirty_ = true;
    }

    bool HandleCalcOrComplexKey(const std::string& name)
    {
        // Alpha / STO shortcut mapping to variables A..F, X, Y, M
        if (alpha_ || stoPending_) {
            char varCh = 0;
            if (name == "SIN") varCh = 'A';
            else if (name == "COS") varCh = 'B';
            else if (name == "TAN") varCh = 'C';
            else if (name == "FRACCION") varCh = 'D';
            else if (name == "RAIZ") varCh = 'E';
            else if (name == "POTENCIA") varCh = 'F';
            else if (name == "X_VAR") varCh = 'X';
            else if (name == "PAREN_ABRE") varCh = 'Y';
            else if (name == "PAREN_CIERRA") varCh = 'M';
            if (varCh != 0) {
                if (stoPending_) {
                    SetVariable(varCh, lastValue_);
                    stoPending_ = false;
                    alpha_ = false;
                    shift_ = false;
                    resultStr_ = std::string(1, varCh) + "<-" + FormatForDisplay(lastValue_);
                    hasEvaluated_ = true;
                    dirty_ = true;
                    return true;
                }
                InsertToken(std::string(1, varCh), false);
                return true;
            }
        }

        if (name == "AC") {
            expression_.clear();
            resultStr_ = "0";
            hasEvaluated_ = false;
            isError_ = false;
            shift_ = false;
            alpha_ = false;
            stoPending_ = false;
            cursorPos_ = 0;
            dirty_ = true;
            return true;
        }
        if (name == "FORMAT") {
            if (mode_ == AppMode::Complex) {
                complexPolar_ = !complexPolar_;
                if (hasEvaluated_ && !isError_) {
                    resultStr_ = FormatComplex(lastComplex_);
                }
                shift_ = false;
                dirty_ = true;
                return true;
            }
            if (format_ == DisplayFormat::Standard) format_ = DisplayFormat::Scientific;
            else if (format_ == DisplayFormat::Scientific) format_ = DisplayFormat::Fixed2;
            else format_ = DisplayFormat::Standard;
            if (hasEvaluated_ && !isError_) {
                resultStr_ = FormatForDisplay(lastValue_);
            }
            shift_ = false;
            dirty_ = true;
            return true;
        }
        if (name == "IZQUIERDA") {
            if (cursorPos_ > 0) --cursorPos_;
            hasEvaluated_ = false;
            dirty_ = true;
            return true;
        }
        if (name == "DERECHA") {
            if (cursorPos_ < expression_.size()) ++cursorPos_;
            hasEvaluated_ = false;
            dirty_ = true;
            return true;
        }
        if (name == "ARRIBA") {
            if (!lastExpr_.empty()) {
                expression_ = lastExpr_;
                cursorPos_ = expression_.size();
                hasEvaluated_ = false;
                isError_ = false;
                dirty_ = true;
            }
            return true;
        }
        if (name == "ABAJO") {
            if (!isError_ && hasEvaluated_) {
                expression_ = "Ans";
                cursorPos_ = expression_.size();
                hasEvaluated_ = false;
                dirty_ = true;
            }
            return true;
        }
        if (name == "RE_PAG" || name == "AV_PAG") {
            shift_ = false;
            dirty_ = true;
            return true;
        }
        if (name == "DEL" || name == "ATRAS") {
            if (isError_) {
                isError_ = false;
                resultStr_ = "0";
                dirty_ = true;
                return true;
            }
            hasEvaluated_ = false;
            if (cursorPos_ > 0 && !expression_.empty()) {
                static constexpr const char* kMultiTokens[] = {
                    "sqrt(", "asin(", "acos(", "atan(", "conj(", "real(", "imag(",
                    "sin(", "cos(", "tan(", "log(", "abs(", "exp(", "arg(", "ln(",
                    "Ans", "*10^"
                };
                size_t delLen = 1;
                for (const char* tok : kMultiTokens) {
                    const size_t tlen = std::strlen(tok);
                    if (cursorPos_ >= tlen &&
                        expression_.compare(cursorPos_ - tlen, tlen, tok) == 0) {
                        delLen = tlen;
                        break;
                    }
                }
                expression_.erase(cursorPos_ - delLen, delLen);
                cursorPos_ -= delLen;
            }
            shift_ = false;
            dirty_ = true;
            return true;
        }
        if (name == "EXE" || name == "OK") {
            EvaluateCurrent();
            return true;
        }

        if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') {
            InsertToken(name, false);
            return true;
        }
        if (name == "PUNTO") {
            InsertToken(".", false);
            return true;
        }
        if (name == "MAS")   { InsertToken("+", true); return true; }
        if (name == "MENOS") { InsertToken("-", true); return true; }
        if (name == "POR")   { InsertToken("*", true); return true; }
        if (name == "ENTRE" || name == "FRACCION") { InsertToken("/", true); return true; }
        if (name == "POTENCIA") {
            InsertToken(shift_ ? "exp(" : "^", !shift_);
            return true;
        }
        if (name == "CUADRADO") {
            InsertToken(shift_ ? "sqrt(" : "^2", !shift_);
            return true;
        }
        if (name == "RAIZ") {
            InsertToken(shift_ ? "^2" : "sqrt(", shift_);
            return true;
        }
        if (name == "LOG_B") {
            InsertToken(shift_ ? "ln(" : "log(", false);
            return true;
        }
        if (name == "SIN") { InsertToken(shift_ ? "asin(" : "sin(", false); return true; }
        if (name == "COS") { InsertToken(shift_ ? "acos(" : "cos(", false); return true; }
        if (name == "TAN") { InsertToken(shift_ ? "atan(" : "tan(", false); return true; }
        if (name == "PAREN_ABRE") {
            if (mode_ == AppMode::Complex && shift_) {
                InsertToken("@", true);
                return true;
            }
            InsertToken("(", false);
            return true;
        }
        if (name == "PAREN_CIERRA") { InsertToken(")", false); return true; }
        if (name == "ANS")          { InsertToken("Ans", false); return true; }
        if (name == "EXP") {
            InsertToken(shift_ ? "pi" : "*10^", !shift_);
            return true;
        }
        if (name == "X_VAR") {
            if (mode_ == AppMode::Table) {
                InsertToken("x", false);
                return true;
            }
            if (mode_ == AppMode::Complex) {
                InsertToken(shift_ ? "@" : "i", shift_);
                return true;
            }
            InsertToken(shift_ ? "pi" : "e", false);
            return true;
        }
        if (name == "VARIABLE") {
            if (shift_) {
                alpha_ = !alpha_;
                shift_ = false;
                dirty_ = true;
                return true;
            }
            prevMode_ = mode_;
            mode_ = AppMode::VariableMenu;
            menuCursor_ = 0;
            dirty_ = true;
            return true;
        }
        if (name == "FUNCTION") {
            InsertToken(shift_ ? "abs(" : "sqrt(", false);
            return true;
        }
        if (name == "CATALOG") {
            if (shift_) {
                stoPending_ = !stoPending_;
                shift_ = false;
                dirty_ = true;
                return true;
            }
            prevMode_ = mode_;
            mode_ = AppMode::CatalogMenu;
            menuCursor_ = 0;
            dirty_ = true;
            return true;
        }
        return false;
    }

    std::string SubstituteStoredVars(const std::string& expr) const
    {
        std::string out;
        out.reserve(expr.size() + 16);
        for (size_t i = 0; i < expr.size(); ++i) {
            const char c = expr[i];
            int varIdx = -1;
            for (int v = 0; v < 9; ++v) {
                if (c == kVarNames[static_cast<size_t>(v)]) {
                    varIdx = v;
                    break;
                }
            }
            const bool prevAlnum = (i > 0) && std::isalnum(static_cast<unsigned char>(expr[i - 1]));
            const bool nextAlnum = (i + 1 < expr.size()) && std::isalnum(static_cast<unsigned char>(expr[i + 1]));
            if (varIdx >= 0 && !prevAlnum && !nextAlnum) {
                out += "(" + FormatDoubleCompact(vars_[static_cast<size_t>(varIdx)], 15) + ")";
            } else {
                out.push_back(c);
            }
        }
        return out;
    }

    void EvaluateCurrent()
    {
        if (expression_.empty()) {
            resultStr_ = "0";
            hasEvaluated_ = true;
            isError_ = false;
            shift_ = false;
            dirty_ = true;
            return;
        }
        lastExpr_ = expression_;
        shift_ = false;
        alpha_ = false;
        hasEvaluated_ = true;

        if (mode_ == AppMode::Complex) {
            try {
                ComplexMathParser cp(expression_, angleMode_, lastComplex_);
                lastComplex_ = cp.ParseAll();
                lastValue_ = lastComplex_.real();
                lastAns_ = lastComplex_.real();
                isError_ = false;
                resultStr_ = FormatComplex(lastComplex_);
            } catch (const EvalError& err) {
                isError_ = true;
                resultStr_ = err.what();
            } catch (...) {
                isError_ = true;
                resultStr_ = "Syntax ERROR";
            }
            dirty_ = true;
            return;
        }

        const std::string substituted = SubstituteStoredVars(expression_);
        const EvalOutput ev = EvaluateWithAngleMode(substituted, angleMode_, lastAns_);
        if (ev.ok && !ev.isError) {
            isError_ = false;
            lastAns_ = ev.value;
            lastValue_ = ev.value;
            resultStr_ = FormatForDisplay(ev.value);
        } else {
            isError_ = true;
            resultStr_ = ev.errorMessage.empty() ? "Syntax ERROR" : ev.errorMessage;
        }
        dirty_ = true;
    }

    std::string FormatComplex(std::complex<double> z) const
    {
        if (complexPolar_) {
            const double mag = std::abs(z);
            double angRad = std::arg(z);
            const char* unit = "";
            if (angleMode_ == AngleMode::Deg) unit = "\xC2\xB0";
            else if (angleMode_ == AngleMode::Gra) unit = "g";
            if (std::abs(mag) < 1e-12) return std::string("0\xE2\x88\xA0" "0") + unit;
            constexpr double kPi = 3.14159265358979323846;
            double angDisp = angRad;
            if (angleMode_ == AngleMode::Deg) { angDisp = angRad * 180.0 / kPi; }
            else if (angleMode_ == AngleMode::Gra) { angDisp = angRad * 200.0 / kPi; }
            return FormatDoubleCompact(mag, 6) + "\xE2\x88\xA0" + FormatDoubleCompact(angDisp, 6) + unit;
        }
        const double re = (std::abs(z.real()) < 1e-12) ? 0.0 : z.real();
        const double im = (std::abs(z.imag()) < 1e-12) ? 0.0 : z.imag();
        if (im == 0.0) return FormatForDisplay(re);
        if (re == 0.0) return FormatDoubleCompact(im, 8) + "i";
        return FormatDoubleCompact(re, 6) + (im >= 0.0 ? "+" : "") + FormatDoubleCompact(im, 6) + "i";
    }

    void ExecuteMatrixOperation(const std::string& opName)
    {
        const int da = matDim_[0];
        const int db = matDim_[1];
        auto atA = [&](int r, int c) { return matData_[0][static_cast<size_t>(r * da + c)]; };
        auto atB = [&](int r, int c) { return matData_[1][static_cast<size_t>(r * db + c)]; };

        auto fmtMat = [&](int d, const std::array<double, 9>& m) {
            std::ostringstream oss;
            oss << "[";
            for (int r = 0; r < d; ++r) {
                if (r > 0) oss << ",";
                oss << "[";
                for (int c = 0; c < d; ++c) {
                    if (c > 0) oss << ",";
                    oss << FormatDoubleCompact(m[static_cast<size_t>(r * d + c)], 6);
                }
                oss << "]";
            }
            oss << "]";
            return oss.str();
        };

        std::array<double, 9> res{};
        isError_ = false;
        hasEvaluated_ = true;

        if (opName == "MatA+MatB" || opName == "MatA-MatB") {
            if (da != db) { isError_ = true; resultStr_ = "Dimension ERROR"; return; }
            const double sgn = (opName == "MatA+MatB") ? 1.0 : -1.0;
            for (int i = 0; i < da * da; ++i) {
                res[static_cast<size_t>(i)] = matData_[0][static_cast<size_t>(i)] + sgn * matData_[1][static_cast<size_t>(i)];
            }
            resultStr_ = fmtMat(da, res);
            return;
        }
        if (opName == "MatA*MatB") {
            if (da != db) { isError_ = true; resultStr_ = "Dimension ERROR"; return; }
            for (int r = 0; r < da; ++r) {
                for (int c = 0; c < da; ++c) {
                    double acc = 0.0;
                    for (int k = 0; k < da; ++k) acc += atA(r, k) * atB(k, c);
                    res[static_cast<size_t>(r * da + c)] = acc;
                }
            }
            resultStr_ = fmtMat(da, res);
            return;
        }
        if (opName == "tr(MatA)") {
            double tr = 0.0;
            for (int i = 0; i < da; ++i) tr += atA(i, i);
            lastValue_ = tr;
            lastAns_ = tr;
            resultStr_ = FormatForDisplay(tr);
            return;
        }
        if (opName == "transpose(MatA)") {
            for (int r = 0; r < da; ++r) {
                for (int c = 0; c < da; ++c) {
                    res[static_cast<size_t>(c * da + r)] = atA(r, c);
                }
            }
            resultStr_ = fmtMat(da, res);
            return;
        }
        if (opName == "det(MatA)") {
            double det = 0.0;
            if (da == 2) {
                det = atA(0, 0) * atA(1, 1) - atA(0, 1) * atA(1, 0);
            } else {
                det = atA(0, 0) * (atA(1, 1) * atA(2, 2) - atA(1, 2) * atA(2, 1))
                    - atA(0, 1) * (atA(1, 0) * atA(2, 2) - atA(1, 2) * atA(2, 0))
                    + atA(0, 2) * (atA(1, 0) * atA(2, 1) - atA(1, 1) * atA(2, 0));
            }
            lastValue_ = det;
            lastAns_ = det;
            resultStr_ = FormatForDisplay(det);
            return;
        }
        if (opName == "inv(MatA)") {
            // Gauss-Jordan inverse on d x d
            double aug[3][6]{};
            for (int r = 0; r < da; ++r) {
                for (int c = 0; c < da; ++c) aug[r][c] = atA(r, c);
                aug[r][da + r] = 1.0;
            }
            for (int col = 0; col < da; ++col) {
                int piv = col;
                for (int r = col + 1; r < da; ++r) {
                    if (std::abs(aug[r][col]) > std::abs(aug[piv][col])) piv = r;
                }
                if (std::abs(aug[piv][col]) < 1e-12) {
                    isError_ = true;
                    resultStr_ = "Math ERROR";
                    return;
                }
                if (piv != col) {
                    for (int c = 0; c < 2 * da; ++c) std::swap(aug[piv][c], aug[col][c]);
                }
                const double div = aug[col][col];
                for (int c = 0; c < 2 * da; ++c) aug[col][c] /= div;
                for (int r = 0; r < da; ++r) {
                    if (r == col) continue;
                    const double factor = aug[r][col];
                    for (int c = 0; c < 2 * da; ++c) aug[r][c] -= factor * aug[col][c];
                }
            }
            for (int r = 0; r < da; ++r) {
                for (int c = 0; c < da; ++c) {
                    res[static_cast<size_t>(r * da + c)] = aug[r][da + c];
                }
            }
            resultStr_ = fmtMat(da, res);
            return;
        }
        isError_ = true;
        resultStr_ = "Syntax ERROR";
    }

    void GenerateTableRows()
    {
        tableRows_.clear();
        tableScroll_ = 0;
        const double step = (std::abs(tableStep_) < 1e-12) ? 1.0 : std::abs(tableStep_);
        const bool forward = (tableEnd_ >= tableStart_);
        const double signedStep = forward ? step : -step;

        std::ostringstream summary;
        for (int k = 0; k < 64; ++k) {
            const double xVal = tableStart_ + static_cast<double>(k) * signedStep;
            if (forward && xVal > tableEnd_ + 1e-9) break;
            if (!forward && xVal < tableEnd_ - 1e-9) break;

            // Substitute lowercase/uppercase 'x' outside known function names
            std::string exprSub;
            exprSub.reserve(tableExpr_.size() + 16);
            const std::string xTok = "(" + FormatDoubleCompact(xVal, 15) + ")";
            for (size_t i = 0; i < tableExpr_.size(); ++i) {
                const char c = tableExpr_[i];
                const bool prevAlpha = (i > 0) && std::isalpha(static_cast<unsigned char>(tableExpr_[i - 1]));
                const bool nextAlpha = (i + 1 < tableExpr_.size()) && std::isalpha(static_cast<unsigned char>(tableExpr_[i + 1]));
                if ((c == 'x' || c == 'X') && !prevAlpha && !nextAlpha) {
                    exprSub += xTok;
                } else {
                    exprSub.push_back(c);
                }
            }
            const EvalOutput ev = EvaluateWithAngleMode(SubstituteStoredVars(exprSub), angleMode_, lastAns_);
            const std::string yStr = (ev.ok && !ev.isError) ? FormatForDisplay(ev.value) : "ERROR";
            tableRows_.emplace_back(xVal, yStr);
            if (k < 4) {
                if (k > 0) summary << "; ";
                summary << "x=" << FormatDoubleCompact(xVal, 6) << ":" << yStr;
            }
        }
        resultStr_ = tableRows_.empty() ? "Empty" : summary.str();
        hasEvaluated_ = true;
        isError_ = false;
    }

    void SolveCurrentEquation()
    {
        hasEvaluated_ = true;
        isError_ = false;
        if (eqnKind_ == 2) {
            // Quadratic a*x^2 + b*x + c = 0
            const double a = eqnCoeffs_[0];
            const double b = eqnCoeffs_[1];
            const double c = eqnCoeffs_[2];
            if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c)) {
                isError_ = true;
                resultStr_ = "Math ERROR";
                return;
            }
            if (std::abs(a) < 1e-12) {
                if (std::abs(b) < 1e-12) {
                    resultStr_ = (std::abs(c) < 1e-12) ? "Infinite" : "No Solution";
                } else {
                    const double x = -c / b;
                    if (!std::isfinite(x)) {
                        isError_ = true;
                        resultStr_ = "Math ERROR";
                        return;
                    }
                    resultStr_ = "x=" + FormatDoubleCompact(x, 8);
                }
                return;
            }
            const double disc = b * b - 4.0 * a * c;
            if (!std::isfinite(disc)) {
                isError_ = true;
                resultStr_ = "Math ERROR";
                return;
            }
            if (disc >= 0.0) {
                const double sq = std::sqrt(disc);
                const double x1 = (-b + sq) / (2.0 * a);
                const double x2 = (-b - sq) / (2.0 * a);
                if (!std::isfinite(x1) || !std::isfinite(x2)) {
                    isError_ = true;
                    resultStr_ = "Math ERROR";
                    return;
                }
                resultStr_ = "x1=" + FormatDoubleCompact(x1, 8) + ", x2=" + FormatDoubleCompact(x2, 8);
            } else {
                const double reRaw = -b / (2.0 * a);
                const double re = (std::abs(reRaw) < 1e-12) ? 0.0 : reRaw;
                const double im = std::abs(std::sqrt(-disc) / (2.0 * a));
                if (!std::isfinite(re) || !std::isfinite(im)) {
                    isError_ = true;
                    resultStr_ = "Math ERROR";
                    return;
                }
                resultStr_ = "x1=" + FormatDoubleCompact(re, 6) + "+" + FormatDoubleCompact(im, 6) + "i, "
                           + "x2=" + FormatDoubleCompact(re, 6) + "-" + FormatDoubleCompact(im, 6) + "i";
            }
            return;
        }

        // Simultaneous 2-var or 3-var linear system
        const int n = (eqnKind_ == 1) ? 3 : 2;
        double aug[3][4]{};
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c <= n; ++c) {
                aug[r][c] = eqnCoeffs_[static_cast<size_t>(r * (n + 1) + c)];
                if (!std::isfinite(aug[r][c])) {
                    isError_ = true;
                    resultStr_ = "Math ERROR";
                    return;
                }
            }
        }
        int rank = 0;
        for (int col = 0; col < n; ++col) {
            int piv = rank;
            for (int r = rank + 1; r < n; ++r) {
                if (std::abs(aug[r][col]) > std::abs(aug[piv][col])) piv = r;
            }
            if (std::abs(aug[piv][col]) < 1e-12) continue;
            if (piv != rank) {
                for (int c = 0; c <= n; ++c) std::swap(aug[piv][c], aug[rank][c]);
            }
            const double div = aug[rank][col];
            for (int c = col; c <= n; ++c) {
                aug[rank][c] /= div;
                if (!std::isfinite(aug[rank][c])) {
                    isError_ = true;
                    resultStr_ = "Math ERROR";
                    return;
                }
            }
            for (int r = 0; r < n; ++r) {
                if (r == rank) continue;
                const double f = aug[r][col];
                for (int c = col; c <= n; ++c) {
                    aug[r][c] -= f * aug[rank][c];
                    if (!std::isfinite(aug[r][c])) {
                        isError_ = true;
                        resultStr_ = "Math ERROR";
                        return;
                    }
                }
            }
            ++rank;
        }
        if (rank < n) {
            for (int r = rank; r < n; ++r) {
                if (std::abs(aug[r][n]) > 1e-9) {
                    resultStr_ = "No Solution";
                    return;
                }
            }
            resultStr_ = "Infinite";
            return;
        }
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(aug[i][n])) {
                isError_ = true;
                resultStr_ = "Math ERROR";
                return;
            }
        }
        static constexpr const char* kUnknowns[3] = { "x", "y", "z" };
        std::ostringstream oss;
        for (int i = 0; i < n; ++i) {
            if (i > 0) oss << ", ";
            oss << kUnknowns[i] << "=" << FormatDoubleCompact(aug[i][n], 8);
        }
        resultStr_ = oss.str();
    }

    std::string FormatForDisplay(double val) const
    {
        if (!std::isfinite(val)) return "Math ERROR";
        if (std::abs(val) < 1e-12) val = 0.0;
        if (format_ == DisplayFormat::Scientific) {
            std::ostringstream oss;
            oss << std::scientific << std::setprecision(6) << val;
            return oss.str();
        }
        if (format_ == DisplayFormat::Fixed2) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << val;
            return oss.str();
        }
        return FormatDoubleCompact(val, 10);
    }

    static std::string NormalizeForAsciiLcd(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size();) {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c == 0xC3 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x97) {
                out.push_back('*');
                i += 2;
            } else if (c == 0xC3 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0xB7) {
                out.push_back('/');
                i += 2;
            } else if (c == 0xE2 && i + 2 < s.size() &&
                       static_cast<unsigned char>(s[i + 1]) == 0x88 &&
                       static_cast<unsigned char>(s[i + 2]) == 0x92) {
                out.push_back('-');
                i += 3;
            } else if (c == 0xE2 && i + 2 < s.size() &&
                       static_cast<unsigned char>(s[i + 1]) == 0x88 &&
                       static_cast<unsigned char>(s[i + 2]) == 0xA0) {
                out.push_back('@');
                i += 3;
            } else if (c == 0xC2 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0xB0) {
                out.push_back('d');
                i += 2;
            } else if (c >= 0x20 && c <= 0x7E) {
                out.push_back(static_cast<char>(c));
                ++i;
            } else {
                ++i;
            }
        }
        return out;
    }
};

} // namespace CasioLinux::detail
