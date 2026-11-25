#include "engineering_cas.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_set>

using json = nlohmann::json;

namespace DocStudio::Cas {

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string TrimUtf8(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string FormatRealClean(double v, int sig = 6) {
    if (!std::isfinite(v)) return "NaN";
    if (std::abs(v) < 1e-12) return "0";
    std::ostringstream oss;
    oss.precision((std::max)(2, sig));
    oss << v;
    return oss.str();
}

bool IsValidIdentifier(const std::string& s) {
    if (s.empty()) return false;
    const unsigned char c0 = static_cast<unsigned char>(s[0]);
    if (!std::isalpha(c0) && c0 != '_' && c0 < 0x80) return false;
    for (unsigned char c : s) {
        if (!std::isalnum(c) && c != '_' && c < 0x80) return false;
    }
    return true;
}

double AngleScaleToRad(CasAngleMode mode) noexcept {
    if (mode == CasAngleMode::Deg) return kPi / 180.0;
    if (mode == CasAngleMode::Gra) return kPi / 200.0;
    return 1.0;
}

double AngleScaleFromRad(CasAngleMode mode) noexcept {
    if (mode == CasAngleMode::Deg) return 180.0 / kPi;
    if (mode == CasAngleMode::Gra) return 200.0 / kPi;
    return 1.0;
}

const char* AngleSuffixUtf8(CasAngleMode mode) noexcept {
    if (mode == CasAngleMode::Deg) return "\xC2\xB0";
    if (mode == CasAngleMode::Gra) return " grad";
    return " rad";
}

struct UnitDef {
    double scale = 1.0;
    double offset = 0.0;
    UnitDim dim{};
};

UnitDim MakeDim(int m, int l, int t, int i = 0, int th = 0, int n = 0, int j = 0) noexcept {
    return UnitDim{ { m, l, t, i, th, n, j } };
}

bool LookupBaseUnit(const std::string& rawTok, UnitDef& out) {
    std::string tok = rawTok;
    auto replaceAll = [&](const std::string& from, const std::string& to) {
        std::size_t pos = 0;
        while ((pos = tok.find(from, pos)) != std::string::npos) {
            tok.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replaceAll("\xCE\xA9", "Ohm");     // Greek Capital Omega
    replaceAll("\xE2\x84\xA6", "Ohm"); // Ohm sign
    replaceAll("\xC2\xB5", "u");       // micro sign
    replaceAll("\xCE\xBC", "u");       // Greek mu

    static const std::unordered_map<std::string, UnitDef> kExactUnits = {
        { "1",     { 1.0, 0.0, MakeDim(0, 0, 0) } },
        { "%",     { 0.01, 0.0, MakeDim(0, 0, 0) } },
        { "rad",   { 1.0, 0.0, MakeDim(0, 0, 0) } },
        { "deg",   { kPi / 180.0, 0.0, MakeDim(0, 0, 0) } },
        { "gra",   { kPi / 200.0, 0.0, MakeDim(0, 0, 0) } },
        { "m",     { 1.0, 0.0, MakeDim(0, 1, 0) } },
        { "in",    { 0.0254, 0.0, MakeDim(0, 1, 0) } },
        { "ft",    { 0.3048, 0.0, MakeDim(0, 1, 0) } },
        { "yd",    { 0.9144, 0.0, MakeDim(0, 1, 0) } },
        { "mi",    { 1609.344, 0.0, MakeDim(0, 1, 0) } },
        { "Angstrom", { 1e-10, 0.0, MakeDim(0, 1, 0) } },
        { "ha",    { 1e4, 0.0, MakeDim(0, 2, 0) } },
        { "acre",  { 4046.8564224, 0.0, MakeDim(0, 2, 0) } },
        { "g",     { 1e-3, 0.0, MakeDim(1, 0, 0) } },
        { "lb",    { 0.45359237, 0.0, MakeDim(1, 0, 0) } },
        { "oz",    { 0.028349523125, 0.0, MakeDim(1, 0, 0) } },
        { "ton",   { 1000.0, 0.0, MakeDim(1, 0, 0) } },
        { "slug",  { 14.593902937, 0.0, MakeDim(1, 0, 0) } },
        { "s",     { 1.0, 0.0, MakeDim(0, 0, 1) } },
        { "min",   { 60.0, 0.0, MakeDim(0, 0, 1) } },
        { "h",     { 3600.0, 0.0, MakeDim(0, 0, 1) } },
        { "hr",    { 3600.0, 0.0, MakeDim(0, 0, 1) } },
        { "Hz",    { 1.0, 0.0, MakeDim(0, 0, -1) } },
        { "rpm",   { 2.0 * kPi / 60.0, 0.0, MakeDim(0, 0, -1) } },
        { "mph",   { 0.44704, 0.0, MakeDim(0, 1, -1) } },
        { "N",     { 1.0, 0.0, MakeDim(1, 1, -2) } },
        { "dyn",   { 1e-5, 0.0, MakeDim(1, 1, -2) } },
        { "lbf",   { 4.4482216152605, 0.0, MakeDim(1, 1, -2) } },
        { "kip",   { 4448.2216152605, 0.0, MakeDim(1, 1, -2) } },
        { "Nm",    { 1.0, 0.0, MakeDim(1, 2, -2) } },
        { "J",     { 1.0, 0.0, MakeDim(1, 2, -2) } },
        { "erg",   { 1e-7, 0.0, MakeDim(1, 2, -2) } },
        { "cal",   { 4.184, 0.0, MakeDim(1, 2, -2) } },
        { "BTU",   { 1055.05585262, 0.0, MakeDim(1, 2, -2) } },
        { "eV",    { 1.602176634e-19, 0.0, MakeDim(1, 2, -2) } },
        { "Wh",    { 3600.0, 0.0, MakeDim(1, 2, -2) } },
        { "W",     { 1.0, 0.0, MakeDim(1, 2, -3) } },
        { "hp",    { 745.699872, 0.0, MakeDim(1, 2, -3) } },
        { "L",     { 1e-3, 0.0, MakeDim(0, 3, 0) } },
        { "l",     { 1e-3, 0.0, MakeDim(0, 3, 0) } },
        { "gal",   { 0.003785411784, 0.0, MakeDim(0, 3, 0) } },
        { "Pa",    { 1.0, 0.0, MakeDim(1, -1, -2) } },
        { "bar",   { 1e5, 0.0, MakeDim(1, -1, -2) } },
        { "psi",   { 6894.757293168, 0.0, MakeDim(1, -1, -2) } },
        { "ksi",   { 6894757.293168, 0.0, MakeDim(1, -1, -2) } },
        { "psf",   { 47.8802589803, 0.0, MakeDim(1, -1, -2) } },
        { "ksf",   { 47880.2589803, 0.0, MakeDim(1, -1, -2) } },
        { "atm",   { 101325.0, 0.0, MakeDim(1, -1, -2) } },
        { "torr",  { 101325.0 / 760.0, 0.0, MakeDim(1, -1, -2) } },
        { "A",     { 1.0, 0.0, MakeDim(0, 0, 0, 1) } },
        { "Ah",    { 3600.0, 0.0, MakeDim(0, 0, 1, 1) } },
        { "V",     { 1.0, 0.0, MakeDim(1, 2, -3, -1) } },
        { "Ohm",   { 1.0, 0.0, MakeDim(1, 2, -3, -2) } },
        { "ohm",   { 1.0, 0.0, MakeDim(1, 2, -3, -2) } },
        { "S",     { 1.0, 0.0, MakeDim(-1, -2, 3, 2) } },
        { "F",     { 1.0, 0.0, MakeDim(-1, -2, 4, 2) } },
        { "H",     { 1.0, 0.0, MakeDim(1, 2, -2, -2) } },
        { "C",     { 1.0, 0.0, MakeDim(0, 0, 1, 1) } },
        { "Wb",    { 1.0, 0.0, MakeDim(1, 2, -2, -1) } },
        { "T",     { 1.0, 0.0, MakeDim(1, 0, -2, -1) } },
        { "K",     { 1.0, 0.0, MakeDim(0, 0, 0, 0, 1) } },
        { "degR",  { 5.0 / 9.0, 0.0, MakeDim(0, 0, 0, 0, 1) } },
        { "degC",  { 1.0, 273.15, MakeDim(0, 0, 0, 0, 1) } },
        { "degF",  { 5.0 / 9.0, 255.3722222222222, MakeDim(0, 0, 0, 0, 1) } },
        { "mol",   { 1.0, 0.0, MakeDim(0, 0, 0, 0, 0, 1) } },
        { "cd",    { 1.0, 0.0, MakeDim(0, 0, 0, 0, 0, 0, 1) } },
        { "lm",    { 1.0, 0.0, MakeDim(0, 0, 0, 0, 0, 0, 1) } },
        { "lx",    { 1.0, 0.0, MakeDim(0, -2, 0, 0, 0, 0, 1) } }
    };

    auto it = kExactUnits.find(tok);
    if (it != kExactUnits.end()) {
        out = it->second;
        return true;
    }

    if (tok.size() >= 2) {
        double prefixMul = 1.0;
        std::size_t plen = 1;
        switch (tok[0]) {
        case 'p': prefixMul = 1e-12; break;
        case 'n': prefixMul = 1e-9; break;
        case 'u': prefixMul = 1e-6; break;
        case 'm': prefixMul = 1e-3; break;
        case 'c': prefixMul = 1e-2; break;
        case 'k': prefixMul = 1e3; break;
        case 'M': prefixMul = 1e6; break;
        case 'G': prefixMul = 1e9; break;
        case 'T': prefixMul = 1e12; break;
        default: plen = 0; break;
        }
        if (plen == 1) {
            const std::string base = tok.substr(1);
            if (base != "%" && base != "degC" && base != "degF" && base != "degR" &&
                base != "min" && base != "hr" && base != "in" && base != "ft" && base != "mph") {
                auto bit = kExactUnits.find(base);
                if (bit != kExactUnits.end() && bit->second.offset == 0.0) {
                    out = bit->second;
                    out.scale *= prefixMul;
                    return true;
                }
            }
        }
    }
    return false;
}

bool TryParseUnitTokenWithPower(const std::string& tok, UnitDef& out) {
    const auto caret = tok.find('^');
    if (caret == std::string::npos) {
        return LookupBaseUnit(tok, out);
    }
    const std::string baseName = tok.substr(0, caret);
    const std::string expStr = tok.substr(caret + 1);
    if (baseName.empty() || expStr.empty()) return false;
    int p = 0;
    try {
        std::size_t idx = 0;
        p = std::stoi(expStr, &idx);
        if (idx != expStr.size()) return false;
    } catch (...) {
        return false;
    }
    UnitDef base{};
    if (!LookupBaseUnit(baseName, base) || base.offset != 0.0) return false;
    out.dim = base.dim.Pow(p);
    out.scale = std::pow(base.scale, static_cast<double>(p));
    out.offset = 0.0;
    return true;
}

bool IsSimplePreferredUnit(const std::string& u) {
    return !u.empty() && u.find('*') == std::string::npos && u.find('/') == std::string::npos && u.find('^') == std::string::npos;
}

bool IsHyphenatedCompoundUnitPair(const std::string& lhs, const std::string& rhs) {
    static const std::unordered_set<std::string> kPairs = {
        "ft-kip", "kip-ft", "N-m", "kN-m", "MN-m", "N-mm", "N-cm", "mN-m",
        "lbf-ft", "ft-lbf", "lbf-in", "in-lbf", "lb-ft", "ft-lb", "lb-in", "in-lb",
        "kW-h", "W-h", "MW-h", "kW-hr", "W-hr", "MW-hr", "A-h", "mA-h", "A-hr", "mA-hr",
        "Pa-s", "mPa-s", "kPa-s", "kg-m", "g-cm"
    };
    return kPairs.find(lhs + "-" + rhs) != kPairs.end();
}

bool EvalCasFunction(
    const std::string& fn,
    const std::vector<QuantityValue>& args,
    const CasEvalOptions& opts,
    QuantityValue& out,
    std::string& outErr,
    std::string& outDiag)
{
    auto needArgs = [&](std::size_t n) -> bool {
        if (args.size() != n) {
            outErr = fn + " requiere " + std::to_string(n) + " argumento(s)";
            outDiag = "SYNTAX_ERR";
            return false;
        }
        return true;
    };
    if (fn == "abs") {
        if (!needArgs(1)) return false;
        out = args[0];
        out.value = { std::abs(args[0].value), 0.0 };
        out.preferPolar = false;
        return true;
    }
    if (fn == "arg" || fn == "phase") {
        if (!needArgs(1)) return false;
        out.value = { std::arg(args[0].value) * AngleScaleFromRad(opts.polarAngleMode), 0.0 };
        out.dim = {};
        out.preferredUnitUtf8.clear();
        out.unitScaleToSi = 1.0;
        out.unitOffsetToSi = 0.0;
        return true;
    }
    if (fn == "re") {
        if (!needArgs(1)) return false;
        out = args[0];
        out.value = { args[0].value.real(), 0.0 };
        out.preferPolar = false;
        return true;
    }
    if (fn == "im") {
        if (!needArgs(1)) return false;
        out = args[0];
        out.value = { args[0].value.imag(), 0.0 };
        out.preferPolar = false;
        return true;
    }
    if (fn == "conj") {
        if (!needArgs(1)) return false;
        out = args[0];
        out.value = std::conj(args[0].value);
        return true;
    }
    if (fn == "rect") {
        if (args.size() == 1) {
            out = args[0];
            out.preferPolar = false;
            return true;
        }
        if (args.size() == 2) {
            if (!args[1].dim.IsDimensionless() || !args[1].IsReal()) {
                outErr = "rect(r, theta) requiere angulo real adimensional";
                outDiag = "DIM_ERR";
                return false;
            }
            out = args[0];
            const double mag = args[0].value.real();
            const double rad = args[1].value.real() * AngleScaleToRad(opts.polarAngleMode);
            out.value = std::polar(mag, rad);
            out.preferPolar = false;
            return true;
        }
        outErr = "rect requiere 1 o 2 argumentos";
        outDiag = "SYNTAX_ERR";
        return false;
    }
    if (fn == "polar") {
        if (args.size() == 1) {
            out = args[0];
            out.preferPolar = true;
            return true;
        }
        if (args.size() == 2) {
            if (args[0].dim != args[1].dim || !args[0].IsReal() || !args[1].IsReal()) {
                outErr = "polar(x, y) requiere dos magnitudes reales de igual dimension";
                outDiag = "DIM_ERR";
                return false;
            }
            out = args[0];
            out.value = { args[0].value.real(), args[1].value.real() };
            out.preferPolar = true;
            return true;
        }
        outErr = "polar requiere 1 o 2 argumentos";
        outDiag = "SYNTAX_ERR";
        return false;
    }
    if (fn == "sqrt") {
        if (!needArgs(1)) return false;
        for (int e : args[0].dim.exp) {
            if ((e % 2) != 0) {
                outErr = "sqrt requiere exponentes dimensionales pares";
                outDiag = "DIM_ERR";
                return false;
            }
        }
        UnitDim d{};
        for (std::size_t k = 0; k < 7; ++k) d.exp[k] = args[0].dim.exp[k] / 2;
        out.dim = d;
        out.value = std::sqrt(args[0].value);
        out.preferredUnitUtf8.clear();
        out.unitScaleToSi = 1.0;
        out.unitOffsetToSi = 0.0;
        out.preferPolar = args[0].preferPolar;
        return true;
    }
    if (fn == "sin" || fn == "cos" || fn == "tan" || fn == "exp" || fn == "ln" ||
        fn == "log10" || fn == "log" || fn == "asin" || fn == "acos" || fn == "atan" ||
        fn == "sinh" || fn == "cosh" || fn == "tanh" || fn == "asinh" || fn == "acosh" || fn == "atanh" ||
        fn == "rad" || fn == "deg" || fn == "floor" || fn == "ceil" || fn == "round") {
        if (!needArgs(1)) return false;
        if (!args[0].dim.IsDimensionless()) {
            outErr = fn + " solo acepta valores adimensionales";
            outDiag = "DIM_ERR";
            return false;
        }
        out.dim = {};
        out.preferredUnitUtf8.clear();
        out.unitScaleToSi = 1.0;
        out.unitOffsetToSi = 0.0;
        const std::complex<double> z = args[0].value;
        const double toRad = AngleScaleToRad(opts.trigAngleMode);
        const double fromRad = AngleScaleFromRad(opts.trigAngleMode);

        if (fn == "sin") out.value = std::sin(z * toRad);
        else if (fn == "cos") out.value = std::cos(z * toRad);
        else if (fn == "tan") out.value = std::tan(z * toRad);
        else if (fn == "asin") out.value = std::asin(z) * fromRad;
        else if (fn == "acos") out.value = std::acos(z) * fromRad;
        else if (fn == "atan") out.value = std::atan(z) * fromRad;
        else if (fn == "sinh") out.value = std::sinh(z);
        else if (fn == "cosh") out.value = std::cosh(z);
        else if (fn == "tanh") out.value = std::tanh(z);
        else if (fn == "asinh") out.value = std::asinh(z);
        else if (fn == "acosh") out.value = std::acosh(z);
        else if (fn == "atanh") out.value = std::atanh(z);
        else if (fn == "exp") out.value = std::exp(z);
        else if (fn == "ln") out.value = std::log(z);
        else if (fn == "log10" || fn == "log") out.value = std::log10(z);
        else if (fn == "rad") out.value = z * (kPi / 180.0);
        else if (fn == "deg") out.value = z * (180.0 / kPi);
        else if (fn == "floor") out.value = { std::floor(z.real()), 0.0 };
        else if (fn == "ceil") out.value = { std::ceil(z.real()), 0.0 };
        else if (fn == "round") out.value = { std::round(z.real()), 0.0 };
        return true;
    }
    if (fn == "sind" || fn == "cosd" || fn == "tand") {
        if (!needArgs(1)) return false;
        if (!args[0].dim.IsDimensionless() || !args[0].IsReal()) {
            outErr = fn + " requiere argumento real en grados";
            outDiag = "DIM_ERR";
            return false;
        }
        out.dim = {};
        out.preferredUnitUtf8.clear();
        out.unitScaleToSi = 1.0;
        out.unitOffsetToSi = 0.0;
        const double r = args[0].value.real() * (kPi / 180.0);
        if (fn == "sind") out.value = { std::sin(r), 0.0 };
        else if (fn == "cosd") out.value = { std::cos(r), 0.0 };
        else out.value = { std::tan(r), 0.0 };
        return true;
    }
    if (fn == "atan2") {
        if (!needArgs(2)) return false;
        if (args[0].dim != args[1].dim || !args[0].IsReal() || !args[1].IsReal()) {
            outErr = "atan2 requiere dos magnitudes reales con igual dimensi\xC3\xB3n";
            outDiag = "DIM_ERR";
            return false;
        }
        out.dim = {};
        out.preferredUnitUtf8.clear();
        out.unitScaleToSi = 1.0;
        out.unitOffsetToSi = 0.0;
        out.value = { std::atan2(args[0].value.real(), args[1].value.real()) * AngleScaleFromRad(opts.trigAngleMode), 0.0 };
        return true;
    }
    if (fn == "pow") {
        if (!needArgs(2)) return false;
        if (!args[1].dim.IsDimensionless()) {
            outErr = "Exponente de pow debe ser adimensional";
            outDiag = "DIM_ERR";
            return false;
        }
        out.preferredUnitUtf8.clear();
        out.unitScaleToSi = 1.0;
        out.unitOffsetToSi = 0.0;
        if (!args[0].dim.IsDimensionless()) {
            if (!args[1].IsReal() || std::abs(args[1].value.real() - std::round(args[1].value.real())) > 1e-9) {
                outErr = "pow sobre magnitud con unidades requiere exponente entero";
                outDiag = "DIM_ERR";
                return false;
            }
            const int p = static_cast<int>(std::llround(args[1].value.real()));
            out.dim = args[0].dim.Pow(p);
            out.value = std::pow(args[0].value, p);
        } else {
            out.dim = {};
            out.value = std::pow(args[0].value, args[1].value);
        }
        return true;
    }
    if (fn == "min" || fn == "max") {
        if (!needArgs(2)) return false;
        if (args[0].dim != args[1].dim) {
            outErr = fn + " requiere dimensiones compatibles";
            outDiag = "DIM_ERR";
            return false;
        }
        out = args[0];
        out.value = (fn == "min")
            ? std::complex<double>((std::min)(args[0].value.real(), args[1].value.real()), 0.0)
            : std::complex<double>((std::max)(args[0].value.real(), args[1].value.real()), 0.0);
        return true;
    }
    outErr = "Funci\xC3\xB3n desconocida: " + fn;
    outDiag = "SYNTAX_ERR";
    return false;
}

enum class TokKind {
    End, Number, Ident, Plus, Minus, Star, Slash, Caret, LParen, RParen, Comma, Equal, Angle, Pipe, Parallel
};

struct Token {
    TokKind kind = TokKind::End;
    std::string text;
    double numVal = 0.0;
    bool imagSuffix = false;
    bool isHyphenUnitSep = false;
};

struct ParsedVal {
    QuantityValue q{};
    bool isRawAffine = false;
};

class ExprParser {
public:
    ExprParser(const std::string& src, const SymbolEnv& env, const CasEvalOptions& opts)
        : src_(src), env_(env), opts_(opts)
    {
        Tokenize();
    }

    CasEvalResult ParseAndEval() {
        CasEvalResult res{};
        if (tokErr_) {
            res.ok = false;
            res.errorUtf8 = tokErrMsg_;
            res.diagnosticCode = "SYNTAX_ERR";
            return res;
        }
        if (tokens_.empty() || tokens_[0].kind == TokKind::End) {
            res.ok = false;
            res.errorUtf8 = "Expresi\xC3\xB3n vac\xC3\xAD""a";
            res.diagnosticCode = "SYNTAX_ERR";
            return res;
        }

        if (tokens_.size() >= 2 && tokens_[0].kind == TokKind::Ident && tokens_[1].kind == TokKind::Equal) {
            res.hasAssignment = true;
            res.assignedVarUtf8 = tokens_[0].text;
            pos_ = 2;
        }

        QuantityValue val{};
        std::string varSub;
        std::string err;
        std::string diag = "SYNTAX_ERR";
        if (!ParseConversion(val, varSub, err, diag)) {
            res.ok = false;
            res.errorUtf8 = err;
            res.diagnosticCode = diag;
            return res;
        }
        if (Peek().kind != TokKind::End) {
            res.ok = false;
            res.errorUtf8 = "Token inesperado: " + Peek().text;
            res.diagnosticCode = "SYNTAX_ERR";
            return res;
        }
        if (!std::isfinite(val.value.real()) || !std::isfinite(val.value.imag())) {
            res.ok = false;
            res.errorUtf8 = "Resultado matem\xC3\xA1tico no finito";
            res.diagnosticCode = "MATH_ERR";
            return res;
        }

        if (opts_.forcePolarOutput) val.preferPolar = true;
        else if (opts_.forceRectOutput) val.preferPolar = false;

        res.ok = true;
        res.diagnosticCode = "OK";
        res.result = val;
        res.formattedResultUtf8 = val.FormatUtf8(6, opts_.polarAngleMode);
        res.varSubExpansionUtf8 = varSub;
        return res;
    }

private:
    std::string src_;
    const SymbolEnv& env_;
    CasEvalOptions opts_;
    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    bool tokErr_ = false;
    std::string tokErrMsg_;

    const Token& Peek(std::size_t lookahead = 0) const {
        static const Token kEnd{};
        if (pos_ + lookahead >= tokens_.size()) return kEnd;
        return tokens_[pos_ + lookahead];
    }

    Token Advance() {
        Token t = Peek();
        if (pos_ < tokens_.size()) ++pos_;
        return t;
    }

    static bool IsIdentStart(unsigned char c) {
        return std::isalpha(c) != 0 || c == '_' || c >= 0x80;
    }

    static bool IsIdentCont(unsigned char c) {
        return std::isalnum(c) != 0 || c == '_' || c == '%' || c >= 0x80;
    }

    void Tokenize() {
        std::size_t i = 0;
        bool afterPipe = false;
        while (i < src_.size()) {
            const unsigned char ch = static_cast<unsigned char>(src_[i]);
            if (std::isspace(ch)) {
                ++i;
                continue;
            }
            if (i + 3 <= src_.size() &&
                static_cast<unsigned char>(src_[i]) == 0xE2 &&
                static_cast<unsigned char>(src_[i + 1]) == 0x88 &&
                static_cast<unsigned char>(src_[i + 2]) == 0xA0) {
                tokens_.push_back({ TokKind::Angle, "\xE2\x88\xA0", 0.0, false });
                i += 3;
                continue;
            }
            if (i + 2 <= src_.size() &&
                static_cast<unsigned char>(src_[i]) == 0xC2 &&
                static_cast<unsigned char>(src_[i + 1]) == 0xB0) {
                tokens_.push_back({ TokKind::Ident, "deg", 0.0, false });
                i += 2;
                continue;
            }
            if (std::isdigit(ch) || (ch == '.' && i + 1 < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i + 1])))) {
                std::size_t start = i;
                while (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) ++i;
                if (i < src_.size() && src_[i] == '.') {
                    ++i;
                    while (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) ++i;
                }
                if (i < src_.size() && (src_[i] == 'e' || src_[i] == 'E')) {
                    std::size_t expSave = i;
                    ++i;
                    if (i < src_.size() && (src_[i] == '+' || src_[i] == '-')) ++i;
                    if (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) {
                        while (i < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i]))) ++i;
                    } else {
                        i = expSave;
                    }
                }
                double val = 0.0;
                try {
                    val = std::stod(src_.substr(start, i - start));
                } catch (...) {
                    tokErr_ = true;
                    tokErrMsg_ = "N\xC3\xBAmero inv\xC3\xA1lido";
                    return;
                }
                bool isImag = false;
                if (i < src_.size() && (src_[i] == 'j' || src_[i] == 'i') &&
                    (i + 1 >= src_.size() || !IsIdentCont(static_cast<unsigned char>(src_[i + 1])))) {
                    isImag = true;
                    ++i;
                } else if (i < src_.size() && (i + 1 >= src_.size() || !IsIdentCont(static_cast<unsigned char>(src_[i + 1])))) {
                    // Inline engineering suffix when unambiguous (never intercept 'm' meter, 'K' Kelvin, 'T' Tesla, or bound vars in env_)
                    const char p = src_[i];
                    if (env_.find(std::string(1, p)) == env_.end()) {
                        double mul = 1.0;
                        if (p == 'p') mul = 1e-12;
                        else if (p == 'n') mul = 1e-9;
                        else if (p == 'u') mul = 1e-6;
                        else if (p == 'k') mul = 1e3;
                        else if (p == 'M') mul = 1e6;
                        else if (p == 'G') mul = 1e9;
                        if (mul != 1.0) {
                            val *= mul;
                            ++i;
                        }
                    }
                }
                tokens_.push_back({ TokKind::Number, src_.substr(start, i - start), val, isImag });
                continue;
            }
            if (ch == '%') {
                tokens_.push_back({ TokKind::Ident, "%", 0.0, false });
                ++i;
                continue;
            }
            if (IsIdentStart(ch)) {
                std::size_t start = i;
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
                tokens_.push_back({ TokKind::Ident, src_.substr(start, i - start), 0.0, false });
                // Bug 4 + FIND-07 fix: recognize hyphenated compound units (e.g. "ft-kip", "kip-ft", "N-m", "lbf-ft")
                // ONLY when in allowlist AND neither side is a bound variable in env_
                while (!afterPipe && i < src_.size() && src_[i] == '-' && i + 1 < src_.size() &&
                       IsIdentStart(static_cast<unsigned char>(src_[i + 1]))) {
                    const std::string& lhsWord = tokens_.back().text;
                    if (env_.find(lhsWord) != env_.end()) break;
                    UnitDef lhsU{};
                    if (!LookupBaseUnit(lhsWord, lhsU) || lhsU.offset != 0.0) break;
                    std::size_t j = i + 1;
                    while (j < src_.size() && IsIdentCont(static_cast<unsigned char>(src_[j]))) {
                        if (j + 3 <= src_.size() &&
                            static_cast<unsigned char>(src_[j]) == 0xE2 &&
                            static_cast<unsigned char>(src_[j + 1]) == 0x88 &&
                            static_cast<unsigned char>(src_[j + 2]) == 0xA0) break;
                        if (j + 2 <= src_.size() &&
                            static_cast<unsigned char>(src_[j]) == 0xC2 &&
                            static_cast<unsigned char>(src_[j + 1]) == 0xB0) break;
                        ++j;
                    }
                    const std::string rhsWord = src_.substr(i + 1, j - (i + 1));
                    if (env_.find(rhsWord) != env_.end()) break;
                    if (!IsHyphenatedCompoundUnitPair(lhsWord, rhsWord)) break;
                    UnitDef rhsU{};
                    if (!LookupBaseUnit(rhsWord, rhsU) || rhsU.offset != 0.0 || lhsU.dim == rhsU.dim) break;
                    Token starTok{ TokKind::Star, "*", 0.0, false, true };
                    tokens_.push_back(starTok);
                    tokens_.push_back({ TokKind::Ident, rhsWord, 0.0, false, false });
                    i = j;
                }
                continue;
            }
            if (src_[i] == '|' && i + 1 < src_.size() && src_[i + 1] == '|') {
                tokens_.push_back({ TokKind::Parallel, "||", 0.0, false });
                i += 2;
                continue;
            }
            switch (src_[i]) {
            case '+': tokens_.push_back({ TokKind::Plus, "+", 0.0, false }); break;
            case '-': tokens_.push_back({ TokKind::Minus, "-", 0.0, false }); break;
            case '*': tokens_.push_back({ TokKind::Star, "*", 0.0, false }); break;
            case '/': tokens_.push_back({ TokKind::Slash, "/", 0.0, false }); break;
            case '^': tokens_.push_back({ TokKind::Caret, "^", 0.0, false }); break;
            case '(': tokens_.push_back({ TokKind::LParen, "(", 0.0, false }); break;
            case ')': tokens_.push_back({ TokKind::RParen, ")", 0.0, false }); break;
            case ',': tokens_.push_back({ TokKind::Comma, ",", 0.0, false }); break;
            case '=': tokens_.push_back({ TokKind::Equal, "=", 0.0, false }); break;
            case '@': tokens_.push_back({ TokKind::Angle, "@", 0.0, false }); break;
            case '<': tokens_.push_back({ TokKind::Angle, "<", 0.0, false }); break;
            case '|':
                tokens_.push_back({ TokKind::Pipe, "|", 0.0, false });
                afterPipe = true;
                break;
            default:
                tokErr_ = true;
                tokErrMsg_ = std::string("Car\xC3\xA1""cter no soportado: ") + src_[i];
                return;
            }
            ++i;
        }
        tokens_.push_back({ TokKind::End, "", 0.0, false });
    }

    bool ParseConversion(QuantityValue& out, std::string& outSub, std::string& outErr, std::string& outDiag) {
        if (!ParseAddSub(out, outSub, outErr, outDiag)) return false;
        if (Peek().kind == TokKind::Pipe) {
            Advance();
            std::string unitTarget;
            while (Peek().kind != TokKind::End) {
                unitTarget += Advance().text;
            }
            unitTarget = TrimUtf8(unitTarget);
            if (unitTarget == "rect") {
                out.preferPolar = false;
                outSub += " | rect";
                return true;
            }
            if (unitTarget == "polar") {
                out.preferPolar = true;
                outSub += " | polar";
                return true;
            }
            UnitDim targetDim{};
            double scale = 1.0, offset = 0.0;
            if (!TryParseUnitExpression(unitTarget, targetDim, scale, offset, &outErr)) {
                outDiag = "SYNTAX_ERR";
                return false;
            }
            if (out.dim != targetDim) {
                outErr = "Conversi\xC3\xB3n dimensional incompatible: [" + out.dim.FormatSiString() +
                         "] != [" + targetDim.FormatSiString() + "]";
                outDiag = "DIM_ERR";
                return false;
            }
            out.preferredUnitUtf8 = unitTarget;
            out.unitScaleToSi = scale;
            out.unitOffsetToSi = offset;
            outSub += " | " + unitTarget;
        }
        return true;
    }

    bool ParseAddSub(QuantityValue& out, std::string& outSub, std::string& outErr, std::string& outDiag) {
        if (!ParseParallel(out, outSub, outErr, outDiag)) return false;
        while (Peek().kind == TokKind::Plus || Peek().kind == TokKind::Minus) {
            const TokKind op = Advance().kind;
            QuantityValue rhs{};
            std::string rhsSub;
            if (!ParseParallel(rhs, rhsSub, outErr, outDiag)) return false;
            if (out.dim != rhs.dim) {
                outErr = "Error dimensional: no se puede " +
                         std::string(op == TokKind::Plus ? "sumar" : "restar") +
                         " [" + out.dim.FormatSiString() + "] y [" + rhs.dim.FormatSiString() + "]";
                outDiag = "DIM_ERR";
                return false;
            }
            out.value = (op == TokKind::Plus) ? (out.value + rhs.value) : (out.value - rhs.value);
            if (out.unitOffsetToSi != 0.0 || rhs.unitOffsetToSi != 0.0) {
                // Temperature deltas/sums of affine temperatures format in Kelvin rather than re-offsetting 273.15
                out.preferredUnitUtf8.clear();
                out.unitScaleToSi = 1.0;
                out.unitOffsetToSi = 0.0;
            } else if (out.preferredUnitUtf8.empty() && !rhs.preferredUnitUtf8.empty()) {
                out.preferredUnitUtf8 = rhs.preferredUnitUtf8;
                out.unitScaleToSi = rhs.unitScaleToSi;
                out.unitOffsetToSi = rhs.unitOffsetToSi;
            }
            outSub += (op == TokKind::Plus ? " + " : " - ") + rhsSub;
        }
        return true;
    }

    bool ParseParallel(QuantityValue& out, std::string& outSub, std::string& outErr, std::string& outDiag) {
        ParsedVal pv{};
        if (!ParseMulDiv(pv, outSub, outErr, outDiag)) return false;
        out = pv.q;
        while (Peek().kind == TokKind::Parallel) {
            Advance();
            ParsedVal rhsPv{};
            std::string rhsSub;
            if (!ParseMulDiv(rhsPv, rhsSub, outErr, outDiag)) return false;
            const QuantityValue& rhs = rhsPv.q;
            if (out.dim != rhs.dim) {
                outErr = "Operador paralelo || requiere dimensiones compatibles";
                outDiag = "DIM_ERR";
                return false;
            }
            const auto sum = out.value + rhs.value;
            if (std::abs(sum) < 1e-300) {
                outErr = "Paralelo indefinido (suma cero)";
                outDiag = "MATH_ERR";
                return false;
            }
            out.value = (out.value * rhs.value) / sum;
            out.unitOffsetToSi = 0.0;
            outSub += " || " + rhsSub;
        }
        return true;
    }

    bool ShouldImplicitMultiplyNext() const {
        const TokKind k = Peek().kind;
        return k == TokKind::Ident || k == TokKind::LParen;
    }

    bool IsUnboundUnitIdent(const std::string& tok) const {
        if (env_.find(tok) != env_.end()) return false;
        UnitDef ud{};
        return LookupBaseUnit(tok, ud);
    }

    static void ApplyUnitMultiply(ParsedVal& outPv, const ParsedVal& rhsPv) {
        QuantityValue& out = outPv.q;
        const QuantityValue& rhs = rhsPv.q;
        const bool lhsDimless = out.dim.IsDimensionless() && out.preferredUnitUtf8.empty();
        const bool rhsDimless = rhs.dim.IsDimensionless() && rhs.preferredUnitUtf8.empty();

        // Bug 3 + FIND-06 fix: apply affine temperature offset ONLY when binding a scalar to a raw temperature unit token
        if (lhsDimless && out.IsReal() && rhsPv.isRawAffine && rhs.unitOffsetToSi != 0.0) {
            out.value = { out.value.real() * rhs.unitScaleToSi + rhs.unitOffsetToSi, 0.0 };
            out.dim = rhs.dim;
            out.preferredUnitUtf8 = rhs.preferredUnitUtf8;
            out.unitScaleToSi = rhs.unitScaleToSi;
            out.unitOffsetToSi = rhs.unitOffsetToSi;
            outPv.isRawAffine = false;
            return;
        }
        if (rhsDimless && rhs.IsReal() && outPv.isRawAffine && out.unitOffsetToSi != 0.0) {
            out.value = { rhs.value.real() * out.unitScaleToSi + out.unitOffsetToSi, 0.0 };
            outPv.isRawAffine = false;
            return;
        }

        outPv.isRawAffine = false;
        out.value *= rhs.value;
        out.dim = out.dim * rhs.dim;
        if (lhsDimless && !rhs.preferredUnitUtf8.empty() && rhs.unitOffsetToSi == 0.0) {
            out.preferredUnitUtf8 = rhs.preferredUnitUtf8;
            out.unitScaleToSi = rhs.unitScaleToSi;
            out.unitOffsetToSi = 0.0;
        } else if (rhsDimless && !out.preferredUnitUtf8.empty() && out.unitOffsetToSi == 0.0) {
            // keep LHS preferred unit
        } else if (!out.preferredUnitUtf8.empty() && !rhs.preferredUnitUtf8.empty() &&
                   out.unitOffsetToSi == 0.0 && rhs.unitOffsetToSi == 0.0) {
            out.preferredUnitUtf8 = out.preferredUnitUtf8 + "*" + rhs.preferredUnitUtf8;
            out.unitScaleToSi *= rhs.unitScaleToSi;
            out.unitOffsetToSi = 0.0;
        } else {
            out.preferredUnitUtf8.clear();
            out.unitScaleToSi = 1.0;
            out.unitOffsetToSi = 0.0;
        }
    }

    bool ParseMulDiv(ParsedVal& outPv, std::string& outSub, std::string& outErr, std::string& outDiag) {
        if (!ParsePower(outPv, outSub, outErr, outDiag)) return false;
        while (true) {
            const TokKind k = Peek().kind;
            if (k == TokKind::Star || k == TokKind::Slash) {
                Advance();
                const bool denomCanBindUnits = (k == TokKind::Slash) &&
                    (Peek().kind == TokKind::Number ||
                     (Peek().kind == TokKind::Ident && Peek(1).kind != TokKind::LParen && IsUnboundUnitIdent(Peek().text)));
                ParsedVal rhsPv{};
                std::string rhsSub;
                if (!ParsePower(rhsPv, rhsSub, outErr, outDiag)) return false;
                if (denomCanBindUnits) {
                    // Bind implicit unit tokens (and hyphenated unit pairs) into the denominator factor,
                    // e.g. "10 m / 2 s" -> "10 m / (2 s)", while keeping explicit "a / b * c" left-associative.
                    while (true) {
                        if (Peek().kind == TokKind::Ident && Peek(1).kind != TokKind::LParen &&
                            IsUnboundUnitIdent(Peek().text)) {
                            ParsedVal unitPv{};
                            std::string unitSub;
                            if (!ParsePower(unitPv, unitSub, outErr, outDiag)) return false;
                            ApplyUnitMultiply(rhsPv, unitPv);
                            rhsSub += " " + unitSub;
                        } else if (Peek().kind == TokKind::Star && Peek().isHyphenUnitSep &&
                                   Peek(1).kind == TokKind::Ident && Peek(2).kind != TokKind::LParen &&
                                   IsUnboundUnitIdent(Peek(1).text)) {
                            Advance();
                            ParsedVal unitPv{};
                            std::string unitSub;
                            if (!ParsePower(unitPv, unitSub, outErr, outDiag)) return false;
                            ApplyUnitMultiply(rhsPv, unitPv);
                            rhsSub += "*" + unitSub;
                        } else {
                            break;
                        }
                    }
                }
                if (k == TokKind::Star) {
                    ApplyUnitMultiply(outPv, rhsPv);
                    outSub += " * " + rhsSub;
                } else {
                    QuantityValue& out = outPv.q;
                    const QuantityValue& rhs = rhsPv.q;
                    if (std::abs(rhs.value) < 1e-18) {
                        outErr = "Divisi\xC3\xB3n entre cero";
                        outDiag = "MATH_ERR";
                        return false;
                    }
                    outPv.isRawAffine = false;
                    out.value /= rhs.value;
                    out.dim = out.dim / rhs.dim;
                    if (!out.preferredUnitUtf8.empty() && !rhs.preferredUnitUtf8.empty() &&
                        out.unitOffsetToSi == 0.0 && rhs.unitOffsetToSi == 0.0 && rhs.unitScaleToSi > 0.0) {
                        if (out.dim.IsDimensionless()) {
                            out.preferredUnitUtf8.clear();
                            out.unitScaleToSi = 1.0;
                        } else {
                            out.preferredUnitUtf8 = out.preferredUnitUtf8 + "/" + rhs.preferredUnitUtf8;
                            out.unitScaleToSi /= rhs.unitScaleToSi;
                        }
                        out.unitOffsetToSi = 0.0;
                    } else if (!out.preferredUnitUtf8.empty() && rhs.dim.IsDimensionless() &&
                               rhs.preferredUnitUtf8.empty() && out.unitOffsetToSi == 0.0) {
                        // Bug 2 fix: keep LHS preferred unit & scale on dimensionless scalar division (e.g. 100 ft / 2 -> 50 ft)
                    } else {
                        out.preferredUnitUtf8.clear();
                        out.unitScaleToSi = 1.0;
                        out.unitOffsetToSi = 0.0;
                    }
                    outSub += " / " + rhsSub;
                }
            } else if (ShouldImplicitMultiplyNext()) {
                ParsedVal rhsPv{};
                std::string rhsSub;
                if (!ParsePower(rhsPv, rhsSub, outErr, outDiag)) return false;
                ApplyUnitMultiply(outPv, rhsPv);
                outSub += " " + rhsSub;
            } else {
                break;
            }
        }
        return true;
    }

    bool ParsePower(ParsedVal& outPv, std::string& outSub, std::string& outErr, std::string& outDiag) {
        if (!ParsePhasor(outPv, outSub, outErr, outDiag)) return false;
        if (Peek().kind == TokKind::Caret) {
            Advance();
            ParsedVal expPv{};
            std::string expSub;
            if (!ParsePower(expPv, expSub, outErr, outDiag)) return false;
            outPv.isRawAffine = false;
            QuantityValue& out = outPv.q;
            const QuantityValue& expVal = expPv.q;
            if (!expVal.dim.IsDimensionless()) {
                outErr = "Exponente debe ser adimensional";
                outDiag = "DIM_ERR";
                return false;
            }
            if (!out.dim.IsDimensionless()) {
                if (!expVal.IsReal() || std::abs(expVal.value.real() - std::round(expVal.value.real())) > 1e-9) {
                    outErr = "Potencia no entera aplicada a magnitud con unidades";
                    outDiag = "DIM_ERR";
                    return false;
                }
                const int p = static_cast<int>(std::llround(expVal.value.real()));
                out.dim = out.dim.Pow(p);
                out.value = std::pow(out.value, p);
                // Bug 1 fix: scale unitScaleToSi & preferredUnit for simple units, otherwise reset scale to 1.0!
                if (p != 0 && IsSimplePreferredUnit(out.preferredUnitUtf8) && out.unitOffsetToSi == 0.0) {
                    out.unitScaleToSi = std::pow(out.unitScaleToSi, static_cast<double>(p));
                    if (p != 1) out.preferredUnitUtf8 += "^" + std::to_string(p);
                } else {
                    out.preferredUnitUtf8.clear();
                    out.unitScaleToSi = 1.0;
                    out.unitOffsetToSi = 0.0;
                }
            } else {
                out.value = std::pow(out.value, expVal.value);
                out.preferredUnitUtf8.clear();
                out.unitScaleToSi = 1.0;
                out.unitOffsetToSi = 0.0;
            }
            outSub += "^" + expSub;
        }
        return true;
    }

    bool ParsePhasor(ParsedVal& outPv, std::string& outSub, std::string& outErr, std::string& outDiag) {
        if (!ParseUnary(outPv, outSub, outErr, outDiag)) return false;
        if (Peek().kind == TokKind::Angle) {
            Advance();
            ParsedVal angPv{};
            std::string angSub;
            if (!ParseUnary(angPv, angSub, outErr, outDiag)) return false;
            outPv.isRawAffine = false;
            const QuantityValue& angVal = angPv.q;
            if (!angVal.dim.IsDimensionless() || !angVal.IsReal()) {
                outErr = "\xC3\x81ngulo del fasor debe ser real adimensional";
                outDiag = "DIM_ERR";
                return false;
            }
            const double mag = outPv.q.value.real();
            const double rad = angVal.value.real() * AngleScaleToRad(opts_.polarAngleMode);
            outPv.q.value = std::polar(mag, rad);
            outPv.q.preferPolar = true;
            outSub += "\xE2\x88\xA0" + angSub + AngleSuffixUtf8(opts_.polarAngleMode);
        }
        return true;
    }

    bool ParseUnary(ParsedVal& outPv, std::string& outSub, std::string& outErr, std::string& outDiag) {
        if (Peek().kind == TokKind::Plus) {
            Advance();
            return ParseUnary(outPv, outSub, outErr, outDiag);
        }
        if (Peek().kind == TokKind::Minus) {
            Advance();
            if (!ParseUnary(outPv, outSub, outErr, outDiag)) return false;
            outPv.isRawAffine = false;
            outPv.q.value = -outPv.q.value;
            if (outPv.q.unitOffsetToSi != 0.0) {
                outPv.q.preferredUnitUtf8.clear();
                outPv.q.unitScaleToSi = 1.0;
                outPv.q.unitOffsetToSi = 0.0;
            }
            outSub = "-" + outSub;
            return true;
        }
        return ParsePrimary(outPv, outSub, outErr, outDiag);
    }

    bool ParsePrimary(ParsedVal& outPv, std::string& outSub, std::string& outErr, std::string& outDiag) {
        outPv.isRawAffine = false;
        QuantityValue& out = outPv.q;
        const Token t = Peek();
        if (t.kind == TokKind::LParen) {
            Advance();
            std::string innerSub;
            if (!ParseAddSub(out, innerSub, outErr, outDiag)) return false;
            if (Peek().kind != TokKind::RParen) {
                outErr = "Falta par\xC3\xA9ntesis de cierre ')'";
                outDiag = "SYNTAX_ERR";
                return false;
            }
            Advance();
            outSub = "(" + innerSub + ")";
            return true;
        }

        if (t.kind == TokKind::Number) {
            Advance();
            out.value = t.imagSuffix ? std::complex<double>(0.0, t.numVal) : std::complex<double>(t.numVal, 0.0);
            out.dim = {};
            out.preferredUnitUtf8.clear();
            out.unitScaleToSi = 1.0;
            out.unitOffsetToSi = 0.0;
            outSub = t.text;
            return true;
        }

        if (t.kind == TokKind::Ident) {
            Advance();
            if (Peek().kind == TokKind::LParen) {
                Advance();
                std::vector<QuantityValue> args;
                std::vector<std::string> argSubs;
                if (Peek().kind != TokKind::RParen) {
                    while (true) {
                        QuantityValue av{};
                        std::string asub;
                        if (!ParseAddSub(av, asub, outErr, outDiag)) return false;
                        args.push_back(av);
                        argSubs.push_back(asub);
                        if (Peek().kind == TokKind::Comma) {
                            Advance();
                            continue;
                        }
                        break;
                    }
                }
                if (Peek().kind != TokKind::RParen) {
                    outErr = "Falta ')' en funci\xC3\xB3n " + t.text;
                    outDiag = "SYNTAX_ERR";
                    return false;
                }
                Advance();

                if (!EvalCasFunction(t.text, args, opts_, out, outErr, outDiag)) return false;
                outSub = t.text + "(";
                for (std::size_t idx = 0; idx < argSubs.size(); ++idx) {
                    if (idx > 0) outSub += ", ";
                    outSub += argSubs[idx];
                }
                outSub += ")";
                return true;
            }

            auto it = env_.find(t.text);
            if (it != env_.end()) {
                out = it->second;
                outSub = "(" + it->second.FormatUtf8(5, opts_.polarAngleMode) + ")";
                return true;
            }

            if (t.text == "pi") { out.value = { kPi, 0.0 }; outSub = "pi"; return true; }
            if (t.text == "e")  { out.value = { std::exp(1.0), 0.0 }; outSub = "e"; return true; }
            if (t.text == "j" || t.text == "i") { out.value = { 0.0, 1.0 }; outSub = t.text; return true; }
            if (t.text == "g0") { out.value = { 9.80665, 0.0 }; out.dim = MakeDim(0, 1, -2); outSub = "9.80665 m/s^2"; return true; }
            if (t.text == "c0") { out.value = { 299792458.0, 0.0 }; out.dim = MakeDim(0, 1, -1); outSub = "c0"; return true; }
            if (t.text == "eps0") { out.value = { 8.8541878128e-12, 0.0 }; out.dim = MakeDim(-1, -3, 4, 2); outSub = "eps0"; return true; }
            if (t.text == "mu0") { out.value = { 1.25663706212e-6, 0.0 }; out.dim = MakeDim(1, 1, -2, -2); outSub = "mu0"; return true; }

            UnitDef ud{};
            if (LookupBaseUnit(t.text, ud)) {
                out.value = { ud.scale, 0.0 };
                out.dim = ud.dim;
                out.preferredUnitUtf8 = t.text;
                out.unitScaleToSi = ud.scale;
                out.unitOffsetToSi = ud.offset;
                outPv.isRawAffine = (ud.offset != 0.0);
                outSub = t.text;
                return true;
            }

            outErr = "S\xC3\xADmbolo o unidad desconocida: " + t.text;
            outDiag = "SYNTAX_ERR";
            return false;
        }

        outErr = "Se esperaba valor, variable o par\xC3\xA9ntesis cerca de: " + t.text;
        outDiag = "SYNTAX_ERR";
        return false;
    }
};

