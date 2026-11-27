#include "d2d_renderer.hpp"
#include "card_render_common.hpp"
#include "math_box_typesetter.hpp"
#include "../core/accessibility_engine.hpp"
#include "../core/collaboration_publish_sync.hpp"
#include "../core/inline_url_parser.hpp"
#include "../core/ink_engine.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/smart_script_reflow.hpp"
#include "../core/structured_database.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../media/native_video_engine.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <objbase.h>
#include <propvarutil.h>
#include <roapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace DocStudio {

namespace {

using Microsoft::WRL::ComPtr;

int QuantizeBucketPx(int px) noexcept {
    if (px <= 160) return 160;
    if (px <= 320) return 320;
    if (px <= 640) return 640;
    if (px <= 960) return 960;
    if (px <= 1400) return 1400;
    if (px <= 2000) return 2000;
    return 2600;
}

D2D1_COLOR_F ToD2DColor(DocColor c) noexcept {
    return D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

DocColor ResolveAccessibleColor(DocColor c, const ViewportOverlayState* ov) noexcept {
    if (!ov || ov->colorBlindPaletteMode == 0) return c;
    return AccessibilityEngine::RemapColorForColorBlind(c, static_cast<ColorBlindPaletteMode>(ov->colorBlindPaletteMode));
}

D2D1_RECT_F ToD2DRect(const DocRect& r) noexcept {
    return D2D1::RectF(
        static_cast<FLOAT>(r.x),
        static_cast<FLOAT>(r.y),
        static_cast<FLOAT>(r.Right()),
        static_cast<FLOAT>(r.Bottom()));
}

std::uint64_t HashBytesFnv1a64(const std::vector<std::uint8_t>& bytes) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    const std::size_t n = bytes.size();
    const std::size_t step = (n > 8192u) ? (n / 4096u) : 1u;
    for (std::size_t i = 0; i < n; i += step) {
        h ^= static_cast<std::uint64_t>(bytes[i]);
        h *= 1099511628211ull;
    }
    h ^= static_cast<std::uint64_t>(n);
    h *= 1099511628211ull;
    return h;
}

// Materializes embedded PDF asset bytes (from migrated .goodnotes / .note / portable .cudoc)
// into a deterministic %TEMP%\CuadernU\pdf_cache\<key>.pdf file so WinRT PDF rendering works
// even when sourcePdfPathUtf8 is empty or originated on another device.
std::string ResolvePdfSourcePathForRender(
    const DocumentModel& doc,
    const std::string& sourcePdfPathUtf8,
    const std::string& sourcePdfAssetKey)
{
    if (!sourcePdfPathUtf8.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(Utf8ToWide(sourcePdfPathUtf8)), ec)) {
            return sourcePdfPathUtf8;
        }
    }
    if (sourcePdfAssetKey.empty()) return sourcePdfPathUtf8;
    const auto* bytes = doc.FindAsset(sourcePdfAssetKey);
    if (!bytes || bytes->empty()) return sourcePdfPathUtf8;

    static std::mutex sCacheMtx;
    static std::unordered_map<std::string, std::string> sMaterialized;
    const std::string cacheKey = sourcePdfAssetKey + "#" + std::to_string(bytes->size()) + "#" + std::to_string(HashBytesFnv1a64(*bytes));
    {
        std::lock_guard<std::mutex> lk(sCacheMtx);
        auto it = sMaterialized.find(cacheKey);
        if (it != sMaterialized.end()) {
            std::error_code ec;
            if (std::filesystem::exists(std::filesystem::path(Utf8ToWide(it->second)), ec)) {
                return it->second;
            }
        }
    }

    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    const std::filesystem::path dir = std::filesystem::path(tmpRoot) / L"CuadernU" / L"pdf_cache";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    char hexName[64]{};
    std::snprintf(hexName, sizeof(hexName), "pdf_%016llx_%zu.pdf",
        static_cast<unsigned long long>(HashBytesFnv1a64(*bytes)), bytes->size());
    const std::filesystem::path outPath = dir / hexName;
    if (!std::filesystem::exists(outPath, ec) || std::filesystem::file_size(outPath, ec) != bytes->size()) {
        std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
        if (ofs.is_open()) {
            ofs.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
        }
    }
    const std::string utf8Out = WideToUtf8(outPath.wstring());
    {
        std::lock_guard<std::mutex> lk(sCacheMtx);
        sMaterialized[cacheKey] = utf8Out;
    }
    return utf8Out;
}

std::string ResolvePdfSourcePathForRender(const DocumentModel& doc, const PageModel& pg) {
    return ResolvePdfSourcePathForRender(doc, pg.sourcePdfPathUtf8, pg.sourcePdfAssetKey);
}

class D2DMathBackend final : public MathTypeset::IMathDrawBackend {
public:
    D2DMathBackend(ID2D1RenderTarget* rt, ID2D1SolidColorBrush* brush, IDWriteFactory* dw)
        : rt_(rt), brush_(brush), dw_(dw) {}

    MathTypeset::TextMetrics2D MeasureText(
        const std::string& utf8,
        double fontSizePx,
        MathTypeset::FontStyleRole role) override
    {
        MathTypeset::TextMetrics2D out{};
        if (!dw_ || utf8.empty()) {
            out.ascent = fontSizePx * 0.78;
            out.descent = fontSizePx * 0.22;
            return out;
        }
        ComPtr<IDWriteTextLayout> layout = CreateLayout(utf8, fontSizePx, role);
        if (!layout) {
            out.width = fontSizePx * 0.52 * static_cast<double>(utf8.size());
            out.ascent = fontSizePx * 0.78;
            out.descent = fontSizePx * 0.22;
            return out;
        }
        DWRITE_TEXT_METRICS tm{};
        if (SUCCEEDED(layout->GetMetrics(&tm))) {
            out.width = static_cast<double>(tm.widthIncludingTrailingWhitespace);
        }
        DWRITE_LINE_METRICS lm{};
        UINT32 actualLines = 0;
        if (SUCCEEDED(layout->GetLineMetrics(&lm, 1, &actualLines)) && actualLines >= 1) {
            out.ascent = static_cast<double>(lm.baseline);
            out.descent = (std::max)(fontSizePx * 0.18, static_cast<double>(lm.height - lm.baseline));
        } else {
            out.ascent = fontSizePx * 0.78;
            out.descent = fontSizePx * 0.22;
        }
        return out;
    }

    void DrawTextRun(
        double x,
        double baselineY,
        const std::string& utf8,
        double fontSizePx,
        MathTypeset::FontStyleRole role,
        DocColor color) override
    {
        if (!rt_ || !brush_ || !dw_ || utf8.empty()) return;
        ComPtr<IDWriteTextLayout> layout = CreateLayout(utf8, fontSizePx, role);
        if (!layout) return;
        double baseline = fontSizePx * 0.78;
        DWRITE_LINE_METRICS lm{};
        UINT32 actualLines = 0;
        if (SUCCEEDED(layout->GetLineMetrics(&lm, 1, &actualLines)) && actualLines >= 1) {
            baseline = static_cast<double>(lm.baseline);
        }
        brush_->SetColor(ToD2DColor(color));
        rt_->DrawTextLayout(
            D2D1::Point2F(static_cast<FLOAT>(x), static_cast<FLOAT>(baselineY - baseline)),
            layout.Get(), brush_, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }

    void DrawLine(
        double x0, double y0,
        double x1, double y1,
        double thickness,
        DocColor color) override
    {
        if (!rt_ || !brush_) return;
        brush_->SetColor(ToD2DColor(color));
        rt_->DrawLine(
            D2D1::Point2F(static_cast<FLOAT>(x0), static_cast<FLOAT>(y0)),
            D2D1::Point2F(static_cast<FLOAT>(x1), static_cast<FLOAT>(y1)),
            brush_, static_cast<FLOAT>((std::max)(0.6, thickness)));
    }

    void DrawRoundedPill(
        double x, double y, double w, double h,
        double radius,
        DocColor fill,
        DocColor stroke,
        double strokeWidth) override
    {
        if (!rt_ || !brush_) return;
        const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF(static_cast<FLOAT>(x), static_cast<FLOAT>(y),
                        static_cast<FLOAT>(x + w), static_cast<FLOAT>(y + h)),
            static_cast<FLOAT>(radius), static_cast<FLOAT>(radius));
        if (fill.a > 0) {
            brush_->SetColor(ToD2DColor(fill));
            rt_->FillRoundedRectangle(rr, brush_);
        }
        if (stroke.a > 0 && strokeWidth > 0.0) {
            brush_->SetColor(ToD2DColor(stroke));
            rt_->DrawRoundedRectangle(rr, brush_, static_cast<FLOAT>(strokeWidth));
        }
    }

private:
    ComPtr<IDWriteTextLayout> CreateLayout(
        const std::string& utf8,
        double fontSizePx,
        MathTypeset::FontStyleRole role)
    {
        const wchar_t* family = L"Segoe UI";
        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
        DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
        switch (role) {
        case MathTypeset::FontStyleRole::UiRegular:
            family = L"Segoe UI";
            weight = DWRITE_FONT_WEIGHT_NORMAL;
            break;
        case MathTypeset::FontStyleRole::UiSemiBold:
            family = L"Segoe UI";
            weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
            break;
        case MathTypeset::FontStyleRole::MathItalic:
            family = L"Cambria Math";
            style = DWRITE_FONT_STYLE_ITALIC;
            break;
        case MathTypeset::FontStyleRole::MathRoman:
            family = L"Cambria Math";
            break;
        case MathTypeset::FontStyleRole::MathBold:
            family = L"Cambria Math";
            weight = DWRITE_FONT_WEIGHT_BOLD;
            break;
        case MathTypeset::FontStyleRole::MathSymbol:
            family = L"Cambria Math";
            break;
        case MathTypeset::FontStyleRole::KeycapLabel:
            family = L"Segoe UI";
            weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
            break;
        }
        ComPtr<IDWriteTextFormat> fmt;
        if (FAILED(dw_->CreateTextFormat(
                family, nullptr, weight, style, DWRITE_FONT_STRETCH_NORMAL,
                static_cast<FLOAT>((std::max)(5.5, fontSizePx)), L"es-ES", &fmt)) || !fmt) {
            return nullptr;
        }
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        const std::wstring wtxt = Utf8ToWide(utf8);
        ComPtr<IDWriteTextLayout> layout;
        if (FAILED(dw_->CreateTextLayout(
                wtxt.c_str(), static_cast<UINT32>(wtxt.size()),
                fmt.Get(), 4096.0f, 512.0f, &layout))) {
            return nullptr;
        }
        return layout;
    }

    ID2D1RenderTarget* rt_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    IDWriteFactory* dw_ = nullptr;
};

} // namespace

// ---------------- PageRenderCache ----------------

PageRenderCache::PageRenderCache(std::size_t maxBytes)
    : cache_(maxBytes)
{
    worker_ = std::thread(&PageRenderCache::WorkerLoop, this);
}

PageRenderCache::~PageRenderCache() {
    Shutdown();
}

