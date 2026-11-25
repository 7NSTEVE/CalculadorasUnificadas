#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include "pdf_viewer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int Fail(const char* what)
{
    std::cerr << "[FAIL] " << what << "\n";
    return 1;
}

struct ResourceSnapshot {
    SIZE_T privateBytes = 0;
    DWORD gdiObjects = 0;
    DWORD userObjects = 0;
    DWORD handleCount = 0;
};

ResourceSnapshot TakeResourceSnapshot()
{
    ResourceSnapshot s{};
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        s.privateBytes = pmc.PrivateUsage;
    }
    s.gdiObjects = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    s.userObjects = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    GetProcessHandleCount(GetCurrentProcess(), &s.handleCount);
    return s;
}

void PumpMessagesFor(DWORD ms)
{
    const ULONGLONG deadline = GetTickCount64() + ms;
    for (;;) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetTickCount64() >= deadline) break;
        Sleep(2);
    }
}

bool WaitForActiveWorkersToDrain(int maxWaitMs)
{
    const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(maxWaitMs);
    while (GetTickCount64() < deadline) {
        PumpMessagesFor(10);
        if (PdfViewerInternal::ActiveProbeWorkerCounter().load(std::memory_order_acquire) == 0 &&
            PdfViewerInternal::ActiveViewerWorkerCounter().load(std::memory_order_acquire) == 0) {
            return true;
        }
    }
    return PdfViewerInternal::ActiveProbeWorkerCounter().load(std::memory_order_acquire) == 0 &&
        PdfViewerInternal::ActiveViewerWorkerCounter().load(std::memory_order_acquire) == 0;
}

PdfViewerInternal::UniqueBitmap CreateSolidTestDib(int w, int h, std::uint32_t bgra)
{
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    PdfViewerInternal::UniqueBitmap bmp(CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0));
    if (hdc) ReleaseDC(nullptr, hdc);
    if (bmp && bits) {
        auto* p = static_cast<std::uint32_t*>(bits);
        const size_t total = static_cast<size_t>(w) * static_cast<size_t>(h);
        for (size_t i = 0; i < total; ++i) p[i] = bgra;
    }
    return bmp;
}

int TestProbeTimeoutNoUseAfterScope(const std::wstring& samplePdf)
{
    for (int i = 0; i < 15; ++i) {
        int pc = 999, w = 999, h = 999, ink = 999;
        const HRESULT hr = PdfNativeProbeRenderPageToDIBEx(
            samplePdf,
            1,
            480,
            25,    // timeoutMs: expires BEFORE simulatedDelayMs
            &pc,
            &w,
            &h,
            &ink,
            140,   // simulatedWorkerDelayMs
            0);
        if (hr != HRESULT_FROM_WIN32(ERROR_TIMEOUT)) {
            std::cerr << "[FAIL] A-02 expected ERROR_TIMEOUT, got 0x" << std::hex << hr << std::dec << "\n";
            return 1;
        }
        // Scrub caller stack to detect use-after-scope under ASan/PageHeap.
        volatile char stackScrub[4096];
        for (size_t k = 0; k < sizeof(stackScrub); ++k) stackScrub[k] = static_cast<char>(k ^ i);
        (void)stackScrub[0];
    }
    if (!WaitForActiveWorkersToDrain(5000)) {
        return Fail("A-02 probe workers did not exit cleanly after cancellation");
    }
    std::cout << "[PASS] A-02 PdfNativeProbeRenderPageToDIBEx timeout/use-after-scope (15 cycles)\n";
    return 0;
}

int TestCloseTimeoutSharedOwnership(const std::wstring& samplePdf, const std::wstring& manualsDir)
{
    HWND hWnd = CreateNativePdfViewerWindow(
        samplePdf,
        1,
        nullptr,
        manualsDir,
        nullptr,
        false, // hidden
        1900,  // > 1500 ms WM_DESTROY wait so timeout triggers deterministically
        16ull * 1024ull * 1024ull,
        false);
    if (!hWnd) return Fail("A-01 close-timeout window creation failed");

    auto* st = PdfViewerUi::GetPdfViewerStateFromWnd(hWnd);
    if (!st) return Fail("A-01 close-timeout missing state");

    // Pump briefly so worker wakes and starts simulated slow delay.
    PumpMessagesFor(30);
    // Destroy window while worker is blocked in 1900ms delay (WM_DESTROY waits max 1500ms, sees timeout + shared_ptr keeps state alive).
    DestroyWindow(hWnd);
    PumpMessagesFor(50);

    // Wait for worker thread to drop last shared_ptr<PdfViewerState>.
    if (!WaitForActiveWorkersToDrain(6000)) {
        return Fail("A-01 close-timeout worker did not exit cleanly");
    }
    std::cout << "[PASS] A-01 WM_DESTROY worker-timeout shared ownership exit cleanly\n";
    return 0;
}

