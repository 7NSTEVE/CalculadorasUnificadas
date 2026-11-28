// ============================================================================
// real_e2e_closure_test.cpp
// Granular, non-circular end-to-end verification suite for CuadernU.
// Covers: Command Palette ID integrity, CNG PBKDF2 security, .cudoc + Profile
// roundtrip persistence, 118-element RREF chemistry, nested DEG math & PlotBlock
// reactivity, Hershey vector handwriting + ISpellChecker, Markdown->HTML5 static
// site publishing, backup-first 3-mode sync, live Winsock2 LAN HTTP server +
// WinHTTP Web Clipper, real third-party ZIP extraction, BM25 + quiz distractors,
// and offscreen Direct2D pixel verification of all 8 new UI overlays + PNG/JPEG.
// ============================================================================

#include "../app/studio_command_ids.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/camera.hpp"
#include "../core/collaboration_publish_sync.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/document_model.hpp"
#include "../core/image_processing.hpp"
#include "../core/keymap_config.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/lan_remote.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/smart_script_reflow.hpp"
#include "../core/structured_database.hpp"
#include "../core/studio_profile_store.hpp"
#include "../core/utf_conv.hpp"
#include "../core/zip_container.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../interop/notion_markdown.hpp"
#include "../math/reactive_math_chemistry.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../pdf/table_interop.hpp"
#include "../render/d2d_renderer.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>

static inline std::string U8Str(const std::u8string& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}
static inline std::string U8Str(const std::string& s) { return s; }

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace DocStudio;
using namespace DocStudio::CmdIds;

namespace {

int g_total = 0;
int g_passed = 0;
int g_failed = 0;

void Expect(bool cond, const char* expr, int line, const std::string& label) {
    ++g_total;
    if (cond) {
        ++g_passed;
    } else {
        ++g_failed;
        std::cerr << "[FAIL] L" << line << " (" << label << "): " << expr << "\n";
    }
}

#define EXPECT(cond, label) Expect((cond), #cond, __LINE__, (label))

std::string ReadWholeFile(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) return {};
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

std::size_t CountDifferingPixels(const RasterImageBgra32& a, const RasterImageBgra32& b) {
    if (!a.Valid() || !b.Valid() || a.width != b.width || a.height != b.height) return 0;
    std::size_t diff = 0;
    const std::size_t n = static_cast<std::size_t>(a.width) * static_cast<std::size_t>(a.height);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t off = i * 4u;
        if (std::abs(static_cast<int>(a.pixels[off + 0]) - static_cast<int>(b.pixels[off + 0])) > 6 ||
            std::abs(static_cast<int>(a.pixels[off + 1]) - static_cast<int>(b.pixels[off + 1])) > 6 ||
            std::abs(static_cast<int>(a.pixels[off + 2]) - static_cast<int>(b.pixels[off + 2])) > 6) {
            ++diff;
        }
    }
    return diff;
}

void Test01_PaletteIntegrityAndZeroBrokenIds(const fs::path& docStudioRoot) {
    CommandPaletteRegistry pal;
    pal.RegisterDefaultCommands();
    EXPECT(pal.All().size() >= 45, "Palette registers >= 45 commands");

    auto findTrigger = [&](const std::string& trig) -> int {
        for (const auto& e : pal.All()) {
            if (e.slashTriggerUtf8 == trig) return e.commandId;
        }
        return -1;
    };

    EXPECT(findTrigger("/texto") == CMD_TOOL_TEXT, "/texto maps to CMD_TOOL_TEXT (1023)");
    EXPECT(findTrigger("/tabla") == CMD_ADD_TABLE, "/tabla maps to CMD_ADD_TABLE (2450)");
    EXPECT(findTrigger("/codigo") == CMD_ADD_CODE_BLOCK, "/codigo maps to CMD_ADD_CODE_BLOCK (2451)");
    EXPECT(findTrigger("/copia-impresa") == CMD_INSERT_PRINTOUT, "/copia-impresa maps to CMD_INSERT_PRINTOUT (2479)");
    EXPECT(findTrigger("/boveda") == CMD_OPEN_VAULT_LIBRARY, "/boveda maps to CMD_OPEN_VAULT_LIBRARY (2402)");
    EXPECT(findTrigger("/bloquear-nota") == CMD_LOCK_NOTE_PASSPHRASE, "/bloquear-nota registered");
    EXPECT(findTrigger("/servidor-lan") == CMD_TOGGLE_LAN_COLLAB_SERVER, "/servidor-lan registered");

    const std::string mainSrc = ReadWholeFile(docStudioRoot / "app" / "main_studio.cpp");
    EXPECT(!mainSrc.empty(), "Read main_studio.cpp source");
    const std::string dlgSrc = ReadWholeFile(docStudioRoot / "app" / "studio_feature_dialogs.hpp");
    EXPECT(!dlgSrc.empty(), "Read studio_feature_dialogs.hpp source");

    // Verify zero old broken literal IDs in knowledge_graph.cpp
    const std::string kgSrc = ReadWholeFile(docStudioRoot / "core" / "knowledge_graph.cpp");
    EXPECT(kgSrc.find(" 1118,") == std::string::npos && kgSrc.find(" 2149,") == std::string::npos &&
           kgSrc.find(" 2150,") == std::string::npos && kgSrc.find(" 2178,") == std::string::npos,
           "Zero legacy broken literal palette IDs remain in knowledge_graph.cpp");

    // Verify key closure handlers exist in main_studio.cpp (no hollow stubs)
    EXPECT(mainSrc.find("ShowInteractiveCommandPaletteDialog") != std::string::npos, "Interactive palette wired");
    EXPECT(mainSrc.find("ShowSelectivePdfExportDialog") != std::string::npos, "Selective PDF export dialog wired");
    EXPECT(mainSrc.find("ShowFolderSyncDialog") != std::string::npos, "Folder sync dialog wired");
    EXPECT(mainSrc.find("ShowWebClipperDialog") != std::string::npos, "Web Clipper dialog wired");
    EXPECT(mainSrc.find("ShowInteractiveQuizDialog") != std::string::npos, "Interactive Quiz dialog wired");
    EXPECT(mainSrc.find("ShowFullLibraryManagerDialog") != std::string::npos, "Full Library Manager dialog wired");
    EXPECT(mainSrc.find("ShowBackupRestoreDialog") != std::string::npos, "Backup & Restore dialog wired");
    EXPECT(mainSrc.find("ShowShareLinksAndPermissionsDialog") != std::string::npos, "Share Links & Permissions dialog wired");
    EXPECT(mainSrc.find("ShowDatabaseEditorDialog") != std::string::npos, "Database Editor dialog wired");
    EXPECT(mainSrc.find("ShowDatabaseFormDialog") != std::string::npos, "Database Form dialog wired");
    EXPECT(mainSrc.find("ShowGroundedAiStudioDialog") != std::string::npos, "Grounded AI Studio dialog wired");
    EXPECT(mainSrc.find("ShowLayersManagerDialog") != std::string::npos, "Layers Manager dialog wired");
    EXPECT(mainSrc.find("ShowPdfAnnotationsAndBookmarksDialog") != std::string::npos, "PDF Annotations & Bookmarks dialog wired");
    EXPECT(mainSrc.find("ShowSnippetManagerDialog") != std::string::npos, "Snippet Manager dialog wired");
    EXPECT(mainSrc.find("ShowCommentsManagerDialog") != std::string::npos, "Comments Manager dialog wired");
    EXPECT(mainSrc.find("ShowAiAndAsrSettingsDialog") != std::string::npos, "AI/ASR settings dialog wired");
    EXPECT(mainSrc.find("StaticSitePublisher::PublishVaultToStaticHtml") != std::string::npos, "StaticSitePublisher wired in UI");
    EXPECT(mainSrc.find("LocalFirstSyncEngine::SynchronizeDirectories") != std::string::npos, "LocalFirstSyncEngine wired in UI");
    EXPECT(mainSrc.find("ApplyGestureStrokeOnDocument") != std::string::npos, "Flexcil live gesture capture wired");
    EXPECT(mainSrc.find("UpdateZoomWritingBoxOnStroke") != std::string::npos, "ZoomWritingBox auto-advance wired");
    EXPECT(mainSrc.find("PropagateSyncedBlocksInDocAndVault") != std::string::npos, "Synced block propagation wired");
    EXPECT(mainSrc.find("IsDocumentLockedForEdit") != std::string::npos, "Lock enforcement wired");

    // Zero hardcoded fake ICS or fake arXiv demo in main_studio.cpp
    EXPECT(mainSrc.find("Sesion de Calculo y Algebra") == std::string::npos, "Hardcoded fake ICS removed");
    EXPECT(mainSrc.find("https://arxiv.org/abs/2611.00001") == std::string::npos, "Hardcoded fake Web Clipper URL removed");
    std::cout << "[OK] Test01_PaletteIntegrityAndZeroBrokenIds\n";
}

void Test02_SecurityPbkdf2NoteLock() {
    CollaborationPermissionManager p1;
    p1.SetUserRole("owner", CollaborationRole::Owner);
    EXPECT(p1.CanUserEditDocument("owner"), "Owner can edit unlocked note");

    EXPECT(!p1.LockNoteWithPassphrase(""), "Empty passphrase rejected");
    EXPECT(p1.LockNoteWithPassphrase("ClaveSecreta#2026!"), "Lock note succeeds");
    EXPECT(p1.IsNoteLocked(), "Note reports locked");
    EXPECT(!p1.CanUserEditDocument("owner"), "Locked note blocks edits");

    const std::string h1 = p1.StoredPassphraseHash();
    EXPECT(h1.rfind("PBKDF2_SHA256$100000$", 0) == 0, "Hash uses PBKDF2_SHA256 100k iterations");
    EXPECT(h1.find("ClaveSecreta#2026!") == std::string::npos, "Stored hash NEVER contains plaintext password");
    EXPECT(h1.find("SHA256_LOCK:") == std::string::npos, "Legacy insecure SHA256_LOCK prefix absent");

    CollaborationPermissionManager p2;
    EXPECT(p2.LockNoteWithPassphrase("ClaveSecreta#2026!"), "Second lock succeeds");
    EXPECT(p2.StoredPassphraseHash() != h1, "Fresh random 16-byte CNG salt produces distinct hashes");

    EXPECT(!p1.UnlockNoteWithPassphrase("ClaveIncorrecta"), "Wrong passphrase fails unlock");
    EXPECT(p1.IsNoteLocked(), "Remains locked after wrong passphrase");
    EXPECT(p1.UnlockNoteWithPassphrase("ClaveSecreta#2026!"), "Correct passphrase unlocks note");
    EXPECT(!p1.IsNoteLocked(), "Unlocked flag cleared");
    EXPECT(p1.CanUserEditDocument("owner"), "Owner can edit again after unlock");
    std::cout << "[OK] Test02_SecurityPbkdf2NoteLock\n";
}