void PageRenderCache::Shutdown() {
    {
        std::lock_guard<std::mutex> lk(queueMtx_);
        if (stop_.load()) return;
        stop_.store(true);
        queue_.clear();
        inFlight_.clear();
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

PageCacheKey PageRenderCache::MakeKey(const PageModel& pg, int targetWidthPx) {
    PageCacheKey k;
    k.pageId = pg.pageId;
    k.sourcePdfPath = pg.sourcePdfPathUtf8;
    k.sourcePageIndex0 = pg.sourcePageIndex0;
    k.widthBucketPx = QuantizeBucketPx(targetWidthPx);
    k.rotationDeg = ((pg.rotationDeg % 360) + 360) % 360;
    k.cropL = static_cast<int>(std::lround(pg.crop.left * 1000.0));
    k.cropT = static_cast<int>(std::lround(pg.crop.top * 1000.0));
    k.cropR = static_cast<int>(std::lround(pg.crop.right * 1000.0));
    k.cropB = static_cast<int>(std::lround(pg.crop.bottom * 1000.0));
    return k;
}

void PageRenderCache::SetMaxBytes(std::size_t maxBytes) { cache_.SetMaxBytes(maxBytes); }
std::size_t PageRenderCache::MaxBytes() const { return cache_.MaxBytes(); }
std::size_t PageRenderCache::BytesUsed() const { return cache_.BytesUsed(); }
std::size_t PageRenderCache::EntryCount() const { return cache_.EntryCount(); }
std::uint64_t PageRenderCache::EvictionCount() const { return cache_.EvictionCount(); }

bool PageRenderCache::TryGet(const PageCacheKey& key, RasterImageBgra32& outImg) {
    return cache_.TryGet(key, outImg);
}

void PageRenderCache::PutDirect(const PageCacheKey& key, RasterImageBgra32 img) {
    const std::size_t bytes = img.ByteSize();
    cache_.Put(key, std::move(img), bytes);
}

void PageRenderCache::RequestAsync(const PageCacheKey& key, std::uint64_t generation, std::function<void()> onReady) {
    if (key.sourcePdfPath.empty()) return;
    {
        std::lock_guard<std::mutex> lk(queueMtx_);
        if (stop_.load()) return;
        auto it = inFlight_.find(key);
        if (it != inFlight_.end() && it->second >= generation) return;
        inFlight_[key] = generation;
        while (queue_.size() >= 64) {
            const auto dropped = std::move(queue_.front());
            queue_.pop_front();
            auto dit = inFlight_.find(dropped.key);
            if (dit != inFlight_.end() && dit->second <= dropped.generation) {
                inFlight_.erase(dit);
            }
        }
        queue_.push_back({ key, generation, std::move(onReady) });
    }
    cv_.notify_one();
}

void PageRenderCache::InvalidateAll(std::uint64_t newGeneration) {
    activeGen_.store(newGeneration, std::memory_order_release);
    cache_.Clear();
    std::lock_guard<std::mutex> lk(queueMtx_);
    queue_.clear();
    inFlight_.clear();
}

void PageRenderCache::WorkerLoop() {
    struct ScopedRoMta {
        HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
        ~ScopedRoMta() { if (SUCCEEDED(hr)) RoUninitialize(); }
    } roMta;
    while (true) {
        PendingJob job;
        {
            std::unique_lock<std::mutex> lk(queueMtx_);
            cv_.wait(lk, [&]() { return stop_.load() || !queue_.empty(); });
            if (stop_.load()) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }

        if (job.generation < activeGen_.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lk(queueMtx_);
            inFlight_.erase(job.key);
            continue;
        }

        RasterImageBgra32 existing;
        if (cache_.TryGet(job.key, existing)) {
            std::lock_guard<std::mutex> lk(queueMtx_);
            inFlight_.erase(job.key);
            continue;
        }

        NormalizedCropRect crop{
            job.key.cropL / 1000.0,
            job.key.cropT / 1000.0,
            job.key.cropR / 1000.0,
            job.key.cropB / 1000.0
        };
        RasterImageBgra32 rendered;
        const std::wstring wpath = Utf8ToWide(job.key.sourcePdfPath);
        const bool ok = WinRtPdfRenderer::RenderPageToBgra32(
            wpath,
            job.key.sourcePageIndex0,
            job.key.widthBucketPx,
            job.key.rotationDeg,
            crop,
            rendered,
            &stop_);

        {
            std::lock_guard<std::mutex> lk(queueMtx_);
            inFlight_.erase(job.key);
        }

        if (ok && job.generation >= activeGen_.load(std::memory_order_acquire)) {
            const std::size_t bytes = rendered.ByteSize();
            cache_.Put(job.key, std::move(rendered), bytes);
            if (job.onReady) job.onReady();
        }
    }
}

// ---------------- D2DRenderer ----------------

struct D2DRenderer::Impl {
    ComPtr<ID2D1Factory> d2dFactory;
    ComPtr<IDWriteFactory> dwFactory;
    ComPtr<IWICImagingFactory> wicFactory;

    HWND hwnd = nullptr;
    ComPtr<ID2D1HwndRenderTarget> hwndRt;
    bool hardwareAccelEnabled = false; // Default FALSE => D2D1_RENDER_TARGET_TYPE_SOFTWARE (strict 0.0% GPU!)
    ComPtr<ID2D1StrokeStyle> roundStroke;
    ComPtr<ID2D1StrokeStyle> dashStroke;

    // Bounded per-render-target bitmap caches (trimmed aggressively on minimize for minimal RAM)
    struct CachedGpuPage {
        PageCacheKey key;
        ComPtr<ID2D1Bitmap> bmp;
    };
    std::vector<CachedGpuPage> pageGpuBitmaps;

    struct CachedGpuAsset {
        std::string key;
        ComPtr<ID2D1Bitmap> bmp;
    };
    std::vector<CachedGpuAsset> assetGpuBitmaps;

    // Single-decode multi-frame GIF89a cache with real WIC /grctlext/Delay timing (never recreates decoder per tick!)
    struct CachedAnimatedGif {
        std::string key;
        std::vector<ComPtr<ID2D1Bitmap>> frames;
        std::vector<int> delaysMs;
        std::uint64_t totalDurationMs = 100;
        std::size_t approxBytes = 0;
    };
    std::vector<CachedAnimatedGif> gifCache;
    std::size_t gifCacheTotalBytes = 0;

    bool lastFrameHadVisibleAnimatedGif = false;
    UINT lastFrameMinGifDelayMs = 100;

    PageRenderCache* activePageCache = nullptr;
    std::uint64_t activeAsyncGen = 0;
    std::function<void()> activeOnAsyncReady;
    bool activeSyncOffscreen = false;

    bool factoriesReady = false;
    bool didCoInit = false;

    bool EnsureStrokeStyles() {
        if (!d2dFactory) return false;
        if (!roundStroke) {
            D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f);
            d2dFactory->CreateStrokeStyle(props, nullptr, 0, &roundStroke);
        }
        if (!dashStroke) {
            D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
                D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_DASH, 0.0f);
            d2dFactory->CreateStrokeStyle(props, nullptr, 0, &dashStroke);
        }
        return true;
    }

    ID2D1Bitmap* UploadBgra32(ID2D1RenderTarget* rt, const RasterImageBgra32& img) {
        if (!rt || !img.Valid()) return nullptr;
        D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        ComPtr<ID2D1Bitmap> bmp;
        const HRESULT hr = rt->CreateBitmap(
            D2D1::SizeU(static_cast<UINT32>(img.width), static_cast<UINT32>(img.height)),
            img.pixels.data(),
            static_cast<UINT32>(img.width * 4),
            props,
            &bmp);
        if (FAILED(hr) || !bmp) return nullptr;
        return bmp.Detach();
    }

    ComPtr<ID2D1Bitmap> AcquireGpuBitmapForEncodedAsset(
        ID2D1RenderTarget* rt,
        const std::string& cacheTag,
        const std::vector<std::uint8_t>& bytes,
        bool cacheResult,
        std::uint32_t animTick = 0,
        std::uint64_t animClockMs = 0,
        bool gifPaused = false)
    {
        if (!rt || !wicFactory || bytes.empty()) return nullptr;
        const bool isGif = (bytes.size() >= 6 && bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8');

        // Fast-path: Single-decode cached GIF89a with real /grctlext/Delay timing
        if (isGif && cacheResult) {
            auto selectGifFrame = [&](const CachedAnimatedGif& g) -> ComPtr<ID2D1Bitmap> {
                if (g.frames.empty()) return nullptr;
                if (g.frames.size() > 1 && !gifPaused) {
                    lastFrameHadVisibleAnimatedGif = true;
                    for (int d : g.delaysMs) {
                        if (d >= 20 && static_cast<UINT>(d) < lastFrameMinGifDelayMs) {
                            lastFrameMinGifDelayMs = static_cast<UINT>(d);
                        }
                    }
                }
                if (gifPaused || g.frames.size() == 1) return g.frames[0];
                if (animClockMs > 0 && g.totalDurationMs > 0) {
                    const std::uint64_t t = animClockMs % g.totalDurationMs;
                    std::uint64_t acc = 0;
                    for (std::size_t i = 0; i < g.frames.size(); ++i) {
                        const int dm = (i < g.delaysMs.size() && g.delaysMs[i] >= 20) ? g.delaysMs[i] : 100;
                        acc += static_cast<std::uint64_t>(dm);
                        if (t < acc) return g.frames[i];
                    }
                    return g.frames.back();
                }
                return g.frames[static_cast<std::size_t>(animTick % g.frames.size())];
            };

            for (const auto& g : gifCache) {
                if (g.key == cacheTag) return selectGifFrame(g);
            }

            // Decode ALL frames ONCE with real WIC delay metadata (capped at 120 frames / 24 MiB budget)
            ComPtr<IWICStream> stream;
            if (SUCCEEDED(wicFactory->CreateStream(&stream)) &&
                SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size())))) {
                ComPtr<IWICBitmapDecoder> dec;
                if (SUCCEEDED(wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &dec)) && dec) {
                    UINT frameCount = 1;
                    dec->GetFrameCount(&frameCount);
                    if (frameCount > 1) {
                        constexpr std::size_t kMaxGifCacheBytes = 24ull * 1024ull * 1024ull;
                        CachedAnimatedGif entry;
                        entry.key = cacheTag;
                        entry.totalDurationMs = 0;

                        UINT canvasW = 0, canvasH = 0;
                        {
                            ComPtr<IWICMetadataQueryReader> rootMeta;
                            if (SUCCEEDED(dec->GetMetadataQueryReader(&rootMeta)) && rootMeta) {
                                PROPVARIANT pv{};
                                PropVariantInit(&pv);
                                if (SUCCEEDED(rootMeta->GetMetadataByName(L"/logscrdesc/Width", &pv)) && pv.vt == VT_UI2) canvasW = pv.uiVal;
                                PropVariantClear(&pv);
                                if (SUCCEEDED(rootMeta->GetMetadataByName(L"/logscrdesc/Height", &pv)) && pv.vt == VT_UI2) canvasH = pv.uiVal;
                                PropVariantClear(&pv);
                            }
                        }
                        if (canvasW == 0 || canvasH == 0 || canvasW > 4096 || canvasH > 4096) {
                            ComPtr<IWICBitmapFrameDecode> f0;
                            if (SUCCEEDED(dec->GetFrame(0, &f0)) && f0) {
                                f0->GetSize(&canvasW, &canvasH);
                            }
                        }
                        std::vector<std::uint8_t> canvasPx;
                        std::vector<std::uint8_t> prevCanvasPx;
                        const bool canComposite = (canvasW > 0 && canvasH > 0 && canvasW <= 2048 && canvasH <= 2048);
                        if (canComposite) {
                            canvasPx.assign(static_cast<std::size_t>(canvasW) * canvasH * 4u, 0u);
                        }

                        const UINT limit = (std::min<UINT>)(frameCount, 120u);
                        for (UINT fi = 0; fi < limit; ++fi) {
                            ComPtr<IWICBitmapFrameDecode> frame;
                            if (FAILED(dec->GetFrame(fi, &frame)) || !frame) continue;
                            int delayMs = 100;
                            UINT disposal = 0;
                            UINT leftPx = 0, topPx = 0;
                            ComPtr<IWICMetadataQueryReader> meta;
                            if (SUCCEEDED(frame->GetMetadataQueryReader(&meta)) && meta) {
                                PROPVARIANT pv{};
                                PropVariantInit(&pv);
                                if (SUCCEEDED(meta->GetMetadataByName(L"/grctlext/Delay", &pv)) && pv.vt == VT_UI2 && pv.uiVal > 0) {
                                    delayMs = (std::max)(20, static_cast<int>(pv.uiVal) * 10);
                                }
                                PropVariantClear(&pv);
                                if (SUCCEEDED(meta->GetMetadataByName(L"/grctlext/Disposal", &pv)) && (pv.vt == VT_UI1 || pv.vt == VT_UI2)) {
                                    disposal = (pv.vt == VT_UI1) ? pv.bVal : pv.uiVal;
                                }
                                PropVariantClear(&pv);
                                if (SUCCEEDED(meta->GetMetadataByName(L"/imgdesc/Left", &pv)) && pv.vt == VT_UI2) leftPx = pv.uiVal;
                                PropVariantClear(&pv);
                                if (SUCCEEDED(meta->GetMetadataByName(L"/imgdesc/Top", &pv)) && pv.vt == VT_UI2) topPx = pv.uiVal;
                                PropVariantClear(&pv);
                            }
                            ComPtr<IWICFormatConverter> conv;
                            if (FAILED(wicFactory->CreateFormatConverter(&conv)) || !conv) continue;
                            if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
                                    WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) continue;

                            UINT fw = 0, fh = 0;
                            conv->GetSize(&fw, &fh);
                            const std::size_t frameBytes = canComposite
                                ? (static_cast<std::size_t>(canvasW) * canvasH * 4u)
                                : (static_cast<std::size_t>(fw) * fh * 4u);
                            if (entry.approxBytes + frameBytes > kMaxGifCacheBytes && !entry.frames.empty()) {
                                break; // Hard 24 MiB cap per GIF to prevent RAM spikes
                            }

                            ComPtr<ID2D1Bitmap> bmp;
                            if (canComposite && fw > 0 && fh > 0 && fw <= canvasW && fh <= canvasH) {
                                if (disposal == 3u) prevCanvasPx = canvasPx;
                                std::vector<std::uint8_t> subPx(static_cast<std::size_t>(fw) * fh * 4u, 0u);
                                if (SUCCEEDED(conv->CopyPixels(nullptr, fw * 4u, static_cast<UINT>(subPx.size()), subPx.data()))) {
                                    for (UINT y = 0; y < fh && (topPx + y) < canvasH; ++y) {
                                        const std::uint8_t* src = subPx.data() + static_cast<std::size_t>(y) * fw * 4u;
                                        std::uint8_t* dst = canvasPx.data() + static_cast<std::size_t>(topPx + y) * canvasW * 4u + static_cast<std::size_t>(leftPx) * 4u;
                                        for (UINT x = 0; x < fw && (leftPx + x) < canvasW; ++x) {
                                            const std::uint8_t sa = src[x * 4 + 3];
                                            if (sa == 255u) {
                                                std::memcpy(dst + x * 4, src + x * 4, 4u);
                                            } else if (sa > 0u) {
                                                const UINT invA = 255u - sa;
                                                dst[x * 4 + 0] = static_cast<std::uint8_t>((std::min<UINT>)(255u, src[x * 4 + 0] + (dst[x * 4 + 0] * invA) / 255u));
                                                dst[x * 4 + 1] = static_cast<std::uint8_t>((std::min<UINT>)(255u, src[x * 4 + 1] + (dst[x * 4 + 1] * invA) / 255u));
                                                dst[x * 4 + 2] = static_cast<std::uint8_t>((std::min<UINT>)(255u, src[x * 4 + 2] + (dst[x * 4 + 2] * invA) / 255u));
                                                dst[x * 4 + 3] = static_cast<std::uint8_t>((std::min<UINT>)(255u, sa + (dst[x * 4 + 3] * invA) / 255u));
                                            }
                                        }
                                    }
                                    const D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(
                                        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                                    rt->CreateBitmap(D2D1::SizeU(canvasW, canvasH), canvasPx.data(), canvasW * 4u, props, &bmp);
                                    if (disposal == 2u) {
                                        for (UINT y = 0; y < fh && (topPx + y) < canvasH; ++y) {
                                            std::uint8_t* dst = canvasPx.data() + static_cast<std::size_t>(topPx + y) * canvasW * 4u + static_cast<std::size_t>(leftPx) * 4u;
                                            const UINT clrW = (std::min)(fw, canvasW - leftPx);
                                            std::memset(dst, 0, static_cast<std::size_t>(clrW) * 4u);
                                        }
                                    } else if (disposal == 3u && prevCanvasPx.size() == canvasPx.size()) {
                                        canvasPx = prevCanvasPx;
                                    }
                                }
                            }
                            if (!bmp) {
                                rt->CreateBitmapFromWicBitmap(conv.Get(), nullptr, &bmp);
                            }
                            if (bmp) {
                                const D2D1_SIZE_U bsz = bmp->GetPixelSize();
                                entry.approxBytes += static_cast<std::size_t>(bsz.width) * bsz.height * 4u;
                                entry.frames.push_back(bmp);
                                entry.delaysMs.push_back(delayMs);
                                entry.totalDurationMs += static_cast<std::uint64_t>(delayMs);
                            }
                        }
                        if (!entry.frames.empty()) {
                            while (!gifCache.empty() && (gifCache.size() >= 12u || gifCacheTotalBytes + entry.approxBytes > kMaxGifCacheBytes)) {
                                gifCacheTotalBytes = (gifCacheTotalBytes > gifCache.front().approxBytes)
                                    ? (gifCacheTotalBytes - gifCache.front().approxBytes) : 0u;
                                gifCache.erase(gifCache.begin());
                            }
                            gifCacheTotalBytes += entry.approxBytes;
                            gifCache.push_back(std::move(entry));
                            return selectGifFrame(gifCache.back());
                        }
                    }
                }
            }
        }

        if (cacheResult && !isGif) {
            for (const auto& entry : assetGpuBitmaps) {
                if (entry.key == cacheTag) return entry.bmp;
            }
        }
        ComPtr<IWICStream> stream;
        if (FAILED(wicFactory->CreateStream(&stream))) return nullptr;
        if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size())))) return nullptr;
        ComPtr<IWICBitmapDecoder> dec;
        if (FAILED(wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &dec))) return nullptr;
        UINT frameCount = 1;
        dec->GetFrameCount(&frameCount);
        UINT frameIdx = 0u;
        if (frameCount > 1 && !gifPaused) {
            if (isGif) {
                lastFrameHadVisibleAnimatedGif = true;
                if (animClockMs > 0) {
                    const UINT limit = (std::min<UINT>)(frameCount, 120u);
                    std::vector<int> delays;
                    delays.reserve(limit);
                    std::uint64_t totalMs = 0;
                    for (UINT fi = 0; fi < limit; ++fi) {
                        int dMs = 100;
                        ComPtr<IWICBitmapFrameDecode> fr;
                        if (SUCCEEDED(dec->GetFrame(fi, &fr)) && fr) {
                            ComPtr<IWICMetadataQueryReader> meta;
                            if (SUCCEEDED(fr->GetMetadataQueryReader(&meta)) && meta) {
                                PROPVARIANT pv{};
                                PropVariantInit(&pv);
                                if (SUCCEEDED(meta->GetMetadataByName(L"/grctlext/Delay", &pv)) && pv.vt == VT_UI2 && pv.uiVal > 0) {
                                    dMs = (std::max)(20, static_cast<int>(pv.uiVal) * 10);
                                }
                                PropVariantClear(&pv);
                            }
                        }
                        if (static_cast<UINT>(dMs) < lastFrameMinGifDelayMs) lastFrameMinGifDelayMs = static_cast<UINT>(dMs);
                        delays.push_back(dMs);
                        totalMs += static_cast<std::uint64_t>(dMs);
                    }
                    if (totalMs > 0) {
                        const std::uint64_t t = animClockMs % totalMs;
                        std::uint64_t acc = 0;
                        frameIdx = limit - 1u;
                        for (UINT fi = 0; fi < limit; ++fi) {
                            acc += static_cast<std::uint64_t>(delays[fi]);
                            if (t < acc) { frameIdx = fi; break; }
                        }
                    }
                } else {
                    frameIdx = static_cast<UINT>(animTick % frameCount);
                }
            } else {
                frameIdx = static_cast<UINT>(animTick % frameCount);
            }
        }
        const std::string finalKey = (frameCount > 1) ? (cacheTag + "#f" + std::to_string(frameIdx)) : cacheTag;
        if (cacheResult) {
            for (const auto& entry : assetGpuBitmaps) {
                if (entry.key == finalKey) return entry.bmp;
            }
        }
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(dec->GetFrame(frameIdx, &frame))) return nullptr;
        ComPtr<IWICFormatConverter> conv;
        if (FAILED(wicFactory->CreateFormatConverter(&conv))) return nullptr;
        if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return nullptr;

        ComPtr<ID2D1Bitmap> bmp;
        if (FAILED(rt->CreateBitmapFromWicBitmap(conv.Get(), nullptr, &bmp)) || !bmp) return nullptr;

        if (cacheResult) {
            if (assetGpuBitmaps.size() >= 32u) assetGpuBitmaps.erase(assetGpuBitmaps.begin());
            assetGpuBitmaps.push_back({ finalKey, bmp });
        }
        return bmp;
    }

    void DrawPaperPattern(
        ID2D1RenderTarget* rt,
        ID2D1SolidColorBrush* brush,
        const DocRect& worldRegion,
        PaperPattern pattern,
        const Camera2D& cam,
        bool darkPaper = false)
    {
        if (pattern == PaperPattern::Plain) return;

        auto drawGridLines = [&](double stepWorld, DocColor col, float pxThickness) {
            const double screenStep = stepWorld * cam.zoom;
            if (screenStep < 4.0) return; // avoid subpixel moire when zoomed very far out
            brush->SetColor(ToD2DColor(col));
            const double startX = std::floor(worldRegion.x / stepWorld) * stepWorld;
            const double endX = worldRegion.Right();
            for (double x = startX; x <= endX; x += stepWorld) {
                const DocPoint p0 = cam.WorldToScreen(x, worldRegion.y);
                const DocPoint p1 = cam.WorldToScreen(x, worldRegion.Bottom());
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                             D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)),
                             brush, pxThickness);
            }
            const double startY = std::floor(worldRegion.y / stepWorld) * stepWorld;
            const double endY = worldRegion.Bottom();
            for (double y = startY; y <= endY; y += stepWorld) {
                const DocPoint p0 = cam.WorldToScreen(worldRegion.x, y);
                const DocPoint p1 = cam.WorldToScreen(worldRegion.Right(), y);
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                             D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)),
                             brush, pxThickness);
            }
        };

        if (pattern == PaperPattern::Ruled) {
            const double step = 24.0;
            if (step * cam.zoom >= 4.0) {
                brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(44, 52, 66, 255) : DocColor::RGBA(205, 221, 244, 255)));
                const double startY = std::floor(worldRegion.y / step) * step;
                for (double y = startY; y <= worldRegion.Bottom(); y += step) {
                    const DocPoint p0 = cam.WorldToScreen(worldRegion.x, y);
                    const DocPoint p1 = cam.WorldToScreen(worldRegion.Right(), y);
                    rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                                 D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)),
                                 brush, 1.0f);
                }
                const double marginX = worldRegion.x + 56.0;
                const DocPoint m0 = cam.WorldToScreen(marginX, worldRegion.y);
                const DocPoint m1 = cam.WorldToScreen(marginX, worldRegion.Bottom());
                brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(120, 52, 60, 220) : DocColor::RGBA(240, 150, 155, 255)));
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(m0.x), static_cast<FLOAT>(m0.y)),
                             D2D1::Point2F(static_cast<FLOAT>(m1.x), static_cast<FLOAT>(m1.y)),
                             brush, 1.2f);
            }
        } else if (pattern == PaperPattern::Grid) {
            drawGridLines(20.0, darkPaper ? DocColor::RGBA(40, 46, 58, 255) : DocColor::RGBA(218, 228, 240, 255), 1.0f);
        } else if (pattern == PaperPattern::Dots) {
            const double step = 20.0;
            if (step * cam.zoom >= 6.0) {
                brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(62, 70, 86, 255) : DocColor::RGBA(175, 186, 204, 255)));
                const float r = (std::max)(1.0f, static_cast<float>(1.2 * cam.zoom));
                const double startX = std::floor(worldRegion.x / step) * step;
                const double startY = std::floor(worldRegion.y / step) * step;
                for (double y = startY; y <= worldRegion.Bottom(); y += step) {
                    for (double x = startX; x <= worldRegion.Right(); x += step) {
                        const DocPoint sp = cam.WorldToScreen(x, y);
                        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(sp.x), static_cast<FLOAT>(sp.y)), r, r), brush);
                    }
                }
            }
        } else if (pattern == PaperPattern::Millimeter) {
            drawGridLines(8.0, darkPaper ? DocColor::RGBA(36, 44, 44, 255) : DocColor::RGBA(228, 240, 232, 255), 0.7f);
            drawGridLines(40.0, darkPaper ? DocColor::RGBA(52, 68, 62, 255) : DocColor::RGBA(178, 214, 190, 255), 1.1f);
        } else if (pattern == PaperPattern::Isometric) {
            const double step = 24.0;
            if (step * cam.zoom >= 5.0) {
                const double dy = step * 0.8660254037844386;
                brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(44, 52, 66, 255) : DocColor::RGBA(206, 219, 236, 255)));
                // Vertical lines
                const double startX = std::floor(worldRegion.x / (step * 0.5)) * (step * 0.5);
                for (double x = startX; x <= worldRegion.Right(); x += step * 0.5) {
                    const DocPoint p0 = cam.WorldToScreen(x, worldRegion.y);
                    const DocPoint p1 = cam.WorldToScreen(x, worldRegion.Bottom());
                    rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                                 D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)), brush, 0.8f);
                }
                // +30 deg and -30 deg diagonals (slope = +/- 1/sqrt(3))
                const double slope = 0.5773502691896257;
                const double spanX = worldRegion.w;
                const double extraY = spanX * slope;
                const double startY = std::floor((worldRegion.y - extraY) / dy) * dy;
                for (double y = startY; y <= worldRegion.Bottom() + extraY; y += dy) {
                    const DocPoint a0 = cam.WorldToScreen(worldRegion.x, y);
                    const DocPoint a1 = cam.WorldToScreen(worldRegion.Right(), y + spanX * slope);
                    rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(a0.x), static_cast<FLOAT>(a0.y)),
                                 D2D1::Point2F(static_cast<FLOAT>(a1.x), static_cast<FLOAT>(a1.y)), brush, 0.8f);
                    const DocPoint b0 = cam.WorldToScreen(worldRegion.x, y);
                    const DocPoint b1 = cam.WorldToScreen(worldRegion.Right(), y - spanX * slope);
                    rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(b0.x), static_cast<FLOAT>(b0.y)),
                                 D2D1::Point2F(static_cast<FLOAT>(b1.x), static_cast<FLOAT>(b1.y)), brush, 0.8f);
                }
            }
        } else if (pattern == PaperPattern::LogLog) {
            const double decade = 140.0;
            if (decade * cam.zoom >= 18.0) {
                const double startX = std::floor(worldRegion.x / decade) * decade;
                const double startY = std::floor(worldRegion.y / decade) * decade;
                for (double dx0 = startX; dx0 <= worldRegion.Right(); dx0 += decade) {
                    for (int m = 1; m <= 9; ++m) {
                        const double x = dx0 + std::log10(static_cast<double>(m)) * decade;
                        brush->SetColor(ToD2DColor(m == 1
                            ? (darkPaper ? DocColor::RGBA(68, 92, 118, 255) : DocColor::RGBA(156, 188, 224, 255))
                            : (darkPaper ? DocColor::RGBA(40, 50, 66, 255) : DocColor::RGBA(218, 230, 244, 255))));
                        const DocPoint p0 = cam.WorldToScreen(x, worldRegion.y);
                        const DocPoint p1 = cam.WorldToScreen(x, worldRegion.Bottom());
                        rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                                     D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)), brush, m == 1 ? 1.2f : 0.7f);
                    }
                }
                for (double dy0 = startY; dy0 <= worldRegion.Bottom(); dy0 += decade) {
                    for (int m = 1; m <= 9; ++m) {
                        const double y = dy0 + (1.0 - std::log10(static_cast<double>(m))) * decade;
                        brush->SetColor(ToD2DColor(m == 1
                            ? (darkPaper ? DocColor::RGBA(68, 92, 118, 255) : DocColor::RGBA(156, 188, 224, 255))
                            : (darkPaper ? DocColor::RGBA(40, 50, 66, 255) : DocColor::RGBA(218, 230, 244, 255))));
                        const DocPoint p0 = cam.WorldToScreen(worldRegion.x, y);
                        const DocPoint p1 = cam.WorldToScreen(worldRegion.Right(), y);
                        rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                                     D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)), brush, m == 1 ? 1.2f : 0.7f);
                    }
                }
            }
        } else if (pattern == PaperPattern::Polar) {
            const double cx = worldRegion.x + worldRegion.w * 0.5;
            const double cy = worldRegion.y + worldRegion.h * 0.5;
            const double maxR = (std::min)(worldRegion.w, worldRegion.h) * 0.44;
            const DocPoint cScr = cam.WorldToScreen(cx, cy);
            brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(56, 72, 96, 255) : DocColor::RGBA(190, 210, 232, 255)));
            for (int ring = 1; ring <= 8; ++ring) {
                const float rPx = static_cast<float>((maxR * ring / 8.0) * cam.zoom);
                rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y)), rPx, rPx),
                    brush, (ring % 2 == 0) ? 1.1f : 0.7f);
            }
            for (int deg = 0; deg < 360; deg += 30) {
                const double rad = deg * (3.14159265358979323846 / 180.0);
                const DocPoint ep = cam.WorldToScreen(cx + maxR * std::cos(rad), cy + maxR * std::sin(rad));
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y)),
                             D2D1::Point2F(static_cast<FLOAT>(ep.x), static_cast<FLOAT>(ep.y)), brush, (deg % 90 == 0) ? 1.2f : 0.7f);
            }
        } else if (pattern == PaperPattern::SmithChart) {
            const double cx = worldRegion.x + worldRegion.w * 0.5;
            const double cy = worldRegion.y + worldRegion.h * 0.5;
            const double R = (std::min)(worldRegion.w, worldRegion.h) * 0.42;
            const DocPoint cScr = cam.WorldToScreen(cx, cy);
            const float rPx = static_cast<float>(R * cam.zoom);
            brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(72, 118, 128, 240) : DocColor::RGBA(146, 186, 198, 255)));
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y)), rPx, rPx), brush, 1.4f);
            // Horizontal real axis
            rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(cScr.x) - rPx, static_cast<FLOAT>(cScr.y)),
                         D2D1::Point2F(static_cast<FLOAT>(cScr.x) + rPx, static_cast<FLOAT>(cScr.y)), brush, 1.1f);
            // Constant-resistance circles: center (r/(1+r), 0), radius 1/(1+r)
            for (double rv : { 0.2, 0.5, 1.0, 2.0, 5.0 }) {
                const double rcX = cx + (rv / (1.0 + rv)) * R;
                const float radPx = static_cast<float>((1.0 / (1.0 + rv)) * R * cam.zoom);
                const DocPoint sc = cam.WorldToScreen(rcX, cy);
                rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(sc.x), static_cast<FLOAT>(sc.y)), radPx, radPx),
                    brush, (std::abs(rv - 1.0) < 0.01) ? 1.2f : 0.8f);
            }
            // Constant-reactance arcs clipped inside unit circle: center (1, +/-1/x), radius 1/|x|
            for (double xv : { -2.0, -1.0, -0.5, 0.5, 1.0, 2.0 }) {
                const double xc = 1.0;
                const double yc = 1.0 / xv;
                const double radN = std::abs(1.0 / xv);
                DocPoint prev{};
                bool havePrev = false;
                for (int s = 0; s <= 48; ++s) {
                    const double ang = static_cast<double>(s) * (6.28318530717958647692 / 48.0);
                    const double gx = xc + radN * std::cos(ang);
                    const double gy = yc + radN * std::sin(ang);
                    if (gx * gx + gy * gy <= 1.005) {
                        const DocPoint cur = cam.WorldToScreen(cx + gx * R, cy - gy * R);
                        if (havePrev) {
                            rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(prev.x), static_cast<FLOAT>(prev.y)),
                                         D2D1::Point2F(static_cast<FLOAT>(cur.x), static_cast<FLOAT>(cur.y)), brush, 0.75f);
                        }
                        prev = cur;
                        havePrev = true;
                    } else {
                        havePrev = false;
                    }
                }
            }
        } else if (pattern == PaperPattern::Cornell) {
            const double headerY = worldRegion.y + 72.0;
            const double summaryY = worldRegion.Bottom() - 108.0;
            const double cueX = worldRegion.x + worldRegion.w * 0.28;
            // Ruled main body
            brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(44, 52, 66, 255) : DocColor::RGBA(214, 226, 244, 255)));
            for (double y = headerY + 24.0; y < summaryY; y += 24.0) {
                const DocPoint p0 = cam.WorldToScreen(cueX, y);
                const DocPoint p1 = cam.WorldToScreen(worldRegion.Right() - 24.0, y);
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                             D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)), brush, 0.9f);
            }
            // Cornell dividers
            brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(156, 76, 86, 220) : DocColor::RGBA(218, 124, 134, 255)));
            const DocPoint h0 = cam.WorldToScreen(worldRegion.x + 18.0, headerY);
            const DocPoint h1 = cam.WorldToScreen(worldRegion.Right() - 18.0, headerY);
            rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(h0.x), static_cast<FLOAT>(h0.y)),
                         D2D1::Point2F(static_cast<FLOAT>(h1.x), static_cast<FLOAT>(h1.y)), brush, 1.4f);
            const DocPoint s0 = cam.WorldToScreen(worldRegion.x + 18.0, summaryY);
            const DocPoint s1 = cam.WorldToScreen(worldRegion.Right() - 18.0, summaryY);
            rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(s0.x), static_cast<FLOAT>(s0.y)),
                         D2D1::Point2F(static_cast<FLOAT>(s1.x), static_cast<FLOAT>(s1.y)), brush, 1.4f);
            const DocPoint c0 = cam.WorldToScreen(cueX, headerY);
            const DocPoint c1 = cam.WorldToScreen(cueX, summaryY);
            rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(c0.x), static_cast<FLOAT>(c0.y)),
                         D2D1::Point2F(static_cast<FLOAT>(c1.x), static_cast<FLOAT>(c1.y)), brush, 1.4f);
        } else if (pattern == PaperPattern::MusicStaff) {
            const double staffGap = 72.0;
            const double lineStep = 7.5;
            if (lineStep * cam.zoom >= 2.5) {
                brush->SetColor(ToD2DColor(darkPaper ? DocColor::RGBA(72, 82, 100, 255) : DocColor::RGBA(168, 180, 198, 255)));
                for (double base = worldRegion.y + 52.0; base + 4.0 * lineStep < worldRegion.Bottom() - 28.0; base += staffGap) {
                    for (int ln = 0; ln < 5; ++ln) {
                        const double y = base + ln * lineStep;
                        const DocPoint p0 = cam.WorldToScreen(worldRegion.x + 36.0, y);
                        const DocPoint p1 = cam.WorldToScreen(worldRegion.Right() - 36.0, y);
                        rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(p0.x), static_cast<FLOAT>(p0.y)),
                                     D2D1::Point2F(static_cast<FLOAT>(p1.x), static_cast<FLOAT>(p1.y)), brush, 0.9f);
                    }
                }
            }
        }
    }

    void DrawAnnotationObject(
        ID2D1RenderTarget* rt,
        ID2D1SolidColorBrush* brush,
        const DocumentModel& doc,
        const AnnotationObject& obj,
        const Camera2D& cam,
        double originWorldX,
        double originWorldY,
        bool isSelected,
        bool cacheGpuAssets,
        const ViewportOverlayState* overlay = nullptr)
    {
        double layerOpacity = 1.0;
        if (!RenderCommon::ShouldDrawObjectOnLayer(doc, obj, layerOpacity, overlay)) return;
        layerOpacity *= std::clamp(obj.opacity, 0.05, 1.0);

        auto toScreen = [&](double lx, double ly) -> D2D1_POINT_2F {
            const DocPoint sp = cam.WorldToScreen(originWorldX + lx, originWorldY + ly);
            return D2D1::Point2F(static_cast<FLOAT>(sp.x), static_cast<FLOAT>(sp.y));
        };
        auto rectToScreen = [&](const DocRect& lr) -> D2D1_RECT_F {
            const DocRect wr{ originWorldX + lr.x, originWorldY + lr.y, lr.w, lr.h };
            return ToD2DRect(cam.WorldRectToScreen(wr));
        };

        // Optional object rotation transform (F-107, F-131)
        D2D1::Matrix3x2F savedTransform = D2D1::Matrix3x2F::Identity();
        const bool applyRotation = (std::abs(obj.rotationDeg) > 0.15);
        if (applyRotation) {
            rt->GetTransform(&savedTransform);
            const D2D1_RECT_F rotBox = rectToScreen(obj.bounds);
            const D2D1_POINT_2F center = D2D1::Point2F((rotBox.left + rotBox.right) * 0.5f, (rotBox.top + rotBox.bottom) * 0.5f);
            rt->SetTransform(D2D1::Matrix3x2F::Rotation(static_cast<FLOAT>(obj.rotationDeg), center) * savedTransform);
        }

        if (obj.kind == ObjectKind::InkStroke) {
            const std::vector<InkPoint> filteredPts = (overlay && overlay->replayActive)
                ? InkEngine::FilterInkPointsAtReplayTime(obj, overlay->replayCursorMs)
                : obj.inkPoints;
            if (filteredPts.empty()) return;

            DocColor baseCol = RenderCommon::ApplyLayerOpacity(ResolveAccessibleColor(obj.strokeColor, overlay), layerOpacity);
            if ((obj.isHighlighter || obj.penStyle == InkPenStyle::Highlighter) && baseCol.a > 140) {
                baseCol.a = 98;
            }
            if (filteredPts.size() == 1) {
                brush->SetColor(ToD2DColor(baseCol));
                const float r = (std::max)(1.0f, static_cast<float>(obj.strokeWidth * 0.5 * cam.zoom));
                rt->FillEllipse(D2D1::Ellipse(toScreen(filteredPts[0].x, filteredPts[0].y), r, r), brush);
            } else {
                // Tinta translúcida (resaltador u opacidad < 100 %): el trazo entero se pinta opaco
                // dentro de una capa que lleva su transparencia, así se aplica una sola vez
                // (revisión del 27-nov-2026). Antes cada segmento llevaba su propia transparencia,
                // se enciman con las puntas redondas y el resaltador salía casi opaco.
                ComPtr<ID2D1Layer> inkLayer;
                if (baseCol.a < 255 && SUCCEEDED(rt->CreateLayer(nullptr, &inkLayer))) {
                    D2D1_RECT_F lb = rectToScreen(obj.bounds);
                    const float pad = static_cast<float>(obj.strokeWidth * cam.zoom * 2.0 + 4.0);
                    lb.left -= pad; lb.top -= pad; lb.right += pad; lb.bottom += pad;
                    rt->PushLayer(D2D1::LayerParameters(lb, nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                      D2D1::IdentityMatrix(), static_cast<FLOAT>(baseCol.a) / 255.0f),
                                  inkLayer.Get());
                }
                for (std::size_t i = 1; i < filteredPts.size(); ++i) {
                    const auto& p0 = filteredPts[i - 1];
                    const auto& p1 = filteredPts[i];
                    const double segDist = std::hypot(p1.x - p0.x, p1.y - p0.y);
                    const double dtMs = (p1.timestampMs > p0.timestampMs)
                        ? static_cast<double>(p1.timestampMs - p0.timestampMs) : 16.0;
                    const double speed = (segDist * 1000.0) / (std::max)(1.0, dtMs);
                    const InkPoint mid{
                        0.5 * (p0.x + p1.x),
                        0.5 * (p0.y + p1.y),
                        0.5f * (p0.pressure + p1.pressure),
                        0.5f * (p0.tiltXDeg + p1.tiltXDeg),
                        0.5f * (p0.tiltYDeg + p1.tiltYDeg),
                        p1.timestampMs
                    };
                    const double widthWorld = InkEngine::ComputeEffectiveWidth(mid, obj, speed);
                    DocColor segCol = baseCol;
                    if (inkLayer) {
                        segCol.a = 255; // la capa ya aplica la transparencia una sola vez
                    } else {
                        const double tiltAlpha = InkEngine::ComputeTiltAlphaMultiplier(mid, obj.penStyle);
                        segCol.a = static_cast<std::uint8_t>(std::clamp(std::lround(baseCol.a * tiltAlpha), 1L, 255L));
                    }
                    brush->SetColor(ToD2DColor(segCol));
                    const float wPx = (std::max)(0.8f, static_cast<float>(widthWorld * cam.zoom));
                    rt->DrawLine(toScreen(p0.x, p0.y), toScreen(p1.x, p1.y), brush, wPx, roundStroke.Get());
                }
                if (inkLayer) rt->PopLayer();
            }
        } else if (obj.kind == ObjectKind::Shape) {
            const float wPx = (std::max)(1.0f, static_cast<float>(obj.strokeWidth * cam.zoom));
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            if (obj.shapeKind == ShapeKind::Rectangle || obj.shapeKind == ShapeKind::SignatureStamp) {
                if (obj.isRedaction) {
                    brush->SetColor(ToD2DColor(DocColor::RGBA(10, 12, 16, 255)));
                    rt->FillRectangle(sr, brush);
                } else {
                    if (obj.fillColor.a > 0) {
                        brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(ResolveAccessibleColor(obj.fillColor, overlay), layerOpacity)));
                        rt->FillRectangle(sr, brush);
                    }
                    brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(ResolveAccessibleColor(obj.strokeColor, overlay), layerOpacity)));
                    rt->DrawRectangle(sr, brush, wPx);
                }
                if (obj.shapeKind == ShapeKind::SignatureStamp && !obj.textUtf8.empty() && dwFactory) {
                    const D2D1_RECT_F inner = D2D1::RectF(sr.left + 3.0f, sr.top + 3.0f, sr.right - 3.0f, sr.bottom - 3.0f);
                    rt->DrawRectangle(inner, brush, 0.8f);
                    const float fontPx = (std::max)(8.0f, static_cast<float>((obj.fontSizePt > 0 ? obj.fontSizePt : 10.5) * cam.zoom));
                    ComPtr<IDWriteTextFormat> fmt;
                    if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontPx, L"es-ES", &fmt)) && fmt) {
                        const std::wstring wtxt = Utf8ToWide(obj.textUtf8);
                        rt->DrawTextW(wtxt.c_str(), static_cast<UINT32>(wtxt.size()), fmt.Get(),
                            D2D1::RectF(inner.left + 4.0f, inner.top + 4.0f, inner.right - 4.0f, inner.bottom - 4.0f), brush);
                    }
                }
            } else if (obj.shapeKind == ShapeKind::Ellipse) {
                const D2D1_ELLIPSE ell = D2D1::Ellipse(
                    D2D1::Point2F((sr.left + sr.right) * 0.5f, (sr.top + sr.bottom) * 0.5f),
                    (sr.right - sr.left) * 0.5f,
                    (sr.bottom - sr.top) * 0.5f);
                if (obj.fillColor.a > 0) {
                    brush->SetColor(ToD2DColor(ResolveAccessibleColor(obj.fillColor, overlay)));
                    rt->FillEllipse(ell, brush);
                }
                brush->SetColor(ToD2DColor(ResolveAccessibleColor(obj.strokeColor, overlay)));
                rt->DrawEllipse(ell, brush, wPx);
            } else {
                const D2D1_POINT_2F p0 = toScreen(obj.shapeStart.x, obj.shapeStart.y);
                const D2D1_POINT_2F p1 = toScreen(obj.shapeEnd.x, obj.shapeEnd.y);
                brush->SetColor(ToD2DColor(ResolveAccessibleColor(obj.strokeColor, overlay)));
                rt->DrawLine(p0, p1, brush, wPx, roundStroke.Get());
                if (obj.shapeKind == ShapeKind::Arrow || obj.shapeKind == ShapeKind::Connector) {
                    const double dx = p1.x - p0.x;
                    const double dy = p1.y - p0.y;
                    const double len = std::hypot(dx, dy);
                    if (len > 2.0) {
                        const double ux = dx / len, uy = dy / len;
                        const double hs = (std::max)(8.0, obj.strokeWidth * 3.2 * cam.zoom);
                        const double px = -uy * (hs * 0.45), py = ux * (hs * 0.45);
                        const D2D1_POINT_2F a = D2D1::Point2F(static_cast<FLOAT>(p1.x - ux * hs + px), static_cast<FLOAT>(p1.y - uy * hs + py));
                        const D2D1_POINT_2F b = D2D1::Point2F(static_cast<FLOAT>(p1.x - ux * hs - px), static_cast<FLOAT>(p1.y - uy * hs - py));
                        rt->DrawLine(a, p1, brush, wPx, roundStroke.Get());
                        rt->DrawLine(b, p1, brush, wPx, roundStroke.Get());
                    }
                    if (obj.shapeKind == ShapeKind::Connector) {
                        rt->FillEllipse(D2D1::Ellipse(p0, 3.5f, 3.5f), brush);
                        rt->FillEllipse(D2D1::Ellipse(p1, 3.5f, 3.5f), brush);
                    }
                }
            }
        } else if (obj.kind == ObjectKind::TextBox) {
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            if (obj.fillColor.a > 0) {
                brush->SetColor(ToD2DColor(obj.fillColor));
                rt->FillRectangle(sr, brush);
            }
            if (dwFactory && !obj.textUtf8.empty()) {
                const float fontPx = (std::max)(6.0f, static_cast<float>(obj.fontSizePt * cam.zoom));
                if (MathTypeset::TextContainsMathMarkup(obj.textUtf8)) {
                    rt->PushAxisAlignedClip(sr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    D2DMathBackend mbk(rt, brush, dwFactory.Get());
                    std::istringstream iss(obj.textUtf8);
                    std::string line;
                    double curY = sr.top + 2.0;
                    const double maxW = (std::max)(12.0, static_cast<double>(sr.right - sr.left) - 4.0);
                    while (std::getline(iss, line) && curY < sr.bottom) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        curY += MathTypeset::DrawRichMathLine(mbk, line, sr.left + 2.0, curY, maxW, fontPx, obj.strokeColor, false);
                    }
                    rt->PopAxisAlignedClip();
                } else {
                    const bool dysMode = (overlay && overlay->dyslexiaFriendlyMode);
                    const float effFontPx = dysMode ? (fontPx * 1.12f) : fontPx;
                    brush->SetColor(ToD2DColor(ResolveAccessibleColor(obj.strokeColor, overlay)));
                    const std::wstring wfam = dysMode
                        ? L"Verdana"
                        : Utf8ToWide(obj.fontFamilyUtf8.empty() ? "Segoe UI" : obj.fontFamilyUtf8);
                    ComPtr<IDWriteTextFormat> fmt;
                    if (SUCCEEDED(dwFactory->CreateTextFormat(
                            wfam.c_str(), nullptr,
                            obj.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                            obj.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                            DWRITE_FONT_STRETCH_NORMAL, effFontPx, L"es-ES", &fmt)) && fmt) {
                        if (dysMode) {
                            fmt->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, effFontPx * 1.48f, effFontPx * 1.15f);
                            fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                        }
                        const std::wstring wtxt = Utf8ToWide(obj.textUtf8);
                        const std::vector<InlineUrlSpan> inlineUrls = ExtractSafeInlineUrls(obj.textUtf8);
                        if (obj.underline || !inlineUrls.empty()) {
                            ComPtr<IDWriteTextLayout> layout;
                            if (SUCCEEDED(dwFactory->CreateTextLayout(
                                    wtxt.c_str(), static_cast<UINT32>(wtxt.size()), fmt.Get(),
                                    (std::max)(4.0f, sr.right - sr.left), (std::max)(4.0f, sr.bottom - sr.top), &layout)) && layout) {
                                if (obj.underline) {
                                    layout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ 0, static_cast<UINT32>(wtxt.size()) });
                                }
                                if (!inlineUrls.empty()) {
                                    const bool isDarkTxt = (overlay && overlay->darkTheme);
                                    const bool isHcTxt = (overlay && overlay->highContrast);
                                    const DocColor linkCol = isHcTxt
                                        ? DocColor::RGBA(0, 54, 196, 255)
                                        : (isDarkTxt ? DocColor::RGBA(96, 178, 255, 255) : DocColor::RGBA(0, 96, 214, 255));
                                    ComPtr<ID2D1SolidColorBrush> linkBr;
                                    rt->CreateSolidColorBrush(ToD2DColor(ResolveAccessibleColor(linkCol, overlay)), &linkBr);
                                    for (const auto& u : inlineUrls) {
                                        if (u.utf16Start < wtxt.size() && u.utf16Len > 0) {
                                            const UINT32 len = static_cast<UINT32>((std::min)(u.utf16Len, wtxt.size() - u.utf16Start));
                                            const DWRITE_TEXT_RANGE rng{ static_cast<UINT32>(u.utf16Start), len };
                                            layout->SetUnderline(TRUE, rng);
                                            if (linkBr) layout->SetDrawingEffect(linkBr.Get(), rng);
                                        }
                                    }
                                }
                                rt->DrawTextLayout(D2D1::Point2F(sr.left, sr.top), layout.Get(), brush);
                            } else {
                                rt->DrawTextW(wtxt.c_str(), static_cast<UINT32>(wtxt.size()), fmt.Get(), sr, brush);
                            }
                        } else {
                            rt->DrawTextW(wtxt.c_str(), static_cast<UINT32>(wtxt.size()), fmt.Get(), sr, brush);
                        }
                    }
                }
            }
        } else if (obj.kind == ObjectKind::Image) {
            const std::vector<std::uint8_t>* bytesPtr = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const D2D1_SIZE_F rtSz = rt->GetSize();
            const bool onScreen = (sr.right >= 0.0f && sr.bottom >= 0.0f && sr.left <= rtSz.width && sr.top <= rtSz.height);
            if (!onScreen) {
                // Skip offscreen image/GIF decode & avoid keeping GIF animation timer awake
            } else if (bytesPtr && !bytesPtr->empty()) {
                const std::string baseTag = !obj.assetKey.empty() ? obj.assetKey : ("inline_" + std::to_string(obj.id));
                const std::string tag = baseTag + "#" + std::to_string(bytesPtr->size()) + "#" + std::to_string(HashBytesFnv1a64(*bytesPtr));
                ComPtr<ID2D1Bitmap> bmp = AcquireGpuBitmapForEncodedAsset(
                    rt, tag, *bytesPtr, cacheGpuAssets,
                    overlay ? overlay->animationTick : 0u,
                    overlay ? overlay->animationClockMs : 0ull,
                    obj.gifPaused);
                if (bmp) {
                    const D2D1_SIZE_F bsz = bmp->GetSize();
                    NormalizedCropRect ic = obj.imageCrop;
                    ic.Clamp();
                    const D2D1_RECT_F srcR = D2D1::RectF(
                        static_cast<FLOAT>(ic.left * bsz.width),
                        static_cast<FLOAT>(ic.top * bsz.height),
                        static_cast<FLOAT>(ic.right * bsz.width),
                        static_cast<FLOAT>(ic.bottom * bsz.height));
                    rt->DrawBitmap(bmp.Get(), sr, static_cast<FLOAT>(layerOpacity), D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &srcR);
                }
            } else {
                brush->SetColor(ToD2DColor(DocColor::RGBA(200, 210, 224, 255)));
                rt->FillRectangle(sr, brush);
            }
        } else if (obj.kind == ObjectKind::PdfCard) {
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const bool isHcCard = (overlay && overlay->highContrast);
            // Subtle guías de interfaz sheet elevation shadow
            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(
                isDarkCard ? DocColor::RGBA(0, 0, 0, 76) : DocColor::RGBA(15, 23, 42, 24), layerOpacity)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(sr.left + 1.0f, sr.top + 2.5f, sr.right + 1.5f, sr.bottom + 3.0f), 3.0f, 3.0f), brush);

            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(DocColor::RGBA(255, 255, 255, 255), layerOpacity)));
            rt->FillRectangle(sr, brush);
            bool drewCardRaster = false;
            const std::vector<std::uint8_t>* bytesPtr = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
            if (bytesPtr && !bytesPtr->empty()) {
                const std::string tag = !obj.assetKey.empty() ? obj.assetKey : ("pdfcard_" + std::to_string(obj.id));
                if (ComPtr<ID2D1Bitmap> bmp = AcquireGpuBitmapForEncodedAsset(rt, tag, *bytesPtr, cacheGpuAssets)) {
                    rt->DrawBitmap(bmp.Get(), sr, static_cast<FLOAT>(layerOpacity));
                    drewCardRaster = true;
                }
            } else if (!obj.pdfSourcePathUtf8.empty() || !obj.pdfSourceAssetKey.empty()) {
                const std::string resolvedPdfPath = ResolvePdfSourcePathForRender(doc, obj.pdfSourcePathUtf8, obj.pdfSourceAssetKey);
                if (!resolvedPdfPath.empty()) {
                    const int targetW = static_cast<int>(std::lround((std::max)(80.0, static_cast<double>(sr.right - sr.left))));
                    PageCacheKey key{};
                    key.pageId = 0x8000000000000000ull | obj.id;
                    key.sourcePdfPath = resolvedPdfPath;
                    key.sourcePageIndex0 = obj.pdfSourcePageIndex0;
                    key.widthBucketPx = QuantizeBucketPx(targetW);
                    key.rotationDeg = 0;
                    key.cropL = static_cast<int>(std::lround(obj.imageCrop.left * 1000.0));
                    key.cropT = static_cast<int>(std::lround(obj.imageCrop.top * 1000.0));
                    key.cropR = static_cast<int>(std::lround(obj.imageCrop.right * 1000.0));
                    key.cropB = static_cast<int>(std::lround(obj.imageCrop.bottom * 1000.0));

                    RasterImageBgra32 img;
                    bool haveRaster = activePageCache && activePageCache->TryGet(key, img);
                    if (!haveRaster && activeSyncOffscreen) {
                        NormalizedCropRect c = obj.imageCrop;
                        c.Clamp();
                        haveRaster = WinRtPdfRenderer::RenderPageToBgra32(
                            Utf8ToWide(resolvedPdfPath), obj.pdfSourcePageIndex0,
                            key.widthBucketPx, key.rotationDeg, c, img);
                        if (haveRaster && activePageCache) activePageCache->PutDirect(key, img);
                    }
                    if (haveRaster && img.Valid()) {
                        ComPtr<ID2D1Bitmap> gpuBmp;
                        if (!activeSyncOffscreen) {
                            for (const auto& entry : pageGpuBitmaps) {
                                if (entry.key == key) { gpuBmp = entry.bmp; break; }
                            }
                        }
                        if (!gpuBmp) {
                            gpuBmp.Attach(UploadBgra32(rt, img));
                            if (gpuBmp && !activeSyncOffscreen) {
                                if (pageGpuBitmaps.size() >= 16) pageGpuBitmaps.erase(pageGpuBitmaps.begin());
                                pageGpuBitmaps.push_back({ key, gpuBmp });
                            }
                        }
                        if (gpuBmp) {
                            rt->DrawBitmap(gpuBmp.Get(), sr, static_cast<FLOAT>(layerOpacity));
                            drewCardRaster = true;
                        }
                    } else if (activePageCache && !activeSyncOffscreen) {
                        activePageCache->RequestAsync(key, activeAsyncGen, activeOnAsyncReady);
                    }
                }
            }
            // Neutral 1px paper border (high-contrast aware)
            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(
                isHcCard ? DocColor::RGBA(25, 32, 44, 235) : DocColor::RGBA(198, 208, 222, 185), layerOpacity)));
            rt->DrawRectangle(sr, brush, isHcCard ? 1.35f : 1.0f);

            // Vector padlock indicator in top-right corner when pinned and selected
            if (isSelected && obj.lockedInPlace) {
                const float bx = sr.right - 22.0f;
                const float by = sr.top + 6.0f;
                brush->SetColor(ToD2DColor(DocColor::RGBA(15, 23, 42, 185)));
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(bx, by, bx + 16.0f, by + 16.0f), 4.0f, 4.0f), brush);
                brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 245)));
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(bx + 4.0f, by + 7.5f, bx + 12.0f, by + 13.5f), 1.5f, 1.5f), brush);
                rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(bx + 8.0f, by + 6.5f), 2.6f, 2.8f), brush, 1.3f);
            }
            if (!drewCardRaster && dwFactory) {
                ComPtr<IDWriteTextFormat> fmt;
                if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (std::max)(8.0f, static_cast<float>(12.0 * cam.zoom)), L"es-ES", &fmt)) && fmt) {
                    brush->SetColor(ToD2DColor(DocColor::RGBA(30, 41, 59, 255)));
                    const std::wstring title = Utf8ToWide(obj.captionUtf8.empty()
                        ? ("P\xC3\xA1g. " + std::to_string(obj.pdfSourcePageIndex0 + 1))
                        : obj.captionUtf8);
                    rt->DrawTextW(title.c_str(), static_cast<UINT32>(title.size()), fmt.Get(),
                        D2D1::RectF(sr.left + 8.0f, sr.top + 8.0f, sr.right - 8.0f, sr.bottom - 8.0f), brush);
                }
            }
        } else if (obj.kind == ObjectKind::StudyTape ||
                   obj.kind == ObjectKind::MathBlock ||
                   obj.kind == ObjectKind::CircuitBlock ||
                   obj.kind == ObjectKind::CalcSessionCard ||
                   obj.kind == ObjectKind::PlotBlock) {
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const bool isHcCard = (overlay && overlay->highContrast);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const auto card = RenderCommon::BuildStemCardLayout(obj, isDarkCard, isHcCard);
            const float cardRad = (std::max)(4.0f, static_cast<float>(10.0 * cam.zoom));
            const D2D1_ROUNDED_RECT cardRR = D2D1::RoundedRect(sr, cardRad, cardRad);
            // Subtle elevation shadow for content card (standard material, never material translúcido)
            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(
                isDarkCard ? DocColor::RGBA(0, 0, 0, 72) : DocColor::RGBA(15, 23, 42, 22), layerOpacity)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(sr.left + 1.0f, sr.top + 2.5f, sr.right + 1.0f, sr.bottom + 2.5f), cardRad, cardRad), brush);
            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(card.bg, layerOpacity)));
            rt->FillRoundedRectangle(cardRR, brush);
            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(card.border, layerOpacity)));
            rt->DrawRoundedRectangle(cardRR, brush, (std::max)(1.0f, static_cast<float>((isHcCard ? 1.55 : 1.25) * cam.zoom)));

            const float pad = static_cast<float>(6.0 * cam.zoom);
            if (dwFactory) {
                rt->PushAxisAlignedClip(sr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                D2DMathBackend mbk(rt, brush, dwFactory.Get());
                const DocRect cardBox{
                    static_cast<double>(sr.left), static_cast<double>(sr.top),
                    static_cast<double>(sr.right - sr.left), static_cast<double>(sr.bottom - sr.top)
                };
                MathTypeset::DrawStemCardContent(
                    mbk, obj, cardBox, cam.zoom,
                    RenderCommon::ApplyLayerOpacity(card.titleColor, layerOpacity),
                    RenderCommon::ApplyLayerOpacity(card.bodyColor, layerOpacity),
                    card.headerUtf8, card.bodyUtf8, isDarkCard);
                rt->PopAxisAlignedClip();
            }

            if (obj.kind == ObjectKind::CalcSessionCard && !obj.pngBytes.empty()) {
                const float imgW = (std::min)(static_cast<float>((sr.right - sr.left) * 0.42f), static_cast<float>(130.0 * cam.zoom));
                const float imgH = (std::min)(static_cast<float>((sr.bottom - sr.top) * 0.60f), imgW * 0.55f);
                if (imgW > 18.0f && imgH > 12.0f) {
                    const D2D1_RECT_F lcdR = D2D1::RectF(sr.right - imgW - pad, sr.top + pad * 3.5f, sr.right - pad, sr.top + pad * 3.5f + imgH);
                    if (ComPtr<ID2D1Bitmap> bmp = AcquireGpuBitmapForEncodedAsset(rt, "calc_lcd_" + std::to_string(obj.id), obj.pngBytes, cacheGpuAssets)) {
                        rt->DrawBitmap(bmp.Get(), lcdR);
                    }
                }
            } else if (obj.kind == ObjectKind::PlotBlock) {
                const auto plot = RenderCommon::BuildPlotRenderData(obj);
                const float px0 = sr.left + pad * 1.2f;
                const float py0 = sr.top + pad * 4.2f;
                const float px1 = (std::max)(px0 + 16.0f, sr.right - pad * 1.2f);
                const float py1 = (std::max)(py0 + 16.0f, sr.bottom - pad * 3.0f);
                brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(19, 23, 33, 245) : DocColor::RGBA(255, 255, 255, 240)));
                rt->FillRectangle(D2D1::RectF(px0, py0, px1, py1), brush);
                brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(71, 85, 105, 235)
                                         : (isHcCard ? DocColor::RGBA(30, 41, 59, 255) : DocColor::RGBA(148, 163, 184, 255))));
                rt->DrawRectangle(D2D1::RectF(px0, py0, px1, py1), brush, 1.0f);

                auto mapPt = [&](double wx, double wy) -> D2D1_POINT_2F {
                    const double nx = (wx - plot.xMin) / (std::max)(1e-9, plot.xMax - plot.xMin);
                    const double ny = (wy - plot.yMin) / (std::max)(1e-9, plot.yMax - plot.yMin);
                    return D2D1::Point2F(
                        static_cast<FLOAT>(px0 + std::clamp(nx, 0.0, 1.0) * (px1 - px0)),
                        static_cast<FLOAT>(py1 - std::clamp(ny, 0.0, 1.0) * (py1 - py0)));
                };
                if (plot.is3D && !plot.surface3DSegments.empty()) {
                    const float segW = (std::max)(1.0f, static_cast<float>(1.35 * cam.zoom));
                    for (const auto& seg : plot.surface3DSegments) {
                        brush->SetColor(ToD2DColor(seg.color));
                        rt->DrawLine(mapPt(seg.a.x, seg.a.y), mapPt(seg.b.x, seg.b.y), brush, segW);
                    }
                } else {
                    if (plot.xMin <= 0.0 && plot.xMax >= 0.0) {
                        const auto a = mapPt(0.0, plot.yMin), b = mapPt(0.0, plot.yMax);
                        rt->DrawLine(a, b, brush, 0.9f);
                    }
                    if (plot.yMin <= 0.0 && plot.yMax >= 0.0) {
                        const auto a = mapPt(plot.xMin, 0.0), b = mapPt(plot.xMax, 0.0);
                        rt->DrawLine(a, b, brush, 0.9f);
                    }
                    brush->SetColor(ToD2DColor(ResolveAccessibleColor(
                        isDarkCard ? DocColor::RGBA(129, 140, 248, 255)
                                   : (isHcCard ? DocColor::RGBA(0, 64, 221, 255) : DocColor::RGBA(79, 70, 229, 255)),
                        overlay)));
                    for (std::size_t i = 1; i < plot.curvePoints.size(); ++i) {
                        rt->DrawLine(mapPt(plot.curvePoints[i - 1].x, plot.curvePoints[i - 1].y),
                                     mapPt(plot.curvePoints[i].x, plot.curvePoints[i].y), brush, (std::max)(1.3f, static_cast<float>(1.8 * cam.zoom)));
                    }
                    brush->SetColor(ToD2DColor(ResolveAccessibleColor(
                        isDarkCard ? DocColor::RGBA(248, 113, 113, 255) : DocColor::RGBA(220, 38, 38, 255),
                        overlay)));
                    for (const auto& sp : plot.scatterPoints) {
                        rt->FillEllipse(D2D1::Ellipse(mapPt(sp.x, sp.y), 3.0f, 3.0f), brush);
                    }
                }
            }
        } else if (obj.kind == ObjectKind::TableBlock) {
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const float rad = (std::max)(4.0f, static_cast<float>(8.0 * cam.zoom));
            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(
                isDarkCard ? DocColor::RGBA(28, 34, 46, 246) : DocColor::RGBA(255, 255, 255, 250), layerOpacity)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush);
            brush->SetColor(ToD2DColor(RenderCommon::ApplyLayerOpacity(
                isDarkCard ? DocColor::RGBA(72, 86, 108, 235) : DocColor::RGBA(198, 208, 224, 255), layerOpacity)));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush, 1.1f);

            std::vector<std::vector<std::string>> rows;
            if (!obj.tableJsonUtf8.empty()) {
                try {
                    const auto tj = nlohmann::json::parse(obj.tableJsonUtf8);
                    if (tj.contains("rows") && tj["rows"].is_array()) {
                        for (const auto& rj : tj["rows"]) {
                            if (!rj.is_array()) continue;
                            std::vector<std::string> rv;
                            for (const auto& cj : rj) rv.push_back(cj.is_string() ? cj.get<std::string>() : cj.dump());
                            rows.push_back(std::move(rv));
                        }
                    }
                } catch (...) {}
            }
            if (rows.empty() && !obj.textUtf8.empty()) {
                std::istringstream iss(obj.textUtf8);
                std::string ln;
                while (std::getline(iss, ln)) {
                    std::vector<std::string> rv;
                    std::istringstream ls(ln);
                    std::string cell;
                    while (std::getline(ls, cell, '|')) rv.push_back(cell);
                    if (!rv.empty()) rows.push_back(std::move(rv));
                }
            }
            if (!rows.empty()) {
                std::size_t maxCols = 1;
                for (const auto& r : rows) maxCols = (std::max)(maxCols, r.size());
                const float cellW = (sr.right - sr.left) / static_cast<float>(maxCols);
                const float cellH = (sr.bottom - sr.top) / static_cast<float>(rows.size());
                // Header band tint
                brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(40, 50, 68, 230) : DocColor::RGBA(241, 245, 250, 245)));
                rt->FillRectangle(D2D1::RectF(sr.left + 1.0f, sr.top + 1.0f, sr.right - 1.0f, sr.top + cellH), brush);
                brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(64, 76, 96, 200) : DocColor::RGBA(214, 222, 234, 255)));
                for (std::size_t r = 1; r < rows.size(); ++r) {
                    const float y = sr.top + static_cast<float>(r) * cellH;
                    rt->DrawLine(D2D1::Point2F(sr.left, y), D2D1::Point2F(sr.right, y), brush, 0.8f);
                }
                for (std::size_t c = 1; c < maxCols; ++c) {
                    const float x = sr.left + static_cast<float>(c) * cellW;
                    rt->DrawLine(D2D1::Point2F(x, sr.top), D2D1::Point2F(x, sr.bottom), brush, 0.8f);
                }
                if (dwFactory) {
                    const float fpx = std::clamp(static_cast<float>(10.5 * cam.zoom), 7.5f, 18.0f);
                    ComPtr<IDWriteTextFormat> fmtN, fmtB;
                    dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fpx, L"es-ES", &fmtN);
                    dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fpx, L"es-ES", &fmtB);
                    brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(238, 242, 250, 255) : DocColor::RGBA(24, 30, 42, 255)));
                    rt->PushAxisAlignedClip(sr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    for (std::size_t r = 0; r < rows.size(); ++r) {
                        for (std::size_t c = 0; c < rows[r].size(); ++c) {
                            const D2D1_RECT_F cr = D2D1::RectF(
                                sr.left + static_cast<float>(c) * cellW + 4.0f,
                                sr.top + static_cast<float>(r) * cellH + 2.0f,
                                sr.left + static_cast<float>(c + 1) * cellW - 4.0f,
                                sr.top + static_cast<float>(r + 1) * cellH - 2.0f);
                            const std::wstring wcell = Utf8ToWide(rows[r][c]);
                            IDWriteTextFormat* f = (r == 0 && fmtB) ? fmtB.Get() : fmtN.Get();
                            if (f) rt->DrawTextW(wcell.c_str(), static_cast<UINT32>(wcell.size()), f, cr, brush);
                        }
                    }
                    rt->PopAxisAlignedClip();
                }
            }
        } else if (obj.kind == ObjectKind::CodeBlock) {
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const float rad = (std::max)(4.0f, static_cast<float>(8.0 * cam.zoom));
            brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(22, 26, 36, 248) : DocColor::RGBA(246, 248, 252, 250)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush);
            brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(72, 84, 108, 235) : DocColor::RGBA(190, 202, 220, 255)));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush, 1.1f);
            if (dwFactory) {
                const float fpx = std::clamp(static_cast<float>(10.0 * cam.zoom), 7.5f, 16.0f);
                ComPtr<IDWriteTextFormat> fmt;
                if (SUCCEEDED(dwFactory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fpx, L"es-ES", &fmt)) && fmt) {
                    const std::string hdrLine = obj.captionUtf8.empty() ? ("[" + obj.codeLanguageUtf8 + "]") : obj.captionUtf8;
                    std::string codeBody = obj.textUtf8;
                    if (!obj.evaluatedResultUtf8.empty()) codeBody += "\n// => " + obj.evaluatedResultUtf8;
                    const std::string fullUtf8 = hdrLine + "\n" + codeBody;
                    const std::wstring wtxt = Utf8ToWide(fullUtf8);
                    const float innerW = (std::max)(20.0f, (sr.right - sr.left) - 16.0f);
                    const float innerH = (std::max)(16.0f, (sr.bottom - sr.top) - 12.0f);

                    ComPtr<IDWriteTextLayout> layout;
                    if (SUCCEEDED(dwFactory->CreateTextLayout(wtxt.c_str(), static_cast<UINT32>(wtxt.size()), fmt.Get(), innerW, innerH, &layout)) && layout) {
                        // True multi-color syntax highlighting powered by RichBlockDocument::TokenizeCodeSyntax (OB-EDIT-027)
                        auto makeColorBrush = [&](DocColor c) -> ComPtr<ID2D1SolidColorBrush> {
                            ComPtr<ID2D1SolidColorBrush> b;
                            rt->CreateSolidColorBrush(ToD2DColor(ResolveAccessibleColor(c, overlay)), &b);
                            return b;
                        };
                        ComPtr<ID2D1SolidColorBrush> bHdr = makeColorBrush(isDarkCard ? DocColor::RGBA(148, 163, 184, 255) : DocColor::RGBA(100, 116, 139, 255));
                        ComPtr<ID2D1SolidColorBrush> bKw  = makeColorBrush(isDarkCard ? DocColor::RGBA(192, 132, 252, 255) : DocColor::RGBA(147, 51, 234, 255));
                        ComPtr<ID2D1SolidColorBrush> bTyp = makeColorBrush(isDarkCard ? DocColor::RGBA(45, 212, 191, 255)  : DocColor::RGBA(13, 148, 136, 255));
                        ComPtr<ID2D1SolidColorBrush> bStr = makeColorBrush(isDarkCard ? DocColor::RGBA(134, 239, 172, 255) : DocColor::RGBA(21, 128, 61, 255));
                        ComPtr<ID2D1SolidColorBrush> bCom = makeColorBrush(isDarkCard ? DocColor::RGBA(120, 136, 158, 255) : DocColor::RGBA(115, 130, 150, 255));
                        ComPtr<ID2D1SolidColorBrush> bNum = makeColorBrush(isDarkCard ? DocColor::RGBA(251, 191, 36, 255)  : DocColor::RGBA(194, 65, 12, 255));
                        ComPtr<ID2D1SolidColorBrush> bOp  = makeColorBrush(isDarkCard ? DocColor::RGBA(96, 165, 250, 255)  : DocColor::RGBA(37, 99, 235, 255));

                        const std::wstring wHdr = Utf8ToWide(hdrLine + "\n");
                        if (bHdr && !wHdr.empty()) {
                            layout->SetDrawingEffect(bHdr.Get(), DWRITE_TEXT_RANGE{ 0u, static_cast<UINT32>(wHdr.size()) });
                            layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_RANGE{ 0u, static_cast<UINT32>(wHdr.size()) });
                        }
                        const auto tokens = RichBlockDocument::TokenizeCodeSyntax(codeBody, obj.codeLanguageUtf8);
                        // Precompute UTF-8 byte offset -> UTF-16 code unit offset map for exact DirectWrite ranges
                        std::vector<UINT32> utf8ToW(codeBody.size() + 1, static_cast<UINT32>(wHdr.size()));
                        {
                            UINT32 wOff = static_cast<UINT32>(wHdr.size());
                            for (std::size_t bi = 0; bi < codeBody.size(); ) {
                                const unsigned char ch = static_cast<unsigned char>(codeBody[bi]);
                                std::size_t step = 1;
                                UINT32 wStep = 1;
                                if ((ch & 0xE0u) == 0xC0u) step = 2;
                                else if ((ch & 0xF0u) == 0xE0u) step = 3;
                                else if ((ch & 0xF8u) == 0xF0u) { step = 4; wStep = 2; }
                                for (std::size_t k = 0; k < step && bi + k < codeBody.size(); ++k) {
                                    utf8ToW[bi + k] = wOff;
                                }
                                bi = (std::min)(codeBody.size(), bi + step);
                                wOff += wStep;
                            }
                            utf8ToW[codeBody.size()] = wOff;
                        }
                        for (const auto& tk : tokens) {
                            if (tk.kind == CodeTokenKind::Plain || tk.length == 0) continue;
                            const std::size_t b0 = (std::min)(tk.start, codeBody.size());
                            const std::size_t b1 = (std::min)(tk.start + tk.length, codeBody.size());
                            if (b1 <= b0) continue;
                            const UINT32 w0 = utf8ToW[b0];
                            const UINT32 wLen = (utf8ToW[b1] > w0) ? (utf8ToW[b1] - w0) : 0u;
                            if (wLen == 0 || w0 + wLen > wtxt.size()) continue;
                            ID2D1SolidColorBrush* tb = nullptr;
                            if (tk.kind == CodeTokenKind::Keyword) tb = bKw.Get();
                            else if (tk.kind == CodeTokenKind::TypeName) tb = bTyp.Get();
                            else if (tk.kind == CodeTokenKind::StringLit) tb = bStr.Get();
                            else if (tk.kind == CodeTokenKind::Comment) { tb = bCom.Get(); layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, DWRITE_TEXT_RANGE{ w0, wLen }); }
                            else if (tk.kind == CodeTokenKind::Number) tb = bNum.Get();
                            else if (tk.kind == CodeTokenKind::Operator) tb = bOp.Get();
                            if (tb) layout->SetDrawingEffect(tb, DWRITE_TEXT_RANGE{ w0, wLen });
                            if (tk.kind == CodeTokenKind::Keyword) layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_RANGE{ w0, wLen });
                        }
                        brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(220, 234, 252, 255) : DocColor::RGBA(28, 36, 52, 255)));
                        rt->PushAxisAlignedClip(sr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                        rt->DrawTextLayout(D2D1::Point2F(sr.left + 8.0f, sr.top + 6.0f), layout.Get(), brush);
                        rt->PopAxisAlignedClip();
                    }
                }
            }
        } else if (obj.kind == ObjectKind::AudioClip) {
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const float rad = (std::min)(14.0f, (sr.bottom - sr.top) * 0.45f);
            brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(32, 46, 68, 240) : DocColor::RGBA(239, 246, 255, 248)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush);
            brush->SetColor(ToD2DColor(DocColor::RGBA(37, 99, 235, 220)));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush, 1.2f);
            // Vector play triangle + waveform bars
            const float cy = (sr.top + sr.bottom) * 0.5f;
            const float px = sr.left + 14.0f;
            brush->SetColor(ToD2DColor(DocColor::RGBA(37, 99, 235, 255)));
            rt->DrawLine(D2D1::Point2F(px - 3.0f, cy - 5.0f), D2D1::Point2F(px + 5.0f, cy), brush, 2.0f, roundStroke.Get());
            rt->DrawLine(D2D1::Point2F(px + 5.0f, cy), D2D1::Point2F(px - 3.0f, cy + 5.0f), brush, 2.0f, roundStroke.Get());
            rt->DrawLine(D2D1::Point2F(px - 3.0f, cy - 5.0f), D2D1::Point2F(px - 3.0f, cy + 5.0f), brush, 2.0f, roundStroke.Get());
            if (dwFactory) {
                ComPtr<IDWriteTextFormat> fmt;
                if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, std::clamp(static_cast<float>(10.5 * cam.zoom), 8.0f, 15.0f), L"es-ES", &fmt)) && fmt) {
                    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(219, 234, 254, 255) : DocColor::RGBA(30, 58, 138, 255)));
                    const std::wstring wcap = Utf8ToWide(obj.captionUtf8.empty() ? "Nota de voz" : obj.captionUtf8);
                    rt->DrawTextW(wcap.c_str(), static_cast<UINT32>(wcap.size()), fmt.Get(),
                        D2D1::RectF(sr.left + 26.0f, sr.top, sr.right - 8.0f, sr.bottom), brush);
                }
            }
        } else if (obj.kind == ObjectKind::LinkCard) {
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const bool isHcCard = (overlay && overlay->highContrast);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const float rad = 10.0f;
            const DocColor bgCol = isHcCard ? DocColor::RGBA(255, 255, 255, 255)
                : (isDarkCard ? DocColor::RGBA(30, 36, 48, 246) : DocColor::RGBA(255, 255, 255, 252));
            const DocColor brdCol = isHcCard ? DocColor::RGBA(0, 56, 195, 255)
                : ResolveAccessibleColor(DocColor::RGBA(0, 98, 214, 255), overlay);
            const DocColor titleCol = isHcCard ? DocColor::RGBA(0, 0, 0, 255)
                : (isDarkCard ? DocColor::RGBA(240, 245, 252, 255) : DocColor::RGBA(20, 26, 36, 255));
            const DocColor subCol = isHcCard ? DocColor::RGBA(18, 24, 34, 255)
                : (isDarkCard ? DocColor::RGBA(172, 184, 204, 255) : DocColor::RGBA(72, 82, 98, 255));

            brush->SetColor(ToD2DColor(bgCol));
            rt->FillRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush);
            brush->SetColor(ToD2DColor(brdCol));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush, 1.25f);

            // Left accent rail + vector chain icon
            const D2D1_RECT_F badge = D2D1::RectF(sr.left + 10.0f, sr.top + 10.0f, sr.left + 38.0f, sr.top + 38.0f);
            brush->SetColor(ToD2DColor(DocColor::RGBA(brdCol.r, brdCol.g, brdCol.b, isDarkCard ? 54 : 28)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(badge, 7.0f, 7.0f), brush);
            brush->SetColor(ToD2DColor(brdCol));
            const float icx = (badge.left + badge.right) * 0.5f;
            const float icy = (badge.top + badge.bottom) * 0.5f;
            rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(icx - 8.5f, icy - 3.5f, icx + 1.5f, icy + 3.5f), 3.0f, 3.0f), brush, 1.5f);
            rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(icx - 1.5f, icy - 3.5f, icx + 8.5f, icy + 3.5f), 3.0f, 3.0f), brush, 1.5f);

            if (dwFactory) {
                const float fScale = std::clamp(static_cast<float>(cam.zoom), 0.72f, 1.35f);
                ComPtr<IDWriteTextFormat> fmtT, fmtS;
                dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f * fScale, L"es-ES", &fmtT);
                dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f * fScale, L"es-ES", &fmtS);
                if (fmtT) {
                    const std::string title = !obj.linkTitleUtf8.empty() ? obj.linkTitleUtf8
                        : (!obj.linkUrlUtf8.empty() ? ExtractUrlDomainUtf8(obj.linkUrlUtf8) : "Enlace del cuaderno");
                    const std::wstring wt = Utf8ToWide(title);
                    brush->SetColor(ToD2DColor(titleCol));
                    rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtT.Get(),
                        D2D1::RectF(sr.left + 48.0f, sr.top + 8.0f, sr.right - 10.0f, sr.top + 28.0f), brush);
                }
                if (fmtS) {
                    const std::string sub = (obj.linkTargetPageIndex >= 0)
                        ? ("Enlace interno \xC2\xB7 P\xC3\xA1gina " + std::to_string(obj.linkTargetPageIndex + 1))
                        : (!obj.linkUrlUtf8.empty() ? ExtractUrlDomainUtf8(obj.linkUrlUtf8) : obj.linkDescriptionUtf8);
                    const std::wstring ws = Utf8ToWide(sub);
                    brush->SetColor(ToD2DColor(brdCol));
                    rt->DrawTextW(ws.c_str(), static_cast<UINT32>(ws.size()), fmtS.Get(),
                        D2D1::RectF(sr.left + 48.0f, sr.top + 27.0f, sr.right - 10.0f, sr.top + 44.0f), brush);
                    if (!obj.linkDescriptionUtf8.empty() && (sr.bottom - sr.top) >= 58.0f) {
                        const std::wstring wd = Utf8ToWide(obj.linkDescriptionUtf8);
                        brush->SetColor(ToD2DColor(subCol));
                        rt->DrawTextW(wd.c_str(), static_cast<UINT32>(wd.size()), fmtS.Get(),
                            D2D1::RectF(sr.left + 12.0f, sr.top + 46.0f, sr.right - 10.0f, sr.bottom - 6.0f), brush);
                    }
                }
            }
        } else if (obj.kind == ObjectKind::VideoClip) {
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const float rad = 10.0f;
            const bool isPlayingThis = (overlay && overlay->activeVideoPlaying && overlay->activeVideoObjectId == obj.id);
            const std::uint64_t curMs = isPlayingThis ? overlay->activeVideoPosMs : obj.videoPositionMs;

            brush->SetColor(ToD2DColor(DocColor::RGBA(16, 20, 28, 252)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush);

            const float ctrlBarH = (std::min)(30.0f, (sr.bottom - sr.top) * 0.25f);
            const D2D1_RECT_F mediaRect = D2D1::RectF(sr.left + 1.0f, sr.top + 1.0f, sr.right - 1.0f, (std::max)(sr.top + 10.0f, sr.bottom - ctrlBarH));
            bool drewFrame = false;
            if (isPlayingThis && overlay->activeVideoFrameBgra && overlay->activeVideoFrameBgra->Valid()) {
                ComPtr<ID2D1Bitmap> liveBmp;
                liveBmp.Attach(UploadBgra32(rt, *overlay->activeVideoFrameBgra));
                if (liveBmp) {
                    rt->DrawBitmap(liveBmp.Get(), mediaRect, static_cast<FLOAT>(layerOpacity));
                    drewFrame = true;
                }
            }
            if (!drewFrame && !obj.videoPosterPngBytes.empty()) {
                const std::string tag = "vposter_" + std::to_string(obj.id) + "#" + std::to_string(obj.videoPosterPngBytes.size());
                if (ComPtr<ID2D1Bitmap> pbmp = AcquireGpuBitmapForEncodedAsset(rt, tag, obj.videoPosterPngBytes, cacheGpuAssets)) {
                    rt->DrawBitmap(pbmp.Get(), mediaRect, static_cast<FLOAT>(layerOpacity));
                    drewFrame = true;
                }
            }

            // Center Play badge when paused (0% CPU/GPU idle preview)
            if (!isPlayingThis) {
                const float mcx = (mediaRect.left + mediaRect.right) * 0.5f;
                const float mcy = (mediaRect.top + mediaRect.bottom) * 0.5f;
                const float pr = (std::min)(22.0f, (std::min)(mediaRect.right - mediaRect.left, mediaRect.bottom - mediaRect.top) * 0.24f);
                brush->SetColor(ToD2DColor(DocColor::RGBA(15, 23, 42, 185)));
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(mcx, mcy), pr, pr), brush);
                brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 240)));
                rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(mcx, mcy), pr, pr), brush, 1.4f);
                const float ts = pr * 0.42f;
                rt->DrawLine(D2D1::Point2F(mcx - ts * 0.6f, mcy - ts), D2D1::Point2F(mcx + ts, mcy), brush, 2.2f, roundStroke.Get());
                rt->DrawLine(D2D1::Point2F(mcx + ts, mcy), D2D1::Point2F(mcx - ts * 0.6f, mcy + ts), brush, 2.2f, roundStroke.Get());
                rt->DrawLine(D2D1::Point2F(mcx - ts * 0.6f, mcy - ts), D2D1::Point2F(mcx - ts * 0.6f, mcy + ts), brush, 2.2f, roundStroke.Get());
            }

            // Bottom control bar + scrubber progress
            const D2D1_RECT_F barRc = D2D1::RectF(sr.left + 1.0f, mediaRect.bottom, sr.right - 1.0f, sr.bottom - 1.0f);
            brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(24, 29, 38, 245) : DocColor::RGBA(244, 247, 252, 248)));
            rt->FillRectangle(barRc, brush);
            const float progX0 = barRc.left + 10.0f;
            const float progX1 = barRc.right - 92.0f;
            const float progY = (barRc.top + barRc.bottom) * 0.5f;
            if (progX1 > progX0 + 24.0f) {
                brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(64, 74, 92, 255) : DocColor::RGBA(205, 214, 228, 255)));
                rt->DrawLine(D2D1::Point2F(progX0, progY), D2D1::Point2F(progX1, progY), brush, 3.0f, roundStroke.Get());
                const double ratio = (obj.videoDurationMs > 0)
                    ? std::clamp(static_cast<double>(curMs) / static_cast<double>(obj.videoDurationMs), 0.0, 1.0)
                    : 0.0;
                const float fillX = progX0 + static_cast<float>(ratio) * (progX1 - progX0);
                brush->SetColor(ToD2DColor(ResolveAccessibleColor(DocColor::RGBA(0, 122, 255, 255), overlay)));
                rt->DrawLine(D2D1::Point2F(progX0, progY), D2D1::Point2F(fillX, progY), brush, 3.0f, roundStroke.Get());
            }
            if (dwFactory) {
                auto fmtSec = [](std::uint64_t ms) -> std::string {
                    const std::uint64_t s = ms / 1000ull;
                    char b[24];
                    std::snprintf(b, sizeof(b), "%02llu:%02llu",
                        static_cast<unsigned long long>(s / 60ull), static_cast<unsigned long long>(s % 60ull));
                    return b;
                };
                ComPtr<IDWriteTextFormat> fmtV;
                if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"es-ES", &fmtV)) && fmtV) {
                    fmtV->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                    fmtV->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    const std::wstring wTime = Utf8ToWide(fmtSec(curMs) + " / " + fmtSec(obj.videoDurationMs));
                    brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(218, 226, 240, 255) : DocColor::RGBA(44, 54, 72, 255)));
                    rt->DrawTextW(wTime.c_str(), static_cast<UINT32>(wTime.size()), fmtV.Get(),
                        D2D1::RectF(barRc.right - 88.0f, barRc.top, barRc.right - 8.0f, barRc.bottom), brush);
                }
            }
            brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(72, 82, 102, 230) : DocColor::RGBA(186, 198, 218, 240)));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush, 1.1f);
        } else if (obj.kind == ObjectKind::HtmlEmbed) {
            const bool isDarkCard = (overlay && overlay->darkTheme);
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const float rad = 10.0f;
            const float hdrH = (std::min)(26.0f, (sr.bottom - sr.top) * 0.22f);
            const bool isLive = (overlay && overlay->activeHtmlObjectId == obj.id);

            brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(26, 31, 42, 250) : DocColor::RGBA(255, 255, 255, 252)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush);

            // Header bar (title + Interactive/Frozen status pill)
            const D2D1_RECT_F hdrRc = D2D1::RectF(sr.left + 1.0f, sr.top + 1.0f, sr.right - 1.0f, sr.top + hdrH);
            brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(34, 41, 56, 255) : DocColor::RGBA(243, 246, 252, 255)));
            rt->FillRectangle(hdrRc, brush);
            const DocColor accentHtml = ResolveAccessibleColor(
                isLive ? DocColor::RGBA(16, 185, 129, 255) : DocColor::RGBA(88, 86, 214, 255), overlay);
            brush->SetColor(ToD2DColor(accentHtml));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hdrRc.left + 11.0f, (hdrRc.top + hdrRc.bottom) * 0.5f), 3.8f, 3.8f), brush);

            if (dwFactory) {
                ComPtr<IDWriteTextFormat> fmtH;
                if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"es-ES", &fmtH)) && fmtH) {
                    fmtH->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    const std::string hdrText = (!obj.htmlTitleUtf8.empty() ? obj.htmlTitleUtf8 : "HTML interactivo") +
                        (isLive ? "  \xC2\xB7  Activo" : "  \xC2\xB7  Congelado (0% GPU)");
                    const std::wstring wh = Utf8ToWide(hdrText);
                    brush->SetColor(ToD2DColor(isDarkCard ? DocColor::RGBA(224, 232, 245, 255) : DocColor::RGBA(36, 46, 66, 255)));
                    rt->DrawTextW(wh.c_str(), static_cast<UINT32>(wh.size()), fmtH.Get(),
                        D2D1::RectF(hdrRc.left + 20.0f, hdrRc.top, hdrRc.right - 8.0f, hdrRc.bottom), brush);
                }
            }

            // Body: frozen PNG snapshot when suspended/offscreen/PDF export, or clean placeholder
            const D2D1_RECT_F bodyRc = D2D1::RectF(sr.left + 1.0f, hdrRc.bottom, sr.right - 1.0f, sr.bottom - 1.0f);
            if (!obj.htmlSnapshotPngBytes.empty()) {
                const std::string tag = "htmlsnap_" + std::to_string(obj.id) + "#" + std::to_string(obj.htmlSnapshotPngBytes.size());
                if (ComPtr<ID2D1Bitmap> sbmp = AcquireGpuBitmapForEncodedAsset(rt, tag, obj.htmlSnapshotPngBytes, cacheGpuAssets)) {
                    rt->DrawBitmap(sbmp.Get(), bodyRc, static_cast<FLOAT>(layerOpacity));
                }
            } else if (!isLive && dwFactory) {
                const bool isHcCard = (overlay && overlay->highContrast);
                ComPtr<IDWriteTextFormat> fmtB;
                if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"es-ES", &fmtB)) && fmtB) {
                    fmtB->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    fmtB->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    const std::wstring wHint = L"Doble clic para activar contenido HTML animado";
                    brush->SetColor(ToD2DColor(isHcCard ? DocColor::RGBA(18, 24, 34, 255)
                        : (isDarkCard ? DocColor::RGBA(172, 184, 204, 255) : DocColor::RGBA(72, 82, 98, 255))));
                    rt->DrawTextW(wHint.c_str(), static_cast<UINT32>(wHint.size()), fmtB.Get(), bodyRc, brush);
                }
            }
            brush->SetColor(ToD2DColor(accentHtml));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(sr, rad, rad), brush, isLive ? 1.5f : 1.05f);
        }

        if (applyRotation) {
            rt->SetTransform(savedTransform);
        }

        if (isSelected && !obj.lockedInPlace) {
            const D2D1_RECT_F sr = rectToScreen(obj.bounds);
            const bool strongFocus = (overlay && overlay->enhancedFocusRing);
            brush->SetColor(ToD2DColor(strongFocus ? DocColor::RGBA(0, 90, 220, 255) : DocColor::RGBA(0, 122, 255, 255)));
            rt->DrawRectangle(D2D1::RectF(sr.left - 3.0f, sr.top - 3.0f, sr.right + 3.0f, sr.bottom + 3.0f),
                brush, strongFocus ? 2.6f : 1.5f, strongFocus ? nullptr : dashStroke.Get());
            // 8 resize handles + top rotation stalk handle (F-103, F-107, F-131)
            if (!overlay || overlay->showSelectionHandles) {
                const float mx = (sr.left + sr.right) * 0.5f;
                const float my = (sr.top + sr.bottom) * 0.5f;
                const D2D1_POINT_2F handles[8] = {
                    { sr.left - 3.0f, sr.top - 3.0f }, { mx, sr.top - 3.0f }, { sr.right + 3.0f, sr.top - 3.0f },
                    { sr.right + 3.0f, my }, { sr.right + 3.0f, sr.bottom + 3.0f }, { mx, sr.bottom + 3.0f },
                    { sr.left - 3.0f, sr.bottom + 3.0f }, { sr.left - 3.0f, my }
                };
                // Rotation stalk
                const D2D1_POINT_2F rotTop = D2D1::Point2F(mx, sr.top - 22.0f);
                rt->DrawLine(D2D1::Point2F(mx, sr.top - 3.0f), rotTop, brush, 1.4f);
                brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 255)));
                rt->FillEllipse(D2D1::Ellipse(rotTop, 4.5f, 4.5f), brush);
                brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 255)));
                rt->DrawEllipse(D2D1::Ellipse(rotTop, 4.5f, 4.5f), brush, 1.5f);
                for (const auto& h : handles) {
                    const D2D1_RECT_F hr = D2D1::RectF(h.x - 4.0f, h.y - 4.0f, h.x + 4.0f, h.y + 4.0f);
                    brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 255)));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(hr, 2.0f, 2.0f), brush);
                    brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 255)));
                    rt->DrawRoundedRectangle(D2D1::RoundedRect(hr, 2.0f, 2.0f), brush, 1.4f);
                }
            }
        }
    }

    void RenderSceneToTarget(
        ID2D1RenderTarget* rt,
        const DocumentModel& doc,
        const Camera2D& cam,
        int widthPx,
        int heightPx,
        PageRenderCache* pageCache,
        std::uint64_t asyncGen,
        std::function<void()> onAsyncPageReady,
        const ViewportOverlayState* overlay,
        bool synchronousOffscreen,
        bool forceCanvasMode = false)
    {
        if (!rt) return;
        EnsureStrokeStyles();
        lastFrameHadVisibleAnimatedGif = false;
        lastFrameMinGifDelayMs = 1000;

        activePageCache = pageCache;
        activeAsyncGen = asyncGen;
        activeOnAsyncReady = onAsyncPageReady;
        activeSyncOffscreen = synchronousOffscreen;

        ComPtr<ID2D1SolidColorBrush> brush;
        if (FAILED(rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush)) || !brush) return;

        const bool isDark = (overlay && overlay->darkTheme);
        const bool isHc = (overlay && overlay->highContrast);

        if (forceCanvasMode || doc.Mode() == DocumentMode::InfiniteCanvas) {
            const PaperTintPreset tintPreset = (overlay && overlay->paperTintPreset >= 0 && overlay->paperTintPreset <= 4)
                ? static_cast<PaperTintPreset>(overlay->paperTintPreset)
                : PaperTintPreset::Porcelana;
            const bool nightTint = (tintPreset == PaperTintPreset::NocheSuave);
            if (tintPreset != PaperTintPreset::Porcelana && !isHc) {
                rt->Clear(ToD2DColor(AccessibilityEngine::PaperTintSurfaceColor(tintPreset, isDark, isHc)));
            } else if (isDark) {
                rt->Clear(ToD2DColor(DocColor::RGBA(25, 28, 36, 255)));
            } else if (isHc) {
                rt->Clear(ToD2DColor(DocColor::RGBA(255, 255, 255, 255)));
            } else {
                rt->Clear(ToD2DColor(DocColor::RGBA(250, 252, 255, 255)));
            }
            const DocRect viewWorld = cam.ScreenRectToWorld({ 0.0, 0.0, static_cast<double>(widthPx), static_cast<double>(heightPx) });
            DrawPaperPattern(rt, brush.Get(), viewWorld, doc.CanvasBackground(), cam, isDark || nightTint);

            const std::vector<std::uint64_t> visibleIds = doc.QueryCanvasRect(viewWorld);
            std::vector<const AnnotationObject*> drawList;
            drawList.reserve(visibleIds.size());
            for (std::uint64_t id : visibleIds) {
                if (const auto* o = doc.FindCanvasObject(id)) drawList.push_back(o);
            }
            std::sort(drawList.begin(), drawList.end(), [](const AnnotationObject* a, const AnnotationObject* b) {
                if (a->lockedInPlace != b->lockedInPlace) return a->lockedInPlace;
                return a->zOrder < b->zOrder;
            });

            for (const auto* o : drawList) {
                const bool sel = RenderCommon::IsObjectSelectedInOverlay(overlay, o->id);
                DrawAnnotationObject(rt, brush.Get(), doc, *o, cam, 0.0, 0.0, sel, !synchronousOffscreen, overlay);
            }
            if (overlay && overlay->draftObject) {
                DrawAnnotationObject(rt, brush.Get(), doc, *overlay->draftObject, cam, 0.0, 0.0, false, !synchronousOffscreen, overlay);
            }
            // Search match highlights on Infinite Canvas (including inside pinned PdfCard printouts)
            if (overlay && overlay->searchMatches) {
                for (std::size_t mi = 0; mi < overlay->searchMatches->size(); ++mi) {
                    const auto& m = (*overlay->searchMatches)[mi];
                    if (!m.onCanvas || !m.hasBox || m.worldBox.Empty()) continue;
                    const bool isActive = (mi == overlay->activeSearchMatchIndex);
                    const D2D1_RECT_F sr = ToD2DRect(cam.WorldRectToScreen(m.worldBox));
                    const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(sr, 3.0f, 3.0f);
                    brush->SetColor(ToD2DColor(isActive ? DocColor::RGBA(251, 146, 60, 125) : DocColor::RGBA(250, 204, 21, 95)));
                    rt->FillRoundedRectangle(rr, brush.Get());
                    if (isActive) {
                        brush->SetColor(ToD2DColor(DocColor::RGBA(234, 88, 12, 235)));
                        rt->DrawRoundedRectangle(rr, brush.Get(), 1.6f);
                    }
                }
            }
            if (overlay && overlay->hasCropPreview) {
                const D2D1_RECT_F sr = ToD2DRect(cam.WorldRectToScreen(overlay->cropPreviewPageLocal));
                brush->SetColor(ToD2DColor(DocColor::RGBA(37, 99, 235, 36)));
                rt->FillRectangle(sr, brush.Get());
                brush->SetColor(ToD2DColor(DocColor::RGBA(37, 99, 235, 255)));
                rt->DrawRectangle(sr, brush.Get(), 2.0f, dashStroke.Get());
            }
            if (overlay && overlay->laserPointer.active) {
                const auto& lp = overlay->laserPointer;
                if (lp.trail.size() >= 2) {
                    for (std::size_t i = 1; i < lp.trail.size(); ++i) {
                        const double frac = static_cast<double>(i) / static_cast<double>(lp.trail.size());
                        brush->SetColor(ToD2DColor(DocColor::RGBA(239, 68, 68, static_cast<std::uint8_t>(40 + 180 * frac))));
                        const DocPoint a = cam.WorldToScreen(lp.trail[i - 1].x, lp.trail[i - 1].y);
                        const DocPoint b = cam.WorldToScreen(lp.trail[i].x, lp.trail[i].y);
                        rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(a.x), static_cast<FLOAT>(a.y)),
                                     D2D1::Point2F(static_cast<FLOAT>(b.x), static_cast<FLOAT>(b.y)),
                                     brush.Get(), static_cast<FLOAT>(2.0 + 3.0 * frac), roundStroke.Get());
                    }
                }
                const DocPoint tip = cam.WorldToScreen(lp.worldX, lp.worldY);
                brush->SetColor(ToD2DColor(DocColor::RGBA(239, 68, 68, 95)));
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(tip.x), static_cast<FLOAT>(tip.y)), 11.0f, 11.0f), brush.Get());
                brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 245)));
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(tip.x), static_cast<FLOAT>(tip.y)), 4.5f, 4.5f), brush.Get());
            }
            DrawHudOverlays(rt, brush.Get(), doc, cam, widthPx, heightPx, overlay);
            return;
        }

        // Paginated PDF / Notebook Mode (guías de interfaz Light/Dark/HighContrast desk surface)
        if (isDark) {
            rt->Clear(ToD2DColor(DocColor::RGBA(20, 23, 30, 255)));
        } else if (isHc) {
            rt->Clear(ToD2DColor(DocColor::RGBA(242, 245, 250, 255)));
        } else {
            rt->Clear(ToD2DColor(DocColor::RGBA(232, 237, 245, 255)));
        }
        const DocRect viewWorld = cam.ScreenRectToWorld({ 0.0, 0.0, static_cast<double>(widthPx), static_cast<double>(heightPx) });
        std::size_t firstPg = 0, lastPg = 0;
        if (!doc.Layout().ComputeVisibleRange(viewWorld.y, viewWorld.Bottom(), firstPg, lastPg, 1)) {
            return;
        }

        const auto& pages = doc.Pages();
        for (std::size_t p = firstPg; p <= lastPg && p < pages.size(); ++p) {
            const auto* place = doc.Layout().PlacementAt(p);
            if (!place) continue;
            if (place->collapsed && !place->collapsedHeader) continue;

            const DocRect pageWorld = place->WorldRect();
            const D2D1_RECT_F pageScreen = ToD2DRect(cam.WorldRectToScreen(pageWorld));

            // Collapsed page range pill banner (F-055: clickable to expand)
            if (place->collapsedHeader) {
                const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(pageScreen, 10.0f, 10.0f);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(36, 44, 60, 235) : DocColor::RGBA(241, 245, 252, 248)));
                rt->FillRoundedRectangle(rr, brush.Get());
                brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 200)));
                rt->DrawRoundedRectangle(rr, brush.Get(), 1.2f);
                if (dwFactory) {
                    ComPtr<IDWriteTextFormat> fmt;
                    if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"es-ES", &fmt)) && fmt) {
                        fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                        brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(191, 219, 254, 255) : DocColor::RGBA(29, 78, 216, 255)));
                        const std::wstring lbl = L"P\u00E1ginas " + std::to_wstring(p + 1) + L"\u2013" +
                            std::to_wstring(place->collapsedEndPage0 + 1) + L" contra\u00EDdas \u2014 Clic para expandir";
                        rt->DrawTextW(lbl.c_str(), static_cast<UINT32>(lbl.size()), fmt.Get(), pageScreen, brush.Get());
                    }
                }
                continue;
            }

            // Multi-layered soft guías de interfaz page shadow + crisp white sheet (content layer: NEVER glass)
            for (int sh = 3; sh >= 1; --sh) {
                const float pad = static_cast<float>(sh) * 2.5f;
                const float dy = static_cast<float>(sh) * 2.2f;
                const std::uint8_t a = isDark
                    ? static_cast<std::uint8_t>(42 - sh * 10)
                    : static_cast<std::uint8_t>(20 - sh * 5);
                brush->SetColor(ToD2DColor(DocColor::RGBA(15, 23, 42, a)));
                rt->FillRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(pageScreen.left - pad * 0.4f, pageScreen.top + dy - 1.0f,
                        pageScreen.right + pad * 0.4f, pageScreen.bottom + dy + pad * 0.4f), 4.0f, 4.0f),
                    brush.Get());
            }
            const PaperTintPreset pgTint = (overlay && overlay->paperTintPreset >= 0 && overlay->paperTintPreset <= 4)
                ? static_cast<PaperTintPreset>(overlay->paperTintPreset)
                : PaperTintPreset::Porcelana;
            const DocColor pageSurfaceCol = (pgTint != PaperTintPreset::Porcelana && !isHc)
                ? AccessibilityEngine::PaperTintSurfaceColor(pgTint, false, false)
                : DocColor::RGBA(255, 255, 255, 255);
            brush->SetColor(ToD2DColor(pageSurfaceCol));
            rt->FillRectangle(pageScreen, brush.Get());
            brush->SetColor(ToD2DColor(isHc ? DocColor::RGBA(25, 32, 44, 220) : DocColor::RGBA(198, 208, 222, 160)));
            rt->DrawRectangle(pageScreen, brush.Get(), isHc ? 1.25f : 0.8f);

            const auto& pg = pages[p];
            if (pg.hasPdfSource && (!pg.sourcePdfPathUtf8.empty() || !pg.sourcePdfAssetKey.empty())) {
                const std::string resolvedPdfPath = ResolvePdfSourcePathForRender(doc, pg);
                if (!resolvedPdfPath.empty()) {
                const int targetW = static_cast<int>(std::lround((std::max)(80.0, static_cast<double>(pageScreen.right - pageScreen.left))));
                PageCacheKey key = PageRenderCache::MakeKey(pg, targetW);
                key.sourcePdfPath = resolvedPdfPath;

                RasterImageBgra32 img;
                bool haveRaster = pageCache && pageCache->TryGet(key, img);
                if (!haveRaster && synchronousOffscreen) {
                    NormalizedCropRect c = pg.crop;
                    c.Clamp();
                    haveRaster = WinRtPdfRenderer::RenderPageToBgra32(
                        Utf8ToWide(resolvedPdfPath), pg.sourcePageIndex0,
                        key.widthBucketPx, key.rotationDeg, c, img);
                    if (haveRaster && pageCache) pageCache->PutDirect(key, img);
                }

                if (haveRaster && img.Valid()) {
                    ComPtr<ID2D1Bitmap> gpuBmp;
                    if (!synchronousOffscreen) {
                        for (const auto& entry : pageGpuBitmaps) {
                            if (entry.key == key) { gpuBmp = entry.bmp; break; }
                        }
                    }
                    if (!gpuBmp) {
                        gpuBmp.Attach(UploadBgra32(rt, img));
                        if (gpuBmp && !synchronousOffscreen) {
                            if (pageGpuBitmaps.size() >= 16) pageGpuBitmaps.erase(pageGpuBitmaps.begin());
                            pageGpuBitmaps.push_back({ key, gpuBmp });
                        }
                    }
                    if (gpuBmp) {
                        rt->DrawBitmap(gpuBmp.Get(), pageScreen);
                    }
                } else {
                    if (pageCache && !synchronousOffscreen) {
                        pageCache->RequestAsync(key, asyncGen, onAsyncPageReady);
                    }
                    if (dwFactory) {
                        ComPtr<IDWriteTextFormat> fmt;
                        if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"es-ES", &fmt)) && fmt) {
                            brush->SetColor(ToD2DColor(DocColor::RGBA(120, 132, 148, 255)));
                            const std::wstring msg = L"Renderizando p\u00E1gina " + std::to_wstring(p + 1) + L"...";
                            rt->DrawTextW(msg.c_str(), static_cast<UINT32>(msg.size()), fmt.Get(),
                                D2D1::RectF(pageScreen.left + 16.0f, pageScreen.top + 16.0f, pageScreen.right - 16.0f, pageScreen.bottom - 16.0f), brush.Get());
                        }
                    }
                }
                }
            }

            // Paper pattern overlay for blank/notebook or lined PDFs
            rt->PushAxisAlignedClip(pageScreen, D2D1_ANTIALIAS_MODE_ALIASED);
            DrawPaperPattern(rt, brush.Get(), pageWorld, pg.background, cam, pgTint == PaperTintPreset::NocheSuave);

            // Annotations
            for (const auto& obj : pg.objects) {
                const bool sel = RenderCommon::IsObjectSelectedInOverlay(overlay, obj.id);
                DrawAnnotationObject(rt, brush.Get(), doc, obj, cam, pageWorld.x, pageWorld.y, sel, !synchronousOffscreen, overlay);
            }
            if (overlay && overlay->draftObject && p == overlay->draftPageIndex) {
                DrawAnnotationObject(rt, brush.Get(), doc, *overlay->draftObject, cam, pageWorld.x, pageWorld.y, false, !synchronousOffscreen, overlay);
            }

            // Search match highlights on this page
            if (overlay && overlay->searchMatches) {
                for (std::size_t mi = 0; mi < overlay->searchMatches->size(); ++mi) {
                    const auto& m = (*overlay->searchMatches)[mi];
                    if (m.onCanvas || m.pageIndex != p) continue;
                    const bool isActive = (mi == overlay->activeSearchMatchIndex);
                    brush->SetColor(ToD2DColor(isActive ? DocColor::RGBA(251, 146, 60, 120) : DocColor::RGBA(250, 204, 21, 90)));
                    if (m.hasBox) {
                        const DocRect mr{
                            pageWorld.x + m.boxNormalized.x * pageWorld.w,
                            pageWorld.y + m.boxNormalized.y * pageWorld.h,
                            m.boxNormalized.w * pageWorld.w,
                            m.boxNormalized.h * pageWorld.h
                        };
                        rt->FillRectangle(ToD2DRect(cam.WorldRectToScreen(mr)), brush.Get());
                    } else {
                        // Highlight page header badge cleanly when offset-only match found
                        const D2D1_RECT_F badge = D2D1::RectF(pageScreen.left + 6.0f, pageScreen.top + 6.0f, pageScreen.left + 96.0f, pageScreen.top + 26.0f);
                        rt->FillRectangle(badge, brush.Get());
                    }
                }
            }

            // Clickable InkLink badges on this page (F-132)
            for (const auto& lk : doc.InkLinks()) {
                if (lk.sourcePageIndex0 != static_cast<int>(p)) continue;
                const DocRect lr{
                    pageWorld.x + lk.sourceRectNorm.left * pageWorld.w,
                    pageWorld.y + lk.sourceRectNorm.top * pageWorld.h,
                    (std::max)(80.0, (lk.sourceRectNorm.right - lk.sourceRectNorm.left) * pageWorld.w),
                    (std::max)(20.0, (lk.sourceRectNorm.bottom - lk.sourceRectNorm.top) * pageWorld.h)
                };
                const D2D1_RECT_F sLr = ToD2DRect(cam.WorldRectToScreen(lr));
                const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(sLr, 6.0f, 6.0f);
                brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 36)));
                rt->FillRoundedRectangle(rr, brush.Get());
                brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 220)));
                rt->DrawRoundedRectangle(rr, brush.Get(), 1.2f);
                if (dwFactory && !lk.labelUtf8.empty()) {
                    ComPtr<IDWriteTextFormat> fmt;
                    if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, std::clamp(static_cast<float>(9.5 * cam.zoom), 7.5f, 13.0f), L"es-ES", &fmt)) && fmt) {
                        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                        const std::wstring wl = Utf8ToWide(lk.labelUtf8);
                        rt->DrawTextW(wl.c_str(), static_cast<UINT32>(wl.size()), fmt.Get(),
                            D2D1::RectF(sLr.left + 6.0f, sLr.top, sLr.right - 4.0f, sLr.bottom), brush.Get());
                    }
                }
            }

            // PDF Hyperlink subtle indicator (F-082) & AcroForm field overlay (F-084)
            for (const auto& pl : pg.pdfLinks) {
                const DocRect lr{
                    pageWorld.x + pl.rectNorm.x * pageWorld.w,
                    pageWorld.y + pl.rectNorm.y * pageWorld.h,
                    pl.rectNorm.w * pageWorld.w,
                    pl.rectNorm.h * pageWorld.h
                };
                const D2D1_RECT_F sLr = ToD2DRect(cam.WorldRectToScreen(lr));
                brush->SetColor(ToD2DColor(DocColor::RGBA(37, 99, 235, 110)));
                rt->DrawLine(D2D1::Point2F(sLr.left, sLr.bottom), D2D1::Point2F(sLr.right, sLr.bottom), brush.Get(), 1.0f, dashStroke.Get());
            }
            for (const auto& ff : pg.formFields) {
                const DocRect fr{
                    pageWorld.x + ff.rectNorm.x * pageWorld.w,
                    pageWorld.y + ff.rectNorm.y * pageWorld.h,
                    ff.rectNorm.w * pageWorld.w,
                    ff.rectNorm.h * pageWorld.h
                };
                const D2D1_RECT_F sFr = ToD2DRect(cam.WorldRectToScreen(fr));
                brush->SetColor(ToD2DColor(DocColor::RGBA(219, 234, 254, 96)));
                rt->FillRectangle(sFr, brush.Get());
                brush->SetColor(ToD2DColor(DocColor::RGBA(59, 130, 246, 180)));
                rt->DrawRectangle(sFr, brush.Get(), 0.9f);
                if (dwFactory) {
                    const std::string disp = (ff.fieldTypeUtf8 == "Btn") ? (ff.checked ? "X" : "") : ff.valueUtf8;
                    if (!disp.empty()) {
                        ComPtr<IDWriteTextFormat> fmt;
                        if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, std::clamp(static_cast<float>(10.0 * cam.zoom), 7.5f, 15.0f), L"es-ES", &fmt)) && fmt) {
                            brush->SetColor(ToD2DColor(DocColor::RGBA(15, 23, 42, 255)));
                            const std::wstring wd = Utf8ToWide(disp);
                            rt->DrawTextW(wd.c_str(), static_cast<UINT32>(wd.size()), fmt.Get(), sFr, brush.Get());
                        }
                    }
                }
            }

            // PDF drag-to-select live selection highlight (F-075, F-076)
            if (overlay && overlay->hasPdfTextSel && overlay->pdfTextSelPage == p) {
                const DocRect selW{
                    pageWorld.x + overlay->pdfTextSelRectLocal.x,
                    pageWorld.y + overlay->pdfTextSelRectLocal.y,
                    overlay->pdfTextSelRectLocal.w,
                    overlay->pdfTextSelRectLocal.h
                };
                brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 68)));
                rt->FillRectangle(ToD2DRect(cam.WorldRectToScreen(selW)), brush.Get());
                brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 210)));
                rt->DrawRectangle(ToD2DRect(cam.WorldRectToScreen(selW)), brush.Get(), 1.1f);
            }

            // Crop preview box
            if (overlay && overlay->hasCropPreview && overlay->cropPageIndex == p) {
                const DocRect cr{
                    pageWorld.x + overlay->cropPreviewPageLocal.x,
                    pageWorld.y + overlay->cropPreviewPageLocal.y,
                    overlay->cropPreviewPageLocal.w,
                    overlay->cropPreviewPageLocal.h
                };
                brush->SetColor(ToD2DColor(DocColor::RGBA(37, 99, 235, 255)));
                rt->DrawRectangle(ToD2DRect(cam.WorldRectToScreen(cr)), brush.Get(), 2.0f, dashStroke.Get());
            }

            rt->PopAxisAlignedClip();
        }
        DrawHudOverlays(rt, brush.Get(), doc, cam, widthPx, heightPx, overlay);
    }

    void DrawHudOverlays(
        ID2D1RenderTarget* rt,
        ID2D1SolidColorBrush* brush,
        const DocumentModel& doc,
        const Camera2D& cam,
        int widthPx,
        int heightPx,
        const ViewportOverlayState* overlay)
    {
        if (!overlay) return;

        const bool isDark = overlay->darkTheme;
        const bool isHc = overlay->highContrast;

        if (overlay->rulerState.visible && d2dFactory) {
            if (overlay->rulerState.mode == RulerMode::Protractor) {
                const DocPoint cScr = cam.WorldToScreen(overlay->rulerState.centerX, overlay->rulerState.centerY);
                const float rPx = static_cast<float>((std::max)(40.0, overlay->rulerState.lengthPt * 0.42) * cam.zoom);
                const double baseRad = overlay->rulerState.angleDeg * (3.14159265358979323846 / 180.0);
                // Semicircle translucent arc + radial degree ticks (F-039)
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(56, 189, 248, 220) : DocColor::RGBA(2, 132, 199, 235)));
                DocPoint prev{};
                for (int deg = 0; deg <= 180; deg += 3) {
                    const double a = baseRad - deg * (3.14159265358979323846 / 180.0);
                    const DocPoint cur{ cScr.x + rPx * std::cos(a), cScr.y + rPx * std::sin(a) };
                    if (deg > 0) {
                        rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(prev.x), static_cast<FLOAT>(prev.y)),
                                     D2D1::Point2F(static_cast<FLOAT>(cur.x), static_cast<FLOAT>(cur.y)), brush, 2.0f);
                    }
                    if (deg % 15 == 0) {
                        const float inner = (deg % 45 == 0) ? (rPx * 0.82f) : (rPx * 0.90f);
                        rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(cScr.x + inner * std::cos(a)), static_cast<FLOAT>(cScr.y + inner * std::sin(a))),
                                     D2D1::Point2F(static_cast<FLOAT>(cur.x), static_cast<FLOAT>(cur.y)), brush, (deg % 45 == 0) ? 1.5f : 0.9f);
                    }
                    prev = cur;
                }
                rt->DrawLine(
                    D2D1::Point2F(static_cast<FLOAT>(cScr.x - rPx * std::cos(baseRad)), static_cast<FLOAT>(cScr.y - rPx * std::sin(baseRad))),
                    D2D1::Point2F(static_cast<FLOAT>(cScr.x + rPx * std::cos(baseRad)), static_cast<FLOAT>(cScr.y + rPx * std::sin(baseRad))),
                    brush, 2.0f);
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y)), 4.0f, 4.0f), brush);
            } else if (overlay->rulerState.mode == RulerMode::Compass) {
                const DocPoint cScr = cam.WorldToScreen(overlay->rulerState.centerX, overlay->rulerState.centerY);
                const float rPx = static_cast<float>((std::max)(18.0, overlay->rulerState.compassRadiusPt) * cam.zoom);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(56, 189, 248, 220) : DocColor::RGBA(2, 132, 199, 235)));
                rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y)), rPx, rPx),
                    brush, 1.8f, dashStroke.Get());
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(cScr.x) - 8.0f, static_cast<FLOAT>(cScr.y)),
                             D2D1::Point2F(static_cast<FLOAT>(cScr.x) + 8.0f, static_cast<FLOAT>(cScr.y)), brush, 1.5f);
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y) - 8.0f),
                             D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y) + 8.0f), brush, 1.5f);
                rt->DrawLine(D2D1::Point2F(static_cast<FLOAT>(cScr.x), static_cast<FLOAT>(cScr.y)),
                             D2D1::Point2F(static_cast<FLOAT>(cScr.x) + rPx, static_cast<FLOAT>(cScr.y)), brush, 1.2f);
            } else {
            const auto rg = RenderCommon::BuildRulerOverlay(overlay->rulerState, cam);
            ComPtr<ID2D1PathGeometry> poly;
            if (SUCCEEDED(d2dFactory->CreatePathGeometry(&poly)) && poly) {
                ComPtr<ID2D1GeometrySink> sink;
                if (SUCCEEDED(poly->Open(&sink)) && sink) {
                    sink->BeginFigure(
                        D2D1::Point2F(static_cast<FLOAT>(rg.corners[0].x), static_cast<FLOAT>(rg.corners[0].y)),
                        D2D1_FIGURE_BEGIN_FILLED);
                    for (int k = 1; k < 4; ++k) {
                        sink->AddLine(D2D1::Point2F(static_cast<FLOAT>(rg.corners[k].x), static_cast<FLOAT>(rg.corners[k].y)));
                    }
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();
                }
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(30, 58, 86, 195)
                                                  : DocColor::RGBA(224, 242, 254, 185)));
                rt->FillGeometry(poly.Get(), brush);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(56, 189, 248, 235)
                                                  : DocColor::RGBA(2, 132, 199, 240)));
                rt->DrawGeometry(poly.Get(), brush, 1.5f);
            }
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(125, 211, 252, 255)
                                              : DocColor::RGBA(3, 105, 161, 255)));
            rt->DrawLine(
                D2D1::Point2F(static_cast<FLOAT>(rg.activeEdge0.x), static_cast<FLOAT>(rg.activeEdge0.y)),
                D2D1::Point2F(static_cast<FLOAT>(rg.activeEdge1.x), static_cast<FLOAT>(rg.activeEdge1.y)),
                brush, 2.2f);
            for (const auto& tk : rg.ticks) {
                rt->DrawLine(
                    D2D1::Point2F(static_cast<FLOAT>(tk.p0.x), static_cast<FLOAT>(tk.p0.y)),
                    D2D1::Point2F(static_cast<FLOAT>(tk.p1.x), static_cast<FLOAT>(tk.p1.y)),
                    brush, tk.major ? 1.4f : 0.9f);
            }
            if (dwFactory && !rg.badgeUtf8.empty()) {
                ComPtr<IDWriteTextFormat> fmt;
                if (SUCCEEDED(dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"es-ES", &fmt)) && fmt) {
                    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    const std::wstring wb = Utf8ToWide(rg.badgeUtf8);
                    const float pillW = 74.0f;
                    const float pillH = 20.0f;
                    const D2D1_RECT_F pillR = D2D1::RectF(
                        static_cast<FLOAT>(rg.badgePos.x) - pillW * 0.5f,
                        static_cast<FLOAT>(rg.badgePos.y) - pillH * 0.5f,
                        static_cast<FLOAT>(rg.badgePos.x) + pillW * 0.5f,
                        static_cast<FLOAT>(rg.badgePos.y) + pillH * 0.5f);
                    const D2D1_ROUNDED_RECT pillRR = D2D1::RoundedRect(pillR, 10.0f, 10.0f);
                    brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(15, 23, 42, 235)
                                                      : DocColor::RGBA(255, 255, 255, 238)));
                    rt->FillRoundedRectangle(pillRR, brush);
                    brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(56, 189, 248, 220)
                                                      : DocColor::RGBA(2, 132, 199, 210)));
                    rt->DrawRoundedRectangle(pillRR, brush, 1.0f);
                    brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(224, 242, 254, 255)
                                                      : DocColor::RGBA(12, 74, 110, 255)));
                    rt->DrawTextW(wb.c_str(), static_cast<UINT32>(wb.size()), fmt.Get(), pillR, brush);
                }
            }
            }
        }

        if (overlay->minimapVisible && widthPx >= 640 && heightPx >= 420) {
            const auto mm = RenderCommon::BuildMinimapOverlay(doc, cam, widthPx, heightPx);
            const D2D1_RECT_F pr = ToD2DRect(mm.panelScreen);
            const D2D1_ROUNDED_RECT mmRR = D2D1::RoundedRect(pr, 10.0f, 10.0f);
            // Soft HIG elevation shadow
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(0, 0, 0, 88) : DocColor::RGBA(15, 23, 42, 28)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(pr.left, pr.top + 2.5f, pr.right, pr.bottom + 2.5f), 10.0f, 10.0f), brush);
            // Frosted guías de interfaz Light / Dark / HighContrast minimap card
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(28, 32, 42, 228)
                                     : (isHc ? DocColor::RGBA(255, 255, 255, 248) : DocColor::RGBA(248, 250, 253, 228))));
            rt->FillRoundedRectangle(mmRR, brush);
            brush->SetColor(ToD2DColor(isHc ? DocColor::RGBA(24, 30, 42, 255)
                                     : (isDark ? DocColor::RGBA(94, 106, 128, 210) : DocColor::RGBA(186, 198, 216, 220))));
            rt->DrawRoundedRectangle(mmRR, brush, isHc ? 1.4f : 1.0f);
            for (const auto& it : mm.items) {
                brush->SetColor(ToD2DColor(it.color));
                rt->FillRoundedRectangle(D2D1::RoundedRect(ToD2DRect(it.rectScreen), 2.0f, 2.0f), brush);
            }
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(56, 189, 248, 240) : DocColor::RGBA(0, 122, 255, 235)));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(ToD2DRect(mm.viewportScreen), 3.0f, 3.0f), brush, 1.5f);
        }

        // Interactive guías de interfaz Knowledge Graph Overlay (M1)
        if (overlay->graphOverlayVisible && overlay->graphOverlay && widthPx >= 420 && heightPx >= 320) {
            const GraphViewData& gv = *overlay->graphOverlay;
            const float gw = (std::min)(540.0f, static_cast<float>(widthPx) - 48.0f);
            const float gh = (std::min)(380.0f, static_cast<float>(heightPx) - 48.0f);
            const float gx0 = (static_cast<float>(widthPx) - gw) * 0.5f;
            const float gy0 = (static_cast<float>(heightPx) - gh) * 0.5f;
            const D2D1_RECT_F gr = D2D1::RectF(gx0, gy0, gx0 + gw, gy0 + gh);
            const D2D1_ROUNDED_RECT grr = D2D1::RoundedRect(gr, 14.0f, 14.0f);

            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(0, 0, 0, 96) : DocColor::RGBA(15, 23, 42, 30)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(gx0, gy0 + 4.0f, gx0 + gw, gy0 + gh + 4.0f), 14.0f, 14.0f), brush);
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(26, 31, 42, 242) : DocColor::RGBA(250, 251, 254, 244)));
            rt->FillRoundedRectangle(grr, brush);
            brush->SetColor(ToD2DColor(isHc ? DocColor::RGBA(24, 30, 42, 255)
                                             : (isDark ? DocColor::RGBA(76, 88, 110, 230) : DocColor::RGBA(196, 206, 222, 235))));
            rt->DrawRoundedRectangle(grr, brush, 1.2f);

            std::unordered_map<std::string, D2D1_POINT_2F> nodePts;
            const float padX = 36.0f, padTop = 44.0f, padBot = 28.0f;
            const float sx = (gw - padX * 2.0f) / 800.0f;
            const float sy = (gh - padTop - padBot) / 560.0f;
            for (const auto& nd : gv.nodes) {
                nodePts[nd.idUtf8] = D2D1::Point2F(gx0 + padX + static_cast<float>(nd.x) * sx, gy0 + padTop + static_cast<float>(nd.y) * sy);
            }
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(96, 120, 158, 175) : DocColor::RGBA(148, 163, 184, 200)));
            for (const auto& e : gv.edges) {
                auto itA = nodePts.find(e.fromIdUtf8);
                auto itB = nodePts.find(e.toIdUtf8);
                if (itA != nodePts.end() && itB != nodePts.end()) {
                    rt->DrawLine(itA->second, itB->second, brush, e.resolved ? 1.2f : 0.8f);
                }
            }
            ComPtr<IDWriteTextFormat> fmtLbl;
            if (dwFactory) {
                dwFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.5f, L"es-ES", &fmtLbl);
            }
            for (const auto& nd : gv.nodes) {
                const D2D1_POINT_2F p = nodePts[nd.idUtf8];
                const float r = nd.isCenter ? 7.5f : (nd.resolved ? 5.5f : 4.2f);
                brush->SetColor(ToD2DColor(nd.isCenter ? DocColor::RGBA(37, 99, 235, 255)
                                         : (nd.resolved ? (isDark ? DocColor::RGBA(147, 197, 253, 240) : DocColor::RGBA(59, 130, 246, 225))
                                                        : DocColor::RGBA(148, 163, 184, 180))));
                rt->FillEllipse(D2D1::Ellipse(p, r, r), brush);
                if (fmtLbl) {
                    const std::wstring wl = Utf8ToWide(nd.labelUtf8);
                    brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(232, 238, 248, 245) : DocColor::RGBA(24, 30, 42, 245)));
                    rt->DrawTextW(wl.c_str(), static_cast<UINT32>(wl.size()), fmtLbl.Get(),
                        D2D1::RectF(p.x + 8.0f, p.y - 8.0f, (std::min)(gr.right - 8.0f, p.x + 150.0f), p.y + 12.0f), brush);
                }
            }
            if (fmtLbl) {
                const std::string modeTag = overlay->graphLocalMode ? "Grafo Local" : "Grafo Global";
                std::string hdrS = modeTag + " (" + std::to_string(gv.nodes.size()) + " nodos, " + std::to_string(gv.edges.size()) + " enlaces)";
                if (!overlay->graphFilterLabelUtf8.empty()) hdrS += " [" + overlay->graphFilterLabelUtf8 + "]";
                const std::wstring hdr = Utf8ToWide(hdrS);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(240, 244, 250, 255) : DocColor::RGBA(15, 23, 42, 255)));
                rt->DrawTextW(hdr.c_str(), static_cast<UINT32>(hdr.size()), fmtLbl.Get(),
                    D2D1::RectF(gx0 + 16.0f, gy0 + 12.0f, gr.right - 168.0f, gy0 + 34.0f), brush);
                // Compact interactive header buttons: [Local/Global] [Filtrar]
                const D2D1_RECT_F btnMode = D2D1::RectF(gr.right - 160.0f, gy0 + 8.0f, gr.right - 88.0f, gy0 + 30.0f);
                const D2D1_RECT_F btnFilt = D2D1::RectF(gr.right - 82.0f, gy0 + 8.0f, gr.right - 14.0f, gy0 + 30.0f);
                brush->SetColor(ToD2DColor(overlay->graphLocalMode ? DocColor::RGBA(37, 99, 235, 235)
                    : (isDark ? DocColor::RGBA(44, 52, 68, 225) : DocColor::RGBA(232, 238, 248, 240))));
                rt->FillRoundedRectangle(D2D1::RoundedRect(btnMode, 5.0f, 5.0f), brush);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(44, 52, 68, 225) : DocColor::RGBA(232, 238, 248, 240)));
                rt->FillRoundedRectangle(D2D1::RoundedRect(btnFilt, 5.0f, 5.0f), brush);
                const std::wstring wMode = overlay->graphLocalMode ? L"Local" : L"Global";
                brush->SetColor(ToD2DColor(overlay->graphLocalMode ? DocColor::RGBA(255, 255, 255, 255)
                    : (isDark ? DocColor::RGBA(232, 238, 248, 255) : DocColor::RGBA(24, 30, 42, 255))));
                rt->DrawTextW(wMode.c_str(), static_cast<UINT32>(wMode.size()), fmtLbl.Get(),
                    D2D1::RectF(btnMode.left + 12.0f, btnMode.top + 3.0f, btnMode.right, btnMode.bottom), brush);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(232, 238, 248, 255) : DocColor::RGBA(24, 30, 42, 255)));
                rt->DrawTextW(L"Filtrar", 7, fmtLbl.Get(),
                    D2D1::RectF(btnFilt.left + 14.0f, btnFilt.top + 3.0f, btnFilt.right, btnFilt.bottom), brush);
            }
        }

        // Helper text formats for guías de interfaz interactive overlays
        auto makeTextFmt = [&](float sizePt, DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                               DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) -> ComPtr<IDWriteTextFormat> {
            ComPtr<IDWriteTextFormat> f;
            if (dwFactory) {
                dwFactory->CreateTextFormat(L"Segoe UI", nullptr, weight,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sizePt, L"es-ES", &f);
                if (f) {
                    f->SetTextAlignment(align);
                    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                    f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                }
            }
            return f;
        };

        const DocColor ovBg = isDark ? DocColor::RGBA(26, 31, 42, 244)
                                     : (isHc ? DocColor::RGBA(255, 255, 255, 252) : DocColor::RGBA(250, 251, 254, 246));
        const DocColor ovCardBg = isDark ? DocColor::RGBA(36, 42, 56, 245)
                                         : (isHc ? DocColor::RGBA(244, 247, 252, 255) : DocColor::RGBA(255, 255, 255, 250));
        const DocColor ovBorder = isHc ? DocColor::RGBA(24, 30, 42, 255)
                                       : (isDark ? DocColor::RGBA(76, 88, 110, 230) : DocColor::RGBA(198, 208, 224, 235));
        const DocColor ovInk = isDark ? DocColor::RGBA(238, 242, 249, 255) : DocColor::RGBA(22, 28, 38, 255);
        const DocColor ovMuted = isDark ? DocColor::RGBA(156, 168, 190, 240) : DocColor::RGBA(96, 108, 128, 240);
        const DocColor ovAccent = isDark ? DocColor::RGBA(96, 165, 250, 255) : DocColor::RGBA(0, 112, 243, 255);

        // 1. Anchored Comment Pins on visible pages (COL)
        if (overlay->commentThreads && doc.Mode() == DocumentMode::PaginatedPdf) {
            auto fmtPin = makeTextFmt(9.5f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
            int pinIdx = 0;
            for (const auto& th : overlay->commentThreads->AllThreads()) {
                ++pinIdx;
                if (th.resolved) continue;
                if (th.pageIndex0 >= doc.Pages().size()) continue;
                const auto* place = doc.Layout().PlacementAt(th.pageIndex0);
                if (!place) continue;
                const DocRect pageW = place->WorldRect();
                const double ax = pageW.x + std::clamp(th.anchorRect.x, 12.0, (std::max)(24.0, pageW.w - 16.0));
                const double ay = pageW.y + std::clamp(th.anchorRect.y, 12.0, (std::max)(24.0, pageW.h - 16.0));
                const DocPoint scr = cam.WorldToScreen(ax, ay);
                if (scr.x < -20.0 || scr.x > widthPx + 20.0 || scr.y < -20.0 || scr.y > heightPx + 20.0) continue;
                const bool isSel = (overlay->selectedCommentThreadId == th.threadId);
                const float pr = isSel ? 12.0f : 10.0f;
                const D2D1_POINT_2F c = D2D1::Point2F(static_cast<FLOAT>(scr.x), static_cast<FLOAT>(scr.y));
                brush->SetColor(ToD2DColor(isSel ? ovAccent : DocColor::RGBA(234, 179, 8, 245)));
                rt->FillEllipse(D2D1::Ellipse(c, pr, pr), brush);
                brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 255)));
                rt->DrawEllipse(D2D1::Ellipse(c, pr, pr), brush, 1.5f);
                if (fmtPin) {
                    const std::wstring numW = std::to_wstring(pinIdx);
                    brush->SetColor(ToD2DColor(isSel ? DocColor::RGBA(255, 255, 255, 255) : DocColor::RGBA(24, 24, 28, 255)));
                    rt->DrawTextW(numW.c_str(), static_cast<UINT32>(numW.size()), fmtPin.Get(),
                        D2D1::RectF(c.x - pr, c.y - 7.0f, c.x + pr, c.y + 8.0f), brush);
                }
            }
        }

        // 2. Distraction-Free Reading View Mode (OB-EDIT-001)
        if (overlay->readingViewMode && widthPx >= 360 && heightPx >= 260) {
            const DocColor readBackdrop = isDark ? DocColor::RGBA(18, 21, 28, 252)
                                                 : (isHc ? DocColor::RGBA(255, 255, 255, 255) : DocColor::RGBA(245, 246, 248, 252));
            brush->SetColor(ToD2DColor(readBackdrop));
            rt->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<FLOAT>(widthPx), static_cast<FLOAT>(heightPx)), brush);

            const float sheetW = (std::min)(740.0f, static_cast<float>(widthPx) - 48.0f);
            const float sx0 = (static_cast<float>(widthPx) - sheetW) * 0.5f;
            const float sy0 = 24.0f;
            const float sy1 = static_cast<float>(heightPx) - 24.0f;
            const D2D1_RECT_F sheetR = D2D1::RectF(sx0, sy0, sx0 + sheetW, sy1);
            const PaperTintPreset readTint = (overlay->paperTintPreset >= 0 && overlay->paperTintPreset <= 4)
                ? static_cast<PaperTintPreset>(overlay->paperTintPreset)
                : PaperTintPreset::Porcelana;
            const bool nightRead = (readTint == PaperTintPreset::NocheSuave);
            const DocColor readSheetCol = (readTint != PaperTintPreset::Porcelana && !isHc)
                ? AccessibilityEngine::PaperTintSurfaceColor(readTint, isDark, isHc)
                : ovCardBg;
            const DocColor readInkCol = (isDark || nightRead)
                ? DocColor::RGBA(238, 242, 250, 255)
                : ovInk;
            brush->SetColor(ToD2DColor(readSheetCol));
            rt->FillRoundedRectangle(D2D1::RoundedRect(sheetR, 12.0f, 12.0f), brush);
            brush->SetColor(ToD2DColor(ovBorder));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(sheetR, 12.0f, 12.0f), brush, 1.0f);

            auto fmtTitle = makeTextFmt(20.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            ComPtr<IDWriteTextFormat> fmtBody;
            if (overlay->dyslexiaFriendlyMode && dwFactory) {
                dwFactory->CreateTextFormat(L"Verdana", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"es-ES", &fmtBody);
                if (fmtBody) {
                    fmtBody->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 22.8f, 17.5f);
                    fmtBody->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                }
            } else {
                fmtBody = makeTextFmt(13.5f, DWRITE_FONT_WEIGHT_NORMAL);
            }
            auto fmtMeta = makeTextFmt(10.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            const std::wstring titleW = Utf8ToWide(overlay->readingModeTitleUtf8.empty() ? "Vista de Lectura" : overlay->readingModeTitleUtf8);
            const std::wstring bodyW = Utf8ToWide(overlay->readingModeTextUtf8.empty()
                ? "Sin contenido textual en este documento. Agregue cuadros de texto o notas para visualizarlas en modo lectura."
                : overlay->readingModeTextUtf8);

            if (fmtMeta) {
                const std::wstring badge = overlay->dyslexiaFriendlyMode ? L"MODO LECTURA \u00B7 ALTA LEGIBILIDAD" : L"MODO LECTURA";
                brush->SetColor(ToD2DColor(ovAccent));
                rt->DrawTextW(badge.c_str(), static_cast<UINT32>(badge.size()), fmtMeta.Get(),
                    D2D1::RectF(sx0 + 36.0f, sy0 + 22.0f, sx0 + sheetW - 36.0f, sy0 + 40.0f), brush);
            }
            if (fmtTitle) {
                brush->SetColor(ToD2DColor(readInkCol));
                rt->DrawTextW(titleW.c_str(), static_cast<UINT32>(titleW.size()), fmtTitle.Get(),
                    D2D1::RectF(sx0 + 36.0f, sy0 + 42.0f, sx0 + sheetW - 36.0f, sy0 + 82.0f), brush);
            }
            brush->SetColor(ToD2DColor(ovBorder));
            rt->DrawLine(D2D1::Point2F(sx0 + 36.0f, sy0 + 86.0f), D2D1::Point2F(sx0 + sheetW - 36.0f, sy0 + 86.0f), brush, 1.0f);
            if (fmtBody) {
                brush->SetColor(ToD2DColor(readInkCol));
                rt->DrawTextW(bodyW.c_str(), static_cast<UINT32>(bodyW.size()), fmtBody.Get(),
                    D2D1::RectF(sx0 + 36.0f, sy0 + 102.0f, sx0 + sheetW - 36.0f, sy1 - 24.0f), brush);
            }
        }

        // 3. Flexcil Pop-up Study Note & Gesture Mode Overlay (FX-PDF-028..038)
        if (overlay->flexcilPopupVisible && overlay->flexcilOverlay && widthPx >= 400 && heightPx >= 260) {
            const DocRect pr = OverlayGeometry::FlexcilPopupPanelRect(widthPx, heightPx);
            const D2D1_RECT_F dr = ToD2DRect(pr);
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(dr, 12.0f, 12.0f);
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(0, 0, 0, 96) : DocColor::RGBA(15, 23, 42, 30)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(dr.left, dr.top + 3.0f, dr.right, dr.bottom + 3.0f), 12.0f, 12.0f), brush);
            brush->SetColor(ToD2DColor(ovBg));
            rt->FillRoundedRectangle(rr, brush);
            brush->SetColor(ToD2DColor(ovBorder));
            rt->DrawRoundedRectangle(rr, brush, 1.2f);

            auto fmtHdr = makeTextFmt(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            auto fmtSub = makeTextFmt(10.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            auto fmtTxt = makeTextFmt(11.0f, DWRITE_FONT_WEIGHT_NORMAL);

            if (fmtHdr) {
                const std::wstring h = L"Nota de estudio flotante";
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawTextW(h.c_str(), static_cast<UINT32>(h.size()), fmtHdr.Get(),
                    D2D1::RectF(dr.left + 14.0f, dr.top + 12.0f, dr.right - 120.0f, dr.top + 34.0f), brush);
            }
            // Gesture status indicator pill
            const D2D1_RECT_F pillR = D2D1::RectF(dr.right - 116.0f, dr.top + 10.0f, dr.right - 12.0f, dr.top + 32.0f);
            brush->SetColor(ToD2DColor(overlay->flexcilGestureMode ? ovAccent : (isDark ? DocColor::RGBA(52, 60, 78, 220) : DocColor::RGBA(230, 236, 246, 240))));
            rt->FillRoundedRectangle(D2D1::RoundedRect(pillR, 9.0f, 9.0f), brush);
            if (fmtSub) {
                fmtSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                const std::wstring gLbl = overlay->flexcilGestureMode ? L"Gestos: ON" : L"Gestos: OFF";
                brush->SetColor(ToD2DColor(overlay->flexcilGestureMode ? DocColor::RGBA(255, 255, 255, 255) : ovMuted));
                rt->DrawTextW(gLbl.c_str(), static_cast<UINT32>(gLbl.size()), fmtSub.Get(),
                    D2D1::RectF(pillR.left, pillR.top + 3.0f, pillR.right, pillR.bottom), brush);
                fmtSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            }
            brush->SetColor(ToD2DColor(ovBorder));
            rt->DrawLine(D2D1::Point2F(dr.left + 12.0f, dr.top + 40.0f), D2D1::Point2F(dr.right - 12.0f, dr.top + 40.0f), brush, 1.0f);

            const auto& studyDoc = overlay->flexcilOverlay->StudyNoteDocument();
            float cardY = dr.top + 50.0f;
            int shown = 0;
            if (!studyDoc.Pages().empty()) {
                for (const auto& obj : studyDoc.Pages()[0].objects) {
                    if (cardY + 62.0f > dr.bottom - 14.0f) break;
                    const D2D1_RECT_F cr = D2D1::RectF(dr.left + 14.0f, cardY, dr.right - 14.0f, cardY + 58.0f);
                    brush->SetColor(ToD2DColor(ovCardBg));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 8.0f, 8.0f), brush);
                    brush->SetColor(ToD2DColor(ovBorder));
                    rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 8.0f, 8.0f), brush, 1.0f);
                    std::string snippet = obj.textUtf8;
                    if (snippet.empty()) snippet = "Captura de estudio #" + std::to_string(obj.id);
                    if (snippet.size() > 92) snippet = snippet.substr(0, 89) + "...";
                    if (fmtTxt) {
                        const std::wstring wSnip = Utf8ToWide(snippet);
                        brush->SetColor(ToD2DColor(ovInk));
                        rt->DrawTextW(wSnip.c_str(), static_cast<UINT32>(wSnip.size()), fmtTxt.Get(),
                            D2D1::RectF(cr.left + 10.0f, cr.top + 8.0f, cr.right - 10.0f, cr.bottom - 20.0f), brush);
                    }
                    if (fmtSub) {
                        const std::wstring cite = Utf8ToWide("Cita vinculada · Clic para ir al origen");
                        brush->SetColor(ToD2DColor(ovAccent));
                        rt->DrawTextW(cite.c_str(), static_cast<UINT32>(cite.size()), fmtSub.Get(),
                            D2D1::RectF(cr.left + 10.0f, cr.bottom - 19.0f, cr.right - 10.0f, cr.bottom - 3.0f), brush);
                    }
                    cardY += 66.0f;
                    ++shown;
                }
            }
            if (shown == 0 && fmtTxt) {
                const std::wstring emptyMsg = L"Active el modo Gestos y trace una L, recuadro o subrayado sobre el documento para capturar citas vinculadas.";
                brush->SetColor(ToD2DColor(ovMuted));
                rt->DrawTextW(emptyMsg.c_str(), static_cast<UINT32>(emptyMsg.size()), fmtTxt.Get(),
                    D2D1::RectF(dr.left + 16.0f, dr.top + 56.0f, dr.right - 16.0f, dr.top + 140.0f), brush);
            }
        }

        // 4. Zoom Writing Box (Page highlight frame + Magnified bottom dock) (GN-INK-024 / NT-INK-019)
        if (overlay->zoomWritingBox && overlay->zoomWritingBox->active && widthPx >= 360 && heightPx >= 280) {
            const ZoomWritingBoxState& zb = *overlay->zoomWritingBox;
            // Draw target frame on active page
            if (doc.Mode() == DocumentMode::PaginatedPdf && overlay->zoomWritingPageIndex < doc.Pages().size()) {
                if (const auto* zplace = doc.Layout().PlacementAt(overlay->zoomWritingPageIndex)) {
                    const DocRect pageW = zplace->WorldRect();
                    const DocPoint tl = cam.WorldToScreen(pageW.x + zb.pageTargetRect.x, pageW.y + zb.pageTargetRect.y);
                    const DocPoint br = cam.WorldToScreen(pageW.x + zb.pageTargetRect.Right(), pageW.y + zb.pageTargetRect.Bottom());
                    const D2D1_RECT_F tr = D2D1::RectF(static_cast<FLOAT>(tl.x), static_cast<FLOAT>(tl.y),
                                                       static_cast<FLOAT>(br.x), static_cast<FLOAT>(br.y));
                    brush->SetColor(ToD2DColor(DocColor::RGBA(0, 122, 255, 28)));
                    rt->FillRectangle(tr, brush);
                    brush->SetColor(ToD2DColor(ovAccent));
                    rt->DrawRectangle(tr, brush, 1.8f);
                    // Auto-advance trigger zone on page frame
                    const float trigX = tr.left + (tr.right - tr.left) * static_cast<FLOAT>(zb.autoAdvanceTriggerRatio);
                    brush->SetColor(ToD2DColor(DocColor::RGBA(59, 130, 246, 52)));
                    rt->FillRectangle(D2D1::RectF(trigX, tr.top, tr.right, tr.bottom), brush);
                }
            }

            // Bottom magnified writing dock
            const DocRect dock = OverlayGeometry::ZoomBoxDockRect(widthPx, heightPx);
            const D2D1_RECT_F dr = ToD2DRect(dock);
            const D2D1_ROUNDED_RECT drr = D2D1::RoundedRect(dr, 12.0f, 12.0f);
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(0, 0, 0, 108) : DocColor::RGBA(15, 23, 42, 36)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(dr.left, dr.top + 3.0f, dr.right, dr.bottom + 3.0f), 12.0f, 12.0f), brush);
            brush->SetColor(ToD2DColor(ovCardBg));
            rt->FillRoundedRectangle(drr, brush);
            brush->SetColor(ToD2DColor(ovAccent));
            rt->DrawRoundedRectangle(drr, brush, 1.5f);

            const float padTop = 26.0f;
            const D2D1_RECT_F writeArea = D2D1::RectF(dr.left + 12.0f, dr.top + padTop, dr.right - 12.0f, dr.bottom - 10.0f);
            // Auto-advance shaded zone inside magnified dock
            const float advX = writeArea.left + (writeArea.right - writeArea.left) * static_cast<FLOAT>(zb.autoAdvanceTriggerRatio);
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(37, 99, 235, 38) : DocColor::RGBA(37, 99, 235, 24)));
            rt->FillRectangle(D2D1::RectF(advX, writeArea.top, writeArea.right, writeArea.bottom), brush);
            brush->SetColor(ToD2DColor(ovBorder));
            for (int rLine = 1; rLine <= 3; ++rLine) {
                const float ly = writeArea.top + (writeArea.bottom - writeArea.top) * (static_cast<float>(rLine) / 4.0f);
                rt->DrawLine(D2D1::Point2F(writeArea.left, ly), D2D1::Point2F(writeArea.right, ly), brush, 0.7f);
            }

            // Render existing strokes inside target rect magnified into the dock
            if (doc.Mode() == DocumentMode::PaginatedPdf && overlay->zoomWritingPageIndex < doc.Pages().size() &&
                zb.pageTargetRect.w > 1.0 && zb.pageTargetRect.h > 1.0) {
                const float sx = (writeArea.right - writeArea.left) / static_cast<FLOAT>(zb.pageTargetRect.w);
                const float sy = (writeArea.bottom - writeArea.top) / static_cast<FLOAT>(zb.pageTargetRect.h);
                rt->PushAxisAlignedClip(writeArea, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                for (const auto& obj : doc.Pages()[overlay->zoomWritingPageIndex].objects) {
                    if (obj.kind != ObjectKind::InkStroke || obj.inkPoints.size() < 2) continue;
                    if (!obj.bounds.Intersects(zb.pageTargetRect)) continue;
                    brush->SetColor(ToD2DColor(obj.strokeColor));
                    const float sw = (std::max)(1.5f, static_cast<FLOAT>(obj.strokeWidth * 0.5 * (sx + sy)));
                    for (std::size_t i = 1; i < obj.inkPoints.size(); ++i) {
                        const auto& p0 = obj.inkPoints[i - 1];
                        const auto& p1 = obj.inkPoints[i];
                        rt->DrawLine(
                            D2D1::Point2F(writeArea.left + static_cast<FLOAT>(p0.x - zb.pageTargetRect.x) * sx,
                                          writeArea.top + static_cast<FLOAT>(p0.y - zb.pageTargetRect.y) * sy),
                            D2D1::Point2F(writeArea.left + static_cast<FLOAT>(p1.x - zb.pageTargetRect.x) * sx,
                                          writeArea.top + static_cast<FLOAT>(p1.y - zb.pageTargetRect.y) * sy),
                            brush, sw);
                    }
                }
                rt->PopAxisAlignedClip();
            }

            auto fmtZ = makeTextFmt(10.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            if (fmtZ) {
                const std::wstring zLbl = Utf8ToWide("Cuadro de escritura con zoom (" +
                    std::to_string(static_cast<int>(std::round(zb.zoomFactor * 100.0))) + "% · avances: " +
                    std::to_string(zb.autoAdvanceCount) + ", saltos: " + std::to_string(zb.lineWrapCount) + ")");
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawTextW(zLbl.c_str(), static_cast<UINT32>(zLbl.size()), fmtZ.Get(),
                    D2D1::RectF(dr.left + 14.0f, dr.top + 6.0f, dr.right - 14.0f, dr.top + 24.0f), brush);
            }
        }

        // 5. Structured Database Panel (All 9 Visual Views: Table..Map) (OB-DATA / NO-DATA)
        if (overlay->dbPanelVisible && overlay->activeDatabase && widthPx >= 460 && heightPx >= 320) {
            const StructuredDatabase& db = *overlay->activeDatabase;
            const DocRect dp = OverlayGeometry::DatabasePanelRect(widthPx, heightPx);
            const D2D1_RECT_F dr = ToD2DRect(dp);
            const D2D1_ROUNDED_RECT drr = D2D1::RoundedRect(dr, 14.0f, 14.0f);
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(0, 0, 0, 108) : DocColor::RGBA(15, 23, 42, 34)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(dr.left, dr.top + 4.0f, dr.right, dr.bottom + 4.0f), 14.0f, 14.0f), brush);
            brush->SetColor(ToD2DColor(ovBg));
            rt->FillRoundedRectangle(drr, brush);
            brush->SetColor(ToD2DColor(ovBorder));
            rt->DrawRoundedRectangle(drr, brush, 1.2f);

            auto fmtTitle = makeTextFmt(13.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            auto fmtTab = makeTextFmt(10.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
            auto fmtCell = makeTextFmt(10.5f, DWRITE_FONT_WEIGHT_NORMAL);
            auto fmtCellBold = makeTextFmt(10.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);

            if (fmtTitle) {
                const std::wstring wTitle = Utf8ToWide(db.Title() + " (" + std::to_string(db.Records().size()) + " registros)");
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawTextW(wTitle.c_str(), static_cast<UINT32>(wTitle.size()), fmtTitle.Get(),
                    D2D1::RectF(dr.left + 18.0f, dr.top + 12.0f, dr.right - 252.0f, dr.top + 34.0f), brush);
            }
            // Compact guías de interfaz Database Action Buttons (Editor CRUD / Formulario / Boton de Accion)
            {
                const wchar_t* kDbAct[3] = { L"Editar BD", L"Formulario", L"Acci\u00F3n" };
                for (int ai = 0; ai < 3; ++ai) {
                    const float bx0 = dr.right - 246.0f + ai * 78.0f;
                    const D2D1_RECT_F br = D2D1::RectF(bx0, dr.top + 10.0f, bx0 + 72.0f, dr.top + 32.0f);
                    brush->SetColor(ToD2DColor(ai == 2 ? ovAccent
                        : (isDark ? DocColor::RGBA(42, 50, 66, 230) : DocColor::RGBA(232, 238, 248, 245))));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(br, 6.0f, 6.0f), brush);
                    if (fmtTab) {
                        brush->SetColor(ToD2DColor(ai == 2 ? DocColor::RGBA(255, 255, 255, 255) : ovInk));
                        rt->DrawTextW(kDbAct[ai], static_cast<UINT32>(std::wcslen(kDbAct[ai])), fmtTab.Get(),
                            D2D1::RectF(br.left, br.top + 4.0f, br.right, br.bottom), brush);
                    }
                }
            }

            // Segmented view tabs (9 views)
            static const wchar_t* kDbTabNames[9] = {
                L"Tabla", L"Lista", L"Galer\u00EDa", L"Kanban", L"Calendario", L"Cronograma", L"Gr\u00E1fico", L"Panel", L"Mapa"
            };
            const int activeView = std::clamp(overlay->dbViewKind, 0, 8);
            const float tabY0 = dr.top + 38.0f;
            const float tabY1 = dr.top + 64.0f;
            const float tabW = (dr.right - dr.left - 36.0f) / 9.0f;
            for (int ti = 0; ti < 9; ++ti) {
                const D2D1_RECT_F tr = D2D1::RectF(dr.left + 18.0f + ti * tabW + 2.0f, tabY0,
                                                   dr.left + 18.0f + (ti + 1) * tabW - 2.0f, tabY1);
                brush->SetColor(ToD2DColor(ti == activeView ? ovAccent
                    : (isDark ? DocColor::RGBA(40, 46, 62, 220) : DocColor::RGBA(236, 240, 248, 240))));
                rt->FillRoundedRectangle(D2D1::RoundedRect(tr, 6.0f, 6.0f), brush);
                if (fmtTab) {
                    brush->SetColor(ToD2DColor(ti == activeView ? DocColor::RGBA(255, 255, 255, 255) : ovInk));
                    rt->DrawTextW(kDbTabNames[ti], static_cast<UINT32>(std::wcslen(kDbTabNames[ti])), fmtTab.Get(),
                        D2D1::RectF(tr.left, tr.top + 5.0f, tr.right, tr.bottom), brush);
                }
            }

            // Build dynamic view configuration from active saved view (filters/sorts/groups) or schema
            DbViewConfig vcfg{};
            if (const auto* savedView = db.FindView(db.ActiveViewId())) {
                vcfg = *savedView;
            }
            vcfg.kind = static_cast<DbViewKind>(activeView);
            std::string statusCol = vcfg.groupByColumnIdUtf8, dateCol = vcfg.dateColumnIdUtf8, endDateCol = vcfg.endDateColumnIdUtf8;
            std::string numCol = vcfg.chartValueColumnIdUtf8, latCol = vcfg.latColumnIdUtf8, lonCol = vcfg.lonColumnIdUtf8;
            for (const auto& c : db.Columns()) {
                if (statusCol.empty() && (c.type == DbColumnType::Status || c.type == DbColumnType::MultiSelect)) statusCol = c.idUtf8;
                if (dateCol.empty() && c.type == DbColumnType::Date) dateCol = c.idUtf8;
                else if (endDateCol.empty() && c.type == DbColumnType::Date) endDateCol = c.idUtf8;
                if (numCol.empty() && (c.type == DbColumnType::Number || c.type == DbColumnType::Formula || c.type == DbColumnType::Rollup)) numCol = c.idUtf8;
                if (c.idUtf8 == "lat" || c.idUtf8 == "latitude") latCol = c.idUtf8;
                if (c.idUtf8 == "lon" || c.idUtf8 == "longitude") lonCol = c.idUtf8;
            }
            if (statusCol.empty() && !db.Columns().empty()) statusCol = db.Columns().front().idUtf8;
            vcfg.groupByColumnIdUtf8 = statusCol;
            vcfg.dateColumnIdUtf8 = dateCol;
            vcfg.endDateColumnIdUtf8 = endDateCol.empty() ? dateCol : endDateCol;
            vcfg.chartValueColumnIdUtf8 = numCol;
            vcfg.latColumnIdUtf8 = latCol;
            vcfg.lonColumnIdUtf8 = lonCol;

            const D2D1_RECT_F body = D2D1::RectF(dr.left + 18.0f, dr.top + 74.0f, dr.right - 18.0f, dr.bottom - 16.0f);
            rt->PushAxisAlignedClip(body, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            const auto recs = db.QueryViewRecords(vcfg);
            if (activeView == 0) { // Table
                const std::size_t colCnt = (std::min<std::size_t>)(db.Columns().size(), 4u) + 1u;
                const float colW = (body.right - body.left) / static_cast<float>(colCnt);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(38, 46, 62, 240) : DocColor::RGBA(238, 242, 249, 250)));
                rt->FillRectangle(D2D1::RectF(body.left, body.top, body.right, body.top + 28.0f), brush);
                if (fmtCellBold) {
                    brush->SetColor(ToD2DColor(ovInk));
                    rt->DrawTextW(L"T\u00EDtulo", 6, fmtCellBold.Get(),
                        D2D1::RectF(body.left + 6.0f, body.top + 6.0f, body.left + colW - 4.0f, body.top + 26.0f), brush);
                    for (std::size_t ci = 0; ci + 1 < colCnt; ++ci) {
                        const std::wstring wn = Utf8ToWide(db.Columns()[ci].nameUtf8);
                        rt->DrawTextW(wn.c_str(), static_cast<UINT32>(wn.size()), fmtCellBold.Get(),
                            D2D1::RectF(body.left + (ci + 1) * colW + 6.0f, body.top + 6.0f, body.left + (ci + 2) * colW - 4.0f, body.top + 26.0f), brush);
                    }
                }
                float ry = body.top + 30.0f;
                for (std::size_t ri = 0; ri < recs.size() && ry + 26.0f <= body.bottom; ++ri) {
                    if (ri % 2 == 1) {
                        brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(32, 38, 52, 180) : DocColor::RGBA(246, 248, 252, 220)));
                        rt->FillRectangle(D2D1::RectF(body.left, ry, body.right, ry + 26.0f), brush);
                    }
                    if (fmtCell) {
                        brush->SetColor(ToD2DColor(ovInk));
                        const std::wstring wt = Utf8ToWide(recs[ri].titleUtf8);
                        rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtCell.Get(),
                            D2D1::RectF(body.left + 6.0f, ry + 4.0f, body.left + colW - 4.0f, ry + 24.0f), brush);
                        for (std::size_t ci = 0; ci + 1 < colCnt; ++ci) {
                            const auto& col = db.Columns()[ci];
                            std::string val;
                            if (auto it = recs[ri].cells.find(col.idUtf8); it != recs[ri].cells.end()) {
                                if (!it->second.textUtf8.empty()) val = it->second.textUtf8;
                                else if (col.type == DbColumnType::Number || col.type == DbColumnType::Formula || col.type == DbColumnType::Rollup) {
                                    char nb[32]; std::snprintf(nb, sizeof(nb), "%.2f", it->second.numberVal); val = nb;
                                } else if (col.type == DbColumnType::Checkbox) val = it->second.boolVal ? "Si" : "No";
                            }
                            const std::wstring wv = Utf8ToWide(val);
                            rt->DrawTextW(wv.c_str(), static_cast<UINT32>(wv.size()), fmtCell.Get(),
                                D2D1::RectF(body.left + (ci + 1) * colW + 6.0f, ry + 4.0f, body.left + (ci + 2) * colW - 4.0f, ry + 24.0f), brush);
                        }
                    }
                    brush->SetColor(ToD2DColor(ovBorder));
                    rt->DrawLine(D2D1::Point2F(body.left, ry + 26.0f), D2D1::Point2F(body.right, ry + 26.0f), brush, 0.6f);
                    ry += 26.0f;
                }
            } else if (activeView == 1) { // List
                float ry = body.top;
                for (const auto& r : recs) {
                    if (ry + 32.0f > body.bottom) break;
                    const D2D1_RECT_F rowR = D2D1::RectF(body.left, ry, body.right, ry + 28.0f);
                    brush->SetColor(ToD2DColor(ovCardBg));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(rowR, 6.0f, 6.0f), brush);
                    brush->SetColor(ToD2DColor(ovBorder));
                    rt->DrawRoundedRectangle(D2D1::RoundedRect(rowR, 6.0f, 6.0f), brush, 0.8f);
                    std::string statusTxt;
                    if (!statusCol.empty()) {
                        if (auto it = r.cells.find(statusCol); it != r.cells.end()) statusTxt = it->second.textUtf8;
                    }
                    if (fmtCellBold) {
                        const std::wstring wt = Utf8ToWide(r.titleUtf8);
                        brush->SetColor(ToD2DColor(ovInk));
                        rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtCellBold.Get(),
                            D2D1::RectF(rowR.left + 12.0f, rowR.top + 6.0f, rowR.right - 130.0f, rowR.bottom - 4.0f), brush);
                    }
                    if (!statusTxt.empty() && fmtCell) {
                        const std::wstring ws = Utf8ToWide(statusTxt);
                        brush->SetColor(ToD2DColor(ovAccent));
                        rt->DrawTextW(ws.c_str(), static_cast<UINT32>(ws.size()), fmtCell.Get(),
                            D2D1::RectF(rowR.right - 124.0f, rowR.top + 6.0f, rowR.right - 10.0f, rowR.bottom - 4.0f), brush);
                    }
                    ry += 34.0f;
                }
            } else if (activeView == 2) { // Cards Gallery
                const int cols = 3;
                const float gap = 10.0f;
                const float cw = (body.right - body.left - gap * (cols - 1)) / cols;
                const float ch = 84.0f;
                for (std::size_t i = 0; i < recs.size(); ++i) {
                    const int cx = static_cast<int>(i % cols);
                    const int cy = static_cast<int>(i / cols);
                    const float x0 = body.left + cx * (cw + gap);
                    const float y0 = body.top + cy * (ch + gap);
                    if (y0 + ch > body.bottom) break;
                    const D2D1_RECT_F cr = D2D1::RectF(x0, y0, x0 + cw, y0 + ch);
                    brush->SetColor(ToD2DColor(ovCardBg));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 8.0f, 8.0f), brush);
                    brush->SetColor(ToD2DColor(ovBorder));
                    rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 8.0f, 8.0f), brush, 1.0f);
                    if (fmtCellBold) {
                        const std::wstring wt = Utf8ToWide(recs[i].titleUtf8);
                        brush->SetColor(ToD2DColor(ovInk));
                        rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtCellBold.Get(),
                            D2D1::RectF(cr.left + 10.0f, cr.top + 10.0f, cr.right - 10.0f, cr.top + 32.0f), brush);
                    }
                    if (!statusCol.empty() && fmtCell) {
                        if (auto it = recs[i].cells.find(statusCol); it != recs[i].cells.end()) {
                            const std::wstring ws = Utf8ToWide(it->second.textUtf8);
                            brush->SetColor(ToD2DColor(ovAccent));
                            rt->DrawTextW(ws.c_str(), static_cast<UINT32>(ws.size()), fmtCell.Get(),
                                D2D1::RectF(cr.left + 10.0f, cr.top + 36.0f, cr.right - 10.0f, cr.bottom - 8.0f), brush);
                        }
                    }
                }
            } else if (activeView == 3) { // Kanban
                const auto buckets = db.QueryGroupedBuckets(vcfg, numCol);
                const std::size_t nCol = (std::max<std::size_t>)(1u, (std::min<std::size_t>)(buckets.size(), 4u));
                const float gap = 10.0f;
                const float kw = (body.right - body.left - gap * (nCol - 1)) / static_cast<float>(nCol);
                for (std::size_t bi = 0; bi < buckets.size() && bi < 4u; ++bi) {
                    const float x0 = body.left + bi * (kw + gap);
                    const D2D1_RECT_F colR = D2D1::RectF(x0, body.top, x0 + kw, body.bottom);
                    brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(32, 38, 50, 210) : DocColor::RGBA(240, 244, 250, 235)));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(colR, 10.0f, 10.0f), brush);
                    if (fmtCellBold) {
                        const std::wstring wh = Utf8ToWide(buckets[bi].groupKeyUtf8 + " (" + std::to_string(buckets[bi].records.size()) + ")");
                        brush->SetColor(ToD2DColor(ovInk));
                        rt->DrawTextW(wh.c_str(), static_cast<UINT32>(wh.size()), fmtCellBold.Get(),
                            D2D1::RectF(x0 + 10.0f, body.top + 8.0f, x0 + kw - 10.0f, body.top + 28.0f), brush);
                    }
                    float ky = body.top + 34.0f;
                    for (const auto& rec : buckets[bi].records) {
                        if (ky + 44.0f > body.bottom - 8.0f) break;
                        const D2D1_RECT_F cr = D2D1::RectF(x0 + 8.0f, ky, x0 + kw - 8.0f, ky + 40.0f);
                        brush->SetColor(ToD2DColor(ovCardBg));
                        rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 7.0f, 7.0f), brush);
                        brush->SetColor(ToD2DColor(ovBorder));
                        rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 7.0f, 7.0f), brush, 0.9f);
                        if (fmtCell) {
                            const std::wstring wt = Utf8ToWide(rec.titleUtf8);
                            brush->SetColor(ToD2DColor(ovInk));
                            rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtCell.Get(),
                                D2D1::RectF(cr.left + 8.0f, cr.top + 8.0f, cr.right - 8.0f, cr.bottom - 4.0f), brush);
                        }
                        ky += 46.0f;
                    }
                }
            } else if (activeView == 4) { // Calendar
                const auto entries = db.BuildCalendarProjection(vcfg);
                const float cellW = (body.right - body.left) / 7.0f;
                const float cellH = (body.bottom - body.top - 22.0f) / 5.0f;
                static const wchar_t* kDays[7] = { L"Lun", L"Mar", L"Mi\u00E9", L"Jue", L"Vie", L"S\u00E1b", L"Dom" };
                for (int d = 0; d < 7; ++d) {
                    if (fmtTab) {
                        brush->SetColor(ToD2DColor(ovMuted));
                        rt->DrawTextW(kDays[d], static_cast<UINT32>(std::wcslen(kDays[d])), fmtTab.Get(),
                            D2D1::RectF(body.left + d * cellW, body.top, body.left + (d + 1) * cellW, body.top + 18.0f), brush);
                    }
                }
                for (int idx = 0; idx < 35; ++idx) {
                    const int col = idx % 7;
                    const int row = idx / 7;
                    const D2D1_RECT_F cr = D2D1::RectF(body.left + col * cellW, body.top + 22.0f + row * cellH,
                                                       body.left + (col + 1) * cellW, body.top + 22.0f + (row + 1) * cellH);
                    brush->SetColor(ToD2DColor(ovBorder));
                    rt->DrawRectangle(cr, brush, 0.6f);
                    if (idx < 31 && fmtCell) {
                        const std::wstring dnum = std::to_wstring(idx + 1);
                        brush->SetColor(ToD2DColor(ovMuted));
                        rt->DrawTextW(dnum.c_str(), static_cast<UINT32>(dnum.size()), fmtCell.Get(),
                            D2D1::RectF(cr.left + 4.0f, cr.top + 2.0f, cr.left + 26.0f, cr.top + 18.0f), brush);
                        // Show matching day entries
                        float ey = cr.top + 18.0f;
                        for (const auto& e : entries) {
                            int eDay = 0;
                            if (e.dateIsoUtf8.size() >= 10) eDay = std::atoi(e.dateIsoUtf8.substr(8, 2).c_str());
                            if (eDay == idx + 1 && ey + 15.0f <= cr.bottom) {
                                brush->SetColor(ToD2DColor(ovAccent));
                                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cr.left + 3.0f, ey, cr.right - 3.0f, ey + 14.0f), 3.0f, 3.0f), brush);
                                const std::wstring et = Utf8ToWide(e.titleUtf8);
                                brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 255)));
                                rt->DrawTextW(et.c_str(), static_cast<UINT32>(et.size()), fmtCell.Get(),
                                    D2D1::RectF(cr.left + 5.0f, ey, cr.right - 4.0f, ey + 14.0f), brush);
                                ey += 16.0f;
                            }
                        }
                    }
                }
            } else if (activeView == 5) { // Timeline
                const auto bars = db.BuildTimelineProjection(vcfg);
                float ty = body.top + 8.0f;
                for (std::size_t bi = 0; bi < bars.size() && ty + 30.0f <= body.bottom; ++bi) {
                    const float barX = body.left + 150.0f + static_cast<float>((bi % 5) * 32);
                    const float barW = std::clamp(static_cast<float>(bars[bi].durationDays + 1) * 28.0f, 64.0f, body.right - barX - 12.0f);
                    if (fmtCell) {
                        const std::wstring wt = Utf8ToWide(bars[bi].titleUtf8);
                        brush->SetColor(ToD2DColor(ovInk));
                        rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtCell.Get(),
                            D2D1::RectF(body.left + 4.0f, ty + 4.0f, body.left + 144.0f, ty + 24.0f), brush);
                    }
                    brush->SetColor(ToD2DColor(ovAccent));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(barX, ty + 3.0f, barX + barW, ty + 23.0f), 6.0f, 6.0f), brush);
                    if (fmtCell) {
                        const std::wstring wd = Utf8ToWide(bars[bi].startDateIsoUtf8 + " (" + std::to_string((std::max)(1, bars[bi].durationDays)) + "d)");
                        brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 255)));
                        rt->DrawTextW(wd.c_str(), static_cast<UINT32>(wd.size()), fmtCell.Get(),
                            D2D1::RectF(barX + 6.0f, ty + 4.0f, barX + barW - 4.0f, ty + 22.0f), brush);
                    }
                    ty += 32.0f;
                }
            } else if (activeView == 6) { // Chart
                const auto pts = db.BuildChartProjection(vcfg);
                double maxV = 1.0;
                for (const auto& p : pts) maxV = (std::max)(maxV, std::abs(p.value));
                const float baseLineY = body.bottom - 34.0f;
                const float chartH = (std::max)(40.0f, baseLineY - body.top - 24.0f);
                brush->SetColor(ToD2DColor(ovBorder));
                rt->DrawLine(D2D1::Point2F(body.left + 20.0f, baseLineY), D2D1::Point2F(body.right - 20.0f, baseLineY), brush, 1.2f);
                if (!pts.empty()) {
                    const float slotW = (body.right - body.left - 48.0f) / static_cast<float>(pts.size());
                    const float barW = (std::min)(54.0f, slotW * 0.62f);
                    for (std::size_t i = 0; i < pts.size(); ++i) {
                        const float bh = static_cast<float>(std::abs(pts[i].value) / maxV) * chartH;
                        const float bx = body.left + 24.0f + i * slotW + (slotW - barW) * 0.5f;
                        brush->SetColor(ToD2DColor(ovAccent));
                        rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(bx, baseLineY - bh, bx + barW, baseLineY), 5.0f, 5.0f), brush);
                        if (fmtTab) {
                            const std::wstring wc = Utf8ToWide(pts[i].categoryUtf8);
                            brush->SetColor(ToD2DColor(ovInk));
                            rt->DrawTextW(wc.c_str(), static_cast<UINT32>(wc.size()), fmtTab.Get(),
                                D2D1::RectF(bx - 12.0f, baseLineY + 6.0f, bx + barW + 12.0f, body.bottom), brush);
                        }
                    }
                }
            } else if (activeView == 7) { // Dashboard
                const auto dash = db.BuildDashboardProjection(vcfg, statusCol, numCol);
                const float kpiW = (body.right - body.left - 24.0f) / 3.0f;
                struct KpiItem { std::wstring label; std::wstring val; };
                char pctBuf[32], avgBuf[32];
                std::snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", dash.completionRatePct);
                std::snprintf(avgBuf, sizeof(avgBuf), "%.2f", dash.primaryNumericAvg);
                const KpiItem kpis[3] = {
                    { L"Registros", std::to_wstring(dash.totalRecords) },
                    { L"Completados", Utf8ToWide(std::to_string(dash.completedCount) + " (" + pctBuf + ")") },
                    { L"Promedio", Utf8ToWide(avgBuf) }
                };
                for (int ki = 0; ki < 3; ++ki) {
                    const D2D1_RECT_F kr = D2D1::RectF(body.left + ki * (kpiW + 12.0f), body.top,
                                                       body.left + ki * (kpiW + 12.0f) + kpiW, body.top + 74.0f);
                    brush->SetColor(ToD2DColor(ovCardBg));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(kr, 10.0f, 10.0f), brush);
                    brush->SetColor(ToD2DColor(ovBorder));
                    rt->DrawRoundedRectangle(D2D1::RoundedRect(kr, 10.0f, 10.0f), brush, 1.0f);
                    if (fmtCell) {
                        brush->SetColor(ToD2DColor(ovMuted));
                        rt->DrawTextW(kpis[ki].label.c_str(), static_cast<UINT32>(kpis[ki].label.size()), fmtCell.Get(),
                            D2D1::RectF(kr.left + 12.0f, kr.top + 10.0f, kr.right - 12.0f, kr.top + 30.0f), brush);
                    }
                    if (fmtTitle) {
                        brush->SetColor(ToD2DColor(ovAccent));
                        rt->DrawTextW(kpis[ki].val.c_str(), static_cast<UINT32>(kpis[ki].val.size()), fmtTitle.Get(),
                            D2D1::RectF(kr.left + 12.0f, kr.top + 34.0f, kr.right - 12.0f, kr.bottom - 8.0f), brush);
                    }
                }
                float by = body.top + 92.0f;
                for (const auto& bk : dash.breakdownByGroup) {
                    if (by + 28.0f > body.bottom) break;
                    const float ratio = dash.totalRecords > 0 ? static_cast<float>(bk.value / dash.totalRecords) : 0.0f;
                    const float barMax = body.right - body.left - 180.0f;
                    if (fmtCell) {
                        const std::wstring wl = Utf8ToWide(bk.categoryUtf8);
                        brush->SetColor(ToD2DColor(ovInk));
                        rt->DrawTextW(wl.c_str(), static_cast<UINT32>(wl.size()), fmtCell.Get(),
                            D2D1::RectF(body.left, by + 2.0f, body.left + 160.0f, by + 22.0f), brush);
                    }
                    brush->SetColor(ToD2DColor(ovAccent));
                    rt->FillRoundedRectangle(D2D1::RoundedRect(
                        D2D1::RectF(body.left + 168.0f, by + 4.0f, body.left + 168.0f + (std::max)(8.0f, barMax * ratio), by + 18.0f), 4.0f, 4.0f), brush);
                    by += 30.0f;
                }
            } else if (activeView == 8) { // Map
                const auto pins = db.BuildMapProjection(vcfg);
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(22, 34, 48, 240) : DocColor::RGBA(232, 243, 252, 245)));
                rt->FillRoundedRectangle(D2D1::RoundedRect(body, 10.0f, 10.0f), brush);
                brush->SetColor(ToD2DColor(ovBorder));
                for (int g = 1; g < 6; ++g) {
                    const float gx = body.left + (body.right - body.left) * (g / 6.0f);
                    const float gy = body.top + (body.bottom - body.top) * (g / 6.0f);
                    rt->DrawLine(D2D1::Point2F(gx, body.top), D2D1::Point2F(gx, body.bottom), brush, 0.5f);
                    rt->DrawLine(D2D1::Point2F(body.left, gy), D2D1::Point2F(body.right, gy), brush, 0.5f);
                }
                for (const auto& pin : pins) {
                    const float px = body.left + 20.0f + static_cast<float>(pin.normX) * (body.right - body.left - 40.0f);
                    const float py = body.top + 20.0f + static_cast<float>(pin.normY) * (body.bottom - body.top - 40.0f);
                    brush->SetColor(ToD2DColor(ovAccent));
                    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 6.0f, 6.0f), brush);
                    brush->SetColor(ToD2DColor(DocColor::RGBA(255, 255, 255, 255)));
                    rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 6.0f, 6.0f), brush, 1.4f);
                    if (fmtCellBold) {
                        const std::wstring wl = Utf8ToWide(pin.titleUtf8);
                        brush->SetColor(ToD2DColor(ovInk));
                        rt->DrawTextW(wl.c_str(), static_cast<UINT32>(wl.size()), fmtCellBold.Get(),
                            D2D1::RectF(px + 9.0f, py - 8.0f, (std::min)(body.right - 6.0f, px + 160.0f), py + 12.0f), brush);
                    }
                }
            }
            rt->PopAxisAlignedClip();
        }

        // 6. Anchored Comments Side Panel (COL)
        if (overlay->commentsPanelVisible && overlay->commentThreads && widthPx >= 360 && heightPx >= 260) {
            const DocRect cp = OverlayGeometry::CommentsPanelRect(widthPx, heightPx);
            const D2D1_RECT_F dr = ToD2DRect(cp);
            brush->SetColor(ToD2DColor(ovBg));
            rt->FillRoundedRectangle(D2D1::RoundedRect(dr, 12.0f, 12.0f), brush);
            brush->SetColor(ToD2DColor(ovBorder));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(dr, 12.0f, 12.0f), brush, 1.2f);
            auto fmtHdr = makeTextFmt(12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            auto fmtTxt = makeTextFmt(10.5f, DWRITE_FONT_WEIGHT_NORMAL);
            if (fmtHdr) {
                const std::wstring h = Utf8ToWide("Comentarios anclados (" + std::to_string(overlay->commentThreads->AllThreads().size()) + ")");
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawTextW(h.c_str(), static_cast<UINT32>(h.size()), fmtHdr.Get(),
                    D2D1::RectF(dr.left + 14.0f, dr.top + 12.0f, dr.right - 14.0f, dr.top + 34.0f), brush);
            }
            float cy = dr.top + 42.0f;
            int idx = 0;
            for (const auto& th : overlay->commentThreads->AllThreads()) {
                ++idx;
                if (cy + 58.0f > dr.bottom - 12.0f) break;
                const D2D1_RECT_F cr = D2D1::RectF(dr.left + 12.0f, cy, dr.right - 12.0f, cy + 54.0f);
                brush->SetColor(ToD2DColor(ovCardBg));
                rt->FillRoundedRectangle(D2D1::RoundedRect(cr, 8.0f, 8.0f), brush);
                brush->SetColor(ToD2DColor(th.resolved ? ovBorder : ovAccent));
                rt->DrawRoundedRectangle(D2D1::RoundedRect(cr, 8.0f, 8.0f), brush, 1.0f);
                if (fmtTxt) {
                    std::string firstAuthor = th.replies.empty() ? "Autor" : th.replies.front().authorUtf8;
                    std::string firstBody = th.replies.empty() ? "" : th.replies.front().bodyUtf8;
                    const std::wstring line = Utf8ToWide("#" + std::to_string(idx) + " [P" + std::to_string(th.pageIndex0 + 1) + "] " +
                        firstAuthor + (th.resolved ? " (Resuelto)" : "") + ": " + firstBody);
                    brush->SetColor(ToD2DColor(ovInk));
                    rt->DrawTextW(line.c_str(), static_cast<UINT32>(line.size()), fmtTxt.Get(),
                        D2D1::RectF(cr.left + 8.0f, cr.top + 6.0f, cr.right - 8.0f, cr.bottom - 6.0f), brush);
                }
                cy += 60.0f;
            }
        }

        // 7. Synchronized Audio Timeline Bar (AUD)
        if ((overlay->audioTimelineVisible || overlay->audioRecording || overlay->audioPlaying) &&
            widthPx >= 360 && heightPx >= 200 &&
            !(overlay->zoomWritingBox && overlay->zoomWritingBox->active)) {
            const DocRect ab = OverlayGeometry::AudioTimelineBarRect(widthPx, heightPx);
            const D2D1_RECT_F dr = ToD2DRect(ab);
            brush->SetColor(ToD2DColor(ovBg));
            rt->FillRoundedRectangle(D2D1::RoundedRect(dr, 12.0f, 12.0f), brush);
            brush->SetColor(ToD2DColor(ovBorder));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(dr, 12.0f, 12.0f), brush, 1.1f);

            // Recording / playing indicator dot
            const DocColor dotCol = overlay->audioRecording ? DocColor::RGBA(239, 68, 68, 255)
                                  : (overlay->audioPlaying ? ovAccent : ovMuted);
            brush->SetColor(ToD2DColor(dotCol));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dr.left + 20.0f, (dr.top + dr.bottom) * 0.5f), 5.5f, 5.5f), brush);

            auto fmtAud = makeTextFmt(10.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            auto fmtTimeStr = [](std::uint64_t ms) -> std::string {
                const std::uint64_t sec = ms / 1000u;
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%02llu:%02llu", static_cast<unsigned long long>(sec / 60u), static_cast<unsigned long long>(sec % 60u));
                return buf;
            };
            if (fmtAud) {
                char spd[24];
                std::snprintf(spd, sizeof(spd), "%.1fx", overlay->audioSpeed);
                const std::wstring wt = Utf8ToWide(fmtTimeStr(overlay->audioCursorMs) + " / " + fmtTimeStr(overlay->audioDurationMs) + "  " + spd);
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtAud.Get(),
                    D2D1::RectF(dr.left + 34.0f, dr.top + 17.0f, dr.left + 168.0f, dr.bottom - 10.0f), brush);
            }

            // Waveform envelope + scrubber cursor + bookmarks
            const D2D1_RECT_F wf = D2D1::RectF(dr.left + 172.0f, dr.top + 10.0f, dr.right - 16.0f, dr.bottom - 10.0f);
            const float midY = (wf.top + wf.bottom) * 0.5f;
            const float halfH = (wf.bottom - wf.top) * 0.46f;
            const std::size_t nBins = overlay->audioEnvelopeBins.empty() ? 48u : overlay->audioEnvelopeBins.size();
            const float stepX = (wf.right - wf.left) / static_cast<float>(nBins);
            const double progress = (overlay->audioDurationMs > 0)
                ? std::clamp(static_cast<double>(overlay->audioCursorMs) / static_cast<double>(overlay->audioDurationMs), 0.0, 1.0)
                : 0.0;
            for (std::size_t bi = 0; bi < nBins; ++bi) {
                const float amp = overlay->audioEnvelopeBins.empty()
                    ? (0.18f + 0.15f * static_cast<float>(std::sin(bi * 0.55)))
                    : std::clamp(overlay->audioEnvelopeBins[bi], 0.08f, 1.0f);
                const float bx = wf.left + bi * stepX + stepX * 0.5f;
                const bool played = (static_cast<double>(bi) / static_cast<double>(nBins)) <= progress;
                brush->SetColor(ToD2DColor(played ? ovAccent : ovMuted));
                rt->DrawLine(D2D1::Point2F(bx, midY - amp * halfH), D2D1::Point2F(bx, midY + amp * halfH), brush, (std::max)(1.5f, stepX * 0.45f));
            }
            if (overlay->audioDurationMs > 0) {
                // Bookmarks in amber
                brush->SetColor(ToD2DColor(DocColor::RGBA(245, 158, 11, 255)));
                for (std::uint64_t bms : overlay->audioBookmarkMs) {
                    const float rx = wf.left + static_cast<FLOAT>(std::clamp(static_cast<double>(bms) / static_cast<double>(overlay->audioDurationMs), 0.0, 1.0)) * (wf.right - wf.left);
                    rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rx, wf.top + 3.0f), 3.0f, 3.0f), brush);
                }
                const float cx = wf.left + static_cast<FLOAT>(progress) * (wf.right - wf.left);
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawLine(D2D1::Point2F(cx, wf.top), D2D1::Point2F(cx, wf.bottom), brush, 1.8f);
            }
        }

        // 8. Document Lock Indicator (AP-COL-076)
        if (overlay->noteLocked && widthPx >= 320) {
            const float bw = 228.0f;
            const float bx0 = (static_cast<float>(widthPx) - bw) * 0.5f;
            const float topOff = static_cast<float>((std::max)(0, overlay->topChromeClearancePx));
            const D2D1_RECT_F lr = D2D1::RectF(bx0, topOff + 12.0f, bx0 + bw, topOff + 36.0f);
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(120, 53, 15, 232) : DocColor::RGBA(254, 243, 199, 242)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(lr, 10.0f, 10.0f), brush);
            brush->SetColor(ToD2DColor(DocColor::RGBA(217, 119, 6, 230)));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(lr, 10.0f, 10.0f), brush, 1.0f);
            // Vector padlock icon (zero emojis)
            const D2D1_RECT_F lockBody = D2D1::RectF(lr.left + 12.0f, lr.top + 11.0f, lr.left + 22.0f, lr.top + 19.0f);
            rt->FillRoundedRectangle(D2D1::RoundedRect(lockBody, 1.5f, 1.5f), brush);
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(lr.left + 17.0f, lr.top + 10.0f), 3.2f, 3.2f), brush, 1.3f);
            auto fmtLk = makeTextFmt(10.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            if (fmtLk) {
                const std::wstring wLk = L"Nota bloqueada (solo lectura)";
                brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(254, 243, 199, 255) : DocColor::RGBA(146, 64, 14, 255)));
                rt->DrawTextW(wLk.c_str(), static_cast<UINT32>(wLk.size()), fmtLk.Get(),
                    D2D1::RectF(lr.left + 28.0f, lr.top + 5.0f, lr.right - 8.0f, lr.bottom - 3.0f), brush);
            }
        }

        // 9. Hover Link Preview Card (OB-LINK-045)
        if (overlay->hasHoverPreview && !overlay->hoverPreviewTitleUtf8.empty() && widthPx >= 320 && heightPx >= 200) {
            const float cw = 290.0f;
            const float ch = 104.0f;
            float px = static_cast<float>(overlay->hoverPreviewScreenPt.x) + 14.0f;
            float py = static_cast<float>(overlay->hoverPreviewScreenPt.y) + 14.0f;
            if (px + cw > static_cast<float>(widthPx) - 12.0f) px = static_cast<float>(widthPx) - cw - 12.0f;
            if (py + ch > static_cast<float>(heightPx) - 12.0f) py = static_cast<float>(overlay->hoverPreviewScreenPt.y) - ch - 12.0f;
            px = (std::max)(12.0f, px);
            py = (std::max)(12.0f, py);
            const D2D1_RECT_F pr = D2D1::RectF(px, py, px + cw, py + ch);
            brush->SetColor(ToD2DColor(isDark ? DocColor::RGBA(0, 0, 0, 96) : DocColor::RGBA(15, 23, 42, 28)));
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(pr.left, pr.top + 3.0f, pr.right, pr.bottom + 3.0f), 10.0f, 10.0f), brush);
            brush->SetColor(ToD2DColor(ovBg));
            rt->FillRoundedRectangle(D2D1::RoundedRect(pr, 10.0f, 10.0f), brush);
            brush->SetColor(ToD2DColor(ovAccent));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(pr, 10.0f, 10.0f), brush, 1.1f);
            auto fmtPt = makeTextFmt(11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            auto fmtPb = makeTextFmt(10.0f, DWRITE_FONT_WEIGHT_NORMAL);
            if (fmtPt) {
                const std::wstring wt = Utf8ToWide(overlay->hoverPreviewTitleUtf8);
                brush->SetColor(ToD2DColor(ovAccent));
                rt->DrawTextW(wt.c_str(), static_cast<UINT32>(wt.size()), fmtPt.Get(),
                    D2D1::RectF(pr.left + 12.0f, pr.top + 8.0f, pr.right - 12.0f, pr.top + 28.0f), brush);
            }
            if (fmtPb) {
                const std::wstring wb = Utf8ToWide(overlay->hoverPreviewExcerptUtf8);
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawTextW(wb.c_str(), static_cast<UINT32>(wb.size()), fmtPb.Get(),
                    D2D1::RectF(pr.left + 12.0f, pr.top + 30.0f, pr.right - 12.0f, pr.bottom - 8.0f), brush);
            }
        }

        // 10. Focused Reading Ruler Overlay (Universal Accessibility)
        if (overlay->readingRulerEnabled && widthPx >= 160 && heightPx >= 120) {
            const float bandH = static_cast<float>(std::clamp(overlay->readingRulerHeightPx, 40, 180));
            const float cy = std::clamp(static_cast<float>(overlay->readingRulerCursorY), bandH * 0.5f, static_cast<float>(heightPx) - bandH * 0.5f);
            const float yTop = (std::max)(0.0f, cy - bandH * 0.5f);
            const float yBot = (std::min)(static_cast<float>(heightPx), cy + bandH * 0.5f);
            const std::uint8_t maskAlpha = static_cast<std::uint8_t>(std::clamp(std::lround(overlay->readingRulerMaskOpacity * 255.0), 40L, 210L));
            const DocColor maskCol = isDark ? DocColor::RGBA(6, 9, 14, maskAlpha) : DocColor::RGBA(15, 23, 42, maskAlpha);
            brush->SetColor(ToD2DColor(maskCol));
            if (yTop > 0.0f) {
                rt->FillRectangle(D2D1::RectF(0.0f, 0.0f, static_cast<FLOAT>(widthPx), yTop), brush);
            }
            if (yBot < static_cast<float>(heightPx)) {
                rt->FillRectangle(D2D1::RectF(0.0f, yBot, static_cast<FLOAT>(widthPx), static_cast<FLOAT>(heightPx)), brush);
            }
            brush->SetColor(ToD2DColor(DocColor::RGBA(ovAccent.r, ovAccent.g, ovAccent.b, 210)));
            rt->DrawLine(D2D1::Point2F(0.0f, yTop), D2D1::Point2F(static_cast<FLOAT>(widthPx), yTop), brush, 1.4f);
            rt->DrawLine(D2D1::Point2F(0.0f, yBot), D2D1::Point2F(static_cast<FLOAT>(widthPx), yBot), brush, 1.4f);
        }

        // 11. Smart Pomodoro Status Pill (Minimal guías de interfaz HUD)
        if (overlay->pomodoroVisible && !overlay->pomodoroBadgeUtf8.empty() && widthPx >= 280) {
            const float pw = 148.0f;
            const float ph = 28.0f;
            const float topOff = static_cast<float>((std::max)(0, overlay->topChromeClearancePx));
            const D2D1_RECT_F pr = D2D1::RectF(static_cast<float>(widthPx) - pw - 16.0f, topOff + 14.0f, static_cast<float>(widthPx) - 16.0f, topOff + 14.0f + ph);
            brush->SetColor(ToD2DColor(ovBg));
            rt->FillRoundedRectangle(D2D1::RoundedRect(pr, 14.0f, 14.0f), brush);
            const DocColor phaseCol = overlay->pomodoroBreakPhase ? DocColor::RGBA(16, 185, 129, 255) : ovAccent;
            brush->SetColor(ToD2DColor(phaseCol));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(pr, 14.0f, 14.0f), brush, 1.2f);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(pr.left + 14.0f, (pr.top + pr.bottom) * 0.5f), 4.2f, 4.2f), brush);
            auto fmtPom = makeTextFmt(11.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            if (fmtPom) {
                const std::wstring wp = Utf8ToWide(overlay->pomodoroBadgeUtf8);
                brush->SetColor(ToD2DColor(ovInk));
                rt->DrawTextW(wp.c_str(), static_cast<UINT32>(wp.size()), fmtPom.Get(),
                    D2D1::RectF(pr.left + 24.0f, pr.top + 6.0f, pr.right - 8.0f, pr.bottom - 4.0f), brush);
            }
        }

        // Composite live material translúcido 2.0 floating overlay (e.g. Appearance Popover) inside Direct2D
        if (overlay->floatingOverlayBgra && overlay->floatingOverlayBgra->Valid() &&
            overlay->floatingOverlayScreenRect.w > 0.0 && overlay->floatingOverlayScreenRect.h > 0.0) {
            ComPtr<ID2D1Bitmap> ovBmp;
            ovBmp.Attach(UploadBgra32(rt, *overlay->floatingOverlayBgra));
            if (ovBmp) {
                rt->DrawBitmap(ovBmp.Get(), ToD2DRect(overlay->floatingOverlayScreenRect));
            }
        }
    }
};