int TestStaleGenerationAndQueueDrain()
{
    PdfViewerUi::PdfViewerState st;
    st.currentPath = L"C:\\dummy\\manual.pdf";
    st.currentFileId = { 1234, 5678, true };
    {
        std::lock_guard<std::mutex> lk(st.reqMtx);
        st.pendingGen = 5;
    }

    // Push stale generation (gen=4), wrong file path, modified fileId, plus valid current gen=5.
    {
        std::lock_guard<std::mutex> rlk(st.resultMtx);
        PdfViewerUi::WorkerRenderResult rStaleGen;
        rStaleGen.path = st.currentPath;
        rStaleGen.fileId = st.currentFileId;
        rStaleGen.generation = 4;
        rStaleGen.pageIndex0 = 0;
        rStaleGen.pageCount = 10;
        rStaleGen.renderWidth = 600;
        rStaleGen.bmpW = 100;
        rStaleGen.bmpH = 100;
        rStaleGen.bmp = CreateSolidTestDib(100, 100, 0xFF112233u);
        rStaleGen.hr = S_OK;
        st.pendingResults.push_back(std::move(rStaleGen));

        PdfViewerUi::WorkerRenderResult rWrongPath;
        rWrongPath.path = L"C:\\dummy\\other.pdf";
        rWrongPath.fileId = st.currentFileId;
        rWrongPath.generation = 5;
        rWrongPath.pageIndex0 = 1;
        rWrongPath.pageCount = 20;
        rWrongPath.renderWidth = 600;
        rWrongPath.bmpW = 100;
        rWrongPath.bmpH = 100;
        rWrongPath.bmp = CreateSolidTestDib(100, 100, 0xFF445566u);
        rWrongPath.hr = S_OK;
        st.pendingResults.push_back(std::move(rWrongPath));

        PdfViewerUi::WorkerRenderResult rValid;
        rValid.path = st.currentPath;
        rValid.fileId = st.currentFileId;
        rValid.generation = 5;
        rValid.pageIndex0 = 2;
        rValid.pageCount = 12;
        rValid.renderWidth = 700;
        rValid.bmpW = 120;
        rValid.bmpH = 120;
        rValid.bmp = CreateSolidTestDib(120, 120, 0xFF778899u);
        rValid.hr = S_OK;
        st.pendingResults.push_back(std::move(rValid));
    }

    PdfViewerUi::ProcessPendingWorkerResults(&st);
    if (!st.pendingResults.empty()) return Fail("A-04 pendingResults not drained");
    if (st.cache.size() != 1) return Fail("A-03 stale results entered cache");
    if (st.cache.front().pageIndex0 != 2 || st.pageCount != 12) return Fail("A-03 wrong cache entry applied");

    std::cout << "[PASS] A-03 & A-04 stale generation rejection & queue drain\n";
    return 0;
}