std::string InferNiceUnitLabel(const UnitDim& d) {
    if (d == MakeDim(1, 2, -3, -1)) return "V";
    if (d == MakeDim(0, 0, 0, 1)) return "A";
    if (d == MakeDim(1, 2, -3, -2)) return "\xCE\xA9";
    if (d == MakeDim(1, 2, -3)) return "W";
    if (d == MakeDim(1, 2, -2)) return "J";
    if (d == MakeDim(1, 1, -2)) return "N";
    if (d == MakeDim(1, -1, -2)) return "Pa";
    if (d == MakeDim(0, 0, -1)) return "Hz";
    if (d == MakeDim(-1, -2, 4, 2)) return "F";
    if (d == MakeDim(1, 2, -2, -2)) return "H";
    return d.FormatSiString();
}

bool IsRawAffineUnitAstNode(const MathJsonNode& n, const SymbolEnv& env) {
    if (n.kind != MathJsonKind::Symbol) return false;
    if (env.find(n.text) != env.end()) return false;
    UnitDef ud{};
    return LookupBaseUnit(n.text, ud) && ud.offset != 0.0;
}

bool EvalAstNodeRec(
    const MathJsonNode& n,
    const SymbolEnv& env,
    const CasEvalOptions& opts,
    QuantityValue& out,
    std::string& outErr,
    std::string& outDiag)
{
    if (n.kind == MathJsonKind::Number) {
        out = QuantityValue{ { n.realVal, 0.0 }, {}, "", 1.0, 0.0, false };
        return true;
    }
    if (n.kind == MathJsonKind::Complex) {
        out = QuantityValue{ { n.realVal, n.imagVal }, {}, "", 1.0, 0.0, false };
        return true;
    }
    if (n.kind == MathJsonKind::Symbol) {
        auto it = env.find(n.text);
        if (it != env.end()) { out = it->second; return true; }
        if (n.text == "pi") { out = QuantityValue{ { kPi, 0.0 }, {}, "", 1.0, 0.0, false }; return true; }
        if (n.text == "e")  { out = QuantityValue{ { std::exp(1.0), 0.0 }, {}, "", 1.0, 0.0, false }; return true; }
        if (n.text == "j" || n.text == "i") { out = QuantityValue{ { 0.0, 1.0 }, {}, "", 1.0, 0.0, false }; return true; }
        if (n.text == "g0") { out = QuantityValue{ { 9.80665, 0.0 }, MakeDim(0, 1, -2), "", 1.0, 0.0, false }; return true; }
        if (n.text == "c0") { out = QuantityValue{ { 299792458.0, 0.0 }, MakeDim(0, 1, -1), "", 1.0, 0.0, false }; return true; }
        if (n.text == "eps0") { out = QuantityValue{ { 8.8541878128e-12, 0.0 }, MakeDim(-1, -3, 4, 2), "", 1.0, 0.0, false }; return true; }
        if (n.text == "mu0") { out = QuantityValue{ { 1.25663706212e-6, 0.0 }, MakeDim(1, 1, -2, -2), "", 1.0, 0.0, false }; return true; }
        UnitDef ud{};
        if (LookupBaseUnit(n.text, ud)) {
            out = QuantityValue{ { ud.scale, 0.0 }, ud.dim, n.text, ud.scale, ud.offset, false };
            return true;
        }
        outErr = "Simbolo desconocido en AST: " + n.text;
        outDiag = "SYNTAX_ERR";
        return false;
    }
    if (n.kind != MathJsonKind::Function) {
        outErr = "Nodo String inesperado en evaluacion numerica";
        outDiag = "SYNTAX_ERR";
        return false;
    }

    const std::string& h = n.text;
    if (h == "HyphenChoice" && n.args.size() >= 3) {
        bool anyBound = false;
        for (std::size_t idx = 2; idx < n.args.size(); ++idx) {
            if (n.args[idx].kind == MathJsonKind::Symbol && env.find(n.args[idx].text) != env.end()) {
                anyBound = true;
                break;
            }
        }
        return EvalAstNodeRec(anyBound ? n.args[1] : n.args[0], env, opts, out, outErr, outDiag);
    }
    if (h == "EngSuffix" && n.args.size() == 2 && n.args[1].kind == MathJsonKind::Symbol) {
        QuantityValue baseNum{};
        if (!EvalAstNodeRec(n.args[0], env, opts, baseNum, outErr, outDiag)) return false;
        const std::string& suf = n.args[1].text;
        auto it = env.find(suf);
        if (it != env.end()) {
            out = it->second;
            out.value *= baseNum.value;
            if (out.unitOffsetToSi != 0.0) {
                out.preferredUnitUtf8.clear();
                out.unitScaleToSi = 1.0;
                out.unitOffsetToSi = 0.0;
            }
            return true;
        }
        double mul = 1.0;
        if (suf == "p") mul = 1e-12;
        else if (suf == "n") mul = 1e-9;
        else if (suf == "u") mul = 1e-6;
        else if (suf == "k") mul = 1e3;
        else if (suf == "M") mul = 1e6;
        else if (suf == "G") mul = 1e9;
        else {
            outErr = "Sufijo desconocido en AST: " + suf;
            outDiag = "SYNTAX_ERR";
            return false;
        }
        out = baseNum;
        out.value *= mul;
        return true;
    }
    if (h == "Equal" && n.args.size() == 2) {
        QuantityValue l{}, r{};
        if (!EvalAstNodeRec(n.args[0], env, opts, l, outErr, outDiag) ||
            !EvalAstNodeRec(n.args[1], env, opts, r, outErr, outDiag)) return false;
        if (l.dim != r.dim) { outErr = "Desajuste dimensional en igualdad"; outDiag = "DIM_ERR"; return false; }
        out = l;
        out.value = l.value - r.value;
        return true;
    }
    if (h == "ConvertUnit" && n.args.size() == 2) {
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag)) return false;
        UnitDim td{}; double sc = 1.0, off = 0.0;
        if (n.args[1].kind != MathJsonKind::String || !TryParseUnitExpression(n.args[1].text, td, sc, off, &outErr)) {
            outDiag = "SYNTAX_ERR";
            return false;
        }
        if (out.dim != td) { outErr = "Conversion dimensional incompatible"; outDiag = "DIM_ERR"; return false; }
        out.preferredUnitUtf8 = n.args[1].text;
        out.unitScaleToSi = sc;
        out.unitOffsetToSi = off;
        return true;
    }
    if (h == "UnitQuantity" && n.args.size() == 2) {
        QuantityValue mag{};
        if (!EvalAstNodeRec(n.args[0], env, opts, mag, outErr, outDiag)) return false;
        if (n.args[1].kind != MathJsonKind::String && n.args[1].kind != MathJsonKind::Symbol) {
            outErr = "Unidad invalida en UnitQuantity";
            outDiag = "SYNTAX_ERR";
            return false;
        }
        UnitDim ud{};
        double sc = 1.0, off = 0.0;
        if (!TryParseUnitExpression(n.args[1].text, ud, sc, off, &outErr)) {
            outDiag = "SYNTAX_ERR";
            return false;
        }
        if (!mag.dim.IsDimensionless()) {
            out = mag;
            out.value *= sc;
            out.dim = mag.dim * ud;
            out.preferredUnitUtf8.clear();
            out.unitScaleToSi = 1.0;
            out.unitOffsetToSi = 0.0;
            return true;
        }
        if (off != 0.0 && mag.IsReal()) {
            out = QuantityValue{ { mag.value.real() * sc + off, 0.0 }, ud, n.args[1].text, sc, off, mag.preferPolar };
        } else {
            out = QuantityValue{ mag.value * sc, ud, n.args[1].text, sc, 0.0, mag.preferPolar };
        }
        return true;
    }
    if (h == "Negate" && n.args.size() == 1) {
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag)) return false;
        out.value = -out.value;
        if (out.unitOffsetToSi != 0.0) {
            out.preferredUnitUtf8.clear();
            out.unitScaleToSi = 1.0;
            out.unitOffsetToSi = 0.0;
        }
        return true;
    }
    if (h == "Add" && !n.args.empty()) {
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag)) return false;
        for (std::size_t i = 1; i < n.args.size(); ++i) {
            QuantityValue rhs{};
            if (!EvalAstNodeRec(n.args[i], env, opts, rhs, outErr, outDiag)) return false;
            if (out.dim != rhs.dim) { outErr = "Error dimensional en suma"; outDiag = "DIM_ERR"; return false; }
            out.value += rhs.value;
            if (out.unitOffsetToSi != 0.0 || rhs.unitOffsetToSi != 0.0) {
                out.preferredUnitUtf8.clear();
                out.unitScaleToSi = 1.0;
                out.unitOffsetToSi = 0.0;
            } else if (out.preferredUnitUtf8.empty() && !rhs.preferredUnitUtf8.empty()) {
                out.preferredUnitUtf8 = rhs.preferredUnitUtf8;
                out.unitScaleToSi = rhs.unitScaleToSi;
                out.unitOffsetToSi = rhs.unitOffsetToSi;
            }
        }
        return true;
    }
    if (h == "Subtract" && n.args.size() == 2) {
        QuantityValue rhs{};
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag) ||
            !EvalAstNodeRec(n.args[1], env, opts, rhs, outErr, outDiag)) return false;
        if (out.dim != rhs.dim) { outErr = "Error dimensional en resta"; outDiag = "DIM_ERR"; return false; }
        out.value -= rhs.value;
        if (out.unitOffsetToSi != 0.0 || rhs.unitOffsetToSi != 0.0) {
            out.preferredUnitUtf8.clear();
            out.unitScaleToSi = 1.0;
            out.unitOffsetToSi = 0.0;
        }
        return true;
    }
    if (h == "Parallel" && n.args.size() == 2) {
        QuantityValue rhs{};
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag) ||
            !EvalAstNodeRec(n.args[1], env, opts, rhs, outErr, outDiag)) return false;
        if (out.dim != rhs.dim) { outErr = "Paralelo || requiere dimensiones compatibles"; outDiag = "DIM_ERR"; return false; }
        const auto sum = out.value + rhs.value;
        if (std::abs(sum) < 1e-300) { outErr = "Paralelo indefinido"; outDiag = "MATH_ERR"; return false; }
        out.value = (out.value * rhs.value) / sum;
        out.unitOffsetToSi = 0.0;
        return true;
    }
    if (h == "Multiply" && !n.args.empty()) {
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag)) return false;
        bool lhsIsRawAffine = IsRawAffineUnitAstNode(n.args[0], env);
        for (std::size_t i = 1; i < n.args.size(); ++i) {
            QuantityValue rhs{};
            if (!EvalAstNodeRec(n.args[i], env, opts, rhs, outErr, outDiag)) return false;
            const bool rhsIsRawAffine = IsRawAffineUnitAstNode(n.args[i], env);
            const bool lhsDimless = out.dim.IsDimensionless() && out.preferredUnitUtf8.empty();
            const bool rhsDimless = rhs.dim.IsDimensionless() && rhs.preferredUnitUtf8.empty();
            if (lhsDimless && out.IsReal() && rhsIsRawAffine && rhs.unitOffsetToSi != 0.0) {
                out.value = { out.value.real() * rhs.unitScaleToSi + rhs.unitOffsetToSi, 0.0 };
                out.dim = rhs.dim;
                out.preferredUnitUtf8 = rhs.preferredUnitUtf8;
                out.unitScaleToSi = rhs.unitScaleToSi;
                out.unitOffsetToSi = rhs.unitOffsetToSi;
                lhsIsRawAffine = false;
                continue;
            }
            if (rhsDimless && rhs.IsReal() && lhsIsRawAffine && out.unitOffsetToSi != 0.0) {
                out.value = { rhs.value.real() * out.unitScaleToSi + out.unitOffsetToSi, 0.0 };
                lhsIsRawAffine = false;
                continue;
            }
            lhsIsRawAffine = false;
            out.value *= rhs.value;
            out.dim = out.dim * rhs.dim;
            if (lhsDimless && !rhs.preferredUnitUtf8.empty() && rhs.unitOffsetToSi == 0.0) {
                out.preferredUnitUtf8 = rhs.preferredUnitUtf8;
                out.unitScaleToSi = rhs.unitScaleToSi;
                out.unitOffsetToSi = 0.0;
            } else if (rhsDimless && !out.preferredUnitUtf8.empty() && out.unitOffsetToSi == 0.0) {
                // keep LHS unit
            } else if (!out.preferredUnitUtf8.empty() && !rhs.preferredUnitUtf8.empty() &&
                       out.unitOffsetToSi == 0.0 && rhs.unitOffsetToSi == 0.0) {
                out.preferredUnitUtf8 += "*" + rhs.preferredUnitUtf8;
                out.unitScaleToSi *= rhs.unitScaleToSi;
                out.unitOffsetToSi = 0.0;
            } else {
                out.preferredUnitUtf8.clear();
                out.unitScaleToSi = 1.0;
                out.unitOffsetToSi = 0.0;
            }
        }
        return true;
    }
    if (h == "Divide" && n.args.size() == 2) {
        QuantityValue rhs{};
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag) ||
            !EvalAstNodeRec(n.args[1], env, opts, rhs, outErr, outDiag)) return false;
        if (std::abs(rhs.value) < 1e-18) { outErr = "Division entre cero"; outDiag = "MATH_ERR"; return false; }
        out.value /= rhs.value;
        out.dim = out.dim / rhs.dim;
        if (!out.preferredUnitUtf8.empty() && !rhs.preferredUnitUtf8.empty() &&
            out.unitOffsetToSi == 0.0 && rhs.unitOffsetToSi == 0.0 && rhs.unitScaleToSi > 0.0) {
            if (out.dim.IsDimensionless()) { out.preferredUnitUtf8.clear(); out.unitScaleToSi = 1.0; }
            else { out.preferredUnitUtf8 += "/" + rhs.preferredUnitUtf8; out.unitScaleToSi /= rhs.unitScaleToSi; }
            out.unitOffsetToSi = 0.0;
        } else if (!out.preferredUnitUtf8.empty() && rhs.dim.IsDimensionless() &&
                   rhs.preferredUnitUtf8.empty() && out.unitOffsetToSi == 0.0) {
            // preserve LHS unit
        } else {
            out.preferredUnitUtf8.clear();
            out.unitScaleToSi = 1.0;
            out.unitOffsetToSi = 0.0;
        }
        return true;
    }
    if (h == "Power" && n.args.size() == 2) {
        if (n.args[0].kind == MathJsonKind::Function && n.args[0].text == "EngSuffix" &&
            n.args[0].args.size() == 2 && n.args[0].args[1].kind == MathJsonKind::Symbol &&
            env.find(n.args[0].args[1].text) != env.end()) {
            const MathJsonNode rewritten = MathJsonNode::MakeFunction("Multiply", {
                n.args[0].args[0],
                MathJsonNode::MakeFunction("Power", { n.args[0].args[1], n.args[1] })
            });
            return EvalAstNodeRec(rewritten, env, opts, out, outErr, outDiag);
        }
        QuantityValue expVal{};
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag) ||
            !EvalAstNodeRec(n.args[1], env, opts, expVal, outErr, outDiag)) return false;
        if (!expVal.dim.IsDimensionless()) { outErr = "Exponente debe ser adimensional"; outDiag = "DIM_ERR"; return false; }
        if (!out.dim.IsDimensionless()) {
            if (!expVal.IsReal() || std::abs(expVal.value.real() - std::round(expVal.value.real())) > 1e-9) {
                outErr = "Potencia no entera aplicada a magnitud con unidades"; outDiag = "DIM_ERR"; return false;
            }
            const int p = static_cast<int>(std::llround(expVal.value.real()));
            out.dim = out.dim.Pow(p);
            out.value = std::pow(out.value, p);
            if (p != 0 && IsSimplePreferredUnit(out.preferredUnitUtf8) && out.unitOffsetToSi == 0.0) {
                out.unitScaleToSi = std::pow(out.unitScaleToSi, static_cast<double>(p));
                if (p != 1) out.preferredUnitUtf8 += "^" + std::to_string(p);
            } else {
                out.preferredUnitUtf8.clear();
                out.unitScaleToSi = 1.0;
                out.unitOffsetToSi = 0.0;
            }
        } else {
            out.value = std::pow(out.value, expVal.value);
            out.preferredUnitUtf8.clear();
            out.unitScaleToSi = 1.0;
            out.unitOffsetToSi = 0.0;
        }
        return true;
    }
    if (h == "Phasor" && n.args.size() == 2) {
        QuantityValue angVal{};
        if (!EvalAstNodeRec(n.args[0], env, opts, out, outErr, outDiag) ||
            !EvalAstNodeRec(n.args[1], env, opts, angVal, outErr, outDiag)) return false;
        if (!angVal.dim.IsDimensionless() || !angVal.IsReal()) {
            outErr = "Angulo del fasor debe ser real adimensional"; outDiag = "DIM_ERR"; return false;
        }
        out.value = std::polar(out.value.real(), angVal.value.real() * AngleScaleToRad(opts.polarAngleMode));
        out.preferPolar = true;
        return true;
    }

    // Function call mapped via HeadToCasFn-style lowercase
    std::string fnLower = h;
    if (!fnLower.empty()) fnLower[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(fnLower[0])));
    std::vector<QuantityValue> evalArgs;
    evalArgs.reserve(n.args.size());
    for (const auto& a : n.args) {
        QuantityValue q{};
        if (!EvalAstNodeRec(a, env, opts, q, outErr, outDiag)) return false;
        evalArgs.push_back(std::move(q));
    }
    return EvalCasFunction(fnLower, evalArgs, opts, out, outErr, outDiag);
}

} // namespace

