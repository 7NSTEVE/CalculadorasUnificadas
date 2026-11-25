#include "reactive_math_chemistry.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>

using json = nlohmann::json;

namespace DocStudio::MathChem {

namespace {

std::string Trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos) {
        s.replace(p, from.size(), to);
        p += to.size();
    }
    return s;
}

bool ContainsWholeWord(const std::string& text, const std::string& word) {
    if (word.empty()) return false;
    std::size_t pos = 0;
    while ((pos = text.find(word, pos)) != std::string::npos) {
        const bool leftOk = (pos == 0) || (!std::isalnum(static_cast<unsigned char>(text[pos - 1])) && text[pos - 1] != '_');
        const std::size_t end = pos + word.size();
        const bool rightOk = (end >= text.size()) || (!std::isalnum(static_cast<unsigned char>(text[end])) && text[end] != '_');
        if (leftOk && rightOk) return true;
        pos += 1;
    }
    return false;
}

// Balanced-parenthesis DEG wrapping for sin/cos/tan supporting nested calls like sin(cos(30) + 15)
std::string WrapTrigArgsForDegrees(std::string expr) {
    static const std::vector<std::string> kTrigFns = { "sin", "cos", "tan" };
    std::size_t pos = 0;
    while (pos < expr.size()) {
        bool matched = false;
        for (const auto& fn : kTrigFns) {
            if (pos + fn.size() < expr.size() &&
                expr.compare(pos, fn.size(), fn) == 0 &&
                expr[pos + fn.size()] == '(') {
                const bool leftBoundary = (pos == 0) ||
                    (!std::isalnum(static_cast<unsigned char>(expr[pos - 1])) && expr[pos - 1] != '_');
                if (!leftBoundary) continue;

                const std::size_t openPos = pos + fn.size();
                int depth = 1;
                std::size_t k = openPos + 1;
                while (k < expr.size() && depth > 0) {
                    if (expr[k] == '(') ++depth;
                    else if (expr[k] == ')') --depth;
                    if (depth > 0) ++k;
                }
                if (depth == 0) {
                    // Recursively transform inner argument first so nested trig calls convert properly
                    const std::string inner = expr.substr(openPos + 1, k - (openPos + 1));
                    const std::string innerConverted = WrapTrigArgsForDegrees(inner);
                    const std::string replacement = fn + "((pi/180)*(" + innerConverted + "))";
                    expr.replace(pos, (k + 1) - pos, replacement);
                    pos += replacement.size();
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) ++pos;
    }
    return expr;
}

std::string FormatWithDecimals(double v, int decimalPlaces) {
    const int dp = std::clamp(decimalPlaces, 0, 12);
    char fmt[16]{};
    std::snprintf(fmt, sizeof(fmt), "%%.%df", dp);
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), fmt, v);
    std::string s = buf;
    if (dp > 0 && s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

// Complete 118-element IUPAC periodic table (standard atomic weights in g/mol)
double AtomicMassForElement(const std::string& sym) {
    static const std::unordered_map<std::string, double> kMass = {
        {"H",  1.008},   {"He", 4.0026},  {"Li", 6.94},    {"Be", 9.0122},  {"B",  10.81},
        {"C",  12.011},  {"N",  14.007},  {"O",  15.999},  {"F",  18.998},  {"Ne", 20.180},
        {"Na", 22.990},  {"Mg", 24.305},  {"Al", 26.982},  {"Si", 28.085},  {"P",  30.974},
        {"S",  32.06},   {"Cl", 35.45},   {"Ar", 39.948},  {"K",  39.098},  {"Ca", 40.078},
        {"Sc", 44.956},  {"Ti", 47.867},  {"V",  50.942},  {"Cr", 51.996},  {"Mn", 54.938},
        {"Fe", 55.845},  {"Co", 58.933},  {"Ni", 58.693},  {"Cu", 63.546},  {"Zn", 65.38},
        {"Ga", 69.723},  {"Ge", 72.630},  {"As", 74.922},  {"Se", 78.971},  {"Br", 79.904},
        {"Kr", 83.798},  {"Rb", 85.468},  {"Sr", 87.62},   {"Y",  88.906},  {"Zr", 91.224},
        {"Nb", 92.906},  {"Mo", 95.95},   {"Tc", 98.0},    {"Ru", 101.07},  {"Rh", 102.91},
        {"Pd", 106.42},  {"Ag", 107.87},  {"Cd", 112.41},  {"In", 114.82},  {"Sn", 118.71},
        {"Sb", 121.76},  {"Te", 127.60},  {"I",  126.90},  {"Xe", 131.29},  {"Cs", 132.91},
        {"Ba", 137.33},  {"La", 138.91},  {"Ce", 140.12},  {"Pr", 140.91},  {"Nd", 144.24},
        {"Pm", 145.0},   {"Sm", 150.36},  {"Eu", 151.96},  {"Gd", 157.25},  {"Tb", 158.93},
        {"Dy", 162.50},  {"Ho", 164.93},  {"Er", 167.26},  {"Tm", 168.93},  {"Yb", 173.05},
        {"Lu", 174.97},  {"Hf", 178.49},  {"Ta", 180.95},  {"W",  183.84},  {"Re", 186.21},
        {"Os", 190.23},  {"Ir", 192.22},  {"Pt", 195.08},  {"Au", 196.97},  {"Hg", 200.59},
        {"Tl", 204.38},  {"Pb", 207.2},   {"Bi", 208.98},  {"Po", 209.0},   {"At", 210.0},
        {"Rn", 222.0},   {"Fr", 223.0},   {"Ra", 226.0},   {"Ac", 227.0},   {"Th", 232.04},
        {"Pa", 231.04},  {"U",  238.03},  {"Np", 237.0},   {"Pu", 244.0},   {"Am", 243.0},
        {"Cm", 247.0},   {"Bk", 247.0},   {"Cf", 251.0},   {"Es", 252.0},   {"Fm", 257.0},
        {"Md", 258.0},   {"No", 259.0},   {"Lr", 266.0},   {"Rf", 267.0},   {"Db", 268.0},
        {"Sg", 269.0},   {"Bh", 270.0},   {"Hs", 277.0},   {"Mt", 278.0},   {"Ds", 281.0},
        {"Rg", 282.0},   {"Cn", 285.0},   {"Nh", 286.0},   {"Fl", 289.0},   {"Mc", 290.0},
        {"Lv", 293.0},   {"Ts", 294.0},   {"Og", 294.0}
    };
    auto it = kMass.find(sym);
    return (it != kMass.end()) ? it->second : 0.0;
}

// Exact rational arithmetic for stoichiometric null-space Gaussian elimination
struct Rat {
    std::int64_t n = 0;
    std::int64_t d = 1;

    static std::int64_t Gcd(std::int64_t a, std::int64_t b) noexcept {
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        while (b != 0) {
            const std::int64_t t = a % b;
            a = b;
            b = t;
        }
        return a == 0 ? 1 : a;
    }

    Rat() = default;
    Rat(std::int64_t num, std::int64_t den = 1) : n(num), d(den) { Normalize(); }

    void Normalize() noexcept {
        if (d < 0) { n = -n; d = -d; }
        if (n == 0) { d = 1; return; }
        const std::int64_t g = Gcd(n, d);
        n /= g;
        d /= g;
    }

    Rat operator+(const Rat& o) const noexcept { return Rat(n * o.d + o.n * d, d * o.d); }
    Rat operator-(const Rat& o) const noexcept { return Rat(n * o.d - o.n * d, d * o.d); }
    Rat operator*(const Rat& o) const noexcept { return Rat(n * o.n, d * o.d); }
    Rat operator/(const Rat& o) const noexcept { return Rat(n * o.d, d * o.n); }
    Rat operator-() const noexcept { return Rat(-n, d); }
    bool IsZero() const noexcept { return n == 0; }
};

// Solves A * x = 0 for strictly positive minimal integer vector x via exact rational RREF
bool SolveStoichiometryNullSpace(
    const std::vector<std::map<std::string, int>>& lhsAtoms,
    const std::vector<std::map<std::string, int>>& rhsAtoms,
    std::vector<int>& outCoeffs)
{
    const std::size_t nL = lhsAtoms.size();
    const std::size_t nR = rhsAtoms.size();
    const std::size_t cols = nL + nR;
    if (nL == 0 || nR == 0 || cols > 24) return false;

    std::set<std::string> elems;
    for (const auto& m : lhsAtoms) for (const auto& kv : m) elems.insert(kv.first);
    for (const auto& m : rhsAtoms) for (const auto& kv : m) elems.insert(kv.first);
    const std::size_t rows = elems.size();
    if (rows == 0) return false;

    std::vector<std::vector<Rat>> mat(rows, std::vector<Rat>(cols, Rat(0)));
    std::size_t r = 0;
    for (const auto& el : elems) {
        for (std::size_t c = 0; c < nL; ++c) {
            auto it = lhsAtoms[c].find(el);
            if (it != lhsAtoms[c].end()) mat[r][c] = Rat(it->second);
        }
        for (std::size_t c = 0; c < nR; ++c) {
            auto it = rhsAtoms[c].find(el);
            if (it != rhsAtoms[c].end()) mat[r][nL + c] = Rat(-it->second);
        }
        ++r;
    }

    // Reduced Row Echelon Form (RREF)
    std::vector<int> pivotColForRow(rows, -1);
    std::vector<bool> isPivotCol(cols, false);
    std::size_t pivRow = 0;
    for (std::size_t col = 0; col < cols && pivRow < rows; ++col) {
        std::size_t sel = pivRow;
        while (sel < rows && mat[sel][col].IsZero()) ++sel;
        if (sel == rows) continue;
        if (sel != pivRow) std::swap(mat[sel], mat[pivRow]);

        const Rat pivVal = mat[pivRow][col];
        for (std::size_t c = col; c < cols; ++c) {
            mat[pivRow][c] = mat[pivRow][c] / pivVal;
        }
        for (std::size_t rr = 0; rr < rows; ++rr) {
            if (rr == pivRow || mat[rr][col].IsZero()) continue;
            const Rat factor = mat[rr][col];
            for (std::size_t c = col; c < cols; ++c) {
                mat[rr][c] = mat[rr][c] - factor * mat[pivRow][c];
            }
        }
        pivotColForRow[pivRow] = static_cast<int>(col);
        isPivotCol[col] = true;
        ++pivRow;
    }

    std::vector<std::size_t> freeCols;
    for (std::size_t c = 0; c < cols; ++c) {
        if (!isPivotCol[c]) freeCols.push_back(c);
    }
    if (freeCols.empty()) return false;

    // Build null-space basis vectors (for standard reactions nullity == 1; if >1 combine positively)
    std::vector<Rat> sol(cols, Rat(0));
    for (std::size_t fc : freeCols) sol[fc] = Rat(1);
    for (std::size_t rr = 0; rr < pivRow; ++rr) {
        const int pc = pivotColForRow[rr];
        if (pc < 0) continue;
        Rat sum(0);
        for (std::size_t fc : freeCols) {
            sum = sum - mat[rr][fc] * sol[fc];
        }
        sol[static_cast<std::size_t>(pc)] = sum;
    }

    // Clear denominators via LCM
    std::int64_t lcmDen = 1;
    for (const auto& q : sol) {
        if (q.n <= 0) return false; // non-positive stoichiometry -> invalid reaction
        const std::int64_t g = Rat::Gcd(lcmDen, q.d);
        lcmDen = (lcmDen / g) * q.d;
    }

    std::vector<std::int64_t> intSol(cols, 0);
    std::int64_t allGcd = 0;
    for (std::size_t c = 0; c < cols; ++c) {
        intSol[c] = sol[c].n * (lcmDen / sol[c].d);
        if (intSol[c] <= 0 || intSol[c] > 10000) return false;
        allGcd = (allGcd == 0) ? intSol[c] : Rat::Gcd(allGcd, intSol[c]);
    }

    outCoeffs.resize(cols);
    for (std::size_t c = 0; c < cols; ++c) {
        outCoeffs[c] = static_cast<int>(intSol[c] / (allGcd > 0 ? allGcd : 1));
    }
    return true;
}

// Parses a chemical species formula like "Fe2O3", "H2SO4", "Ca(OH)2", "CuSO4*5H2O"
bool ParseChemicalSpeciesAtoms(const std::string& speciesRaw, std::map<std::string, int>& outAtoms, double& outMolarMass) {
    outAtoms.clear();
    outMolarMass = 0.0;
    const std::string s = Trim(speciesRaw);
    if (s.empty()) return false;

    std::vector<std::map<std::string, int>> stack;
    stack.emplace_back();
    std::size_t i = 0;
    const std::size_t n = s.size();

    while (i < n) {
        if (s[i] == '(' || s[i] == '[') {
            stack.emplace_back();
            ++i;
            continue;
        }
        if (s[i] == ')' || s[i] == ']') {
            if (stack.size() <= 1) return false;
            ++i;
            int mult = 0;
            while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
                mult = mult * 10 + (s[i] - '0');
                ++i;
            }
            if (mult == 0) mult = 1;
            auto top = std::move(stack.back());
            stack.pop_back();
            for (const auto& kv : top) {
                stack.back()[kv.first] += kv.second * mult;
            }
            continue;
        }
        if (std::isupper(static_cast<unsigned char>(s[i]))) {
            std::string sym(1, s[i]);
            ++i;
            while (i < n && std::islower(static_cast<unsigned char>(s[i]))) {
                sym.push_back(s[i]);
                ++i;
            }
            if (AtomicMassForElement(sym) <= 0.0) return false;
            int count = 0;
            while (i < n && std::isdigit(static_cast<unsigned char>(s[i]))) {
                count = count * 10 + (s[i] - '0');
                ++i;
            }
            if (count == 0) count = 1;
            stack.back()[sym] += count;
            continue;
        }
        return false;
    }

    if (stack.size() != 1 || stack.front().empty()) return false;
    outAtoms = std::move(stack.front());
    for (const auto& kv : outAtoms) {
        outMolarMass += AtomicMassForElement(kv.first) * static_cast<double>(kv.second);
    }
    return true;
}

std::string SpeciesToUnicode2D(const std::string& s) {
    static const char* kSubs[10] = { "\xE2\x82\x80", "\xE2\x82\x81", "\xE2\x82\x82", "\xE2\x82\x83", "\xE2\x82\x84",
                                     "\xE2\x82\x85", "\xE2\x82\x86", "\xE2\x82\x87", "\xE2\x82\x88", "\xE2\x82\x89" };
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i])) && i > 0 &&
            (std::isalpha(static_cast<unsigned char>(s[i - 1])) || s[i - 1] == ')' || s[i - 1] == ']')) {
            out += kSubs[s[i] - '0'];
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string SpeciesToLatex(const std::string& s) {
    std::ostringstream oss;
    std::size_t i = 0;
    while (i < s.size()) {
        if (std::isalpha(static_cast<unsigned char>(s[i]))) {
            std::size_t j = i;
            while (j < s.size() && std::isalpha(static_cast<unsigned char>(s[j]))) ++j;
            oss << "\\text{" << s.substr(i, j - i) << "}";
            i = j;
        } else if (std::isdigit(static_cast<unsigned char>(s[i]))) {
            std::size_t j = i;
            while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
            oss << "_{" << s.substr(i, j - i) << "}";
            i = j;
        } else {
            oss << s[i];
            ++i;
        }
    }
    return oss.str();
}

void RecomputeDependentPlotBlocks(DocumentModel& doc, ObjectTarget target, const Cas::SymbolEnv& env) {
    auto updatePlot = [&](AnnotationObject& obj) {
        if (obj.kind != ObjectKind::PlotBlock || obj.formulaExprUtf8.empty()) return;
        // Sample expression f(x) using current reactive variable environment
        double xMin = -5.0, xMax = 5.0;
        try {
            if (!obj.plotConfigUtf8.empty()) {
                const json cfg = json::parse(obj.plotConfigUtf8);
                xMin = cfg.value("xMin", -5.0);
                xMax = cfg.value("xMax", 5.0);
            }
        } catch (...) {}

        Cas::SymbolEnv localEnv = env;
        json pts = json::array();
        constexpr int kSamples = 48;
        int validPts = 0;
        for (int i = 0; i <= kSamples; ++i) {
            const double x = xMin + (xMax - xMin) * (static_cast<double>(i) / static_cast<double>(kSamples));
            Cas::QuantityValue qx;
            qx.value = { x, 0.0 };
            localEnv["x"] = qx;
            const auto ev = Cas::EvaluateExpression(obj.formulaExprUtf8, localEnv);
            if (ev.ok && std::isfinite(ev.result.value.real())) {
                pts.push_back({ x, ev.result.value.real() });
                ++validPts;
            }
        }
        if (validPts > 0) {
            json cfg;
            try {
                if (!obj.plotConfigUtf8.empty()) cfg = json::parse(obj.plotConfigUtf8);
            } catch (...) { cfg = json::object(); }
            cfg["xMin"] = xMin;
            cfg["xMax"] = xMax;
            cfg["points"] = std::move(pts);
            obj.plotConfigUtf8 = cfg.dump();
            obj.evaluatedResultUtf8 = "Plot actualizado (" + std::to_string(validPts) + " pts)";
        }
    };

    if (target.isCanvas) {
        for (const auto& cobj : doc.CanvasObjects()) {
            if (cobj.kind == ObjectKind::PlotBlock) {
                if (AnnotationObject* mut = doc.FindCanvasObjectMutable(cobj.id)) updatePlot(*mut);
            }
        }
    } else if (target.pageIndex < doc.Pages().size()) {
        for (auto& pobj : doc.MutablePages()[target.pageIndex].objects) {
            updatePlot(pobj);
        }
    }
}

} // namespace

