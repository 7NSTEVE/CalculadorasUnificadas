#pragma once

#include "../bridge/calc_bridge.hpp"
#include "../bridge/lab_report_exporter.hpp"
#include "../circuit/mna_solver.hpp"
#include "../core/command_stack.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/ink_engine.hpp"
#include "../core/notebook_vault.hpp"
#include "../math/engineering_cas.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::AppActions {

inline std::vector<int> ParseDelimitedInts(const std::string& s) {
    std::string norm = s;
    for (char& ch : norm) {
        if (ch == ',' || ch == ';' || ch == '|') ch = ' ';
    }
    std::istringstream iss(norm);
    std::vector<int> out;
    int v = 0;
    while (iss >> v) out.push_back(v);
    return out;
}

inline std::uint64_t LastInsertedId(const DocumentModel& doc, ObjectTarget target) noexcept {
    const auto& vec = target.isCanvas
        ? doc.CanvasObjects()
        : (target.pageIndex < doc.Pages().size() ? doc.Pages()[target.pageIndex].objects : doc.CanvasObjects());
    return vec.empty() ? 0ull : vec.back().id;
}

inline std::uint64_t AddFormulaNodeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& exprUtf8,
    const std::string& prevStepUtf8,
    std::string& outStatus)
{
    if (exprUtf8.empty()) return 0;
    AnnotationObject mb;
    mb.kind = ObjectKind::MathBlock;
    mb.layerId = doc.ActiveLayerId();
    mb.bounds = { x, y, 330.0, prevStepUtf8.empty() ? 86.0 : 108.0 };
    mb.formulaExprUtf8 = exprUtf8;
    const auto eqPos = exprUtf8.find('=');
    if (eqPos != std::string::npos && eqPos > 0) {
        mb.formulaVarNameUtf8 = exprUtf8.substr(0, eqPos);
        while (!mb.formulaVarNameUtf8.empty() && std::isspace(static_cast<unsigned char>(mb.formulaVarNameUtf8.back()))) {
            mb.formulaVarNameUtf8.pop_back();
        }
    }
    mb.stepCheckInputUtf8 = prevStepUtf8;

    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(mb)))) {
        return 0;
    }
    const std::uint64_t id = LastInsertedId(doc, target);
    const auto rep = Cas::EvaluateDocumentMathDag(doc);
    outStatus = rep.ok
        ? ("#varsub DAG evaluado (" + std::to_string(rep.evaluatedCount) + " nodos)")
        : (rep.errors.empty() ? "Error evaluando f\xC3\xB3rmula" : rep.errors.front());
    return id;
}

inline std::uint64_t AddCircuitNodeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& netlistUtf8,
    double freqHz,
    std::string& outStatus)
{
    if (netlistUtf8.empty()) return 0;
    const auto sol = Circuit::SolveMnaNetlist(netlistUtf8, freqHz);
    AnnotationObject cb;
    cb.kind = ObjectKind::CircuitBlock;
    cb.layerId = doc.ActiveLayerId();
    cb.bounds = { x, y, 360.0, 128.0 };
    cb.circuitNetlistUtf8 = netlistUtf8;
    cb.captionUtf8 = (freqHz > 0.0)
        ? ("MNA AC @ " + std::to_string(static_cast<int>(freqHz)) + " Hz")
        : "MNA Punto Operaci\xC3\xB3n DC";
    if (sol.ok) {
        std::string txt = sol.summaryUtf8;
        for (const auto& eq : sol.kclKvlEquations) {
            txt += "\n" + eq;
        }
        cb.evaluatedResultUtf8 = txt;
        cb.textUtf8 = txt;
        outStatus = "Circuito MNA resuelto (" + std::to_string(sol.branches.size()) + " ramas)";
    } else {
        cb.evaluatedResultUtf8 = "ERR: " + sol.errorUtf8;
        outStatus = sol.errorUtf8;
    }
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(cb)))) return 0;
    return LastInsertedId(doc, target);
}

