#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DocStudio::Cas {

// ---------------- F-251: Locale-Aware CSV Numeric Parser ----------------

struct CsvParseResult {
    bool ok = false;
    char delimiter = ',';
    bool decimalCommaLocale = false;
    std::vector<std::string> headers;
    std::vector<std::vector<double>> rows;
    std::vector<DocPoint> xyPoints;
    std::string errorUtf8;
};

namespace detail_stat {

inline std::string Trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::vector<std::string> SplitCsvLine(const std::string& line, char delim) {
    std::vector<std::string> cells;
    std::string cur;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                cur.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == delim && !inQuotes) {
            cells.push_back(Trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    cells.push_back(Trim(cur));
    return cells;
}

inline bool ParseLocaleDouble(std::string cell, bool allowDecimalComma, double& out) {
    cell = Trim(cell);
    if (cell.empty()) return false;
    if (allowDecimalComma) {
        for (char& ch : cell) {
            if (ch == ',') ch = '.';
        }
    }
    try {
        std::size_t idx = 0;
        const double v = std::stod(cell, &idx);
        if (idx != cell.size() || !std::isfinite(v)) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

inline std::string FmtClean(double v, int prec = 5) {
    if (!std::isfinite(v)) return "NaN";
    if (std::abs(v) < 1e-12) return "0";
    std::ostringstream oss;
    oss.precision((std::max)(2, prec));
    oss << v;
    return oss.str();
}

} // namespace detail_stat

inline CsvParseResult ParseCsvNumericTable(
    const std::string& csvTextUtf8,
    std::size_t xCol = 0,
    std::size_t yCol = 1)
{
    CsvParseResult res;
    std::vector<std::string> lines;
    {
        std::istringstream iss(csvTextUtf8);
        std::string ln;
        while (std::getline(iss, ln)) {
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            ln = detail_stat::Trim(ln);
            if (!ln.empty()) lines.push_back(ln);
        }
    }
    if (lines.empty()) {
        res.errorUtf8 = "CSV vacio";
        return res;
    }

    // Detect delimiter across lines without mutating decimal commas first
    std::size_t semiCount = 0, tabCount = 0, commaCount = 0;
    for (const auto& ln : lines) {
        bool inQ = false;
        for (char c : ln) {
            if (c == '"') inQ = !inQ;
            else if (!inQ) {
                if (c == ';') ++semiCount;
                else if (c == '\t') ++tabCount;
                else if (c == ',') ++commaCount;
            }
        }
    }
    if (semiCount > 0 && semiCount >= tabCount) {
        res.delimiter = ';';
    } else if (tabCount > 0) {
        res.delimiter = '\t';
    } else {
        res.delimiter = ',';
    }

    // Detect decimal comma locale if cells contain ',' without '.'
    bool sawDecimalComma = false;
    for (std::size_t r = 0; r < lines.size(); ++r) {
        const auto cells = detail_stat::SplitCsvLine(lines[r], res.delimiter);
        if (res.delimiter != ',') {
            for (const auto& c : cells) {
                if (c.find(',') != std::string::npos && c.find('.') == std::string::npos) {
                    // Verify digits around comma
                    const auto cp = c.find(',');
                    if (cp > 0 && cp + 1 < c.size() &&
                        std::isdigit(static_cast<unsigned char>(c[cp - 1])) &&
                        std::isdigit(static_cast<unsigned char>(c[cp + 1]))) {
                        sawDecimalComma = true;
                    }
                }
            }
        } else {
            // Even with comma delimiter, quoted "0,5","1,25" indicates decimal comma
            if (lines[r].find('"') != std::string::npos) {
                for (const auto& c : cells) {
                    if (c.find(',') != std::string::npos && c.find('.') == std::string::npos) {
                        sawDecimalComma = true;
                    }
                }
            }
        }
    }
    res.decimalCommaLocale = sawDecimalComma;

    for (std::size_t r = 0; r < lines.size(); ++r) {
        const auto cells = detail_stat::SplitCsvLine(lines[r], res.delimiter);
        std::vector<double> numRow;
        numRow.reserve(cells.size());
        bool rowNumeric = !cells.empty();
        for (const auto& c : cells) {
            double v = 0.0;
            if (!detail_stat::ParseLocaleDouble(c, true, v)) {
                rowNumeric = false;
                break;
            }
            numRow.push_back(v);
        }
        if (rowNumeric) {
            if (xCol < numRow.size() && yCol < numRow.size()) {
                res.xyPoints.push_back({ numRow[xCol], numRow[yCol] });
            }
            res.rows.push_back(std::move(numRow));
        } else if (r == 0 && res.headers.empty()) {
            res.headers = cells;
        }
    }

    res.ok = !res.xyPoints.empty();
    if (!res.ok) res.errorUtf8 = "No se encontraron filas numericas validas en CSV";
    return res;
}

// ---------------- F-252: Extended Regressions + Uncertainty Bounds ----------------

enum class RegressionModelKind : std::uint8_t {
    Linear = 0,
    Polynomial = 1,
    Exponential = 2, // y = a * exp(b * x)
    Power = 3,       // y = a * x^b
    Logarithmic = 4  // y = a + b * ln(x)
};

struct ExtendedRegressionResult {
    bool ok = false;
    RegressionModelKind kind = RegressionModelKind::Linear;
    int degree = 1;
    std::vector<double> coeffs;
    double rSquared = 0.0;
    double adjRSquared = 0.0;
    double rmse = 0.0;
    std::vector<double> coeffStdErrors;
    std::string equationUtf8;
    std::string latexUtf8;
    std::vector<DocPoint> dataPoints;
    std::vector<DocPoint> fittedCurve;
    std::vector<DocPoint> lowerBound95;
    std::vector<DocPoint> upperBound95;
    std::string errorUtf8;
};

inline ExtendedRegressionResult FitCsvRegressionExtended(
    const std::string& csvTextUtf8,
    RegressionModelKind kind,
    int polyDegree = 1,
    std::size_t xCol = 0,
    std::size_t yCol = 1)
{
    ExtendedRegressionResult res;
    res.kind = kind;
    const auto parsed = ParseCsvNumericTable(csvTextUtf8, xCol, yCol);
    if (!parsed.ok) {
        res.errorUtf8 = parsed.errorUtf8;
        return res;
    }
    res.dataPoints = parsed.xyPoints;
    const std::size_t n = res.dataPoints.size();

    // Build transformed regression pairs (tx, ty)
    std::vector<double> tx;
    std::vector<double> ty;
    tx.reserve(n);
    ty.reserve(n);
    for (const auto& pt : res.dataPoints) {
        if (kind == RegressionModelKind::Exponential) {
            if (pt.y <= 0.0) { res.errorUtf8 = "Regresion exponencial requiere y > 0"; return res; }
            tx.push_back(pt.x);
            ty.push_back(std::log(pt.y));
        } else if (kind == RegressionModelKind::Power) {
            if (pt.x <= 0.0 || pt.y <= 0.0) { res.errorUtf8 = "Regresion potencia requiere x > 0 e y > 0"; return res; }
            tx.push_back(std::log(pt.x));
            ty.push_back(std::log(pt.y));
        } else if (kind == RegressionModelKind::Logarithmic) {
            if (pt.x <= 0.0) { res.errorUtf8 = "Regresion logaritmica requiere x > 0"; return res; }
            tx.push_back(std::log(pt.x));
            ty.push_back(pt.y);
        } else {
            tx.push_back(pt.x);
            ty.push_back(pt.y);
        }
    }

    const int deg = (kind == RegressionModelKind::Polynomial) ? std::clamp(polyDegree, 1, 4) : 1;
    res.degree = deg;
    const std::size_t m = static_cast<std::size_t>(deg + 1);
    if (n < m) {
        res.errorUtf8 = "Puntos insuficientes para regresion";
        return res;
    }

    // Normal equations augmented with Identity on right to get (A^T A)^-1 for coeff std errors & 95% CI
    std::vector<std::vector<double>> aug(m, std::vector<double>(m + m + 1, 0.0));
    for (std::size_t k = 0; k < n; ++k) {
        std::vector<double> xp(2 * m, 1.0);
        for (std::size_t p = 1; p < xp.size(); ++p) xp[p] = xp[p - 1] * tx[k];
        for (std::size_t r = 0; r < m; ++r) {
            for (std::size_t c = 0; c < m; ++c) aug[r][c] += xp[r + c];
            aug[r][m] += xp[r] * ty[k];
        }
    }
    for (std::size_t r = 0; r < m; ++r) aug[r][m + 1 + r] = 1.0;

    for (std::size_t col = 0; col < m; ++col) {
        std::size_t piv = col;
        for (std::size_t r = col + 1; r < m; ++r) {
            if (std::abs(aug[r][col]) > std::abs(aug[piv][col])) piv = r;
        }
        if (std::abs(aug[piv][col]) < 1e-14) {
            res.errorUtf8 = "Matriz de regresion singular";
            return res;
        }
        std::swap(aug[col], aug[piv]);
        const double inv = 1.0 / aug[col][col];
        for (std::size_t c = 0; c < aug[col].size(); ++c) aug[col][c] *= inv;
        for (std::size_t r = 0; r < m; ++r) {
            if (r == col) continue;
            const double f = aug[r][col];
            for (std::size_t c = 0; c < aug[r].size(); ++c) aug[r][c] -= f * aug[col][c];
        }
    }

    std::vector<double> beta(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) beta[i] = aug[i][m];

    auto evalTransformed = [&](double u) {
        double v = 0.0, up = 1.0;
        for (double b : beta) { v += b * up; up *= u; }
        return v;
    };

    if (kind == RegressionModelKind::Exponential || kind == RegressionModelKind::Power) {
        res.coeffs = { std::exp(beta[0]), beta[1] };
    } else {
        res.coeffs = beta;
    }

    auto evalOrig = [&](double x) {
        if (kind == RegressionModelKind::Exponential) return res.coeffs[0] * std::exp(res.coeffs[1] * x);
        if (kind == RegressionModelKind::Power) return res.coeffs[0] * std::pow(x, res.coeffs[1]);
        if (kind == RegressionModelKind::Logarithmic) return res.coeffs[0] + res.coeffs[1] * std::log(x);
        return evalTransformed(x);
    };

    double meanY = 0.0;
    for (const auto& pt : res.dataPoints) meanY += pt.y;
    meanY /= static_cast<double>(n);

    double ssTot = 0.0, ssRes = 0.0, ssTransRes = 0.0;
    double xMin = res.dataPoints[0].x, xMax = res.dataPoints[0].x;
    for (std::size_t k = 0; k < n; ++k) {
        const auto& pt = res.dataPoints[k];
        xMin = (std::min)(xMin, pt.x);
        xMax = (std::max)(xMax, pt.x);
        const double yh = evalOrig(pt.x);
        ssTot += (pt.y - meanY) * (pt.y - meanY);
        ssRes += (pt.y - yh) * (pt.y - yh);
        const double th = evalTransformed(tx[k]);
        ssTransRes += (ty[k] - th) * (ty[k] - th);
    }

    res.rSquared = (ssTot > 1e-15) ? (1.0 - ssRes / ssTot) : 1.0;
    if (n > m && ssTot > 1e-15) {
        res.adjRSquared = 1.0 - (1.0 - res.rSquared) * static_cast<double>(n - 1) / static_cast<double>(n - m);
    } else {
        res.adjRSquared = res.rSquared;
    }
    res.rmse = std::sqrt(ssRes / static_cast<double>(n));

    // Standard errors in transformed space (xtxInv = aug[i][m+1+j])
    const double dof = (n > m) ? static_cast<double>(n - m) : 1.0;
    const double s2 = ssTransRes / dof;
    res.coeffStdErrors.resize(m, 0.0);
    for (std::size_t i = 0; i < m; ++i) {
        res.coeffStdErrors[i] = std::sqrt((std::max)(0.0, s2 * aug[i][m + 1 + i]));
    }
    // 95% Student-t critical value approximation (accurate from dof=1..inf)
    const double tCrit = (dof <= 1.0) ? 12.706 : (dof <= 2.0) ? 4.303 : (dof <= 5.0) ? 2.571 : (dof <= 15.0) ? 2.131 : 1.96;

    for (std::size_t i = 0; i < 64; ++i) {
        const double x = xMin + (static_cast<double>(i) / 63.0) * (std::max)(1e-6, xMax - xMin);
        const double u = (kind == RegressionModelKind::Power || kind == RegressionModelKind::Logarithmic)
            ? std::log((std::max)(1e-9, x)) : x;
        double varU = 0.0;
        std::vector<double> phi(m, 1.0);
        for (std::size_t p = 1; p < m; ++p) phi[p] = phi[p - 1] * u;
        for (std::size_t r = 0; r < m; ++r) {
            for (std::size_t c = 0; c < m; ++c) {
                varU += phi[r] * aug[r][m + 1 + c] * phi[c];
            }
        }
        const double seTrans = std::sqrt((std::max)(0.0, s2 * varU));
        const double yHat = evalOrig(x);
        res.fittedCurve.push_back({ x, yHat });
        if (kind == RegressionModelKind::Exponential || kind == RegressionModelKind::Power) {
            const double tHat = evalTransformed(u);
            res.lowerBound95.push_back({ x, std::exp(tHat - tCrit * seTrans) });
            res.upperBound95.push_back({ x, std::exp(tHat + tCrit * seTrans) });
        } else {
            res.lowerBound95.push_back({ x, yHat - tCrit * seTrans });
            res.upperBound95.push_back({ x, yHat + tCrit * seTrans });
        }
    }

    std::ostringstream eq, lx;
    if (kind == RegressionModelKind::Exponential) {
        eq << "y = " << detail_stat::FmtClean(res.coeffs[0]) << " * exp(" << detail_stat::FmtClean(res.coeffs[1]) << "*x)";
        lx << "y = " << detail_stat::FmtClean(res.coeffs[0]) << " e^{" << detail_stat::FmtClean(res.coeffs[1]) << " x}";
    } else if (kind == RegressionModelKind::Power) {
        eq << "y = " << detail_stat::FmtClean(res.coeffs[0]) << " * x^(" << detail_stat::FmtClean(res.coeffs[1]) << ")";
        lx << "y = " << detail_stat::FmtClean(res.coeffs[0]) << " x^{" << detail_stat::FmtClean(res.coeffs[1]) << "}";
    } else if (kind == RegressionModelKind::Logarithmic) {
        eq << "y = " << detail_stat::FmtClean(res.coeffs[0]) << (res.coeffs[1] >= 0.0 ? " + " : " - ")
           << detail_stat::FmtClean(std::abs(res.coeffs[1])) << "*ln(x)";
        lx << "y = " << detail_stat::FmtClean(res.coeffs[0]) << (res.coeffs[1] >= 0.0 ? " + " : " - ")
           << detail_stat::FmtClean(std::abs(res.coeffs[1])) << "\\ln(x)";
    } else {
        eq << "y = ";
        lx << "y = ";
        for (std::size_t k = 0; k < res.coeffs.size(); ++k) {
            if (k > 0) {
                eq << (res.coeffs[k] >= 0.0 ? " + " : " - ");
                lx << (res.coeffs[k] >= 0.0 ? " + " : " - ");
            } else if (res.coeffs[k] < 0.0) {
                eq << "-";
                lx << "-";
            }
            eq << detail_stat::FmtClean(std::abs(res.coeffs[k]));
            lx << detail_stat::FmtClean(std::abs(res.coeffs[k]));
            if (k == 1) { eq << "x"; lx << "x"; }
            else if (k > 1) { eq << "x^" << k; lx << "x^{" << k << "}"; }
        }
    }
    eq << "  (R\xC2\xB2=" << detail_stat::FmtClean(res.rSquared) << ", RMSE=" << detail_stat::FmtClean(res.rmse) << ")";
    res.equationUtf8 = eq.str();
    res.latexUtf8 = lx.str();
    res.ok = true;
    return res;
}

// ---------------- F-244: 2D/3D Engineering Vectors with UnitDim ----------------

struct EngVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    UnitDim dim{};
    std::string preferredUnitUtf8;
    double unitScaleToSi = 1.0;
    bool is2D = false;
};

inline bool VecAdd(const EngVector3& a, const EngVector3& b, EngVector3& out, std::string* outErr = nullptr) {
    if (a.dim != b.dim) {
        if (outErr) *outErr = "Suma vectorial dimensionalmente incompatible";
        return false;
    }
    out = a;
    out.x = a.x + b.x;
    out.y = a.y + b.y;
    out.z = a.z + b.z;
    out.is2D = a.is2D && b.is2D;
    return true;
}

inline bool VecSub(const EngVector3& a, const EngVector3& b, EngVector3& out, std::string* outErr = nullptr) {
    if (a.dim != b.dim) {
        if (outErr) *outErr = "Resta vectorial dimensionalmente incompatible";
        return false;
    }
    out = a;
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    out.is2D = a.is2D && b.is2D;
    return true;
}

inline EngVector3 VecScale(const EngVector3& a, double k) noexcept {
    EngVector3 out = a;
    out.x *= k;
    out.y *= k;
    out.z *= k;
    return out;
}

inline QuantityValue VecDot(const EngVector3& a, const EngVector3& b) {
    QuantityValue q{};
    q.value = { a.x * b.x + a.y * b.y + a.z * b.z, 0.0 };
    q.dim = a.dim * b.dim;
    if (!a.preferredUnitUtf8.empty() && !b.preferredUnitUtf8.empty()) {
        q.preferredUnitUtf8 = a.preferredUnitUtf8 + "*" + b.preferredUnitUtf8;
        q.unitScaleToSi = a.unitScaleToSi * b.unitScaleToSi;
    }
    return q;
}

inline EngVector3 VecCross(const EngVector3& a, const EngVector3& b) {
    EngVector3 out{};
    out.x = a.y * b.z - a.z * b.y;
    out.y = a.z * b.x - a.x * b.z;
    out.z = a.x * b.y - a.y * b.x;
    out.dim = a.dim * b.dim;
    if (!a.preferredUnitUtf8.empty() && !b.preferredUnitUtf8.empty()) {
        out.preferredUnitUtf8 = a.preferredUnitUtf8 + "*" + b.preferredUnitUtf8;
        out.unitScaleToSi = a.unitScaleToSi * b.unitScaleToSi;
    }
    out.is2D = false;
    return out;
}

inline QuantityValue VecNorm(const EngVector3& a) {
    QuantityValue q{};
    q.value = { std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z), 0.0 };
    q.dim = a.dim;
    q.preferredUnitUtf8 = a.preferredUnitUtf8;
    q.unitScaleToSi = a.unitScaleToSi;
    return q;
}

inline bool VecNormalize(const EngVector3& a, EngVector3& out, std::string* outErr = nullptr) {
    const double n = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    if (n < 1e-15) {
        if (outErr) *outErr = "No se puede normalizar vector nulo";
        return false;
    }
    out.x = a.x / n;
    out.y = a.y / n;
    out.z = a.z / n;
    out.dim = {}; // unit direction vector is dimensionless
    out.preferredUnitUtf8.clear();
    out.unitScaleToSi = 1.0;
    out.is2D = a.is2D;
    return true;
}

inline bool VecAngleDeg(const EngVector3& a, const EngVector3& b, double& outDeg, std::string* outErr = nullptr) {
    const double na = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    const double nb = std::sqrt(b.x * b.x + b.y * b.y + b.z * b.z);
    if (na < 1e-15 || nb < 1e-15) {
        if (outErr) *outErr = "Angulo indefinido para vector nulo";
        return false;
    }
    const double cosT = std::clamp((a.x * b.x + a.y * b.y + a.z * b.z) / (na * nb), -1.0, 1.0);
    constexpr double kPi = 3.14159265358979323846;
    outDeg = std::acos(cosT) * (180.0 / kPi);
    return true;
}

inline bool VecProjectOnto(const EngVector3& a, const EngVector3& onto, EngVector3& out, std::string* outErr = nullptr) {
    const double b2 = onto.x * onto.x + onto.y * onto.y + onto.z * onto.z;
    if (b2 < 1e-15) {
        if (outErr) *outErr = "Proyeccion sobre vector nulo";
        return false;
    }
    const double scale = (a.x * onto.x + a.y * onto.y + a.z * onto.z) / b2;
    out = a;
    out.x = scale * onto.x;
    out.y = scale * onto.y;
    out.z = scale * onto.z;
    return true;
}

inline bool VerifyRightHandFrame(const EngVector3& e1, const EngVector3& e2, const EngVector3& e3, double tol = 1e-6) {
    auto norm2 = [](const EngVector3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; };
    if (std::abs(norm2(e1) - 1.0) > tol || std::abs(norm2(e2) - 1.0) > tol || std::abs(norm2(e3) - 1.0) > tol) {
        return false;
    }
    auto dot = [](const EngVector3& a, const EngVector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    if (std::abs(dot(e1, e2)) > tol || std::abs(dot(e2, e3)) > tol || std::abs(dot(e1, e3)) > tol) {
        return false;
    }
    const EngVector3 c = VecCross(e1, e2);
    const double triple = dot(c, e3);
    return triple > (1.0 - tol);
}

inline std::string VecFormatUtf8(const EngVector3& v) {
    const double s = (v.unitScaleToSi != 0.0) ? v.unitScaleToSi : 1.0;
    std::ostringstream oss;
    oss << "<" << detail_stat::FmtClean(v.x / s) << ", " << detail_stat::FmtClean(v.y / s);
    if (!v.is2D) oss << ", " << detail_stat::FmtClean(v.z / s);
    oss << ">";
    const std::string u = !v.preferredUnitUtf8.empty() ? v.preferredUnitUtf8 : v.dim.FormatSiString();
    if (!u.empty()) oss << " " << u;
    return oss.str();
}

inline std::string VecFormatLatex(const EngVector3& v) {
    const double s = (v.unitScaleToSi != 0.0) ? v.unitScaleToSi : 1.0;
    std::ostringstream oss;
    oss << "\\begin{bmatrix} " << detail_stat::FmtClean(v.x / s) << " \\\\ " << detail_stat::FmtClean(v.y / s);
    if (!v.is2D) oss << " \\\\ " << detail_stat::FmtClean(v.z / s);
    oss << " \\end{bmatrix}";
    const std::string u = !v.preferredUnitUtf8.empty() ? v.preferredUnitUtf8 : v.dim.FormatSiString();
    if (!u.empty()) oss << "\\,\\mathrm{" << u << "}";
    return oss.str();
}

// ---------------- F-243: Editable Free Body Diagram (FBD) & Equilibrium Solver ----------------

enum class FbdSupportKind : std::uint8_t {
    Pin = 0,
    RollerX = 1,
    RollerY = 2,
    Fixed = 3
};

struct FbdForce {
    std::string id;
    std::string label;
    double x = 0.0;
    double y = 0.0;
    double mag = 0.0;
    double angleDeg = 0.0;
    bool unknownMag = false;
    std::string unitUtf8 = "N";
};

struct FbdMoment {
    std::string id;
    std::string label;
    double x = 0.0;
    double y = 0.0;
    double momentNm = 0.0;
    bool unknown = false;
};

struct FbdSupport {
    std::string id;
    std::string label;
    FbdSupportKind kind = FbdSupportKind::Pin;
    double x = 0.0;
    double y = 0.0;
    double rollerNormalAngleDeg = 90.0;
};

struct FbdDistributedLoad {
    std::string id;
    std::string label;
    double x0 = 0.0, y0 = 0.0;
    double x1 = 1.0, y1 = 0.0;
    double w0NPerM = 0.0;
    double w1NPerM = 0.0;
    double angleDeg = -90.0;
};

struct FbdModel {
    std::string title;
    double pivotX = 0.0;
    double pivotY = 0.0;
    std::vector<FbdForce> forces;
    std::vector<FbdMoment> moments;
    std::vector<FbdSupport> supports;
    std::vector<FbdDistributedLoad> distLoads;
};

struct FbdSolveResult {
    bool ok = false;
    bool inEquilibrium = false;
    std::string determinacyStatus; // "DETERMINATE", "INDETERMINATE", "UNSTABLE", "VERIFIED"
    std::vector<std::pair<std::string, double>> reactions;
    double netFx = 0.0;
    double netFy = 0.0;
    double netMoment = 0.0;
    std::string stepsTraceUtf8;
    std::string errorUtf8;
};

inline FbdSolveResult SolveFbdEquilibrium(const FbdModel& model) {
    FbdSolveResult res;
    constexpr double kPi = 3.14159265358979323846;

    double knownFx = 0.0, knownFy = 0.0, knownM = 0.0;
    struct UnknownCol {
        std::string name;
        double cFx = 0.0;
        double cFy = 0.0;
        double cM = 0.0;
    };
    std::vector<UnknownCol> unknowns;

    for (const auto& f : model.forces) {
        const double rad = f.angleDeg * kPi / 180.0;
        const double ux = std::cos(rad), uy = std::sin(rad);
        const double rx = f.x - model.pivotX, ry = f.y - model.pivotY;
        const double um = rx * uy - ry * ux;
        if (f.unknownMag) {
            unknowns.push_back({ f.label.empty() ? f.id : f.label, ux, uy, um });
        } else {
            knownFx += f.mag * ux;
            knownFy += f.mag * uy;
            knownM += f.mag * um;
        }
    }

    for (const auto& dl : model.distLoads) {
        const double dx = dl.x1 - dl.x0, dy = dl.y1 - dl.y0;
        const double L = std::hypot(dx, dy);
        if (L < 1e-12) continue;
        // Exact integration of linear distributed load w(t) = w0*(1-t) + w1*t, t in [0, 1]:
        // F = L * (w0 + w1)/2, R_eff = L * [((w0 + w1)/2)*(r0 - pivot) + ((w0 + 2*w1)/6)*dr]
        // Handles antisymmetric pure-couple loads (w0 + w1 == 0) cleanly without division by wSum.
        const double wSum = dl.w0NPerM + dl.w1NPerM;
        const double F = 0.5 * wSum * L;
        const double rad = dl.angleDeg * kPi / 180.0;
        const double ux = std::cos(rad), uy = std::sin(rad);
        const double fx = F * ux, fy = F * uy;
        const double rxEff = L * (0.5 * wSum * (dl.x0 - model.pivotX) + ((dl.w0NPerM + 2.0 * dl.w1NPerM) / 6.0) * dx);
        const double ryEff = L * (0.5 * wSum * (dl.y0 - model.pivotY) + ((dl.w0NPerM + 2.0 * dl.w1NPerM) / 6.0) * dy);
        knownFx += fx;
        knownFy += fy;
        knownM += rxEff * uy - ryEff * ux;
    }

    for (const auto& m : model.moments) {
        if (m.unknown) {
            unknowns.push_back({ m.label.empty() ? m.id : m.label, 0.0, 0.0, 1.0 });
        } else {
            knownM += m.momentNm;
        }
    }

    for (const auto& s : model.supports) {
        const std::string base = s.label.empty() ? s.id : s.label;
        const double rx = s.x - model.pivotX, ry = s.y - model.pivotY;
        if (s.kind == FbdSupportKind::Pin) {
            unknowns.push_back({ base + "_x", 1.0, 0.0, -ry });
            unknowns.push_back({ base + "_y", 0.0, 1.0, rx });
        } else if (s.kind == FbdSupportKind::RollerX) {
            // Roller free along X -> reaction normal along Y (0, 1)
            unknowns.push_back({ base + "_y", 0.0, 1.0, rx });
        } else if (s.kind == FbdSupportKind::RollerY) {
            const double rad = s.rollerNormalAngleDeg * kPi / 180.0;
            const double ux = std::cos(rad), uy = std::sin(rad);
            unknowns.push_back({ base + "_n", ux, uy, rx * uy - ry * ux });
        } else { // Fixed
            unknowns.push_back({ base + "_x", 1.0, 0.0, -ry });
            unknowns.push_back({ base + "_y", 0.0, 1.0, rx });
            unknowns.push_back({ base + "_M", 0.0, 0.0, 1.0 });
        }
    }

    std::ostringstream trace;
    trace << "Pivote (" << model.pivotX << ", " << model.pivotY << ")\n";
    trace << "Cargas conocidas: Fx=" << knownFx << " N, Fy=" << knownFy << " N, M=" << knownM << " N*m\n";

    if (unknowns.empty()) {
        res.netFx = knownFx;
        res.netFy = knownFy;
        res.netMoment = knownM;
        res.inEquilibrium = (std::abs(knownFx) < 1e-6 && std::abs(knownFy) < 1e-6 && std::abs(knownM) < 1e-6);
        res.determinacyStatus = "VERIFIED";
        res.ok = true;
        res.stepsTraceUtf8 = trace.str();
        return res;
    }

    if (unknowns.size() > 3) {
        res.determinacyStatus = "INDETERMINATE";
        res.errorUtf8 = "Sistema estaticamente indeterminado (" + std::to_string(unknowns.size()) + " incognitas > 3 ecuaciones)";
        res.stepsTraceUtf8 = trace.str();
        return res;
    }
    if (unknowns.size() < 3) {
        // Note: 2 unknowns could still be collinear/solvable if one equation is trivial 0=0,
        // otherwise it's underconstrained ("UNSTABLE")
        bool fxActive = (std::abs(knownFx) > 1e-9);
        for (const auto& u : unknowns) if (std::abs(u.cFx) > 1e-9) fxActive = true;
        if (unknowns.size() == 2 && !fxActive) {
            // Pure vertical/moment system (e.g. two vertical reactions on horizontal beam with vertical loads)
            double a00 = unknowns[0].cFy, a01 = unknowns[1].cFy, b0 = -knownFy;
            double a10 = unknowns[0].cM,  a11 = unknowns[1].cM,  b1 = -knownM;
            const double det2 = a00 * a11 - a01 * a10;
            if (std::abs(det2) > 1e-10) {
                const double r0 = (b0 * a11 - a01 * b1) / det2;
                const double r1 = (a00 * b1 - b0 * a10) / det2;
                res.reactions.push_back({ unknowns[0].name, r0 });
                res.reactions.push_back({ unknowns[1].name, r1 });
                res.netFx = knownFx;
                res.netFy = knownFy + r0 * unknowns[0].cFy + r1 * unknowns[1].cFy;
                res.netMoment = knownM + r0 * unknowns[0].cM + r1 * unknowns[1].cM;
                res.inEquilibrium = true;
                res.determinacyStatus = "DETERMINATE";
                res.ok = true;
                trace << unknowns[0].name << " = " << r0 << ", " << unknowns[1].name << " = " << r1 << "\n";
                res.stepsTraceUtf8 = trace.str();
                return res;
            }
        }
        res.determinacyStatus = "UNSTABLE";
        res.errorUtf8 = "Sistema cinematicamente inestable (< 3 reacciones independientes)";
        res.stepsTraceUtf8 = trace.str();
        return res;
    }

    // 3x3 linear system A * r = -known
    double aug[3][4] = {
        { unknowns[0].cFx, unknowns[1].cFx, unknowns[2].cFx, -knownFx },
        { unknowns[0].cFy, unknowns[1].cFy, unknowns[2].cFy, -knownFy },
        { unknowns[0].cM,  unknowns[1].cM,  unknowns[2].cM,  -knownM  }
    };
    for (int col = 0; col < 3; ++col) {
        int piv = col;
        for (int r = col + 1; r < 3; ++r) {
            if (std::abs(aug[r][col]) > std::abs(aug[piv][col])) piv = r;
        }
        if (std::abs(aug[piv][col]) < 1e-10) {
            res.determinacyStatus = "UNSTABLE";
            res.errorUtf8 = "Reacciones linealmente dependientes (sistema inestable / det=0)";
            res.stepsTraceUtf8 = trace.str();
            return res;
        }
        if (piv != col) {
            for (int c = 0; c < 4; ++c) std::swap(aug[col][c], aug[piv][c]);
        }
        const double inv = 1.0 / aug[col][col];
        for (int c = col; c < 4; ++c) aug[col][c] *= inv;
        for (int r = 0; r < 3; ++r) {
            if (r == col) continue;
            const double f = aug[r][col];
            for (int c = col; c < 4; ++c) aug[r][c] -= f * aug[col][c];
        }
    }

    res.netFx = knownFx;
    res.netFy = knownFy;
    res.netMoment = knownM;
    for (int i = 0; i < 3; ++i) {
        const double rv = aug[i][3];
        res.reactions.push_back({ unknowns[i].name, rv });
        res.netFx += rv * unknowns[i].cFx;
        res.netFy += rv * unknowns[i].cFy;
        res.netMoment += rv * unknowns[i].cM;
        trace << unknowns[i].name << " = " << rv << "\n";
    }
    res.inEquilibrium = (std::abs(res.netFx) < 1e-6 && std::abs(res.netFy) < 1e-6 && std::abs(res.netMoment) < 1e-6);
    res.determinacyStatus = "DETERMINATE";
    res.ok = true;
    res.stepsTraceUtf8 = trace.str();
    return res;
}

// JSON helpers for FbdModel (implemented in engineering_cas.cpp)
std::string FbdModelToJson(const FbdModel& model);
bool FbdModelFromJson(const std::string& jsonUtf8, FbdModel& outModel, std::string* outErr = nullptr);

// ---------------- F-253: Significant Figures & Uncertainty Propagation ----------------

struct UncertainValue {
    double value = 0.0;
    double uncertainty = 0.0;
    UnitDim dim{};
    std::string unitUtf8;
    int sigFigs = 0;
};

inline int SigFigsForMulDiv(int sfA, int sfB) noexcept {
    return (std::min)(sfA, sfB);
}

inline int DecPlacesForAddSub(int dpA, int dpB) noexcept {
    return (std::min)(dpA, dpB);
}

inline std::string FormatSignificantFigures(double val, int sigFigs) {
    if (!std::isfinite(val)) return "NaN";
    const int sf = std::clamp(sigFigs, 1, 15);
    if (std::abs(val) < 1e-15) {
        if (sf == 1) return "0";
        return "0." + std::string(static_cast<std::size_t>(sf - 1), '0');
    }
    int exp10 = static_cast<int>(std::floor(std::log10(std::abs(val))));
    int decPlaces = sf - 1 - exp10;
    double factor = std::pow(10.0, decPlaces);
    double rounded = std::round(val * factor) / factor;
    if (std::abs(rounded) > 0.0) {
        const int newExp10 = static_cast<int>(std::floor(std::log10(std::abs(rounded) * (1.0 + 1e-12))));
        if (newExp10 != exp10) {
            exp10 = newExp10;
            decPlaces = sf - 1 - exp10;
            factor = std::pow(10.0, decPlaces);
            rounded = std::round(val * factor) / factor;
        }
    }
    if (decPlaces >= 0 && std::abs(rounded) < 1e9) {
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss.precision(decPlaces);
        oss << rounded;
        return oss.str();
    }
    std::ostringstream oss;
    oss.setf(std::ios::scientific);
    oss.precision(sf - 1);
    oss << rounded;
    return oss.str();
}

inline std::string FormatValueWithUncertainty(
    double val,
    double unc,
    int uncSigFigs = 1,
    const std::string& unitUtf8 = "")
{
    if (!std::isfinite(val) || !std::isfinite(unc)) return "NaN";
    const double absUnc = std::abs(unc);
    if (absUnc < 1e-15) {
        return detail_stat::FmtClean(val, 6) + " +/- 0" + (unitUtf8.empty() ? "" : (" " + unitUtf8));
    }
    const int sf = std::clamp(uncSigFigs, 1, 2);
    const int exp10 = static_cast<int>(std::floor(std::log10(absUnc)));
    const int decPlaces = (std::max)(0, sf - 1 - exp10);
    const double scale = std::pow(10.0, sf - 1 - exp10);
    const double rUnc = std::round(absUnc * scale) / scale;
    const double rVal = std::round(val * scale) / scale;
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(decPlaces);
    oss << rVal << " \xC2\xB1 " << rUnc;
    if (!unitUtf8.empty()) oss << " " << unitUtf8;
    return oss.str();
}

struct UncertaintyPropResult {
    bool ok = false;
    double mean = 0.0;
    double rssUncertainty = 0.0;
    double mcMean = 0.0;
    double mcStdDev = 0.0;
    double mcCi95Low = 0.0;
    double mcCi95High = 0.0;
    UnitDim dim{};
    std::string formattedRssUtf8;
    std::string formattedMcUtf8;
    std::vector<std::pair<std::string, double>> sensitivityContributions;
    std::string errorUtf8;
};

inline UncertaintyPropResult PropagateUncertainty(
    const std::string& exprUtf8,
    const std::unordered_map<std::string, UncertainValue>& vars,
    std::size_t mcSamples = 10000,
    std::uint64_t seed = 0x51A7BEEFCAFE0001ULL)
{
    UncertaintyPropResult res;
    if (exprUtf8.empty()) {
        res.errorUtf8 = "Expresion vacia en propagacion de incertidumbre";
        return res;
    }

    SymbolEnv nomEnv;
    for (const auto& kv : vars) {
        QuantityValue q{};
        q.value = { kv.second.value, 0.0 };
        q.dim = kv.second.dim;
        q.preferredUnitUtf8 = kv.second.unitUtf8;
        nomEnv[kv.first] = q;
    }

    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(exprUtf8, ast);
    auto evalAt = [&](const SymbolEnv& env, QuantityValue& outQ, std::string& err) -> bool {
        CasEvalResult ev{};
        if (hasAst && EvaluateAst(ast, env, ev) && ev.ok && ev.result.IsReal()) {
            outQ = ev.result;
            return true;
        }
        ev = EvaluateExpression(exprUtf8, env);
        if (!ev.ok || !ev.result.IsReal()) {
            err = ev.ok ? "Resultado complejo en propagacion real" : ev.errorUtf8;
            return false;
        }
        outQ = ev.result;
        return true;
    };

    QuantityValue nomQ{};
    if (!evalAt(nomEnv, nomQ, res.errorUtf8)) return res;
    res.mean = nomQ.value.real();
    res.dim = nomQ.dim;

    // Central finite-difference RSS partial derivatives
    double rssVar = 0.0;
    for (const auto& kv : vars) {
        const double sigma = std::abs(kv.second.uncertainty);
        if (sigma <= 0.0) {
            res.sensitivityContributions.push_back({ kv.first, 0.0 });
            continue;
        }
        const double h = (std::max)(1e-7, (std::min)(sigma * 0.05, std::abs(kv.second.value) * 1e-5 + 1e-7));
        SymbolEnv ep = nomEnv, em = nomEnv;
        ep[kv.first].value = { kv.second.value + h, 0.0 };
        em[kv.first].value = { kv.second.value - h, 0.0 };
        QuantityValue qp{}, qm{};
        std::string tmpErr;
        if (!evalAt(ep, qp, tmpErr) || !evalAt(em, qm, tmpErr)) {
            res.errorUtf8 = tmpErr.empty() ? "Error evaluando derivada parcial" : tmpErr;
            return res;
        }
        const double dfdx = (qp.value.real() - qm.value.real()) / (2.0 * h);
        const double contrib = (dfdx * sigma) * (dfdx * sigma);
        rssVar += contrib;
        res.sensitivityContributions.push_back({ kv.first, contrib });
    }
    res.rssUncertainty = std::sqrt(rssVar);
    if (rssVar > 1e-30) {
        for (auto& sc : res.sensitivityContributions) sc.second /= rssVar;
    }

    // Monte Carlo simulation (SplitMix64 + Box-Muller)
    const std::size_t nMc = std::clamp<std::size_t>(mcSamples, 100, 200000);
    std::uint64_t rngState = seed ? seed : 0x9E3779B97F4A7C15ULL;
    auto nextUniform = [&]() -> double {
        rngState += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = rngState;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z = z ^ (z >> 31);
        return (static_cast<double>(z >> 11) + 0.5) / 9007199254740992.0;
    };
    auto nextNormal = [&]() -> double {
        constexpr double kTwoPi = 6.28318530717958647692;
        const double u1 = (std::max)(1e-15, nextUniform());
        const double u2 = nextUniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
    };

    std::vector<double> samples;
    samples.reserve(nMc);
    SymbolEnv mcEnv = nomEnv;
    double sumMc = 0.0;
    for (std::size_t s = 0; s < nMc; ++s) {
        for (const auto& kv : vars) {
            const double val = kv.second.value + kv.second.uncertainty * nextNormal();
            mcEnv[kv.first].value = { val, 0.0 };
        }
        QuantityValue mq{};
        std::string tmpErr;
        if (evalAt(mcEnv, mq, tmpErr) && std::isfinite(mq.value.real())) {
            const double yv = mq.value.real();
            samples.push_back(yv);
            sumMc += yv;
        }
    }
    if (samples.empty()) {
        res.errorUtf8 = "Ninguna muestra Monte Carlo fue valida";
        return res;
    }

    res.mcMean = sumMc / static_cast<double>(samples.size());
    double varMc = 0.0;
    for (double yv : samples) {
        const double d = yv - res.mcMean;
        varMc += d * d;
    }
    res.mcStdDev = (samples.size() > 1) ? std::sqrt(varMc / static_cast<double>(samples.size() - 1)) : 0.0;
    std::sort(samples.begin(), samples.end());
    const std::size_t loIdx = static_cast<std::size_t>(0.025 * static_cast<double>(samples.size() - 1));
    const std::size_t hiIdx = static_cast<std::size_t>(0.975 * static_cast<double>(samples.size() - 1));
    res.mcCi95Low = samples[loIdx];
    res.mcCi95High = samples[hiIdx];

    const std::string uLbl = nomQ.preferredUnitUtf8.empty() ? nomQ.dim.FormatSiString() : nomQ.preferredUnitUtf8;
    res.formattedRssUtf8 = FormatValueWithUncertainty(res.mean, res.rssUncertainty, 2, uLbl);
    res.formattedMcUtf8 = FormatValueWithUncertainty(res.mcMean, res.mcStdDev, 2, uLbl);
    res.ok = true;
    return res;
}

} // namespace DocStudio::Cas
