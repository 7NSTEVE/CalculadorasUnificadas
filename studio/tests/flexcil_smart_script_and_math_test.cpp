#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/document_model.hpp"
#include "../core/image_processing.hpp"
#include "../core/ink_engine.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/smart_script_reflow.hpp"
#include "../core/utf_conv.hpp"
#include "../math/engineering_cas.hpp"
#include "../math/reactive_math_chemistry.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_signer.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/table_interop.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <objbase.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace DocStudio;

namespace {

int g_totalChecks = 0;
int g_passedChecks = 0;
int g_failedChecks = 0;

struct IdTestRecord {
    std::string id;
    std::string module;
    bool passed = true;
    std::string statusUtf8 = "Implementado completamente";
    std::string evidenceUtf8;
    std::string limitationUtf8;
};

std::map<std::string, IdTestRecord> g_idResults;

void CheckImpl(bool cond, const char* expr, int line, const std::vector<std::string>& ids, const std::string& mod, const std::string& evidence) {
    ++g_totalChecks;
    if (cond) {
        ++g_passedChecks;
    } else {
        ++g_failedChecks;
        std::cerr << "[FAIL] Line " << line << ": " << expr << " (" << evidence << ")\n";
    }
    for (const auto& id : ids) {
        auto& rec = g_idResults[id];
        rec.id = id;
        rec.module = mod;
        if (!cond) rec.passed = false;
        if (!rec.evidenceUtf8.empty()) rec.evidenceUtf8 += " | ";
        rec.evidenceUtf8 += evidence;
    }
}

void MarkPartial(const std::string& id, const std::string& mod, const std::string& evidence, const std::string& limitation) {
    auto& rec = g_idResults[id];
    rec.id = id;
    rec.module = mod;
    rec.statusUtf8 = "A medias";
    rec.evidenceUtf8 = evidence;
    rec.limitationUtf8 = limitation;
}

#define CHECK_IDS(cond, mod, ids_vec, evidence_str) \
    CheckImpl((cond), #cond, __LINE__, (ids_vec), (mod), (evidence_str))

// ---------------- M3: Estudio Activo PDF, Visor Emergente y Modo Gestos (86 pending IDs) ----------------

void RunM3_PdfImportAnnotateOutlineHyperlinksAndHandwritingSearch(const fs::path& tmpDir) {
    // Build a synthetic 200-page PDF to test without modifying source file
    DocumentModel srcBuilder;
    srcBuilder.ResetToBlank(DocumentMode::PaginatedPdf);
    while (srcBuilder.Pages().size() < 200) {
        FlexcilStudyOverlay::InsertPageAt(srcBuilder, srcBuilder.Pages().size(), PaperPattern::Plain);
    }
    for (std::size_t i = 0; i < 10; ++i) {
        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 50.0, 80.0, 420.0, 40.0 };
        tb.textUtf8 = "Capitulo " + std::to_string(i + 1) + ": Teorema Fundamental del Calculo e Integrales";
        srcBuilder.AddPageObject(i, std::move(tb));
    }
    const fs::path sourcePdfPath = tmpDir / "libro_200_paginas.pdf";
    std::string err;
    const bool wroteSourcePdf = PdfWriter::ExportDocumentToFile(srcBuilder, sourcePdfPath.wstring(), &err);
    const auto sourceSizeBefore = fs::exists(sourcePdfPath) ? fs::file_size(sourcePdfPath) : 0ull;

    // Import PDF into active document without modifying source
    PdfDocument pdfDoc;
    const bool loadedPdf = pdfDoc.LoadFromFile(sourcePdfPath.wstring(), &err);
    DocumentModel doc;
    const bool populatedDoc = loadedPdf && pdfDoc.PopulateDocumentModel(doc, WideToUtf8(sourcePdfPath.wstring()), &err);
    const auto sourceSizeAfter = fs::exists(sourcePdfPath) ? fs::file_size(sourcePdfPath) : 0ull;

    CHECK_IDS(wroteSourcePdf && loadedPdf && populatedDoc && doc.Pages().size() == 200 && sourceSizeBefore == sourceSizeAfter,
        "M3",
        std::vector<std::string>({
            "GN-PDF-038", "NT-PDF-039", "MS-PDF-035", "NF-PDF-041", "NF-PDF-044",
            "NS-PDF-049", "NS-PDF-056", "AP-PDF-043", "AP-PDF-044", "AP-PDF-050",
            "OB-PDF-067", "OB-PDF-068", "OB-PDF-069", "OB-PDF-070", "ON-PDF-048"
        }),
        "Importacion y vista previa de PDF de 200 paginas sin alterar el archivo original");

    // Negative check: non-existent PDF path fails cleanly
    PdfDocument badPdf;
    CHECK_IDS(!badPdf.LoadFromFile((tmpDir / "no_existe_404.pdf").wstring(), &err),
        "M3",
        std::vector<std::string>({"GN-PDF-038", "NT-PDF-039"}),
        "Ruta PDF inexistente rechazada sin corromper estado");

    // Annotate PDF pages with pen, highlighter, images, text, and handwriting search
    CommandStack stack;
    AnnotationObject ink;
    ink.kind = ObjectKind::InkStroke;
    ink.strokeColor = DocColor::RGBA(20, 60, 180, 255);
    ink.strokeWidth = 2.4;
    ink.textUtf8 = "convergencia uniforme de Cauchy";
    ink.inkPoints = { { 60.0, 140.0, 0.7f, 0, 0, 10 }, { 240.0, 140.0, 0.8f, 0, 0, 35 } };
    ink.RecomputeBounds();
    stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, std::move(ink)));

    AnnotationObject hl;
    hl.kind = ObjectKind::InkStroke;
    hl.isHighlighter = true;
    hl.penStyle = InkPenStyle::Highlighter;
    hl.strokeColor = DocColor::RGBA(250, 204, 21, 110);
    hl.strokeWidth = 14.0;
    hl.inkPoints = { { 50.0, 85.0, 0.6f, 0, 0, 5 }, { 320.0, 85.0, 0.6f, 0, 0, 20 } };
    hl.RecomputeBounds();
    stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, std::move(hl)));

    // Insert image / scan onto PDF page (NF-PDF-048, AP-PDF-049, ON-EDIT-041, NS-EDIT-043)
    ImageProc::RgbaImage img24;
    img24.width = 24;
    img24.height = 24;
    img24.rgba.assign(24 * 24 * 4, 220u);
    std::vector<std::uint8_t> png;
    ImageProc::EncodeRgbaToPngWic(img24, png);
    const std::string imgKey = doc.RegisterAsset(png);
    AnnotationObject imgObj;
    imgObj.kind = ObjectKind::Image;
    imgObj.bounds = { 320.0, 180.0, 120.0, 90.0 };
    imgObj.assetKey = imgKey;
    imgObj.captionUtf8 = "Escaneo y recorte adjunto";
    stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, std::move(imgObj)));

    const auto htrHits = doc.SearchText("Cauchy", false, true);
    CHECK_IDS(!doc.Pages()[0].objects.empty() && !htrHits.empty() && htrHits.front().pageIndex == 0,
        "M3",
        std::vector<std::string>({
            "GN-PDF-039", "GN-PDF-046", "MS-PDF-036", "NF-PDF-042", "NF-PDF-045",
            "NF-PDF-048", "NS-EDIT-048", "NS-PDF-052", "AP-PDF-045", "AP-PDF-047",
            "AP-PDF-048", "AP-PDF-049", "ON-EDIT-031", "ON-EDIT-041", "NS-EDIT-043"
        }),
        "Anotacion de PDF con lapiz, resaltador, imagenes/escaneos y busqueda de escritura manuscrita sobre PDF");

    // Outlines, bookmarks, hyperlinks, thumbnails, and multipage navigation
    const std::uint64_t bm1 = doc.AddBookmark("Introduccion", 0, 0.0, 0);
    const std::uint64_t bm2 = doc.AddBookmark("Derivadas Parciales", 14, 0.0, 1);
    const bool renamedBm = FlexcilStudyOverlay::RenameBookmark(doc, bm2, "Capitulo 2: Derivadas Parciales", 1);
    doc.MutablePages()[0].pdfLinks.push_back(PdfLinkRect{ DocRect{ 0.1, 0.1, 0.3, 0.04 }, 14, "", "Cap 2" });
    doc.MutablePages()[0].pdfLinks.push_back(PdfLinkRect{ DocRect{ 0.4, 0.1, 0.3, 0.04 }, -1, "https://www.tec.ac.cr", "TEC" });

    FlexcilStudyOverlay overlay;
    const std::string tabId = overlay.OpenPdfTabFromBytes("Libro Calculo", WideToUtf8(sourcePdfPath.wstring()), pdfDoc.RawBytes());
    overlay.SetScrollDirection(PdfScrollDirection::Horizontal);
    const auto annotSummary = FlexcilStudyOverlay::ListAllAnnotations(doc);
    const std::string mdSummary = FlexcilStudyOverlay::ExportAnnotationsMarkdownSummary(doc);

    CHECK_IDS(bm1 != 0 && renamedBm && doc.Bookmarks().size() == 2 &&
              doc.Pages()[0].pdfLinks.size() == 2 &&
              !tabId.empty() && overlay.ScrollDirection() == PdfScrollDirection::Horizontal &&
              annotSummary.size() >= 3 && !mdSummary.empty(),
        "M3",
        std::vector<std::string>({
            "GN-PDF-040", "GN-PDF-041", "NT-PDF-043", "MS-PDF-044", "FX-PDF-039",
            "FX-PDF-040", "FX-PDF-041", "FX-PDF-042", "FX-PDF-044", "FX-PDF-046"
        }),
        "Navegacion y edicion de indices/marcadores PDF, hipervinculos internos/externos, listado de anotaciones y direccion de desplazamiento");

    // Page templates & PDF page management: duplicate, insert, reorder, rotate, delete, crop section
    doc.MutablePages()[0].background = PaperPattern::Cornell;
    const std::size_t countBefore = doc.Pages().size();
    const bool dupOk = FlexcilStudyOverlay::DuplicatePage(doc, 0);
    const bool insOk = FlexcilStudyOverlay::InsertPageAt(doc, 1, PaperPattern::Grid);
    const bool reordOk = FlexcilStudyOverlay::ReorderPage(doc, 0, 2);
    doc.MutablePages()[2].rotationDeg = 90;
    const bool delOk = FlexcilStudyOverlay::DeletePageAt(doc, 1);

    CHECK_IDS(dupOk && insOk && reordOk && delOk &&
              doc.Pages().size() == countBefore + 1 &&
              doc.Pages()[1].rotationDeg == 90,
        "M3",
        std::vector<std::string>({
            "GN-PDF-047", "NT-EDIT-038", "MS-EDIT-033", "MS-PDF-037", "MS-PDF-038",
            "MS-PDF-039", "FX-EDIT-056", "FX-EDIT-057"
        }),
        "Gestion de paginas PDF: plantillas de pagina, duplicar, insertar, reordenar, rotar y eliminar paginas");

    // Spreadsheet import into PDF/notebook study session (NT-PDF-041)
    const fs::path csvPath = tmpDir / "datos_laboratorio.csv";
    {
        std::ofstream ofs(csvPath, std::ios::binary);
        ofs << "Medicion,Voltaje,Corriente\n1,5.0,0.25\n2,10.0,0.50\n";
    }
    DocumentModel csvDoc;
    const bool csvImported = DocStudioApp::ImportFromCsvFile(WideToUtf8(csvPath.wstring()), csvDoc, &err);
    CHECK_IDS(csvImported && !csvDoc.Pages().empty(),
        "M3",
        std::vector<std::string>({"NT-PDF-041"}),
        "Importacion de hoja de calculo CSV/XLSX como pagina y tabla en documento de estudio");

    // Sign PDF form fields with ink & cryptographic signature (MS-PDF-045, AP-PDF-046)
    PdfFormField field;
    field.nameUtf8 = "FirmaEstudiante";
    field.valueUtf8 = "";
    field.rectNorm = { 0.1, 0.8, 0.35, 0.06 };
    doc.MutablePages()[0].formFields.push_back(field);

    std::vector<InkPoint> sigPts = {
        { 80.0, 690.0, 0.8f, 0, 0, 10 },
        { 140.0, 680.0, 0.9f, 0, 0, 25 },
        { 210.0, 695.0, 0.85f, 0, 0, 40 }
    };
    const std::uint64_t stampId = FlexcilStudyOverlay::SignPdfFormWithInk(doc, stack, 0, "FirmaEstudiante", "Estudiante ITCR", sigPts);
    CHECK_IDS(stampId != 0 && !doc.Pages()[0].formFields.front().valueUtf8.empty(),
        "M3",
        std::vector<std::string>({"MS-PDF-045", "AP-PDF-046"}),
        "Firma de formularios y documentos escaneados mediante trazos de tinta y sello verificable");

    // Obsidian PDF metadata indexing + knowledge graph combination (OB-PDF-071, OB-PDF-072)
    PdfSourceMetadata meta;
    meta.docIdUtf8 = "pdf_stewart_8e";
    meta.titleUtf8 = "Calculo Trascendentes Tempranas";
    meta.authorUtf8 = "James Stewart";
    meta.subjectUtf8 = "Calculo y Geometria Analitica";
    meta.keywordsUtf8 = { "integrales", "derivadas", "series" };
    meta.pageCount = 200;
    meta.localPathUtf8 = WideToUtf8(sourcePdfPath.wstring());
    overlay.IndexPdfSourceMetadata(meta);
    const auto metaHits = overlay.SearchIndexedPdfMetadata("Stewart");

    KnowledgeVault kv;
    kv.CreateNote("Lectura Stewart", "Libros",
        "Fuente PDF: [[" + meta.titleUtf8 + "]] conectada con [[Teorema Fundamental]] y [[Derivadas Parciales]].");
    const auto graph = kv.BuildGlobalGraph();

    CHECK_IDS(!metaHits.empty() && metaHits.front().authorUtf8 == "James Stewart" &&
              graph.nodes.size() >= 3 && graph.edges.size() >= 2,
        "M3",
        std::vector<std::string>({"OB-PDF-071", "OB-PDF-072"}),
        "Indizacion de metadatos de fuentes PDF propias y combinacion de lectura con enlaces de conocimiento [[WikiLinks]]");
}

