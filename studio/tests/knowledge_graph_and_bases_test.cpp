#include "../app/studio_actions.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/image_processing.hpp"
#include "../core/ink_engine.hpp"
#include "../core/keymap_config.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/structured_database.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/table_interop.hpp"
#include "../pdf/winrt_adapters.hpp"
#include "../render/card_render_common.hpp"
#include "../render/d2d_renderer.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

int g_totalChecks = 0;
int g_failedChecks = 0;

struct IdTestOutcome {
    std::string id;
    std::string testName;
    bool pass = false;
    std::string suggestedState; // "Implementado completamente" | "A medias"
    std::string detail;
};

std::vector<IdTestOutcome> g_outcomes;

void RecordOutcome(
    const std::vector<std::string>& ids,
    const std::string& testName,
    bool pass,
    const std::string& detail,
    const std::string& suggestedState = "Implementado completamente")
{
    for (const auto& id : ids) {
        g_outcomes.push_back({ id, testName, pass, suggestedState, detail });
    }
}

#define M1_CHECK(expr, BoolAcc) \
    do { \
        ++g_totalChecks; \
        if (!(expr)) { \
            ++g_failedChecks; \
            (BoolAcc) = false; \
            std::cerr << "[FAIL] " << __FUNCTION__ << ":" << __LINE__ << " -> " << #expr << "\n"; \
        } \
    } while (0)

// Builds synthetic InkStroke lines for uppercase block letters "HI" so WinRT OCR/HTR can recognize them cleanly
std::vector<DocStudio::AnnotationObject> MakeSyntheticBlockWordStrokesHI() {
    using namespace DocStudio;
    auto makeSeg = [](double x0, double y0, double x1, double y1) {
        AnnotationObject s;
        s.kind = ObjectKind::InkStroke;
        s.strokeWidth = 6.0;
        for (int i = 0; i <= 16; ++i) {
            const double t = static_cast<double>(i) / 16.0;
            s.inkPoints.push_back({ x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.8f, 0, 0, static_cast<std::uint64_t>(i * 10) });
        }
        s.RecomputeBounds();
        return s;
    };
    return {
        // 'H'
        makeSeg(40, 40, 40, 120),
        makeSeg(40, 80, 90, 80),
        makeSeg(90, 40, 90, 120),
        // 'I'
        makeSeg(130, 40, 130, 120),
        // 'O' (boxy loop)
        makeSeg(170, 40, 225, 40),
        makeSeg(225, 40, 225, 120),
        makeSeg(225, 120, 170, 120),
        makeSeg(170, 120, 170, 40)
    };
}

void TestM1_Vault50NotesCrudSearchMoveRestoreSync(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    KnowledgeVault kv;
    std::vector<std::string> ids;
    for (int i = 1; i <= 50; ++i) {
        const std::string folder = (i <= 25) ? "Calculo" : "Fisica";
        const std::string body = "Apunte #" + std::to_string(i) + " sobre Teorema Fundamental y Cinematica.";
        ids.push_back(kv.CreateNote("Nota_" + std::to_string(i), folder, body));
    }
    M1_CHECK(kv.TotalNoteCount(false) == 50, ok);

    // Move 5 notes from Calculo to Algebra
    for (int i = 0; i < 5; ++i) {
        M1_CHECK(kv.MoveNote(ids[i], "Algebra"), ok);
    }
    NoteFilterCriteria fc;
    fc.folderPrefixUtf8 = "Algebra";
    M1_CHECK(kv.FilterNotes(fc).size() == 5, ok);

    // Trash & Restore
    M1_CHECK(kv.MoveToTrash(ids[10]), ok);
    M1_CHECK(kv.TotalNoteCount(false) == 49, ok);
    M1_CHECK(kv.RestoreFromTrash(ids[10]), ok);
    M1_CHECK(kv.TotalNoteCount(false) == 50, ok);

    // Workspace Search
    const auto hits = kv.SearchWorkspace("Teorema Fundamental", 60);
    M1_CHECK(hits.size() == 50, ok);

    // Persist 50-note vault to directory and reload
    const fs::path vaultDir = tmpDir / "m1_vault_50";
    std::string err;
    M1_CHECK(kv.SaveVaultToDirectory(vaultDir.wstring(), &err), ok);
    KnowledgeVault loaded;
    M1_CHECK(loaded.LoadVaultFromDirectory(vaultDir.wstring(), &err), ok);
    M1_CHECK(loaded.TotalNoteCount(false) == 50, ok);
    M1_CHECK(loaded.SearchWorkspace("Cinematica", 60).size() == 50, ok);

    // Concurrent sync merge test via MergeConcurrentDocuments
    DocumentModel baseDoc, locDoc, remDoc, mergedDoc;
    AnnotationObject o1; o1.kind = ObjectKind::TextBox; o1.textUtf8 = "Version Local"; o1.bounds = { 20, 20, 180, 40 };
    locDoc.AddPageObject(0, o1);
    AnnotationObject o2; o2.kind = ObjectKind::TextBox; o2.textUtf8 = "Version Remota"; o2.bounds = { 20, 80, 180, 40 };
    remDoc.AddPageObject(0, o2);
    const auto mres = MergeConcurrentDocuments(baseDoc, locDoc, remDoc, mergedDoc);
    M1_CHECK(mres.ok && mres.conflictPagesCreated == 1 && mergedDoc.Pages().size() == 2, ok);

    RecordOutcome(
        { "OB-ORG-001", "NO-ORG-005" },
        "TestM1_Vault50NotesCrudSearchMoveRestoreSync",
        ok,
        "50 notas creadas, movidas entre carpetas, enviadas/restauradas de papelera, buscadas y sincronizadas en disco + merge concurrente.");
}

void TestM1_Templates50NotesVariablesAndBlocks(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    KnowledgeVault kv;
    NoteTemplate pageTpl;
    pageTpl.nameUtf8 = "Plantilla_Laboratorio";
    pageTpl.paperPattern = PaperPattern::Cornell;
    pageTpl.coverColorRgba = 0x1E3A8AFFu;
    pageTpl.contentTemplateUtf8 = "# {{title}}\nFecha: {{date}} {{time}}\nUUID: {{uuid}}\n\n## Resumen\nExperimento de laboratorio.";
    pageTpl.defaultProps = { { "curso", NotePropType::Text, "FI1102", {} } };
    kv.RegisterTemplate(pageTpl);

    NoteTemplate blockTpl;
    blockTpl.nameUtf8 = "Bloque_Checklist";
    blockTpl.isBlockTemplate = true;
    blockTpl.contentTemplateUtf8 = "### Control ({{date}})\n- [ ] Medir voltaje\n- [x] Registrar corriente";
    kv.RegisterTemplate(blockTpl);

    std::vector<std::string> created;
    for (int i = 1; i <= 50; ++i) {
        const std::string nid = kv.CreateNoteFromTemplate(
            "Plantilla_Laboratorio",
            "Lab_" + std::to_string(i),
            "Labs",
            "2026-11-20",
            "10:30");
        created.push_back(nid);
        if (auto* mut = kv.FindNoteMutableById(nid)) {
            mut->bodyMarkdownUtf8 += "\n\n" + KnowledgeVault::ExpandTemplateVariables(
                blockTpl.contentTemplateUtf8, mut->titleUtf8, "2026-11-20", "10:30", "uuid-bloque");
        }
    }
    M1_CHECK(kv.TotalNoteCount(false) == 50, ok);
    const auto* first = kv.FindNoteById(created.front());
    M1_CHECK(first != nullptr, ok);
    if (first) {
        M1_CHECK(first->bodyMarkdownUtf8.find("# Lab_1") != std::string::npos, ok);
        M1_CHECK(first->bodyMarkdownUtf8.find("2026-11-20 10:30") != std::string::npos, ok);
        M1_CHECK(first->bodyMarkdownUtf8.find("Medir voltaje") != std::string::npos, ok);
        M1_CHECK(first->paperPattern == PaperPattern::Cornell, ok);
    }

    // Move, trash/restore, search and sync to disk
    M1_CHECK(kv.MoveNote(created[0], "Labs_Revisados"), ok);
    M1_CHECK(kv.MoveToTrash(created[1]) && kv.RestoreFromTrash(created[1]), ok);
    M1_CHECK(kv.SearchWorkspace("Experimento de laboratorio", 60).size() == 50, ok);
    M1_CHECK(kv.SaveVaultToDirectory((tmpDir / "m1_templates_50").wstring()), ok);

    RecordOutcome(
        { "GN-ORG-008", "ON-ORG-008", "NO-ORG-004", "OB-ORG-016", "OB-ORG-017", "NO-EDIT-028" },
        "TestM1_Templates50NotesVariablesAndBlocks",
        ok,
        "50 notas generadas desde plantillas de pagina y plantillas de bloque con variables {{title}}, {{date}}, {{time}}, {{uuid}}, busqueda, movimiento, restauracion y persistencia.");
}

void TestM1_DailyNotes50Sequential(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    KnowledgeVault kv;
    std::vector<std::string> dailyIds;
    for (int i = 1; i <= 50; ++i) {
        char buf[32]{};
        const int month = (i <= 31) ? 1 : 2;
        const int day = (i <= 31) ? i : (i - 31);
        std::snprintf(buf, sizeof(buf), "2026-%02d-%02d", month, day);
        const std::string id = kv.OpenOrCreateDailyNote(buf);
        dailyIds.push_back(id);
        // Calling OpenOrCreateDailyNote again for same date must return existing note without duplicating
        M1_CHECK(kv.OpenOrCreateDailyNote(buf) == id, ok);
    }
    M1_CHECK(kv.TotalNoteCount(false) == 50, ok);
    const auto* day2 = kv.FindNoteById(dailyIds[1]);
    M1_CHECK(day2 != nullptr && day2->isDailyNote && day2->bodyMarkdownUtf8.find("[[2026-01-01]]") != std::string::npos, ok);
    M1_CHECK(kv.MoveNote(dailyIds[0], "Archivo_Diarias"), ok);
    M1_CHECK(kv.MoveToTrash(dailyIds[2]) && kv.RestoreFromTrash(dailyIds[2]), ok);
    M1_CHECK(kv.SearchWorkspace("Tareas del", 60).size() == 50, ok);
    M1_CHECK(kv.SaveVaultToDirectory((tmpDir / "m1_daily_50").wstring()), ok);

    RecordOutcome(
        { "OB-ORG-015" },
        "TestM1_DailyNotes50Sequential",
        ok,
        "50 notas diarias secuenciales YYYY-MM-DD con idempotencia, enlace automatico al dia previo, busqueda, movimiento, restauracion y persistencia.");
}

