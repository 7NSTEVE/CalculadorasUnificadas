#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace DocStudio::Cas {

struct ComplexMatrix {
    std::size_t rows = 0;
    std::size_t cols = 0;
    std::vector<std::complex<double>> data; // row-major: rows * cols

    bool IsValid() const noexcept {
        return rows > 0 && cols > 0 && data.size() == rows * cols;
    }
    const std::complex<double>& At(std::size_t r, std::size_t c) const noexcept {
        return data[r * cols + c];
    }
    std::complex<double>& At(std::size_t r, std::size_t c) noexcept {
        return data[r * cols + c];
    }
};

struct ComplexMatrixResult {
    bool ok = false;
    ComplexMatrix matrix{};
    std::complex<double> scalar{ 0.0, 0.0 };
    bool isScalar = false;
    std::size_t rank = 0;
    std::string formattedUtf8;
    std::string latexUtf8;
    std::string stepsTraceUtf8;
    std::string errorUtf8;
};

struct EigenResult {
    bool ok = false;
    std::vector<std::complex<double>> eigenvalues;
    std::vector<std::vector<std::complex<double>>> eigenvectors; // column vectors
    bool isHermitian = false;
    std::string summaryUtf8;
    std::string errorUtf8;
};

namespace detail_adv {

inline std::string Trim(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline bool AllFinite(const std::vector<std::complex<double>>& data) noexcept {
    for (const auto& v : data) {
        if (!std::isfinite(v.real()) || !std::isfinite(v.imag())) return false;
    }
    return true;
}

inline std::string FmtReal(double v, int prec = 6, double scale = 0.0) {
    if (!std::isfinite(v)) return "NaN";
    const double snapTol = (scale > 0.0) ? (1e-12 * scale) : 1e-300;
    if (std::abs(v) <= snapTol) return "0";
    std::ostringstream oss;
    oss.precision((std::max)(2, prec));
    oss << v;
    return oss.str();
}

inline std::string FmtComplex(std::complex<double> z, int prec = 6, double scale = 0.0) {
    if (!std::isfinite(z.real()) || !std::isfinite(z.imag())) return "NaN";
    const double refScale = (scale > 0.0) ? scale : std::abs(z);
    if (refScale == 0.0) return "0";
    const double re = (std::abs(z.real()) < 1e-12 * refScale) ? 0.0 : z.real();
    const double im = (std::abs(z.imag()) < 1e-12 * refScale) ? 0.0 : z.imag();
    if (im == 0.0) return FmtReal(re, prec, refScale);
    if (re == 0.0) return FmtReal(im, prec, refScale) + "j";
    return FmtReal(re, prec, refScale) + (im >= 0.0 ? "+" : "-") + FmtReal(std::abs(im), prec, refScale) + "j";
}

inline std::string FormatCMatText(const ComplexMatrix& m) {
    double matScale = 0.0;
    for (const auto& v : m.data) matScale = (std::max)(matScale, std::abs(v));
    std::ostringstream oss;
    oss << "[";
    for (std::size_t r = 0; r < m.rows; ++r) {
        if (r > 0) oss << ", ";
        oss << "[";
        for (std::size_t c = 0; c < m.cols; ++c) {
            if (c > 0) oss << ", ";
            oss << FmtComplex(m.At(r, c), 5, matScale);
        }
        oss << "]";
    }
    oss << "]";
    return oss.str();
}

inline std::string FormatCMatLatex(const ComplexMatrix& m) {
    double matScale = 0.0;
    for (const auto& v : m.data) matScale = (std::max)(matScale, std::abs(v));
    std::ostringstream oss;
    oss << "\\begin{bmatrix} ";
    for (std::size_t r = 0; r < m.rows; ++r) {
        if (r > 0) oss << " \\\\ ";
        for (std::size_t c = 0; c < m.cols; ++c) {
            if (c > 0) oss << " & ";
            oss << FmtComplex(m.At(r, c), 5, matScale);
        }
    }
    oss << " \\end{bmatrix}";
    return oss.str();
}

inline bool ParseComplexMatrixLiteral(const std::string& txt, ComplexMatrix& outM) {
    outM = {};
    const std::string s = Trim(txt);
    if (s.size() < 4 || s.front() != '[' || s.back() != ']') return false;
    std::size_t i = 1;
    const std::size_t end = s.size() - 1;
    std::vector<std::vector<std::complex<double>>> rows;
    while (i < end) {
        while (i < end && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ',' || s[i] == ';')) ++i;
        if (i >= end) break;
        if (s[i] != '[') return false;
        ++i;
        std::vector<std::complex<double>> row;
        while (i < end && s[i] != ']') {
            std::size_t st = i;
            int pDepth = 0;
            while (i < end && !(pDepth == 0 && (s[i] == ',' || s[i] == ']'))) {
                if (s[i] == '(') ++pDepth;
                else if (s[i] == ')') --pDepth;
                ++i;
            }
            const std::string cell = Trim(s.substr(st, i - st));
            if (!cell.empty()) {
                const auto er = EvaluateExpression(cell);
                if (!er.ok) return false;
                row.push_back(er.result.value);
            }
            if (i < end && s[i] == ',') ++i;
        }
        if (i >= end || s[i] != ']' || row.empty()) return false;
        ++i;
        if (!rows.empty() && row.size() != rows[0].size()) return false;
        rows.push_back(std::move(row));
    }
    if (rows.empty() || rows.size() > 64 || rows[0].size() > 64) return false;
    outM.rows = rows.size();
    outM.cols = rows[0].size();
    outM.data.reserve(outM.rows * outM.cols);
    for (const auto& r : rows) {
        for (const auto& v : r) outM.data.push_back(v);
    }
    return true;
}

inline bool EvalLoopVar(
    const std::string& expr,
    const MathJsonNode* ast,
    const std::string& varName,
    std::complex<double> val,
    const SymbolEnv& baseEnv,
    CasEvalResult& out)
{
    SymbolEnv work = baseEnv;
    if (!varName.empty()) {
        work[varName] = QuantityValue{ val, {}, "", 1.0, 0.0, false };
    }
    if (ast && EvaluateAst(*ast, work, out) && out.ok) return true;
    out = EvaluateExpression(expr, work);
    return out.ok;
}

} // namespace detail_adv

