// ============================================================================
// cuadernu_studio_ffi.cpp
// Cross-platform C ABI wrapping the single-source C++17 CuadernU Studio core:
//   - DocumentModel + CommandStack + .cudoc ZIP storage (100% Windows parity)
//   - InkEngine (smoothing, shape snap, eraser) + SpatialIndex Quadtree
//   - PdfParser + PdfWriter (vector PDF) + OpenXml (DOCX/PPTX) + CSV/XLSX/DXF/MD
//   - EngineeringCAS (#varsub, SI units, phasors, equations, SEL, matrices, plots)
//   - ReactiveMathChemistry (118-element reaction balancer, molar mass, 2D)
//   - MNA Circuit Lab (DC/AC/Bode/Transient/Symbolic Laplace/Quine-McCluskey K-Map)
//   - KnowledgeVault Graph 2D + SM-2 StudySetManager + HorariosTEC Solver
// ============================================================================

#include "cuadernu_studio_ffi.h"

#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/ink_engine.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/utf_conv.hpp"
#include "../circuit/mna_solver.hpp"
#include "../math/engineering_cas.hpp"
#include "../math/reactive_math_chemistry.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/table_interop.hpp"
#include "academic/horarios_tec/horarios_tec.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

using json = nlohmann::json;
using namespace DocStudio;

int CopyStringOut(const std::string& s, char* outBuf, int outSize) noexcept
{
    const int needed = static_cast<int>(s.size()) + 1;
    if (!outBuf || outSize <= 0) return needed;
    if (outSize < needed) {
        std::memcpy(outBuf, s.data(), static_cast<std::size_t>(outSize - 1));
        outBuf[outSize - 1] = '\0';
        return needed;
    }
    std::memcpy(outBuf, s.data(), s.size());
    outBuf[s.size()] = '\0';
    return static_cast<int>(s.size());
}

std::string LowerExtUtf8(const std::string& pathUtf8)
{
    std::string ext = std::filesystem::path(Utf8ToWide(pathUtf8)).extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::vector<int> ParseIntCsv(const char* csv)
{
    std::vector<int> out;
    if (!csv || !*csv) return out;
    std::stringstream ss(csv);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        std::size_t i = 0;
        while (i < tok.size() && std::isspace(static_cast<unsigned char>(tok[i]))) ++i;
        if (i >= tok.size()) continue;
        try {
            out.push_back(std::stoi(tok.substr(i)));
        } catch (...) {
        }
    }
    return out;
}

json SerializeObject(const AnnotationObject& obj)
{
    json j;
    j["id"] = obj.id;
    j["zOrder"] = obj.zOrder;
    j["kind"] = static_cast<int>(obj.kind);
    j["layerId"] = obj.layerId;
    j["bounds"] = { obj.bounds.x, obj.bounds.y, obj.bounds.w, obj.bounds.h };
    j["strokeRgba"] = obj.strokeColor.PackRgba();
    j["fillRgba"] = obj.fillColor.PackRgba();
    j["strokeWidth"] = obj.strokeWidth;
    j["isHighlighter"] = obj.isHighlighter;
    j["penStyle"] = static_cast<int>(obj.penStyle);
    j["lockedInPlace"] = obj.lockedInPlace;
    j["opacity"] = obj.opacity;

    if (obj.kind == ObjectKind::InkStroke && !obj.inkPoints.empty()) {
        json pts = json::array();
        for (const auto& pt : obj.inkPoints) {
            pts.push_back({ pt.x, pt.y, pt.pressure });
        }
        j["inkPoints"] = std::move(pts);
    } else if (obj.kind == ObjectKind::Shape) {
        j["shapeKind"] = static_cast<int>(obj.shapeKind);
        j["shapeStart"] = { obj.shapeStart.x, obj.shapeStart.y };
        j["shapeEnd"] = { obj.shapeEnd.x, obj.shapeEnd.y };
        if (!obj.textUtf8.empty()) j["text"] = obj.textUtf8;
    } else if (obj.kind == ObjectKind::TextBox || obj.kind == ObjectKind::CodeBlock) {
        j["text"] = obj.textUtf8;
        j["fontSizePt"] = obj.fontSizePt;
        j["bold"] = obj.bold;
        j["italic"] = obj.italic;
    } else if (obj.kind == ObjectKind::MathBlock) {
        j["formulaExpr"] = obj.formulaExprUtf8;
        j["formulaVarName"] = obj.formulaVarNameUtf8;
        j["evaluatedResult"] = obj.evaluatedResultUtf8;
        j["text"] = obj.textUtf8;
    } else if (obj.kind == ObjectKind::CalcSessionCard) {
        j["calcModelId"] = obj.calcModelIdUtf8;
        j["formulaExpr"] = obj.formulaExprUtf8;
        j["evaluatedResult"] = obj.evaluatedResultUtf8;
        j["calcKeySeq"] = obj.calcKeySeqUtf8;
        j["caption"] = obj.captionUtf8;
        j["assetKey"] = obj.assetKey;
        j["imagePixelW"] = obj.imagePixelW;
        j["imagePixelH"] = obj.imagePixelH;
    } else if (obj.kind == ObjectKind::StudyTape) {
        j["tapeRevealed"] = obj.tapeRevealed;
        j["text"] = obj.textUtf8;
    } else if (obj.kind == ObjectKind::Image || obj.kind == ObjectKind::PdfCard) {
        j["assetKey"] = obj.assetKey;
        j["caption"] = obj.captionUtf8;
        j["imagePixelW"] = obj.imagePixelW;
        j["imagePixelH"] = obj.imagePixelH;
        j["printoutText"] = obj.printoutTextUtf8;
        j["printoutSourceName"] = obj.printoutSourceNameUtf8;
        j["printoutSourceKind"] = obj.printoutSourceKindUtf8;
        j["pdfSourcePageIndex0"] = obj.pdfSourcePageIndex0;
    }
    if (!obj.tags.empty()) {
        j["tags"] = obj.tags;
    }
    return j;
}

Cas::SymbolEnv ParseSymbolEnvJson(const char* envJsonUtf8)
{
    Cas::SymbolEnv env;
    if (!envJsonUtf8 || !*envJsonUtf8) return env;
    try {
        const json j = json::parse(envJsonUtf8);
        if (j.is_object()) {
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (it.value().is_string()) {
                    const auto r = Cas::EvaluateExpression(it.value().get<std::string>(), env);
                    if (r.ok) env[it.key()] = r.result;
                } else if (it.value().is_number()) {
                    Cas::QuantityValue q{};
                    q.value = { it.value().get<double>(), 0.0 };
                    env[it.key()] = q;
                }
            }
        }
    } catch (...) {
    }
    return env;
}

} // namespace