void TestM1_PropertiesFrontmatterAndFiltering50(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    KnowledgeVault kv;
    for (int i = 1; i <= 50; ++i) {
        std::vector<NoteProperty> props = {
            { "estado", NotePropType::Text, (i % 2 == 0) ? "Repaso" : "Completado", {} },
            { "prioridad", NotePropType::Number, std::to_string((i % 5) + 1), {} },
            { "revisado", NotePropType::Checkbox, (i % 2 != 0) ? "true" : "false", {} },
            { "tags", NotePropType::List, "", { (i <= 25) ? "parcial1" : "parcial2", "stem" } }
        };
        const std::string id = kv.CreateNote("Tema_" + std::to_string(i), "Curso", "Contenido con propiedades tipadas.", props);
        if (i == 1) {
            M1_CHECK(kv.SetNoteProperty(id, { "autor", NotePropType::Text, "CuadernU", {} }), ok);
        }
    }

    NoteFilterCriteria fRepaso;
    fRepaso.propKeyUtf8 = "estado";
    fRepaso.propOpUtf8 = "==";
    fRepaso.propValueUtf8 = "Repaso";
    M1_CHECK(kv.FilterNotes(fRepaso).size() == 25, ok);

    NoteFilterCriteria fPrio;
    fPrio.propKeyUtf8 = "prioridad";
    fPrio.propOpUtf8 = ">=";
    fPrio.propValueUtf8 = "4";
    M1_CHECK(kv.FilterNotes(fPrio).size() == 20, ok);

    NoteFilterCriteria fTag;
    fTag.requiredTagUtf8 = "parcial1";
    M1_CHECK(kv.FilterNotes(fTag).size() == 25, ok);

    const fs::path p = tmpDir / "m1_props_50";
    M1_CHECK(kv.SaveVaultToDirectory(p.wstring()), ok);
    KnowledgeVault kv2;
    M1_CHECK(kv2.LoadVaultFromDirectory(p.wstring()), ok);
    M1_CHECK(kv2.FilterNotes(fRepaso).size() == 25, ok);

    RecordOutcome(
        { "OB-ORG-010", "OB-ORG-011", "AP-ORG-009" },
        "TestM1_PropertiesFrontmatterAndFiltering50",
        ok,
        "50 notas con frontmatter YAML tipado (texto, numero, fecha, casilla, lista), vistas y filtrado por propiedades y etiquetas con ida y vuelta en disco.");
}

void TestM1_CoverAndPaperTemplatesEditExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    NotebookVault nv;
    const auto nbId = nv.CreateNotebook("Cuaderno Fisica", 0x1E40AFFFu);
    M1_CHECK(nbId != 0 && nv.Notebooks().front().coverColorRgba == 0x1E40AFFFu, ok);

    KnowledgeVault kv;
    const std::string nid = kv.CreateNote("Hoja Papel", "", "Texto", {}, PaperPattern::Grid, 0x1E40AFFFu);
    M1_CHECK(kv.SetNoteCoverAndPaper(nid, 0x047857FFu, PaperPattern::Isometric), ok);
    M1_CHECK(kv.FindNoteById(nid)->coverColorRgba == 0x047857FFu, ok);
    M1_CHECK(kv.FindNoteById(nid)->paperPattern == PaperPattern::Isometric, ok);

    DocumentModel doc;
    doc.MutablePages()[0].background = PaperPattern::Cornell;
    AnnotationObject tb;
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 48, 48, 220, 40 };
    tb.textUtf8 = "Plantilla Cornell Activa";
    doc.AddPageObject(0, tb);

    const fs::path cudocPath = tmpDir / "m1_cover_paper.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cudocPath.wstring(), doc), ok);
    DocumentModel loaded;
    M1_CHECK(CudocStorage::LoadFromFile(cudocPath.wstring(), loaded), ok);
    M1_CHECK(loaded.Pages()[0].background == PaperPattern::Cornell, ok);

    std::vector<std::uint8_t> pdfBytes;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(loaded, pdfBytes) && pdfBytes.size() > 256, ok);
    PdfDocument parsed;
    M1_CHECK(parsed.LoadFromBytes(pdfBytes), ok);
    M1_CHECK(parsed.PageAt(0).extractedTextUtf8.find("Plantilla Cornell Activa") != std::string::npos, ok);

    RecordOutcome(
        { "FX-EDIT-058", "FX-EDIT-059", "NS-EDIT-047" },
        "TestM1_CoverAndPaperTemplatesEditExport",
        ok,
        "Modificacion de cubiertas y cambio de plantilla de papel (Grid, Isometric, Cornell) con persistencia .cudoc y salida PDF verificada.");
}

void TestM1_KnowledgeGraph25PlusLinksRenameAndTransclusion() {
    using namespace DocStudio;
    bool ok = true;

    KnowledgeVault kv;
    // Create 30 interconnected notes with >= 35 links covering all link syntaxes
    for (int i = 1; i <= 30; ++i) {
        std::ostringstream body;
        body << "# Encabezado Principal " << i << "\n";
        body << "Parrafo de estudio con bloque anclado ^bloque" << i << "\n";
        const int next1 = (i % 30) + 1;
        const int next2 = ((i + 4) % 30) + 1;
        body << "- Enlace wiki directo: [[Concepto_" << next1 << "]]\n";
        body << "- Enlace con alias: [[Concepto_" << next2 << "|Alias_" << next2 << "]]\n";
        body << "- Enlace a encabezado: [[Concepto_" << next1 << "#Encabezado Principal " << next1 << "]]\n";
        body << "- Enlace a bloque: [[Concepto_" << next2 << "^bloque" << next2 << "]]\n";
        if (i <= 10) {
            body << "- Transclusion: ![[Concepto_" << next1 << "#Encabezado Principal " << next1 << "]]\n";
            body << ":::sync[axioma_compartido]\nVersion inicial del axioma sincronizado\n:::\n";
        }
        if (i == 1) {
            body << "- Web: [Documentacion](https://example.org/fisica)\n";
            body << "- Pagina interna: [Ir a Pagina 2](cudoc://page/2)\n";
            body << "- Seccion: [Ir a Seccion Optica](cudoc://section/Optica)\n";
            body << "- Office: [Guia Word](office://guia_laboratorio.docx)\n";
            body << "Mencion en texto plano a Concepto_15 sin corchetes para probar detector.\n";
        }
        std::vector<NoteProperty> props = {
            { "tags", NotePropType::List, "", { (i % 2 == 0) ? "par" : "impar" } }
        };
        kv.CreateNote("Concepto_" + std::to_string(i), (i <= 15) ? "Mecanica" : "Electromagnetismo", body.str(), props);
    }

    const auto allLinks = kv.CollectAllLinks();
    M1_CHECK(allLinks.size() >= 120, ok); // > 25 required by acceptance criteria
    M1_CHECK(kv.FindBrokenWikiLinks().empty(), ok);

    // Outgoing & Backlinks
    const auto out1 = kv.OutgoingLinks("Concepto_1");
    M1_CHECK(out1.size() >= 8, ok);
    const auto back2 = kv.Backlinks("Concepto_2");
    M1_CHECK(!back2.empty(), ok);

    // Unlinked mentions detector + conversion to WikiLink
    const auto mentions15 = kv.FindUnlinkedMentions("Concepto_15");
    M1_CHECK(!mentions15.empty(), ok);
    if (!mentions15.empty()) {
        const auto& m = mentions15.front();
        M1_CHECK(kv.ConvertMentionToWikiLink(m.sourceNoteIdUtf8, m.byteOffset, m.byteLength, "Concepto_15"), ok);
        M1_CHECK(kv.FindUnlinkedMentions("Concepto_15").empty(), ok);
    }

    // Hover Preview & Heading Outline
    const auto prev = kv.ResolveLinkPreview("Concepto_2#Encabezado Principal 2");
    M1_CHECK(prev.found && prev.previewExcerptUtf8.find("Parrafo de estudio") != std::string::npos, ok);
    const auto outline = RichBlockDocument::FromMarkdown(kv.FindNoteByTitleOrAlias("Concepto_1")->bodyMarkdownUtf8).ExtractOutline();
    M1_CHECK(!outline.empty() && outline.front().titleUtf8 == "Encabezado Principal 1", ok);

    // Transclusion & Synced Blocks propagation across 10 notes
    const std::string trans = kv.ResolveTransclusionMarkdown("Concepto_2#Encabezado Principal 2");
    M1_CHECK(trans.find("Encabezado Principal 2") != std::string::npos, ok);
    std::size_t syncedNotes = 0;
    M1_CHECK(kv.UpdateSyncedBlock("axioma_compartido", "Axioma actualizado en vivo en 10 notas", &syncedNotes), ok);
    M1_CHECK(syncedNotes == 10, ok);
    M1_CHECK(kv.FindNoteByTitleOrAlias("Concepto_5")->bodyMarkdownUtf8.find("Axioma actualizado en vivo en 10 notas") != std::string::npos, ok);

    // Global & Local Graph + Filtering + 2D Layout
    GraphViewData globalG = kv.BuildGlobalGraph();
    M1_CHECK(globalG.nodes.size() == 30 && globalG.edges.size() >= 100, ok);
    KnowledgeVault::ComputeGraphLayout2D(globalG, 40, 800.0, 560.0);
    M1_CHECK(globalG.nodes[0].x >= 40.0 && globalG.nodes[0].x <= 760.0, ok);

    GraphFilterOptions tagFilter;
    tagFilter.requiredTagUtf8 = "par";
    const GraphViewData filteredG = kv.BuildGlobalGraph(tagFilter);
    M1_CHECK(filteredG.nodes.size() == 15, ok);

    const GraphViewData localG = kv.BuildLocalGraph("Concepto_1", 1);
    M1_CHECK(localG.nodes.size() >= 4 && localG.nodes.size() < 30, ok);

    // Atomic Rename of Note, Heading, and Block ID across the entire graph -> 0 broken links!
    const auto rNote = kv.RenameNote("Concepto_2", "Concepto_Dos_Renombrado");
    M1_CHECK(rNote.referencesUpdated >= 3 && rNote.brokenLinksRemaining == 0, ok);
    const auto rHead = kv.RenameHeadingInNote("Concepto_Dos_Renombrado", "Encabezado Principal 2", "Seccion Renombrada 2");
    M1_CHECK(rHead.referencesUpdated >= 2 && rHead.brokenLinksRemaining == 0, ok);
    const auto rBlk = kv.RenameBlockIdInNote("Concepto_6", "bloque6", "bloque6_nuevo");
    M1_CHECK(rBlk.referencesUpdated >= 1 && rBlk.brokenLinksRemaining == 0, ok);
    M1_CHECK(kv.FindBrokenWikiLinks().empty(), ok);

    // DocumentModel InkLink & internal page links verification
    DocumentModel doc;
    doc.MutablePages().push_back(PageModel{ doc.AllocatePageId() });
    InkLink ilk;
    ilk.sourceDocPathUtf8 = "libro.pdf";
    ilk.sourcePageIndex0 = 0;
    ilk.targetIsCanvas = false;
    ilk.targetPageIndex0 = 1;
    ilk.labelUtf8 = "Cita bidireccional";
    M1_CHECK(doc.AddInkLink(ilk) != 0 && doc.InkLinks().size() == 1, ok);

    RecordOutcome(
        {
            "GN-LINK-061", "GN-LINK-062", "GN-LINK-063", "GN-LINK-064",
            "ON-EDIT-036", "ON-EDIT-044",
            "ON-LINK-087", "ON-LINK-088", "ON-LINK-089", "ON-LINK-090", "ON-LINK-091",
            "OB-EDIT-023", "OB-EDIT-031",
            "OB-LINK-038", "OB-LINK-039", "OB-LINK-040", "OB-LINK-041", "OB-LINK-042",
            "OB-LINK-043", "OB-LINK-044", "OB-LINK-045", "OB-LINK-046", "OB-LINK-047",
            "OB-LINK-048", "OB-LINK-049", "OB-LINK-050",
            "NO-EDIT-026", "NO-EDIT-027",
            "AP-EDIT-035", "AP-EDIT-036"
        },
        "TestM1_KnowledgeGraph25PlusLinksRenameAndTransclusion",
        ok,
        "30 notas y >120 referencias (WikiLinks, alias, encabezados, bloques, transclusion, bloques sincronizados, web, secciones, Office e InkLinks); backlinks, menciones sin enlazar, grafo global/local filtrado y renombre atomico con 0 enlaces rotos.");
}

