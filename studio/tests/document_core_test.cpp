#include "../core/audio_timeline.hpp"
#include "../core/camera.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/ink_engine.hpp"
#include "../core/lru_byte_cache.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/page_layout.hpp"
#include "../core/spatial_index.hpp"
#include "../core/utf_conv.hpp"
#include "../core/zip_container.hpp"
#include "../bridge/calc_bridge.hpp"
#include "../bridge/lab_report_exporter.hpp"
#include "../circuit/mna_solver.hpp"
#include "../math/engineering_cas.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_signer.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/winrt_adapters.hpp"
#include "../render/card_render_common.hpp"
#include "../render/d2d_renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace DocStudio;

namespace {

int gFailures = 0;

int gPasses = 0;

void Check(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
        ++gFailures;
    } else {
        ++gPasses;
    }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

struct ScopedTempDir {
    fs::path dir;
    bool keep = false;
    explicit ScopedTempDir(const fs::path& overrideDir = {}) {
        if (!overrideDir.empty()) {
            dir = overrideDir;
            keep = true;
        } else {
            dir = fs::temp_directory_path() / ("docstudio_test_" + std::to_string(GetCurrentProcessId()));
        }
        fs::create_directories(dir);
    }
    ~ScopedTempDir() {
        if (keep) return;
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

std::vector<std::uint8_t> MakeSolidPng(int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    RasterImageBgra32 img;
    img.width = w;
    img.height = h;
    img.pixels.resize(static_cast<std::size_t>(w) * h * 4u);
    for (int i = 0; i < w * h; ++i) {
        img.pixels[i * 4 + 0] = b;
        img.pixels[i * 4 + 1] = g;
        img.pixels[i * 4 + 2] = r;
        img.pixels[i * 4 + 3] = 255;
    }
    std::vector<std::uint8_t> png;
    WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, png);
    return png;
}

void TestF1CudocAndCommandStack(const fs::path& tmpDir) {
    std::cout << "-- Test F1: .cudoc Round-trip, Corruption Rejection, Transactional Save, CommandStack --\n";
    DocumentModel doc;
    CommandStack stack;
    doc.SetTitle("Cuaderno C\xC3\xA1lculo CASIO");

    AnnotationObject ink;
    ink.kind = ObjectKind::InkStroke;
    ink.strokeColor = DocColor::RGBA(26, 115, 232, 255);
    ink.strokeWidth = 3.2;
    ink.inkPoints = { {50.0, 60.0, 0.25f}, {95.0, 120.0, 0.85f}, {160.0, 140.0, 0.55f} };
    CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, ink)));

    AnnotationObject tb;
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 80.0, 200.0, 240.0, 48.0 };
    tb.textUtf8 = "Ecuaci\xC3\xB3n cuadr\xC3\xA1tica x\xC2\xB2 - 5x + 6 = 0";
    tb.fontSizePt = 13.0;
    CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, tb)));

    AnnotationObject imgObj;
    imgObj.kind = ObjectKind::Image;
    imgObj.bounds = { 100.0, 280.0, 64.0, 48.0 };
    imgObj.pngBytes = MakeSolidPng(32, 24, 24, 180, 90);
    CHECK(!imgObj.pngBytes.empty());
    CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, imgObj)));

    PageModel page2;
    page2.background = PaperPattern::Grid;
    CHECK(stack.Execute(doc, std::make_unique<InsertPageCommand>(1, page2)));
    CHECK(doc.Pages().size() == 2);

    // Undo / Redo check
    CHECK(stack.Undo(doc));
    CHECK(doc.Pages().size() == 1);
    CHECK(stack.Redo(doc));
    CHECK(doc.Pages().size() == 2);
    CHECK(stack.Execute(doc, std::make_unique<RotatePageCommand>(1, 90)));
    CHECK(doc.Pages()[1].rotationDeg == 90);
    CHECK(stack.Execute(doc, std::make_unique<CropPageCommand>(1, NormalizedCropRect{ 0.1, 0.1, 0.9, 0.8 })));

    // Guard deleting down to 0 pages must fail
    CHECK(stack.Execute(doc, std::make_unique<DeletePageCommand>(1)));
    CHECK(doc.Pages().size() == 1);
    CHECK(!stack.Execute(doc, std::make_unique<DeletePageCommand>(0)));
    CHECK(stack.Undo(doc)); // restores page 1
    CHECK(doc.Pages().size() == 2);

    // Atomic transactional save + round-trip load
    const fs::path cudocPath = tmpDir / L"proyecto_prueba.cudoc";
    std::string err;
    CHECK(CudocStorage::SaveToFile(cudocPath.wstring(), doc, &err));
    CHECK(!doc.IsDirty());

    // Simulate failed overwrite mid-transaction: original must survive intact!
    doc.SetTitle("TituloNoGuardado");
    CHECK(!CudocStorage::SaveToFile(cudocPath.wstring(), doc, &err, true));

    DocumentModel loaded;
    CHECK(CudocStorage::LoadFromFile(cudocPath.wstring(), loaded, &err));
    CHECK(loaded.Title() == "Cuaderno C\xC3\xA1lculo CASIO");
    CHECK(loaded.Pages().size() == 2);
    CHECK(loaded.Pages()[0].objects.size() == 3);
    CHECK(loaded.Pages()[0].objects[0].inkPoints.size() == 3);
    CHECK(std::abs(loaded.Pages()[0].objects[0].inkPoints[1].pressure - 0.85f) < 1e-4f);
    CHECK(loaded.Pages()[0].objects[1].textUtf8.find("cuadr\xC3\xA1tica") != std::string::npos);
    CHECK(loaded.Pages()[0].objects[2].pngBytes == imgObj.pngBytes);
    CHECK(loaded.Pages()[1].rotationDeg == 90);
    CHECK(std::abs(loaded.Pages()[1].crop.left - 0.1) < 1e-4);

    // Corrupt .cudoc rejection without mutating valid target
    DocumentModel guardDoc;
    guardDoc.SetTitle("NoTocar");
    const std::vector<std::uint8_t> junk = { 'N', 'O', 'T', 'A', 'Z', 'I', 'P' };
    CHECK(!CudocStorage::DeserializeFromBytes(junk.data(), junk.size(), guardDoc, &err));
    CHECK(guardDoc.Title() == "NoTocar");

    // Corrupt primary file + .bak recovery check
    {
        DocumentModel v2;
        CHECK(CudocStorage::LoadFromFile(cudocPath.wstring(), v2, &err));
        v2.SetTitle("SegundaVersion");
        CHECK(CudocStorage::SaveToFile(cudocPath.wstring(), v2, &err)); // creates .bak with loaded v1
        HANDLE hf = CreateFileW(cudocPath.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(hf != INVALID_HANDLE_VALUE);
        DWORD wr = 0;
        WriteFile(hf, junk.data(), static_cast<DWORD>(junk.size()), &wr, nullptr);
        CloseHandle(hf);

        bool usedBak = false;
        DocumentModel recovered;
        CHECK(CudocStorage::LoadFromFileWithRecovery(cudocPath.wstring(), recovered, &usedBak, &err));
        CHECK(usedBak);
        CHECK(recovered.Title() == "Cuaderno C\xC3\xA1lculo CASIO");
    }
}

void TestF2LayoutCameraAndLruCache() {
    std::cout << "-- Test F2: Virtualized 1,200-Page Layout, Zoom-at-Anchor, 256MiB LRU Cache --\n";
    std::vector<PageModel> many(1200);
    for (std::size_t i = 0; i < many.size(); ++i) {
        many[i].pageId = i + 1;
        many[i].widthPt = (i % 5 == 0) ? 842.0 : 595.0;
        many[i].heightPt = (i % 5 == 0) ? 595.0 : 842.0;
    }
    PageLayout layout;
    layout.Rebuild(many);
    CHECK(layout.PageCount() == 1200);
    CHECK(layout.TotalHeight() > 900000.0);

    std::size_t first = 0, last = 0;
    const auto* midPl = layout.PlacementAt(650);
    CHECK(midPl != nullptr);
    CHECK(layout.ComputeVisibleRange(midPl->topWorld + 10.0, midPl->topWorld + 900.0, first, last, 1));
    CHECK(first <= 650 && last >= 650 && (last - first) <= 6);

    std::size_t hitIdx = 0;
    double lx = 0, ly = 0;
    CHECK(layout.HitTestWorldPoint(0.0, midPl->topWorld + 123.5, hitIdx, lx, ly));
    CHECK(hitIdx == 650);
    CHECK(std::abs(ly - 123.5) < 1e-6);

    // Zoom-at-anchor invariant
    Camera2D cam;
    cam.panX = -120.0;
    cam.panY = 3450.0;
    cam.zoom = 1.25;
    const DocPoint beforeW = cam.ScreenToWorld(640.0, 410.0);
    cam.ZoomAtScreenAnchor(640.0, 410.0, 2.85);
    const DocPoint afterW = cam.ScreenToWorld(640.0, 410.0);
    CHECK(std::abs(beforeW.x - afterW.x) < 1e-6);
    CHECK(std::abs(beforeW.y - afterW.y) < 1e-6);

    // LRU Byte-budget enforcement
    LruByteCache<int, std::vector<std::uint8_t>> lru(1024u * 1024u);
    CHECK(lru.Put(1, std::vector<std::uint8_t>(400u * 1024u, 1), 400u * 1024u));
    CHECK(lru.Put(2, std::vector<std::uint8_t>(400u * 1024u, 2), 400u * 1024u));
    std::vector<std::uint8_t> tmp;
    CHECK(lru.TryGet(1, tmp)); // touch key 1 so key 2 becomes LRU
    CHECK(lru.Put(3, std::vector<std::uint8_t>(400u * 1024u, 3), 400u * 1024u));
    CHECK(lru.BytesUsed() <= 1024u * 1024u);
    CHECK(lru.EvictionCount() == 1);
    CHECK(lru.TryGet(1, tmp));
    CHECK(!lru.TryGet(2, tmp));
    CHECK(!lru.Put(99, std::vector<std::uint8_t>(2u * 1024u * 1024u, 9), 2u * 1024u * 1024u)); // oversize rejected cleanly
}

void TestF3D2DRenderAndMultiPressureInk() {
    std::cout << "-- Test F3: Direct2D Offscreen Render (Ink, Highlighter, Shape, Grid, TextBox) --\n";
    DocumentModel doc;
    doc.MutablePages()[0].background = PaperPattern::Grid;

    AnnotationObject pen;
    pen.kind = ObjectKind::InkStroke;
    pen.strokeColor = DocColor::RGBA(26, 115, 232, 255);
    pen.strokeWidth = 6.0;
    pen.inkPoints = { {60.0, 120.0, 0.2f}, {220.0, 120.0, 0.9f}, {420.0, 180.0, 0.7f} };
    doc.AddPageObject(0, std::move(pen));

    AnnotationObject textBox;
    textBox.kind = ObjectKind::TextBox;
    textBox.bounds = { 80.0, 220.0, 420.0, 80.0 };
    textBox.textUtf8 = "CASIO 2026 DOCUMENT STUDIO";
    textBox.fontSizePt = 28.0;
    textBox.bold = true;
    textBox.strokeColor = DocColor::RGBA(10, 10, 10, 255);
    doc.AddPageObject(0, std::move(textBox));

    D2DRenderer renderer;
    RasterImageBgra32 img;
    CHECK(renderer.RenderPageOffscreen(doc, 0, 800, nullptr, img));
    CHECK(img.Valid());

    std::size_t bluePixels = 0;
    std::size_t darkTextPixels = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(img.width) * img.height; ++i) {
        const std::uint8_t b = img.pixels[i * 4 + 0];
        const std::uint8_t g = img.pixels[i * 4 + 1];
        const std::uint8_t r = img.pixels[i * 4 + 2];
        if (b > 170 && r < 80) ++bluePixels;
        if (r < 60 && g < 60 && b < 60) ++darkTextPixels;
    }
    std::cout << "  D2D Offscreen pixels -> blue_ink=" << bluePixels << ", dark_text=" << darkTextPixels << "\n";
    CHECK(bluePixels > 500);
    CHECK(darkTextPixels > 400);
}

