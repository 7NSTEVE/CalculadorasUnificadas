#include "core/audio_timeline.hpp"
#include "core/camera.hpp"
#include "core/command_stack.hpp"
#include "core/cudoc_storage.hpp"
#include "core/doc_types.hpp"
#include "core/document_model.hpp"
#include "core/ink_engine.hpp"
#include "core/lru_byte_cache.hpp"
#include "core/notebook_vault.hpp"
#include "core/page_layout.hpp"
#include "core/spatial_index.hpp"
#include "core/utf_conv.hpp"
#include "core/zip_container.hpp"
#include "bridge/calc_bridge.hpp"
#include "bridge/lab_report_exporter.hpp"
#include "circuit/mna_solver.hpp"
#include "math/engineering_cas.hpp"
#include "pdf/interop_openxml.hpp"
#include "pdf/pdf_parser.hpp"
#include "pdf/pdf_signer.hpp"
#include "pdf/pdf_writer.hpp"
#include "pdf/winrt_adapters.hpp"
#include "render/card_render_common.hpp"
#include "render/d2d_renderer.hpp"

#include <cairo.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace DocStudio;

namespace {

int gChecksPassed = 0;
int gChecksFailed = 0;

void Check(bool cond, const char* expr, int line) {
    if (cond) {
        ++gChecksPassed;
    } else {
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
        ++gChecksFailed;
    }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

std::wstring PathToW(const fs::path& p) {
    return Utf8ToWide(p.string());
}

fs::path JoinUtf8(const fs::path& base, const std::string& nameUtf8) {
    return base / fs::path(nameUtf8);
}

bool ReadFileBytes(const fs::path& p, std::vector<std::uint8_t>& out) {
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    const auto sz = ifs.tellg();
    if (sz < 0) return false;
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!out.empty()) {
        ifs.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    }
    return static_cast<bool>(ifs);
}

bool WriteFileBytes(const fs::path& p, const std::vector<std::uint8_t>& bytes) {
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    if (!bytes.empty()) {
        ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    ofs.flush();
    return static_cast<bool>(ofs);
}

struct ScopedTempDir {
    fs::path dir;
    bool keep = false;
    explicit ScopedTempDir(const fs::path& overrideDir = {}) {
        if (!overrideDir.empty()) {
            dir = overrideDir;
            keep = true;
        } else {
            dir = fs::temp_directory_path() / ("docstudio_linux_test_" + std::to_string(::getpid()));
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
    img.pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
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

bool CreateTestPfxFile(const fs::path& pfxPath, const std::string& passwordUtf8, const std::string& cnUtf8) {
    EVP_PKEY* pkey = EVP_RSA_gen(2048);
    if (!pkey) return false;
    X509* cert = X509_new();
    if (!cert) {
        EVP_PKEY_free(pkey);
        return false;
    }
    X509_set_version(cert, 2);
    ASN1_INTEGER_set_uint64(X509_get_serialNumber(cert), static_cast<std::uint64_t>(::getpid()) * 131u + 7u);
    X509_gmtime_adj(X509_getm_notBefore(cert), -60);
    X509_gmtime_adj(X509_getm_notAfter(cert), 365L * 24L * 3600L);
    X509_NAME* name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_UTF8,
        reinterpret_cast<const unsigned char*>(cnUtf8.data()),
        static_cast<int>(cnUtf8.size()), -1, 0);
    X509_set_issuer_name(cert, name);
    X509_set_pubkey(cert, pkey);
    const int signOk = X509_sign(cert, pkey, EVP_sha256());
    if (signOk <= 0) {
        X509_free(cert);
        EVP_PKEY_free(pkey);
        return false;
    }

    PKCS12* p12 = PKCS12_create(
        passwordUtf8.c_str(),
        "DocStudioTestKey",
        pkey,
        cert,
        nullptr,
        0, 0, 0, 0, 0);
    X509_free(cert);
    EVP_PKEY_free(pkey);
    if (!p12) return false;

    std::FILE* fp = std::fopen(pfxPath.c_str(), "wb");
    if (!fp) {
        PKCS12_free(p12);
        return false;
    }
    const int wr = i2d_PKCS12_fp(fp, p12);
    std::fclose(fp);
    PKCS12_free(p12);
    return wr == 1;
}

void TestF1CudocAndCommandStack(const fs::path& tmpDir) {
    std::cout << "-- Test F1: .cudoc Round-trip, Corruption Rejection, Transactional Save (.bak), CommandStack --\n";
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
    CHECK(imgObj.pngBytes.size() >= 8 && imgObj.pngBytes[0] == 0x89u && imgObj.pngBytes[1] == 'P' &&
          imgObj.pngBytes[2] == 'N' && imgObj.pngBytes[3] == 'G');
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
    CHECK(stack.Undo(doc));
    CHECK(doc.Pages().size() == 2);

    // Atomic transactional save + round-trip load (including non-ASCII UTF-8 filename test)
    const fs::path cudocPath = JoinUtf8(tmpDir, "proyecto_\xC3\xB1_prueba.cudoc");
    std::string err;
    CHECK(CudocStorage::SaveToFile(PathToW(cudocPath), doc, &err));
    CHECK(!doc.IsDirty());

    // Simulate failed overwrite mid-transaction: original must survive intact!
    doc.SetTitle("TituloNoGuardado");
    CHECK(!CudocStorage::SaveToFile(PathToW(cudocPath), doc, &err, true));

    DocumentModel loaded;
    CHECK(CudocStorage::LoadFromFile(PathToW(cudocPath), loaded, &err));
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
        CHECK(CudocStorage::LoadFromFile(PathToW(cudocPath), v2, &err));
        v2.SetTitle("SegundaVersion");
        CHECK(CudocStorage::SaveToFile(PathToW(cudocPath), v2, &err)); // creates .bak containing v1
        CHECK(WriteFileBytes(cudocPath, junk));

        bool usedBak = false;
        DocumentModel recovered;
        CHECK(CudocStorage::LoadFromFileWithRecovery(PathToW(cudocPath), recovered, &usedBak, &err));
        CHECK(usedBak);
        CHECK(recovered.Title() == "Cuaderno C\xC3\xA1lculo CASIO");
    }
}

void TestF2LayoutCameraAndLruCache(const fs::path& manualsDir) {
    std::cout << "-- Test F2: 1,200-Page Layout, Zoom-at-Anchor, LRU Cache & Async PageRenderCache --\n";
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

    Camera2D cam;
    cam.panX = -120.0;
    cam.panY = 3450.0;
    cam.zoom = 1.25;
    const DocPoint beforeW = cam.ScreenToWorld(640.0, 410.0);
    cam.ZoomAtScreenAnchor(640.0, 410.0, 2.85);
    const DocPoint afterW = cam.ScreenToWorld(640.0, 410.0);
    CHECK(std::abs(beforeW.x - afterW.x) < 1e-6);
    CHECK(std::abs(beforeW.y - afterW.y) < 1e-6);

    LruByteCache<int, std::vector<std::uint8_t>> lru(1024u * 1024u);
    CHECK(lru.Put(1, std::vector<std::uint8_t>(400u * 1024u, 1), 400u * 1024u));
    CHECK(lru.Put(2, std::vector<std::uint8_t>(400u * 1024u, 2), 400u * 1024u));
    std::vector<std::uint8_t> tmp;
    CHECK(lru.TryGet(1, tmp));
    CHECK(lru.Put(3, std::vector<std::uint8_t>(400u * 1024u, 3), 400u * 1024u));
    CHECK(lru.BytesUsed() <= 1024u * 1024u);
    CHECK(lru.EvictionCount() == 1);
    CHECK(lru.TryGet(1, tmp));
    CHECK(!lru.TryGet(2, tmp));
    CHECK(!lru.Put(99, std::vector<std::uint8_t>(2u * 1024u * 1024u, 9), 2u * 1024u * 1024u));

    // PageRenderCache async stress (>64 queue drops -> inFlight_ must not leak) + byte budget eviction + clean shutdown
    const fs::path laPdf = JoinUtf8(manualsDir, "fx-570_991LA_X_ES.pdf");
    if (fs::exists(laPdf)) {
        PageRenderCache prCache(2ull * 1024ull * 1024ull); // 2 MiB budget forces evictions
        std::atomic<int> readyCount{ 0 };
        std::vector<PageCacheKey> keys;
        for (int i = 0; i < 72; ++i) {
            PageModel pm;
            pm.pageId = static_cast<std::uint64_t>(i + 1);
            pm.hasPdfSource = true;
            pm.sourcePdfPathUtf8 = laPdf.string();
            pm.sourcePageIndex0 = i % 10;
            const PageCacheKey k = PageRenderCache::MakeKey(pm, 320);
            keys.push_back(k);
            prCache.RequestAsync(k, 1, [&]() { readyCount.fetch_add(1, std::memory_order_relaxed); });
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (readyCount.load(std::memory_order_relaxed) == 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        CHECK(readyCount.load(std::memory_order_relaxed) > 0);
        CHECK(prCache.BytesUsed() <= prCache.MaxBytes());

        // Re-request a dropped early key (generation 2) to prove dropped jobs didn't stick in inFlight_
        std::atomic<bool> reRequestedReady{ false };
        prCache.RequestAsync(keys[0], 2, [&]() { reRequestedReady.store(true, std::memory_order_release); });
        const auto d2 = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        RasterImageBgra32 probe;
        while (!reRequestedReady.load(std::memory_order_acquire) &&
               !prCache.TryGet(keys[0], probe) &&
               std::chrono::steady_clock::now() < d2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        CHECK(reRequestedReady.load(std::memory_order_acquire) || prCache.TryGet(keys[0], probe));

        prCache.Shutdown();
    }
}

void TestF3CairoOffscreenAndVectorRender() {
    std::cout << "-- Test F3: Cairo/PangoCairo Offscreen Render, RenderToCairo, EncodeBgra32ToPngBytes --\n";
    DocumentModel doc;
    doc.MutablePages()[0].background = PaperPattern::Grid;

    AnnotationObject pen;
    pen.kind = ObjectKind::InkStroke;
    pen.strokeColor = DocColor::RGBA(26, 115, 232, 255);
    pen.strokeWidth = 6.0;
    pen.inkPoints = { {60.0, 120.0, 0.2f}, {220.0, 120.0, 0.9f}, {420.0, 180.0, 0.7f} };
    const std::uint64_t penId = doc.AllocateObjectId();
    pen.id = penId;
    doc.AddPageObject(0, std::move(pen));

    AnnotationObject hl;
    hl.kind = ObjectKind::InkStroke;
    hl.isHighlighter = true;
    hl.strokeColor = DocColor::RGBA(250, 204, 21, 255);
    hl.strokeWidth = 14.0;
    hl.inkPoints = { {60.0, 320.0, 0.5f}, {320.0, 320.0, 0.5f} };
    doc.AddPageObject(0, std::move(hl));

    AnnotationObject stamp;
    stamp.kind = ObjectKind::Shape;
    stamp.shapeKind = ShapeKind::SignatureStamp;
    stamp.bounds = { 300.0, 360.0, 220.0, 72.0 };
    stamp.strokeColor = DocColor::RGBA(20, 72, 160, 255);
    stamp.fillColor = DocColor::RGBA(236, 244, 255, 255);
    stamp.textUtf8 = "Firmado: CASIO Linux";
    doc.AddPageObject(0, std::move(stamp));

    AnnotationObject textBox;
    textBox.kind = ObjectKind::TextBox;
    textBox.bounds = { 80.0, 220.0, 420.0, 80.0 };
    textBox.textUtf8 = "CASIO 2026 DOCUMENT STUDIO";
    textBox.fontSizePt = 28.0;
    textBox.bold = true;
    textBox.strokeColor = DocColor::RGBA(10, 10, 10, 255);
    doc.AddPageObject(0, std::move(textBox));

    D2DRenderer renderer;
    CHECK(renderer.InitializeFactories());
    renderer.Resize(800, 600);

    RasterImageBgra32 img;
    CHECK(renderer.RenderPageOffscreen(doc, 0, 800, nullptr, img));
    CHECK(img.Valid());
    CHECK(img.width == 800);
    CHECK(img.height > 900);
    CHECK(img.pixels.size() == static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 4u);

    std::size_t bluePixels = 0;
    std::size_t darkTextPixels = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height); ++i) {
        const std::uint8_t b = img.pixels[i * 4 + 0];
        const std::uint8_t g = img.pixels[i * 4 + 1];
        const std::uint8_t r = img.pixels[i * 4 + 2];
        if (b > 170 && r < 80) ++bluePixels;
        if (r < 60 && g < 60 && b < 60) ++darkTextPixels;
    }
    std::cout << "  Cairo Offscreen pixels -> size=" << img.width << "x" << img.height
              << ", blue_ink=" << bluePixels << ", dark_text=" << darkTextPixels << "\n";
    CHECK(bluePixels > 500);
    CHECK(darkTextPixels > 400);

    // EncodeBgra32ToPngBytes verification
    std::vector<std::uint8_t> encodedPng;
    CHECK(WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, encodedPng));
    CHECK(encodedPng.size() > 1000);
    CHECK(encodedPng[0] == 0x89u && encodedPng[1] == 'P' && encodedPng[2] == 'N' && encodedPng[3] == 'G');

    // Direct RenderToCairo with overlay (selection handles + crop preview mask + search highlight)
    {
        constexpr int cW = 640;
        constexpr int cH = 480;
        cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, cW, cH);
        CHECK(surf && cairo_surface_status(surf) == CAIRO_STATUS_SUCCESS);
        cairo_t* cr = cairo_create(surf);

        Camera2D cam;
        cam.panX = -320.0;
        cam.panY = 0.0;
        cam.zoom = 0.75;

        std::vector<SearchMatch> matches(1);
        matches[0].pageIndex = 0;
        matches[0].hasBox = true;
        matches[0].boxNormalized = { 0.1, 0.25, 0.4, 0.08 };

        ViewportOverlayState overlay;
        overlay.selectedObjectId = penId;
        overlay.hasCropPreview = true;
        overlay.cropPageIndex = 0;
        overlay.cropPreviewPageLocal = { 40.0, 40.0, 500.0, 700.0 };
        overlay.searchMatches = &matches;
        overlay.activeSearchMatchIndex = 0;

        renderer.RenderToCairo(cr, doc, cam, cW, cH, nullptr, 1, nullptr, &overlay);
        cairo_destroy(cr);
        cairo_surface_flush(surf);

        const unsigned char* px = cairo_image_surface_get_data(surf);
        const int stride = cairo_image_surface_get_stride(surf);
        std::size_t nonBg = 0;
        for (int y = 0; y < cH; ++y) {
            const unsigned char* row = px + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
            for (int x = 0; x < cW; ++x) {
                const std::uint8_t b = row[x * 4 + 0];
                const std::uint8_t g = row[x * 4 + 1];
                const std::uint8_t r = row[x * 4 + 2];
                if (r > 200 && g > 200 && b > 200) ++nonBg;
            }
        }
        CHECK(nonBg > 10000);
        cairo_surface_destroy(surf);
    }

    // RenderHwndViewportToOffscreen exact size check
    {
        Camera2D vpCam;
        vpCam.panX = -300.0;
        vpCam.panY = 0.0;
        vpCam.zoom = 1.0;
        RasterImageBgra32 vpImg;
        CHECK(renderer.RenderHwndViewportToOffscreen(doc, vpCam, 600, 420, nullptr, vpImg, nullptr));
        CHECK(vpImg.Valid());
        CHECK(vpImg.width == 600 && vpImg.height == 420);
        CHECK(vpImg.pixels.size() == 600u * 420u * 4u);
    }

    // RenderInfiniteCanvasOffscreen exact size check
    {
        DocumentModel cDoc;
        cDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        cDoc.SetCanvasBackground(PaperPattern::Dots);
        AnnotationObject rect;
        rect.kind = ObjectKind::Shape;
        rect.shapeKind = ShapeKind::Rectangle;
        rect.bounds = { 20.0, 20.0, 200.0, 120.0 };
        rect.fillColor = DocColor::RGBA(230, 30, 30, 255);
        cDoc.AddCanvasObject(std::move(rect));

        RasterImageBgra32 cImg;
        CHECK(renderer.RenderInfiniteCanvasOffscreen(cDoc, { 0.0, 0.0, 480.0, 360.0 }, 480, 360, cImg));
        CHECK(cImg.Valid());
        CHECK(cImg.width == 480 && cImg.height == 360);
        CHECK(cImg.pixels.size() == 480u * 360u * 4u);
        std::size_t redPx = 0;
        for (std::size_t i = 0; i < 480u * 360u; ++i) {
            if (cImg.pixels[i * 4 + 2] > 180 && cImg.pixels[i * 4 + 0] < 60) ++redPx;
        }
        CHECK(redPx > 5000);
    }

    // DiscardDeviceResources + re-render sanity check (must not crash or UAF)
    renderer.DiscardDeviceResources();
    RasterImageBgra32 afterDiscard;
    CHECK(renderer.RenderPageOffscreen(doc, 0, 400, nullptr, afterDiscard));
    CHECK(afterDiscard.Valid() && afterDiscard.width == 400);
}

void TestF4RealCasioPdfEditMergeSplitFlatten(const fs::path& manualsDir, const fs::path& tmpDir) {
    std::cout << "-- Test F4: Real CASIO PDF (fx-570_991LA_X_ES.pdf + all manuals) Poppler Render, Split, Rotate, Crop, Flatten, Merge --\n";
    const fs::path srcPdf = JoinUtf8(manualsDir, "fx-570_991LA_X_ES.pdf");
    CHECK(fs::exists(srcPdf));
    if (!fs::exists(srcPdf)) return;

    PdfDocument parser;
    std::string err;
    CHECK(parser.LoadFromFile(PathToW(srcPdf), &err));
    std::cout << "  fx-570_991LA_X_ES.pdf version=" << parser.PdfVersion() << " pages=" << parser.PageCount() << "\n";
    CHECK(parser.PageCount() >= 5);
    if (parser.PageCount() < 5) return;

    int popplerCount = 0;
    double wPt = 0.0, hPt = 0.0;
    CHECK(WinRtPdfRenderer::GetPageCountAndSize(PathToW(srcPdf), 0, popplerCount, wPt, hPt, &err));
    CHECK(popplerCount == static_cast<int>(parser.PageCount()));
    CHECK(wPt > 100.0 && hPt > 100.0);

    // Poppler crop + rotation check on fx-570_991LA_X_ES.pdf
    {
        RasterImageBgra32 fullImg, cropRotImg;
        CHECK(WinRtPdfRenderer::RenderPageToBgra32(PathToW(srcPdf), 0, 640, 0, NormalizedCropRect{}, fullImg, nullptr, &err));
        CHECK(WinRtPdfRenderer::RenderPageToBgra32(
            PathToW(srcPdf), 0, 640, 90, NormalizedCropRect{ 0.1, 0.1, 0.6, 0.8 }, cropRotImg, nullptr, &err));
        CHECK(fullImg.Valid() && cropRotImg.Valid());
        CHECK(cropRotImg.width > cropRotImg.height || fullImg.height > fullImg.width);
    }

    const fs::path splitPath = JoinUtf8(tmpDir, "la_x_split2.pdf");
    CHECK(PdfWriter::SplitPdfFile(PathToW(srcPdf), { 0, 2 }, PathToW(splitPath), &err));

    PdfDocument splitParsed;
    CHECK(splitParsed.LoadFromFile(PathToW(splitPath), &err));
    CHECK(splitParsed.PageCount() == 2);
    if (splitParsed.PageCount() != 2) return;

    DocumentModel editDoc;
    CHECK(splitParsed.PopulateDocumentModel(editDoc, splitPath.string(), &err));

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
    imgAnnot.pngBytes = MakeSolidPng(24, 24, 15, 190, 40);
    CHECK(stack.Execute(editDoc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, imgAnnot)));

    const fs::path flattenedPdf = JoinUtf8(tmpDir, "la_x_flattened.pdf");
    CHECK(PdfWriter::ExportDocumentToFile(editDoc, PathToW(flattenedPdf), &err));

    PdfDocument reParsed;
    CHECK(reParsed.LoadFromFile(PathToW(flattenedPdf), &err));
    CHECK(reParsed.PageCount() == 2);
    if (reParsed.PageCount() >= 2) {
        CHECK(((reParsed.PageAt(0).rotationDeg % 360) + 360) % 360 == 90);
        const auto& p1Box = reParsed.PageAt(1).hasCropBox ? reParsed.PageAt(1).cropBox : reParsed.PageAt(1).mediaBox;
        CHECK(p1Box.w < reParsed.PageAt(1).mediaBox.w * 0.86);
    }

    RasterImageBgra32 popplerRaster;
    CHECK(WinRtPdfRenderer::RenderPageToBgra32(PathToW(flattenedPdf), 0, 1000, 0, NormalizedCropRect{}, popplerRaster, nullptr, &err));
    CHECK(popplerRaster.Valid());
    CHECK(popplerRaster.width > popplerRaster.height);

    std::size_t redInkPixels = 0;
    std::size_t greenImgPixels = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(popplerRaster.width) * static_cast<std::size_t>(popplerRaster.height); ++i) {
        const std::uint8_t b = popplerRaster.pixels[i * 4 + 0];
        const std::uint8_t g = popplerRaster.pixels[i * 4 + 1];
        const std::uint8_t r = popplerRaster.pixels[i * 4 + 2];
        if (r > 180 && g < 80 && b < 80) ++redInkPixels;
        if (g > 150 && r < 60 && b < 80) ++greenImgPixels;
    }
    std::cout << "  Poppler flattened check -> rotated=" << popplerRaster.width << "x" << popplerRaster.height
              << ", red_ink_px=" << redInkPixels << ", green_xobj_px=" << greenImgPixels << "\n";
    CHECK(redInkPixels > 1200);
    CHECK(greenImgPixels > 800);