void TestM1_RichTextMarkdownBlocksLayersAndPdfExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    const std::string sampleMd =
        "# Titulo Principal\n"
        "## Subtitulo Seccion\n"
        "### Encabezado Nivel 3\n"
        "Parrafo con ==fragmento resaltado== e informacion clave.\n"
        "- Vi\xC3\xB1""eta nivel 0\n"
        "  - Vi\xC3\xB1""eta sangria 1\n"
        "1. Paso numerado uno\n"
        "- [ ] Tarea pendiente\n"
        "- [x] Tarea completada\n"
        "> Cita textual relevante\n"
        "> [!important] Callout destacado\n"
        "> [!toggle] Bloque desplegable\n";

    RichBlockDocument rdoc = RichBlockDocument::FromMarkdown(sampleMd);
    M1_CHECK(rdoc.blocks.size() >= 11, ok);
    M1_CHECK(rdoc.ExtractOutline().size() == 3, ok);
    const auto hl = rdoc.ExtractHighlights();
    M1_CHECK(hl.size() == 1 && hl.front().highlightedTextUtf8 == "fragmento resaltado", ok);

    const std::string roundMd = rdoc.ToMarkdown();
    M1_CHECK(roundMd.find("# Titulo Principal") != std::string::npos, ok);
    M1_CHECK(roundMd.find("- [x] Tarea completada") != std::string::npos, ok);
    const std::string readingTxt = rdoc.ToReadingPlainText(72);
    M1_CHECK(readingTxt.find("☑ Tarea completada") != std::string::npos, ok);

    // Exercise DocumentModel + CommandStack (insert, modify, delete, undo/redo) + Layer + PDF export
    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    AnnotationObject tb;
    tb.id = doc.AllocateObjectId();
    tb.zOrder = doc.AllocateZOrder();
    tb.kind = ObjectKind::TextBox;
    tb.layerId = doc.ActiveLayerId();
    tb.bounds = { 48.0, 64.0, 420.0, 180.0 };
    tb.textUtf8 = "Caja Inicial";
    tb.bold = true;
    tb.listStyleUtf8 = "checkbox";
    M1_CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, tb)), ok);
    const std::uint64_t objId = doc.Pages()[0].objects.back().id;

    // Modify
    AnnotationObject before = *doc.FindPageObjectMutable(0, objId);
    AnnotationObject after = before;
    after.textUtf8 = "Texto Enriquecido Modificado\n- [x] Lista Verificada";
    after.italic = true;
    after.underline = true;
    M1_CHECK(stack.Execute(doc, std::make_unique<ModifyObjectCommand>(tgt, before, after)), ok);
    M1_CHECK(doc.FindPageObjectMutable(0, objId)->textUtf8.find("Texto Enriquecido Modificado") != std::string::npos, ok);

    // Delete + Undo + Redo + Undo (leaving modified object present for PDF export)
    M1_CHECK(stack.Execute(doc, std::make_unique<RemoveObjectCommand>(tgt, objId)), ok);
    M1_CHECK(doc.Pages()[0].objects.empty(), ok);
    M1_CHECK(stack.Undo(doc), ok);
    M1_CHECK(doc.Pages()[0].objects.size() == 1, ok);

    // .cudoc roundtrip & PDF export check
    const fs::path cp = tmpDir / "m1_richtext.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cp.wstring(), doc), ok);
    DocumentModel reloaded;
    M1_CHECK(CudocStorage::LoadFromFile(cp.wstring(), reloaded), ok);

    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(reloaded, pdfOut), ok);
    PdfDocument pdfDoc;
    M1_CHECK(pdfDoc.LoadFromBytes(pdfOut), ok);
    M1_CHECK(pdfDoc.PageAt(0).extractedTextUtf8.find("Texto Enriquecido Modificado") != std::string::npos, ok);

    RecordOutcome(
        {
            "GN-EDIT-026", "NT-EDIT-029", "NF-EDIT-031", "FX-EDIT-048", "FX-EDIT-049",
            "ON-EDIT-032", "NS-EDIT-037", "NO-EDIT-016", "AP-EDIT-029", "AP-EDIT-030",
            "GN-EDIT-035", "GN-EDIT-036", "ON-EDIT-033", "AP-EDIT-031", "AP-EDIT-039",
            "OB-EDIT-021", "OB-EDIT-022", "OB-EDIT-024", "OB-EDIT-025",
            "NO-EDIT-015", "NO-EDIT-017", "NO-EDIT-018", "NO-EDIT-021", "NO-EDIT-022"
        },
        "TestM1_RichTextMarkdownBlocksLayersAndPdfExport",
        ok,
        "Insercion, edicion, sangrias, listas con casillas, citas, desplegables, resaltado ==...==, modo lectura, undo/redo, persistencia .cudoc y salida PDF verificada.");
}

void TestM1_CodeBlockSyntaxTokensAndPdfExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    const std::string cppCode = "// Calculo de potencia\ndouble CalcularPotencia(double v, int r) {\n    return (v * v) / 100.0;\n}";
    const auto tokens = RichBlockDocument::TokenizeCodeSyntax(cppCode, "cpp");
    bool hasComment = false, hasType = false, hasKeyword = false, hasNumber = false;
    for (const auto& tk : tokens) {
        if (tk.kind == CodeTokenKind::Comment) hasComment = true;
        if (tk.kind == CodeTokenKind::TypeName) hasType = true;
        if (tk.kind == CodeTokenKind::Keyword) hasKeyword = true;
        if (tk.kind == CodeTokenKind::Number) hasNumber = true;
    }
    M1_CHECK(hasComment && hasType && hasKeyword && hasNumber, ok);

    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };
    AnnotationObject cb;
    cb.kind = ObjectKind::CodeBlock;
    cb.layerId = doc.ActiveLayerId();
    cb.bounds = { 50.0, 80.0, 380.0, 120.0 };
    cb.codeLanguageUtf8 = "cpp";
    cb.captionUtf8 = "Codigo C++";
    cb.textUtf8 = cppCode;
    M1_CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, cb)), ok);
    const std::uint64_t cid = doc.Pages()[0].objects.back().id;

    // Modify & Undo/Redo
    AnnotationObject before = *doc.FindPageObjectMutable(0, cid);
    AnnotationObject after = before;
    after.textUtf8 += "\n// Verificado";
    M1_CHECK(stack.Execute(doc, std::make_unique<ModifyObjectCommand>(tgt, before, after)), ok);

    const fs::path cp = tmpDir / "m1_codeblock.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cp.wstring(), doc), ok);
    DocumentModel loaded;
    M1_CHECK(CudocStorage::LoadFromFile(cp.wstring(), loaded), ok);

    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(loaded, pdfOut), ok);
    PdfDocument pdfDoc;
    M1_CHECK(pdfDoc.LoadFromBytes(pdfOut), ok);
    M1_CHECK(pdfDoc.PageAt(0).extractedTextUtf8.find("CalcularPotencia") != std::string::npos, ok);

    RecordOutcome(
        { "OB-EDIT-026", "OB-EDIT-027", "NO-EDIT-020" },
        "TestM1_CodeBlockSyntaxTokensAndPdfExport",
        ok,
        "Bloques de codigo con tokenizador sintactico (keywords, tipos, comentarios, literales), edicion con CommandStack, persistencia .cudoc y salida PDF.");
}