bool UnitDim::IsDimensionless() const noexcept {
    for (int e : exp) if (e != 0) return false;
    return true;
}

UnitDim UnitDim::operator*(const UnitDim& o) const noexcept {
    UnitDim r{};
    for (std::size_t i = 0; i < 7; ++i) r.exp[i] = exp[i] + o.exp[i];
    return r;
}

UnitDim UnitDim::operator/(const UnitDim& o) const noexcept {
    UnitDim r{};
    for (std::size_t i = 0; i < 7; ++i) r.exp[i] = exp[i] - o.exp[i];
    return r;
}

UnitDim UnitDim::Pow(int p) const noexcept {
    UnitDim r{};
    for (std::size_t i = 0; i < 7; ++i) r.exp[i] = exp[i] * p;
    return r;
}

std::string UnitDim::FormatSiString() const {
    static constexpr const char* kNames[7] = { "kg", "m", "s", "A", "K", "mol", "cd" };
    std::string pos, neg;
    for (std::size_t i = 0; i < 7; ++i) {
        if (exp[i] > 0) {
            if (!pos.empty()) pos += "\xC2\xB7";
            pos += kNames[i];
            if (exp[i] > 1) pos += "^" + std::to_string(exp[i]);
        } else if (exp[i] < 0) {
            if (!neg.empty()) neg += "\xC2\xB7";
            neg += kNames[i];
            if (-exp[i] > 1) neg += "^" + std::to_string(-exp[i]);
        }
    }
    if (pos.empty() && neg.empty()) return "";
    if (neg.empty()) return pos;
    if (pos.empty()) return "1/" + neg;
    return pos + "/" + neg;
}