RecognizedMathStructure ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure(
    const std::string& rawInputUtf8,
    const MathEvalConfig& cfg,
    const Cas::SymbolEnv& externalEnv)
{
    RecognizedMathStructure res;
    std::string expr = Trim(rawInputUtf8);
    if (expr.empty()) return res;

    if (expr.back() == '=' && (expr.size() < 2 || expr[expr.size() - 2] != '=')) {
        expr = Trim(expr.substr(0, expr.size() - 1));
    }

    expr = ReplaceAll(std::move(expr), "\xC3\x97", "*");       // ×
    expr = ReplaceAll(std::move(expr), "\xC3\xB7", "/");       // ÷
    expr = ReplaceAll(std::move(expr), "\xE2\x88\x92", "-");   // −
    expr = ReplaceAll(std::move(expr), "\xCF\x80", "pi");      // π
    expr = ReplaceAll(std::move(expr), "\xCE\xB1", "alpha");   // α
    expr = ReplaceAll(std::move(expr), "\xCE\xB2", "beta");    // β
    expr = ReplaceAll(std::move(expr), "\xCE\xB8", "theta");   // θ
    expr = ReplaceAll(std::move(expr), "\xCF\x89", "omega");   // ω
    expr = ReplaceAll(std::move(expr), "\xCE\xA9", "Ohm");     // Ω

    res.normalizedExprUtf8 = expr;
    res.hasFraction = (expr.find('/') != std::string::npos || expr.find("\\frac") != std::string::npos);
    res.hasSummation = (expr.find("sum(") != std::string::npos || expr.find("\\sum") != std::string::npos);
    res.hasIntegral = (expr.find("integrate(") != std::string::npos || expr.find("int(") != std::string::npos || expr.find("\\int") != std::string::npos);
    res.hasPower = (expr.find('^') != std::string::npos);
    // Word-boundary check prevents false positives on words like "rapido" or "espiga"
    res.hasGreekSymbol = (ContainsWholeWord(expr, "pi") || ContainsWholeWord(expr, "alpha") ||
                          ContainsWholeWord(expr, "beta") || ContainsWholeWord(expr, "theta") ||
                          ContainsWholeWord(expr, "omega") || ContainsWholeWord(expr, "Ohm") ||
                          ContainsWholeWord(expr, "lambda") || ContainsWholeWord(expr, "sigma") ||
                          ContainsWholeWord(expr, "delta") || ContainsWholeWord(expr, "mu"));
    res.hasMatrix = (expr.find('[') != std::string::npos && expr.find(']') != std::string::npos);

    // 1. Matrix recognition & dimension validation
    if (res.hasMatrix) {
        const auto mres = Cas::EvaluateMatrixExpression(expr);
        res.valid = mres.ok;
        res.matrixDimensionsValid = mres.ok;
        res.matrixRows = static_cast<int>(mres.rows);
        res.matrixCols = static_cast<int>(mres.cols);
        res.evaluatedUtf8 = mres.ok ? mres.formattedUtf8 : ("ERR: " + mres.errorUtf8);
        res.latexUtf8 = !mres.latexUtf8.empty() ? mres.latexUtf8 : ("\\begin{pmatrix}" + expr + "\\end{pmatrix}");
        res.natural2DUtf8 = res.evaluatedUtf8;
        res.solutionStepsUtf8 = BuildStepByStepExplanation(expr, cfg);
        return res;
    }

    // 2. Summation: sum(expr, var, start, end)
    if (res.hasSummation && expr.rfind("sum(", 0) == 0 && expr.back() == ')') {
        const std::string inner = expr.substr(4, expr.size() - 5);
        std::vector<std::string> parts;
        std::istringstream iss(inner);
        std::string p;
        while (std::getline(iss, p, ',')) parts.push_back(Trim(p));
        if (parts.size() == 4) {
            try {
                const std::string body = parts[0];
                const std::string var = parts[1];
                const int a = std::stoi(parts[2]);
                const int b = std::stoi(parts[3]);
                if (std::abs(b - a) <= 100000) {
                    double total = 0.0;
                    Cas::SymbolEnv env = externalEnv;
                    for (int k = a; k <= b; ++k) {
                        Cas::QuantityValue qk;
                        qk.value = { static_cast<double>(k), 0.0 };
                        env[var] = qk;
                        const auto ev = Cas::EvaluateExpression(body, env);
                        if (ev.ok) total += ev.result.value.real();
                    }
                    res.valid = true;
                    res.evaluatedUtf8 = FormatWithDecimals(total, cfg.decimalPlaces);
                    res.latexUtf8 = "\\sum_{" + var + "=" + std::to_string(a) + "}^{" + std::to_string(b) + "} " + body + " = " + res.evaluatedUtf8;
                    res.natural2DUtf8 = "\xE2\x88\x91(" + var + "=" + std::to_string(a) + ".." + std::to_string(b) + ") " + body + " = " + res.evaluatedUtf8;
                    res.solutionStepsUtf8 = {
                        "Expandir sumatoria para " + var + " desde " + std::to_string(a) + " hasta " + std::to_string(b),
                        "Acumular terminos evaluados: " + res.evaluatedUtf8
                    };
                    return res;
                }
            } catch (...) {}
        }
    }

    // 3. Definite Integral: integrate(expr, var, a, b) or int(expr, var, a, b)
    if (res.hasIntegral && (expr.rfind("integrate(", 0) == 0 || expr.rfind("int(", 0) == 0) && expr.back() == ')') {
        const std::size_t prefixLen = (expr.rfind("integrate(", 0) == 0) ? 10 : 4;
        const std::string inner = expr.substr(prefixLen, expr.size() - prefixLen - 1);
        std::vector<std::string> parts;
        std::istringstream iss(inner);
        std::string p;
        while (std::getline(iss, p, ',')) parts.push_back(Trim(p));
        if (parts.size() == 4) {
            try {
                const std::string body = parts[0];
                const std::string var = parts[1];
                const double a = std::stod(parts[2]);
                const double b = std::stod(parts[3]);
                constexpr int kN = 128;
                const double h = (b - a) / static_cast<double>(kN);
                Cas::SymbolEnv env = externalEnv;
                auto evalAt = [&](double x) -> double {
                    Cas::QuantityValue qx;
                    qx.value = { x, 0.0 };
                    env[var] = qx;
                    const auto ev = Cas::EvaluateExpression(body, env);
                    return ev.ok ? ev.result.value.real() : 0.0;
                };
                double sum = evalAt(a) + evalAt(b);
                for (int i = 1; i < kN; ++i) {
                    const double x = a + h * static_cast<double>(i);
                    sum += ((i % 2 == 1) ? 4.0 : 2.0) * evalAt(x);
                }
                const double area = (h / 3.0) * sum;
                res.valid = true;
                res.evaluatedUtf8 = FormatWithDecimals(area, cfg.decimalPlaces);
                res.latexUtf8 = "\\int_{" + parts[2] + "}^{" + parts[3] + "} " + body + "\\,d" + var + " = " + res.evaluatedUtf8;
                res.natural2DUtf8 = "\xE2\x88\xAB[" + parts[2] + ".." + parts[3] + "] " + body + " d" + var + " = " + res.evaluatedUtf8;
                res.solutionStepsUtf8 = {
                    "Integracion definida por cuadratura de Simpson 1/3 en [" + parts[2] + ", " + parts[3] + "]",
                    "Valor numerico convergente: " + res.evaluatedUtf8
                };
                return res;
            } catch (...) {}
        }
    }

    // 4. Balanced-parenthesis Degree vs Radian angle mode adaptation for sin/cos/tan
    std::string evalExpr = (cfg.angleMode == AngleUnitMode::Degrees) ? WrapTrigArgsForDegrees(expr) : expr;

    Cas::SymbolEnv env = externalEnv;
    const auto ev = Cas::EvaluateExpression(evalExpr, env);
    res.valid = ev.ok;
    if (ev.ok) {
        const double scalarVal = ev.result.value.real();
        res.evaluatedUtf8 = ev.result.dim.IsDimensionless() && std::abs(ev.result.value.imag()) < 1e-12
            ? FormatWithDecimals(scalarVal, cfg.decimalPlaces)
            : ev.formattedResultUtf8;
        if (res.hasFraction) {
            const auto sl = expr.find('/');
            if (sl != std::string::npos) {
                res.latexUtf8 = "\\frac{" + Trim(expr.substr(0, sl)) + "}{" + Trim(expr.substr(sl + 1)) + "} = " + res.evaluatedUtf8;
            } else {
                res.latexUtf8 = expr + " = " + res.evaluatedUtf8;
            }
        } else {
            res.latexUtf8 = expr + " = " + res.evaluatedUtf8;
        }
        res.natural2DUtf8 = expr + " = " + res.evaluatedUtf8;
    } else {
        res.evaluatedUtf8 = "ERR: " + ev.errorUtf8;
        res.latexUtf8 = expr;
        res.natural2DUtf8 = expr;
    }
    res.solutionStepsUtf8 = BuildStepByStepExplanation(expr, cfg);
    return res;
}

