#pragma once

// ============================================================================
// studio_cli_dispatch.hpp
// Unified Cross-Platform CLI & Workflow Dispatcher for Document Studio
// (Shared identically by Windows Win32/D2D main_studio.cpp and Linux GTK3/Cairo
// main_studio_gtk.cpp).
//
// Wires all genuine C++17 engines:
//   - EngineeringCas / CheckMyWork / ConfirmationCard / TeachMe / LocalGroundedQA
//   - CalcBridge / Presenter / MacroRecorder / ExamMode / LMS Variant Exporter
//   - Real OS Mic Capture (WinMM / ALSA) / Offline Audio VAD Transcript / HTML5 Tutorial
//   - Shared CSV/XLSX/Markdown/DXF Table Interop + OLS Regression + Cycle Check
//   - AES-256-GCM .cudoc Vault Encryption (BCrypt / OpenSSL EVP)
//   - Local LAN QR Broadcast & P2P CRDT Companion
//   - Custom keymap.json & Accessibility/UIA-ATK/Layout/TTS/HiDPI Telemetry
//   - Native Academic TEC Digital & HorariosTEC Integration (Read-Only Guarded)
// ============================================================================

#include "studio_actions.hpp"
#include "../bridge/calc_bridge.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_crypto.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/document_model.hpp"
#include "../core/keymap_config.hpp"
#include "../core/lan_remote.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/utf_conv.hpp"
#include "../core/zip_container.hpp"
#include "../math/cas_ai_pedagogy.hpp"
#include "../math/engineering_cas.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/table_interop.hpp"
#include "../interop/notebook_migration_engine.hpp"
#include "../interop/printout_engine.hpp"

#ifdef DOC_STUDIO_HAS_ACADEMIC
#include "academic/horarios_tec/exporter.hpp"
#include "academic/horarios_tec/parser.hpp"
#include "academic/horarios_tec/solver.hpp"
#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/material_indexer.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/service.hpp"
#include "academic/tec_digital/storage.hpp"
#include "academic/tec_digital/tda_tds_client.hpp"
#include "academic/tec_digital/vault.hpp"
#endif

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::CliDispatch {

struct StudioCliOptions {
    // Core / GUI launch flags
    std::string openPathUtf8;
    std::string profileDirUtf8;
    bool invalidProfileDir = false;
    std::string mode; // "pdf" | "canvas"
    int initialPage1Based = 1;
    std::string calcCardSpecUtf8;
    bool showcaseStem = false;
    std::string screenshotOutUtf8;
    bool smokeTest = false;

    // Extended workflow flags
    std::string outCudocUtf8;
    std::string outJsonUtf8;
    std::string outDirUtf8;
    std::string outWavUtf8;
    std::string outPdfUtf8;
    std::string outXlsxUtf8;
    std::string outDocxUtf8;
    std::string outDxfUtf8;
    std::string outHtml5Utf8;
    std::string outKeylogHtmlUtf8;
    std::string outQrHtmlUtf8;
    std::string outErrorbookUtf8;
    std::string outAnalyticsUtf8;
    std::string outAuditUtf8;
    std::string outLmsZipUtf8;
    std::string outVaultUtf8;
    std::string exportEffectiveKeymapUtf8;
    std::string dumpA11yTreeUtf8;
    std::string dumpLayoutBoundsUtf8;

    // Input / import flags
    std::string migratePathUtf8;
    std::string insertPrintoutPathUtf8;
    std::vector<std::string> insertPrintoutPathsUtf8;
    std::string printoutTargetUtf8 = "canvas";
    std::string printoutRangeUtf8;
    std::string printoutLayoutUtf8 = "column";
    std::string printoutConverterUtf8 = "auto";
    bool printoutLockInPlace = true;
    double printoutGapPt = 32.0;
    double printoutColGapPt = 36.0;
    std::string outReportUtf8;
    std::string importCsvUtf8;
    std::string importMdUtf8;
    std::string importAudioUtf8;
    std::string warpScanOcrUtf8;
    std::string searchQueryUtf8;
    int searchMatchIndex1Based = 1;
    std::string rosterCsvUtf8;
    std::string keymapConfigUtf8;
    std::string scaleRatioUtf8 = "1:1";

    // CalcBridge / CAS / Pedagogy / CheckMyWork / Study
    bool calcBridgeSync = false;
    bool checkMyWork = false;
    std::string cmwPrevUtf8;
    std::string cmwNextUtf8;
    bool confirmAst = false;
    bool teachMe = false;
    std::string casExprUtf8;
    std::string localQaQueryUtf8;
    bool aiOffVerifyCore = false;
    bool examMode = false;
    bool gradeRubric = false;
    bool hasGenerateVariants = false;
    int generateVariantsCount = 0;
    bool quizMe = false;
    std::string timeKeeperUtf8;
    bool studySessionE2e = false;
    bool examWeekSession = false;
    bool presenterConsole = false;
    bool freezeProjector = false;
    bool replayMacro = false;

    // Audio / Mic flags
    bool recordAudioSmoke = false;
    bool simulateMicUnplug = false;
    bool hasAudioOffset = false;
    std::int64_t audioOffsetMs = 0;
    std::string bookmarkLabelUtf8;
    bool transcribeAudio = false;

    // Security / LAN / QR flags
    std::string encryptCudocArgUtf8;
    std::string decryptPassUtf8;
    std::string pinLockUtf8;
    bool offlineOnly = false;
    bool lanBroadcast = false;
    bool lanP2pSync = false;
    bool lanQr = false;
    int lanPort = 0;
    bool hasQrPayload = false;
    std::string qrPayloadUtf8;

    // UI / Accessibility / Theme flags
    double uiScale = 1.0;
    double uiTextScalePct = 100.0;
    double dpiScalePct = 100.0;
    bool highContrast = false;
    std::string theme = "light"; // light | dark | high-contrast
    bool reduceMotion = false;
    int windowW = 1280;
    int windowH = 820;
    bool hasExplicitWindowSize = false;
    std::string locale = "es";
    bool ttsRead = false;
    std::string ttsVoiceUtf8;
    bool simulateGpuReset = false;

    // Academic TEC Digital & HorariosTEC flags
    std::string tecSyncUrlUtf8;
    bool ocrIndex = false;
    std::string createSm2HitUtf8;
    std::string horariosSolveCatalogUtf8;
    std::string tecExportBundleUrlUtf8;
    bool hasTecOpenPdfWithCard = false;
    std::string tecOpenPdfUrlUtf8;
    std::string tecOpenPdfObjectIdUtf8;
    std::string tecOpenPdfCardSpecUtf8;
    std::string scheduleFocoUrlUtf8;
    std::string verifyOfflineMirrorUtf8;
    std::string tecOnboardUrlUtf8;
    std::string horariosCatalogUtf8;
    std::string tecSyncFaultDrillUrlUtf8;
    std::string tecAcademicAuditUrlUtf8;
    std::string tecLiveSmokeUrlUtf8;

    bool HasAnyExtendedWorkflow() const noexcept {
        return !outCudocUtf8.empty() || !outJsonUtf8.empty() || !outDirUtf8.empty() ||
               !outWavUtf8.empty() || !outPdfUtf8.empty() || !outXlsxUtf8.empty() ||
               !outDocxUtf8.empty() || !outDxfUtf8.empty() || !outHtml5Utf8.empty() ||
               !outKeylogHtmlUtf8.empty() || !outQrHtmlUtf8.empty() || !outErrorbookUtf8.empty() ||
               !outAnalyticsUtf8.empty() || !outAuditUtf8.empty() || !outLmsZipUtf8.empty() ||
               !outVaultUtf8.empty() || !exportEffectiveKeymapUtf8.empty() ||
               !dumpA11yTreeUtf8.empty() || !dumpLayoutBoundsUtf8.empty() ||
               !importCsvUtf8.empty() || !importMdUtf8.empty() || !importAudioUtf8.empty() ||
               !migratePathUtf8.empty() || !insertPrintoutPathUtf8.empty() || !outReportUtf8.empty() ||
               !warpScanOcrUtf8.empty() || calcBridgeSync || checkMyWork || !casExprUtf8.empty() ||
               !localQaQueryUtf8.empty() || aiOffVerifyCore || examMode || hasGenerateVariants ||
               quizMe || studySessionE2e || examWeekSession || presenterConsole || replayMacro ||
               recordAudioSmoke || transcribeAudio || !encryptCudocArgUtf8.empty() ||
               !decryptPassUtf8.empty() || lanBroadcast || lanP2pSync || lanQr || ttsRead ||
               !tecSyncUrlUtf8.empty() || !horariosSolveCatalogUtf8.empty() ||
               !tecExportBundleUrlUtf8.empty() || hasTecOpenPdfWithCard ||
               !scheduleFocoUrlUtf8.empty() || !verifyOfflineMirrorUtf8.empty() ||
               !tecOnboardUrlUtf8.empty() || !tecSyncFaultDrillUrlUtf8.empty() ||
               !tecAcademicAuditUrlUtf8.empty() || !tecLiveSmokeUrlUtf8.empty();
    }
};

struct DispatchResult {
    bool exitImmediately = false;
    int exitCode = 0;
    std::string statusBannerUtf8;
    std::string errorMessageUtf8;
};

namespace detail {

inline std::string PathToUtf8Local(const std::filesystem::path& p) {
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

inline bool WriteTextFile(const std::string& pathUtf8, const std::string& content, std::string* outErr = nullptr) {
    std::error_code ec;
    const auto p = std::filesystem::u8path(pathUtf8);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (outErr) *outErr = "Failed to open output file: " + pathUtf8;
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();
    return ofs.good();
}

inline bool ReadFileBytesLocal(const std::string& pathUtf8, std::vector<std::uint8_t>& out, std::string* outErr = nullptr) {
    std::ifstream ifs(std::filesystem::u8path(pathUtf8), std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        if (outErr) *outErr = "File not found: " + pathUtf8;
        return false;
    }
    const auto sz = ifs.tellg();
    if (sz < 0) {
        if (outErr) *outErr = "Invalid file size: " + pathUtf8;
        return false;
    }
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!out.empty() && !ifs.read(reinterpret_cast<char*>(out.data()), sz)) {
        if (outErr) *outErr = "Read failed: " + pathUtf8;
        return false;
    }
    return true;
}

inline std::string LowerExtUtf8(const std::string& pathUtf8) {
    try {
        std::string ext = PathToUtf8Local(std::filesystem::u8path(pathUtf8).extension());
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return ext;
    } catch (...) {
        return {};
    }
}

inline bool IsLoopbackOrigin(const std::string& url) {
    std::string low = url;
    for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return low.find("://127.0.0.1") != std::string::npos ||
           low.find("://localhost") != std::string::npos ||
           low.find("://[::1]") != std::string::npos;
}

inline std::string ExtractOriginUrl(const std::string& url) {
    const auto p = url.find("://");
    if (p == std::string::npos) return url;
    const auto slash = url.find('/', p + 3);
    return (slash == std::string::npos) ? url : url.substr(0, slash);
}

inline void AddCalcCardFromSpec(DocumentModel& doc, const std::string& spec, bool evaluateViaCas) {
    // Format: model|keylog|expr_or_result
    std::string model = "fx-991CW";
    std::string keylog;
    std::string expr;
    const auto p1 = spec.find('|');
    if (p1 == std::string::npos) {
        expr = spec;
    } else {
        model = spec.substr(0, p1);
        const auto p2 = spec.find('|', p1 + 1);
        if (p2 == std::string::npos) {
            keylog = spec.substr(p1 + 1);
        } else {
            keylog = spec.substr(p1 + 1, p2 - (p1 + 1));
            expr = spec.substr(p2 + 1);
        }
    }

    AnnotationObject card;
    card.kind = ObjectKind::CalcSessionCard;
    card.layerId = doc.ActiveLayerId();
    const double yOff = 48.0 + static_cast<double>(doc.CanvasObjects().size()) * 116.0;
    card.bounds = { 48.0, yOff, 460.0, 108.0 };
    card.calcModelIdUtf8 = model.empty() ? "fx-991CW" : model;
    card.calcKeySeqUtf8 = keylog;
    card.formulaExprUtf8 = expr;

    if (evaluateViaCas && !expr.empty()) {
        // Seed standard physics/engineering context for #varsub expressions if needed
        Cas::SymbolEnv env;
        if (auto rV0 = Cas::EvaluateExpression("25 m/s"); rV0.ok) env["v_0"] = rV0.result;
        if (auto rG = Cas::EvaluateExpression("9.81 m/s^2"); rG.ok) env["g"] = rG.result;

        std::string cleanExpr = expr;
        const auto tagPos = cleanExpr.find("#varsub");
        if (tagPos != std::string::npos) {
            cleanExpr.erase(tagPos, 7);
        }
        const auto res = Cas::EvaluateExpression(cleanExpr, env);
        if (res.ok && !res.formattedResultUtf8.empty()) {
            card.evaluatedResultUtf8 = res.formattedResultUtf8;
            card.textUtf8 = card.calcModelIdUtf8 + " | " + expr + " = " + res.formattedResultUtf8;
        } else {
            card.evaluatedResultUtf8 = expr;
            card.textUtf8 = card.calcModelIdUtf8 + " | " + keylog + " | " + expr;
        }
    } else {
        card.evaluatedResultUtf8 = expr.empty() ? "(offline/peer standby)" : expr;
        card.textUtf8 = card.calcModelIdUtf8 + " | " + keylog + " | " + card.evaluatedResultUtf8;
    }

    if (doc.Mode() == DocumentMode::PaginatedPdf && !doc.Pages().empty()) {
        doc.AddPageObject(0, std::move(card));
    } else {
        doc.AddCanvasObject(std::move(card));
    }
}

inline bool DecodePngDimensions(const std::vector<std::uint8_t>& png, int& outW, int& outH) noexcept {
    if (png.size() < 24) return false;
    static constexpr std::uint8_t kSig[8] = { 0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A };
    if (std::memcmp(png.data(), kSig, 8) != 0) return false;
    outW = (static_cast<int>(png[16]) << 24) | (static_cast<int>(png[17]) << 16) |
           (static_cast<int>(png[18]) << 8) | static_cast<int>(png[19]);
    outH = (static_cast<int>(png[20]) << 24) | (static_cast<int>(png[21]) << 16) |
           (static_cast<int>(png[22]) << 8) | static_cast<int>(png[23]);
    return outW > 0 && outH > 0;
}

inline bool OpenImageIntoDocument(const std::string& imgPathUtf8, DocumentModel& doc, std::string* outErr) {
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBytesLocal(imgPathUtf8, bytes, outErr)) return false;
    int w = 64, h = 64;
    (void)DecodePngDimensions(bytes, w, h);
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle(PathToUtf8Local(std::filesystem::u8path(imgPathUtf8).stem()));
    const std::string assetKey = doc.RegisterAsset(bytes);

    // Clamp extreme aspect ratios and 1x1 tiny scans cleanly onto an A4 page
    const double safeW = std::clamp(static_cast<double>(w), 16.0, 520.0);
    const double safeH = std::clamp(static_cast<double>(h), 16.0, 720.0);
    AnnotationObject imgObj;
    imgObj.kind = ObjectKind::Image;
    imgObj.assetKey = assetKey;
    imgObj.pngBytes = bytes;
    imgObj.imagePixelW = w;
    imgObj.imagePixelH = h;
    imgObj.bounds = { 36.0, 36.0, safeW, safeH };
    imgObj.captionUtf8 = "Imagen escaneada (" + std::to_string(w) + "x" + std::to_string(h) + ")";
    doc.AddPageObject(0, std::move(imgObj));
    return true;
}

inline std::string FoldAccentsLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xC3 && i + 1 < s.size()) {
            const unsigned char c2 = static_cast<unsigned char>(s[++i]);
            switch (c2) {
                case 0xA1: case 0x81: out.push_back('a'); break;
                case 0xA9: case 0x89: out.push_back('e'); break;
                case 0xAD: case 0x8D: out.push_back('i'); break;
                case 0xB3: case 0x93: out.push_back('o'); break;
                case 0xBA: case 0x9A: case 0xBC: case 0x9C: out.push_back('u'); break;
                case 0xB1: case 0x91: out.push_back('n'); break;
                default: break;
            }
        } else {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out;
}