    const fs::path mergedPdf = JoinUtf8(tmpDir, "la_x_merged.pdf");
    CHECK(PdfWriter::MergePdfFiles({ PathToW(splitPath), PathToW(flattenedPdf) }, PathToW(mergedPdf), &err));
    PdfDocument mergedParsed;
    CHECK(mergedParsed.LoadFromFile(PathToW(mergedPdf), &err));
    CHECK(mergedParsed.PageCount() == 4);

    int manualPdfCount = 0;
    if (fs::exists(manualsDir)) {
        for (const auto& entry : fs::directory_iterator(manualsDir)) {
            if (!entry.is_regular_file()) continue;
            const std::string ext = entry.path().extension().string();
            if (ext != ".pdf" && ext != ".PDF") continue;
            ++manualPdfCount;
            PdfDocument mDoc;
            std::string mErr;
            const bool ok = mDoc.LoadFromFile(PathToW(entry.path()), &mErr);
            std::cout << "  Manual PDF: " << entry.path().filename().string()
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
        const bool pOk = WinRtPdfRenderer::RenderPageToBgra32(PathToW(srcPdf), p, 640, 0, NormalizedCropRect{}, pgImg, nullptr, &pgErr);
        std::size_t nonWhite = 0;
        if (pOk && pgImg.Valid()) {
            for (std::size_t k = 0; k < static_cast<std::size_t>(pgImg.width) * static_cast<std::size_t>(pgImg.height); ++k) {
                if (pgImg.pixels[k * 4 + 0] < 245 || pgImg.pixels[k * 4 + 1] < 245 || pgImg.pixels[k * 4 + 2] < 245) ++nonWhite;
            }
        }
        std::cout << "  Poppler Render fx-570_991LA_X_ES p=" << p << " ok=" << pOk << " valid=" << pgImg.Valid()
                  << " size=" << pgImg.width << "x" << pgImg.height << " nonWhite=" << nonWhite
                  << " err=\"" << pgErr << "\"\n";
        CHECK(pOk && pgImg.Valid());
        CHECK(nonWhite > 100);
    }
}

void TestF5TextExtractionReplacementAndOcr(const fs::path& manualsDir) {
    std::cout << "-- Test F5: PDF Text Extraction, Replacement, DocumentModel::SearchText, Offline Tesseract OCR --\n";
    CHECK(NormalizeLigaturesUtf8("Con\xEF\xAC\x81gura \xEF\xAC\x82ujo") == "Configura flujo");

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

    // SearchText & extraction check on real CASIO fx-570_991LA_X_ES.pdf
    const fs::path laPdf = JoinUtf8(manualsDir, "fx-570_991LA_X_ES.pdf");
    PdfDocument casioDoc;
    CHECK(casioDoc.LoadFromFile(PathToW(laPdf), &err));
    if (casioDoc.PageCount() > 0) {
        std::size_t nonEmptyTextPages = 0;
        for (std::size_t i = 0; i < (std::min<std::size_t>)(casioDoc.PageCount(), 8u); ++i) {
            if (!casioDoc.PageAt(i).extractedTextUtf8.empty()) ++nonEmptyTextPages;
        }
        std::cout << "  fx-570_991LA_X_ES.pdf pages with extracted text in first 8 pages: " << nonEmptyTextPages << "\n";
        const auto honestCap = casioDoc.InspectTextReplacement(0, "ImpossibleNonMatchingSubsetGlyphXYZ");
        CHECK(!honestCap.supportedDirectStream);
        CHECK(honestCap.supportedVisualRedaction);

        DocumentModel model;
        CHECK(casioDoc.PopulateDocumentModel(model, laPdf.string(), &err));
        const auto hits = model.SearchText("CASIO", false);
        std::cout << "  DocumentModel::SearchText(\"CASIO\") matches on fx-570_991LA_X_ES: " << hits.size() << "\n";
        CHECK(!hits.empty());
    }

    // Offline Tesseract OCR test on crisp PangoCairo render
    const auto langs = WinRtOcrAdapter::ListAvailableLanguageTags();
    std::cout << "  Tesseract available languages: " << langs.size() << "\n";
    CHECK(!langs.empty());
    if (!langs.empty()) {
        DocumentModel ocrDoc;
        AnnotationObject headline;
        headline.kind = ObjectKind::TextBox;
        headline.bounds = { 60.0, 120.0, 480.0, 100.0 };
        headline.textUtf8 = "CALCULADORA\nCASIO 2026";
        headline.fontSizePt = 36.0;
        headline.bold = true;
        headline.strokeColor = DocColor::RGBA(0, 0, 0, 255);
        ocrDoc.AddPageObject(0, headline);

        D2DRenderer d2d;
        RasterImageBgra32 img;
        CHECK(d2d.RenderPageOffscreen(ocrDoc, 0, 1200, nullptr, img));
        const OcrOutput ocr = WinRtOcrAdapter::RecognizeBgra32(img.width, img.height, img.pixels.data(), "en-US");
        std::cout << "  OCR output (" << ocr.languageTagUtf8 << "): \"" << ocr.fullTextUtf8
                  << "\", words=" << ocr.words.size() << "\n";
        CHECK(ocr.engineAvailable);
        CHECK(!ocr.words.empty());
        CHECK(ocr.fullTextUtf8.find("CASIO") != std::string::npos ||
              ocr.fullTextUtf8.find("CALCULADORA") != std::string::npos);

        // Also run OCR on rendered page 0 of fx-570_991LA_X_ES.pdf
        RasterImageBgra32 laPage0;
        if (WinRtPdfRenderer::RenderPageToBgra32(PathToW(laPdf), 0, 1000, 0, NormalizedCropRect{}, laPage0, nullptr, &err)) {
            const OcrOutput pdfOcr = WinRtOcrAdapter::RecognizeBgra32(laPage0.width, laPage0.height, laPage0.pixels.data());
            std::cout << "  fx-570_991LA_X_ES page 0 OCR words=" << pdfOcr.words.size() << "\n";
            CHECK(pdfOcr.engineAvailable);
            CHECK(!pdfOcr.fullTextUtf8.empty());
        }
    }
}

void TestF6InfiniteCanvas100kQuadtree(const fs::path& tmpDir) {
    std::cout << "-- Test F6: Infinite Canvas Quadtree 100,000 Objects Benchmark & Export --\n";
    DocumentModel canvasDoc;
    canvasDoc.ResetToBlank(DocumentMode::InfiniteCanvas);

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

    const fs::path canvasPdf = JoinUtf8(tmpDir, "canvas_region.pdf");
    CanvasPdfExportOptions opts;
    opts.paginated = false;
    opts.hasCustomRegion = true;
    opts.customRegionWorld = query;
    std::string err;
    CHECK(PdfWriter::ExportInfiniteCanvasToFile(canvasDoc, opts, PathToW(canvasPdf), &err));

    RasterImageBgra32 img;
    CHECK(WinRtPdfRenderer::RenderPageToBgra32(PathToW(canvasPdf), 0, 800, 0, NormalizedCropRect{}, img, nullptr, &err));
    CHECK(img.Valid());
}

void TestF7Pkcs7SignatureAndOpenXml(const fs::path& manualsDir, const fs::path& tmpDir) {
    std::cout << "-- Test F7: OpenSSL CMS/PKCS#7 SHA-256 PDF Signature + PFX + Tamper Rejection + DOCX/PPTX Round-trip --\n";
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

    // 1. Self-signed ExportAndSignDocument + VerifyPdfFile
    const fs::path signedPdf = JoinUtf8(tmpDir, "signed_test.pdf");
    PdfSignatureOptions sopts;
    sopts.signerNameUtf8 = "Ing. Prueba";
    sopts.reasonUtf8 = "Validacion Cryptografica";
    sopts.addVisualStamp = true;
    std::string err;
    CHECK(PdfSigner::ExportAndSignDocument(doc, sopts, PathToW(signedPdf), L"", L"", &err));

    const auto vGood = PdfSigner::VerifyPdfFile(PathToW(signedPdf));
    std::cout << "  Self-signed Verify -> hasSig=" << vGood.hasSignature << " byteRangeOk=" << vGood.byteRangeValid
              << " cryptoValid=" << vGood.cryptographicValid << " subj=\"" << vGood.signerSubjectUtf8 << "\"\n";
    CHECK(vGood.hasSignature);
    CHECK(vGood.byteRangeValid);
    CHECK(vGood.cryptographicValid);
    CHECK(vGood.signerSubjectUtf8 == "Ing. Prueba");
    CHECK(vGood.byteRange.size() == 4);

    // 2. Tamper body byte -> MUST fail cryptographic verification
    std::vector<std::uint8_t> tampered;
    CHECK(ReadFileBytes(signedPdf, tampered));
    const std::string asStr(reinterpret_cast<const char*>(tampered.data()), tampered.size());
    const std::size_t objPos = asStr.find("endobj");
    CHECK(objPos != std::string::npos && objPos > 10);
    tampered[objPos - 4] ^= 0x01u;
    const auto vTampered = PdfSigner::VerifyPdfBytes(tampered);
    CHECK(vTampered.hasSignature);
    CHECK(vTampered.byteRangeValid);
    CHECK(!vTampered.cryptographicValid);

    // 3. Tamper trailing byte -> MUST fail /ByteRange length check
    std::vector<std::uint8_t> truncated;
    CHECK(ReadFileBytes(signedPdf, truncated));
    truncated.push_back(' ');
    const auto vRangeBad = PdfSigner::VerifyPdfBytes(truncated);
    CHECK(vRangeBad.hasSignature);
    CHECK(!vRangeBad.byteRangeValid);
    CHECK(!vRangeBad.cryptographicValid);

    // 4. SignPdfBytesWithPfx using OpenSSL-generated PKCS#12 + Verify
    const fs::path pfxPath = JoinUtf8(tmpDir, "test_signer.pfx");
    CHECK(CreateTestPfxFile(pfxPath, "clave1234", "CASIO PFX Test Cert"));
    std::vector<std::uint8_t> basePdf;
    CHECK(PdfWriter::ExportDocumentToBytes(doc, basePdf, &err));
    std::vector<std::uint8_t> pfxSignedBytes;
    CHECK(PdfSigner::SignPdfBytesWithPfx(basePdf, PathToW(pfxPath), L"clave1234", sopts, pfxSignedBytes, &err));
    const auto vPfx = PdfSigner::VerifyPdfBytes(pfxSignedBytes);
    CHECK(vPfx.hasSignature && vPfx.byteRangeValid && vPfx.cryptographicValid);
    CHECK(vPfx.signerSubjectUtf8 == "CASIO PFX Test Cert");
    std::vector<std::uint8_t> badPwdSigned;
    CHECK(!PdfSigner::SignPdfBytesWithPfx(basePdf, PathToW(pfxPath), L"wrong_password", sopts, badPwdSigned, &err));

    // 5. Sign real fx-570_991LA_X_ES.pdf split PDF + verify with both PdfSigner and Poppler
    const fs::path splitPath = JoinUtf8(tmpDir, "la_x_split2.pdf");
    if (fs::exists(splitPath)) {
        std::vector<std::uint8_t> realBytes;
        CHECK(ReadFileBytes(splitPath, realBytes));
        std::vector<std::uint8_t> realSigned;
        CHECK(PdfSigner::SignPdfBytesWithSelfSigned(realBytes, sopts, realSigned, &err));
        const auto vReal = PdfSigner::VerifyPdfBytes(realSigned);
        CHECK(vReal.hasSignature && vReal.byteRangeValid && vReal.cryptographicValid);
    }
    (void)manualsDir;

    // 6. Bidirectional OpenXML .docx & .pptx round-trip
    const fs::path docxPath = JoinUtf8(tmpDir, "exportado.docx");
    const fs::path pptxPath = JoinUtf8(tmpDir, "exportado.pptx");
    std::vector<std::vector<std::uint8_t>> snaps = { MakeSolidPng(120, 160, 200, 220, 240) };
    CHECK(OpenXmlInterop::ExportToDocxFile(doc, PathToW(docxPath), snaps, &err));
    CHECK(OpenXmlInterop::ExportToPptxFile(doc, PathToW(pptxPath), snaps, &err));

    DocumentModel fromDocx, fromPptx;
    CHECK(OpenXmlInterop::ImportFromDocxFile(PathToW(docxPath), fromDocx, &err));
    CHECK(OpenXmlInterop::ImportFromPptxFile(PathToW(pptxPath), fromPptx, &err));
    CHECK(!fromDocx.Pages().empty());
    CHECK(fromDocx.Pages()[0].extractedTextUtf8.find("fx-991CW") != std::string::npos);
    CHECK(!fromPptx.Pages().empty());
    CHECK(fromPptx.Pages()[0].extractedTextUtf8.find("fx-991CW") != std::string::npos);
}

void TestF8P0AuditRegressionFixes(const fs::path& manualsDir, const fs::path& tmpDir) {
    std::cout << "-- Test F8: P0 Audit Regression Fixes (Quadtree Bounds, Unicode Search/PDF, ZIP Budget/Path, Embedded PDF, >200 Pages, Catalog) --\n";

    // 1. Quadtree dynamic expansion for extreme coordinates outside initial bounds
    {
        DocumentModel extremeDoc;
        extremeDoc.ResetToBlank(DocumentMode::InfiniteCanvas);

        AnnotationObject farNeg;
        farNeg.zOrder = 1;
        farNeg.kind = ObjectKind::Shape;
        farNeg.shapeKind = ShapeKind::Rectangle;
        farNeg.bounds = { -5.0e7, -5.0e7, 200.0, 120.0 };
        extremeDoc.AddCanvasObject(farNeg);
        const std::uint64_t negId = extremeDoc.CanvasObjects().back().id;

        AnnotationObject farPos;
        farPos.zOrder = 2;
        farPos.kind = ObjectKind::Shape;
        farPos.shapeKind = ShapeKind::Rectangle;
        farPos.bounds = { 8.0e7, 8.0e7, 200.0, 120.0 };
        extremeDoc.AddCanvasObject(farPos);
        const std::uint64_t posId = extremeDoc.CanvasObjects().back().id;

        const auto negHits = extremeDoc.QueryCanvasRect({ -5.0e7 - 100.0, -5.0e7 - 100.0, 500.0, 500.0 });
        const auto posHits = extremeDoc.QueryCanvasRect({ 8.0e7 - 100.0, 8.0e7 - 100.0, 500.0, 500.0 });
        CHECK(std::find(negHits.begin(), negHits.end(), negId) != negHits.end());
        CHECK(std::find(posHits.begin(), posHits.end(), posId) != posHits.end());
    }

    // 2. Unicode case-folding & accent-insensitive SearchText
    {
        DocumentModel searchDoc;
        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 40.0, 40.0, 500.0, 80.0 };
        tb.textUtf8 = "C\xC3\xA1lculo de \xC3\x81NGULO \xCE\xA3\xCE\xB8 y \xC3\xB1""and\xC3\xBA";
        searchDoc.AddPageObject(0, tb);

        CHECK(!searchDoc.SearchText("\xC3\xA1ngulo", false, false).empty());
        CHECK(!searchDoc.SearchText("\xCF\x83\xCE\xB8", false, false).empty());
        CHECK(!searchDoc.SearchText("calculo", false, true).empty());
        CHECK(!searchDoc.SearchText("nandu", false, true).empty());
        CHECK(searchDoc.SearchText("CALCULO", true, false).empty());
    }

