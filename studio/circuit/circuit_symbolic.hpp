#pragma once

#include "circuit_models.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <iomanip>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::Circuit {

namespace Detail {

inline std::string FormatLatexNumber(double v) {
    if (!std::isfinite(v) || std::abs(v) == 0.0) return "0";
    std::ostringstream oss;
    oss.precision(4);
    oss << v;
    const std::string s = oss.str();
    const std::size_t ePos = s.find_first_of("eE");
    if (ePos == std::string::npos) return s;

    std::string mant = s.substr(0, ePos);
    int expVal = 0;
    try {
        expVal = std::stoi(s.substr(ePos + 1));
    } catch (...) {
        return s;
    }
    if (mant.find('.') != std::string::npos) {
        while (!mant.empty() && mant.back() == '0') mant.pop_back();
        if (!mant.empty() && mant.back() == '.') mant.pop_back();
    }
    if (mant == "1") return "10^{" + std::to_string(expVal) + "}";
    if (mant == "-1") return "-10^{" + std::to_string(expVal) + "}";
    return mant + " \\times 10^{" + std::to_string(expVal) + "}";
}

} // namespace Detail

// -------- 1D Real Polynomial & Rational Function --------

struct Poly1D {
    std::vector<double> coeffs; // coeffs[k] * s^k

    void Trim(double eps = 0.0) {
        if (eps > 0.0) {
            for (double& v : coeffs) {
                if (std::abs(v) <= eps) v = 0.0;
            }
        }
        while (!coeffs.empty() && coeffs.back() == 0.0) {
            coeffs.pop_back();
        }
        if (coeffs.empty()) coeffs.push_back(0.0);
    }

    void TrimRelative(double relEps = 1e-11) {
        double maxAbs = 0.0;
        for (double v : coeffs) maxAbs = (std::max)(maxAbs, std::abs(v));
        const double tol = maxAbs * relEps;
        for (double& v : coeffs) {
            if (std::abs(v) <= tol) v = 0.0;
        }
        while (!coeffs.empty() && coeffs.back() == 0.0) {
            coeffs.pop_back();
        }
        if (coeffs.empty()) coeffs.push_back(0.0);
    }

    int Degree() const noexcept {
        for (int i = static_cast<int>(coeffs.size()) - 1; i >= 0; --i) {
            if (coeffs[static_cast<std::size_t>(i)] != 0.0) return i;
        }
        return 0;
    }

    std::complex<double> Eval(std::complex<double> s) const noexcept {
        std::complex<double> res{ 0.0, 0.0 };
        for (int i = static_cast<int>(coeffs.size()) - 1; i >= 0; --i) {
            res = res * s + coeffs[static_cast<std::size_t>(i)];
        }
        return res;
    }

    Poly1D operator+(const Poly1D& o) const {
        Poly1D r;
        const std::size_t n = (std::max)(coeffs.size(), o.coeffs.size());
        r.coeffs.assign(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            const double a = (i < coeffs.size()) ? coeffs[i] : 0.0;
            const double b = (i < o.coeffs.size()) ? o.coeffs[i] : 0.0;
            const double sum = a + b;
            r.coeffs[i] = (std::abs(sum) <= 1e-13 * (std::abs(a) + std::abs(b))) ? 0.0 : sum;
        }
        r.Trim();
        return r;
    }

    Poly1D operator-(const Poly1D& o) const {
        Poly1D r;
        const std::size_t n = (std::max)(coeffs.size(), o.coeffs.size());
        r.coeffs.assign(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) {
            const double a = (i < coeffs.size()) ? coeffs[i] : 0.0;
            const double b = (i < o.coeffs.size()) ? o.coeffs[i] : 0.0;
            const double diff = a - b;
            r.coeffs[i] = (std::abs(diff) <= 1e-13 * (std::abs(a) + std::abs(b))) ? 0.0 : diff;
        }
        r.Trim();
        return r;
    }

    Poly1D operator*(const Poly1D& o) const {
        Poly1D r;
        if (coeffs.empty() || o.coeffs.empty()) {
            r.coeffs = { 0.0 };
            return r;
        }
        r.coeffs.assign(coeffs.size() + o.coeffs.size() - 1, 0.0);
        std::vector<double> absAcc(r.coeffs.size(), 0.0);
        for (std::size_t i = 0; i < coeffs.size(); ++i) {
            for (std::size_t j = 0; j < o.coeffs.size(); ++j) {
                const double prod = coeffs[i] * o.coeffs[j];
                r.coeffs[i + j] += prod;
                absAcc[i + j] += std::abs(prod);
            }
        }
        for (std::size_t k = 0; k < r.coeffs.size(); ++k) {
            if (std::abs(r.coeffs[k]) <= 1e-13 * absAcc[k]) r.coeffs[k] = 0.0;
        }
        r.Trim();
        return r;
    }

    std::string Format(const char* var = "s", bool latex = false) const {
        // Compute characteristic frequency scale so physical s^k coefficients (e.g. 1e-12*s^2) are never suppressed
        int lowIdx = -1, highIdx = -1;
        for (int i = 0; i < static_cast<int>(coeffs.size()); ++i) {
            if (coeffs[static_cast<std::size_t>(i)] != 0.0) {
                if (lowIdx < 0) lowIdx = i;
                highIdx = i;
            }
        }
        if (lowIdx < 0) return "0";
        double wc = 1.0;
        if (highIdx > lowIdx) {
            const double ratio = std::abs(coeffs[static_cast<std::size_t>(lowIdx)]) /
                                 std::abs(coeffs[static_cast<std::size_t>(highIdx)]);
            if (ratio > 0.0 && std::isfinite(ratio)) {
                wc = std::pow(ratio, 1.0 / static_cast<double>(highIdx - lowIdx));
            }
        }
        double maxScaled = 0.0;
        double wPow = 1.0;
        for (std::size_t p = 0; p < coeffs.size(); ++p) {
            maxScaled = (std::max)(maxScaled, std::abs(coeffs[p]) * wPow);
            wPow *= wc;
        }
        const double scaledEps = maxScaled * 1e-7;

        std::ostringstream oss;
        oss.precision(4);
        bool any = false;
        wPow = 1.0;
        for (std::size_t p = 0; p < coeffs.size(); ++p) {
            const double val = coeffs[p];
            if (val == 0.0 || std::abs(val) * wPow <= scaledEps) {
                wPow *= wc;
                continue;
            }
            wPow *= wc;
            if (any) oss << (val >= 0.0 ? " + " : " - ");
            else if (val < 0.0) oss << "-";
            const double av = std::abs(val);
            if (p == 0 || std::abs(av - 1.0) > 1e-5) {
                if (latex) oss << Detail::FormatLatexNumber(av);
                else oss << av;
                if (p >= 1 && !latex) oss << "*";
                else if (p >= 1 && latex) oss << " ";
            }
            if (p == 1) oss << var;
            else if (p >= 2) {
                if (latex) oss << var << "^{" << p << "}";
                else oss << var << "^" << p;
            }
            any = true;
        }
        if (!any) return "0";
        return oss.str();
    }
};

namespace Detail {

inline double ComputeCharOmegaScale(const Poly1D& a, const Poly1D& b) noexcept {
    int lowIdx = -1, highIdx = -1;
    double lowVal = 0.0, highVal = 0.0;
    auto scan = [&](const Poly1D& p) {
        for (int i = 0; i < static_cast<int>(p.coeffs.size()); ++i) {
            const double av = std::abs(p.coeffs[static_cast<std::size_t>(i)]);
            if (av == 0.0) continue;
            if (lowIdx < 0 || i < lowIdx) { lowIdx = i; lowVal = av; }
            else if (i == lowIdx) lowVal = (std::max)(lowVal, av);
            if (highIdx < 0 || i > highIdx) { highIdx = i; highVal = av; }
            else if (i == highIdx) highVal = (std::max)(highVal, av);
        }
    };
    scan(a);
    scan(b);
    if (highIdx > lowIdx && lowVal > 0.0 && highVal > 0.0) {
        const double wc = std::pow(lowVal / highVal, 1.0 / static_cast<double>(highIdx - lowIdx));
        if (std::isfinite(wc) && wc > 1e-30 && wc < 1e30) return wc;
    }
    return 1.0;
}

inline Poly1D ScalePolyVar(const Poly1D& p, double wc) {
    if (wc == 1.0) return p;
    Poly1D out;
    out.coeffs.resize(p.coeffs.size(), 0.0);
    double wPow = 1.0;
    for (std::size_t i = 0; i < p.coeffs.size(); ++i) {
        out.coeffs[i] = p.coeffs[i] * wPow;
        wPow *= wc;
    }
    out.Trim();
    return out;
}

inline std::pair<Poly1D, Poly1D> PolyDivMod(Poly1D a, const Poly1D& b) {
    a.Trim();
    Poly1D divisor = b;
    divisor.Trim();
    const double wc = ComputeCharOmegaScale(a, divisor);
    Poly1D au = ScalePolyVar(a, wc);
    Poly1D bu = ScalePolyVar(divisor, wc);
    const int degB = bu.Degree();
    if (degB == 0 && bu.coeffs[0] == 0.0) {
        return { Poly1D{ { 0.0 } }, a };
    }
    const double leadB = bu.coeffs[static_cast<std::size_t>(degB)];
    int degA = au.Degree();
    if (degA < degB) return { Poly1D{ { 0.0 } }, a };

    Poly1D qu;
    qu.coeffs.assign(static_cast<std::size_t>(degA - degB + 1), 0.0);
    std::vector<double> rem = au.coeffs;
    std::vector<double> absAcc(au.coeffs.size(), 0.0);
    for (std::size_t i = 0; i < au.coeffs.size(); ++i) absAcc[i] = std::abs(au.coeffs[i]);
    for (int k = degA - degB; k >= 0; --k) {
        const double coeff = rem[static_cast<std::size_t>(k + degB)] / leadB;
        qu.coeffs[static_cast<std::size_t>(k)] = coeff;
        for (int j = 0; j <= degB; ++j) {
            const double term = coeff * bu.coeffs[static_cast<std::size_t>(j)];
            rem[static_cast<std::size_t>(k + j)] -= term;
            absAcc[static_cast<std::size_t>(k + j)] += std::abs(term);
        }
    }
    for (std::size_t i = 0; i < rem.size(); ++i) {
        if (std::abs(rem[i]) <= 1e-12 * absAcc[i]) rem[i] = 0.0;
    }
    Poly1D ru{ std::move(rem) };
    qu.Trim();
    ru.Trim();
    return { ScalePolyVar(qu, 1.0 / wc), ScalePolyVar(ru, 1.0 / wc) };
}

inline Poly1D PolyGcd(Poly1D a, Poly1D b) {
    a.Trim();
    b.Trim();
    const double wc = ComputeCharOmegaScale(a, b);
    Poly1D au = ScalePolyVar(a, wc);
    Poly1D bu = ScalePolyVar(b, wc);
    auto normalizeMax = [](Poly1D& p) {
        double maxAbs = 0.0;
        for (double c : p.coeffs) maxAbs = (std::max)(maxAbs, std::abs(c));
        if (maxAbs > 0.0) {
            for (double& c : p.coeffs) c /= maxAbs;
        }
        p.TrimRelative(1e-10);
        const int d = p.Degree();
        const double l = p.coeffs[static_cast<std::size_t>(d)];
        if (l != 0.0) {
            for (double& c : p.coeffs) c /= l;
        }
        p.Trim();
    };
    normalizeMax(au);
    normalizeMax(bu);
    for (int iter = 0; iter < 32; ++iter) {
        if (bu.Degree() == 0 && std::abs(bu.coeffs[0]) < 1e-8) break;
        if (bu.Degree() == 0) return Poly1D{ { 1.0 } };
        auto dm = PolyDivMod(au, bu);
        au = std::move(bu);
        bu = std::move(dm.second);
        bu.TrimRelative(1e-8);
        if (bu.Degree() > 0 || std::abs(bu.coeffs[0]) > 1e-8) {
            normalizeMax(bu);
        }
    }
    normalizeMax(au);
    return ScalePolyVar(au, 1.0 / wc);
}

} // namespace Detail

struct RationalPoly {
    Poly1D num{ { 0.0 } };
    Poly1D den{ { 1.0 } };

