#include "mcp_server.hpp"

#include "../core/cudoc_crypto.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/image_processing.hpp"
#include "../core/ink_engine.hpp"
#include "../core/inline_url_parser.hpp"
#include "../core/utf_conv.hpp"
#include "../media/webview2_html_host.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio::Mcp {

namespace {

std::uint64_t NowEpochSec() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string LowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<InkPoint> ParseInkPointsJson(const json& arr) {
    std::vector<InkPoint> pts;
    if (!arr.is_array()) return pts;
    pts.reserve(arr.size());
    std::uint64_t fallbackMs = 0;
    for (const auto& it : arr) {
        InkPoint p{};
        if (it.is_array() && it.size() >= 2) {
            p.x = it[0].get<double>();
            p.y = it[1].get<double>();
            if (it.size() >= 3) p.pressure = it[2].get<float>();
            if (it.size() >= 4) p.timestampMs = it[3].get<std::uint64_t>();
            else p.timestampMs = fallbackMs;
        } else if (it.is_object()) {
            p.x = it.value("x", 0.0);
            p.y = it.value("y", 0.0);
            p.pressure = it.value("pressure", 0.5f);
            p.tiltXDeg = it.value("tilt_x", 0.0f);
            p.tiltYDeg = it.value("tilt_y", 0.0f);
            p.timestampMs = it.value("t_ms", fallbackMs);
        } else {
            continue;
        }
        fallbackMs += 16u;
        pts.push_back(p);
    }
    return pts;
}

DocRect ParseRectJson(const json& j, DocRect fallback = { 72.0, 72.0, 260.0, 120.0 }) {
    if (!j.is_object()) return fallback;
    return {
        j.value("x", fallback.x),
        j.value("y", fallback.y),
        j.value("w", fallback.w),
        j.value("h", fallback.h)
    };
}

AnnotationObject* FindObjectMutable(DocumentModel& doc, ObjectTarget target, std::uint64_t id) {
    if (target.isCanvas) return doc.FindCanvasObjectMutable(id);
    if (target.pageIndex < doc.Pages().size()) return doc.FindPageObjectMutable(target.pageIndex, id);
    return nullptr;
}

const std::vector<AnnotationObject>* GetObjectListConst(const DocumentModel& doc, ObjectTarget target) {
    if (target.isCanvas) return &doc.CanvasObjects();
    if (target.pageIndex < doc.Pages().size()) return &doc.Pages()[target.pageIndex].objects;
    return nullptr;
}

std::vector<AnnotationObject> BuildEngineeringStickerObjects(
    DocumentModel& doc,
    const std::string& kindRaw,
    double ox,
    double oy,
    double scale)
{
    const std::string k = LowerAscii(kindRaw);
    std::vector<AnnotationObject> out;
    auto mkLine = [&](double x0, double y0, double x1, double y1, ShapeKind sk = ShapeKind::Line, DocColor col = DocColor::RGBA(24, 30, 42, 255)) {
        AnnotationObject o{};
        o.id = doc.AllocateObjectId();
        o.zOrder = doc.AllocateZOrder();
        o.layerId = doc.ActiveLayerId();
        o.kind = ObjectKind::Shape;
        o.shapeKind = sk;
        o.shapeStart = { ox + x0 * scale, oy + y0 * scale };
        o.shapeEnd = { ox + x1 * scale, oy + y1 * scale };
        o.strokeColor = col;
        o.strokeWidth = 2.0 * scale;
        o.RecomputeBounds();
        out.push_back(std::move(o));
    };
    auto mkTxt = [&](double x, double y, double w, double h, const std::string& txt, double fontPt = 11.0) {
        AnnotationObject o{};
        o.id = doc.AllocateObjectId();
        o.zOrder = doc.AllocateZOrder();
        o.layerId = doc.ActiveLayerId();
        o.kind = ObjectKind::TextBox;
        o.bounds = { ox + x * scale, oy + y * scale, w * scale, h * scale };
        o.textUtf8 = txt;
        o.fontSizePt = fontPt * scale;
        o.strokeColor = DocColor::RGBA(24, 30, 42, 255);
        out.push_back(std::move(o));
    };

    if (k == "opamp" || k == "op_amp") {
        mkLine(20, 10, 20, 90);
        mkLine(20, 10, 100, 50);
        mkLine(20, 90, 100, 50);
        mkLine(0, 30, 20, 30);
        mkLine(0, 70, 20, 70);
        mkLine(100, 50, 125, 50);
        mkTxt(24, 20, 20, 18, "+");
        mkTxt(24, 58, 20, 18, "-");
    } else if (k == "resistor") {
        mkLine(0, 30, 20, 30);
        mkLine(20, 30, 30, 14);
        mkLine(30, 14, 42, 46);
        mkLine(42, 46, 54, 14);
        mkLine(54, 14, 66, 46);
        mkLine(66, 46, 78, 14);
        mkLine(78, 14, 88, 30);
        mkLine(88, 30, 110, 30);
        mkTxt(38, 48, 48, 20, "R");
    } else if (k == "axes" || k == "ejes") {
        mkLine(10, 110, 170, 110, ShapeKind::Arrow, DocColor::RGBA(37, 99, 235, 255));
        mkLine(30, 130, 30, 10, ShapeKind::Arrow, DocColor::RGBA(37, 99, 235, 255));
        mkTxt(162, 112, 24, 18, "x");
        mkTxt(12, 6, 24, 18, "y");
    } else {
        AnnotationObject box{};
        box.id = doc.AllocateObjectId();
        box.zOrder = doc.AllocateZOrder();
        box.layerId = doc.ActiveLayerId();
        box.kind = ObjectKind::TextBox;
        box.bounds = { ox, oy, 180.0 * scale, 96.0 * scale };
        box.fillColor = DocColor::RGBA(254, 249, 195, 248);
        box.strokeColor = DocColor::RGBA(202, 138, 4, 255);
        box.strokeWidth = 1.5;
        box.textUtf8 = "Nota";
        box.fontSizePt = 13.0 * scale;
        out.push_back(std::move(box));
    }
    return out;
}

json SerializeSessionSnapshot(const SessionEntry& s) {
    json pagesArr = json::array();
    for (std::size_t i = 0; i < s.doc.Pages().size(); ++i) {
        const auto& pg = s.doc.Pages()[i];
        pagesArr.push_back({
            { "index", i },
            { "page_id", pg.pageId },
            { "width_pt", pg.widthPt },
            { "height_pt", pg.heightPt },
            { "effective_width_pt", pg.EffectiveWidthPt() },
            { "effective_height_pt", pg.EffectiveHeightPt() },
            { "rotation_deg", pg.rotationDeg },
            { "background", PaperPatternToString(pg.background) },
            { "has_pdf_source", pg.hasPdfSource },
            { "object_count", pg.objects.size() },
            { "has_text", !pg.extractedTextUtf8.empty() || !pg.ocrTextUtf8.empty() }
        });
    }
    json layersArr = json::array();
    for (const auto& ly : s.doc.Layers()) {
        layersArr.push_back({
            { "id", ly.id },
            { "name", ly.nameUtf8 },
            { "visible", ly.visible },
            { "locked", ly.locked },
            { "include_in_export", ly.includeInExport },
            { "opacity", ly.opacity }
        });
    }
    return {
        { "session_id", s.sessionId },
        { "title", s.doc.Title() },
        { "uuid", s.doc.DocumentUuid() },
        { "mode", s.doc.Mode() == DocumentMode::InfiniteCanvas ? "infinite_canvas" : "paginated_pdf" },
        { "canvas_background", PaperPatternToString(s.doc.CanvasBackground()) },
        { "file_path", WideToUtf8(s.filePathW) },
        { "revision", s.doc.Revision() },
        { "dirty", s.doc.IsDirty() },
        { "double_page_spread", s.doc.DoublePageSpread() },
        { "active_page_index", s.activePageIndex },
        { "active_layer_id", s.doc.ActiveLayerId() },
        { "can_undo", s.cmdStack.CanUndo() },
        { "can_redo", s.cmdStack.CanRedo() },
        { "undo_count", s.cmdStack.UndoCount() },
        { "redo_count", s.cmdStack.RedoCount() },
        { "canvas_object_count", s.doc.CanvasObjects().size() },
        { "page_count", s.doc.Pages().size() },
        { "pages", std::move(pagesArr) },
        { "layers", std::move(layersArr) },
        { "bookmark_count", s.doc.Bookmarks().size() },
        { "attachment_count", s.doc.Attachments().size() },
        { "ink_link_count", s.doc.InkLinks().size() },
        { "exam_mode_locked", s.examModeLocked }
    };
}

} // namespace