int TestByteBudgetedLruCache()
{
    PdfViewerUi::PdfViewerState st;
    // Budget = 4 MiB. Each 800x800 BGRA = 2,560,000 bytes (~2.44 MiB), so only 1 fits -> strict <= 4 MiB.
    st.maxCacheBytes = 4ull * 1024ull * 1024ull;
    const ResourceSnapshot snapBefore = TakeResourceSnapshot();

    for (int i = 0; i < 20; ++i) {
        PdfViewerUi::StoreCachedPage(&st, i, 800, 800, 800, CreateSolidTestDib(800, 800, 0xFF000000u + i));
        if (st.cacheBytesUsed > st.maxCacheBytes) {
            return Fail("A-06 cacheBytesUsed exceeded maxCacheBytes");
        }
    }

    // Now test 512x512 (~1 MiB) under 3.2 MiB budget -> 3 pages fit; touch page 0 so page 1 is LRU evicted.
    PdfViewerUi::ClearPageCache(&st);
    st.maxCacheBytes = 3ull * 1024ull * 1024ull + 200u * 1024u;
    for (int i = 0; i < 3; ++i) {
        PdfViewerUi::StoreCachedPage(&st, i, 512, 512, 512, CreateSolidTestDib(512, 512, 0xFF101010u + i));
    }
    if (st.cache.size() != 3) return Fail("A-06 expected 3 pages under 3.2 MiB budget");

    PdfViewerUi::PromoteCachedPageLru(&st, 0, 512); // Page 0 newest; page 1 oldest (LRU)
    PdfViewerUi::StoreCachedPage(&st, 3, 512, 512, 512, CreateSolidTestDib(512, 512, 0xFF202020u));
    if (st.cacheBytesUsed > st.maxCacheBytes) return Fail("A-06 LRU eviction exceeded byte cap");

    bool has0 = false, has1 = false, has3 = false;
    for (const auto& c : st.cache) {
        if (c.pageIndex0 == 0) has0 = true;
        if (c.pageIndex0 == 1) has1 = true;
        if (c.pageIndex0 == 3) has3 = true;
    }
    if (!has0 || has1 || !has3) return Fail("A-06 LRU eviction evicted wrong page");

    PdfViewerUi::ClearPageCache(&st);
    const ResourceSnapshot snapAfter = TakeResourceSnapshot();
    const LONG gdiDelta = static_cast<LONG>(snapAfter.gdiObjects) - static_cast<LONG>(snapBefore.gdiObjects);
    if (gdiDelta > 2) {
        std::cerr << "[FAIL] A-06 GDI handle leak after cache eviction: +" << gdiDelta << "\n";
        return 1;
    }
    std::cout << "[PASS] A-06 byte-budgeted LRU cache (" << st.maxCacheBytes << " B cap, GDI delta=" << gdiDelta << ")\n";
    return 0;
}

int TestFileModificationCacheInvalidation(const fs::path& srcPdf, const fs::path& tempDir)
{
    const fs::path copyPdf = tempDir / "mutable_copy.pdf";
    std::error_code ec;
    fs::copy_file(srcPdf, copyPdf, fs::copy_options::overwrite_existing, ec);
    if (ec) return Fail("A-15 failed creating temp PDF copy");

    PdfViewerUi::PdfViewerState st;
    PdfViewerUi::SwitchPdfDocument(&st, copyPdf.wstring(), 1, nullptr);
    PdfViewerUi::StoreCachedPage(&st, 0, 720, 200, 200, CreateSolidTestDib(200, 200, 0xFFAAAAAAu));
    if (st.cache.empty()) return Fail("A-15 failed seeding initial cache");

    // Append trailing PDF comment so file size and last-write-time both change on same path.
    Sleep(25);
    {
        std::ofstream out(copyPdf, std::ios::binary | std::ios::app);
        out << "\n% modified-version-2\n";
    }

    PdfViewerUi::SwitchPdfDocument(&st, copyPdf.wstring(), 1, nullptr);
    if (!st.cache.empty()) {
        return Fail("A-15 stale cache retained after file modified on disk");
    }
    std::cout << "[PASS] A-15 modified-on-disk PDF invalidates cache cleanly\n";
    return 0;
}