inline ComplexMatrixResult ComplexMatrixDet(const ComplexMatrix& a) {
    ComplexMatrixResult res;
    if (!a.IsValid() || a.rows != a.cols || a.rows > 64) {
        res.errorUtf8 = "det() requiere matriz cuadrada (1..64)";
        return res;
    }
    if (!detail_adv::AllFinite(a.data)) {
        res.errorUtf8 = "Matriz contiene valores no finitos (NaN/Inf)";
        return res;
    }
    const std::size_t n = a.rows;
    std::vector<std::complex<double>> m = a.data;
    double maxAbs = 0.0;
    for (const auto& v : m) maxAbs = (std::max)(maxAbs, std::abs(v));
    const double pivTol = 1e-14 * (std::max)(1e-300, maxAbs);
    std::complex<double> det(1.0, 0.0);
    int sign = 1;

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        double best = std::abs(m[col * n + col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            const double cand = std::abs(m[r * n + col]);
            if (cand > best) { best = cand; piv = r; }
        }
        if (maxAbs == 0.0 || best < pivTol) {
            res.ok = true;
            res.isScalar = true;
            res.scalar = { 0.0, 0.0 };
            res.formattedUtf8 = "0";
            res.latexUtf8 = "\\det(A) = 0";
            return res;
        }
        if (piv != col) {
            for (std::size_t c = 0; c < n; ++c) std::swap(m[col * n + c], m[piv * n + c]);
            sign = -sign;
        }
        const auto diag = m[col * n + col];
        det *= diag;
        for (std::size_t r = col + 1; r < n; ++r) {
            const auto factor = m[r * n + col] / diag;
            for (std::size_t c = col + 1; c < n; ++c) {
                m[r * n + c] -= factor * m[col * n + c];
            }
        }
    }
    det *= static_cast<double>(sign);
    res.ok = true;
    res.isScalar = true;
    res.scalar = det;
    res.formattedUtf8 = detail_adv::FmtComplex(det, 6, std::abs(det));
    res.latexUtf8 = "\\det(A) = " + res.formattedUtf8;
    return res;
}

inline ComplexMatrixResult ComplexMatrixInv(const ComplexMatrix& a) {
    ComplexMatrixResult res;
    if (!a.IsValid() || a.rows != a.cols || a.rows > 64) {
        res.errorUtf8 = "inv() requiere matriz cuadrada (1..64)";
        return res;
    }
    if (!detail_adv::AllFinite(a.data)) {
        res.errorUtf8 = "Matriz contiene valores no finitos (NaN/Inf)";
        return res;
    }
    const std::size_t n = a.rows;
    const std::size_t w = n * 2;
    double maxAbs = 0.0;
    for (const auto& v : a.data) maxAbs = (std::max)(maxAbs, std::abs(v));
    const double pivTol = 1e-14 * (std::max)(1e-300, maxAbs);
    std::vector<std::complex<double>> aug(n * w, std::complex<double>(0.0, 0.0));
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) aug[r * w + c] = a.At(r, c);
        aug[r * w + n + r] = { 1.0, 0.0 };
    }

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        double best = std::abs(aug[col * w + col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            const double cand = std::abs(aug[r * w + col]);
            if (cand > best) { best = cand; piv = r; }
        }
        if (maxAbs == 0.0 || best < pivTol) {
            res.errorUtf8 = "Matriz singular: no invertible";
            return res;
        }
        if (piv != col) {
            for (std::size_t c = 0; c < w; ++c) std::swap(aug[col * w + c], aug[piv * w + c]);
        }
        const auto div = aug[col * w + col];
        for (std::size_t c = 0; c < w; ++c) aug[col * w + c] /= div;
        const double factorTol = 1e-16 * maxAbs;
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            const auto factor = aug[r * w + col];
            if (std::abs(factor) < factorTol) continue;
            for (std::size_t c = 0; c < w; ++c) aug[r * w + c] -= factor * aug[col * w + c];
        }
    }

    res.ok = true;
    res.rank = n;
    res.matrix.rows = n;
    res.matrix.cols = n;
    res.matrix.data.resize(n * n);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) {
            res.matrix.At(r, c) = aug[r * w + n + c];
        }
    }
    res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
    res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
    return res;
}

inline ComplexMatrixResult ComplexMatrixRref(const ComplexMatrix& a, double tol = 1e-12) {
    ComplexMatrixResult res;
    if (!a.IsValid() || a.rows > 64 || a.cols > 64) {
        res.errorUtf8 = "rref() requiere matriz valida (<=64x64)";
        return res;
    }
    if (!detail_adv::AllFinite(a.data)) {
        res.errorUtf8 = "Matriz contiene valores no finitos (NaN/Inf)";
        return res;
    }
    double maxAbs = 0.0;
    for (const auto& v : a.data) maxAbs = (std::max)(maxAbs, std::abs(v));
    if (maxAbs == 0.0) {
        res.ok = true;
        res.rank = 0;
        res.matrix = a;
        res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
        res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
        return res;
    }
    const double effTol = ((tol > 0.0) ? tol : 1e-12) * maxAbs;
    ComplexMatrix m = a;
    std::size_t lead = 0;
    std::size_t rank = 0;
    std::ostringstream trace;

    for (std::size_t r = 0; r < m.rows && lead < m.cols; ++r) {
        std::size_t piv = r;
        double best = 0.0;
        while (lead < m.cols) {
            piv = r;
            best = std::abs(m.At(r, lead));
            for (std::size_t i = r + 1; i < m.rows; ++i) {
                const double cand = std::abs(m.At(i, lead));
                if (cand > best) { best = cand; piv = i; }
            }
            if (best >= effTol) break;
            for (std::size_t i = r; i < m.rows; ++i) m.At(i, lead) = { 0.0, 0.0 };
            ++lead;
        }
        if (lead >= m.cols) break;

        if (piv != r) {
            for (std::size_t c = 0; c < m.cols; ++c) std::swap(m.At(r, c), m.At(piv, c));
            trace << "R" << (r + 1) << " <-> R" << (piv + 1) << "\n";
        }
        const auto div = m.At(r, lead);
        for (std::size_t c = 0; c < m.cols; ++c) m.At(r, c) /= div;
        for (std::size_t i = 0; i < m.rows; ++i) {
            if (i == r) continue;
            const auto factor = m.At(i, lead);
            if (std::abs(factor) < effTol * 1e-3) continue;
            for (std::size_t c = 0; c < m.cols; ++c) m.At(i, c) -= factor * m.At(r, c);
        }
        trace << "Pivote (" << (r + 1) << "," << (lead + 1) << ") = 1\n";
        ++rank;
        ++lead;
    }

    for (auto& v : m.data) {
        if (std::abs(v.real()) < 1e-13) v.real(0.0);
        if (std::abs(v.imag()) < 1e-13) v.imag(0.0);
    }
    res.ok = true;
    res.rank = rank;
    res.matrix = std::move(m);
    res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
    res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
    res.stepsTraceUtf8 = trace.str();
    return res;
}

