#include "mcp_server.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio::Mcp {

namespace {

std::string ToLowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::wstring DiscoverCurrentExePathW() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return std::wstring(buf, n);
#endif
    return {};
}

std::wstring DiscoverRepoRootFromExe(const std::wstring& exePathW) {
    std::error_code ec;
    fs::path p = exePathW.empty() ? fs::current_path(ec) : fs::path(exePathW).parent_path();
    for (int depth = 0; depth < 8 && !p.empty(); ++depth) {
        if (fs::exists(p / L"studio" / L"CMakeLists.txt", ec) &&
            fs::exists(p / L"tools" / L"check_workspace_hygiene.py", ec)) {
            return p.wstring();
        }
        const fs::path parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return {};
}

std::wstring DiscoverBuildDirFromExe(const std::wstring& exePathW, const std::wstring& repoRootW) {
    std::error_code ec;
    if (!exePathW.empty()) {
        fs::path p = fs::path(exePathW).parent_path();
        for (int depth = 0; depth < 4 && !p.empty(); ++depth) {
            if (fs::exists(p / L"CTestTestfile.cmake", ec)) return p.wstring();
            const fs::path parent = p.parent_path();
            if (parent == p) break;
            p = parent;
        }
    }
    if (!repoRootW.empty()) {
        const fs::path candidate = fs::path(repoRootW) / L"build" / L"studio-x64";
        if (fs::exists(candidate / L"CTestTestfile.cmake", ec)) return candidate.wstring();
    }
    return {};
}

std::string GetEnvString(const char* name) {
    if (!name) return {};
#ifdef _WIN32
    const DWORD need = GetEnvironmentVariableA(name, nullptr, 0);
    if (need == 0) return {};
    std::string out(need, '\0');
    const DWORD got = GetEnvironmentVariableA(name, out.data(), need);
    if (got == 0 || got >= need) return {};
    out.resize(got);
    return out;
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

std::string RenderDocumentAsMarkdownSummary(const DocumentModel& doc) {
    std::ostringstream oss;
    oss << "# " << doc.Title() << "\n\n";
    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        for (const auto& o : doc.CanvasObjects()) {
            if (!o.textUtf8.empty()) oss << o.textUtf8 << "\n\n";
            if (!o.formulaExprUtf8.empty()) {
                oss << "$$" << o.formulaExprUtf8;
                if (!o.evaluatedResultUtf8.empty()) oss << " = " << o.evaluatedResultUtf8;
                oss << "$$\n\n";
            }
            if (!o.htrRecognizedTextUtf8.empty()) oss << "> HTR: " << o.htrRecognizedTextUtf8 << "\n\n";
        }
    } else {
        for (std::size_t i = 0; i < doc.Pages().size(); ++i) {
            const auto& pg = doc.Pages()[i];
            oss << "## P\xC3\xA1gina " << (i + 1) << "\n\n";
            if (!pg.extractedTextUtf8.empty()) oss << pg.extractedTextUtf8 << "\n\n";
            if (!pg.ocrTextUtf8.empty()) oss << pg.ocrTextUtf8 << "\n\n";
            for (const auto& o : pg.objects) {
                if (!o.textUtf8.empty()) oss << o.textUtf8 << "\n\n";
                if (!o.formulaExprUtf8.empty()) {
                    oss << "$$" << o.formulaExprUtf8;
                    if (!o.evaluatedResultUtf8.empty()) oss << " = " << o.evaluatedResultUtf8;
                    oss << "$$\n\n";
                }
            }
        }
    }
    return oss.str();
}

} // namespace

// ---------------- Shared JSON Helpers ----------------

ObjectTarget TargetFromArgs(const json& args, const DocumentModel& doc) {
    ObjectTarget t{};
    if (args.contains("is_canvas") && args["is_canvas"].is_boolean()) {
        t.isCanvas = args["is_canvas"].get<bool>();
    } else if (args.contains("page_index") && args["page_index"].is_number_integer()) {
        t.isCanvas = false;
    } else {
        t.isCanvas = (doc.Mode() == DocumentMode::InfiniteCanvas);
    }
    if (!t.isCanvas) {
        const int idx = args.value("page_index", 0);
        if (idx >= 0 && !doc.Pages().empty()) {
            t.pageIndex = (std::min)(static_cast<std::size_t>(idx), doc.Pages().size() - 1u);
        } else {
            t.pageIndex = 0;
        }
    }
    return t;
}

DocColor ParseColorJson(const json& j, DocColor fallback) {
    if (j.is_string()) {
        std::string s = j.get<std::string>();
        if (!s.empty() && s[0] == '#') s.erase(s.begin());
        if (s.size() == 6 || s.size() == 8) {
            try {
                const auto r = static_cast<std::uint8_t>(std::stoul(s.substr(0, 2), nullptr, 16));
                const auto g = static_cast<std::uint8_t>(std::stoul(s.substr(2, 2), nullptr, 16));
                const auto b = static_cast<std::uint8_t>(std::stoul(s.substr(4, 2), nullptr, 16));
                const auto a = (s.size() == 8) ? static_cast<std::uint8_t>(std::stoul(s.substr(6, 2), nullptr, 16)) : static_cast<std::uint8_t>(255u);
                return DocColor::RGBA(r, g, b, a);
            } catch (...) {}
        }
    } else if (j.is_array() && j.size() >= 3) {
        const auto r = static_cast<std::uint8_t>(std::clamp(j[0].get<int>(), 0, 255));
        const auto g = static_cast<std::uint8_t>(std::clamp(j[1].get<int>(), 0, 255));
        const auto b = static_cast<std::uint8_t>(std::clamp(j[2].get<int>(), 0, 255));
        const auto a = (j.size() >= 4) ? static_cast<std::uint8_t>(std::clamp(j[3].get<int>(), 0, 255)) : static_cast<std::uint8_t>(255u);
        return DocColor::RGBA(r, g, b, a);
    } else if (j.is_object()) {
        const auto r = static_cast<std::uint8_t>(std::clamp(j.value("r", static_cast<int>(fallback.r)), 0, 255));
        const auto g = static_cast<std::uint8_t>(std::clamp(j.value("g", static_cast<int>(fallback.g)), 0, 255));
        const auto b = static_cast<std::uint8_t>(std::clamp(j.value("b", static_cast<int>(fallback.b)), 0, 255));
        const auto a = static_cast<std::uint8_t>(std::clamp(j.value("a", static_cast<int>(fallback.a)), 0, 255));
        return DocColor::RGBA(r, g, b, a);
    }
    return fallback;
}