    // 3. ZIP traversal rejection & cumulative 512 MiB budget enforcement
    {
        ZipArchive za;
        za.SetTextEntry("safe_dir/entry123.txt", "hello");
        std::vector<std::uint8_t> zipBytes;
        std::string zErr;
        CHECK(za.Serialize(zipBytes, false, &zErr));
        const std::string origName = "safe_dir/entry123.txt";
        const std::string evilName = "../a_evil_entry12.txt";
        bool patched = false;
        for (std::size_t i = 0; i + origName.size() <= zipBytes.size(); ++i) {
            if (std::memcmp(zipBytes.data() + i, origName.data(), origName.size()) == 0) {
                std::memcpy(zipBytes.data() + i, evilName.data(), evilName.size());
                patched = true;
            }
        }
        CHECK(patched);
        ZipArchive badZa;
        CHECK(!badZa.Deserialize(zipBytes.data(), zipBytes.size(), &zErr));
    }
    {
        ZipArchive bombZa;
        std::vector<std::uint8_t> zeros(64ull * 1024ull * 1024ull, 0u);
        for (int i = 0; i < 9; ++i) {
            bombZa.SetEntry("z_" + std::to_string(i) + ".bin", zeros);
        }
        zeros.clear();
        zeros.shrink_to_fit();
        std::vector<std::uint8_t> bombZip;
        std::string bErr;
        CHECK(bombZa.Serialize(bombZip, true, &bErr));
        bombZa = ZipArchive{};
        ZipArchive parsedBomb;
        CHECK(!parsedBomb.Deserialize(bombZip.data(), bombZip.size(), &bErr));
    }