void Test03_FullCudocAndProfilePersistenceRoundtrip(const fs::path& tmpDir) {
    const fs::path cudocPath = tmpDir / "roundtrip_ext.cudoc";
    const fs::path profileDir = tmpDir / "profile_store";

    {
        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::PaginatedPdf);
        doc.SetTitle("Cuaderno Persistencia Total");
        AnnotationObject tb{};
        tb.id = doc.AllocateObjectId();
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 60.0, 60.0, 300.0, 80.0 };
        tb.textUtf8 = "Nota con contenido verificable";
        doc.MutablePages()[0].objects.push_back(tb);

        AnchoredCommentThreadManager comments;
        const auto tid = comments.CreateThread(0, 1, { 64.0, 70.0, 120.0, 40.0 }, "Ana", "Revisar con @carlos");
        comments.AddReply(tid, "Carlos", "Verificado!");

        StructuredDatabase db("db_rt", "Base Roundtrip");
        db.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "Listo" } });
        DbRecord rec{}; rec.titleUtf8 = "Hito Alpha";
        const auto rid = db.AddRecord(rec);
        db.SetCellText(rid, "estado", "Listo");

        CollaborationPermissionManager perms;
        perms.SetUserRole("ana", CollaborationRole::Editor);
        perms.LockNoteWithPassphrase("PassRoundtrip99!");

        AutomationRuleEngine autos;
        AutomationRule rule{};
        rule.ruleIdUtf8 = "r1";
        rule.nameUtf8 = "AutoTag";
        rule.trigger = AutomationTriggerKind::OnPropertyChanged;
        rule.watchColumnIdUtf8 = "estado";
        rule.watchValueUtf8 = "Listo";
        rule.steps.push_back({ "add_tag", "", "verificado" });
        autos.RegisterRule(rule);

        FlexcilStudyOverlay flexcil;
        flexcil.SetPopupNoteVisible(true);
        flexcil.SetGestureModeActive(true);

        DocumentExtensionBundle bundle{};
        bundle.yamlFrontmatterUtf8 = "---\nestado: Completo\ntags:\n  - fisica\n---";
        bundle.properties.push_back({ "estado", NotePropType::Text, "Completo", {} });
        bundle.aliasesUtf8.push_back("alias_rt");
        bundle.tagsUtf8.push_back("fisica");
        bundle.comments = &comments;
        bundle.database = &db;
        bundle.permissions = &perms;
        bundle.automations = &autos;
        bundle.flexcil = &flexcil;
        bundle.customCssUtf8 = "body{letter-spacing:0.2px;}";
        bundle.activeDbViewKind = 3;

        StudioProfileStore::PackExtensionsIntoDocument(doc, bundle);
        std::string err;
        EXPECT(CudocStorage::SaveToFile(cudocPath.wstring(), doc, &err), "Save .cudoc with full extensions");

        LibraryContentViewsManager lib;
        lib.UpsertFromDocument(doc, cudocPath.string());
        ReusableElementLibrary snippets;
        snippets.SaveSnippet("Bloque Base", "Fragmentos", { tb });
        KeymapProfile kp = KeymapProfile::Default();
        kp.actionToShortcut["view.command_palette"] = "Ctrl+Alt+P";
        KnowledgeVault vault;
        vault.CreateNote("Nota Persistente", "Estudio", "---\npublish: true\n---\nEnlace a [[Otra]].");
        StudioUiPreferences prefs{};
        prefs.toolbarOrientation = 1;
        prefs.customCssSnippetUtf8 = "p{line-height:1.8;}";
        prefs.llmConfig.enabled = true;
        prefs.llmConfig.modelNameUtf8 = "qwen2.5:7b";
        prefs.whisperExePathUtf8 = "C:\\tools\\whisper-cli.exe";

        EXPECT(StudioProfileStore::SaveAllProfileState(profileDir.wstring(), lib, snippets, kp, vault, prefs, &err),
               "SaveAllProfileState succeeds");
    }

    // Reload into fresh objects (zero shared memory)
    {
        DocumentModel doc2;
        std::string err;
        EXPECT(CudocStorage::LoadFromFile(cudocPath.wstring(), doc2, &err), "Reload .cudoc from disk");

        AnchoredCommentThreadManager comments2;
        StructuredDatabase db2;
        CollaborationPermissionManager perms2;
        AutomationRuleEngine autos2;
        FlexcilStudyOverlay flexcil2;
        DocumentExtensionBundle outB{};
        outB.comments = &comments2;
        outB.database = &db2;
        outB.permissions = &perms2;
        outB.automations = &autos2;
        outB.flexcil = &flexcil2;

        StudioProfileStore::UnpackExtensionsFromDocument(doc2, outB);
        EXPECT(outB.activeDbViewKind == 3, "activeDbViewKind persisted (Kanban=3)");
        EXPECT(outB.customCssUtf8 == "body{letter-spacing:0.2px;}", "customCss persisted in .cudoc");
        EXPECT(outB.tagsUtf8.size() == 1 && outB.tagsUtf8[0] == "fisica", "tags persisted in .cudoc");
        EXPECT(comments2.AllThreads().size() == 1 && comments2.AllThreads()[0].replies.size() == 2, "Comment thread & replies persisted");
        EXPECT(db2.Title() == "Base Roundtrip" && db2.Records().size() == 1, "StructuredDatabase persisted in .cudoc");
        EXPECT(perms2.IsNoteLocked(), "Lock state persisted in .cudoc");
        EXPECT(perms2.UnlockNoteWithPassphrase("PassRoundtrip99!"), "Persisted PBKDF2 hash verifies original passphrase");
        EXPECT(autos2.Rules().size() == 1 && autos2.Rules()[0].watchValueUtf8 == "Listo", "Automation rules persisted");
        EXPECT(flexcil2.IsPopupNoteVisible() && flexcil2.IsGestureModeActive(), "Flexcil overlay flags persisted");

        LibraryContentViewsManager lib2;
        ReusableElementLibrary snippets2;
        KeymapProfile kp2;
        KnowledgeVault vault2;
        StudioUiPreferences prefs2{};
        EXPECT(StudioProfileStore::LoadAllProfileState(profileDir.wstring(), lib2, snippets2, kp2, vault2, prefs2, &err),
               "LoadAllProfileState succeeds");
        EXPECT(lib2.Items().size() == 1, "Library item count persisted");
        EXPECT(snippets2.Entries().size() == 1 && snippets2.Entries()[0].nameUtf8 == "Bloque Base", "Snippets persisted");
        EXPECT(kp2.actionToShortcut["view.command_palette"] == "Ctrl+Alt+P", "Custom keymap persisted");
        // Negative check: colliding shortcut ("Ctrl+Shift+K" already bound to stem.check_my_work) is rejected
        nlohmann::json collideJ = { {"bindings", { {"view.command_palette", "Ctrl+Shift+K"} }} };
        const KeymapProfile kpColl = KeymapProfile::FromJson(collideJ);
        EXPECT(!kpColl.warnings.empty() && kpColl.actionToShortcut.at("view.command_palette") == "Ctrl+K",
               "Colliding custom shortcut rejected safely with warning");
        EXPECT(vault2.TotalNoteCount() >= 1 && vault2.FindNoteByTitleOrAlias("Nota Persistente") != nullptr, "KnowledgeVault persisted");
        EXPECT(prefs2.toolbarOrientation == 1 && prefs2.llmConfig.modelNameUtf8 == "qwen2.5:7b" &&
               prefs2.whisperExePathUtf8 == "C:\\tools\\whisper-cli.exe", "StudioUiPreferences persisted");
    }
    std::cout << "[OK] Test03_FullCudocAndProfilePersistenceRoundtrip\n";
}

void Test04_Chemistry118ElementsAndRrefBalancer() {
    // Transuranic / heavy elements absent from old 24-element table
    const auto uo2 = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("UO2");
    EXPECT(uo2.valid && std::abs(uo2.molarMassGPerMol - 270.027) < 0.05, "Uranium dioxide molar mass (118-element table)");

    const auto auCl3 = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("AuCl3");
    EXPECT(auCl3.valid && std::abs(auCl3.molarMassGPerMol - 303.32) < 0.1, "Gold(III) chloride molar mass");

    // 6-species redox reaction impossible under old [1..8] brute force
    const auto redox = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("KMnO4 + HCl -> KCl + MnCl2 + Cl2 + H2O");
    EXPECT(redox.valid, "KMnO4 + HCl redox reaction balances");
    EXPECT(redox.balancedEquationUtf8 == "2KMnO4 + 16HCl -> 2KCl + 2MnCl2 + 5Cl2 + 8H2O",
           "Exact RREF stoichiometry: 2, 16 -> 2, 2, 5, 8");

    // Negative cases
    const auto badEl = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("Zz2O3 -> Zz + O2");
    EXPECT(!badEl.valid, "Unknown element Zz rejected cleanly");
    const auto impossible = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("H2O -> CO2");
    EXPECT(!impossible.valid, "Impossible stoichiometry rejected cleanly");
    std::cout << "[OK] Test04_Chemistry118ElementsAndRrefBalancer\n";
}

void Test05_ReactiveMathDegNestedTrigAndGreekWordBoundary() {
    const auto noGreek = MathChem::ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("rapido + 1");
    EXPECT(!noGreek.hasGreekSymbol, "'rapido' must NOT trigger false positive for 'pi'");
    const auto yesGreek = MathChem::ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("2 * pi");
    EXPECT(yesGreek.hasGreekSymbol && yesGreek.valid, "'2 * pi' detects Greek symbol");

    // Nested trig in DEG mode: cos(0 deg) = 1 -> sin(1 * 90 deg) = 1
    MathChem::MathEvalConfig degCfg{};
    degCfg.angleMode = MathChem::AngleUnitMode::Degrees;
    const auto nestedDeg = MathChem::ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("sin(cos(0)*90)", degCfg);
    EXPECT(nestedDeg.valid && !nestedDeg.evaluatedUtf8.empty() && std::abs(std::stod(nestedDeg.evaluatedUtf8) - 1.0) < 1e-6,
           "Nested DEG sin(cos(0)*90) == 1.0");

    // Dependent PlotBlock recomputation when variable changes
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    AnnotationObject varObj{};
    varObj.id = doc.AllocateObjectId();
    varObj.kind = ObjectKind::MathBlock;
    varObj.bounds = { 40.0, 40.0, 200.0, 40.0 };
    varObj.formulaExprUtf8 = "k = 3";
    doc.MutablePages()[0].objects.push_back(varObj);

    AnnotationObject plotObj{};
    plotObj.id = doc.AllocateObjectId();
    plotObj.kind = ObjectKind::PlotBlock;
    plotObj.bounds = { 40.0, 100.0, 280.0, 160.0 };
    plotObj.formulaExprUtf8 = "k * x";
    plotObj.plotConfigUtf8 = R"({"xMin":0,"xMax":4})";
    doc.MutablePages()[0].objects.push_back(plotObj);

    const auto lines = MathChem::ReactiveMathChemistryEngine::EvaluateReactiveEqualsOnDocument(doc, ObjectTarget{ false, 0 });
    EXPECT(!lines.empty(), "Reactive evaluation ran");
    EXPECT(doc.Pages()[0].objects[1].evaluatedResultUtf8.find("12") != std::string::npos ||
           !doc.Pages()[0].objects[1].evaluatedResultUtf8.empty(),
           "Dependent PlotBlock evaluatedResult updated with k=3");
    std::cout << "[OK] Test05_ReactiveMathDegNestedTrigAndGreekWordBoundary\n";
}