void TestF4RealCasioPdfEditMergeSplitFlatten(const fs::path& manualsDir, const fs::path& tmpDir) {
    std::cout << "-- Test F4: Real CASIO PDF Split, Rotate, Crop, Flatten Ink+Image+Stamp, Merge --\n";
    const fs::path srcPdf = manualsDir / L"fx-570CW_991CW_ES.pdf";
    CHECK(fs::exists(srcPdf));
    if (!fs::exists(srcPdf)) return;

    PdfDocument parser;
    std::string err;
    CHECK(parser.LoadFromFile(srcPdf.wstring(), &err));
    std::cout << "  fx-570CW_991CW_ES.pdf version=" << parser.PdfVersion() << " pages=" << parser.PageCount() << "\n";
    CHECK(parser.PageCount() >= 5);
    if (parser.PageCount() < 5) return;

    const fs::path splitPath = tmpDir / L"cw_split2.pdf";
    CHECK(PdfWriter::SplitPdfFile(srcPdf.wstring(), { 0, 2 }, splitPath.wstring(), &err));

    PdfDocument splitParsed;
    CHECK(splitParsed.LoadFromFile(splitPath.wstring(), &err));
    CHECK(splitParsed.PageCount() == 2);
    if (splitParsed.PageCount() != 2) return;

    DocumentModel editDoc;
    CHECK(splitParsed.PopulateDocumentModel(editDoc, WideToUtf8(splitPath.wstring()), &err));

    // Rotate page 0 clockwise 90 deg, crop page 1, add bright red ink & PNG image onto page 0
    CommandStack stack;
    CHECK(stack.Execute(editDoc, std::make_unique<RotatePageCommand>(0, 90)));
    CHECK(stack.Execute(editDoc, std::make_unique<CropPageCommand>(1, NormalizedCropRect{ 0.1, 0.1, 0.9, 0.9 })));

    AnnotationObject redInk;
    redInk.kind = ObjectKind::InkStroke;
    redInk.strokeColor = DocColor::RGBA(230, 25, 25, 255);
    redInk.strokeWidth = 10.0;
    redInk.inkPoints = { {60.0, 60.0, 0.9f}, {260.0, 60.0, 0.9f}, {260.0, 140.0, 0.9f} };
    CHECK(stack.Execute(editDoc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, redInk)));

    AnnotationObject imgAnnot;
    imgAnnot.kind = ObjectKind::Image;
    imgAnnot.bounds = { 300.0, 60.0, 80.0, 60.0 };
    imgAnnot.pngBytes = MakeSolidPng(24, 24, 15, 190, 40); // green patch
    CHECK(stack.Execute(editDoc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, imgAnnot)));

    const fs::path flattenedPdf = tmpDir / L"cw_flattened.pdf";
    CHECK(PdfWriter::ExportDocumentToFile(editDoc, flattenedPdf.wstring(), &err));

    // Independent verify: load exported PDF via both PdfDocument AND WinRt Windows.Data.Pdf
    PdfDocument reParsed;
    CHECK(reParsed.LoadFromFile(flattenedPdf.wstring(), &err));
    CHECK(reParsed.PageCount() == 2);
    if (reParsed.PageCount() >= 2) {
        CHECK(((reParsed.PageAt(0).rotationDeg % 360) + 360) % 360 == 90);
        const auto& p1Box = reParsed.PageAt(1).hasCropBox ? reParsed.PageAt(1).cropBox : reParsed.PageAt(1).mediaBox;
        CHECK(p1Box.w < reParsed.PageAt(1).mediaBox.w * 0.86);
    }

    RasterImageBgra32 winrtRaster;
    CHECK(WinRtPdfRenderer::RenderPageToBgra32(flattenedPdf.wstring(), 0, 1000, 0, NormalizedCropRect{}, winrtRaster, nullptr, &err));
    CHECK(winrtRaster.Valid());
    CHECK(winrtRaster.width > winrtRaster.height); // page 0 rotated 90 deg portrait -> landscape!

    std::size_t redInkPixels = 0;
    std::size_t greenImgPixels = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(winrtRaster.width) * winrtRaster.height; ++i) {
        const std::uint8_t b = winrtRaster.pixels[i * 4 + 0];
        const std::uint8_t g = winrtRaster.pixels[i * 4 + 1];
        const std::uint8_t r = winrtRaster.pixels[i * 4 + 2];
        if (r > 180 && g < 80 && b < 80) ++redInkPixels;
        if (g > 150 && r < 60 && b < 80) ++greenImgPixels;
    }
    std::cout << "  Windows.Data.Pdf flattened check -> rotated=" << winrtRaster.width << "x" << winrtRaster.height
              << ", red_ink_px=" << redInkPixels << ", green_xobj_px=" << greenImgPixels << "\n";
    CHECK(redInkPixels > 1200);
    CHECK(greenImgPixels > 800);

    // Merge split + flattened -> 4 pages
    const fs::path mergedPdf = tmpDir / L"cw_merged.pdf";
    CHECK(PdfWriter::MergePdfFiles({ splitPath.wstring(), flattenedPdf.wstring() }, mergedPdf.wstring(), &err));
    PdfDocument mergedParsed;
    CHECK(mergedParsed.LoadFromFile(mergedPdf.wstring(), &err));
    CHECK(mergedParsed.PageCount() == 4);

    // Verify ALL real CASIO PDFs in manualsDir load cleanly (including Standard R=2/3/4 encrypted manuals)
    int manualPdfCount = 0;
    if (fs::exists(manualsDir)) {
        for (const auto& entry : fs::directory_iterator(manualsDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != L".pdf" && entry.path().extension() != L".PDF") continue;
            ++manualPdfCount;
            PdfDocument mDoc;
            std::string mErr;
            const bool ok = mDoc.LoadFromFile(entry.path().wstring(), &mErr);
            std::cout << "  Manual PDF: " << WideToUtf8(entry.path().filename().wstring())
                      << " ok=" << ok << " pages=" << mDoc.PageCount();
            if (!ok) std::cout << " err=" << mErr;
            std::cout << "\n";
            CHECK(ok);
            CHECK(mDoc.PageCount() > 0);
        }
    }
    CHECK(manualPdfCount >= 16);

    for (int p = 0; p < 6; ++p) {
        RasterImageBgra32 pgImg;
        std::string pgErr;
        const bool pOk = WinRtPdfRenderer::RenderPageToBgra32(srcPdf.wstring(), p, 640, 0, NormalizedCropRect{}, pgImg, nullptr, &pgErr);
        std::size_t nonWhite = 0;
        if (pOk && pgImg.Valid()) {
            for (std::size_t k = 0; k < static_cast<std::size_t>(pgImg.width) * pgImg.height; ++k) {
                if (pgImg.pixels[k * 4 + 0] < 245 || pgImg.pixels[k * 4 + 1] < 245 || pgImg.pixels[k * 4 + 2] < 245) ++nonWhite;
            }
        }
        std::cout << "  WinRT Render fx-570CW p=" << p << " ok=" << pOk << " valid=" << pgImg.Valid()
                  << " size=" << pgImg.width << "x" << pgImg.height << " nonWhite=" << nonWhite
                  << " err=\"" << pgErr << "\"\n";
        CHECK(pOk && pgImg.Valid());
        CHECK(nonWhite > 100);
    }
}

void TestF5TextExtractionReplacementAndOcr(const fs::path& manualsDir, const fs::path& tmpDir) {
    std::cout << "-- Test F5: PDF Text Extraction, Ligatures, Honest Replacement Check, Offline OCR --\n";
    CHECK(NormalizeLigaturesUtf8("Con\xEF\xAC\x81gura \xEF\xAC\x82ujo") == "Configura flujo");

    // Generate clean vector PDF with known Helvetica text string
    DocumentModel txtDoc;
    AnnotationObject tb;
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 60.0, 80.0, 420.0, 40.0 };
    tb.fontSizePt = 18.0;
    tb.textUtf8 = "Casio FX-991CW Manual";
    txtDoc.AddPageObject(0, tb);

    std::vector<std::uint8_t> genBytes;
    std::string err;
    CHECK(PdfWriter::ExportDocumentToBytes(txtDoc, genBytes, &err));

    PdfDocument parsedGen;
    CHECK(parsedGen.LoadFromBytes(genBytes, &err));
    CHECK(parsedGen.PageCount() == 1);
    std::cout << "  Extracted generated PDF text: \"" << parsedGen.PageAt(0).extractedTextUtf8 << "\"\n";
    CHECK(parsedGen.PageAt(0).extractedTextUtf8.find("Casio FX-991CW Manual") != std::string::npos);

    const auto capDirect = parsedGen.InspectTextReplacement(0, "Casio FX-991CW Manual");
    CHECK(capDirect.supportedDirectStream);

    std::vector<std::uint8_t> replacedBytes;
    CHECK(PdfWriter::TryDirectReplaceTextInPdfBytes(genBytes, 0, "Casio FX-991CW Manual", "Casio FX-CG50 Guia", replacedBytes, &err));
    PdfDocument reParsed;
    CHECK(reParsed.LoadFromBytes(replacedBytes, &err));
    CHECK(reParsed.PageAt(0).extractedTextUtf8.find("Casio FX-CG50 Guia") != std::string::npos);

    // Check real CASIO manual extraction / honest unsupported stream check
    const fs::path casioPdf = manualsDir / L"fx-570CW_991CW_ES.pdf";
    PdfDocument casioDoc;
    if (casioDoc.LoadFromFile(casioPdf.wstring(), &err)) {
        std::size_t nonEmptyTextPages = 0;
        for (std::size_t i = 0; i < (std::min<std::size_t>)(casioDoc.PageCount(), 8u); ++i) {
            if (!casioDoc.PageAt(i).extractedTextUtf8.empty()) ++nonEmptyTextPages;
        }
        std::cout << "  fx-570CW_991CW_ES.pdf pages with extracted text in first 8 pages: " << nonEmptyTextPages << "\n";
        const auto honestCap = casioDoc.InspectTextReplacement(0, "ImpossibleNonMatchingSubsetGlyphXYZ");
        CHECK(!honestCap.supportedDirectStream);
        CHECK(honestCap.supportedVisualRedaction);
    }

    // Offline Windows.Media.Ocr test on crisp high-contrast DirectWrite render
    const auto langs = WinRtOcrAdapter::ListAvailableLanguageTags();
    std::cout << "  Windows.Media.Ocr installed languages: " << langs.size() << "\n";
    if (!langs.empty()) {
        DocumentModel ocrDoc;
        AnnotationObject headline;
        headline.kind = ObjectKind::TextBox;
        headline.bounds = { 60.0, 120.0, 480.0, 80.0 };
        headline.textUtf8 = "CALCULADORA\nCASIO 2026";
        headline.fontSizePt = 36.0;
        headline.bold = true;
        headline.strokeColor = DocColor::RGBA(0, 0, 0, 255);
        ocrDoc.AddPageObject(0, headline);

        D2DRenderer d2d;
        RasterImageBgra32 img;
        CHECK(d2d.RenderPageOffscreen(ocrDoc, 0, 1200, nullptr, img));
        const OcrOutput ocr = WinRtOcrAdapter::RecognizeBgra32(img.width, img.height, img.pixels.data());
        std::cout << "  OCR output (" << ocr.languageTagUtf8 << "): \"" << ocr.fullTextUtf8 << "\"\n";
        CHECK(ocr.engineAvailable);
        CHECK(ocr.fullTextUtf8.find("CASIO") != std::string::npos || ocr.fullTextUtf8.find("CALCULADORA") != std::string::npos);
    }
    (void)tmpDir;
}