D2DRenderer::D2DRenderer() : impl_(new Impl()) {}

D2DRenderer::~D2DRenderer() {
    const bool uninit = impl_ && impl_->didCoInit;
    delete impl_;
    impl_ = nullptr;
    if (uninit) CoUninitialize();
}

bool D2DRenderer::InitializeFactories() {
    if (impl_->factoriesReady) return true;
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hrCo)) impl_->didCoInit = true;
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, impl_->d2dFactory.ReleaseAndGetAddressOf());
    if (FAILED(hr) || !impl_->d2dFactory) return false;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(impl_->dwFactory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) return false;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->wicFactory));
    if (FAILED(hr) || !impl_->wicFactory) return false;
    impl_->factoriesReady = true;
    return true;
}

void D2DRenderer::AttachHwnd(HWND hwnd, int widthPx, int heightPx) {
    impl_->hwnd = hwnd;
    if (!InitializeFactories() || !hwnd) return;
    if (impl_->hwndRt) return;

    const D2D1_SIZE_U sz = D2D1::SizeU(
        static_cast<UINT32>((std::max)(1, widthPx)),
        static_cast<UINT32>((std::max)(1, heightPx)));
    // Strict 0.0% GPU mode by default (D2D1_RENDER_TARGET_TYPE_SOFTWARE);
    // user can optionally enable D2D1_RENDER_TARGET_TYPE_DEFAULT in Settings.
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        impl_->hardwareAccelEnabled ? D2D1_RENDER_TARGET_TYPE_DEFAULT : D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f);
    impl_->d2dFactory->CreateHwndRenderTarget(
        rtProps,
        D2D1::HwndRenderTargetProperties(hwnd, sz, D2D1_PRESENT_OPTIONS_NONE),
        &impl_->hwndRt);
}