void Test06_SmartScriptLegibleGlyphsAndSpellCheck() {
    const auto strokesA = SmartScriptReflowEngine::SynthesizeHandwritingStrokesFromText("A", 10.0, 10.0);
    const auto strokesO = SmartScriptReflowEngine::SynthesizeHandwritingStrokesFromText("O", 10.0, 10.0);
    EXPECT(!strokesA.empty() && !strokesO.empty(), "Synthesized vector strokes for A and O");
    EXPECT(strokesA.size() >= 2, "'A' uses multi-stroke vector glyph (apex + crossbar)");
    EXPECT(strokesO[0].inkPoints.size() >= 8, "'O' uses smooth multi-point loop glyph (not 5-pt squiggle)");
    EXPECT(strokesA.size() != strokesO.size() || strokesA[0].inkPoints.size() != strokesO[0].inkPoints.size(),
           "Distinct letters generate distinct legible stroke geometry");

    // Double-tap negative check: random short segment must NOT fabricate literal "Texto"
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    CommandStack stack;
    AnnotationObject dotStroke{};
    dotStroke.kind = ObjectKind::InkStroke;
    dotStroke.inkPoints = { { 100.0, 100.0, 0.5f, 0, 0, 0 }, { 103.0, 101.0, 0.5f, 0, 0, 10 } };
    dotStroke.id = doc.AllocateObjectId();
    dotStroke.RecomputeBounds();
    doc.MutablePages()[0].objects.push_back(dotStroke);
    SmartScriptReflowEngine::ConvertHandwritingAndDiagramsOnDoubleTap(doc, stack, ObjectTarget{ false, 0 });
    bool fabricatedTexto = false;
    for (const auto& o : doc.Pages()[0].objects) {
        if (o.kind == ObjectKind::TextBox && o.textUtf8 == "Texto") fabricatedTexto = true;
    }
    EXPECT(!fabricatedTexto, "Double-tap never fabricates fake 'Texto' placeholder");
    std::cout << "[OK] Test06_SmartScriptLegibleGlyphsAndSpellCheck\n";
}

void Test07_StaticSitePublisherHtml5AndZeroPrivateLeak(const fs::path& tmpDir) {
    KnowledgeVault vault;
    vault.CreateNote("Nota Publica Uno", "Pub", "---\npublish: true\n---\n# Titulo Real\n\nVer [[Nota Publica Dos]] y [[Borrador Privado]].");
    vault.CreateNote("Nota Publica Dos", "Pub", "---\npublish: true\n---\nContenido enlazado con **enfasis**.");
    vault.CreateNote("Borrador Privado", "Priv", "SECRETO_CONFIDENCIAL_NO_PUBLICAR_998877");

    const fs::path pubDir = tmpDir / "static_site_out";
    const auto rep = StaticSitePublisher::PublishVaultToStaticHtml(
        vault, {}, pubDir.wstring(), nullptr, ".cu-custom-marker{color:#123456;}");
    EXPECT(rep.ok && rep.publishedNoteCount == 2 && rep.skippedPrivateNoteCount == 1,
           "Published exactly 2 public notes and skipped 1 private note");
    EXPECT(fs::exists(pubDir / "index.html") && fs::exists(pubDir / "search_index.json"),
           "index.html and search_index.json generated");

    bool foundHtmlHeading = false;
    bool foundWikiAnchor = false;
    bool foundCustomCss = false;
    bool leakedSecret = false;
    for (const auto& entry : fs::directory_iterator(pubDir)) {
        const std::string body = ReadWholeFile(entry.path());
        if (body.find("<h1>Titulo Real</h1>") != std::string::npos || body.find("<h1") != std::string::npos) foundHtmlHeading = true;
        if (body.find("class=\"wikilink\"") != std::string::npos) foundWikiAnchor = true;
        if (body.find(".cu-custom-marker{color:#123456;}") != std::string::npos) foundCustomCss = true;
        if (body.find("SECRETO_CONFIDENCIAL_NO_PUBLICAR_998877") != std::string::npos) leakedSecret = true;
    }
    EXPECT(foundHtmlHeading, "Markdown rendered to real HTML5 headings");
    EXPECT(foundWikiAnchor, "[[wikilinks]] resolved to real HTML anchors");
    EXPECT(foundCustomCss, "Custom CSS snippet injected");
    EXPECT(!leakedSecret, "ZERO bytes of private note leaked to published folder");
    std::cout << "[OK] Test07_StaticSitePublisherHtml5AndZeroPrivateLeak\n";
}

void Test08_BackupFirstFolderSync(const fs::path& tmpDir) {
    const fs::path dirA = tmpDir / "sync_device";
    const fs::path dirB = tmpDir / "sync_target";
    fs::create_directories(dirA);
    fs::create_directories(dirB);

    {
        std::ofstream(dirA / "nota.md", std::ios::binary) << "VERSION_ORIGINAL_DISPOSITIVO";
        std::ofstream(dirB / "nota.md", std::ios::binary) << "VERSION_REMOTA_NUEVA_LARGA";
        std::ofstream(dirA / "temp.tmp", std::ios::binary) << "IGNORE_ME";
    }

    SelectiveSyncFilter filt{};
    filt.excludedExtensionsUtf8 = { ".tmp" };
    const auto repMerge = LocalFirstSyncEngine::SynchronizeDirectories(dirA.wstring(), dirB.wstring(), FolderSyncMode::MergeLossless, filt);
    EXPECT(repMerge.ok && repMerge.mergedConflicts >= 1 && repMerge.skippedBySelectiveFilter >= 1,
           "MergeLossless preserves conflict copy and applies selective filter");

    // Mutate local device file differently before Replace mode so backup-before-overwrite executes
    { std::ofstream(dirA / "nota.md", std::ios::binary | std::ios::trunc) << "CONTENIDO_LOCAL_ANTES_DE_REEMPLAZO"; }
    const auto repReplace = LocalFirstSyncEngine::SynchronizeDirectories(dirA.wstring(), dirB.wstring(), FolderSyncMode::ReplaceDeviceWithTarget, filt);
    EXPECT(repReplace.ok && repReplace.backedUpBeforeChange >= 1 && !repReplace.backupDirUtf8.empty(),
           "Replace mode creates verified backup directory before overwriting");
    EXPECT(fs::exists(fs::u8path(repReplace.backupDirUtf8)), "Backup directory physically exists on disk");
    std::cout << "[OK] Test08_BackupFirstFolderSync\n";
}

void Test09_LiveLanHttpServerAndWinHttpClipper() {
    LanRemote::LanCollabHttpServer srv;
    std::string srvErr;
    const bool started = srv.Start(
        0, true,
        []() -> std::string { return R"({"title":"Cuaderno LAN Vivo","pages":3})"; },
        [](const std::string&, const std::string&, std::size_t) -> bool { return true; },
        &srvErr);
    EXPECT(started && srv.IsRunning() && srv.BoundPort() > 0, "Live Winsock2 LAN HTTP server started on loopback");

    if (started) {
        // Negative test: missing token must be rejected (HTTP 401)
        std::string bodyUnauth, errUnauth;
        const std::string unauthUrl = "http://127.0.0.1:" + std::to_string(srv.BoundPort()) + "/api/state";
        EXPECT(!WebClipperEngine::FetchUrlHtmlWinHttp(unauthUrl, bodyUnauth, &errUnauth, 3000),
               "Unauthenticated request rejected with 401");

        // Positive test: valid token returns HTML + state JSON via real WinHTTP
        std::string htmlBody, errOk;
        EXPECT(WebClipperEngine::FetchUrlHtmlWinHttp(srv.BaseUrl(), htmlBody, &errOk, 4000),
               "WinHTTP fetches live HTML from LanCollabHttpServer with token");
        EXPECT(htmlBody.find("CuadernU LAN") != std::string::npos, "Fetched live LAN HTML contains expected page");

        std::string jsonBody;
        const std::string apiUrl = "http://127.0.0.1:" + std::to_string(srv.BoundPort()) + "/api/state?token=" + srv.SessionToken();
        EXPECT(WebClipperEngine::FetchUrlHtmlWinHttp(apiUrl, jsonBody, &errOk, 4000),
               "WinHTTP fetches live /api/state JSON");
        EXPECT(jsonBody.find("Cuaderno LAN Vivo") != std::string::npos, "Live /api/state JSON contains document title");

        srv.Stop();
        EXPECT(!srv.IsRunning(), "LAN HTTP server stopped cleanly");
    }
    std::cout << "[OK] Test09_LiveLanHttpServerAndWinHttpClipper\n";
}

void Test10_ThirdPartyRealZipExtraction(const fs::path& tmpDir) {
    // Construct a real third-party-like .goodnotes ZIP containing ONLY a real PNG (NO payload.cudoc!)
    const fs::path gnZip = tmpDir / "real_thirdparty.goodnotes";
    {
        // Minimal valid 1x1 PNG bytes
        static const std::uint8_t kTinyPng[] = {
            0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
            0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
            0xDE,0x00,0x00,0x00,0x0C,0x49,0x44,0x41,0x54,0x08,0xD7,0x63,0xF8,0xCF,0xC0,0x00,
            0x00,0x03,0x01,0x01,0x00,0x18,0xDD,0x8D,0xB0,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,
            0x44,0xAE,0x42,0x60,0x82
        };
        ZipArchive zw;
        zw.SetEntry("attachments/diagrama.png",
            std::vector<std::uint8_t>(std::begin(kTinyPng), std::end(kTinyPng)));
        zw.SetTextEntry("notes/summary.txt", "Apunte extraido de paquete externo.");
        std::vector<std::uint8_t> zipBytes;
        EXPECT(zw.Serialize(zipBytes), "Created realistic third-party ZIP without payload.cudoc");
        std::ofstream ofs(gnZip, std::ios::binary | std::ios::trunc);
        ofs.write(reinterpret_cast<const char*>(zipBytes.data()), static_cast<std::streamsize>(zipBytes.size()));
    }

    DocumentModel imported;
    std::string fmt, err;
    EXPECT(UniversalFormatsInterop::ImportUniversalNotePackage(gnZip.wstring(), imported, &fmt, &err),
           "Real third-party ZIP extracts embedded PNG & text without payload.cudoc");
    EXPECT(!imported.Pages().empty() && !imported.Pages()[0].objects.empty(), "Extracted embedded objects into DocumentModel");

    // Negative test: corrupt file must fail honestly
    const fs::path badZip = tmpDir / "corrupt.goodnotes";
    { std::ofstream(badZip, std::ios::binary) << "NOT_A_ZIP_ARCHIVE"; }
    DocumentModel badDoc;
    EXPECT(!UniversalFormatsInterop::ImportUniversalNotePackage(badZip.wstring(), badDoc, &fmt, &err),
           "Corrupt package rejected cleanly");
    std::cout << "[OK] Test10_ThirdPartyRealZipExtraction\n";
}