    // 4. Unicode math symbols preserved in exported PDF & rendered with dark pixels
    {
        DocumentModel mathDoc;
        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 50.0, 100.0, 480.0, 80.0 };
        tb.fontSizePt = 18.0;
        tb.strokeColor = DocColor::RGBA(0, 0, 0, 255);
        tb.textUtf8 = "Int \xE2\x88\xAB \xE2\x88\x91 \xCE\xB8 \xE2\x88\x9A \xCE\xA9 \xCF\x80 \xE2\x89\xA5 \xE2\x89\xA4 \xE2\x89\xA0 \xE2\x84\x9D";
        mathDoc.AddPageObject(0, tb);

        const fs::path mathPdf = JoinUtf8(tmpDir, "f8_unicode_math.pdf");
        std::string err;
        CHECK(PdfWriter::ExportDocumentToFile(mathDoc, PathToW(mathPdf), &err));

        PdfDocument reParsed;
        CHECK(reParsed.LoadFromFile(PathToW(mathPdf), &err));
        CHECK(reParsed.PageCount() == 1);
        if (reParsed.PageCount() >= 1) {
            const std::string& txt = reParsed.PageAt(0).extractedTextUtf8;
            CHECK(txt.find('?') == std::string::npos);
            CHECK(txt.find("\xE2\x88\xAB") != std::string::npos);
            CHECK(txt.find("\xCE\xB8") != std::string::npos);
            CHECK(txt.find("\xE2\x88\x9A") != std::string::npos);
            CHECK(txt.find("\xCF\x80") != std::string::npos);
        }