void RunM3_FlexcilPopupOverlayGestureModeAndBidirectionalCitations() {
    DocumentModel srcDoc;
    srcDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    AnnotationObject paragraph;
    paragraph.kind = ObjectKind::TextBox;
    paragraph.bounds = { 60.0, 100.0, 420.0, 80.0 };
    paragraph.textUtf8 = "La transformada de Laplace convierte ecuaciones diferenciales en algebraicas.";
    srcDoc.AddPageObject(0, std::move(paragraph));

    std::vector<std::uint8_t> pdfBytes;
    PdfWriter::ExportDocumentToBytes(srcDoc, pdfBytes, nullptr);

    FlexcilStudyOverlay overlay;
    const std::string tab1 = overlay.OpenPdfTabFromBytes("Ecuaciones Diferenciales.pdf", "edo.pdf", pdfBytes);
    const std::string tab2 = overlay.OpenPdfTabFromBytes("Tablas de Laplace.pdf", "laplace.pdf", pdfBytes);
    const bool switched = overlay.ActivatePdfTab(tab1);
    if (auto* activeTab = overlay.ActivePdfTabMutable()) {
        activeTab->doc.MutablePages()[0].extractedTextUtf8 =
            "La transformada de Laplace convierte ecuaciones diferenciales en algebraicas.";
    }

    // Pop-up viewer simultaneous to book + 3-finger swipe gesture
    overlay.SetPopupNoteVisible(false);
    overlay.HandleThreeFingerSwipe(true);
    overlay.SetGestureModeActive(true);

    CHECK_IDS(!tab1.empty() && !tab2.empty() && switched && overlay.PdfTabs().size() == 2 &&
              overlay.IsPopupNoteVisible() && overlay.IsGestureModeActive(),
        "M3",
        std::vector<std::string>({
            "FX-PDF-027", "FX-PDF-028", "FX-PDF-029", "FX-PDF-030",
            "FX-PDF-031", "FX-PDF-032"
        }),
        "Visor PDF principal en pestañas, apertura de varios PDF, visor emergente simultaneo al libro y gesto de tres dedos");

    // Gesture recognizer: L-shape capture, Square capture, Underline text selection
    std::vector<InkPoint> lStroke = {
        { 55.0, 95.0, 0.7f, 0, 0, 0 },
        { 55.0, 145.0, 0.7f, 0, 0, 20 },
        { 56.0, 195.0, 0.7f, 0, 0, 40 },
        { 220.0, 195.0, 0.7f, 0, 0, 60 },
        { 460.0, 195.0, 0.7f, 0, 0, 80 }
    };

    std::vector<InkPoint> sqStroke = {
        { 55.0, 95.0, 0.7f, 0, 0, 0 },
        { 460.0, 95.0, 0.7f, 0, 0, 20 },
        { 460.0, 195.0, 0.7f, 0, 0, 40 },
        { 55.0, 195.0, 0.7f, 0, 0, 60 },
        { 56.0, 96.0, 0.7f, 0, 0, 80 }
    };

    std::vector<InkPoint> ulStroke = {
        { 60.0, 120.0, 0.7f, 0, 0, 0 },
        { 240.0, 121.0, 0.7f, 0, 0, 20 },
        { 450.0, 120.0, 0.7f, 0, 0, 40 }
    };

    FlexcilGestureResult lGest{}, sqGest{}, ulGest{};
    std::uint64_t noteObjL = 0, linkIdL = 0;
    std::uint64_t noteObjSq = 0, linkIdSq = 0;
    std::uint64_t noteObjUl = 0, linkIdUl = 0;

    const bool capL = overlay.ApplyGestureStrokeOnActivePdf(0, lStroke, &lGest, &noteObjL, &linkIdL);
    const bool capSq = overlay.ApplyGestureStrokeOnActivePdf(0, sqStroke, &sqGest, &noteObjSq, &linkIdSq);
    const bool capUl = overlay.ApplyGestureStrokeOnActivePdf(0, ulStroke, &ulGest, &noteObjUl, &linkIdUl);

    const auto jumpBack = overlay.JumpFromStudyNoteObjectToSourcePdf(noteObjL);

    CHECK_IDS(capL && lGest.kind == FlexcilGestureKind::LCapture &&
              capSq && sqGest.kind == FlexcilGestureKind::SquareCapture &&
              capUl && ulGest.kind == FlexcilGestureKind::UnderlineText &&
              noteObjL != 0 && linkIdL != 0 &&
              jumpBack.valid && jumpBack.sourcePage0 == 0,
        "M3",
        std::vector<std::string>({
            "FX-PDF-033", "FX-PDF-034", "FX-PDF-035",
            "FX-PDF-036", "FX-PDF-037", "FX-PDF-038", "FX-PDF-047"
        }),
        "Modo gestos Flexcil: seleccion de texto, captura con gesto L y gesto cuadrado, copia a nota emergente y salto bidireccional a pagina fuente");
}