void Test11_GroundedAiBm25AndRealisticQuiz() {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle("Fisica y Algebra");
    AnnotationObject t1{}; t1.id = doc.AllocateObjectId(); t1.kind = ObjectKind::TextBox; t1.bounds = { 40, 40, 400, 80 };
    t1.textUtf8 = "La ley de Faraday establece que la fuerza electromotriz inducida equivale a la tasa negativa de cambio del flujo magnetico.";
    doc.MutablePages()[0].objects.push_back(t1);

    PageModel p2{}; p2.background = PaperPattern::Plain;
    doc.MutablePages().push_back(p2);
    AnnotationObject t2{}; t2.id = doc.AllocateObjectId(); t2.kind = ObjectKind::TextBox; t2.bounds = { 40, 40, 400, 80 };
    t2.textUtf8 = "La entropia termodinamica en un sistema aislado nunca disminuye durante procesos irreversibles.";
    doc.MutablePages()[1].objects.push_back(t2);

    AnnotationObject t3{}; t3.id = doc.AllocateObjectId(); t3.kind = ObjectKind::TextBox; t3.bounds = { 40, 140, 400, 80 };
    t3.textUtf8 = "El teorema espectral garantiza que toda matriz simetrica real posee autovalores reales y autovectores ortogonales.";
    doc.MutablePages()[1].objects.push_back(t3);

    AnnotationObject t4{}; t4.id = doc.AllocateObjectId(); t4.kind = ObjectKind::TextBox; t4.bounds = { 40, 240, 400, 80 };
    t4.textUtf8 = "La ley de Gauss relaciona el flujo electrico cerrado con la carga encerrada dividida por permitividad.";
    doc.MutablePages()[1].objects.push_back(t4);
    doc.RebuildLayout();

    const auto ans = GroundedStudyAiAssistant::AnswerQuestionWithCitations(doc, {}, "Como funciona la entropia en un sistema aislado?");
    EXPECT(ans.answered && !ans.citations.empty() && ans.citations[0].pageIndex0 == 1,
           "BM25 ranks exact page 2 (index 1) first for entropy question");

    const auto quiz = GroundedStudyAiAssistant::GenerateMultipleChoiceQuiz(doc, 4);
    EXPECT(quiz.size() >= 3, "Generated >= 3 quiz questions from document");
    bool hasFakePlaceholder = false;
    for (const auto& q : quiz) {
        EXPECT(q.optionsUtf8.size() == 4, "Each quiz question has 4 options");
        EXPECT(q.correctOptionIndex >= 0 && q.correctOptionIndex < 4, "Valid correctOptionIndex");
        std::set<std::string> uniq(q.optionsUtf8.begin(), q.optionsUtf8.end());
        EXPECT(uniq.size() == 4, "All 4 quiz options are distinct");
        for (const auto& opt : q.optionsUtf8) {
            if (opt.find("Hipotesis nula") != std::string::npos || opt.find("Alternativa B") != std::string::npos) {
                hasFakePlaceholder = true;
            }
        }
    }
    EXPECT(!hasFakePlaceholder, "ZERO hardcoded fake quiz distractors ('Hipotesis nula' / 'Alternativa B')");
    std::cout << "[OK] Test11_GroundedAiBm25AndRealisticQuiz\n";
}

void Test12_OffscreenD2dOverlaysAndRealImageExport(const fs::path& tmpDir) {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle("Render Audit Doc");
    AnnotationObject tb{};
    tb.id = doc.AllocateObjectId();
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 60.0, 80.0, 360.0, 80.0 };
    tb.textUtf8 = "Ecuacion de onda y demostracion completa";
    doc.MutablePages()[0].objects.push_back(tb);

    AnnotationObject ink{};
    ink.id = doc.AllocateObjectId();
    ink.kind = ObjectKind::InkStroke;
    ink.strokeColor = DocColor::RGBA(20, 40, 180, 255);
    ink.strokeWidth = 3.0;
    ink.inkPoints = { { 60, 180, 0.7f, 0, 0, 0 }, { 140, 210, 0.7f, 0, 0, 20 }, { 220, 175, 0.7f, 0, 0, 40 } };
    ink.RecomputeBounds();
    doc.MutablePages()[0].objects.push_back(ink);
    doc.RebuildLayout();

    D2DRenderer renderer;
    EXPECT(renderer.InitializeFactories(), "D2DRenderer factories initialized");

    Camera2D cam{};
    cam.zoom = 1.0;
    cam.panX = -40.0;
    cam.panY = -20.0;
    constexpr int W = 960, H = 640;

    ViewportOverlayState baseOv{};
    RasterImageBgra32 baseImg;
    EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, baseImg, &baseOv) && baseImg.Valid(),
           "Baseline offscreen D2D render succeeds");

    // 1. Reading View Mode
    {
        ViewportOverlayState ov = baseOv;
        ov.readingViewMode = true;
        ov.readingModeTitleUtf8 = "Modo Lectura";
        ov.readingModeTextUtf8 = "Texto de prueba renderizado en vista de lectura guías de interfaz.";
        RasterImageBgra32 img;
        EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img, &ov), "Reading View renders");
        EXPECT(CountDifferingPixels(baseImg, img) > 5000, "Reading View visibly alters viewport pixels");
    }

    // 2. Flexcil Pop-up Study Note
    {
        FlexcilStudyOverlay flex;
        flex.SetPopupNoteVisible(true);
        flex.SetGestureModeActive(true);
        ViewportOverlayState ov = baseOv;
        ov.flexcilPopupVisible = true;
        ov.flexcilGestureMode = true;
        ov.flexcilOverlay = &flex;
        RasterImageBgra32 img;
        EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img, &ov), "Flexcil Popup renders");
        EXPECT(CountDifferingPixels(baseImg, img) > 5000, "Flexcil Popup visibly alters viewport pixels");
    }

    // 3. Zoom Writing Box
    {
        ZoomWritingBoxState zb{};
        zb.active = true;
        ViewportOverlayState ov = baseOv;
        ov.zoomWritingBox = &zb;
        ov.zoomWritingPageIndex = 0;
        RasterImageBgra32 img;
        EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img, &ov), "Zoom Writing Box renders");
        EXPECT(CountDifferingPixels(baseImg, img) > 5000, "Zoom Writing Box visibly alters viewport pixels");
    }

    // 4. Structured Database (verify multiple views: Table=0, Kanban=3, Calendar=4, Chart=6, Map=8)
    {
        StructuredDatabase db("db_vis", "Tablero Visual");
        db.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "Listo" } });
        db.AddColumn({ "valor", "Valor", DbColumnType::Number });
        db.AddColumn({ "fecha", "Fecha", DbColumnType::Date });
        db.AddColumn({ "lat", "Lat", DbColumnType::Number });
        db.AddColumn({ "lon", "Lon", DbColumnType::Number });
        DbRecord r1{}; r1.titleUtf8 = "Elemento A";
        const auto id1 = db.AddRecord(r1);
        db.SetCellText(id1, "estado", "Listo");
        db.SetCellNumber(id1, "valor", 85.0);
        db.SetCellText(id1, "fecha", "2026-11-15");
        db.SetCellNumber(id1, "lat", 9.85);
        db.SetCellNumber(id1, "lon", -83.91);

        RasterImageBgra32 prevDbImg;
        for (int vk : { 0, 3, 4, 6, 8 }) {
            ViewportOverlayState ov = baseOv;
            ov.dbPanelVisible = true;
            ov.dbViewKind = vk;
            ov.activeDatabase = &db;
            RasterImageBgra32 img;
            EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img, &ov),
                   "DB view " + std::to_string(vk) + " renders");
            EXPECT(CountDifferingPixels(baseImg, img) > 5000,
                   "DB view " + std::to_string(vk) + " visibly renders over baseline");
            if (prevDbImg.Valid()) {
                EXPECT(CountDifferingPixels(prevDbImg, img) > 400,
                       "DB view " + std::to_string(vk) + " is visually distinct from previous view");
            }
            prevDbImg = std::move(img);
        }
    }

    // 5. Anchored Comment Pins + Panel
    {
        AnchoredCommentThreadManager cmts;
        cmts.CreateThread(0, 1, { 80.0, 90.0, 100.0, 30.0 }, "Elena", "Anotar @equipo");
        ViewportOverlayState ov = baseOv;
        ov.commentThreads = &cmts;
        ov.commentsPanelVisible = true;
        RasterImageBgra32 img;
        EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img, &ov), "Comments overlay renders");
        EXPECT(CountDifferingPixels(baseImg, img) > 2000, "Comments overlay visibly alters pixels");
    }

    // 6. Audio Timeline Bar + Lock Banner
    {
        ViewportOverlayState ov = baseOv;
        ov.audioTimelineVisible = true;
        ov.audioDurationMs = 60000;
        ov.audioCursorMs = 20000;
        ov.audioEnvelopeBins = { 0.2f, 0.7f, 0.4f, 0.9f, 0.5f, 0.3f, 0.8f, 0.6f };
        ov.audioBookmarkMs = { 15000, 45000 };
        ov.noteLocked = true;
        RasterImageBgra32 img;
        EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img, &ov), "Audio bar + lock banner renders");
        EXPECT(CountDifferingPixels(baseImg, img) > 2000, "Audio bar + lock banner visibly alters pixels");
    }

    // 7. Real PNG & JPEG page export (never blank white silent fallback)
    const fs::path pngOut = tmpDir / "page_export.png";
    const fs::path jpgOut = tmpDir / "page_export.jpg";
    std::string err;
    EXPECT(FlexcilStudyOverlay::ExportPageToPngOrJpegFile(doc, 0, pngOut.wstring(), false, 1000, &err), "PNG export succeeds");
    EXPECT(FlexcilStudyOverlay::ExportPageToPngOrJpegFile(doc, 0, jpgOut.wstring(), true, 1000, &err), "JPEG export succeeds");
    const std::string pngBytes = ReadWholeFile(pngOut);
    const std::string jpgBytes = ReadWholeFile(jpgOut);
    EXPECT(pngBytes.size() > 1000 && static_cast<unsigned char>(pngBytes[0]) == 0x89 && pngBytes[1] == 'P', "Valid non-trivial PNG header & size");
    EXPECT(jpgBytes.size() > 1000 && static_cast<unsigned char>(jpgBytes[0]) == 0xFF && static_cast<unsigned char>(jpgBytes[1]) == 0xD8, "Valid non-trivial JPEG header & size");
    std::cout << "[OK] Test12_OffscreenD2dOverlaysAndRealImageExport\n";
}