void D2DRenderer::SetHardwareAcceleration(bool enable) {
    if (!impl_ || impl_->hardwareAccelEnabled == enable) return;
    impl_->hardwareAccelEnabled = enable;
    DiscardDeviceResources();
}

bool D2DRenderer::IsHardwareAccelerationEnabled() const noexcept {
    return impl_ && impl_->hardwareAccelEnabled;
}

void D2DRenderer::TrimCaches(bool aggressive) {
    if (!impl_) return;
    if (aggressive) {
        impl_->pageGpuBitmaps.clear();
        impl_->assetGpuBitmaps.clear();
        impl_->gifCache.clear();
        impl_->gifCacheTotalBytes = 0;
    } else {
        while (impl_->pageGpuBitmaps.size() > 8u) impl_->pageGpuBitmaps.erase(impl_->pageGpuBitmaps.begin());
        while (impl_->assetGpuBitmaps.size() > 16u) impl_->assetGpuBitmaps.erase(impl_->assetGpuBitmaps.begin());
    }
}

bool D2DRenderer::LastFrameHadVisibleAnimatedGif() const noexcept {
    return impl_ && impl_->lastFrameHadVisibleAnimatedGif;
}

UINT D2DRenderer::LastFrameMinGifDelayMs() const noexcept {
    return impl_ ? std::clamp<UINT>(impl_->lastFrameMinGifDelayMs, 20u, 1000u) : 100u;
}