std::string FormatColorHex(DocColor c) {
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    return buf;
}

PaperPattern ParsePaperPattern(const std::string& s, PaperPattern fallback) {
    const std::string k = ToLowerAscii(s);
    if (k == "plain" || k == "liso" || k == "blank") return PaperPattern::Plain;
    if (k == "ruled" || k == "rayado" || k == "lined") return PaperPattern::Ruled;
    if (k == "grid" || k == "cuadricula") return PaperPattern::Grid;
    if (k == "dots" || k == "puntos" || k == "dotgrid") return PaperPattern::Dots;
    if (k == "millimeter" || k == "milimetrado") return PaperPattern::Millimeter;
    if (k == "isometric" || k == "isometrico") return PaperPattern::Isometric;
    if (k == "loglog" || k == "log_log") return PaperPattern::LogLog;
    if (k == "polar") return PaperPattern::Polar;
    if (k == "smith" || k == "smithchart" || k == "smith_chart") return PaperPattern::SmithChart;
    if (k == "cornell") return PaperPattern::Cornell;
    if (k == "music" || k == "musicstaff" || k == "pentagrama") return PaperPattern::MusicStaff;
    return fallback;
}

std::string PaperPatternToString(PaperPattern p) {
    switch (p) {
    case PaperPattern::Plain: return "plain";
    case PaperPattern::Ruled: return "ruled";
    case PaperPattern::Grid: return "grid";
    case PaperPattern::Dots: return "dots";
    case PaperPattern::Millimeter: return "millimeter";
    case PaperPattern::Isometric: return "isometric";
    case PaperPattern::LogLog: return "loglog";
    case PaperPattern::Polar: return "polar";
    case PaperPattern::SmithChart: return "smith_chart";
    case PaperPattern::Cornell: return "cornell";
    case PaperPattern::MusicStaff: return "music_staff";
    }
    return "plain";
}

std::string ObjectKindToString(ObjectKind k) {
    switch (k) {
    case ObjectKind::InkStroke: return "ink_stroke";
    case ObjectKind::Shape: return "shape";
    case ObjectKind::TextBox: return "text_box";
    case ObjectKind::Image: return "image";
    case ObjectKind::PdfCard: return "pdf_card";
    case ObjectKind::StudyTape: return "study_tape";
    case ObjectKind::MathBlock: return "math_block";
    case ObjectKind::CircuitBlock: return "circuit_block";
    case ObjectKind::CalcSessionCard: return "calc_session_card";
    case ObjectKind::PlotBlock: return "plot_block";
    case ObjectKind::TableBlock: return "table_block";
    case ObjectKind::CodeBlock: return "code_block";
    case ObjectKind::AudioClip: return "audio_clip";
    case ObjectKind::LinkCard: return "link_card";
    case ObjectKind::VideoClip: return "video_clip";
    case ObjectKind::HtmlEmbed: return "html_embed";
    }
    return "unknown";
}

InkPenStyle ParseInkPenStyle(const std::string& s) {
    const std::string k = ToLowerAscii(s);
    if (k == "fountain" || k == "pluma") return InkPenStyle::Fountain;
    if (k == "brush" || k == "pincel") return InkPenStyle::Brush;
    if (k == "pencil" || k == "lapiz") return InkPenStyle::Pencil;
    if (k == "highlighter" || k == "resaltador") return InkPenStyle::Highlighter;
    return InkPenStyle::Ballpoint;
}

ShapeKind ParseShapeKind(const std::string& s) {
    const std::string k = ToLowerAscii(s);
    if (k == "line" || k == "linea") return ShapeKind::Line;
    if (k == "arrow" || k == "flecha") return ShapeKind::Arrow;
    if (k == "ellipse" || k == "circle" || k == "circulo") return ShapeKind::Ellipse;
    if (k == "signature" || k == "signature_stamp" || k == "firma") return ShapeKind::SignatureStamp;
    if (k == "connector" || k == "conector") return ShapeKind::Connector;
    return ShapeKind::Rectangle;
}

json SerializeObjectSummary(const AnnotationObject& o) {
    json j = {
        { "id", o.id },
        { "kind", ObjectKindToString(o.kind) },
        { "layer_id", o.layerId },
        { "z_order", o.zOrder },
        { "bounds", { { "x", o.bounds.x }, { "y", o.bounds.y }, { "w", o.bounds.w }, { "h", o.bounds.h } } },
        { "locked_in_place", o.lockedInPlace },
        { "group_id", o.groupId },
        { "rotation_deg", o.rotationDeg },
        { "opacity", o.opacity }
    };
    if (!o.textUtf8.empty()) j["text"] = o.textUtf8;
    if (!o.captionUtf8.empty()) j["caption"] = o.captionUtf8;
    if (!o.formulaExprUtf8.empty()) j["formula_expr"] = o.formulaExprUtf8;
    if (!o.evaluatedResultUtf8.empty()) j["evaluated_result"] = o.evaluatedResultUtf8;
    if (!o.tags.empty()) j["tags"] = o.tags;
    if (o.kind == ObjectKind::InkStroke) j["point_count"] = o.inkPoints.size();
    return j;
}