inline std::uint64_t AddKMapNodeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    int numVars,
    const std::string& mintermsCsv,
    std::string& outStatus)
{
    const auto mins = ParseDelimitedInts(mintermsCsv);
    const auto kmap = Circuit::SimplifyKarnaughMap(numVars, mins);
    AnnotationObject kb;
    kb.kind = ObjectKind::MathBlock;
    kb.layerId = doc.ActiveLayerId();
    kb.bounds = { x, y, 360.0, 128.0 };
    kb.formulaExprUtf8 = "Mapa-K (" + std::to_string(kmap.numVars) + " vars) Sigma(" + mintermsCsv + ")";
    kb.evaluatedResultUtf8 = kmap.ok ? kmap.minimizedSopUtf8 : kmap.errorUtf8;
    if (kmap.ok) {
        std::ostringstream tt;
        tt << kmap.minimizedSopUtf8 << "  [Primos: " << kmap.primeImplicants.size() << "]\nTabla Verdad (1s): ";
        bool firstRow = true;
        for (std::size_t idx = 0; idx < kmap.gridValues.size(); ++idx) {
            if (kmap.gridValues[idx] != 1) continue;
            if (!firstRow) tt << ", ";
            firstRow = false;
            for (int b = kmap.numVars - 1; b >= 0; --b) {
                tt << (((static_cast<int>(idx) >> b) & 1) ? '1' : '0');
            }
        }
        if (firstRow) tt << "(ninguno)";
        kb.textUtf8 = tt.str();
    } else {
        kb.textUtf8 = kmap.errorUtf8;
    }
    outStatus = kmap.ok ? ("Quine-McCluskey: " + kmap.minimizedSopUtf8) : kmap.errorUtf8;
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(kb)))) return 0;
    return LastInsertedId(doc, target);
}

inline std::uint64_t AddPlotNodeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& exprOrCsv,
    double xMin,
    double xMax,
    std::string& outStatus)
{
    if (exprOrCsv.empty()) return 0;
    AnnotationObject pb;
    pb.kind = ObjectKind::PlotBlock;
    pb.layerId = doc.ActiveLayerId();
    pb.bounds = { x, y, 360.0, 210.0 };
    const bool looksCsv = (exprOrCsv.find('\n') != std::string::npos || exprOrCsv.find(',') != std::string::npos);
    nlohmann::json cfg;
    if (looksCsv) {
        cfg["csv"] = exprOrCsv;
        cfg["degree"] = 1;
        const auto reg = Cas::FitCsvRegression(exprOrCsv, 1);
        pb.captionUtf8 = "Regresi\xC3\xB3n CSV (OLS)";
        pb.evaluatedResultUtf8 = reg.ok ? reg.equationUtf8 : "CSV insuficiente";
    } else {
        cfg["expr"] = exprOrCsv;
        cfg["xMin"] = xMin;
        cfg["xMax"] = (xMax > xMin) ? xMax : (xMin + 10.0);
        pb.formulaExprUtf8 = exprOrCsv;
        pb.captionUtf8 = "y = " + exprOrCsv;
    }
    pb.plotConfigUtf8 = cfg.dump();
    outStatus = "Gr\xC3\xA1" "fico 2D insertado";
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(pb)))) return 0;
    return LastInsertedId(doc, target);
}

inline std::uint64_t AddCalcCardAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const CalcBridge::CalcStepRecord& rec,
    std::string& outStatus)
{
    AnnotationObject card = CalcBridge::BuildCalcSessionCard(rec, { x, y, 330.0, 108.0 });
    card.layerId = doc.ActiveLayerId();
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(card)))) return 0;
    const std::uint64_t id = LastInsertedId(doc, target);
    Cas::EvaluateDocumentMathDag(doc);
    outStatus = "Tarjeta CASIO agregada al estudio";
    return id;
}

inline std::uint64_t AddStudyTapeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& captionUtf8,
    const std::string& hiddenAnswerUtf8,
    std::string& outStatus)
{
    AnnotationObject tape;
    tape.kind = ObjectKind::StudyTape;
    tape.layerId = doc.ActiveLayerId();
    tape.bounds = { x, y, 260.0, 56.0 };
    tape.captionUtf8 = captionUtf8.empty() ? "Concepto oculto" : captionUtf8;
    tape.textUtf8 = hiddenAnswerUtf8;
    tape.tapeRevealed = false;
    outStatus = "Study Tape agregada (clic con Mano para revelar/ocultar)";
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(tape)))) return 0;
    return LastInsertedId(doc, target);
}