struct CuadernuDocHandle {
    std::mutex mtx;
    DocumentModel doc;
    CommandStack stack;

    explicit CuadernuDocHandle(int mode)
        : stack(256)
    {
        doc.ResetToBlank(mode == 1 ? DocumentMode::InfiniteCanvas : DocumentMode::PaginatedPdf);
    }
};

extern "C" {

int cuadernu_ffi_capabilities_json(char* out_json, int out_size)
{
    json j;
    j["engine"] = "CuadernU C++17 Unified Engine v47";
    j["cudocFormatVersion"] = CudocStorage::kCurrentFormatVersion;
    j["cudocMagic"] = CudocStorage::kFormatMagic;
    j["features"] = {
        "cudoc_zip_interop",
        "infinite_canvas_quadtree",
        "paginated_notebook",
        "ink_smoothing_shape_snap",
        "casio_session_cards",
        "vector_pdf_export",
        "pdf_docx_pptx_csv_xlsx_md_import",
        "dxf_cad_export",
        "engineering_cas_varsub",
        "reactive_chemistry_118",
        "mna_circuit_dc_ac_bode_transient_laplace",
        "karnaugh_quine_mccluskey",
        "knowledge_graph_2d",
        "sm2_flashcards",
        "horarios_tec_solver"
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

CuadernuDocHandle* cuadernu_doc_create(int mode)
{
    return new CuadernuDocHandle(mode);
}

void cuadernu_doc_destroy(CuadernuDocHandle* doc)
{
    delete doc;
}

int cuadernu_doc_set_title(CuadernuDocHandle* doc, const char* title_utf8)
{
    if (!doc || !title_utf8) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    doc->doc.SetTitle(title_utf8);
    return 1;
}

int cuadernu_doc_set_mode(CuadernuDocHandle* doc, int mode)
{
    if (!doc) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    doc->doc.SetMode(mode == 1 ? DocumentMode::InfiniteCanvas : DocumentMode::PaginatedPdf);
    return 1;
}

int cuadernu_doc_set_paper(CuadernuDocHandle* doc, int paper_pattern)
{
    if (!doc) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    const int clamped = std::clamp(paper_pattern, 0, static_cast<int>(PaperPattern::MusicStaff));
    const PaperPattern pat = static_cast<PaperPattern>(clamped);
    doc->doc.SetCanvasBackground(pat);
    for (auto& p : doc->doc.MutablePages()) {
        p.background = pat;
    }
    doc->doc.BumpRevision();
    return 1;
}

int cuadernu_doc_add_page(CuadernuDocHandle* doc, int paper_pattern)
{
    if (!doc) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    PageModel p{};
    p.pageId = doc->doc.AllocatePageId();
    const int clamped = std::clamp(paper_pattern, 0, static_cast<int>(PaperPattern::MusicStaff));
    p.background = static_cast<PaperPattern>(clamped);
    doc->doc.MutablePages().push_back(std::move(p));
    doc->doc.RebuildLayout();
    doc->doc.SetDirty(true);
    doc->doc.BumpRevision();
    return static_cast<int>(doc->doc.Pages().size()) - 1;
}

int cuadernu_doc_page_count(const CuadernuDocHandle* doc)
{
    if (!doc) return 0;
    return static_cast<int>(doc->doc.Pages().size());
}

uint64_t cuadernu_doc_add_ink_stroke(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    const float* xy_pressure_triples,
    int point_count,
    uint32_t rgba,
    double stroke_width,
    int pen_style,
    int is_highlighter)
{
    if (!doc || !xy_pressure_triples || point_count <= 0) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);

    AnnotationObject obj{};
    obj.kind = ObjectKind::InkStroke;
    obj.layerId = doc->doc.ActiveLayerId();
    obj.strokeColor = DocColor::UnpackRgba(rgba);
    obj.strokeWidth = std::clamp(stroke_width, 0.5, 48.0);
    obj.penStyle = static_cast<InkPenStyle>(std::clamp(pen_style, 0, 4));
    obj.isHighlighter = (is_highlighter != 0) || (obj.penStyle == InkPenStyle::Highlighter);

    std::vector<InkPoint> raw;
    raw.reserve(static_cast<std::size_t>(point_count));
    for (int i = 0; i < point_count; ++i) {
        InkPoint pt{};
        pt.x = static_cast<double>(xy_pressure_triples[i * 3 + 0]);
        pt.y = static_cast<double>(xy_pressure_triples[i * 3 + 1]);
        pt.pressure = std::clamp(xy_pressure_triples[i * 3 + 2], 0.05f, 1.0f);
        pt.timestampMs = static_cast<std::uint64_t>(i * 8);
        raw.push_back(pt);
    }
    obj.inkPoints = InkEngine::SmoothInkStroke(raw, 0.25);
    if (obj.inkPoints.empty()) obj.inkPoints = std::move(raw);
    obj.RecomputeBounds();

    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    auto cmd = std::make_unique<AddObjectCommand>(target, std::move(obj));
    auto* rawCmd = cmd.get();
    if (!doc->stack.Execute(doc->doc, std::move(cmd))) return 0;
    return rawCmd->ObjectId();
}

uint64_t cuadernu_doc_add_text_box(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* text_utf8,
    double font_size_pt,
    uint32_t rgba)
{
    if (!doc || !text_utf8) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);

    AnnotationObject obj{};
    obj.kind = ObjectKind::TextBox;
    obj.layerId = doc->doc.ActiveLayerId();
    obj.bounds = { x, y, (std::max)(40.0, w), (std::max)(24.0, h) };
    obj.textUtf8 = text_utf8;
    obj.fontSizePt = std::clamp(font_size_pt, 8.0, 72.0);
    obj.strokeColor = DocColor::UnpackRgba(rgba);

    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    auto cmd = std::make_unique<AddObjectCommand>(target, std::move(obj));
    auto* rawCmd = cmd.get();
    if (!doc->stack.Execute(doc->doc, std::move(cmd))) return 0;
    return rawCmd->ObjectId();
}

uint64_t cuadernu_doc_add_shape(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    int shape_kind,
    double x, double y, double w, double h,
    uint32_t stroke_rgba,
    uint32_t fill_rgba,
    double stroke_width)
{
    if (!doc) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);

    AnnotationObject obj{};
    obj.kind = ObjectKind::Shape;
    obj.layerId = doc->doc.ActiveLayerId();
    obj.shapeKind = static_cast<ShapeKind>(std::clamp(shape_kind, 0, 5));
    obj.shapeStart = { x, y };
    obj.shapeEnd = { x + w, y + h };
    obj.bounds = DocRect::FromMinMax(
        (std::min)(x, x + w), (std::min)(y, y + h),
        (std::max)(x, x + w), (std::max)(y, y + h));
    obj.strokeColor = DocColor::UnpackRgba(stroke_rgba);
    obj.fillColor = DocColor::UnpackRgba(fill_rgba);
    obj.strokeWidth = std::clamp(stroke_width, 0.5, 24.0);

    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    auto cmd = std::make_unique<AddObjectCommand>(target, std::move(obj));
    auto* rawCmd = cmd.get();
    if (!doc->stack.Execute(doc->doc, std::move(cmd))) return 0;
    return rawCmd->ObjectId();
}