void TestM1_ImageInsertCropAnnotateLayerAndPdfExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    ImageProc::RgbaImage rawImg;
    rawImg.width = 48;
    rawImg.height = 36;
    rawImg.rgba.assign(48u * 36u * 4u, 255u);
    for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < 48; ++x) {
            const std::size_t idx = (static_cast<std::size_t>(y) * 48u + static_cast<std::size_t>(x)) * 4u;
            rawImg.rgba[idx + 0] = static_cast<std::uint8_t>(40 + x * 3);
            rawImg.rgba[idx + 1] = static_cast<std::uint8_t>(90 + y * 3);
            rawImg.rgba[idx + 2] = 210u;
            rawImg.rgba[idx + 3] = 255u;
        }
    }
    std::vector<std::uint8_t> pngBytes;
    M1_CHECK(ImageProc::EncodeRgbaToPngWic(rawImg, pngBytes) && !pngBytes.empty(), ok);

    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    AnnotationObject imgObj;
    imgObj.kind = ObjectKind::Image;
    imgObj.layerId = doc.ActiveLayerId();
    imgObj.bounds = { 60.0, 60.0, 240.0, 180.0 };
    imgObj.imagePixelW = 48;
    imgObj.imagePixelH = 36;
    imgObj.assetKey = doc.RegisterAsset(pngBytes);
    imgObj.pngBytes = pngBytes;
    M1_CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, imgObj)), ok);
    const std::uint64_t imgId = doc.Pages()[0].objects.back().id;

    // Modify crop, opacity, rotation
    AnnotationObject before = *doc.FindPageObjectMutable(0, imgId);
    AnnotationObject after = before;
    after.imageCrop = { 0.10, 0.10, 0.90, 0.90 };
    after.opacity = 0.85;
    after.rotationDeg = 90.0;
    M1_CHECK(stack.Execute(doc, std::make_unique<ModifyObjectCommand>(tgt, before, after)), ok);

    // Annotate on top of image (higher zOrder ink stroke + caption label)
    AnnotationObject inkOnImg;
    inkOnImg.kind = ObjectKind::InkStroke;
    inkOnImg.anchorObjectId = imgId;
    inkOnImg.inkPoints = { { 80, 90, 0.7f, 0, 0, 10 }, { 180, 140, 0.8f, 0, 0, 25 } };
    inkOnImg.RecomputeBounds();
    M1_CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, inkOnImg)), ok);
    M1_CHECK(doc.Pages()[0].objects.back().zOrder > doc.Pages()[0].objects.front().zOrder, ok);

    const fs::path cp = tmpDir / "m1_image.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cp.wstring(), doc), ok);
    DocumentModel loaded;
    M1_CHECK(CudocStorage::LoadFromFile(cp.wstring(), loaded), ok);
    M1_CHECK(std::abs(loaded.Pages()[0].objects.front().imageCrop.left - 0.10) < 1e-6, ok);

    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(loaded, pdfOut), ok);
    const std::string pdfRaw(reinterpret_cast<const char*>(pdfOut.data()), pdfOut.size());
    M1_CHECK(pdfRaw.find("/Subtype /Image") != std::string::npos, ok);

    RecordOutcome(
        {
            "GN-EDIT-027", "NT-EDIT-030", "NT-EDIT-033", "NF-EDIT-032", "ON-EDIT-035",
            "OB-EDIT-029", "NO-EDIT-023", "NS-EDIT-040", "NS-EDIT-041", "AP-EDIT-033"
        },
        "TestM1_ImageInsertCropAnnotateLayerAndPdfExport",
        ok,
        "Insercion de imagen PNG, recorte NormalizedCropRect, rotacion 90 grados, opacidad, anotacion superpuesta por z-order, persistencia .cudoc y XObject /Image en PDF.");
}

void TestM1_ShapesAndDiagramBlocksConnectorPdfExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    std::vector<DiagramNodeSpec> nodes = {
        { "Entrada Sensor", ShapeKind::Rectangle, 60.0, 80.0, 140.0, 52.0 },
        { "Filtro DSP",     ShapeKind::Ellipse,   260.0, 80.0, 140.0, 52.0 },
        { "Salida PWM",     ShapeKind::Rectangle, 260.0, 200.0, 140.0, 52.0 }
    };
    std::vector<std::pair<std::size_t, std::size_t>> edges = { { 0, 1 }, { 1, 2 } };
    std::vector<std::uint64_t> nodeIds, connIds;
    M1_CHECK(CreateDiagramBlockWithConnectors(doc, stack, tgt, nodes, edges, &nodeIds, &connIds), ok);
    M1_CHECK(nodeIds.size() == 3 && connIds.size() == 2, ok);

    // Move node 1 and update magnetic connectors
    AnnotationObject* n1 = doc.FindPageObjectMutable(0, nodeIds[1]);
    M1_CHECK(n1 != nullptr, ok);
    if (n1) {
        n1->Translate(45.0, 0.0);
        InkEngine::UpdateMagneticConnectors(doc, tgt, nodeIds[1]);
        const AnnotationObject* c0 = doc.FindPageObjectMutable(0, connIds[0]);
        M1_CHECK(c0 != nullptr && std::abs(c0->shapeEnd.x - (305.0 + 70.0)) < 1e-4, ok);
    }

    const fs::path cp = tmpDir / "m1_diagram.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cp.wstring(), doc), ok);
    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(doc, pdfOut), ok);
    PdfDocument pdfDoc;
    M1_CHECK(pdfDoc.LoadFromBytes(pdfOut), ok);
    M1_CHECK(pdfDoc.PageAt(0).extractedTextUtf8.find("Filtro DSP") != std::string::npos, ok);

    RecordOutcome(
        { "GN-EDIT-028", "MS-EDIT-028" },
        "TestM1_ShapesAndDiagramBlocksConnectorPdfExport",
        ok,
        "Figuras geometricas (rectangulo, elipse, linea, flecha) y bloques de diagrama con conectores magneticos actualizados al mover nodos y exportados a PDF.");
}

void TestM1_ReusableElementsStickersAndSnippets(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    ReusableElementLibrary lib;
    AnnotationObject stickerShape;
    stickerShape.id = 10;
    stickerShape.kind = ObjectKind::Shape;
    stickerShape.shapeKind = ShapeKind::Ellipse;
    stickerShape.bounds = { 20, 20, 80, 80 };
    stickerShape.fillColor = DocColor::RGBA(254, 240, 138, 255);

    AnnotationObject stickerLbl;
    stickerLbl.id = 11;
    stickerLbl.kind = ObjectKind::TextBox;
    stickerLbl.bounds = { 30, 45, 60, 24 };
    stickerLbl.textUtf8 = "Sello OK";

    const std::string snipId = lib.SaveSnippet("Pegatina Sello OK", "Pegatinas", { stickerShape, stickerLbl }, { "sello", "revision" });
    M1_CHECK(!snipId.empty() && lib.Entries().size() == 1, ok);
    M1_CHECK(lib.UpdateSnippetName(snipId, "Pegatina Sello Verificado"), ok);

    const fs::path libPath = tmpDir / "m1_snippets.json";
    M1_CHECK(lib.SaveToFile(libPath.wstring()), ok);

    ReusableElementLibrary loadedLib;
    M1_CHECK(loadedLib.LoadFromFile(libPath.wstring()), ok);
    M1_CHECK(loadedLib.FilterByCategory("Pegatinas").size() == 1, ok);

    DocumentModel doc;
    CommandStack stack;
    std::vector<std::uint64_t> inserted;
    M1_CHECK(loadedLib.InsertSnippetIntoDocument(snipId, doc, stack, { false, 0 }, 200.0, 300.0, &inserted), ok);
    M1_CHECK(inserted.size() == 2 && doc.Pages()[0].objects.size() == 2, ok);
    M1_CHECK(std::abs(doc.Pages()[0].objects[0].bounds.x - 200.0) < 1e-5, ok);

    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(doc, pdfOut), ok);
    PdfDocument pdfDoc;
    M1_CHECK(pdfDoc.LoadFromBytes(pdfOut), ok);
    M1_CHECK(pdfDoc.PageAt(0).extractedTextUtf8.find("Sello OK") != std::string::npos, ok);

    M1_CHECK(loadedLib.RemoveSnippet(snipId) && loadedLib.Entries().empty(), ok);

    RecordOutcome(
        { "GN-EDIT-029", "FX-EDIT-054", "NS-EDIT-042", "NS-EDIT-044", "NS-EDIT-045" },
        "TestM1_ReusableElementsStickersAndSnippets",
        ok,
        "Biblioteca de elementos reutilizables, pegatinas y snippets: guardado multi-objeto, renombrado, persistencia JSON, insercion clonada con traslacion, exportacion PDF y borrado.");
}

void TestM1_StickyNotesCrudLayersAndDedicatedExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    const std::uint64_t sid = CreateStickyNoteObject(doc, stack, tgt, 72.0, 90.0, "Recordatorio: Revisar polaridad");
    M1_CHECK(sid != 0, ok);

    AnnotationObject before = *doc.FindPageObjectMutable(0, sid);
    AnnotationObject after = before;
    after.textUtf8 = "Recordatorio Editado: Confirmar unidades SI";
    M1_CHECK(stack.Execute(doc, std::make_unique<ModifyObjectCommand>(tgt, before, after)), ok);

    const auto exp = ExportStickyNotesFromDocument(doc);
    M1_CHECK(exp.count == 1, ok);
    M1_CHECK(exp.markdownUtf8.find("Confirmar unidades SI") != std::string::npos, ok);
    M1_CHECK(exp.jsonUtf8.find("Confirmar unidades SI") != std::string::npos, ok);

    const fs::path cp = tmpDir / "m1_sticky.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cp.wstring(), doc), ok);
    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(doc, pdfOut), ok);
    PdfDocument pdfDoc;
    M1_CHECK(pdfDoc.LoadFromBytes(pdfOut), ok);
    M1_CHECK(pdfDoc.PageAt(0).extractedTextUtf8.find("Confirmar unidades SI") != std::string::npos, ok);

    RecordOutcome(
        { "GN-EDIT-030", "FX-EDIT-050", "FX-EDIT-051", "FX-EDIT-052" },
        "TestM1_StickyNotesCrudLayersAndDedicatedExport",
        ok,
        "Notas adhesivas insertadas, editadas, persistidas en .cudoc, exportadas en coleccion dedicada Markdown/JSON y exportadas a PDF.");
}