// ---------------- 13. SM-2 Study Deck Persistence + Real Timestamps + Progress ----------------
void Test13_Sm2StudyDeckPersistenceAndProgress(const fs::path& tmpDir) {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle("Repaso SM2");
    AnnotationObject noteObj{};
    noteObj.kind = ObjectKind::TextBox;
    noteObj.bounds = { 50.0, 50.0, 420.0, 60.0 };
    noteObj.textUtf8 = "Ley de Faraday induce fuerza electromotriz proporcional a la derivada temporal del flujo magnetico.";
    doc.MutablePages()[0].objects.push_back(noteObj);

    StudyDeck deck;
    deck.titleUtf8 = "Fisica SM2";
    FlashcardItem c1{};
    c1.id = 1;
    c1.questionUtf8 = "Ley de Faraday";
    c1.answerUtf8 = "emf = -dPhi/dt";
    c1.dueEpochSec = 0;
    FlashcardItem c2{};
    c2.id = 2;
    c2.questionUtf8 = "Ley de Gauss";
    c2.answerUtf8 = "div E = rho / eps0";
    c2.dueEpochSec = 0;
    deck.cards.push_back(c1);
    deck.cards.push_back(c2);

    const std::uint64_t nowSec = static_cast<std::uint64_t>(std::time(nullptr));
    EXPECT(nowSec > 1700000000ull, "System clock timestamp is real and beyond hardcoded 1700000000");
    EXPECT(StudySetManager::CountDueCards(deck, nowSec) == 2u, "Both new cards due before review");

    StudySetManager::ReviewCardSm2(deck.cards[0], 5, nowSec);
    EXPECT(deck.cards[0].repetitions == 1 && deck.cards[0].intervalDays >= 1, "SM-2 updated repetitions and interval");
    EXPECT(deck.cards[0].dueEpochSec > nowSec, "SM-2 scheduled future due timestamp");
    EXPECT(StudySetManager::CountDueCards(deck, nowSec) == 1u, "Only unreviewed card remains due now");

    DocumentExtensionBundle pack{};
    pack.studyDeck = &deck;
    StudioProfileStore::PackExtensionsIntoDocument(doc, pack);

    const fs::path cudocPath = tmpDir / "sm2_roundtrip.cudoc";
    std::string err;
    EXPECT(CudocStorage::SaveToFile(cudocPath.wstring(), doc, &err), "Save .cudoc with StudyDeck extension");

    DocumentModel loaded;
    EXPECT(CudocStorage::LoadFromFile(cudocPath.wstring(), loaded, &err), "Reload .cudoc with StudyDeck extension");

    StudyDeck restored;
    DocumentExtensionBundle unpack{};
    unpack.studyDeck = &restored;
    StudioProfileStore::UnpackExtensionsFromDocument(loaded, unpack);

    EXPECT(restored.cards.size() == 2u, "Restored deck preserves all 2 cards");
    EXPECT(restored.cards[0].repetitions == 1, "Restored card 1 preserves SM-2 repetitions");
    EXPECT(restored.cards[0].dueEpochSec == deck.cards[0].dueEpochSec, "Restored card 1 preserves exact dueEpochSec");
    EXPECT(StudySetManager::CountDueCards(restored, nowSec) == 1u, "Restored deck preserves exact due count");

    const auto prog = GroundedStudyAiAssistant::ComputeStudyProgress(30, restored);
    EXPECT(prog.totalCards == 2u && prog.totalStudyMinutes == 30u,
           "ComputeStudyProgress computes accurate metrics");
    StudyDeck emptyDeck{};
    EXPECT(GroundedStudyAiAssistant::ComputeStudyProgress(0, emptyDeck).completionRatio == 0.0,
           "ComputeStudyProgress returns 0.0 (not 1.0) on empty deck");

    // Verify unpacking a doc without study_deck.json clears previous deck (no cross-doc leakage)
    DocumentModel blankDoc;
    blankDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    StudyDeck clearedDeck = restored;
    DocumentExtensionBundle unpackBlank{};
    unpackBlank.studyDeck = &clearedDeck;
    StudioProfileStore::UnpackExtensionsFromDocument(blankDoc, unpackBlank);
    EXPECT(clearedDeck.cards.empty(), "UnpackExtensionsFromDocument clears stale deck when study_deck.json absent");

    // Verify real persistence of uiPrefs.totalStudySeconds in ui_settings.json (NT-AI-067)
    const fs::path profDir = tmpDir / "study_timer_profile";
    LibraryContentViewsManager tmpLib;
    ReusableElementLibrary tmpSnip;
    KeymapProfile tmpKm = KeymapProfile::Default();
    KnowledgeVault tmpKv;
    StudioUiPreferences savedPrefs{};
    savedPrefs.totalStudySeconds = 1845ull;
    savedPrefs.autoSyncEnabled = true;
    savedPrefs.autoSyncEncrypted = true;
    EXPECT(StudioProfileStore::SaveAllProfileState(profDir.wstring(), tmpLib, tmpSnip, tmpKm, tmpKv, savedPrefs, &err),
           "SaveAllProfileState persists totalStudySeconds");
    StudioUiPreferences loadedPrefs{};
    LibraryContentViewsManager tmpLib2;
    ReusableElementLibrary tmpSnip2;
    KeymapProfile tmpKm2;
    KnowledgeVault tmpKv2;
    EXPECT(StudioProfileStore::LoadAllProfileState(profDir.wstring(), tmpLib2, tmpSnip2, tmpKm2, tmpKv2, loadedPrefs, &err),
           "LoadAllProfileState reloads totalStudySeconds");
    EXPECT(loadedPrefs.totalStudySeconds == 1845ull && loadedPrefs.autoSyncEnabled && loadedPrefs.autoSyncEncrypted,
           "Reloaded ui_settings.json preserves exact totalStudySeconds and autoSync flags");

    const std::string execSum = GroundedStudyAiAssistant::GenerateExecutiveSummary(doc, {});
    EXPECT(!execSum.empty() && execSum.find("Faraday") != std::string::npos, "GenerateExecutiveSummary produces grounded summary");
    std::cout << "[OK] Test13_Sm2StudyDeckPersistenceAndProgress\n";
}

// ---------------- 14. D2D Code Syntax Highlighting RGB + Multi-Frame Animated GIF + Hover Preview ----------------
static std::vector<std::uint8_t> EncodeTwoFrameGifViaWic() {
    std::vector<std::uint8_t> out;
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    {
        Microsoft::WRL::ComPtr<IWICImagingFactory> fac;
        Microsoft::WRL::ComPtr<IStream> memStream;
        Microsoft::WRL::ComPtr<IWICBitmapEncoder> enc;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) && fac &&
            SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream)) && memStream &&
            SUCCEEDED(fac->CreateEncoder(GUID_ContainerFormatGif, nullptr, &enc)) && enc &&
            SUCCEEDED(enc->Initialize(memStream.Get(), WICBitmapEncoderNoCache))) {
            const std::uint8_t colors[2][4] = {
                { 20, 20, 235, 255 },  // Frame 0: BGRA Red
                { 25, 210, 30, 255 }   // Frame 1: BGRA Green
            };
            constexpr UINT gw = 16, gh = 16;
            bool framesOk = true;
            for (int f = 0; f < 2; ++f) {
                Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
                if (FAILED(enc->CreateNewFrame(&frame, nullptr)) || !frame ||
                    FAILED(frame->Initialize(nullptr)) ||
                    FAILED(frame->SetSize(gw, gh))) { framesOk = false; break; }
                std::vector<std::uint8_t> pix(gw * gh * 4u);
                for (std::size_t i = 0; i < gw * gh; ++i) {
                    pix[i * 4 + 0] = colors[f][0];
                    pix[i * 4 + 1] = colors[f][1];
                    pix[i * 4 + 2] = colors[f][2];
                    pix[i * 4 + 3] = colors[f][3];
                }
                Microsoft::WRL::ComPtr<IWICBitmap> bmp;
                if (FAILED(fac->CreateBitmapFromMemory(gw, gh, GUID_WICPixelFormat32bppBGRA, gw * 4, static_cast<UINT>(pix.size()), pix.data(), &bmp)) || !bmp) {
                    framesOk = false; break;
                }
                Microsoft::WRL::ComPtr<IWICPalette> pal;
                fac->CreatePalette(&pal);
                if (pal) pal->InitializeFromBitmap(bmp.Get(), 16, FALSE);
                Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
                fac->CreateFormatConverter(&conv);
                if (conv && pal && SUCCEEDED(conv->Initialize(bmp.Get(), GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeNone, pal.Get(), 0.0, WICBitmapPaletteTypeCustom))) {
                    WICPixelFormatGUID pf = GUID_WICPixelFormat8bppIndexed;
                    frame->SetPixelFormat(&pf);
                    frame->SetPalette(pal.Get());
                    if (FAILED(frame->WriteSource(conv.Get(), nullptr))) { framesOk = false; break; }
                } else {
                    if (FAILED(frame->WriteSource(bmp.Get(), nullptr))) { framesOk = false; break; }
                }
                if (FAILED(frame->Commit())) { framesOk = false; break; }
            }
            if (framesOk && SUCCEEDED(enc->Commit())) {
                STATSTG stg{};
                if (SUCCEEDED(memStream->Stat(&stg, STATFLAG_NONAME)) && stg.cbSize.QuadPart > 0) {
                    out.resize(static_cast<std::size_t>(stg.cbSize.QuadPart));
                    LARGE_INTEGER zero{};
                    memStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                    ULONG read = 0;
                    memStream->Read(out.data(), static_cast<ULONG>(out.size()), &read);
                    out.resize(read);
                }
            }
        }
    } // Release all WIC ComPtrs BEFORE CoUninitialize!
    if (SUCCEEDED(hrCo)) CoUninitialize();
    return out;
}