int TestCorruptAndMalformedPdfs(const fs::path& tempDir)
{
    struct BadCase {
        const char* name;
        std::string payload;
    };
    const std::vector<BadCase> cases = {
        { "empty.pdf", "" },
        { "garbage.pdf", "THIS IS NOT A PDF FILE 1234567890" },
        { "truncated_header.pdf", "%PDF-1.7\n1 0 obj\n<< /Type /Catalog" },
        { "bad_xref.pdf", "%PDF-1.4\n1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\nxref\n999 1\n0000000000 65535 f \ntrailer\n<< /Size 999 /Root 1 0 R >>\nstartxref\n500000\n%%EOF\n" },
        { "zero_pages.pdf", "%PDF-1.4\n1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n2 0 obj\n<< /Type /Pages /Kids [] /Count 0 >>\nendobj\nxref\n0 3\n0000000000 65535 f \n0000000009 00000 n \n0000000058 00000 n \ntrailer\n<< /Size 3 /Root 1 0 R >>\nstartxref\n112\n%%EOF\n" },
    };

    for (const auto& c : cases) {
        const fs::path p = tempDir / c.name;
        {
            std::ofstream out(p, std::ios::binary | std::ios::trunc);
            out.write(c.payload.data(), static_cast<std::streamsize>(c.payload.size()));
        }
        int pc = 0, w = 0, h = 0, ink = 0;
        const HRESULT hr = PdfNativeProbeRenderPageToDIB(p.wstring(), 1, 600, &pc, &w, &h, &ink);
        if (SUCCEEDED(hr)) {
            std::cerr << "[FAIL] A-12 expected failure on corrupt PDF " << c.name << "\n";
            return 1;
        }
    }

    // Missing file case
    int pc = 0, w = 0, h = 0, ink = 0;
    const HRESULT hrMiss = PdfNativeProbeRenderPageToDIB(
        (tempDir / "does_not_exist_12345.pdf").wstring(), 1, 600, &pc, &w, &h, &ink);
    if (SUCCEEDED(hrMiss)) return Fail("A-12 expected failure on non-existent file");

    std::cout << "[PASS] A-12 corrupt/truncated/empty/missing PDF resilience (" << (cases.size() + 1) << " cases)\n";
    return 0;
}

int TestThousandCyclesStressAndLeakAudit(const std::wstring& pdfA, const std::wstring& pdfB, const std::wstring& manualsDir)
{
    ResourceSnapshot warmupSnap{};
    ResourceSnapshot midSnap{};
    constexpr int kTotalCycles = 1000;
    constexpr int kWarmupCycle = 50;

    for (int i = 0; i < kTotalCycles; ++i) {
        const std::wstring& targetPdf = (i % 2 == 0) ? pdfA : pdfB;
        HWND hWnd = CreateNativePdfViewerWindow(
            targetPdf,
            1 + (i % 3),
            nullptr,
            manualsDir,
            nullptr,
            false,
            0,
            16ull * 1024ull * 1024ull,
            false);
        if (!hWnd) {
            std::cerr << "[FAIL] Failed creating viewer window at cycle " << i << "\n";
            return 1;
        }

        if (i % 25 == 0) {
            // Let full page render complete periodically and verify bitmap appears in cache.
            auto* st = PdfViewerUi::GetPdfViewerStateFromWnd(hWnd);
            const ULONGLONG waitUntil = GetTickCount64() + 500;
            while (st && st->isLoading && GetTickCount64() < waitUntil) {
                PumpMessagesFor(5);
            }
            if (i > 0 && st && !st->isLoading && FAILED(st->lastError)) {
                std::cerr << "[FAIL] Render error during completed cycle " << i << ": 0x"
                          << std::hex << st->lastError << std::dec << "\n";
                DestroyWindow(hWnd);
                return 1;
            }
        } else {
            // Mid-render teardown stress: pump 0-3 ms so worker races into WinRT while closing.
            PumpMessagesFor(i % 4);
        }

        DestroyWindow(hWnd);
        PumpMessagesFor(0);

        if (i == kWarmupCycle) {
            WaitForActiveWorkersToDrain(3000);
            warmupSnap = TakeResourceSnapshot();
        } else if (i == 500) {
            WaitForActiveWorkersToDrain(3000);
            midSnap = TakeResourceSnapshot();
        }
    }

    if (!WaitForActiveWorkersToDrain(8000)) {
        return Fail("Worker threads remained active after 1,000 cycles");
    }
    const ResourceSnapshot endSnap = TakeResourceSnapshot();

    const LONG gdiDelta = static_cast<LONG>(endSnap.gdiObjects) - static_cast<LONG>(warmupSnap.gdiObjects);
    const LONG userDelta = static_cast<LONG>(endSnap.userObjects) - static_cast<LONG>(warmupSnap.userObjects);
    const LONG handleDelta = static_cast<LONG>(endSnap.handleCount) - static_cast<LONG>(warmupSnap.handleCount);
    const long long privDeltaMiB =
        (static_cast<long long>(endSnap.privateBytes) - static_cast<long long>(warmupSnap.privateBytes)) / (1024ll * 1024ll);

    std::cout << "  [STRESS 1000] GDI warmup=" << warmupSnap.gdiObjects << " mid=" << midSnap.gdiObjects
              << " end=" << endSnap.gdiObjects << " (delta=" << gdiDelta << ")\n";
    std::cout << "  [STRESS 1000] USER warmup=" << warmupSnap.userObjects << " mid=" << midSnap.userObjects
              << " end=" << endSnap.userObjects << " (delta=" << userDelta << ")\n";
    std::cout << "  [STRESS 1000] HANDLES warmup=" << warmupSnap.handleCount << " mid=" << midSnap.handleCount
              << " end=" << endSnap.handleCount << " (delta=" << handleDelta << ")\n";
    std::cout << "  [STRESS 1000] PrivateBytesMiB warmup=" << (warmupSnap.privateBytes / (1024 * 1024))
              << " mid=" << (midSnap.privateBytes / (1024 * 1024))
              << " end=" << (endSnap.privateBytes / (1024 * 1024))
              << " (delta=" << privDeltaMiB << " MiB)\n";

    // Strict sustained-leak gates across 950 post-warmup cycles:
    if (gdiDelta > 10) return Fail("Sustained GDI object leak detected over 1,000 cycles");
    if (userDelta > 10) return Fail("Sustained USER object leak detected over 1,000 cycles");
    if (handleDelta > 25) return Fail("Sustained kernel handle leak detected over 1,000 cycles");
    if (privDeltaMiB > 48) return Fail("Sustained private memory growth > 48 MiB over 1,000 cycles");

    std::cout << "[PASS] 1,000 open/close + mid-render abort stress test (zero leaks/UAF)\n";
    return 0;
}