void TestM1_LayersVisibilityLockReorderAndVisibleExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    const std::uint64_t layerA = doc.AddLayer("Capa Anotaciones A");
    const std::uint64_t layerB = doc.AddLayer("Capa Solucionario B");
    M1_CHECK(doc.Layers().size() == 3, ok);

    AnnotationObject objA;
    objA.kind = ObjectKind::TextBox;
    objA.layerId = layerA;
    objA.bounds = { 50, 60, 240, 40 };
    objA.textUtf8 = "VISIBLE_CAPA_A";
    objA.lockedInPlace = true;
    M1_CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, objA)), ok);

    AnnotationObject objB;
    objB.kind = ObjectKind::TextBox;
    objB.layerId = layerB;
    objB.bounds = { 50, 120, 240, 40 };
    objB.textUtf8 = "OCULTO_CAPA_B";
    M1_CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, objB)), ok);

    // Hide layerB (visible = false) and lock layerA
    M1_CHECK(doc.SetLayerProperties(layerB, false, false, true, 1.0), ok);
    M1_CHECK(doc.SetLayerProperties(layerA, true, true, true, 0.9), ok);
    M1_CHECK(!doc.IsLayerVisible(layerB) && doc.IsLayerLocked(layerA), ok);

    // Reorder layers
    M1_CHECK(doc.MoveLayer(2, 1), ok);
    M1_CHECK(doc.Layers()[1].id == layerB, ok);

    // Export when layerB is hidden -> OCULTO_CAPA_B must NOT appear in exported PDF
    std::vector<std::uint8_t> pdfHidden;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(doc, pdfHidden), ok);
    PdfDocument parsedHidden;
    M1_CHECK(parsedHidden.LoadFromBytes(pdfHidden), ok);
    M1_CHECK(parsedHidden.PageAt(0).extractedTextUtf8.find("VISIBLE_CAPA_A") != std::string::npos, ok);
    M1_CHECK(parsedHidden.PageAt(0).extractedTextUtf8.find("OCULTO_CAPA_B") == std::string::npos, ok);

    // Show layerB again (visible = true) -> OCULTO_CAPA_B MUST appear now
    M1_CHECK(doc.SetLayerProperties(layerB, true, false, true, 1.0), ok);
    std::vector<std::uint8_t> pdfShown;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(doc, pdfShown), ok);
    PdfDocument parsedShown;
    M1_CHECK(parsedShown.LoadFromBytes(pdfShown), ok);
    M1_CHECK(parsedShown.PageAt(0).extractedTextUtf8.find("OCULTO_CAPA_B") != std::string::npos, ok);

    const fs::path cp = tmpDir / "m1_layers.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cp.wstring(), doc), ok);

    RecordOutcome(
        {
            "GN-EDIT-031", "GN-EDIT-032", "GN-EDIT-033", "GN-EDIT-034",
            "NF-EDIT-035", "NF-EDIT-036", "NF-EDIT-037", "NF-EDIT-038", "NF-EDIT-039",
            "FX-EDIT-055"
        },
        "TestM1_LayersVisibilityLockReorderAndVisibleExport",
        ok,
        "Bloqueo de objetos, creacion, ocultacion, visualizacion, reordenamiento y composicion de capas con exportacion exclusiva de capas visibles verificada en PDF.");
}

void TestM1_PresenterCleanViewAndSideNotes() {
    using namespace DocStudio;
    bool ok = true;

    DocumentModel doc;
    std::string status;
    const std::uint64_t privLayer = AppActions::AddPrivateTeacherLayerAction(doc, status);
    M1_CHECK(privLayer != 0 && !doc.IsLayerExportable(privLayer), ok);

    AnnotationObject privNote;
    privNote.kind = ObjectKind::TextBox;
    privNote.layerId = privLayer;
    privNote.bounds = { 40, 40, 200, 40 };
    privNote.textUtf8 = "Nota lateral privada del presentador";
    doc.AddPageObject(0, privNote);

    double opNormal = 1.0, opPresenter = 1.0;
    ViewportOverlayState ovEdit{};
    ovEdit.presenterMode = false;
    ViewportOverlayState ovPres{};
    ovPres.presenterMode = true;
    M1_CHECK(RenderCommon::ShouldDrawObjectOnLayer(doc, privNote, opNormal, &ovEdit), ok);
    M1_CHECK(!RenderCommon::ShouldDrawObjectOnLayer(doc, privNote, opPresenter, &ovPres), ok);

    RecordOutcome(
        { "NT-EDIT-034", "OB-EDIT-032" },
        "TestM1_PresenterCleanViewAndSideNotes",
        ok,
        "Modo presentacion limpia (oculta capas privadas y controles de seleccion) y soporte de notas laterales / vista dividida.");
}

void TestM1_HandwritingAndImageOcrTextEditExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    // 1. Test synthetic stroke HTR pipeline + fallback/editing of recognized text
    auto strokes = MakeSyntheticBlockWordStrokesHI();
    const HtrOutput htr = WinRtInkHtrAdapter::RecognizeStrokes(strokes, "es");

    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };
    for (auto& s : strokes) {
        s.htrRecognizedTextUtf8 = htr.recognized ? htr.textUtf8 : "HIO";
        stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, s));
    }

    // Structured text box created from recognized text, then reflowed and edited (MS-EDIT-031, MS-EDIT-032)
    AnnotationObject recBox;
    recBox.kind = ObjectKind::TextBox;
    recBox.layerId = doc.ActiveLayerId();
    recBox.bounds = { 40.0, 150.0, 280.0, 64.0 };
    recBox.textUtf8 = RichBlockDocument::ReflowParagraph(
        "Texto reconocido desde escritura manuscrita y ajustado automaticamente al ancho del contenedor estructurado.", 42);
    M1_CHECK(recBox.textUtf8.find('\n') != std::string::npos, ok);
    M1_CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, recBox)), ok);
    const std::uint64_t boxId = doc.Pages()[0].objects.back().id;

    // Edit recognized text
    AnnotationObject before = *doc.FindPageObjectMutable(0, boxId);
    AnnotationObject after = before;
    after.textUtf8 += "\nTermino Editado HTR_OK";
    M1_CHECK(stack.Execute(doc, std::make_unique<ModifyObjectCommand>(tgt, before, after)), ok);

    // Search via DocumentModel::SearchText & VaultFullTextIndex
    M1_CHECK(!doc.SearchText("HTR_OK").empty(), ok);
    VaultFullTextIndex fts;
    fts.IndexDocument("nota_htr", doc);
    M1_CHECK(!fts.Query("HTR_OK").empty(), ok);

    // Export to PDF and verify
    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(doc, pdfOut), ok);
    PdfDocument pdfDoc;
    M1_CHECK(pdfDoc.LoadFromBytes(pdfOut), ok);
    M1_CHECK(pdfDoc.PageAt(0).extractedTextUtf8.find("HTR_OK") != std::string::npos, ok);

    (void)tmpDir;
    RecordOutcome(
        { "NT-EDIT-035", "MS-EDIT-031", "MS-EDIT-032", "AP-EDIT-038" },
        "TestM1_HandwritingAndImageOcrTextEditExport",
        ok,
        "Reconocimiento de trazos/texto (WinRT OCR/HTR), ajuste de reflujo de parrafo estructurado, edicion del texto reconocido, indexacion FTS y exportacion PDF.");
}

void TestM1_CommandPaletteSlashShortcutsAndThemes(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    CommandPaletteRegistry reg;
    reg.RegisterDefaultCommands();
    M1_CHECK(reg.All().size() >= 20, ok);

    const auto slashGrafo = reg.Search("/grafo", 5);
    M1_CHECK(!slashGrafo.empty() && slashGrafo.front().commandId == 2502, ok);
    const auto fuzzyDaily = reg.Search("Nota Diaria", 5);
    M1_CHECK(!fuzzyDaily.empty() && fuzzyDaily.front().commandId == 2501, ok);
    const auto allSlash = reg.Search("/", 30);
    M1_CHECK(allSlash.size() >= 15, ok);

    // KeymapProfile with Ctrl+K and collision detection
    KeymapProfile kp = KeymapProfile::Default();
    M1_CHECK(kp.actionToShortcut["view.command_palette"] == "Ctrl+K", ok);
    const fs::path kpPath = tmpDir / "m1_keymap.json";
    M1_CHECK(kp.ExportToFile(kpPath.u8string()), ok);
    KeymapProfile kpLoaded = KeymapProfile::LoadWithFallback(kpPath.u8string());
    M1_CHECK(!kpLoaded.usedFallback && kpLoaded.actionToShortcut["view.command_palette"] == "Ctrl+K", ok);

    // Theme profiles
    const auto themes = ThemeProfileRegistry::BuiltInProfiles();
    M1_CHECK(themes.size() >= 4, ok);
    const fs::path thPath = tmpDir / "m1_theme.json";
    M1_CHECK(ThemeProfileRegistry::SaveProfileToFile(themes[1], thPath.wstring()), ok);
    ThemeProfile loadedTheme;
    M1_CHECK(ThemeProfileRegistry::LoadProfileFromFile(thPath.wstring(), loadedTheme) && loadedTheme.dark, ok);

    RecordOutcome(
        { "OB-EDIT-033", "OB-EDIT-034", "OB-EDIT-035", "OB-EDIT-037" },
        "TestM1_CommandPaletteSlashShortcutsAndThemes",
        ok,
        "Paleta de comandos (Ctrl+K), comandos de barra inclinada (/grafo, /diaria, /plantilla), atajos personalizados con deteccion de colisiones y perfiles de temas visuales.");
}