inline bool ToggleStudyTapeIfHit(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    std::uint64_t objId,
    std::string& outStatus)
{
    AnnotationObject* obj = target.isCanvas
        ? doc.FindCanvasObjectMutable(objId)
        : doc.FindPageObjectMutable(target.pageIndex, objId);
    if (!obj || obj->kind != ObjectKind::StudyTape) return false;
    AnnotationObject before = *obj;
    AnnotationObject after = before;
    after.tapeRevealed = !before.tapeRevealed;
    if (cmdStack.Execute(doc, std::make_unique<ModifyObjectCommand>(target, std::move(before), std::move(after)))) {
        outStatus = after.tapeRevealed ? "Study Tape revelada" : "Study Tape oculta";
        return true;
    }
    return false;
}

// Unified stroke/gesture completion across Windows Win32 and Linux GTK3
inline void HandleStrokeCompletedAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    AnnotationObject& draftObj,
    bool isLassoTool,
    bool holdSnapEnabled,
    double stabilization,
    std::vector<std::uint64_t>& outLassoSelection,
    std::uint64_t& outPrimarySelectedId,
    std::string& outStatus)
{
    draftObj.layerId = doc.ActiveLayerId();

    if (isLassoTool) {
        if (InkEngine::IsClosedLassoLoop(draftObj.inkPoints) || draftObj.inkPoints.size() >= 6) {
            outLassoSelection = InkEngine::SelectObjectsInsideLasso(doc, target, draftObj.inkPoints, true);
            outPrimarySelectedId = outLassoSelection.empty() ? 0ull : outLassoSelection.back();
            outStatus = "Lazo seleccion\xC3\xB3 " + std::to_string(outLassoSelection.size()) + " objeto(s)";
        }
        return;
    }

    if (draftObj.kind == ObjectKind::InkStroke) {
        if (InkEngine::IsScribbleEraseGesture(draftObj.inkPoints)) {
            const auto eraseIds = InkEngine::FindObjectsForScribbleErase(doc, target, draftObj.inkPoints, true);
            if (!eraseIds.empty()) {
                cmdStack.Execute(doc, std::make_unique<BatchEraseObjectsCommand>(target, eraseIds));
                outLassoSelection.clear();
                outPrimarySelectedId = 0;
                outStatus = "Scribble-to-Erase borr\xC3\xB3 " + std::to_string(eraseIds.size()) + " objeto(s)";
                return;
            }
        }

        if (holdSnapEnabled) {
            const auto snap = InkEngine::RecognizeHeldShape(draftObj.inkPoints);
            if (snap.recognized) {
                draftObj.kind = ObjectKind::Shape;
                draftObj.shapeKind = snap.kind;
                draftObj.shapeStart = snap.start;
                draftObj.shapeEnd = snap.end;
                draftObj.bounds = snap.bounds;
                draftObj.inkSnapped = true;
                draftObj.inkPoints.clear();
                outStatus = "Forma ajustada: " + snap.label;
            }
        }

        if (draftObj.kind == ObjectKind::InkStroke) {
            draftObj.stabilization = stabilization;
            draftObj.inkPoints = InkEngine::SmoothInkStroke(draftObj.inkPoints, stabilization);
        }
    }

    draftObj.RecomputeBounds();
    if (!draftObj.bounds.Empty()) {
        if (cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(draftObj)))) {
            outLassoSelection.clear();
            outPrimarySelectedId = LastInsertedId(doc, target);
        }
    }
}

inline std::uint64_t AddSurface3DPlotNodeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& exprUtf8,
    double xMin,
    double xMax,
    double yMin,
    double yMax,
    std::string& outStatus)
{
    if (exprUtf8.empty()) return 0;
    AnnotationObject pb;
    pb.kind = ObjectKind::PlotBlock;
    pb.layerId = doc.ActiveLayerId();
    pb.bounds = { x, y, 390.0, 250.0 };
    pb.formulaExprUtf8 = exprUtf8;
    pb.captionUtf8 = "Superficie 3D CVD: z = " + exprUtf8;
    nlohmann::json cfg;
    cfg["mode"] = "3d";
    cfg["is3D"] = true;
    cfg["expr"] = exprUtf8;
    cfg["var"] = "x";
    cfg["yVar"] = "y";
    cfg["xMin"] = xMin;
    cfg["xMax"] = (xMax > xMin) ? xMax : (xMin + 6.0);
    cfg["yMin"] = yMin;
    cfg["yMax"] = (yMax > yMin) ? yMax : (yMin + 6.0);
    cfg["gridSteps"] = 18;
    cfg["yawDeg"] = 42.0;
    cfg["pitchDeg"] = 28.0;
    pb.plotConfigUtf8 = cfg.dump();

    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(pb)))) return 0;
    const std::uint64_t newId = LastInsertedId(doc, target);
    Cas::EvaluateDocumentMathDag(doc);
    outStatus = "Superficie 3D z = " + exprUtf8 + " (paleta CVD accesible) insertada";
    return newId;
}