std::size_t D2DRenderer::CachedBitmapCount() const noexcept {
    if (!impl_) return 0u;
    std::size_t n = impl_->pageGpuBitmaps.size() + impl_->assetGpuBitmaps.size();
    for (const auto& g : impl_->gifCache) n += g.frames.size();
    return n;
}

bool D2DRenderer::InspectGifMetadataWic(
    const std::vector<std::uint8_t>& gifBytes,
    int& outFrameCount,
    std::vector<int>& outDelaysMs)
{
    outFrameCount = 0;
    outDelaysMs.clear();
    if (gifBytes.size() < 6 || std::memcmp(gifBytes.data(), "GIF8", 4) != 0) return false;
    ComPtr<IWICImagingFactory> wic;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))) || !wic) {
        return false;
    }
    ComPtr<IWICStream> stream;
    if (FAILED(wic->CreateStream(&stream)) || !stream) return false;
    if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(gifBytes.data()), static_cast<DWORD>(gifBytes.size())))) return false;
    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &dec)) || !dec) return false;
    UINT fc = 0;
    if (FAILED(dec->GetFrameCount(&fc)) || fc == 0) return false;
    outFrameCount = static_cast<int>(fc);
    for (UINT i = 0; i < fc; ++i) {
        int delayMs = 100;
        ComPtr<IWICBitmapFrameDecode> fr;
        if (SUCCEEDED(dec->GetFrame(i, &fr)) && fr) {
            ComPtr<IWICMetadataQueryReader> meta;
            if (SUCCEEDED(fr->GetMetadataQueryReader(&meta)) && meta) {
                PROPVARIANT pv{};
                PropVariantInit(&pv);
                if (SUCCEEDED(meta->GetMetadataByName(L"/grctlext/Delay", &pv))) {
                    if (pv.vt == VT_UI2 && pv.uiVal > 0) {
                        delayMs = (std::max)(20, static_cast<int>(pv.uiVal) * 10);
                    }
                }
                PropVariantClear(&pv);
            }
        }
        outDelaysMs.push_back(delayMs);
    }
    return true;
}