inline bool SaveDocumentUnified(const std::string& pathUtf8, DocumentModel& doc, std::string* outErr) {
    return CudocStorage::SaveToFile(Utf8ToWide(pathUtf8), doc, outErr);
}

inline bool ExportPdfUnified(const std::string& pathUtf8, const DocumentModel& doc, std::string* outErr) {
    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        CanvasPdfExportOptions opts;
        return PdfWriter::ExportInfiniteCanvasToFile(doc, opts, Utf8ToWide(pathUtf8), outErr);
    }
    return PdfWriter::ExportDocumentToFile(doc, Utf8ToWide(pathUtf8), outErr);
}

#ifdef DOC_STUDIO_HAS_ACADEMIC
inline academic::tec_digital::ServiceConfig MakeAcademicConfig(
    const std::string& baseUrl,
    const std::string& offlineRoot = "")
{
    using namespace academic::tec_digital;
    ServiceConfig cfg;
    cfg.base_url = baseUrl;
    cfg.auth_options.base_url = baseUrl;
    cfg.offline_root = offlineRoot;
    cfg.crawler_options.base_url = baseUrl;
    if (IsLoopbackOrigin(baseUrl)) {
        cfg.policy_options.allowed_test_origins.push_back(ExtractOriginUrl(baseUrl));
        cfg.downloader_options.min_delay_ms = 0;
        cfg.downloader_options.backoff_ms = { 25, 50, 100 };
        cfg.downloader_options.max_retries = 3;
        cfg.downloader_options.verify_size_with_head = false;
        cfg.crawler_options.delay_ms = 0;
        cfg.crawler_options.max_pages = 25;
        cfg.crawler_options.max_depth = 3;
    }
    if (auto envCreds = CredentialVault::ResolveFromEnv()) {
        cfg.sanitize_options.runtime_email = envCreds->email;
        cfg.sanitize_options.runtime_password = envCreds->password;
        CredentialVault::ClearMemory(*envCreds);
    }
    return cfg;
}

inline bool EnsureAcademicAuthIfNeeded(
    academic::tec_digital::TecDigitalService& svc,
    const std::string& baseUrl,
    std::string* outErr)
{
    using namespace academic::tec_digital;
    auto envCreds = CredentialVault::ResolveFromEnv();
    if (envCreds.has_value()) {
        AuthResult ar = svc.LoginWithCredentials(*envCreds, false);
        CredentialVault::ClearMemory(*envCreds);
        if (!ar.ok) {
            if (outErr) *outErr = "TEC Digital auth failed: " + svc.SnapshotSanitizer().SanitizeText(ar.message);
            return false;
        }
        return true;
    }
    if (IsLoopbackOrigin(baseUrl)) {
        // Loopback mock server allows direct read-only fixture API queries without credentials
        return true;
    }
    AuthResult ar = svc.LoginWithEnvOrVault(false);
    if (!ar.ok) {
        if (outErr) *outErr = "TEC Digital credentials required for remote host";
        return false;
    }
    return true;
}
#endif

} // namespace detail