    void Normalize(bool reduceGcd = false) {
        num.Trim();
        den.Trim();
        if (den.Degree() == 0 && den.coeffs[0] == 0.0) {
            den.coeffs = { 1.0 };
            return;
        }
        const double wc = Detail::ComputeCharOmegaScale(den, num);
        Poly1D numU = Detail::ScalePolyVar(num, wc);
        Poly1D denU = Detail::ScalePolyVar(den, wc);
        numU.TrimRelative(1e-12);
        denU.TrimRelative(1e-12);
        if (denU.Degree() == 0 && denU.coeffs[0] == 0.0) {
            den = { { 1.0 } };
            return;
        }

        // Strip common factors of u^k
        double maxN = 0.0, maxD = 0.0;
        for (double c : numU.coeffs) maxN = (std::max)(maxN, std::abs(c));
        for (double c : denU.coeffs) maxD = (std::max)(maxD, std::abs(c));
        std::size_t commonPow = 0;
        while (commonPow < numU.coeffs.size() && commonPow < denU.coeffs.size() &&
               std::abs(numU.coeffs[commonPow]) <= maxN * 1e-12 &&
               std::abs(denU.coeffs[commonPow]) <= maxD * 1e-12) {
            ++commonPow;
        }
        if (commonPow > 0) {
            numU.coeffs.erase(numU.coeffs.begin(), numU.coeffs.begin() + static_cast<std::ptrdiff_t>(commonPow));
            denU.coeffs.erase(denU.coeffs.begin(), denU.coeffs.begin() + static_cast<std::ptrdiff_t>(commonPow));
            if (numU.coeffs.empty()) numU.coeffs.push_back(0.0);
            if (denU.coeffs.empty()) denU.coeffs.push_back(1.0);
        }
        if (reduceGcd && numU.Degree() > 0 && denU.Degree() > 0) {
            const Poly1D g = Detail::PolyGcd(numU, denU);
            if (g.Degree() >= 1) {
                numU = Detail::PolyDivMod(numU, g).first;
                denU = Detail::PolyDivMod(denU, g).first;
            }
        }
        num = Detail::ScalePolyVar(numU, 1.0 / wc);
        den = Detail::ScalePolyVar(denU, 1.0 / wc);

        // Normalize so lowest non-zero denominator coefficient is +1 (or clean constant term = 1)
        double pivot = 0.0;
        for (double c : den.coeffs) {
            if (c != 0.0) {
                pivot = c;
                break;
            }
        }
        if (pivot != 0.0) {
            for (double& c : num.coeffs) c /= pivot;
            for (double& c : den.coeffs) c /= pivot;
        }
        num.Trim();
        den.Trim();
    }

    std::complex<double> Eval(std::complex<double> s) const noexcept {
        const std::complex<double> d = den.Eval(s);
        if (std::abs(d) < 1e-30) return { 0.0, 0.0 };
        return num.Eval(s) / d;
    }

    RationalPoly operator+(const RationalPoly& o) const {
        RationalPoly r;
        r.num = (num * o.den) + (o.num * den);
        r.den = den * o.den;
        r.Normalize(true);
        return r;
    }

    RationalPoly operator-(const RationalPoly& o) const {
        RationalPoly r;
        r.num = (num * o.den) - (o.num * den);
        r.den = den * o.den;
        r.Normalize(true);
        return r;
    }

    RationalPoly operator*(const RationalPoly& o) const {
        RationalPoly r;
        r.num = num * o.num;
        r.den = den * o.den;
        r.Normalize(true);
        return r;
    }

    RationalPoly operator/(const RationalPoly& o) const {
        RationalPoly r;
        r.num = num * o.den;
        r.den = den * o.num;
        r.Normalize(true);
        return r;
    }

    std::string FormatUtf8(const char* var = "s") const {
        if (den.Degree() == 0 && std::abs(den.coeffs[0] - 1.0) < 1e-9) {
            return num.Format(var, false);
        }
        return "(" + num.Format(var, false) + ") / (" + den.Format(var, false) + ")";
    }

    std::string FormatLatex(const char* var = "s") const {
        if (den.Degree() == 0 && std::abs(den.coeffs[0] - 1.0) < 1e-9) {
            return num.Format(var, true);
        }
        return "\\frac{" + num.Format(var, true) + "}{" + den.Format(var, true) + "}";
    }
};

// -------- Multivariate Symbolic Polynomial (for small-circuit Bareiss symbolic H(s)) --------

struct SymMonomial {
    double coeff = 0.0;
    int sPower = 0;
    std::map<std::string, int> vars;

    bool SameSignature(const SymMonomial& o) const noexcept {
        return sPower == o.sPower && vars == o.vars;
    }

    bool operator<(const SymMonomial& o) const noexcept {
        if (sPower != o.sPower) return sPower < o.sPower;
        return vars < o.vars;
    }
};

struct SymPoly {
    std::vector<SymMonomial> terms;
    bool overflowed = false;
    static constexpr std::size_t kMaxTerms = 512;

    static SymPoly Constant(double v) {
        SymPoly p;
        if (std::abs(v) > 1e-14) p.terms.push_back({ v, 0, {} });
        return p;
    }

    static SymPoly Var(const std::string& name, int sPow = 0, double coeff = 1.0) {
        SymPoly p;
        if (std::abs(coeff) > 1e-14) {
            SymMonomial m;
            m.coeff = coeff;
            m.sPower = sPow;
            if (!name.empty()) m.vars[name] = 1;
            p.terms.push_back(std::move(m));
        }
        return p;
    }

    void Canonicalize() {
        std::sort(terms.begin(), terms.end());
        std::vector<SymMonomial> merged;
        for (const auto& t : terms) {
            if (std::abs(t.coeff) <= 1e-14) continue;
            if (!merged.empty() && merged.back().SameSignature(t)) {
                merged.back().coeff += t.coeff;
                if (std::abs(merged.back().coeff) <= 1e-14) merged.pop_back();
            } else {
                merged.push_back(t);
            }
        }
        if (merged.size() > kMaxTerms) {
            overflowed = true;
            merged.resize(kMaxTerms);
        }
        terms = std::move(merged);
    }

    bool IsZero() const noexcept { return terms.empty(); }

    SymPoly operator+(const SymPoly& o) const {
        SymPoly r;
        r.overflowed = overflowed || o.overflowed;
        r.terms = terms;
        r.terms.insert(r.terms.end(), o.terms.begin(), o.terms.end());
        r.Canonicalize();
        return r;
    }

    SymPoly operator-(const SymPoly& o) const {
        SymPoly r;
        r.overflowed = overflowed || o.overflowed;
        r.terms = terms;
        for (auto t : o.terms) {
            t.coeff = -t.coeff;
            r.terms.push_back(std::move(t));
        }
        r.Canonicalize();
        return r;
    }

    SymPoly operator*(const SymPoly& o) const {
        SymPoly r;
        r.overflowed = overflowed || o.overflowed;
        if (terms.empty() || o.terms.empty()) return r;
        if (terms.size() * o.terms.size() > kMaxTerms * 4) {
            r.overflowed = true;
            return r;
        }
        for (const auto& a : terms) {
            for (const auto& b : o.terms) {
                SymMonomial m;
                m.coeff = a.coeff * b.coeff;
                m.sPower = a.sPower + b.sPower;
                m.vars = a.vars;
                for (const auto& kv : b.vars) m.vars[kv.first] += kv.second;
                r.terms.push_back(std::move(m));
            }
        }
        r.Canonicalize();
        return r;
    }

    std::string ToLatex() const {
        if (terms.empty()) return "0";
        std::ostringstream oss;
        oss.precision(4);
        bool first = true;
        for (const auto& t : terms) {
            if (std::abs(t.coeff) <= 1e-14) continue;
            if (!first) oss << (t.coeff >= 0.0 ? " + " : " - ");
            else if (t.coeff < 0.0) oss << "-";
            first = false;

            const double ac = std::abs(t.coeff);
            const bool hasSymbols = (t.sPower != 0 || !t.vars.empty());
            if (!hasSymbols || std::abs(ac - 1.0) > 1e-6) {
                oss << Detail::FormatLatexNumber(ac);
                if (hasSymbols) oss << " ";
            }
            if (t.sPower == 1) oss << "s";
            else if (t.sPower > 1) oss << "s^{" << t.sPower << "}";
            for (const auto& kv : t.vars) {
                if (t.sPower > 0 || &kv != &(*t.vars.begin())) oss << " ";
                oss << kv.first;
                if (kv.second > 1) oss << "^{" << kv.second << "}";
            }
        }
        return oss.str();
    }
};

// -------- Aberth-Ehrlich Polynomial Root Finder & Routh-Hurwitz --------

inline std::vector<std::complex<double>> FindPolynomialRootsAberth(const Poly1D& polyIn) {
    Poly1D p = polyIn;
    p.Trim();
    int deg = p.Degree();
    std::vector<std::complex<double>> roots;
    if (deg <= 0) return roots;

    // Factor out zero roots s^m
    std::size_t zeroMult = 0;
    while (zeroMult < p.coeffs.size() && p.coeffs[zeroMult] == 0.0) {
        roots.emplace_back(0.0, 0.0);
        ++zeroMult;
    }
    if (zeroMult > 0) {
        p.coeffs.erase(p.coeffs.begin(), p.coeffs.begin() + static_cast<std::ptrdiff_t>(zeroMult));
        deg = p.Degree();
        if (deg <= 0) return roots;
    }

    if (deg == 1) {
        roots.emplace_back(-p.coeffs[0] / p.coeffs[1], 0.0);
        return roots;
    }
    if (deg == 2) {
        const double a = p.coeffs[2];
        const double b = p.coeffs[1];
        const double c = p.coeffs[0];
        const double disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            const double sq = std::sqrt(disc);
            roots.emplace_back((-b + sq) / (2.0 * a), 0.0);
            roots.emplace_back((-b - sq) / (2.0 * a), 0.0);
        } else {
            const double sq = std::sqrt(-disc);
            roots.emplace_back(-b / (2.0 * a), sq / (2.0 * a));
            roots.emplace_back(-b / (2.0 * a), -sq / (2.0 * a));
        }
        return roots;
    }

    // Scale variable s = wc * u so coefficients have balanced O(1) magnitudes
    const double wc = Detail::ComputeCharOmegaScale(p, p);
    const Poly1D pu = Detail::ScalePolyVar(p, wc);
    const double lead = pu.coeffs[static_cast<std::size_t>(deg)];
    std::vector<double> norm(static_cast<std::size_t>(deg + 1), 0.0);
    double maxRatio = 0.0;
    for (int i = 0; i <= deg; ++i) {
        norm[static_cast<std::size_t>(i)] = pu.coeffs[static_cast<std::size_t>(i)] / lead;
        if (i < deg) maxRatio = (std::max)(maxRatio, std::abs(norm[static_cast<std::size_t>(i)]));
    }
    const double cauchyR = (std::max)(0.5, 1.0 + maxRatio * 0.5);

    std::vector<std::complex<double>> z(static_cast<std::size_t>(deg));
    for (int k = 0; k < deg; ++k) {
        const double ang = 2.0 * kPi * (static_cast<double>(k) + 0.137) / static_cast<double>(deg);
        const double rad = cauchyR * (0.75 + 0.25 * static_cast<double>(k) / static_cast<double>(deg));
        z[static_cast<std::size_t>(k)] = std::polar(rad, ang);
    }

    auto evalPDp = [&](std::complex<double> s) -> std::pair<std::complex<double>, std::complex<double>> {
        std::complex<double> pv = norm[static_cast<std::size_t>(deg)];
        std::complex<double> dpv{ 0.0, 0.0 };
        for (int i = deg - 1; i >= 0; --i) {
            dpv = dpv * s + pv;
            pv = pv * s + norm[static_cast<std::size_t>(i)];
        }
        return { pv, dpv };
    };

    for (int iter = 0; iter < 120; ++iter) {
        double maxStep = 0.0;
        for (int k = 0; k < deg; ++k) {
            const auto [pv, dpv] = evalPDp(z[static_cast<std::size_t>(k)]);
            if (std::abs(pv) < 1e-13) continue;
            std::complex<double> sumInv{ 0.0, 0.0 };
            for (int j = 0; j < deg; ++j) {
                if (j == k) continue;
                const std::complex<double> diff = z[static_cast<std::size_t>(k)] - z[static_cast<std::size_t>(j)];
                if (std::abs(diff) > 1e-14) sumInv += std::complex<double>(1.0, 0.0) / diff;
            }
            const std::complex<double> ratio = (std::abs(dpv) > 1e-15) ? (pv / dpv) : pv;
            const std::complex<double> denom = std::complex<double>(1.0, 0.0) - ratio * sumInv;
            const std::complex<double> w = (std::abs(denom) > 1e-15) ? (ratio / denom) : ratio;
            z[static_cast<std::size_t>(k)] -= w;
            maxStep = (std::max)(maxStep, std::abs(w));
        }
        if (maxStep < 1e-11) break;
    }

    for (auto& r : z) {
        if (std::abs(r.imag()) < 1e-9) r.imag(0.0);
        roots.push_back(r * wc);
    }
    return roots;
}