json SerializeObjectFull(const AnnotationObject& o) {
    json j = SerializeObjectSummary(o);
    j["stroke_color"] = FormatColorHex(o.strokeColor);
    j["fill_color"] = FormatColorHex(o.fillColor);
    j["stroke_width"] = o.strokeWidth;
    j["is_highlighter"] = o.isHighlighter;
    j["is_redaction"] = o.isRedaction;
    j["font_family"] = o.fontFamilyUtf8;
    j["font_size_pt"] = o.fontSizePt;
    j["bold"] = o.bold;
    j["italic"] = o.italic;
    j["underline"] = o.underline;
    j["list_style"] = o.listStyleUtf8;
    j["anchor_object_id"] = o.anchorObjectId;
    j["tape_revealed"] = o.tapeRevealed;
    j["connector_from_id"] = o.connectorFromId;
    j["connector_to_id"] = o.connectorToId;
    j["htr_text"] = o.htrRecognizedTextUtf8;
    j["circuit_netlist"] = o.circuitNetlistUtf8;
    j["plot_config"] = o.plotConfigUtf8;
    j["table_json"] = o.tableJsonUtf8;
    j["code_language"] = o.codeLanguageUtf8;
    j["link_url"] = o.linkUrlUtf8;
    j["link_title"] = o.linkTitleUtf8;
    j["calc_model"] = o.calcModelIdUtf8;
    j["calc_key_seq"] = o.calcKeySeqUtf8;
    j["html_title"] = o.htmlTitleUtf8;
    j["video_source"] = o.videoSourcePathUtf8;
    j["image_pixel_w"] = o.imagePixelW;
    j["image_pixel_h"] = o.imagePixelH;
    return j;
}

json MakeToolSchema(const std::vector<std::string>& actions, json extraProps, std::vector<std::string> required) {
    json props = extraProps.is_object() ? std::move(extraProps) : json::object();
    if (!actions.empty()) {
        props["action"] = {
            { "type", "string" },
            { "enum", actions },
            { "description", "Accion a ejecutar dentro de este dominio." }
        };
    }
    json schema = {
        { "type", "object" },
        { "properties", std::move(props) }
    };
    if (!required.empty()) {
        schema["required"] = std::move(required);
    }
    return schema;
}

// ---------------- McpServerContext ----------------

McpServerContext::McpServerContext(McpServerOptions opts)
    : options_(std::move(opts))
{
    if (options_.cuadernuExePathW.empty()) {
        options_.cuadernuExePathW = DiscoverCurrentExePathW();
    }
    if (options_.repoRootW.empty()) {
        options_.repoRootW = DiscoverRepoRootFromExe(options_.cuadernuExePathW);
    }
    if (options_.buildDirW.empty()) {
        options_.buildDirW = DiscoverBuildDirFromExe(options_.cuadernuExePathW, options_.repoRootW);
    }

    if (options_.sandboxMode) {
        std::error_code ec;
        const std::wstring baseDir = !options_.buildDirW.empty()
            ? (fs::path(options_.buildDirW) / L"mcp_sandbox").wstring()
            : (fs::temp_directory_path(ec) / L"cuadernu_mcp_sandbox").wstring();
        const unsigned long pid =
#ifdef _WIN32
            static_cast<unsigned long>(GetCurrentProcessId());
#else
            1ul;
#endif
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        sandboxRootW_ = (fs::path(baseDir) / (L"run_" + std::to_wstring(pid) + L"_" + std::to_wstring(ms))).wstring();
        fs::create_directories(sandboxRootW_, ec);
        options_.profileDirW = sandboxRootW_;
        StudioProfileStore::SetProfileRootOverride(sandboxRootW_);
    } else if (!options_.profileDirW.empty()) {
        StudioProfileStore::SetProfileRootOverride(options_.profileDirW);
    }

    commandPalette.RegisterDefaultCommands();
}

McpServerContext::~McpServerContext() {
    guiManager.CloseAllBackgroundGuis();
    tutorSidecar.Stop();
    lanServer.Stop();
    calcIpc.Close();
    speechSynth.Shutdown();
    if (options_.sandboxMode && !sandboxRootW_.empty()) {
        std::error_code ec;
        fs::remove_all(fs::path(sandboxRootW_), ec);
    }
}

std::wstring McpServerContext::EffectiveProfileRootW() const {
    if (!options_.profileDirW.empty()) return options_.profileDirW;
    return StudioProfileStore::DefaultProfileRootDir();
}

std::wstring McpServerContext::EffectiveArtifactsDirW() const {
    std::error_code ec;
    fs::path dir = fs::path(EffectiveProfileRootW()) / L"mcp_artifacts";
    fs::create_directories(dir, ec);
    return dir.wstring();
}

std::shared_ptr<SessionEntry> McpServerContext::CreateSession(
    const std::string& titleUtf8,
    DocumentMode mode,
    PaperPattern bg,
    std::string optionalCustomId)
{
    auto entry = std::make_shared<SessionEntry>();
    entry->doc.ResetToBlank(mode);
    entry->doc.SetTitle(titleUtf8.empty() ? "Cuaderno MCP" : titleUtf8);
    if (mode == DocumentMode::InfiniteCanvas) {
        entry->doc.SetCanvasBackground(bg);
    } else if (!entry->doc.MutablePages().empty()) {
        entry->doc.MutablePages()[0].background = bg;
    }
    entry->doc.RebuildLayout();
    entry->flexcil.SetPopupNoteVisible(false);
    entry->flexcil.SetGestureModeActive(false);
    entry->camera.zoom = 1.0;
    entry->camera.panX = -16.0;
    entry->camera.panY = -16.0;

    std::lock_guard<std::mutex> lk(sessionMtx_);
    std::string id = optionalCustomId.empty()
        ? ("sess_" + std::to_string(nextSessionSeq_++))
        : std::move(optionalCustomId);
    while (sessions_.count(id) != 0) {
        id = "sess_" + std::to_string(nextSessionSeq_++);
    }
    entry->sessionId = id;
    sessions_[id] = entry;
    activeSessionId_ = id;
    return entry;
}

std::shared_ptr<SessionEntry> McpServerContext::FindSession(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lk(sessionMtx_);
    if (sessionId.empty()) {
        auto it = sessions_.find(activeSessionId_);
        return (it != sessions_.end()) ? it->second : nullptr;
    }
    auto it = sessions_.find(sessionId);
    return (it != sessions_.end()) ? it->second : nullptr;
}

std::shared_ptr<SessionEntry> McpServerContext::GetOrCreateSession(const json& args) {
    const std::string reqId = args.value("session_id", "");
    if (auto existing = FindSession(reqId)) return existing;
    return CreateSession("Cuaderno MCP", DocumentMode::PaginatedPdf, PaperPattern::Grid, reqId);
}

bool McpServerContext::CloseSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lk(sessionMtx_);
    const std::string key = sessionId.empty() ? activeSessionId_ : sessionId;
    auto it = sessions_.find(key);
    if (it == sessions_.end()) return false;
    sessions_.erase(it);
    if (activeSessionId_ == key) {
        activeSessionId_ = sessions_.empty() ? "" : sessions_.begin()->first;
    }
    return true;
}