uint64_t cuadernu_doc_add_math_block(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* expr_utf8,
    int angle_degrees)
{
    if (!doc || !expr_utf8) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);

    AnnotationObject obj{};
    obj.kind = ObjectKind::MathBlock;
    obj.layerId = doc->doc.ActiveLayerId();
    obj.bounds = { x, y, (std::max)(140.0, w), (std::max)(44.0, h) };
    obj.formulaExprUtf8 = expr_utf8;
    obj.textUtf8 = expr_utf8;

    Cas::CasEvalOptions opts{};
    opts.trigAngleMode = (angle_degrees != 0) ? Cas::CasAngleMode::Deg : Cas::CasAngleMode::Rad;
    opts.polarAngleMode = opts.trigAngleMode;
    const auto res = Cas::EvaluateExpression(expr_utf8, {}, opts);
    if (res.ok) {
        if (res.hasAssignment) obj.formulaVarNameUtf8 = res.assignedVarUtf8;
        obj.evaluatedResultUtf8 = res.formattedResultUtf8;
    } else {
        MathChem::MathEvalConfig cfg{};
        cfg.angleMode = (angle_degrees != 0) ? MathChem::AngleUnitMode::Degrees : MathChem::AngleUnitMode::Radians;
        const auto mres = MathChem::ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure(expr_utf8, cfg);
        obj.evaluatedResultUtf8 = mres.evaluatedUtf8.empty() ? res.errorUtf8 : mres.evaluatedUtf8;
    }

    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    auto cmd = std::make_unique<AddObjectCommand>(target, std::move(obj));
    auto* rawCmd = cmd.get();
    if (!doc->stack.Execute(doc->doc, std::move(cmd))) return 0;
    (void)Cas::EvaluateDocumentMathDag(doc->doc);
    return rawCmd->ObjectId();
}

uint64_t cuadernu_doc_add_casio_card(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* calc_model_utf8,
    const char* expr_utf8,
    const char* result_utf8,
    const char* key_seq_utf8,
    const uint8_t* png_bytes,
    size_t png_size)
{
    if (!doc) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);

    AnnotationObject obj{};
    obj.kind = ObjectKind::CalcSessionCard;
    obj.layerId = doc->doc.ActiveLayerId();
    obj.bounds = { x, y, (std::max)(180.0, w), (std::max)(96.0, h) };
    obj.calcModelIdUtf8 = calc_model_utf8 ? calc_model_utf8 : "fx-CG50";
    obj.formulaExprUtf8 = expr_utf8 ? expr_utf8 : "";
    obj.evaluatedResultUtf8 = result_utf8 ? result_utf8 : "";
    obj.calcKeySeqUtf8 = key_seq_utf8 ? key_seq_utf8 : "";
    obj.captionUtf8 = obj.calcModelIdUtf8 + (obj.formulaExprUtf8.empty() ? "" : (" · " + obj.formulaExprUtf8));
    obj.textUtf8 = obj.captionUtf8 + (obj.evaluatedResultUtf8.empty() ? "" : (" = " + obj.evaluatedResultUtf8));

    if (png_bytes && png_size > 8) {
        std::vector<std::uint8_t> bytes(png_bytes, png_bytes + png_size);
        obj.assetKey = doc->doc.RegisterAsset(std::move(bytes));
        obj.imagePixelW = 384;
        obj.imagePixelH = 216;
    }

    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    auto cmd = std::make_unique<AddObjectCommand>(target, std::move(obj));
    auto* rawCmd = cmd.get();
    if (!doc->stack.Execute(doc->doc, std::move(cmd))) return 0;
    return rawCmd->ObjectId();
}

uint64_t cuadernu_doc_add_sticky_note(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y,
    const char* text_utf8,
    uint32_t fill_rgba)
{
    if (!doc || !text_utf8) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);
    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    const DocColor col = (fill_rgba != 0) ? DocColor::UnpackRgba(fill_rgba) : DocColor::RGBA(254, 249, 195, 248);
    return CreateStickyNoteObject(doc->doc, doc->stack, target, x, y, text_utf8, col);
}

uint64_t cuadernu_doc_add_study_tape(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* label_utf8)
{
    if (!doc) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);

    AnnotationObject obj{};
    obj.kind = ObjectKind::StudyTape;
    obj.layerId = doc->doc.ActiveLayerId();
    obj.bounds = { x, y, (std::max)(60.0, w), (std::max)(24.0, h) };
    obj.fillColor = DocColor::RGBA(251, 191, 36, 235);
    obj.strokeColor = DocColor::RGBA(180, 83, 9, 255);
    obj.textUtf8 = label_utf8 ? label_utf8 : "Oclusi\xC3\xB3n de estudio";
    obj.tapeRevealed = false;

    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    auto cmd = std::make_unique<AddObjectCommand>(target, std::move(obj));
    auto* rawCmd = cmd.get();
    if (!doc->stack.Execute(doc->doc, std::move(cmd))) return 0;
    return rawCmd->ObjectId();
}

int cuadernu_doc_toggle_study_tape(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    uint64_t object_id)
{
    if (!doc || object_id == 0) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    AnnotationObject* obj = (on_canvas != 0)
        ? doc->doc.FindCanvasObjectMutable(object_id)
        : doc->doc.FindPageObjectMutable(static_cast<std::size_t>(std::max(0, page_index)), object_id);
    if (!obj || obj->kind != ObjectKind::StudyTape) return 0;
    obj->tapeRevealed = !obj->tapeRevealed;
    doc->doc.BumpRevision();
    return obj->tapeRevealed ? 1 : 0;
}