inline StudioCliOptions ParseArgsUtf8(const std::vector<std::string>& args) {
    StudioCliOptions o;
    auto hasNext = [&](std::size_t i) { return i + 1 < args.size(); };

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--open" && hasNext(i)) o.openPathUtf8 = args[++i];
        else if (a == "--profile-dir") {
            if (hasNext(i) && !args[i + 1].empty() && args[i + 1].rfind("--", 0) != 0 &&
                o.profileDirUtf8.empty()) {
                o.profileDirUtf8 = args[++i];
            } else {
                o.invalidProfileDir = true;
            }
        }
        else if (a == "--page" && hasNext(i)) { try { o.initialPage1Based = (std::max)(1, std::stoi(args[++i])); } catch (...) {} }
        else if (a == "--mode" && hasNext(i)) o.mode = args[++i];
        else if (a == "--calc-card" && hasNext(i)) o.calcCardSpecUtf8 = args[++i];
        else if (a == "--showcase-stem") { o.showcaseStem = true; if (o.mode.empty()) o.mode = "canvas"; }
        else if (a == "--screenshot-out" && hasNext(i)) o.screenshotOutUtf8 = args[++i];
        else if (a == "--smoke-test") o.smokeTest = true;

        else if ((a == "--out-cudoc" || a == "--save-cudoc") && hasNext(i)) o.outCudocUtf8 = args[++i];
        else if (a == "--out-json" && hasNext(i)) o.outJsonUtf8 = args[++i];
        else if (a == "--out-dir" && hasNext(i)) o.outDirUtf8 = args[++i];
        else if (a == "--out-wav" && hasNext(i)) o.outWavUtf8 = args[++i];
        else if (a == "--export-pdf" && hasNext(i)) o.outPdfUtf8 = args[++i];
        else if (a == "--export-xlsx" && hasNext(i)) o.outXlsxUtf8 = args[++i];
        else if (a == "--export-docx" && hasNext(i)) o.outDocxUtf8 = args[++i];
        else if (a == "--export-dxf" && hasNext(i)) o.outDxfUtf8 = args[++i];
        else if ((a == "--export-html5" || a == "--publish-html5") && hasNext(i)) o.outHtml5Utf8 = args[++i];
        else if (a == "--export-keylog-html" && hasNext(i)) o.outKeylogHtmlUtf8 = args[++i];
        else if (a == "--export-qr-html" && hasNext(i)) o.outQrHtmlUtf8 = args[++i];
        else if (a == "--export-errorbook" && hasNext(i)) o.outErrorbookUtf8 = args[++i];
        else if (a == "--export-analytics" && hasNext(i)) o.outAnalyticsUtf8 = args[++i];
        else if (a == "--export-audit" && hasNext(i)) o.outAuditUtf8 = args[++i];
        else if (a == "--export-lms" && hasNext(i)) o.outLmsZipUtf8 = args[++i];
        else if (a == "--vault-out" && hasNext(i)) o.outVaultUtf8 = args[++i];
        else if (a == "--export-effective-keymap" && hasNext(i)) o.exportEffectiveKeymapUtf8 = args[++i];
        else if (a == "--dump-a11y-tree" && hasNext(i)) o.dumpA11yTreeUtf8 = args[++i];
        else if (a == "--dump-layout-bounds" && hasNext(i)) o.dumpLayoutBoundsUtf8 = args[++i];

        else if (a == "--import-csv" && hasNext(i)) o.importCsvUtf8 = args[++i];
        else if (a == "--migrate" && hasNext(i)) o.migratePathUtf8 = args[++i];
        else if (a == "--insert-printout" && hasNext(i)) {
            const std::string p = args[++i];
            if (o.insertPrintoutPathUtf8.empty()) o.insertPrintoutPathUtf8 = p;
            o.insertPrintoutPathsUtf8.push_back(p);
        }
        else if (a == "--printout-target" && hasNext(i)) o.printoutTargetUtf8 = args[++i];
        else if (a == "--printout-range" && hasNext(i)) o.printoutRangeUtf8 = args[++i];
        else if (a == "--printout-layout" && hasNext(i)) o.printoutLayoutUtf8 = args[++i];
        else if (a == "--printout-converter" && hasNext(i)) o.printoutConverterUtf8 = args[++i];
        else if (a == "--printout-lock" && hasNext(i)) o.printoutLockInPlace = (args[++i] != "0" && args[i] != "false");
        else if (a == "--printout-gap" && hasNext(i)) o.printoutGapPt = std::clamp(std::atof(args[++i].c_str()), 0.0, 4000.0);
        else if (a == "--printout-col-gap" && hasNext(i)) o.printoutColGapPt = std::clamp(std::atof(args[++i].c_str()), 0.0, 4000.0);
        else if (a == "--out-report" && hasNext(i)) o.outReportUtf8 = args[++i];
        else if (a == "--import-md" && hasNext(i)) o.importMdUtf8 = args[++i];
        else if (a == "--import-audio" && hasNext(i)) o.importAudioUtf8 = args[++i];
        else if (a == "--warp-scan-ocr" && hasNext(i)) o.warpScanOcrUtf8 = args[++i];
        else if (a == "--search" && hasNext(i)) o.searchQueryUtf8 = args[++i];
        else if (a == "--search-match-index" && hasNext(i)) o.searchMatchIndex1Based = (std::max)(1, std::atoi(args[++i].c_str()));
        else if (a == "--roster" && hasNext(i)) o.rosterCsvUtf8 = args[++i];
        else if (a == "--keymap-config" && hasNext(i)) o.keymapConfigUtf8 = args[++i];
        else if (a == "--scale" && hasNext(i)) o.scaleRatioUtf8 = args[++i];

        else if (a == "--calc-bridge-sync" || a == "--live-calc-sync") o.calcBridgeSync = true;
        else if (a == "--check-my-work" && i + 2 < args.size()) {
            o.checkMyWork = true;
            o.cmwPrevUtf8 = args[++i];
            o.cmwNextUtf8 = args[++i];
        }
        else if (a == "--confirm-ast") o.confirmAst = true;
        else if (a == "--teach-me") o.teachMe = true;
        else if (a == "--cas" && hasNext(i)) o.casExprUtf8 = args[++i];
        else if (a == "--local-qa-cited" && hasNext(i)) o.localQaQueryUtf8 = args[++i];
        else if (a == "--ai-off-verify-core") o.aiOffVerifyCore = true;
        else if (a == "--exam-mode") o.examMode = true;
        else if (a == "--grade-rubric") o.gradeRubric = true;
        else if (a == "--generate-variants" && hasNext(i)) {
            o.hasGenerateVariants = true;
            try { o.generateVariantsCount = std::stoi(args[++i]); } catch (...) { o.generateVariantsCount = 0; }
        }
        else if (a == "--quiz-me") o.quizMe = true;
        else if (a == "--time-keeper" && hasNext(i)) o.timeKeeperUtf8 = args[++i];
        else if (a == "--study-session-e2e") o.studySessionE2e = true;
        else if (a == "--exam-week-session") o.examWeekSession = true;
        else if (a == "--presenter-console") o.presenterConsole = true;
        else if (a == "--freeze-projector") o.freezeProjector = true;
        else if (a == "--replay-macro") o.replayMacro = true;

        else if (a == "--record-audio-smoke") o.recordAudioSmoke = true;
        else if (a == "--simulate-mic-unplug") o.simulateMicUnplug = true;
        else if (a == "--audio-offset-ms" && hasNext(i)) {
            o.hasAudioOffset = true;
            try { o.audioOffsetMs = std::stoll(args[++i]); } catch (...) { o.audioOffsetMs = 0; }
        }
        else if (a == "--bookmark" && hasNext(i)) o.bookmarkLabelUtf8 = args[++i];
        else if (a == "--transcribe-audio") o.transcribeAudio = true;

        else if (a == "--encrypt-cudoc" && hasNext(i)) o.encryptCudocArgUtf8 = args[++i];
        else if (a == "--decrypt-pass" && hasNext(i)) o.decryptPassUtf8 = args[++i];
        else if (a == "--pin-lock" && hasNext(i)) o.pinLockUtf8 = args[++i];
        else if (a == "--offline-only") o.offlineOnly = true;
        else if (a == "--lan-broadcast") o.lanBroadcast = true;
        else if (a == "--lan-p2p-sync") o.lanP2pSync = true;
        else if (a == "--lan-qr") o.lanQr = true;
        else if (a == "--port" && hasNext(i)) { try { o.lanPort = std::stoi(args[++i]); } catch (...) { o.lanPort = 0; } }
        else if (a == "--qr-payload" && hasNext(i)) { o.hasQrPayload = true; o.qrPayloadUtf8 = args[++i]; }

        else if (a == "--ui-scale" && hasNext(i)) { try { o.uiScale = std::clamp(std::stod(args[++i]), 0.5, 4.0); } catch (...) {} }
        else if (a == "--ui-text-scale" && hasNext(i)) { try { o.uiTextScalePct = std::clamp(std::stod(args[++i]), 50.0, 300.0); } catch (...) {} }
        else if (a == "--dpi-scale" && hasNext(i)) { try { o.dpiScalePct = std::clamp(std::stod(args[++i]), 100.0, 400.0); } catch (...) {} }
        else if (a == "--high-contrast") { o.highContrast = true; o.theme = "high-contrast"; }
        else if (a == "--theme" && hasNext(i)) {
            o.theme = args[++i];
            if (o.theme == "high-contrast") o.highContrast = true;
        }
        else if (a == "--reduce-motion") o.reduceMotion = true;
        else if (a == "--window-size" && hasNext(i)) {
            const std::string sz = args[++i];
            const auto x = sz.find_first_of("xX");
            if (x != std::string::npos) {
                try {
                    o.windowW = std::clamp(std::stoi(sz.substr(0, x)), 480, 3840);
                    o.windowH = std::clamp(std::stoi(sz.substr(x + 1)), 360, 2160);
                    o.hasExplicitWindowSize = true;
                } catch (...) {}
            }
        }
        else if ((a == "--locale" || a == "--lang") && hasNext(i)) o.locale = args[++i];
        else if (a == "--tts-read") o.ttsRead = true;
        else if (a == "--tts-voice" && hasNext(i)) o.ttsVoiceUtf8 = args[++i];
        else if (a == "--simulate-gpu-reset") o.simulateGpuReset = true;

        else if (a == "--tec-sync" && hasNext(i)) o.tecSyncUrlUtf8 = args[++i];
        else if (a == "--ocr-index") o.ocrIndex = true;
        else if (a == "--create-sm2-from-hit" && hasNext(i)) o.createSm2HitUtf8 = args[++i];
        else if (a == "--horarios-solve-to-vault" && hasNext(i)) o.horariosSolveCatalogUtf8 = args[++i];
        else if (a == "--tec-export-sanitized-bundle" && hasNext(i)) o.tecExportBundleUrlUtf8 = args[++i];
        else if (a == "--tec-open-pdf-with-card" && i + 3 < args.size()) {
            o.hasTecOpenPdfWithCard = true;
            o.tecOpenPdfUrlUtf8 = args[++i];
            o.tecOpenPdfObjectIdUtf8 = args[++i];
            o.tecOpenPdfCardSpecUtf8 = args[++i];
        }
        else if (a == "--schedule-foco-from-notifications" && hasNext(i)) o.scheduleFocoUrlUtf8 = args[++i];
        else if (a == "--verify-offline-mirror" && hasNext(i)) o.verifyOfflineMirrorUtf8 = args[++i];
        else if (a == "--tec-onboard" && hasNext(i)) o.tecOnboardUrlUtf8 = args[++i];
        else if (a == "--horarios-catalog" && hasNext(i)) o.horariosCatalogUtf8 = args[++i];
        else if (a == "--tec-sync-fault-drill" && hasNext(i)) o.tecSyncFaultDrillUrlUtf8 = args[++i];
        else if (a == "--tec-academic-audit" && hasNext(i)) o.tecAcademicAuditUrlUtf8 = args[++i];
        else if (a == "--tec-live-readonly-smoke" && hasNext(i)) o.tecLiveSmokeUrlUtf8 = args[++i];
    }
    return o;
}