bool McpServerContext::SwitchActiveSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lk(sessionMtx_);
    if (sessions_.count(sessionId) == 0) return false;
    activeSessionId_ = sessionId;
    return true;
}

std::string McpServerContext::ActiveSessionId() const {
    std::lock_guard<std::mutex> lk(sessionMtx_);
    return activeSessionId_;
}

std::vector<std::string> McpServerContext::ListSessionIds() const {
    std::lock_guard<std::mutex> lk(sessionMtx_);
    std::vector<std::string> ids;
    ids.reserve(sessions_.size());
    for (const auto& kv : sessions_) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

void McpServerContext::PackExtensions(SessionEntry& s) const {
    DocumentExtensionBundle b{};
    b.yamlFrontmatterUtf8 = s.yamlFrontmatterUtf8;
    b.properties = s.properties;
    b.aliasesUtf8 = s.aliasesUtf8;
    b.tagsUtf8 = s.tagsUtf8;
    b.comments = &s.comments;
    b.database = &s.database;
    b.permissions = &s.permissions;
    b.automations = &s.automations;
    b.flexcil = &s.flexcil;
    b.studyDeck = &s.studyDeck;
    b.customCssUtf8 = s.customCssUtf8;
    b.activeDbViewKind = s.activeDbViewKind;
    StudioProfileStore::PackExtensionsIntoDocument(s.doc, b);
}

void McpServerContext::UnpackExtensions(SessionEntry& s) const {
    DocumentExtensionBundle b{};
    b.comments = &s.comments;
    b.database = &s.database;
    b.permissions = &s.permissions;
    b.automations = &s.automations;
    b.flexcil = &s.flexcil;
    b.studyDeck = &s.studyDeck;
    StudioProfileStore::UnpackExtensionsFromDocument(s.doc, b);
    s.yamlFrontmatterUtf8 = std::move(b.yamlFrontmatterUtf8);
    s.properties = std::move(b.properties);
    s.aliasesUtf8 = std::move(b.aliasesUtf8);
    s.tagsUtf8 = std::move(b.tagsUtf8);
    s.customCssUtf8 = std::move(b.customCssUtf8);
    s.activeDbViewKind = b.activeDbViewKind;
}

void McpServerContext::EnsureProfileLoaded() {
    std::lock_guard<std::recursive_mutex> lk(storeMtx);
    if (profileLoaded_) return;
    profileLoaded_ = true;
    const std::wstring rootW = EffectiveProfileRootW();
    StudioProfileStore::LoadAllProfileState(rootW, library, snippets, keymap, knowledgeVault, uiPrefs, nullptr);
    aiVault.LoadFromDirectory(rootW, nullptr);
    const fs::path nbVaultPath = fs::path(rootW) / L"notebook_vault.json";
    std::error_code ec;
    if (fs::exists(nbVaultPath, ec)) {
        notebookVault.LoadFromFile(nbVaultPath.wstring(), nullptr);
    }
    const fs::path ftsPath = fs::path(rootW) / L"fts_index.json";
    if (fs::exists(ftsPath, ec)) {
        ftsIndex.LoadFromFile(ftsPath.wstring(), nullptr);
    }
}

bool McpServerContext::SaveProfileIfAllowed(std::string* outErr) {
    if (options_.noProfileWrite) {
        if (outErr) *outErr = "Perfil en modo solo lectura (--no-profile-write).";
        return false;
    }
    EnsureProfileLoaded();
    std::lock_guard<std::recursive_mutex> lk(storeMtx);
    const std::wstring rootW = EffectiveProfileRootW();
    const bool ok = StudioProfileStore::SaveAllProfileState(rootW, library, snippets, keymap, knowledgeVault, uiPrefs, outErr);
    aiVault.SaveToDirectory(rootW, nullptr);
    notebookVault.SaveToFile((fs::path(rootW) / L"notebook_vault.json").wstring(), nullptr);
    ftsIndex.SaveToFile((fs::path(rootW) / L"fts_index.json").wstring(), nullptr);
    return ok;
}

D2DRenderer& McpServerContext::EnsureRenderer() {
    if (!rendererInitialized_) {
        renderer_.SetHardwareAcceleration(false); // Strict 0% GPU software offscreen rendering
        renderer_.InitializeFactories();
        rendererInitialized_ = true;
    }
    return renderer_;
}

bool McpServerContext::RenderPagePng(
    SessionEntry& s,
    std::size_t pageIdx,
    int widthPx,
    std::vector<std::uint8_t>& outPng,
    std::string* outErr)
{
    outPng.clear();
    if (s.doc.Mode() == DocumentMode::InfiniteCanvas) {
        DocRect region{ 0.0, 0.0, 960.0, 720.0 };
        if (!s.doc.CanvasObjects().empty()) {
            double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
            for (const auto& o : s.doc.CanvasObjects()) {
                minX = (std::min)(minX, o.bounds.x);
                minY = (std::min)(minY, o.bounds.y);
                maxX = (std::max)(maxX, o.bounds.Right());
                maxY = (std::max)(maxY, o.bounds.Bottom());
            }
            region = { minX - 32.0, minY - 32.0, (std::max)(320.0, (maxX - minX) + 64.0), (std::max)(240.0, (maxY - minY) + 64.0) };
        }
        const int w = std::clamp(widthPx, 120, 4096);
        const int h = std::clamp(static_cast<int>(w * (region.h / (std::max)(1.0, region.w))), 120, 4096);
        return RenderCanvasRegionPng(s, region, w, h, outPng, outErr);
    }

    if (pageIdx >= s.doc.Pages().size()) {
        if (outErr) *outErr = "Indice de pagina fuera de rango.";
        return false;
    }
    RasterImageBgra32 img;
    const int w = std::clamp(widthPx, 120, 4096);
    if (!EnsureRenderer().RenderPageOffscreen(s.doc, pageIdx, w, &s.pageCache, img) || !img.Valid()) {
        if (outErr) *outErr = "RenderPageOffscreen fallo.";
        return false;
    }
    if (!WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, outPng)) {
        if (outErr) *outErr = "No se pudo codificar PNG.";
        return false;
    }
    return true;
}

