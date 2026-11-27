#include "mcp_server.hpp"

#include "../bridge/calc_bridge.hpp"
#include "../bridge/lab_report_exporter.hpp"
#include "../circuit/mna_solver.hpp"
#include "../core/utf_conv.hpp"
#include "../interop/notebook_migration_engine.hpp"
#include "../interop/notion_markdown.hpp"
#include "../interop/printout_engine.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../math/engineering_cas.hpp"
#include "../math/reactive_math_chemistry.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_signer.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/table_interop.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio::Mcp {

namespace {

std::string LowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<InkPoint> ParsePoints(const json& arr) {
    std::vector<InkPoint> out;
    if (!arr.is_array()) return out;
    for (const auto& p : arr) {
        if (p.is_array() && p.size() >= 2) out.push_back({ p[0].get<double>(), p[1].get<double>(), 0.5f, 0.0f, 0.0f, 0u });
        else if (p.is_object()) out.push_back({ p.value("x", 0.0), p.value("y", 0.0), 0.5f, 0.0f, 0.0f, 0u });
    }
    return out;
}

Cas::SymbolEnv ParseSymbolEnv(const json& args) {
    Cas::SymbolEnv env;
    if (args.contains("env") && args["env"].is_object()) {
        for (auto it = args["env"].begin(); it != args["env"].end(); ++it) {
            std::string expr = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
            const auto ev = Cas::EvaluateExpression(expr, env);
            if (ev.ok) env[it.key()] = ev.result;
        }
    }
    return env;
}

std::string FormatCplx(std::complex<double> z) {
    std::ostringstream oss;
    oss.precision(5);
    if (std::abs(z.imag()) < 1e-9) { oss << z.real(); return oss.str(); }
    oss << z.real() << (z.imag() >= 0.0 ? "+" : "") << z.imag() << "j";
    return oss.str();
}

} // namespace