int cuadernu_doc_erase_at(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double radius)
{
    if (!doc) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    const double r = (std::max)(4.0, radius);
    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };

    std::vector<std::uint64_t> hits;
    const auto checkList = [&](const std::vector<AnnotationObject>& objs) {
        for (const auto& o : objs) {
            if (o.lockedInPlace) continue;
            if (!o.bounds.Contains(x, y, r)) continue;
            if (o.kind == ObjectKind::InkStroke && !o.inkPoints.empty()) {
                bool touched = false;
                for (const auto& pt : o.inkPoints) {
                    const double dx = pt.x - x;
                    const double dy = pt.y - y;
                    if (dx * dx + dy * dy <= r * r) {
                        touched = true;
                        break;
                    }
                }
                if (touched) hits.push_back(o.id);
            } else {
                hits.push_back(o.id);
            }
        }
    };

    if (target.isCanvas) {
        checkList(doc->doc.CanvasObjects());
    } else if (target.pageIndex < doc->doc.Pages().size()) {
        checkList(doc->doc.Pages()[target.pageIndex].objects);
    }

    if (hits.empty()) return 0;
    auto cmd = std::make_unique<BatchEraseObjectsCommand>(target, hits);
    if (!doc->stack.Execute(doc->doc, std::move(cmd))) return 0;
    return static_cast<int>(hits.size());
}

int cuadernu_doc_remove_object(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    uint64_t object_id)
{
    if (!doc || object_id == 0) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    const ObjectTarget target{ on_canvas != 0, static_cast<std::size_t>(std::max(0, page_index)) };
    auto cmd = std::make_unique<RemoveObjectCommand>(target, object_id);
    return doc->stack.Execute(doc->doc, std::move(cmd)) ? 1 : 0;
}

int cuadernu_doc_undo(CuadernuDocHandle* doc)
{
    if (!doc) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);
    return doc->stack.Undo(doc->doc) ? 1 : 0;
}

int cuadernu_doc_redo(CuadernuDocHandle* doc)
{
    if (!doc) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);
    return doc->stack.Redo(doc->doc) ? 1 : 0;
}

int cuadernu_doc_can_undo(const CuadernuDocHandle* doc)
{
    return (doc && doc->stack.CanUndo()) ? 1 : 0;
}

int cuadernu_doc_can_redo(const CuadernuDocHandle* doc)
{
    return (doc && doc->stack.CanRedo()) ? 1 : 0;
}

int cuadernu_doc_get_snapshot_json(const CuadernuDocHandle* doc, char* out_json, int out_size)
{
    if (!doc) return -1;
    auto* mutDoc = const_cast<CuadernuDocHandle*>(doc);
    std::lock_guard<std::mutex> lk(mutDoc->mtx);

    json j;
    j["title"] = doc->doc.Title();
    j["uuid"] = doc->doc.DocumentUuid();
    j["mode"] = static_cast<int>(doc->doc.Mode());
    j["paperPattern"] = static_cast<int>(doc->doc.CanvasBackground());
    j["revision"] = doc->doc.Revision();
    j["dirty"] = doc->doc.IsDirty();
    j["canUndo"] = doc->stack.CanUndo();
    j["canRedo"] = doc->stack.CanRedo();
    j["activeLayerId"] = doc->doc.ActiveLayerId();

    json pagesJson = json::array();
    for (std::size_t pi = 0; pi < doc->doc.Pages().size(); ++pi) {
        const auto& pg = doc->doc.Pages()[pi];
        json pj;
        pj["index"] = pi;
        pj["pageId"] = pg.pageId;
        pj["widthPt"] = pg.widthPt;
        pj["heightPt"] = pg.heightPt;
        pj["background"] = static_cast<int>(pg.background);
        pj["hasPdfSource"] = pg.hasPdfSource;
        pj["extractedText"] = pg.extractedTextUtf8;
        json objs = json::array();
        for (const auto& o : pg.objects) objs.push_back(SerializeObject(o));
        pj["objects"] = std::move(objs);
        pagesJson.push_back(std::move(pj));
    }
    j["pages"] = std::move(pagesJson);

    json canvasObjs = json::array();
    for (const auto& o : doc->doc.CanvasObjects()) {
        canvasObjs.push_back(SerializeObject(o));
    }
    j["canvasObjects"] = std::move(canvasObjs);
    j["assetCount"] = doc->doc.Assets().size();

    return CopyStringOut(j.dump(), out_json, out_size);
}

int64_t cuadernu_doc_get_asset_bytes(
    const CuadernuDocHandle* doc,
    const char* asset_key_utf8,
    uint8_t* out_buf,
    size_t out_size)
{
    if (!doc || !asset_key_utf8) return -1;
    auto* mutDoc = const_cast<CuadernuDocHandle*>(doc);
    std::lock_guard<std::mutex> lk(mutDoc->mtx);
    const auto* bytes = doc->doc.FindAsset(asset_key_utf8);
    if (!bytes) return -1;
    if (!out_buf || out_size < bytes->size()) {
        return static_cast<int64_t>(bytes->size());
    }
    if (!bytes->empty()) {
        std::memcpy(out_buf, bytes->data(), bytes->size());
    }
    return static_cast<int64_t>(bytes->size());
}

int cuadernu_doc_search_json(
    const CuadernuDocHandle* doc,
    const char* query_utf8,
    int accent_insensitive,
    char* out_json,
    int out_size)
{
    if (!doc || !query_utf8) return -1;
    auto* mutDoc = const_cast<CuadernuDocHandle*>(doc);
    std::lock_guard<std::mutex> lk(mutDoc->mtx);

    const auto hits = doc->doc.SearchText(query_utf8, false, accent_insensitive != 0);
    json arr = json::array();
    for (const auto& h : hits) {
        arr.push_back({
            {"onCanvas", h.onCanvas},
            {"pageIndex", h.pageIndex},
            {"objectId", h.objectId},
            {"excerpt", h.snippetUtf8},
            {"hasBox", h.hasBox},
            {"box", {h.worldBox.x, h.worldBox.y, h.worldBox.w, h.worldBox.h}}
        });
    }
    return CopyStringOut(arr.dump(), out_json, out_size);
}

int cuadernu_doc_evaluate_math_dag_json(CuadernuDocHandle* doc, char* out_json, int out_size)
{
    if (!doc) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);
    const auto rep = Cas::EvaluateDocumentMathDag(doc->doc);
    const auto vars = Cas::InspectDocumentVariables(doc->doc);
    json vArr = json::array();
    for (const auto& v : vars) {
        vArr.push_back({
            {"name", v.varNameUtf8},
            {"quantity", v.physicalQuantityUtf8},
            {"siUnits", v.siBaseUnitsUtf8},
            {"value", v.currentValueUtf8},
            {"source", v.sourceNodeLabelUtf8}
        });
    }
    json j = {
        {"ok", rep.ok},
        {"evaluatedCount", rep.evaluatedCount},
        {"cycleCount", rep.cycleCount},
        {"errorCount", rep.errorCount},
        {"errors", rep.errors},
        {"variables", std::move(vArr)}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_doc_save_cudoc(CuadernuDocHandle* doc, const char* path_utf8)
{
    if (!doc || !path_utf8 || !*path_utf8) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);
    std::string err;
    return CudocStorage::SaveToFile(Utf8ToWide(path_utf8), doc->doc, &err) ? 1 : 0;
}