void D2DRenderer::Resize(int widthPx, int heightPx) {
    if (impl_->hwndRt) {
        impl_->hwndRt->Resize(D2D1::SizeU(
            static_cast<UINT32>((std::max)(1, widthPx)),
            static_cast<UINT32>((std::max)(1, heightPx))));
    }
}

void D2DRenderer::DiscardDeviceResources() {
    TrimCaches(true);
    impl_->hwndRt.Reset();
}

void D2DRenderer::RenderViewport(
    const DocumentModel& doc,
    const Camera2D& camera,
    int widthPx,
    int heightPx,
    PageRenderCache* pageCache,
    std::uint64_t asyncGen,
    std::function<void()> onAsyncPageReady,
    const ViewportOverlayState* overlay)
{
    if (!InitializeFactories()) return;
    if (!impl_->hwndRt && impl_->hwnd) AttachHwnd(impl_->hwnd, widthPx, heightPx);
    if (!impl_->hwndRt) return;

    impl_->hwndRt->BeginDraw();
    impl_->RenderSceneToTarget(impl_->hwndRt.Get(), doc, camera, widthPx, heightPx,
        pageCache, asyncGen, std::move(onAsyncPageReady), overlay, false);
    const HRESULT hr = impl_->hwndRt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

bool D2DRenderer::RenderPageOffscreen(
    const DocumentModel& doc,
    std::size_t pageIndex0,
    int widthPx,
    PageRenderCache* pageCache,
    RasterImageBgra32& outBgra)
{
    if (!InitializeFactories() || !impl_->wicFactory || pageIndex0 >= doc.Pages().size()) return false;
    const auto* place = doc.Layout().PlacementAt(pageIndex0);
    if (!place || place->widthWorld <= 0.0) return false;

    const int w = std::clamp(widthPx, 64, 3000);
    const double zoom = static_cast<double>(w) / place->widthWorld;
    const int h = (std::max)(64, static_cast<int>(std::lround(place->heightWorld * zoom)));

    Camera2D cam;
    cam.panX = -place->widthWorld * 0.5;
    cam.panY = place->topWorld;
    cam.zoom = zoom;

    return RenderHwndViewportToOffscreen(doc, cam, w, h, pageCache, outBgra, nullptr);
}

bool D2DRenderer::RenderInfiniteCanvasOffscreen(
    const DocumentModel& doc,
    const DocRect& worldRegion,
    int widthPx,
    int heightPx,
    RasterImageBgra32& outBgra)
{
    if (worldRegion.w <= 0.0 || worldRegion.h <= 0.0) return false;
    if (!InitializeFactories() || !impl_->wicFactory) return false;
    const int w = (std::max)(1, widthPx);

    Camera2D cam;
    cam.panX = worldRegion.x;
    cam.panY = worldRegion.y;
    cam.zoom = static_cast<double>(w) / worldRegion.w;

    ComPtr<IWICBitmap> wicBmp;
    if (FAILED(impl_->wicFactory->CreateBitmap(
            static_cast<UINT>(w), static_cast<UINT>((std::max)(1, heightPx)),
            GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wicBmp)) || !wicBmp) {
        return false;
    }

    const int h = (std::max)(1, heightPx);
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);
    ComPtr<ID2D1RenderTarget> wicRt;
    if (FAILED(impl_->d2dFactory->CreateWicBitmapRenderTarget(wicBmp.Get(), props, &wicRt)) || !wicRt) {
        return false;
    }

    wicRt->BeginDraw();
    impl_->RenderSceneToTarget(wicRt.Get(), doc, cam, w, h, nullptr, 1, nullptr, nullptr, true, true);
    if (FAILED(wicRt->EndDraw())) return false;

    outBgra.width = w;
    outBgra.height = h;
    outBgra.pixels.resize(static_cast<std::size_t>(w) * h * 4u);
    WICRect lockRect{ 0, 0, w, h };
    return SUCCEEDED(wicBmp->CopyPixels(&lockRect, static_cast<UINT>(w * 4), static_cast<UINT>(outBgra.pixels.size()), outBgra.pixels.data()));
}