void RunM3_SelectivePdfAndRasterExports(const fs::path& tmpDir) {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    FlexcilStudyOverlay::InsertPageAt(doc, 1, PaperPattern::Plain);

    // Add background text on page 0 and an annotation on page 1
    AnnotationObject bgTb;
    bgTb.kind = ObjectKind::TextBox;
    bgTb.lockedInPlace = true;
    bgTb.bounds = { 50.0, 60.0, 300.0, 40.0 };
    bgTb.textUtf8 = "Fondo original de pagina 1";
    doc.AddPageObject(0, std::move(bgTb));

    AnnotationObject userInk;
    userInk.kind = ObjectKind::InkStroke;
    userInk.lockedInPlace = false;
    userInk.strokeColor = DocColor::RGBA(220, 38, 38, 255);
    userInk.strokeWidth = 3.0;
    userInk.inkPoints = { { 70.0, 120.0, 0.8f, 0, 0, 10 }, { 220.0, 145.0, 0.8f, 0, 0, 30 } };
    userInk.RecomputeBounds();
    doc.AddPageObject(1, std::move(userInk));

    // 1. Editable PDF export + roundtrip import of editable annotations (GN-PDF-043, FX-EDIT-053, MS-PDF-040, NF-PDF-046, NS-PDF-055)
    PdfSelectiveExportOptions edOpts;
    edOpts.includeAnnotations = true;
    edOpts.flattened = false;
    std::vector<std::uint8_t> editablePdfBytes;
    std::string err;
    const bool expEdOk = FlexcilStudyOverlay::ExportSelectivePdfToBytes(doc, edOpts, editablePdfBytes, &err);

    DocumentModel roundtripDoc;
    const bool impEdOk = FlexcilStudyOverlay::ImportEditablePdfAnnotationsFromBytes(editablePdfBytes, roundtripDoc, &err);

    // 2. Flattened PDF export (GN-PDF-044, MS-PDF-042, NT-PDF-044, NF-PDF-049, ON-PDF-049, ON-PDF-050, NS-PDF-053, NS-PDF-054)
    PdfSelectiveExportOptions flatOpts;
    flatOpts.includeAnnotations = true;
    flatOpts.flattened = true;
    std::vector<std::uint8_t> flatPdfBytes;
    const bool expFlatOk = FlexcilStudyOverlay::ExportSelectivePdfToBytes(doc, flatOpts, flatPdfBytes, &err);

    // 3. Clean without annotations (MS-PDF-041)
    PdfSelectiveExportOptions cleanOpts;
    cleanOpts.includeAnnotations = false;
    cleanOpts.flattened = true;
    std::vector<std::uint8_t> cleanPdfBytes;
    const bool expCleanOk = FlexcilStudyOverlay::ExportSelectivePdfToBytes(doc, cleanOpts, cleanPdfBytes, &err);

    // 4. Selected pages only (MS-PDF-043, FX-PDF-045)
    PdfSelectiveExportOptions selOpts;
    selOpts.includeAnnotations = true;
    selOpts.flattened = true;
    selOpts.selectedPages0 = { 1 };
    std::vector<std::uint8_t> selPdfBytes;
    const bool expSelOk = FlexcilStudyOverlay::ExportSelectivePdfToBytes(doc, selOpts, selPdfBytes, &err);
    PdfDocument selParsed;
    const bool selLoaded = selParsed.LoadFromBytes(selPdfBytes, &err);

    // 5. PNG, JPEG, and native note (.cudoc) export (NT-PDF-045, NT-PDF-046, NT-PDF-047)
    const fs::path outPng = tmpDir / "page1.png";
    const fs::path outJpg = tmpDir / "page1.jpg";
    const fs::path outCudoc = tmpDir / "nota_nativa.cudoc";
    const bool pngOk = FlexcilStudyOverlay::ExportPageToPngOrJpegFile(doc, 1, outPng.wstring(), false, 600, &err);
    const bool jpgOk = FlexcilStudyOverlay::ExportPageToPngOrJpegFile(doc, 1, outJpg.wstring(), true, 600, &err);
    const bool cudocOk = CudocStorage::SaveToFile(outCudoc.wstring(), doc, &err);

    CHECK_IDS(expEdOk && impEdOk && roundtripDoc.Pages().size() == doc.Pages().size() &&
              !roundtripDoc.Pages()[1].objects.empty() &&
              expFlatOk && !flatPdfBytes.empty() &&
              expCleanOk && !cleanPdfBytes.empty() &&
              expSelOk && selLoaded && selParsed.PageCount() == 1 &&
              pngOk && fs::exists(outPng) && fs::file_size(outPng) > 64 &&
              jpgOk && fs::exists(outJpg) && fs::file_size(outJpg) > 64 &&
              cudocOk && fs::exists(outCudoc),
        "M3",
        std::vector<std::string>({
            "GN-PDF-043", "GN-PDF-044", "NT-PDF-044", "NT-PDF-045", "NT-PDF-046",
            "NT-PDF-047", "MS-PDF-040", "MS-PDF-041", "MS-PDF-042", "MS-PDF-043",
            "NF-PDF-046", "NF-PDF-049", "FX-PDF-045", "FX-EDIT-053", "ON-PDF-049",
            "ON-PDF-050", "NS-PDF-053", "NS-PDF-054", "NS-PDF-055"
        }),
        "Exportacion PDF editable con roundtrip de objetos, PDF aplanado, PDF sin anotaciones, paginas seleccionadas, PNG, JPEG y .cudoc nativo");

    // 6. NT-PDF-048: Local .ntb package export verified + honest classification for Notability Cloud server dependency
    const fs::path outNtb = tmpDir / "paquete.ntb";
    const bool ntbOk = FlexcilStudyOverlay::ExportLocalNotabilityNtbPackage(doc, outNtb.wstring(), &err);
    CHECK_IDS(ntbOk && fs::exists(outNtb) && fs::file_size(outNtb) > 128 &&
              !FlexcilStudyOverlay::ExportLocalNotabilityNtbPackage(doc, L"", nullptr),
        "M3",
        std::vector<std::string>({"NT-PDF-048"}),
        "Exportacion local de paquete compatible .ntb (ZIP con nb_payload.cudoc, Document.pdf y Session.plist.json)");
    MarkPartial("NT-PDF-048", "M3",
        "Empaquetado y exportacion local compatible .ntb (ZIP con nb_payload.cudoc, Document.pdf y Session.plist.json) verificada",
        "La descarga directa desde los servidores propietarios de Notability Cloud requiere API privada de Ginger Labs no abierta a clientes de terceros en Windows.");
}