int cuadernu_doc_load_cudoc(CuadernuDocHandle* doc, const char* path_utf8)
{
    if (!doc || !path_utf8 || !*path_utf8) return 0;
    std::lock_guard<std::mutex> lk(doc->mtx);
    DocumentModel loaded;
    std::string err;
    if (!CudocStorage::LoadFromFileWithRecovery(Utf8ToWide(path_utf8), loaded, nullptr, &err)) {
        return 0;
    }
    doc->doc = std::move(loaded);
    doc->stack.Clear();
    return 1;
}

int cuadernu_doc_export_pdf(const CuadernuDocHandle* doc, const char* path_utf8)
{
    if (!doc || !path_utf8 || !*path_utf8) return 0;
    auto* mutDoc = const_cast<CuadernuDocHandle*>(doc);
    std::lock_guard<std::mutex> lk(mutDoc->mtx);
    std::string err;
    if (doc->doc.Mode() == DocumentMode::InfiniteCanvas) {
        CanvasPdfExportOptions opts{};
        opts.paginated = true;
        return PdfWriter::ExportInfiniteCanvasToFile(doc->doc, opts, Utf8ToWide(path_utf8), &err) ? 1 : 0;
    }
    return PdfWriter::ExportDocumentToFile(doc->doc, Utf8ToWide(path_utf8), &err) ? 1 : 0;
}

int cuadernu_doc_export_markdown(const CuadernuDocHandle* doc, char* out_md, int out_size)
{
    if (!doc) return -1;
    auto* mutDoc = const_cast<CuadernuDocHandle*>(doc);
    std::lock_guard<std::mutex> lk(mutDoc->mtx);
    std::ostringstream oss;
    oss << "# " << doc->doc.Title() << "\n\n";
    auto dumpObjs = [&](const std::vector<AnnotationObject>& objs) {
        for (const auto& o : objs) {
            if (o.kind == ObjectKind::TextBox && !o.textUtf8.empty()) {
                oss << o.textUtf8 << "\n\n";
            } else if (o.kind == ObjectKind::MathBlock) {
                oss << "$$ " << o.formulaExprUtf8 << " = " << o.evaluatedResultUtf8 << " $$\n\n";
            } else if (o.kind == ObjectKind::CalcSessionCard) {
                oss << "> **CASIO " << o.calcModelIdUtf8 << ":** `" << o.formulaExprUtf8
                    << "` = **" << o.evaluatedResultUtf8 << "**\n\n";
            } else if (o.kind == ObjectKind::PdfCard && !o.printoutTextUtf8.empty()) {
                oss << "### " << o.captionUtf8 << "\n\n" << o.printoutTextUtf8 << "\n\n";
            }
        }
    };
    if (doc->doc.Mode() == DocumentMode::InfiniteCanvas) {
        dumpObjs(doc->doc.CanvasObjects());
    } else {
        for (std::size_t i = 0; i < doc->doc.Pages().size(); ++i) {
            const auto& pg = doc->doc.Pages()[i];
            oss << "## P\xC3\xA1gina " << (i + 1) << "\n\n";
            if (!pg.extractedTextUtf8.empty()) oss << pg.extractedTextUtf8 << "\n\n";
            dumpObjs(pg.objects);
        }
    }
    return CopyStringOut(oss.str(), out_md, out_size);
}

int cuadernu_doc_export_dxf(const CuadernuDocHandle* doc, const char* path_utf8, const char* scale_ratio_utf8)
{
    if (!doc || !path_utf8 || !*path_utf8) return 0;
    auto* mutDoc = const_cast<CuadernuDocHandle*>(doc);
    std::lock_guard<std::mutex> lk(mutDoc->mtx);
    std::string err;
    return DocStudioApp::ExportToDxfFile(doc->doc, path_utf8, scale_ratio_utf8 ? scale_ratio_utf8 : "1:1", &err) ? 1 : 0;
}

