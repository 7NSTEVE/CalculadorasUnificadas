#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/document_model.hpp"
#include "../core/ink_engine.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/utf_conv.hpp"
#include "../interop/printout_engine.hpp"
#include "../app/canvas_manipulation.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <roapi.h>
#endif

namespace {

int gPassed = 0;
int gFailed = 0;

void Expect(bool cond, const char* expr, int line) {
    if (cond) {
        ++gPassed;
    } else {
        ++gFailed;
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
    }
}

#define EXPECT(c) Expect((c), #c, __LINE__)

} // namespace

int main(int argc, char** argv) {
    using namespace DocStudio;

    std::filesystem::path fixDir;
    if (argc >= 2) {
        fixDir = std::filesystem::u8path(argv[1]);
    } else {
        fixDir = std::filesystem::temp_directory_path() / L"cuadernu_printout_fixtures";
    }
    std::error_code ec;
    std::filesystem::create_directories(fixDir, ec);

    const std::filesystem::path pdf200Path = fixDir / L"fixture_200p.pdf";
    const std::filesystem::path docxPath   = fixDir / L"fixture.docx";
    const std::filesystem::path pptxPath   = fixDir / L"fixture.pptx";

    // Auto-generate fixtures if not already present in fixDir
    if (!std::filesystem::exists(pdf200Path, ec) ||
        !std::filesystem::exists(docxPath, ec) ||
        !std::filesystem::exists(pptxPath, ec)) {
        const std::filesystem::path scriptPath =
            std::filesystem::path(__FILE__).parent_path() / L"fixtures" / L"make_printout_fixtures.py";
        std::wstring cmd = L"\"\"D:\\DevTools\\Python311\\python.exe\" \"" +
                           scriptPath.wstring() + L"\" --out \"" + fixDir.wstring() + L"\"\"";
        const int pyRc = _wsystem(cmd.c_str());
        EXPECT(pyRc == 0);
    }

    EXPECT(std::filesystem::exists(pdf200Path, ec));
    EXPECT(std::filesystem::exists(docxPath, ec));
    EXPECT(std::filesystem::exists(pptxPath, ec));

    // =========================================================================
    // Test 1 & 2: Insert 200-page PDF into Infinite Canvas (no 6-page limit)
    //             and verify source SHA-256 is untouched (ON-PDF-045, ON-PDF-051)
    // =========================================================================
    const std::string pdfShaBefore = Printout::ComputeFileSha256Hex(pdf200Path.wstring());
    EXPECT(pdfShaBefore.size() == 64);

    DocumentModel canvasDoc;
    canvasDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack canvasStack;

    Printout::Options opts200{};
    opts200.target = Printout::Target::InfiniteCanvas;
    opts200.layout = Printout::CanvasLayout::VerticalColumn;
    opts200.rasterDpi = 144;
    opts200.lockInPlace = true;
    opts200.ocrWhenNoText = true;

    const auto t0 = std::chrono::steady_clock::now();
    Printout::Report rep200{};
    const bool ok200 = Printout::InsertPrintoutFromFile(
        canvasDoc, canvasStack, pdf200Path.wstring(), opts200, 0, { 60.0, 60.0 }, rep200);
    const auto elapsed200Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();

    EXPECT(ok200);
    EXPECT(rep200.totalSourcePages == 200);
    EXPECT(rep200.pagesInserted == 200);
    EXPECT(rep200.sourceSha256Before == pdfShaBefore);
    EXPECT(rep200.sourceSha256After == pdfShaBefore);
    EXPECT(canvasDoc.Mode() == DocumentMode::InfiniteCanvas);
    EXPECT(canvasDoc.CanvasObjects().size() == 200);
    for (const auto& obj : canvasDoc.CanvasObjects()) {
        EXPECT(obj.kind == ObjectKind::PdfCard);
        EXPECT(obj.lockedInPlace);
        EXPECT(!obj.pdfSourcePathUtf8.empty() || !obj.pdfSourceAssetKey.empty() || !obj.assetKey.empty());
        if (!obj.pdfSourceAssetKey.empty()) {
            EXPECT(canvasDoc.FindAsset(obj.pdfSourceAssetKey) != nullptr);
        }
    }
    std::cout << "[INFO] 200-page PDF printout inserted in " << elapsed200Ms << " ms ("
              << rep200.pagesWithText << " pages with text, "
              << rep200.pagesWithOcr << " via OCR)" << std::endl;

    // =========================================================================
    // Test 3: SearchText("Clave-137") on Infinite Canvas (FX-PDF-043, GN-PDF-042, AP-EDIT-042)
    // =========================================================================
    const auto hits137 = canvasDoc.SearchText("Clave-137", false, true);
    EXPECT(hits137.size() == 1);
    if (!hits137.empty()) {
        const auto& m = hits137[0];
        EXPECT(m.onCanvas);
        const auto& card137 = canvasDoc.CanvasObjects()[136]; // 0-based index 136 = page 137
        EXPECT(m.objectId == card137.id);
        EXPECT(m.hasBox);
        EXPECT(!m.worldBox.Empty());
        EXPECT(m.worldBox.x >= card137.bounds.x - 1.0);
        EXPECT(m.worldBox.y >= card137.bounds.y - 1.0);
        EXPECT(m.worldBox.Right() <= card137.bounds.Right() + 1.0);
        EXPECT(m.worldBox.Bottom() <= card137.bounds.Bottom() + 1.0);
    }

    // =========================================================================
    // Test 4: Accent-insensitive search ("ecuacion" -> "ecuación") + FTS index (ON-ORG-011)
    // =========================================================================
    const auto hitsBernoulli = canvasDoc.SearchText("ecuacion de Bernoulli", false, true);
    EXPECT(hitsBernoulli.size() == 199); // all pages except image-only page 150

    VaultFullTextIndex fts;
    fts.IndexDocument("canvas200", canvasDoc);
    const auto ftsHits = fts.Query("Clave-137", 10);
    EXPECT(!ftsHits.empty());
    if (!ftsHits.empty()) {
        EXPECT(ftsHits.front().objectId == canvasDoc.CanvasObjects()[136].id);
        EXPECT(ftsHits.front().sourceKindUtf8 == "printout");
    }

    // =========================================================================
    // Test 5: OCR on image-only page 150 ("ESCANEADO150") (ON-PDF-047, ON-ORG-013)
    // =========================================================================
    EXPECT(rep200.ocrEngineAvailable);
    if (rep200.ocrEngineAvailable) {
        const auto& card150 = canvasDoc.CanvasObjects()[149];
        EXPECT(card150.printoutTextFromOcr);
        EXPECT(!card150.printoutTextUtf8.empty());
        const auto ocrHits = canvasDoc.SearchText("ESCANEADO", false, true);
        EXPECT(!ocrHits.empty());
        if (!ocrHits.empty()) {
            EXPECT(ocrHits.front().onCanvas);
            EXPECT(ocrHits.front().objectId == card150.id);
        }
    }

    // =========================================================================
    // Test 7: Annotate over pinned PdfCard; Lasso & Scribble-Erase ignore pinned card
    //         while selecting/erasing the ink stroke on top (ON-PDF-046, NF-EDIT-034, MS-EDIT-030)
    // =========================================================================
    const DocRect c0Bounds = canvasDoc.CanvasObjects()[0].bounds;
    AnnotationObject strokeOnCard{};
    strokeOnCard.kind = ObjectKind::InkStroke;
    strokeOnCard.layerId = canvasDoc.ActiveLayerId();
    strokeOnCard.strokeWidth = 3.0;
    strokeOnCard.inkPoints = {
        { c0Bounds.x + 40.0, c0Bounds.y + 40.0, 0.7f, 0.0f, 0.0f, 10 },
        { c0Bounds.x + 180.0, c0Bounds.y + 90.0, 0.8f, 0.0f, 0.0f, 25 },
        { c0Bounds.x + 260.0, c0Bounds.y + 120.0, 0.7f, 0.0f, 0.0f, 40 }
    };
    strokeOnCard.RecomputeBounds();
    EXPECT(canvasStack.Execute(canvasDoc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, strokeOnCard)));
    const std::uint64_t inkId = canvasDoc.CanvasObjects().back().id;

    // Lasso polygon enclosing both card 0 and the ink stroke on top of it
    const std::vector<InkPoint> lassoLoop = {
        { c0Bounds.x - 10.0, c0Bounds.y - 10.0, 0.5f, 0, 0, 0 },
        { c0Bounds.Right() + 10.0, c0Bounds.y - 10.0, 0.5f, 0, 0, 0 },
        { c0Bounds.Right() + 10.0, c0Bounds.Bottom() + 10.0, 0.5f, 0, 0, 0 },
        { c0Bounds.x - 10.0, c0Bounds.Bottom() + 10.0, 0.5f, 0, 0, 0 },
        { c0Bounds.x - 10.0, c0Bounds.y - 10.0, 0.5f, 0, 0, 0 }
    };
    const auto lassoIds = InkEngine::SelectObjectsInsideLasso(canvasDoc, ObjectTarget{ true, 0 }, lassoLoop, true);
    EXPECT(lassoIds.size() == 1);
    if (!lassoIds.empty()) {
        EXPECT(lassoIds[0] == inkId);
    }
    const auto scribbleIds = InkEngine::FindObjectsForScribbleErase(canvasDoc, ObjectTarget{ true, 0 }, strokeOnCard.inkPoints, true);
    EXPECT(scribbleIds.size() == 1);
    if (!scribbleIds.empty()) {
        EXPECT(scribbleIds[0] == inkId);
    }
    // Undo the test ink stroke so we have exact 200 cards for persistence & undo tests
    EXPECT(canvasStack.Undo(canvasDoc));
    EXPECT(canvasDoc.CanvasObjects().size() == 200);

    // =========================================================================
    // Test 8: Save 200-card canvas to .cudoc -> reload -> verify persistence & search
    // =========================================================================
    const std::filesystem::path cudoc200Path = fixDir / L"canvas_200p.cudoc";
    std::string saveErr;
    const auto tSave0 = std::chrono::steady_clock::now();
    EXPECT(CudocStorage::SaveToFile(cudoc200Path.wstring(), canvasDoc, &saveErr));
    const auto saveMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - tSave0).count();
    const auto cudocBytes = std::filesystem::file_size(cudoc200Path, ec);
    EXPECT(cudocBytes > 10000);
    std::cout << "[INFO] 200-page canvas .cudoc size = " << (cudocBytes / 1024)
              << " KB (saved in " << saveMs << " ms)" << std::endl;

    DocumentModel reloadedDoc;
    EXPECT(CudocStorage::LoadFromFile(cudoc200Path.wstring(), reloadedDoc, &saveErr));
    EXPECT(reloadedDoc.CanvasObjects().size() == 200);
    EXPECT(reloadedDoc.CanvasObjects()[136].lockedInPlace);
    const auto reloadedHits137 = reloadedDoc.SearchText("Clave-137", false, true);
    EXPECT(reloadedHits137.size() == 1);

    // =========================================================================
    // Test 9: Single-step Undo (Ctrl+Z) removes all 200 cards; Redo (Ctrl+Y) restores them
    // =========================================================================
    EXPECT(canvasStack.Undo(canvasDoc));
    EXPECT(canvasDoc.CanvasObjects().empty());
    EXPECT(canvasStack.Redo(canvasDoc));
    EXPECT(canvasDoc.CanvasObjects().size() == 200);

    // =========================================================================
    // Test 6: Insert 200-page PDF into a 3-page Paginated Notebook after activePageIndex=1
    //         (page 2) -> 203 pages total, new pages at indices 2..201 (ON-PDF-045, NT-PDF-042)
    // =========================================================================
    DocumentModel nbDoc;
    nbDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    for (int extra = 0; extra < 2; ++extra) {
        PageModel p{};
        p.pageId = nbDoc.AllocatePageId();
        p.extractedTextUtf8 = "OriginalPage_" + std::to_string(extra + 2);
        nbDoc.MutablePages().push_back(std::move(p));
    }
    nbDoc.MutablePages()[0].extractedTextUtf8 = "OriginalPage_1";
    nbDoc.RebuildLayout();
    EXPECT(nbDoc.Pages().size() == 3);

    CommandStack nbStack;
    Printout::Options nbOpts{};
    nbOpts.target = Printout::Target::NotebookPages;
    nbOpts.ocrWhenNoText = false; // fast vector page insertion test
    Printout::Report nbRep{};
    EXPECT(Printout::InsertPrintoutFromFile(
        nbDoc, nbStack, pdf200Path.wstring(), nbOpts, 1 /* after page 2 */, { 0, 0 }, nbRep));
    EXPECT(nbDoc.Pages().size() == 203);
    EXPECT(nbDoc.Pages()[0].extractedTextUtf8 == "OriginalPage_1");
    EXPECT(nbDoc.Pages()[1].extractedTextUtf8 == "OriginalPage_2");
    EXPECT(nbDoc.Pages()[2].hasPdfSource);
    EXPECT(nbDoc.Pages()[2].sourcePageIndex0 == 0);
    EXPECT(nbDoc.Pages()[201].hasPdfSource);
    EXPECT(nbDoc.Pages()[201].sourcePageIndex0 == 199);
    EXPECT(nbDoc.Pages()[202].extractedTextUtf8 == "OriginalPage_3");
    const auto nbSearch = nbDoc.SearchText("Clave-137", false, true);
    EXPECT(nbSearch.size() == 1);
    if (!nbSearch.empty()) {
        EXPECT(!nbSearch[0].onCanvas);
        EXPECT(nbSearch[0].pageIndex == 2 + 136);
        EXPECT(nbSearch[0].hasBox);
    }
    EXPECT(nbStack.Undo(nbDoc));
    EXPECT(nbDoc.Pages().size() == 3);
    EXPECT(nbStack.Redo(nbDoc));
    EXPECT(nbDoc.Pages().size() == 203);

    // =========================================================================
    // Test 10: PowerPoint (.pptx) printout via Microsoft PowerPoint COM
    //          (NT-PDF-040, NS-PDF-050, NF-EXT-061)
    // =========================================================================
    const std::string pptxShaBefore = Printout::ComputeFileSha256Hex(pptxPath.wstring());
    DocumentModel pptDoc;
    pptDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack pptStack;
    Printout::Options pptOpts{};
    pptOpts.target = Printout::Target::InfiniteCanvas;
    pptOpts.layout = Printout::CanvasLayout::Grid;
    pptOpts.gridColumns = 3;
    pptOpts.converter = Printout::ConverterChoice::MicrosoftOffice;
    Printout::Report pptRep{};
    const bool pptInserted = Printout::InsertPrintoutFromFile(
        pptDoc, pptStack, pptxPath.wstring(), pptOpts, 0, { 60.0, 60.0 }, pptRep);
    if (!pptInserted) {
        std::cerr << "[PowerPoint] " << pptRep.errorUtf8 << '\n';
    }
    EXPECT(pptInserted);
    EXPECT(pptRep.converterUsedUtf8 == "Microsoft PowerPoint");
    EXPECT(pptRep.visualFidelityExact);
    EXPECT(pptRep.pagesInserted == 5);
    EXPECT(pptRep.sourceSha256Before == pptxShaBefore);
    EXPECT(pptRep.sourceSha256After == pptxShaBefore);
    EXPECT(pptDoc.CanvasObjects().size() == 5);
    if (!pptDoc.CanvasObjects().empty()) {
        // 16:9 landscape aspect ratio preserved
        EXPECT(pptDoc.CanvasObjects()[0].bounds.w > pptDoc.CanvasObjects()[0].bounds.h * 1.5);
    }
    const auto pptHits = pptDoc.SearchText("Diapositiva_Maxwell_04", false, true);
    EXPECT(pptHits.size() == 1);
    if (!pptHits.empty()) {
        EXPECT(pptHits[0].onCanvas);
        EXPECT(pptHits[0].objectId == pptDoc.CanvasObjects()[3].id);
    }

    // =========================================================================
    // Test 11: Word (.docx) printout via Microsoft Word COM
    //          (GN-PDF-045, NF-PDF-043, NS-PDF-051)
    // =========================================================================
    const std::string docxShaBefore = Printout::ComputeFileSha256Hex(docxPath.wstring());
    DocumentModel wordDoc;
    wordDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack wordStack;
    Printout::Options wordOpts{};
    wordOpts.target = Printout::Target::InfiniteCanvas;
    wordOpts.converter = Printout::ConverterChoice::MicrosoftOffice;
    Printout::Report wordRep{};
    EXPECT(Printout::InsertPrintoutFromFile(
        wordDoc, wordStack, docxPath.wstring(), wordOpts, 0, { 60.0, 60.0 }, wordRep));
    EXPECT(wordRep.converterUsedUtf8 == "Microsoft Word");
    EXPECT(wordRep.visualFidelityExact);
    EXPECT(wordRep.pagesInserted == 3);
    EXPECT(wordRep.sourceSha256Before == docxShaBefore);
    EXPECT(wordRep.sourceSha256After == docxShaBefore);
    EXPECT(wordDoc.SearchText("CapituloWordDos", false, true).size() == 1);
    EXPECT(wordDoc.SearchText("Celda_Fourier_42", false, true).size() == 1);

    // =========================================================================
    // Test 12: Fallback converters (LibreOffice headless & Native OpenXML)
    // =========================================================================
    {
        DocumentModel loDoc;
        loDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack loStack;
        Printout::Options loOpts{};
        loOpts.target = Printout::Target::InfiniteCanvas;
        loOpts.converter = Printout::ConverterChoice::LibreOffice;
        Printout::Report loRep{};
        EXPECT(Printout::InsertPrintoutFromFile(
            loDoc, loStack, docxPath.wstring(), loOpts, 0, { 60.0, 60.0 }, loRep));
        EXPECT(loRep.converterUsedUtf8 == "LibreOffice");
        EXPECT(loRep.visualFidelityExact);
        EXPECT(loRep.pagesInserted >= 1);
        EXPECT(!loDoc.SearchText("Celda_Fourier_42", false, true).empty());
    }
    {
        DocumentModel natDoc;
        natDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack natStack;
        Printout::Options natOpts{};
        natOpts.target = Printout::Target::InfiniteCanvas;
        natOpts.converter = Printout::ConverterChoice::NativeEditable;
        Printout::Report natRep{};
        EXPECT(Printout::InsertPrintoutFromFile(
            natDoc, natStack, docxPath.wstring(), natOpts, 0, { 60.0, 60.0 }, natRep));
        EXPECT(natRep.converterUsedUtf8 == "Nativo");
        EXPECT(!natRep.visualFidelityExact);
        EXPECT(!natRep.warningsUtf8.empty());
        EXPECT(natRep.pagesInserted >= 1);
    }

    // =========================================================================
    // Test 13: Corrupt / invalid PDF fails gracefully without mutating document
    // =========================================================================
    {
        const std::filesystem::path badPdf = fixDir / L"corrupt.pdf";
        {
            std::ofstream ofs(badPdf, std::ios::binary);
            ofs << "NOT_A_VALID_PDF_FILE_HEADER";
        }
        DocumentModel cleanDoc;
        cleanDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack cleanStack;
        Printout::Report badRep{};
        EXPECT(!Printout::InsertPrintoutFromFile(
            cleanDoc, cleanStack, badPdf.wstring(), opts200, 0, { 60.0, 60.0 }, badRep));
        EXPECT(!badRep.errorUtf8.empty());
        EXPECT(cleanDoc.CanvasObjects().empty());
    }

    // =========================================================================
    // Test 14: Page range 150-160 inserts exactly 11 sheets (pages 150..160)
    // =========================================================================
    {
        DocumentModel rangeDoc;
        rangeDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack rangeStack;
        Printout::Options rOpts = opts200;
        rOpts.firstPage1 = 150;
        rOpts.lastPage1 = 160;
        Printout::Report rRep{};
        EXPECT(Printout::InsertPrintoutFromFile(
            rangeDoc, rangeStack, pdf200Path.wstring(), rOpts, 0, { 60.0, 60.0 }, rRep));
        EXPECT(rRep.pagesInserted == 11);
        EXPECT(rangeDoc.CanvasObjects().size() == 11);
        if (rangeDoc.CanvasObjects().size() == 11) {
            EXPECT(rangeDoc.CanvasObjects().front().pdfSourcePageIndex0 == 149);
            EXPECT(rangeDoc.CanvasObjects().back().pdfSourcePageIndex0 == 159);
        }
    }

    // =========================================================================
    // Test 15: Legacy .cudoc compatibility (object without new printout fields)
    // =========================================================================
    {
        DocumentModel legDoc;
        legDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject legacyCard{};
        legacyCard.kind = ObjectKind::PdfCard;
        legacyCard.bounds = { 50.0, 50.0, 400.0, 560.0 };
        legacyCard.captionUtf8 = "Legacy PDF Card";
        legDoc.AddCanvasObject(legacyCard);
        std::vector<std::uint8_t> zipBytes;
        EXPECT(CudocStorage::SerializeToBytes(legDoc, zipBytes));
        DocumentModel legOut;
        EXPECT(CudocStorage::DeserializeFromBytes(zipBytes.data(), zipBytes.size(), legOut));
        EXPECT(legOut.CanvasObjects().size() == 1);
        if (!legOut.CanvasObjects().empty()) {
            EXPECT(!legOut.CanvasObjects()[0].lockedInPlace);
            EXPECT(legOut.CanvasObjects()[0].printoutTextUtf8.empty());
        }
    }

    // =========================================================================
    // Test 16: 1,600-page full-book Infinite Canvas scalability, Quadtree > 1e6 pt & search
    // =========================================================================
    {
        DocumentModel book1600Doc;
        book1600Doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack book1600Stack;
        Printout::Options bOpts = opts200;
        bOpts.ocrWhenNoText = false; // Pure vector text extraction speed for 1,600 pages

        const auto tStart1600 = std::chrono::steady_clock::now();
        double nextY = 60.0;
        for (int batch = 0; batch < 8; ++batch) {
            Printout::Report bRep{};
            EXPECT(Printout::InsertPrintoutFromFile(
                book1600Doc, book1600Stack, pdf200Path.wstring(), bOpts, 0, { 60.0, nextY }, bRep));
            EXPECT(bRep.pagesInserted == 200);
            if (!book1600Doc.CanvasObjects().empty()) {
                nextY = book1600Doc.CanvasObjects().back().bounds.Bottom() + 32.0;
            }
        }
        const auto insert1600Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - tStart1600).count();

        EXPECT(book1600Doc.CanvasObjects().size() == 1600);
        const auto& lastCard = book1600Doc.CanvasObjects().back();
        EXPECT(lastCard.bounds.y > 1100000.0); // Exceeds old 1e6 Quadtree boundary

        // Verify Quadtree spatial query at page 1,600 (y > 1,100,000 pt)
        const auto visAt1600 = book1600Doc.QueryCanvasRect(lastCard.bounds);
        bool foundLastInQuadtree = false;
        for (std::uint64_t vid : visAt1600) {
            if (vid == lastCard.id) { foundLastInQuadtree = true; break; }
        }
        EXPECT(foundLastInQuadtree);

        const auto tSearch0 = std::chrono::steady_clock::now();
        const auto hits1600 = book1600Doc.SearchText("Clave-200", false, true);
        const auto search1600Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - tSearch0).count();

        EXPECT(hits1600.size() == 8);
        if (!hits1600.empty()) {
            EXPECT(hits1600.back().objectId == lastCard.id);
            EXPECT(hits1600.back().worldBox.y > 1100000.0);
        }
        std::cout << "[INFO] 1,600-page Infinite Canvas printout inserted in " << insert1600Ms
                  << " ms; search across 1,600 sheets completed in " << search1600Ms << " ms." << std::endl;
    }

    // =========================================================================
    // Test 17: Multi-format Canvas Printout (PDF + DOCX + PPTX + Excel CSV)
    // on the SAME Infinite Canvas with custom pageGapPt = 140.0 pt
    // =========================================================================
    {
        const std::filesystem::path csvPath = fixDir / L"fixture_excel.csv";
        {
            std::ofstream ofs(csvPath, std::ios::binary | std::ios::trunc);
            ofs << "Experimento,Frecuencia_Hz,Amplitud_V,Comentario\r\n"
                << "EXP-EXCEL-01,60.0,120.5,ResonanciaFourier\r\n"
                << "EXP-EXCEL-02,120.0,240.0,ArmonicoSegundo\r\n";
        }
        DocumentModel multiDoc;
        multiDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack multiStack;

        Printout::Options mOpts{};
        mOpts.target = Printout::Target::InfiniteCanvas;
        mOpts.layout = Printout::CanvasLayout::VerticalColumn;
        mOpts.firstPage1 = 1;
        mOpts.lastPage1 = 3;
        mOpts.pageGapPt = 140.0; // Custom 140 pt spacing between sheets!
        mOpts.colGapPt = 80.0;
        mOpts.lockInPlace = true;
        mOpts.ocrWhenNoText = false;

        Printout::Report rPdf{}, rDocx{}, rPptx{}, rExcel{};
        EXPECT(Printout::InsertPrintoutFromFile(multiDoc, multiStack, pdf200Path.wstring(), mOpts, 0, { 60.0, 60.0 }, rPdf));
        EXPECT(rPdf.pagesInserted == 3);
        // Verify custom 140 pt vertical gap between page 1 and page 2
        if (multiDoc.CanvasObjects().size() >= 2) {
            const double actualGap = multiDoc.CanvasObjects()[1].bounds.y - multiDoc.CanvasObjects()[0].bounds.Bottom();
            EXPECT(std::abs(actualGap - 140.0) < 0.5);
        }

        mOpts.firstPage1 = 1;
        mOpts.lastPage1 = 0; // All pages
        EXPECT(Printout::InsertPrintoutFromFile(multiDoc, multiStack, docxPath.wstring(), mOpts, 0, { 680.0, 60.0 }, rDocx));
        EXPECT(Printout::InsertPrintoutFromFile(multiDoc, multiStack, pptxPath.wstring(), mOpts, 0, { 1300.0, 60.0 }, rPptx));
        EXPECT(Printout::InsertPrintoutFromFile(multiDoc, multiStack, csvPath.wstring(),  mOpts, 0, { 2020.0, 60.0 }, rExcel));
        EXPECT(rExcel.pagesInserted >= 1);

        // Search across all 4 formats on the SAME Infinite Canvas
        EXPECT(!multiDoc.SearchText("Clave-002", false, true).empty());              // From PDF
        EXPECT(!multiDoc.SearchText("Celda_Fourier_42", false, true).empty());       // From Word DOCX
        EXPECT(!multiDoc.SearchText("Diapositiva_Maxwell_04", false, true).empty()); // From PowerPoint PPTX
        EXPECT(!multiDoc.SearchText("ResonanciaFourier", false, true).empty());      // From Excel/CSV
    }

    // =========================================================================
    // Test 18: Region Snip (Circular/Lasso Polygon Mask & Rectangular Cutout)
    //          copied as Image and pasted onto another canvas/page location
    // =========================================================================
    {
        RasterImageBgra32 snipImg;
        snipImg.width = 160;
        snipImg.height = 120;
        snipImg.pixels.resize(static_cast<std::size_t>(160 * 120 * 4), 255u);

        const DocRect worldRect{ 100.0, 200.0, 160.0, 120.0 };
        std::vector<DocPoint> circlePoly;
        for (int k = 0; k < 32; ++k) {
            const double a = (2.0 * 3.14159265358979323846 * k) / 32.0;
            circlePoly.push_back({ 180.0 + 55.0 * std::cos(a), 260.0 + 45.0 * std::sin(a) });
        }
        CanvasManip::ApplyPolygonMaskToBgra32(snipImg, worldRect, circlePoly);

        // Center pixel (80, 60) must be opaque inside the circle; corner pixel (0, 0) must be transparent (alpha=0)
        const std::uint8_t centerAlpha = snipImg.pixels[(60u * 160u + 80u) * 4u + 3u];
        const std::uint8_t cornerAlpha = snipImg.pixels[3u];
        EXPECT(centerAlpha == 255u);
        EXPECT(cornerAlpha == 0u);

        std::vector<std::uint8_t> snipPng;
        EXPECT(WinRtPdfRenderer::EncodeBgra32ToPngBytes(snipImg, snipPng));
        EXPECT(!snipPng.empty());

        DocumentModel snipDoc;
        snipDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack snipStack;
        AnnotationObject pastedImg{};
        pastedImg.kind = ObjectKind::Image;
        pastedImg.bounds = { 800.0, 120.0, 160.0, 120.0 };
        pastedImg.pngBytes = std::move(snipPng);
        pastedImg.imagePixelW = snipImg.width;
        pastedImg.imagePixelH = snipImg.height;
        pastedImg.captionUtf8 = "Recorte Lazo/Circular";
        EXPECT(snipStack.Execute(snipDoc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, std::move(pastedImg))));
        EXPECT(snipDoc.CanvasObjects().size() == 1);
        EXPECT(snipDoc.CanvasObjects()[0].kind == ObjectKind::Image);
        EXPECT(!snipDoc.CanvasObjects()[0].pngBytes.empty());
    }

    WinRtPdfRenderer::ClearOpenDocumentCache();
    std::cout << "PrintoutTest: " << gPassed << " passed, " << gFailed << " failed." << std::endl;
    std::fflush(stdout);
    std::fflush(stderr);
    return (gFailed == 0) ? 0 : 1;
}
