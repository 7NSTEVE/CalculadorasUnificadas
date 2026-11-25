#include "tutor_document_baker.hpp"
#include "../math/engineering_cas.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace DocStudio::Tutor {

using nlohmann::json;

TutorBakeReport BakeTutorSessionToDocument(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const TutorSessionStateMachine& session,
    double originX,
    double originY)
{
    TutorBakeReport rep;
    const auto& entries = session.SceneEntries();
    if (entries.empty()) {
        rep.summaryUtf8 = "No hay pasos en la pizarra del tutor para hornear.";
        return rep;
    }

    std::vector<AnnotationObject> batch;
    batch.reserve(entries.size() + 2);

    const std::uint64_t layerId = doc.ActiveLayerId();
    double curY = originY;
    constexpr double kColW = 540.0;
    constexpr double kGapY = 18.0;

    for (const auto& e : entries) {
        if (e.dimmed) continue;
        json payload = json::object();
        try {
            if (!e.payloadJsonUtf8.empty()) payload = json::parse(e.payloadJsonUtf8);
        } catch (...) {}

        AnnotationObject obj;
        obj.layerId = layerId;
        obj.tags.push_back("#tutor");
        if (!e.checkpointId.empty()) obj.tags.push_back("#" + e.checkpointId);

        if (e.actionType == "write_math" || e.actionType == "expand_intermediate_steps") {
            obj.kind = ObjectKind::MathBlock;
            std::string expr = e.latexOrTextUtf8;
            if (expr.empty() && payload.contains("substeps") && payload["substeps"].is_array()) {
                for (const auto& ss : payload["substeps"]) {
                    if (ss.is_object() && ss.contains("latex") && ss["latex"].is_string()) {
                        if (!expr.empty()) expr += " ; ";
                        expr += ss["latex"].get<std::string>();
                    }
                }
            }
            obj.formulaExprUtf8 = expr;
            obj.captionUtf8 = e.stepLabelUtf8.empty() ? ("Paso " + std::to_string(e.stepIndex)) : e.stepLabelUtf8;
            obj.bounds = { originX, curY, kColW, 92.0 };
            obj.strokeColor = DocColor::RGBA(0, 102, 224, 255);
            curY += obj.bounds.h + kGapY;
            ++rep.mathBlocksCount;
            batch.push_back(std::move(obj));
        } else if (e.actionType == "write_text" || e.actionType == "annotate_step" || e.actionType == "student_note") {
            obj.kind = ObjectKind::TextBox;
            obj.textUtf8 = e.latexOrTextUtf8;
            obj.captionUtf8 = (e.actionType == "student_note") ? "Nota del estudiante" : "Explicaci\xC3\xB3n del tutor";
            obj.fontSizePt = 13.0;
            const double lines = static_cast<double>((std::max)(std::size_t{ 2 }, (obj.textUtf8.size() / 68) + 1));
            const double h = std::clamp(34.0 + lines * 20.0, 68.0, 240.0);
            obj.bounds = { originX, curY, kColW, h };
            obj.strokeColor = DocColor::RGBA(22, 26, 34, 255);
            curY += obj.bounds.h + kGapY;
            ++rep.textBlocksCount;
            batch.push_back(std::move(obj));
        } else if (e.actionType == "plot_riemann_sum" || e.actionType == "plot_pv_diagram" ||
                   e.actionType == "plot_function" || e.actionType == "draw_thermal_shell") {
            obj.kind = ObjectKind::PlotBlock;
            obj.plotConfigUtf8 = e.payloadJsonUtf8;
            obj.captionUtf8 = e.stepLabelUtf8.empty() ? "Visualizaci\xC3\xB3n Interactiva STEM" : e.stepLabelUtf8;
            obj.bounds = { originX, curY, kColW, 210.0 };
            curY += obj.bounds.h + kGapY;
            ++rep.plotBlocksCount;
            batch.push_back(std::move(obj));
        } else if (e.actionType == "sync_calculator_state") {
            obj.kind = ObjectKind::CalcSessionCard;
            obj.calcModelIdUtf8 = payload.value("model", "fx-CG50");
            if (payload.contains("keys") && payload["keys"].is_array()) {
                std::string seq;
                for (const auto& k : payload["keys"]) {
                    if (k.is_string()) {
                        if (!seq.empty()) seq += " ";
                        seq += k.get<std::string>();
                    }
                }
                obj.calcKeySeqUtf8 = seq;
            }
            obj.evaluatedResultUtf8 = payload.value("expected_result", "");
            obj.captionUtf8 = "Verificaci\xC3\xB3n Casio (" + obj.calcModelIdUtf8 + ")";
            obj.bounds = { originX, curY, kColW, 112.0 };
            curY += obj.bounds.h + kGapY;
            ++rep.calcCardsCount;
            batch.push_back(std::move(obj));
        } else if (e.actionType == "trace_origin" || e.actionType == "draw_arrow") {
            obj.kind = ObjectKind::Shape;
            obj.shapeKind = ShapeKind::Arrow;
            obj.shapeStart = { originX + kColW + 18.0, (std::max)(originY, curY - 120.0) };
            obj.shapeEnd = { originX + kColW + 18.0, (std::max)(originY + 24.0, curY - 24.0) };
            obj.bounds = DocRect::FromMinMax(
                obj.shapeStart.x - 8.0, obj.shapeStart.y,
                obj.shapeEnd.x + 8.0, obj.shapeEnd.y);
            obj.strokeColor = DocColor::RGBA(0, 102, 224, 255);
            obj.captionUtf8 = payload.value("rule_es", payload.value("caption", "Origen"));
            batch.push_back(std::move(obj));
        }
    }

    if (batch.empty()) {
        rep.summaryUtf8 = "No hubo objetos visibles para insertar en el documento.";
        return rep;
    }

    const std::size_t count = batch.size();
    if (!cmdStack.Execute(
            doc,
            std::make_unique<BatchAddObjectsCommand>(
                target, std::move(batch), "Hornear sesi\xC3\xB3n de Pizarra Tutor"))) {
        rep.summaryUtf8 = "Error ejecutando transacci\xC3\xB3n at\xC3\xB3mica en CommandStack.";
        return rep;
    }

    doc.SetExtensionPayload("tutor_session.json", session.ExportStateJson());
    Cas::EvaluateDocumentMathDag(doc);

    rep.ok = true;
    rep.bakedObjectsCount = count;
    rep.summaryUtf8 = "Sesi\xC3\xB3n horneada: " + std::to_string(count) + " bloques en el cuaderno (Ctrl+Z disponible).";
    return rep;
}

} // namespace DocStudio::Tutor