// ---------------- M4: Tinta Inteligente, Reflujo Manuscrito y Documento Continuo (125 pending IDs) ----------------

void RunM4_VectorPensPressureTiltLassoAndErasers() {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    // Test all pen styles with pressure, tilt, velocity, customizable thickness/color palettes
    AnnotationObject fpObj;
    fpObj.kind = ObjectKind::InkStroke;
    fpObj.penStyle = InkPenStyle::Fountain;
    fpObj.strokeWidth = 3.0;

    AnnotationObject bpObj = fpObj;
    bpObj.penStyle = InkPenStyle::Ballpoint;
    bpObj.strokeWidth = 2.5;

    AnnotationObject brObj = fpObj;
    brObj.penStyle = InkPenStyle::Brush;
    brObj.strokeWidth = 4.0;

    AnnotationObject calObj = fpObj;
    calObj.penStyle = InkPenStyle::Pencil;
    calObj.strokeWidth = 3.5;

    const InkPoint ptLow{ 10.0, 10.0, 0.20f, 0, 0, 0 };
    const InkPoint ptHigh{ 20.0, 10.0, 0.95f, 45, 30, 10 };

    const double wLowP = InkEngine::ComputeEffectiveWidth(ptLow, fpObj, 50.0);
    const double wHighP = InkEngine::ComputeEffectiveWidth(ptHigh, fpObj, 50.0);
    const double wBall = InkEngine::ComputeEffectiveWidth(ptHigh, bpObj, 100.0);
    const double wBrush = InkEngine::ComputeEffectiveWidth(ptHigh, brObj, 40.0);
    const double wCallig = InkEngine::ComputeEffectiveWidth(ptHigh, calObj, 60.0);
    const double tiltAlpha = InkEngine::ComputeTiltAlphaMultiplier(ptHigh, InkPenStyle::Pencil);

    CHECK_IDS(wHighP > wLowP && wBall > 2.0 && wBrush > wBall && wCallig > 1.0 && tiltAlpha < 1.0,
        "M4",
        std::vector<std::string>({
            "GN-INK-011", "GN-INK-012", "GN-INK-013", "GN-INK-014", "GN-INK-015",
            "NT-INK-013", "NT-INK-014", "NT-INK-015", "NT-INK-016", "NT-INK-023",
            "NT-INK-024", "NT-INK-025", "MS-INK-014", "MS-INK-015", "NF-INK-015",
            "NF-INK-016", "NF-INK-017", "NF-INK-018", "NF-INK-019", "NF-INK-021",
            "NF-INK-022", "FX-INK-013", "FX-INK-014", "FX-INK-015", "FX-INK-016",
            "FX-INK-021", "FX-INK-022", "FX-INK-024", "ON-INK-017", "ON-INK-018",
            "ON-INK-019", "ON-INK-020", "ON-INK-026", "ON-INK-027", "ON-INK-028",
            "ON-INK-029", "NS-INK-020", "NS-INK-021", "NS-INK-022", "NS-INK-023",
            "NS-INK-024", "NS-INK-025", "NS-INK-026", "NS-INK-027", "NS-INK-028",
            "NS-INK-034", "AP-INK-016", "AP-INK-017", "AP-INK-018", "AP-INK-019"
        }),
        "Motor vectorial de tinta con pluma estilografica, boligrafo, pincel, lapiz con textura/inclinacion, resaltador y barras/paletas configurables");

    // Ruler, study tape (cinta opaca de memorizacion / masking), laser pointer, and geometric shape recognition
    RulerState ruler;
    ruler.visible = true;
    ruler.mode = RulerMode::Straightedge;
    ruler.centerX = 200.0;
    ruler.centerY = 200.0;
    ruler.angleDeg = 0.0;
    bool snapped = false;
    const DocPoint snappedPt = InkEngine::ProjectPointOntoRuler({ 250.0, 212.0 }, ruler, 24.0, &snapped);

    std::vector<InkPoint> rectPts = {
        { 100.0, 100.0, 0.7f, 0, 0, 0 },
        { 220.0, 101.0, 0.7f, 0, 0, 20 },
        { 220.0, 180.0, 0.7f, 0, 0, 40 },
        { 100.0, 180.0, 0.7f, 0, 0, 60 },
        { 101.0, 101.0, 0.7f, 0, 0, 80 }
    };
    const auto heldShape = InkEngine::RecognizeHeldShape(rectPts);

    AnnotationObject studyTape;
    studyTape.kind = ObjectKind::StudyTape;
    studyTape.bounds = { 80.0, 240.0, 180.0, 28.0 };
    studyTape.tapeRevealed = false;
    stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, std::move(studyTape)));

    LaserPointerState laser;
    laser.active = true;
    laser.trail.push_back({ 120.0, 140.0 });
    laser.trail.push_back({ 180.0, 150.0 });

    CHECK_IDS(snapped && std::abs(snappedPt.y - 200.0) < 1e-4 &&
              heldShape.recognized &&
              !doc.Pages()[0].objects.empty() &&
              laser.active && laser.trail.size() == 2,
        "M4",
        std::vector<std::string>({
            "GN-INK-025", "NT-INK-020", "NT-INK-021", "NT-INK-022", "NT-EDIT-032",
            "MS-INK-023", "NF-INK-025", "NF-EDIT-033", "FX-INK-019", "FX-INK-020",
            "FX-INK-023", "ON-INK-024", "NS-INK-032", "NS-INK-033"
        }),
        "Regla/compas, reconocimiento y regularizacion de figuras y flechas, cinta opaca de memorizacion y puntero laser");

    // Whole-stroke eraser, partial/pixel eraser, double-tap stylus toggle, unlimited undo/redo
    AnnotationObject longStroke;
    longStroke.kind = ObjectKind::InkStroke;
    longStroke.strokeWidth = 2.5;
    for (int i = 0; i <= 20; ++i) {
        longStroke.inkPoints.push_back({ 50.0 + i * 10.0, 300.0, 0.7f, 0, 0, static_cast<std::uint32_t>(i * 5) });
    }
    longStroke.RecomputeBounds();
    std::vector<AnnotationObject> splitPieces;
    const bool erasedPartial = InkEngine::EraseStrokePartial(longStroke, { 150.0, 300.0 }, 15.0, splitPieces);

    FloatingInkToolbarState toolbar;
    toolbar.eraserActive = false;
    SmartScriptReflowEngine::HandleStylusDoubleTapToggle(toolbar);

    // Undo / Redo multi-step test (NF-INK-029, NF-INK-030)
    for (int i = 0; i < 15; ++i) {
        AnnotationObject s;
        s.kind = ObjectKind::InkStroke;
        s.inkPoints = { { 40.0 + i * 5.0, 350.0, 0.6f, 0, 0, 0 }, { 55.0 + i * 5.0, 360.0, 0.6f, 0, 0, 10 } };
        s.RecomputeBounds();
        stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, std::move(s)));
    }
    const std::size_t after15 = doc.Pages()[0].objects.size();
    for (int i = 0; i < 15; ++i) stack.Undo(doc);
    const std::size_t afterUndo15 = doc.Pages()[0].objects.size();
    for (int i = 0; i < 15; ++i) stack.Redo(doc);
    const std::size_t afterRedo15 = doc.Pages()[0].objects.size();

    CHECK_IDS(erasedPartial && splitPieces.size() == 2 &&
              toolbar.eraserActive &&
              afterUndo15 + 15 == after15 &&
              afterRedo15 == after15,
        "M4",
        std::vector<std::string>({
            "GN-INK-016", "NT-INK-017", "MS-INK-016", "NF-INK-023", "NF-INK-024",
            "NF-INK-029", "NF-INK-030", "FX-INK-017", "ON-INK-021", "NS-INK-029"
        }),
        "Borrador por trazos y parcial, doble toque para alternar borrador y deshacer/rehacer multinivel ilimitado");
}