double QuantityValue::PhaseDeg() const noexcept {
    return std::arg(value) * (180.0 / kPi);
}

double QuantityValue::PhaseInMode(CasAngleMode mode) const noexcept {
    return std::arg(value) * AngleScaleFromRad(mode);
}

std::string QuantityValue::FormatUtf8(int sigDigits) const {
    return FormatUtf8(sigDigits, CasAngleMode::Deg);
}

std::string QuantityValue::FormatUtf8(int sigDigits, CasAngleMode angleMode) const {
    const double scale = (unitScaleToSi != 0.0) ? unitScaleToSi : 1.0;
    const std::complex<double> scaled = (value - std::complex<double>(unitOffsetToSi, 0.0)) / scale;

    std::string numStr;
    if (std::abs(scaled.imag()) <= 1e-10) {
        numStr = FormatRealClean(scaled.real(), sigDigits);
    } else if (preferPolar) {
        const double m = std::abs(scaled);
        const double ang = std::arg(scaled) * AngleScaleFromRad(angleMode);
        numStr = FormatRealClean(m, sigDigits) + " \xE2\x88\xA0 " +
                 FormatRealClean(ang, sigDigits) + AngleSuffixUtf8(angleMode);
    } else {
        const double re = scaled.real();
        const double im = scaled.imag();
        if (std::abs(re) <= 1e-10) {
            numStr = FormatRealClean(im, sigDigits) + "j";
        } else {
            numStr = FormatRealClean(re, sigDigits) + (im >= 0.0 ? " + " : " - ") +
                     FormatRealClean(std::abs(im), sigDigits) + "j";
        }
    }

    const std::string unitLbl = !preferredUnitUtf8.empty() ? preferredUnitUtf8 : InferNiceUnitLabel(dim);
    return unitLbl.empty() ? numStr : (numStr + " " + unitLbl);
}