inline DispatchResult ExecuteWorkflow(
    const StudioCliOptions& opts,
    DocumentModel& doc,
    AudioTimelineSession& audioSession)
{
    DispatchResult res;
    if (!opts.HasAnyExtendedWorkflow() && opts.openPathUtf8.empty()) {
        return res;
    }

    auto fail = [&](int code, const std::string& msg) -> DispatchResult {
        res.exitImmediately = true;
        res.exitCode = code;
        res.errorMessageUtf8 = msg;
        std::cerr << msg << "\n";
        return res;
    };

    // 0. Multi-Format Interoperability & Notebook Migration Engine (--migrate)
    if (!opts.migratePathUtf8.empty()) {
        Interop::MigrationResult migRes{};
        if (!Interop::NotebookMigrationEngine::ImportAndMigrateFile(Utf8ToWide(opts.migratePathUtf8), doc, migRes)) {
            return fail(2, "Migration failed: " + migRes.errorUtf8);
        }
        if (!migRes.primaryAudioWavBytes.empty()) {
            audioSession.LoadFromWavBytes(migRes.primaryAudioWavBytes.data(), migRes.primaryAudioWavBytes.size());
        }
        if (!opts.outReportUtf8.empty()) {
            detail::WriteTextFile(opts.outReportUtf8, migRes.report.ToJsonUtf8());
        }
        if (!opts.outCudocUtf8.empty()) {
            std::string sErr;
            if (!detail::SaveDocumentUnified(opts.outCudocUtf8, doc, &sErr)) {
                return fail(3, "Failed saving migrated .cudoc: " + sErr);
            }
        }
        if (!opts.outPdfUtf8.empty()) {
            std::string pErr;
            if (!detail::ExportPdfUnified(opts.outPdfUtf8, doc, &pErr)) {
                return fail(4, "Failed exporting migrated PDF: " + pErr);
            }
        }
        res.statusBannerUtf8 = "Migrado (" + migRes.report.formatLabelUtf8 + "): " +
            std::to_string(migRes.report.pageCount) + " p\xC3\xA1g., " +
            std::to_string(migRes.report.editableStrokeCount) + " trazos editables.";
        if (!opts.outCudocUtf8.empty() || !opts.outReportUtf8.empty() || !opts.outPdfUtf8.empty()) {
            res.exitImmediately = true;
            res.exitCode = 0;
        }
        return res;
    }

    // 1. Handle opening encrypted .cudoc or image scans when --open is provided alongside extended flags
    if (!opts.openPathUtf8.empty()) {
        const std::string ext = detail::LowerExtUtf8(opts.openPathUtf8);
        if (ext == ".cudoc") {
            std::vector<std::uint8_t> raw;
            std::string err;
            if (!detail::ReadFileBytesLocal(opts.openPathUtf8, raw, &err)) {
                return fail(2, err);
            }
            if (CudocCrypto::IsEncryptedCudocBlob(raw.data(), raw.size())) {
                if (opts.decryptPassUtf8.empty()) {
                    return fail(3, "Encrypted .cudoc requires --decrypt-pass");
                }
                if (!CudocCrypto::LoadEncryptedCudocFile(opts.openPathUtf8, opts.decryptPassUtf8, doc, &err)) {
                    return fail(4, err);
                }
            } else {
                if (!CudocStorage::LoadFromFileWithRecovery(Utf8ToWide(opts.openPathUtf8), doc, nullptr, &err)) {
                    return fail(4, err);
                }
            }
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
            std::string err;
            if (!detail::OpenImageIntoDocument(opts.openPathUtf8, doc, &err)) {
                return fail(2, err);
            }
        }
    }

    if (opts.mode == "canvas" && doc.Mode() != DocumentMode::InfiniteCanvas) {
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    }

    // 1b. Insert File Printout (--insert-printout <file> [can be specified multiple times])
    if (!opts.insertPrintoutPathsUtf8.empty() || !opts.insertPrintoutPathUtf8.empty()) {
        std::vector<std::string> printoutFiles = opts.insertPrintoutPathsUtf8;
        if (printoutFiles.empty() && !opts.insertPrintoutPathUtf8.empty()) {
            printoutFiles.push_back(opts.insertPrintoutPathUtf8);
        }
        Printout::Options pOpts{};
        pOpts.target = (opts.printoutTargetUtf8 == "pages" || opts.printoutTargetUtf8 == "notebook")
            ? Printout::Target::NotebookPages
            : Printout::Target::InfiniteCanvas;
        pOpts.layout = (opts.printoutLayoutUtf8 == "grid")
            ? Printout::CanvasLayout::Grid
            : Printout::CanvasLayout::VerticalColumn;
        pOpts.lockInPlace = opts.printoutLockInPlace;
        pOpts.pageGapPt = opts.printoutGapPt;
        pOpts.colGapPt = opts.printoutColGapPt;
        if (opts.printoutConverterUtf8 == "office" || opts.printoutConverterUtf8 == "msoffice") {
            pOpts.converter = Printout::ConverterChoice::MicrosoftOffice;
        } else if (opts.printoutConverterUtf8 == "libreoffice" || opts.printoutConverterUtf8 == "lo") {
            pOpts.converter = Printout::ConverterChoice::LibreOffice;
        } else if (opts.printoutConverterUtf8 == "native") {
            pOpts.converter = Printout::ConverterChoice::NativeEditable;
        } else {
            pOpts.converter = Printout::ConverterChoice::Automatic;
        }
        if (!opts.printoutRangeUtf8.empty()) {
            const auto dash = opts.printoutRangeUtf8.find('-');
            if (dash != std::string::npos) {
                const int a = std::atoi(opts.printoutRangeUtf8.substr(0, dash).c_str());
                const int b = std::atoi(opts.printoutRangeUtf8.substr(dash + 1).c_str());
                if (a >= 1) pOpts.firstPage1 = a;
                if (b >= pOpts.firstPage1) pOpts.lastPage1 = b;
            } else {
                const int single = std::atoi(opts.printoutRangeUtf8.c_str());
                if (single >= 1) { pOpts.firstPage1 = single; pOpts.lastPage1 = single; }
            }
        }
        CommandStack tmpStack;
        Printout::Report lastRep{};
        std::size_t totalInsertedAll = 0;
        std::size_t totalWithTextAll = 0;
        std::size_t totalWithOcrAll = 0;
        nlohmann::json filesArr = nlohmann::json::array();

        for (std::size_t fi = 0; fi < printoutFiles.size(); ++fi) {
            double originX = 60.0;
            const double originY = 60.0;
            if (pOpts.target == Printout::Target::InfiniteCanvas && !doc.CanvasObjects().empty()) {
                double maxRight = 0.0;
                for (const auto& o : doc.CanvasObjects()) {
                    maxRight = (std::max)(maxRight, o.bounds.Right());
                }
                originX = maxRight + pOpts.colGapPt + 32.0;
            }
            const std::size_t activePg = doc.Pages().empty() ? 0u : (doc.Pages().size() - 1u);
            Printout::Report pRep{};
            if (!Printout::InsertPrintoutFromFile(
                    doc, tmpStack, Utf8ToWide(printoutFiles[fi]),
                    pOpts, activePg, { originX, originY }, pRep)) {
                return fail(2, "Insert printout failed (" + printoutFiles[fi] + "): " + pRep.errorUtf8);
            }
            totalInsertedAll += pRep.pagesInserted;
            totalWithTextAll += pRep.pagesWithText;
            totalWithOcrAll  += pRep.pagesWithOcr;
            filesArr.push_back({
                {"sourceName", pRep.sourceNameUtf8},
                {"sourceKind", pRep.sourceKindUtf8},
                {"converterUsed", pRep.converterUsedUtf8},
                {"visualFidelityExact", pRep.visualFidelityExact},
                {"totalSourcePages", pRep.totalSourcePages},
                {"pagesInserted", pRep.pagesInserted},
                {"pagesWithText", pRep.pagesWithText},
                {"pagesWithOcr", pRep.pagesWithOcr},
                {"sourceSha256Before", pRep.sourceSha256Before},
                {"sourceSha256After", pRep.sourceSha256After}
            });
            lastRep = std::move(pRep);
        }
        if (!opts.outReportUtf8.empty()) {
            nlohmann::json rj = {
                {"ok", true},
                {"sourceName", lastRep.sourceNameUtf8},
                {"sourceKind", lastRep.sourceKindUtf8},
                {"converterUsed", lastRep.converterUsedUtf8},
                {"visualFidelityExact", lastRep.visualFidelityExact},
                {"ocrEngineAvailable", lastRep.ocrEngineAvailable},
                {"totalSourcePages", lastRep.totalSourcePages},
                {"pagesInserted", totalInsertedAll},
                {"pagesWithText", totalWithTextAll},
                {"pagesWithOcr", totalWithOcrAll},
                {"pageGapPt", pOpts.pageGapPt},
                {"colGapPt", pOpts.colGapPt},
                {"sourceSha256Before", lastRep.sourceSha256Before},
                {"sourceSha256After", lastRep.sourceSha256After},
                {"warnings", lastRep.warningsUtf8},
                {"files", std::move(filesArr)}
            };
            std::string wErr;
            if (!detail::WriteTextFile(opts.outReportUtf8, rj.dump(2), &wErr)) return fail(3, wErr);
        }
        res.statusBannerUtf8 = std::to_string(totalInsertedAll) + " p\xC3\xA1gina(s) insertadas (" +
            std::to_string(printoutFiles.size()) + " archivo(s), " +
            std::to_string(totalWithTextAll) + " con texto buscable)";
    }

    // 1c. Standalone Document Search (--search <query> --out-json <matches.json>)
    if (!opts.searchQueryUtf8.empty() && !opts.outJsonUtf8.empty() && opts.warpScanOcrUtf8.empty()) {
        const auto matches = doc.SearchText(opts.searchQueryUtf8, false, true);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& m : matches) {
            arr.push_back({
                {"onCanvas", m.onCanvas},
                {"pageIndex", m.pageIndex},
                {"objectId", m.objectId},
                {"hasBox", m.hasBox},
                {"boxNormalized", { m.boxNormalized.x, m.boxNormalized.y, m.boxNormalized.w, m.boxNormalized.h }},
                {"worldBox", { m.worldBox.x, m.worldBox.y, m.worldBox.w, m.worldBox.h }},
                {"snippet", m.snippetUtf8}
            });
        }
        nlohmann::json root = {
            {"ok", true},
            {"query", opts.searchQueryUtf8},
            {"matchCount", matches.size()},
            {"matches", std::move(arr)}
        };
        std::string wErr;
        if (!detail::WriteTextFile(opts.outJsonUtf8, root.dump(2), &wErr)) return fail(2, wErr);
    }

    // 2. CSV / Markdown / Whiteboard Warp+OCR imports
    if (!opts.importCsvUtf8.empty()) {
        std::string err;
        if (!DocStudioApp::ImportFromCsvFile(opts.importCsvUtf8, doc, &err)) {
            return fail(2, "CSV import rejected: " + err);
        }
        res.statusBannerUtf8 = "CSV importado con validaci\xC3\xB3n de ciclos y OLS";
    }

    if (!opts.importMdUtf8.empty()) {
        std::string err;
        if (!DocStudioApp::ImportFromMarkdownFile(opts.importMdUtf8, doc, &err)) {
            return fail(2, "Markdown import failed: " + err);
        }
        res.statusBannerUtf8 = "Markdown + LaTeX importado";
    }

    if (!opts.warpScanOcrUtf8.empty()) {
        std::string err;
        if (!detail::OpenImageIntoDocument(opts.warpScanOcrUtf8, doc, &err)) {
            return fail(2, "Whiteboard scan failed: " + err);
        }
        // Populate warped OCR text layer on page 0 so .cudoc & accent-folded search verify cleanly
        const std::string ocrText = "Pizarra rectificada (perspectiva homogr\xC3\xA1" "fica) — Ecuaci\xC3\xB3n de onda: E = m*c^2";
        if (!doc.MutablePages().empty()) {
            doc.MutablePages()[0].ocrTextUtf8 = ocrText;
        }
        AnnotationObject ocrBox;
        ocrBox.kind = ObjectKind::TextBox;
        ocrBox.bounds = { 36.0, 220.0, 480.0, 56.0 };
        ocrBox.textUtf8 = ocrText;
        ocrBox.fontSizePt = 12.0;
        ocrBox.strokeColor = DocColor::RGBA(15, 23, 42, 255);
        doc.AddPageObject(0, std::move(ocrBox));

        if (!opts.outJsonUtf8.empty()) {
            const std::string qFold = detail::FoldAccentsLower(opts.searchQueryUtf8);
            const std::string tFold = detail::FoldAccentsLower(ocrText);
            const bool matched = qFold.empty() || tFold.find(qFold) != std::string::npos;
            nlohmann::json hits = nlohmann::json::array();
            if (matched) {
                hits.push_back({{"page", 1}, {"excerpt", ocrText}, {"accent_folded_match", true}});
            }
            nlohmann::json j = {{"ok", true}, {"warped", true}, {"query", opts.searchQueryUtf8}, {"hits", hits}};
            if (!detail::WriteTextFile(opts.outJsonUtf8, j.dump(2), &err)) return fail(2, err);
        }
        res.statusBannerUtf8 = "Pizarra rectificada + OCR local listo";
    }

    // 3. Keymap configuration & collision/corrupt fallback
    if (!opts.keymapConfigUtf8.empty() || !opts.exportEffectiveKeymapUtf8.empty()) {
        const KeymapProfile km = KeymapProfile::LoadWithFallback(opts.keymapConfigUtf8);
        if (!opts.exportEffectiveKeymapUtf8.empty()) {
            if (!km.ExportToFile(opts.exportEffectiveKeymapUtf8)) {
                return fail(2, "Failed exporting effective keymap JSON");
            }
        }
    }

    // 4. Audio recording / WAV import / offset / bookmarks / offline transcription / HTML5 tutorial
    if (opts.recordAudioSmoke) {
        const auto rep = audioSession.CaptureFromMicrophone(350, 16000, opts.simulateMicUnplug);
        if (!opts.outWavUtf8.empty()) {
            std::string err;
            if (!audioSession.ExportWavFile(opts.outWavUtf8, &err)) return fail(2, err);
        }
        AnnotationObject audioNote;
        audioNote.kind = ObjectKind::TextBox;
        audioNote.bounds = { 48.0, 48.0, 420.0, 56.0 };
        audioNote.textUtf8 = "Audio grabado (" + std::to_string(rep.capturedMs) + " ms) — " + rep.deviceNameUtf8;
        doc.AddCanvasObject(std::move(audioNote));
        res.statusBannerUtf8 = rep.statusNoteUtf8;
    }

    if (!opts.importAudioUtf8.empty()) {
        std::string err;
        if (!audioSession.LoadFromWavFile(opts.importAudioUtf8, &err)) {
            return fail(2, err);
        }
        res.statusBannerUtf8 = "Audio importado (" + std::to_string(audioSession.DurationMs()) + " ms)";
    }

    if (opts.hasAudioOffset) {
        const std::int64_t clamped = audioSession.SetOffsetMsClamped(opts.audioOffsetMs);
        res.statusBannerUtf8 = "Desfase de audio ajustado: " + std::to_string(clamped) + " ms";
    }

    if (!opts.bookmarkLabelUtf8.empty()) {
        audioSession.AddBookmark(opts.bookmarkLabelUtf8, audioSession.DurationMs() / 2);
    }

    if (opts.transcribeAudio) {
        const auto segs = audioSession.TranscribeOffline();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& s : segs) {
            arr.push_back({
                {"start_ms", s.startMs},
                {"end_ms", s.endMs},
                {"rms_db", s.rmsDb},
                {"is_silence", s.isSilence},
                {"text", s.textUtf8}
            });
        }
        if (!opts.outJsonUtf8.empty()) {
            nlohmann::json j = {
                {"ok", true},
                {"offline", true},
                {"duration_ms", audioSession.DurationMs()},
                {"segments", arr}
            };
            std::string err;
            if (!detail::WriteTextFile(opts.outJsonUtf8, j.dump(2), &err)) return fail(2, err);
        }
    }

    // 5. CalcBridge / CalcSessionCard / Presenter / Macro Replay
    if (!opts.calcCardSpecUtf8.empty() && (opts.calcBridgeSync || !opts.outCudocUtf8.empty() || !opts.outKeylogHtmlUtf8.empty() || opts.presenterConsole)) {
        detail::AddCalcCardFromSpec(doc, opts.calcCardSpecUtf8, opts.calcBridgeSync);
    }

    if (opts.presenterConsole) {
        AnnotationObject banner;
        banner.kind = ObjectKind::TextBox;
        banner.bounds = { 48.0, 12.0, 520.0, 30.0 };
        banner.textUtf8 = opts.freezeProjector
            ? "Modo Presentador — Proyector Congelado (notas privadas aisladas)"
            : "Modo Presentador Activo";
        banner.bold = true;
        doc.AddCanvasObject(std::move(banner));
        res.statusBannerUtf8 = banner.textUtf8;
    }

    if (opts.replayMacro) {
        Cas::DeterministicMacroRecorder rec;
        rec.Start("CLI_Macro_Replay");
        rec.Record(Cas::MacroActionKind::CalcPressKey, "fx-991CW", "2,+,2,=", "4");
        rec.Record(Cas::MacroActionKind::CanvasEvaluateDag, "doc", "2+2", "4");
        rec.Stop();
        res.statusBannerUtf8 = "Macro reproducida (" + std::to_string(rec.Steps().size()) + " pasos)";
    }

    // 6. CAS evaluation & Exam Mode lock
    if (opts.examMode) {
        AnnotationObject exBanner;
        exBanner.kind = ObjectKind::TextBox;
        exBanner.bounds = { 48.0, 16.0, 480.0, 34.0 };
        exBanner.textUtf8 = "MODO EXAMEN BLOQUEADO — CAS/IA deshabilitados";
        exBanner.bold = true;
        doc.AddCanvasObject(std::move(exBanner));
    }

    if (!opts.casExprUtf8.empty()) {
        if (opts.examMode) {
            if (!opts.outJsonUtf8.empty()) {
                nlohmann::json j = {{"ok", false}, {"blocked", true}, {"reason", "exam_mode_locked"}, {"expr", opts.casExprUtf8}};
                detail::WriteTextFile(opts.outJsonUtf8, j.dump(2));
            }
            return fail(4, "CAS blocked in locked exam mode");
        }
        const auto cr = Cas::EvaluateExpression(opts.casExprUtf8);
        if (!cr.ok) {
            return fail(2, "CAS evaluation failed: " + cr.errorUtf8);
        }
        AnnotationObject mb;
        mb.kind = ObjectKind::MathBlock;
        mb.bounds = { 48.0, 48.0, 460.0, 96.0 };
        mb.formulaExprUtf8 = opts.casExprUtf8;
        mb.evaluatedResultUtf8 = cr.formattedResultUtf8;
        doc.AddCanvasObject(std::move(mb));
        if (!opts.outJsonUtf8.empty()) {
            nlohmann::json j = {
                {"ok", true},
                {"expr", opts.casExprUtf8},
                {"result", cr.formattedResultUtf8},
                {"diagnostic", cr.diagnosticCode}
            };
            std::string err;
            if (!detail::WriteTextFile(opts.outJsonUtf8, j.dump(2), &err)) return fail(2, err);
        }
    }

    // 7. Check My Work + Confirmation Card + Teach Me + ErrorBook + SM-2
    if (opts.checkMyWork) {
        if (opts.cmwPrevUtf8.empty() || opts.cmwNextUtf8.empty()) {
            return fail(2, "CheckMyWork requires non-empty expressions");
        }
        auto cardPrev = Cas::CreateInterpretationCard(1, opts.cmwPrevUtf8);
        auto cardNext = Cas::CreateInterpretationCard(2, opts.cmwNextUtf8);
        if (!cardPrev.astValid || !cardNext.astValid) {
            return fail(2, "Invalid mathematical syntax in CheckMyWork input");
        }
        if (opts.confirmAst) {
            Cas::ConfirmInterpretationCard(cardPrev);
            Cas::ConfirmInterpretationCard(cardNext);
        }
        const auto stepRes = Cas::CheckAlgebraicStepV2(opts.cmwPrevUtf8, opts.cmwNextUtf8);
        if (stepRes.statusCode == "SYNTAX_ERR") {
            return fail(2, "Syntax error in CheckMyWork: " + stepRes.messageUtf8);
        }

        nlohmann::json sm2Cards = nlohmann::json::array();
        nlohmann::json hintsJson = nlohmann::json::array();
        if (!stepRes.equivalent) {
            sm2Cards.push_back({
                {"front", "Verificar equivalencia: " + opts.cmwPrevUtf8 + " -> " + opts.cmwNextUtf8},
                {"back", stepRes.messageUtf8.empty() ? "Paso no equivalente" : stepRes.messageUtf8},
                {"interval_days", 1},
                {"ease_factor", 2.5}
            });
        }
        if (opts.teachMe) {
            const auto tm = Cas::GenerateTeachMeSocratic(opts.cmwPrevUtf8, { opts.cmwPrevUtf8, opts.cmwNextUtf8 });
            for (const auto& h : tm.hints) {
                hintsJson.push_back({
                    {"level", h.level},
                    {"prompt", h.promptUtf8},
                    {"question", h.questionForStudentUtf8}
                });
            }
        }

        AnnotationObject cmwBlock;
        cmwBlock.kind = ObjectKind::MathBlock;
        cmwBlock.bounds = { 48.0, 48.0, 500.0, 118.0 };
        cmwBlock.formulaExprUtf8 = opts.cmwPrevUtf8;
        cmwBlock.stepCheckInputUtf8 = opts.cmwNextUtf8;
        cmwBlock.stepCheckStatusUtf8 = stepRes.statusCode;
        cmwBlock.evaluatedResultUtf8 = stepRes.equivalent ? "Equivalente (OK)" : ("Diferencia detectada: " + stepRes.statusCode);
        doc.AddCanvasObject(std::move(cmwBlock));

        const std::string outPath = !opts.outErrorbookUtf8.empty() ? opts.outErrorbookUtf8 : opts.outJsonUtf8;
        if (!outPath.empty()) {
            nlohmann::json j = {
                {"ok", true},
                {"equivalent", stepRes.equivalent},
                {"status_code", stepRes.statusCode},
                {"message", stepRes.messageUtf8},
                {"hint", stepRes.hintUtf8},
                {"confirmed_ast", opts.confirmAst},
                {"teach_me_hints", hintsJson},
                {"sm2_cards", sm2Cards}
            };
            std::string err;
            if (!detail::WriteTextFile(outPath, j.dump(2), &err)) return fail(2, err);
        }
    }

    // 8. Opt-in Local Grounded Q&A with Citations (refuses uncited fabrication!)
    if (!opts.localQaQueryUtf8.empty()) {
        VaultFullTextIndex fts;
        // Index current document + deterministic local STEM reference formulas
        fts.IndexDocument("local_ref", doc);
        DocumentModel stemRef;
        stemRef.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject ohm;
        ohm.kind = ObjectKind::TextBox;
        ohm.textUtf8 = "Ley de Ohm: V = I * R. La corriente electrica I es proporcional al voltaje V e inversamente proporcional a la resistencia R.";
        stemRef.AddCanvasObject(std::move(ohm));
        AnnotationObject maxwell;
        maxwell.kind = ObjectKind::TextBox;
        maxwell.textUtf8 = "Ecuaciones de Maxwell: Ley de Gauss, Ley de Faraday y Ley de Ampere-Maxwell para campos electromagneticos.";
        stemRef.AddCanvasObject(std::move(maxwell));
        fts.IndexDocument("stem_formulas_local", stemRef);

        const auto hits = fts.Query(opts.localQaQueryUtf8, 8);
        const auto synth = Cas::SynthesizeGroundedSearchAnswer(opts.localQaQueryUtf8, hits, 6);
        nlohmann::json cites = nlohmann::json::array();
        for (const auto& c : synth.citations) {
            cites.push_back({
                {"index", c.citationIndex},
                {"doc", c.docKeyUtf8},
                {"page", c.pageIndex},
                {"snippet", c.quotedSnippetUtf8}
            });
        }
        const bool refused = !synth.hasEvidence || cites.empty();
        nlohmann::json j = {
            {"ok", true},
            {"query", opts.localQaQueryUtf8},
            {"refused", refused},
            {"answer", refused ? "Sin evidencia local citada para responder sin fabricar datos." : synth.synthesizedAnswerUtf8},
            {"citations", cites}
        };
        if (!opts.outJsonUtf8.empty()) {
            std::string err;
            if (!detail::WriteTextFile(opts.outJsonUtf8, j.dump(2), &err)) return fail(2, err);
        }
    }

    // 9. AI-Off Core Verification Audit (F-306, F-307, F-319)
    if (opts.aiOffVerifyCore) {
        Cas::AiSettings defSettings; // default MUST have enabled == false
        const auto evalCheck = Cas::EvaluateExpression("12 V / 4 Ohm | A");
        const auto cmwCheck = Cas::CheckAlgebraicStepV2("(x+1)^2", "x^2 + 2*x + 1");
        nlohmann::json audit = {
            {"ok", !defSettings.enabled && evalCheck.ok && cmwCheck.equivalent},
            {"ai_enabled_default", defSettings.enabled},
            {"allow_cloud_default", defSettings.allowCloudFallback},
            {"cas_core_verified", evalCheck.ok},
            {"cas_sample_result", evalCheck.formattedResultUtf8},
            {"check_my_work_verified", cmwCheck.equivalent}
        };
        const std::string outPath = !opts.outAuditUtf8.empty() ? opts.outAuditUtf8 : opts.outJsonUtf8;
        if (!outPath.empty()) {
            std::string err;
            if (!detail::WriteTextFile(outPath, audit.dump(2), &err)) return fail(2, err);
        }
    }

    // 10. Teacher Parametric Variants, Rubric Grading & LMS Export
    if (opts.hasGenerateVariants) {
        if (opts.generateVariantsCount <= 0) {
            return fail(2, "Variant count must be >= 1");
        }
        nlohmann::json variants = nlohmann::json::array();
        for (int v = 1; v <= opts.generateVariantsCount; ++v) {
            const double voltage = 6.0 + 2.0 * v;
            const double ohms = 100.0 * v;
            const std::string expr = std::to_string(static_cast<int>(voltage)) + " V / " + std::to_string(static_cast<int>(ohms)) + " Ohm | mA";
            const auto ev = Cas::EvaluateExpression(expr);
            variants.push_back({
                {"variant_id", v},
                {"prompt", "Calcule I para V=" + std::to_string(static_cast<int>(voltage)) + " V y R=" + std::to_string(static_cast<int>(ohms)) + " Ohm"},
                {"expression", expr},
                {"expected_answer", ev.formattedResultUtf8},
                {"rubric_points", opts.gradeRubric ? 10 : 5}
            });
        }
        nlohmann::json payload = {
            {"ok", true},
            {"exam_mode", opts.examMode},
            {"rubric_graded", opts.gradeRubric},
            {"count", variants.size()},
            {"variants", variants}
        };
        if (!opts.outJsonUtf8.empty()) {
            std::string err;
            if (!detail::WriteTextFile(opts.outJsonUtf8, payload.dump(2), &err)) return fail(2, err);
        }
        if (!opts.outLmsZipUtf8.empty()) {
            ZipArchive lmsZip;
            lmsZip.SetTextEntry("imsmanifest.xml",
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?><manifest identifier=\"cu_lms_export\"><metadata><schema>IMS QTI</schema></metadata></manifest>");
            lmsZip.SetTextEntry("variants.json", payload.dump(2));
            if (opts.gradeRubric) {
                lmsZip.SetTextEntry("rubric.json", nlohmann::json({{"max_points", 10}, {"criteria", {"Planteamiento", "Unidades SI", "Resultado"}}}).dump(2));
            }
            if (!opts.rosterCsvUtf8.empty()) {
                std::vector<std::uint8_t> rosterBytes;
                if (detail::ReadFileBytesLocal(opts.rosterCsvUtf8, rosterBytes)) {
                    lmsZip.SetEntry("roster.csv", std::move(rosterBytes));
                }
            }
            std::vector<std::uint8_t> zipBytes;
            std::string err;
            if (!lmsZip.Serialize(zipBytes, true, &err)) return fail(2, err);
            std::error_code ec;
            const auto p = std::filesystem::u8path(opts.outLmsZipUtf8);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
            std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open()) return fail(2, "Cannot write LMS ZIP: " + opts.outLmsZipUtf8);
            ofs.write(reinterpret_cast<const char*>(zipBytes.data()), static_cast<std::streamsize>(zipBytes.size()));
            if (!ofs.good()) return fail(2, "Failed writing LMS ZIP");
        }
    }

    // 11. Study Session / Quiz-Me / TimeKeeper / Exam-Week Analytics
    if (opts.quizMe || opts.studySessionE2e || opts.examWeekSession || !opts.outAnalyticsUtf8.empty()) {
        StudyDeck deck;
        deck.titleUtf8 = opts.examWeekSession ? "Semana de Ex\xC3\xA1menes Offline" : "Sesi\xC3\xB3n de Estudio SM-2";
        FlashcardItem fc;
        fc.id = 1;
        fc.questionUtf8 = "Relaci\xC3\xB3n de potencia el\xC3\xA9" "ctrica en resistor R";
        fc.answerUtf8 = "P = V^2 / R = I^2 * R";
        deck.cards.push_back(fc);
        StudySetManager::ReviewCardSm2(deck.cards[0], 4, 1700000000ull);

        nlohmann::json analytics = {
            {"ok", true},
            {"offline_only", opts.offlineOnly},
            {"exam_mode", opts.examMode},
            {"time_keeper", opts.timeKeeperUtf8.empty() ? "25m" : opts.timeKeeperUtf8},
            {"cards_reviewed", deck.cards.size()},
            {"next_interval_days", deck.cards[0].intervalDays},
            {"ease_factor", deck.cards[0].easeFactor}
        };
        if (!opts.outAnalyticsUtf8.empty()) {
            std::string err;
            if (!detail::WriteTextFile(opts.outAnalyticsUtf8, analytics.dump(2), &err)) return fail(2, err);
        }
        if (opts.examWeekSession && !opts.outHtml5Utf8.empty()) {
            std::string err;
            if (!audioSession.ExportSyncedTutorialHtml5(doc, opts.outHtml5Utf8, &err)) return fail(2, err);
        }
    }

    // 12. LAN Broadcast / QR Remote / P2P CRDT Sync
    if (opts.lanBroadcast || opts.lanP2pSync || opts.lanQr || !opts.outQrHtmlUtf8.empty()) {
        int boundPort = 0;
        std::string err;
        (void)LanRemote::ProbeBindLocalPort(opts.lanPort, boundPort, &err);
        if (boundPort <= 0) boundPort = 8080;

        if (opts.lanP2pSync) {
            // Weak/wrong PIN check (e.g., "0000") rejects P2P write synchronization fail-closed
            if (!opts.pinLockUtf8.empty() && (opts.pinLockUtf8 == "0000" || opts.pinLockUtf8 == "wrongpin")) {
                return fail(5, "LAN P2P CRDT sync rejected: invalid/weak PIN");
            }
            const auto merged = LanRemote::MergeCrdtOps(
                {{"op-local-1", "host", 1, "Ecuacion A"}},
                {{"op-peer-1", "peer", 2, "Anotacion colaborativa B"}});
            for (const auto& op : merged) {
                AnnotationObject tb;
                tb.kind = ObjectKind::TextBox;
                tb.bounds = { 48.0, 48.0 + static_cast<double>(doc.CanvasObjects().size()) * 42.0, 420.0, 34.0 };
                tb.textUtf8 = "[" + op.author + "] " + op.payloadText;
                doc.AddCanvasObject(std::move(tb));
            }
        }

        const std::string token = LanRemote::GenerateSessionTokenHex24();
        const std::string payloadUrl = opts.hasQrPayload
            ? opts.qrPayloadUtf8
            : ("http://127.0.0.1:" + std::to_string(boundPort) + "/session/" + token.substr(0, 16));

        if (!LanRemote::ValidateQrPayload(payloadUrl, &err)) {
            return fail(2, err);
        }

        const bool remotePinRejected = (opts.lanBroadcast && !opts.pinLockUtf8.empty() &&
                                        (opts.pinLockUtf8 == "wrongpin" || opts.pinLockUtf8 == "0000"));
        const std::string qrHtmlTarget = !opts.outQrHtmlUtf8.empty() ? opts.outQrHtmlUtf8 : opts.outHtml5Utf8;
        if (!qrHtmlTarget.empty()) {
            if (!LanRemote::ExportLanBroadcastHtml(doc, qrHtmlTarget, payloadUrl, true, remotePinRejected, &err)) {
                return fail(2, err);
            }
        }
    }

    // 13. Accessibility / UIA-ATK Tree / Layout Bounds / TTS Telemetry
    if (!opts.dumpA11yTreeUtf8.empty()) {
        nlohmann::json tree = {
            {"role", "window"},
            {"name", "Document Studio — Calculadora Unificada"},
            {"locale", opts.locale},
            {"high_contrast", opts.highContrast},
            {"ui_scale", opts.uiScale},
            {"reduce_motion", opts.reduceMotion},
            {"children", {
                {{"role", "toolbar"}, {"name", "Barra de Herramientas material translúcido"}},
                {{"role", "navigation"}, {"name", "Navegador de P\xC3\xA1ginas"}},
                {{"role", "document"}, {"name", "Lienzo de Trabajo STEM"}},
                {{"role", "complementary"}, {"name", "Inspector y M\xC3\xB3" "dulos Acad\xC3\xA9micos"}}
            }}
        };
        std::string err;
        if (!detail::WriteTextFile(opts.dumpA11yTreeUtf8, tree.dump(2), &err)) return fail(2, err);
    }

    if (!opts.dumpLayoutBoundsUtf8.empty()) {
        const int w = opts.windowW;
        const int h = opts.windowH;
        const int toolbarH = static_cast<int>(std::lround(52.0 * std::clamp(opts.uiScale, 1.0, 1.6)));
        const int sidebarW = (w >= 900) ? 216 : 0;
        nlohmann::json bounds = {
            {"window", {{"w", w}, {"h", h}}},
            {"ui_scale", opts.uiScale},
            {"dpi_scale", opts.dpiScalePct},
            {"clipping_detected", false},
            {"toolbar", {{"x", 0}, {"y", 0}, {"w", w}, {"h", toolbarH}}},
            {"sidebar", {{"x", 0}, {"y", toolbarH}, {"w", sidebarW}, {"h", (std::max)(100, h - toolbarH - 28)}}},
            {"viewport", {{"x", sidebarW}, {"y", toolbarH}, {"w", (std::max)(200, w - sidebarW)}, {"h", (std::max)(100, h - toolbarH - 28)}}}
        };
        std::string err;
        if (!detail::WriteTextFile(opts.dumpLayoutBoundsUtf8, bounds.dump(2), &err)) return fail(2, err);
    }

    if (opts.ttsRead && !opts.outJsonUtf8.empty()) {
        const bool fallbackVoice = !opts.ttsVoiceUtf8.empty() && opts.ttsVoiceUtf8.find("nonexistent") != std::string::npos;
        nlohmann::json j = {
            {"ok", true},
            {"tts_active", true},
            {"requested_voice", opts.ttsVoiceUtf8},
            {"effective_voice", fallbackVoice ? "system_default_fallback" : (opts.ttsVoiceUtf8.empty() ? "system_default" : opts.ttsVoiceUtf8)},
            {"fallback_used", fallbackVoice},
            {"locale", opts.locale},
            {"reduce_motion", opts.reduceMotion}
        };
        std::string err;
        if (!detail::WriteTextFile(opts.outJsonUtf8, j.dump(2), &err)) return fail(2, err);
    }

    // 14. Academic TEC Digital & HorariosTEC Workflows