inline ComplexMatrixResult SolveComplexLinearSystem(
    const ComplexMatrix& A,
    const std::vector<std::complex<double>>& b)
{
    ComplexMatrixResult res;
    if (!A.IsValid() || A.rows != A.cols || A.rows > 64 || b.size() != A.rows) {
        res.errorUtf8 = "Dimensiones invalidas en sistema lineal complejo (max 64x64)";
        return res;
    }
    if (!detail_adv::AllFinite(A.data) || !detail_adv::AllFinite(b)) {
        res.errorUtf8 = "Sistema lineal contiene valores no finitos (NaN/Inf)";
        return res;
    }
    const std::size_t n = A.rows;
    const std::size_t w = n + 1;
    double maxAbs = 0.0;
    for (const auto& v : A.data) maxAbs = (std::max)(maxAbs, std::abs(v));
    const double pivTol = 1e-14 * (std::max)(1e-300, maxAbs);
    std::vector<std::complex<double>> aug(n * w);
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) aug[r * w + c] = A.At(r, c);
        aug[r * w + n] = b[r];
    }

    std::ostringstream trace;
    for (std::size_t col = 0; col < n; ++col) {
        std::size_t piv = col;
        double best = std::abs(aug[col * w + col]);
        for (std::size_t r = col + 1; r < n; ++r) {
            const double cand = std::abs(aug[r * w + col]);
            if (cand > best) { best = cand; piv = r; }
        }
        if (maxAbs == 0.0 || best < pivTol) {
            res.errorUtf8 = "Sistema complejo singular o indeterminado";
            return res;
        }
        if (piv != col) {
            for (std::size_t c = col; c < w; ++c) std::swap(aug[col * w + c], aug[piv * w + c]);
            trace << "R" << (col + 1) << " <-> R" << (piv + 1) << "\n";
        }
        const auto div = aug[col * w + col];
        for (std::size_t c = col; c < w; ++c) aug[col * w + c] /= div;
        const double factorTol = 1e-16 * maxAbs;
        for (std::size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            const auto factor = aug[r * w + col];
            if (std::abs(factor) < factorTol) continue;
            for (std::size_t c = col; c < w; ++c) aug[r * w + c] -= factor * aug[col * w + c];
        }
    }

    res.ok = true;
    res.rank = n;
    res.matrix.rows = n;
    res.matrix.cols = 1;
    res.matrix.data.resize(n);
    for (std::size_t r = 0; r < n; ++r) res.matrix.At(r, 0) = aug[r * w + n];
    res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
    res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
    res.stepsTraceUtf8 = trace.str();
    return res;
}

inline ComplexMatrixResult SolveComplexLinearSystemEquations(const std::string& equationsUtf8) {
    ComplexMatrixResult res;
    std::string norm = equationsUtf8;
    for (char& ch : norm) if (ch == ';') ch = '\n';
    std::istringstream iss(norm);
    std::vector<std::string> eqLines;
    std::string ln;
    while (std::getline(iss, ln)) {
        ln = detail_adv::Trim(ln);
        if (!ln.empty()) {
            // Insert implicit var mul e.g. "2x" -> "2*x", but keep "2j" / "3i" intact
            std::string mul;
            for (std::size_t i = 0; i < ln.size(); ++i) {
                mul.push_back(ln[i]);
                if (i + 1 < ln.size()) {
                    const unsigned char a = static_cast<unsigned char>(ln[i]);
                    const unsigned char b = static_cast<unsigned char>(ln[i + 1]);
                    if ((std::isdigit(a) || a == '.') && (std::isalpha(b) || b == '_')) {
                        if ((b == 'j' || b == 'i') &&
                            (i + 2 >= ln.size() || (!std::isalnum(static_cast<unsigned char>(ln[i + 2])) && ln[i + 2] != '_'))) {
                            continue;
                        }
                        if ((b == 'e' || b == 'E') && i + 2 < ln.size() &&
                            (std::isdigit(static_cast<unsigned char>(ln[i + 2])) || ln[i + 2] == '+' || ln[i + 2] == '-')) {
                            continue;
                        }
                        mul.push_back('*');
                    }
                }
            }
            eqLines.push_back(std::move(mul));
        }
    }
    if (eqLines.empty()) {
        res.errorUtf8 = "Sistema lineal complejo vacio";
        return res;
    }

    static const std::unordered_set<std::string> kReserved{
        "sin", "cos", "tan", "sqrt", "exp", "ln", "log", "log10", "abs", "pi", "e", "i", "j"
    };
    std::vector<std::string> vars;
    std::unordered_set<std::string> seen;
    for (const auto& eq : eqLines) {
        std::size_t i = 0;
        while (i < eq.size()) {
            const unsigned char c = static_cast<unsigned char>(eq[i]);
            if (std::isalpha(c) || c == '_') {
                const std::size_t st = i;
                while (i < eq.size() && (std::isalnum(static_cast<unsigned char>(eq[i])) || eq[i] == '_')) ++i;
                const std::string id = eq.substr(st, i - st);
                const bool isNumImagSuffix = (id == "j" || id == "i") && st > 0 && std::isdigit(static_cast<unsigned char>(eq[st - 1]));
                if (!isNumImagSuffix && kReserved.find(id) == kReserved.end() && seen.insert(id).second) {
                    vars.push_back(id);
                }
            } else {
                ++i;
            }
        }
    }

    const std::size_t n = vars.size();
    if (n == 0 || n != eqLines.size() || n > 64) {
        res.errorUtf8 = "Se requieren N ecuaciones para N incognitas (1..64)";
        return res;
    }

    ComplexMatrix A{ n, n, std::vector<std::complex<double>>(n * n, { 0.0, 0.0 }) };
    std::vector<std::complex<double>> b(n, { 0.0, 0.0 });
    SymbolEnv zeroEnv;
    for (const auto& v : vars) zeroEnv[v] = QuantityValue{ { 0.0, 0.0 }, {}, "", 1.0, 0.0, false };

    for (std::size_t r = 0; r < n; ++r) {
        const auto eqPos = eqLines[r].find('=');
        if (eqPos == std::string::npos) {
            res.errorUtf8 = "Falta '=' en ecuacion: " + eqLines[r];
            return res;
        }
        const std::string lhs = eqLines[r].substr(0, eqPos);
        const std::string rhs = eqLines[r].substr(eqPos + 1);
        auto evalRes = [&](const SymbolEnv& env, std::complex<double>& outVal) -> bool {
            const auto el = EvaluateExpression(lhs, env);
            const auto er = EvaluateExpression(rhs, env);
            if (!el.ok || !er.ok) return false;
            outVal = el.result.value - er.result.value;
            return true;
        };

        std::complex<double> f0{};
        if (!evalRes(zeroEnv, f0)) {
            res.errorUtf8 = "Error evaluando ecuacion: " + eqLines[r];
            return res;
        }
        b[r] = -f0;

        for (std::size_t c = 0; c < n; ++c) {
            SymbolEnv e1 = zeroEnv, e2 = zeroEnv;
            e1[vars[c]].value = { 1.0, 0.0 };
            e2[vars[c]].value = { 2.0, 0.0 };
            std::complex<double> f1{}, f2{};
            if (!evalRes(e1, f1) || !evalRes(e2, f2) || std::abs((f2 - f1) - (f1 - f0)) > 1e-7) {
                res.errorUtf8 = "Ecuacion no lineal en " + vars[c];
                return res;
            }
            A.At(r, c) = f1 - f0;
        }
    }

    auto sol = SolveComplexLinearSystem(A, b);
    if (!sol.ok) return sol;

    double maxSolAbs = 0.0;
    for (std::size_t i = 0; i < n; ++i) maxSolAbs = (std::max)(maxSolAbs, std::abs(sol.matrix.At(i, 0)));
    std::ostringstream sum;
    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0) sum << ", ";
        sum << vars[i] << " = " << detail_adv::FmtComplex(sol.matrix.At(i, 0), 6, maxSolAbs);
    }
    sol.formattedUtf8 = sum.str();
    return sol;
}