inline bool ComputeRouthHurwitzStable(const Poly1D& denIn) {
    Poly1D d = denIn;
    d.Trim();
    const int n = d.Degree();
    if (n <= 0) return d.coeffs[0] != 0.0;

    // Scale s = wc * u (wc > 0 preserves LHP root locations) and normalize
    const double wc = Detail::ComputeCharOmegaScale(d, d);
    d = Detail::ScalePolyVar(d, wc);
    if (d.coeffs[static_cast<std::size_t>(n)] < 0.0) {
        for (double& c : d.coeffs) c = -c;
    }
    double maxC = 0.0;
    for (int i = 0; i <= n; ++i) maxC = (std::max)(maxC, std::abs(d.coeffs[static_cast<std::size_t>(i)]));
    if (maxC <= 0.0) return false;
    for (int i = 0; i <= n; ++i) d.coeffs[static_cast<std::size_t>(i)] /= maxC;

    // Necessary condition: all coefficients strictly positive
    for (int i = 0; i <= n; ++i) {
        if (d.coeffs[static_cast<std::size_t>(i)] <= 1e-14) return false;
    }

    const int cols = (n + 2) / 2;
    std::vector<std::vector<double>> routh(static_cast<std::size_t>(n + 1), std::vector<double>(static_cast<std::size_t>(cols), 0.0));
    for (int i = 0; i <= n; ++i) {
        const std::size_t row = static_cast<std::size_t>(i % 2);
        const std::size_t col = static_cast<std::size_t>(i / 2);
        routh[row][col] = d.coeffs[static_cast<std::size_t>(n - i)];
    }

    for (int r = 2; r <= n; ++r) {
        const double piv = routh[static_cast<std::size_t>(r - 1)][0];
        if (std::abs(piv) <= 1e-13) return false;
        for (int c = 0; c < cols - 1; ++c) {
            const double a = routh[static_cast<std::size_t>(r - 1)][0];
            const double b = routh[static_cast<std::size_t>(r - 2)][static_cast<std::size_t>(c + 1)];
            const double x = routh[static_cast<std::size_t>(r - 2)][0];
            const double y = routh[static_cast<std::size_t>(r - 1)][static_cast<std::size_t>(c + 1)];
            routh[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = (a * b - x * y) / piv;
        }
        if (routh[static_cast<std::size_t>(r)][0] <= 1e-12) return false;
    }
    return true;
}

// -------- Bode Extended, Nyquist, Root-Locus & State-Space Structures --------

struct InitialCondition {
    std::string componentName;
    double value = 0.0; // v_C(0^-) or i_L(0^-)
};

struct SwitchEvent {
    std::string switchName;
    double toggleTimeSec = 0.0;
    bool initiallyClosed = false;
    bool closedAfterT0 = true;
};

struct NyquistPoint {
    double freqHz = 0.0;
    double re = 0.0;
    double im = 0.0;
};

struct RootLocusPoint {
    double gainK = 0.0;
    std::vector<std::complex<double>> closedLoopPoles;
};

struct StateSpaceModel {
    bool ok = false;
    std::vector<std::string> stateNames;
    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
    std::vector<std::vector<double>> A;
    std::vector<std::vector<double>> B;
    std::vector<std::vector<double>> C;
    std::vector<std::vector<double>> D;
    std::string stateVectorLatex;
    std::string equationsLatex;
    std::string diagnosticUtf8;
};

struct BodeExtendedPoint {
    double freqHz = 1.0;
    double magDb = 0.0;
    double phaseDeg = 0.0;
    double phaseUnwrappedDeg = 0.0;
    double realH = 0.0;
    double imagH = 0.0;
};

struct BodeAnalysisResult {
    bool ok = false;
    std::vector<BodePoint> points;
    std::vector<BodeExtendedPoint> extendedPoints;
    std::vector<NyquistPoint> nyquistPoints;
    double cutoffHz = 0.0;
    bool hasCutoff = false;
    double resonantPeakDb = -1e9;
    double resonantFreqHz = 0.0;
    double gainMarginDb = 0.0;
    double phaseMarginDeg = 0.0;
    bool hasGainMargin = false;
    bool hasPhaseMargin = false;
    std::string errorUtf8;
};

struct FullSymbolicCircuitReport {
    bool ok = false;
    std::string errorUtf8;

    // 1. Nodal / MNA Matrix
    std::string mnaMatrixLatex;
    std::vector<std::string> nodalKclLatex;

    // 2. Mesh (KVL) Equations
    std::vector<std::string> meshLoopsDesc;
    std::string meshMatrixLatex;
    std::vector<std::string> meshKvlEquationsLatex;

    // 3. DC Superposition
    struct SuperpositionTerm {
        std::string sourceName;
        std::map<int, double> nodeContributionsV;
        std::string latexSummary;
    };
    std::vector<SuperpositionTerm> dcSuperposition;
    std::string dcSuperpositionTotalLatex;

    // 4. AC Phasor Analysis
    double acFreqHz = 0.0;
    double omega0RadPerSec = 0.0;
    std::string acPhasorMatrixLatex;
    std::map<int, std::complex<double>> acNodePhasors;
    std::vector<std::string> acNodePhasorsLatex;

    // 5. Laplace (s) with ICs & t=0 Switch Transitions
    std::vector<InitialCondition> resolvedInitialConditions;
    std::vector<std::string> laplaceIcEquationsLatex;
    std::map<int, RationalPoly> nodeVoltagesS;
    std::string outputResponseSLatex;

    // 6. Transfer Function H(s)
    RationalPoly transferFunction;
    std::string transferFunctionUtf8;
    std::string transferFunctionLatex;
    std::string symbolicHOfSLatex;
    double dcGain = 0.0;

    // 7. State-Space (x, A, B, C, D)
    StateSpaceModel stateSpace;

    // 8. Frequency & Stability Analysis
    std::vector<std::complex<double>> zeros;
    std::vector<std::complex<double>> poles;
    std::vector<BodePoint> bodeSweep;
    std::vector<double> bodeUnwrappedPhaseDeg;
    std::vector<NyquistPoint> nyquistPoints;
    std::vector<RootLocusPoint> rootLocusBranches;
    double gainMarginDb = 0.0;
    double phaseMarginDeg = 0.0;
    bool hasGainMargin = false;
    bool hasPhaseMargin = false;
    bool isBibostable = true;
};

namespace Detail {

inline bool SolveComplexSystemSymbolic(
    std::vector<std::vector<std::complex<double>>>& A,
    std::vector<std::complex<double>>& b)
{
    const std::size_t n = b.size();
    double matNorm = 0.0;
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) matNorm = (std::max)(matNorm, std::abs(A[r][c]));
    }
    if (matNorm == 0.0 || !std::isfinite(matNorm)) return false;
    const double pivTol = matNorm * 1e-15;

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        double best = std::abs(A[col][col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            const double val = std::abs(A[r][col]);
            if (val > best) { best = val; piv = r; }
        }
        if (best < pivTol || !std::isfinite(best)) return false;
        if (piv != col) {
            std::swap(A[col], A[piv]);
            std::swap(b[col], b[piv]);
        }
        const std::complex<double> inv = std::complex<double>(1.0, 0.0) / A[col][col];
        for (std::size_t c = col; c < n; ++c) A[col][c] *= inv;
        b[col] *= inv;
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            const std::complex<double> f = A[r][col];
            if (std::abs(f) == 0.0) continue;
            for (std::size_t c = col; c < n; ++c) A[r][c] -= f * A[col][c];
            b[r] -= f * b[col];
        }
    }
    return true;
}

inline std::complex<double> ComplexDetLU(std::vector<std::vector<std::complex<double>>> A) {
    const std::size_t n = A.size();
    if (n == 0) return { 1.0, 0.0 };
    double matNorm = 0.0;
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) matNorm = (std::max)(matNorm, std::abs(A[r][c]));
    }
    if (matNorm == 0.0 || !std::isfinite(matNorm)) return { 0.0, 0.0 };
    const double pivTol = matNorm * 1e-15;

    std::complex<double> det{ 1.0, 0.0 };
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        double best = std::abs(A[col][col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            const double val = std::abs(A[r][col]);
            if (val > best) { best = val; piv = r; }
        }
        if (best < pivTol || !std::isfinite(best)) return { 0.0, 0.0 };
        if (piv != col) {
            std::swap(A[col], A[piv]);
            det = -det;
        }
        const std::complex<double> diag = A[col][col];
        det *= diag;
        for (std::size_t r = col + 1; r < n; ++r) {
            const std::complex<double> f = A[r][col] / diag;
            for (std::size_t c = col; c < n; ++c) A[r][c] -= f * A[col][c];
        }
    }
    return det;
}

struct MatrixPencil {
    int maxNode = 0;
    std::size_t dim = 0;
    std::vector<std::vector<double>> M0;
    std::vector<std::vector<double>> M1;
    std::vector<double> b0;
    std::vector<std::string> unknownNames;
    double charOmega = 1000.0;
};