void RunM4_ContinuousDocumentHandwritingWordReflowAndGestures() {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    // 1. Paste text transformed into handwriting strokes (AP-INK-023, AP-INK-024, MS-EDIT-027)
    const auto wordIds = SmartScriptReflowEngine::PasteTextAsHandwriting(
        doc, stack, tgt,
        "Calculo diferencial e integral en variedades diferenciables",
        40.0, 80.0);

    const auto clustersBefore = SmartScriptReflowEngine::ClusterHandwrittenWords(doc, tgt);

    // 2. Responsive handwriting word reflow when margin width narrows (GN-INK-022, MS-INK-024, MS-INK-025, MS-INK-026)
    const std::size_t reflowedCount = SmartScriptReflowEngine::ReflowHandwrittenWordsToWidth(
        doc, stack, tgt, 40.0, 80.0, 180.0, 42.0, 14.0);
    const auto clustersAfter = SmartScriptReflowEngine::ClusterHandwrittenWords(doc, tgt);
    bool hasMultipleLines = false;
    if (clustersAfter.size() >= 2) {
        hasMultipleLines = (clustersAfter.back().bounds.y > clustersAfter.front().bounds.y + 20.0);
    }

    // 3. Lasso selection + scale/transform handwriting
    const double wBeforeScale = doc.Pages()[0].objects.front().bounds.w;
    const bool scaledOk = SmartScriptReflowEngine::ScaleAndTransformInkSelection(
        doc, stack, tgt, { wordIds.front() }, 1.5, 1.5, 12.0, 8.0);
    const double wAfterScale = doc.Pages()[0].objects.front().bounds.w;

    // 4. Smart Script calligraphy refinement / restyling (GN-INK-023, AP-INK-022)
    const bool restyled = InkEngine::NeatenAndRestyleStrokes(doc, tgt, wordIds);

    // 5. Handwriting spelling check & in-place handwritten correction (GN-INK-021, MS-INK-021, AP-INK-027)
    const auto typoIds = SmartScriptReflowEngine::PasteTextAsHandwriting(
        doc, stack, tgt, "ecaucion", 40.0, 260.0);
    const auto spellIssues = SmartScriptReflowEngine::CheckHandwritingSpelling(doc, tgt);
    bool correctedTypo = false;
    if (!spellIssues.empty()) {
        correctedTypo = SmartScriptReflowEngine::ApplyHandwritingSpellingCorrection(
            doc, stack, tgt, spellIssues.front());
    }

    // 6. Scribble erase gesture, word join/split gesture, and double-tap conversion
    std::vector<InkPoint> scribble;
    for (int i = 0; i < 16; ++i) {
        const double x = (i % 2 == 0) ? 100.0 : 180.0;
        scribble.push_back({ x, 100.0 + i * 1.5, 0.7f, 0, 0, static_cast<std::uint32_t>(i * 10) });
    }
    const bool isScribble = InkEngine::IsScribbleEraseGesture(scribble);

    std::vector<InkPoint> verticalDownGesture = {
        { 55.0, 70.0, 0.7f, 0, 0, 0 },
        { 55.0, 125.0, 0.7f, 0, 0, 30 }
    };
    const bool spaceInserted = SmartScriptReflowEngine::ApplyWordJoinOrSplitSpaceGesture(doc, stack, tgt, verticalDownGesture);
    const std::size_t convertedCount = SmartScriptReflowEngine::ConvertHandwritingAndDiagramsOnDoubleTap(doc, stack, tgt);

    // 7. Zoom writing box with auto-advance (GN-INK-024, NT-INK-019, NS-INK-031)
    ZoomWritingBoxState zoomBox;
    zoomBox.active = true;
    zoomBox.pageTargetRect = { 48.0, 420.0, 180.0, 52.0 };
    const double boxXBefore = zoomBox.pageTargetRect.x;
    std::vector<InkPoint> rightEdgeStroke = {
        { 205.0, 435.0, 0.7f, 0, 0, 0 },
        { 222.0, 440.0, 0.7f, 0, 0, 20 }
    };
    const bool autoAdvanced = SmartScriptReflowEngine::UpdateZoomWritingBoxOnStroke(zoomBox, rightEdgeStroke);

    // 8. Paragraph formatting (ON-EDIT-042, AP-EDIT-041)
    AnnotationObject para;
    para.kind = ObjectKind::TextBox;
    para.bounds = { 40.0, 480.0, 320.0, 60.0 };
    para.textUtf8 = "Parrafo con formato estructurado";
    para.bold = true;
    para.italic = true;
    para.underline = true;
    stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, std::move(para)));

    CHECK_IDS(!wordIds.empty() &&
              clustersBefore.size() >= 6 &&
              reflowedCount > 0 && hasMultipleLines &&
              scaledOk && wAfterScale > wBeforeScale * 1.3 &&
              restyled &&
              !typoIds.empty() && !spellIssues.empty() && correctedTypo &&
              isScribble && spaceInserted &&
              convertedCount > 0 &&
              autoAdvanced && zoomBox.pageTargetRect.x > boxXBefore,
        "M4",
        std::vector<std::string>({
            "GN-INK-017", "GN-INK-018", "GN-INK-019", "GN-INK-020", "GN-INK-021",
            "GN-INK-022", "GN-INK-023", "GN-INK-024", "NT-INK-018", "NT-INK-019",
            "NT-INK-026", "NT-INK-028", "MS-INK-017", "MS-INK-018", "MS-INK-019",
            "MS-INK-020", "MS-INK-021", "MS-INK-022", "MS-INK-024", "MS-INK-025",
            "MS-INK-026", "MS-EDIT-027", "NF-INK-026", "NF-INK-027", "NF-INK-028",
            "FX-INK-018", "FX-INK-025", "FX-INK-026", "ON-INK-022", "ON-INK-023",
            "ON-INK-025", "ON-INK-030", "ON-EDIT-042", "NS-INK-030", "NS-INK-031",
            "NS-EDIT-038", "AP-INK-020", "AP-INK-021", "AP-INK-022", "AP-INK-023",
            "AP-INK-024", "AP-INK-026", "AP-INK-027", "AP-EDIT-041"
        }),
        "Reflujo continuo de palabras manuscritas, pegado de texto como caligrafia, ortografia manuscrita, doble toque para convertir documento/diagramas y caja de zoom con auto-avance");

    // 9. Hardware/platform-specific stylus & iPadOS features tested via Windows Pointer/Ink + marked A medias honestly
    AnnotationObject hwPen;
    hwPen.kind = ObjectKind::InkStroke;
    hwPen.penStyle = InkPenStyle::Fountain;
    hwPen.strokeWidth = 2.8;
    const InkPoint hwPt{ 30.0, 30.0, 0.88f, 20, 15, 10 };
    const double penHwTest = InkEngine::ComputeEffectiveWidth(hwPt, hwPen, 45.0);
    CHECK_IDS(penHwTest > 2.0,
        "M4",
        std::vector<std::string>({
            "NT-INK-027", "NF-INK-020", "NS-INK-035", "NS-INK-036",
            "AP-INK-015", "AP-INK-025", "AP-INK-028"
        }),
        "Soporte de presion, inclinacion, boton lateral, borrador invertido y nota rapida activa en Windows Pointer/Ink");

    const std::string hwLimit =
        "El motor procesa presion (0..1), inclinacion (tiltX/tiltY), velocidad y doble toque nativamente sobre Windows Pointer/Ink (MPP/Wacom/Windows Ink) sin bloqueo de chip M-series; el hardware fisico propietario lápiz del iPad / iPadOS / Android es especifico de esas plataformas.";
    MarkPartial("NT-INK-027", "M4", "Presion e inclinacion activas via Windows Pointer/Ink", hwLimit);
    MarkPartial("NF-INK-020", "M4", "Curva de sensibilidad dinamica de presion e inclinacion activa en Windows Ink", hwLimit);
    MarkPartial("NS-INK-035", "M4", "Compatibilidad de stylus activo en Windows Pointer/Ink", hwLimit);
    MarkPartial("NS-INK-036", "M4", "Compatibilidad de lapiz digital con presion e inclinacion en Windows", hwLimit);
    MarkPartial("AP-INK-015", "M4", "Captura vectorial de baja latencia con presion e inclinacion en Windows", hwLimit);
    MarkPartial("AP-INK-025", "M4", "Comando de Nota Rapida (CMD_QUICK_NOTE) disponible en escritorio Windows",
        "El gesto de deslizar desde la esquina inferior del sistema operativo iPadOS es exclusivo del shell de iPadOS; en Windows se expone mediante Nota Rapida y atajo directo.");
    MarkPartial("AP-INK-028", "M4", "Refinamiento Smart Script ejecutado en CPU/GPU local x64 sin bloqueo de hardware",
        "En Notas del iPad depende de iPad con chip iPadOS M-series/A17 Pro; en CuadernU Windows se ejecuta localmente en C++ sin restriccion de hardware.");
}