bool TryParseUnitExpression(
    const std::string& unitExprUtf8,
    UnitDim& outDim,
    double& outScaleToSi,
    double& outOffsetToSi,
    std::string* outErr)
{
    const std::string trimmed = TrimUtf8(unitExprUtf8);
    if (trimmed.empty()) {
        if (outErr) *outErr = "Unidad destino vac\xC3\xAD""a";
        return false;
    }

    UnitDef single{};
    if (TryParseUnitTokenWithPower(trimmed, single)) {
        outDim = single.dim;
        outScaleToSi = single.scale;
        outOffsetToSi = single.offset;
        return true;
    }

    // Bug 4 fix: support '*', '/', AND '-' (e.g. ft-kip, kip-ft, N-m, lbf-ft) when '-' does not follow '^'
    outDim = {};
    outScaleToSi = 1.0;
    outOffsetToSi = 0.0;

    bool dividing = false;
    std::size_t pos = 0;
    while (pos < trimmed.size()) {
        std::size_t sep = pos;
        while (sep < trimmed.size()) {
            const char ch = trimmed[sep];
            if (ch == '*' || ch == '/') break;
            if (ch == '-' && sep > pos && trimmed[sep - 1] != '^') break;
            ++sep;
        }
        const std::string part = TrimUtf8(trimmed.substr(pos, sep - pos));
        if (part.empty()) {
            if (outErr) *outErr = "Expresi\xC3\xB3n de unidad inv\xC3\xA1lida: " + trimmed;
            return false;
        }
        UnitDef partDef{};
        if (!TryParseUnitTokenWithPower(part, partDef) || partDef.offset != 0.0) {
            if (outErr) *outErr = "Unidad desconocida: " + part;
            return false;
        }
        if (dividing) {
            outDim = outDim / partDef.dim;
            outScaleToSi /= partDef.scale;
        } else {
            outDim = outDim * partDef.dim;
            outScaleToSi *= partDef.scale;
        }
        if (sep < trimmed.size()) {
            dividing = (trimmed[sep] == '/');
            pos = sep + 1;
        } else {
            break;
        }
    }
    return true;
}

CasEvalResult EvaluateExpression(const std::string& exprUtf8, const SymbolEnv& env) {
    return EvaluateExpression(exprUtf8, env, CasEvalOptions{});
}

CasEvalResult EvaluateExpression(const std::string& exprUtf8, const SymbolEnv& env, const CasEvalOptions& opts) {
    if (exprUtf8.find('[') != std::string::npos) {
        const auto mres = EvaluateComplexMatrixExpression(exprUtf8);
        if (mres.ok) {
            CasEvalResult r;
            r.ok = true;
            r.formattedResultUtf8 = mres.formattedUtf8;
            r.result.value = mres.isScalar ? mres.scalar
                : (mres.matrix.data.empty() ? std::complex<double>{0.0, 0.0} : mres.matrix.data.front());
            r.diagnosticCode = "OK";
            return r;
        }
    }
    ExprParser parser(exprUtf8, env, opts);
    return parser.ParseAndEval();
}

bool EvaluateAst(
    const MathJsonNode& ast,
    const SymbolEnv& env,
    CasEvalResult& out,
    const CasEvalOptions& opts)
{
    out = {};
    QuantityValue q{};
    std::string err, diag = "OK";
    if (!EvalAstNodeRec(ast, env, opts, q, err, diag)) {
        out.ok = false;
        out.errorUtf8 = err;
        out.diagnosticCode = diag;
        return false;
    }
    if (!std::isfinite(q.value.real()) || !std::isfinite(q.value.imag())) {
        out.ok = false;
        out.errorUtf8 = "Resultado no finito";
        out.diagnosticCode = "MATH_ERR";
        return false;
    }
    if (opts.forcePolarOutput) q.preferPolar = true;
    else if (opts.forceRectOutput) q.preferPolar = false;
    out.ok = true;
    out.diagnosticCode = "OK";
    out.result = q;
    out.formattedResultUtf8 = q.FormatUtf8(6, opts.polarAngleMode);
    return true;
}

// ---------------- Group 2: Table DAG & Margin Side-Calc Hooks ----------------

namespace {

bool ParseCellCoord(const std::string& ref, int& outCol, int& outRow) {
    std::string r = TrimUtf8(ref);
    if (r.empty()) return false;
    // A1..Z99 or AA1..
    std::size_t i = 0;
    while (i < r.size() && std::isalpha(static_cast<unsigned char>(r[i]))) ++i;
    if (i == 0 || i >= r.size()) return false;
    const std::string colStr = r.substr(0, i);
    const std::string rowStr = r.substr(i);
    for (char c : rowStr) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    int col = 0;
    for (char c : colStr) {
        const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (up < 'A' || up > 'Z') return false;
        col = col * 26 + (up - 'A' + 1);
    }
    try {
        outCol = col - 1;
        outRow = std::stoi(rowStr) - 1;
        return outCol >= 0 && outRow >= 0;
    } catch (...) {
        return false;
    }
}

std::string FormatCellCoord(int col, int row) {
    std::string colStr;
    int c = col + 1;
    while (c > 0) {
        --c;
        colStr.insert(colStr.begin(), static_cast<char>('A' + (c % 26)));
        c /= 26;
    }
    return colStr + std::to_string(row + 1);
}

std::string ExpandTableRangeFunctions(
    const std::string& expr,
    const std::unordered_map<std::string, QuantityValue>& cellVals,
    bool& outRangeErr,
    std::string& outErr)
{
    outRangeErr = false;
    std::string s = expr;
    static const char* kAggs[] = { "SUM", "AVG", "MEAN", "MIN", "MAX", "COUNT", "PROD", "STDEV" };

    for (const char* fn : kAggs) {
        const std::string pat = std::string(fn) + "(";
        std::size_t pos = 0;
        while ((pos = s.find(pat, pos)) != std::string::npos) {
            // Ensure word boundary before fn
            if (pos > 0 && (std::isalnum(static_cast<unsigned char>(s[pos - 1])) || s[pos - 1] == '_')) {
                pos += pat.size();
                continue;
            }
            const auto close = s.find(')', pos + pat.size());
            if (close == std::string::npos) break;
            const std::string inner = s.substr(pos + pat.size(), close - (pos + pat.size()));
            const auto colon = inner.find(':');
            if (colon == std::string::npos) {
                pos = close + 1;
                continue;
            }
            int c0 = 0, r0 = 0, c1 = 0, r1 = 0;
            if (!ParseCellCoord(inner.substr(0, colon), c0, r0) ||
                !ParseCellCoord(inner.substr(colon + 1), c1, r1)) {
                outRangeErr = true;
                outErr = "Rango invalido en tabla: " + inner;
                return s;
            }
            if (c0 > c1) std::swap(c0, c1);
            if (r0 > r1) std::swap(r0, r1);

            std::vector<QuantityValue> vals;
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    const auto it = cellVals.find(FormatCellCoord(c, r));
                    if (it != cellVals.end()) vals.push_back(it->second);
                }
            }
            if (vals.empty()) {
                outRangeErr = true;
                outErr = "Rango vacio en " + std::string(fn) + "(" + inner + ")";
                return s;
            }
            const std::string fnStr(fn);
            std::string rep;
            if (fnStr == "COUNT") {
                rep = std::to_string(vals.size());
            } else {
                const UnitDim refDim = vals[0].dim;
                for (std::size_t k = 1; k < vals.size(); ++k) {
                    if (vals[k].dim != refDim && fnStr != "PROD") {
                        outRangeErr = true;
                        outErr = "Dimensiones incompatibles en rango " + inner;
                        return s;
                    }
                }
                double acc = (fnStr == "PROD") ? 1.0 : vals[0].value.real();
                if (fnStr == "SUM" || fnStr == "AVG" || fnStr == "MEAN" || fnStr == "STDEV") {
                    acc = 0.0;
                    for (const auto& q : vals) acc += q.value.real();
                    if (fnStr == "AVG" || fnStr == "MEAN") acc /= static_cast<double>(vals.size());
                    else if (fnStr == "STDEV") {
                        const double mean = acc / static_cast<double>(vals.size());
                        double var = 0.0;
                        for (const auto& q : vals) { const double d = q.value.real() - mean; var += d * d; }
                        acc = (vals.size() > 1) ? std::sqrt(var / static_cast<double>(vals.size() - 1)) : 0.0;
                    }
                } else if (fnStr == "MIN") {
                    for (const auto& q : vals) acc = (std::min)(acc, q.value.real());
                } else if (fnStr == "MAX") {
                    for (const auto& q : vals) acc = (std::max)(acc, q.value.real());
                } else if (fnStr == "PROD") {
                    for (const auto& q : vals) acc *= q.value.real();
                }
                QuantityValue rq{};
                rq.value = { acc, 0.0 };
                rq.dim = (fnStr == "PROD") ? UnitDim{} : refDim;
                if (fnStr != "PROD" && !vals[0].preferredUnitUtf8.empty() && vals[0].unitOffsetToSi == 0.0) {
                    rq.preferredUnitUtf8 = vals[0].preferredUnitUtf8;
                    rq.unitScaleToSi = vals[0].unitScaleToSi;
                }
                rep = "(" + rq.FormatUtf8(10) + ")";
            }
            s.replace(pos, close - pos + 1, rep);
            pos += rep.size();
        }
    }
    return s;
}

bool HasTagInsensitive(const AnnotationObject& obj, const char* tag) {
    auto lowerStr = [](std::string x) {
        for (char& c : x) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return x;
    };
    const std::string target = lowerStr(tag);
    for (const auto& t : obj.tags) {
        std::string clean = lowerStr(t);
        if (!clean.empty() && clean[0] == '#') clean.erase(clean.begin());
        if (clean == target) return true;
    }
    return false;
}