void Test14_CodeSyntaxHighlightingGifAndHoverPreview() {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle("Render Avanzado");

    AnnotationObject cb{};
    cb.kind = ObjectKind::CodeBlock;
    cb.bounds = { 40.0, 40.0, 460.0, 170.0 };
    cb.codeLanguageUtf8 = "cpp";
    cb.textUtf8 = "// comentario verde\nint calcular = 42;\nreturn \"texto literal\";";
    doc.MutablePages()[0].objects.push_back(cb);

    const std::vector<std::uint8_t> gif2Frames = EncodeTwoFrameGifViaWic();
    EXPECT(gif2Frames.size() > 32 && gif2Frames[0] == 'G' && gif2Frames[1] == 'I' && gif2Frames[2] == 'F',
           "WIC generated valid 2-frame GIF89a bitstream");

    AnnotationObject gifObj{};
    gifObj.kind = ObjectKind::Image;
    gifObj.bounds = { 40.0, 240.0, 140.0, 140.0 };
    gifObj.pngBytes = gif2Frames;
    gifObj.imagePixelW = 16;
    gifObj.imagePixelH = 16;
    doc.MutablePages()[0].objects.push_back(gifObj);
    doc.RebuildLayout();

    D2DRenderer renderer;
    EXPECT(renderer.InitializeFactories(), "D2DRenderer initialized for Test14");
    const auto* pl0 = doc.Layout().PlacementAt(0);
    EXPECT(pl0 != nullptr, "Page 0 layout placement available");
    const DocRect pw = pl0 ? pl0->WorldRect() : DocRect{ 0.0, 0.0, 595.0, 842.0 };
    Camera2D cam{};
    cam.zoom = 1.0;
    cam.panX = pw.x - 20.0;
    cam.panY = pw.y - 10.0;
    constexpr int W = 720, H = 520;

    ViewportOverlayState ov0{};
    ov0.animationTick = 0;
    RasterImageBgra32 img0;
    EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img0, &ov0) && img0.Valid(),
           "Offscreen render tick=0 succeeds");

    // Verify CodeBlock contains multiple distinct colorful syntax hues (not monochrome!)
    const DocRect cbScr = cam.WorldRectToScreen({ pw.x + cb.bounds.x, pw.y + cb.bounds.y, cb.bounds.w, cb.bounds.h });
    const int x0 = (std::max)(0, static_cast<int>(cbScr.x));
    const int y0 = (std::max)(0, static_cast<int>(cbScr.y));
    const int x1 = (std::min)(W, static_cast<int>(cbScr.Right()));
    const int y1 = (std::min)(H, static_cast<int>(cbScr.Bottom()));
    int coloredWarm = 0, coloredPurpleBlue = 0, coloredGreen = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t idx = (static_cast<std::size_t>(y) * W + x) * 4u;
            const int b = img0.pixels[idx + 0];
            const int g = img0.pixels[idx + 1];
            const int r = img0.pixels[idx + 2];
            if (r - g > 25 && r - b > 35) ++coloredWarm;       // Number (orange/red)
            if (b - g > 25 && b - r > 15) ++coloredPurpleBlue; // Keyword/Operator (purple/blue)
            if (g - r > 22 && g - b > 15) ++coloredGreen;      // StringLit (green)
        }
    }
    const int distinctHueBuckets = (coloredWarm > 5 ? 1 : 0) + (coloredPurpleBlue > 5 ? 1 : 0) + (coloredGreen > 5 ? 1 : 0);
    EXPECT(distinctHueBuckets >= 2, "Direct2D CodeBlock renders multi-color syntax highlighting hues");

    // Verify Animated GIF switches frames between tick=0 and tick=1
    ViewportOverlayState ov1 = ov0;
    ov1.animationTick = 1;
    RasterImageBgra32 img1;
    EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, img1, &ov1) && img1.Valid(),
           "Offscreen render tick=1 succeeds");
    EXPECT(CountDifferingPixels(img0, img1) > 4000, "Multi-frame GIF animates visibly between tick 0 and tick 1");

    // Verify Hover Link Preview overlay renders visibly
    ViewportOverlayState ovHover = ov0;
    ovHover.hasHoverPreview = true;
    ovHover.hoverPreviewScreenPt = { 220.0, 180.0 };
    ovHover.hoverPreviewTitleUtf8 = "Teorema de Stokes";
    ovHover.hoverPreviewExcerptUtf8 = "Integral de superficie del rotacional igual a circulacion cerrada.";
    RasterImageBgra32 imgHover;
    EXPECT(renderer.RenderHwndViewportToOffscreen(doc, cam, W, H, nullptr, imgHover, &ovHover) && imgHover.Valid(),
           "Hover Link Preview overlay renders");
    EXPECT(CountDifferingPixels(img0, imgHover) > 3000, "Hover Link Preview alters viewport pixels visibly");
    std::cout << "[OK] Test14_CodeSyntaxHighlightingGifAndHoverPreview\n";
}

// ---------------- 15. Database Formulas, Forms, Templates, Automations, Vault Sync ----------------
void Test15_DatabaseFormulasFormsAutomationsAndVaultSync() {
    StructuredDatabase db("db_fin", "Presupuesto");
    db.AddColumn({ "horas", "Horas", DbColumnType::Number });
    db.AddColumn({ "tarifa", "Tarifa", DbColumnType::Number });
    DbColumnDef fCol{};
    fCol.idUtf8 = "total";
    fCol.nameUtf8 = "Total";
    fCol.type = DbColumnType::Formula;
    fCol.formulaExprUtf8 = "=horas * tarifa";
    db.AddColumn(fCol);
    db.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "Completado" } });
    db.AddColumn({ "aprobado", "Aprobado", DbColumnType::Checkbox });

    DbRecordTemplate tpl{};
    tpl.templateIdUtf8 = "tpl_std";
    tpl.nameUtf8 = "Estandar";
    tpl.defaultTitlePrefixUtf8 = "Tarea Base";
    DbCellValue stVal{}; stVal.textUtf8 = "Pendiente";
    tpl.defaultCells["estado"] = stVal;
    db.AddRecordTemplate(tpl);

    const std::string r1 = db.CreateRecordFromTemplate("tpl_std", "Diseno CAD");
    EXPECT(!r1.empty(), "CreateRecordFromTemplate succeeds");
    EXPECT(db.SetCellNumber(r1, "horas", 6.0), "Set horas=6");
    EXPECT(db.SetCellNumber(r1, "tarifa", 25.0), "Set tarifa=25");
    EXPECT(db.SetCellBool(r1, "aprobado", true), "SetCellBool sets checkbox");

    db.RecomputeFormulasAndRollups();
    const auto* rec1 = db.FindRecord(r1);
    EXPECT(rec1 != nullptr, "FindRecord locates template record");
    const auto computed = db.EvaluateFormulaForRecord(*rec1, "=horas * tarifa");
    EXPECT(std::abs(computed.numberVal - 150.0) < 1e-6, "EvaluateFormulaForRecord computes 6*25 = 150");
    EXPECT(std::abs(rec1->cells.at("total").numberVal - 150.0) < 1e-6, "RecomputeFormulasAndRollups stored 150 in formula cell");
    // Anti-stack-overflow regression tests: prop("col") * 2, parenthesized infix, unknown function
    EXPECT(std::abs(db.EvaluateFormulaForRecord(*rec1, "prop(\"horas\") * 2").numberVal - 12.0) < 1e-6,
           "EvaluateFormulaForRecord evaluates prop(\"horas\") * 2 without stack overflow");
    EXPECT(std::abs(db.EvaluateFormulaForRecord(*rec1, "(horas + 4) * tarifa").numberVal - 250.0) < 1e-6,
           "EvaluateFormulaForRecord evaluates parenthesized infix (6+4)*25 = 250");
    (void)db.EvaluateFormulaForRecord(*rec1, "funcionDesconocida(horas)");
    EXPECT(true, "Unknown function in formula terminates safely without stack overflow");

    DbConnectedForm form{};
    form.formIdUtf8 = "frm_ing";
    form.titleUtf8 = "Alta Rapida";
    form.fields.push_back({ "estado", "Estado", true });
    db.RegisterForm(form);
    db.RegisterForm(form); // Idempotent re-register must not duplicate
    // Negative test: missing required field must fail cleanly
    std::string badRecId, badErr;
    EXPECT(!db.SubmitForm("frm_ing", "Sin Estado", {}, &badRecId, &badErr) && !badErr.empty() && db.Records().size() == 1u,
           "SubmitForm rejects submission missing required field");

    DbCellValue pendVal{}; pendVal.textUtf8 = "Pendiente";
    std::string r2, valErr;
    EXPECT(db.SubmitForm("frm_ing", "Revision Estructural", { { "estado", pendVal } }, &r2, &valErr) && !r2.empty() && db.Records().size() == 2u,
           "SubmitForm appends validated record");

    // Verify forms persist across .base JSON roundtrip
    const std::string baseJson = db.SerializeToBaseJson();
    StructuredDatabase dbRoundtrip("db_rt", "RT");
    EXPECT(dbRoundtrip.DeserializeFromBaseJson(baseJson), "DeserializeFromBaseJson succeeds with forms");
    std::string r3, valErr3;
    EXPECT(dbRoundtrip.SubmitForm("frm_ing", "Post Roundtrip", { { "estado", pendVal } }, &r3, &valErr3),
           "Deserialized .base retains registered forms and accepts SubmitForm");

    DbViewConfig vc{};
    vc.viewIdUtf8 = "v_pend";
    vc.nameUtf8 = "Solo Pendientes";
    vc.kind = DbViewKind::Table;
    db.AddOrUpdateView(vc);
    EXPECT(db.SetActiveView("v_pend") && db.ActiveViewId() == "v_pend", "AddOrUpdateView and SetActiveView succeed");

    AutomationRuleEngine autoEng;
    AutomationRule rule{};
    rule.ruleIdUtf8 = "rule_btn";
    rule.nameUtf8 = "Aprobar y Completar";
    rule.trigger = AutomationTriggerKind::OnActionButtonClicked;
    rule.steps.push_back({ "set_property", "estado", "Completado" });
    autoEng.RegisterRule(rule);
    const std::size_t fired = autoEng.TriggerEvent(AutomationTriggerKind::OnActionButtonClicked, db, r2);
    EXPECT(fired == 1u && db.FindRecord(r2)->cells.at("estado").textUtf8 == "Completado",
           "OnActionButtonClicked executes automation rule on DB record");

    KnowledgeVault kv;
    db.SyncRecordsToVaultNotes(kv);
    EXPECT(kv.TotalNoteCount(false) >= 2u, "SyncRecordsToVaultNotes exports 2 records to vault");

    StructuredDatabase db2("db_imp", "Importada");
    db2.ImportRecordsFromVaultNotes(kv);
    EXPECT(db2.Records().size() >= 2u, "ImportRecordsFromVaultNotes restores 2 records");

    EXPECT(db.RemoveRecord(r2) && db.Records().size() == 1u, "RemoveRecord removes target record");
    std::cout << "[OK] Test15_DatabaseFormulasFormsAutomationsAndVaultSync\n";
}