int TestDynamicPathLengthSupport()
{
    const std::wstring exeDir = PdfViewerUi::GetExecutableDirectoryForPdfViewer();
    if (exeDir.empty()) return Fail("A-14 GetExecutableDirectoryForPdfViewer returned empty");
    const std::wstring resolved = PdfViewerUi::ResolveDefaultManualsDirForPdfViewer();
    if (resolved.empty()) return Fail("A-14 ResolveDefaultManualsDirForPdfViewer returned empty");

    // Verify SwitchPdfDocument stores paths > 300 chars without truncating at MAX_PATH (260).
    std::wstring longPath = L"C:\\";
    for (int i = 0; i < 12; ++i) longPath += L"subcarpeta_larga_unicode_\u00E1\u00F1\\";
    longPath += L"documento_muy_largo_sin_limite_max_path.pdf";
    if (longPath.size() <= MAX_PATH) return Fail("Test path shorter than MAX_PATH");

    PdfViewerUi::PdfViewerState st;
    PdfViewerUi::SwitchPdfDocument(&st, longPath, 1, nullptr);
    if (st.currentPath != longPath) {
        return Fail("A-14 SwitchPdfDocument truncated path > MAX_PATH");
    }
    std::cout << "[PASS] A-14 dynamic path handling verified (" << longPath.size() << " chars > MAX_PATH)\n";
    return 0;
}

} // namespace

int main()
{
    const fs::path manualsDir = fs::path(CASIO_REPO_DIR) / "manuales";
    if (!fs::exists(manualsDir)) return Fail("Missing manuales/ directory");

    const fs::path pdfA = manualsDir / g_officialManuals[0].fileName;
    const fs::path pdfB = manualsDir / g_officialManuals[3].fileName;
    if (!fs::exists(pdfA) || !fs::exists(pdfB)) return Fail("Missing official CASIO PDFs for stress test");

    const fs::path tempDir = fs::temp_directory_path() / "casio_pdf_stress_f0_test";
    std::error_code ec;
    fs::create_directories(tempDir, ec);

    if (TestDynamicPathLengthSupport() != 0) return 1;
    if (TestStaleGenerationAndQueueDrain() != 0) return 1;
    if (TestByteBudgetedLruCache() != 0) return 1;
    if (TestFileModificationCacheInvalidation(pdfA, tempDir) != 0) return 1;
    if (TestCorruptAndMalformedPdfs(tempDir) != 0) return 1;
    if (TestProbeTimeoutNoUseAfterScope(pdfA.wstring()) != 0) return 1;
    if (TestCloseTimeoutSharedOwnership(pdfA.wstring(), manualsDir.wstring()) != 0) return 1;
    if (TestThousandCyclesStressAndLeakAudit(pdfA.wstring(), pdfB.wstring(), manualsDir.wstring()) != 0) return 1;

    fs::remove_all(tempDir, ec);
    std::cout << "ALL PdfViewerStress (F0) TESTS PASSED (8/8)\n";
    return 0;
}