inline bool BuildLinearMatrixPencil(
    const std::vector<CircuitComponent>& components,
    const std::map<std::string, double>& capIcV,
    const std::map<std::string, double>& indIcI,
    bool unitFirstSource,
    MatrixPencil& out)
{
    int maxNode = 0;
    std::size_t numAux = 0;
    std::vector<int> auxIdx(components.size(), -1);
    double sumLogW = 0.0;
    std::size_t wCnt = 0;
    double avgR = 1000.0;
    std::size_t rCnt = 0;

    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        if (!c.active) continue;
        maxNode = (std::max)(maxNode, c.nodePos);
        maxNode = (std::max)(maxNode, c.nodeNeg);
        if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
            c.kind == ComponentKind::Potentiometer || c.kind == ComponentKind::Transformer) {
            maxNode = (std::max)(maxNode, c.nodeOut);
        }
        if (c.kind == ComponentKind::Transformer) maxNode = (std::max)(maxNode, c.nodeCtrlPos);
        if (c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Vccs) {
            maxNode = (std::max)(maxNode, c.nodeCtrlPos);
            maxNode = (std::max)(maxNode, c.nodeCtrlNeg);
        }
        if (c.kind == ComponentKind::Resistor && c.value > 0.0) {
            avgR = (rCnt == 0) ? c.value : std::sqrt(avgR * c.value);
            ++rCnt;
        }
        if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
            c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Vcc ||
            c.kind == ComponentKind::PulseSource || c.kind == ComponentKind::Inductor ||
            c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Vcvs ||
            c.kind == ComponentKind::Transformer || c.kind == ComponentKind::Ammeter ||
            c.kind == ComponentKind::CurrentProbe) {
            auxIdx[i] = static_cast<int>(numAux++);
        }
    }
    if (maxNode <= 0) return false;

    for (const auto& c : components) {
        if (!c.active) continue;
        if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
            if (c.value > 0.0) {
                sumLogW += std::log(1.0 / (avgR * c.value));
                ++wCnt;
            }
        } else if (c.kind == ComponentKind::Inductor && c.value > 0.0) {
            sumLogW += std::log(avgR / c.value);
            ++wCnt;
        }
    }

    out.maxNode = maxNode;
    out.dim = static_cast<std::size_t>(maxNode) + numAux;
    out.charOmega = (wCnt > 0) ? std::exp(sumLogW / static_cast<double>(wCnt)) : 1000.0;
    out.M0.assign(out.dim, std::vector<double>(out.dim, 0.0));
    out.M1.assign(out.dim, std::vector<double>(out.dim, 0.0));
    out.b0.assign(out.dim, 0.0);
    out.unknownNames.clear();
    for (int n = 1; n <= maxNode; ++n) out.unknownNames.push_back("V_{" + std::to_string(n) + "}(s)");

    for (int n = 0; n < maxNode; ++n) {
        out.M0[static_cast<std::size_t>(n)][static_cast<std::size_t>(n)] += 1e-12;
    }

    auto stampG = [&](std::vector<std::vector<double>>& mat, int a, int b, double g) {
        if (a > 0) mat[static_cast<std::size_t>(a - 1)][static_cast<std::size_t>(a - 1)] += g;
        if (b > 0) mat[static_cast<std::size_t>(b - 1)][static_cast<std::size_t>(b - 1)] += g;
        if (a > 0 && b > 0) {
            mat[static_cast<std::size_t>(a - 1)][static_cast<std::size_t>(b - 1)] -= g;
            mat[static_cast<std::size_t>(b - 1)][static_cast<std::size_t>(a - 1)] -= g;
        }
    };

    bool excited = false;
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        if (!c.active) continue;
        if (c.kind == ComponentKind::Resistor) {
            stampG(out.M0, c.nodePos, c.nodeNeg, 1.0 / (std::max)(1e-9, std::abs(c.value)));
        } else if (c.kind == ComponentKind::Potentiometer) {
            const double alpha = std::clamp(c.auxValue > 0.0 ? c.auxValue : 0.5, 0.001, 0.999);
            const double rTot = (std::max)(1e-6, std::abs(c.value));
            stampG(out.M0, c.nodePos, c.nodeOut, 1.0 / ((1.0 - alpha) * rTot));
            stampG(out.M0, c.nodeOut, c.nodeNeg, 1.0 / (alpha * rTot));
        } else if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
            const double cap = (std::max)(1e-15, std::abs(c.value));
            stampG(out.M1, c.nodePos, c.nodeNeg, cap);
            auto itIc = capIcV.find(c.name);
            if (itIc != capIcV.end()) {
                const double iIc = cap * itIc->second;
                if (c.nodePos > 0) out.b0[static_cast<std::size_t>(c.nodePos - 1)] += iIc;
                if (c.nodeNeg > 0) out.b0[static_cast<std::size_t>(c.nodeNeg - 1)] -= iIc;
            }
        } else if (c.kind == ComponentKind::Inductor) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            out.unknownNames.push_back("I_{" + c.name + "}(s)");
            if (c.nodePos > 0) {
                out.M0[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0;
                out.M0[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0;
            }
            if (c.nodeNeg > 0) {
                out.M0[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0;
                out.M0[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0;
            }
            out.M0[k][k] -= 1e-9;
            out.M1[k][k] -= (std::max)(1e-12, std::abs(c.value));
            auto itIc = indIcI.find(c.name);
            if (itIc != indIcI.end()) {
                out.b0[k] = -c.value * itIc->second;
            }
        } else if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
                   c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Vcc ||
                   c.kind == ComponentKind::PulseSource) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            out.unknownNames.push_back("I_{" + c.name + "}(s)");
            if (c.nodePos > 0) {
                out.M0[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0;
                out.M0[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0;
            }
            if (c.nodeNeg > 0 && c.kind != ComponentKind::Vcc) {
                out.M0[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0;
                out.M0[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0;
            }
            double val = c.value;
            if (unitFirstSource) {
                val = (!excited) ? 1.0 : 0.0;
                excited = true;
            }
            out.b0[k] = val;
        } else if (c.kind == ComponentKind::CurrentSource) {
            double val = c.value;
            if (unitFirstSource) {
                val = (!excited) ? 1.0 : 0.0;
                excited = true;
            }
            if (c.nodePos > 0) out.b0[static_cast<std::size_t>(c.nodePos - 1)] -= val;
            if (c.nodeNeg > 0) out.b0[static_cast<std::size_t>(c.nodeNeg - 1)] += val;
        } else if (c.kind == ComponentKind::OpAmp) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            out.unknownNames.push_back("I_{" + c.name + ",out}(s)");
            if (c.nodeOut > 0) out.M0[static_cast<std::size_t>(c.nodeOut - 1)][k] += 1.0;
            if (c.nodePos > 0) out.M0[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0;
            if (c.nodeNeg > 0) out.M0[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0;
        } else if (c.kind == ComponentKind::Vcvs) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            out.unknownNames.push_back("I_{" + c.name + "}(s)");
            const double av = (c.auxValue != 0.0) ? c.auxValue : c.value;
            if (c.nodePos > 0) { out.M0[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; out.M0[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (c.nodeNeg > 0) { out.M0[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0; out.M0[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0; }
            if (c.nodeCtrlPos > 0) out.M0[k][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= av;
            if (c.nodeCtrlNeg > 0) out.M0[k][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] += av;
        } else if (c.kind == ComponentKind::Vccs) {
            const double gm = (c.auxValue != 0.0) ? c.auxValue : c.value;
            if (c.nodePos > 0 && c.nodeCtrlPos > 0) out.M0[static_cast<std::size_t>(c.nodePos - 1)][static_cast<std::size_t>(c.nodeCtrlPos - 1)] += gm;
            if (c.nodePos > 0 && c.nodeCtrlNeg > 0) out.M0[static_cast<std::size_t>(c.nodePos - 1)][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] -= gm;
            if (c.nodeNeg > 0 && c.nodeCtrlPos > 0) out.M0[static_cast<std::size_t>(c.nodeNeg - 1)][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= gm;
            if (c.nodeNeg > 0 && c.nodeCtrlNeg > 0) out.M0[static_cast<std::size_t>(c.nodeNeg - 1)][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] += gm;
        } else if (c.kind == ComponentKind::Transformer) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            out.unknownNames.push_back("I_{" + c.name + ",s}(s)");
            const double nRatio = (std::abs(c.value) > 1e-9) ? c.value : 1.0;
            if (c.nodeOut > 0) { out.M0[static_cast<std::size_t>(c.nodeOut - 1)][k] += 1.0; out.M0[k][static_cast<std::size_t>(c.nodeOut - 1)] += 1.0; }
            if (c.nodeCtrlPos > 0) { out.M0[static_cast<std::size_t>(c.nodeCtrlPos - 1)][k] -= 1.0; out.M0[k][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= 1.0; }
            if (c.nodePos > 0) { out.M0[static_cast<std::size_t>(c.nodePos - 1)][k] -= nRatio; out.M0[k][static_cast<std::size_t>(c.nodePos - 1)] -= nRatio; }
            if (c.nodeNeg > 0) { out.M0[static_cast<std::size_t>(c.nodeNeg - 1)][k] += nRatio; out.M0[k][static_cast<std::size_t>(c.nodeNeg - 1)] += nRatio; }
        } else if (c.kind == ComponentKind::Switch || c.kind == ComponentKind::PushButton) {
            const bool closed = (c.auxValue > 0.5);
            stampG(out.M0, c.nodePos, c.nodeNeg, closed ? 1e6 : 1e-9);
        } else if (c.kind == ComponentKind::Wire || c.kind == ComponentKind::Connector || c.kind == ComponentKind::Fuse) {
            stampG(out.M0, c.nodePos, c.nodeNeg, 1e6);
        } else if (c.kind == ComponentKind::Ground) {
            stampG(out.M0, c.nodePos, 0, 1e6);
        } else if (c.kind == ComponentKind::Ammeter || c.kind == ComponentKind::CurrentProbe) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            out.unknownNames.push_back("I_{" + c.name + "}(s)");
            if (c.nodePos > 0) { out.M0[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; out.M0[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (c.nodeNeg > 0) { out.M0[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0; out.M0[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0; }
        } else if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
                   c.kind == ComponentKind::SchottkyDiode || c.kind == ComponentKind::ZenerDiode) {
            // Small-signal approximation if diode is present in symbolic pass
            stampG(out.M0, c.nodePos, c.nodeNeg, 1.0 / 25.0);
        }
    }
    return true;
}

inline std::map<int, RationalPoly> ExtractAllNodeRationalDft(const MatrixPencil& mp) {
    std::map<int, RationalPoly> out;
    if (mp.dim == 0) return out;
    const std::size_t N = mp.dim + 1;
    const double w0 = (mp.charOmega > 1e-12) ? mp.charOmega : 1000.0;

    // Equilibrate rows of (M0 + w0*u*M1) by a fixed diagonal D so det(D*(M0 + s*M1)) is O(1)
    // and common factor det(D) cancels identically between numerator and denominator.
    std::vector<double> rowScale(mp.dim, 1.0);
    for (std::size_t r = 0; r < mp.dim; ++r) {
        double maxRow = 0.0;
        for (std::size_t c = 0; c < mp.dim; ++c) {
            maxRow = (std::max)(maxRow, std::abs(mp.M0[r][c]));
            maxRow = (std::max)(maxRow, w0 * std::abs(mp.M1[r][c]));
        }
        if (maxRow > 0.0 && std::isfinite(maxRow)) {
            rowScale[r] = 1.0 / maxRow;
        }
    }

    std::vector<std::complex<double>> detSamples(N);
    std::vector<std::vector<std::complex<double>>> numSamples(
        static_cast<std::size_t>(mp.maxNode), std::vector<std::complex<double>>(N, { 0.0, 0.0 }));

    double usedRu = 1.0;
    double usedTheta0 = 0.0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        const double ru = (attempt == 0) ? 1.0 : 1.07;
        const double theta0 = (attempt == 0) ? 0.0 : 0.137;
        bool allSolved = true;
        std::fill(detSamples.begin(), detSamples.end(), std::complex<double>{ 0.0, 0.0 });
        for (auto& vec : numSamples) std::fill(vec.begin(), vec.end(), std::complex<double>{ 0.0, 0.0 });

        for (std::size_t k = 0; k < N; ++k) {
            const double ang = theta0 + 2.0 * kPi * static_cast<double>(k) / static_cast<double>(N);
            const std::complex<double> u = std::polar(ru, ang);
            const std::complex<double> s = u * w0;

            std::vector<std::vector<std::complex<double>>> A(mp.dim, std::vector<std::complex<double>>(mp.dim));
            std::vector<std::complex<double>> b(mp.dim);
            for (std::size_t r = 0; r < mp.dim; ++r) {
                const double rs = rowScale[r];
                b[r] = { rs * mp.b0[r], 0.0 };
                for (std::size_t c = 0; c < mp.dim; ++c) {
                    A[r][c] = rs * (std::complex<double>(mp.M0[r][c], 0.0) + s * mp.M1[r][c]);
                }
            }
            const std::complex<double> det = ComplexDetLU(A);
            detSamples[k] = det;
            if (std::abs(det) > 0.0 && SolveComplexSystemSymbolic(A, b)) {
                for (int n = 0; n < mp.maxNode; ++n) {
                    numSamples[static_cast<std::size_t>(n)][k] = det * b[static_cast<std::size_t>(n)];
                }
            } else {
                allSolved = false;
            }
        }
        usedRu = ru;
        usedTheta0 = theta0;
        if (allSolved) break;
    }

    double maxDet = 0.0;
    for (const auto& d : detSamples) maxDet = (std::max)(maxDet, std::abs(d));
    if (maxDet == 0.0 || !std::isfinite(maxDet)) return out;

    for (std::size_t k = 0; k < N; ++k) {
        detSamples[k] /= maxDet;
        for (int n = 0; n < mp.maxNode; ++n) {
            numSamples[static_cast<std::size_t>(n)][k] /= maxDet;
        }
    }

    const std::complex<double> uShift = std::polar(usedRu, usedTheta0);
    auto idftToPolyU = [&](const std::vector<std::complex<double>>& samples) -> Poly1D {
        Poly1D p;
        p.coeffs.assign(N, 0.0);
        std::complex<double> shiftPow{ 1.0, 0.0 };
        double maxC = 0.0;
        for (std::size_t m = 0; m < N; ++m) {
            std::complex<double> sum{ 0.0, 0.0 };
            for (std::size_t k = 0; k < N; ++k) {
                const double ang = -2.0 * kPi * static_cast<double>(m * k) / static_cast<double>(N);
                sum += samples[k] * std::polar(1.0, ang);
            }
            const std::complex<double> cm = (sum / static_cast<double>(N)) / shiftPow;
            p.coeffs[m] = cm.real();
            maxC = (std::max)(maxC, std::abs(p.coeffs[m]));
            shiftPow *= uShift;
        }
        const double uTol = maxC * 1e-9;
        for (double& c : p.coeffs) {
            if (std::abs(c) <= uTol) c = 0.0;
        }
        while (p.coeffs.size() > 1 && p.coeffs.back() == 0.0) p.coeffs.pop_back();
        if (p.coeffs.empty()) p.coeffs.push_back(0.0);
        return p;
    };

    const Poly1D denPolyU = idftToPolyU(detSamples);
    for (int n = 1; n <= mp.maxNode; ++n) {
        RationalPoly rpU;
        rpU.num = idftToPolyU(numSamples[static_cast<std::size_t>(n - 1)]);
        rpU.den = denPolyU;
        rpU.Normalize(true);

        RationalPoly rp;
        rp.num = Detail::ScalePolyVar(rpU.num, 1.0 / w0);
        rp.den = Detail::ScalePolyVar(rpU.den, 1.0 / w0);
        rp.Normalize(false);
        out[n] = std::move(rp);
    }
    return out;
}

inline std::string TryBuildSymbolicTransferLatex(
    const std::vector<CircuitComponent>& components,
    int outputNode)
{
    int maxNode = 0;
    std::size_t numAux = 0;
    std::vector<int> auxIdx(components.size(), -1);
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        if (!c.active) continue;
        maxNode = (std::max)(maxNode, c.nodePos);
        maxNode = (std::max)(maxNode, c.nodeNeg);
        if (c.kind == ComponentKind::OpAmp) maxNode = (std::max)(maxNode, c.nodeOut);
        if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
            c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Inductor ||
            c.kind == ComponentKind::OpAmp) {
            auxIdx[i] = static_cast<int>(numAux++);
        }
    }
    const std::size_t dim = static_cast<std::size_t>(maxNode) + numAux;
    if (maxNode <= 0 || dim == 0 || dim > 6 || outputNode < 1 || outputNode > maxNode) return "";

    // Build symbolic MNA matrix over SymPoly using branch admittances / impedances
    // To avoid fractions in Resistor 1/R, multiply each resistor node or use G_{name} / R_{name}
    // Wait: using conductance symbol G_{name} or clearing R denominators makes it exact polynomial!
    // Even cleaner: clear denominators per resistor? If only <= 4 resistors, multiply row by R or use R^{-1} -> multiply numerator & denom by prod(R)!
    std::vector<std::vector<SymPoly>> M(dim, std::vector<SymPoly>(dim));
    std::vector<SymPoly> b(dim);

    // Collect resistors so we represent Y_R = 1/R as G_R or clear R via product if few
    std::vector<std::string> resNames;
    for (const auto& c : components) {
        if (c.active && c.kind == ComponentKind::Resistor) resNames.push_back(c.name);
    }
    if (resNames.size() > 4) return "";

    auto stampSym = [&](int n1, int n2, const SymPoly& y) {
        if (n1 > 0) M[static_cast<std::size_t>(n1 - 1)][static_cast<std::size_t>(n1 - 1)] = M[static_cast<std::size_t>(n1 - 1)][static_cast<std::size_t>(n1 - 1)] + y;
        if (n2 > 0) M[static_cast<std::size_t>(n2 - 1)][static_cast<std::size_t>(n2 - 1)] = M[static_cast<std::size_t>(n2 - 1)][static_cast<std::size_t>(n2 - 1)] + y;
        if (n1 > 0 && n2 > 0) {
            M[static_cast<std::size_t>(n1 - 1)][static_cast<std::size_t>(n2 - 1)] = M[static_cast<std::size_t>(n1 - 1)][static_cast<std::size_t>(n2 - 1)] - y;
            M[static_cast<std::size_t>(n2 - 1)][static_cast<std::size_t>(n1 - 1)] = M[static_cast<std::size_t>(n2 - 1)][static_cast<std::size_t>(n1 - 1)] - y;
        }
    };

    SymPoly prodR = SymPoly::Constant(1.0);
    for (const auto& rn : resNames) prodR = prodR * SymPoly::Var(rn);

    bool excited = false;
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        if (!c.active) continue;
        if (c.kind == ComponentKind::Resistor) {
            SymPoly y = SymPoly::Constant(1.0);
            for (const auto& rn : resNames) {
                if (rn != c.name) y = y * SymPoly::Var(rn);
            }
            stampSym(c.nodePos, c.nodeNeg, y);
        } else if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
            SymPoly y = SymPoly::Var(c.name, 1) * prodR;
            stampSym(c.nodePos, c.nodeNeg, y);
        } else if (c.kind == ComponentKind::Inductor) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            if (c.nodePos > 0) {
                M[static_cast<std::size_t>(c.nodePos - 1)][k] = M[static_cast<std::size_t>(c.nodePos - 1)][k] + prodR;
                M[k][static_cast<std::size_t>(c.nodePos - 1)] = M[k][static_cast<std::size_t>(c.nodePos - 1)] + SymPoly::Constant(1.0);
            }
            if (c.nodeNeg > 0) {
                M[static_cast<std::size_t>(c.nodeNeg - 1)][k] = M[static_cast<std::size_t>(c.nodeNeg - 1)][k] - prodR;
                M[k][static_cast<std::size_t>(c.nodeNeg - 1)] = M[k][static_cast<std::size_t>(c.nodeNeg - 1)] - SymPoly::Constant(1.0);
            }
            M[k][k] = M[k][k] - SymPoly::Var(c.name, 1);
        } else if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery || c.kind == ComponentKind::AcSource) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            if (c.nodePos > 0) {
                M[static_cast<std::size_t>(c.nodePos - 1)][k] = M[static_cast<std::size_t>(c.nodePos - 1)][k] + prodR;
                M[k][static_cast<std::size_t>(c.nodePos - 1)] = M[k][static_cast<std::size_t>(c.nodePos - 1)] + SymPoly::Constant(1.0);
            }
            if (c.nodeNeg > 0) {
                M[static_cast<std::size_t>(c.nodeNeg - 1)][k] = M[static_cast<std::size_t>(c.nodeNeg - 1)][k] - prodR;
                M[k][static_cast<std::size_t>(c.nodeNeg - 1)] = M[k][static_cast<std::size_t>(c.nodeNeg - 1)] - SymPoly::Constant(1.0);
            }
            if (!excited) {
                b[k] = SymPoly::Constant(1.0);
                excited = true;
            }
        } else if (c.kind == ComponentKind::CurrentSource) {
            if (!excited) {
                if (c.nodePos > 0) b[static_cast<std::size_t>(c.nodePos - 1)] = b[static_cast<std::size_t>(c.nodePos - 1)] - prodR;
                if (c.nodeNeg > 0) b[static_cast<std::size_t>(c.nodeNeg - 1)] = b[static_cast<std::size_t>(c.nodeNeg - 1)] + prodR;
                excited = true;
            }
        } else if (c.kind == ComponentKind::OpAmp) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxIdx[i]);
            if (c.nodeOut > 0) M[static_cast<std::size_t>(c.nodeOut - 1)][k] = M[static_cast<std::size_t>(c.nodeOut - 1)][k] + prodR;
            if (c.nodePos > 0) M[k][static_cast<std::size_t>(c.nodePos - 1)] = M[k][static_cast<std::size_t>(c.nodePos - 1)] + SymPoly::Constant(1.0);
            if (c.nodeNeg > 0) M[k][static_cast<std::size_t>(c.nodeNeg - 1)] = M[k][static_cast<std::size_t>(c.nodeNeg - 1)] - SymPoly::Constant(1.0);
        }
    }
    if (!excited) return "";

    // Laplace expansion determinant for dim <= 6 (with memoization/overflow guard)
    auto symDet = [](const std::vector<std::vector<SymPoly>>& mat) -> SymPoly {
        const std::size_t n = mat.size();
        auto rec = [&](auto& self, std::size_t row, std::uint32_t colMask) -> SymPoly {
            if (row == n) return SymPoly::Constant(1.0);
            SymPoly acc;
            int sign = 1;
            for (std::size_t c = 0; c < n; ++c) {
                if (colMask & (1u << c)) continue;
                if (!mat[row][c].IsZero()) {
                    SymPoly sub = self(self, row + 1, colMask | (1u << c));
                    if (sub.overflowed) { acc.overflowed = true; return acc; }
                    SymPoly term = mat[row][c] * sub;
                    if (sign > 0) acc = acc + term;
                    else acc = acc - term;
                    if (acc.overflowed) return acc;
                }
                sign = -sign;
            }
            return acc;
        };
        return rec(rec, 0, 0u);
    };

    const SymPoly den = symDet(M);
    if (den.overflowed || den.IsZero()) return "";

    std::vector<std::vector<SymPoly>> Mnum = M;
    const std::size_t targetCol = static_cast<std::size_t>(outputNode - 1);
    for (std::size_t r = 0; r < dim; ++r) Mnum[r][targetCol] = b[r];
    const SymPoly num = symDet(Mnum);
    if (num.overflowed) return "";

    // Flip signs if denominator has all negative terms
    bool allNeg = !den.terms.empty();
    for (const auto& t : den.terms) {
        if (t.coeff > 0.0) { allNeg = false; break; }
    }
    SymPoly cleanNum = num;
    SymPoly cleanDen = den;
    if (allNeg) {
        cleanNum = SymPoly::Constant(0.0) - num;
        cleanDen = SymPoly::Constant(0.0) - den;
    }
    return "H(s) = \\frac{" + cleanNum.ToLatex() + "}{" + cleanDen.ToLatex() + "}";
}

} // namespace Detail

// -------- Public Bode Analysis (with Margins, Cutoff, Nyquist) --------

inline BodeAnalysisResult AnalyzeBodeSweep(
    const std::vector<CircuitComponent>& components,
    int outputNode,
    double fStartHz,
    double fEndHz,
    std::size_t numPoints = 41)
{
    BodeAnalysisResult res;
    if (components.empty() || numPoints < 2 || fStartHz <= 0.0 || fEndHz <= fStartHz) {
        res.errorUtf8 = "Parametros invalidos para analisis de Bode.";
        return res;
    }

    const double logA = std::log10(fStartHz);
    const double logB = std::log10(fEndHz);
    res.points.reserve(numPoints);
    res.extendedPoints.reserve(numPoints);
    res.nyquistPoints.reserve(numPoints);

    double prevWrapped = 0.0;
    double unwrappedOffset = 0.0;
    bool hasPrev = false;

    for (std::size_t i = 0; i < numPoints; ++i) {
        const double f = std::pow(10.0, logA + (static_cast<double>(i) / static_cast<double>(numPoints - 1)) * (logB - logA));
        const CircuitSolveResult sr = SolveMnaCircuit(components, f);
        if (!sr.ok) continue;
        auto it = sr.nodeVoltages.find(outputNode);
        if (it == sr.nodeVoltages.end()) continue;

        const std::complex<double> H = it->second;
        const double mag = (std::max)(1e-15, std::abs(H));
        const double db = 20.0 * std::log10(mag);
        double ph = std::arg(H) * (180.0 / kPi);

        if (!hasPrev) {
            if (std::abs(ph - 180.0) < 1e-6) ph = -180.0;
            prevWrapped = ph;
            hasPrev = true;
        } else {
            double delta = ph - prevWrapped;
            while (delta > 180.0) { delta -= 360.0; unwrappedOffset -= 360.0; }
            while (delta < -180.0) { delta += 360.0; unwrappedOffset += 360.0; }
            prevWrapped = ph;
        }
        const double unwrapped = ph + unwrappedOffset;

        res.points.push_back({ f, db, ph });
        res.extendedPoints.push_back({ f, db, ph, unwrapped, H.real(), H.imag() });
        res.nyquistPoints.push_back({ f, H.real(), H.imag() });

        if (db > res.resonantPeakDb) {
            res.resonantPeakDb = db;
            res.resonantFreqHz = f;
        }
    }

    if (res.extendedPoints.empty()) {
        res.errorUtf8 = "No se pudo resolver ningun punto de frecuencia.";
        return res;
    }

    // Canonicalize initial DC negative-gain phase (+180 deg branch -> -180 deg) when phase decreases
    if (res.extendedPoints.size() >= 2 &&
        res.extendedPoints.front().phaseUnwrappedDeg > 90.0 &&
        res.extendedPoints.back().phaseUnwrappedDeg < res.extendedPoints.front().phaseUnwrappedDeg) {
        for (auto& ep : res.extendedPoints) {
            ep.phaseUnwrappedDeg -= 360.0;
        }
    }

    // -3 dB cutoff relative to passband/resonant peak gain (works for LP, HP, and BP filters)
    const double refDb = res.resonantPeakDb;
    const double targetDb = refDb - 3.0;
    for (std::size_t i = 1; i < res.extendedPoints.size(); ++i) {
        const double y0 = res.extendedPoints[i - 1].magDb;
        const double y1 = res.extendedPoints[i].magDb;
        if ((y0 >= targetDb && y1 <= targetDb) || (y0 <= targetDb && y1 >= targetDb)) {
            const double t = (std::abs(y1 - y0) > 1e-12) ? ((targetDb - y0) / (y1 - y0)) : 0.5;
            const double lf0 = std::log10(res.extendedPoints[i - 1].freqHz);
            const double lf1 = std::log10(res.extendedPoints[i].freqHz);
            res.cutoffHz = std::pow(10.0, lf0 + t * (lf1 - lf0));
            res.hasCutoff = true;
            break;
        }
    }

    // Phase margin at 0 dB gain crossover; Gain margin at -180 deg phase crossover
    for (std::size_t i = 1; i < res.extendedPoints.size(); ++i) {
        const auto& a = res.extendedPoints[i - 1];
        const auto& b = res.extendedPoints[i];
        if (!res.hasPhaseMargin && ((a.magDb >= 0.0 && b.magDb <= 0.0) || (a.magDb <= 0.0 && b.magDb >= 0.0))) {
            const double t = (std::abs(b.magDb - a.magDb) > 1e-12) ? (-a.magDb / (b.magDb - a.magDb)) : 0.5;
            const double phGc = a.phaseUnwrappedDeg + t * (b.phaseUnwrappedDeg - a.phaseUnwrappedDeg);
            res.phaseMarginDeg = 180.0 + phGc;
            res.hasPhaseMargin = true;
        }
        if (!res.hasGainMargin && ((a.phaseUnwrappedDeg >= -180.0 && b.phaseUnwrappedDeg <= -180.0) ||
                                   (a.phaseUnwrappedDeg <= -180.0 && b.phaseUnwrappedDeg >= -180.0))) {
            const double t = (std::abs(b.phaseUnwrappedDeg - a.phaseUnwrappedDeg) > 1e-12)
                ? ((-180.0 - a.phaseUnwrappedDeg) / (b.phaseUnwrappedDeg - a.phaseUnwrappedDeg))
                : 0.5;
            const double dbPc = a.magDb + t * (b.magDb - a.magDb);
            res.gainMarginDb = -dbPc;
            res.hasGainMargin = true;
        }
    }

    res.ok = true;
    return res;
}

// -------- State-Space Extraction (x, A, B, C, D) --------

inline StateSpaceModel ExtractCircuitStateSpace(
    const std::vector<CircuitComponent>& components,
    int outputNode = 1)
{
    StateSpaceModel ss;
    int maxNode = 0;
    std::vector<std::size_t> capIdx, indIdx, inSrcIdx;
    std::vector<int> auxVars(components.size(), -1);
    std::size_t numOtherAux = 0;

    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        if (!c.active) continue;
        maxNode = (std::max)(maxNode, (std::max)(c.nodePos, c.nodeNeg));
        if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
            c.kind == ComponentKind::Potentiometer || c.kind == ComponentKind::Transformer) {
            maxNode = (std::max)(maxNode, c.nodeOut);
        }
        if (c.kind == ComponentKind::Transformer) maxNode = (std::max)(maxNode, c.nodeCtrlPos);
        if (c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Vccs) {
            maxNode = (std::max)(maxNode, c.nodeCtrlPos);
            maxNode = (std::max)(maxNode, c.nodeCtrlNeg);
        }
        for (int en : c.extraNodes) maxNode = (std::max)(maxNode, en);
        if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
            capIdx.push_back(i);
            ss.stateNames.push_back("v_{" + c.name + "}");
            auxVars[i] = static_cast<int>(numOtherAux++);
        } else if (c.kind == ComponentKind::Inductor) {
            indIdx.push_back(i);
            ss.stateNames.push_back("i_{" + c.name + "}");
        } else if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
                   c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Vcc ||
                   c.kind == ComponentKind::PulseSource) {
            inSrcIdx.push_back(i);
            ss.inputNames.push_back(c.name);
            auxVars[i] = static_cast<int>(numOtherAux++);
        } else if (c.kind == ComponentKind::CurrentSource) {
            inSrcIdx.push_back(i);
            ss.inputNames.push_back(c.name);
        } else if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Vcvs ||
                   c.kind == ComponentKind::Transformer || c.kind == ComponentKind::Ammeter ||
                   c.kind == ComponentKind::CurrentProbe) {
            auxVars[i] = static_cast<int>(numOtherAux++);
        }
    }

    const std::size_t nStates = capIdx.size() + indIdx.size();
    const std::size_t nInputs = inSrcIdx.size();
    ss.outputNames = { "V_{" + std::to_string(outputNode) + "}" };
    if (maxNode <= 0 || outputNode <= 0 || outputNode > maxNode) {
        ss.diagnosticUtf8 = "Output node out of range for state-space extraction.";
        return ss;
    }

    const std::size_t dim = static_cast<std::size_t>(maxNode) + numOtherAux;
    std::vector<std::vector<std::complex<double>>> Malg(dim, std::vector<std::complex<double>>(dim, { 0.0, 0.0 }));
    for (int k = 0; k < maxNode; ++k) Malg[static_cast<std::size_t>(k)][static_cast<std::size_t>(k)] += 1e-12;

    auto addG = [&](int a, int b, double g) {
        if (a > 0) Malg[static_cast<std::size_t>(a - 1)][static_cast<std::size_t>(a - 1)] += g;
        if (b > 0) Malg[static_cast<std::size_t>(b - 1)][static_cast<std::size_t>(b - 1)] += g;
        if (a > 0 && b > 0) {
            Malg[static_cast<std::size_t>(a - 1)][static_cast<std::size_t>(b - 1)] -= g;
            Malg[static_cast<std::size_t>(b - 1)][static_cast<std::size_t>(a - 1)] -= g;
        }
    };

    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& c = components[i];
        if (!c.active) continue;
        if (c.kind == ComponentKind::Resistor) {
            addG(c.nodePos, c.nodeNeg, 1.0 / (std::max)(1e-9, std::abs(c.value)));
        } else if (c.kind == ComponentKind::Potentiometer) {
            const double alpha = std::clamp(c.auxValue > 0.0 ? c.auxValue : 0.5, 0.001, 0.999);
            const double rTot = (std::max)(1e-6, std::abs(c.value));
            addG(c.nodePos, c.nodeOut, 1.0 / ((1.0 - alpha) * rTot));
            addG(c.nodeOut, c.nodeNeg, 1.0 / (alpha * rTot));
        } else if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor ||
                   c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
                   c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Vcc ||
                   c.kind == ComponentKind::PulseSource || c.kind == ComponentKind::Ammeter ||
                   c.kind == ComponentKind::CurrentProbe) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxVars[i]);
            if (c.nodePos > 0) { Malg[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; Malg[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (c.nodeNeg > 0 && c.kind != ComponentKind::Vcc) {
                Malg[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0;
                Malg[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0;
            }
        } else if (c.kind == ComponentKind::OpAmp) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxVars[i]);
            if (c.nodeOut > 0) Malg[static_cast<std::size_t>(c.nodeOut - 1)][k] += 1.0;
            if (c.nodePos > 0) Malg[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0;
            if (c.nodeNeg > 0) Malg[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0;
        } else if (c.kind == ComponentKind::Vcvs) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxVars[i]);
            const double av = (c.auxValue != 0.0) ? c.auxValue : c.value;
            if (c.nodePos > 0) { Malg[static_cast<std::size_t>(c.nodePos - 1)][k] += 1.0; Malg[k][static_cast<std::size_t>(c.nodePos - 1)] += 1.0; }
            if (c.nodeNeg > 0) { Malg[static_cast<std::size_t>(c.nodeNeg - 1)][k] -= 1.0; Malg[k][static_cast<std::size_t>(c.nodeNeg - 1)] -= 1.0; }
            if (c.nodeCtrlPos > 0) Malg[k][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= av;
            if (c.nodeCtrlNeg > 0) Malg[k][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] += av;
        } else if (c.kind == ComponentKind::Vccs) {
            const double gm = (c.auxValue != 0.0) ? c.auxValue : c.value;
            if (c.nodePos > 0 && c.nodeCtrlPos > 0) Malg[static_cast<std::size_t>(c.nodePos - 1)][static_cast<std::size_t>(c.nodeCtrlPos - 1)] += gm;
            if (c.nodePos > 0 && c.nodeCtrlNeg > 0) Malg[static_cast<std::size_t>(c.nodePos - 1)][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] -= gm;
            if (c.nodeNeg > 0 && c.nodeCtrlPos > 0) Malg[static_cast<std::size_t>(c.nodeNeg - 1)][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= gm;
            if (c.nodeNeg > 0 && c.nodeCtrlNeg > 0) Malg[static_cast<std::size_t>(c.nodeNeg - 1)][static_cast<std::size_t>(c.nodeCtrlNeg - 1)] += gm;
        } else if (c.kind == ComponentKind::Transformer) {
            const std::size_t k = static_cast<std::size_t>(maxNode + auxVars[i]);
            const double nRatio = (std::abs(c.value) > 1e-9) ? c.value : 1.0;
            if (c.nodeOut > 0) { Malg[static_cast<std::size_t>(c.nodeOut - 1)][k] += 1.0; Malg[k][static_cast<std::size_t>(c.nodeOut - 1)] += 1.0; }
            if (c.nodeCtrlPos > 0) { Malg[static_cast<std::size_t>(c.nodeCtrlPos - 1)][k] -= 1.0; Malg[k][static_cast<std::size_t>(c.nodeCtrlPos - 1)] -= 1.0; }
            if (c.nodePos > 0) { Malg[static_cast<std::size_t>(c.nodePos - 1)][k] -= nRatio; Malg[k][static_cast<std::size_t>(c.nodePos - 1)] -= nRatio; }
            if (c.nodeNeg > 0) { Malg[static_cast<std::size_t>(c.nodeNeg - 1)][k] += nRatio; Malg[k][static_cast<std::size_t>(c.nodeNeg - 1)] += nRatio; }
        } else if (c.kind == ComponentKind::Switch || c.kind == ComponentKind::PushButton) {
            addG(c.nodePos, c.nodeNeg, (c.auxValue > 0.5) ? 1e6 : 1e-9);
        } else if (c.kind == ComponentKind::Wire || c.kind == ComponentKind::Connector || c.kind == ComponentKind::Fuse) {
            addG(c.nodePos, c.nodeNeg, 1e6);
        } else if (c.kind == ComponentKind::Ground) {
            addG(c.nodePos, 0, 1e6);
        }
    }

    ss.A.assign(nStates, std::vector<double>(nStates, 0.0));
    ss.B.assign(nStates, std::vector<double>(nInputs, 0.0));
    ss.C.assign(1, std::vector<double>(nStates, 0.0));
    ss.D.assign(1, std::vector<double>(nInputs, 0.0));

    auto solveForRhs = [&](const std::vector<double>& rhsReal, std::vector<double>& solOut) -> bool {
        auto Acopy = Malg;
        std::vector<std::complex<double>> b(dim);
        for (std::size_t r = 0; r < dim; ++r) {
            double maxRow = 0.0;
            for (std::size_t c = 0; c < dim; ++c) maxRow = (std::max)(maxRow, std::abs(Acopy[r][c]));
            const double scale = (maxRow > 0.0 && std::isfinite(maxRow)) ? (1.0 / maxRow) : 1.0;
            for (std::size_t c = 0; c < dim; ++c) Acopy[r][c] *= scale;
            b[r] = { rhsReal[r] * scale, 0.0 };
        }
        if (!Detail::SolveComplexSystemSymbolic(Acopy, b)) return false;
        solOut.resize(dim);
        for (std::size_t r = 0; r < dim; ++r) solOut[r] = b[r].real();
        return true;
    };

    auto extractDerivs = [&](const std::vector<double>& sol, std::vector<double>& dx, double& yOut) {
        dx.assign(nStates, 0.0);
        for (std::size_t ci = 0; ci < capIdx.size(); ++ci) {
            const std::size_t compI = capIdx[ci];
            const double iC = sol[static_cast<std::size_t>(maxNode + auxVars[compI])];
            dx[ci] = iC / (std::max)(1e-15, std::abs(components[compI].value));
        }
        for (std::size_t li = 0; li < indIdx.size(); ++li) {
            const std::size_t compI = indIdx[li];
            const auto& c = components[compI];
            const double vp = (c.nodePos > 0) ? sol[static_cast<std::size_t>(c.nodePos - 1)] : 0.0;
            const double vn = (c.nodeNeg > 0) ? sol[static_cast<std::size_t>(c.nodeNeg - 1)] : 0.0;
            dx[capIdx.size() + li] = (vp - vn) / (std::max)(1e-12, std::abs(c.value));
        }
        yOut = sol[static_cast<std::size_t>(outputNode - 1)];
    };

    for (std::size_t col = 0; col < nStates; ++col) {
        std::vector<double> rhs(dim, 0.0);
        if (col < capIdx.size()) {
            const std::size_t compI = capIdx[col];
            rhs[static_cast<std::size_t>(maxNode + auxVars[compI])] = 1.0;
        } else {
            const auto& c = components[indIdx[col - capIdx.size()]];
            if (c.nodePos > 0) rhs[static_cast<std::size_t>(c.nodePos - 1)] -= 1.0;
            if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] += 1.0;
        }
        std::vector<double> sol;
        if (!solveForRhs(rhs, sol)) {
            ss.diagnosticUtf8 = "Improper reactive network (capacitor-Vsource loop or inductor cutset).";
            return ss;
        }
        std::vector<double> dx;
        double y = 0.0;
        extractDerivs(sol, dx, y);
        for (std::size_t r = 0; r < nStates; ++r) ss.A[r][col] = dx[r];
        ss.C[0][col] = y;
    }

    for (std::size_t col = 0; col < nInputs; ++col) {
        const std::size_t compI = inSrcIdx[col];
        const auto& c = components[compI];
        std::vector<double> rhs(dim, 0.0);
        if (c.kind == ComponentKind::CurrentSource) {
            if (c.nodePos > 0) rhs[static_cast<std::size_t>(c.nodePos - 1)] -= 1.0;
            if (c.nodeNeg > 0) rhs[static_cast<std::size_t>(c.nodeNeg - 1)] += 1.0;
        } else {
            rhs[static_cast<std::size_t>(maxNode + auxVars[compI])] = 1.0;
        }
        std::vector<double> sol;
        if (!solveForRhs(rhs, sol)) {
            ss.diagnosticUtf8 = "Singular algebraic MNA during input excitation.";
            return ss;
        }
        std::vector<double> dx;
        double y = 0.0;
        extractDerivs(sol, dx, y);
        for (std::size_t r = 0; r < nStates; ++r) ss.B[r][col] = dx[r];
        ss.D[0][col] = y;
    }

    // Format LaTeX state vector & matrices
    auto fmtMat = [](const std::vector<std::vector<double>>& mat) -> std::string {
        if (mat.empty() || mat[0].empty()) return "\\begin{bmatrix}\\end{bmatrix}";
        std::ostringstream oss;
        oss.precision(4);
        oss << "\\begin{bmatrix} ";
        for (std::size_t r = 0; r < mat.size(); ++r) {
            if (r > 0) oss << " \\\\ ";
            for (std::size_t c = 0; c < mat[r].size(); ++c) {
                if (c > 0) oss << " & ";
                const double val = (std::abs(mat[r][c]) < 1e-12 ? 0.0 : mat[r][c]);
                oss << Detail::FormatLatexNumber(val);
            }
        }
        oss << " \\end{bmatrix}";
        return oss.str();
    };

    {
        std::ostringstream xv;
        xv << "\\mathbf{x} = \\begin{bmatrix} ";
        for (std::size_t i = 0; i < ss.stateNames.size(); ++i) {
            if (i > 0) xv << " \\\\ ";
            xv << ss.stateNames[i];
        }
        xv << " \\end{bmatrix}";
        ss.stateVectorLatex = xv.str();
    }
    ss.equationsLatex = "\\dot{\\mathbf{x}} = " + fmtMat(ss.A) + "\\mathbf{x} + " + fmtMat(ss.B) + "\\mathbf{u},\\quad "
                        "\\mathbf{y} = " + fmtMat(ss.C) + "\\mathbf{x} + " + fmtMat(ss.D) + "\\mathbf{u}";
    ss.ok = true;
    return ss;
}

// -------- Comprehensive Full Symbolic Circuit Analysis (F-240, F-250, SUP-04) --------

inline FullSymbolicCircuitReport AnalyzeFullSymbolicCircuit(
    const std::vector<CircuitComponent>& components,
    int outputNode = 1,
    double acFreqHz = 60.0,
    const std::vector<InitialCondition>& manualIcs = {},
    const std::vector<SwitchEvent>& switches = {},
    double bodeFStartHz = 1.0,
    double bodeFEndHz = 1e6,
    std::size_t bodePoints = 41)
{
    FullSymbolicCircuitReport rep;
    if (components.empty()) {
        rep.errorUtf8 = "Circuito vacio.";
        return rep;
    }

    // Apply t=0^- switch state first if switch transitions exist to resolve ICs
    std::vector<CircuitComponent> preT0Comps = components;
    std::vector<CircuitComponent> postT0Comps = components;
    std::map<std::string, double> capIcV;
    std::map<std::string, double> indIcI;

    if (!switches.empty()) {
        for (const auto& sw : switches) {
            for (auto& c : preT0Comps) {
                if (c.name == sw.switchName) c.auxValue = sw.initiallyClosed ? 1.0 : 0.0;
            }
            for (auto& c : postT0Comps) {
                if (c.name == sw.switchName) c.auxValue = sw.closedAfterT0 ? 1.0 : 0.0;
            }
        }
        const CircuitSolveResult dcPre = SolveMnaCircuit(preT0Comps, 0.0);
        if (dcPre.ok) {
            std::map<std::string, BranchResult> preBranchByName;
            for (const auto& br : dcPre.branches) preBranchByName[br.name] = br;
            for (std::size_t i = 0; i < preT0Comps.size(); ++i) {
                const auto& c = preT0Comps[i];
                if (!c.active) continue;
                if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
                    const double vp = dcPre.nodeVoltages.count(c.nodePos) ? dcPre.nodeVoltages.at(c.nodePos).real() : 0.0;
                    const double vn = dcPre.nodeVoltages.count(c.nodeNeg) ? dcPre.nodeVoltages.at(c.nodeNeg).real() : 0.0;
                    const double vc = vp - vn;
                    capIcV[c.name] = vc;
                    rep.resolvedInitialConditions.push_back({ c.name, vc });
                } else if (c.kind == ComponentKind::Inductor) {
                    auto itBr = preBranchByName.find(c.name);
                    if (itBr != preBranchByName.end()) {
                        const double il = itBr->second.currentA.real();
                        indIcI[c.name] = il;
                        rep.resolvedInitialConditions.push_back({ c.name, il });
                    }
                }
            }
        }
    }
    for (const auto& ic : manualIcs) {
        for (const auto& c : postT0Comps) {
            if (!c.active) continue;
            if (c.name == ic.componentName) {
                if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) capIcV[c.name] = ic.value;
                else if (c.kind == ComponentKind::Inductor) indIcI[c.name] = ic.value;
            }
        }
        bool updated = false;
        for (auto& existing : rep.resolvedInitialConditions) {
            if (existing.componentName == ic.componentName) {
                existing.value = ic.value;
                updated = true;
                break;
            }
        }
        if (!updated) rep.resolvedInitialConditions.push_back(ic);
    }

    // 1. Build Matrix Pencil for both exact response (with ICs) and unit-source H(s)
    Detail::MatrixPencil mpResponse;
    if (!Detail::BuildLinearMatrixPencil(postT0Comps, capIcV, indIcI, false, mpResponse)) {
        rep.errorUtf8 = "No hay nodos no-referencia (>0).";
        return rep;
    }
    const int outN = std::clamp(outputNode, 1, mpResponse.maxNode);

    // Format MNA matrix LaTeX: M(s) x(s) = b(s)
    {
        std::ostringstream mat;
        mat.precision(4);
        mat << "\\begin{bmatrix} ";
        for (std::size_t r = 0; r < mpResponse.dim; ++r) {
            if (r > 0) mat << " \\\\ ";
            for (std::size_t col = 0; col < mpResponse.dim; ++col) {
                if (col > 0) mat << " & ";
                const double m0 = (std::abs(mpResponse.M0[r][col]) < 1e-11) ? 0.0 : mpResponse.M0[r][col];
                const double m1 = (std::abs(mpResponse.M1[r][col]) < 1e-11) ? 0.0 : mpResponse.M1[r][col];
                if (m1 == 0.0) mat << Detail::FormatLatexNumber(m0);
                else if (m0 == 0.0) mat << Detail::FormatLatexNumber(m1) << "s";
                else mat << Detail::FormatLatexNumber(m0) << (m1 >= 0 ? " + " : " - ") << Detail::FormatLatexNumber(std::abs(m1)) << "s";
            }
        }
        mat << " \\end{bmatrix} \\begin{bmatrix} ";
        for (std::size_t r = 0; r < mpResponse.unknownNames.size(); ++r) {
            if (r > 0) mat << " \\\\ ";
            mat << mpResponse.unknownNames[r];
        }
        mat << " \\end{bmatrix} = \\begin{bmatrix} ";
        for (std::size_t r = 0; r < mpResponse.dim; ++r) {
            if (r > 0) mat << " \\\\ ";
            const double bVal = (std::abs(mpResponse.b0[r]) < 1e-12 ? 0.0 : mpResponse.b0[r]);
            mat << Detail::FormatLatexNumber(bVal);
        }
        mat << " \\end{bmatrix}";
        rep.mnaMatrixLatex = mat.str();
    }

    // Symbolic Nodal KCL equations (with OpAmp nodeOut fix!)
    auto nodeTermLatex = [](int a, int b) -> std::string {
        const std::string va = (a > 0) ? ("V_{" + std::to_string(a) + "}(s)") : "0";
        const std::string vb = (b > 0) ? ("V_{" + std::to_string(b) + "}(s)") : "0";
        if (b == 0) return va;
        if (a == 0) return "-" + vb;
        return "(" + va + " - " + vb + ")";
    };
    for (int n = 1; n <= mpResponse.maxNode; ++n) {
        std::ostringstream eq;
        bool first = true;
        for (const auto& c : postT0Comps) {
            if (!c.active) continue;
            if (c.kind == ComponentKind::OpAmp) {
                if (c.nodeOut == n) {
                    if (!first) eq << " + ";
                    first = false;
                    eq << "I_{" << c.name << ",\\text{out}}(s)";
                }
                continue;
            }
            if (c.nodePos != n && c.nodeNeg != n) continue;
            const int other = (c.nodePos == n) ? c.nodeNeg : c.nodePos;
            const double sgn = (c.nodePos == n) ? 1.0 : -1.0;
            if (c.kind == ComponentKind::Resistor) {
                if (!first) eq << " + ";
                first = false;
                eq << "\\frac{" << nodeTermLatex(n, other) << "}{" << c.name << "}";
            } else if (c.kind == ComponentKind::Capacitor || c.kind == ComponentKind::PolarizedCapacitor) {
                if (!first) eq << " + ";
                first = false;
                eq << "s " << c.name << " " << nodeTermLatex(n, other);
                if (capIcV.count(c.name) && std::abs(capIcV[c.name]) > 1e-12) {
                    const double v0 = capIcV[c.name];
                    eq << (sgn > 0 ? " - " : " + ") << c.name << " (" << Detail::FormatLatexNumber(v0) << ")";
                }
            } else if (c.kind == ComponentKind::Inductor) {
                if (!first) eq << " + ";
                first = false;
                eq << "\\frac{" << nodeTermLatex(n, other) << "}{s " << c.name << "}";
                if (indIcI.count(c.name) && std::abs(indIcI[c.name]) > 1e-12) {
                    eq << (sgn > 0 ? " + " : " - ") << "\\frac{" << Detail::FormatLatexNumber(indIcI[c.name]) << "}{s}";
                }
            } else if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::Battery ||
                       c.kind == ComponentKind::AcSource || c.kind == ComponentKind::Vcc) {
                if (!first) eq << " + ";
                first = false;
                eq << (sgn > 0 ? "+" : "-") << "I_{" << c.name << "}(s)";
            } else if (c.kind == ComponentKind::CurrentSource) {
                if (!first) eq << " + ";
                first = false;
                eq << (sgn > 0 ? "+" : "-") << c.name << "(s)";
            } else if (c.kind == ComponentKind::Diode || c.kind == ComponentKind::Led ||
                       c.kind == ComponentKind::ZenerDiode || c.kind == ComponentKind::SchottkyDiode) {
                if (!first) eq << " + ";
                first = false;
                eq << "g_{" << c.name << "} " << nodeTermLatex(n, other);
            }
        }
        if (first) eq << "0";
        eq << " = 0";
        rep.nodalKclLatex.push_back(eq.str());
        rep.laplaceIcEquationsLatex.push_back(eq.str());
    }
    for (const auto& c : postT0Comps) {
        if (c.active && c.kind == ComponentKind::OpAmp) {
            const std::string cstr = nodeTermLatex(c.nodePos, c.nodeNeg) + " = 0";
            rep.nodalKclLatex.push_back(cstr);
            rep.laplaceIcEquationsLatex.push_back(cstr);
        }
    }

    // 2. Mesh (KVL) Fundamental Cycle Extraction (Spanning tree + Supermesh for shared current sources)
    {
        struct GraphEdge {
            std::size_t compIdx;
            int u, v;
        };
        struct LoopEdge {
            std::size_t compIdx;
            int dirSign; // +1 if traversed nodePos -> nodeNeg, -1 if nodeNeg -> nodePos
        };
        struct MeshCycle {
            bool isCurrentSourceLoop = false;
            std::size_t curSourceCompIdx = 0;
            std::vector<LoopEdge> edges;
        };

        std::vector<GraphEdge> treeCandidates;
        std::vector<std::size_t> curSources;
        for (std::size_t i = 0; i < postT0Comps.size(); ++i) {
            const auto& c = postT0Comps[i];
            if (!c.active) continue;
            if (c.kind == ComponentKind::CurrentSource) {
                curSources.push_back(i);
            } else if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
                       c.kind == ComponentKind::Ground || c.kind == ComponentKind::VoltageProbe ||
                       c.kind == ComponentKind::Voltmeter || c.kind == ComponentKind::Oscilloscope ||
                       c.kind == ComponentKind::Vccs) {
                // Ideal/buffered open-circuit terminals or 1-pin reference do not form mesh edges
                continue;
            } else {
                const int nNeg = (c.kind == ComponentKind::Vcc) ? 0 : c.nodeNeg;
                if (c.nodePos != nNeg && c.nodePos >= 0 && nNeg >= 0) {
                    treeCandidates.push_back({ i, c.nodePos, nNeg });
                }
            }
        }

        // Union-Find to find spanning forest and chord edges (fundamental meshes)
        std::vector<int> dsuP(static_cast<std::size_t>(mpResponse.maxNode + 1));
        for (int n = 0; n <= mpResponse.maxNode; ++n) dsuP[static_cast<std::size_t>(n)] = n;
        auto findR = [&](int x) {
            while (dsuP[static_cast<std::size_t>(x)] != x) x = dsuP[static_cast<std::size_t>(x)];
            return x;
        };

        std::vector<GraphEdge> treeEdges;
        std::vector<GraphEdge> chordEdges;
        for (const auto& e : treeCandidates) {
            const int ru = findR(e.u);
            const int rv = findR(e.v);
            if (ru != rv) {
                dsuP[static_cast<std::size_t>(rv)] = ru;
                treeEdges.push_back(e);
            } else {
                chordEdges.push_back(e);
            }
        }

        auto findTreeCycle = [&](std::size_t chordCompIdx, int u, int v, std::vector<LoopEdge>& outLoop) -> bool {
            if (u == v) return false;
            std::vector<int> parentNode(static_cast<std::size_t>(mpResponse.maxNode + 1), -1);
            std::vector<int> parentEdge(static_cast<std::size_t>(mpResponse.maxNode + 1), -1);
            std::queue<int> q;
            q.push(v);
            parentNode[static_cast<std::size_t>(v)] = v;
            while (!q.empty()) {
                const int cur = q.front();
                q.pop();
                if (cur == u) break;
                for (std::size_t ei = 0; ei < treeEdges.size(); ++ei) {
                    const auto& te = treeEdges[ei];
                    int nxt = -1;
                    if (te.u == cur) nxt = te.v;
                    else if (te.v == cur) nxt = te.u;
                    if (nxt >= 0 && parentNode[static_cast<std::size_t>(nxt)] < 0) {
                        parentNode[static_cast<std::size_t>(nxt)] = cur;
                        parentEdge[static_cast<std::size_t>(nxt)] = static_cast<int>(ei);
                        q.push(nxt);
                    }
                }
            }
            if (parentNode[static_cast<std::size_t>(u)] < 0) return false;

            outLoop.clear();
            outLoop.push_back({ chordCompIdx, +1 }); // traversed u -> v (nodePos -> nodeNeg)
            std::vector<LoopEdge> revPath;
            int walk = u;
            while (walk != v && parentEdge[static_cast<std::size_t>(walk)] >= 0) {
                const int ei = parentEdge[static_cast<std::size_t>(walk)];
                const int prev = parentNode[static_cast<std::size_t>(walk)];
                const auto& te = treeEdges[static_cast<std::size_t>(ei)];
                // Loop moves prev -> walk along the tree path from v to u
                const int dir = (te.u == prev && te.v == walk) ? +1 : -1;
                revPath.push_back({ te.compIdx, dir });
                walk = prev;
            }
            std::reverse(revPath.begin(), revPath.end());
            outLoop.insert(outLoop.end(), revPath.begin(), revPath.end());
            return true;
        };

        std::vector<MeshCycle> meshes;
        for (const auto& chord : chordEdges) {
            std::vector<LoopEdge> loop;
            if (findTreeCycle(chord.compIdx, chord.u, chord.v, loop)) {
                meshes.push_back({ false, 0, std::move(loop) });
            }
        }
        for (std::size_t ci : curSources) {
            const auto& cs = postT0Comps[ci];
            if (cs.nodePos != cs.nodeNeg && cs.nodePos >= 0 && cs.nodeNeg >= 0) {
                std::vector<LoopEdge> loop;
                if (findTreeCycle(ci, cs.nodePos, cs.nodeNeg, loop)) {
                    meshes.push_back({ true, ci, std::move(loop) });
                }
            }
        }

        // Map each component edge to all meshes traversing it and their orientation signs
        std::map<std::size_t, std::vector<std::pair<std::size_t, int>>> edgeToMeshes;
        for (std::size_t mIdx = 0; mIdx < meshes.size(); ++mIdx) {
            for (const auto& le : meshes[mIdx].edges) {
                edgeToMeshes[le.compIdx].push_back({ mIdx, le.dirSign });
            }
        }

        auto formatMeshCurrentExpr = [&](std::size_t compIdx, std::size_t selfMesh, int selfSign) -> std::string {
            const auto& hits = edgeToMeshes[compIdx];
            if (hits.size() <= 1) {
                return "I_{m" + std::to_string(selfMesh + 1) + "}";
            }
            std::ostringstream cs;
            cs << "(I_{m" << (selfMesh + 1) << "}";
            for (const auto& h : hits) {
                if (h.first == selfMesh) continue;
                const int rel = selfSign * h.second;
                cs << (rel >= 0 ? " + " : " - ") << "I_{m" << (h.first + 1) << "}";
            }
            cs << ")";
            return cs.str();
        };

        for (std::size_t mIdx = 0; mIdx < meshes.size(); ++mIdx) {
            const auto& mesh = meshes[mIdx];
            std::string desc = "M" + std::to_string(mIdx + 1) + ": [";
            for (std::size_t k = 0; k < mesh.edges.size(); ++k) {
                if (k > 0) desc += " -> ";
                desc += postT0Comps[mesh.edges[k].compIdx].name;
            }
            desc += "]";
            rep.meshLoopsDesc.push_back(std::move(desc));

            if (mesh.isCurrentSourceLoop) {
                const auto& cs = postT0Comps[mesh.curSourceCompIdx];
                bool sharesBranchWithOtherMesh = false;
                if (meshes.size() >= 2) {
                    for (const auto& le : mesh.edges) {
                        if (edgeToMeshes[le.compIdx].size() > 1) {
                            sharesBranchWithOtherMesh = true;
                            break;
                        }
                    }
                }
                if (sharesBranchWithOtherMesh) {
                    // Find another mesh sharing a branch so supermesh coupling references real mesh indices
                    std::size_t coupledMesh = mIdx;
                    for (const auto& le : mesh.edges) {
                        for (const auto& h : edgeToMeshes[le.compIdx]) {
                            if (h.first != mIdx) { coupledMesh = h.first; break; }
                        }
                        if (coupledMesh != mIdx) break;
                    }
                    rep.meshKvlEquationsLatex.push_back(
                        "\\text{Malla " + std::to_string(mIdx + 1) + " (" + cs.name + ", acoplada con M" +
                        std::to_string(coupledMesh + 1) + "): }I_{m" + std::to_string(mIdx + 1) + "} = " + cs.name);
                } else {
                    rep.meshKvlEquationsLatex.push_back(
                        "\\text{Malla " + std::to_string(mIdx + 1) + " (" + cs.name + "): }I_{m" + std::to_string(mIdx + 1) + "} = " + cs.name);
                }
                continue;
            }

            std::ostringstream kvl;
            kvl << "\\text{Malla " << (mIdx + 1) << ": }";
            bool firstTerm = true;
            for (const auto& le : mesh.edges) {
                const auto& comp = postT0Comps[le.compIdx];
                const std::string iExpr = formatMeshCurrentExpr(le.compIdx, mIdx, le.dirSign);
                if (comp.kind == ComponentKind::Resistor) {
                    if (!firstTerm) kvl << " + ";
                    firstTerm = false;
                    kvl << comp.name << " " << iExpr;
                } else if (comp.kind == ComponentKind::Capacitor || comp.kind == ComponentKind::PolarizedCapacitor) {
                    if (!firstTerm) kvl << " + ";
                    firstTerm = false;
                    kvl << "\\frac{" << iExpr << "}{s " << comp.name << "}";
                } else if (comp.kind == ComponentKind::Inductor) {
                    if (!firstTerm) kvl << " + ";
                    firstTerm = false;
                    kvl << "s " << comp.name << " " << iExpr;
                } else if (comp.kind == ComponentKind::VoltageSource || comp.kind == ComponentKind::Battery ||
                           comp.kind == ComponentKind::AcSource || comp.kind == ComponentKind::Vcc ||
                           comp.kind == ComponentKind::PulseSource) {
                    if (!firstTerm) kvl << (le.dirSign >= 0 ? " + " : " - ");
                    else if (le.dirSign < 0) kvl << "-";
                    firstTerm = false;
                    kvl << "V_{" << comp.name << "}";
                } else {
                    if (!firstTerm) kvl << (le.dirSign >= 0 ? " + " : " - ");
                    else if (le.dirSign < 0) kvl << "-";
                    firstTerm = false;
                    kvl << "V_{" << comp.name << "}";
                }
            }
            if (firstTerm) kvl << "0";
            kvl << " = 0";
            rep.meshKvlEquationsLatex.push_back(kvl.str());
        }
        rep.meshMatrixLatex = "\\mathbf{Z}_m(s)\\mathbf{I}_m(s) = \\mathbf{V}_m(s)\\quad (\\text{dim}=" +
                              std::to_string(rep.meshKvlEquationsLatex.size()) + ")";
    }

    // 3. DC Superposition per independent V/I source
    {
        std::vector<std::size_t> srcIndices;
        std::vector<std::size_t> allZeroableSources;
        for (std::size_t i = 0; i < postT0Comps.size(); ++i) {
            const auto& c = postT0Comps[i];
            if (!c.active) continue;
            if (c.kind == ComponentKind::VoltageSource || c.kind == ComponentKind::CurrentSource ||
                c.kind == ComponentKind::Battery || c.kind == ComponentKind::Vcc ||
                c.kind == ComponentKind::PulseSource || c.kind == ComponentKind::AcSource) {
                allZeroableSources.push_back(i);
                const bool isAcOnlyZeroDc = (c.kind == ComponentKind::AcSource && std::abs(c.dcOffset) <= 1e-12);
                if (!isAcOnlyZeroDc) {
                    srcIndices.push_back(i);
                }
            }
        }
        double totalOutV = 0.0;
        for (std::size_t sIdx : srcIndices) {
            std::vector<CircuitComponent> singleComps = postT0Comps;
            for (std::size_t other : allZeroableSources) {
                if (other != sIdx) {
                    singleComps[other].value = 0.0;
                    singleComps[other].dcOffset = 0.0;
                }
            }
            const CircuitSolveResult sr = SolveMnaCircuit(singleComps, 0.0);
            if (sr.ok) {
                FullSymbolicCircuitReport::SuperpositionTerm term;
                term.sourceName = postT0Comps[sIdx].name;
                for (const auto& kv : sr.nodeVoltages) {
                    term.nodeContributionsV[kv.first] = kv.second.real();
                }
                const double vo = sr.nodeVoltages.count(outN) ? sr.nodeVoltages.at(outN).real() : 0.0;
                totalOutV += vo;
                term.latexSummary = "V_{" + std::to_string(outN) + "}^{(" + term.sourceName + ")} = " +
                                    Detail::FormatLatexNumber(vo) + "\\text{ V}";
                rep.dcSuperposition.push_back(std::move(term));
            }
        }
        rep.dcSuperpositionTotalLatex = "V_{" + std::to_string(outN) + "}^{\\text{total}} = " +
                                        Detail::FormatLatexNumber(totalOutV) + "\\text{ V}";
    }

    // 4. AC Phasor Analysis at acFreqHz
    if (acFreqHz > 0.0) {
        rep.acFreqHz = acFreqHz;
        rep.omega0RadPerSec = 2.0 * kPi * acFreqHz;
        const CircuitSolveResult acSol = SolveMnaCircuit(postT0Comps, acFreqHz);
        if (acSol.ok) {
            rep.acNodePhasors = acSol.nodeVoltages;
            for (const auto& kv : acSol.nodeVoltages) {
                double mag = std::abs(kv.second);
                double angDeg = std::arg(kv.second) * 180.0 / kPi;
                if (mag < 1e-12) {
                    mag = 0.0;
                    angDeg = 0.0;
                } else if (std::abs(angDeg) < 1e-12) {
                    angDeg = 0.0;
                }
                const std::string angStr = Detail::FormatLatexNumber(angDeg);
                std::ostringstream oss;
                oss << "\\tilde{V}_{" << kv.first << "} = " << Detail::FormatLatexNumber(mag) << "\\angle ";
                if (angStr.find('^') != std::string::npos) {
                    oss << "\\left(" << angStr << "\\right)^\\circ\\text{ V}";
                } else {
                    oss << angStr << "^\\circ\\text{ V}";
                }
                rep.acNodePhasorsLatex.push_back(oss.str());
            }
            rep.acPhasorMatrixLatex = "\\mathbf{Y}(j\\omega_0)\\tilde{\\mathbf{V}} = \\tilde{\\mathbf{I}},\\quad \\omega_0 = " +
                                      Detail::FormatLatexNumber(rep.omega0RadPerSec) + "\\text{ rad/s}";
        }
    }

    // 5. Laplace s-domain responses with ICs
    rep.nodeVoltagesS = Detail::ExtractAllNodeRationalDft(mpResponse);
    if (rep.nodeVoltagesS.count(outN)) {
        rep.outputResponseSLatex = "V_{" + std::to_string(outN) + "}(s) = " + rep.nodeVoltagesS[outN].FormatLatex("s");
    }

    // 6. Exact Transfer Function H(s) via unit-source Matrix Pencil
    Detail::MatrixPencil mpUnit;
    if (Detail::BuildLinearMatrixPencil(postT0Comps, {}, {}, true, mpUnit)) {
        auto unitRat = Detail::ExtractAllNodeRationalDft(mpUnit);
        if (unitRat.count(outN)) {
            rep.transferFunction = unitRat[outN];
            rep.transferFunction.Normalize(true);
            rep.dcGain = rep.transferFunction.Eval({ 0.0, 0.0 }).real();
            if (rep.transferFunction.num.Degree() == 0 && rep.transferFunction.den.Degree() == 0) {
                std::ostringstream tf;
                tf.precision(4);
                tf << "H(s) = " << rep.dcGain;
                rep.transferFunctionUtf8 = tf.str();
                rep.transferFunctionLatex = "H(s) = " + Detail::FormatLatexNumber(rep.dcGain);
            } else {
                rep.transferFunctionUtf8 = "H(s) = " + rep.transferFunction.FormatUtf8("s");
                rep.transferFunctionLatex = "H(s) = " + rep.transferFunction.FormatLatex("s");
            }
        }
    }
    rep.symbolicHOfSLatex = Detail::TryBuildSymbolicTransferLatex(postT0Comps, outN);

    // 7. State-Space
    rep.stateSpace = ExtractCircuitStateSpace(postT0Comps, outN);

    // 8. Poles, Zeros, Bode, Nyquist, Root-Locus & BIBO Stability
    rep.zeros = FindPolynomialRootsAberth(rep.transferFunction.num);
    rep.poles = FindPolynomialRootsAberth(rep.transferFunction.den);
    rep.isBibostable = ComputeRouthHurwitzStable(rep.transferFunction.den);
    for (const auto& p : rep.poles) {
        if (p.real() >= 0.0) { rep.isBibostable = false; break; }
    }

    const BodeAnalysisResult bodeRes = AnalyzeBodeSweep(postT0Comps, outN, bodeFStartHz, bodeFEndHz, bodePoints);
    if (bodeRes.ok) {
        rep.bodeSweep = bodeRes.points;
        for (const auto& ep : bodeRes.extendedPoints) {
            rep.bodeUnwrappedPhaseDeg.push_back(ep.phaseUnwrappedDeg);
        }
        rep.nyquistPoints = bodeRes.nyquistPoints;
        rep.gainMarginDb = bodeRes.gainMarginDb;
        rep.phaseMarginDeg = bodeRes.phaseMarginDeg;
        rep.hasGainMargin = bodeRes.hasGainMargin;
        rep.hasPhaseMargin = bodeRes.hasPhaseMargin;
    }

    // Root-Locus for 1 + K * H(s) = 0 (D(s) + K * N(s) = 0)
    const std::vector<double> gains{ 0.0, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0 };
    for (double K : gains) {
        Poly1D charPoly = rep.transferFunction.den + (rep.transferFunction.num * Poly1D{ { K } });
        RootLocusPoint rlp;
        rlp.gainK = K;
        rlp.closedLoopPoles = FindPolynomialRootsAberth(charPoly);
        rep.rootLocusBranches.push_back(std::move(rlp));
    }

    rep.ok = true;
    return rep;
}

} // namespace DocStudio::Circuit