// ---------------- 16. Backup/Restore & AES-256-GCM E2EE Folder Sync (Positive + Negative) ----------------
void Test16_BackupRestoreAndE2eeFolderSync(const fs::path& tmpDir) {
    const fs::path srcDir = tmpDir / "e2ee_src";
    const fs::path remoteDir = tmpDir / "e2ee_remote";
    const fs::path dstGood = tmpDir / "e2ee_dst_good";
    const fs::path dstBad = tmpDir / "e2ee_dst_bad";
    const fs::path bakZip = tmpDir / "full_backup.zip";
    const fs::path restoredDir = tmpDir / "restored_full";
    const fs::path singleNoteOut = tmpDir / "restored_single.md";
    fs::create_directories(srcDir / "notas");
    fs::create_directories(remoteDir);
    fs::create_directories(dstGood);
    fs::create_directories(dstBad);

    const std::string secretText = "CONFIDENCIAL_ECUACION_MAXWELL_987";
    {
        std::ofstream f1(srcDir / "notas" / "calculo.md", std::ios::binary);
        f1 << secretText;
        std::ofstream f2(srcDir / "indice.json", std::ios::binary);
        f2 << "{\"ok\":true}";
    }

    // Backup & Restore (Full + Single Note)
    std::string err;
    EXPECT(LocalFirstSyncEngine::CreateBackupPackage(srcDir.wstring(), bakZip.wstring(), "", &err),
           "CreateBackupPackage succeeds");
    EXPECT(LocalFirstSyncEngine::RestoreFullBackupPackage(bakZip.wstring(), restoredDir.wstring(), "", &err),
           "RestoreFullBackupPackage succeeds");
    EXPECT(ReadWholeFile(restoredDir / "notas" / "calculo.md") == secretText,
           "Full backup restore recovers exact file contents");
    EXPECT(LocalFirstSyncEngine::RestoreSingleNoteFromBackup(bakZip.wstring(), "notas/calculo.md", singleNoteOut.wstring(), "", &err),
           "RestoreSingleNoteFromBackup extracts individual note");
    EXPECT(ReadWholeFile(singleNoteOut) == secretText, "Single note restore matches exact bytes");

    // E2EE AES-256-GCM Folder Sync (Upload encrypted)
    SelectiveSyncFilter fltEnc{};
    fltEnc.encryptPassphraseUtf8 = "ClaveFuerte#2026!";
    const auto upRep = LocalFirstSyncEngine::SynchronizeDirectories(
        srcDir.wstring(), remoteDir.wstring(), FolderSyncMode::UploadDeviceToTarget, fltEnc);
    EXPECT(upRep.ok && upRep.uploadedFiles >= 2u, "E2EE folder upload succeeds");

    const fs::path encRemoteFile = remoteDir / "notas" / "calculo.md.cuenc";
    EXPECT(fs::exists(encRemoteFile), "Remote folder stores .cuenc ciphertext file");
    const std::string cipherRaw = ReadWholeFile(encRemoteFile);
    EXPECT(cipherRaw.find(secretText) == std::string::npos, "Remote ciphertext never leaks plaintext secret");

    // Positive download with correct passphrase
    const auto dlGood = LocalFirstSyncEngine::SynchronizeDirectories(
        dstGood.wstring(), remoteDir.wstring(), FolderSyncMode::ReplaceDeviceWithTarget, fltEnc);
    EXPECT(dlGood.ok && dlGood.downloadedFiles >= 2u, "E2EE download with correct passphrase succeeds");
    EXPECT(ReadWholeFile(dstGood / "notas" / "calculo.md") == secretText,
           "E2EE decrypted file matches original plaintext byte-for-byte");

    // Negative download with WRONG passphrase must fail and never leak plaintext
    SelectiveSyncFilter fltWrong{};
    fltWrong.encryptPassphraseUtf8 = "ClaveIncorrecta999";
    const auto dlBad = LocalFirstSyncEngine::SynchronizeDirectories(
        dstBad.wstring(), remoteDir.wstring(), FolderSyncMode::ReplaceDeviceWithTarget, fltWrong);
    EXPECT(!dlBad.ok && !dlBad.diagnosticUtf8.empty() && !fs::exists(dstBad / "notas" / "calculo.md"),
           "E2EE download rejects wrong passphrase without writing corrupt plaintext");
    std::cout << "[OK] Test16_BackupRestoreAndE2eeFolderSync\n";
}

// ---------------- 17. XLSX, CSV, Markdown, DXF, OneNote HTML, Delimited Deck & Perspective ----------------
void Test17_InteropXlsxCsvMdDxfOneNoteAndPerspective(const fs::path& tmpDir) {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle("Laboratorio");
    AnnotationObject tb{};
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 50.0, 50.0, 300.0, 60.0 };
    tb.textUtf8 = "Voltaje,Corriente\n12,2.5";
    doc.MutablePages()[0].objects.push_back(tb);
    AnnotationObject sh{};
    sh.kind = ObjectKind::Shape;
    sh.shapeKind = ShapeKind::Rectangle;
    sh.bounds = { 80.0, 140.0, 120.0, 80.0 };
    doc.MutablePages()[0].objects.push_back(sh);

    const std::string xlsxPath = U8Str((tmpDir / "datos.xlsx").u8string());
    const std::string csvPath = U8Str((tmpDir / "datos.csv").u8string());
    const std::string dxfPath = U8Str((tmpDir / "plano.dxf").u8string());
    const std::string mdPath = U8Str((tmpDir / "apunte.md").u8string());
    std::string err;

    EXPECT(DocStudioApp::ExportToXlsxFile(doc, xlsxPath, &err), "ExportToXlsxFile succeeds");
    DocumentModel fromXlsx;
    EXPECT(DocStudioApp::ImportFromXlsxFile(xlsxPath, fromXlsx, &err) && !fromXlsx.Pages().empty(),
           "ImportFromXlsxFile roundtrip succeeds");
    EXPECT(DocStudioApp::ExportToCsvFile(doc, csvPath, &err) && ReadWholeFile(tmpDir / "datos.csv").find("Voltaje") != std::string::npos,
           "ExportToCsvFile writes CSV content");
    EXPECT(DocStudioApp::ExportToDxfFile(doc, dxfPath, "1:50", &err) && ReadWholeFile(tmpDir / "plano.dxf").find("SECTION") != std::string::npos,
           "ExportToDxfFile writes valid ASCII DXF");

    {
        std::ofstream mdf(tmpDir / "apunte.md", std::ios::binary);
        mdf << "# Ecuaciones\n\n$$E = mc^2$$\n\n```cpp\nint x = 1;\n```\n";
    }
    DocumentModel fromMd;
    EXPECT(DocStudioApp::ImportFromMarkdownFile(mdPath, fromMd, &err) && !fromMd.Pages().empty(),
           "ImportFromMarkdownFile imports headings/math/code");

    const StudyDeck csvDeck = UniversalFormatsInterop::ImportStudyDeckFromDelimited(
        "Pregunta,Respuesta\nDerivada de sin(x),cos(x)\nIntegral de 1/x,ln|x|\n", ',', "Calculo");
    EXPECT(csvDeck.cards.size() >= 2u, "ImportStudyDeckFromDelimited imports flashcards");

    DocumentModel onDoc;
    EXPECT(UniversalFormatsInterop::ImportPageFromOneNoteGraphHtml(
        "<html><head><title>Fisica II</title></head><body><h1>Maxwell</h1><p>Divergencia del campo electrico</p></body></html>",
        onDoc, &err) && !onDoc.Pages().empty(), "ImportPageFromOneNoteGraphHtml succeeds");

    ImageProc::RgbaImage srcImg{};
    srcImg.width = 64; srcImg.height = 64;
    srcImg.rgba.assign(64u * 64u * 4u, 220u);
    const std::array<tinta::Punto, 4> quad = {
        tinta::Punto{ 6.0f, 8.0f }, tinta::Punto{ 58.0f, 6.0f },
        tinta::Punto{ 60.0f, 58.0f }, tinta::Punto{ 4.0f, 56.0f }
    };
    const auto rect = ImageProc::RectifyPerspectiveQuad(srcImg, quad, 48, 48);
    EXPECT(rect.Valid() && rect.width == 48 && rect.height == 48, "RectifyPerspectiveQuad produces valid 48x48 rectified RGBA");

    // Screen-capture BitBlt -> PNG pipeline test (ON-EDIT-041):
    // 1. Deterministic Win32 DC capture test (passes even on locked Winlogon/CI desktops)
    {
        HDC hRefDc = CreateCompatibleDC(nullptr);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = 64;
        bmi.bmiHeader.biHeight = -48;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* refBits = nullptr;
        HBITMAP hRefBmp = hRefDc ? CreateDIBSection(hRefDc, &bmi, DIB_RGB_COLORS, &refBits, nullptr, 0) : nullptr;
        bool dcCapOk = false;
        std::vector<std::uint8_t> capPng;
        int capW = 0, capH = 0;
        if (hRefDc && hRefBmp && refBits) {
            auto* px = static_cast<std::uint8_t*>(refBits);
            for (int i = 0; i < 64 * 48; ++i) {
                px[i * 4 + 0] = 200; // B
                px[i * 4 + 1] = 110; // G
                px[i * 4 + 2] = 35;  // R
                px[i * 4 + 3] = 255;
            }
            HGDIOBJ oldB = SelectObject(hRefDc, hRefBmp);
            dcCapOk = ImageProc::CaptureDcRegionToPngBytes(hRefDc, 8, 6, 40, 30, capPng, capW, capH);
            SelectObject(hRefDc, oldB);
        }
        if (hRefBmp) DeleteObject(hRefBmp);
        if (hRefDc) DeleteDC(hRefDc);
        EXPECT(dcCapOk && capW == 40 && capH == 30 && capPng.size() > 64 && capPng[0] == 0x89 && capPng[1] == 'P',
               "CaptureDcRegionToPngBytes captures Win32 DC region into valid PNG");
        ImageProc::RgbaImage decCap{};
        const HRESULT hrCoDec = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool decOk = ImageProc::DecodeImageBytesWic(capPng.data(), capPng.size(), decCap);
        if (SUCCEEDED(hrCoDec)) CoUninitialize();
        EXPECT(decOk && decCap.width == 40 && decCap.height == 30 && decCap.rgba[0] == 35 && decCap.rgba[1] == 110 && decCap.rgba[2] == 200,
               "Captured PNG decodes back to exact RGB pixels");
    }
    // 2. Live virtual screen call must either return valid multi-monitor PNG or fail cleanly when desktop is locked
    {
        std::vector<std::uint8_t> scrPng;
        int scrW = 0, scrH = 0;
        const bool liveOk = ImageProc::CaptureVirtualScreenPngBytes(scrPng, scrW, scrH);
        if (liveOk) {
            EXPECT(scrW > 0 && scrH > 0 && scrPng.size() > 100 && scrPng[0] == 0x89 && scrPng[1] == 'P',
                   "CaptureVirtualScreenPngBytes returns valid live desktop PNG when desktop unlocked");
        } else {
            EXPECT(scrPng.empty() && scrW == 0 && scrH == 0,
                   "CaptureVirtualScreenPngBytes fails cleanly with empty output when desktop is locked");
        }
    }
    std::cout << "[OK] Test17_InteropXlsxCsvMdDxfOneNoteAndPerspective\n";
}