bool D2DRenderer::RenderHwndViewportToOffscreen(
    const DocumentModel& doc,
    const Camera2D& camera,
    int widthPx,
    int heightPx,
    PageRenderCache* pageCache,
    RasterImageBgra32& outBgra,
    const ViewportOverlayState* overlay)
{
    if (!InitializeFactories() || !impl_->wicFactory) return false;
    const int w = (std::max)(1, widthPx);
    const int h = (std::max)(1, heightPx);

    ComPtr<IWICBitmap> wicBmp;
    if (FAILED(impl_->wicFactory->CreateBitmap(
            static_cast<UINT>(w), static_cast<UINT>(h),
            GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wicBmp)) || !wicBmp) {
        return false;
    }

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);
    ComPtr<ID2D1RenderTarget> wicRt;
    if (FAILED(impl_->d2dFactory->CreateWicBitmapRenderTarget(wicBmp.Get(), props, &wicRt)) || !wicRt) {
        return false;
    }

    wicRt->BeginDraw();
    impl_->RenderSceneToTarget(wicRt.Get(), doc, camera, w, h, pageCache, 1, nullptr, overlay, true);
    if (FAILED(wicRt->EndDraw())) return false;

    outBgra.width = w;
    outBgra.height = h;
    outBgra.pixels.resize(static_cast<std::size_t>(w) * h * 4u);
    WICRect lockRect{ 0, 0, w, h };
    return SUCCEEDED(wicBmp->CopyPixels(&lockRect, static_cast<UINT>(w * 4), static_cast<UINT>(outBgra.pixels.size()), outBgra.pixels.data()));
}