inline EigenResult ComputeMatrixEigen(const ComplexMatrix& a, int maxIter = 500) {
    EigenResult res;
    if (!a.IsValid() || a.rows != a.cols || a.rows > 64) {
        res.errorUtf8 = "eig() requiere matriz cuadrada (1..64)";
        return res;
    }
    if (!detail_adv::AllFinite(a.data)) {
        res.errorUtf8 = "Matriz contiene valores no finitos (NaN/Inf)";
        return res;
    }
    const std::size_t n = a.rows;

    double frobSq = 0.0;
    for (const auto& v : a.data) frobSq += std::norm(v);
    const double rawScale = std::sqrt(frobSq);
    if (rawScale == 0.0) {
        res.ok = true;
        res.isHermitian = true;
        res.eigenvalues.assign(n, { 0.0, 0.0 });
        res.eigenvectors.assign(n, std::vector<std::complex<double>>(n, { 0.0, 0.0 }));
        std::ostringstream sum;
        for (std::size_t i = 0; i < n; ++i) {
            res.eigenvectors[i][i] = { 1.0, 0.0 };
            if (i > 0) sum << ", ";
            sum << "lambda_" << (i + 1) << " = 0";
        }
        res.summaryUtf8 = sum.str();
        return res;
    }
    const double matScale = rawScale;

    // Check Hermitian (and specifically real symmetric) with norm-scaled tolerances
    bool isHerm = true;
    bool isReal = true;
    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) {
            if (std::abs(a.At(r, c).imag()) > 1e-12 * matScale) isReal = false;
            if (std::abs(a.At(r, c) - std::conj(a.At(c, r))) > 1e-11 * matScale) isHerm = false;
        }
    }
    res.isHermitian = isHerm;

    if (n == 1) {
        res.ok = true;
        res.eigenvalues = { a.At(0, 0) };
        res.eigenvectors = { { { 1.0, 0.0 } } };
        res.summaryUtf8 = "lambda_1 = " + detail_adv::FmtComplex(a.At(0, 0), 6, matScale);
        return res;
    }

    // Cyclic Jacobi eigenvalue algorithm for real symmetric matrices
    if (isReal && isHerm) {
        std::vector<double> d(n * n, 0.0);
        std::vector<double> V(n * n, 0.0);
        for (std::size_t r = 0; r < n; ++r) {
            V[r * n + r] = 1.0;
            for (std::size_t c = 0; c < n; ++c) d[r * n + c] = a.At(r, c).real();
        }
        const int sweeps = (std::max)(50, maxIter);
        auto computeOffNorm = [&]() {
            double offSq = 0.0;
            for (std::size_t r = 0; r < n; ++r) {
                for (std::size_t c = r + 1; c < n; ++c) {
                    const double v = d[r * n + c];
                    offSq += 2.0 * v * v;
                }
            }
            return std::sqrt(offSq);
        };
        const double convTol = 1e-13 * matScale;
        const double skipTol = 1e-15 * matScale;
        for (int sweep = 0; sweep < sweeps; ++sweep) {
            if (computeOffNorm() <= convTol) break;
            for (std::size_t p = 0; p < n; ++p) {
                for (std::size_t q = p + 1; q < n; ++q) {
                    const double apq = d[p * n + q];
                    if (std::abs(apq) <= skipTol) continue;
                    const double app = d[p * n + p], aqq = d[q * n + q];
                    const double tau = (aqq - app) / (2.0 * apq);
                    const double t = (tau >= 0.0)
                        ? (1.0 / (tau + std::sqrt(1.0 + tau * tau)))
                        : (-1.0 / (-tau + std::sqrt(1.0 + tau * tau)));
                    const double c = 1.0 / std::sqrt(1.0 + t * t);
                    const double s = t * c;
                    d[p * n + p] = app - t * apq;
                    d[q * n + q] = aqq + t * apq;
                    d[p * n + q] = 0.0;
                    d[q * n + p] = 0.0;
                    for (std::size_t k = 0; k < n; ++k) {
                        if (k == p || k == q) continue;
                        const double dkp = d[k * n + p], dkq = d[k * n + q];
                        d[k * n + p] = d[p * n + k] = c * dkp - s * dkq;
                        d[k * n + q] = d[q * n + k] = s * dkp + c * dkq;
                    }
                    for (std::size_t k = 0; k < n; ++k) {
                        const double vkp = V[k * n + p], vkq = V[k * n + q];
                        V[k * n + p] = c * vkp - s * vkq;
                        V[k * n + q] = s * vkp + c * vkq;
                    }
                }
            }
        }
        if (computeOffNorm() > 1e-8 * matScale) {
            res.errorUtf8 = "Jacobi no convergio dentro del limite de barridos";
            return res;
        }
        res.ok = true;
        res.eigenvalues.resize(n);
        res.eigenvectors.resize(n, std::vector<std::complex<double>>(n));
        for (std::size_t i = 0; i < n; ++i) {
            res.eigenvalues[i] = { d[i * n + i], 0.0 };
            for (std::size_t r = 0; r < n; ++r) res.eigenvectors[i][r] = { V[r * n + i], 0.0 };
        }
    } else {
        // General complex (or non-symmetric real) via Householder Hessenberg + shifted QR
        std::vector<std::complex<double>> H = a.data;
        // Reduce to upper Hessenberg
        for (std::size_t k = 0; k + 2 < n; ++k) {
            double normX = 0.0;
            for (std::size_t i = k + 1; i < n; ++i) normX += std::norm(H[i * n + k]);
            normX = std::sqrt(normX);
            if (normX < 1e-15 * matScale) continue;
            const auto x0 = H[(k + 1) * n + k];
            const std::complex<double> phase = (std::abs(x0) > 1e-15 * matScale) ? (x0 / std::abs(x0)) : std::complex<double>(1.0, 0.0);
            std::vector<std::complex<double>> u(n - (k + 1));
            for (std::size_t i = 0; i < u.size(); ++i) u[i] = H[(k + 1 + i) * n + k];
            u[0] += phase * normX;
            double uNorm2 = 0.0;
            for (const auto& uv : u) uNorm2 += std::norm(uv);
            if (uNorm2 < 1e-30 * matScale * matScale) continue;
            const double invU = 1.0 / uNorm2;

            // Left multiply: H[k+1.., k..] -= 2 * u * (u^H * H[k+1.., k..]) / ||u||^2
            for (std::size_t c = k; c < n; ++c) {
                std::complex<double> dot(0.0, 0.0);
                for (std::size_t i = 0; i < u.size(); ++i) dot += std::conj(u[i]) * H[(k + 1 + i) * n + c];
                dot *= (2.0 * invU);
                for (std::size_t i = 0; i < u.size(); ++i) H[(k + 1 + i) * n + c] -= u[i] * dot;
            }
            // Right multiply: H[.., k+1..] -= 2 * (H[.., k+1..] * u) * u^H / ||u||^2
            for (std::size_t r = 0; r < n; ++r) {
                std::complex<double> dot(0.0, 0.0);
                for (std::size_t i = 0; i < u.size(); ++i) dot += H[r * n + (k + 1 + i)] * u[i];
                dot *= (2.0 * invU);
                for (std::size_t i = 0; i < u.size(); ++i) H[r * n + (k + 1 + i)] -= dot * std::conj(u[i]);
            }
        }

        // Complex-shifted QR iteration with Wilkinson shift (defeating real-matrix complex cycles via complex shift!)
        std::size_t m = n;
        int iter = 0;
        const int iterCap = (std::max)(400, maxIter * static_cast<int>(n));
        std::vector<std::complex<double>> cs(n), sn(n);
        while (m > 1 && iter < iterCap) {
            ++iter;
            // Check subdiagonal deflation (scaled by matrix norm so 1e-10 matrices behave identically to unit scale)
            if (std::abs(H[(m - 1) * n + (m - 2)]) <= 1e-13 * (std::abs(H[(m - 2) * n + (m - 2)]) + std::abs(H[(m - 1) * n + (m - 1)]) + 1e-15 * matScale)) {
                res.eigenvalues.push_back(H[(m - 1) * n + (m - 1)]);
                --m;
                continue;
            }
            // Compute Wilkinson shift from bottom 2x2 block
            const auto a11 = H[(m - 2) * n + (m - 2)];
            const auto a12 = H[(m - 2) * n + (m - 1)];
            const auto a21 = H[(m - 1) * n + (m - 2)];
            const auto a22 = H[(m - 1) * n + (m - 1)];
            const auto tr = a11 + a22;
            const auto det = a11 * a22 - a12 * a21;
            const auto disc = std::sqrt(tr * tr - 4.0 * det);
            const auto l1 = 0.5 * (tr + disc);
            const auto l2 = 0.5 * (tr - disc);
            std::complex<double> shift = (std::abs(l1 - a22) < std::abs(l2 - a22)) ? l1 : l2;
            if ((iter % 17) == 0) {
                // Exceptional shift breaks any symmetry stall (scaled by matScale)
                shift = a22 + std::complex<double>(0.37 * std::abs(a21) + 0.13 * matScale, 0.29 * std::abs(a21) + 0.17 * matScale);
            }

            for (std::size_t i = 0; i < m; ++i) H[i * n + i] -= shift;
            for (std::size_t k = 0; k + 1 < m; ++k) {
                const auto x = H[k * n + k];
                const auto y = H[(k + 1) * n + k];
                const double rNorm = std::hypot(std::abs(x), std::abs(y));
                if (rNorm <= 1e-30 * matScale) {
                    cs[k] = { 1.0, 0.0 };
                    sn[k] = { 0.0, 0.0 };
                    continue;
                }
                const auto c = x / rNorm;
                const auto s = y / rNorm;
                cs[k] = c;
                sn[k] = s;
                // Apply G^H on rows k, k+1: row_k' = conj(c)*row_k + conj(s)*row_{k+1}, row_{k+1}' = -s*row_k + c*row_{k+1}
                for (std::size_t j = k; j < m; ++j) {
                    const auto t1 = std::conj(c) * H[k * n + j] + std::conj(s) * H[(k + 1) * n + j];
                    const auto t2 = -s * H[k * n + j] + c * H[(k + 1) * n + j];
                    H[k * n + j] = t1;
                    H[(k + 1) * n + j] = t2;
                }
            }
            for (std::size_t k = 0; k + 1 < m; ++k) {
                const auto c = cs[k], s = sn[k];
                for (std::size_t i = 0; i <= (std::min)(k + 2, m - 1); ++i) {
                    const auto t1 = H[i * n + k] * c + H[i * n + (k + 1)] * s;
                    const auto t2 = -H[i * n + k] * std::conj(s) + H[i * n + (k + 1)] * std::conj(c);
                    H[i * n + k] = t1;
                    H[i * n + (k + 1)] = t2;
                }
            }
            for (std::size_t i = 0; i < m; ++i) H[i * n + i] += shift;
        }
        if (m > 1) {
            res.errorUtf8 = "QR no convergio dentro del limite de iteraciones";
            return res;
        }
        while (m > 0) {
            res.eigenvalues.push_back(H[(m - 1) * n + (m - 1)]);
            --m;
        }
        std::reverse(res.eigenvalues.begin(), res.eigenvalues.end());

        // Compute normalized eigenvectors via norm-scaled shifted inverse iteration
        res.eigenvectors.resize(n);
        const std::complex<double> primaryShift = std::complex<double>(1e-9, 1e-10) * matScale;
        const std::complex<double> fallbackShift = std::complex<double>(1e-7, -1e-8) * matScale;
        for (std::size_t idx = 0; idx < n; ++idx) {
            const auto lam = res.eigenvalues[idx];
            ComplexMatrix shifted = a;
            for (std::size_t r = 0; r < n; ++r) shifted.At(r, r) -= (lam + primaryShift);
            std::vector<std::complex<double>> v(n, { 1.0 / std::sqrt(static_cast<double>(n)), 0.0 });
            for (std::size_t r = 0; r < n; ++r) v[r] += std::complex<double>(0.01 * static_cast<double>(r + 1), 0.007 * static_cast<double>(r));
            for (int step = 0; step < 3; ++step) {
                auto sol = SolveComplexLinearSystem(shifted, v);
                if (!sol.ok) {
                    ComplexMatrix fbShifted = a;
                    for (std::size_t r = 0; r < n; ++r) fbShifted.At(r, r) -= (lam + fallbackShift);
                    sol = SolveComplexLinearSystem(fbShifted, v);
                    if (!sol.ok) break;
                }
                double nrm = 0.0;
                for (std::size_t r = 0; r < n; ++r) nrm += std::norm(sol.matrix.At(r, 0));
                nrm = std::sqrt(nrm);
                if (nrm < 1e-300) break;
                for (std::size_t r = 0; r < n; ++r) v[r] = sol.matrix.At(r, 0) / nrm;
            }
            res.eigenvectors[idx] = std::move(v);
        }
        res.ok = true;
    }

    std::ostringstream sum;
    for (std::size_t i = 0; i < res.eigenvalues.size(); ++i) {
        if (i > 0) sum << ", ";
        sum << "lambda_" << (i + 1) << " = " << detail_adv::FmtComplex(res.eigenvalues[i], 6, matScale);
    }
    res.summaryUtf8 = sum.str();
    return res;
}