bool McpServerContext::RenderCanvasRegionPng(
    SessionEntry& s,
    const DocRect& worldRect,
    int widthPx,
    int heightPx,
    std::vector<std::uint8_t>& outPng,
    std::string* outErr)
{
    outPng.clear();
    RasterImageBgra32 img;
    const int w = std::clamp(widthPx, 64, 4096);
    const int h = std::clamp(heightPx, 64, 4096);
    if (!EnsureRenderer().RenderInfiniteCanvasOffscreen(s.doc, worldRect, w, h, img) || !img.Valid()) {
        if (outErr) *outErr = "RenderInfiniteCanvasOffscreen fallo.";
        return false;
    }
    if (!WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, outPng)) {
        if (outErr) *outErr = "No se pudo codificar PNG.";
        return false;
    }
    return true;
}

bool McpServerContext::RenderViewportPng(
    SessionEntry& s,
    int widthPx,
    int heightPx,
    bool presenterMode,
    bool darkTheme,
    std::vector<std::uint8_t>& outPng,
    std::string* outErr)
{
    outPng.clear();
    ViewportOverlayState ov{};
    ov.presenterMode = presenterMode;
    ov.darkTheme = darkTheme;
    ov.laserPointer = s.laserPointer;
    ov.rulerState = s.rulerState;
    ov.commentThreads = &s.comments;
    ov.activeDatabase = &s.database;
    ov.flexcilOverlay = &s.flexcil;
    ov.noteLocked = s.permissions.IsNoteLocked();

    RasterImageBgra32 img;
    const int w = std::clamp(widthPx, 120, 4096);
    const int h = std::clamp(heightPx, 120, 4096);
    if (!EnsureRenderer().RenderHwndViewportToOffscreen(s.doc, s.camera, w, h, &s.pageCache, img, &ov) || !img.Valid()) {
        if (outErr) *outErr = "RenderHwndViewportToOffscreen fallo.";
        return false;
    }
    if (!WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, outPng)) {
        if (outErr) *outErr = "No se pudo codificar PNG.";
        return false;
    }
    return true;
}