inline std::uint64_t AddMatrixOrLinearSystemAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& inputUtf8,
    std::string& outStatus)
{
    if (inputUtf8.empty()) return 0;
    AnnotationObject mb;
    mb.kind = ObjectKind::MathBlock;
    mb.layerId = doc.ActiveLayerId();
    mb.bounds = { x, y, 360.0, 110.0 };

    const bool isMatrix = (inputUtf8.find('[') != std::string::npos);
    if (isMatrix) {
        const auto mres = Cas::EvaluateMatrixExpression(inputUtf8);
        mb.formulaExprUtf8 = "Matriz: " + inputUtf8;
        mb.evaluatedResultUtf8 = mres.ok ? mres.formattedUtf8 : ("ERR: " + mres.errorUtf8);
        mb.textUtf8 = mb.evaluatedResultUtf8;
        outStatus = mres.ok ? ("Matriz evaluada: " + mres.formattedUtf8) : mres.errorUtf8;
    } else {
        const auto sres = Cas::SolveLinearSystemEquations(inputUtf8);
        mb.formulaExprUtf8 = "SEL: " + inputUtf8;
        mb.evaluatedResultUtf8 = sres.ok ? sres.summaryUtf8 : ("ERR: " + sres.errorUtf8);
        mb.textUtf8 = mb.evaluatedResultUtf8;
        outStatus = sres.ok ? ("Sistema lineal resuelto: " + sres.summaryUtf8) : sres.errorUtf8;
    }

    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(mb)))) return 0;
    return LastInsertedId(doc, target);
}

inline std::uint64_t AddTransientLaplaceCircuitAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& netlistUtf8,
    double durationMs,
    double sourceFreqHz,
    int probedNode,
    std::string& outStatus)
{
    if (netlistUtf8.empty()) return 0;
    const double durSec = (std::max)(1e-5, durationMs * 1e-3);
    const auto tr = Circuit::SimulateTransientNetlist(netlistUtf8, durSec, 160, sourceFreqHz, probedNode);
    const auto lap = Circuit::AnalyzeSymbolicLaplaceNetlist(netlistUtf8, probedNode);

    AnnotationObject cb;
    cb.kind = ObjectKind::CircuitBlock;
    cb.layerId = doc.ActiveLayerId();
    cb.bounds = { x, y, 390.0, 146.0 };
    cb.circuitNetlistUtf8 = netlistUtf8;
    cb.captionUtf8 = "MNA Transitorio + Laplace (s)";
    std::string summary = tr.ok ? tr.summaryUtf8 : ("ERR: " + tr.errorUtf8);
    if (lap.ok) {
        summary += "\n" + lap.transferFunctionUtf8;
        if (!lap.nodalEquationsLatex.empty()) {
            summary += "\nKCL(s): " + lap.nodalEquationsLatex.front();
        }
    }
    cb.evaluatedResultUtf8 = summary;
    cb.textUtf8 = summary;
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(cb)))) return 0;
    const std::uint64_t cId = LastInsertedId(doc, target);

    if (tr.ok && tr.nodeWaveforms.count(tr.probedNode)) {
        AnnotationObject osc;
        osc.kind = ObjectKind::PlotBlock;
        osc.layerId = doc.ActiveLayerId();
        osc.bounds = { x + 408.0, y, 390.0, 220.0 };
        osc.captionUtf8 = "Osciloscopio V(Nodo " + std::to_string(tr.probedNode) + ") [ms vs V]";
        osc.evaluatedResultUtf8 = tr.summaryUtf8;

        nlohmann::json cfg;
        nlohmann::json wx = nlohmann::json::array();
        nlohmann::json wy = nlohmann::json::array();
        const auto& wf = tr.nodeWaveforms.at(tr.probedNode);
        for (std::size_t i = 0; i < tr.timeSec.size() && i < wf.size(); ++i) {
            wx.push_back(tr.timeSec[i] * 1000.0);
            wy.push_back(wf[i]);
        }
        cfg["waveX"] = std::move(wx);
        cfg["waveY"] = std::move(wy);
        osc.plotConfigUtf8 = cfg.dump();
        cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(osc)));
    }

    outStatus = tr.ok ? tr.summaryUtf8 : tr.errorUtf8;
    return cId;
}

inline bool CycleSelectedMathSubModeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    std::uint64_t selectedObjId,
    std::string& outStatus)
{
    AnnotationObject* obj = (selectedObjId != 0)
        ? (target.isCanvas ? doc.FindCanvasObjectMutable(selectedObjId) : doc.FindPageObjectMutable(target.pageIndex, selectedObjId))
        : nullptr;
    if (!obj || obj->kind != ObjectKind::MathBlock) {
        // Fallback: pick most recent MathBlock in target
        auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
        for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
            if (it->kind == ObjectKind::MathBlock) {
                selectedObjId = it->id;
                obj = target.isCanvas ? doc.FindCanvasObjectMutable(selectedObjId) : doc.FindPageObjectMutable(target.pageIndex, selectedObjId);
                break;
            }
        }
    }
    if (!obj || obj->kind != ObjectKind::MathBlock) {
        outStatus = "Seleccione un bloque matem\xC3\xA1" "tico primero";
        return false;
    }

    AnnotationObject before = *obj;
    AnnotationObject after = before;
    if (before.mathSubMode == MathSubMode::VarSub) after.mathSubMode = MathSubMode::NoSub;
    else if (before.mathSubMode == MathSubMode::NoSub) after.mathSubMode = MathSubMode::NoVar;
    else after.mathSubMode = MathSubMode::VarSub;

    if (!cmdStack.Execute(doc, std::make_unique<ModifyObjectCommand>(target, std::move(before), std::move(after)))) {
        return false;
    }
    Cas::EvaluateDocumentMathDag(doc);
    const char* lbl = (after.mathSubMode == MathSubMode::VarSub) ? "#varsub (F\xC3\xB3rmula + Sustituci\xC3\xB3n + Resultado)"
                    : (after.mathSubMode == MathSubMode::NoSub)  ? "#nosub (F\xC3\xB3rmula + Resultado)"
                                                                 : "#novar (Solo Resultado)";
    outStatus = std::string("Modo Calcpad activo: ") + lbl;
    return true;
}

inline bool UpdateParameterSliderAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::string& varNameUtf8,
    double newVal,
    double minVal,
    double maxVal,
    const std::string& unitSuffixUtf8,
    std::string& outStatus)
{
    if (varNameUtf8.empty()) return false;
    std::ostringstream exprOss;
    exprOss.precision(6);
    exprOss << varNameUtf8 << " = " << newVal;
    if (!unitSuffixUtf8.empty()) exprOss << " " << unitSuffixUtf8;
    const std::string newExpr = exprOss.str();

    // Check if variable node already exists in target
    const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
    std::uint64_t foundId = 0;
    for (const auto& o : vec) {
        if (o.kind == ObjectKind::MathBlock && o.formulaVarNameUtf8 == varNameUtf8) {
            foundId = o.id;
            break;
        }
    }

    if (foundId != 0) {
        AnnotationObject* cur = target.isCanvas
            ? doc.FindCanvasObjectMutable(foundId)
            : doc.FindPageObjectMutable(target.pageIndex, foundId);
        if (!cur) return false;
        AnnotationObject before = *cur;
        AnnotationObject after = before;
        after.formulaExprUtf8 = newExpr;
        after.hasSlider = true;
        after.sliderMin = minVal;
        after.sliderMax = maxVal;
        after.sliderValue = newVal;
        cmdStack.Execute(doc, std::make_unique<ModifyObjectCommand>(target, std::move(before), std::move(after)));
    } else {
        AnnotationObject mb;
        mb.kind = ObjectKind::MathBlock;
        mb.layerId = doc.ActiveLayerId();
        mb.bounds = { 80.0, 80.0, 320.0, 84.0 };
        mb.formulaVarNameUtf8 = varNameUtf8;
        mb.formulaExprUtf8 = newExpr;
        mb.hasSlider = true;
        mb.sliderMin = minVal;
        mb.sliderMax = maxVal;
        mb.sliderValue = newVal;
        cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(mb)));
    }

    const auto rep = Cas::EvaluateDocumentMathDag(doc);
    outStatus = "Slider " + newExpr + " actualizado (" + std::to_string(rep.evaluatedCount) + " nodos reactivos)";
    return true;
}