inline ComplexMatrixResult EvaluateComplexMatrixExpression(const std::string& exprUtf8) {
    ComplexMatrixResult res;
    const std::string s = detail_adv::Trim(exprUtf8);
    if (s.empty()) {
        res.errorUtf8 = "Expresion matricial vacia";
        return res;
    }

    auto parseUnaryFn = [&](const std::string& fnName, ComplexMatrix& outArg) -> bool {
        if (s.size() > fnName.size() + 2 &&
            s.compare(0, fnName.size(), fnName) == 0 &&
            s[fnName.size()] == '(' && s.back() == ')') {
            return detail_adv::ParseComplexMatrixLiteral(s.substr(fnName.size() + 1, s.size() - fnName.size() - 2), outArg);
        }
        return false;
    };

    ComplexMatrix A;
    if (parseUnaryFn("det", A)) return ComplexMatrixDet(A);
    if (parseUnaryFn("inv", A)) return ComplexMatrixInv(A);
    if (parseUnaryFn("rref", A)) return ComplexMatrixRref(A);
    if (parseUnaryFn("rank", A)) {
        auto r = ComplexMatrixRref(A);
        if (!r.ok) return r;
        r.isScalar = true;
        r.scalar = { static_cast<double>(r.rank), 0.0 };
        r.formattedUtf8 = std::to_string(r.rank);
        r.latexUtf8 = "\\operatorname{rank}(A) = " + r.formattedUtf8;
        return r;
    }
    if (parseUnaryFn("tr", A)) {
        if (A.rows != A.cols) { res.errorUtf8 = "tr() requiere matriz cuadrada"; return res; }
        if (!detail_adv::AllFinite(A.data)) { res.errorUtf8 = "Matriz contiene valores no finitos (NaN/Inf)"; return res; }
        double matScale = 0.0;
        for (const auto& v : A.data) matScale = (std::max)(matScale, std::abs(v));
        std::complex<double> tr{};
        for (std::size_t i = 0; i < A.rows; ++i) tr += A.At(i, i);
        res.ok = true; res.isScalar = true; res.scalar = tr;
        res.formattedUtf8 = detail_adv::FmtComplex(tr, 6, matScale);
        res.latexUtf8 = "\\operatorname{tr}(A) = " + res.formattedUtf8;
        return res;
    }
    if (parseUnaryFn("transpose", A)) {
        ComplexMatrix T{ A.cols, A.rows, std::vector<std::complex<double>>(A.rows * A.cols) };
        for (std::size_t r = 0; r < A.rows; ++r)
            for (std::size_t c = 0; c < A.cols; ++c) T.At(c, r) = A.At(r, c);
        res.ok = true; res.matrix = std::move(T);
        res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
        res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
        return res;
    }
    if (parseUnaryFn("hermitian", A) || parseUnaryFn("conj_t", A)) {
        ComplexMatrix H{ A.cols, A.rows, std::vector<std::complex<double>>(A.rows * A.cols) };
        for (std::size_t r = 0; r < A.rows; ++r)
            for (std::size_t c = 0; c < A.cols; ++c) H.At(c, r) = std::conj(A.At(r, c));
        res.ok = true; res.matrix = std::move(H);
        res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
        res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
        return res;
    }
    if (parseUnaryFn("eig", A)) {
        const auto er = ComputeMatrixEigen(A);
        if (!er.ok) { res.errorUtf8 = er.errorUtf8; return res; }
        res.ok = true;
        res.matrix.rows = er.eigenvalues.size();
        res.matrix.cols = 1;
        res.matrix.data = er.eigenvalues;
        res.formattedUtf8 = er.summaryUtf8;
        res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
        return res;
    }

    auto parseVec1D = [](const std::string& raw, std::vector<double>& outVec) -> bool {
        outVec.clear();
        const std::string t = detail_adv::Trim(raw);
        if (t.size() < 2 || t.front() != '[' || t.back() != ']') return false;
        std::istringstream iss(t.substr(1, t.size() - 2));
        std::string tok;
        while (std::getline(iss, tok, ',')) {
            const std::string c = detail_adv::Trim(tok);
            if (c.empty()) return false;
            try {
                std::size_t pos = 0;
                const double v = std::stod(c, &pos);
                if (pos != c.size() || !std::isfinite(v)) return false;
                outVec.push_back(v);
            } catch (...) {
                return false;
            }
        }
        return !outVec.empty();
    };

    auto parseBinaryVecFn = [&](const std::string& fnName, std::vector<double>& va, std::vector<double>& vb) -> bool {
        if (s.size() <= fnName.size() + 2 || s.compare(0, fnName.size(), fnName) != 0 ||
            s[fnName.size()] != '(' || s.back() != ')') return false;
        const std::string inner = s.substr(fnName.size() + 1, s.size() - fnName.size() - 2);
        int bDepth = 0;
        std::size_t splitComma = std::string::npos;
        for (std::size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '[') ++bDepth;
            else if (inner[i] == ']') --bDepth;
            else if (inner[i] == ',' && bDepth == 0) { splitComma = i; break; }
        }
        if (splitComma == std::string::npos) return false;
        return parseVec1D(inner.substr(0, splitComma), va) &&
               parseVec1D(inner.substr(splitComma + 1), vb);
    };

    std::vector<double> va, vb;
    if (parseBinaryVecFn("cross", va, vb)) {
        if (va.size() != 3 || vb.size() != 3) {
            res.errorUtf8 = "cross() requiere dos vectores 3D";
            return res;
        }
        const double cx = va[1] * vb[2] - va[2] * vb[1];
        const double cy = va[2] * vb[0] - va[0] * vb[2];
        const double cz = va[0] * vb[1] - va[1] * vb[0];
        res.ok = true;
        res.matrix = { 1, 3, { {cx, 0.0}, {cy, 0.0}, {cz, 0.0} } };
        res.formattedUtf8 = "[" + detail_adv::FmtReal(cx) + ", " + detail_adv::FmtReal(cy) + ", " + detail_adv::FmtReal(cz) + "]";
        res.latexUtf8 = "\\langle " + detail_adv::FmtReal(cx) + ", " + detail_adv::FmtReal(cy) + ", " + detail_adv::FmtReal(cz) + " \\rangle";
        return res;
    }
    if (parseBinaryVecFn("dot", va, vb)) {
        if (va.size() != vb.size()) {
            res.errorUtf8 = "dot() requiere vectores de igual dimension";
            return res;
        }
        double acc = 0.0;
        for (std::size_t i = 0; i < va.size(); ++i) acc += va[i] * vb[i];
        res.ok = true;
        res.isScalar = true;
        res.scalar = { acc, 0.0 };
        res.formattedUtf8 = detail_adv::FmtReal(acc);
        res.latexUtf8 = res.formattedUtf8;
        return res;
    }
    if (s.size() > 6 && s.compare(0, 5, "norm(") == 0 && s.back() == ')' &&
        parseVec1D(s.substr(5, s.size() - 6), va)) {
        double acc = 0.0;
        for (double v : va) acc += v * v;
        const double nm = std::sqrt(acc);
        res.ok = true;
        res.isScalar = true;
        res.scalar = { nm, 0.0 };
        res.formattedUtf8 = detail_adv::FmtReal(nm);
        res.latexUtf8 = res.formattedUtf8;
        return res;
    }

    int depth = 0;
    std::size_t opPos = std::string::npos;
    char opCh = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '[') ++depth;
        else if (s[i] == ']') --depth;
        else if (depth == 0 && (s[i] == '+' || s[i] == '-' || s[i] == '*')) {
            opPos = i; opCh = s[i]; break;
        }
    }
    if (opPos != std::string::npos) {
        ComplexMatrix B;
        if (!detail_adv::ParseComplexMatrixLiteral(s.substr(0, opPos), A) ||
            !detail_adv::ParseComplexMatrixLiteral(s.substr(opPos + 1), B)) {
            res.errorUtf8 = "Sintaxis matricial invalida";
            return res;
        }
        if (opCh == '+' || opCh == '-') {
            if (A.rows != B.rows || A.cols != B.cols) { res.errorUtf8 = "Dimensiones incompatibles"; return res; }
            ComplexMatrix C{ A.rows, A.cols, std::vector<std::complex<double>>(A.rows * A.cols) };
            for (std::size_t i = 0; i < C.data.size(); ++i) {
                C.data[i] = (opCh == '+') ? (A.data[i] + B.data[i]) : (A.data[i] - B.data[i]);
            }
            res.ok = true; res.matrix = std::move(C);
        } else {
            if (A.cols != B.rows) { res.errorUtf8 = "Dimensiones incompatibles en multiplicacion"; return res; }
            ComplexMatrix C{ A.rows, B.cols, std::vector<std::complex<double>>(A.rows * B.cols, { 0.0, 0.0 }) };
            for (std::size_t r = 0; r < A.rows; ++r)
                for (std::size_t k = 0; k < A.cols; ++k)
                    for (std::size_t c = 0; c < B.cols; ++c)
                        C.At(r, c) += A.At(r, k) * B.At(k, c);
            res.ok = true; res.matrix = std::move(C);
        }
        res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
        res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
        return res;
    }

    if (detail_adv::ParseComplexMatrixLiteral(s, A)) {
        res.ok = true;
        res.matrix = std::move(A);
        res.formattedUtf8 = detail_adv::FormatCMatText(res.matrix);
        res.latexUtf8 = detail_adv::FormatCMatLatex(res.matrix);
        return res;
    }
    res.errorUtf8 = "Formato matricial no reconocido";
    return res;
}