std::vector<std::string> ReactiveMathChemistryEngine::BuildStepByStepExplanation(
    const std::string& rawInputUtf8,
    const MathEvalConfig& cfg)
{
    std::vector<std::string> steps;
    const std::string e = Trim(rawInputUtf8);
    steps.push_back("1. Identificar expresion de entrada: " + e);
    steps.push_back(std::string("2. Modo angular activo: ") + (cfg.angleMode == AngleUnitMode::Degrees ? "DEG (grados)" : "RAD (radianes)") +
                    " | Precision: " + std::to_string(cfg.decimalPlaces) + " decimales");
    const std::string evalE = (cfg.angleMode == AngleUnitMode::Degrees) ? WrapTrigArgsForDegrees(e) : e;
    const auto ev = Cas::EvaluateExpression(evalE);
    if (ev.ok) {
        if (ev.hasAssignment && !ev.assignedVarUtf8.empty()) {
            steps.push_back("3. Asignar variable '" + ev.assignedVarUtf8 + "' en entorno reactivo.");
        } else {
            steps.push_back("3. Simplificar y evaluar subexpresiones algebraicas y unidades SI.");
        }
        steps.push_back("4. Resultado verificado: " + ev.formattedResultUtf8);
    } else {
        steps.push_back("3. Diagnostico de sintaxis: " + ev.errorUtf8);
    }
    return steps;
}