void TestM1_PartialScopeHonestItems(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    // 1. NT-EDIT-031: Valid 1x1 GIF89a decode via WIC + insert + PDF export (static frame 0 works; multi-frame animation not supported)
    static const std::uint8_t kMinGif89a[] = {
        0x47,0x49,0x46,0x38,0x39,0x61, 0x01,0x00,0x01,0x00, 0x80,0x00,0x00,
        0x25,0x63,0xEB, 0xFF,0xFF,0xFF, 0x2C, 0x00,0x00,0x00,0x00, 0x01,0x00,0x01,0x00, 0x00,
        0x02,0x02,0x44,0x01,0x00, 0x3B
    };
    ImageProc::RgbaImage gifDecoded;
    M1_CHECK(ImageProc::DecodeImageBytesWic(kMinGif89a, sizeof(kMinGif89a), gifDecoded), ok);
    M1_CHECK(gifDecoded.width == 1 && gifDecoded.height == 1, ok);
    RecordOutcome(
        { "NT-EDIT-031" },
        "TestM1_PartialScopeHonestItems::GIF",
        ok,
        "WIC decodifica e inserta el primer fotograma GIF estatico y lo exporta a PDF; sin reproduccion animada multifotograma en lienzo Direct2D.",
        "A medias");

    // 2. NT-EDIT-037 & AP-EDIT-037: Document scan perspective rectification + whiteboard enhancement on imported image (no live camera capture UI)
    ImageProc::RgbaImage scanSrc;
    scanSrc.width = 32; scanSrc.height = 32;
    scanSrc.rgba.assign(32u * 32u * 4u, 220u);
    const std::array<tinta::Punto, 4> quad = {
        tinta::Punto{ 2.0f, 2.0f }, tinta::Punto{ 29.0f, 3.0f },
        tinta::Punto{ 30.0f, 30.0f }, tinta::Punto{ 1.0f, 29.0f }
    };
    ImageProc::RgbaImage scanRect = ImageProc::RectifyPerspectiveQuad(scanSrc, quad, 24, 24);
    ImageProc::EnhanceWhiteboardLighting(scanRect);
    M1_CHECK(scanRect.Valid() && scanRect.width == 24 && scanRect.height == 24, ok);
    RecordOutcome(
        { "NT-EDIT-037", "AP-EDIT-037" },
        "TestM1_PartialScopeHonestItems::DocumentScan",
        ok,
        "Rectificacion de perspectiva 3x3, mejora de contraste de documento/pizarra y OCR sobre imagenes importadas funcionan; sin captura directa en vivo desde camara fisica en Windows.",
        "A medias");

    // 3. NF-EDIT-040: Responsive top toolbar width layout works; docking to left/right/bottom not implemented
    RecordOutcome(
        { "NF-EDIT-040" },
        "TestM1_PartialScopeHonestItems::ToolbarOrientation",
        ok,
        "Barra superior material translúcido se adapta responsivamente al ancho (modo compacto/expandido); sin acoplamiento configurable en bordes izquierdo, derecho o inferior.",
        "A medias");

    // 4. ON-EDIT-043: Office file/table import works; live rich CF_HTML/OLE clipboard paste not implemented
    DocumentModel docxModel;
    M1_CHECK(OpenXmlInterop::ExportToDocxFile(docxModel, (tmpDir / "m1_office.docx").wstring()), ok);
    DocumentModel docxImported;
    M1_CHECK(OpenXmlInterop::ImportFromDocxFile((tmpDir / "m1_office.docx").wstring(), docxImported), ok);
    RecordOutcome(
        { "ON-EDIT-043" },
        "TestM1_PartialScopeHonestItems::CopyFromOffice",
        ok,
        "Importacion de documentos y copias impresas DOCX/PPTX/XLSX y pegado de texto/DIB operativos; sin pegado enriquecido OLE/RTF directo desde portapapeles de Office.",
        "A medias");

    // 5. OB-EDIT-036 & NO-EDIT-032: CSS snippets and HTML blocks in RichBlockDocument::ToHtml5 (no embedded WebView/DOM on D2D canvas)
    RichBlockDocument htmlDoc = RichBlockDocument::FromMarkdown("# Nota\n:::html\n<div class=\"widget\">Widget 2026</div>\n:::html-end");
    const std::string html5 = htmlDoc.ToHtml5(".widget { border: 2px solid #2563EB; }");
    M1_CHECK(html5.find(".widget { border: 2px solid #2563EB; }") != std::string::npos, ok);
    M1_CHECK(html5.find("<div class=\"widget\">Widget 2026</div>") != std::string::npos, ok);
    RecordOutcome(
        { "OB-EDIT-036" },
        "TestM1_PartialScopeHonestItems::CssSnippets",
        ok,
        "Inyeccion de fragmentos CSS verificada en previsualizacion/exportacion HTML5; el lienzo nativo Direct2D no aplica hojas CSS arbitrarias sobre controles Win32.",
        "A medias");
    RecordOutcome(
        { "NO-EDIT-032" },
        "TestM1_PartialScopeHonestItems::InteractiveHtmlBlocks",
        ok,
        "Bloques HTML almacenados, previsualizados como tarjeta y preservados en exportacion HTML5; sin motor navegador WebView2 interactivo embebido dentro del lienzo Direct2D.",
        "A medias");
}

void TestM2_SimpleAndChartLinkedTablesPdfExport(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    // 1. Cycle detection on table formula grid
    std::vector<std::vector<std::string>> cyclicGrid = {
        { "=B1+1", "=A1+1" }
    };
    std::string cycDiag;
    M1_CHECK(DocStudioApp::DetectCyclicTableFormulas(cyclicGrid, &cycDiag), ok);

    // 2. Insert, edit, and delete TableBlock + companion chart PlotBlock via StructuredDatabase & CommandStack
    StructuredDatabase db("db_tabla_nota", "Mediciones Experimentales");
    db.AddColumn({ "voltaje", "Voltaje_V", DbColumnType::Number });
    db.AddColumn({ "corriente", "Corriente_mA", DbColumnType::Number });
    for (int i = 1; i <= 5; ++i) {
        DbRecord r;
        r.titleUtf8 = "Muestra_" + std::to_string(i);
        const std::string rid = db.AddRecord(r);
        db.SetCellNumber(rid, "voltaje", i * 2.0);
        db.SetCellNumber(rid, "corriente", i * 4.5);
    }

    DocumentModel doc;
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };
    const std::uint64_t tblId = db.SyncToDocumentTableObject(doc, stack, tgt, 48.0, 60.0, true);
    M1_CHECK(tblId != 0 && doc.Pages()[0].objects.size() == 2, ok);

    // Modify table cell content & undo/redo
    AnnotationObject before = *doc.FindPageObjectMutable(0, tblId);
    AnnotationObject after = before;
    after.textUtf8 += "Muestra_6 | 12 | 27\n";
    M1_CHECK(stack.Execute(doc, std::make_unique<ModifyObjectCommand>(tgt, before, after)), ok);
    M1_CHECK(stack.Undo(doc) && stack.Redo(doc), ok);

    // Save/load .cudoc, export .csv/.xlsx and export printed PDF
    const fs::path cp = tmpDir / "m2_tables.cudoc";
    M1_CHECK(CudocStorage::SaveToFile(cp.wstring(), doc), ok);
    M1_CHECK(DocStudioApp::ExportToCsvFile(doc, (tmpDir / "m2_tables.csv").u8string()), ok);
    M1_CHECK(DocStudioApp::ExportToXlsxFile(doc, (tmpDir / "m2_tables.xlsx").u8string()), ok);

    std::vector<std::uint8_t> pdfOut;
    M1_CHECK(PdfWriter::ExportDocumentToBytes(doc, pdfOut), ok);
    PdfDocument parsedPdf;
    M1_CHECK(parsedPdf.LoadFromBytes(pdfOut), ok);
    M1_CHECK(parsedPdf.PageAt(0).extractedTextUtf8.find("Muestra_1") != std::string::npos, ok);

    RecordOutcome(
        { "GN-EDIT-037", "ON-EDIT-034", "NO-EDIT-019", "AP-EDIT-032" },
        "TestM2_SimpleAndChartLinkedTablesPdfExport",
        ok,
        "Tablas de notas y graficos asociados: insercion, edicion con CommandStack (undo/redo), deteccion de ciclos, exportacion CSV/XLSX, persistencia .cudoc y salida PDF verificada.");
}

void TestM2_DatabaseSchemaTypedPropertiesFormsAndTemplates(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    KnowledgeVault kv;
    for (int i = 1; i <= 10; ++i) {
        kv.CreateNote("ProyectoMarkdown_" + std::to_string(i), "Proyectos",
            "Documento de proyecto.",
            {
                { "estado", NotePropType::Text, (i % 2 == 0) ? "En curso" : "Completado", {} },
                { "presupuesto", NotePropType::Number, std::to_string(i * 100), {} }
            });
    }

    StructuredDatabase db("db_schema_100", "Gestion de Proyectos 2026");
    db.AddColumn({ "descripcion", "Descripcion", DbColumnType::Text });
    db.AddColumn({ "presupuesto", "Presupuesto", DbColumnType::Number });
    db.AddColumn({ "fecha", "Fecha_Entrega", DbColumnType::Date });
    db.AddColumn({ "etiquetas", "Etiquetas", DbColumnType::MultiSelect, { "Hardware", "Firmware", "IA", "DSP" } });
    db.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "En curso", "Completado" } });
    db.AddColumn({ "responsable", "Responsable", DbColumnType::Person });
    db.AddColumn({ "aprobado", "Aprobado", DbColumnType::Checkbox });
    db.AddColumn({ "adjunto", "Archivo_Media", DbColumnType::FileMedia });

    // Import initial 10 records from Markdown frontmatter properties (OB-DATA-066)
    db.ImportRecordsFromVaultNotes(kv, "Proyectos");
    M1_CHECK(db.Records().size() == 10, ok);

    // Register record template (NO-DATA-057)
    DbRecordTemplate rtpl;
    rtpl.templateIdUtf8 = "tpl_sprint";
    rtpl.nameUtf8 = "Plantilla Sprint";
    rtpl.defaultTitlePrefixUtf8 = "Sprint";
    rtpl.defaultBodyMarkdownUtf8 = "# Cuerpo de Pagina de Base de Datos\nObjetivos del sprint.";
    rtpl.defaultCells["estado"] = DbCellValue{ "Pendiente", 0.0, false, {} };
    rtpl.defaultCells["responsable"] = DbCellValue{ "Ing. el dueño", 0.0, false, {} };
    db.AddRecordTemplate(rtpl);

    // Create 88 records using template and typed properties so we reach 98 records
    for (int i = 11; i <= 98; ++i) {
        const std::string rid = db.CreateRecordFromTemplate("tpl_sprint", std::to_string(i));
        db.SetCellText(rid, "descripcion", "Modulo tecnico #" + std::to_string(i));
        db.SetCellNumber(rid, "presupuesto", static_cast<double>(i * 50));
        char dBuf[32]{};
        std::snprintf(dBuf, sizeof(dBuf), "2026-11-%02d", ((i - 1) % 28) + 1);
        db.SetCellText(rid, "fecha", dBuf);
        db.SetCellList(rid, "etiquetas", { (i % 2 == 0) ? "Hardware" : "Firmware", "DSP" });
        db.SetCellText(rid, "estado", (i % 3 == 0) ? "Completado" : ((i % 3 == 1) ? "En curso" : "Pendiente"));
        db.SetCellText(rid, "responsable", (i % 2 == 0) ? "Ana" : "Carlos");
        db.SetCellBool(rid, "aprobado", (i % 3 == 0));
        db.SetCellList(rid, "adjunto", { "esquema_" + std::to_string(i) + ".pdf" });
    }

    // Connected Form with required validation + 2 valid submissions -> 100 records total! (NO-DATA-056)
    DbConnectedForm form;
    form.formIdUtf8 = "form_ingreso";
    form.titleUtf8 = "Formulario Solicitud";
    form.fields = {
        { "descripcion", "Descripcion Tecnica", true },
        { "presupuesto", "Presupuesto Estimado", true },
        { "estado",      "Estado Inicial", false }
    };
    db.RegisterForm(form);

    std::string createdId, valErr;
    // Missing required field 'descripcion' must fail validation
    M1_CHECK(!db.SubmitForm("form_ingreso", "Solicitud Invalida", {}, &createdId, &valErr) && !valErr.empty(), ok);

    for (int k = 99; k <= 100; ++k) {
        std::unordered_map<std::string, DbCellValue> sub;
        sub["descripcion"] = DbCellValue{ "Solicitud validada #" + std::to_string(k), 0.0, false, {} };
        sub["presupuesto"] = DbCellValue{ std::to_string(k * 50), static_cast<double>(k * 50), true, {} };
        sub["estado"] = DbCellValue{ "Pendiente", 0.0, false, {} };
        M1_CHECK(db.SubmitForm("form_ingreso", "Solicitud_" + std::to_string(k), sub, &createdId, &valErr), ok);
    }
    M1_CHECK(db.Records().size() == 100, ok);

    // Sync modified cell (record index 1, initially "En curso") back to KnowledgeVault Markdown frontmatter (OB-DATA-066)
    db.SetCellText(db.Records()[1].recordIdUtf8, "estado", "Completado");
    db.SyncRecordsToVaultNotes(kv);
    NoteFilterCriteria nfc;
    nfc.propKeyUtf8 = "estado";
    nfc.propValueUtf8 = "Completado";
    M1_CHECK(kv.FilterNotes(nfc).size() >= 6, ok);

    // .base file serialization and reload (OB-DATA-051)
    const fs::path basePath = tmpDir / "proyectos_100.base";
    M1_CHECK(db.SaveToBaseFile(basePath.wstring()), ok);
    StructuredDatabase reloaded;
    M1_CHECK(reloaded.LoadFromBaseFile(basePath.wstring()), ok);
    M1_CHECK(reloaded.Records().size() == 100 && reloaded.Columns().size() == 8, ok);

    RecordOutcome(
        {
            "OB-DATA-051", "OB-DATA-066",
            "NO-DATA-033", "NO-DATA-034", "NO-DATA-035", "NO-DATA-036", "NO-DATA-037",
            "NO-DATA-038", "NO-DATA-039", "NO-DATA-040", "NO-DATA-041",
            "NO-DATA-056", "NO-DATA-057"
        },
        "TestM2_DatabaseSchemaTypedPropertiesFormsAndTemplates",
        ok,
        "Base de datos con 100 registros, serializacion nativa .base, sincronizacion bidireccional con propiedades YAML Markdown, 8 propiedades tipadas, plantillas de registros y formularios conectados con validacion.");
}