inline std::size_t ClearItemsByFilterAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    EraserFilterMode filterMode,
    std::string& outStatus)
{
    const auto ids = InkEngine::CollectObjectsMatchingFilter(doc, target, filterMode, true);
    if (ids.empty()) {
        outStatus = "No hay elementos coincidentes para borrar en la vista activa";
        return 0;
    }
    cmdStack.Execute(doc, std::make_unique<BatchEraseObjectsCommand>(target, ids));
    outStatus = "Borrado selectivo elimin\xC3\xB3 " + std::to_string(ids.size()) + " elemento(s)";
    return ids.size();
}

inline bool AssignTagToObjectAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    std::uint64_t objId,
    const std::string& rawTagUtf8,
    std::string& outStatus)
{
    if (objId == 0 || rawTagUtf8.empty()) {
        outStatus = "Seleccione un objeto antes de etiquetar";
        return false;
    }
    std::string cleanTag = rawTagUtf8;
    while (!cleanTag.empty() && (cleanTag.front() == '#' || cleanTag.front() == ' ')) cleanTag.erase(cleanTag.begin());
    if (cleanTag.empty()) return false;

    AnnotationObject* cur = target.isCanvas
        ? doc.FindCanvasObjectMutable(objId)
        : doc.FindPageObjectMutable(target.pageIndex, objId);
    if (!cur) return false;

    AnnotationObject before = *cur;
    AnnotationObject after = before;
    if (std::find(after.tags.begin(), after.tags.end(), cleanTag) == after.tags.end()) {
        after.tags.push_back(cleanTag);
    }
    if (cmdStack.Execute(doc, std::make_unique<ModifyObjectCommand>(target, std::move(before), std::move(after)))) {
        outStatus = "Etiqueta #" + cleanTag + " asignada";
        return true;
    }
    return false;
}

inline std::string FormatTagSummaryBoardUtf8(const DocumentModel& doc) {
    const auto items = CollectTaggedItems(doc, "");
    if (items.empty()) {
        return "No hay objetos etiquetados en el documento actual.\nUse 'Etiquetar Objeto (#examen / #importante / #duda)' sobre cualquier tarjeta o nota.";
    }
    std::ostringstream oss;
    oss << "Tablero de Etiquetas (" << items.size() << " entradas):\n\n";
    for (const auto& it : items) {
        oss << "  [#" << it.tagUtf8 << "] "
            << (it.isCanvas ? "Lienzo Infinito" : ("P\xC3\xA1" "gina " + std::to_string(it.pageIndex + 1)))
            << " -> " << it.summaryUtf8 << "\n";
    }
    return oss.str();
}

inline void PopulateStemShowcaseScene(DocumentModel& doc, CommandStack& cmdStack) {
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    doc.SetTitle("Laboratorio Ingenieria STEM - Lienzo Reactivo");
    doc.SetCanvasBackground(PaperPattern::Grid);
    cmdStack.Clear();

    const ObjectTarget target{ true, 0 };
    std::string st;

    // 1. Calcpad dimensional unit synthesis + #varsub
    AddFormulaNodeAction(doc, cmdStack, target, 36.0, 28.0, "P_tot = 3 bar + 100 psi", "", st);
    const std::uint64_t idF = AddFormulaNodeAction(doc, cmdStack, target, 36.0, 128.0, "k = 1.2", "", st);
    AssignTagToObjectAction(doc, cmdStack, target, idF, "examen", st);
    UpdateParameterSliderAction(doc, cmdStack, target, "k", 1.2, 0.2, 4.0, "", st);

    // 2. Casio ClassWiz session card
    CalcBridge::CalcStepRecord rec;
    rec.modelIdUtf8 = "fx-991CW";
    rec.keySeqUtf8 = "4.7 [x10^x] 3 [x] 2 [x10^x] [-] 3 [=]";
    rec.expressionUtf8 = "V_out = 4.7 kOhm * 2 mA | V";
    const auto qv = Cas::EvaluateExpression("4.7 kOhm * 2 mA | V");
    rec.resultUtf8 = qv.ok ? qv.formattedResultUtf8 : "9.4 V";
    rec.latexUtf8 = "V_{out} = 4.7\\,\\text{k}\\Omega \\cdot 2\\,\\text{mA}";
    AddCalcCardAction(doc, cmdStack, target, 36.0, 230.0, rec, st);

    // 3. 3D CVD-accessible surface z = sin(k*(x^2 + y^2)) / (1 + 0.25*(x^2 + y^2))
    AddSurface3DPlotNodeAction(
        doc, cmdStack, target, 392.0, 28.0,
        "sin(k * (x^2 + y^2))", -2.2, 2.2, -2.2, 2.2, st);

    // 4. Nonlinear Shockley Diode Half-Wave Rectifier + Filter Capacitor + Transient Oscilloscope + Laplace(s)
    AddTransientLaplaceCircuitAction(
        doc, cmdStack, target, 36.0, 358.0,
        "V1 1 0 10\nD1 1 2\nR1 2 0 1k\nC1 2 0 2.2u",
        35.0, 60.0, 2, st);

    cmdStack.SetUndoBoundary();
}