bool D2DRenderer::HitTestTextBoxInlineUrl(
    const AnnotationObject& obj,
    double localX,
    double localY,
    double zoom,
    bool dyslexiaMode,
    InlineUrlSpan& outSpan) const
{
    if (obj.kind != ObjectKind::TextBox || obj.textUtf8.empty()) return false;
    if (MathTypeset::TextContainsMathMarkup(obj.textUtf8)) return false;
    const std::vector<InlineUrlSpan> urls = ExtractSafeInlineUrls(obj.textUtf8);
    if (urls.empty()) return false;

    double pxLocal = localX;
    double pyLocal = localY;
    if (std::abs(obj.rotationDeg) > 0.15) {
        const double cx = obj.bounds.x + obj.bounds.w * 0.5;
        const double cy = obj.bounds.y + obj.bounds.h * 0.5;
        const double rad = -obj.rotationDeg * 3.14159265358979323846 / 180.0;
        const double dx = localX - cx;
        const double dy = localY - cy;
        pxLocal = cx + dx * std::cos(rad) - dy * std::sin(rad);
        pyLocal = cy + dx * std::sin(rad) + dy * std::cos(rad);
    }
    if (pxLocal < obj.bounds.x || pyLocal < obj.bounds.y ||
        pxLocal > obj.bounds.Right() || pyLocal > obj.bounds.Bottom()) {
        return false;
    }

    if (!impl_ || !impl_->dwFactory) {
        const_cast<D2DRenderer*>(this)->InitializeFactories();
    }
    if (!impl_ || !impl_->dwFactory) return false;

    const double effZoom = (std::max)(0.2, zoom);
    const float fontPx = (std::max)(6.0f, static_cast<float>(obj.fontSizePt * effZoom));
    const float effFontPx = dyslexiaMode ? (fontPx * 1.12f) : fontPx;
    const std::wstring wfam = dyslexiaMode
        ? L"Verdana"
        : Utf8ToWide(obj.fontFamilyUtf8.empty() ? "Segoe UI" : obj.fontFamilyUtf8);

    ComPtr<IDWriteTextFormat> fmt;
    if (FAILED(impl_->dwFactory->CreateTextFormat(
            wfam.c_str(), nullptr,
            obj.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
            obj.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, effFontPx, L"es-ES", &fmt)) || !fmt) {
        return false;
    }
    if (dyslexiaMode) {
        fmt->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, effFontPx * 1.48f, effFontPx * 1.15f);
        fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    const std::wstring wtxt = Utf8ToWide(obj.textUtf8);
    const float boxW = (std::max)(4.0f, static_cast<float>(obj.bounds.w * effZoom));
    const float boxH = (std::max)(4.0f, static_cast<float>(obj.bounds.h * effZoom));
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(impl_->dwFactory->CreateTextLayout(
            wtxt.c_str(), static_cast<UINT32>(wtxt.size()), fmt.Get(), boxW, boxH, &layout)) || !layout) {
        return false;
    }

    const float relX = static_cast<float>((pxLocal - obj.bounds.x) * effZoom);
    const float relY = static_cast<float>((pyLocal - obj.bounds.y) * effZoom);
    BOOL isTrailing = FALSE;
    BOOL isInside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    if (FAILED(layout->HitTestPoint(relX, relY, &isTrailing, &isInside, &metrics)) || !isInside) {
        return false;
    }

    const std::size_t hitPos = static_cast<std::size_t>(metrics.textPosition);
    for (const auto& u : urls) {
        if (hitPos >= u.utf16Start && hitPos < (u.utf16Start + u.utf16Len)) {
            outSpan = u;
            return true;
        }
    }
    return false;
}

} // namespace DocStudio