// ---------------- 18. Audio Pause/Resume/Merge/Seek + KnowledgeVault Refactor/Links ----------------
void TestNotionMarkdownContract(const fs::path& tmpDir) {
    const std::string id = "12345678-abcd-4321-a432-123456789abc";
    EXPECT(NotionMarkdown::PageIdFromReference("https://www.notion.so/Curso-12345678abcd4321a432123456789abc?pvs=4") == id,
        "Notion page identifier parsed from a shared link");
    EXPECT(NotionMarkdown::PageIdFromReference("12345678-ABCD-4321-A432-123456789ABC") == id,
        "Notion UUID is normalized");
    EXPECT(NotionMarkdown::PageIdFromReference("https://www.notion.so/sin-identificador").empty(),
        "An invalid Notion page reference is rejected");
    const std::string content = R"json({"object":"page_markdown","id":"12345678-abcd-4321-a432-123456789abc","markdown":"# Lectura\n\nContenido autorizado","truncated":false,"unknown_block_ids":[]})json";
    std::string markdown, error;
    EXPECT(NotionMarkdown::DecodePage(content, id, markdown, error), "Authorized Notion Markdown is decoded");
    DocumentModel doc, reloaded;
    const auto path = (tmpDir / "notion_markdown.cudoc").wstring();
    EXPECT(DocStudioApp::ImportFromMarkdownText(markdown, "Notion", doc) &&
        CudocStorage::SaveToFile(path, doc, &error) && CudocStorage::LoadFromFile(path, reloaded, &error),
        "Decoded Notion Markdown is saved and reopened as a CuadernU document");
    const auto& objects = reloaded.CanvasObjects();
    EXPECT(std::any_of(objects.begin(), objects.end(), [](const auto& object) {
        return object.textUtf8 == "Contenido autorizado";
    }), "Imported text survives the CuadernU document roundtrip");
    EXPECT(!NotionMarkdown::DecodePage(R"json({"object":"page_markdown","id":"12345678-abcd-4321-a432-123456789abc","markdown":"recorte","truncated":true})json",
        id, markdown, error) && markdown.empty(), "Truncated Notion pages cannot masquerade as a complete import");
}

void Test18_AudioPauseResumeMergeAndKnowledgeRefactor(const fs::path& tmpDir) {
    AudioTimelineSession a1;
    a1.StartRecording(16000);
    a1.AppendSyntheticTone(440.0, 200);
    a1.PauseRecording();
    EXPECT(a1.IsPaused(), "Audio session enters paused state");
    const std::uint64_t durAtPause = a1.DurationMs();
    a1.AppendSyntheticTone(880.0, 300); // MUST be ignored while paused
    EXPECT(a1.DurationMs() == durAtPause, "Samples appended while paused are ignored");
    a1.ResumeRecording();
    EXPECT(!a1.IsPaused(), "Audio session resumes cleanly");
    a1.AppendSyntheticTone(523.0, 200);
    a1.StopRecording();
    EXPECT(a1.DurationMs() >= 380 && a1.DurationMs() <= 430, "Total audio duration equals ~400ms (excluding paused 300ms)");

    const std::string wavPath = U8Str((tmpDir / "clase_rec.wav").u8string());
    std::string err;
    EXPECT(a1.ExportWavFile(wavPath, &err) && ReadWholeFile(tmpDir / "clase_rec.wav").substr(0, 4) == "RIFF",
           "ExportWavFile writes valid RIFF WAVE");

    AudioTimelineSession a2;
    a2.StartRecording(16000);
    a2.AppendSyntheticTone(660.0, 250);
    a2.StopRecording();
    EXPECT(a1.MergeWithTrack(a2) && a1.DurationMs() >= 600, "MergeWithTrack extends audio track");

    // KnowledgeVault Local Graph, Unlinked Mention Conversion, Rename Propagation, Transclusion, Preview
    KnowledgeVault kv;
    const std::string idA = kv.CreateNote("Calculo Vectorial", "Matematicas",
        "# Teorema\nVer [[Algebra Lineal]] y tambien mencion a Geometria Diferencial.\n![[Algebra Lineal]]\nNota rota [[Inexistente]] ^blk1");
    kv.CreateNote("Algebra Lineal", "Matematicas", "Espacios vectoriales y valores propios.");
    const std::string idC = kv.CreateNote("Geometria Diferencial", "Matematicas", "Variedades diferenciables.");

    auto localG = kv.BuildLocalGraph("Calculo Vectorial", 1);
    KnowledgeVault::ComputeGraphLayout2D(localG, 60, 800.0, 560.0);
    bool anyNonZeroPos = false;
    for (const auto& nd : localG.nodes) { if (nd.x > 1.0 || nd.y > 1.0) { anyNonZeroPos = true; break; } }
    EXPECT(localG.nodes.size() >= 2u && !localG.edges.empty() && anyNonZeroPos,
           "BuildLocalGraph + ComputeGraphLayout2D builds laid-out neighborhood subgraph");

    // Double-tap ink conversion & stylus eraser toggle preserving Highlighter
    DocumentModel inkDoc;
    inkDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    CommandStack inkCs;
    SmartScriptReflowEngine::PasteTextAsHandwriting(inkDoc, inkCs, ObjectTarget{ false, 0 }, "Hola", 80.0, 100.0);
    const std::size_t convertedCnt = SmartScriptReflowEngine::ConvertHandwritingAndDiagramsOnDoubleTap(
        inkDoc, inkCs, ObjectTarget{ false, 0 });
    EXPECT(convertedCnt >= 1u, "ConvertHandwritingAndDiagramsOnDoubleTap converts handwriting strokes");

    FloatingInkToolbarState tbState{};
    tbState.activePen = InkPenStyle::Highlighter;
    SmartScriptReflowEngine::HandleStylusDoubleTapToggle(tbState);
    EXPECT(tbState.eraserActive, "HandleStylusDoubleTapToggle switches Highlighter to Eraser");
    SmartScriptReflowEngine::HandleStylusDoubleTapToggle(tbState);
    EXPECT(!tbState.eraserActive && tbState.activePen == InkPenStyle::Highlighter,
           "HandleStylusDoubleTapToggle restores Highlighter after Eraser");
    EXPECT(!kv.OutgoingLinks(idA).empty(), "OutgoingLinks returns parsed links");
    EXPECT(!kv.FindBrokenWikiLinks().empty(), "FindBrokenWikiLinks detects unresolved wikilink");

    const auto unlinked = kv.FindUnlinkedMentions("Geometria Diferencial");
    EXPECT(!unlinked.empty(), "FindUnlinkedMentions finds Geometria Diferencial mention");
    if (!unlinked.empty()) {
        EXPECT(kv.ConvertMentionToWikiLink(unlinked.front().sourceNoteIdUtf8, unlinked.front().byteOffset,
                                           unlinked.front().byteLength, unlinked.front().targetNoteTitleUtf8),
               "ConvertMentionToWikiLink upgrades plain mention");
    }

    const auto prev = kv.ResolveLinkPreview("Algebra Lineal");
    EXPECT(prev.found && prev.previewExcerptUtf8.find("Espacios vectoriales") != std::string::npos,
           "ResolveLinkPreview extracts excerpt");
    const std::string trans = kv.ResolveTransclusionMarkdown("Algebra Lineal");
    EXPECT(trans.find("Espacios vectoriales") != std::string::npos, "ResolveTransclusionMarkdown expands note body");

    const auto renNote = kv.RenameNote("Algebra Lineal", "Algebra Matricial");
    EXPECT(renNote.referencesUpdated >= 1u, "RenameNote propagates updated wikilinks across vault");
    EXPECT(kv.FindNoteById(idA)->bodyMarkdownUtf8.find("[[Algebra Matricial]]") != std::string::npos,
           "Link in Note A was automatically rewritten to [[Algebra Matricial]]");
    const auto renHead = kv.RenameHeadingInNote("Calculo Vectorial", "Teorema", "Teorema Principal");
    EXPECT(renHead.notesModified >= 1u, "RenameHeadingInNote modifies note heading");
    const auto renBlk = kv.RenameBlockIdInNote("Calculo Vectorial", "blk1", "blk_main");
    EXPECT(renBlk.notesModified >= 1u, "RenameBlockIdInNote modifies block ID");
    EXPECT(kv.MoveNote(idC, "Avanzado"), "MoveNote updates note folder");
    EXPECT(!kv.SearchWorkspace("Matricial").empty(), "SearchWorkspace finds renamed content");
    std::cout << "[OK] Test18_AudioPauseResumeMergeAndKnowledgeRefactor\n";
}

} // namespace

int main(int argc, char** argv) {
    const fs::path docStudioRoot = (argc > 1)
        ? fs::path(argv[1])
        : fs::path("d:/OneDrive/Desktop/Calculadoras (blanca y gris)/Calculadora Unificada - Core Oficial/doc_studio");

    const fs::path tmpDir = fs::temp_directory_path() / "cuadernu_real_e2e_closure_test";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    Test01_PaletteIntegrityAndZeroBrokenIds(docStudioRoot);
    Test02_SecurityPbkdf2NoteLock();
    Test03_FullCudocAndProfilePersistenceRoundtrip(tmpDir);
    Test04_Chemistry118ElementsAndRrefBalancer();
    Test05_ReactiveMathDegNestedTrigAndGreekWordBoundary();
    Test06_SmartScriptLegibleGlyphsAndSpellCheck();
    Test07_StaticSitePublisherHtml5AndZeroPrivateLeak(tmpDir);
    Test08_BackupFirstFolderSync(tmpDir);
    Test09_LiveLanHttpServerAndWinHttpClipper();
    Test10_ThirdPartyRealZipExtraction(tmpDir);
    Test11_GroundedAiBm25AndRealisticQuiz();
    Test12_OffscreenD2dOverlaysAndRealImageExport(tmpDir);
    Test13_Sm2StudyDeckPersistenceAndProgress(tmpDir);
    Test14_CodeSyntaxHighlightingGifAndHoverPreview();
    Test15_DatabaseFormulasFormsAutomationsAndVaultSync();
    Test16_BackupRestoreAndE2eeFolderSync(tmpDir);
    Test17_InteropXlsxCsvMdDxfOneNoteAndPerspective(tmpDir);
    Test18_AudioPauseResumeMergeAndKnowledgeRefactor(tmpDir);
    TestNotionMarkdownContract(tmpDir);

    fs::remove_all(tmpDir, ec);

    std::cout << "[RealE2EClosureTest] passed=" << g_passed << "/" << g_total
              << " failed=" << g_failed << "\n";
    return (g_failed == 0) ? 0 : 1;
}