inline std::uint64_t AddCasioReverseBridgeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& exprUtf8,
    const std::string& modelIdUtf8,
    std::string& outStatus)
{
    if (exprUtf8.empty()) return 0;
    const auto plan = CalcBridge::TranslateExpressionToCasioKeys(exprUtf8, modelIdUtf8);
    CalcBridge::CalcStepRecord rec;
    rec.modelIdUtf8 = plan.modelIdUtf8;
    rec.keySeqUtf8 = plan.keySequenceUtf8;
    rec.expressionUtf8 = exprUtf8;
    rec.resultUtf8 = plan.evaluatedUtf8;
    rec.latexUtf8 = plan.latexUtf8;
    const std::uint64_t id = AddCalcCardAction(doc, cmdStack, target, x, y, rec, outStatus);
    outStatus = "Macro CASIO " + plan.modelIdUtf8 + " [" + plan.appModeUtf8 + "]: " + plan.keySequenceUtf8;
    return id;
}

inline std::uint64_t AddPhasorDiagramNodeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& phasorListUtf8,
    std::string& outStatus)
{
    if (phasorListUtf8.empty()) return 0;
    std::istringstream iss(phasorListUtf8);
    std::string item;
    nlohmann::json pArr = nlohmann::json::array();
    std::ostringstream convText;
    Cas::SymbolEnv env;
    int validCount = 0;

    while (std::getline(iss, item, ';')) {
        const auto first = item.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        item = item.substr(first);
        const auto ev = Cas::EvaluateExpression(item, env);
        if (!ev.ok) continue;
        if (ev.hasAssignment && !ev.assignedVarUtf8.empty()) {
            env[ev.assignedVarUtf8] = ev.result;
            convText << ev.assignedVarUtf8 << ": ";
        }
        convText << CalcBridge::FormatPhasorAllFormsUtf8(ev.result.value, ev.result.preferredUnitUtf8) << "\n";
        nlohmann::json pj;
        pj["mag"] = ev.result.Mag();
        pj["deg"] = ev.result.PhaseDeg();
        pArr.push_back(std::move(pj));
        ++validCount;
    }
    if (validCount == 0) {
        outStatus = "No se pudieron evaluar fasores (use r @ deg, ej: V = 120 @ 0 V; I = 4 @ -36.87 A)";
        return 0;
    }

    AnnotationObject mb;
    mb.kind = ObjectKind::MathBlock;
    mb.layerId = doc.ActiveLayerId();
    mb.bounds = { x, y, 420.0, 118.0 };
    mb.formulaExprUtf8 = "Fasores AC (Rect / Polar / Exp)";
    mb.evaluatedResultUtf8 = convText.str();
    mb.textUtf8 = convText.str();
    cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(mb)));

    AnnotationObject pb;
    pb.kind = ObjectKind::PlotBlock;
    pb.layerId = doc.ActiveLayerId();
    pb.bounds = { x + 436.0, y, 300.0, 250.0 };
    pb.captionUtf8 = "Diagrama Fasorial (" + std::to_string(validCount) + " vectores)";
    nlohmann::json cfg;
    cfg["mode"] = "phasor";
    cfg["phasors"] = std::move(pArr);
    pb.plotConfigUtf8 = cfg.dump();
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(pb)))) return 0;
    outStatus = "Diagrama fasorial generado (" + std::to_string(validCount) + " fasores en Rect/Polar/Exp)";
    return LastInsertedId(doc, target);
}