        RasterImageBgra32 img;
        CHECK(WinRtPdfRenderer::RenderPageToBgra32(
            PathToW(mathPdf), 0, 900, 0, NormalizedCropRect{}, img, nullptr, &err));
        std::size_t darkPx = 0;
        if (img.Valid()) {
            for (std::size_t i = 0; i < static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height); ++i) {
                if (img.pixels[i * 4 + 0] < 120 && img.pixels[i * 4 + 1] < 120 && img.pixels[i * 4 + 2] < 120) ++darkPx;
            }
        }
        CHECK(darkPx > 200);
    }

    // 5. Embedded source PDF survives source deletion in .cudoc + honest failure when source is missing
    const fs::path laPdf = JoinUtf8(manualsDir, "fx-570_991LA_X_ES.pdf");
    if (fs::exists(laPdf)) {
        const fs::path tempSrcPdf = JoinUtf8(tmpDir, "temp_la_source.pdf");
        fs::copy_file(laPdf, tempSrcPdf, fs::copy_options::overwrite_existing);

        PdfDocument srcParsed;
        std::string err;
        CHECK(srcParsed.LoadFromFile(PathToW(tempSrcPdf), &err));
        const std::size_t origPages = srcParsed.PageCount();
        DocumentModel model;
        CHECK(srcParsed.PopulateDocumentModel(model, tempSrcPdf.string(), &err));

        std::vector<std::uint8_t> cudocZip;
        CHECK(CudocStorage::SerializeToBytes(model, cudocZip, &err));

        fs::remove(tempSrcPdf);
        CHECK(!fs::exists(tempSrcPdf));

        DocumentModel restored;
        CHECK(CudocStorage::DeserializeFromBytes(cudocZip.data(), cudocZip.size(), restored, &err));
        const fs::path recoveredPdf = JoinUtf8(tmpDir, "f8_recovered_from_cudoc.pdf");
        CHECK(PdfWriter::ExportDocumentToFile(restored, PathToW(recoveredPdf), &err));

        PdfDocument recParsed;
        CHECK(recParsed.LoadFromFile(PathToW(recoveredPdf), &err));
        CHECK(recParsed.PageCount() == origPages);
    }
    {
        DocumentModel missingSrcDoc;
        PageModel pm;
        pm.pageId = 999;
        pm.hasPdfSource = true;
        pm.sourcePdfPathUtf8 = JoinUtf8(tmpDir, "definitely_missing_12345.pdf").string();
        pm.sourcePageIndex0 = 0;
        pm.widthPt = 595.0;
        pm.heightPt = 842.0;
        CommandStack tmpStack;
        CHECK(tmpStack.Execute(missingSrcDoc, std::make_unique<InsertPageCommand>(1, pm)));
        CHECK(tmpStack.Execute(missingSrcDoc, std::make_unique<DeletePageCommand>(0)));
        const fs::path outFailPdf = JoinUtf8(tmpDir, "f8_should_fail.pdf");
        std::string err;
        CHECK(!PdfWriter::ExportDocumentToFile(missingSrcDoc, PathToW(outFailPdf), &err));
    }

    // 6. Canvas PdfCard real content export + >200 paginated canvas export
    if (fs::exists(laPdf)) {
        DocumentModel cardDoc;
        cardDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        AnnotationObject card;
        card.kind = ObjectKind::PdfCard;
        card.pdfSourcePathUtf8 = laPdf.string();
        card.pdfSourcePageIndex0 = 0;
        card.bounds = { 50.0, 50.0, 320.0, 440.0 };
        cardDoc.AddCanvasObject(card);

        const fs::path cardPdf = JoinUtf8(tmpDir, "f8_canvas_pdfcard.pdf");
        CanvasPdfExportOptions copts;
        copts.paginated = false;
        copts.hasCustomRegion = true;
        copts.customRegionWorld = { 0.0, 0.0, 420.0, 540.0 };
        std::string err;
        CHECK(PdfWriter::ExportInfiniteCanvasToFile(cardDoc, copts, PathToW(cardPdf), &err));

        RasterImageBgra32 img;
        CHECK(WinRtPdfRenderer::RenderPageToBgra32(
            PathToW(cardPdf), 0, 800, 0, NormalizedCropRect{}, img, nullptr, &err));
        std::size_t nonWhite = 0;
        if (img.Valid()) {
            for (std::size_t i = 0; i < static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height); ++i) {
                if (img.pixels[i * 4 + 0] < 235 || img.pixels[i * 4 + 1] < 235 || img.pixels[i * 4 + 2] < 235) ++nonWhite;
            }
        }
        CHECK(nonWhite > 2000);
    }
    {
        DocumentModel bigCanvas;
        bigCanvas.ResetToBlank(DocumentMode::InfiniteCanvas);
        for (int i = 0; i < 205; ++i) {
            AnnotationObject sh;
            sh.kind = ObjectKind::Shape;
            sh.shapeKind = ShapeKind::Rectangle;
            sh.strokeColor = DocColor::RGBA(37, 99, 235, 255);
            sh.bounds = { i * 620.0 + 40.0, 40.0, 80.0, 40.0 };
            bigCanvas.AddCanvasObject(sh);
        }
        const fs::path bigPdf = JoinUtf8(tmpDir, "f8_canvas_205p.pdf");
        CanvasPdfExportOptions pOpts;
        pOpts.paginated = true;
        pOpts.pageWidthPt = 595.0;
        pOpts.pageHeightPt = 842.0;
        pOpts.maxPages = 0;
        std::string err;
        CHECK(PdfWriter::ExportInfiniteCanvasToFile(bigCanvas, pOpts, PathToW(bigPdf), &err));

        PdfDocument bigParsed;
        CHECK(bigParsed.LoadFromFile(PathToW(bigPdf), &err));
        CHECK(bigParsed.PageCount() >= 205);
    }

    // 7. Catalog preservation during TryDirectReplaceTextInPdfBytes (/Lang preserved)
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
        CHECK(PdfWriter::TryDirectReplaceTextInPdfBytes(
            pdfBytes, 0, "Alpha Beta Target", "Gamma Delta Done", outBytes, &err));
        const std::string outStr(outBytes.begin(), outBytes.end());
        CHECK(outStr.find("/Lang") != std::string::npos);
        CHECK(outStr.find("es-CR") != std::string::npos);
        PdfDocument outParsed;
        CHECK(outParsed.LoadFromBytes(outBytes, &err));
        CHECK(outParsed.CatalogRef().obj > 0);
        CHECK(outParsed.PageAt(0).extractedTextUtf8.find("Gamma Delta Done") != std::string::npos);
    }
}