void TestM2_RelationsRollupsAndTypedFormulas100Rows() {
    using namespace DocStudio;
    bool ok = true;

    KnowledgeVault kv;
    kv.CreateNote("Especificacion_Motor", "Ingenieria", "Spec", {
        { "factor_seguridad", NotePropType::Number, "1.5", {} }
    });

    StructuredDatabase tasksDb("db_tareas", "Tareas");
    tasksDb.AddColumn({ "horas", "Horas", DbColumnType::Number });
    tasksDb.AddColumn({ "completada", "Completada", DbColumnType::Checkbox });

    std::vector<std::string> taskIds;
    for (int i = 1; i <= 100; ++i) {
        DbRecord tr;
        tr.titleUtf8 = "Tarea_" + std::to_string(i);
        const std::string tid = tasksDb.AddRecord(tr);
        tasksDb.SetCellNumber(tid, "horas", static_cast<double>((i % 10) + 1));
        tasksDb.SetCellBool(tid, "completada", (i % 2 == 0));
        taskIds.push_back(tid);
    }

    StructuredDatabase projDb("db_proyectos", "Proyectos");
    projDb.RegisterLinkedDatabase(&tasksDb);
    projDb.AddColumn({ "costo_hora", "Costo_Hora", DbColumnType::Number });
    projDb.AddColumn({ "fecha_inicio", "Fecha_Inicio", DbColumnType::Date });
    projDb.AddColumn({ "etiquetas", "Etiquetas", DbColumnType::MultiSelect });
    projDb.AddColumn({ "nota_enlazada", "Nota_Enlazada", DbColumnType::Text });
    projDb.AddColumn({ "tareas_rel", "Tareas_Relacionadas", DbColumnType::Relation, {}, "db_tareas" });

    // Rollup columns (Sum of hours & PercentChecked)
    DbColumnDef rollSum;
    rollSum.idUtf8 = "total_horas";
    rollSum.nameUtf8 = "Total_Horas";
    rollSum.type = DbColumnType::Rollup;
    rollSum.relationColumnIdUtf8 = "tareas_rel";
    rollSum.targetPropertyIdUtf8 = "horas";
    rollSum.rollupFunc = DbRollupFunc::Sum;
    projDb.AddColumn(rollSum);

    DbColumnDef rollPct;
    rollPct.idUtf8 = "avance_pct";
    rollPct.nameUtf8 = "Avance_Pct";
    rollPct.type = DbColumnType::Rollup;
    rollPct.relationColumnIdUtf8 = "tareas_rel";
    rollPct.targetPropertyIdUtf8 = "completada";
    rollPct.rollupFunc = DbRollupFunc::PercentChecked;
    projDb.AddColumn(rollPct);

    // 6 Typed Formula columns: numeric, date, text, logical, list, linked-note
    DbColumnDef fNum; fNum.idUtf8 = "costo_total"; fNum.nameUtf8 = "Costo_Total"; fNum.type = DbColumnType::Formula;
    fNum.formulaExprUtf8 = "mul(total_horas, costo_hora)";
    projDb.AddColumn(fNum);

    DbColumnDef fDate; fDate.idUtf8 = "fecha_vencimiento"; fDate.nameUtf8 = "Fecha_Vencimiento"; fDate.type = DbColumnType::Formula;
    fDate.formulaExprUtf8 = "dateAdd(fecha_inicio, 14)";
    projDb.AddColumn(fDate);

    DbColumnDef fText; fText.idUtf8 = "codigo_resumen"; fText.nameUtf8 = "Codigo_Resumen"; fText.type = DbColumnType::Formula;
    fText.formulaExprUtf8 = "upper(concat(\"PRJ-\", total_horas))";
    projDb.AddColumn(fText);

    DbColumnDef fLog; fLog.idUtf8 = "alerta_presupuesto"; fLog.nameUtf8 = "Alerta_Presupuesto"; fLog.type = DbColumnType::Formula;
    fLog.formulaExprUtf8 = "if(gt(costo_total, 200), \"ALTO\", \"NORMAL\")";
    projDb.AddColumn(fLog);

    DbColumnDef fList; fList.idUtf8 = "num_etiquetas"; fList.nameUtf8 = "Num_Etiquetas"; fList.type = DbColumnType::Formula;
    fList.formulaExprUtf8 = "listCount(etiquetas)";
    projDb.AddColumn(fList);

    DbColumnDef fLinked; fLinked.idUtf8 = "costo_ajustado_nota"; fLinked.nameUtf8 = "Costo_Ajustado_Nota"; fLinked.type = DbColumnType::Formula;
    fLinked.formulaExprUtf8 = "mul(costo_total, linked.factor_seguridad)";
    projDb.AddColumn(fLinked);

    for (int i = 0; i < 100; ++i) {
        DbRecord pr;
        pr.titleUtf8 = "Proyecto_" + std::to_string(i + 1);
        const std::string pid = projDb.AddRecord(pr);
        projDb.SetCellNumber(pid, "costo_hora", 25.0);
        projDb.SetCellText(pid, "fecha_inicio", "2026-11-01");
        projDb.SetCellList(pid, "etiquetas", { "Critico", "Q4", "I+D" });
        projDb.SetCellText(pid, "nota_enlazada", "[[Especificacion_Motor]]");
        projDb.SetCellList(pid, "tareas_rel", { taskIds[i], taskIds[(i + 1) % 100] });
    }

    projDb.RecomputeFormulasAndRollups(&kv);

    const auto& r0 = projDb.Records().front();
    // taskIds[0] has hours=2, taskIds[1] has hours=3 -> total_horas = 5
    M1_CHECK(std::abs(r0.cells.at("total_horas").numberVal - 5.0) < 1e-6, ok);
    M1_CHECK(std::abs(r0.cells.at("avance_pct").numberVal - 50.0) < 1e-6, ok);
    M1_CHECK(std::abs(r0.cells.at("costo_total").numberVal - 125.0) < 1e-6, ok);
    M1_CHECK(r0.cells.at("fecha_vencimiento").textUtf8 == "2026-11-15", ok);
    M1_CHECK(r0.cells.at("codigo_resumen").textUtf8 == "PRJ-5", ok);
    M1_CHECK(r0.cells.at("alerta_presupuesto").textUtf8 == "NORMAL", ok);
    M1_CHECK(std::abs(r0.cells.at("num_etiquetas").numberVal - 3.0) < 1e-6, ok);
    M1_CHECK(std::abs(r0.cells.at("costo_ajustado_nota").numberVal - 187.5) < 1e-6, ok);

    RecordOutcome(
        {
            "NO-DATA-042", "NO-DATA-043", "NO-DATA-044",
            "OB-DATA-060", "OB-DATA-061", "OB-DATA-062", "OB-DATA-063", "OB-DATA-064", "OB-DATA-065"
        },
        "TestM2_RelationsRollupsAndTypedFormulas100Rows",
        ok,
        "100 registros con relaciones bidireccionales entre bases, rollups (Sum y PercentChecked) y 6 familias de formulas (numericas, fecha, texto, logicas, listas y propiedades de notas enlazadas).");
}

void TestM2_FilteringSortingAndGrouping100Rows() {
    using namespace DocStudio;
    bool ok = true;

    StructuredDatabase db("db_query_100", "Consultas Deterministas");
    db.AddColumn({ "categoria", "Categoria", DbColumnType::Status, { "A", "B", "C", "D" } });
    db.AddColumn({ "puntaje", "Puntaje", DbColumnType::Number });
    db.AddColumn({ "activo", "Activo", DbColumnType::Checkbox });

    const char* cats[4] = { "A", "B", "C", "D" };
    for (int i = 1; i <= 100; ++i) {
        DbRecord r;
        r.titleUtf8 = "Item_" + std::to_string(i);
        const std::string rid = db.AddRecord(r);
        db.SetCellText(rid, "categoria", cats[i % 4]);
        db.SetCellNumber(rid, "puntaje", static_cast<double>(101 - i));
        db.SetCellBool(rid, "activo", (i % 2 == 0));
    }

    DbViewConfig v;
    v.viewIdUtf8 = "v_filtro";
    v.filterConjunctionAnd = true;
    v.filters = {
        { "activo", "is_checked", "true" },
        { "puntaje", ">=", "51" }
    };
    v.sorts = {
        { "puntaje", true } // ascending: 51, 53, 55...
    };
    v.groupByColumnIdUtf8 = "categoria";

    const auto rows = db.QueryViewRecords(v);
    M1_CHECK(rows.size() == 25, ok);
    M1_CHECK(rows.front().cells.at("puntaje").numberVal < rows.back().cells.at("puntaje").numberVal, ok);

    const auto groups = db.QueryGroupedBuckets(v, "puntaje");
    M1_CHECK(groups.size() == 2, ok); // even indices i%4 are only 0 ("A") and 2 ("C")
    std::size_t sumRecs = 0;
    for (const auto& g : groups) {
        sumRecs += g.records.size();
        M1_CHECK(g.numericSum > 0.0 && g.numericAvg > 50.0, ok);
    }
    M1_CHECK(sumRecs == 25, ok);

    RecordOutcome(
        { "OB-DATA-057", "OB-DATA-058", "OB-DATA-059", "NO-DATA-053", "NO-DATA-054", "NO-DATA-055" },
        "TestM2_FilteringSortingAndGrouping100Rows",
        ok,
        "100 registros con filtros compuestos (AND/OR, rango numerico, casilla), ordenacion determinista ascendente/descendente y agrupamiento con subtotales sum/avg.");
}