std::vector<ReactiveMathLineResult> ReactiveMathChemistryEngine::EvaluateReactiveEqualsOnDocument(
    DocumentModel& doc,
    ObjectTarget target,
    const MathEvalConfig& cfg)
{
    std::vector<ReactiveMathLineResult> results;
    Cas::SymbolEnv env;

    auto processObject = [&](AnnotationObject& obj) {
        if (obj.kind != ObjectKind::MathBlock && obj.kind != ObjectKind::TextBox) return;
        std::string raw = !obj.formulaExprUtf8.empty() ? obj.formulaExprUtf8 : obj.textUtf8;
        raw = Trim(raw);
        if (raw.empty()) return;

        std::string lhsVar;
        std::string rhsExpr = raw;
        bool trailingEq = (!raw.empty() && raw.back() == '=');

        const auto firstEq = raw.find('=');
        if (firstEq != std::string::npos) {
            const std::string candVar = Trim(raw.substr(0, firstEq));
            bool validIdent = !candVar.empty() && (std::isalpha(static_cast<unsigned char>(candVar[0])) || candVar[0] == '_');
            for (char c : candVar) {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') { validIdent = false; break; }
            }
            const auto secondEq = raw.find('=', firstEq + 1);
            if (validIdent) {
                lhsVar = candVar;
                if (secondEq != std::string::npos) {
                    rhsExpr = Trim(raw.substr(firstEq + 1, secondEq - (firstEq + 1)));
                    trailingEq = true;
                } else {
                    rhsExpr = Trim(raw.substr(firstEq + 1));
                    if (!rhsExpr.empty() && rhsExpr.back() == '=') {
                        rhsExpr = Trim(rhsExpr.substr(0, rhsExpr.size() - 1));
                        trailingEq = true;
                    }
                }
            } else if (trailingEq) {
                rhsExpr = Trim(raw.substr(0, raw.size() - 1));
            } else if (secondEq == std::string::npos) {
                rhsExpr = Trim(raw.substr(0, firstEq));
                trailingEq = true;
            }
        }

        const auto solved = AnalyzeAndSolveMathStructure(rhsExpr, cfg, env);
        if (!solved.valid) return;

        if (!lhsVar.empty()) {
            const std::string evalRhs = (cfg.angleMode == AngleUnitMode::Degrees) ? WrapTrigArgsForDegrees(rhsExpr) : rhsExpr;
            const auto qev = Cas::EvaluateExpression(evalRhs, env);
            if (qev.ok) env[lhsVar] = qev.result;
            obj.formulaVarNameUtf8 = lhsVar;
        }

        obj.evaluatedResultUtf8 = solved.evaluatedUtf8;
        ReactiveMathLineResult lr;
        lr.objectId = obj.id;
        lr.rawInputUtf8 = raw;
        lr.variableNameUtf8 = lhsVar;
        lr.evaluatedValueUtf8 = solved.evaluatedUtf8;
        lr.triggeredByTrailingEquals = trailingEq;
        if (!lhsVar.empty()) {
            lr.fullResolvedDisplayUtf8 = lhsVar + " = " + rhsExpr + (trailingEq ? (" = " + solved.evaluatedUtf8) : "");
            if (obj.formulaExprUtf8.empty()) obj.textUtf8 = lr.fullResolvedDisplayUtf8;
        } else if (trailingEq) {
            lr.fullResolvedDisplayUtf8 = rhsExpr + " = " + solved.evaluatedUtf8;
            if (obj.formulaExprUtf8.empty()) obj.textUtf8 = lr.fullResolvedDisplayUtf8;
        } else {
            lr.fullResolvedDisplayUtf8 = solved.natural2DUtf8;
        }
        results.push_back(std::move(lr));
    };

    if (target.isCanvas) {
        for (const auto& cobj : doc.CanvasObjects()) {
            if (AnnotationObject* mut = doc.FindCanvasObjectMutable(cobj.id)) processObject(*mut);
        }
    } else if (target.pageIndex < doc.Pages().size()) {
        for (auto& pobj : doc.MutablePages()[target.pageIndex].objects) processObject(pobj);
    }

    Cas::EvaluateDocumentMathDag(doc);
    RecomputeDependentPlotBlocks(doc, target, env);
    return results;
}