void TestF9P1P2GoodnotesOneNoteParity(const fs::path& tmpDir) {
    std::cout << "-- Test F9: Ink Smoothing/Snap/Scribble/Lasso/Anchors, Layers, StudyTape, Vault/WAL/FTS/SM-2, Audio Sync --\n";

    // 1. Ink Smoothing (<0.30 vs >=0.30 Catmull-Rom) & Tilt metrics
    {
        std::vector<InkPoint> raw;
        for (int i = 0; i < 8; ++i) {
            raw.push_back({ i * 15.0, (i % 2 == 0 ? 40.0 : 46.0), 0.7f, 20.0f, 10.0f, static_cast<std::uint64_t>(i * 20) });
        }
        const auto fastPts = InkEngine::SmoothInkStroke(raw, 0.15);
        const auto smoothPts = InkEngine::SmoothInkStroke(raw, 0.75);
        CHECK(fastPts.size() == raw.size());
        CHECK(smoothPts.size() > raw.size());
        AnnotationObject dummy;
        dummy.penStyle = InkPenStyle::Pencil;
        dummy.strokeWidth = 3.0;
        CHECK(InkEngine::ComputeEffectiveWidth(raw[0], dummy, 200.0) >= 3.0);
        CHECK(InkEngine::ComputeTiltAlphaMultiplier(raw[0], InkPenStyle::Pencil) < 1.0);
    }

    // 2. Hold-to-Snap Shapes: Line, Arrow, Ellipse, Rectangle
    {
        std::vector<InkPoint> linePts;
        for (int i = 0; i <= 10; ++i) linePts.push_back({ 20.0 + i * 20.0, 50.0 + (i % 2) * 1.5, 0.6f });
        const auto sLine = InkEngine::RecognizeHeldShape(linePts);
        CHECK(sLine.recognized && sLine.kind == ShapeKind::Line);

        std::vector<InkPoint> arrowPts;
        for (int i = 0; i <= 14; ++i) arrowPts.push_back({ 20.0 + i * 10.0, 100.0, 0.6f });
        arrowPts.push_back({ 140.0, 86.0, 0.6f });
        arrowPts.push_back({ 160.0, 100.0, 0.6f });
        arrowPts.push_back({ 140.0, 114.0, 0.6f });
        const auto sArrow = InkEngine::RecognizeHeldShape(arrowPts);
        CHECK(sArrow.recognized && sArrow.kind == ShapeKind::Arrow);

        std::vector<InkPoint> circlePts;
        for (int i = 0; i <= 24; ++i) {
            const double th = (static_cast<double>(i) / 24.0) * 6.283185307;
            circlePts.push_back({ 200.0 + 50.0 * std::cos(th), 200.0 + 50.0 * std::sin(th), 0.7f });
        }
        const auto sCircle = InkEngine::RecognizeHeldShape(circlePts);
        CHECK(sCircle.recognized && sCircle.kind == ShapeKind::Ellipse);

        std::vector<InkPoint> rectPts;
        for (int i = 0; i < 6; ++i) rectPts.push_back({ 50.0 + i * 20.0, 50.0, 0.7f });
        for (int i = 0; i < 6; ++i) rectPts.push_back({ 150.0, 50.0 + i * 20.0, 0.7f });
        for (int i = 0; i < 6; ++i) rectPts.push_back({ 150.0 - i * 20.0, 150.0, 0.7f });
        for (int i = 0; i <= 6; ++i) rectPts.push_back({ 50.0, 150.0 - i * 20.0, 0.7f });
        const auto sRect = InkEngine::RecognizeHeldShape(rectPts);
        CHECK(sRect.recognized && sRect.kind == ShapeKind::Rectangle);
    }

    // 3. Scribble-to-Erase & Circle-to-Lasso & Anchored Ink
    {
        DocumentModel d;
        d.ResetToBlank(DocumentMode::InfiniteCanvas);

        AnnotationObject s1;
        s1.kind = ObjectKind::InkStroke;
        s1.inkPoints = { { 100.0, 100.0, 0.8f }, { 180.0, 100.0, 0.8f } };
        d.AddCanvasObject(s1);
        const std::uint64_t s1Id = d.CanvasObjects().back().id;

        AnnotationObject parentTb;
        parentTb.kind = ObjectKind::TextBox;
        parentTb.bounds = { 320.0, 320.0, 140.0, 50.0 };
        parentTb.textUtf8 = "Ancla padre";
        d.AddCanvasObject(parentTb);
        const std::uint64_t parentId = d.CanvasObjects().back().id;

        AnnotationObject anchoredChild;
        anchoredChild.kind = ObjectKind::InkStroke;
        anchoredChild.anchorObjectId = parentId;
        anchoredChild.inkPoints = { { 330.0, 390.0, 0.8f }, { 420.0, 390.0, 0.8f } };
        d.AddCanvasObject(anchoredChild);
        const std::uint64_t childId = d.CanvasObjects().back().id;

        std::vector<InkPoint> scribble;
        for (int i = 0; i < 10; ++i) {
            scribble.push_back({ 105.0 + i * 7.0, (i % 2 == 0) ? 80.0 : 120.0, 0.7f });
        }
        CHECK(InkEngine::IsScribbleEraseGesture(scribble));
        const auto eraseIds = InkEngine::FindObjectsForScribbleErase(d, { true, 0 }, scribble);
        CHECK(std::find(eraseIds.begin(), eraseIds.end(), s1Id) != eraseIds.end());
        CHECK(std::find(eraseIds.begin(), eraseIds.end(), parentId) == eraseIds.end());

        std::vector<InkPoint> lasso;
        for (int i = 0; i <= 24; ++i) {
            const double th = (static_cast<double>(i) / 24.0) * 6.283185307;
            lasso.push_back({ 390.0 + 130.0 * std::cos(th), 355.0 + 110.0 * std::sin(th), 0.5f });
        }
        CHECK(InkEngine::IsClosedLassoLoop(lasso));
        const auto lassoIds = InkEngine::SelectObjectsInsideLasso(d, { true, 0 }, lasso);
        CHECK(std::find(lassoIds.begin(), lassoIds.end(), parentId) != lassoIds.end());
        CHECK(std::find(lassoIds.begin(), lassoIds.end(), childId) != lassoIds.end());

        InkEngine::UpdateAnchoredChildrenAfterMove(d, { true, 0 }, parentId, 120.0, -45.0);
        const auto* movedChild = d.FindCanvasObject(childId);
        CHECK(movedChild != nullptr && std::abs(movedChild->inkPoints[0].x - 450.0) < 1e-6);
        CHECK(movedChild != nullptr && std::abs(movedChild->inkPoints[0].y - 345.0) < 1e-6);
    }

    // 4. Layers + Non-Exportable Layer + StudyTape + Links + Collapse + .cudoc round-trip + Audio Timeline
    {
        DocumentModel doc;
        const std::uint64_t hiddenExportLayer = doc.AddLayer("Solo Pantalla");
        CHECK(doc.SetLayerProperties(hiddenExportLayer, true, false, false, 0.85));

        AnnotationObject draftNote;
        draftNote.kind = ObjectKind::TextBox;
        draftNote.layerId = hiddenExportLayer;
        draftNote.bounds = { 50.0, 50.0, 260.0, 40.0 };
        draftNote.textUtf8 = "SECRETO_NO_EXPORTAR";
        doc.AddPageObject(0, draftNote);

        AnnotationObject tape;
        tape.kind = ObjectKind::StudyTape;
        tape.bounds = { 50.0, 120.0, 280.0, 45.0 };
        tape.captionUtf8 = "Ley de Ohm";
        tape.textUtf8 = "V = I * R";
        tape.tapeRevealed = false;
        doc.AddPageObject(0, tape);

        InkLink link;
        link.sourceDocPathUtf8 = "manuales/fx-570_991LA_X_ES.pdf";
        link.sourcePageIndex0 = 4;
        link.targetIsCanvas = true;
        link.labelUtf8 = "Referencia S<=>D";
        doc.AddInkLink(link);
        doc.ToggleCollapsedRange(2, 4);
        CHECK(doc.IsPageCollapsed(3));

        AudioTimelineSession audio;
        audio.StartRecording(16000);
        audio.AppendSyntheticTone(440.0, 400);
        audio.StopRecording();
        CHECK(audio.DurationMs() >= 390 && audio.DurationMs() <= 410);
        const auto envBins = audio.ComputeEnvelopeBins(16);
        CHECK(envBins.size() == 16 && envBins[4] > 0.05f);
        std::vector<std::uint8_t> wavBytes;
        CHECK(audio.ExportWavBytes(wavBytes));
        AudioTimelineSession decoded;
        CHECK(decoded.LoadFromWavBytes(wavBytes.data(), wavBytes.size()));
        CHECK(decoded.DurationMs() == audio.DurationMs());

        doc.SetNamedAsset("rec_01.wav", std::move(wavBytes));

        AnnotationObject timedStroke;
        timedStroke.kind = ObjectKind::InkStroke;
        timedStroke.inkPoints = {
            { 60.0, 220.0, 0.8f, 0.0f, 0.0f, 100ull },
            { 140.0, 220.0, 0.8f, 0.0f, 0.0f, 200ull },
            { 220.0, 220.0, 0.8f, 0.0f, 0.0f, 350ull }
        };
        timedStroke.inkStartTimeMs = 100;
        timedStroke.inkEndTimeMs = 350;
        doc.AddPageObject(0, timedStroke);
        CHECK(InkEngine::FilterInkPointsAtReplayTime(timedStroke, 210ull).size() == 2);

        const fs::path cudocPath = JoinUtf8(tmpDir, "f9_notebook.cudoc");
        std::string err;
        CHECK(CudocStorage::SaveToFile(PathToW(cudocPath), doc, &err));

        DocumentModel reloaded;
        CHECK(CudocStorage::LoadFromFile(PathToW(cudocPath), reloaded, &err));
        CHECK(reloaded.Layers().size() == 2);
        CHECK(!reloaded.IsLayerExportable(hiddenExportLayer));
        CHECK(reloaded.InkLinks().size() == 1);
        CHECK(reloaded.IsPageCollapsed(3));
        CHECK(reloaded.FindAsset("rec_01.wav") != nullptr);

        std::vector<std::uint8_t> pdfOut;
        CHECK(PdfWriter::ExportDocumentToBytes(reloaded, pdfOut, &err));
        PdfDocument pdfParsed;
        CHECK(pdfParsed.LoadFromBytes(pdfOut, &err));
        CHECK(pdfParsed.PageAt(0).extractedTextUtf8.find("SECRETO_NO_EXPORTAR") == std::string::npos);
        CHECK(pdfParsed.PageAt(0).extractedTextUtf8.find("Ley de Ohm") != std::string::npos);

        NotebookVault vault;
        const auto nbId = vault.CreateNotebook("Circuitos II");
        const auto secId = vault.CreateSection(nbId, "Fasores");
        const auto pgId = vault.AddPageEntry(nbId, secId, "Impedancia", "notas/imp.cudoc");
        CHECK(nbId > 0 && secId > 0 && pgId > 0);
        const fs::path vaultPath = JoinUtf8(tmpDir, "vault.json");
        CHECK(vault.SaveToFile(PathToW(vaultPath), &err));
        NotebookVault vaultLoad;
        CHECK(vaultLoad.LoadFromFile(PathToW(vaultPath), &err));
        CHECK(vaultLoad.Notebooks().size() == 1 && vaultLoad.Notebooks()[0].sections[0].pages.size() == 1);

        const fs::path walPath = JoinUtf8(tmpDir, "f9_notebook.cudoc-wal");
        CHECK(CudocWal::AppendRecord(PathToW(walPath), { "set_title", R"({"title":"Recuperado WAL"})" }, &err));
        CHECK(CudocWal::AppendRecord(PathToW(walPath), { "add_obj", R"({"isCanvas":false,"pageIndex":0,"kind":2,"text":"Fasor impedancia complejo","x":60,"y":280,"w":220,"h":40})" }, &err));
        {
            std::ofstream ofs(walPath, std::ios::binary | std::ios::app);
            const char junk[8] = { 'C', 'W', 'A', 'L', 0x50, 0x00, 0x00, 0x00 };
            ofs.write(junk, sizeof(junk));
        }
        DocumentModel walDoc;
        std::size_t replayed = 0;
        CHECK(CudocWal::RecoverDocument(PathToW(cudocPath), PathToW(walPath), walDoc, &replayed, &err));
        CHECK(replayed == 2 && walDoc.Title() == "Recuperado WAL");

        VaultFullTextIndex fts;
        fts.IndexDocument("notas/imp.cudoc", walDoc);
        const auto ftsHits = fts.Query("fasor impedancia");
        CHECK(!ftsHits.empty());

        auto deck = StudySetManager::BuildDeckFromDocument(reloaded);
        CHECK(!deck.cards.empty());
        StudySetManager::ReviewCardSm2(deck.cards[0], 5, 1000000ull);
        CHECK(deck.cards[0].repetitions == 1 && deck.cards[0].intervalDays == 1);
        StudySetManager::ReviewCardSm2(deck.cards[0], 5, 1086400ull);
        CHECK(deck.cards[0].repetitions == 2 && deck.cards[0].intervalDays == 6);
    }
}