bool McpServerContext::WriteBytesToFile(const std::wstring& pathW, const std::vector<std::uint8_t>& bytes, std::string* outErr) const {
    try {
        const fs::path p(pathW);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (outErr) *outErr = "No se pudo abrir archivo para escribir: " + WideToUtf8(pathW);
            return false;
        }
        if (!bytes.empty()) {
            ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        ofs.flush();
        return ofs.good();
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool McpServerContext::ReadBytesFromFile(const std::wstring& pathW, std::vector<std::uint8_t>& outBytes, std::string* outErr) const {
    outBytes.clear();
    try {
        std::ifstream ifs(fs::path(pathW), std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            if (outErr) *outErr = "Archivo no encontrado: " + WideToUtf8(pathW);
            return false;
        }
        const auto sz = ifs.tellg();
        if (sz < 0) return false;
        outBytes.resize(static_cast<std::size_t>(sz));
        ifs.seekg(0, std::ios::beg);
        if (!outBytes.empty()) {
            ifs.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(outBytes.size()));
        }
        return ifs.good();
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

std::wstring McpServerContext::ResolveArtifactPathW(
    const std::string& requestedPathUtf8,
    const std::string& defaultStem,
    const std::string& extWithDot) const
{
    if (!requestedPathUtf8.empty()) {
        return Utf8ToWide(requestedPathUtf8);
    }
    static std::atomic<std::uint64_t> sSeq{ 0 };
    const std::uint64_t seq = ++sSeq;
    const fs::path dir(EffectiveArtifactsDirW());
    return (dir / Utf8ToWide(defaultStem + "_" + std::to_string(seq) + extWithDot)).wstring();
}

void McpServerContext::SyncRedactorSecrets(SecretRedactor& redactor) {
    EnsureProfileLoaded();
    {
        std::lock_guard<std::recursive_mutex> lk(storeMtx);
        for (const auto& p : aiVault.Providers()) {
            if (!p.apiKeyUtf8.empty()) redactor.RegisterSecret(p.apiKeyUtf8);
        }
        if (!uiPrefs.llmConfig.apiKeyUtf8.empty()) {
            redactor.RegisterSecret(uiPrefs.llmConfig.apiKeyUtf8);
        }
    }
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
    if (geminiStore_) {
        const auto& t = geminiStore_->Tokens();
        if (!t.accessTokenUtf8.empty()) redactor.RegisterSecret(t.accessTokenUtf8);
        if (!t.refreshTokenUtf8.empty()) redactor.RegisterSecret(t.refreshTokenUtf8);
    }
#endif
}

#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
GeminiNotebook::GeminiNotebookSyncStore& McpServerContext::EnsureGeminiStore() {
    std::lock_guard<std::recursive_mutex> lk(storeMtx);
    if (!geminiStore_) {
        const std::wstring dirW = (fs::path(EffectiveProfileRootW()) / L"gemini_notebook").wstring();
        geminiStore_ = std::make_unique<GeminiNotebook::GeminiNotebookSyncStore>(dirW);
        geminiStore_->LoadAll(nullptr);
    }
    return *geminiStore_;
}
#endif

#ifdef DOC_STUDIO_HAS_ACADEMIC
TecDigital::TecAcademicStore& McpServerContext::EnsureTecStore() {
    std::lock_guard<std::recursive_mutex> lk(storeMtx);
    if (!tecStore_) {
        const std::string rootUtf8 = WideToUtf8((fs::path(EffectiveProfileRootW()) / L"tec_digital").wstring());
        tecStore_ = std::make_unique<TecDigital::TecAcademicStore>(rootUtf8);
        tecStore_->LoadSnapshotFromDisk(nullptr);
        tecStore_->LoadGlobalScheduleIndexFromDisk(2026, nullptr);
        tecStore_->TryRestoreVaultSession();
    }
    return *tecStore_;
}
#endif

bool McpServerContext::IsTecMutationPolicyAllowed(std::string* outReason) const {
    if (options_.readonlyTec) {
        if (outReason) *outReason = "Servidor MCP iniciado con --readonly-tec (mutaciones TEC Digital deshabilitadas).";
        return false;
    }
    const std::string mask = GetEnvString("MUTATION_MASK");
    if (mask == "0" || ToLowerAscii(mask) == "false" || ToLowerAscii(mask) == "off") {
        if (outReason) *outReason = "MUTATION_MASK=0 activo en el entorno (mutaciones TEC Digital bloqueadas).";
        return false;
    }
    return true;
}

bool McpServerContext::ResetSandbox(std::string* outErr) {
    if (!options_.sandboxMode) {
        if (outErr) *outErr = "sandbox_reset solo esta permitido cuando el servidor se ejecuta con --sandbox.";
        return false;
    }
    guiManager.CloseAllBackgroundGuis();
    {
        std::lock_guard<std::mutex> lk(sessionMtx_);
        sessions_.clear();
        activeSessionId_.clear();
    }
    {
        std::lock_guard<std::recursive_mutex> lk(storeMtx);
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        geminiStore_.reset();
#endif
#ifdef DOC_STUDIO_HAS_ACADEMIC
        tecStore_.reset();
#endif
        library = LibraryContentViewsManager{};
        snippets = ReusableElementLibrary{};
        knowledgeVault = KnowledgeVault{};
        notebookVault = NotebookVault{};
        ftsIndex = VaultFullTextIndex{};
        profileLoaded_ = false;
    }
    std::error_code ec;
    if (!sandboxRootW_.empty()) {
        fs::remove_all(fs::path(sandboxRootW_), ec);
        fs::create_directories(fs::path(sandboxRootW_), ec);
    }
    return true;
}

// ---------------- McpServer ----------------

McpServer::McpServer(McpServerOptions opts)
    : ctx_(std::move(opts)), router_("cuadernu", "2.1.0")
{
    RegisterDocumentAndCanvasTools(router_, ctx_);
    RegisterKnowledgeAndSecurityTools(router_, ctx_);
    RegisterInteropAndStemTools(router_, ctx_);
    RegisterAcademicAndAiTools(router_, ctx_);
    RegisterCaptureGuiAndTestTools(router_, ctx_);
    RegisterResourcesAndPrompts();
    ctx_.SyncRedactorSecrets(router_.Redactor());
}

void McpServer::RegisterResourcesAndPrompts() {
    // 14 MCP Resources & Templates (cuadernu://...)
    router_.RegisterResource({ "cuadernu://capabilities", "Capacidades CuadernU", "Mapa completo de herramientas, comandos y estado del entorno.", "application/json" });
    router_.RegisterResource({ "cuadernu://sessions", "Sesiones Abiertas", "Lista de sesiones .cudoc abiertas en el servidor MCP.", "application/json" });
    router_.RegisterResource({ "cuadernu://library/index", "Indice de Biblioteca", "Carpetas, cuadernos, etiquetas y vistas de la biblioteca de CuadernU.", "application/json" });
    router_.RegisterResource({ "cuadernu://vault/notes", "Notas de Boveda", "Notas Markdown activas con propiedades y etiquetas.", "application/json" });
    router_.RegisterResource({ "cuadernu://vault/graph", "Grafo de Conocimiento", "Nodos 2D y enlaces bidireccionales del grafo de conocimiento.", "application/json" });
    router_.RegisterResource({ "cuadernu://tec/snapshot", "Instantanea TEC Digital", "Resumen del snapshot local de cursos, evaluaciones y expediente TEC Digital.", "application/json" });
    router_.RegisterResource({ "cuadernu://tec/radar", "Radar de Cambios TEC", "Eventos recientes detectados por el Radar de TEC Digital.", "application/json" });
    router_.RegisterResource({ "cuadernu://tec/catalog", "Catalogo Global TEC", "Catalogo unificado de archivos de todos los espacios TEC Digital.", "application/json" });
    router_.RegisterResource({ "cuadernu://tec/horarios/index", "Indice Global HorariosTEC", "Resumen del indice global de horarios de todas las escuelas del TEC.", "application/json" });
    router_.RegisterResource({ "cuadernu://gemini/inventory", "Inventario Gemini Notebook", "Cuadernos mapeados y fuentes sincronizadas con Gemini Notebook / NotebookLM.", "application/json" });
    router_.RegisterResource({ "cuadernu://gui/instances", "Instancias GUI CuadernU", "Instancias GUI ocultas en segundo plano y ventanas vivas conectables.", "application/json" });

    router_.RegisterResourceTemplate({ "cuadernu://session/{id}/snapshot", "Snapshot de Sesion", "Estado JSON completo (paginas, capas, objetos, marcadores) de la sesion.", "application/json" });
    router_.RegisterResourceTemplate({ "cuadernu://session/{id}/markdown", "Markdown de Sesion", "Representacion Markdown limpia de la sesion .cudoc.", "text/markdown" });
    router_.RegisterResourceTemplate({ "cuadernu://session/{id}/page/{n}/png", "Captura PNG de Pagina", "Imagen PNG renderizada offscreen (0% GPU) de la pagina n (0-based).", "image/png" });

    router_.SetResourceReader([this](const std::string& uri, McpResourceContent& out, std::string& outErr) -> bool {
        out.uri = uri;
        auto emitToolJson = [&](const McpToolResult& r) -> bool {
            if (r.isError) {
                outErr = r.errorMessageUtf8.empty() ? ("Error leyendo recurso: " + uri) : r.errorMessageUtf8;
                return false;
            }
            out.mimeType = "application/json";
            out.textUtf8 = r.data.dump(2);
            return true;
        };
        if (uri == "cuadernu://capabilities") return emitToolJson(router_.CallToolDirect("cuadernu_capabilities", json::object()));
        if (uri == "cuadernu://sessions") return emitToolJson(router_.CallToolDirect("cuadernu_session", { { "action", "list" } }));
        if (uri == "cuadernu://library/index") return emitToolJson(router_.CallToolDirect("cuadernu_library", { { "action", "list_items" } }));
        if (uri == "cuadernu://vault/notes") return emitToolJson(router_.CallToolDirect("cuadernu_knowledge", { { "action", "note_list" } }));
        if (uri == "cuadernu://vault/graph") return emitToolJson(router_.CallToolDirect("cuadernu_knowledge", { { "action", "graph_global" } }));
        if (uri == "cuadernu://tec/snapshot") return emitToolJson(router_.CallToolDirect("cuadernu_tec_academic", { { "action", "get_snapshot" } }));
        if (uri == "cuadernu://tec/radar") return emitToolJson(router_.CallToolDirect("cuadernu_tec_academic", { { "action", "get_snapshot" }, { "section", "radar" } }));
        if (uri == "cuadernu://tec/catalog") return emitToolJson(router_.CallToolDirect("cuadernu_tec_academic", { { "action", "global_file_catalog" } }));
        if (uri == "cuadernu://tec/horarios/index") return emitToolJson(router_.CallToolDirect("cuadernu_tec_horarios", { { "action", "search_global_index" }, { "max_results", 20 } }));
        if (uri == "cuadernu://gemini/inventory") return emitToolJson(router_.CallToolDirect("cuadernu_list_sync_inventory", json::object()));
        if (uri == "cuadernu://gui/instances") return emitToolJson(router_.CallToolDirect("cuadernu_gui", { { "action", "list_instances" } }));

        // Templates: cuadernu://session/{id}/...
        const std::string prefix = "cuadernu://session/";
        if (uri.rfind(prefix, 0) == 0) {
            const std::string rest = uri.substr(prefix.size());
            const std::size_t slash = rest.find('/');
            if (slash == std::string::npos) {
                outErr = "URI de sesion mal formado.";
                return false;
            }
            const std::string sid = rest.substr(0, slash);
            const std::string sub = rest.substr(slash + 1);
            auto sess = ctx_.FindSession(sid);
            if (!sess) {
                outErr = "Sesion no encontrada: " + sid;
                return false;
            }
            if (sub == "snapshot") {
                const auto r = router_.CallToolDirect("cuadernu_session", { { "action", "snapshot" }, { "session_id", sid } });
                out.mimeType = "application/json";
                out.textUtf8 = r.data.dump(2);
                return !r.isError;
            }
            if (sub == "markdown") {
                out.mimeType = "text/markdown";
                out.textUtf8 = RenderDocumentAsMarkdownSummary(sess->doc);
                return true;
            }
            if (sub.rfind("page/", 0) == 0 && sub.size() > 9 && sub.compare(sub.size() - 4, 4, "/png") == 0) {
                const std::string numStr = sub.substr(5, sub.size() - 9);
                const std::size_t pIdx = static_cast<std::size_t>((std::max)(0, std::atoi(numStr.c_str())));
                std::vector<std::uint8_t> png;
                if (!ctx_.RenderPagePng(*sess, pIdx, 1024, png, &outErr)) return false;
                out.mimeType = "image/png";
                out.isBlob = true;
                out.blobBytes = std::move(png);
                return true;
            }
        }
        outErr = "Recurso desconocido: " + uri;
        return false;
    });

    // 6 MCP Prompts
    auto safeArgStr = [](const json& a, const char* key, const std::string& def) -> std::string {
        if (a.is_object() && a.contains(key) && a[key].is_string()) {
            const std::string v = a[key].get<std::string>();
            if (!v.empty()) return v;
        }
        return def;
    };

    router_.RegisterPrompt({
        "organize_tec_semester",
        "Organiza el semestre en TEC Digital: sincroniza cursos, evaluaciones, Radar de cambios, biblioteca de cuadernos y calendario.",
        { { "period_hint", "Periodo academico opcional (ej. S-2-2026)", false } },
        [safeArgStr](const json& args) -> json {
            const std::string period = safeArgStr(args, "period_hint", "semestre actual");
            return {
                { "description", "Flujo guiado para organizar un semestre universitario con TEC Digital y CuadernU." },
                { "messages", json::array({
                    { { "role", "user" }, { "content", { { "type", "text" }, { "text",
                        "Organiza mi semestre (" + period + ") usando CuadernU:\n"
                        "1. Consulta cuadernu_tec_academic(action='status') y cuadernu_tec_academic(action='get_snapshot', section='communities').\n"
                        "2. Revisa cuadernu_tec_academic(action='get_snapshot', section='radar') y cuadernu_tec_academic(action='global_file_catalog').\n"
                        "3. Crea carpetas por curso con cuadernu_library y notas de resumen en cuadernu_knowledge.\n"
                        "4. Registra entregas proximas en cuadernu_database con vista Kanban (view_grouped) y Calendario (calendar).\n"
                        "5. No ejecutes ninguna mutacion en cuadernu_tec_mutation sin mi confirmacion explicita." } } } }
                }) }
            };
        }
    });

    router_.RegisterPrompt({
        "review_and_annotate_pdf",
        "Revisa un PDF academico o tecnico, extrae texto, agrega subrayados/comentarios/enlaces Flexcil y captura vista previa.",
        { { "pdf_path", "Ruta absoluta al archivo PDF a revisar", true } },
        [safeArgStr](const json& args) -> json {
            const std::string pdfPath = safeArgStr(args, "pdf_path", "");
            return {
                { "description", "Flujo de revision y anotacion de PDF en segundo plano." },
                { "messages", json::array({
                    { { "role", "user" }, { "content", { { "type", "text" }, { "text",
                        "Revisa y anota el PDF '" + pdfPath + "' en segundo plano:\n"
                        "1. Inspecciona con cuadernu_pdf_review(action='inspect_pdf') e importa con cuadernu_interop_import(action='insert_printout').\n"
                        "2. Agrega anotaciones clave con cuadernu_draw e hilos en cuadernu_pdf_review(action='comment_create_thread').\n"
                        "3. Captura las paginas anotadas con cuadernu_capture(action='page_png') y verifica su legibilidad con cuadernu_test(action='ocr_verify_capture')." } } } }
                }) }
            };
        }
    });

    router_.RegisterPrompt({
        "create_stem_presentation",
        "Crea una presentacion STEM 16:9 con formulas 2D, graficas, circuitos, notas privadas del docente y exporta a PPTX.",
        { { "topic", "Tema principal de la presentacion", true } },
        [safeArgStr](const json& args) -> json {
            const std::string topic = safeArgStr(args, "topic", "Ingenieria STEM");
            return {
                { "description", "Creacion de presentacion STEM editable en CuadernU." },
                { "messages", json::array({
                    { { "role", "user" }, { "content", { { "type", "text" }, { "text",
                        "Crea una presentacion 16:9 sobre '" + topic + "':\n"
                        "1. Inicia con cuadernu_presentation(action='create_deck', aspect='16:9').\n"
                        "2. Agrega formulas con cuadernu_stem_math(action='add_formula_node'), graficas con cuadernu_stem_math(action='add_plot_node') y circuitos con cuadernu_stem_circuit(action='add_circuit_node').\n"
                        "3. Coloca notas privadas del orador con cuadernu_presentation(action='set_speaker_notes').\n"
                        "4. Exporta a .pptx con cuadernu_presentation(action='export_pptx') y verifica con cuadernu_capture(action='page_png')." } } } }
                }) }
            };
        }
    });

    router_.RegisterPrompt({
        "sync_course_to_notebooklm",
        "Planifica y sincroniza los materiales de un curso (PDF, DOCX, PPTX, apuntes .cudoc) hacia Gemini Notebook / NotebookLM.",
        { { "course_name", "Nombre o codigo del curso", true }, { "directory_path", "Carpeta local con materiales", false } },
        [safeArgStr](const json& args) -> json {
            const std::string course = safeArgStr(args, "course_name", "Curso");
            const std::string dir = safeArgStr(args, "directory_path", "");
            return {
                { "description", "Sincronizacion incremental SHA-256 hacia Gemini Notebook / NotebookLM." },
                { "messages", json::array({
                    { { "role", "user" }, { "content", { { "type", "text" }, { "text",
                        "Sincroniza el curso '" + course + "' (" + dir + ") con Gemini Notebook:\n"
                        "1. Verifica estado con cuadernu_gemini_notebook(action='status').\n"
                        "2. Ejecuta dry-run incremental con cuadernu_plan_tec_course_sync.\n"
                        "3. Convierte apuntes .cudoc a Markdown con cuadernu_export_cudoc_to_markdown y sincroniza con cuadernu_sync_sources." } } } }
                }) }
            };
        }
    });

    router_.RegisterPrompt({
        "visual_regression_audit",
        "Ejecuta auditoria visual completa en segundo plano (GUI oculta sin robar foco, ciclo DPI, diff de pixeles y CTest).",
        { { "target_theme", "Tema a auditar: light, dark o high-contrast", false } },
        [safeArgStr](const json& args) -> json {
            const std::string theme = safeArgStr(args, "target_theme", "light");
            return {
                { "description", "Auditoria visual y QA automatizado en segundo plano." },
                { "messages", json::array({
                    { { "role", "user" }, { "content", { { "type", "text" }, { "text",
                        "Ejecuta una auditoria visual en segundo plano con tema '" + theme + "':\n"
                        "1. Lanza GUI oculta con cuadernu_gui(action='spawn_background', theme='" + theme + "').\n"
                        "2. Toma capturas con cuadernu_capture(action='gui_window_png') y verifica no-blanco con cuadernu_test(action='assert_visual_nonblank').\n"
                        "3. Ejecuta cuadernu_test(action='dump_layout_bounds'), cuadernu_test(action='run_dpi_cycle') y cuadernu_test(action='run_ctest').\n"
                        "4. Cierra la instancia oculta con cuadernu_gui(action='close_background')." } } } }
                }) }
            };
        }
    });

    router_.RegisterPrompt({
        "tutor_guided_problem_session",
        "Inicia una sesion guiada en la Pizarra del Tutor Audiovisual, verifica pasos y hornea la solucion al cuaderno .cudoc.",
        { { "problem_topic", "Problema o tema de Fisica/Ingenieria", true } },
        [safeArgStr](const json& args) -> json {
            const std::string topic = safeArgStr(args, "problem_topic", "Fisica 3");
            return {
                { "description", "Sesion paso a paso con la Pizarra Tutor Audiovisual." },
                { "messages", json::array({
                    { { "role", "user" }, { "content", { { "type", "text" }, { "text",
                        "Prepara una sesion guiada sobre '" + topic + "':\n"
                        "1. Inicia el sidecar con cuadernu_tutor_whiteboard(action='sidecar_start') y busca ejercicios con cuadernu_tutor_whiteboard(action='banco_search', query='" + topic + "').\n"
                        "2. Aplica beats con cuadernu_tutor_whiteboard(action='session_apply_beat') y verifica pasos con cuadernu_stem_math(action='check_step') o cuadernu_tutor_whiteboard(action='verify_step').\n"
                        "3. Hornea la pizarra al documento con cuadernu_tutor_whiteboard(action='bake_to_document') y captura PNG con cuadernu_capture(action='page_png')." } } } }
                }) }
            };
        }
    });
}

std::string McpServer::HandleJsonRpcLine(const std::string& lineUtf8) {
    ctx_.SyncRedactorSecrets(router_.Redactor());
    return router_.HandleJsonRpcLine(lineUtf8);
}

McpToolResult McpServer::CallTool(const std::string& toolName, const json& args) {
    ctx_.SyncRedactorSecrets(router_.Redactor());
    return router_.CallToolDirect(toolName, args);
}

int McpServer::RunStdio() {
    ctx_.SyncRedactorSecrets(router_.Redactor());
    return router_.RunStdioServer();
}

int McpServer::RunHttp(std::uint16_t port, const std::string& bearerTokenUtf8, std::atomic<bool>* stopFlag) {
    ctx_.SyncRedactorSecrets(router_.Redactor());
    return router_.RunHttpServer(port, bearerTokenUtf8, stopFlag);
}

std::string McpServer::GenerateAgentMcpConfigJson(
    const std::wstring& cuadernuExePathW,
    const std::string& optionalGoogleMcpEndpointUtf8)
{
    const std::string exeUtf8 = WideToUtf8(cuadernuExePathW);
    json servers = {
        { "cuadernu", {
            { "command", exeUtf8.empty() ? "CuadernU.exe" : exeUtf8 },
            { "args", json::array({ "--mcp-stdio" }) }
        } }
    };
    if (!optionalGoogleMcpEndpointUtf8.empty()) {
        servers["google-gemini-notebook"] = {
            { "url", optionalGoogleMcpEndpointUtf8 },
            { "transport", "streamable-http" }
        };
    }
    return json({ { "mcpServers", std::move(servers) } }).dump(2);
}

} // namespace DocStudio::Mcp