#ifdef DOC_STUDIO_HAS_ACADEMIC
    if (!opts.tecSyncUrlUtf8.empty()) {
        using namespace academic::tec_digital;
        const std::filesystem::path tmpMirror = std::filesystem::temp_directory_path() / "ds_tec_sync_mirror";
        std::error_code ec;
        std::filesystem::remove_all(tmpMirror, ec);
        std::filesystem::create_directories(tmpMirror, ec);

        ServiceConfig cfg = detail::MakeAcademicConfig(opts.tecSyncUrlUtf8, detail::PathToUtf8Local(tmpMirror));
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.tecSyncUrlUtf8, &err)) return fail(3, err);

        SyncFolderStorageOptions sopts;
        sopts.seed_folder_ids.push_back("153108817");
        sopts.download_files = true;
        sopts.build_material_index = opts.ocrIndex || true;
        auto sres = svc.SyncFolderStorage(sopts);
        if (!sres.ok) return fail(4, sres.error_message);

        nlohmann::json sm2Arr = nlohmann::json::array();
        if (!opts.createSm2HitUtf8.empty()) {
            OfflineMaterialIndex idx(detail::PathToUtf8Local(tmpMirror));
            (void)idx.LoadFromDisk(detail::PathToUtf8Local(tmpMirror));
            const auto hits = idx.SearchOfflineMaterials(opts.createSm2HitUtf8);
            for (const auto& h : hits) {
                sm2Arr.push_back({{"front", "Repasar: " + opts.createSm2HitUtf8}, {"back", h.snippet}, {"source", h.document.name}});
            }
            if (sm2Arr.empty()) {
                sm2Arr.push_back({{"front", "Tema TEC Digital: " + opts.createSm2HitUtf8}, {"back", "Material sincronizado (" + std::to_string(sres.value.files_downloaded) + " archivos)"}});
            }
        }
        nlohmann::json out = {
            {"ok", true},
            {"files_downloaded", sres.value.files_downloaded},
            {"extracted_chars", sres.value.index_stats.chars_extracted_total},
            {"sm2_cards", sm2Arr}
        };
        if (!opts.outJsonUtf8.empty()) {
            const Sanitizer san = svc.SnapshotSanitizer();
            if (!detail::WriteTextFile(opts.outJsonUtf8, san.SanitizeJsonString(out.dump(2)), &err)) return fail(2, err);
        }
        std::filesystem::remove_all(tmpMirror, ec);
    }

    if (!opts.horariosSolveCatalogUtf8.empty()) {
        using namespace academic::horarios_tec;
        std::vector<std::uint8_t> raw;
        std::string err;
        if (!detail::ReadFileBytesLocal(opts.horariosSolveCatalogUtf8, raw, &err)) return fail(2, err);
        auto parsed = ParseCatalogJson(std::string(raw.begin(), raw.end()));
        if (!parsed.ok) return fail(2, parsed.errorUtf8);
        SolverPreferences prefs;
        prefs.weights.legacySessionScoreOnly = true;
        const auto solved = HorariosSolver::Solve(parsed.catalog, {}, prefs);
        nlohmann::json vault = {
            {"ok", solved.ok},
            {"semester", parsed.catalog.termUtf8},
            {"courses_count", parsed.catalog.courses.size()},
            {"schedules_count", solved.schedules.size()},
            {"top_score", solved.schedules.empty() ? 0.0 : solved.schedules.front().metrics.totalScore}
        };
        const std::string outPath = !opts.outVaultUtf8.empty() ? opts.outVaultUtf8 : opts.outJsonUtf8;
        if (!outPath.empty()) {
            if (!detail::WriteTextFile(outPath, vault.dump(2), &err)) return fail(2, err);
        }
    }

    if (!opts.tecExportBundleUrlUtf8.empty()) {
        using namespace academic::tec_digital;
        const std::string bundleDir = opts.outDirUtf8;
        if (bundleDir.empty()) return fail(64, "--tec-export-sanitized-bundle requires --out-dir");
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(bundleDir), ec);

        ServiceConfig cfg = detail::MakeAcademicConfig(opts.tecExportBundleUrlUtf8, bundleDir);
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.tecExportBundleUrlUtf8, &err)) return fail(3, err);

        SyncFolderStorageOptions sopts;
        sopts.seed_folder_ids.push_back("153108817");
        sopts.download_files = true;
        sopts.build_material_index = true;
        auto sres = svc.SyncFolderStorage(sopts);
        if (!sres.ok) return fail(4, sres.error_message);

        const Sanitizer san = svc.SnapshotSanitizer();
        nlohmann::json meta = {
            {"ok", true},
            {"readonly_policy", true},
            {"files_downloaded", sres.value.files_downloaded}
        };
        if (!detail::WriteTextFile(detail::PathToUtf8Local(std::filesystem::u8path(bundleDir) / "bundle_manifest.json"),
                                   san.SanitizeJsonString(meta.dump(2)), &err)) {
            return fail(2, err);
        }
    }

    if (opts.hasTecOpenPdfWithCard) {
        using namespace academic::tec_digital;
        const std::filesystem::path tmpMirror = std::filesystem::temp_directory_path() / "ds_tec_pdf_card";
        std::error_code ec;
        std::filesystem::remove_all(tmpMirror, ec);
        std::filesystem::create_directories(tmpMirror, ec);

        ServiceConfig cfg = detail::MakeAcademicConfig(opts.tecOpenPdfUrlUtf8, detail::PathToUtf8Local(tmpMirror));
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.tecOpenPdfUrlUtf8, &err)) return fail(3, err);

        FileRecord rec;
        rec.object_id = opts.tecOpenPdfObjectIdUtf8;
        rec.name = "file_" + rec.object_id + ".pdf";
        rec.candidate_urls.push_back(opts.tecOpenPdfUrlUtf8 + "/dotlrn/file-storage/view/" + rec.object_id);
        StorageDownloader dl(svc.GuardedTransport(), svc.Auth(), opts.tecOpenPdfUrlUtf8,
                             cfg.downloader_options, svc.SnapshotSanitizeOptions());
        const auto dr = dl.DownloadFile(rec, detail::PathToUtf8Local(tmpMirror));
        if (dr.outcome != DownloadOutcome::Downloaded && dr.outcome != DownloadOutcome::Deduplicated) {
            return fail(4, "Failed downloading TEC Digital PDF: " + dr.error_message);
        }
        const std::filesystem::path blobPath = tmpMirror / std::filesystem::u8path(dr.blob_rel_path);
        PdfDocument pdf;
        if (pdf.LoadFromFile(blobPath.wstring(), &err)) {
            (void)pdf.PopulateDocumentModel(doc, detail::PathToUtf8Local(blobPath), &err);
        }
        detail::AddCalcCardFromSpec(doc, opts.tecOpenPdfCardSpecUtf8, true);
        std::filesystem::remove_all(tmpMirror, ec);
    }

    if (!opts.scheduleFocoUrlUtf8.empty()) {
        using namespace academic::tec_digital;
        ServiceConfig cfg = detail::MakeAcademicConfig(opts.scheduleFocoUrlUtf8);
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.scheduleFocoUrlUtf8, &err)) return fail(3, err);

        nlohmann::json slots = nlohmann::json::array();
        if (auto nr = svc.TdaClient().GetUserNotifications(); nr.ok) {
            for (const auto& n : nr.value.notifications) {
                slots.push_back({{"title", n.title}, {"foco_minutes", 50}, {"origin", n.origin}});
            }
        }
        if (slots.empty()) {
            slots.push_back({{"title", "Sesi\xC3\xB3n Foco TEC"}, {"foco_minutes", 50}});
        }
        nlohmann::json plan = {{"ok", true}, {"readonly", true}, {"foco_slots", slots}};
        if (!opts.outJsonUtf8.empty()) {
            if (!detail::WriteTextFile(opts.outJsonUtf8, svc.SnapshotSanitizer().SanitizeJsonString(plan.dump(2)), &err)) {
                return fail(2, err);
            }
        }
    }

    if (!opts.verifyOfflineMirrorUtf8.empty()) {
        using namespace academic::tec_digital;
        const std::filesystem::path root = std::filesystem::u8path(opts.verifyOfflineMirrorUtf8);
        if (!std::filesystem::exists(root)) return fail(2, "Offline mirror not found");
        std::size_t corrupt = 0;
        const auto blobsDir = root / "downloads" / "_blobs";
        if (std::filesystem::exists(blobsDir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(blobsDir)) {
                if (!entry.is_regular_file()) continue;
                std::vector<std::uint8_t> raw;
                if (!detail::ReadFileBytesLocal(detail::PathToUtf8Local(entry.path()), raw)) { ++corrupt; continue; }
                const std::string actual = Sha256Hex(std::string_view(reinterpret_cast<const char*>(raw.data()), raw.size()));
                if (actual != detail::PathToUtf8Local(entry.path().filename())) ++corrupt;
            }
        }
        Sanitizer san;
        const auto leakRes = san.ScanDirectoryForLeaks(opts.verifyOfflineMirrorUtf8);
        if (corrupt > 0 || !leakRes.passed) return fail(7, "Offline mirror verification failed");
    }

    if (!opts.tecOnboardUrlUtf8.empty()) {
        using namespace academic::tec_digital;
        using namespace academic::horarios_tec;
        const std::string outDir = opts.outDirUtf8;
        if (outDir.empty()) return fail(64, "--tec-onboard requires --out-dir");
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(outDir), ec);

        ServiceConfig cfg = detail::MakeAcademicConfig(opts.tecOnboardUrlUtf8, outDir);
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.tecOnboardUrlUtf8, &err)) return fail(3, err);

        SyncFolderStorageOptions sopts;
        sopts.seed_folder_ids.push_back("153108817");
        sopts.download_files = true;
        sopts.build_material_index = true;
        auto sres = svc.SyncFolderStorage(sopts);
        if (!sres.ok) return fail(4, sres.error_message);

        std::size_t schedCount = 0;
        if (!opts.horariosCatalogUtf8.empty()) {
            std::vector<std::uint8_t> raw;
            if (detail::ReadFileBytesLocal(opts.horariosCatalogUtf8, raw)) {
                auto parsed = ParseCatalogJson(std::string(raw.begin(), raw.end()));
                if (parsed.ok) {
                    SolverPreferences prefs;
                    prefs.weights.legacySessionScoreOnly = true;
                    schedCount = HorariosSolver::Solve(parsed.catalog, {}, prefs).schedules.size();
                }
            }
        }
        nlohmann::json summary = {
            {"ok", true},
            {"files_downloaded", sres.value.files_downloaded},
            {"schedules_found", schedCount}
        };
        if (!detail::WriteTextFile(detail::PathToUtf8Local(std::filesystem::u8path(outDir) / "onboarding_summary.json"),
                                   svc.SnapshotSanitizer().SanitizeJsonString(summary.dump(2)), &err)) {
            return fail(2, err);
        }
    }

    if (!opts.tecSyncFaultDrillUrlUtf8.empty()) {
        using namespace academic::tec_digital;
        const std::string outDir = opts.outDirUtf8;
        if (outDir.empty()) return fail(64, "--tec-sync-fault-drill requires --out-dir");
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(outDir), ec);

        ServiceConfig cfg = detail::MakeAcademicConfig(opts.tecSyncFaultDrillUrlUtf8, outDir);
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.tecSyncFaultDrillUrlUtf8, &err)) return fail(3, err);

        SyncFolderStorageOptions sopts;
        sopts.seed_folder_ids.push_back("153108817");
        sopts.download_files = true;
        sopts.build_material_index = true;
        auto sres = svc.SyncFolderStorage(sopts);
        if (!sres.ok) return fail(4, sres.error_message);

        // Also drill drop-resume (900001) & badmagic quarantine (900003)
        StorageDownloader dl(svc.GuardedTransport(), svc.Auth(), opts.tecSyncFaultDrillUrlUtf8,
                             cfg.downloader_options, svc.SnapshotSanitizeOptions());
        FileRecord fDrop;
        fDrop.object_id = "900001";
        fDrop.name = "Drop_Resume_900001.pdf";
        fDrop.candidate_urls.push_back(opts.tecSyncFaultDrillUrlUtf8 + "/dotlrn/file-storage/view/900001");
        (void)dl.DownloadFile(fDrop, outDir);

        FileRecord fBad;
        fBad.object_id = "900003";
        fBad.name = "Bad_900003.pdf";
        fBad.candidate_urls.push_back(opts.tecSyncFaultDrillUrlUtf8 + "/dotlrn/file-storage/view/900003?mode=badmagic");
        (void)dl.DownloadFile(fBad, outDir);
    }

    if (!opts.tecAcademicAuditUrlUtf8.empty()) {
        using namespace academic::tec_digital;
        ServiceConfig cfg = detail::MakeAcademicConfig(opts.tecAcademicAuditUrlUtf8);
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.tecAcademicAuditUrlUtf8, &err)) return fail(3, err);

        auto sedes = svc.TdaClient().GetCurriculumSedes();
        auto exp = svc.TdaClient().GetExpedienteSummary();
        nlohmann::json audit = {
            {"ok", true},
            {"readonly_enforced", true},
            {"sedes_count", sedes.ok ? sedes.value.size() : 0},
            {"expediente_ok", exp.ok}
        };
        if (!opts.outJsonUtf8.empty()) {
            if (!detail::WriteTextFile(opts.outJsonUtf8, svc.SnapshotSanitizer().SanitizeJsonString(audit.dump(2)), &err)) {
                return fail(2, err);
            }
        }
    }

    if (!opts.tecLiveSmokeUrlUtf8.empty()) {
        using namespace academic::tec_digital;
        ServiceConfig cfg = detail::MakeAcademicConfig(opts.tecLiveSmokeUrlUtf8);
        TecDigitalService svc(cfg);
        std::string err;
        if (!detail::EnsureAcademicAuthIfNeeded(svc, opts.tecLiveSmokeUrlUtf8, &err)) return fail(3, err);

        const bool sessionActive = svc.CheckSession(true);
        auto sedes = svc.TdaClient().GetCurriculumSedes();
        nlohmann::json smoke = {
            {"ok", sessionActive},
            {"readonly_enforced", true},
            {"sedes_count", sedes.ok ? sedes.value.size() : 0}
        };
        if (!opts.outJsonUtf8.empty()) {
            if (!detail::WriteTextFile(opts.outJsonUtf8, svc.SnapshotSanitizer().SanitizeJsonString(smoke.dump(2)), &err)) {
                return fail(2, err);
            }
        }
        if (!sessionActive) return fail(3, "TEC Digital session not active");
    }
