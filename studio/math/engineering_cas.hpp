#pragma once

#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "cas_mathjson_ast.hpp"

#include <array>
#include <complex>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DocStudio::Cas {

// 7 SI base dimensions: [0]=M(kg), [1]=L(m), [2]=T(s), [3]=I(A), [4]=Theta(K), [5]=N(mol), [6]=J(cd)
struct UnitDim {
    std::array<int, 7> exp{ 0, 0, 0, 0, 0, 0, 0 };

    bool operator==(const UnitDim& o) const noexcept { return exp == o.exp; }
    bool operator!=(const UnitDim& o) const noexcept { return exp != o.exp; }
    bool IsDimensionless() const noexcept;

    UnitDim operator*(const UnitDim& o) const noexcept;
    UnitDim operator/(const UnitDim& o) const noexcept;
    UnitDim Pow(int p) const noexcept;
    std::string FormatSiString() const;
};

struct QuantityValue {
    std::complex<double> value{ 0.0, 0.0 };
    UnitDim dim{};
    std::string preferredUnitUtf8;
    double unitScaleToSi = 1.0;
    double unitOffsetToSi = 0.0; // e.g., degC -> K
    bool preferPolar = false;

    bool IsReal(double tol = 1e-11) const noexcept { return std::abs(value.imag()) <= tol; }
    double Mag() const noexcept { return std::abs(value); }
    double PhaseDeg() const noexcept;
    double PhaseInMode(CasAngleMode mode) const noexcept;
    std::string FormatUtf8(int sigDigits = 6) const;
    std::string FormatUtf8(int sigDigits, CasAngleMode angleMode) const;
};

struct CasEvalResult {
    bool ok = false;
    bool hasAssignment = false;
    std::string assignedVarUtf8;
    QuantityValue result{};
    std::string formattedResultUtf8;
    std::string varSubExpansionUtf8; // "#varsub" substituted expression with resolved values+units
    std::string errorUtf8;
    std::string diagnosticCode = "OK"; // "OK", "DIM_ERR", "SYNTAX_ERR", "MATH_ERR"
};

using SymbolEnv = std::unordered_map<std::string, QuantityValue>;

bool TryParseUnitExpression(
    const std::string& unitExprUtf8,
    UnitDim& outDim,
    double& outScaleToSi,
    double& outOffsetToSi,
    std::string* outErr = nullptr);

// Evaluates scalar/complex/phasor expression with strict SI & imperial unit tracking + conversion (`expr | targetUnit`)
CasEvalResult EvaluateExpression(const std::string& exprUtf8, const SymbolEnv& env = {});
CasEvalResult EvaluateExpression(const std::string& exprUtf8, const SymbolEnv& env, const CasEvalOptions& opts);
bool EvaluateAst(
    const MathJsonNode& ast,
    const SymbolEnv& env,
    CasEvalResult& out,
    const CasEvalOptions& opts = {});

// Step-by-step algebraic equation solver (F-232)
AlgebraicSolveResult SolveAlgebraicEquation(
    const std::string& eqUtf8,
    const std::string& varName = "x",
    const SymbolEnv& env = {});

// Evaluates a sequence of MathBlock / CalcSessionCard / table / margin objects across document in topological dependency order
struct DagEvaluationReport {
    bool ok = true;
    int evaluatedCount = 0;
    int cycleCount = 0;
    int errorCount = 0;
    std::vector<std::string> errors;
};

DagEvaluationReport EvaluateDocumentMathDag(DocumentModel& doc);

// Deterministic multi-point algebraic & dimensional equivalence checker between two expressions
struct StepCheckResult {
    bool equivalent = false;
    bool dimensionMismatch = false;
    double maxResidual = 0.0;
    std::string statusCode; // "OK", "DIFF", "DIM_ERR", "SYNTAX_ERR"
    std::string messageUtf8;
};

StepCheckResult CheckAlgebraicStep(
    const std::string& prevExprUtf8,
    const std::string& nextExprUtf8,
    const std::string& varName = "x",
    const SymbolEnv& baseEnv = {});

// Function 2D / Parametric / 3D Surface / CSV OLS Regression Sampler
struct PlotPoint3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool valid = true;
};

struct RegressionResult {
    bool ok = false;
    int degree = 1;
    std::vector<double> coeffs; // c0 + c1*x + c2*x^2...
    double rSquared = 0.0;
    std::string equationUtf8;
    std::vector<DocPoint> dataPoints;
    std::vector<DocPoint> fittedCurve;
};

std::vector<DocPoint> SampleFunction2D(
    const std::string& exprUtf8,
    const std::string& varName,
    double xMin,
    double xMax,
    std::size_t numSamples,
    const SymbolEnv& env = {});

