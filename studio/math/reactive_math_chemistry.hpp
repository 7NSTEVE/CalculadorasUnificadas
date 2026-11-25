#pragma once

#include "../core/command_stack.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "engineering_cas.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace DocStudio::MathChem {

// ---------------- Per-Note Math Evaluation Settings (MS-MATH-055 / MS-MATH-056) ----------------

enum class AngleUnitMode : std::uint8_t {
    Radians = 0,
    Degrees = 1
};

struct MathEvalConfig {
    AngleUnitMode angleMode = AngleUnitMode::Radians;
    int decimalPlaces = 4; // 0..12
    bool exactFractionWhenPossible = false;
};

// ---------------- 2D Math Structure Recognition & LaTeX Conversion ----------------

struct RecognizedMathStructure {
    bool valid = false;
    bool hasFraction = false;
    bool hasSummation = false;
    bool hasIntegral = false;
    bool hasPower = false;
    bool hasGreekSymbol = false;
    bool hasMatrix = false;
    int matrixRows = 0;
    int matrixCols = 0;
    bool matrixDimensionsValid = true;
    std::string normalizedExprUtf8;
    std::string latexUtf8;
    std::string natural2DUtf8;
    std::string evaluatedUtf8;
    std::vector<std::string> solutionStepsUtf8;
};

// ---------------- Chemical Formula & Stoichiometric Reaction Balancer (MS-MATH-058) ----------------

struct ChemicalAnalysisResult {
    bool valid = false;
    bool isReaction = false;
    bool balanced = false;
    std::string inputUtf8;
    std::string balancedEquationUtf8;
    std::string natural2DUtf8; // Unicode subscripts & reaction arrow e.g. Fe₂O₃ + 3CO → 2Fe + 3CO₂
    std::string latexUtf8;
    double molarMassGPerMol = 0.0; // for single compound or first reactant
    std::map<std::string, int> atomCountsLeft;
    std::map<std::string, int> atomCountsRight;
};

// ---------------- notas matemáticas del sistema Notes Reactive '=' Page Session (AP-MATH-051..058) ----------------

struct ReactiveMathLineResult {
    std::uint64_t objectId = 0;
    std::string rawInputUtf8;
    std::string variableNameUtf8;
    std::string evaluatedValueUtf8;
    std::string fullResolvedDisplayUtf8; // e.g. "d = v0 * t + 0.5 * a * t^2 = 126.48"
    bool triggeredByTrailingEquals = false;
};

class ReactiveMathChemistryEngine {
public:
    // Recognizes and evaluates fractions, summations, integrals, powers, Greek symbols, matrices & LaTeX
    static RecognizedMathStructure AnalyzeAndSolveMathStructure(
        const std::string& rawInputUtf8,
        const MathEvalConfig& cfg = {},
        const Cas::SymbolEnv& externalEnv = {});

    // Generates step-by-step pedagogical explanation for equations, derivatives, integrals, and linear systems
    static std::vector<std::string> BuildStepByStepExplanation(
        const std::string& rawInputUtf8,
        const MathEvalConfig& cfg = {});

    // notas matemáticas del sistema Notes reactive '=' evaluation across all objects on a page/canvas:
    // Resolves any expression ending with '=' using declared variables on the page and updates dependent plots
    static std::vector<ReactiveMathLineResult> EvaluateReactiveEqualsOnDocument(
        DocumentModel& doc,
        ObjectTarget target,
        const MathEvalConfig& cfg = {});

    // Modifies a declared variable on the page and immediately recomputes all dependent '=' expressions and plots
    static bool UpdateDeclaredVariableAndRecompute(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        const std::string& varNameUtf8,
        double newValue,
        const MathEvalConfig& cfg = {});

    // Chemical formula parser, molar mass calculator, 2D formatter and stoichiometric reaction balancer (MS-MATH-058)
    static ChemicalAnalysisResult AnalyzeAndBalanceChemistry(const std::string& formulaOrReactionUtf8);
};

} // namespace DocStudio::MathChem