void TestF10P3StemEngineeringSuperiority(const fs::path& tmpDir) {
    std::cout << "-- Test F10: #varsub Dimensional CAS, Phasors, StepChecker, Plots/CSV, MNA AC/DC/OpAmp/Bode, K-Map, Casio Card --\n";

    // 1. Strict dimensional SI/imperial analysis & unit conversion
    {
        const auto ohm = Cas::EvaluateExpression("V = (4.7 kOhm) * (2 mA) | mV");
        CHECK(ohm.ok && ohm.hasAssignment && ohm.assignedVarUtf8 == "V");
        CHECK(std::abs(ohm.result.value.real() - 9.4) < 1e-9);
        CHECK(ohm.formattedResultUtf8.find("9400") != std::string::npos && ohm.formattedResultUtf8.find("mV") != std::string::npos);

        const auto speed = Cas::EvaluateExpression("100 km/h | m/s");
        CHECK(speed.ok && std::abs(speed.result.value.real() - (100000.0 / 3600.0)) < 1e-6);

        const auto phasor = Cas::EvaluateExpression("I = (10 @ 30 V) / (5 @ -15 Ohm)");
        CHECK(phasor.ok && std::abs(phasor.result.Mag() - 2.0) < 1e-6);
        CHECK(std::abs(phasor.result.PhaseDeg() - 45.0) < 1e-4);

        CHECK(!Cas::EvaluateExpression("4.7 kOhm + 2 mA").ok);
        CHECK(!Cas::EvaluateExpression("12 V | s").ok);
    }

    // 2. Reactive #varsub DAG out-of-order topological evaluation & cycle rejection
    {
        DocumentModel mathDoc;
        mathDoc.ResetToBlank(DocumentMode::InfiniteCanvas);

        AnnotationObject nodeP;
        nodeP.kind = ObjectKind::MathBlock;
        nodeP.bounds = { 40.0, 200.0, 320.0, 70.0 };
        nodeP.formulaVarNameUtf8 = "P";
        nodeP.formulaExprUtf8 = "P = V * I | mW";
        mathDoc.AddCanvasObject(nodeP);
        const std::uint64_t pId = mathDoc.CanvasObjects().back().id;

        AnnotationObject nodeV;
        nodeV.kind = ObjectKind::MathBlock;
        nodeV.bounds = { 40.0, 40.0, 260.0, 60.0 };
        nodeV.formulaVarNameUtf8 = "V";
        nodeV.formulaExprUtf8 = "V = 5 V";
        mathDoc.AddCanvasObject(nodeV);

        AnnotationObject nodeI;
        nodeI.kind = ObjectKind::MathBlock;
        nodeI.bounds = { 40.0, 120.0, 260.0, 60.0 };
        nodeI.formulaVarNameUtf8 = "I";
        nodeI.formulaExprUtf8 = "I = 2 mA";
        mathDoc.AddCanvasObject(nodeI);

        const auto dag = Cas::EvaluateDocumentMathDag(mathDoc);
        CHECK(dag.ok && dag.evaluatedCount == 3);
        const auto* resP = mathDoc.FindCanvasObject(pId);
        CHECK(resP != nullptr && resP->evaluatedResultUtf8.find("10 mW") != std::string::npos);

        DocumentModel cycDoc;
        AnnotationObject a, b;
        a.kind = ObjectKind::MathBlock; a.formulaVarNameUtf8 = "A"; a.formulaExprUtf8 = "A = B + 1";
        b.kind = ObjectKind::MathBlock; b.formulaVarNameUtf8 = "B"; b.formulaExprUtf8 = "B = A + 1";
        cycDoc.AddPageObject(0, a);
        cycDoc.AddPageObject(0, b);
        const auto cycRep = Cas::EvaluateDocumentMathDag(cycDoc);
        CHECK(!cycRep.ok && cycRep.cycleCount == 2);
    }

    // 3. Algebraic step checker & 2D/3D plots & CSV OLS regression
    {
        const auto sOk = Cas::CheckAlgebraicStep("(x + 1)^2", "x^2 + 2*x + 1");
        CHECK(sOk.equivalent && sOk.statusCode == "OK");

        const auto sDiff = Cas::CheckAlgebraicStep("(x + 1)^2", "x^2 + 1");
        CHECK(!sDiff.equivalent && sDiff.statusCode == "DIFF");

        const auto sDim = Cas::CheckAlgebraicStep("5 m", "5 s");
        CHECK(!sDim.equivalent && sDim.dimensionMismatch && sDim.statusCode == "DIM_ERR");

        const auto curve = Cas::SampleFunction2D("sin(x)", "x", 0.0, 3.14159265, 32);
        CHECK(curve.size() == 32 && std::abs(curve[15].y - 1.0) < 0.05);

        const auto surf = Cas::SampleSurface3D("x^2 + y^2", -1.0, 1.0, 5, -1.0, 1.0, 5);
        CHECK(surf.size() == 25);

        const auto reg = Cas::FitCsvRegression("0,1\n1,3\n2,5\n3,7\n", 1);
        CHECK(reg.ok && reg.coeffs.size() == 2 && std::abs(reg.coeffs[0] - 1.0) < 1e-6 && std::abs(reg.coeffs[1] - 2.0) < 1e-6);
        CHECK(reg.rSquared > 0.9999);
    }

    // 4. MNA Linear Circuit (DC Divider, AC RC -3dB Filter, OpAmp -10x), Bode, K-Map & Exporters
    {
        const auto dcDiv = Circuit::SolveMnaNetlist("V1 1 0 12; R1 1 2 2k; R2 2 0 4k", 0.0);
        CHECK(dcDiv.ok && std::abs(dcDiv.nodeVoltages.at(2).real() - 8.0) < 1e-5);
        CHECK(std::abs(dcDiv.totalPowerSumW) < 1e-6);

        std::vector<Circuit::CircuitComponent> rcComps;
        std::string nErr;
        CHECK(Circuit::ParseNetlistText("V1 1 0 1; R1 1 2 1k; C1 2 0 1u", rcComps, &nErr));
        const double fc = 1.0 / (2.0 * 3.1415926535897932 * 1000.0 * 1e-6);
        const auto rcAc = Circuit::SolveMnaCircuit(rcComps, fc);
        CHECK(rcAc.ok && std::abs(std::abs(rcAc.nodeVoltages.at(2)) - 0.7071067) < 1e-3);

        const auto bode = Circuit::ComputeBodeSweep(rcComps, 2, 10.0, 10000.0, 15);
        CHECK(bode.size() == 15 && bode.front().magDb > -0.5 && bode.back().magDb < -30.0);

        const auto opamp = Circuit::SolveMnaNetlist("V1 1 0 1; R1 1 2 1k; R2 2 3 10k; OPAMP U1 0 2 3", 0.0);
        CHECK(opamp.ok && std::abs(opamp.nodeVoltages.at(3).real() + 10.0) < 1e-4);

        const auto kmap = Circuit::SimplifyKarnaughMap(4, { 0, 1, 2, 3, 8, 9, 10, 11 });
        CHECK(kmap.ok && kmap.minimizedSopUtf8 == "F = B'");

        CHECK(Circuit::ExportCircuitToSvg(rcComps, &rcAc).find("<svg") != std::string::npos);
        CHECK(Circuit::ExportCircuitToDxf(rcComps, &rcAc).find("ENTITIES") != std::string::npos);
        CHECK(Circuit::ExportCircuitToKiCadNetlist(rcComps).find(".end") != std::string::npos);
    }

    // 5. Casio Session Card Bridge + Offscreen Render & Vector PDF Export of STEM Cards
    {
        DocumentModel stemDoc;
        stemDoc.ResetToBlank(DocumentMode::InfiniteCanvas);

        AnnotationObject mb;
        mb.kind = ObjectKind::MathBlock;
        mb.bounds = { 30.0, 30.0, 280.0, 70.0 };
        mb.formulaVarNameUtf8 = "Vo";
        mb.formulaExprUtf8 = "Vo = 9.4 V";
        mb.evaluatedResultUtf8 = "9.4 V";
        mb.textUtf8 = "5*2 = 9.4 V";
        stemDoc.AddCanvasObject(mb);

        CalcBridge::CalcStepRecord crec;
        crec.modelIdUtf8 = "fx-991CW";
        crec.keySeqUtf8 = "2+3=";
        crec.expressionUtf8 = "2 + 3";
        stemDoc.AddCanvasObject(CalcBridge::BuildCalcSessionCard(crec, { 30.0, 120.0, 280.0, 80.0 }));

        AnnotationObject plt;
        plt.kind = ObjectKind::PlotBlock;
        plt.bounds = { 30.0, 220.0, 280.0, 160.0 };
        plt.formulaExprUtf8 = "x^2 - 2";
        plt.plotConfigUtf8 = R"({"expr":"x^2 - 2","xMin":-3,"xMax":3})";
        stemDoc.AddCanvasObject(plt);

        D2DRenderer d2d;
        RasterImageBgra32 img;
        CHECK(d2d.RenderInfiniteCanvasOffscreen(stemDoc, { 0.0, 0.0, 360.0, 420.0 }, 720, 840, img));
        CHECK(img.Valid());

        const fs::path stemPdf = JoinUtf8(tmpDir, "f10_stem_canvas.pdf");
        CanvasPdfExportOptions copts;
        copts.paginated = false;
        copts.hasCustomRegion = true;
        copts.customRegionWorld = { 0.0, 0.0, 360.0, 420.0 };
        std::string err;
        CHECK(PdfWriter::ExportInfiniteCanvasToFile(stemDoc, copts, PathToW(stemPdf), &err));

        PdfDocument stemParsed;
        CHECK(stemParsed.LoadFromFile(PathToW(stemPdf), &err));
        CHECK(stemParsed.PageAt(0).extractedTextUtf8.find("CASIO fx-991CW") != std::string::npos);
    }
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

    // 5. 3D CVD Surface Plot + Ink Prediction, Selective Eraser & Ruler Snap
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

        const fs::path cudocP = JoinUtf8(tmpDir, "f11_full.cudoc");
        std::string err;
        CHECK(CudocStorage::SaveToFile(PathToW(cudocP), doc, &err));
        DocumentModel loaded;
        CHECK(CudocStorage::LoadFromFile(PathToW(cudocP), loaded, &err));
        CHECK(loaded.Bookmarks().size() == 1 && loaded.Bookmarks()[0].titleUtf8 == "Portada Tecnica");
        CHECK(!loaded.Pages()[0].objects.empty() && loaded.Pages()[0].objects[0].tags.size() == 2);

        const std::string html = Bridge::ExportCalculationReportHtml(loaded);
        CHECK(html.find("<!DOCTYPE html>") != std::string::npos);
        CHECK(html.find("#examen") != std::string::npos);
        CHECK(html.find("<svg") != std::string::npos);
        CHECK(html.find("H(s)") != std::string::npos);

        PageVersionHistory hist;
        const std::uint64_t v1 = hist.CaptureSnapshot(loaded, 0, 1000ull, "v1");
        loaded.MutablePages()[0].objects.clear();
        CHECK(loaded.Pages()[0].objects.empty());
        CHECK(hist.RestoreSnapshot(loaded, v1));
        CHECK(!loaded.Pages()[0].objects.empty());

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
    }
}

} // namespace

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    const fs::path manualsDir = (argc >= 2) ? fs::path(argv[1]) : fs::path("manuales");
    const fs::path keepDir = (argc >= 3) ? fs::path(argv[2]) : fs::path{};
    ScopedTempDir tmp(keepDir);

    TestF1CudocAndCommandStack(tmp.dir);
    TestF2LayoutCameraAndLruCache(manualsDir);
    TestF3CairoOffscreenAndVectorRender();
    TestF4RealCasioPdfEditMergeSplitFlatten(manualsDir, tmp.dir);
    TestF5TextExtractionReplacementAndOcr(manualsDir);
    TestF6InfiniteCanvas100kQuadtree(tmp.dir);
    TestF7Pkcs7SignatureAndOpenXml(manualsDir, tmp.dir);
    TestF8P0AuditRegressionFixes(manualsDir, tmp.dir);
    TestF9P1P2GoodnotesOneNoteParity(tmp.dir);
    TestF10P3StemEngineeringSuperiority(tmp.dir);
    TestF11FourDocsComplianceSuite(tmp.dir);

    WinRtPdfRenderer::ClearOpenDocumentCache();

    std::cout << "\nTOTAL CHECKS: passed=" << gChecksPassed << ", failed=" << gChecksFailed << "\n";
    if (gChecksFailed == 0) {
        std::cout << "ALL DocumentCoreLinuxTest CHECKS PASSED (F1-F11)\n";
        return 0;
    }
    std::cerr << "DocumentCoreLinuxTest FAILED with " << gChecksFailed << " failure(s)\n";
    return 1;
}