int cuadernu_doc_import_file_json(
    CuadernuDocHandle* doc,
    const char* path_utf8,
    int target_infinite_canvas,
    char* out_report_json,
    int out_size)
{
    if (!doc || !path_utf8 || !*path_utf8) return -1;
    std::lock_guard<std::mutex> lk(doc->mtx);

    const std::string ext = LowerExtUtf8(path_utf8);
    const std::wstring wpath = Utf8ToWide(path_utf8);
    const std::string baseName = std::filesystem::path(wpath).filename().string();
    std::string err;
    bool ok = false;
    int pagesAdded = 0;

    if (ext == ".cudoc") {
        DocumentModel loaded;
        ok = CudocStorage::LoadFromFileWithRecovery(wpath, loaded, nullptr, &err);
        if (ok) {
            pagesAdded = static_cast<int>(loaded.Pages().size());
            doc->doc = std::move(loaded);
            doc->stack.Clear();
        }
    } else if (ext == ".pdf") {
        PdfDocument pdf;
        ok = pdf.LoadFromFile(wpath, &err);
        if (ok && pdf.PageCount() > 0) {
            pagesAdded = static_cast<int>(pdf.PageCount());
            if (target_infinite_canvas != 0) {
                doc->doc.SetMode(DocumentMode::InfiniteCanvas);
                double curY = 60.0;
                for (const auto& existing : doc->doc.CanvasObjects()) {
                    curY = (std::max)(curY, existing.bounds.Bottom() + 40.0);
                }
                for (std::size_t i = 0; i < pdf.PageCount(); ++i) {
                    const auto& pi = pdf.PageAt(i);
                    const DocRect box = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
                    const double pw = (box.w > 18.0) ? box.w : 595.0;
                    const double ph = (box.h > 18.0) ? box.h : 842.0;
                    const double dw = (pw >= ph) ? 640.0 : 520.0;
                    const double dh = dw * (ph / pw);
                    AnnotationObject card{};
                    card.kind = ObjectKind::PdfCard;
                    card.layerId = doc->doc.ActiveLayerId();
                    card.bounds = { 60.0, curY, dw, dh };
                    card.pdfSourcePathUtf8 = path_utf8;
                    card.pdfSourcePageIndex0 = static_cast<int>(i);
                    card.printoutTextUtf8 = pi.extractedTextUtf8;
                    card.printoutTextRuns = pi.textBoxes;
                    card.printoutSourceKindUtf8 = "pdf";
                    card.printoutSourceNameUtf8 = baseName;
                    card.lockedInPlace = true;
                    card.captionUtf8 = baseName + " \xC2\xB7 P\xC3\xA1g. " + std::to_string(i + 1);
                    doc->doc.AddCanvasObject(std::move(card));
                    curY += dh + 32.0;
                }
            } else {
                doc->doc.SetMode(DocumentMode::PaginatedPdf);
                for (std::size_t i = 0; i < pdf.PageCount(); ++i) {
                    const auto& pi = pdf.PageAt(i);
                    PageModel pm{};
                    pm.pageId = doc->doc.AllocatePageId();
                    const DocRect box = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
                    if (box.w > 18.0 && box.h > 18.0) { pm.widthPt = box.w; pm.heightPt = box.h; }
                    pm.hasPdfSource = true;
                    pm.sourcePdfPathUtf8 = path_utf8;
                    pm.sourcePageIndex0 = static_cast<int>(i);
                    pm.extractedTextUtf8 = pi.extractedTextUtf8;
                    pm.textBoxes = pi.textBoxes;
                    doc->doc.MutablePages().push_back(std::move(pm));
                }
                doc->doc.RebuildLayout();
            }
            doc->doc.SetDirty(true);
            doc->doc.BumpRevision();
        }
    } else if (ext == ".docx") {
        DocumentModel tmp;
        ok = OpenXmlInterop::ImportFromDocxFile(wpath, tmp, &err);
        if (ok) {
            pagesAdded = static_cast<int>(tmp.Pages().size());
            doc->doc = std::move(tmp);
            doc->stack.Clear();
        }
    } else if (ext == ".pptx") {
        DocumentModel tmp;
        ok = OpenXmlInterop::ImportFromPptxFile(wpath, tmp, &err);
        if (ok) {
            pagesAdded = static_cast<int>(tmp.Pages().size());
            doc->doc = std::move(tmp);
            doc->stack.Clear();
        }
    } else if (ext == ".csv") {
        DocumentModel tmp;
        ok = DocStudioApp::ImportFromCsvFile(path_utf8, tmp, &err);
        if (ok) {
            pagesAdded = static_cast<int>(tmp.Pages().size());
            doc->doc = std::move(tmp);
            doc->stack.Clear();
        }
    } else if (ext == ".xlsx") {
        DocumentModel tmp;
        ok = DocStudioApp::ImportFromXlsxFile(path_utf8, tmp, &err);
        if (ok) {
            pagesAdded = static_cast<int>(tmp.Pages().size());
            doc->doc = std::move(tmp);
            doc->stack.Clear();
        }
    } else if (ext == ".md" || ext == ".txt") {
        DocumentModel tmp;
        ok = DocStudioApp::ImportFromMarkdownFile(path_utf8, tmp, &err);
        if (ok) {
            pagesAdded = static_cast<int>(tmp.Pages().size());
            doc->doc = std::move(tmp);
            doc->stack.Clear();
        }
    } else {
        err = "Formato no soportado: " + ext;
    }

    json j = {
        {"ok", ok},
        {"format", ext},
        {"pagesImported", pagesAdded},
        {"error", err}
    };
    return CopyStringOut(j.dump(), out_report_json, out_size);
}

int cuadernu_doc_build_flashcards_json(const CuadernuDocHandle* doc, char* out_json, int out_size)
{
    if (!doc) return -1;
    auto* mutDoc = const_cast<CuadernuDocHandle*>(doc);
    std::lock_guard<std::mutex> lk(mutDoc->mtx);
    const StudyDeck deck = StudySetManager::BuildDeckFromDocument(doc->doc, doc->doc.Title());
    json cards = json::array();
    for (const auto& c : deck.cards) {
        cards.push_back({
            {"id", c.id},
            {"question", c.questionUtf8},
            {"answer", c.answerUtf8},
            {"repetitions", c.repetitions},
            {"intervalDays", c.intervalDays},
            {"easeFactor", c.easeFactor},
            {"dueEpochSec", c.dueEpochSec}
        });
    }
    json j = { {"title", deck.titleUtf8}, {"cards", std::move(cards)} };
    return CopyStringOut(j.dump(), out_json, out_size);
}

// ============================================================================
// 2. Engineering CAS & Reactive Chemistry
// ============================================================================