TableSheetModel ParseTableFromObject(const AnnotationObject& obj) {
    TableSheetModel sheet;
    sheet.tableNameUtf8 = obj.captionUtf8.empty() ? "Tabla" : obj.captionUtf8;
    if (!obj.plotConfigUtf8.empty()) {
        try {
            const json cfg = json::parse(obj.plotConfigUtf8);
            if (cfg.is_object() && cfg.contains("cells") && cfg["cells"].is_array()) {
                for (const auto& cj : cfg["cells"]) {
                    sheet.cells.push_back({
                        cj.value("ref", ""), cj.value("expr", ""), cj.value("exportVar", "")
                    });
                }
                return sheet;
            }
        } catch (...) {}
    }
    // Parse semicolon/newline rows like "A1=10 m; B1=2 s; C1==A1/B1 | m/s"
    std::string norm = obj.formulaExprUtf8;
    for (char& c : norm) if (c == ';') c = '\n';
    std::istringstream iss(norm);
    std::string line;
    int autoRow = 0;
    while (std::getline(iss, line)) {
        line = TrimUtf8(line);
        if (line.empty()) continue;
        // Support "A1=expr" or tab/pipe-separated cells "10 m | 2 s | =A1/B1"
        const auto eq = line.find('=');
        int colCheck = 0, rowCheck = 0;
        if (eq != std::string::npos && ParseCellCoord(line.substr(0, eq), colCheck, rowCheck)) {
            sheet.cells.push_back({ TrimUtf8(line.substr(0, eq)), TrimUtf8(line.substr(eq + 1)), "" });
        } else {
            // Split on '\t' or ','
            std::istringstream riss(line);
            std::string cell;
            int col = 0;
            const char delim = (line.find('\t') != std::string::npos) ? '\t' : ',';
            while (std::getline(riss, cell, delim)) {
                cell = TrimUtf8(cell);
                if (!cell.empty()) {
                    sheet.cells.push_back({ FormatCellCoord(col, autoRow), cell, "" });
                }
                ++col;
            }
            ++autoRow;
        }
    }
    return sheet;
}

} // namespace

TableEvalReport EvaluateTableDag(const TableSheetModel& sheet, SymbolEnv& ioEnv) {
    TableEvalReport rep;
    const std::size_t n = sheet.cells.size();
    if (n == 0) return rep;

    std::unordered_map<std::string, std::size_t> refToIdx;
    for (std::size_t i = 0; i < n; ++i) {
        refToIdx[sheet.cells[i].cellRef] = i;
    }

    auto extractRefs = [&](const std::string& raw) {
        std::unordered_set<std::string> refs;
        std::size_t i = 0;
        while (i < raw.size()) {
            if (std::isalpha(static_cast<unsigned char>(raw[i]))) {
                const std::size_t st = i;
                while (i < raw.size() && std::isalnum(static_cast<unsigned char>(raw[i]))) ++i;
                const std::string tok = raw.substr(st, i - st);
                // Range A1:B5?
                if (i < raw.size() && raw[i] == ':' && i + 1 < raw.size() && std::isalpha(static_cast<unsigned char>(raw[i + 1]))) {
                    const std::size_t st2 = i + 1;
                    std::size_t j = st2;
                    while (j < raw.size() && std::isalnum(static_cast<unsigned char>(raw[j]))) ++j;
                    int c0 = 0, r0 = 0, c1 = 0, r1 = 0;
                    if (ParseCellCoord(tok, c0, r0) && ParseCellCoord(raw.substr(st2, j - st2), c1, r1)) {
                        if (c0 > c1) std::swap(c0, c1);
                        if (r0 > r1) std::swap(r0, r1);
                        for (int r = r0; r <= r1; ++r)
                            for (int c = c0; c <= c1; ++c)
                                refs.insert(FormatCellCoord(c, r));
                        i = j;
                        continue;
                    }
                }
                refs.insert(tok);
            } else {
                ++i;
            }
        }
        return refs;
    };

    std::vector<std::vector<std::size_t>> adj(n);
    std::vector<int> indeg(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        std::string expr = TrimUtf8(sheet.cells[i].rawExprUtf8);
        if (!expr.empty() && expr[0] == '=') expr = expr.substr(1);
        for (const auto& r : extractRefs(expr)) {
            auto it = refToIdx.find(r);
            if (it != refToIdx.end() && it->second != i) {
                adj[it->second].push_back(i);
                ++indeg[i];
            } else if (it != refToIdx.end() && it->second == i) {
                // self-cycle
                ++indeg[i];
            }
        }
    }

    std::vector<std::size_t> order;
    std::vector<std::size_t> q;
    for (std::size_t i = 0; i < n; ++i) if (indeg[i] == 0) q.push_back(i);
    std::size_t head = 0;
    while (head < q.size()) {
        const std::size_t u = q[head++];
        order.push_back(u);
        for (std::size_t v : adj[u]) {
            if (--indeg[v] == 0) q.push_back(v);
        }
    }
    if (order.size() < n) {
        rep.ok = false;
        rep.cycleCount = static_cast<int>(n - order.size());
        rep.errors.push_back("Ciclo de dependencias en celdas de tabla.");
    }

    rep.results.resize(n);
    for (std::size_t i = 0; i < n; ++i) rep.results[i].cellRef = sheet.cells[i].cellRef;

    SymbolEnv tableEnv = ioEnv;
    std::unordered_map<std::string, QuantityValue> cellVals;

    for (std::size_t idx : order) {
        const auto& cell = sheet.cells[idx];
        std::string expr = TrimUtf8(cell.rawExprUtf8);
        if (!expr.empty() && expr[0] == '=') expr = TrimUtf8(expr.substr(1));

        bool rangeErr = false;
        std::string rErr;
        const std::string expanded = ExpandTableRangeFunctions(expr, cellVals, rangeErr, rErr);
        if (rangeErr) {
            rep.ok = false;
            ++rep.errorCount;
            rep.results[idx].ok = false;
            rep.results[idx].errorUtf8 = rErr;
            rep.errors.push_back(rErr);
            continue;
        }
        const CasEvalResult er = EvaluateExpression(expanded, tableEnv);
        if (er.ok) {
            rep.results[idx].ok = true;
            rep.results[idx].value = er.result;
            rep.results[idx].formattedUtf8 = er.formattedResultUtf8;
            cellVals[cell.cellRef] = er.result;
            tableEnv[cell.cellRef] = er.result;
            if (!cell.exportVarName.empty() && IsValidIdentifier(cell.exportVarName)) {
                ioEnv[cell.exportVarName] = er.result;
                tableEnv[cell.exportVarName] = er.result;
            }
            ++rep.evaluatedCount;
        } else {
            rep.ok = false;
            ++rep.errorCount;
            rep.results[idx].ok = false;
            rep.results[idx].errorUtf8 = er.errorUtf8;
            rep.errors.push_back(cell.cellRef + ": " + er.errorUtf8);
        }
    }
    return rep;
}

CasEvalResult EvaluateMarginSideCalc(const std::string& marginTextUtf8, SymbolEnv& ioEnv) {
    std::string raw = TrimUtf8(marginTextUtf8);
    if (raw.compare(0, 6, "#calc ") == 0 || raw.compare(0, 6, "@calc ") == 0) {
        raw = TrimUtf8(raw.substr(6));
    }
    const auto arrow = raw.find("=>");
    if (arrow != std::string::npos) {
        raw = TrimUtf8(raw.substr(0, arrow));
    }
    const CasEvalResult er = EvaluateExpression(raw, ioEnv);
    if (er.ok && er.hasAssignment && IsValidIdentifier(er.assignedVarUtf8)) {
        ioEnv[er.assignedVarUtf8] = er.result;
    }
    return er;
}

DagEvaluationReport EvaluateDocumentMathDag(DocumentModel& doc) {
    DagEvaluationReport report{};
    struct NodeRef {
        bool isCanvas = false;
        bool isCalcCard = false;
        bool isTable = false;
        bool isMargin = false;
        std::size_t pageIdx = 0;
        std::uint64_t objId = 0;
        std::string varName;
        std::string expr;
        TableSheetModel tableSheet;
    };

    auto stripDirective = [](std::string raw, MathSubMode& ioMode) {
        auto checkTag = [&](const char* tag, MathSubMode m) {
            const auto pos = raw.find(tag);
            if (pos != std::string::npos) {
                ioMode = m;
                raw.erase(pos, std::strlen(tag));
            }
        };
        checkTag("#varsub", MathSubMode::VarSub);
        checkTag("#nosub", MathSubMode::NoSub);
        checkTag("#novar", MathSubMode::NoVar);
        return TrimUtf8(raw);
    };

    std::vector<NodeRef> nodes;
    auto scanObj = [&](bool isCanvas, std::size_t pgIdx, const AnnotationObject& obj) {
        if (HasTagInsensitive(obj, "table") || obj.captionUtf8 == "table") {
            NodeRef nr;
            nr.isCanvas = isCanvas;
            nr.isTable = true;
            nr.pageIdx = pgIdx;
            nr.objId = obj.id;
            nr.tableSheet = ParseTableFromObject(obj);
            std::string combined;
            for (const auto& c : nr.tableSheet.cells) {
                combined += " " + c.rawExprUtf8;
                if (!c.exportVarName.empty() && nr.varName.empty()) nr.varName = c.exportVarName;
            }
            nr.expr = combined;
            nodes.push_back(std::move(nr));
            return;
        }
        if (obj.kind == ObjectKind::MathBlock) {
            NodeRef nr;
            nr.isCanvas = isCanvas;
            nr.pageIdx = pgIdx;
            nr.objId = obj.id;
            MathSubMode m = obj.mathSubMode;
            nr.expr = stripDirective(obj.formulaExprUtf8, m);
            nr.varName = TrimUtf8(obj.formulaVarNameUtf8);
            if (nr.varName.empty()) {
                const auto eqPos = nr.expr.find('=');
                if (eqPos != std::string::npos && eqPos > 0) {
                    const std::string cand = TrimUtf8(nr.expr.substr(0, eqPos));
                    if (IsValidIdentifier(cand)) nr.varName = cand;
                }
            }
            nodes.push_back(std::move(nr));
        } else if (obj.kind == ObjectKind::CalcSessionCard &&
                   (!obj.formulaVarNameUtf8.empty() || !obj.formulaExprUtf8.empty())) {
            // Bug 5 fix: evaluate CalcSessionCard even when formulaVarNameUtf8 is empty
            NodeRef nr;
            nr.isCanvas = isCanvas;
            nr.isCalcCard = true;
            nr.pageIdx = pgIdx;
            nr.objId = obj.id;
            MathSubMode m = obj.mathSubMode;
            nr.expr = stripDirective(obj.formulaExprUtf8.empty() ? obj.evaluatedResultUtf8 : obj.formulaExprUtf8, m);
            nr.varName = TrimUtf8(obj.formulaVarNameUtf8);
            if (nr.varName.empty()) {
                const auto eqPos = nr.expr.find('=');
                if (eqPos != std::string::npos && eqPos > 0) {
                    const std::string cand = TrimUtf8(nr.expr.substr(0, eqPos));
                    if (IsValidIdentifier(cand)) nr.varName = cand;
                }
            }
            nodes.push_back(std::move(nr));
        } else if (obj.kind == ObjectKind::TextBox) {
            const bool marginTagged = HasTagInsensitive(obj, "margin") || HasTagInsensitive(obj, "sidecalc") || HasTagInsensitive(obj, "margen");
            const bool calcPrefix = (obj.textUtf8.compare(0, 6, "#calc ") == 0 || obj.textUtf8.compare(0, 6, "@calc ") == 0);
            if (marginTagged || calcPrefix || !obj.formulaExprUtf8.empty()) {
                NodeRef nr;
                nr.isCanvas = isCanvas;
                nr.isMargin = true;
                nr.pageIdx = pgIdx;
                nr.objId = obj.id;
                std::string raw = !obj.formulaExprUtf8.empty() ? obj.formulaExprUtf8 : obj.textUtf8;
                if (raw.compare(0, 6, "#calc ") == 0 || raw.compare(0, 6, "@calc ") == 0) raw = TrimUtf8(raw.substr(6));
                const auto arrow = raw.find("=>");
                if (arrow != std::string::npos) raw = TrimUtf8(raw.substr(0, arrow));
                MathSubMode m = obj.mathSubMode;
                nr.expr = stripDirective(raw, m);
                nr.varName = TrimUtf8(obj.formulaVarNameUtf8);
                if (nr.varName.empty()) {
                    const auto eqPos = nr.expr.find('=');
                    if (eqPos != std::string::npos && eqPos > 0) {
                        const std::string cand = TrimUtf8(nr.expr.substr(0, eqPos));
                        if (IsValidIdentifier(cand)) nr.varName = cand;
                    }
                }
                nodes.push_back(std::move(nr));
            }
        }
    };

    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        for (const auto& obj : doc.Pages()[p].objects) scanObj(false, p, obj);
    }
    for (const auto& obj : doc.CanvasObjects()) scanObj(true, 0, obj);

    if (nodes.empty()) return report;

    std::unordered_map<std::string, std::size_t> varToNode;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i].varName.empty()) varToNode[nodes[i].varName] = i;
        if (nodes[i].isTable) {
            for (const auto& c : nodes[i].tableSheet.cells) {
                if (!c.exportVarName.empty() && IsValidIdentifier(c.exportVarName)) {
                    varToNode[c.exportVarName] = i;
                }
            }
        }
    }

    auto extractIdentRefs = [](const std::string& expr) {
        std::unordered_set<std::string> ids;
        std::size_t i = 0;
        const auto eq = expr.find('=');
        if (eq != std::string::npos) i = eq + 1;
        while (i < expr.size()) {
            const unsigned char c = static_cast<unsigned char>(expr[i]);
            if (std::isalpha(c) || c == '_') {
                std::size_t st = i;
                while (i < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_')) ++i;
                ids.insert(expr.substr(st, i - st));
            } else {
                ++i;
            }
        }
        return ids;
    };

    const std::size_t n = nodes.size();
    std::vector<std::vector<std::size_t>> adj(n);
    std::vector<int> indeg(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        for (const auto& ref : extractIdentRefs(nodes[i].expr)) {
            auto it = varToNode.find(ref);
            if (it != varToNode.end() && it->second != i) {
                adj[it->second].push_back(i);
                ++indeg[i];
            }
        }
    }

    std::vector<std::size_t> order;
    order.reserve(n);
    std::vector<std::size_t> q;
    for (std::size_t i = 0; i < n; ++i) if (indeg[i] == 0) q.push_back(i);
    std::size_t head = 0;
    while (head < q.size()) {
        const std::size_t u = q[head++];
        order.push_back(u);
        for (std::size_t v : adj[u]) {
            if (--indeg[v] == 0) q.push_back(v);
        }
    }

    if (order.size() < n) {
        report.ok = false;
        report.cycleCount = static_cast<int>(n - order.size());
        report.errors.push_back("Ciclo de dependencias en f\xC3\xB3rmulas del lienzo.");
    }

    SymbolEnv env;
    for (std::size_t idx : order) {
        const auto& nr = nodes[idx];
        AnnotationObject* mutObj = nr.isCanvas
            ? doc.FindCanvasObjectMutable(nr.objId)
            : doc.FindPageObjectMutable(nr.pageIdx, nr.objId);
        if (!mutObj) continue;

        if (nr.isTable) {
            const auto trep = EvaluateTableDag(nr.tableSheet, env);
            std::ostringstream tblOut;
            for (std::size_t r = 0; r < trep.results.size(); ++r) {
                if (r > 0) tblOut << "; ";
                tblOut << trep.results[r].cellRef << "="
                       << (trep.results[r].ok ? trep.results[r].formattedUtf8 : ("ERR:" + trep.results[r].errorUtf8));
            }
            mutObj->evaluatedResultUtf8 = tblOut.str();
            mutObj->textUtf8 = tblOut.str();
            if (trep.ok) {
                report.evaluatedCount += trep.evaluatedCount;
            } else {
                report.ok = false;
                report.errorCount += trep.errorCount;
                for (const auto& e : trep.errors) report.errors.push_back(e);
            }
            continue;
        }

        const CasEvalResult er = EvaluateExpression(nr.expr, env);
        if (er.ok) {
            if (!nr.varName.empty()) {
                env[nr.varName] = er.result;
            }
            MathSubMode subMode = mutObj->mathSubMode;
            stripDirective(mutObj->formulaExprUtf8, subMode);
            mutObj->evaluatedResultUtf8 = er.formattedResultUtf8;

            const std::string symRhs = [&]() {
                const auto eqp = nr.expr.find('=');
                return eqp == std::string::npos ? nr.expr : TrimUtf8(nr.expr.substr(eqp + 1));
            }();

            if (nr.isCalcCard) {
                // Bug 5 fix: preserve "CASIO <model> | <keys> -> <result>" and update calcLatexUtf8
                const std::string model = mutObj->calcModelIdUtf8.empty() ? std::string("ClassWiz") : mutObj->calcModelIdUtf8;
                mutObj->textUtf8 = "CASIO " + model +
                    (mutObj->calcKeySeqUtf8.empty() ? std::string("") : (" | " + mutObj->calcKeySeqUtf8)) +
                    " -> " + er.formattedResultUtf8;
                mutObj->calcLatexUtf8 = "$" + ConvertCasExprToLatex(nr.expr) + " = " + ConvertCasExprToLatex(er.formattedResultUtf8) + "$";
            } else if (nr.isMargin) {
                const auto arrow = mutObj->textUtf8.find("=>");
                const std::string prefix = (arrow != std::string::npos)
                    ? TrimUtf8(mutObj->textUtf8.substr(0, arrow))
                    : (mutObj->textUtf8.empty() ? nr.expr : TrimUtf8(mutObj->textUtf8));
                mutObj->textUtf8 = prefix + " => " + er.formattedResultUtf8;
                mutObj->calcLatexUtf8 = FormatVarSubLatex2D(nr.varName, symRhs, er.varSubExpansionUtf8, er.formattedResultUtf8, subMode);
            } else {
                if (subMode == MathSubMode::NoSub) {
                    if (!nr.varName.empty()) {
                        mutObj->textUtf8 = nr.varName + " = " + symRhs + " = " + er.formattedResultUtf8;
                    } else {
                        mutObj->textUtf8 = symRhs + " = " + er.formattedResultUtf8;
                    }
                } else if (subMode == MathSubMode::NoVar) {
                    mutObj->textUtf8 = er.formattedResultUtf8;
                } else {
                    if (!nr.varName.empty() && symRhs != er.varSubExpansionUtf8) {
                        mutObj->textUtf8 = nr.varName + " = " + symRhs + " = " + er.varSubExpansionUtf8 + " = " + er.formattedResultUtf8;
                    } else if (!nr.varName.empty()) {
                        mutObj->textUtf8 = nr.varName + " = " + er.formattedResultUtf8;
                    } else {
                        mutObj->textUtf8 = er.varSubExpansionUtf8 + " = " + er.formattedResultUtf8;
                    }
                }
                mutObj->calcLatexUtf8 = FormatVarSubLatex2D(nr.varName, symRhs, er.varSubExpansionUtf8, er.formattedResultUtf8, subMode);
            }

            if (!mutObj->stepCheckInputUtf8.empty()) {
                const auto sc = CheckAlgebraicStep(mutObj->stepCheckInputUtf8, nr.expr, "x", env);
                mutObj->stepCheckStatusUtf8 = sc.statusCode + ": " + sc.messageUtf8;
            }
            ++report.evaluatedCount;
        } else {
            report.ok = false;
            ++report.errorCount;
            mutObj->evaluatedResultUtf8 = (nr.isCalcCard ? "ERR: " : "ERROR: ") + er.errorUtf8;
            mutObj->stepCheckStatusUtf8 = er.diagnosticCode.empty() ? "ERR" : er.diagnosticCode;
            if (nr.isCalcCard) {
                const std::string model = mutObj->calcModelIdUtf8.empty() ? std::string("ClassWiz") : mutObj->calcModelIdUtf8;
                mutObj->textUtf8 = "CASIO " + model +
                    (mutObj->calcKeySeqUtf8.empty() ? std::string("") : (" | " + mutObj->calcKeySeqUtf8)) +
                    " -> " + mutObj->evaluatedResultUtf8;
            }
            report.errors.push_back(er.errorUtf8);
        }
    }

    auto syncPlotEnv = [&](AnnotationObject& pobj) {
        if (pobj.kind != ObjectKind::PlotBlock) return;
        json cfg = json::object();
        if (!pobj.plotConfigUtf8.empty()) {
            try { cfg = json::parse(pobj.plotConfigUtf8); } catch (...) { cfg = json::object(); }
        }
        if (!cfg.is_object()) cfg = json::object();
        json envJson = json::object();
        for (const auto& kv : env) {
            if (kv.second.IsReal()) {
                const double dispVal = (kv.second.unitScaleToSi > 0.0 && !kv.second.preferredUnitUtf8.empty())
                    ? ((kv.second.value.real() - kv.second.unitOffsetToSi) / kv.second.unitScaleToSi)
                    : kv.second.value.real();
                envJson[kv.first] = dispVal;
            }
        }
        cfg["env"] = std::move(envJson);
        pobj.plotConfigUtf8 = cfg.dump();
    };
    for (auto& pg : doc.MutablePages()) {
        for (auto& o : pg.objects) syncPlotEnv(o);
    }
    for (std::size_t i = 0; i < doc.CanvasObjects().size(); ++i) {
        const auto id = doc.CanvasObjects()[i].id;
        if (auto* mutP = doc.FindCanvasObjectMutable(id)) {
            if (mutP->kind == ObjectKind::PlotBlock) syncPlotEnv(*mutP);
        }
    }

    doc.BumpRevision();
    return report;
}