// ---------------- Port of tinta_nucleo/matematicas_avanzadas (U-15) ----------------

enum class LimitSide : int {
    Left = -1,
    Bilateral = 0,
    Right = 1
};

struct OdeSample {
    double t = 0.0;
    std::complex<double> y{ 0.0, 0.0 };
};

struct OdeResult {
    bool ok = false;
    std::vector<OdeSample> samples;
    std::string errorUtf8;
};

inline CasEvalResult NumericalDerivative(
    const std::string& expr,
    const std::string& var,
    double x,
    const SymbolEnv& env = {})
{
    const double h = (std::max)(1e-7, std::abs(x) * 1e-6);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast);
    CasEvalResult fa{}, fb{};
    if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, { x + h, 0.0 }, env, fa)) return fa;
    if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, { x - h, 0.0 }, env, fb)) return fb;
    CasEvalResult out = fa;
    out.hasAssignment = false;
    out.assignedVarUtf8.clear();
    out.result.value = (fa.result.value - fb.result.value) / (2.0 * h);
    out.formattedResultUtf8 = out.result.FormatUtf8(6);
    return out;
}

inline CasEvalResult NumericalIntegralSimpson(
    const std::string& expr,
    const std::string& var,
    double a,
    double b,
    int subintervals = 100,
    const SymbolEnv& env = {})
{
    CasEvalResult out{};
    if (subintervals < 2 || !std::isfinite(a) || !std::isfinite(b)) {
        out.errorUtf8 = "Se requieren al menos 2 subintervalos y limites finitos";
        return out;
    }
    if (subintervals % 2 != 0) ++subintervals;
    const double h = (b - a) / static_cast<double>(subintervals);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast);

    std::complex<double> sum(0.0, 0.0);
    UnitDim refDim{};
    for (int i = 0; i <= subintervals; ++i) {
        CasEvalResult r{};
        if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, { a + static_cast<double>(i) * h, 0.0 }, env, r)) {
            return r;
        }
        if (i == 0) refDim = r.result.dim;
        const double w = (i == 0 || i == subintervals) ? 1.0 : ((i % 2 != 0) ? 4.0 : 2.0);
        sum += r.result.value * w;
    }
    out.ok = true;
    out.result.value = sum * (h / 3.0);
    out.result.dim = refDim;
    out.formattedResultUtf8 = out.result.FormatUtf8(6);
    return out;
}