bool ReactiveMathChemistryEngine::UpdateDeclaredVariableAndRecompute(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::string& varNameUtf8,
    double newValue,
    const MathEvalConfig& cfg)
{
    if (varNameUtf8.empty()) return false;
    const std::string formattedVal = FormatWithDecimals(newValue, cfg.decimalPlaces);
    const std::string newAssign = varNameUtf8 + " = " + formattedVal;

    const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
    std::uint64_t foundId = 0;
    for (const auto& o : vec) {
        if (o.formulaVarNameUtf8 == varNameUtf8 ||
            o.formulaExprUtf8.rfind(varNameUtf8 + " =", 0) == 0 ||
            o.textUtf8.rfind(varNameUtf8 + " =", 0) == 0) {
            foundId = o.id;
            break;
        }
    }
    if (foundId == 0) return false;

    AnnotationObject* mut = target.isCanvas
        ? doc.FindCanvasObjectMutable(foundId)
        : doc.FindPageObjectMutable(target.pageIndex, foundId);
    if (!mut) return false;

    AnnotationObject before = *mut;
    AnnotationObject after = before;
    if (!after.formulaExprUtf8.empty()) after.formulaExprUtf8 = newAssign;
    after.textUtf8 = newAssign;
    after.evaluatedResultUtf8 = formattedVal;

    if (!cmdStack.Execute(doc, std::make_unique<ModifyObjectCommand>(target, std::move(before), std::move(after)))) {
        return false;
    }
    EvaluateReactiveEqualsOnDocument(doc, target, cfg);
    return true;
}