namespace detail_ped {
StepCheckResultV2 CheckAlgebraicStepV2WithHintVars(
    const std::string& prevLineUtf8,
    const std::string& nextLineUtf8,
    const SymbolEnv& baseEnv,
    const std::unordered_set<std::string>& extraBoundVars);
}

StepCheckResult CheckAlgebraicStep(
    const std::string& prevExprUtf8,
    const std::string& nextExprUtf8,
    const std::string& varName,
    const SymbolEnv& baseEnv)
{
    StepCheckResult out{};
    // Preserve backwards compatibility when probing RHS directly with varName if single-RHS step check
    auto stripLhs = [](const std::string& s) {
        const auto p = s.find('=');
        return (p == std::string::npos) ? s : s.substr(p + 1);
    };
    const std::string rhsA = stripLhs(prevExprUtf8);
    const std::string rhsB = stripLhs(nextExprUtf8);

    std::unordered_set<std::string> hintVars;
    if (!varName.empty()) hintVars.insert(varName);
    const auto v2 = detail_ped::CheckAlgebraicStepV2WithHintVars(rhsA, rhsB, baseEnv, hintVars);
    out.equivalent = v2.equivalent;
    out.dimensionMismatch = v2.dimensionMismatch;
    out.maxResidual = v2.maxResidual;
    if (v2.statusCode == "OK" || v2.statusCode == "DIM_ERR" || v2.statusCode == "SYNTAX_ERR") {
        out.statusCode = v2.statusCode;
    } else {
        out.statusCode = "DIFF";
    }
    out.messageUtf8 = v2.messageUtf8;
    return out;
}

std::vector<DocPoint> SampleFunction2D(
    const std::string& exprUtf8,
    const std::string& varName,
    double xMin,
    double xMax,
    std::size_t numSamples,
    const SymbolEnv& env)
{
    std::vector<DocPoint> pts;
    if (numSamples < 2 || xMax <= xMin) return pts;
    pts.reserve(numSamples);
    SymbolEnv workEnv = env;
    const std::string vn = varName.empty() ? "x" : varName;
    std::unordered_set<std::string> boundVars{ vn };
    for (const auto& kv : env) boundVars.insert(kv.first);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(exprUtf8, ast, boundVars);

    for (std::size_t i = 0; i < numSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(numSamples - 1);
        const double x = xMin + t * (xMax - xMin);
        workEnv[vn] = QuantityValue{ { x, 0.0 }, {}, "", 1.0, 0.0, false };
        CasEvalResult ev{};
        if (!hasAst || !EvaluateAst(ast, workEnv, ev)) {
            ev = EvaluateExpression(exprUtf8, workEnv);
        }
        if (ev.ok && std::isfinite(ev.result.value.real())) {
            pts.push_back({ x, ev.result.value.real() });
        }
    }
    return pts;
}

std::vector<PlotPoint3D> SampleSurface3D(
    const std::string& exprUtf8,
    double xMin, double xMax, std::size_t nx,
    double yMin, double yMax, std::size_t ny,
    const SymbolEnv& env)
{
    std::vector<PlotPoint3D> grid;
    if (nx < 2 || ny < 2 || xMax <= xMin || yMax <= yMin) return grid;
    grid.reserve(nx * ny);
    SymbolEnv workEnv = env;
    std::unordered_set<std::string> boundVars{ "x", "y" };
    for (const auto& kv : env) boundVars.insert(kv.first);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(exprUtf8, ast, boundVars);

    for (std::size_t j = 0; j < ny; ++j) {
        const double y = yMin + (static_cast<double>(j) / static_cast<double>(ny - 1)) * (yMax - yMin);
        workEnv["y"] = QuantityValue{ { y, 0.0 }, {}, "", 1.0, 0.0, false };
        for (std::size_t i = 0; i < nx; ++i) {
            const double x = xMin + (static_cast<double>(i) / static_cast<double>(nx - 1)) * (xMax - xMin);
            workEnv["x"] = QuantityValue{ { x, 0.0 }, {}, "", 1.0, 0.0, false };
            CasEvalResult ev{};
            if (!hasAst || !EvaluateAst(ast, workEnv, ev)) {
                ev = EvaluateExpression(exprUtf8, workEnv);
            }
            const bool ok = ev.ok && std::isfinite(ev.result.value.real());
            grid.push_back({ x, y, ok ? ev.result.value.real() : 0.0, ok });
        }
    }
    return grid;
}

RegressionResult FitCsvRegression(const std::string& csvTextUtf8, int polyDegree) {
    RegressionResult res{};
    const int deg = std::clamp(polyDegree, 1, 4);
    res.degree = deg;

    // Bug 6 fix: delegate to locale-aware CSV parser (handles ';' + decimal commas like "0,5; 1,25")
    const auto ext = FitCsvRegressionExtended(csvTextUtf8, RegressionModelKind::Polynomial, deg);
    res.dataPoints = ext.dataPoints;
    if (!ext.ok) return res;
    res.ok = true;
    res.coeffs = ext.coeffs;
    res.rSquared = ext.rSquared;
    res.equationUtf8 = ext.equationUtf8;
    res.fittedCurve = ext.fittedCurve;
    return res;
}

// ---------------- Linear System (SEL) & Matrix Evaluator ----------------

namespace {

bool SolveRealLinearSystem(
    std::vector<std::vector<double>>& aug,
    std::vector<double>& outX,
    double& outDet,
    std::string& outTrace)
{
    const std::size_t n = aug.size();
    if (n == 0 || aug[0].size() != n + 1) return false;
    outDet = 1.0;
    std::ostringstream trace;
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        double best = std::abs(aug[col][col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            if (std::abs(aug[r][col]) > best) {
                best = std::abs(aug[r][col]);
                piv = r;
            }
        }
        if (best < 1e-12) {
            outDet = 0.0;
            return false;
        }
        if (piv != col) {
            std::swap(aug[col], aug[piv]);
            outDet = -outDet;
            trace << "R" << (col + 1) << " <-> R" << (piv + 1) << "\n";
        }
        const double diag = aug[col][col];
        outDet *= diag;
        for (std::size_t c = col; c <= n; ++c) aug[col][c] /= diag;
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            const double f = aug[r][col];
            if (std::abs(f) < 1e-15) continue;
            for (std::size_t c = col; c <= n; ++c) aug[r][c] -= f * aug[col][c];
        }
    }
    outX.resize(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        outX[i] = aug[i][n];
    }
    outTrace = trace.str();
    return true;
}

std::vector<std::string> ExtractLinearSystemVars(const std::vector<std::string>& eqs) {
    std::vector<std::string> vars;
    std::unordered_set<std::string> seen;
    static const std::unordered_set<std::string> kReserved{
        "sin", "cos", "tan", "sqrt", "exp", "ln", "log", "abs", "pi", "e"
    };
    for (const auto& eq : eqs) {
        std::size_t i = 0;
        while (i < eq.size()) {
            const unsigned char c = static_cast<unsigned char>(eq[i]);
            if (std::isalpha(c) || c == '_') {
                const std::size_t st = i;
                while (i < eq.size() && (std::isalnum(static_cast<unsigned char>(eq[i])) || eq[i] == '_')) ++i;
                const std::string id = eq.substr(st, i - st);
                if (kReserved.find(id) == kReserved.end() && seen.insert(id).second) {
                    vars.push_back(id);
                }
            } else {
                ++i;
            }
        }
    }
    return vars;
}

std::string NormalizeImplicitVarMul(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (std::size_t i = 0; i < s.size(); ++i) {
        out.push_back(s[i]);
        if (i + 1 < s.size()) {
            const unsigned char a = static_cast<unsigned char>(s[i]);
            const unsigned char b = static_cast<unsigned char>(s[i + 1]);
            if ((std::isdigit(a) || a == '.') && (std::isalpha(b) || b == '_')) {
                if ((b == 'e' || b == 'E') && i + 2 < s.size() &&
                    (std::isdigit(static_cast<unsigned char>(s[i + 2])) || s[i + 2] == '+' || s[i + 2] == '-')) {
                    continue;
                }
                out.push_back('*');
            }
        }
    }
    return out;
}

bool ParseMatrixLiteral(const std::string& txt, std::vector<std::vector<double>>& outM) {
    outM.clear();
    const std::string s = TrimUtf8(txt);
    if (s.size() < 4 || s.front() != '[' || s.back() != ']') return false;
    std::size_t i = 1;
    const std::size_t end = s.size() - 1;
    while (i < end) {
        while (i < end && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',' || s[i] == ';')) ++i;
        if (i >= end) break;
        if (s[i] != '[') return false;
        ++i;
        std::vector<double> row;
        while (i < end && s[i] != ']') {
            std::size_t st = i;
            while (i < end && s[i] != ',' && s[i] != ']') ++i;
            const std::string cell = TrimUtf8(s.substr(st, i - st));
            if (!cell.empty()) {
                const auto er = EvaluateExpression(cell);
                if (!er.ok || !er.result.IsReal()) return false;
                row.push_back(er.result.value.real());
            }
            if (i < end && s[i] == ',') ++i;
        }
        if (i >= end || s[i] != ']' || row.empty()) return false;
        ++i;
        if (!outM.empty() && row.size() != outM[0].size()) return false;
        outM.push_back(std::move(row));
    }
    return !outM.empty();
}

std::string FormatMatrixText(const std::vector<std::vector<double>>& m) {
    std::ostringstream oss;
    oss << "[";
    for (std::size_t r = 0; r < m.size(); ++r) {
        if (r > 0) oss << ", ";
        oss << "[";
        for (std::size_t c = 0; c < m[r].size(); ++c) {
            if (c > 0) oss << ", ";
            oss << FormatRealClean(m[r][c], 5);
        }
        oss << "]";
    }
    oss << "]";
    return oss.str();
}

std::string FormatMatrixLatex(const std::vector<std::vector<double>>& m) {
    std::ostringstream oss;
    oss << "\\begin{bmatrix} ";
    for (std::size_t r = 0; r < m.size(); ++r) {
        if (r > 0) oss << " \\\\ ";
        for (std::size_t c = 0; c < m[r].size(); ++c) {
            if (c > 0) oss << " & ";
            oss << FormatRealClean(m[r][c], 5);
        }
    }
    oss << " \\end{bmatrix}";
    return oss.str();
}

} // namespace