void RegisterInteropAndStemTools(McpProtocolRouter& router, McpServerContext& ctx) {
    // ---------------- Group E: PDF Review, Presentations, Import & Export (4 tools) ----------------

    router.RegisterTool({
        "cuadernu_pdf_review",
        "Revision avanzada de documentos PDF: inspeccionar, extraer texto, buscar, reemplazar texto, censurar/redactar, unir, dividir, firmar digitalmente, verificar firmas, gestos/enlaces Flexcil, hilos de comentarios y exportacion selectiva.",
        MakeToolSchema(
            {
                "inspect_pdf", "extract_text", "search", "replace_text", "add_redaction",
                "merge_pdf", "split_pdf", "sign_pdf", "verify_signature",
                "flexcil_gesture", "flexcil_add_ink_link", "flexcil_jump_to_source",
                "annotations_list", "annotations_export_md", "selective_export_pdf",
                "comment_create_thread", "comment_reply", "comment_resolve", "comment_reopen",
                "comment_list", "comment_audit_log"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "pdf_path", { { "type", "string" } } },
                { "input_paths", { { "type", "array" } } },
                { "output_path", { { "type", "string" } } },
                { "page_index", { { "type", "integer" } } },
                { "pages", { { "type", "array" } } },
                { "query", { { "type", "string" } } },
                { "old_text", { { "type", "string" } } },
                { "new_text", { { "type", "string" } } },
                { "rect", { { "type", "object" } } },
                { "signer_name", { { "type", "string" } } },
                { "reason", { { "type", "string" } } },
                { "location", { { "type", "string" } } },
                { "points", { { "type", "array" } } },
                { "label", { { "type", "string" } } },
                { "object_id", { { "type", "integer" } } },
                { "include_annotations", { { "type", "boolean" } } },
                { "flattened", { { "type", "boolean" } } },
                { "only_annotated", { { "type", "boolean" } } },
                { "thread_id", { { "type", "integer" } } },
                { "author", { { "type", "string" } } },
                { "body", { { "type", "string" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");

            if (action == "inspect_pdf" || action == "extract_text") {
                const std::string pp = args.value("pdf_path", "");
                if (pp.empty()) return McpToolResult::Err("Se requiere 'pdf_path'.");
                PdfDocument pdf;
                std::string err;
                if (!pdf.LoadFromFile(Utf8ToWide(pp), &err)) return McpToolResult::Err(err);
                DocumentModel tmp;
                pdf.PopulateDocumentModel(tmp, pp, &err);
                json pages = json::array();
                std::ostringstream allText;
                for (std::size_t i = 0; i < tmp.Pages().size(); ++i) {
                    const auto& pg = tmp.Pages()[i];
                    pages.push_back({
                        { "index", i },
                        { "width_pt", pg.widthPt },
                        { "height_pt", pg.heightPt },
                        { "rotation_deg", pg.rotationDeg },
                        { "link_count", pg.pdfLinks.size() },
                        { "form_field_count", pg.formFields.size() },
                        { "text", pg.extractedTextUtf8 }
                    });
                    if (!pg.extractedTextUtf8.empty()) allText << "--- Pagina " << (i + 1) << " ---\n" << pg.extractedTextUtf8 << "\n";
                }
                return McpToolResult::Ok({
                    { "page_count", pdf.PageCount() },
                    { "pages", std::move(pages) },
                    { "full_text", allText.str() }
                });
            }
            if (action == "replace_text") {
                const std::string pp = args.value("pdf_path", "");
                if (pp.empty()) return McpToolResult::Err("Se requiere 'pdf_path'.");
                std::vector<std::uint8_t> inBytes, outBytes;
                std::string err;
                if (!ctx.ReadBytesFromFile(Utf8ToWide(pp), inBytes, &err)) return McpToolResult::Err(err);
                const std::size_t pIdx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                if (!PdfWriter::TryDirectReplaceTextInPdfBytes(inBytes, pIdx, args.value("old_text", ""), args.value("new_text", ""), outBytes, &err)) {
                    return McpToolResult::Err("No se pudo reemplazar directamente en el stream PDF: " + err);
                }
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "pdf_replaced", ".pdf");
                if (!ctx.WriteBytesToFile(outW, outBytes, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "replaced", true }, { "output_path", WideToUtf8(outW) } });
            }
            if (action == "merge_pdf") {
                std::vector<std::wstring> ins;
                if (args.contains("input_paths") && args["input_paths"].is_array()) {
                    for (const auto& v : args["input_paths"]) if (v.is_string()) ins.push_back(Utf8ToWide(v.get<std::string>()));
                }
                if (ins.size() < 2) return McpToolResult::Err("Se requieren al menos 2 rutas en 'input_paths'.");
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "merged", ".pdf");
                std::string err;
                const bool ok = PdfWriter::MergePdfFiles(ins, outW, &err);
                return ok ? McpToolResult::Ok({ { "merged", true }, { "output_path", WideToUtf8(outW) }, { "input_count", ins.size() } })
                          : McpToolResult::Err(err);
            }
            if (action == "split_pdf") {
                const std::string pp = args.value("pdf_path", "");
                std::vector<std::size_t> pages;
                if (args.contains("pages") && args["pages"].is_array()) {
                    for (const auto& p : args["pages"]) pages.push_back(p.get<std::size_t>());
                }
                if (pp.empty() || pages.empty()) return McpToolResult::Err("Se requieren 'pdf_path' y 'pages' (indices base 0).");
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "split", ".pdf");
                std::string err;
                const bool ok = PdfWriter::SplitPdfFile(Utf8ToWide(pp), pages, outW, &err);
                return ok ? McpToolResult::Ok({ { "split", true }, { "output_path", WideToUtf8(outW) }, { "page_count", pages.size() } })
                          : McpToolResult::Err(err);
            }
            if (action == "verify_signature") {
                const std::string pp = args.value("pdf_path", "");
                if (pp.empty()) return McpToolResult::Err("Se requiere 'pdf_path'.");
                const auto v = PdfSigner::VerifyPdfFile(Utf8ToWide(pp));
                return McpToolResult::Ok({
                    { "has_signature", v.hasSignature },
                    { "byte_range_valid", v.byteRangeValid },
                    { "cryptographic_valid", v.cryptographicValid },
                    { "trusted_by_system_root", v.trustedBySystemRoot },
                    { "signer_subject", v.signerSubjectUtf8 },
                    { "reason", v.reasonUtf8 },
                    { "location", v.locationUtf8 },
                    { "diagnostic", v.diagnosticUtf8 }
                });
            }

            auto s = ctx.GetOrCreateSession(args);
            const ObjectTarget tgt = TargetFromArgs(args, s->doc);

            if (action == "search") {
                const auto hits = s->doc.SearchText(args.value("query", ""), args.value("case_sensitive", false), args.value("accent_insensitive", true));
                json arr = json::array();
                for (const auto& h : hits) {
                    arr.push_back({ { "page_index", h.pageIndex }, { "on_canvas", h.onCanvas }, { "object_id", h.objectId }, { "snippet", h.snippetUtf8 } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "matches", std::move(arr) } });
            }
            if (action == "add_redaction") {
                AnnotationObject r{};
                r.id = s->doc.AllocateObjectId();
                r.zOrder = s->doc.AllocateZOrder();
                r.layerId = s->doc.ActiveLayerId();
                r.kind = ObjectKind::Shape;
                r.shapeKind = ShapeKind::Rectangle;
                r.isRedaction = true;
                r.fillColor = DocColor::RGBA(15, 23, 42, 255);
                r.strokeColor = DocColor::RGBA(15, 23, 42, 255);
                if (args.contains("rect") && args["rect"].is_object()) {
                    const auto& rc = args["rect"];
                    r.bounds = { rc.value("x", 72.0), rc.value("y", 72.0), rc.value("w", 200.0), rc.value("h", 24.0) };
                } else {
                    r.bounds = { 72.0, 72.0, 200.0, 24.0 };
                }
                s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(tgt, r));
                return McpToolResult::Ok({ { "object_id", r.id }, { "redaction", true } });
            }
            if (action == "sign_pdf") {
                PdfSignatureOptions opt{};
                opt.signerNameUtf8 = args.value("signer_name", "CuadernU");
                opt.reasonUtf8 = args.value("reason", "Aprobado");
                opt.locationUtf8 = args.value("location", "Local");
                opt.visualPageIndex0 = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "signed", ".pdf");
                std::string err;
                std::vector<std::uint8_t> pdfBytes, signedBytes;
                if (args.contains("pdf_path") && !args["pdf_path"].get<std::string>().empty()) {
                    if (!ctx.ReadBytesFromFile(Utf8ToWide(args["pdf_path"].get<std::string>()), pdfBytes, &err)) return McpToolResult::Err(err);
                } else {
                    if (!PdfWriter::ExportDocumentToBytes(s->doc, pdfBytes, &err)) return McpToolResult::Err(err);
                }
                if (!PdfSigner::SignPdfBytesWithSelfSigned(pdfBytes, opt, signedBytes, &err)) return McpToolResult::Err(err);
                if (!ctx.WriteBytesToFile(outW, signedBytes, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "signed", true }, { "output_path", WideToUtf8(outW) }, { "bytes", signedBytes.size() } });
            }
            if (action == "flexcil_gesture") {
                const auto pts = ParsePoints(args.value("points", json::array()));
                FlexcilGestureResult gres{};
                std::uint64_t noteId = 0, linkId = 0;
                const bool ok = s->flexcil.ApplyGestureStrokeOnDocument(
                    s->doc, WideToUtf8(s->filePathW), static_cast<std::size_t>(std::max(0, args.value("page_index", 0))),
                    pts, &gres, &noteId, &linkId);
                return McpToolResult::Ok({
                    { "applied", ok },
                    { "extracted_text", gres.extractedTextUtf8 },
                    { "created_note_object_id", noteId },
                    { "created_ink_link_id", linkId }
                });
            }
            if (action == "flexcil_add_ink_link") {
                InkLink lnk{};
                lnk.sourceDocPathUtf8 = args.value("pdf_path", WideToUtf8(s->filePathW));
                lnk.sourcePageIndex0 = args.value("page_index", 0);
                lnk.targetIsCanvas = tgt.isCanvas;
                lnk.targetPageIndex0 = static_cast<int>(tgt.pageIndex);
                lnk.targetObjectId = args.value("object_id", std::uint64_t{ 0 });
                lnk.labelUtf8 = args.value("label", "Referencia PDF");
                const std::uint64_t id = s->doc.AddInkLink(std::move(lnk));
                return McpToolResult::Ok({ { "ink_link_id", id } });
            }
            if (action == "flexcil_jump_to_source") {
                const std::uint64_t oid = args.value("object_id", std::uint64_t{ 0 });
                for (const auto& l : s->doc.InkLinks()) {
                    if (l.targetObjectId == oid || l.id == oid) {
                        return McpToolResult::Ok({ { "found", true }, { "source_doc", l.sourceDocPathUtf8 }, { "source_page", l.sourcePageIndex0 }, { "label", l.labelUtf8 } });
                    }
                }
                return McpToolResult::Ok({ { "found", false } });
            }
            if (action == "annotations_list") {
                const auto items = FlexcilStudyOverlay::ListAllAnnotations(s->doc);
                json arr = json::array();
                for (const auto& it : items) {
                    arr.push_back({ { "page_index", it.pageIndex0 }, { "object_id", it.objectId }, { "layer_id", it.layerId }, { "summary", it.summaryUtf8 } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "annotations", std::move(arr) } });
            }
            if (action == "annotations_export_md") {
                return McpToolResult::Ok({ { "markdown", FlexcilStudyOverlay::ExportAnnotationsMarkdownSummary(s->doc) } });
            }
            if (action == "selective_export_pdf") {
                PdfSelectiveExportOptions opt{};
                opt.includeAnnotations = args.value("include_annotations", true);
                opt.flattened = args.value("flattened", true);
                opt.onlyAnnotatedPages = args.value("only_annotated", false);
                if (args.contains("pages") && args["pages"].is_array()) {
                    for (const auto& p : args["pages"]) opt.selectedPages0.push_back(p.get<std::size_t>());
                }
                std::vector<std::uint8_t> outBytes;
                std::string err;
                if (!FlexcilStudyOverlay::ExportSelectivePdfToBytes(s->doc, opt, outBytes, &err)) return McpToolResult::Err(err);
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "selective", ".pdf");
                if (!ctx.WriteBytesToFile(outW, outBytes, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "output_path", WideToUtf8(outW) }, { "bytes", outBytes.size() } });
            }
            if (action == "comment_create_thread") {
                const std::uint64_t tid = s->comments.CreateThread(
                    static_cast<std::size_t>(std::max(0, args.value("page_index", 0))),
                    args.value("object_id", std::uint64_t{ 0 }),
                    {},
                    args.value("author", "Agente IA"),
                    args.value("body", ""));
                return McpToolResult::Ok({ { "thread_id", tid } });
            }
            if (action == "comment_reply") {
                const bool ok = s->comments.AddReply(args.value("thread_id", std::uint64_t{ 0 }), args.value("author", "Agente IA"), args.value("body", ""));
                return ok ? McpToolResult::Ok({ { "replied", true } }) : McpToolResult::Err("Hilo no encontrado.");
            }
            if (action == "comment_resolve") {
                const bool ok = s->comments.ResolveThread(args.value("thread_id", std::uint64_t{ 0 }), args.value("author", "Agente IA"));
                return ok ? McpToolResult::Ok({ { "resolved", true } }) : McpToolResult::Err("Hilo no encontrado.");
            }
            if (action == "comment_reopen") {
                const bool ok = s->comments.ReopenThread(args.value("thread_id", std::uint64_t{ 0 }), args.value("author", "Agente IA"));
                return ok ? McpToolResult::Ok({ { "reopened", true } }) : McpToolResult::Err("Hilo no encontrado.");
            }
            if (action == "comment_list") {
                auto serializeThreads = [](const std::vector<AnchoredCommentThread>& ts) {
                    json arr = json::array();
                    for (const auto& t : ts) {
                        json reps = json::array();
                        for (const auto& r : t.replies) reps.push_back({ { "reply_id", r.replyId }, { "author", r.authorUtf8 }, { "body", r.bodyUtf8 }, { "mentions", r.mentionsUtf8 } });
                        arr.push_back({ { "thread_id", t.threadId }, { "page_index", t.pageIndex0 }, { "anchor_object_id", t.anchorObjectId }, { "resolved", t.resolved }, { "replies", std::move(reps) } });
                    }
                    return arr;
                };
                return McpToolResult::Ok({
                    { "active", serializeThreads(s->comments.ActiveThreads()) },
                    { "resolved", serializeThreads(s->comments.ResolvedThreads()) }
                });
            }
            if (action == "comment_audit_log") {
                return McpToolResult::Ok({ { "audit_log", s->comments.AuditLog() } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_pdf_review: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_presentation",
        "Creacion, edicion, ordenamiento y exportacion de presentaciones (16:9 / 4:3, notas del orador en capa privada, importar/exportar .pptx editable con snapshots limpios, modo presentador, proyector y puntero laser).",
        MakeToolSchema(
            {
                "create_deck", "add_slide", "reorder_slides", "set_speaker_notes",
                "import_pptx", "export_pptx", "presenter_render_slide",
                "keylog_presenter_export", "projector_open", "projector_freeze", "projector_sync", "projector_close", "laser_pointer"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "aspect", { { "type", "string" }, { "enum", json::array({ "16:9", "4:3" }) } } },
                { "slide_title", { { "type", "string" } } },
                { "bullets", { { "type", "array" } } },
                { "body", { { "type", "string" } } },
                { "notes", { { "type", "string" } } },
                { "slide_index", { { "type", "integer" } } },
                { "from_index", { { "type", "integer" } } },
                { "to_index", { { "type", "integer" } } },
                { "path", { { "type", "string" } } },
                { "include_snapshots", { { "type", "boolean" } } },
                { "width_px", { { "type", "integer" } } },
                { "expression", { { "type", "string" } } },
                { "model", { { "type", "string" } } },
                { "active", { { "type", "boolean" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");

            auto ensureSpeakerLayer = [](DocumentModel& doc) -> std::uint64_t {
                for (const auto& ly : doc.Layers()) {
                    if (!ly.includeInExport) return ly.id;
                }
                const std::uint64_t lid = doc.AddLayer("Notas del orador");
                doc.SetLayerProperties(lid, true, false, false, 0.9);
                return lid;
            };

            if (action == "create_deck") {
                const bool widescreen = (args.value("aspect", "16:9") != "4:3");
                auto s = ctx.CreateSession(args.value("title", "Presentacion"), DocumentMode::PaginatedPdf, PaperPattern::Plain);
                if (!s->doc.MutablePages().empty()) {
                    s->doc.MutablePages()[0].widthPt = widescreen ? 960.0 : 720.0;
                    s->doc.MutablePages()[0].heightPt = 540.0;
                    s->doc.RebuildLayout();
                }
                if (args.contains("slide_title") || args.contains("title")) {
                    AnnotationObject titleObj{};
                    titleObj.id = s->doc.AllocateObjectId();
                    titleObj.zOrder = s->doc.AllocateZOrder();
                    titleObj.layerId = 1;
                    titleObj.kind = ObjectKind::TextBox;
                    titleObj.bounds = { 72.0, 160.0, (widescreen ? 816.0 : 576.0), 80.0 };
                    titleObj.textUtf8 = args.value("slide_title", args.value("title", "Presentacion"));
                    titleObj.fontSizePt = 32.0;
                    titleObj.bold = true;
                    titleObj.strokeColor = DocColor::RGBA(15, 23, 42, 255);
                    s->doc.AddPageObject(0, std::move(titleObj));
                }
                return McpToolResult::Ok({ { "session_id", s->sessionId }, { "aspect", widescreen ? "16:9" : "4:3" }, { "slide_count", s->doc.Pages().size() } });
            }
            if (action == "import_pptx") {
                const std::string p = args.value("path", "");
                if (p.empty()) return McpToolResult::Err("Se requiere 'path' del archivo .pptx.");
                auto s = ctx.CreateSession("Presentacion Importada", DocumentMode::PaginatedPdf, PaperPattern::Plain);
                std::string err;
                if (!OpenXmlInterop::ImportFromPptxFile(Utf8ToWide(p), s->doc, &err)) {
                    ctx.CloseSession(s->sessionId);
                    return McpToolResult::Err(err);
                }
                return McpToolResult::Ok({ { "session_id", s->sessionId }, { "title", s->doc.Title() }, { "slide_count", s->doc.Pages().size() } });
            }

            auto s = ctx.GetOrCreateSession(args);
            if (action == "add_slide") {
                const double wPt = !s->doc.Pages().empty() ? s->doc.Pages()[0].widthPt : 960.0;
                const double hPt = !s->doc.Pages().empty() ? s->doc.Pages()[0].heightPt : 540.0;
                PageModel pg{};
                pg.pageId = s->doc.AllocatePageId();
                pg.widthPt = wPt;
                pg.heightPt = hPt;
                pg.background = ParsePaperPattern(args.value("background", "plain"), PaperPattern::Plain);
                const std::size_t idx = s->doc.Pages().size();
                s->cmdStack.Execute(s->doc, std::make_unique<InsertPageCommand>(idx, pg));
                if (args.contains("slide_title")) {
                    AnnotationObject hdr{};
                    hdr.id = s->doc.AllocateObjectId();
                    hdr.zOrder = s->doc.AllocateZOrder();
                    hdr.layerId = 1;
                    hdr.kind = ObjectKind::TextBox;
                    hdr.bounds = { 56.0, 36.0, wPt - 112.0, 56.0 };
                    hdr.textUtf8 = args["slide_title"].get<std::string>();
                    hdr.fontSizePt = 24.0;
                    hdr.bold = true;
                    hdr.strokeColor = DocColor::RGBA(15, 23, 42, 255);
                    s->doc.AddPageObject(idx, std::move(hdr));
                }
                std::string bodyTxt = args.value("body", "");
                if (args.contains("bullets") && args["bullets"].is_array()) {
                    for (const auto& b : args["bullets"]) {
                        if (b.is_string()) {
                            if (!bodyTxt.empty()) bodyTxt += "\n";
                            bodyTxt += "• " + b.get<std::string>();
                        }
                    }
                }
                if (!bodyTxt.empty()) {
                    AnnotationObject bd{};
                    bd.id = s->doc.AllocateObjectId();
                    bd.zOrder = s->doc.AllocateZOrder();
                    bd.layerId = 1;
                    bd.kind = ObjectKind::TextBox;
                    bd.bounds = { 56.0, 110.0, wPt - 112.0, hPt - 150.0 };
                    bd.textUtf8 = std::move(bodyTxt);
                    bd.fontSizePt = 16.0;
                    bd.strokeColor = DocColor::RGBA(30, 41, 59, 255);
                    s->doc.AddPageObject(idx, std::move(bd));
                }
                if (args.contains("notes") && !args["notes"].get<std::string>().empty()) {
                    const std::uint64_t nLayer = ensureSpeakerLayer(s->doc);
                    AnnotationObject nt{};
                    nt.id = s->doc.AllocateObjectId();
                    nt.zOrder = s->doc.AllocateZOrder();
                    nt.layerId = nLayer;
                    nt.kind = ObjectKind::TextBox;
                    nt.bounds = { 56.0, hPt - 90.0, wPt - 112.0, 70.0 };
                    nt.textUtf8 = args["notes"].get<std::string>();
                    nt.fontSizePt = 11.0;
                    s->doc.AddPageObject(idx, std::move(nt));
                }
                return McpToolResult::Ok({ { "slide_index", idx }, { "slide_count", s->doc.Pages().size() } });
            }
            if (action == "reorder_slides") {
                const bool ok = s->cmdStack.Execute(s->doc, std::make_unique<ReorderPageCommand>(
                    static_cast<std::size_t>(std::max(0, args.value("from_index", 0))),
                    static_cast<std::size_t>(std::max(0, args.value("to_index", 0)))));
                return ok ? McpToolResult::Ok({ { "reordered", true } }) : McpToolResult::Err("Indices fuera de rango.");
            }
            if (action == "set_speaker_notes") {
                const int rawIdx = args.contains("slide_index") ? args.value("slide_index", 0) : args.value("page_index", 0);
                const std::size_t idx = static_cast<std::size_t>(std::max(0, rawIdx));
                if (idx >= s->doc.Pages().size()) return McpToolResult::Err("Diapositiva fuera de rango.");
                const std::uint64_t nLayer = ensureSpeakerLayer(s->doc);
                if (!args.value("append", false)) {
                    std::vector<std::uint64_t> oldNoteIds;
                    for (const auto& existing : s->doc.Pages()[idx].objects) {
                        if (existing.layerId == nLayer && existing.kind == ObjectKind::TextBox) {
                            oldNoteIds.push_back(existing.id);
                        }
                    }
                    if (!oldNoteIds.empty()) {
                        s->cmdStack.Execute(s->doc, std::make_unique<BatchEraseObjectsCommand>(ObjectTarget{ false, idx }, oldNoteIds));
                    }
                }
                AnnotationObject nt{};
                nt.id = s->doc.AllocateObjectId();
                nt.zOrder = s->doc.AllocateZOrder();
                nt.layerId = nLayer;
                nt.kind = ObjectKind::TextBox;
                nt.bounds = { 48.0, s->doc.Pages()[idx].heightPt - 90.0, s->doc.Pages()[idx].widthPt - 96.0, 72.0 };
                nt.textUtf8 = args.value("notes", "");
                nt.fontSizePt = 11.0;
                s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, idx }, std::move(nt)));
                return McpToolResult::Ok({ { "slide_index", idx }, { "private_layer_id", nLayer } });
            }
            if (action == "export_pptx") {
                std::vector<std::vector<std::uint8_t>> snaps;
                if (args.value("include_snapshots", false)) {
                    for (std::size_t i = 0; i < s->doc.Pages().size(); ++i) {
                        std::vector<std::uint8_t> png;
                        ctx.RenderPagePng(*s, i, args.value("width_px", 1280), png);
                        snaps.push_back(std::move(png));
                    }
                }
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_deck", ".pptx");
                std::string err;
                if (!OpenXmlInterop::ExportToPptxFile(s->doc, outW, snaps, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "exported", true }, { "path", WideToUtf8(outW) }, { "slide_count", s->doc.Pages().size() } });
            }
            if (action == "presenter_render_slide") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("slide_index", 0)));
                if (idx >= s->doc.Pages().size()) return McpToolResult::Err("Diapositiva fuera de rango.");
                s->activePageIndex = idx;
                std::vector<std::uint8_t> png;
                std::string err;
                const int wPx = std::clamp(args.value("width_px", 1280), 320, 2400);
                const int hPx = std::clamp(static_cast<int>(std::lround(wPx * (s->doc.Pages()[idx].heightPt / (std::max)(1.0, s->doc.Pages()[idx].widthPt)))), 200, 1800);
                if (!ctx.RenderViewportPng(*s, wPx, hPx, true, args.value("dark_theme", false), png, &err)) {
                    return McpToolResult::Err(err);
                }
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), "slide_" + std::to_string(idx), ".png");
                ctx.WriteBytesToFile(outW, png);
                return McpToolResult::OkWithImage({ { "slide_index", idx }, { "path", WideToUtf8(outW) }, { "presenter_mode", true } }, std::move(png));
            }
            if (action == "keylog_presenter_export") {
                const auto plan = CalcBridge::TranslateExpressionToCasioKeys(
                    args.value("expression", "sin(30)+2^3"),
                    args.value("model", "fx-991CW"));
                auto pres = CalcBridge::BuildKeyLogPresenterFromPlan(plan);
                json steps = json::array();
                do {
                    steps.push_back(CalcBridge::FormatPresenterStepCalloutUtf8(pres));
                } while (CalcBridge::StepPresenterNext(pres));
                return McpToolResult::Ok({ { "model", plan.modelIdUtf8 }, { "key_sequence", plan.keySequenceUtf8 }, { "callouts", std::move(steps) } });
            }
            if (action == "laser_pointer") {
                s->laserPointer.active = args.value("active", true);
                s->laserPointer.worldX = args.value("x", 0.0);
                s->laserPointer.worldY = args.value("y", 0.0);
                if (s->laserPointer.active) s->laserPointer.trail.push_back({ s->laserPointer.worldX, s->laserPointer.worldY });
                else s->laserPointer.trail.clear();
                return McpToolResult::Ok({ { "active", s->laserPointer.active }, { "x", s->laserPointer.worldX }, { "y", s->laserPointer.worldY } });
            }
            if (action == "projector_open" || action == "projector_freeze" || action == "projector_sync" || action == "projector_close") {
                if (!ctx.guiManager.HasActiveConnection()) {
                    return McpToolResult::Err("Para operar la ventana Proyector conecte o lance una instancia GUI con cuadernu_gui (spawn_background o attach_live).");
                }
                json resp;
                std::string err;
                const bool ok = ctx.guiManager.SendRequest({ { "action", action }, { "params", args } }, resp, "", 15000, &err);
                return ok ? McpToolResult::Ok(resp) : McpToolResult::Err(err);
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_presentation: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_interop_import",
        "Importacion y migracion universal: .goodnotes, .note, .one, .onepkg, .tinta, .nebo, .nsa, .ntb, .docx, .xlsx, .csv, .md, JSON Canvas, OneNote HTML, Notion, Web Clipper, Printout PDF/Office con OCR y audio.",
        MakeToolSchema(
            {
                "migrate_notebook", "insert_printout", "import_docx", "import_xlsx", "import_csv",
                "import_markdown", "import_json_canvas", "import_onenote_html", "import_notion_page",
                "web_clipper_fetch", "web_clipper_extract", "import_audio"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "path", { { "type", "string" } } },
                { "content", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "url", { { "type", "string" } } },
                { "notion_token", { { "type", "string" } } },
                { "target", { { "type", "string" }, { "enum", json::array({ "notebook_pages", "infinite_canvas" }) } } },
                { "ocr_when_no_text", { { "type", "boolean" } } },
                { "lock_in_place", { { "type", "boolean" } } }
            }),
        [&ctx, &router](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");
            if (args.contains("notion_token") && args["notion_token"].is_string()) {
                router.Redactor().RegisterSecret(args["notion_token"].get<std::string>());
            }
            auto s = ctx.GetOrCreateSession(args);

            if (action == "migrate_notebook") {
                const std::string p = args.value("path", "");
                if (p.empty()) return McpToolResult::Err("Se requiere 'path'.");
                Interop::MigrationResult res{};
                const bool ok = Interop::NotebookMigrationEngine::ImportAndMigrateFile(Utf8ToWide(p), s->doc, res);
                if (!ok) return McpToolResult::Err(res.errorUtf8);
                if (!res.primaryAudioWavBytes.empty()) {
                    s->audioSession.LoadFromWavBytes(res.primaryAudioWavBytes.data(), res.primaryAudioWavBytes.size());
                }
                return McpToolResult::Ok({
                    { "ok", true },
                    { "session_id", s->sessionId },
                    { "format", res.report.formatLabelUtf8 },
                    { "pages", res.report.pageCount },
                    { "strokes", res.report.editableStrokeCount },
                    { "summary", res.report.FormatHumanSummaryUtf8() }
                });
            }
            if (action == "insert_printout") {
                const std::string p = args.value("path", "");
                if (p.empty()) return McpToolResult::Err("Se requiere 'path' (PDF/DOCX/PPTX).");
                Printout::Options opt{};
                opt.target = (LowerAscii(args.value("target", "notebook_pages")) == "infinite_canvas")
                    ? Printout::Target::InfiniteCanvas : Printout::Target::NotebookPages;
                opt.lockInPlace = args.value("lock_in_place", true);
                opt.ocrWhenNoText = args.value("ocr_when_no_text", true);
                Printout::Report rep{};
                const bool ok = Printout::InsertPrintoutFromFile(
                    s->doc, s->cmdStack, Utf8ToWide(p), opt, s->activePageIndex, { 64.0, 64.0 }, rep);
                if (!ok) return McpToolResult::Err(rep.errorUtf8);
                return McpToolResult::Ok({
                    { "ok", true },
                    { "converter", rep.converterUsedUtf8 },
                    { "pages_inserted", rep.pagesInserted },
                    { "pages_with_text", rep.pagesWithText },
                    { "pages_with_ocr", rep.pagesWithOcr },
                    { "warnings", rep.warningsUtf8 }
                });
            }
            if (action == "import_docx") {
                std::string err;
                const bool ok = OpenXmlInterop::ImportFromDocxFile(Utf8ToWide(args.value("path", "")), s->doc, &err);
                return ok ? McpToolResult::Ok({ { "imported", true }, { "pages", s->doc.Pages().size() } }) : McpToolResult::Err(err);
            }
            if (action == "import_xlsx") {
                std::string err;
                const bool ok = DocStudioApp::ImportFromXlsxFile(args.value("path", ""), s->doc, &err);
                return ok ? McpToolResult::Ok({ { "imported", true }, { "pages", s->doc.Pages().size() } }) : McpToolResult::Err(err);
            }
            if (action == "import_csv") {
                std::string err;
                const bool ok = DocStudioApp::ImportFromCsvFile(args.value("path", ""), s->doc, &err);
                return ok ? McpToolResult::Ok({ { "imported", true }, { "pages", s->doc.Pages().size() } }) : McpToolResult::Err(err);
            }
            if (action == "import_markdown") {
                std::string err;
                bool ok = false;
                if (args.contains("path") && !args["path"].get<std::string>().empty()) {
                    ok = DocStudioApp::ImportFromMarkdownFile(args["path"].get<std::string>(), s->doc, &err);
                } else {
                    ok = DocStudioApp::ImportFromMarkdownText(args.value("content", ""), args.value("title", "Markdown"), s->doc);
                }
                return ok ? McpToolResult::Ok({ { "imported", true }, { "pages", s->doc.Pages().size() } }) : McpToolResult::Err(err);
            }
            if (action == "import_json_canvas") {
                std::string content = args.value("content", "");
                if (content.empty() && args.contains("path")) {
                    std::vector<std::uint8_t> b;
                    std::string err;
                    if (!ctx.ReadBytesFromFile(Utf8ToWide(args["path"].get<std::string>()), b, &err)) return McpToolResult::Err(err);
                    content.assign(reinterpret_cast<const char*>(b.data()), b.size());
                }
                std::string err;
                const bool ok = UniversalFormatsInterop::ImportFromJsonCanvas(content, s->doc, &err);
                return ok ? McpToolResult::Ok({ { "imported", true }, { "canvas_objects", s->doc.CanvasObjects().size() } }) : McpToolResult::Err(err);
            }
            if (action == "import_onenote_html") {
                std::string err;
                const bool ok = UniversalFormatsInterop::ImportPageFromOneNoteGraphHtml(args.value("content", ""), s->doc, &err);
                return ok ? McpToolResult::Ok({ { "imported", true }, { "pages", s->doc.Pages().size() } }) : McpToolResult::Err(err);
            }
            if (action == "import_notion_page") {
                std::string md, err;
                bool ok = false;
                if (args.contains("content") && !args["content"].get<std::string>().empty()) {
                    ok = NotionMarkdown::DecodePage(args["content"].get<std::string>(), args.value("page_id", ""), md, err);
                } else {
                    ok = NotionMarkdown::FetchPage(args.value("url", ""), Utf8ToWide(args.value("notion_token", "")), md, err);
                }
                if (!ok) return McpToolResult::Err(err);
                DocStudioApp::ImportFromMarkdownText(md, args.value("title", "Notion Page"), s->doc);
                return McpToolResult::Ok({ { "imported", true }, { "markdown", md }, { "pages", s->doc.Pages().size() } });
            }
            if (action == "web_clipper_fetch" || action == "web_clipper_extract") {
                std::string html = args.value("content", "");
                const std::string url = args.value("url", "");
                if (action == "web_clipper_fetch" || html.empty()) {
                    std::string err;
                    if (!WebClipperEngine::FetchUrlHtmlWinHttp(url, html, &err)) return McpToolResult::Err(err);
                }
                WebClipperTemplate tpl{};
                tpl.idUtf8 = "default";
                tpl.nameUtf8 = "Articulo Web";
                tpl.markdownTemplateUtf8 = "# {{title}}\n\nFuente: {{url}}\n\n{{selection}}\n\n{{content}}";
                const auto clipped = WebClipperEngine::ClipHtmlPage(url, html, args.value("selection", ""), tpl);
                return McpToolResult::Ok({
                    { "ok", clipped.ok },
                    { "title", clipped.extractedTitleUtf8 },
                    { "author", clipped.extractedAuthorUtf8 },
                    { "markdown", clipped.markdownOutputUtf8 }
                });
            }
            if (action == "import_audio") {
                std::string err;
                const bool ok = s->audioSession.LoadFromWavFile(args.value("path", ""), &err);
                return ok ? McpToolResult::Ok({ { "loaded", true }, { "duration_ms", s->audioSession.DurationMs() }, { "sample_rate", s->audioSession.SampleRateHz() } })
                          : McpToolResult::Err(err);
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_interop_import: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_interop_export",
        "Exportacion universal: PDF (paginado o lienzo infinito), DOCX, XLSX, CSV, DXF CAD, SVG, Markdown, JSON Canvas, OneNote HTML, paquete universal (.nebo/.goodnotes/.note), Anki, informe de laboratorio HTML5, tutorial de audio HTML5 e ICS.",
        MakeToolSchema(
            {
                "export_pdf", "export_docx", "export_xlsx", "export_csv", "export_dxf", "export_svg",
                "export_markdown", "export_json_canvas", "export_onenote_html", "export_universal_pkg",
                "export_anki_deck", "export_lab_report_html", "export_synced_audio_html5", "export_ics"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "path", { { "type", "string" } } },
                { "page_index", { { "type", "integer" } } },
                { "scale_ratio", { { "type", "string" } } },
                { "format_ext", { { "type", "string" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const std::string action = args.value("action", "");
            std::string err;

            if (action == "export_pdf") {
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ".pdf");
                bool ok = false;
                if (s->doc.Mode() == DocumentMode::InfiniteCanvas) {
                    CanvasPdfExportOptions opt{};
                    ok = PdfWriter::ExportInfiniteCanvasToFile(s->doc, opt, outW, &err);
                } else {
                    ok = PdfWriter::ExportDocumentToFile(s->doc, outW, &err);
                }
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "export_docx") {
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ".docx");
                const bool ok = OpenXmlInterop::ExportToDocxFile(s->doc, outW, {}, &err);
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "export_xlsx") {
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ".xlsx");
                const bool ok = DocStudioApp::ExportToXlsxFile(s->doc, WideToUtf8(outW), &err);
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "export_csv") {
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ".csv");
                const bool ok = DocStudioApp::ExportToCsvFile(s->doc, WideToUtf8(outW), &err);
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "export_dxf") {
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ".dxf");
                const bool ok = DocStudioApp::ExportToDxfFile(s->doc, WideToUtf8(outW), args.value("scale_ratio", "1:1"), &err);
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "export_svg") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                const std::string svg = UniversalFormatsInterop::ExportPageToSvg(s->doc, idx);
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_p" + std::to_string(idx), ".svg");
                ctx.WriteBytesToFile(outW, std::vector<std::uint8_t>(svg.begin(), svg.end()));
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "svg", svg } });
            }
            if (action == "export_markdown") {
                const std::string md = Bridge::ExportCalculationReportMarkdown(s->doc);
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ".md");
                ctx.WriteBytesToFile(outW, std::vector<std::uint8_t>(md.begin(), md.end()));
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "markdown", md } });
            }
            if (action == "export_json_canvas") {
                const std::string jc = UniversalFormatsInterop::ExportToJsonCanvas(s->doc);
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ".canvas");
                ctx.WriteBytesToFile(outW, std::vector<std::uint8_t>(jc.begin(), jc.end()));
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "json_canvas", jc } });
            }
            if (action == "export_onenote_html") {
                const std::string html = UniversalFormatsInterop::ExportPageToOneNoteGraphHtml(s->doc, static_cast<std::size_t>(std::max(0, args.value("page_index", 0))));
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_onenote", ".html");
                ctx.WriteBytesToFile(outW, std::vector<std::uint8_t>(html.begin(), html.end()));
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) } });
            }
            if (action == "export_universal_pkg") {
                const std::string ext = args.value("format_ext", ".nebo");
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId, ext);
                const bool ok = UniversalFormatsInterop::ExportUniversalNotePackage(s->doc, outW, ext, &err);
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "export_anki_deck") {
                const auto deck = s->studyDeck.cards.empty() ? StudySetManager::BuildDeckFromDocument(s->doc) : s->studyDeck;
                const std::string tsv = UniversalFormatsInterop::ExportStudyDeckToDelimited(deck, '\t');
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_anki", ".tsv");
                ctx.WriteBytesToFile(outW, std::vector<std::uint8_t>(tsv.begin(), tsv.end()));
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "card_count", deck.cards.size() }, { "tsv", tsv } });
            }
            if (action == "export_lab_report_html") {
                const std::string html = Bridge::ExportCalculationReportHtml(s->doc);
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_lab", ".html");
                ctx.WriteBytesToFile(outW, std::vector<std::uint8_t>(html.begin(), html.end()));
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "bytes", html.size() } });
            }
            if (action == "export_synced_audio_html5") {
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_audio", ".html");
                const bool ok = s->audioSession.ExportSyncedTutorialHtml5(s->doc, WideToUtf8(outW), &err);
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "export_ics") {
                const auto rep = StudyAssistantTools::ExtractTasksAndDeadlines(s->doc, &ctx.knowledgeVault, nullptr);
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_tasks", ".ics");
                ctx.WriteBytesToFile(outW, std::vector<std::uint8_t>(rep.icsCalendarUtf8.begin(), rep.icsCalendarUtf8.end()));
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "task_count", rep.extracted.size() }, { "ics", rep.icsCalendarUtf8 } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_interop_export: " + action);
        }
    });

    // ---------------- Group F: STEM Math, Circuits & Casio 4-Model Bridge (3 tools) ----------------

    router.RegisterTool({
        "cuadernu_stem_math",
        "Motor STEM CAS simbólico con unidades SI/imperiales y fasores, DAG de documento/tablas, solucionador algebraico/SEL/matrices, verificacion paso a paso, tutor socratico, graficas 2D/3D, regresion CSV, quimica/balanceo, estatica/incertidumbre, busqueda AST de formulas y bloque matematico en lienzo.",
        MakeToolSchema(
            {
                "eval_cas", "eval_doc_dag", "eval_table_dag", "solve_equation", "solve_linear_system",
                "eval_matrix", "check_step", "teach_me_socratic", "sample_plot_2d", "sample_plot_3d",
                "fit_csv_regression", "add_formula_node", "add_plot_node", "inspect_var_dimensions",
                "chemistry_analyze_balance", "statics_fbd_solve", "uncertainty_propagate",
                "formula_ast_fingerprint", "reactive_math_eval"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "page_index", { { "type", "integer" } } },
                { "expr", { { "type", "string" } } },
                { "var", { { "type", "string" } } },
                { "env", { { "type", "object" } } },
                { "prev_expr", { { "type", "string" } } },
                { "next_expr", { { "type", "string" } } },
                { "steps", { { "type", "array" } } },
                { "x_min", { { "type", "number" } } },
                { "x_max", { { "type", "number" } } },
                { "y_min", { { "type", "number" } } },
                { "y_max", { { "type", "number" } } },
                { "samples", { { "type", "integer" } } },
                { "csv", { { "type", "string" } } },
                { "degree", { { "type", "integer" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } },
                { "w", { { "type", "number" } } },
                { "h", { { "type", "number" } } },
                { "cells", { { "type", "array" } } },
                { "fbd_json", { { "type", "string" } } },
                { "uncertain_vars", { { "type", "object" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");
            auto env = ParseSymbolEnv(args);

            if (action == "eval_cas") {
                const auto r = Cas::EvaluateExpression(args.value("expr", ""), env);
                return McpToolResult::Ok({
                    { "ok", r.ok },
                    { "result", r.formattedResultUtf8 },
                    { "varsub", r.varSubExpansionUtf8 },
                    { "assigned_var", r.assignedVarUtf8 },
                    { "code", r.diagnosticCode },
                    { "error", r.errorUtf8 }
                });
            }
            if (action == "solve_equation") {
                const auto r = Cas::SolveDeterministic(args.value("expr", ""), args.value("var", "x"), env);
                json steps = json::array();
                for (const auto& st : r.primaryMethod.steps) {
                    steps.push_back({ { "step", st.stepNumber }, { "title", st.titleUtf8 }, { "expr", st.expressionUtf8 }, { "latex", st.latexUtf8 }, { "rationale", st.rationaleUtf8 } });
                }
                return McpToolResult::Ok({
                    { "ok", r.ok },
                    { "method", r.primaryMethod.methodNameUtf8 },
                    { "solutions", r.primaryMethod.solutionsUtf8 },
                    { "summary", r.primaryMethod.finalSummaryUtf8 },
                    { "steps", std::move(steps) },
                    { "error", r.errorUtf8 }
                });
            }
            if (action == "solve_linear_system") {
                const auto r = Cas::SolveLinearSystemEquations(args.value("expr", ""));
                return McpToolResult::Ok({
                    { "ok", r.ok },
                    { "vars", r.varNames },
                    { "values", r.values },
                    { "determinant", r.determinant },
                    { "summary", r.summaryUtf8 },
                    { "trace", r.stepsTraceUtf8 },
                    { "error", r.errorUtf8 }
                });
            }
            if (action == "eval_matrix") {
                const auto r = Cas::EvaluateMatrixExpression(args.value("expr", ""));
                return McpToolResult::Ok({
                    { "ok", r.ok },
                    { "rows", r.rows },
                    { "cols", r.cols },
                    { "is_scalar", r.isScalar },
                    { "scalar", r.scalarValue },
                    { "matrix", r.matrix },
                    { "formatted", r.formattedUtf8 },
                    { "latex", r.latexUtf8 },
                    { "error", r.errorUtf8 }
                });
            }
            if (action == "check_step") {
                if (args.contains("steps") && args["steps"].is_array()) {
                    std::vector<std::string> lines;
                    for (const auto& l : args["steps"]) if (l.is_string()) lines.push_back(l.get<std::string>());
                    const auto rep = Cas::CheckMyWork(lines, env);
                    return McpToolResult::Ok({ { "all_valid", rep.allValid }, { "first_error_line", rep.firstErrorLineIndex }, { "summary", rep.summaryUtf8 } });
                }
                const auto r = Cas::CheckAlgebraicStepV2(args.value("prev_expr", ""), args.value("next_expr", ""), env);
                return McpToolResult::Ok({
                    { "equivalent", r.equivalent },
                    { "dimension_mismatch", r.dimensionMismatch },
                    { "max_residual", r.maxResidual },
                    { "status_code", r.statusCode },
                    { "message", r.messageUtf8 },
                    { "hint", r.hintUtf8 }
                });
            }
            if (action == "teach_me_socratic") {
                std::vector<std::string> stSteps;
                if (args.contains("steps") && args["steps"].is_array()) {
                    for (const auto& l : args["steps"]) if (l.is_string()) stSteps.push_back(l.get<std::string>());
                }
                const auto resp = Cas::GenerateTeachMeSocratic(args.value("expr", ""), stSteps, args.value("var", "x"), env);
                json hints = json::array();
                for (const auto& h : resp.hints) {
                    hints.push_back({ { "level", h.level }, { "prompt", h.promptUtf8 }, { "question", h.questionForStudentUtf8 } });
                }
                return McpToolResult::Ok({ { "ok", resp.ok }, { "context", resp.contextSummaryUtf8 }, { "hints", std::move(hints) } });
            }
            if (action == "sample_plot_2d") {
                const auto pts = Cas::SampleFunction2D(
                    args.value("expr", "sin(x)"), args.value("var", "x"),
                    args.value("x_min", -6.28), args.value("x_max", 6.28),
                    static_cast<std::size_t>(std::clamp(args.value("samples", 64), 4, 512)), env);
                json arr = json::array();
                for (const auto& p : pts) arr.push_back({ p.x, p.y });
                return McpToolResult::Ok({ { "count", arr.size() }, { "points", std::move(arr) } });
            }
            if (action == "sample_plot_3d") {
                const auto pts = Cas::SampleSurface3D(
                    args.value("expr", "sin(x)*cos(y)"),
                    args.value("x_min", -3.0), args.value("x_max", 3.0), 16,
                    args.value("y_min", -3.0), args.value("y_max", 3.0), 16, env);
                json arr = json::array();
                for (const auto& p : pts) if (p.valid) arr.push_back({ p.x, p.y, p.z });
                return McpToolResult::Ok({ { "count", arr.size() }, { "points", std::move(arr) } });
            }
            if (action == "fit_csv_regression") {
                const auto r = Cas::FitCsvRegression(args.value("csv", ""), args.value("degree", 1));
                return McpToolResult::Ok({ { "ok", r.ok }, { "degree", r.degree }, { "coeffs", r.coeffs }, { "r_squared", r.rSquared }, { "equation", r.equationUtf8 } });
            }
            if (action == "eval_table_dag") {
                Cas::TableSheetModel sheet{};
                if (args.contains("cells") && args["cells"].is_array()) {
                    for (const auto& c : args["cells"]) {
                        sheet.cells.push_back({ c.value("ref", "A1"), c.value("expr", ""), c.value("export_var", "") });
                    }
                }
                const auto rep = Cas::EvaluateTableDag(sheet, env);
                json res = json::array();
                for (const auto& o : rep.results) res.push_back({ { "ref", o.cellRef }, { "ok", o.ok }, { "formatted", o.formattedUtf8 }, { "error", o.errorUtf8 } });
                return McpToolResult::Ok({ { "ok", rep.ok }, { "evaluated", rep.evaluatedCount }, { "cycles", rep.cycleCount }, { "results", std::move(res) }, { "errors", rep.errors } });
            }
            if (action == "chemistry_analyze_balance") {
                const auto c = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry(args.value("expr", ""));
                return McpToolResult::Ok({
                    { "valid", c.valid },
                    { "is_reaction", c.isReaction },
                    { "balanced", c.balanced },
                    { "balanced_equation", c.balancedEquationUtf8 },
                    { "natural_2d", c.natural2DUtf8 },
                    { "latex", c.latexUtf8 },
                    { "molar_mass_g_mol", c.molarMassGPerMol }
                });
            }
            if (action == "statics_fbd_solve") {
                Cas::FbdModel model{};
                std::string err;
                if (!Cas::FbdModelFromJson(args.value("fbd_json", "{}"), model, &err)) return McpToolResult::Err(err);
                const auto sol = Cas::SolveFbdEquilibrium(model);
                json rx = json::object();
                for (const auto& [k, v] : sol.reactions) rx[k] = v;
                return McpToolResult::Ok({
                    { "ok", sol.ok },
                    { "in_equilibrium", sol.inEquilibrium },
                    { "status", sol.determinacyStatus },
                    { "reactions", std::move(rx) },
                    { "trace", sol.stepsTraceUtf8 },
                    { "error", sol.errorUtf8 }
                });
            }
            if (action == "uncertainty_propagate") {
                std::unordered_map<std::string, Cas::UncertainValue> uvars;
                if (args.contains("uncertain_vars") && args["uncertain_vars"].is_object()) {
                    for (auto it = args["uncertain_vars"].begin(); it != args["uncertain_vars"].end(); ++it) {
                        Cas::UncertainValue uv{};
                        uv.value = it.value().value("value", 0.0);
                        uv.uncertainty = it.value().value("uncertainty", 0.0);
                        uv.unitUtf8 = it.value().value("unit", "");
                        uvars[it.key()] = uv;
                    }
                }
                const auto pr = Cas::PropagateUncertainty(args.value("expr", ""), uvars, 5000);
                return McpToolResult::Ok({
                    { "ok", pr.ok },
                    { "mean", pr.mean },
                    { "rss_uncertainty", pr.rssUncertainty },
                    { "formatted_rss", pr.formattedRssUtf8 },
                    { "formatted_mc", pr.formattedMcUtf8 },
                    { "error", pr.errorUtf8 }
                });
            }
            if (action == "formula_ast_fingerprint") {
                const auto fp = Cas::ComputeFormulaFingerprint(args.value("expr", ""), true);
                return McpToolResult::Ok({ { "ok", fp.ok }, { "hash64", fp.hash64 }, { "canonical", fp.canonicalAlphaExpr }, { "vars", fp.varOrder } });
            }

            auto s = ctx.GetOrCreateSession(args);
            const ObjectTarget tgt = TargetFromArgs(args, s->doc);

            if (action == "eval_doc_dag") {
                const auto rep = Cas::EvaluateDocumentMathDag(s->doc);
                return McpToolResult::Ok({ { "ok", rep.ok }, { "evaluated", rep.evaluatedCount }, { "cycles", rep.cycleCount }, { "errors", rep.errors } });
            }
            if (action == "inspect_var_dimensions") {
                const auto vars = Cas::InspectDocumentVariables(s->doc);
                json arr = json::array();
                for (const auto& v : vars) {
                    arr.push_back({ { "var", v.varNameUtf8 }, { "quantity", v.physicalQuantityUtf8 }, { "si_units", v.siBaseUnitsUtf8 }, { "value", v.currentValueUtf8 }, { "source", v.sourceNodeLabelUtf8 } });
                }
                return McpToolResult::Ok({ { "variables", std::move(arr) } });
            }
            if (action == "add_formula_node") {
                const std::string expr = args.value("expr", "x = 2 + 2");
                const auto ev = Cas::EvaluateExpression(expr, env);
                AnnotationObject o{};
                o.id = s->doc.AllocateObjectId();
                o.zOrder = s->doc.AllocateZOrder();
                o.layerId = s->doc.ActiveLayerId();
                o.kind = ObjectKind::MathBlock;
                o.bounds = { args.value("x", 72.0), args.value("y", 72.0), args.value("w", 360.0), args.value("h", 64.0) };
                o.formulaExprUtf8 = expr;
                o.formulaVarNameUtf8 = ev.assignedVarUtf8;
                o.evaluatedResultUtf8 = ev.ok ? ev.formattedResultUtf8 : ev.errorUtf8;
                o.textUtf8 = ev.ok ? (expr + " = " + ev.formattedResultUtf8) : expr;
                s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(tgt, o));
                Cas::EvaluateDocumentMathDag(s->doc);
                return McpToolResult::Ok({ { "object_id", o.id }, { "ok", ev.ok }, { "result", o.evaluatedResultUtf8 } });
            }
            if (action == "add_plot_node") {
                AnnotationObject o{};
                o.id = s->doc.AllocateObjectId();
                o.zOrder = s->doc.AllocateZOrder();
                o.layerId = s->doc.ActiveLayerId();
                o.kind = ObjectKind::PlotBlock;
                o.bounds = { args.value("x", 72.0), args.value("y", 160.0), args.value("w", 400.0), args.value("h", 240.0) };
                o.formulaExprUtf8 = args.value("expr", "sin(x)");
                o.plotConfigUtf8 = json{
                    { "expr", o.formulaExprUtf8 },
                    { "xMin", args.value("x_min", -6.28) },
                    { "xMax", args.value("x_max", 6.28) },
                    { "is3D", args.value("is_3d", false) }
                }.dump();
                o.captionUtf8 = args.value("caption", "Grafica " + o.formulaExprUtf8);
                s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(tgt, o));
                return McpToolResult::Ok({ { "object_id", o.id }, { "expr", o.formulaExprUtf8 } });
            }
            if (action == "reactive_math_eval") {
                const auto res = MathChem::ReactiveMathChemistryEngine::EvaluateReactiveEqualsOnDocument(s->doc, tgt);
                json arr = json::array();
                for (const auto& r : res) {
                    arr.push_back({ { "object_id", r.objectId }, { "var", r.variableNameUtf8 }, { "value", r.evaluatedValueUtf8 }, { "display", r.fullResolvedDisplayUtf8 } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "results", std::move(arr) } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_stem_math: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_stem_circuit",
        "Simulacion de circuitos MNA (DC, AC fasorial, Bode, transitorio, Laplace simbolico H(s), Thevenin/Norton), Mapas de Karnaugh, decodificador de resistencias, bloques de circuito en lienzo y exportacion SVG/DXF/KiCad.",
        MakeToolSchema(
            {
                "solve_dc", "solve_ac", "sweep_bode", "simulate_transient", "symbolic_laplace",
                "thevenin_norton", "decode_resistor", "kmap_simplify",
                "add_circuit_node", "export_svg", "export_dxf", "export_kicad"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "page_index", { { "type", "integer" } } },
                { "netlist", { { "type", "string" } } },
                { "freq_hz", { { "type", "number" } } },
                { "output_node", { { "type", "integer" } } },
                { "f_start_hz", { { "type", "number" } } },
                { "f_end_hz", { { "type", "number" } } },
                { "duration_sec", { { "type", "number" } } },
                { "steps", { { "type", "integer" } } },
                { "node_a", { { "type", "integer" } } },
                { "node_b", { { "type", "integer" } } },
                { "smd_code", { { "type", "string" } } },
                { "color_bands", { { "type", "array" } } },
                { "num_vars", { { "type", "integer" } } },
                { "minterms", { { "type", "array" } } },
                { "dont_cares", { { "type", "array" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");
            const std::string net = args.value("netlist", "");

            if (action == "solve_dc" || action == "solve_ac") {
                const double f = (action == "solve_ac") ? args.value("freq_hz", 60.0) : 0.0;
                const auto sol = Circuit::SolveMnaNetlist(net, f);
                json nodes = json::object();
                for (const auto& [nid, v] : sol.nodeVoltages) nodes[std::to_string(nid)] = FormatCplx(v);
                json brs = json::array();
                for (const auto& b : sol.branches) {
                    brs.push_back({ { "name", b.name }, { "current_a", FormatCplx(b.currentA) }, { "voltage_v", FormatCplx(b.voltageDropV) }, { "power_w", b.activePowerW } });
                }
                return McpToolResult::Ok({
                    { "ok", sol.ok }, { "freq_hz", f }, { "node_voltages", std::move(nodes) },
                    { "branches", std::move(brs) }, { "summary", sol.summaryUtf8 }, { "error", sol.errorUtf8 }
                });
            }
            if (action == "sweep_bode") {
                std::vector<Circuit::CircuitComponent> comps;
                if (!Circuit::ParseNetlistText(net, comps)) return McpToolResult::Err("Netlist invalido.");
                const auto pts = Circuit::ComputeBodeSweep(
                    comps, args.value("output_node", 1),
                    args.value("f_start_hz", 10.0), args.value("f_end_hz", 1e5),
                    static_cast<std::size_t>(std::clamp(args.value("steps", 31), 5, 200)));
                json arr = json::array();
                for (const auto& p : pts) arr.push_back({ { "freq_hz", p.freqHz }, { "mag_db", p.magDb }, { "phase_deg", p.phaseDeg } });
                return McpToolResult::Ok({ { "points", std::move(arr) } });
            }
            if (action == "simulate_transient") {
                const auto tr = Circuit::SimulateTransientNetlist(
                    net, args.value("duration_sec", 0.02),
                    static_cast<std::size_t>(std::clamp(args.value("steps", 120), 10, 1000)),
                    args.value("freq_hz", 60.0), args.value("output_node", 1));
                return McpToolResult::Ok({
                    { "ok", tr.ok }, { "v_max", tr.vMax }, { "v_min", tr.vMin },
                    { "v_rms", tr.vRms }, { "v_dc_avg", tr.vDcAvg }, { "summary", tr.summaryUtf8 }, { "error", tr.errorUtf8 }
                });
            }
            if (action == "symbolic_laplace") {
                const auto lap = Circuit::AnalyzeSymbolicLaplaceNetlist(net, args.value("output_node", 1));
                return McpToolResult::Ok({
                    { "ok", lap.ok },
                    { "transfer_function", lap.transferFunctionUtf8 },
                    { "transfer_function_latex", lap.transferFunctionLatex },
                    { "symbolic_h_of_s", lap.symbolicHOfSLatex },
                    { "dc_gain", lap.dcGain },
                    { "nodal_equations", lap.nodalEquationsLatex },
                    { "error", lap.errorUtf8 }
                });
            }
            if (action == "thevenin_norton") {
                std::vector<Circuit::CircuitComponent> comps;
                if (!Circuit::ParseNetlistText(net, comps)) return McpToolResult::Err("Netlist invalido.");
                const auto tn = Circuit::ComputeTheveninNorton(comps, args.value("node_a", 1), args.value("node_b", 0), args.value("freq_hz", 0.0));
                return McpToolResult::Ok({ { "ok", tn.ok }, { "v_th", tn.vTheveninV }, { "r_th", tn.rTheveninOhm }, { "i_norton", tn.iNortonA }, { "error", tn.errorUtf8 } });
            }
            if (action == "decode_resistor") {
                Circuit::ResistorCodeResult r{};
                if (args.contains("smd_code")) {
                    r = Circuit::DecodeSmdResistorCode(args["smd_code"].get<std::string>());
                } else if (args.contains("color_bands") && args["color_bands"].is_array()) {
                    std::vector<std::string> cols;
                    for (const auto& c : args["color_bands"]) if (c.is_string()) cols.push_back(c.get<std::string>());
                    r = Circuit::DecodeResistorColorNames(cols);
                }
                return McpToolResult::Ok({ { "ok", r.ok }, { "ohms", r.ohms }, { "tolerance_pct", r.tolerancePct }, { "formatted", r.formattedUtf8 }, { "error", r.errorUtf8 } });
            }
            if (action == "kmap_simplify") {
                std::vector<int> mt, dc;
                if (args.contains("minterms") && args["minterms"].is_array()) for (const auto& v : args["minterms"]) mt.push_back(v.get<int>());
                if (args.contains("dont_cares") && args["dont_cares"].is_array()) for (const auto& v : args["dont_cares"]) dc.push_back(v.get<int>());
                const auto km = Circuit::SimplifyKarnaughMap(args.value("num_vars", 4), mt, dc);
                return McpToolResult::Ok({ { "ok", km.ok }, { "minimized_sop", km.minimizedSopUtf8 }, { "prime_implicants", km.primeImplicants }, { "error", km.errorUtf8 } });
            }
            if (action == "add_circuit_node") {
                auto s = ctx.GetOrCreateSession(args);
                const ObjectTarget tgt = TargetFromArgs(args, s->doc);
                const auto sol = Circuit::SolveMnaNetlist(net, args.value("freq_hz", 0.0));
                AnnotationObject o{};
                o.id = s->doc.AllocateObjectId();
                o.zOrder = s->doc.AllocateZOrder();
                o.layerId = s->doc.ActiveLayerId();
                o.kind = ObjectKind::CircuitBlock;
                o.bounds = { args.value("x", 72.0), args.value("y", 72.0), args.value("w", 400.0), args.value("h", 220.0) };
                o.circuitNetlistUtf8 = net;
                o.evaluatedResultUtf8 = sol.ok ? sol.summaryUtf8 : sol.errorUtf8;
                o.captionUtf8 = args.value("caption", "Circuito MNA");
                s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(tgt, o));
                return McpToolResult::Ok({ { "object_id", o.id }, { "ok", sol.ok }, { "summary", o.evaluatedResultUtf8 } });
            }
            if (action == "export_svg" || action == "export_dxf" || action == "export_kicad") {
                std::vector<Circuit::CircuitComponent> comps;
                if (!Circuit::ParseNetlistText(net, comps)) return McpToolResult::Err("Netlist invalido.");
                const auto sol = Circuit::SolveMnaCircuit(comps, args.value("freq_hz", 0.0));
                if (action == "export_svg") return McpToolResult::Ok({ { "svg", Circuit::ExportCircuitToSvg(comps, sol.ok ? &sol : nullptr) } });
                if (action == "export_dxf") return McpToolResult::Ok({ { "dxf", Circuit::ExportCircuitToDxf(comps, sol.ok ? &sol : nullptr) } });
                return McpToolResult::Ok({ { "kicad_netlist", Circuit::ExportCircuitToKiCadNetlist(comps, args.value("title", "CuadernUCircuit")) } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_stem_circuit: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_casio_bridge",
        "Puente Casio de 4 modelos integrado en CuadernU (fx-570LA X, fx-570VN PLUS, fx-CG50, fx-991CW): traductor de teclas, comparador 4 modelos, tarjetas de calculo en lienzo e IPC CalcIpcChannel.",
        MakeToolSchema(
            { "translate_to_keys", "compare_4_models", "add_calc_card", "refresh_calc_card", "ipc_open", "ipc_send", "ipc_poll", "ipc_close" },
            {
                { "session_id", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "page_index", { { "type", "integer" } } },
                { "expr", { { "type", "string" } } },
                { "model", { { "type", "string" } } },
                { "object_id", { { "type", "integer" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } },
                { "channel_name", { { "type", "string" } } },
                { "key_seq", { { "type", "string" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");
            const auto env = ParseSymbolEnv(args);

            if (action == "translate_to_keys") {
                const auto plan = CalcBridge::TranslateExpressionToCasioKeys(
                    args.value("expr", ""), args.value("model", "fx-991CW"), env);
                return McpToolResult::Ok({
                    { "ok", plan.ok },
                    { "model", plan.modelIdUtf8 },
                    { "app_mode", plan.appModeUtf8 },
                    { "key_sequence", plan.keySequenceUtf8 },
                    { "evaluated", plan.evaluatedUtf8 },
                    { "latex", plan.latexUtf8 },
                    { "warnings", plan.warningsUtf8 }
                });
            }
            if (action == "compare_4_models") {
                const auto rep = CalcBridge::CompareCalculatorModels(args.value("expr", ""), env);
                json models = json::array();
                for (const auto& m : rep.models) {
                    models.push_back({
                        { "model", m.modelIdUtf8 },
                        { "supported", m.supported },
                        { "app_mode", m.appModeUtf8 },
                        { "key_sequence", m.keySequenceUtf8 },
                        { "display_result", m.displayResultUtf8 },
                        { "diff_note", m.diffNoteUtf8 }
                    });
                }
                return McpToolResult::Ok({
                    { "ok", rep.ok },
                    { "cas_reference", rep.casReferenceResultUtf8 },
                    { "all_agree", rep.allResultsAgreeWithinTol },
                    { "diff_summary", rep.diffSummaryUtf8 },
                    { "models", std::move(models) }
                });
            }
            if (action == "add_calc_card") {
                auto s = ctx.GetOrCreateSession(args);
                const ObjectTarget tgt = TargetFromArgs(args, s->doc);
                const auto plan = CalcBridge::TranslateExpressionToCasioKeys(
                    args.value("expr", "sin(30)+2"), args.value("model", "fx-991CW"), env);
                AnnotationObject o{};
                o.id = s->doc.AllocateObjectId();
                o.zOrder = s->doc.AllocateZOrder();
                o.layerId = s->doc.ActiveLayerId();
                o.kind = ObjectKind::CalcSessionCard;
                o.bounds = { args.value("x", 72.0), args.value("y", 72.0), 340.0, 96.0 };
                o.calcModelIdUtf8 = plan.modelIdUtf8;
                o.calcKeySeqUtf8 = plan.keySequenceUtf8;
                o.calcLatexUtf8 = plan.latexUtf8;
                o.formulaExprUtf8 = args.value("expr", "");
                o.evaluatedResultUtf8 = plan.evaluatedUtf8;
                s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(tgt, o));
                return McpToolResult::Ok({ { "object_id", o.id }, { "model", o.calcModelIdUtf8 }, { "key_sequence", o.calcKeySeqUtf8 }, { "result", o.evaluatedResultUtf8 } });
            }
            if (action == "refresh_calc_card") {
                auto s = ctx.GetOrCreateSession(args);
                const ObjectTarget tgt = TargetFromArgs(args, s->doc);
                const std::uint64_t oid = args.value("object_id", std::uint64_t{ 0 });
                AnnotationObject* o = tgt.isCanvas ? s->doc.FindCanvasObjectMutable(oid)
                    : (tgt.pageIndex < s->doc.Pages().size() ? s->doc.FindPageObjectMutable(tgt.pageIndex, oid) : nullptr);
                if (!o) return McpToolResult::Err("Tarjeta de calculo no encontrada.");
                const bool ok = CalcBridge::RefreshCalcSessionCard(*o, env);
                return McpToolResult::Ok({ { "refreshed", ok }, { "result", o->evaluatedResultUtf8 } });
            }
            if (action == "ipc_open") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                CalcBridge::CalcIpcConfig cfg{};
                cfg.channelNameUtf8 = args.value("channel_name", "default");
                cfg.role = CalcBridge::CalcIpcRole::DocumentStudio;
                std::string err;
                const bool ok = ctx.calcIpc.Open(cfg, &err);
                return ok ? McpToolResult::Ok({ { "open", true }, { "directory", ctx.calcIpc.ChannelDirectoryUtf8() } })
                          : McpToolResult::Err(err);
            }
            if (action == "ipc_send") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                CalcBridge::CalcIpcMessage msg{};
                msg.sender = CalcBridge::CalcIpcRole::DocumentStudio;
                msg.modelIdUtf8 = args.value("model", "fx-991CW");
                msg.expressionUtf8 = args.value("expr", "");
                msg.keySeqUtf8 = args.value("key_seq", "");
                std::string err;
                const bool ok = ctx.calcIpc.Send(msg, &err);
                return ok ? McpToolResult::Ok({ { "sent", true } }) : McpToolResult::Err(err);
            }
            if (action == "ipc_poll") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                std::string err;
                const auto msgs = ctx.calcIpc.Poll(32, &err);
                json arr = json::array();
                for (const auto& m : msgs) {
                    arr.push_back({ { "seq", m.seq }, { "model", m.modelIdUtf8 }, { "expr", m.expressionUtf8 }, { "result", m.resultUtf8 }, { "key_seq", m.keySeqUtf8 } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "messages", std::move(arr) } });
            }
            if (action == "ipc_close") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                ctx.calcIpc.Close();
                return McpToolResult::Ok({ { "open", false } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_casio_bridge: " + action);
        }
    });
}

} // namespace DocStudio::Mcp