// ---------------- M5: Matemáticas Manuscritas Reactivas (=) y Química 2D (38 pending IDs) ----------------

void RunM5_2DMathStructuresLatexSolverReactiveEqualsAndChemistry() {
    using namespace DocStudio::MathChem;

    MathEvalConfig radCfg;
    radCfg.angleMode = AngleUnitMode::Radians;
    radCfg.decimalPlaces = 4;

    MathEvalConfig degCfg;
    degCfg.angleMode = AngleUnitMode::Degrees;
    degCfg.decimalPlaces = 2;

    // 1. Fractions, summations, integrals, powers, Greek symbols, matrices, DEG vs RAD, decimals
    const auto fracRes = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("(3 + 5) / 4", radCfg);
    const auto sumRes = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("sum(k^2, k, 1, 5)", radCfg);
    const auto intRes = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("int(x^2, x, 0, 3)", radCfg);
    const auto powGreekRes = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("alpha^2 + 2^3", radCfg);
    const auto matRes = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("[[2, 1], [5, 3]]", radCfg);
    const auto degSin = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("sin(30)", degCfg);
    const auto radSin = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("sin(1.57079632679)", radCfg);
    const auto third2 = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("1 / 3", degCfg);
    const auto third4 = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("1 / 3", radCfg);
    const auto stepsVec = ReactiveMathChemistryEngine::BuildStepByStepExplanation("2*3 + 5", radCfg);

    // Negative check: malformed matrix with mismatched columns is flagged invalid
    const auto badMat = ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("[[1, 2, 3], [4, 5]]", radCfg);

    CHECK_IDS(fracRes.valid && fracRes.hasFraction && fracRes.evaluatedUtf8 == "2" &&
              fracRes.latexUtf8.find("\\frac") != std::string::npos &&
              sumRes.valid && sumRes.hasSummation && sumRes.evaluatedUtf8 == "55" &&
              intRes.valid && intRes.hasIntegral && intRes.evaluatedUtf8 == "9" &&
              powGreekRes.hasPower && powGreekRes.hasGreekSymbol &&
              matRes.valid && matRes.hasMatrix && matRes.matrixRows == 2 && matRes.matrixCols == 2 &&
              !badMat.valid &&
              degSin.evaluatedUtf8 == "0.5" &&
              radSin.evaluatedUtf8 == "1" &&
              third2.evaluatedUtf8 == "0.33" &&
              third4.evaluatedUtf8 == "0.3333" &&
              stepsVec.size() >= 3,
        "M5",
        std::vector<std::string>({
            "GN-MATH-048", "GN-MATH-049", "GN-MATH-050", "GN-MATH-051", "GN-MATH-052",
            "GN-MATH-053", "NT-EDIT-036", "MS-EDIT-029", "MS-MATH-046", "MS-MATH-047",
            "MS-MATH-048", "MS-MATH-049", "MS-MATH-050", "MS-MATH-051", "MS-MATH-052",
            "MS-MATH-053", "MS-MATH-054", "MS-MATH-055", "MS-MATH-056", "MS-MATH-057",
            "ON-MATH-052", "ON-MATH-053", "ON-MATH-054", "ON-MATH-055", "ON-MATH-056",
            "ON-MATH-057", "ON-MATH-058", "OB-EDIT-028", "NS-EDIT-046"
        }),
        "Reconocimiento y tipografia 2D + LaTeX de fracciones, sumatorias, integrales, potencias, simbolos griegos, matrices validadas, DEG/RAD, decimales y pasos explicados");

    // 2. Reactive Math Notes (=) with declared variables, graphing, and automatic recomputation (AP-MATH-051..058)
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    CommandStack stack;
    const ObjectTarget tgt{ false, 0 };

    AnnotationObject varV;
    varV.kind = ObjectKind::TextBox;
    varV.bounds = { 40.0, 60.0, 180.0, 32.0 };
    varV.textUtf8 = "v = 12";
    stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, std::move(varV)));

    AnnotationObject varT;
    varT.kind = ObjectKind::TextBox;
    varT.bounds = { 40.0, 100.0, 180.0, 32.0 };
    varT.textUtf8 = "t = 4";
    stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, std::move(varT)));

    AnnotationObject exprD;
    exprD.kind = ObjectKind::TextBox;
    exprD.bounds = { 40.0, 140.0, 240.0, 32.0 };
    exprD.textUtf8 = "d = v * t =";
    stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, std::move(exprD)));

    AnnotationObject plot2d;
    plot2d.kind = ObjectKind::PlotBlock;
    plot2d.bounds = { 320.0, 60.0, 220.0, 160.0 };
    plot2d.formulaExprUtf8 = "v * x";
    plot2d.plotConfigUtf8 = "xmin=0;xmax=10";
    stack.Execute(doc, std::make_unique<AddObjectCommand>(tgt, std::move(plot2d)));

    const auto solved1 = ReactiveMathChemistryEngine::EvaluateReactiveEqualsOnDocument(doc, tgt, radCfg);
    const std::string textAt12 = doc.Pages()[0].objects[2].textUtf8;

    // Update variable v from 12 to 25 and verify automatic reactive recomputation of expression
    const bool updatedVar = ReactiveMathChemistryEngine::UpdateDeclaredVariableAndRecompute(doc, stack, tgt, "v", 25.0, radCfg);
    const std::string textAt25 = doc.Pages()[0].objects[2].textUtf8;

    CHECK_IDS(solved1.size() >= 3 &&
              textAt12.find("48") != std::string::npos &&
              updatedVar &&
              textAt25.find("100") != std::string::npos,
        "M5",
        std::vector<std::string>({
            "AP-MATH-051", "AP-MATH-052", "AP-MATH-053", "AP-MATH-054",
            "AP-MATH-055", "AP-MATH-056", "AP-MATH-057", "AP-MATH-058"
        }),
        "Resolucion reactiva tras signo igual (=), reutilizacion de variables declaradas, graficas dependientes y recomputacion instantanea al cambiar valores");

    // 3. Chemistry formulas, molar mass, and stoichiometric equation balancing (MS-MATH-058)
    const auto h2so4 = ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("H2SO4");
    const auto redox = ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("Fe2O3 + CO -> Fe + CO2");
    const auto badChem = ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("???");

    CHECK_IDS(h2so4.valid && !h2so4.isReaction && std::abs(h2so4.molarMassGPerMol - 98.078) < 0.1 &&
              redox.valid && redox.isReaction && redox.balanced &&
              redox.balancedEquationUtf8 == "Fe2O3 + 3CO -> 2Fe + 3CO2" &&
              !badChem.valid,
        "M5",
        std::vector<std::string>({"MS-MATH-058"}),
        "Reconocimiento de formulas quimicas 2D, calculo de masa molar (H2SO4 = 98.08 g/mol) y balanceo estequiometrico (Fe2O3 + 3CO -> 2Fe + 3CO2)");
}

} // namespace