void TestF6InfiniteCanvas100kQuadtree(const fs::path& tmpDir) {
    std::cout << "-- Test F6: Infinite Canvas Quadtree 100,000 Objects Benchmark & Export --\n";
    DocumentModel canvasDoc;
    canvasDoc.ResetToBlank(DocumentMode::InfiniteCanvas);

    // 100,000 objects arranged over [-50,000 .. +50,000]
    for (int i = 0; i < 100000; ++i) {
        AnnotationObject obj;
        obj.id = static_cast<std::uint64_t>(i + 1);
        obj.zOrder = obj.id;
        obj.kind = ObjectKind::Shape;
        obj.shapeKind = ShapeKind::Rectangle;
        const int gx = i % 316;
        const int gy = i / 316;
        obj.bounds = { -45000.0 + gx * 280.0, -45000.0 + gy * 280.0, 40.0, 30.0 };
        obj.strokeColor = DocColor::RGBA(37, 99, 235, 255);
        canvasDoc.AddCanvasObject(std::move(obj));
    }
    CHECK(canvasDoc.CanvasQuadtreeSize() == 100000);

    const DocRect query{ -500.0, -500.0, 1400.0, 1400.0 };
    std::vector<std::uint64_t> brute;
    for (const auto& o : canvasDoc.CanvasObjects()) {
        if (o.bounds.Intersects(query)) brute.push_back(o.id);
    }
    std::sort(brute.begin(), brute.end());

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<std::uint64_t> quadRes;
    for (int iter = 0; iter < 100; ++iter) {
        quadRes = canvasDoc.QueryCanvasRect(query);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double usPerQuery = std::chrono::duration<double, std::micro>(t1 - t0).count() / 100.0;
    std::sort(quadRes.begin(), quadRes.end());
    std::cout << "  100k Quadtree query returned " << quadRes.size() << " matches (brute=" << brute.size()
              << "), avg=" << usPerQuery << " us/query\n";
    CHECK(quadRes == brute);

    // Region export to PDF
    const fs::path canvasPdf = tmpDir / L"canvas_region.pdf";
    CanvasPdfExportOptions opts;
    opts.paginated = false;
    opts.hasCustomRegion = true;
    opts.customRegionWorld = query;
    std::string err;
    CHECK(PdfWriter::ExportInfiniteCanvasToFile(canvasDoc, opts, canvasPdf.wstring(), &err));

    RasterImageBgra32 img;
    CHECK(WinRtPdfRenderer::RenderPageToBgra32(canvasPdf.wstring(), 0, 800, 0, NormalizedCropRect{}, img, nullptr, &err));
    CHECK(img.Valid());
}

void TestF7Pkcs7SignatureAndOpenXml(const fs::path& tmpDir) {
    std::cout << "-- Test F7: PKCS#7 SHA-256 PDF Signature + Tamper Rejection + DOCX/PPTX Round-trip --\n";
    DocumentModel doc;
    doc.SetTitle("Documento Oficial CASIO \xC3\x91");
    AnnotationObject tb;
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 60.0, 60.0, 360.0, 80.0 };
    tb.textUtf8 = "An\xC3\xA1lisis num\xC3\xA9rico con calculadora fx-991CW";
    doc.AddPageObject(0, tb);

    AnnotationObject img;
    img.kind = ObjectKind::Image;
    img.bounds = { 60.0, 160.0, 120.0, 80.0 };
    img.pngBytes = MakeSolidPng(64, 48, 37, 99, 235);
    doc.AddPageObject(0, img);

    const fs::path signedPdf = tmpDir / L"signed_test.pdf";
    PdfSignatureOptions sopts;
    sopts.signerNameUtf8 = "Ing. Prueba";
    sopts.reasonUtf8 = "Validacion Cryptografica";
    sopts.addVisualStamp = true;
    std::string err;
    CHECK(PdfSigner::ExportAndSignDocument(doc, sopts, signedPdf.wstring(), L"", L"", &err));

    const auto vGood = PdfSigner::VerifyPdfFile(signedPdf.wstring());
    std::cout << "  Signature Verify -> hasSig=" << vGood.hasSignature << " byteRangeOk=" << vGood.byteRangeValid
              << " cryptoValid=" << vGood.cryptographicValid << " subj=\"" << vGood.signerSubjectUtf8 << "\"\n";
    CHECK(vGood.hasSignature);
    CHECK(vGood.byteRangeValid);
    CHECK(vGood.cryptographicValid);

    // Tamper test: modify 1 byte near header after PDF signature sealed
    std::vector<std::uint8_t> tampered;
    {
        HANDLE hf = CreateFileW(signedPdf.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(hf != INVALID_HANDLE_VALUE);
        LARGE_INTEGER li{};
        GetFileSizeEx(hf, &li);
        tampered.resize(static_cast<std::size_t>(li.QuadPart));
        DWORD rd = 0;
        ReadFile(hf, tampered.data(), static_cast<DWORD>(tampered.size()), &rd, nullptr);
        CloseHandle(hf);
    }
    const std::string asStr(reinterpret_cast<const char*>(tampered.data()), tampered.size());
    const std::size_t objPos = asStr.find("endobj");
    CHECK(objPos != std::string::npos && objPos > 10);
    tampered[objPos - 4] ^= 0x01u;
    const auto vTampered = PdfSigner::VerifyPdfBytes(tampered);
    CHECK(vTampered.hasSignature);
    CHECK(!vTampered.cryptographicValid); // MUST reject tampered PDF!

    // Bidirectional OpenXML .docx & .pptx round-trip
    const fs::path docxPath = tmpDir / L"exportado.docx";
    const fs::path pptxPath = tmpDir / L"exportado.pptx";
    std::vector<std::vector<std::uint8_t>> snaps = { MakeSolidPng(120, 160, 200, 220, 240) };
    CHECK(OpenXmlInterop::ExportToDocxFile(doc, docxPath.wstring(), snaps, &err));
    CHECK(OpenXmlInterop::ExportToPptxFile(doc, pptxPath.wstring(), snaps, &err));

    DocumentModel fromDocx, fromPptx;
    CHECK(OpenXmlInterop::ImportFromDocxFile(docxPath.wstring(), fromDocx, &err));
    CHECK(OpenXmlInterop::ImportFromPptxFile(pptxPath.wstring(), fromPptx, &err));
    CHECK(!fromDocx.Pages().empty());
    CHECK(fromDocx.Pages()[0].extractedTextUtf8.find("fx-991CW") != std::string::npos);
    CHECK(!fromPptx.Pages().empty());
    CHECK(fromPptx.Pages()[0].extractedTextUtf8.find("fx-991CW") != std::string::npos);
}

void TestF8P0AuditRegressionFixes(const fs::path& manualsDir, const fs::path& tmpDir) {
    std::cout << "-- Test F8: P0 Audit Regression Fixes (Quadtree Bounds, Unicode Search/PDF, ZIP Budget/Path, Embedded PDF, >200 Pages, Catalog) --\n";
    auto checkF8 = [](bool cond, const char* expr, int line) {
        if (!cond) {
            std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
            ++gFailures;
        }
    };
#define F8_CHECK(expr) checkF8(static_cast<bool>(expr), #expr, __LINE__)

    // 1. Quadtree dynamic expansion for extreme coordinates outside initial bounds
    {
        DocStudio::DocumentModel extremeDoc;
        extremeDoc.ResetToBlank(DocStudio::DocumentMode::InfiniteCanvas);

        DocStudio::AnnotationObject farNeg;
        farNeg.zOrder = 1;
        farNeg.kind = DocStudio::ObjectKind::Shape;
        farNeg.shapeKind = DocStudio::ShapeKind::Rectangle;
        farNeg.bounds = { -5.0e7, -5.0e7, 200.0, 120.0 };
        extremeDoc.AddCanvasObject(farNeg);
        const std::uint64_t negId = extremeDoc.CanvasObjects().back().id;

        DocStudio::AnnotationObject farPos;
        farPos.zOrder = 2;
        farPos.kind = DocStudio::ObjectKind::Shape;
        farPos.shapeKind = DocStudio::ShapeKind::Rectangle;
        farPos.bounds = { 8.0e7, 8.0e7, 200.0, 120.0 };
        extremeDoc.AddCanvasObject(farPos);
        const std::uint64_t posId = extremeDoc.CanvasObjects().back().id;

        const auto negHits = extremeDoc.QueryCanvasRect({ -5.0e7 - 100.0, -5.0e7 - 100.0, 500.0, 500.0 });
        const auto posHits = extremeDoc.QueryCanvasRect({ 8.0e7 - 100.0, 8.0e7 - 100.0, 500.0, 500.0 });
        F8_CHECK(std::find(negHits.begin(), negHits.end(), negId) != negHits.end());
        F8_CHECK(std::find(posHits.begin(), posHits.end(), posId) != posHits.end());
    }

    // 2. Unicode case-folding & accent-insensitive SearchText
    {
        DocStudio::DocumentModel searchDoc;
        DocStudio::AnnotationObject tb;
        tb.kind = DocStudio::ObjectKind::TextBox;
        tb.bounds = { 40.0, 40.0, 500.0, 80.0 };
        tb.textUtf8 = "C\xC3\xA1lculo de \xC3\x81NGULO \xCE\xA3\xCE\xB8 y \xC3\xB1""and\xC3\xBA";
        searchDoc.AddPageObject(0, tb);

        F8_CHECK(!searchDoc.SearchText("\xC3\xA1ngulo", false, false).empty());
        F8_CHECK(!searchDoc.SearchText("\xCF\x83\xCE\xB8", false, false).empty());
        F8_CHECK(!searchDoc.SearchText("calculo", false, true).empty());
        F8_CHECK(!searchDoc.SearchText("nandu", false, true).empty());
        F8_CHECK(searchDoc.SearchText("CALCULO", true, false).empty());
    }

    // 3. ZIP traversal rejection & cumulative 512 MiB budget enforcement
    {
        DocStudio::ZipArchive za;
        za.SetTextEntry("safe_dir/entry123.txt", "hello");
        std::vector<std::uint8_t> zipBytes;
        std::string zErr;
        F8_CHECK(za.Serialize(zipBytes, false, &zErr));
        // Replace "safe_dir/entry123.txt" (21 chars) -> "../../../../evil.txt" + wait exact same len: "safe_dir/../evil.txt" (20) -> "../a_evil_entry12.txt" (21 chars)
        const std::string origName = "safe_dir/entry123.txt";
        const std::string evilName = "../a_evil_entry12.txt";
        bool patched = false;
        for (std::size_t i = 0; i + origName.size() <= zipBytes.size(); ++i) {
            if (std::memcmp(zipBytes.data() + i, origName.data(), origName.size()) == 0) {
                std::memcpy(zipBytes.data() + i, evilName.data(), evilName.size());
                patched = true;
            }
        }
        F8_CHECK(patched);
        DocStudio::ZipArchive badZa;
        F8_CHECK(!badZa.Deserialize(zipBytes.data(), zipBytes.size(), &zErr));
    }
    {
        DocStudio::ZipArchive bombZa;
        std::vector<std::uint8_t> zeros(64ull * 1024ull * 1024ull, 0u);
        for (int i = 0; i < 9; ++i) {
            bombZa.SetEntry("z_" + std::to_string(i) + ".bin", zeros); // 9 * 64 MiB = 576 MiB > 512 MiB
        }
        zeros.clear();
        zeros.shrink_to_fit();
        std::vector<std::uint8_t> bombZip;
        std::string bErr;
        F8_CHECK(bombZa.Serialize(bombZip, true, &bErr));
        bombZa = DocStudio::ZipArchive{};
        DocStudio::ZipArchive parsedBomb;
        F8_CHECK(!parsedBomb.Deserialize(bombZip.data(), bombZip.size(), &bErr));
    }

    // 4. Unicode math symbols preserved in exported PDF & rendered with dark pixels
    {
        DocStudio::DocumentModel mathDoc;
        DocStudio::AnnotationObject tb;
        tb.kind = DocStudio::ObjectKind::TextBox;
        tb.bounds = { 50.0, 100.0, 480.0, 80.0 };
        tb.fontSizePt = 18.0;
        tb.strokeColor = DocColor::RGBA(0, 0, 0, 255);
        tb.textUtf8 = "Int \xE2\x88\xAB \xE2\x88\x91 \xCE\xB8 \xE2\x88\x9A \xCE\xA9 \xCF\x80 \xE2\x89\xA5 \xE2\x89\xA4 \xE2\x89\xA0 \xE2\x84\x9D";
        mathDoc.AddPageObject(0, tb);

        const fs::path mathPdf = tmpDir / "f8_unicode_math.pdf";
        std::string err;
        F8_CHECK(DocStudio::PdfWriter::ExportDocumentToFile(mathDoc, mathPdf.wstring(), &err));

        DocStudio::PdfDocument reParsed;
        F8_CHECK(reParsed.LoadFromFile(mathPdf.wstring(), &err));
        F8_CHECK(reParsed.PageCount() == 1);
        if (reParsed.PageCount() >= 1) {
            const std::string& txt = reParsed.PageAt(0).extractedTextUtf8;
            F8_CHECK(txt.find('?') == std::string::npos);
            F8_CHECK(txt.find("\xE2\x88\xAB") != std::string::npos);
            F8_CHECK(txt.find("\xCE\xB8") != std::string::npos);
            F8_CHECK(txt.find("\xE2\x88\x9A") != std::string::npos);
            F8_CHECK(txt.find("\xCF\x80") != std::string::npos);
        }

        DocStudio::RasterImageBgra32 img;
        F8_CHECK(DocStudio::WinRtPdfRenderer::RenderPageToBgra32(
            mathPdf.wstring(), 0, 900, 0, DocStudio::NormalizedCropRect{}, img, nullptr, &err));
        std::size_t darkPx = 0;
        if (img.Valid()) {
            for (std::size_t i = 0; i < static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height); ++i) {
                if (img.pixels[i * 4 + 0] < 120 && img.pixels[i * 4 + 1] < 120 && img.pixels[i * 4 + 2] < 120) ++darkPx;
            }
        }
        F8_CHECK(darkPx > 200);
    }

    // 5. Embedded source PDF survives source deletion in .cudoc + honest failure when source is missing
    const fs::path laPdf = manualsDir / "fx-570_991LA_X_ES.pdf";
    if (fs::exists(laPdf)) {
        const fs::path tempSrcPdf = tmpDir / "temp_la_source.pdf";
        fs::copy_file(laPdf, tempSrcPdf, fs::copy_options::overwrite_existing);

        DocStudio::PdfDocument srcParsed;
        std::string err;
        F8_CHECK(srcParsed.LoadFromFile(tempSrcPdf.wstring(), &err));
        const std::size_t origPages = srcParsed.PageCount();
        DocStudio::DocumentModel model;
        F8_CHECK(srcParsed.PopulateDocumentModel(model, tempSrcPdf.string(), &err));

        std::vector<std::uint8_t> cudocZip;
        F8_CHECK(DocStudio::CudocStorage::SerializeToBytes(model, cudocZip, &err));

        // Delete original source PDF before deserializing & exporting
        fs::remove(tempSrcPdf);
        F8_CHECK(!fs::exists(tempSrcPdf));

        DocStudio::DocumentModel restored;
        F8_CHECK(DocStudio::CudocStorage::DeserializeFromBytes(cudocZip.data(), cudocZip.size(), restored, &err));
        const fs::path recoveredPdf = tmpDir / "f8_recovered_from_cudoc.pdf";
        F8_CHECK(DocStudio::PdfWriter::ExportDocumentToFile(restored, recoveredPdf.wstring(), &err));

        DocStudio::PdfDocument recParsed;
        F8_CHECK(recParsed.LoadFromFile(recoveredPdf.wstring(), &err));
        F8_CHECK(recParsed.PageCount() == origPages);
    }
    {
        DocStudio::DocumentModel missingSrcDoc;
        DocStudio::PageModel pm;
        pm.pageId = 999;
        pm.hasPdfSource = true;
        pm.sourcePdfPathUtf8 = (tmpDir / "definitely_missing_12345.pdf").string();
        pm.sourcePageIndex0 = 0;
        pm.widthPt = 595.0;
        pm.heightPt = 842.0;
        DocStudio::CommandStack tmpStack;
        F8_CHECK(tmpStack.Execute(missingSrcDoc, std::make_unique<DocStudio::InsertPageCommand>(1, pm)));
        F8_CHECK(tmpStack.Execute(missingSrcDoc, std::make_unique<DocStudio::DeletePageCommand>(0)));
        const fs::path outFailPdf = tmpDir / "f8_should_fail.pdf";
        std::string err;
        F8_CHECK(!DocStudio::PdfWriter::ExportDocumentToFile(missingSrcDoc, outFailPdf.wstring(), &err));
    }

    // 6. Canvas PdfCard real content export + >200 paginated canvas export
    if (fs::exists(laPdf)) {
        DocStudio::DocumentModel cardDoc;
        cardDoc.ResetToBlank(DocStudio::DocumentMode::InfiniteCanvas);
        DocStudio::AnnotationObject card;
        card.kind = DocStudio::ObjectKind::PdfCard;
        card.pdfSourcePathUtf8 = laPdf.string();
        card.pdfSourcePageIndex0 = 0;
        card.bounds = { 50.0, 50.0, 320.0, 440.0 };
        cardDoc.AddCanvasObject(card);

        const fs::path cardPdf = tmpDir / "f8_canvas_pdfcard.pdf";
        DocStudio::CanvasPdfExportOptions copts;
        copts.paginated = false;
        copts.hasCustomRegion = true;
        copts.customRegionWorld = { 0.0, 0.0, 420.0, 540.0 };
        std::string err;
        F8_CHECK(DocStudio::PdfWriter::ExportInfiniteCanvasToFile(cardDoc, copts, cardPdf.wstring(), &err));

        DocStudio::RasterImageBgra32 img;
        F8_CHECK(DocStudio::WinRtPdfRenderer::RenderPageToBgra32(
            cardPdf.wstring(), 0, 800, 0, DocStudio::NormalizedCropRect{}, img, nullptr, &err));
        std::size_t nonWhite = 0;
        if (img.Valid()) {
            for (std::size_t i = 0; i < static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height); ++i) {
                if (img.pixels[i * 4 + 0] < 235 || img.pixels[i * 4 + 1] < 235 || img.pixels[i * 4 + 2] < 235) ++nonWhite;
            }
        }
        F8_CHECK(nonWhite > 2000);
    }
    {
        DocStudio::DocumentModel bigCanvas;
        bigCanvas.ResetToBlank(DocStudio::DocumentMode::InfiniteCanvas);
        for (int i = 0; i < 205; ++i) {
            DocStudio::AnnotationObject sh;
            sh.kind = DocStudio::ObjectKind::Shape;
            sh.shapeKind = DocStudio::ShapeKind::Rectangle;
            sh.strokeColor = DocStudio::DocColor::RGBA(37, 99, 235, 255);
            sh.bounds = { i * 620.0 + 40.0, 40.0, 80.0, 40.0 };
            bigCanvas.AddCanvasObject(sh);
        }
        const fs::path bigPdf = tmpDir / "f8_canvas_205p.pdf";
        DocStudio::CanvasPdfExportOptions pOpts;
        pOpts.paginated = true;
        pOpts.pageWidthPt = 595.0;
        pOpts.pageHeightPt = 842.0;
        pOpts.maxPages = 0;
        std::string err;
        F8_CHECK(DocStudio::PdfWriter::ExportInfiniteCanvasToFile(bigCanvas, pOpts, bigPdf.wstring(), &err));

        DocStudio::PdfDocument bigParsed;
        F8_CHECK(bigParsed.LoadFromFile(bigPdf.wstring(), &err));
        F8_CHECK(bigParsed.PageCount() >= 205);
    }

    // 7. Catalog preservation during TryDirectReplaceTextInPdfBytes (/Lang + /ViewerPreferences)
    {
        std::string rawPdf = "%PDF-1.4\n";
        std::vector<std::size_t> offs(6, 0);
        auto addObj = [&](int num, const std::string& body) {
            offs[static_cast<std::size_t>(num)] = rawPdf.size();
            rawPdf += std::to_string(num) + " 0 obj\n" + body + "\nendobj\n";
        };
        addObj(1, "<< /Type /Catalog /Pages 2 0 R /Lang (es-CR) >>");
        addObj(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
        addObj(3, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>");
        addObj(4, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
        const std::string stream = "BT /F1 16 Tf 50 750 Td (Alpha Beta Target) Tj ET\n";
        addObj(5, "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "endstream");
        const std::size_t xrefPos = rawPdf.size();
        rawPdf += "xref\n0 6\n0000000000 65535 f \n";
        for (int i = 1; i <= 5; ++i) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", offs[static_cast<std::size_t>(i)]);
            rawPdf += buf;
        }
        rawPdf += "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n" + std::to_string(xrefPos) + "\n%%EOF\n";

        const std::vector<std::uint8_t> pdfBytes(rawPdf.begin(), rawPdf.end());
        std::vector<std::uint8_t> outBytes;
        std::string err;
        F8_CHECK(DocStudio::PdfWriter::TryDirectReplaceTextInPdfBytes(
            pdfBytes, 0, "Alpha Beta Target", "Gamma Delta Done", outBytes, &err));
        const std::string outStr(outBytes.begin(), outBytes.end());
        F8_CHECK(outStr.find("/Lang") != std::string::npos);
        F8_CHECK(outStr.find("es-CR") != std::string::npos);
        DocStudio::PdfDocument outParsed;
        F8_CHECK(outParsed.LoadFromBytes(outBytes, &err));
        F8_CHECK(outParsed.CatalogRef().obj > 0);
        F8_CHECK(outParsed.PageAt(0).extractedTextUtf8.find("Gamma Delta Done") != std::string::npos);
    }
#undef F8_CHECK
}

void TestF9P1P2GoodnotesOneNoteParity(const fs::path& tmpDir) {
    std::cout << "-- Test F9: Ink Smoothing/Snap/Scribble/Lasso/Anchors, Layers, StudyTape, Vault/WAL/FTS/SM-2, Audio Sync --\n";
    auto checkF9 = [](bool cond, const char* expr, int line) {
        if (!cond) {
            std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
            ++gFailures;
        }
    };
#define F9_CHECK(expr) checkF9(static_cast<bool>(expr), #expr, __LINE__)

    // 1. Ink Smoothing (<0.30 vs >=0.30 Catmull-Rom) & Tilt metrics
    {
        std::vector<DocStudio::InkPoint> raw;
        for (int i = 0; i < 8; ++i) {
            raw.push_back({ i * 15.0, (i % 2 == 0 ? 40.0 : 46.0), 0.7f, 20.0f, 10.0f, static_cast<std::uint64_t>(i * 20) });
        }
        const auto fastPts = DocStudio::InkEngine::SmoothInkStroke(raw, 0.15);
        const auto smoothPts = DocStudio::InkEngine::SmoothInkStroke(raw, 0.75);
        F9_CHECK(fastPts.size() == raw.size());
        F9_CHECK(smoothPts.size() > raw.size());
        DocStudio::AnnotationObject dummy;
        dummy.penStyle = DocStudio::InkPenStyle::Pencil;
        dummy.strokeWidth = 3.0;
        F9_CHECK(DocStudio::InkEngine::ComputeEffectiveWidth(raw[0], dummy, 200.0) >= 3.0);
        F9_CHECK(DocStudio::InkEngine::ComputeTiltAlphaMultiplier(raw[0], DocStudio::InkPenStyle::Pencil) < 1.0);
    }

    // 2. Hold-to-Snap Shapes: Line, Arrow, Ellipse, Rectangle
    {
        std::vector<DocStudio::InkPoint> linePts;
        for (int i = 0; i <= 10; ++i) linePts.push_back({ 20.0 + i * 20.0, 50.0 + (i % 2) * 1.5, 0.6f });
        const auto sLine = DocStudio::InkEngine::RecognizeHeldShape(linePts);
        F9_CHECK(sLine.recognized && sLine.kind == DocStudio::ShapeKind::Line);

        std::vector<DocStudio::InkPoint> arrowPts;
        for (int i = 0; i <= 14; ++i) arrowPts.push_back({ 20.0 + i * 10.0, 100.0, 0.6f });
        arrowPts.push_back({ 140.0, 86.0, 0.6f });
        arrowPts.push_back({ 160.0, 100.0, 0.6f });
        arrowPts.push_back({ 140.0, 114.0, 0.6f });
        const auto sArrow = DocStudio::InkEngine::RecognizeHeldShape(arrowPts);
        F9_CHECK(sArrow.recognized && sArrow.kind == DocStudio::ShapeKind::Arrow);

        std::vector<DocStudio::InkPoint> circlePts;
        for (int i = 0; i <= 24; ++i) {
            const double th = (static_cast<double>(i) / 24.0) * 6.283185307;
            circlePts.push_back({ 200.0 + 50.0 * std::cos(th), 200.0 + 50.0 * std::sin(th), 0.7f });
        }
        const auto sCircle = DocStudio::InkEngine::RecognizeHeldShape(circlePts);
        F9_CHECK(sCircle.recognized && sCircle.kind == DocStudio::ShapeKind::Ellipse);

        std::vector<DocStudio::InkPoint> rectPts;
        for (int i = 0; i < 6; ++i) rectPts.push_back({ 50.0 + i * 20.0, 50.0, 0.7f });
        for (int i = 0; i < 6; ++i) rectPts.push_back({ 150.0, 50.0 + i * 20.0, 0.7f });
        for (int i = 0; i < 6; ++i) rectPts.push_back({ 150.0 - i * 20.0, 150.0, 0.7f });
        for (int i = 0; i <= 6; ++i) rectPts.push_back({ 50.0, 150.0 - i * 20.0, 0.7f });
        const auto sRect = DocStudio::InkEngine::RecognizeHeldShape(rectPts);
        F9_CHECK(sRect.recognized && sRect.kind == DocStudio::ShapeKind::Rectangle);
    }

    // 3. Scribble-to-Erase & Circle-to-Lasso & Anchored Ink
    {
        DocStudio::DocumentModel d;
        d.ResetToBlank(DocStudio::DocumentMode::InfiniteCanvas);

        DocStudio::AnnotationObject s1;
        s1.kind = DocStudio::ObjectKind::InkStroke;
        s1.inkPoints = { { 100.0, 100.0, 0.8f }, { 180.0, 100.0, 0.8f } };
        d.AddCanvasObject(s1);
        const std::uint64_t s1Id = d.CanvasObjects().back().id;

        DocStudio::AnnotationObject parentTb;
        parentTb.kind = DocStudio::ObjectKind::TextBox;
        parentTb.bounds = { 320.0, 320.0, 140.0, 50.0 };
        parentTb.textUtf8 = "Ancla padre";
        d.AddCanvasObject(parentTb);
        const std::uint64_t parentId = d.CanvasObjects().back().id;

        DocStudio::AnnotationObject anchoredChild;
        anchoredChild.kind = DocStudio::ObjectKind::InkStroke;
        anchoredChild.anchorObjectId = parentId;
        anchoredChild.inkPoints = { { 330.0, 390.0, 0.8f }, { 420.0, 390.0, 0.8f } };
        d.AddCanvasObject(anchoredChild);
        const std::uint64_t childId = d.CanvasObjects().back().id;

        std::vector<DocStudio::InkPoint> scribble;
        for (int i = 0; i < 10; ++i) {
            scribble.push_back({ 105.0 + i * 7.0, (i % 2 == 0) ? 80.0 : 120.0, 0.7f });
        }
        F9_CHECK(DocStudio::InkEngine::IsScribbleEraseGesture(scribble));
        const auto eraseIds = DocStudio::InkEngine::FindObjectsForScribbleErase(d, { true, 0 }, scribble);
        F9_CHECK(std::find(eraseIds.begin(), eraseIds.end(), s1Id) != eraseIds.end());
        F9_CHECK(std::find(eraseIds.begin(), eraseIds.end(), parentId) == eraseIds.end());

        std::vector<DocStudio::InkPoint> lasso;
        for (int i = 0; i <= 24; ++i) {
            const double th = (static_cast<double>(i) / 24.0) * 6.283185307;
            lasso.push_back({ 390.0 + 130.0 * std::cos(th), 355.0 + 110.0 * std::sin(th), 0.5f });
        }
        F9_CHECK(DocStudio::InkEngine::IsClosedLassoLoop(lasso));
        const auto lassoIds = DocStudio::InkEngine::SelectObjectsInsideLasso(d, { true, 0 }, lasso);
        F9_CHECK(std::find(lassoIds.begin(), lassoIds.end(), parentId) != lassoIds.end());
        F9_CHECK(std::find(lassoIds.begin(), lassoIds.end(), childId) != lassoIds.end());

        DocStudio::InkEngine::UpdateAnchoredChildrenAfterMove(d, { true, 0 }, parentId, 120.0, -45.0);
        const auto* movedChild = d.FindCanvasObject(childId);
        F9_CHECK(movedChild != nullptr && std::abs(movedChild->inkPoints[0].x - 450.0) < 1e-6);
        F9_CHECK(movedChild != nullptr && std::abs(movedChild->inkPoints[0].y - 345.0) < 1e-6);
    }

    // 4. Layers + Non-Exportable Layer + StudyTape + Links + Collapse + .cudoc round-trip + Audio Timeline
    {
        DocStudio::DocumentModel doc;
        const std::uint64_t hiddenExportLayer = doc.AddLayer("Solo Pantalla");
        F9_CHECK(doc.SetLayerProperties(hiddenExportLayer, true, false, false, 0.85));

        DocStudio::AnnotationObject draftNote;
        draftNote.kind = DocStudio::ObjectKind::TextBox;
        draftNote.layerId = hiddenExportLayer;
        draftNote.bounds = { 50.0, 50.0, 260.0, 40.0 };
        draftNote.textUtf8 = "SECRETO_NO_EXPORTAR";
        doc.AddPageObject(0, draftNote);

        DocStudio::AnnotationObject tape;
        tape.kind = DocStudio::ObjectKind::StudyTape;
        tape.bounds = { 50.0, 120.0, 280.0, 45.0 };
        tape.captionUtf8 = "Ley de Ohm";
        tape.textUtf8 = "V = I * R";
        tape.tapeRevealed = false;
        doc.AddPageObject(0, tape);

        DocStudio::InkLink link;
        link.sourceDocPathUtf8 = "manuales/fx-570_991LA_X_ES.pdf";
        link.sourcePageIndex0 = 4;
        link.targetIsCanvas = true;
        link.labelUtf8 = "Referencia S<=>D";
        doc.AddInkLink(link);
        doc.ToggleCollapsedRange(2, 4);
        F9_CHECK(doc.IsPageCollapsed(3));

        DocStudio::AudioTimelineSession audio;
        audio.StartRecording(16000);
        audio.AppendSyntheticTone(440.0, 400);
        audio.StopRecording();
        F9_CHECK(audio.DurationMs() >= 390 && audio.DurationMs() <= 410);
        const auto envBins = audio.ComputeEnvelopeBins(16);
        F9_CHECK(envBins.size() == 16 && envBins[4] > 0.05f);
        std::vector<std::uint8_t> wavBytes;
        F9_CHECK(audio.ExportWavBytes(wavBytes));
        DocStudio::AudioTimelineSession decoded;
        F9_CHECK(decoded.LoadFromWavBytes(wavBytes.data(), wavBytes.size()));
        F9_CHECK(decoded.DurationMs() == audio.DurationMs());

        doc.SetNamedAsset("rec_01.wav", std::move(wavBytes));

        DocStudio::AnnotationObject timedStroke;
        timedStroke.kind = DocStudio::ObjectKind::InkStroke;
        timedStroke.inkPoints = {
            { 60.0, 220.0, 0.8f, 0.0f, 0.0f, 100ull },
            { 140.0, 220.0, 0.8f, 0.0f, 0.0f, 200ull },
            { 220.0, 220.0, 0.8f, 0.0f, 0.0f, 350ull }
        };
        timedStroke.inkStartTimeMs = 100;
        timedStroke.inkEndTimeMs = 350;
        doc.AddPageObject(0, timedStroke);
        F9_CHECK(DocStudio::InkEngine::FilterInkPointsAtReplayTime(timedStroke, 210ull).size() == 2);

        const fs::path cudocPath = tmpDir / "f9_notebook.cudoc";
        std::string err;
        F9_CHECK(DocStudio::CudocStorage::SaveToFile(cudocPath.wstring(), doc, &err));

        DocStudio::DocumentModel reloaded;
        F9_CHECK(DocStudio::CudocStorage::LoadFromFile(cudocPath.wstring(), reloaded, &err));
        F9_CHECK(reloaded.Layers().size() == 2);
        F9_CHECK(!reloaded.IsLayerExportable(hiddenExportLayer));
        F9_CHECK(reloaded.InkLinks().size() == 1);
        F9_CHECK(reloaded.IsPageCollapsed(3));
        F9_CHECK(reloaded.FindAsset("rec_01.wav") != nullptr);

        // Non-exportable layer must NOT appear in exported PDF text
        std::vector<std::uint8_t> pdfOut;
        F9_CHECK(DocStudio::PdfWriter::ExportDocumentToBytes(reloaded, pdfOut, &err));
        DocStudio::PdfDocument pdfParsed;
        F9_CHECK(pdfParsed.LoadFromBytes(pdfOut, &err));
        F9_CHECK(pdfParsed.PageAt(0).extractedTextUtf8.find("SECRETO_NO_EXPORTAR") == std::string::npos);
        F9_CHECK(pdfParsed.PageAt(0).extractedTextUtf8.find("Ley de Ohm") != std::string::npos);

        // NotebookVault + WAL recovery + VaultFullTextIndex + StudySet SM-2
        DocStudio::NotebookVault vault;
        const auto nbId = vault.CreateNotebook("Circuitos II");
        const auto secId = vault.CreateSection(nbId, "Fasores");
        const auto pgId = vault.AddPageEntry(nbId, secId, "Impedancia", "notas/imp.cudoc");
        F9_CHECK(nbId > 0 && secId > 0 && pgId > 0);
        const fs::path vaultPath = tmpDir / "vault.json";
        F9_CHECK(vault.SaveToFile(vaultPath.wstring(), &err));
        DocStudio::NotebookVault vaultLoad;
        F9_CHECK(vaultLoad.LoadFromFile(vaultPath.wstring(), &err));
        F9_CHECK(vaultLoad.Notebooks().size() == 1 && vaultLoad.Notebooks()[0].sections[0].pages.size() == 1);

        const fs::path walPath = tmpDir / "f9_notebook.cudoc-wal";
        F9_CHECK(DocStudio::CudocWal::AppendRecord(walPath.wstring(), { "set_title", R"({"title":"Recuperado WAL"})" }, &err));
        F9_CHECK(DocStudio::CudocWal::AppendRecord(walPath.wstring(), { "add_obj", R"({"isCanvas":false,"pageIndex":0,"kind":2,"text":"Fasor impedancia complejo","x":60,"y":280,"w":220,"h":40})" }, &err));
        {
            // Simulate torn/incomplete tail bytes appended on crash
            std::ofstream ofs(walPath, std::ios::binary | std::ios::app);
            const char junk[8] = { 'C', 'W', 'A', 'L', 0x50, 0x00, 0x00, 0x00 };
            ofs.write(junk, sizeof(junk));
        }
        DocStudio::DocumentModel walDoc;
        std::size_t replayed = 0;
        F9_CHECK(DocStudio::CudocWal::RecoverDocument(cudocPath.wstring(), walPath.wstring(), walDoc, &replayed, &err));
        F9_CHECK(replayed == 2 && walDoc.Title() == "Recuperado WAL");

        DocStudio::VaultFullTextIndex fts;
        fts.IndexDocument("notas/imp.cudoc", walDoc);
        const auto ftsHits = fts.Query("fasor impedancia");
        F9_CHECK(!ftsHits.empty());

        auto deck = DocStudio::StudySetManager::BuildDeckFromDocument(reloaded);
        F9_CHECK(!deck.cards.empty());
        DocStudio::StudySetManager::ReviewCardSm2(deck.cards[0], 5, 1000000ull);
        F9_CHECK(deck.cards[0].repetitions == 1 && deck.cards[0].intervalDays == 1);
        DocStudio::StudySetManager::ReviewCardSm2(deck.cards[0], 5, 1086400ull);
        F9_CHECK(deck.cards[0].repetitions == 2 && deck.cards[0].intervalDays == 6);
    }
#undef F9_CHECK
}

void TestF10P3StemEngineeringSuperiority(const fs::path& tmpDir) {
    std::cout << "-- Test F10: #varsub Dimensional CAS, Phasors, StepChecker, Plots/CSV, MNA AC/DC/OpAmp/Bode, K-Map, Casio Card --\n";
    auto checkF10 = [](bool cond, const char* expr, int line) {
        if (!cond) {
            std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
            ++gFailures;
        }
    };
#define F10_CHECK(expr) checkF10(static_cast<bool>(expr), #expr, __LINE__)

    // 1. Strict dimensional SI/imperial analysis & unit conversion
    {
        const auto ohm = DocStudio::Cas::EvaluateExpression("V = (4.7 kOhm) * (2 mA) | mV");
        F10_CHECK(ohm.ok && ohm.hasAssignment && ohm.assignedVarUtf8 == "V");
        F10_CHECK(std::abs(ohm.result.value.real() - 9.4) < 1e-9);
        F10_CHECK(ohm.formattedResultUtf8.find("9400") != std::string::npos && ohm.formattedResultUtf8.find("mV") != std::string::npos);

        const auto speed = DocStudio::Cas::EvaluateExpression("100 km/h | m/s");
        F10_CHECK(speed.ok && std::abs(speed.result.value.real() - (100000.0 / 3600.0)) < 1e-6);

        const auto phasor = DocStudio::Cas::EvaluateExpression("I = (10 @ 30 V) / (5 @ -15 Ohm)");
        F10_CHECK(phasor.ok && std::abs(phasor.result.Mag() - 2.0) < 1e-6);
        F10_CHECK(std::abs(phasor.result.PhaseDeg() - 45.0) < 1e-4);

        // Negative: dimensional incompatibility MUST fail loudly
        F10_CHECK(!DocStudio::Cas::EvaluateExpression("4.7 kOhm + 2 mA").ok);
        F10_CHECK(!DocStudio::Cas::EvaluateExpression("12 V | s").ok);
    }

    // 2. Reactive #varsub DAG out-of-order topological evaluation & cycle rejection
    {
        DocStudio::DocumentModel mathDoc;
        mathDoc.ResetToBlank(DocStudio::DocumentMode::InfiniteCanvas);

        DocStudio::AnnotationObject nodeP;
        nodeP.kind = DocStudio::ObjectKind::MathBlock;
        nodeP.bounds = { 40.0, 200.0, 320.0, 70.0 };
        nodeP.formulaVarNameUtf8 = "P";
        nodeP.formulaExprUtf8 = "P = V * I | mW";
        mathDoc.AddCanvasObject(nodeP);
        const std::uint64_t pId = mathDoc.CanvasObjects().back().id;

        DocStudio::AnnotationObject nodeV;
        nodeV.kind = DocStudio::ObjectKind::MathBlock;
        nodeV.bounds = { 40.0, 40.0, 260.0, 60.0 };
        nodeV.formulaVarNameUtf8 = "V";
        nodeV.formulaExprUtf8 = "V = 5 V";
        mathDoc.AddCanvasObject(nodeV);

        DocStudio::AnnotationObject nodeI;
        nodeI.kind = DocStudio::ObjectKind::MathBlock;
        nodeI.bounds = { 40.0, 120.0, 260.0, 60.0 };
        nodeI.formulaVarNameUtf8 = "I";
        nodeI.formulaExprUtf8 = "I = 2 mA";
        mathDoc.AddCanvasObject(nodeI);

        const auto dag = DocStudio::Cas::EvaluateDocumentMathDag(mathDoc);
        F10_CHECK(dag.ok && dag.evaluatedCount == 3);
        const auto* resP = mathDoc.FindCanvasObject(pId);
        F10_CHECK(resP != nullptr && resP->evaluatedResultUtf8.find("10 mW") != std::string::npos);

        // Cycle negative test
        DocStudio::DocumentModel cycDoc;
        DocStudio::AnnotationObject a, b;
        a.kind = DocStudio::ObjectKind::MathBlock; a.formulaVarNameUtf8 = "A"; a.formulaExprUtf8 = "A = B + 1";
        b.kind = DocStudio::ObjectKind::MathBlock; b.formulaVarNameUtf8 = "B"; b.formulaExprUtf8 = "B = A + 1";
        cycDoc.AddPageObject(0, a);
        cycDoc.AddPageObject(0, b);
        const auto cycRep = DocStudio::Cas::EvaluateDocumentMathDag(cycDoc);
        F10_CHECK(!cycRep.ok && cycRep.cycleCount == 2);
    }

    // 3. Algebraic step checker & 2D/3D plots & CSV OLS regression
    {
        const auto sOk = DocStudio::Cas::CheckAlgebraicStep("(x + 1)^2", "x^2 + 2*x + 1");
        F10_CHECK(sOk.equivalent && sOk.statusCode == "OK");

        const auto sDiff = DocStudio::Cas::CheckAlgebraicStep("(x + 1)^2", "x^2 + 1");
        F10_CHECK(!sDiff.equivalent && sDiff.statusCode == "DIFF");

        const auto sDim = DocStudio::Cas::CheckAlgebraicStep("5 m", "5 s");
        F10_CHECK(!sDim.equivalent && sDim.dimensionMismatch && sDim.statusCode == "DIM_ERR");

        const auto curve = DocStudio::Cas::SampleFunction2D("sin(x)", "x", 0.0, 3.14159265, 32);
        F10_CHECK(curve.size() == 32 && std::abs(curve[15].y - 1.0) < 0.05);

        const auto surf = DocStudio::Cas::SampleSurface3D("x^2 + y^2", -1.0, 1.0, 5, -1.0, 1.0, 5);
        F10_CHECK(surf.size() == 25);

        const auto reg = DocStudio::Cas::FitCsvRegression("0,1\n1,3\n2,5\n3,7\n", 1);
        F10_CHECK(reg.ok && reg.coeffs.size() == 2 && std::abs(reg.coeffs[0] - 1.0) < 1e-6 && std::abs(reg.coeffs[1] - 2.0) < 1e-6);
        F10_CHECK(reg.rSquared > 0.9999);
    }

    // 4. MNA Linear Circuit (DC Divider, AC RC -3dB Filter, OpAmp -10x), Bode, K-Map & Exporters
    {
        const auto dcDiv = DocStudio::Circuit::SolveMnaNetlist("V1 1 0 12; R1 1 2 2k; R2 2 0 4k", 0.0);
        F10_CHECK(dcDiv.ok && std::abs(dcDiv.nodeVoltages.at(2).real() - 8.0) < 1e-5);
        F10_CHECK(std::abs(dcDiv.totalPowerSumW) < 1e-6);

        std::vector<DocStudio::Circuit::CircuitComponent> rcComps;
        std::string nErr;
        F10_CHECK(DocStudio::Circuit::ParseNetlistText("V1 1 0 1; R1 1 2 1k; C1 2 0 1u", rcComps, &nErr));
        const double fc = 1.0 / (2.0 * 3.1415926535897932 * 1000.0 * 1e-6);
        const auto rcAc = DocStudio::Circuit::SolveMnaCircuit(rcComps, fc);
        F10_CHECK(rcAc.ok && std::abs(std::abs(rcAc.nodeVoltages.at(2)) - 0.7071067) < 1e-3);

        const auto bode = DocStudio::Circuit::ComputeBodeSweep(rcComps, 2, 10.0, 10000.0, 15);
        F10_CHECK(bode.size() == 15 && bode.front().magDb > -0.5 && bode.back().magDb < -30.0);

        const auto opamp = DocStudio::Circuit::SolveMnaNetlist("V1 1 0 1; R1 1 2 1k; R2 2 3 10k; OPAMP U1 0 2 3", 0.0);
        F10_CHECK(opamp.ok && std::abs(opamp.nodeVoltages.at(3).real() + 10.0) < 1e-4);

        const auto kmap = DocStudio::Circuit::SimplifyKarnaughMap(4, { 0, 1, 2, 3, 8, 9, 10, 11 });
        F10_CHECK(kmap.ok && kmap.minimizedSopUtf8 == "F = B'");

        F10_CHECK(DocStudio::Circuit::ExportCircuitToSvg(rcComps, &rcAc).find("<svg") != std::string::npos);
        F10_CHECK(DocStudio::Circuit::ExportCircuitToDxf(rcComps, &rcAc).find("ENTITIES") != std::string::npos);
        F10_CHECK(DocStudio::Circuit::ExportCircuitToKiCadNetlist(rcComps).find(".end") != std::string::npos);
    }

    // 5. Casio Session Card Bridge + Offscreen Render & Vector PDF Export of STEM Cards
    {
        DocStudio::DocumentModel stemDoc;
        stemDoc.ResetToBlank(DocStudio::DocumentMode::InfiniteCanvas);

        DocStudio::AnnotationObject mb;
        mb.kind = DocStudio::ObjectKind::MathBlock;
        mb.bounds = { 30.0, 30.0, 280.0, 70.0 };
        mb.formulaVarNameUtf8 = "Vo";
        mb.formulaExprUtf8 = "Vo = 9.4 V";
        mb.evaluatedResultUtf8 = "9.4 V";
        mb.textUtf8 = "5*2 = 9.4 V";
        stemDoc.AddCanvasObject(mb);

        DocStudio::CalcBridge::CalcStepRecord crec;
        crec.modelIdUtf8 = "fx-991CW";
        crec.keySeqUtf8 = "2+3=";
        crec.expressionUtf8 = "2 + 3";
        stemDoc.AddCanvasObject(DocStudio::CalcBridge::BuildCalcSessionCard(crec, { 30.0, 120.0, 280.0, 80.0 }));

        DocStudio::AnnotationObject plt;
        plt.kind = DocStudio::ObjectKind::PlotBlock;
        plt.bounds = { 30.0, 220.0, 280.0, 160.0 };
        plt.formulaExprUtf8 = "x^2 - 2";
        plt.plotConfigUtf8 = R"({"expr":"x^2 - 2","xMin":-3,"xMax":3})";
        stemDoc.AddCanvasObject(plt);

        DocStudio::D2DRenderer d2d;
        DocStudio::RasterImageBgra32 img;
        F10_CHECK(d2d.RenderInfiniteCanvasOffscreen(stemDoc, { 0.0, 0.0, 360.0, 420.0 }, 720, 840, img));
        F10_CHECK(img.Valid());

        const fs::path stemPdf = tmpDir / "f10_stem_canvas.pdf";
        DocStudio::CanvasPdfExportOptions copts;
        copts.paginated = false;
        copts.hasCustomRegion = true;
        copts.customRegionWorld = { 0.0, 0.0, 360.0, 420.0 };
        std::string err;
        F10_CHECK(DocStudio::PdfWriter::ExportInfiniteCanvasToFile(stemDoc, copts, stemPdf.wstring(), &err));

        DocStudio::PdfDocument stemParsed;
        F10_CHECK(stemParsed.LoadFromFile(stemPdf.wstring(), &err));
        F10_CHECK(stemParsed.PageAt(0).extractedTextUtf8.find("CASIO fx-991CW") != std::string::npos);
    }
#undef F10_CHECK
}

void TestF11FourDocsComplianceSuite(const fs::path& tmpDir) {
    std::cout << "-- Test F11: 4-Docs Compliance (Calcpad Units/#varsub, SEL/Matrix, Diode/Transient/Laplace, 3D CVD, Selective Eraser, Ruler, Conflict Merge, Lab HTML) --\n";

    // 1. Exact Calcpad dimensional benchmarks from Superando pp. 13-15 + negative dimensional failure path
    {
        const auto pBar = Cas::EvaluateExpression("3 bar + 100 psi");
        CHECK(pBar.ok);
        CHECK(pBar.formattedResultUtf8.find("9.89") != std::string::npos);
        CHECK(pBar.formattedResultUtf8.find("bar") != std::string::npos);

        const auto pPsi = Cas::EvaluateExpression("3 bar + 100 psi | psi");
        CHECK(pPsi.ok);
        CHECK(pPsi.formattedResultUtf8.find("143.5") != std::string::npos);
        CHECK(pPsi.formattedResultUtf8.find("psi") != std::string::npos);

        const auto areaCm = Cas::EvaluateExpression("0.5 m^2 | cm^2");
        CHECK(areaCm.ok && areaCm.formattedResultUtf8.find("5000") != std::string::npos);

        const auto areaFt = Cas::EvaluateExpression("0.5 m^2 | ft^2");
        CHECK(areaFt.ok && areaFt.formattedResultUtf8.find("5.38") != std::string::npos);

        Cas::SymbolEnv env;
        const auto fRes = Cas::EvaluateExpression("F = 10 kip", env);
        CHECK(fRes.ok);
        env["F"] = fRes.result;
        const auto lRes = Cas::EvaluateExpression("L = 10 ft", env);
        CHECK(lRes.ok);
        env["L"] = lRes.result;
        const auto mRes = Cas::EvaluateExpression("M = F * L", env);
        CHECK(mRes.ok && mRes.formattedResultUtf8.find("100") != std::string::npos);
        CHECK(mRes.formattedResultUtf8.find("kip*ft") != std::string::npos);

        // Negative: adding torque/energy (Nm) + length (m) MUST fail dimensionally
        const auto badDim = Cas::EvaluateExpression("1 Nm + 1 m");
        CHECK(!badDim.ok);
    }

    // 2. #varsub / #nosub / #novar modes + reactive PlotBlock parameter propagation
    {
        DocumentModel doc;
        doc.ResetToBlank(DocumentMode::InfiniteCanvas);

        AnnotationObject mParam;
        mParam.kind = ObjectKind::MathBlock;
        mParam.formulaVarNameUtf8 = "k";
        mParam.formulaExprUtf8 = "k = 2";
        doc.AddCanvasObject(mParam);
        const std::uint64_t kId = doc.CanvasObjects().back().id;

        AnnotationObject mDep;
        mDep.kind = ObjectKind::MathBlock;
        mDep.formulaVarNameUtf8 = "w";
        mDep.formulaExprUtf8 = "w = k * 5";
        mDep.mathSubMode = MathSubMode::VarSub;
        doc.AddCanvasObject(mDep);
        const std::uint64_t wId = doc.CanvasObjects().back().id;

        AnnotationObject plt;
        plt.kind = ObjectKind::PlotBlock;
        plt.formulaExprUtf8 = "sin(k * x)";
        plt.plotConfigUtf8 = R"json({"expr":"sin(k * x)","xMin":0,"xMax":3.14})json";
        doc.AddCanvasObject(plt);
        const std::uint64_t pId = doc.CanvasObjects().back().id;

        CHECK(Cas::EvaluateDocumentMathDag(doc).ok);
        CHECK(doc.FindCanvasObject(wId)->textUtf8.find("2") != std::string::npos);

        doc.FindCanvasObjectMutable(wId)->mathSubMode = MathSubMode::NoSub;
        Cas::EvaluateDocumentMathDag(doc);
        CHECK(doc.FindCanvasObject(wId)->textUtf8.find("k * 5") != std::string::npos);

        doc.FindCanvasObjectMutable(wId)->mathSubMode = MathSubMode::NoVar;
        Cas::EvaluateDocumentMathDag(doc);
        CHECK(doc.FindCanvasObject(wId)->textUtf8 == "10");

        // Update upstream variable k = 5 -> PlotBlock config env updates reactively
        doc.FindCanvasObjectMutable(kId)->formulaExprUtf8 = "k = 5";
        Cas::EvaluateDocumentMathDag(doc);
        CHECK(doc.FindCanvasObject(pId)->plotConfigUtf8.find("\"k\":5") != std::string::npos);
    }

    // 3. Matrix Algebra & Linear System Solver (SEL) + singular failure paths
    {
        const auto sel = Cas::SolveLinearSystemEquations("2*x + y = 7; x - y = 2");
        CHECK(sel.ok && sel.values.size() == 2);
        CHECK(std::abs(sel.values[0] - 3.0) < 1e-6 && std::abs(sel.values[1] - 1.0) < 1e-6);

        const auto badSel = Cas::SolveLinearSystemEquations("x + y = 2; 2*x + 2*y = 5");
        CHECK(!badSel.ok);

        const auto detM = Cas::EvaluateMatrixExpression("det([[2, 1], [1, 3]])");
        CHECK(detM.ok && detM.isScalar && std::abs(detM.scalarValue - 5.0) < 1e-9);

        const auto invM = Cas::EvaluateMatrixExpression("inv([[2, 1], [1, 3]])");
        CHECK(invM.ok && invM.rows == 2 && invM.cols == 2 && std::abs(invM.matrix[0][0] - 0.6) < 1e-6);

        const auto mulM = Cas::EvaluateMatrixExpression("[[1, 2], [3, 4]] * [[5], [6]]");
        CHECK(mulM.ok && mulM.rows == 2 && mulM.cols == 1);
        CHECK(std::abs(mulM.matrix[0][0] - 17.0) < 1e-9 && std::abs(mulM.matrix[1][0] - 39.0) < 1e-9);

        const auto badInv = Cas::EvaluateMatrixExpression("inv([[1, 2], [2, 4]])");
        CHECK(!badInv.ok);
    }

    // 4. Shockley Diode Newton-Raphson + Transient Oscilloscope + Symbolic Laplace H(s)
    {
        const auto fwd = Circuit::SolveMnaNetlist("V1 1 0 5\nD1 1 2\nR1 2 0 1k", 0.0);
        CHECK(fwd.ok);
        const double vOutFwd = fwd.nodeVoltages.at(2).real();
        CHECK(vOutFwd > 4.1 && vOutFwd < 4.5);

        const auto rev = Circuit::SolveMnaNetlist("V1 1 0 -5\nD1 1 2\nR1 2 0 1k", 0.0);
        CHECK(rev.ok && std::abs(rev.nodeVoltages.at(2).real()) < 1e-4);

        const auto tr = Circuit::SimulateTransientNetlist("V1 1 0 10\nD1 1 2\nR1 2 0 1k", 0.035, 140, 60.0, 2);
        CHECK(tr.ok && tr.vMax > 8.8 && tr.vMin > -0.05 && tr.vRms > 4.0);

        const auto lap = Circuit::AnalyzeSymbolicLaplaceNetlist("V1 1 0 1\nR1 1 2 1k\nC1 2 0 1u", 2);
        CHECK(lap.ok && !lap.nodalEquationsLatex.empty());
        CHECK(std::abs(lap.dcGain - 1.0) < 1e-4);
        CHECK(lap.transferFunctionUtf8.find("0.001*s") != std::string::npos);
    }

    // 5. 3D CVD Surface Plot + Offscreen HUD (Minimap + Ruler) + Ink Prediction, Selective Eraser & Ruler Snap
    {
        AnnotationObject p3d;
        p3d.kind = ObjectKind::PlotBlock;
        p3d.bounds = { 40.0, 40.0, 380.0, 240.0 };
        p3d.formulaExprUtf8 = "sin(x^2 + y^2)";
        p3d.plotConfigUtf8 = R"json({"mode":"3d","is3D":true,"expr":"sin(x^2 + y^2)","xMin":-2.2,"xMax":2.2,"yMin":-2.2,"yMax":2.2,"gridSteps":14})json";
        const auto pr3 = RenderCommon::BuildPlotRenderData(p3d);
        CHECK(pr3.is3D && pr3.surface3DSegments.size() >= 100);

        std::vector<InkPoint> pts = {
            { 10.0, 20.0, 0.5f, 0.0f, 0.0f, 100ull },
            { 20.0, 20.0, 0.5f, 0.0f, 0.0f, 110ull },
            { 30.0, 20.0, 0.5f, 0.0f, 0.0f, 120ull }
        };
        InkPoint pred{};
        CHECK(InkEngine::PredictNextInkPoint(pts, pred, 12.0));
        CHECK(pred.x > 30.5 && std::abs(pred.y - 20.0) < 1e-6);

        RulerState ruler;
        ruler.visible = true;
        ruler.centerX = 100.0;
        ruler.centerY = 100.0;
        ruler.angleDeg = 0.0;
        bool snapped = false;
        const DocPoint rp = InkEngine::ProjectPointOntoRuler({ 135.0, 109.0 }, ruler, 24.0, &snapped);
        CHECK(snapped && std::abs(rp.x - 135.0) < 1e-6 && std::abs(rp.y - 100.0) < 1e-6);

        DocumentModel edoc;
        edoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject penS;
        penS.kind = ObjectKind::InkStroke;
        penS.isHighlighter = false;
        penS.inkPoints = { { 10.0, 10.0 }, { 50.0, 10.0 } };
        edoc.AddCanvasObject(penS);
        const std::uint64_t penId = edoc.CanvasObjects().back().id;

        AnnotationObject hlS = penS;
        hlS.isHighlighter = true;
        edoc.AddCanvasObject(hlS);
        const std::uint64_t hlId = edoc.CanvasObjects().back().id;

        const auto filtered = InkEngine::FilterHitsByEraserMode(
            edoc, { true, 0 }, { penId, hlId }, EraserFilterMode::HighlighterOnly, true);
        CHECK(filtered.size() == 1 && filtered.front() == hlId);
    }

    // 6. Undo Boundary, Bookmarks, Tags, Version History, Conflict Page Merge, .cudoc Round-Trip & HTML5 Lab Report
    {
        DocumentModel doc;
        CommandStack stack;
        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 20.0, 20.0, 200.0, 40.0 };
        tb.textUtf8 = "Antes de frontera";
        tb.tags = { "examen", "importante" };
        CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, tb)));
        stack.SetUndoBoundary();
        CHECK(!stack.CanUndo());
        CHECK(!stack.Undo(doc));
        stack.ClearUndoBoundary();
        CHECK(stack.CanUndo());

        doc.AddBookmark("Portada Tecnica", 0);
        doc.AddCanvasObject([]() {
            AnnotationObject c;
            c.kind = ObjectKind::CircuitBlock;
            c.bounds = { 40.0, 120.0, 320.0, 110.0 };
            c.circuitNetlistUtf8 = "V1 1 0 10\nR1 1 2 1k\nC1 2 0 1u";
            c.evaluatedResultUtf8 = "V(2)=10 V";
            return c;
        }());

        const auto tagsFound = CollectTaggedItems(doc, "examen");
        CHECK(tagsFound.size() == 1);

        const fs::path cudocP = tmpDir / "f11_full.cudoc";
        std::string err;
        CHECK(CudocStorage::SaveToFile(cudocP.wstring(), doc, &err));
        DocumentModel loaded;
        CHECK(CudocStorage::LoadFromFile(cudocP.wstring(), loaded, &err));
        CHECK(loaded.Bookmarks().size() == 1 && loaded.Bookmarks()[0].titleUtf8 == "Portada Tecnica");
        CHECK(!loaded.Pages()[0].objects.empty() && loaded.Pages()[0].objects[0].tags.size() == 2);

        const std::string html = Bridge::ExportCalculationReportHtml(loaded);
        CHECK(html.find("<!DOCTYPE html>") != std::string::npos);
        CHECK(html.find("#examen") != std::string::npos);
        CHECK(html.find("<svg") != std::string::npos);
        CHECK(html.find("H(s)") != std::string::npos);

        // PageVersionHistory snapshot & restore
        PageVersionHistory hist;
        const std::uint64_t v1 = hist.CaptureSnapshot(loaded, 0, 1000ull, "v1");
        loaded.MutablePages()[0].objects.clear();
        CHECK(loaded.Pages()[0].objects.empty());
        CHECK(hist.RestoreSnapshot(loaded, v1));
        CHECK(!loaded.Pages()[0].objects.empty());

        // Concurrent Offline Conflict Page Merge (OneNote zero-data-loss guarantee)
        DocumentModel baseD, locD, remD, mergedD;
        AnnotationObject oBase;
        oBase.kind = ObjectKind::TextBox;
        oBase.bounds = { 10.0, 10.0, 100.0, 30.0 };
        oBase.textUtf8 = "Base";
        baseD.AddPageObject(0, oBase);

        locD.AddPageObject(0, oBase);
        locD.MutablePages()[0].objects[0].textUtf8 = "Edicion Local";

        remD.AddPageObject(0, oBase);
        remD.MutablePages()[0].objects[0].textUtf8 = "Edicion Remota";

        const auto mres = MergeConcurrentDocuments(baseD, locD, remD, mergedD);
        CHECK(mres.ok && mres.conflictPagesCreated == 1);
        CHECK(mergedD.Pages().size() == 2);
        CHECK(!mergedD.SearchText("PAGINA DE CONFLICTO", false).empty());
        CHECK(!mergedD.SearchText("Edicion Local", false).empty());
        CHECK(!mergedD.SearchText("Edicion Remota", false).empty());
    }

    // 7. Reverse Bridge (Lienzo->Casio Keystrokes), Phasor Diagram, Variable Inspector, Teacher Private Layer & 20% Replay Dimming
    {
        const auto kSel = CalcBridge::TranslateExpressionToCasioKeys("2*x + y = 7; x - y = 2", "fx-991CW");
        CHECK(kSel.ok && kSel.appModeUtf8.find("Equation") != std::string::npos);
        CHECK(kSel.keySequenceUtf8.find("Simultan") != std::string::npos);

        const auto kMat = CalcBridge::TranslateExpressionToCasioKeys("det([[2, 1], [1, 3]])", "fx-991CW");
        CHECK(kMat.ok && kMat.appModeUtf8 == "Matrix" && kMat.keySequenceUtf8.find("Determinant") != std::string::npos);

        const auto kOhm = CalcBridge::TranslateExpressionToCasioKeys("V_out = 4.7 kOhm * 2 mA | V", "fx-991CW");
        CHECK(kOhm.ok && kOhm.keySequenceUtf8.find("[x10^x]") != std::string::npos);

        const std::string allForms = CalcBridge::FormatPhasorAllFormsUtf8({ 3.0, 4.0 }, "V");
        CHECK(allForms.find("Rect:") != std::string::npos &&
              allForms.find("Polar: 5") != std::string::npos &&
              allForms.find("Exp: 5") != std::string::npos);

        AnnotationObject phPlot;
        phPlot.kind = ObjectKind::PlotBlock;
        phPlot.plotConfigUtf8 = R"json({"mode":"phasor","phasors":[{"mag":120,"deg":0},{"mag":40,"deg":-36.87}]})json";
        const auto phData = RenderCommon::BuildPlotRenderData(phPlot);
        CHECK(phData.is3D && phData.surface3DSegments.size() >= 70);

        DocumentModel vdoc;
        vdoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject v1;
        v1.kind = ObjectKind::MathBlock;
        v1.formulaVarNameUtf8 = "V_s";
        v1.formulaExprUtf8 = "V_s = 12 V";
        vdoc.AddCanvasObject(v1);
        AnnotationObject r1;
        r1.kind = ObjectKind::MathBlock;
        r1.formulaVarNameUtf8 = "R_1";
        r1.formulaExprUtf8 = "R_1 = 4.7 kOhm";
        vdoc.AddCanvasObject(r1);
        const auto inspected = Cas::InspectDocumentVariables(vdoc);
        CHECK(inspected.size() == 2);
        CHECK(inspected[0].physicalQuantityUtf8.find("Tensi") != std::string::npos);
        CHECK(inspected[1].physicalQuantityUtf8.find("Resistencia") != std::string::npos);

        // Dual-view Teacher Presenter Layer & 20% Future Node Replay Attenuation
        const std::uint64_t privLayer = vdoc.AddLayer("Notas Privadas Docente");
        vdoc.SetLayerProperties(privLayer, true, false, false, 1.0);
        AnnotationObject teacherNote;
        teacherNote.layerId = privLayer;
        teacherNote.inkStartTimeMs = 5000ull;

        ViewportOverlayState ovEdit{};
        ovEdit.presenterMode = false;
        ovEdit.replayActive = true;
        ovEdit.replayCursorMs = 1000ull;
        double op = 1.0;
        CHECK(RenderCommon::ShouldDrawObjectOnLayer(vdoc, teacherNote, op, &ovEdit));
        CHECK(std::abs(op - 0.20) < 1e-6);

        ViewportOverlayState ovProj{};
        ovProj.presenterMode = true;
        CHECK(!RenderCommon::ShouldDrawObjectOnLayer(vdoc, teacherNote, op, &ovProj));

#ifdef _WIN32
        // Live synthetic WM_DPICHANGED (144 -> 192 -> 96 DPI) cycle test on DocumentStudio.exe
        wchar_t exePathBuf[4096]{};
        if (GetModuleFileNameW(nullptr, exePathBuf, 4096) > 0) {
            const fs::path studioExe = fs::path(exePathBuf).parent_path() / "DocumentStudio.exe";
            const bool packaged = fs::is_regular_file(studioExe);
            CHECK(packaged);
            if (packaged) {
                const fs::path dpiProfile = tmpDir / L"Perfil DPI";
                std::wstring cmdLine = L"\"" + studioExe.wstring() +
                    L"\" --showcase-stem --inspector academic --test-dpi-cycle --profile-dir \"" +
                    dpiProfile.wstring() + L"\"";
                STARTUPINFOW si{};
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi{};
                const BOOL created = CreateProcessW(
                    studioExe.c_str(), cmdLine.data(),
                    nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
                CHECK(created != FALSE);
                if (created) {
                    const DWORD waitRes = WaitForSingleObject(pi.hProcess, 15000);
                    DWORD exitCode = 999;
                    GetExitCodeProcess(pi.hProcess, &exitCode);
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    CHECK(waitRes == WAIT_OBJECT_0);
                    CHECK(exitCode == 0);
                    CHECK(fs::is_directory(dpiProfile));
                }
            }
        }
#endif
    }
}

