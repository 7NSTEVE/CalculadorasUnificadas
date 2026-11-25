#pragma once

#include <complex>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace DocStudio::Cas {

enum class CasAngleMode : std::uint8_t {
    Rad = 0,
    Deg = 1,
    Gra = 2
};

struct CasEvalOptions {
    CasAngleMode trigAngleMode = CasAngleMode::Rad;
    CasAngleMode polarAngleMode = CasAngleMode::Deg;
    bool forcePolarOutput = false;
    bool forceRectOutput = false;
};

// Unified Symbolic / MathJSON AST (used by Group 1 Symbolic CAS, Group 3/4 Compiled Eval,
// Group 4 FormulaFingerprint F-172, and Group 5 MathJSON / ConfirmationCard / CheckMyWork / Solve).
enum class MathJsonKind : std::uint8_t {
    Number = 0,
    Complex = 1,
    Symbol = 2,
    String = 3,
    Function = 4
};

struct MathJsonNode {
    MathJsonKind kind = MathJsonKind::Number;
    double realVal = 0.0;
    double imagVal = 0.0;
    std::string text; // Symbol name, String literal, or Function head
    std::vector<MathJsonNode> args;

    bool operator==(const MathJsonNode& o) const noexcept {
        if (kind != o.kind) return false;
        if (kind == MathJsonKind::Number) {
            return realVal == o.realVal;
        }
        if (kind == MathJsonKind::Complex) {
            return realVal == o.realVal && imagVal == o.imagVal;
        }
        return text == o.text && args == o.args;
    }
    bool operator!=(const MathJsonNode& o) const noexcept { return !(*this == o); }

    static MathJsonNode MakeNumber(double v) {
        MathJsonNode n;
        n.kind = MathJsonKind::Number;
        n.realVal = v;
        return n;
    }
    static MathJsonNode MakeComplex(double re, double im) {
        MathJsonNode n;
        n.kind = MathJsonKind::Complex;
        n.realVal = re;
        n.imagVal = im;
        return n;
    }
    static MathJsonNode MakeSymbol(std::string sym) {
        MathJsonNode n;
        n.kind = MathJsonKind::Symbol;
        n.text = std::move(sym);
        return n;
    }
    static MathJsonNode MakeString(std::string s) {
        MathJsonNode n;
        n.kind = MathJsonKind::String;
        n.text = std::move(s);
        return n;
    }
    static MathJsonNode MakeFunction(std::string head, std::vector<MathJsonNode> children = {}) {
        MathJsonNode n;
        n.kind = MathJsonKind::Function;
        n.text = std::move(head);
        n.args = std::move(children);
        return n;
    }
};

// JSON serialization & validation (implemented in engineering_cas.cpp)
bool ParseMathJson(const std::string& mathJsonUtf8, MathJsonNode& outAst, std::string* outErr = nullptr);
std::string ToMathJsonString(const MathJsonNode& ast, bool pretty = false);
bool ValidateMathJsonAst(const MathJsonNode& ast, std::vector<std::string>& outErrors);

// Bidirectional expression / LaTeX <-> MathJSON AST
bool ParseExpressionToMathJson(
    const std::string& exprOrLatexUtf8,
    MathJsonNode& outAst,
    std::string* outErr = nullptr);
bool ParseExpressionToMathJson(
    const std::string& exprOrLatexUtf8,
    MathJsonNode& outAst,
    const std::unordered_set<std::string>& boundVars,
    std::string* outErr = nullptr);
std::string MathJsonToExpression(const MathJsonNode& ast);
std::string MathJsonToLatex(const MathJsonNode& ast);
std::vector<std::string> ExtractFreeSymbols(
    const MathJsonNode& ast,
    const std::unordered_map<std::string, struct QuantityValue>& boundEnv = {});

// Symbolic canonicalization, simplification, expansion, differentiation (F-232, F-314)
MathJsonNode CanonicalizeMathJson(const MathJsonNode& ast, int maxExpandTerms = 128);
MathJsonNode SimplifySymbolicAst(const MathJsonNode& ast);
MathJsonNode ExpandSymbolicAst(const MathJsonNode& ast, int maxExpandTerms = 128);
bool DifferentiateAst(
    const MathJsonNode& ast,
    const std::string& varName,
    MathJsonNode& outDeriv,
    std::string* outErr = nullptr);

struct SymbolicOpResult {
    bool ok = false;
    MathJsonNode ast{};
    std::string expressionUtf8;
    std::string latexUtf8;
    std::string errorUtf8;
};

SymbolicOpResult SimplifySymbolicExpression(const std::string& exprUtf8);
SymbolicOpResult ExpandSymbolicExpression(const std::string& exprUtf8);
SymbolicOpResult DifferentiateExpression(const std::string& exprUtf8, const std::string& varName = "x");

// Step-by-step algebraic equation solver (F-232)
struct AlgebraicSolveStep {
    int stepNumber = 1;
    std::string titleUtf8;
    std::string expressionUtf8;
    std::string latexUtf8;
};

struct AlgebraicSolveResult {
    bool ok = false;
    bool solvedExact = false;
    std::string varNameUtf8 = "x";
    std::vector<std::complex<double>> solutions;
    std::vector<std::string> solutionStringsUtf8;
    std::vector<AlgebraicSolveStep> steps;
    double maxResidual = 0.0;
    std::string summaryUtf8;
    std::string errorUtf8;
};

} // namespace DocStudio::Cas
