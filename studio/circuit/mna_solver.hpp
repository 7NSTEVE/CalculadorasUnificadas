#pragma once

#include "circuit_models.hpp"
#include "circuit_schematic.hpp"
#include "circuit_symbolic.hpp"
#include "circuit_exporters.hpp"
#include "circuit_toolbox_recognizer.hpp"
#include "code_mcu_control.hpp"

#include <string>
#include <vector>

namespace DocStudio::Circuit {

// Quine-McCluskey Boolean / Karnaugh Map Simplifier (2..4 variables)
KMapSolution SimplifyKarnaughMap(
    int numVars,
    const std::vector<int>& minterms,
    const std::vector<int>& dontCares = {},
    const std::vector<std::string>& varNames = { "A", "B", "C", "D" });

// Engineering CAD / Schematic Exporters (backwards-compatible signatures)
std::string ExportCircuitToSvg(const std::vector<CircuitComponent>& comps, const CircuitSolveResult* sol = nullptr);
std::string ExportCircuitToDxf(const std::vector<CircuitComponent>& comps, const CircuitSolveResult* sol = nullptr);
std::string ExportCircuitToKiCadNetlist(const std::vector<CircuitComponent>& comps, const std::string& title = "DocStudioCircuit");

// Symbolic s-Domain / Laplace Nodal Formulation & Transfer Function H(s)
LaplaceSymbolicResult AnalyzeSymbolicLaplace(
    const std::vector<CircuitComponent>& components,
    int outputNode = 1);

LaplaceSymbolicResult AnalyzeSymbolicLaplaceNetlist(
    const std::string& netlistUtf8,
    int outputNode = 1);

} // namespace DocStudio::Circuit