void RegisterDocumentAndCanvasTools(McpProtocolRouter& router, McpServerContext& ctx) {
    // ---------------- Group A: Meta & Legacy Gemini Notebook Compatibility (6 tools) ----------------

    router.RegisterTool({
        "cuadernu_capabilities",
        "Descubre todas las herramientas, dominios, sesiones activas, instancias GUI, comandos de paleta y capacidades del entorno de CuadernU.",
        MakeToolSchema(
            {},
            {
                { "command_query", { { "type", "string" }, { "description", "Filtro opcional para buscar en la paleta de comandos." } } },
                { "include_tools", { { "type", "boolean" }, { "description", "Incluir resumen de las herramientas MCP registradas." } } }
            },
            {}),
        [&router, &ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            std::string wv2Ver;
            const bool hasWv2 = WebView2HtmlHost::IsWebView2RuntimeInstalled(&wv2Ver);
            const auto ocrLangs = WinRtOcrAdapter::ListAvailableLanguageTags();
            const auto tutorCfg = Tutor::TutorSidecarBridge::DiscoverDefaultConfig();
            const bool hasPythonTutor = !tutorCfg.pythonExePath.empty() && !tutorCfg.serverScriptPath.empty();

            json out = {
                { "server_name", router.ServerName() },
                { "server_version", router.ServerVersion() },
                { "tool_count", router.Tools().size() },
                { "resource_count", router.Resources().size() },
                { "resource_template_count", router.ResourceTemplates().size() },
                { "prompt_count", router.Prompts().size() },
                { "profile_root", WideToUtf8(ctx.EffectiveProfileRootW()) },
                { "artifacts_dir", WideToUtf8(ctx.EffectiveArtifactsDirW()) },
                { "sandbox_mode", ctx.Options().sandboxMode },
                { "readonly_tec", ctx.Options().readonlyTec },
                { "no_profile_write", ctx.Options().noProfileWrite },
                { "tec_mutation_policy_allowed", ctx.IsTecMutationPolicyAllowed() },
                { "active_session_id", ctx.ActiveSessionId() },
                { "sessions", ctx.ListSessionIds() },
                { "gui_instances", [&]() {
                    json arr = json::array();
                    for (const auto& inst : ctx.guiManager.ListInstances(ctx.EffectiveProfileRootW())) {
                        arr.push_back({
                            { "pid", inst.pid },
                            { "instance_id", inst.instanceId },
                            { "mode", inst.mode },
                            { "managed", inst.managedByThisServer },
                            { "alive", inst.alive }
                        });
                    }
                    return arr;
                }() },
                { "environment", {
                    { "webview2_available", hasWv2 },
                    { "webview2_version", wv2Ver },
                    { "winrt_ocr_languages", ocrLangs },
                    { "tutor_python_available", hasPythonTutor },
                    { "has_gemini_notebook_module",
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
                      true
#else
                      false
#endif
                    },
                    { "has_tec_academic_module",
#ifdef DOC_STUDIO_HAS_ACADEMIC
                      true
#else
                      false
#endif
                    }
                } },
                { "command_palette_total", ctx.commandPalette.All().size() }
            };

            if (args.contains("command_query") && args["command_query"].is_string()) {
                const auto hits = ctx.commandPalette.Search(args["command_query"].get<std::string>(), 40);
                json cmds = json::array();
                for (const auto& c : hits) {
                    cmds.push_back({
                        { "command_id", c.commandId },
                        { "slash", c.slashTriggerUtf8 },
                        { "title", c.titleUtf8 },
                        { "category", c.categoryUtf8 },
                        { "shortcut", c.shortcutUtf8 }
                    });
                }
                out["command_matches"] = std::move(cmds);
            }
            if (args.value("include_tools", true)) {
                json tArr = json::array();
                for (const auto& t : router.Tools()) {
                    tArr.push_back({ { "name", t.name }, { "description", t.description } });
                }
                out["tools"] = std::move(tArr);
            }
            return McpToolResult::Ok(std::move(out));
        }
    });

    // Legacy 5 Gemini Notebook MCP Bridge Tools (100% backward-compatible with CuadernUMcpBridge)
    auto registerLegacyGnTool = [&](const std::string& name, const std::string& desc, json schema) {
        router.RegisterTool({
            name,
            desc,
            std::move(schema),
            [&ctx, name](const json& args, const McpRequestContext&) -> McpToolResult {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                auto& gnStore = ctx.EnsureGeminiStore();
                const json rpc = {
                    { "jsonrpc", "2.0" },
                    { "id", 1 },
                    { "method", "tools/call" },
                    { "params", { { "name", name }, { "arguments", args } } }
                };
                const std::string raw = GeminiNotebook::CuadernUMcpBridge::HandleJsonRpcRequestLine(
                    rpc.dump(), gnStore, ctx.geminiTransportOverride);
                try {
                    const json resp = json::parse(raw);
                    if (resp.contains("error")) {
                        return McpToolResult::Err(resp["error"].value("message", "Error en puente Gemini Notebook"));
                    }
                    if (resp.contains("result") && resp["result"].contains("content") &&
                        resp["result"]["content"].is_array() && !resp["result"]["content"].empty()) {
                        const std::string txt = resp["result"]["content"][0].value("text", "{}");
                        const bool isErr = resp["result"].value("isError", false);
                        try {
                            json parsed = json::parse(txt);
                            return isErr ? McpToolResult::Err(txt, parsed) : McpToolResult::Ok(parsed);
                        } catch (...) {
                            return isErr ? McpToolResult::Err(txt) : McpToolResult::Ok({ { "text", txt } });
                        }
                    }
                    return McpToolResult::Ok(resp.value("result", json::object()));
                } catch (const std::exception& ex) {
                    return McpToolResult::Err(std::string("Respuesta invalida de puente Gemini Notebook: ") + ex.what());
                }
#else
                (void)ctx;
                (void)args;
                return McpToolResult::Err("Modulo Gemini Notebook deshabilitado en esta compilacion.");
#endif
            }
        });
    };

    registerLegacyGnTool(
        "cuadernu_notebook_status",
        "Estado del modulo Google Gemini Notebook / NotebookLM y autenticacion local de CuadernU.",
        MakeToolSchema({}, {}, {}));
    registerLegacyGnTool(
        "cuadernu_list_sync_inventory",
        "Lista los cuadernos mapeados, fuentes sincronizadas e historial de sincronizacion con Gemini Notebook.",
        MakeToolSchema({}, { { "notebook_key", { { "type", "string" } } } }, {}));
    registerLegacyGnTool(
        "cuadernu_plan_tec_course_sync",
        "Clasifica y planifica la sincronizacion de materiales de un curso de TEC Digital hacia Gemini Notebook.",
        MakeToolSchema({}, {
            { "course_code", { { "type", "string" } } },
            { "course_name", { { "type", "string" } } },
            { "materials", { { "type", "array" } } }
        }, { "course_code" }));
    registerLegacyGnTool(
        "cuadernu_sync_sources",
        "Ejecuta la sincronizacion de fuentes locales, URLs o YouTube hacia un cuaderno en Google Gemini Notebook.",
        MakeToolSchema({}, {
            { "notebook_key", { { "type", "string" } } },
            { "notebook_title", { { "type", "string" } } },
            { "remote_notebook_name", { { "type", "string" } } },
            { "sources", { { "type", "array" } } }
        }, { "notebook_key" }));
    registerLegacyGnTool(
        "cuadernu_export_cudoc_to_markdown",
        "Exporta un archivo .cudoc de CuadernU a Markdown limpio optimizado para indexacion en NotebookLM / Gemini.",
        MakeToolSchema({}, {
            { "cudoc_path", { { "type", "string" } } },
            { "output_md_path", { { "type", "string" } } }
        }, { "cudoc_path" }));

    // ---------------- Group B: Sessions, Documents, Pages, Layers, Attachments (4 tools) ----------------

    router.RegisterTool({
        "cuadernu_session",
        "Gestiona sesiones multi-documento .cudoc (crear, abrir, abrir/guardar cifrado, guardar, undo/redo, historial persistente, borradores, versiones, merge de conflictos y CSS personalizado).",
        MakeToolSchema(
            {
                "new", "open", "open_encrypted", "save", "save_encrypted", "close", "list", "switch",
                "snapshot", "set_title", "set_mode", "undo", "redo", "history_save", "history_load",
                "drafts_list", "draft_recover", "version_capture", "version_list", "version_restore",
                "conflict_merge", "custom_css_get", "custom_css_set"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "mode", { { "type", "string" }, { "enum", json::array({ "paginated_pdf", "infinite_canvas" }) } } },
                { "background", { { "type", "string" } } },
                { "path", { { "type", "string" } } },
                { "passphrase", { { "type", "string" } } },
                { "page_index", { { "type", "integer" } } },
                { "label", { { "type", "string" } } },
                { "version_id", { { "type", "integer" } } },
                { "base_path", { { "type", "string" } } },
                { "remote_path", { { "type", "string" } } },
                { "css", { { "type", "string" } } }
            }),
        [&ctx, &router](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");
            if (args.contains("passphrase") && args["passphrase"].is_string()) {
                router.Redactor().RegisterSecret(args["passphrase"].get<std::string>());
            }
            if (action == "list") {
                json arr = json::array();
                for (const auto& id : ctx.ListSessionIds()) {
                    if (auto s = ctx.FindSession(id)) {
                        arr.push_back({
                            { "session_id", s->sessionId },
                            { "title", s->doc.Title() },
                            { "mode", s->doc.Mode() == DocumentMode::InfiniteCanvas ? "infinite_canvas" : "paginated_pdf" },
                            { "file_path", WideToUtf8(s->filePathW) },
                            { "active", id == ctx.ActiveSessionId() }
                        });
                    }
                }
                return McpToolResult::Ok({ { "active_session_id", ctx.ActiveSessionId() }, { "sessions", std::move(arr) } });
            }
            if (action == "new") {
                const std::string title = args.value("title", "Sin titulo");
                const std::string modeStr = LowerAscii(args.value("mode", "paginated_pdf"));
                const DocumentMode dm = (modeStr == "infinite_canvas" || modeStr == "canvas")
                    ? DocumentMode::InfiniteCanvas : DocumentMode::PaginatedPdf;
                const PaperPattern bg = ParsePaperPattern(args.value("background", "grid"), PaperPattern::Grid);
                auto s = ctx.CreateSession(title, dm, bg, args.value("session_id", ""));
                return McpToolResult::Ok(SerializeSessionSnapshot(*s));
            }
            if (action == "switch") {
                const std::string sid = args.value("session_id", "");
                if (!ctx.SwitchActiveSession(sid)) return McpToolResult::Err("Sesion no encontrada: " + sid);
                auto s = ctx.FindSession(sid);
                return McpToolResult::Ok(SerializeSessionSnapshot(*s));
            }
            if (action == "close") {
                const std::string sid = args.value("session_id", ctx.ActiveSessionId());
                if (!ctx.CloseSession(sid)) return McpToolResult::Err("No se pudo cerrar sesion: " + sid);
                return McpToolResult::Ok({ { "closed", sid }, { "active_session_id", ctx.ActiveSessionId() } });
            }
            if (action == "open" || action == "open_encrypted") {
                const std::string pathUtf8 = args.value("path", "");
                if (pathUtf8.empty()) return McpToolResult::Err("Se requiere el parametro 'path'.");
                auto s = ctx.CreateSession("Cargando", DocumentMode::PaginatedPdf, PaperPattern::Plain, args.value("session_id", ""));
                std::string err;
                bool ok = false;
                if (action == "open_encrypted") {
                    const std::string pass = args.value("passphrase", "");
                    ok = CudocCrypto::LoadEncryptedCudocFile(pathUtf8, pass, s->doc, &err);
                } else {
                    bool usedBak = false;
                    ok = CudocStorage::LoadFromFileWithRecovery(Utf8ToWide(pathUtf8), s->doc, &usedBak, &err);
                }
                if (!ok) {
                    ctx.CloseSession(s->sessionId);
                    return McpToolResult::Err("Error al abrir .cudoc: " + err);
                }
                s->filePathW = Utf8ToWide(pathUtf8);
                ctx.UnpackExtensions(*s);
                s->cmdStack.Clear();
                s->cmdStack.MarkClean(s->doc);
                return McpToolResult::Ok(SerializeSessionSnapshot(*s));
            }
            if (action == "drafts_list") {
                const fs::path draftsDir = fs::path(ctx.EffectiveProfileRootW()) / L"drafts";
                json arr = json::array();
                std::error_code ec;
                if (fs::exists(draftsDir, ec)) {
                    for (const auto& entry : fs::directory_iterator(draftsDir, ec)) {
                        if (entry.is_regular_file(ec)) {
                            arr.push_back({
                                { "name", WideToUtf8(entry.path().filename().wstring()) },
                                { "path", WideToUtf8(entry.path().wstring()) },
                                { "size_bytes", entry.file_size(ec) }
                            });
                        }
                    }
                }
                return McpToolResult::Ok({ { "drafts_dir", WideToUtf8(draftsDir.wstring()) }, { "drafts", std::move(arr) } });
            }
            if (action == "draft_recover") {
                const std::string pathUtf8 = args.value("path", "");
                if (pathUtf8.empty()) return McpToolResult::Err("Se requiere 'path' del borrador.");
                auto s = ctx.CreateSession("Borrador Recuperado", DocumentMode::PaginatedPdf);
                std::string err;
                std::size_t replayed = 0;
                const std::wstring wPath = Utf8ToWide(pathUtf8);
                const std::wstring walPath = wPath + L"-wal";
                const bool ok = fs::exists(walPath)
                    ? CudocWal::RecoverDocument(wPath, walPath, s->doc, &replayed, &err)
                    : CudocStorage::LoadFromFileWithRecovery(wPath, s->doc, nullptr, &err);
                if (!ok) {
                    ctx.CloseSession(s->sessionId);
                    return McpToolResult::Err("No se pudo recuperar borrador: " + err);
                }
                ctx.UnpackExtensions(*s);
                s->cmdStack.MarkCleanUnreachable(s->doc);
                json res = SerializeSessionSnapshot(*s);
                res["replayed_wal_records"] = replayed;
                return McpToolResult::Ok(std::move(res));
            }

            auto s = ctx.GetOrCreateSession(args);
            if (!s) return McpToolResult::Err("No se pudo obtener la sesion.");

            if (action == "snapshot") {
                return McpToolResult::Ok(SerializeSessionSnapshot(*s));
            }
            if (action == "set_title") {
                s->doc.SetTitle(args.value("title", s->doc.Title()));
                s->doc.SetDirty(true);
                return McpToolResult::Ok(SerializeSessionSnapshot(*s));
            }
            if (action == "set_mode") {
                const std::string m = LowerAscii(args.value("mode", "paginated_pdf"));
                s->doc.SetMode((m == "infinite_canvas" || m == "canvas") ? DocumentMode::InfiniteCanvas : DocumentMode::PaginatedPdf);
                if (s->doc.Mode() == DocumentMode::PaginatedPdf && s->doc.Pages().empty()) {
                    PageModel pg{};
                    pg.pageId = s->doc.AllocatePageId();
                    pg.background = s->doc.CanvasBackground();
                    s->doc.MutablePages().push_back(std::move(pg));
                    s->doc.RebuildLayout();
                }
                s->doc.SetDirty(true);
                return McpToolResult::Ok(SerializeSessionSnapshot(*s));
            }
            if (action == "save" || action == "save_encrypted") {
                const std::wstring dstW = args.contains("path") && !args["path"].get<std::string>().empty()
                    ? Utf8ToWide(args["path"].get<std::string>())
                    : (!s->filePathW.empty() ? s->filePathW : ctx.ResolveArtifactPathW("", s->sessionId, ".cudoc"));
                ctx.PackExtensions(*s);
                std::string err;
                bool ok = false;
                if (action == "save_encrypted") {
                    const std::string pass = args.value("passphrase", "");
                    ok = CudocCrypto::SaveEncryptedCudocFile(WideToUtf8(dstW), s->doc, pass, &err);
                } else {
                    ok = CudocStorage::SaveToFile(dstW, s->doc, &err);
                }
                if (!ok) return McpToolResult::Err("Error al guardar .cudoc: " + err);
                s->filePathW = dstW;
                s->cmdStack.MarkClean(s->doc);
                return McpToolResult::Ok({ { "saved", true }, { "path", WideToUtf8(dstW) }, { "session_id", s->sessionId } });
            }
            if (action == "undo") {
                const bool ok = s->cmdStack.Undo(s->doc);
                return McpToolResult::Ok({ { "undone", ok }, { "can_undo", s->cmdStack.CanUndo() }, { "can_redo", s->cmdStack.CanRedo() }, { "revision", s->doc.Revision() } });
            }
            if (action == "redo") {
                const bool ok = s->cmdStack.Redo(s->doc);
                return McpToolResult::Ok({ { "redone", ok }, { "can_undo", s->cmdStack.CanUndo() }, { "can_redo", s->cmdStack.CanRedo() }, { "revision", s->doc.Revision() } });
            }
            if (action == "history_save") {
                const std::wstring hp = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_history", ".cuhist");
                const bool ok = s->cmdStack.SaveSessionHistoryToFile(hp, s->doc);
                return ok ? McpToolResult::Ok({ { "saved", true }, { "path", WideToUtf8(hp) } })
                          : McpToolResult::Err("Error al guardar historial de sesion.");
            }
            if (action == "history_load") {
                const std::string p = args.value("path", "");
                if (p.empty()) return McpToolResult::Err("Se requiere 'path' del archivo de historial.");
                const bool ok = s->cmdStack.LoadSessionHistoryFromFile(Utf8ToWide(p), s->doc);
                return ok ? McpToolResult::Ok(SerializeSessionSnapshot(*s))
                          : McpToolResult::Err("No se pudo cargar el historial de sesion.");
            }
            if (action == "version_capture") {
                const std::size_t pIdx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                const std::uint64_t vid = s->versionHistory.CaptureSnapshot(s->doc, pIdx, NowEpochSec(), args.value("label", "MCP Snapshot"));
                return vid != 0
                    ? McpToolResult::Ok({ { "version_id", vid }, { "page_index", pIdx } })
                    : McpToolResult::Err("Indice de pagina invalido para version_capture.");
            }
            if (action == "version_list") {
                const std::size_t pIdx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                json arr = json::array();
                for (const auto& v : s->versionHistory.SnapshotsForPage(pIdx)) {
                    arr.push_back({
                        { "version_id", v.versionId },
                        { "page_index", v.pageIndex },
                        { "timestamp_sec", v.timestampSec },
                        { "label", v.authorOrLabelUtf8 },
                        { "object_count", v.snapshotPage.objects.size() }
                    });
                }
                return McpToolResult::Ok({ { "versions", std::move(arr) } });
            }
            if (action == "version_restore") {
                const std::uint64_t vid = args.value("version_id", std::uint64_t{ 0 });
                const bool ok = s->versionHistory.RestoreSnapshot(s->doc, vid);
                return ok ? McpToolResult::Ok(SerializeSessionSnapshot(*s))
                          : McpToolResult::Err("Version no encontrada: " + std::to_string(vid));
            }
            if (action == "conflict_merge") {
                const std::string basePath = args.value("base_path", "");
                const std::string remotePath = args.value("remote_path", "");
                if (remotePath.empty()) return McpToolResult::Err("Se requiere 'remote_path'.");
                DocumentModel baseDoc, remoteDoc, mergedDoc;
                std::string err;
                if (!basePath.empty()) {
                    if (!CudocStorage::LoadFromFile(Utf8ToWide(basePath), baseDoc, &err)) {
                        return McpToolResult::Err("Error al abrir base_path: " + err);
                    }
                } else {
                    baseDoc.ResetToBlank(s->doc.Mode());
                }
                if (!CudocStorage::LoadFromFile(Utf8ToWide(remotePath), remoteDoc, &err)) {
                    return McpToolResult::Err("Error al abrir remote_path: " + err);
                }
                const auto rep = MergeConcurrentDocuments(baseDoc, s->doc, remoteDoc, mergedDoc);
                s->doc = std::move(mergedDoc);
                s->doc.SetDirty(true);
                return McpToolResult::Ok({
                    { "ok", rep.ok },
                    { "merged_clean_pages", rep.mergedCleanPages },
                    { "conflict_pages_created", rep.conflictPagesCreated },
                    { "conflicts", rep.conflictDescriptions }
                });
            }
            if (action == "custom_css_get") {
                return McpToolResult::Ok({ { "css", s->customCssUtf8 } });
            }
            if (action == "custom_css_set") {
                s->customCssUtf8 = args.value("css", "");
                s->doc.SetExtensionPayload("custom_css.txt", s->customCssUtf8);
                s->doc.SetDirty(true);
                return McpToolResult::Ok({ { "updated", true }, { "length", s->customCssUtf8.size() } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_session: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_page",
        "Administra paginas del documento: listar, agregar, insertar, eliminar, duplicar, reordenar, rotar, recortar, cambiar tamano (A4/Carta/16:9/4:3/custom), fondo de papel (11 patrones), vista doble, rangos colapsados, marcadores e indice.",
        MakeToolSchema(
            {
                "list", "add", "insert", "delete", "duplicate", "reorder", "rotate", "crop",
                "set_size", "set_background", "toggle_spread", "collapse_range",
                "bookmark_list", "bookmark_add", "bookmark_rename", "bookmark_remove", "outline_extract"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "page_index", { { "type", "integer" } } },
                { "from_index", { { "type", "integer" } } },
                { "to_index", { { "type", "integer" } } },
                { "delta_deg", { { "type", "integer" } } },
                { "preset", { { "type", "string" }, { "description", "a4, letter, slide_16_9, slide_4_3" } } },
                { "width_pt", { { "type", "number" } } },
                { "height_pt", { { "type", "number" } } },
                { "background", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "crop", { { "type", "object" } } },
                { "enabled", { { "type", "boolean" } } },
                { "start_page", { { "type", "integer" } } },
                { "end_page", { { "type", "integer" } } },
                { "title", { { "type", "string" } } },
                { "world_y", { { "type", "number" } } },
                { "level", { { "type", "integer" } } },
                { "bookmark_id", { { "type", "integer" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const std::string action = args.value("action", "");

            auto applyPresetDims = [&](PageModel& pg) {
                const std::string preset = LowerAscii(args.value("preset", ""));
                if (preset == "letter" || preset == "carta") { pg.widthPt = 612.0; pg.heightPt = 792.0; }
                else if (preset == "slide_16_9" || preset == "16:9") { pg.widthPt = 960.0; pg.heightPt = 540.0; }
                else if (preset == "slide_4_3" || preset == "4:3") { pg.widthPt = 720.0; pg.heightPt = 540.0; }
                else if (preset == "a4") { pg.widthPt = 595.0; pg.heightPt = 842.0; }
                if (args.contains("width_pt")) pg.widthPt = std::clamp(args["width_pt"].get<double>(), 100.0, 4000.0);
                if (args.contains("height_pt")) pg.heightPt = std::clamp(args["height_pt"].get<double>(), 100.0, 4000.0);
            };

            if (action == "list") {
                return McpToolResult::Ok({ { "pages", SerializeSessionSnapshot(*s)["pages"] } });
            }
            if (action == "add" || action == "insert") {
                PageModel pg{};
                pg.pageId = s->doc.AllocatePageId();
                pg.background = ParsePaperPattern(args.value("background", "plain"), PaperPattern::Plain);
                applyPresetDims(pg);
                const std::size_t at = (action == "insert")
                    ? (std::min)(static_cast<std::size_t>(std::max(0, args.value("page_index", 0))), s->doc.Pages().size())
                    : s->doc.Pages().size();
                s->cmdStack.Execute(s->doc, std::make_unique<InsertPageCommand>(at, pg));
                s->activePageIndex = at;
                return McpToolResult::Ok({ { "inserted_index", at }, { "page_count", s->doc.Pages().size() }, { "page_id", pg.pageId } });
            }
            if (action == "delete") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                if (idx >= s->doc.Pages().size() || s->doc.Pages().size() <= 1) {
                    return McpToolResult::Err("Indice invalido o no se puede eliminar la unica pagina.");
                }
                s->cmdStack.Execute(s->doc, std::make_unique<DeletePageCommand>(idx));
                if (s->activePageIndex >= s->doc.Pages().size()) s->activePageIndex = s->doc.Pages().size() - 1;
                return McpToolResult::Ok({ { "deleted_index", idx }, { "page_count", s->doc.Pages().size() } });
            }
            if (action == "duplicate") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                if (!FlexcilStudyOverlay::DuplicatePage(s->doc, idx)) {
                    return McpToolResult::Err("No se pudo duplicar la pagina.");
                }
                return McpToolResult::Ok({ { "duplicated_from", idx }, { "page_count", s->doc.Pages().size() } });
            }
            if (action == "reorder") {
                const std::size_t fromIdx = static_cast<std::size_t>(std::max(0, args.value("from_index", 0)));
                const std::size_t toIdx = static_cast<std::size_t>(std::max(0, args.value("to_index", 0)));
                if (!s->cmdStack.Execute(s->doc, std::make_unique<ReorderPageCommand>(fromIdx, toIdx))) {
                    return McpToolResult::Err("Indices fuera de rango para reorder.");
                }
                return McpToolResult::Ok({ { "reordered", true }, { "from", fromIdx }, { "to", toIdx } });
            }
            if (action == "rotate") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                const int delta = args.value("delta_deg", 90);
                if (!s->cmdStack.Execute(s->doc, std::make_unique<RotatePageCommand>(idx, delta))) {
                    return McpToolResult::Err("Indice fuera de rango para rotate.");
                }
                return McpToolResult::Ok({ { "page_index", idx }, { "rotation_deg", s->doc.Pages()[idx].rotationDeg } });
            }
            if (action == "crop") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                NormalizedCropRect rc{};
                if (args.contains("crop") && args["crop"].is_object()) {
                    rc.left = args["crop"].value("left", 0.0);
                    rc.top = args["crop"].value("top", 0.0);
                    rc.right = args["crop"].value("right", 1.0);
                    rc.bottom = args["crop"].value("bottom", 1.0);
                    rc.Clamp();
                }
                if (!s->cmdStack.Execute(s->doc, std::make_unique<CropPageCommand>(idx, rc))) {
                    return McpToolResult::Err("Indice fuera de rango para crop.");
                }
                return McpToolResult::Ok({ { "cropped", true }, { "page_index", idx } });
            }
            if (action == "set_size") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                if (idx >= s->doc.Pages().size()) return McpToolResult::Err("Pagina fuera de rango.");
                applyPresetDims(s->doc.MutablePages()[idx]);
                s->doc.RebuildLayout();
                s->doc.BumpRevision();
                s->doc.SetDirty(true);
                return McpToolResult::Ok({
                    { "page_index", idx },
                    { "width_pt", s->doc.Pages()[idx].widthPt },
                    { "height_pt", s->doc.Pages()[idx].heightPt }
                });
            }
            if (action == "set_background") {
                const ObjectTarget tgt = TargetFromArgs(args, s->doc);
                const PaperPattern bg = ParsePaperPattern(args.value("background", "grid"), PaperPattern::Grid);
                s->cmdStack.Execute(s->doc, std::make_unique<SetBackgroundCommand>(tgt, bg));
                return McpToolResult::Ok({ { "background", PaperPatternToString(bg) }, { "is_canvas", tgt.isCanvas }, { "page_index", tgt.pageIndex } });
            }
            if (action == "toggle_spread") {
                const bool en = args.contains("enabled") ? args["enabled"].get<bool>() : !s->doc.DoublePageSpread();
                s->doc.SetDoublePageSpread(en);
                return McpToolResult::Ok({ { "double_page_spread", s->doc.DoublePageSpread() } });
            }
            if (action == "collapse_range") {
                const int sp = args.value("start_page", 0);
                const int ep = args.value("end_page", sp);
                s->doc.ToggleCollapsedRange(sp, ep);
                return McpToolResult::Ok({ { "toggled", true }, { "start_page", sp }, { "end_page", ep } });
            }
            if (action == "bookmark_list") {
                json arr = json::array();
                for (const auto& bm : s->doc.Bookmarks()) {
                    arr.push_back({ { "id", bm.id }, { "title", bm.titleUtf8 }, { "page_index", bm.pageIndex }, { "world_y", bm.worldY }, { "level", bm.level } });
                }
                return McpToolResult::Ok({ { "bookmarks", std::move(arr) } });
            }
            if (action == "bookmark_add") {
                const std::uint64_t bid = s->doc.AddBookmark(
                    args.value("title", "Marcador"),
                    static_cast<std::size_t>(std::max(0, args.value("page_index", 0))),
                    args.value("world_y", 0.0),
                    args.value("level", 0));
                return McpToolResult::Ok({ { "bookmark_id", bid } });
            }
            if (action == "bookmark_rename") {
                const std::uint64_t bid = args.value("bookmark_id", std::uint64_t{ 0 });
                const bool ok = FlexcilStudyOverlay::RenameBookmark(s->doc, bid, args.value("title", ""), args.value("level", -1));
                return ok ? McpToolResult::Ok({ { "renamed", true }, { "bookmark_id", bid } })
                          : McpToolResult::Err("Marcador no encontrado.");
            }
            if (action == "bookmark_remove") {
                const bool ok = s->doc.RemoveBookmark(args.value("bookmark_id", std::uint64_t{ 0 }));
                return ok ? McpToolResult::Ok({ { "removed", true } }) : McpToolResult::Err("Marcador no encontrado.");
            }
            if (action == "outline_extract") {
                json headings = json::array();
                for (std::size_t i = 0; i < s->doc.Pages().size(); ++i) {
                    const auto& pg = s->doc.Pages()[i];
                    for (const auto& o : pg.objects) {
                        if (o.kind == ObjectKind::TextBox && (o.bold || o.fontSizePt >= 16.0 || (!o.textUtf8.empty() && o.textUtf8[0] == '#'))) {
                            headings.push_back({ { "page_index", i }, { "object_id", o.id }, { "title", o.textUtf8 } });
                        }
                    }
                }
                return McpToolResult::Ok({ { "outline", std::move(headings) } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_page: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_layer",
        "Gestiona capas del documento (listar, agregar, eliminar, mover, activar, visibilidad/bloqueo/exportacion/opacidad y capa privada de notas del presentador/docente).",
        MakeToolSchema(
            { "list", "add", "remove", "move", "set_active", "set_properties", "toggle_teacher_private" },
            {
                { "session_id", { { "type", "string" } } },
                { "name", { { "type", "string" } } },
                { "layer_id", { { "type", "integer" } } },
                { "from_index", { { "type", "integer" } } },
                { "to_index", { { "type", "integer" } } },
                { "visible", { { "type", "boolean" } } },
                { "locked", { { "type", "boolean" } } },
                { "include_in_export", { { "type", "boolean" } } },
                { "opacity", { { "type", "number" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const std::string action = args.value("action", "");
            if (action == "list") {
                return McpToolResult::Ok({ { "active_layer_id", s->doc.ActiveLayerId() }, { "layers", SerializeSessionSnapshot(*s)["layers"] } });
            }
            if (action == "add") {
                const std::uint64_t lid = s->doc.AddLayer(args.value("name", "Nueva capa"));
                return McpToolResult::Ok({ { "layer_id", lid }, { "layer_count", s->doc.Layers().size() } });
            }
            if (action == "remove") {
                const bool ok = s->doc.RemoveLayer(args.value("layer_id", std::uint64_t{ 0 }));
                return ok ? McpToolResult::Ok({ { "removed", true } }) : McpToolResult::Err("No se pudo eliminar capa.");
            }
            if (action == "move") {
                const bool ok = s->doc.MoveLayer(
                    static_cast<std::size_t>(std::max(0, args.value("from_index", 0))),
                    static_cast<std::size_t>(std::max(0, args.value("to_index", 0))));
                return ok ? McpToolResult::Ok({ { "moved", true } }) : McpToolResult::Err("Indices de capa invalidos.");
            }
            if (action == "set_active") {
                const std::uint64_t lid = args.value("layer_id", std::uint64_t{ 1 });
                s->doc.SetActiveLayerId(lid);
                return McpToolResult::Ok({ { "active_layer_id", s->doc.ActiveLayerId() } });
            }
            if (action == "set_properties") {
                const std::uint64_t lid = args.value("layer_id", s->doc.ActiveLayerId());
                const bool ok = s->doc.SetLayerProperties(
                    lid,
                    args.value("visible", true),
                    args.value("locked", false),
                    args.value("include_in_export", true),
                    args.value("opacity", 1.0));
                return ok ? McpToolResult::Ok({ { "updated", true }, { "layer_id", lid } })
                          : McpToolResult::Err("Capa no encontrada.");
            }
            if (action == "toggle_teacher_private") {
                for (const auto& ly : s->doc.Layers()) {
                    if (!ly.includeInExport || LowerAscii(ly.nameUtf8).find("docente") != std::string::npos ||
                        LowerAscii(ly.nameUtf8).find("orador") != std::string::npos) {
                        s->doc.SetActiveLayerId(ly.id);
                        return McpToolResult::Ok({ { "layer_id", ly.id }, { "created", false }, { "include_in_export", false } });
                    }
                }
                const std::uint64_t lid = s->doc.AddLayer("Notas privadas del orador");
                s->doc.SetLayerProperties(lid, true, false, false, 0.92);
                s->doc.SetActiveLayerId(lid);
                return McpToolResult::Ok({ { "layer_id", lid }, { "created", true }, { "include_in_export", false } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_layer: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_attachment",
        "Gestiona archivos adjuntos incrustados y payloads JSON de extension dentro de un .cudoc.",
        MakeToolSchema(
            { "list", "add", "extract", "remove", "extension_get", "extension_set" },
            {
                { "session_id", { { "type", "string" } } },
                { "file_path", { { "type", "string" } } },
                { "filename", { { "type", "string" } } },
                { "mime_type", { { "type", "string" } } },
                { "content_base64", { { "type", "string" } } },
                { "asset_key", { { "type", "string" } } },
                { "output_path", { { "type", "string" } } },
                { "name", { { "type", "string" } } },
                { "payload", { { "type", "string" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const std::string action = args.value("action", "");
            if (action == "list") {
                json atts = json::array();
                for (const auto& a : s->doc.Attachments()) {
                    atts.push_back({ { "filename", a.filenameUtf8 }, { "mime_type", a.mimeTypeUtf8 }, { "asset_key", a.assetKey }, { "size_bytes", a.sizeBytes } });
                }
                json exts = json::array();
                for (const auto& [k, v] : s->doc.ExtensionPayloads()) {
                    exts.push_back({ { "name", k }, { "bytes", v.size() } });
                }
                return McpToolResult::Ok({ { "attachments", std::move(atts) }, { "extensions", std::move(exts) } });
            }
            if (action == "add") {
                std::vector<std::uint8_t> bytes;
                std::string fname = args.value("filename", "adjunto.bin");
                if (args.contains("file_path") && !args["file_path"].get<std::string>().empty()) {
                    const std::wstring fpW = Utf8ToWide(args["file_path"].get<std::string>());
                    std::string err;
                    if (!ctx.ReadBytesFromFile(fpW, bytes, &err)) return McpToolResult::Err(err);
                    if (!args.contains("filename")) fname = WideToUtf8(fs::path(fpW).filename().wstring());
                } else if (args.contains("content_base64")) {
                    bytes = Base64Decode(args["content_base64"].get<std::string>());
                }
                const std::string key = s->doc.AddAttachment(fname, args.value("mime_type", "application/octet-stream"), std::move(bytes));
                return McpToolResult::Ok({ { "asset_key", key }, { "filename", fname } });
            }
            if (action == "extract") {
                const std::string key = args.value("asset_key", "");
                const auto* bytes = s->doc.FindAsset(key);
                if (!bytes) return McpToolResult::Err("Asset no encontrado: " + key);
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "att_" + key, ".bin");
                std::string err;
                if (!ctx.WriteBytesToFile(outW, *bytes, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "size_bytes", bytes->size() } });
            }
            if (action == "remove") {
                const bool ok = s->doc.RemoveAttachment(args.value("asset_key", ""));
                return ok ? McpToolResult::Ok({ { "removed", true } }) : McpToolResult::Err("Adjunto no encontrado.");
            }
            if (action == "extension_get") {
                const std::string n = args.value("name", "");
                const auto* p = s->doc.FindExtensionPayload(n);
                return p ? McpToolResult::Ok({ { "name", n }, { "payload", *p } })
                         : McpToolResult::Err("Extension no encontrada: " + n);
            }
            if (action == "extension_set") {
                const std::string n = args.value("name", "");
                if (n.empty()) return McpToolResult::Err("Se requiere 'name'.");
                s->doc.SetExtensionPayload(n, args.value("payload", ""));
                s->doc.SetDirty(true);
                return McpToolResult::Ok({ { "updated", true }, { "name", n } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_attachment: " + action);
        }
    });

    // ---------------- Group C: Background Writing, Drawing, Canvas Edit, SmartScript, Snippets (4 tools) ----------------

    router.RegisterTool({
        "cuadernu_draw",
        "Escribe y dibuja programaticamente en segundo plano todos los 16 ObjectKind de CuadernU: tinta vectorial, escritura manuscrita sintetizada (write_handwriting), formas, cuadros de texto, notas adhesivas, cinta de estudio, diagramas, stickers, imagenes, LinkCard, video, HtmlEmbed, codigo y tablas.",
        MakeToolSchema(
            {
                "add_ink_stroke", "write_handwriting", "add_shape", "add_text_box", "add_sticky_note",
                "add_study_tape", "toggle_study_tape", "add_diagram", "add_sticker", "add_image",
                "add_link_card", "textbox_url_to_link_card", "add_video_clip", "add_html_embed",
                "add_code_block", "add_table_block", "batch_add"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "page_index", { { "type", "integer" } } },
                { "layer_id", { { "type", "integer" } } },
                { "points", { { "type", "array" } } },
                { "pen_style", { { "type", "string" } } },
                { "stroke_color", {} },
                { "fill_color", {} },
                { "stroke_width", { { "type", "number" } } },
                { "stabilization", { { "type", "number" } } },
                { "snap_shape", { { "type", "boolean" } } },
                { "shape_kind", { { "type", "string" } } },
                { "start", { { "type", "object" } } },
                { "end", { { "type", "object" } } },
                { "bounds", { { "type", "object" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } },
                { "w", { { "type", "number" } } },
                { "h", { { "type", "number" } } },
                { "text", { { "type", "string" } } },
                { "font_family", { { "type", "string" } } },
                { "font_size_pt", { { "type", "number" } } },
                { "bold", { { "type", "boolean" } } },
                { "italic", { { "type", "boolean" } } },
                { "underline", { { "type", "boolean" } } },
                { "list_style", { { "type", "string" } } },
                { "is_redaction", { { "type", "boolean" } } },
                { "object_id", { { "type", "integer" } } },
                { "nodes", { { "type", "array" } } },
                { "edges", { { "type", "array" } } },
                { "sticker_kind", { { "type", "string" } } },
                { "scale", { { "type", "number" } } },
                { "image_path", { { "type", "string" } } },
                { "image_base64", { { "type", "string" } } },
                { "caption", { { "type", "string" } } },
                { "url", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "description", { { "type", "string" } } },
                { "target_page_index", { { "type", "integer" } } },
                { "video_path", { { "type", "string" } } },
                { "html", { { "type", "string" } } },
                { "allow_network", { { "type", "boolean" } } },
                { "language", { { "type", "string" } } },
                { "table_json", {} },
                { "tags", { { "type", "array" } } },
                { "items", { { "type", "array" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const ObjectTarget tgt = TargetFromArgs(args, s->doc);
            const std::string action = args.value("action", "");

            auto applyCommon = [&](AnnotationObject& o, const json& src) {
                o.id = s->doc.AllocateObjectId();
                o.zOrder = s->doc.AllocateZOrder();
                o.layerId = src.value("layer_id", s->doc.ActiveLayerId());
                if (src.contains("opacity")) o.opacity = std::clamp(src["opacity"].get<double>(), 0.05, 1.0);
                if (src.contains("rotation_deg")) o.rotationDeg = src["rotation_deg"].get<double>();
                if (src.contains("locked_in_place")) o.lockedInPlace = src["locked_in_place"].get<bool>();
                if (src.contains("tags") && src["tags"].is_array()) {
                    for (const auto& t : src["tags"]) if (t.is_string()) o.tags.push_back(t.get<std::string>());
                }
            };

            auto buildRect = [&](const json& src, DocRect def) -> DocRect {
                if (src.contains("bounds") && src["bounds"].is_object()) return ParseRectJson(src["bounds"], def);
                return { src.value("x", def.x), src.value("y", def.y), src.value("w", def.w), src.value("h", def.h) };
            };

            auto addSingleObj = [&](AnnotationObject o) -> McpToolResult {
                const std::uint64_t id = o.id;
                json summary = SerializeObjectSummary(o);
                if (!s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(tgt, std::move(o)))) {
                    return McpToolResult::Err("No se pudo agregar el objeto al documento.");
                }
                return McpToolResult::Ok({ { "object_id", id }, { "object", std::move(summary) } });
            };

            if (action == "add_ink_stroke") {
                auto rawPts = ParseInkPointsJson(args.value("points", json::array()));
                if (rawPts.empty()) return McpToolResult::Err("Se requieren puntos en 'points'.");
                const double stab = std::clamp(args.value("stabilization", 0.25), 0.0, 1.0);
                auto smoothPts = InkEngine::SmoothInkStroke(rawPts, stab);
                if (args.value("snap_shape", false)) {
                    const auto snapped = InkEngine::RecognizeHeldShape(smoothPts);
                    if (snapped.recognized) {
                        AnnotationObject sh{};
                        applyCommon(sh, args);
                        sh.kind = ObjectKind::Shape;
                        sh.shapeKind = snapped.kind;
                        sh.shapeStart = snapped.start;
                        sh.shapeEnd = snapped.end;
                        sh.bounds = snapped.bounds;
                        sh.strokeColor = ParseColorJson(args.value("stroke_color", json("#181E2AFF")));
                        sh.strokeWidth = args.value("stroke_width", 2.5);
                        sh.inkSnapped = true;
                        sh.RecomputeBounds();
                        return addSingleObj(std::move(sh));
                    }
                }
                AnnotationObject st{};
                applyCommon(st, args);
                st.kind = ObjectKind::InkStroke;
                st.penStyle = ParseInkPenStyle(args.value("pen_style", "ballpoint"));
                st.isHighlighter = (st.penStyle == InkPenStyle::Highlighter);
                st.strokeColor = ParseColorJson(
                    args.contains("stroke_color") ? args["stroke_color"]
                        : (args.contains("color") ? args["color"] : json(st.isHighlighter ? "#FDE04788" : "#181E2AFF")));
                st.strokeWidth = args.value("stroke_width", st.isHighlighter ? 14.0 : 2.5);
                st.stabilization = stab;
                st.inkPoints = std::move(smoothPts);
                if (!st.inkPoints.empty()) {
                    st.inkStartTimeMs = st.inkPoints.front().timestampMs;
                    st.inkEndTimeMs = st.inkPoints.back().timestampMs;
                }
                st.RecomputeBounds();
                return addSingleObj(std::move(st));
            }
            if (action == "write_handwriting") {
                const std::string text = args.value("text", "");
                if (text.empty()) return McpToolResult::Err("Se requiere 'text' para escribir a mano alzada.");
                const double sx = args.value("x", 72.0);
                const double sy = args.value("y", 96.0);
                const auto ids = SmartScriptReflowEngine::PasteTextAsHandwriting(s->doc, s->cmdStack, tgt, text, sx, sy);
                const bool hasColor = args.contains("stroke_color") || args.contains("color");
                const DocColor col = hasColor
                    ? ParseColorJson(args.contains("stroke_color") ? args["stroke_color"] : args["color"])
                    : DocColor::RGBA(24, 30, 42, 255);
                const double reqH = args.value("height_pt", 0.0);
                const double scaleF = (reqH >= 8.0 && reqH <= 180.0) ? (reqH / 22.0) : 1.0;
                if (hasColor || std::abs(scaleF - 1.0) > 0.02) {
                    for (const auto id : ids) {
                        if (auto* o = FindObjectMutable(s->doc, tgt, id)) {
                            if (hasColor) o->strokeColor = col;
                            if (std::abs(scaleF - 1.0) > 0.02) {
                                for (auto& pt : o->inkPoints) {
                                    pt.x = sx + (pt.x - sx) * scaleF;
                                    pt.y = sy + (pt.y - sy) * scaleF;
                                }
                                o->RecomputeBounds();
                            }
                        }
                    }
                    s->doc.BumpRevision();
                }
                return McpToolResult::Ok({ { "stroke_ids", ids }, { "stroke_count", ids.size() }, { "text", text } });
            }
            if (action == "add_shape") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::Shape;
                o.shapeKind = ParseShapeKind(args.value("shape_kind", "rectangle"));
                o.bounds = buildRect(args, { 80.0, 80.0, 180.0, 90.0 });
                o.shapeStart = args.contains("start") && args["start"].is_object()
                    ? DocPoint{ args["start"].value("x", o.bounds.x), args["start"].value("y", o.bounds.y) }
                    : DocPoint{ o.bounds.x, o.bounds.y };
                o.shapeEnd = args.contains("end") && args["end"].is_object()
                    ? DocPoint{ args["end"].value("x", o.bounds.Right()), args["end"].value("y", o.bounds.Bottom()) }
                    : DocPoint{ o.bounds.Right(), o.bounds.Bottom() };
                o.strokeColor = ParseColorJson(args.value("stroke_color", json("#1D4ED8FF")));
                o.fillColor = ParseColorJson(args.value("fill_color", json("#00000000")), DocColor::RGBA(0, 0, 0, 0));
                o.strokeWidth = args.value("stroke_width", 2.2);
                o.isRedaction = args.value("is_redaction", false);
                if (o.isRedaction) {
                    o.fillColor = DocColor::RGBA(15, 23, 42, 255);
                    o.strokeColor = DocColor::RGBA(15, 23, 42, 255);
                }
                o.connectorFromId = args.value("connector_from_id", std::uint64_t{ 0 });
                o.connectorToId = args.value("connector_to_id", std::uint64_t{ 0 });
                o.textUtf8 = args.value("text", "");
                o.RecomputeBounds();
                return addSingleObj(std::move(o));
            }
            if (action == "add_text_box") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::TextBox;
                o.bounds = buildRect(args, { 72.0, 72.0, 420.0, 84.0 });
                o.textUtf8 = args.value("text", "");
                o.fontFamilyUtf8 = args.value("font_family", "Segoe UI");
                o.fontSizePt = args.value("font_size_pt", 14.0);
                o.bold = args.value("bold", false);
                o.italic = args.value("italic", false);
                o.underline = args.value("underline", false);
                o.listStyleUtf8 = args.value("list_style", "");
                o.strokeColor = ParseColorJson(args.value("stroke_color", json("#181E2AFF")));
                o.fillColor = ParseColorJson(args.value("fill_color", json("#00000000")), DocColor::RGBA(0, 0, 0, 0));
                return addSingleObj(std::move(o));
            }
            if (action == "add_sticky_note") {
                const DocColor fill = ParseColorJson(args.value("fill_color", json("#FEF9C3F8")), DocColor::RGBA(254, 249, 195, 248));
                const std::uint64_t id = CreateStickyNoteObject(
                    s->doc, s->cmdStack, tgt,
                    args.value("x", 96.0), args.value("y", 96.0),
                    args.value("text", "Nota adhesiva"), fill);
                return McpToolResult::Ok({ { "object_id", id } });
            }
            if (action == "add_study_tape") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::StudyTape;
                o.bounds = buildRect(args, { 96.0, 120.0, 220.0, 34.0 });
                o.textUtf8 = args.value("text", "Concepto oculto");
                o.captionUtf8 = args.value("caption", "Pregunta de repaso");
                o.tapeRevealed = args.value("revealed", false);
                o.fillColor = ParseColorJson(args.value("fill_color", json("#6366F1E6")));
                o.strokeColor = ParseColorJson(args.value("stroke_color", json("#4338CAFF")));
                return addSingleObj(std::move(o));
            }
            if (action == "toggle_study_tape") {
                const std::uint64_t id = args.value("object_id", std::uint64_t{ 0 });
                auto* o = FindObjectMutable(s->doc, tgt, id);
                if (!o || o->kind != ObjectKind::StudyTape) return McpToolResult::Err("StudyTape no encontrado.");
                AnnotationObject before = *o;
                AnnotationObject after = *o;
                after.tapeRevealed = args.contains("revealed") ? args["revealed"].get<bool>() : !after.tapeRevealed;
                s->cmdStack.Execute(s->doc, std::make_unique<ModifyObjectCommand>(tgt, before, after));
                return McpToolResult::Ok({ { "object_id", id }, { "tape_revealed", after.tapeRevealed } });
            }
            if (action == "add_diagram") {
                std::vector<DiagramNodeSpec> nodes;
                if (args.contains("nodes") && args["nodes"].is_array()) {
                    int idx = 0;
                    for (const auto& n : args["nodes"]) {
                        DiagramNodeSpec sp{};
                        sp.labelUtf8 = n.value("label", "Nodo " + std::to_string(idx + 1));
                        sp.shape = ParseShapeKind(n.value("shape", "rectangle"));
                        sp.x = n.value("x", 80.0 + (idx % 3) * 200.0);
                        sp.y = n.value("y", 80.0 + (idx / 3) * 110.0);
                        sp.w = n.value("w", 150.0);
                        sp.h = n.value("h", 58.0);
                        if (n.contains("fill_color")) sp.fill = ParseColorJson(n["fill_color"], sp.fill);
                        if (n.contains("stroke_color")) sp.stroke = ParseColorJson(n["stroke_color"], sp.stroke);
                        nodes.push_back(std::move(sp));
                        ++idx;
                    }
                }
                std::vector<std::pair<std::size_t, std::size_t>> edges;
                if (args.contains("edges") && args["edges"].is_array()) {
                    for (const auto& e : args["edges"]) {
                        if (e.is_array() && e.size() >= 2) {
                            edges.emplace_back(e[0].get<std::size_t>(), e[1].get<std::size_t>());
                        } else if (e.is_object()) {
                            edges.emplace_back(e.value("from", std::size_t{ 0 }), e.value("to", std::size_t{ 0 }));
                        }
                    }
                }
                std::vector<std::uint64_t> nodeIds, connIds;
                const bool ok = CreateDiagramBlockWithConnectors(s->doc, s->cmdStack, tgt, nodes, edges, &nodeIds, &connIds);
                return ok ? McpToolResult::Ok({ { "node_ids", nodeIds }, { "connector_ids", connIds } })
                          : McpToolResult::Err("No se pudo crear el diagrama (verifique 'nodes').");
            }
            if (action == "add_sticker") {
                auto objs = BuildEngineeringStickerObjects(
                    s->doc, args.value("sticker_kind", "opamp"),
                    args.value("x", 100.0), args.value("y", 100.0),
                    std::clamp(args.value("scale", 1.0), 0.25, 4.0));
                std::vector<std::uint64_t> ids;
                for (const auto& o : objs) ids.push_back(o.id);
                s->cmdStack.Execute(s->doc, std::make_unique<BatchAddObjectsCommand>(tgt, std::move(objs), "Agregar sticker"));
                return McpToolResult::Ok({ { "object_ids", ids }, { "count", ids.size() } });
            }
            if (action == "add_image") {
                std::vector<std::uint8_t> raw;
                if (args.contains("image_path") && !args["image_path"].get<std::string>().empty()) {
                    std::string err;
                    if (!ctx.ReadBytesFromFile(Utf8ToWide(args["image_path"].get<std::string>()), raw, &err)) {
                        return McpToolResult::Err(err);
                    }
                } else if (args.contains("image_base64")) {
                    raw = Base64Decode(args["image_base64"].get<std::string>());
                }
                if (raw.empty()) return McpToolResult::Err("Se requiere 'image_path' o 'image_base64'.");
                ImageProc::RgbaImage img{};
                std::vector<std::uint8_t> png = raw;
                int pw = 0, ph = 0;
#ifdef _WIN32
                if (ImageProc::DecodeImageBytesWic(raw.data(), raw.size(), img) && img.Valid()) {
                    pw = img.width;
                    ph = img.height;
                    std::vector<std::uint8_t> reenc;
                    if (ImageProc::EncodeRgbaToPngWic(img, reenc) && !reenc.empty()) png = std::move(reenc);
                }
#endif
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::Image;
                o.assetKey = s->doc.RegisterAsset(png);
                o.pngBytes = std::move(png);
                o.imagePixelW = pw;
                o.imagePixelH = ph;
                o.captionUtf8 = args.value("caption", "");
                const double defW = pw > 0 ? std::clamp(static_cast<double>(pw) * 0.5, 80.0, 440.0) : 260.0;
                const double defH = (pw > 0 && ph > 0) ? defW * (static_cast<double>(ph) / static_cast<double>(pw)) : 180.0;
                o.bounds = buildRect(args, { 72.0, 72.0, defW, defH });
                return addSingleObj(std::move(o));
            }
            if (action == "add_link_card") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::LinkCard;
                o.bounds = buildRect(args, { 72.0, 72.0, 320.0, 78.0 });
                o.linkUrlUtf8 = args.value("url", "");
                o.linkTitleUtf8 = args.value("title", o.linkUrlUtf8);
                o.linkDescriptionUtf8 = args.value("description", "");
                o.linkTargetPageIndex = args.value("target_page_index", -1);
                return addSingleObj(std::move(o));
            }
            if (action == "textbox_url_to_link_card") {
                const std::uint64_t id = args.value("object_id", std::uint64_t{ 0 });
                auto* tb = FindObjectMutable(s->doc, tgt, id);
                if (!tb || tb->kind != ObjectKind::TextBox) return McpToolResult::Err("TextBox no encontrado.");
                const auto spans = ExtractSafeInlineUrls(tb->textUtf8);
                if (spans.empty()) return McpToolResult::Err("El TextBox no contiene ninguna URL segura (http/https/mailto/www).");
                AnnotationObject card{};
                applyCommon(card, args);
                card.kind = ObjectKind::LinkCard;
                card.bounds = { tb->bounds.x, tb->bounds.Bottom() + 10.0, (std::max)(280.0, tb->bounds.w), 74.0 };
                card.linkUrlUtf8 = spans.front().normalizedUrlUtf8;
                card.linkTitleUtf8 = spans.front().isYouTube ? "Video YouTube" : spans.front().rawTextUtf8;
                card.linkDescriptionUtf8 = spans.front().normalizedUrlUtf8;
                return addSingleObj(std::move(card));
            }
            if (action == "add_video_clip") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::VideoClip;
                o.bounds = buildRect(args, { 72.0, 72.0, 360.0, 210.0 });
                o.videoSourcePathUtf8 = args.value("video_path", "");
                o.captionUtf8 = args.value("caption", "Video");
                o.videoMuted = args.value("muted", false);
                o.videoLoop = args.value("loop", false);
                return addSingleObj(std::move(o));
            }
            if (action == "add_html_embed") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::HtmlEmbed;
                o.bounds = buildRect(args, { 72.0, 72.0, 420.0, 260.0 });
                o.htmlSourceUtf8 = args.value("html", "");
                o.htmlUrlUtf8 = args.value("url", "");
                o.htmlTitleUtf8 = args.value("title", "Componente HTML5");
                o.htmlAllowNetwork = args.value("allow_network", false);
                return addSingleObj(std::move(o));
            }
            if (action == "add_code_block") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::CodeBlock;
                o.bounds = buildRect(args, { 72.0, 72.0, 440.0, 140.0 });
                o.textUtf8 = args.value("text", "");
                o.codeLanguageUtf8 = args.value("language", "cpp");
                o.fontFamilyUtf8 = "Consolas";
                o.fontSizePt = args.value("font_size_pt", 11.0);
                return addSingleObj(std::move(o));
            }
            if (action == "add_table_block") {
                AnnotationObject o{};
                applyCommon(o, args);
                o.kind = ObjectKind::TableBlock;
                o.bounds = buildRect(args, { 72.0, 72.0, 440.0, 160.0 });
                if (args.contains("table_json")) {
                    o.tableJsonUtf8 = args["table_json"].is_string()
                        ? args["table_json"].get<std::string>()
                        : args["table_json"].dump();
                } else {
                    o.tableJsonUtf8 = R"({"headers":["Col A","Col B"],"rows":[["1","2"]]})";
                }
                o.captionUtf8 = args.value("title", "Tabla");
                return addSingleObj(std::move(o));
            }
            if (action == "batch_add") {
                if (!args.contains("items") || !args["items"].is_array()) {
                    return McpToolResult::Err("Se requiere arreglo 'items' para batch_add.");
                }
                std::vector<AnnotationObject> batch;
                std::vector<std::uint64_t> ids;
                for (const auto& item : args["items"]) {
                    const std::string kindStr = LowerAscii(item.value("kind", "text_box"));
                    AnnotationObject o{};
                    applyCommon(o, item);
                    if (kindStr == "ink_stroke") {
                        o.kind = ObjectKind::InkStroke;
                        o.penStyle = ParseInkPenStyle(item.value("pen_style", "ballpoint"));
                        o.isHighlighter = (o.penStyle == InkPenStyle::Highlighter);
                        o.strokeColor = ParseColorJson(item.value("stroke_color", json("#181E2AFF")));
                        o.strokeWidth = item.value("stroke_width", 2.4);
                        o.inkPoints = ParseInkPointsJson(item.value("points", json::array()));
                        o.RecomputeBounds();
                    } else if (kindStr == "shape") {
                        o.kind = ObjectKind::Shape;
                        o.shapeKind = ParseShapeKind(item.value("shape_kind", "rectangle"));
                        o.bounds = buildRect(item, { 80, 80, 160, 80 });
                        o.shapeStart = { o.bounds.x, o.bounds.y };
                        o.shapeEnd = { o.bounds.Right(), o.bounds.Bottom() };
                        o.strokeColor = ParseColorJson(item.value("stroke_color", json("#1D4ED8FF")));
                        o.fillColor = ParseColorJson(item.value("fill_color", json("#00000000")), DocColor::RGBA(0, 0, 0, 0));
                        o.strokeWidth = item.value("stroke_width", 2.0);
                        o.textUtf8 = item.value("text", "");
                        o.RecomputeBounds();
                    } else if (kindStr == "math_block") {
                        o.kind = ObjectKind::MathBlock;
                        o.bounds = buildRect(item, { 72, 72, 340, 64 });
                        o.formulaExprUtf8 = item.value("formula_expr", item.value("text", ""));
                        o.evaluatedResultUtf8 = item.value("evaluated_result", "");
                    } else {
                        o.kind = ObjectKind::TextBox;
                        o.bounds = buildRect(item, { 72, 72, 380, 72 });
                        o.textUtf8 = item.value("text", "");
                        o.fontSizePt = item.value("font_size_pt", 14.0);
                        o.bold = item.value("bold", false);
                        o.strokeColor = ParseColorJson(item.value("stroke_color", json("#181E2AFF")));
                    }
                    ids.push_back(o.id);
                    batch.push_back(std::move(o));
                }
                s->cmdStack.Execute(s->doc, std::make_unique<BatchAddObjectsCommand>(tgt, std::move(batch), "Agregar lote MCP"));
                return McpToolResult::Ok({ { "object_ids", ids }, { "count", ids.size() } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_draw: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_canvas_edit",
        "Inspecciona, consulta espacialmente, edita, mueve, escala, rota, agrupa, borra (completo/parcial/selectivo), selecciona con lazo, ancla, embellece tinta, inserta espacio vertical, ajusta espaciado, recorta regiones y reproduce cronologia de trazos.",
        MakeToolSchema(
            {
                "list_objects", "get_object", "query_rect", "modify_object", "move", "scale", "rotate",
                "group", "ungroup", "duplicate", "lock_in_place", "set_tags", "tag_board",
                "erase_object", "erase_at", "erase_partial", "erase_by_filter", "clear_highlights",
                "lasso_select", "anchor_ink", "neaten_ink", "snap_shape", "restyle_ink",
                "insert_vertical_space", "adjust_sheet_spacing", "snip_region", "replay_range", "replay_at_time"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "page_index", { { "type", "integer" } } },
                { "object_id", { { "type", "integer" } } },
                { "object_ids", { { "type", "array" } } },
                { "rect", { { "type", "object" } } },
                { "dx", { { "type", "number" } } },
                { "dy", { { "type", "number" } } },
                { "scale_x", { { "type", "number" } } },
                { "scale_y", { { "type", "number" } } },
                { "rotation_deg", { { "type", "number" } } },
                { "locked", { { "type", "boolean" } } },
                { "tags", { { "type", "array" } } },
                { "tag_filter", { { "type", "string" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } },
                { "radius", { { "type", "number" } } },
                { "filter_mode", { { "type", "string" } } },
                { "polygon", { { "type", "array" } } },
                { "parent_id", { { "type", "integer" } } },
                { "stroke_color", {} },
                { "stroke_width", { { "type", "number" } } },
                { "y_cut", { { "type", "number" } } },
                { "delta_y", { { "type", "number" } } },
                { "gap_pt", { { "type", "number" } } },
                { "cursor_ms", { { "type", "integer" } } },
                { "patch", { { "type", "object" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const ObjectTarget tgt = TargetFromArgs(args, s->doc);
            const std::string action = args.value("action", "");

            auto collectIds = [&]() -> std::vector<std::uint64_t> {
                std::vector<std::uint64_t> ids;
                if (args.contains("object_ids") && args["object_ids"].is_array()) {
                    for (const auto& v : args["object_ids"]) ids.push_back(v.get<std::uint64_t>());
                } else if (args.contains("object_id")) {
                    ids.push_back(args["object_id"].get<std::uint64_t>());
                }
                return ids;
            };

            if (action == "list_objects") {
                const auto* list = GetObjectListConst(s->doc, tgt);
                if (!list) return McpToolResult::Err("Pagina invalida.");
                json arr = json::array();
                for (const auto& o : *list) arr.push_back(SerializeObjectSummary(o));
                return McpToolResult::Ok({ { "count", list->size() }, { "objects", std::move(arr) } });
            }
            if (action == "get_object") {
                auto* o = FindObjectMutable(s->doc, tgt, args.value("object_id", std::uint64_t{ 0 }));
                if (!o) return McpToolResult::Err("Objeto no encontrado.");
                return McpToolResult::Ok(SerializeObjectFull(*o));
            }
            if (action == "query_rect") {
                const DocRect r = ParseRectJson(args.value("rect", json::object()), { 0, 0, 600, 842 });
                json arr = json::array();
                if (tgt.isCanvas) {
                    for (const auto id : s->doc.QueryCanvasRect(r)) {
                        if (auto* o = s->doc.FindCanvasObjectMutable(id)) arr.push_back(SerializeObjectSummary(*o));
                    }
                } else if (tgt.pageIndex < s->doc.Pages().size()) {
                    for (const auto& o : s->doc.Pages()[tgt.pageIndex].objects) {
                        if (o.bounds.Intersects(r)) arr.push_back(SerializeObjectSummary(o));
                    }
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "objects", std::move(arr) } });
            }
            if (action == "modify_object") {
                const std::uint64_t id = args.value("object_id", std::uint64_t{ 0 });
                auto* o = FindObjectMutable(s->doc, tgt, id);
                if (!o) return McpToolResult::Err("Objeto no encontrado.");
                AnnotationObject before = *o;
                AnnotationObject after = *o;
                const json& p = args.contains("patch") && args["patch"].is_object() ? args["patch"] : args;
                if (p.contains("text")) after.textUtf8 = p["text"].get<std::string>();
                if (p.contains("caption")) after.captionUtf8 = p["caption"].get<std::string>();
                if (p.contains("bounds") && p["bounds"].is_object()) after.bounds = ParseRectJson(p["bounds"], after.bounds);
                if (p.contains("stroke_color")) after.strokeColor = ParseColorJson(p["stroke_color"], after.strokeColor);
                if (p.contains("fill_color")) after.fillColor = ParseColorJson(p["fill_color"], after.fillColor);
                if (p.contains("stroke_width")) after.strokeWidth = p["stroke_width"].get<double>();
                if (p.contains("font_size_pt")) after.fontSizePt = p["font_size_pt"].get<double>();
                if (p.contains("bold")) after.bold = p["bold"].get<bool>();
                if (p.contains("italic")) after.italic = p["italic"].get<bool>();
                if (p.contains("underline")) after.underline = p["underline"].get<bool>();
                if (p.contains("opacity")) after.opacity = std::clamp(p["opacity"].get<double>(), 0.05, 1.0);
                if (p.contains("rotation_deg")) after.rotationDeg = p["rotation_deg"].get<double>();
                if (p.contains("layer_id")) after.layerId = p["layer_id"].get<std::uint64_t>();
                s->cmdStack.Execute(s->doc, std::make_unique<ModifyObjectCommand>(tgt, before, after));
                return McpToolResult::Ok(SerializeObjectFull(after));
            }
            if (action == "move") {
                const double dx = args.value("dx", 0.0), dy = args.value("dy", 0.0);
                const auto ids = collectIds();
                std::vector<AnnotationObject> befs, afts;
                for (const auto id : ids) {
                    if (auto* o = FindObjectMutable(s->doc, tgt, id)) {
                        if (o->lockedInPlace) continue;
                        befs.push_back(*o);
                        AnnotationObject a = *o;
                        a.Translate(dx, dy);
                        afts.push_back(std::move(a));
                    }
                }
                if (befs.empty()) return McpToolResult::Err("Ningun objeto movible encontrado.");
                s->cmdStack.Execute(s->doc, std::make_unique<BatchModifyObjectsCommand>(tgt, befs, afts, "Mover objetos"));
                for (const auto id : ids) {
                    InkEngine::UpdateAnchoredChildrenAfterMove(s->doc, tgt, id, dx, dy);
                    InkEngine::UpdateMagneticConnectors(s->doc, tgt, id);
                }
                return McpToolResult::Ok({ { "moved_count", afts.size() }, { "dx", dx }, { "dy", dy } });
            }
            if (action == "scale") {
                const auto ids = collectIds();
                const double sx = args.value("scale_x", 1.0);
                const double sy = args.value("scale_y", sx);
                const bool ok = SmartScriptReflowEngine::ScaleAndTransformInkSelection(
                    s->doc, s->cmdStack, tgt, ids, sx, sy, args.value("dx", 0.0), args.value("dy", 0.0));
                return ok ? McpToolResult::Ok({ { "scaled", true }, { "count", ids.size() } })
                          : McpToolResult::Err("No se pudo escalar la seleccion.");
            }
            if (action == "rotate") {
                const double deg = args.value("rotation_deg", 0.0);
                const auto ids = collectIds();
                std::vector<AnnotationObject> befs, afts;
                for (const auto id : ids) {
                    if (auto* o = FindObjectMutable(s->doc, tgt, id)) {
                        befs.push_back(*o);
                        AnnotationObject a = *o;
                        a.rotationDeg = deg;
                        afts.push_back(std::move(a));
                    }
                }
                if (befs.empty()) return McpToolResult::Err("Objeto no encontrado para rotar.");
                s->cmdStack.Execute(s->doc, std::make_unique<BatchModifyObjectsCommand>(tgt, befs, afts, "Rotar objetos"));
                return McpToolResult::Ok({ { "rotated", afts.size() }, { "rotation_deg", deg } });
            }
            if (action == "group" || action == "ungroup") {
                const auto ids = collectIds();
                const std::uint64_t gid = (action == "group") ? s->doc.AllocateObjectId() : 0u;
                std::vector<AnnotationObject> befs, afts;
                for (const auto id : ids) {
                    if (auto* o = FindObjectMutable(s->doc, tgt, id)) {
                        befs.push_back(*o);
                        AnnotationObject a = *o;
                        a.groupId = gid;
                        afts.push_back(std::move(a));
                    }
                }
                if (!befs.empty()) s->cmdStack.Execute(s->doc, std::make_unique<BatchModifyObjectsCommand>(tgt, befs, afts, action));
                return McpToolResult::Ok({ { "group_id", gid }, { "count", afts.size() } });
            }
            if (action == "duplicate") {
                const auto ids = collectIds();
                const double dx = args.value("dx", 18.0), dy = args.value("dy", 18.0);
                std::vector<AnnotationObject> clones;
                std::vector<std::uint64_t> newIds;
                for (const auto id : ids) {
                    if (auto* o = FindObjectMutable(s->doc, tgt, id)) {
                        AnnotationObject c = *o;
                        c.id = s->doc.AllocateObjectId();
                        c.zOrder = s->doc.AllocateZOrder();
                        c.Translate(dx, dy);
                        newIds.push_back(c.id);
                        clones.push_back(std::move(c));
                    }
                }
                if (clones.empty()) return McpToolResult::Err("Nada que duplicar.");
                s->cmdStack.Execute(s->doc, std::make_unique<BatchAddObjectsCommand>(tgt, std::move(clones), "Duplicar objetos"));
                return McpToolResult::Ok({ { "new_object_ids", newIds } });
            }
            if (action == "lock_in_place") {
                const bool lk = args.value("locked", true);
                const auto ids = collectIds();
                std::vector<AnnotationObject> befs, afts;
                for (const auto id : ids) {
                    if (auto* o = FindObjectMutable(s->doc, tgt, id)) {
                        befs.push_back(*o);
                        AnnotationObject a = *o;
                        a.lockedInPlace = lk;
                        afts.push_back(std::move(a));
                    }
                }
                if (!befs.empty()) s->cmdStack.Execute(s->doc, std::make_unique<BatchModifyObjectsCommand>(tgt, befs, afts, "Bloquear objetos"));
                return McpToolResult::Ok({ { "locked", lk }, { "count", afts.size() } });
            }
            if (action == "set_tags") {
                auto* o = FindObjectMutable(s->doc, tgt, args.value("object_id", std::uint64_t{ 0 }));
                if (!o) return McpToolResult::Err("Objeto no encontrado.");
                AnnotationObject before = *o, after = *o;
                after.tags.clear();
                if (args.contains("tags") && args["tags"].is_array()) {
                    for (const auto& t : args["tags"]) if (t.is_string()) after.tags.push_back(t.get<std::string>());
                }
                s->cmdStack.Execute(s->doc, std::make_unique<ModifyObjectCommand>(tgt, before, after));
                return McpToolResult::Ok({ { "object_id", after.id }, { "tags", after.tags } });
            }
            if (action == "tag_board") {
                const auto hits = CollectTaggedItems(s->doc, args.value("tag_filter", ""));
                json arr = json::array();
                for (const auto& h : hits) {
                    arr.push_back({ { "is_canvas", h.isCanvas }, { "page_index", h.pageIndex }, { "object_id", h.objectId }, { "tag", h.tagUtf8 }, { "summary", h.summaryUtf8 } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "items", std::move(arr) } });
            }
            if (action == "erase_object") {
                const auto ids = collectIds();
                if (ids.empty()) return McpToolResult::Err("Se requiere object_id u object_ids.");
                s->cmdStack.Execute(s->doc, std::make_unique<BatchEraseObjectsCommand>(tgt, ids));
                return McpToolResult::Ok({ { "erased_count", ids.size() } });
            }
            if (action == "erase_at" || action == "erase_partial") {
                const DocPoint center{ args.value("x", 0.0), args.value("y", 0.0) };
                const double radius = std::clamp(args.value("radius", 16.0), 1.0, 200.0);
                const auto* list = GetObjectListConst(s->doc, tgt);
                if (!list) return McpToolResult::Err("Objetivo invalido.");
                if (action == "erase_partial") {
                    std::size_t splitCount = 0;
                    std::vector<AnnotationObject> copy = *list;
                    for (const auto& o : copy) {
                        if (o.kind != ObjectKind::InkStroke || o.lockedInPlace) continue;
                        std::vector<AnnotationObject> pieces;
                        if (InkEngine::EraseStrokePartial(o, center, radius, pieces)) {
                            for (auto& pc : pieces) { pc.id = s->doc.AllocateObjectId(); pc.zOrder = s->doc.AllocateZOrder(); }
                            s->cmdStack.Execute(s->doc, std::make_unique<ReplaceStrokeWithSplitsCommand>(tgt, o, std::move(pieces)));
                            ++splitCount;
                        }
                    }
                    return McpToolResult::Ok({ { "strokes_split", splitCount } });
                }
                std::vector<std::uint64_t> hits;
                const DocRect probe{ center.x - radius, center.y - radius, radius * 2.0, radius * 2.0 };
                for (const auto& o : *list) {
                    if (!o.lockedInPlace && o.bounds.Intersects(probe)) hits.push_back(o.id);
                }
                if (!hits.empty()) s->cmdStack.Execute(s->doc, std::make_unique<BatchEraseObjectsCommand>(tgt, hits));
                return McpToolResult::Ok({ { "erased_ids", hits }, { "erased_count", hits.size() } });
            }
            if (action == "erase_by_filter" || action == "clear_highlights") {
                const std::string fm = LowerAscii(args.value("filter_mode", action == "clear_highlights" ? "highlighter" : "all"));
                EraserFilterMode mode = EraserFilterMode::All;
                if (fm == "highlighter" || fm == "highlighter_only") mode = EraserFilterMode::HighlighterOnly;
                else if (fm == "ink" || fm == "ink_only") mode = EraserFilterMode::InkOnly;
                else if (fm == "shapes" || fm == "shapes_only") mode = EraserFilterMode::ShapesOnly;
                const auto hits = InkEngine::CollectObjectsMatchingFilter(s->doc, tgt, mode, true);
                if (!hits.empty()) s->cmdStack.Execute(s->doc, std::make_unique<BatchEraseObjectsCommand>(tgt, hits));
                return McpToolResult::Ok({ { "erased_ids", hits }, { "erased_count", hits.size() } });
            }
            if (action == "lasso_select") {
                const auto poly = ParseInkPointsJson(args.value("polygon", json::array()));
                const auto hits = InkEngine::SelectObjectsInsideLasso(s->doc, tgt, poly, true);
                s->selectionIds = hits;
                return McpToolResult::Ok({ { "selected_ids", hits }, { "count", hits.size() } });
            }
            if (action == "anchor_ink") {
                const std::uint64_t childId = args.value("object_id", std::uint64_t{ 0 });
                const std::uint64_t parentId = args.value("parent_id", std::uint64_t{ 0 });
                auto* ch = FindObjectMutable(s->doc, tgt, childId);
                auto* pa = FindObjectMutable(s->doc, tgt, parentId);
                if (!ch || !pa) return McpToolResult::Err("Objeto hijo o padre no encontrado.");
                AnnotationObject before = *ch, after = *ch;
                after.anchorObjectId = parentId;
                after.anchorOffset = { ch->bounds.x - pa->bounds.x, ch->bounds.y - pa->bounds.y };
                s->cmdStack.Execute(s->doc, std::make_unique<ModifyObjectCommand>(tgt, before, after));
                return McpToolResult::Ok({ { "anchored", true }, { "object_id", childId }, { "parent_id", parentId } });
            }
            if (action == "neaten_ink") {
                const auto ids = collectIds();
                const bool ok = InkEngine::NeatenAndRestyleStrokes(s->doc, tgt, ids);
                return McpToolResult::Ok({ { "neatened", ok }, { "count", ids.size() } });
            }
            if (action == "snap_shape") {
                const std::uint64_t id = args.value("object_id", std::uint64_t{ 0 });
                auto* o = FindObjectMutable(s->doc, tgt, id);
                if (!o || o->kind != ObjectKind::InkStroke) return McpToolResult::Err("Trazo InkStroke no encontrado.");
                const auto snapped = InkEngine::RecognizeHeldShape(o->inkPoints);
                if (!snapped.recognized) return McpToolResult::Ok({ { "recognized", false } });
                AnnotationObject before = *o, after = *o;
                after.kind = ObjectKind::Shape;
                after.shapeKind = snapped.kind;
                after.shapeStart = snapped.start;
                after.shapeEnd = snapped.end;
                after.bounds = snapped.bounds;
                after.inkSnapped = true;
                after.RecomputeBounds();
                s->cmdStack.Execute(s->doc, std::make_unique<ModifyObjectCommand>(tgt, before, after));
                return McpToolResult::Ok({ { "recognized", true }, { "label", snapped.label }, { "object", SerializeObjectSummary(after) } });
            }
            if (action == "restyle_ink") {
                const auto ids = collectIds();
                std::vector<AnnotationObject> befs, afts;
                for (const auto id : ids) {
                    if (auto* o = FindObjectMutable(s->doc, tgt, id)) {
                        befs.push_back(*o);
                        AnnotationObject a = *o;
                        if (args.contains("stroke_color")) a.strokeColor = ParseColorJson(args["stroke_color"], a.strokeColor);
                        if (args.contains("stroke_width")) a.strokeWidth = args["stroke_width"].get<double>();
                        if (args.contains("pen_style")) a.penStyle = ParseInkPenStyle(args["pen_style"].get<std::string>());
                        afts.push_back(std::move(a));
                    }
                }
                if (!befs.empty()) s->cmdStack.Execute(s->doc, std::make_unique<BatchModifyObjectsCommand>(tgt, befs, afts, "Reestilizar tinta"));
                return McpToolResult::Ok({ { "restyled", afts.size() } });
            }
            if (action == "insert_vertical_space") {
                const double yCut = args.value("y_cut", 200.0);
                const double deltaY = args.value("delta_y", 80.0);
                const auto* list = GetObjectListConst(s->doc, tgt);
                if (!list) return McpToolResult::Err("Objetivo invalido.");
                std::vector<AnnotationObject> befs, afts;
                for (const auto& o : *list) {
                    if (!o.lockedInPlace && o.bounds.y >= yCut) {
                        befs.push_back(o);
                        AnnotationObject a = o;
                        a.Translate(0.0, deltaY);
                        afts.push_back(std::move(a));
                    }
                }
                if (!befs.empty()) s->cmdStack.Execute(s->doc, std::make_unique<BatchModifyObjectsCommand>(tgt, befs, afts, "Insertar espacio vertical"));
                return McpToolResult::Ok({ { "shifted_count", afts.size() }, { "y_cut", yCut }, { "delta_y", deltaY } });
            }
            if (action == "adjust_sheet_spacing") {
                const double gap = std::clamp(args.value("gap_pt", 32.0), 0.0, 400.0);
                std::vector<AnnotationObject> befs, afts;
                double curY = 48.0;
                for (const auto& o : s->doc.CanvasObjects()) {
                    if (o.kind == ObjectKind::PdfCard || o.kind == ObjectKind::Image) {
                        befs.push_back(o);
                        AnnotationObject a = o;
                        a.Translate(0.0, curY - a.bounds.y);
                        curY = a.bounds.Bottom() + gap;
                        afts.push_back(std::move(a));
                    }
                }
                if (!befs.empty()) s->cmdStack.Execute(s->doc, std::make_unique<BatchModifyObjectsCommand>(ObjectTarget{ true, 0 }, befs, afts, "Ajustar espaciado entre hojas"));
                return McpToolResult::Ok({ { "adjusted_sheets", afts.size() }, { "gap_pt", gap } });
            }
            if (action == "snip_region") {
                const DocRect r = ParseRectJson(args.value("rect", json::object()), { 50, 50, 300, 200 });
                std::vector<std::uint8_t> png;
                std::string err;
                const int outW = std::clamp(static_cast<int>(std::lround(r.w * 2.0)), 64, 2400);
                const int outH = std::clamp(static_cast<int>(std::lround(r.h * 2.0)), 64, 2400);
                if (!ctx.RenderCanvasRegionPng(*s, r, outW, outH, png, &err)) {
                    return McpToolResult::Err("Error al recortar region: " + err);
                }
                AnnotationObject imgObj{};
                imgObj.id = s->doc.AllocateObjectId();
                imgObj.zOrder = s->doc.AllocateZOrder();
                imgObj.layerId = s->doc.ActiveLayerId();
                imgObj.kind = ObjectKind::Image;
                imgObj.bounds = { r.Right() + 24.0, r.y, r.w, r.h };
                imgObj.assetKey = s->doc.RegisterAsset(png);
                imgObj.pngBytes = png;
                imgObj.imagePixelW = outW;
                imgObj.imagePixelH = outH;
                imgObj.captionUtf8 = "Recorte de region";
                s->cmdStack.Execute(s->doc, std::make_unique<AddObjectCommand>(tgt, imgObj));
                return McpToolResult::OkWithImage({ { "object_id", imgObj.id }, { "width_px", outW }, { "height_px", outH } }, std::move(png));
            }
            if (action == "replay_range") {
                const auto [mn, mx] = InkEngine::ComputeReplayTimelineRange(s->doc, tgt);
                return McpToolResult::Ok({ { "min_ms", mn }, { "max_ms", mx }, { "duration_ms", mx >= mn ? (mx - mn) : 0u } });
            }
            if (action == "replay_at_time") {
                const std::uint64_t tMs = args.value("cursor_ms", std::uint64_t{ 0 });
                const auto* list = GetObjectListConst(s->doc, tgt);
                if (!list) return McpToolResult::Err("Objetivo invalido.");
                std::size_t visibleStrokes = 0, visiblePoints = 0;
                for (const auto& o : *list) {
                    if (o.kind != ObjectKind::InkStroke) continue;
                    const auto pts = InkEngine::FilterInkPointsAtReplayTime(o, tMs);
                    if (!pts.empty()) { ++visibleStrokes; visiblePoints += pts.size(); }
                }
                return McpToolResult::Ok({ { "cursor_ms", tMs }, { "visible_strokes", visibleStrokes }, { "visible_points", visiblePoints } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_canvas_edit: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_smart_script",
        "Motor SmartScript, OCR/HTR offline WinRT y procesamiento de imagenes de pizarra/escaneo con rectificacion de perspectiva 4 puntos.",
        MakeToolSchema(
            {
                "cluster_words", "reflow_handwriting", "check_spelling", "apply_spelling",
                "convert_handwriting_to_text_shapes", "word_space_gesture",
                "ocr_page", "ocr_image", "htr_recognize",
                "image_enhance_whiteboard", "image_remove_bg", "image_rectify_perspective", "scan_rectify_to_pdf"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "page_index", { { "type", "integer" } } },
                { "left_margin_pt", { { "type", "number" } } },
                { "top_margin_pt", { { "type", "number" } } },
                { "max_line_width_pt", { { "type", "number" } } },
                { "cluster_index", { { "type", "integer" } } },
                { "gesture_points", { { "type", "array" } } },
                { "language", { { "type", "string" } } },
                { "width_px", { { "type", "integer" } } },
                { "image_path", { { "type", "string" } } },
                { "output_path", { { "type", "string" } } },
                { "quad", { { "type", "array" } } },
                { "out_w", { { "type", "integer" } } },
                { "out_h", { { "type", "integer" } } },
                { "luma_threshold", { { "type", "integer" } } },
                { "object_ids", { { "type", "array" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const ObjectTarget tgt = TargetFromArgs(args, s->doc);
            const std::string action = args.value("action", "");

            if (action == "cluster_words") {
                const auto clusters = SmartScriptReflowEngine::ClusterHandwrittenWords(s->doc, tgt);
                json arr = json::array();
                for (std::size_t i = 0; i < clusters.size(); ++i) {
                    arr.push_back({
                        { "cluster_index", i },
                        { "stroke_ids", clusters[i].strokeIds },
                        { "word", clusters[i].recognizedWordUtf8 },
                        { "bounds", { { "x", clusters[i].bounds.x }, { "y", clusters[i].bounds.y }, { "w", clusters[i].bounds.w }, { "h", clusters[i].bounds.h } } }
                    });
                }
                return McpToolResult::Ok({ { "count", clusters.size() }, { "clusters", std::move(arr) } });
            }
            if (action == "reflow_handwriting") {
                const std::size_t moved = SmartScriptReflowEngine::ReflowHandwrittenWordsToWidth(
                    s->doc, s->cmdStack, tgt,
                    args.value("left_margin_pt", 56.0),
                    args.value("top_margin_pt", 72.0),
                    args.value("max_line_width_pt", 460.0));
                return McpToolResult::Ok({ { "reflowed_clusters", moved } });
            }
            if (action == "check_spelling") {
                const auto sugs = SmartScriptReflowEngine::CheckHandwritingSpelling(s->doc, tgt);
                json arr = json::array();
                for (const auto& sg : sugs) {
                    arr.push_back({ { "cluster_index", sg.clusterIndex }, { "misspelled", sg.misspelledWordUtf8 }, { "suggested", sg.correctedWordUtf8 } });
                }
                return McpToolResult::Ok({ { "suggestions", std::move(arr) } });
            }
            if (action == "apply_spelling") {
                const auto sugs = SmartScriptReflowEngine::CheckHandwritingSpelling(s->doc, tgt);
                const std::size_t ci = static_cast<std::size_t>(std::max(0, args.value("cluster_index", 0)));
                for (const auto& sg : sugs) {
                    if (sg.clusterIndex == ci) {
                        const bool ok = SmartScriptReflowEngine::ApplyHandwritingSpellingCorrection(s->doc, s->cmdStack, tgt, sg);
                        return McpToolResult::Ok({ { "applied", ok }, { "corrected", sg.correctedWordUtf8 } });
                    }
                }
                return McpToolResult::Err("Sugerencia ortografica no encontrada para ese cluster.");
            }
            if (action == "convert_handwriting_to_text_shapes") {
                const std::size_t n = SmartScriptReflowEngine::ConvertHandwritingAndDiagramsOnDoubleTap(s->doc, s->cmdStack, tgt);
                return McpToolResult::Ok({ { "converted_count", n } });
            }
            if (action == "word_space_gesture") {
                const auto gpts = ParseInkPointsJson(args.value("gesture_points", json::array()));
                const bool ok = SmartScriptReflowEngine::ApplyWordJoinOrSplitSpaceGesture(s->doc, s->cmdStack, tgt, gpts);
                return McpToolResult::Ok({ { "applied", ok } });
            }
            if (action == "ocr_page") {
                const std::size_t pIdx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                std::vector<std::uint8_t> png;
                std::string err;
                if (!ctx.RenderPagePng(*s, pIdx, args.value("width_px", 1200), png, &err)) {
                    return McpToolResult::Err(err);
                }
                ImageProc::RgbaImage rgba{};
#ifdef _WIN32
                if (!ImageProc::DecodeImageBytesWic(png.data(), png.size(), rgba) || !rgba.Valid()) {
                    return McpToolResult::Err("Error al decodificar PNG para OCR.");
                }
                std::vector<std::uint8_t> bgra = rgba.rgba;
                for (std::size_t i = 0; i + 3 < bgra.size(); i += 4) std::swap(bgra[i], bgra[i + 2]);
                const auto ocr = WinRtOcrAdapter::RecognizeBgra32(rgba.width, rgba.height, bgra.data(), args.value("language", ""));
                if (pIdx < s->doc.Pages().size() && !ocr.fullTextUtf8.empty()) {
                    s->doc.MutablePages()[pIdx].ocrTextUtf8 = ocr.fullTextUtf8;
                }
                return McpToolResult::Ok({
                    { "engine_available", ocr.engineAvailable },
                    { "language", ocr.languageTagUtf8 },
                    { "full_text", ocr.fullTextUtf8 },
                    { "lines", ocr.linesUtf8 },
                    { "word_count", ocr.words.size() },
                    { "error", ocr.errorUtf8 }
                });
#else
                return McpToolResult::Err("OCR WinRT disponible solo en Windows.");
#endif
            }
            if (action == "ocr_image") {
                const std::string ip = args.value("image_path", "");
                if (ip.empty()) return McpToolResult::Err("Se requiere 'image_path'.");
                std::vector<std::uint8_t> raw;
                std::string err;
                if (!ctx.ReadBytesFromFile(Utf8ToWide(ip), raw, &err)) return McpToolResult::Err(err);
                ImageProc::RgbaImage rgba{};
#ifdef _WIN32
                if (!ImageProc::DecodeImageBytesWic(raw.data(), raw.size(), rgba) || !rgba.Valid()) {
                    return McpToolResult::Err("No se pudo decodificar la imagen.");
                }
                std::vector<std::uint8_t> bgra = rgba.rgba;
                for (std::size_t i = 0; i + 3 < bgra.size(); i += 4) std::swap(bgra[i], bgra[i + 2]);
                const auto ocr = WinRtOcrAdapter::RecognizeBgra32(rgba.width, rgba.height, bgra.data(), args.value("language", ""));
                return McpToolResult::Ok({
                    { "engine_available", ocr.engineAvailable },
                    { "language", ocr.languageTagUtf8 },
                    { "full_text", ocr.fullTextUtf8 },
                    { "lines", ocr.linesUtf8 },
                    { "word_count", ocr.words.size() },
                    { "error", ocr.errorUtf8 }
                });
#else
                return McpToolResult::Err("OCR WinRT disponible solo en Windows.");
#endif
            }
            if (action == "htr_recognize") {
                std::vector<AnnotationObject> strokes;
                const auto* list = GetObjectListConst(s->doc, tgt);
                if (list) {
                    for (const auto& o : *list) {
                        if (o.kind == ObjectKind::InkStroke) strokes.push_back(o);
                    }
                }
                const auto htr = WinRtInkHtrAdapter::RecognizeStrokes(strokes, args.value("language", ""));
                return McpToolResult::Ok({
                    { "recognized", htr.recognized },
                    { "used_native_ink_recognizer", htr.usedNativeInkRecognizer },
                    { "text", htr.textUtf8 },
                    { "math_latex", htr.mathLatexUtf8 },
                    { "candidates", htr.candidatesUtf8 },
                    { "error", htr.errorUtf8 }
                });
            }
            if (action == "image_enhance_whiteboard" || action == "image_remove_bg" ||
                action == "image_rectify_perspective" || action == "scan_rectify_to_pdf") {
                const std::string ip = args.value("image_path", "");
                if (ip.empty()) return McpToolResult::Err("Se requiere 'image_path'.");
                std::vector<std::uint8_t> raw;
                std::string err;
                if (!ctx.ReadBytesFromFile(Utf8ToWide(ip), raw, &err)) return McpToolResult::Err(err);
                ImageProc::RgbaImage img{};
#ifdef _WIN32
                if (!ImageProc::DecodeImageBytesWic(raw.data(), raw.size(), img) || !img.Valid()) {
                    return McpToolResult::Err("No se pudo decodificar la imagen.");
                }
                if (action == "image_enhance_whiteboard") {
                    ImageProc::EnhanceWhiteboardLighting(img);
                } else if (action == "image_remove_bg") {
                    ImageProc::RemovePaperBackground(img, static_cast<std::uint8_t>(std::clamp(args.value("luma_threshold", 220), 0, 255)));
                } else {
                    std::array<tinta::Punto, 4> quad = {
                        tinta::Punto{ 0.0f, 0.0f },
                        tinta::Punto{ static_cast<float>(img.width - 1), 0.0f },
                        tinta::Punto{ static_cast<float>(img.width - 1), static_cast<float>(img.height - 1) },
                        tinta::Punto{ 0.0f, static_cast<float>(img.height - 1) }
                    };
                    if (args.contains("quad") && args["quad"].is_array() && args["quad"].size() == 4) {
                        for (int i = 0; i < 4; ++i) {
                            const auto& q = args["quad"][i];
                            quad[i] = { q.value("x", quad[i].x), q.value("y", quad[i].y) };
                        }
                    }
                    const int ow = std::clamp(args.value("out_w", img.width), 64, 3000);
                    const int oh = std::clamp(args.value("out_h", img.height), 64, 3000);
                    img = ImageProc::RectifyPerspectiveQuad(img, quad, ow, oh);
                    ImageProc::EnhanceWhiteboardLighting(img);
                }
                std::vector<std::uint8_t> outPng;
                if (!ImageProc::EncodeRgbaToPngWic(img, outPng)) return McpToolResult::Err("Error al codificar PNG.");
                if (action == "scan_rectify_to_pdf") {
                    DocumentModel tmpDoc;
                    tmpDoc.ResetToBlank(DocumentMode::PaginatedPdf);
                    AnnotationObject im{};
                    im.id = tmpDoc.AllocateObjectId();
                    im.zOrder = tmpDoc.AllocateZOrder();
                    im.kind = ObjectKind::Image;
                    im.bounds = { 0, 0, 595.0, 842.0 };
                    im.assetKey = tmpDoc.RegisterAsset(outPng);
                    im.pngBytes = outPng;
                    im.imagePixelW = img.width;
                    im.imagePixelH = img.height;
                    tmpDoc.AddPageObject(0, std::move(im));
                    const std::wstring pdfW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "scan_rectified", ".pdf");
                    if (!PdfWriter::ExportDocumentToFile(tmpDoc, pdfW, &err)) return McpToolResult::Err(err);
                    return McpToolResult::Ok({ { "pdf_path", WideToUtf8(pdfW) }, { "width_px", img.width }, { "height_px", img.height } });
                }
                const std::wstring pngW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "processed_img", ".png");
                if (!ctx.WriteBytesToFile(pngW, outPng, &err)) return McpToolResult::Err(err);
                return McpToolResult::OkWithImage({ { "path", WideToUtf8(pngW) }, { "width_px", img.width }, { "height_px", img.height } }, std::move(outPng));
#else
                return McpToolResult::Err("Procesamiento WIC disponible solo en Windows.");
#endif
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_smart_script: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_snippets",
        "Biblioteca de elementos reutilizables, pegatinas, diagramas guardados y exportacion de notas adhesivas.",
        MakeToolSchema(
            { "list", "save", "rename", "remove", "insert", "export_sticky_notes" },
            {
                { "session_id", { { "type", "string" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "page_index", { { "type", "integer" } } },
                { "snippet_id", { { "type", "string" } } },
                { "name", { { "type", "string" } } },
                { "category", { { "type", "string" } } },
                { "object_ids", { { "type", "array" } } },
                { "tags", { { "type", "array" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            ctx.EnsureProfileLoaded();
            const std::string action = args.value("action", "");
            std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);

            if (action == "list") {
                const std::string cat = args.value("category", "");
                const auto entries = cat.empty() ? ctx.snippets.Entries() : ctx.snippets.FilterByCategory(cat);
                json arr = json::array();
                for (const auto& e : entries) {
                    arr.push_back({ { "id", e.idUtf8 }, { "name", e.nameUtf8 }, { "category", e.categoryUtf8 }, { "tags", e.tagsUtf8 }, { "object_count", e.objects.size() } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "snippets", std::move(arr) } });
            }
            if (action == "rename") {
                const bool ok = ctx.snippets.UpdateSnippetName(args.value("snippet_id", ""), args.value("name", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "renamed", true } }) : McpToolResult::Err("Recorte no encontrado.");
            }
            if (action == "remove") {
                const bool ok = ctx.snippets.RemoveSnippet(args.value("snippet_id", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "removed", true } }) : McpToolResult::Err("Recorte no encontrado.");
            }

            auto s = ctx.GetOrCreateSession(args);
            const ObjectTarget tgt = TargetFromArgs(args, s->doc);
            if (action == "save") {
                std::vector<AnnotationObject> objs;
                if (args.contains("object_ids") && args["object_ids"].is_array()) {
                    for (const auto& v : args["object_ids"]) {
                        if (auto* o = FindObjectMutable(s->doc, tgt, v.get<std::uint64_t>())) objs.push_back(*o);
                    }
                } else if (const auto* list = GetObjectListConst(s->doc, tgt)) {
                    objs = *list;
                }
                if (objs.empty()) return McpToolResult::Err("No hay objetos para guardar en el recorte.");
                std::vector<std::string> tags;
                if (args.contains("tags") && args["tags"].is_array()) {
                    for (const auto& t : args["tags"]) if (t.is_string()) tags.push_back(t.get<std::string>());
                }
                const std::string sid = ctx.snippets.SaveSnippet(
                    args.value("name", "Recorte"), args.value("category", "Fragmentos"), objs, tags);
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "snippet_id", sid }, { "object_count", objs.size() } });
            }
            if (action == "insert") {
                std::vector<std::uint64_t> insertedIds;
                const bool ok = ctx.snippets.InsertSnippetIntoDocument(
                    args.value("snippet_id", ""), s->doc, s->cmdStack, tgt,
                    args.value("x", 80.0), args.value("y", 80.0), &insertedIds);
                return ok ? McpToolResult::Ok({ { "inserted_ids", insertedIds }, { "count", insertedIds.size() } })
                          : McpToolResult::Err("Recorte no encontrado.");
            }
            if (action == "export_sticky_notes") {
                const auto res = ExportStickyNotesFromDocument(s->doc);
                return McpToolResult::Ok({ { "count", res.count }, { "markdown", res.markdownUtf8 }, { "json", res.jsonUtf8 } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_snippets: " + action);
        }
    });
}

} // namespace DocStudio::Mcp