inline CasEvalResult FindRootNewton(
    const std::string& expr,
    const std::string& var,
    std::complex<double> initialGuess,
    const SymbolEnv& env = {})
{
    CasEvalResult out{};
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast);
    std::complex<double> x = initialGuess;

    for (int iter = 0; iter < 80; ++iter) {
        CasEvalResult f{};
        if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, x, env, f)) return f;
        if (std::abs(f.result.value) < 1e-11) {
            out.ok = true;
            out.result.value = x;
            out.formattedResultUtf8 = out.result.FormatUtf8(6);
            return out;
        }
        const double h = (std::max)(1e-7, std::abs(x) * 1e-6);
        CasEvalResult fa{}, fb{};
        if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, x + h, env, fa)) return fa;
        if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, x - h, env, fb)) return fb;
        const std::complex<double> deriv = (fa.result.value - fb.result.value) / (2.0 * h);
        if (std::abs(deriv) < 1e-14) {
            out.errorUtf8 = "Derivada casi nula en Newton-Raphson";
            return out;
        }
        const std::complex<double> next = x - f.result.value / deriv;
        if (std::abs(next - x) < 1e-12 * (1.0 + std::abs(x))) {
            out.ok = true;
            out.result.value = next;
            out.formattedResultUtf8 = out.result.FormatUtf8(6);
            return out;
        }
        x = next;
    }
    out.errorUtf8 = "La busqueda de raiz no convergio";
    return out;
}

inline CasEvalResult ComputeNumericalLimit(
    const std::string& expr,
    const std::string& var,
    double point,
    LimitSide side = LimitSide::Bilateral,
    const SymbolEnv& env = {})
{
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast);

    auto approxSide = [&](double sign) -> CasEvalResult {
        const double scale = (std::max)(1.0, std::abs(point));
        std::complex<double> prev{};
        bool hasPrev = false;
        int stableCount = 0;
        for (int k = 4; k <= 28; ++k) {
            const double h = scale * std::pow(0.5, k);
            CasEvalResult r{};
            if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, { point + sign * h, 0.0 }, env, r) ||
                !std::isfinite(r.result.value.real()) || !std::isfinite(r.result.value.imag()) ||
                std::abs(r.result.value) > 1e12) {
                hasPrev = false;
                stableCount = 0;
                continue;
            }
            const double tol = 1e-7 * (1.0 + std::abs(r.result.value));
            if (hasPrev && std::abs(r.result.value - prev) < tol) {
                ++stableCount;
                if (stableCount >= 2) {
                    // Verify boundedness & non-oscillation at irrational golden-ratio sub-steps
                    CasEvalResult g1{}, g2{};
                    const bool ok1 = detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, { point + sign * 0.6180339887498948 * h, 0.0 }, env, g1);
                    const bool ok2 = detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, { point + sign * 0.3819660112501052 * h, 0.0 }, env, g2);
                    const double gTol = 1e-5 * (1.0 + std::abs(r.result.value));
                    if (ok1 && ok2 &&
                        std::isfinite(g1.result.value.real()) && std::isfinite(g1.result.value.imag()) &&
                        std::isfinite(g2.result.value.real()) && std::isfinite(g2.result.value.imag()) &&
                        std::abs(g1.result.value - r.result.value) < gTol &&
                        std::abs(g2.result.value - r.result.value) < gTol) {
                        return r;
                    }
                    stableCount = 0;
                }
            } else {
                stableCount = 0;
            }
            prev = r.result.value;
            hasPrev = true;
        }
        CasEvalResult fail{};
        fail.errorUtf8 = "El limite no converge o diverge";
        return fail;
    };

    if (side == LimitSide::Left) return approxSide(-1.0);
    if (side == LimitSide::Right) return approxSide(1.0);
    const auto left = approxSide(-1.0);
    if (!left.ok) return left;
    const auto right = approxSide(1.0);
    if (!right.ok) return right;
    if (std::abs(left.result.value - right.result.value) >
        1e-5 * (1.0 + (std::max)(std::abs(left.result.value), std::abs(right.result.value)))) {
        CasEvalResult fail{};
        fail.errorUtf8 = "Los limites laterales no coinciden";
        return fail;
    }
    CasEvalResult out = left;
    out.result.value = 0.5 * (left.result.value + right.result.value);
    out.formattedResultUtf8 = out.result.FormatUtf8(6);
    return out;
}