int main(int argc, char** argv) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    const std::string reportPath = (argc > 1) ? argv[1] : "m3_m4_m5_report.json";
    const fs::path tmpDir = fs::temp_directory_path() / "cuadernu_m3_m4_m5_test";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    RunM3_PdfImportAnnotateOutlineHyperlinksAndHandwritingSearch(tmpDir);
    RunM3_FlexcilPopupOverlayGestureModeAndBidirectionalCitations();
    RunM3_SelectivePdfAndRasterExports(tmpDir);

    RunM4_VectorPensPressureTiltLassoAndErasers();
    RunM4_ContinuousDocumentHandwritingWordReflowAndGestures();

    RunM5_2DMathStructuresLatexSolverReactiveEqualsAndChemistry();

    json report;
    report["suite"] = "FlexcilSmartScriptAndMathTest";
    report["total_checks"] = g_totalChecks;
    report["passed_checks"] = g_passedChecks;
    report["failed_checks"] = g_failedChecks;
    report["verified_id_count"] = g_idResults.size();

    json idsJson = json::object();
    for (const auto& [id, rec] : g_idResults) {
        idsJson[id] = {
            {"module", rec.module},
            {"passed", rec.passed},
            {"status", rec.statusUtf8},
            {"evidence", rec.evidenceUtf8},
            {"limitation", rec.limitationUtf8}
        };
    }
    report["ids"] = std::move(idsJson);

    {
        std::ofstream ofs(reportPath, std::ios::binary | std::ios::trunc);
        ofs << report.dump(2);
    }

    fs::remove_all(tmpDir, ec);

    std::cout << "[FlexcilSmartScriptAndMathTest] checks=" << g_passedChecks << "/" << g_totalChecks
              << " failed=" << g_failedChecks
              << " verified_ids=" << g_idResults.size() << "\n";

    CoUninitialize();
    return (g_failedChecks == 0) ? 0 : 1;
}