std::vector<PlotPoint3D> SampleSurface3D(
    const std::string& exprUtf8,
    double xMin, double xMax, std::size_t nx,
    double yMin, double yMax, std::size_t ny,
    const SymbolEnv& env = {});

RegressionResult FitCsvRegression(
    const std::string& csvTextUtf8,
    int polyDegree = 1);

// -------- Matrix Algebra & Linear System (SEL) Deterministic Solver --------

struct LinearSystemResult {
    bool ok = false;
    std::vector<std::string> varNames;
    std::vector<double> values;
    double determinant = 0.0;
    std::string summaryUtf8;      // e.g. "x = 3, y = 1 (det = -3)"
    std::string stepsTraceUtf8;   // Augmented matrix & pivot elimination steps
    std::string errorUtf8;
};

// Solves systems such as "2*x + y = 7; x - y = 2" or "3*I1 - I2 = 12; -I1 + 4*I2 = 5"
LinearSystemResult SolveLinearSystemEquations(const std::string& equationsUtf8);

struct MatrixEvalResult {
    bool ok = false;
    std::size_t rows = 0;
    std::size_t cols = 0;
    std::vector<std::vector<double>> matrix;
    bool isScalar = false;
    double scalarValue = 0.0;
    std::string formattedUtf8;
    std::string latexUtf8;
    std::string errorUtf8;
};

// Evaluates [[a,b],[c,d]] +/-/* [[e,f],[g,h]], det([[...]]), inv([[...]]), tr([[...]]), transpose([[...]])
MatrixEvalResult EvaluateMatrixExpression(const std::string& exprUtf8);

// -------- Formula Variable Inspector ("What Each Variable Means" - Physical Quantity, SI Dim, Value & Node) --------

struct VariableInspectorEntry {
    std::string varNameUtf8;
    std::string physicalQuantityUtf8; // e.g. "Tension Electrica", "Presion / Esfuerzo", "Impedancia / Resistencia"
    std::string siBaseUnitsUtf8;      // e.g. "kg*m^2*s^-3*A^-1"
    std::string currentValueUtf8;     // e.g. "9.4 V"
    std::string sourceNodeLabelUtf8;  // e.g. "Lienzo #3" or "Pagina 1 #2"
    std::string formulaExprUtf8;
};

std::string DescribePhysicalDimensionEs(const UnitDim& dim);
std::vector<VariableInspectorEntry> InspectDocumentVariables(const DocumentModel& doc);
std::string FormatVariableTooltipUtf8(const VariableInspectorEntry& entry);
std::unordered_map<std::string, std::string> BuildDocumentVariableTooltipMap(const DocumentModel& doc);

// -------- Group 2: Reactive Table-Cell & Margin Side-Calc Models (SUP-01) --------

struct TableCellInput {
    std::string cellRef;       // e.g. "A1", "B2"
    std::string rawExprUtf8;   // e.g. "10 m", "=A1*B1 | kW", "=SUM(A1:A4)"
    std::string exportVarName; // optional symbol exported to document SymbolEnv
};

struct TableCellOutput {
    std::string cellRef;
    bool ok = false;
    QuantityValue value{};
    std::string formattedUtf8;
    std::string errorUtf8;
};

struct TableSheetModel {
    std::string tableNameUtf8 = "Tabla";
    std::vector<TableCellInput> cells;
};

struct TableEvalReport {
    bool ok = true;
    int evaluatedCount = 0;
    int cycleCount = 0;
    int errorCount = 0;
    std::vector<TableCellOutput> results;
    std::vector<std::string> errors;
};

TableEvalReport EvaluateTableDag(const TableSheetModel& sheet, SymbolEnv& ioEnv);
CasEvalResult EvaluateMarginSideCalc(const std::string& marginTextUtf8, SymbolEnv& ioEnv);

} // namespace DocStudio::Cas

// Modular M1_2 C++17 extensions (header-only + engineering_cas.cpp implementations; zero Win32/Cairo deps)
#include "latex_mathml_ast.hpp"
#include "plot_math_engine.hpp"
#include "advanced_math_engine.hpp"
#include "engineering_statics_uncertainty.hpp"
#include "formula_sheet_index.hpp"
#include "cas_ai_pedagogy.hpp"

namespace DocStudio::Cas {

inline std::string FormatVarSubLatex2D(
    const std::string& varNameUtf8,
    const std::string& symRhsUtf8,
    const std::string& varSubUtf8,
    const std::string& resultUtf8,
    MathSubMode mode)
{
    return FormatVarSubLatex2D(varNameUtf8, symRhsUtf8, varSubUtf8, resultUtf8, static_cast<int>(mode));
}

} // namespace DocStudio::Cas