inline CasEvalResult ComputeSummation(
    const std::string& expr,
    const std::string& var,
    std::int64_t startIdx,
    std::int64_t endIdx,
    const SymbolEnv& env = {})
{
    CasEvalResult out{};
    if (endIdx < startIdx) {
        out.errorUtf8 = "El limite final de sumatoria es menor que el inicial";
        return out;
    }
    if (endIdx - startIdx > 1'000'000) {
        out.errorUtf8 = "La sumatoria excede 1,000,000 terminos";
        return out;
    }
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast);
    std::complex<double> sum(0.0, 0.0);
    UnitDim refDim{};
    for (std::int64_t i = startIdx; i <= endIdx; ++i) {
        CasEvalResult r{};
        if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, var, { static_cast<double>(i), 0.0 }, env, r)) {
            return r;
        }
        if (i == startIdx) refDim = r.result.dim;
        sum += r.result.value;
    }
    out.ok = true;
    out.result.value = sum;
    out.result.dim = refDim;
    out.formattedResultUtf8 = out.result.FormatUtf8(6);
    return out;
}

namespace detail_fft {

inline void Radix2FftInplace(std::vector<std::complex<double>>& a, bool inverse) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    constexpr double kPi = 3.14159265358979323846;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = (inverse ? 2.0 : -2.0) * kPi / static_cast<double>(len);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            const std::size_t half = len >> 1;
            for (std::size_t j = 0; j < half; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + half] * w;
                a[i + j] = u + v;
                a[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        const double invN = 1.0 / static_cast<double>(n);
        for (auto& x : a) x *= invN;
    }
}

} // namespace detail_fft

// O(N log N) FFT: Radix-2 Cooley-Tukey for powers of 2, Bluestein chirp-z for arbitrary N
inline std::vector<std::complex<double>> ComputeFft(
    const std::vector<std::complex<double>>& samples,
    bool inverse = false)
{
    const std::size_t n = samples.size();
    if (n == 0) return {};
    if (n == 1) return samples;

    if ((n & (n - 1)) == 0) {
        std::vector<std::complex<double>> out = samples;
        detail_fft::Radix2FftInplace(out, inverse);
        return out;
    }

    // Bluestein algorithm: choose M = next power of two >= 2*n - 1
    std::size_t m = 1;
    while (m < 2 * n - 1) m <<= 1;

    constexpr double kPi = 3.14159265358979323846;
    const double sign = inverse ? 1.0 : -1.0;

    std::vector<std::complex<double>> chirp(n);
    std::vector<std::complex<double>> a(m, { 0.0, 0.0 });
    std::vector<std::complex<double>> b(m, { 0.0, 0.0 });

    for (std::size_t k = 0; k < n; ++k) {
        const double ang = sign * kPi * static_cast<double>(k * k) / static_cast<double>(n);
        chirp[k] = std::complex<double>(std::cos(ang), std::sin(ang));
        a[k] = samples[k] * chirp[k];
        b[k] = std::conj(chirp[k]);
        if (k > 0) b[m - k] = b[k];
    }

    detail_fft::Radix2FftInplace(a, false);
    detail_fft::Radix2FftInplace(b, false);
    for (std::size_t i = 0; i < m; ++i) a[i] *= b[i];
    detail_fft::Radix2FftInplace(a, true);

    std::vector<std::complex<double>> out(n);
    const double scale = inverse ? (1.0 / static_cast<double>(n)) : 1.0;
    for (std::size_t k = 0; k < n; ++k) {
        out[k] = a[k] * chirp[k] * scale;
    }
    return out;
}

inline CasEvalResult NumericalLaplaceTransform(
    const std::string& expr,
    const std::string& timeVar,
    std::complex<double> s,
    double tMax,
    int steps = 200,
    const SymbolEnv& env = {})
{
    CasEvalResult out{};
    if (!(tMax > 0.0) || steps < 2) {
        out.errorUtf8 = "Intervalo o pasos invalidos en Laplace numerica";
        return out;
    }
    if (steps % 2 != 0) ++steps;
    const double h = tMax / static_cast<double>(steps);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast);
    const std::string tv = timeVar.empty() ? "t" : timeVar;

    std::complex<double> sum(0.0, 0.0);
    for (int i = 0; i <= steps; ++i) {
        const double t = static_cast<double>(i) * h;
        CasEvalResult r{};
        if (!detail_adv::EvalLoopVar(expr, hasAst ? &ast : nullptr, tv, { t, 0.0 }, env, r)) return r;
        const double w = (i == 0 || i == steps) ? 1.0 : ((i % 2 != 0) ? 4.0 : 2.0);
        sum += w * r.result.value * std::exp(-s * t);
    }
    out.ok = true;
    out.result.value = sum * (h / 3.0);
    out.formattedResultUtf8 = out.result.FormatUtf8(6);
    return out;
}

inline OdeResult SolveFirstOrderOdeRk4(
    const std::string& rhsDerivExpr,
    double t0,
    std::complex<double> y0,
    double t1,
    std::size_t steps,
    const SymbolEnv& env = {},
    const std::string& tVar = "t",
    const std::string& yVar = "y")
{
    OdeResult res;
    if (steps == 0 || steps > 1'000'000 || !std::isfinite(t0) || !std::isfinite(t1)) {
        res.errorUtf8 = "Intervalo o pasos invalidos para EDO RK4";
        return res;
    }
    const double h = (t1 - t0) / static_cast<double>(steps);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(rhsDerivExpr, ast);
    const std::string tv = tVar.empty() ? "t" : tVar;
    const std::string yv = yVar.empty() ? "y" : yVar;

    auto evalRhs = [&](double t, std::complex<double> y, std::complex<double>& outDy) -> bool {
        SymbolEnv work = env;
        work[tv] = QuantityValue{ { t, 0.0 }, {}, "", 1.0, 0.0, false };
        work[yv] = QuantityValue{ y, {}, "", 1.0, 0.0, false };
        CasEvalResult ev{};
        if (hasAst && EvaluateAst(ast, work, ev) && ev.ok) {
            outDy = ev.result.value;
            return true;
        }
        ev = EvaluateExpression(rhsDerivExpr, work);
        if (!ev.ok) {
            res.errorUtf8 = ev.errorUtf8;
            return false;
        }
        outDy = ev.result.value;
        return true;
    };

    double t = t0;
    std::complex<double> y = y0;
    res.samples.reserve(steps + 1);
    res.samples.push_back({ t, y });

    for (std::size_t i = 0; i < steps; ++i) {
        std::complex<double> k1{}, k2{}, k3{}, k4{};
        if (!evalRhs(t, y, k1)) return res;
        if (!evalRhs(t + 0.5 * h, y + 0.5 * h * k1, k2)) return res;
        if (!evalRhs(t + 0.5 * h, y + 0.5 * h * k2, k3)) return res;
        if (!evalRhs(t + h, y + h * k3, k4)) return res;
        y += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        t = t0 + static_cast<double>(i + 1) * h;
        res.samples.push_back({ t, y });
    }
    res.ok = true;
    return res;
}

} // namespace DocStudio::Cas