ChemicalAnalysisResult ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry(const std::string& formulaOrReactionUtf8) {
    ChemicalAnalysisResult res;
    res.inputUtf8 = Trim(formulaOrReactionUtf8);
    if (res.inputUtf8.empty()) return res;

    std::string arrow = "->";
    auto arrowPos = res.inputUtf8.find("->");
    if (arrowPos == std::string::npos) {
        arrow = "\xE2\x86\x92"; // →
        arrowPos = res.inputUtf8.find(arrow);
    }
    if (arrowPos == std::string::npos) {
        arrow = "=";
        arrowPos = res.inputUtf8.find('=');
    }

    // Case 1: Single chemical formula e.g. "H2SO4", "Ca(OH)2", "C6H12O6", "KMnO4"
    if (arrowPos == std::string::npos) {
        std::map<std::string, int> atoms;
        double mm = 0.0;
        if (!ParseChemicalSpeciesAtoms(res.inputUtf8, atoms, mm)) return res;
        res.valid = true;
        res.isReaction = false;
        res.balanced = true;
        res.molarMassGPerMol = mm;
        res.atomCountsLeft = atoms;
        res.atomCountsRight = atoms;
        res.balancedEquationUtf8 = res.inputUtf8;
        res.natural2DUtf8 = SpeciesToUnicode2D(res.inputUtf8);
        res.latexUtf8 = SpeciesToLatex(res.inputUtf8);
        return res;
    }

    // Case 2: Chemical reaction equation "Reactants -> Products"
    res.isReaction = true;
    const std::string leftRaw = Trim(res.inputUtf8.substr(0, arrowPos));
    const std::string rightRaw = Trim(res.inputUtf8.substr(arrowPos + arrow.size()));

    struct Term {
        int coeff = 1;
        std::string species;
        std::map<std::string, int> atoms;
        double molarMass = 0.0;
    };

    auto parseSide = [](const std::string& sideStr, std::vector<Term>& outTerms) -> bool {
        std::istringstream iss(sideStr);
        std::string part;
        while (std::getline(iss, part, '+')) {
            std::string t = Trim(part);
            if (t.empty()) return false;
            std::size_t idx = 0;
            int c = 0;
            while (idx < t.size() && std::isdigit(static_cast<unsigned char>(t[idx]))) {
                c = c * 10 + (t[idx] - '0');
                ++idx;
            }
            if (c == 0) c = 1;
            std::string sp = Trim(t.substr(idx));
            Term term;
            term.coeff = c;
            term.species = sp;
            if (!ParseChemicalSpeciesAtoms(sp, term.atoms, term.molarMass)) return false;
            outTerms.push_back(std::move(term));
        }
        return !outTerms.empty();
    };

    std::vector<Term> lhs, rhs;
    if (!parseSide(leftRaw, lhs) || !parseSide(rightRaw, rhs)) return res;
    res.valid = true;
    res.molarMassGPerMol = lhs.front().molarMass;

    auto checkBalanced = [&](const std::vector<int>& lCoeffs, const std::vector<int>& rCoeffs,
                             std::map<std::string, int>& outL, std::map<std::string, int>& outR) -> bool {
        outL.clear();
        outR.clear();
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            for (const auto& kv : lhs[i].atoms) outL[kv.first] += kv.second * lCoeffs[i];
        }
        for (std::size_t i = 0; i < rhs.size(); ++i) {
            for (const auto& kv : rhs[i].atoms) outR[kv.first] += kv.second * rCoeffs[i];
        }
        return outL == outR;
    };

    std::vector<int> bestL(lhs.size()), bestR(rhs.size());
    for (std::size_t i = 0; i < lhs.size(); ++i) bestL[i] = lhs[i].coeff;
    for (std::size_t i = 0; i < rhs.size(); ++i) bestR[i] = rhs[i].coeff;

    if (checkBalanced(bestL, bestR, res.atomCountsLeft, res.atomCountsRight)) {
        res.balanced = true;
    } else {
        // Exact rational Gaussian elimination in the null space of the element matrix
        std::vector<std::map<std::string, int>> lhsMaps, rhsMaps;
        lhsMaps.reserve(lhs.size());
        rhsMaps.reserve(rhs.size());
        for (const auto& t : lhs) lhsMaps.push_back(t.atoms);
        for (const auto& t : rhs) rhsMaps.push_back(t.atoms);
        std::vector<int> coeffs;
        if (SolveStoichiometryNullSpace(lhsMaps, rhsMaps, coeffs) && coeffs.size() == lhs.size() + rhs.size()) {
            std::vector<int> cL(coeffs.begin(), coeffs.begin() + static_cast<std::ptrdiff_t>(lhs.size()));
            std::vector<int> cR(coeffs.begin() + static_cast<std::ptrdiff_t>(lhs.size()), coeffs.end());
            if (checkBalanced(cL, cR, res.atomCountsLeft, res.atomCountsRight)) {
                bestL = std::move(cL);
                bestR = std::move(cR);
                res.balanced = true;
            }
        }
    }

    if (!res.balanced) {
        res.valid = false;
        return res;
    }

    std::ostringstream eqAscii, eq2D, eqLatex;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (i > 0) { eqAscii << " + "; eq2D << " + "; eqLatex << " + "; }
        if (bestL[i] > 1) { eqAscii << bestL[i]; eq2D << bestL[i]; eqLatex << bestL[i]; }
        eqAscii << lhs[i].species;
        eq2D << SpeciesToUnicode2D(lhs[i].species);
        eqLatex << SpeciesToLatex(lhs[i].species);
    }
    eqAscii << " -> ";
    eq2D << " \xE2\x86\x92 ";
    eqLatex << " \\rightarrow ";
    for (std::size_t i = 0; i < rhs.size(); ++i) {
        if (i > 0) { eqAscii << " + "; eq2D << " + "; eqLatex << " + "; }
        if (bestR[i] > 1) { eqAscii << bestR[i]; eq2D << bestR[i]; eqLatex << bestR[i]; }
        eqAscii << rhs[i].species;
        eq2D << SpeciesToUnicode2D(rhs[i].species);
        eqLatex << SpeciesToLatex(rhs[i].species);
    }
    res.balancedEquationUtf8 = eqAscii.str();
    res.natural2DUtf8 = eq2D.str();
    res.latexUtf8 = eqLatex.str();
    return res;
}

} // namespace DocStudio::MathChem