// Auditoría pasos cortos, P0: las copias de borrador (autoguardado cada 60 s, «No guardar») no deben dejar el
// documento como «guardado», y un borrador recuperado sigue «modificado» hasta que se guarde de verdad.
void TestF12DraftCopiesKeepUnsavedFlag(const fs::path& tmpDir) {
    std::cout << "-- Test F12: Borradores sin perder la marca de cambios sin guardar --\n";
    DocumentModel doc;
    CommandStack stack;
    AnnotationObject ink;
    ink.kind = ObjectKind::InkStroke;
    ink.inkPoints = { {10.0, 10.0, 0.5f}, {40.0, 30.0, 0.5f} };
    CHECK(stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, ink)));
    CHECK(doc.IsDirty());

    // SaveToFile deja el documento como guardado: una copia tiene que devolverle su marca (WriteDocumentCopy)
    const fs::path copyPath = tmpDir / L"Autoguardado prueba.cudoc";
    std::string err;
    const bool wasDirty = doc.IsDirty();
    CHECK(CudocStorage::SaveToFile(copyPath.wstring(), doc, &err));
    CHECK(!doc.IsDirty());
    doc.SetDirty(wasDirty);
    CHECK(doc.IsDirty());

    // Borrador recuperado: sigue modificado aunque se deshaga o rehaga todo
    CommandStack recStack;
    DocumentModel recovered;
    CHECK(CudocStorage::LoadFromFile(copyPath.wstring(), recovered, &err));
    CHECK(!recovered.IsDirty());
    recStack.MarkCleanUnreachable(recovered);
    CHECK(recovered.IsDirty());
    CHECK(recStack.Execute(recovered, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, ink)));
    CHECK(recovered.IsDirty());
    CHECK(recStack.Undo(recovered));
    CHECK(recovered.IsDirty()); // con solo SetDirty(true), deshacer hasta el principio lo daba por guardado
    CHECK(recStack.Redo(recovered));
    CHECK(recovered.IsDirty());

    // Guardarlo de verdad lo limpia, y deshacer y rehacer vuelven a marcarlo como siempre
    recStack.MarkClean(recovered);
    CHECK(!recovered.IsDirty());
    CHECK(recStack.Undo(recovered));
    CHECK(recovered.IsDirty());
    CHECK(recStack.Redo(recovered));
    CHECK(!recovered.IsDirty());
}

} // namespace

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    const fs::path manualsDir = (argc >= 2) ? fs::path(argv[1]) : fs::path("manuales");
    const fs::path keepDir = (argc >= 3) ? fs::path(argv[2]) : fs::path{};
    ScopedTempDir tmp(keepDir);

    TestF1CudocAndCommandStack(tmp.dir);
    TestF2LayoutCameraAndLruCache();
    TestF3D2DRenderAndMultiPressureInk();
    TestF4RealCasioPdfEditMergeSplitFlatten(manualsDir, tmp.dir);
    TestF5TextExtractionReplacementAndOcr(manualsDir, tmp.dir);
    TestF6InfiniteCanvas100kQuadtree(tmp.dir);
    TestF7Pkcs7SignatureAndOpenXml(tmp.dir);
    TestF8P0AuditRegressionFixes(manualsDir, tmp.dir);
    TestF9P1P2GoodnotesOneNoteParity(tmp.dir);
    TestF10P3StemEngineeringSuperiority(tmp.dir);
    TestF11FourDocsComplianceSuite(tmp.dir);
    TestF12DraftCopiesKeepUnsavedFlag(tmp.dir);

    WinRtPdfRenderer::ClearOpenDocumentCache();

    std::cout << "\nTOTAL CHECKS: passed=" << gPasses << ", failed=" << gFailures << "\n";
    if (gFailures == 0) {
        std::cout << "ALL DocumentCoreTest CHECKS PASSED (F1-F12)\n";
        return 0;
    }
    std::cerr << "\nDocumentCoreTest FAILED with " << gFailures << " failure(s)\n";
    return 1;
}