inline std::uint64_t AddBodePlotNodeAction(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& netlistUtf8,
    int outNode,
    double fStartHz,
    double fEndHz,
    std::string& outStatus)
{
    std::vector<Circuit::CircuitComponent> comps;
    std::string err;
    if (!Circuit::ParseNetlistText(netlistUtf8, comps, &err)) {
        outStatus = err;
        return 0;
    }
    const auto sweep = Circuit::ComputeBodeSweep(comps, outNode, (std::max)(0.1, fStartHz), (std::max)(fStartHz * 10.0, fEndHz), 61);
    if (sweep.empty()) {
        outStatus = "No se pudo calcular barrido de Bode";
        return 0;
    }

    double peakDb = -1e18;
    for (const auto& bp : sweep) peakDb = (std::max)(peakDb, bp.magDb);
    double cutoffHz = 0.0;
    for (std::size_t i = 1; i < sweep.size(); ++i) {
        const double d0 = sweep[i - 1].magDb - (peakDb - 3.0103);
        const double d1 = sweep[i].magDb - (peakDb - 3.0103);
        if ((d0 >= 0.0 && d1 <= 0.0) || (d0 <= 0.0 && d1 >= 0.0)) {
            const double t = std::abs(d0) / (std::max)(1e-9, std::abs(d0 - d1));
            cutoffHz = sweep[i - 1].freqHz + t * (sweep[i].freqHz - sweep[i - 1].freqHz);
            break;
        }
    }

    nlohmann::json cfg;
    nlohmann::json wx = nlohmann::json::array();
    nlohmann::json wy = nlohmann::json::array();
    for (const auto& bp : sweep) {
        wx.push_back(std::log10((std::max)(1e-9, bp.freqHz)));
        wy.push_back(bp.magDb);
    }
    cfg["waveX"] = std::move(wx);
    cfg["waveY"] = std::move(wy);

    AnnotationObject pb;
    pb.kind = ObjectKind::PlotBlock;
    pb.layerId = doc.ActiveLayerId();
    pb.bounds = { x, y, 390.0, 230.0 };
    std::ostringstream cap;
    cap.precision(4);
    cap << "Diagrama Bode Semilog |H(jw)| dB vs log10(f)";
    if (cutoffHz > 0.0) cap << " [fc=" << cutoffHz << " Hz]";
    pb.captionUtf8 = cap.str();
    pb.plotConfigUtf8 = cfg.dump();
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(pb)))) return 0;
    outStatus = pb.captionUtf8;
    return LastInsertedId(doc, target);
}

inline std::string FormatVariableInspectorUtf8(const DocumentModel& doc) {
    const auto vars = Cas::InspectDocumentVariables(doc);
    if (vars.empty()) {
        return "No hay variables matem\xC3\xA1ticas definidas en el documento actual.\nInserte un bloque #varsub (ej. V_s = 12 V, R_1 = 4.7 kOhm).";
    }
    std::ostringstream oss;
    oss << "Inspector F\xC3\xADsico de Variables (" << vars.size() << " s\xC3\xADmbolos activos):\n\n";
    for (const auto& v : vars) {
        oss << "  \xE2\x80\xA2 " << v.varNameUtf8 << " = " << v.currentValueUtf8 << "\n"
            << "      Magnitud: " << v.physicalQuantityUtf8 << "  |  Base SI: [" << v.siBaseUnitsUtf8 << "]\n"
            << "      Origen:   " << v.sourceNodeLabelUtf8 << "  (" << v.formulaExprUtf8 << ")\n\n";
    }
    return oss.str();
}

inline std::uint64_t AddPrivateTeacherLayerAction(DocumentModel& doc, std::string& outStatus) {
    const std::uint64_t lid = doc.AddLayer("Notas Privadas Docente");
    doc.SetLayerProperties(lid, true, false, false, 0.92);
    doc.SetActiveLayerId(lid);
    outStatus = "Capa 'Notas Privadas Docente' activa (visible al editar; oculta en Modo Presentador y al exportar PDF)";
    return lid;
}

} // namespace DocStudio::AppActions