int cuadernu_cas_eval_json(
    const char* expr_utf8,
    const char* env_json_utf8,
    int angle_degrees,
    char* out_json,
    int out_size)
{
    if (!expr_utf8) return -1;
    const auto env = ParseSymbolEnvJson(env_json_utf8);
    Cas::CasEvalOptions opts{};
    opts.trigAngleMode = (angle_degrees != 0) ? Cas::CasAngleMode::Deg : Cas::CasAngleMode::Rad;
    opts.polarAngleMode = opts.trigAngleMode;
    const auto res = Cas::EvaluateExpression(expr_utf8, env, opts);
    json j = {
        {"ok", res.ok},
        {"hasAssignment", res.hasAssignment},
        {"assignedVar", res.assignedVarUtf8},
        {"formatted", res.formattedResultUtf8},
        {"varSub", res.varSubExpansionUtf8},
        {"siUnits", res.result.dim.FormatSiString()},
        {"physicalQuantity", Cas::DescribePhysicalDimensionEs(res.result.dim)},
        {"real", res.result.value.real()},
        {"imag", res.result.value.imag()},
        {"magnitude", res.result.Mag()},
        {"phaseDeg", res.result.PhaseDeg()},
        {"diagnosticCode", res.diagnosticCode},
        {"error", res.errorUtf8}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_cas_solve_equation_json(
    const char* equation_utf8,
    const char* var_name_utf8,
    char* out_json,
    int out_size)
{
    if (!equation_utf8) return -1;
    const std::string var = (var_name_utf8 && *var_name_utf8) ? var_name_utf8 : "x";
    const auto sol = Cas::SolveAlgebraicEquation(equation_utf8, var, {});
    json stepsArr = json::array();
    if (!sol.steps.empty()) {
        for (const auto& st : sol.steps) {
            stepsArr.push_back(st.titleUtf8.empty() ? st.expressionUtf8
                                                    : (st.titleUtf8 + ": " + st.expressionUtf8));
        }
    } else {
        const auto fallbackSteps = MathChem::ReactiveMathChemistryEngine::BuildStepByStepExplanation(equation_utf8);
        for (const auto& s : fallbackSteps) stepsArr.push_back(s);
    }
    json j = {
        {"ok", sol.ok},
        {"solvedExact", sol.solvedExact},
        {"summary", sol.summaryUtf8},
        {"roots", sol.solutionStringsUtf8},
        {"steps", std::move(stepsArr)},
        {"maxResidual", sol.maxResidual},
        {"error", sol.errorUtf8}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_cas_solve_linear_system_json(const char* equations_utf8, char* out_json, int out_size)
{
    if (!equations_utf8) return -1;
    const auto res = Cas::SolveLinearSystemEquations(equations_utf8);
    json j = {
        {"ok", res.ok},
        {"varNames", res.varNames},
        {"values", res.values},
        {"determinant", res.determinant},
        {"summary", res.summaryUtf8},
        {"stepsTrace", res.stepsTraceUtf8},
        {"error", res.errorUtf8}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_cas_matrix_eval_json(const char* expr_utf8, char* out_json, int out_size)
{
    if (!expr_utf8) return -1;
    const auto res = Cas::EvaluateMatrixExpression(expr_utf8);
    json j = {
        {"ok", res.ok},
        {"rows", res.rows},
        {"cols", res.cols},
        {"isScalar", res.isScalar},
        {"scalarValue", res.scalarValue},
        {"matrix", res.matrix},
        {"formatted", res.formattedUtf8},
        {"latex", res.latexUtf8},
        {"error", res.errorUtf8}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_cas_sample_plot_json(
    const char* expr_utf8,
    const char* var_name_utf8,
    double x_min,
    double x_max,
    int num_samples,
    char* out_json,
    int out_size)
{
    if (!expr_utf8) return -1;
    const std::string var = (var_name_utf8 && *var_name_utf8) ? var_name_utf8 : "x";
    const std::size_t n = static_cast<std::size_t>(std::clamp(num_samples, 8, 1024));
    const auto pts = Cas::SampleFunction2D(expr_utf8, var, x_min, x_max, n, {});
    json arr = json::array();
    for (const auto& p : pts) {
        if (std::isfinite(p.x) && std::isfinite(p.y)) {
            arr.push_back({ p.x, p.y });
        }
    }
    json j = { {"ok", !arr.empty()}, {"points", std::move(arr)} };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_cas_check_step_json(
    const char* prev_expr_utf8,
    const char* next_expr_utf8,
    const char* var_name_utf8,
    char* out_json,
    int out_size)
{
    if (!prev_expr_utf8 || !next_expr_utf8) return -1;
    const std::string var = (var_name_utf8 && *var_name_utf8) ? var_name_utf8 : "x";
    const auto res = Cas::CheckAlgebraicStep(prev_expr_utf8, next_expr_utf8, var, {});
    json j = {
        {"equivalent", res.equivalent},
        {"dimensionMismatch", res.dimensionMismatch},
        {"maxResidual", res.maxResidual},
        {"statusCode", res.statusCode},
        {"message", res.messageUtf8}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_chem_analyze_json(const char* formula_or_reaction_utf8, char* out_json, int out_size)
{
    if (!formula_or_reaction_utf8) return -1;
    const auto res = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry(formula_or_reaction_utf8);
    json j = {
        {"valid", res.valid},
        {"isReaction", res.isReaction},
        {"balanced", res.balanced},
        {"input", res.inputUtf8},
        {"balancedEquation", res.balancedEquationUtf8},
        {"natural2D", res.natural2DUtf8},
        {"latex", res.latexUtf8},
        {"molarMassGPerMol", res.molarMassGPerMol},
        {"atomsLeft", res.atomCountsLeft},
        {"atomsRight", res.atomCountsRight}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

// ============================================================================
// 3. MNA Electronic Circuit Lab & K-Map
// ============================================================================

int cuadernu_circuit_solve_json(
    const char* netlist_utf8,
    const char* mode_utf8,
    double param1,
    double param2,
    int probe_node,
    char* out_json,
    int out_size)
{
    if (!netlist_utf8) return -1;
    const std::string mode = (mode_utf8 && *mode_utf8) ? mode_utf8 : "dc";
    std::vector<Circuit::CircuitComponent> comps;
    std::string parseErr;
    if (!Circuit::ParseNetlistText(netlist_utf8, comps, &parseErr) || comps.empty()) {
        json j = { {"ok", false}, {"error", parseErr.empty() ? "Netlist vac\xC3\xADo o inv\xC3\xA1lido." : parseErr} };
        return CopyStringOut(j.dump(), out_json, out_size);
    }

    if (mode == "bode") {
        const double f0 = (param1 > 0.0) ? param1 : 10.0;
        const double f1 = (param2 > f0) ? param2 : 1e6;
        const auto pts = Circuit::ComputeBodeSweep(comps, (std::max)(1, probe_node), f0, f1, 48);
        json arr = json::array();
        for (const auto& p : pts) {
            arr.push_back({ {"freqHz", p.freqHz}, {"magDb", p.magDb}, {"phaseDeg", p.phaseDeg} });
        }
        json j = { {"ok", !arr.empty()}, {"mode", "bode"}, {"points", std::move(arr)} };
        return CopyStringOut(j.dump(), out_json, out_size);
    }

    if (mode == "transient") {
        const double dur = (param1 > 0.0) ? param1 : 0.01;
        const double fSrc = (param2 >= 0.0) ? param2 : 0.0;
        const int targetNode = (std::max)(1, probe_node);
        const auto tr = Circuit::SimulateTransientMna(comps, dur, 160, fSrc, targetNode);
        json pts = json::array();
        auto wfIt = tr.nodeWaveforms.find(tr.probedNode);
        if (wfIt == tr.nodeWaveforms.end()) wfIt = tr.nodeWaveforms.find(targetNode);
        if (wfIt == tr.nodeWaveforms.end() && !tr.nodeWaveforms.empty()) wfIt = tr.nodeWaveforms.begin();
        if (wfIt != tr.nodeWaveforms.end()) {
            const auto& volts = wfIt->second;
            const std::size_t n = (std::min)(tr.timeSec.size(), volts.size());
            for (std::size_t i = 0; i < n; ++i) {
                pts.push_back({ tr.timeSec[i], volts[i] });
            }
        }
        json j = {
            {"ok", tr.ok},
            {"mode", "transient"},
            {"probedNode", tr.probedNode},
            {"vMax", tr.vMax},
            {"vMin", tr.vMin},
            {"vRms", tr.vRms},
            {"vDcAvg", tr.vDcAvg},
            {"summary", tr.summaryUtf8},
            {"points", std::move(pts)},
            {"error", tr.errorUtf8}
        };
        return CopyStringOut(j.dump(), out_json, out_size);
    }

    if (mode == "laplace") {
        const auto lap = Circuit::AnalyzeSymbolicLaplace(comps, (std::max)(1, probe_node));
        json j = {
            {"ok", lap.ok},
            {"mode", "laplace"},
            {"transferFunction", lap.transferFunctionUtf8},
            {"transferFunctionLatex", lap.transferFunctionLatex},
            {"symbolicHOfSLatex", lap.symbolicHOfSLatex},
            {"dcGain", lap.dcGain},
            {"nodalEquations", lap.nodalEquationsLatex},
            {"error", lap.errorUtf8}
        };
        return CopyStringOut(j.dump(), out_json, out_size);
    }

    const double freqHz = (mode == "ac" && param1 > 0.0) ? param1 : 0.0;
    const auto sol = Circuit::SolveMnaCircuit(comps, freqHz);
    json nodes = json::object();
    for (const auto& kv : sol.nodeVoltages) {
        nodes[std::to_string(kv.first)] = {
            {"real", kv.second.real()},
            {"imag", kv.second.imag()},
            {"mag", std::abs(kv.second)},
            {"phaseDeg", std::atan2(kv.second.imag(), kv.second.real()) * 180.0 / Circuit::kPi}
        };
    }
    json branches = json::array();
    for (const auto& b : sol.branches) {
        branches.push_back({
            {"name", b.name},
            {"currentMagA", std::abs(b.currentA)},
            {"currentRealA", b.currentA.real()},
            {"voltageMagV", std::abs(b.voltageDropV)},
            {"powerW", b.activePowerW}
        });
    }
    json j = {
        {"ok", sol.ok},
        {"mode", freqHz > 0.0 ? "ac" : "dc"},
        {"freqHz", sol.freqHz},
        {"summary", sol.summaryUtf8},
        {"nodeVoltages", std::move(nodes)},
        {"branches", std::move(branches)},
        {"kclKvlEquations", sol.kclKvlEquations},
        {"totalPowerSumW", sol.totalPowerSumW},
        {"error", sol.errorUtf8}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_circuit_kmap_json(
    int num_vars,
    const char* minterms_csv_utf8,
    const char* dontcares_csv_utf8,
    char* out_json,
    int out_size)
{
    const int nv = std::clamp(num_vars, 2, 4);
    const auto mins = ParseIntCsv(minterms_csv_utf8);
    const auto dcs = ParseIntCsv(dontcares_csv_utf8);
    const auto sol = Circuit::SimplifyKarnaughMap(nv, mins, dcs);
    json j = {
        {"ok", sol.ok},
        {"numVars", sol.numVars},
        {"minimizedSop", sol.minimizedSopUtf8},
        {"primeImplicants", sol.primeImplicants},
        {"gridValues", sol.gridValues},
        {"error", sol.errorUtf8}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

// ============================================================================
// 4. Knowledge Graph, SM-2 & Horarios TEC
// ============================================================================

int cuadernu_graph_build_json(const char* notes_json_utf8, char* out_json, int out_size)
{
    if (!notes_json_utf8) return -1;
    KnowledgeVault vault;
    try {
        const json arr = json::parse(notes_json_utf8);
        if (arr.is_array()) {
            for (const auto& item : arr) {
                const std::string title = item.value("title", "Sin t\xC3\xADtulo");
                const std::string folder = item.value("folder", "");
                const std::string body = item.value("body", "");
                const std::string id = vault.CreateNote(title, folder, body);
                if (item.contains("tags") && item["tags"].is_array()) {
                    if (auto* n = vault.FindNoteMutableById(id)) {
                        for (const auto& t : item["tags"]) {
                            if (t.is_string()) n->tagsUtf8.push_back(t.get<std::string>());
                        }
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        json errJ = { {"ok", false}, {"error", e.what()} };
        return CopyStringOut(errJ.dump(), out_json, out_size);
    }

    auto graph = vault.BuildGlobalGraph({});
    KnowledgeVault::ComputeGraphLayout2D(graph, 60, 800.0, 560.0);
    const auto broken = vault.FindBrokenWikiLinks();

    json nodes = json::array();
    for (const auto& n : graph.nodes) {
        nodes.push_back({
            {"id", n.idUtf8},
            {"label", n.labelUtf8},
            {"folder", n.folderUtf8},
            {"tags", n.tagsUtf8},
            {"degree", n.degree},
            {"resolved", n.resolved},
            {"x", n.x},
            {"y", n.y}
        });
    }
    json edges = json::array();
    for (const auto& e : graph.edges) {
        edges.push_back({
            {"from", e.fromIdUtf8},
            {"to", e.toIdUtf8},
            {"resolved", e.resolved}
        });
    }
    json j = {
        {"ok", true},
        {"nodes", std::move(nodes)},
        {"edges", std::move(edges)},
        {"brokenLinkCount", broken.size()}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_sm2_review_json(
    int repetitions,
    int interval_days,
    double ease_factor,
    int quality_0_to_5,
    uint64_t now_epoch_sec,
    char* out_json,
    int out_size)
{
    FlashcardItem card{};
    card.repetitions = (std::max)(0, repetitions);
    card.intervalDays = (std::max)(0, interval_days);
    card.easeFactor = (ease_factor >= 1.3) ? ease_factor : 2.5;
    StudySetManager::ReviewCardSm2(card, std::clamp(quality_0_to_5, 0, 5), now_epoch_sec);
    json j = {
        {"repetitions", card.repetitions},
        {"intervalDays", card.intervalDays},
        {"easeFactor", card.easeFactor},
        {"dueEpochSec", card.dueEpochSec}
    };
    return CopyStringOut(j.dump(), out_json, out_size);
}

int cuadernu_horarios_solve_json(
    const char* catalog_json_utf8,
    const char* busy_slots_json_utf8,
    char* out_json,
    int out_size)
{
    if (!catalog_json_utf8) return -1;
    academic::horarios_tec::SolverPreferences prefs{};
    prefs.maxResults = 30;
    const auto res = academic::horarios_tec::HorariosSolver::SolveFromJson(
        catalog_json_utf8,
        busy_slots_json_utf8 ? busy_slots_json_utf8 : "",
        prefs);
    const std::string outStr = academic::horarios_tec::ExportJson(res, { -1, true, true });
    return CopyStringOut(outStr, out_json, out_size);
}

} // extern "C"