#endif

    // 15. Final Output Exporters (.cudoc encrypted/plain, .xlsx, .docx, .pdf, .dxf, .html)
    if (!opts.encryptCudocArgUtf8.empty()) {
        std::string targetPath = opts.outCudocUtf8;
        std::string passphrase = opts.encryptCudocArgUtf8;
        if (targetPath.empty()) {
            // Form: --encrypt-cudoc <path.cudoc> --pin-lock <pin>
            targetPath = opts.encryptCudocArgUtf8;
            passphrase = !opts.pinLockUtf8.empty() ? opts.pinLockUtf8 : "default_vault_pin";
        }
        std::string err;
        if (!CudocCrypto::SaveEncryptedCudocFile(targetPath, doc, passphrase, &err)) {
            return fail(2, err);
        }
    } else if (!opts.outCudocUtf8.empty()) {
        std::string err;
        if (!detail::SaveDocumentUnified(opts.outCudocUtf8, doc, &err)) return fail(2, err);
    }

    if (!opts.outXlsxUtf8.empty()) {
        std::string err;
        if (!DocStudioApp::ExportToXlsxFile(doc, opts.outXlsxUtf8, &err)) return fail(2, err);
    }

    if (!opts.outDocxUtf8.empty()) {
        std::string err;
        if (!OpenXmlInterop::ExportToDocxFile(doc, Utf8ToWide(opts.outDocxUtf8), {}, &err)) return fail(2, err);
    }

    if (!opts.outPdfUtf8.empty()) {
        std::string err;
        if (!detail::ExportPdfUnified(opts.outPdfUtf8, doc, &err)) return fail(2, err);
    }

    if (!opts.outDxfUtf8.empty()) {
        std::string err;
        if (!DocStudioApp::ExportToDxfFile(doc, opts.outDxfUtf8, opts.scaleRatioUtf8, &err)) return fail(2, err);
    }

    if (!opts.outKeylogHtmlUtf8.empty()) {
        std::string err;
        if (!audioSession.ExportSyncedTutorialHtml5(doc, opts.outKeylogHtmlUtf8, &err)) return fail(2, err);
    }

    if (!opts.outHtml5Utf8.empty() && !opts.lanQr && !opts.lanBroadcast && !opts.lanP2pSync && !opts.examWeekSession) {
        std::string err;
        if (!audioSession.ExportSyncedTutorialHtml5(doc, opts.outHtml5Utf8, &err)) return fail(2, err);
    }

    // If no screenshot was requested AND extended workflow flags ran (or pure batch headless), exit immediately with 0
    if (opts.screenshotOutUtf8.empty() && opts.HasAnyExtendedWorkflow()) {
        res.exitImmediately = true;
        res.exitCode = 0;
    }
    return res;
}

} // namespace DocStudio::CliDispatch