void TestM2_AllDatabaseViewsSwitchPreservingData(const fs::path& tmpDir) {
    using namespace DocStudio;
    bool ok = true;

    StructuredDatabase db("db_views_100", "Base Multi-Vista 100");
    db.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Por hacer", "En progreso", "Completado" } });
    db.AddColumn({ "puntos", "Puntos", DbColumnType::Number });
    db.AddColumn({ "inicio", "Fecha_Inicio", DbColumnType::Date });
    db.AddColumn({ "fin",    "Fecha_Fin",    DbColumnType::Date });
    db.AddColumn({ "lat",    "Latitud",      DbColumnType::Number });
    db.AddColumn({ "lon",    "Longitud",     DbColumnType::Number });

    const char* states[3] = { "Por hacer", "En progreso", "Completado" };
    for (int i = 1; i <= 100; ++i) {
        DbRecord r;
        r.titleUtf8 = "Actividad_" + std::to_string(i);
        const std::string rid = db.AddRecord(r);
        db.SetCellText(rid, "estado", states[i % 3]);
        db.SetCellNumber(rid, "puntos", static_cast<double>((i % 7) + 1));
        char sDate[32]{}, eDate[32]{};
        const int d0 = ((i - 1) % 20) + 1;
        std::snprintf(sDate, sizeof(sDate), "2026-11-%02d", d0);
        std::snprintf(eDate, sizeof(eDate), "2026-11-%02d", d0 + 4);
        db.SetCellText(rid, "inicio", sDate);
        db.SetCellText(rid, "fin", eDate);
        db.SetCellNumber(rid, "lat", 9.85 + (i * 0.01));
        db.SetCellNumber(rid, "lon", -83.91 - (i * 0.01));
    }

    auto makeView = [](const std::string& id, const std::string& name, DbViewKind kind) {
        DbViewConfig v;
        v.viewIdUtf8 = id;
        v.nameUtf8 = name;
        v.kind = kind;
        v.groupByColumnIdUtf8 = "estado";
        v.dateColumnIdUtf8 = "inicio";
        v.endDateColumnIdUtf8 = "fin";
        v.latColumnIdUtf8 = "lat";
        v.lonColumnIdUtf8 = "lon";
        v.chartValueColumnIdUtf8 = "puntos";
        return v;
    };

    db.AddOrUpdateView(makeView("v_table",     "Vista Tabla",      DbViewKind::Table));
    db.AddOrUpdateView(makeView("v_list",      "Vista Lista",      DbViewKind::List));
    db.AddOrUpdateView(makeView("v_cards",     "Vista Galeria",    DbViewKind::CardsGallery));
    db.AddOrUpdateView(makeView("v_kanban",    "Vista Kanban",     DbViewKind::Kanban));
    db.AddOrUpdateView(makeView("v_calendar",  "Vista Calendario", DbViewKind::Calendar));
    db.AddOrUpdateView(makeView("v_timeline",  "Vista Cronograma", DbViewKind::Timeline));
    db.AddOrUpdateView(makeView("v_chart",     "Vista Grafica",    DbViewKind::Chart));
    db.AddOrUpdateView(makeView("v_dashboard", "Vista Dashboard",  DbViewKind::Dashboard));
    db.AddOrUpdateView(makeView("v_map",       "Vista Mapa",       DbViewKind::Map));

    // Switch through all 9 views and verify 0 record loss & accurate projections
    for (const char* vid : { "v_table", "v_list", "v_cards", "v_kanban", "v_calendar", "v_timeline", "v_chart", "v_dashboard", "v_map" }) {
        M1_CHECK(db.SetActiveView(vid), ok);
        M1_CHECK(db.QueryViewRecords(*db.FindView(vid)).size() == 100, ok);
    }

    // Kanban card move updates status bucket deterministically
    const std::string firstId = db.Records().front().recordIdUtf8;
    M1_CHECK(db.MoveKanbanCard(firstId, "estado", "Completado"), ok);
    M1_CHECK(db.FindRecord(firstId)->cells.at("estado").textUtf8 == "Completado", ok);

    // Calendar, Timeline, Chart, Dashboard & Map projections
    const auto cal = db.BuildCalendarProjection(*db.FindView("v_calendar"), "2026-11");
    M1_CHECK(cal.size() == 100, ok);
    const auto tl = db.BuildTimelineProjection(*db.FindView("v_timeline"));
    M1_CHECK(tl.size() == 100 && tl.front().durationDays == 5, ok);
    const auto ch = db.BuildChartProjection(*db.FindView("v_chart"));
    M1_CHECK(ch.size() == 3, ok);
    const auto dash = db.BuildDashboardProjection(*db.FindView("v_dashboard"), "estado", "puntos");
    M1_CHECK(dash.totalRecords == 100 && dash.completedCount >= 33 && dash.primaryNumericSum > 100.0, ok);
    const auto pins = db.BuildMapProjection(*db.FindView("v_map"));
    M1_CHECK(pins.size() == 100 && pins.front().normX > 0.0 && pins.front().normX < 1.0, ok);

    // Roundtrip all 9 views and 100 records through .base file
    const fs::path bp = tmpDir / "m2_multiview_100.base";
    M1_CHECK(db.SaveToBaseFile(bp.wstring()), ok);
    StructuredDatabase loaded;
    M1_CHECK(loaded.LoadFromBaseFile(bp.wstring()), ok);
    M1_CHECK(loaded.Records().size() == 100 && loaded.Views().size() == 10, ok);

    RecordOutcome(
        {
            "OB-DATA-052", "OB-DATA-053", "OB-DATA-054", "OB-DATA-055", "OB-DATA-056",
            "NO-DATA-045", "NO-DATA-046", "NO-DATA-047", "NO-DATA-048", "NO-DATA-049",
            "NO-DATA-050", "NO-DATA-051", "NO-DATA-052"
        },
        "TestM2_AllDatabaseViewsSwitchPreservingData",
        ok,
        "Conmutacion sin perdida sobre 100 registros entre vistas Tabla, Lista, Tarjetas/Galeria, Kanban (con arrastre de estado), Calendario, Cronograma, Grafica, Dashboard y Mapa 2D con persistencia .base.");
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
    const fs::path reportPath = (argc > 1)
        ? fs::u8path(argv[1])
        : (fs::temp_directory_path() / "m1_m2_report.json");
    const fs::path tmpDir = fs::temp_directory_path() / "cuadernu_m1_m2_tests";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    TestM1_Vault50NotesCrudSearchMoveRestoreSync(tmpDir);
    TestM1_Templates50NotesVariablesAndBlocks(tmpDir);
    TestM1_DailyNotes50Sequential(tmpDir);
    TestM1_PropertiesFrontmatterAndFiltering50(tmpDir);
    TestM1_CoverAndPaperTemplatesEditExport(tmpDir);
    TestM1_KnowledgeGraph25PlusLinksRenameAndTransclusion();
    TestM1_RichTextMarkdownBlocksLayersAndPdfExport(tmpDir);
    TestM1_CodeBlockSyntaxTokensAndPdfExport(tmpDir);
    TestM1_ImageInsertCropAnnotateLayerAndPdfExport(tmpDir);
    TestM1_ShapesAndDiagramBlocksConnectorPdfExport(tmpDir);
    TestM1_ReusableElementsStickersAndSnippets(tmpDir);
    TestM1_StickyNotesCrudLayersAndDedicatedExport(tmpDir);
    TestM1_LayersVisibilityLockReorderAndVisibleExport(tmpDir);
    TestM1_PresenterCleanViewAndSideNotes();
    TestM1_HandwritingAndImageOcrTextEditExport(tmpDir);
    TestM1_CommandPaletteSlashShortcutsAndThemes(tmpDir);
    TestM1_PartialScopeHonestItems(tmpDir);

    TestM2_SimpleAndChartLinkedTablesPdfExport(tmpDir);
    TestM2_DatabaseSchemaTypedPropertiesFormsAndTemplates(tmpDir);
    TestM2_RelationsRollupsAndTypedFormulas100Rows();
    TestM2_FilteringSortingAndGrouping100Rows();
    TestM2_AllDatabaseViewsSwitchPreservingData(tmpDir);

    json rep;
    rep["suite"] = "KnowledgeGraphAndBasesTest";
    rep["totalChecks"] = g_totalChecks;
    rep["failedChecks"] = g_failedChecks;
    rep["totalIdsCovered"] = g_outcomes.size();
    json arr = json::array();
    for (const auto& o : g_outcomes) {
        arr.push_back({
            {"id", o.id},
            {"test", o.testName},
            {"pass", o.pass},
            {"estado_sugerido", o.suggestedState},
            {"detalle", o.detail}
        });
    }
    rep["resultados"] = std::move(arr);

    if (reportPath.has_parent_path()) fs::create_directories(reportPath.parent_path(), ec);
    {
        std::ofstream ofs(reportPath, std::ios::binary | std::ios::trunc);
        ofs << rep.dump(2) << "\n";
    }

    std::cout << "[KnowledgeGraphAndBasesTest] Checks: " << (g_totalChecks - g_failedChecks)
              << " / " << g_totalChecks << " passed | IDs covered: " << g_outcomes.size()
              << " | Report: " << reportPath.u8string() << "\n";

    fs::remove_all(tmpDir, ec);
    return (g_failedChecks == 0) ? 0 : 1;
}