LinearSystemResult SolveLinearSystemEquations(const std::string& equationsUtf8) {
    LinearSystemResult res{};
    std::string norm = equationsUtf8;
    for (char& ch : norm) {
        if (ch == ';') ch = '\n';
    }
    std::istringstream iss(norm);
    std::vector<std::string> eqLines;
    std::string ln;
    while (std::getline(iss, ln)) {
        ln = TrimUtf8(ln);
        if (!ln.empty()) eqLines.push_back(NormalizeImplicitVarMul(ln));
    }
    if (eqLines.empty()) {
        res.errorUtf8 = "Sistema lineal vac\xC3\xADo";
        return res;
    }

    res.varNames = ExtractLinearSystemVars(eqLines);
    const std::size_t n = res.varNames.size();
    if (n == 0 || n != eqLines.size() || n > 64) {
        res.errorUtf8 = "El sistema requiere N ecuaciones para N inc\xC3\xB3gnitas (encontradas " +
            std::to_string(eqLines.size()) + " ecs, " + std::to_string(n) + " vars)";
        return res;
    }

    std::vector<std::vector<double>> aug(n, std::vector<double>(n + 1, 0.0));
    SymbolEnv zeroEnv;
    for (const auto& v : res.varNames) {
        zeroEnv[v] = QuantityValue{ { 0.0, 0.0 }, {}, "", 1.0, 0.0, false };
    }

    for (std::size_t r = 0; r < n; ++r) {
        const auto eqPos = eqLines[r].find('=');
        if (eqPos == std::string::npos) {
            res.errorUtf8 = "Falta '=' en ecuaci\xC3\xB3n: " + eqLines[r];
            return res;
        }
        const std::string lhs = eqLines[r].substr(0, eqPos);
        const std::string rhs = eqLines[r].substr(eqPos + 1);

        auto evalResidual = [&](const SymbolEnv& e, double& outVal) -> bool {
            const auto el = EvaluateExpression(lhs, e);
            const auto er = EvaluateExpression(rhs, e);
            if (!el.ok || !er.ok || !el.result.IsReal() || !er.result.IsReal()) return false;
            outVal = el.result.value.real() - er.result.value.real();
            return true;
        };

        double f0 = 0.0;
        if (!evalResidual(zeroEnv, f0)) {
            res.errorUtf8 = "No se pudo evaluar ecuaci\xC3\xB3n lineal: " + eqLines[r];
            return res;
        }
        aug[r][n] = -f0;

        for (std::size_t c = 0; c < n; ++c) {
            SymbolEnv e1 = zeroEnv;
            e1[res.varNames[c]].value = { 1.0, 0.0 };
            double f1 = 0.0;
            if (!evalResidual(e1, f1)) {
                res.errorUtf8 = "Coeficiente inv\xC3\xA1lido en variable " + res.varNames[c];
                return res;
            }
            SymbolEnv e2 = zeroEnv;
            e2[res.varNames[c]].value = { 2.0, 0.0 };
            double f2 = 0.0;
            if (!evalResidual(e2, f2) || std::abs((f2 - f1) - (f1 - f0)) > 1e-7) {
                res.errorUtf8 = "La ecuaci\xC3\xB3n no es lineal en " + res.varNames[c];
                return res;
            }
            aug[r][c] = f1 - f0;
        }
    }

    std::ostringstream matHdr;
    matHdr << "Matriz aumentada [A|b]:\n";
    for (std::size_t r = 0; r < n; ++r) {
        matHdr << "  [ ";
        for (std::size_t c = 0; c < n; ++c) matHdr << FormatRealClean(aug[r][c], 4) << " ";
        matHdr << "| " << FormatRealClean(aug[r][n], 4) << " ]\n";
    }

    std::string pivTrace;
    if (!SolveRealLinearSystem(aug, res.values, res.determinant, pivTrace)) {
        res.errorUtf8 = "Sistema singular o sin soluci\xC3\xB3n \xC3\xBAnica (det = 0)";
        return res;
    }

    std::ostringstream sum;
    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0) sum << ", ";
        sum << res.varNames[i] << " = " << FormatRealClean(res.values[i], 6);
    }
    sum << "  (det = " << FormatRealClean(res.determinant, 5) << ")";
    res.summaryUtf8 = sum.str();
    res.stepsTraceUtf8 = matHdr.str() + pivTrace + "Soluci\xC3\xB3n: " + res.summaryUtf8;
    res.ok = true;
    return res;
}

MatrixEvalResult EvaluateMatrixExpression(const std::string& exprUtf8) {
    MatrixEvalResult res{};
    const std::string s = TrimUtf8(exprUtf8);
    if (s.empty()) {
        res.errorUtf8 = "Expresi\xC3\xB3n matricial vac\xC3\xAD""a";
        return res;
    }

    auto parseUnaryFn = [&](const std::string& fnName, std::vector<std::vector<double>>& outArg) -> bool {
        if (s.size() > fnName.size() + 2 &&
            s.compare(0, fnName.size(), fnName) == 0 &&
            s[fnName.size()] == '(' && s.back() == ')') {
            return ParseMatrixLiteral(s.substr(fnName.size() + 1, s.size() - fnName.size() - 2), outArg);
        }
        return false;
    };

    std::vector<std::vector<double>> A;
    if (parseUnaryFn("det", A)) {
        if (A.size() != A[0].size()) { res.errorUtf8 = "det() requiere matriz cuadrada"; return res; }
        const std::size_t n = A.size();
        std::vector<std::vector<double>> aug(n, std::vector<double>(n + 1, 0.0));
        for (std::size_t r = 0; r < n; ++r) {
            for (std::size_t c = 0; c < n; ++c) aug[r][c] = A[r][c];
        }
        std::vector<double> dummy;
        double det = 0.0;
        std::string tr;
        SolveRealLinearSystem(aug, dummy, det, tr);
        res.ok = true;
        res.isScalar = true;
        res.scalarValue = det;
        res.formattedUtf8 = FormatRealClean(det, 6);
        res.latexUtf8 = "\\det(A) = " + res.formattedUtf8;
        return res;
    }

    if (parseUnaryFn("tr", A)) {
        if (A.size() != A[0].size()) { res.errorUtf8 = "tr() requiere matriz cuadrada"; return res; }
        double traceVal = 0.0;
        for (std::size_t i = 0; i < A.size(); ++i) traceVal += A[i][i];
        res.ok = true;
        res.isScalar = true;
        res.scalarValue = traceVal;
        res.formattedUtf8 = FormatRealClean(traceVal, 6);
        res.latexUtf8 = "\\operatorname{tr}(A) = " + res.formattedUtf8;
        return res;
    }

    if (parseUnaryFn("transpose", A)) {
        const std::size_t R = A.size(), C = A[0].size();
        res.matrix.assign(C, std::vector<double>(R, 0.0));
        for (std::size_t r = 0; r < R; ++r)
            for (std::size_t c = 0; c < C; ++c)
                res.matrix[c][r] = A[r][c];
        res.rows = C; res.cols = R;
        res.formattedUtf8 = FormatMatrixText(res.matrix);
        res.latexUtf8 = FormatMatrixLatex(res.matrix);
        res.ok = true;
        return res;
    }

    if (parseUnaryFn("inv", A)) {
        if (A.size() != A[0].size()) { res.errorUtf8 = "inv() requiere matriz cuadrada"; return res; }
        const std::size_t n = A.size();
        res.matrix.assign(n, std::vector<double>(n, 0.0));
        for (std::size_t col = 0; col < n; ++col) {
            std::vector<std::vector<double>> aug(n, std::vector<double>(n + 1, 0.0));
            for (std::size_t r = 0; r < n; ++r) {
                for (std::size_t c = 0; c < n; ++c) aug[r][c] = A[r][c];
                aug[r][n] = (r == col) ? 1.0 : 0.0;
            }
            std::vector<double> x;
            double det = 0.0;
            std::string tr;
            if (!SolveRealLinearSystem(aug, x, det, tr)) {
                res.errorUtf8 = "Matriz singular: no invertible (det = 0)";
                return res;
            }
            for (std::size_t r = 0; r < n; ++r) res.matrix[r][col] = x[r];
        }
        res.rows = n; res.cols = n;
        res.formattedUtf8 = FormatMatrixText(res.matrix);
        res.latexUtf8 = FormatMatrixLatex(res.matrix);
        res.ok = true;
        return res;
    }

    int depth = 0;
    std::size_t opPos = std::string::npos;
    char opCh = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '[') ++depth;
        else if (s[i] == ']') --depth;
        else if (depth == 0 && (s[i] == '+' || s[i] == '-' || s[i] == '*')) {
            opPos = i;
            opCh = s[i];
            break;
        }
    }

    if (opPos != std::string::npos) {
        std::vector<std::vector<double>> B;
        if (!ParseMatrixLiteral(s.substr(0, opPos), A) || !ParseMatrixLiteral(s.substr(opPos + 1), B)) {
            res.errorUtf8 = "Sintaxis matricial inv\xC3\xA1lida cerca de '" + std::string(1, opCh) + "'";
            return res;
        }
        const std::size_t rA = A.size(), cA = A[0].size();
        const std::size_t rB = B.size(), cB = B[0].size();
        if (opCh == '+' || opCh == '-') {
            if (rA != rB || cA != cB) {
                res.errorUtf8 = "Dimensiones incompatibles en suma/resta matricial";
                return res;
            }
            res.matrix.assign(rA, std::vector<double>(cA, 0.0));
            for (std::size_t r = 0; r < rA; ++r)
                for (std::size_t c = 0; c < cA; ++c)
                    res.matrix[r][c] = (opCh == '+') ? (A[r][c] + B[r][c]) : (A[r][c] - B[r][c]);
            res.rows = rA; res.cols = cA;
        } else {
            if (cA != rB) {
                res.errorUtf8 = "Dimensiones incompatibles en multiplicaci\xC3\xB3n matricial";
                return res;
            }
            res.matrix.assign(rA, std::vector<double>(cB, 0.0));
            for (std::size_t r = 0; r < rA; ++r)
                for (std::size_t k = 0; k < cA; ++k)
                    for (std::size_t c = 0; c < cB; ++c)
                        res.matrix[r][c] += A[r][k] * B[k][c];
            res.rows = rA; res.cols = cB;
        }
        res.formattedUtf8 = FormatMatrixText(res.matrix);
        res.latexUtf8 = FormatMatrixLatex(res.matrix);
        res.ok = true;
        return res;
    }

    if (ParseMatrixLiteral(s, A)) {
        res.rows = A.size();
        res.cols = A[0].size();
        res.matrix = std::move(A);
        res.formattedUtf8 = FormatMatrixText(res.matrix);
        res.latexUtf8 = FormatMatrixLatex(res.matrix);
        res.ok = true;
        return res;
    }

    res.errorUtf8 = "Formato matricial no reconocido";
    return res;
}

std::string DescribePhysicalDimensionEs(const UnitDim& dim) {
    const auto& e = dim.exp;
    if (dim.IsDimensionless()) return "Magnitud Adimensional / Escalar";
    if (e == std::array<int, 7>{ 0, 1, 0, 0, 0, 0, 0 }) return "Longitud / Desplazamiento";
    if (e == std::array<int, 7>{ 0, 2, 0, 0, 0, 0, 0 }) return "\xC3\x81rea / Secci\xC3\xB3n Transversal";
    if (e == std::array<int, 7>{ 0, 3, 0, 0, 0, 0, 0 }) return "Volumen";
    if (e == std::array<int, 7>{ 1, 0, 0, 0, 0, 0, 0 }) return "Masa";
    if (e == std::array<int, 7>{ 0, 0, 1, 0, 0, 0, 0 }) return "Tiempo / Per\xC3\xADodo";
    if (e == std::array<int, 7>{ 0, 0, -1, 0, 0, 0, 0 }) return "Frecuencia / Velocidad Angular";
    if (e == std::array<int, 7>{ 0, 1, -1, 0, 0, 0, 0 }) return "Velocidad Lineal";
    if (e == std::array<int, 7>{ 0, 1, -2, 0, 0, 0, 0 }) return "Aceleraci\xC3\xB3n";
    if (e == std::array<int, 7>{ 1, 1, -2, 0, 0, 0, 0 }) return "Fuerza / Peso";
    if (e == std::array<int, 7>{ 1, -1, -2, 0, 0, 0, 0 }) return "Presi\xC3\xB3n / Esfuerzo Mec\xC3\xA1nico";
    if (e == std::array<int, 7>{ 1, 2, -2, 0, 0, 0, 0 }) return "Energ\xC3\xAD" "a / Trabajo / Momento Torsor";
    if (e == std::array<int, 7>{ 1, 2, -3, 0, 0, 0, 0 }) return "Potencia Activa";
    if (e == std::array<int, 7>{ 0, 0, 0, 1, 0, 0, 0 }) return "Corriente El\xC3\xA9" "ctrica";
    if (e == std::array<int, 7>{ 0, 0, 1, 1, 0, 0, 0 }) return "Carga El\xC3\xA9" "ctrica";
    if (e == std::array<int, 7>{ 1, 2, -3, -1, 0, 0, 0 }) return "Tensi\xC3\xB3n / Potencial El\xC3\xA9" "ctrico";
    if (e == std::array<int, 7>{ 1, 2, -3, -2, 0, 0, 0 }) return "Resistencia / Impedancia El\xC3\xA9" "ctrica";
    if (e == std::array<int, 7>{ -1, -2, 3, 2, 0, 0, 0 }) return "Conductancia / Admitancia";
    if (e == std::array<int, 7>{ -1, -2, 4, 2, 0, 0, 0 }) return "Capacitancia El\xC3\xA9" "ctrica";
    if (e == std::array<int, 7>{ 1, 2, -2, -2, 0, 0, 0 }) return "Inductancia Magn\xC3\xA9tica";
    if (e == std::array<int, 7>{ 0, 0, 0, 0, 1, 0, 0 }) return "Temperatura Termodin\xC3\xA1mica";
    return "Magnitud F\xC3\xADsica Compuesta";
}

std::vector<VariableInspectorEntry> InspectDocumentVariables(const DocumentModel& doc) {
    struct RawNode {
        bool isCanvas = false;
        std::size_t pageIdx = 0;
        std::uint64_t objId = 0;
        std::string varName;
        std::string expr;
    };
    std::vector<RawNode> raw;
    auto collect = [&](bool isCanvas, std::size_t pIdx, const AnnotationObject& o) {
        if (o.kind != ObjectKind::MathBlock && o.kind != ObjectKind::CalcSessionCard) return;
        std::string vn = TrimUtf8(o.formulaVarNameUtf8);
        std::string ex = o.formulaExprUtf8;
        if (vn.empty()) {
            const auto eq = ex.find('=');
            if (eq != std::string::npos && eq > 0) {
                const std::string cand = TrimUtf8(ex.substr(0, eq));
                if (IsValidIdentifier(cand)) vn = cand;
            }
        }
        if (vn.empty() || ex.empty()) return;
        raw.push_back({ isCanvas, pIdx, o.id, vn, ex });
    };
    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        for (const auto& o : doc.Pages()[p].objects) collect(false, p, o);
    }
    for (const auto& o : doc.CanvasObjects()) collect(true, 0, o);

    SymbolEnv env;
    std::vector<VariableInspectorEntry> out;
    std::vector<bool> resolved(raw.size(), false);
    for (std::size_t pass = 0; pass < raw.size() + 1; ++pass) {
        bool progress = false;
        for (std::size_t i = 0; i < raw.size(); ++i) {
            if (resolved[i]) continue;
            std::string cleanExpr = raw[i].expr;
            for (const char* dir : { "#varsub", "#nosub", "#novar" }) {
                const auto pos = cleanExpr.find(dir);
                if (pos != std::string::npos) cleanExpr.erase(pos, std::strlen(dir));
            }
            const auto ev = EvaluateExpression(cleanExpr, env);
            if (ev.ok) {
                env[raw[i].varName] = ev.result;
                resolved[i] = true;
                progress = true;
                VariableInspectorEntry ent;
                ent.varNameUtf8 = raw[i].varName;
                ent.physicalQuantityUtf8 = DescribePhysicalDimensionEs(ev.result.dim);
                ent.siBaseUnitsUtf8 = ev.result.dim.IsDimensionless() ? "1 (adimensional)" : ev.result.dim.FormatSiString();
                ent.currentValueUtf8 = ev.formattedResultUtf8;
                ent.sourceNodeLabelUtf8 = raw[i].isCanvas
                    ? ("Lienzo #" + std::to_string(raw[i].objId))
                    : ("P\xC3\xA1" "gina " + std::to_string(raw[i].pageIdx + 1) + " #" + std::to_string(raw[i].objId));
                ent.formulaExprUtf8 = raw[i].expr;
                out.push_back(std::move(ent));
            }
        }
        if (!progress) break;
    }
    return out;
}

std::string FormatVariableTooltipUtf8(const VariableInspectorEntry& entry) {
    return entry.varNameUtf8 + " = " + entry.currentValueUtf8 +
           " | Magnitud: " + entry.physicalQuantityUtf8 +
           " | SI: [" + entry.siBaseUnitsUtf8 + "]" +
           " | Origen: " + entry.sourceNodeLabelUtf8;
}

std::unordered_map<std::string, std::string> BuildDocumentVariableTooltipMap(const DocumentModel& doc) {
    std::unordered_map<std::string, std::string> map;
    for (const auto& ent : InspectDocumentVariables(doc)) {
        map[ent.varNameUtf8] = FormatVariableTooltipUtf8(ent);
    }
    return map;
}

} // namespace DocStudio::Cas

#include "cas_symbolic_impl.hpp"
#include "cas_pedagogy_impl.hpp"
