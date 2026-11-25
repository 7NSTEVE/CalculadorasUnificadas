#include "render/d2d_renderer.hpp"
#include "render/card_render_common.hpp"
#include "render/math_box_typesetter.hpp"
#include "core/ink_engine.hpp"
#include "core/utf_conv.hpp"

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace DocStudio {

namespace {

int QuantizeBucketPx(int px) noexcept {
    if (px <= 160) return 160;
    if (px <= 320) return 320;
    if (px <= 640) return 640;
    if (px <= 960) return 960;
    if (px <= 1400) return 1400;
    if (px <= 2000) return 2000;
    return 2600;
}

void CairoSurfaceDeleter(cairo_surface_t* s) noexcept {
    if (s) cairo_surface_destroy(s);
}

using CairoSurfacePtr = std::shared_ptr<cairo_surface_t>;

void SetCairoColor(cairo_t* cr, DocColor c) noexcept {
    cairo_set_source_rgba(
        cr,
        static_cast<double>(c.r) / 255.0,
        static_cast<double>(c.g) / 255.0,
        static_cast<double>(c.b) / 255.0,
        static_cast<double>(c.a) / 255.0);
}

CairoSurfacePtr UploadBgra32ToCairoSurface(const RasterImageBgra32& img) {
    if (!img.Valid()) return nullptr;
    cairo_surface_t* raw = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, img.width, img.height);
    if (!raw || cairo_surface_status(raw) != CAIRO_STATUS_SUCCESS) {
        if (raw) cairo_surface_destroy(raw);
        return nullptr;
    }
    cairo_surface_flush(raw);
    unsigned char* dst = cairo_image_surface_get_data(raw);
    const int dstStride = cairo_image_surface_get_stride(raw);
    const std::size_t rowBytes = static_cast<std::size_t>(img.width) * 4u;
    if (!dst || dstStride < static_cast<int>(rowBytes)) {
        cairo_surface_destroy(raw);
        return nullptr;
    }
    for (int y = 0; y < img.height; ++y) {
        std::memcpy(
            dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(dstStride),
            img.pixels.data() + static_cast<std::size_t>(y) * rowBytes,
            rowBytes);
    }
    cairo_surface_mark_dirty(raw);
    return CairoSurfacePtr(raw, CairoSurfaceDeleter);
}

struct PngMemReaderState {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    std::size_t pos = 0;
};

cairo_status_t ReadPngFromMemory(void* closure, unsigned char* data, unsigned int length) {
    auto* st = static_cast<PngMemReaderState*>(closure);
    if (!st || st->pos + length > st->size) return CAIRO_STATUS_READ_ERROR;
    std::memcpy(data, st->data + st->pos, length);
    st->pos += length;
    return CAIRO_STATUS_SUCCESS;
}

CairoSurfacePtr DecodeEncodedAssetToCairoSurface(const std::vector<std::uint8_t>& bytes, std::size_t& outByteSize) {
    outByteSize = 0;
    if (bytes.empty()) return nullptr;

    // Fast path for PNG bytes via native Cairo PNG stream decoder
    if (bytes.size() >= 8 &&
        bytes[0] == 0x89u && bytes[1] == 0x50u && bytes[2] == 0x4Eu && bytes[3] == 0x47u &&
        bytes[4] == 0x0Du && bytes[5] == 0x0Au && bytes[6] == 0x1Au && bytes[7] == 0x0Au) {
        PngMemReaderState st{ bytes.data(), bytes.size(), 0 };
        cairo_surface_t* pngSurf = cairo_image_surface_create_from_png_stream(ReadPngFromMemory, &st);
        if (pngSurf && cairo_surface_status(pngSurf) == CAIRO_STATUS_SUCCESS) {
            const int w = cairo_image_surface_get_width(pngSurf);
            const int h = cairo_image_surface_get_height(pngSurf);
            if (w > 0 && h > 0 && w <= 8192 && h <= 8192) {
                outByteSize = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
                return CairoSurfacePtr(pngSurf, CairoSurfaceDeleter);
            }
        }
        if (pngSurf) cairo_surface_destroy(pngSurf);
    }

    // Fallback for JPEG/BMP/WebP/etc. via GdkPixbufLoader -> premultiplied BGRA8 CAIRO_FORMAT_ARGB32
    GError* gerr = nullptr;
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
    if (!loader) return nullptr;

    if (!gdk_pixbuf_loader_write(
            loader,
            reinterpret_cast<const guchar*>(bytes.data()),
            static_cast<gsize>(bytes.size()),
            &gerr)) {
        if (gerr) g_error_free(gerr);
        g_object_unref(loader);
        return nullptr;
    }
    if (!gdk_pixbuf_loader_close(loader, &gerr)) {
        if (gerr) g_error_free(gerr);
        g_object_unref(loader);
        return nullptr;
    }

    GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (!pixbuf) {
        g_object_unref(loader);
        return nullptr;
    }
    g_object_ref(pixbuf);
    g_object_unref(loader);

    const int w = gdk_pixbuf_get_width(pixbuf);
    const int h = gdk_pixbuf_get_height(pixbuf);
    const int nCh = gdk_pixbuf_get_n_channels(pixbuf);
    const int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    const gboolean hasAlpha = gdk_pixbuf_get_has_alpha(pixbuf);
    const guint8* src = gdk_pixbuf_get_pixels(pixbuf);

    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (nCh != 3 && nCh != 4) || !src) {
        g_object_unref(pixbuf);
        return nullptr;
    }

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (!surf || cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        if (surf) cairo_surface_destroy(surf);
        g_object_unref(pixbuf);
        return nullptr;
    }

    cairo_surface_flush(surf);
    unsigned char* dstData = cairo_image_surface_get_data(surf);
    const int dstStride = cairo_image_surface_get_stride(surf);

    for (int y = 0; y < h; ++y) {
        const guint8* srcRow = src + static_cast<std::size_t>(y) * static_cast<std::size_t>(rowstride);
        std::uint8_t* dstRow = dstData + static_cast<std::size_t>(y) * static_cast<std::size_t>(dstStride);
        for (int x = 0; x < w; ++x) {
            const guint8* sp = srcRow + static_cast<std::size_t>(x) * static_cast<std::size_t>(nCh);
            const std::uint8_t r = sp[0];
            const std::uint8_t g = sp[1];
            const std::uint8_t b = sp[2];
            const std::uint8_t a = (hasAlpha && nCh == 4) ? sp[3] : 255u;
            std::uint8_t pr = r, pg = g, pb = b;
            if (a == 0) {
                pr = pg = pb = 0;
            } else if (a < 255) {
                pr = static_cast<std::uint8_t>((static_cast<unsigned>(r) * a + 127u) / 255u);
                pg = static_cast<std::uint8_t>((static_cast<unsigned>(g) * a + 127u) / 255u);
                pb = static_cast<std::uint8_t>((static_cast<unsigned>(b) * a + 127u) / 255u);
            }
            std::uint8_t* dp = dstRow + static_cast<std::size_t>(x) * 4u;
            dp[0] = pb; // Little-endian ARGB32 = [B, G, R, A]
            dp[1] = pg;
            dp[2] = pr;
            dp[3] = a;
        }
    }
    cairo_surface_mark_dirty(surf);
    g_object_unref(pixbuf);

    outByteSize = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    return CairoSurfacePtr(surf, CairoSurfaceDeleter);
}

void DrawSurfaceStretched(cairo_t* cr, cairo_surface_t* surf, const DocRect& dstRect) {
    if (!surf || dstRect.w <= 0.0 || dstRect.h <= 0.0) return;
    const int sw = cairo_image_surface_get_width(surf);
    const int sh = cairo_image_surface_get_height(surf);
    if (sw <= 0 || sh <= 0) return;

    cairo_save(cr);
    cairo_translate(cr, dstRect.x, dstRect.y);
    cairo_scale(cr, dstRect.w / static_cast<double>(sw), dstRect.h / static_cast<double>(sh));
    cairo_set_source_surface(cr, surf, 0.0, 0.0);
    if (cairo_pattern_t* pat = cairo_get_source(cr)) {
        cairo_pattern_set_filter(pat, CAIRO_FILTER_GOOD);
        cairo_pattern_set_extend(pat, CAIRO_EXTEND_PAD);
    }
    cairo_rectangle(cr, 0.0, 0.0, static_cast<double>(sw), static_cast<double>(sh));
    cairo_fill(cr);
    cairo_restore(cr);
}

void DrawPangoTextInRect(
    cairo_t* cr,
    const std::string& textUtf8,
    const std::string& familyUtf8,
    double fontPx,
    bool bold,
    bool semibold,
    bool italic,
    DocColor color,
    const DocRect& rect)
{
    if (textUtf8.empty() || rect.w <= 1.0 || rect.h <= 1.0) return;

    PangoLayout* layout = pango_cairo_create_layout(cr);
    if (!layout) return;

    std::string famList = familyUtf8.empty() ? std::string("DejaVu Sans, Noto Sans, Sans")
                                             : (familyUtf8 + ", DejaVu Sans, Noto Sans, Sans");
    PangoFontDescription* desc = pango_font_description_new();
    pango_font_description_set_family(desc, famList.c_str());
    pango_font_description_set_weight(
        desc,
        bold ? PANGO_WEIGHT_BOLD : (semibold ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_NORMAL));
    pango_font_description_set_style(desc, italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
    pango_font_description_set_absolute_size(desc, std::max(6.0, fontPx) * PANGO_SCALE);

    pango_layout_set_font_description(layout, desc);
    pango_layout_set_width(layout, static_cast<int>(std::max(1.0, rect.w) * PANGO_SCALE));
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_text(layout, textUtf8.data(), static_cast<int>(textUtf8.size()));

    cairo_save(cr);
    cairo_rectangle(cr, rect.x, rect.y, rect.w, rect.h);
    cairo_clip(cr);
    SetCairoColor(cr, color);
    cairo_move_to(cr, rect.x, rect.y);
    pango_cairo_show_layout(cr, layout);
    cairo_restore(cr);

    pango_font_description_free(desc);
    g_object_unref(layout);
}

void CairoRoundedRectPath(cairo_t* cr, double x, double y, double w, double h, double r) {
    const double rad = std::clamp(r, 0.0, 0.5 * (std::min)(w, h));
    constexpr double kHalfPi = 1.57079632679489661923;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - rad, y + rad, rad, -kHalfPi, 0.0);
    cairo_arc(cr, x + w - rad, y + h - rad, rad, 0.0, kHalfPi);
    cairo_arc(cr, x + rad, y + h - rad, rad, kHalfPi, 2.0 * kHalfPi);
    cairo_arc(cr, x + rad, y + rad, rad, 2.0 * kHalfPi, 3.0 * kHalfPi);
    cairo_close_path(cr);
}

class CairoMathBackend final : public MathTypeset::IMathDrawBackend {
public:
    explicit CairoMathBackend(cairo_t* cr) : cr_(cr) {}

    MathTypeset::TextMetrics2D MeasureText(
        const std::string& utf8,
        double fontSizePx,
        MathTypeset::FontStyleRole role) override
    {
        MathTypeset::TextMetrics2D out{};
        if (!cr_ || utf8.empty()) {
            out.ascent = fontSizePx * 0.78;
            out.descent = fontSizePx * 0.22;
            return out;
        }
        PangoLayout* layout = CreateLayout(utf8, fontSizePx, role);
        if (!layout) {
            out.width = fontSizePx * 0.52 * static_cast<double>(utf8.size());
            out.ascent = fontSizePx * 0.78;
            out.descent = fontSizePx * 0.22;
            return out;
        }
        int pw = 0, ph = 0;
        pango_layout_get_pixel_size(layout, &pw, &ph);
        const double base = static_cast<double>(pango_layout_get_baseline(layout)) / static_cast<double>(PANGO_SCALE);
        out.width = static_cast<double>(pw);
        out.ascent = (base > 1.0) ? base : (fontSizePx * 0.78);
        out.descent = (std::max)(fontSizePx * 0.18, static_cast<double>(ph) - out.ascent);
        g_object_unref(layout);
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
        if (!cr_ || utf8.empty()) return;
        PangoLayout* layout = CreateLayout(utf8, fontSizePx, role);
        if (!layout) return;
        const double base = static_cast<double>(pango_layout_get_baseline(layout)) / static_cast<double>(PANGO_SCALE);
        cairo_save(cr_);
        SetCairoColor(cr_, color);
        cairo_move_to(cr_, x, baselineY - ((base > 1.0) ? base : fontSizePx * 0.78));
        pango_cairo_show_layout(cr_, layout);
        cairo_restore(cr_);
        g_object_unref(layout);
    }

    void DrawLine(
        double x0, double y0,
        double x1, double y1,
        double thickness,
        DocColor color) override
    {
        if (!cr_) return;
        cairo_save(cr_);
        SetCairoColor(cr_, color);
        cairo_set_line_width(cr_, (std::max)(0.6, thickness));
        cairo_set_line_cap(cr_, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr_, x0, y0);
        cairo_line_to(cr_, x1, y1);
        cairo_stroke(cr_);
        cairo_restore(cr_);
    }

    void DrawRoundedPill(
        double x, double y, double w, double h,
        double radius,
        DocColor fill,
        DocColor stroke,
        double strokeWidth) override
    {
        if (!cr_) return;
        cairo_save(cr_);
        CairoRoundedRectPath(cr_, x, y, w, h, radius);
        if (fill.a > 0) {
            SetCairoColor(cr_, fill);
            if (stroke.a > 0 && strokeWidth > 0.0) cairo_fill_preserve(cr_);
            else cairo_fill(cr_);
        }
        if (stroke.a > 0 && strokeWidth > 0.0) {
            SetCairoColor(cr_, stroke);
            cairo_set_line_width(cr_, strokeWidth);
            cairo_stroke(cr_);
        }
        cairo_restore(cr_);
    }

private:
    PangoLayout* CreateLayout(
        const std::string& utf8,
        double fontSizePx,
        MathTypeset::FontStyleRole role)
    {
        PangoLayout* layout = pango_cairo_create_layout(cr_);
        if (!layout) return nullptr;
        const char* family = "Inter, Noto Sans, DejaVu Sans, Sans";
        PangoWeight weight = PANGO_WEIGHT_NORMAL;
        PangoStyle style = PANGO_STYLE_NORMAL;
        switch (role) {
        case MathTypeset::FontStyleRole::UiRegular:
            family = "Inter, Noto Sans, DejaVu Sans, Sans";
            break;
        case MathTypeset::FontStyleRole::UiSemiBold:
            family = "Inter, Noto Sans, DejaVu Sans, Sans";
            weight = PANGO_WEIGHT_SEMIBOLD;
            break;
        case MathTypeset::FontStyleRole::MathItalic:
            family = "STIX Two Math, DejaVu Serif, Liberation Serif, Noto Serif, Serif";
            style = PANGO_STYLE_ITALIC;
            break;
        case MathTypeset::FontStyleRole::MathRoman:
            family = "STIX Two Math, DejaVu Serif, Liberation Serif, Noto Serif, Serif";
            break;
        case MathTypeset::FontStyleRole::MathBold:
            family = "STIX Two Math, DejaVu Serif, Liberation Serif, Noto Serif, Serif";
            weight = PANGO_WEIGHT_BOLD;
            break;
        case MathTypeset::FontStyleRole::MathSymbol:
            family = "STIX Two Math, DejaVu Serif, Noto Sans Math, DejaVu Sans, Serif";
            break;
        case MathTypeset::FontStyleRole::KeycapLabel:
            family = "Inter, Noto Sans, DejaVu Sans, Sans";
            weight = PANGO_WEIGHT_SEMIBOLD;
            break;
        }
        PangoFontDescription* desc = pango_font_description_new();
        pango_font_description_set_family(desc, family);
        pango_font_description_set_weight(desc, weight);
        pango_font_description_set_style(desc, style);
        pango_font_description_set_absolute_size(desc, (std::max)(5.5, fontSizePx) * PANGO_SCALE);
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_text(layout, utf8.data(), static_cast<int>(utf8.size()));
        pango_font_description_free(desc);
        return layout;
    }

    cairo_t* cr_ = nullptr;
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
        if (stop_.load(std::memory_order_acquire)) return;
        stop_.store(true, std::memory_order_release);
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
        if (stop_.load(std::memory_order_acquire)) return;
        auto it = inFlight_.find(key);
        if (it != inFlight_.end() && it->second >= generation) return;
        while (queue_.size() >= 64) {
            PendingJob dropped = std::move(queue_.front());
            queue_.pop_front();
            auto dit = inFlight_.find(dropped.key);
            if (dit != inFlight_.end() && dit->second <= dropped.generation) {
                inFlight_.erase(dit);
            }
        }
        inFlight_[key] = generation;
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
    while (true) {
        PendingJob job;
        {
            std::unique_lock<std::mutex> lk(queueMtx_);
            cv_.wait(lk, [&]() { return stop_.load(std::memory_order_acquire) || !queue_.empty(); });
            if (stop_.load(std::memory_order_acquire)) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }

        if (job.generation < activeGen_.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lk(queueMtx_);
            auto it = inFlight_.find(job.key);
            if (it != inFlight_.end() && it->second <= job.generation) inFlight_.erase(it);
            continue;
        }

        RasterImageBgra32 existing;
        if (cache_.TryGet(job.key, existing)) {
            std::lock_guard<std::mutex> lk(queueMtx_);
            auto it = inFlight_.find(job.key);
            if (it != inFlight_.end() && it->second <= job.generation) inFlight_.erase(it);
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

        const bool stillFresh = ok && !stop_.load(std::memory_order_acquire) &&
            job.generation >= activeGen_.load(std::memory_order_acquire);
        if (stillFresh) {
            const std::size_t bytes = rendered.ByteSize();
            cache_.Put(job.key, std::move(rendered), bytes);
        }

        {
            std::lock_guard<std::mutex> lk(queueMtx_);
            auto it = inFlight_.find(job.key);
            if (it != inFlight_.end() && it->second <= job.generation) inFlight_.erase(it);
        }

        if (stillFresh && job.onReady) {
            job.onReady();
        }
    }
}

// ---------------- D2DRenderer ----------------

struct D2DRenderer::Impl {
    bool initialized = false;
    int widthPx = 0;
    int heightPx = 0;

    // Byte-budgeted LRU surface caches holding shared_ptr to prevent UAF on eviction or discard
    LruByteCache<PageCacheKey, CairoSurfacePtr, PageCacheKeyHash> pageSurfaceCache{ 64ull * 1024ull * 1024ull };
    LruByteCache<std::string, CairoSurfacePtr> assetSurfaceCache{ 32ull * 1024ull * 1024ull };

    CairoSurfacePtr AcquireAssetSurface(
        const std::string& tag,
        const std::vector<std::uint8_t>& encodedBytes,
        bool cacheSurface)
    {
        if (cacheSurface) {
            CairoSurfacePtr hit;
            if (assetSurfaceCache.TryGet(tag, hit) && hit) return hit;
        }
        std::size_t byteSize = 0;
        CairoSurfacePtr decoded = DecodeEncodedAssetToCairoSurface(encodedBytes, byteSize);
        if (decoded && cacheSurface && byteSize > 0) {
            assetSurfaceCache.Put(tag, decoded, byteSize);
        }
        return decoded;
    }

    void DrawPaperPattern(
        cairo_t* cr,
        const DocRect& worldRegion,
        PaperPattern pattern,
        const Camera2D& cam)
    {
        if (pattern == PaperPattern::Plain) return;

        auto drawGridLines = [&](double stepWorld, DocColor col, double pxThickness) {
            const double screenStep = stepWorld * cam.zoom;
            if (screenStep < 4.0) return;
            SetCairoColor(cr, col);
            cairo_set_line_width(cr, pxThickness);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
            const double startX = std::floor(worldRegion.x / stepWorld) * stepWorld;
            const double endX = worldRegion.Right();
            for (double x = startX; x <= endX; x += stepWorld) {
                const DocPoint p0 = cam.WorldToScreen(x, worldRegion.y);
                const DocPoint p1 = cam.WorldToScreen(x, worldRegion.Bottom());
                cairo_move_to(cr, p0.x, p0.y);
                cairo_line_to(cr, p1.x, p1.y);
            }
            const double startY = std::floor(worldRegion.y / stepWorld) * stepWorld;
            const double endY = worldRegion.Bottom();
            for (double y = startY; y <= endY; y += stepWorld) {
                const DocPoint p0 = cam.WorldToScreen(worldRegion.x, y);
                const DocPoint p1 = cam.WorldToScreen(worldRegion.Right(), y);
                cairo_move_to(cr, p0.x, p0.y);
                cairo_line_to(cr, p1.x, p1.y);
            }
            cairo_stroke(cr);
        };

        if (pattern == PaperPattern::Ruled) {
            const double step = 24.0;
            if (step * cam.zoom >= 4.0) {
                SetCairoColor(cr, DocColor::RGBA(205, 221, 244, 255));
                cairo_set_line_width(cr, 1.0);
                const double startY = std::floor(worldRegion.y / step) * step;
                for (double y = startY; y <= worldRegion.Bottom(); y += step) {
                    const DocPoint p0 = cam.WorldToScreen(worldRegion.x, y);
                    const DocPoint p1 = cam.WorldToScreen(worldRegion.Right(), y);
                    cairo_move_to(cr, p0.x, p0.y);
                    cairo_line_to(cr, p1.x, p1.y);
                }
                cairo_stroke(cr);

                const double marginX = worldRegion.x + 56.0;
                const DocPoint m0 = cam.WorldToScreen(marginX, worldRegion.y);
                const DocPoint m1 = cam.WorldToScreen(marginX, worldRegion.Bottom());
                SetCairoColor(cr, DocColor::RGBA(240, 150, 155, 255));
                cairo_set_line_width(cr, 1.2);
                cairo_move_to(cr, m0.x, m0.y);
                cairo_line_to(cr, m1.x, m1.y);
                cairo_stroke(cr);
            }
        } else if (pattern == PaperPattern::Grid) {
            drawGridLines(20.0, DocColor::RGBA(218, 228, 240, 255), 1.0);
        } else if (pattern == PaperPattern::Dots) {
            const double step = 20.0;
            if (step * cam.zoom >= 6.0) {
                SetCairoColor(cr, DocColor::RGBA(175, 186, 204, 255));
                const double r = (std::max)(1.0, 1.2 * cam.zoom);
                const double startX = std::floor(worldRegion.x / step) * step;
                const double startY = std::floor(worldRegion.y / step) * step;
                constexpr double kTwoPi = 6.28318530717958647692;
                for (double y = startY; y <= worldRegion.Bottom(); y += step) {
                    for (double x = startX; x <= worldRegion.Right(); x += step) {
                        const DocPoint sp = cam.WorldToScreen(x, y);
                        cairo_new_sub_path(cr);
                        cairo_arc(cr, sp.x, sp.y, r, 0.0, kTwoPi);
                    }
                }
                cairo_fill(cr);
            }
        } else if (pattern == PaperPattern::Millimeter) {
            drawGridLines(8.0, DocColor::RGBA(228, 240, 232, 255), 0.7);
            drawGridLines(40.0, DocColor::RGBA(178, 214, 190, 255), 1.1);
        }
    }

    void DrawAnnotationObject(
        cairo_t* cr,
        const DocumentModel& doc,
        const AnnotationObject& obj,
        const Camera2D& cam,
        double originWorldX,
        double originWorldY,
        bool isSelected,
        bool cacheSurfaces,
        const ViewportOverlayState* overlay = nullptr)
    {
        double layerOpacity = 1.0;
        if (!RenderCommon::ShouldDrawObjectOnLayer(doc, obj, layerOpacity, overlay)) return;

        auto toScreen = [&](double lx, double ly) -> DocPoint {
            return cam.WorldToScreen(originWorldX + lx, originWorldY + ly);
        };
        auto rectToScreen = [&](const DocRect& lr) -> DocRect {
            return cam.WorldRectToScreen({ originWorldX + lr.x, originWorldY + lr.y, lr.w, lr.h });
        };

        if (obj.kind == ObjectKind::InkStroke) {
            const std::vector<InkPoint> filteredPts = (overlay && overlay->replayActive)
                ? InkEngine::FilterInkPointsAtReplayTime(obj, overlay->replayCursorMs)
                : obj.inkPoints;
            if (filteredPts.empty()) return;

            DocColor baseCol = RenderCommon::ApplyLayerOpacity(obj.strokeColor, layerOpacity);
            if ((obj.isHighlighter || obj.penStyle == InkPenStyle::Highlighter) && baseCol.a > 140) {
                baseCol.a = 98;
            }
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

            if (filteredPts.size() == 1) {
                SetCairoColor(cr, baseCol);
                const DocPoint pt = toScreen(filteredPts[0].x, filteredPts[0].y);
                const double r = (std::max)(1.0, obj.strokeWidth * 0.5 * cam.zoom);
                cairo_arc(cr, pt.x, pt.y, r, 0.0, 6.28318530717958647692);
                cairo_fill(cr);
            } else {
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
                    const double tiltAlpha = InkEngine::ComputeTiltAlphaMultiplier(mid, obj.penStyle);
                    DocColor segCol = baseCol;
                    segCol.a = static_cast<std::uint8_t>(std::clamp(std::lround(baseCol.a * tiltAlpha), 1L, 255L));
                    SetCairoColor(cr, segCol);
                    const double wPx = (std::max)(0.8, widthWorld * cam.zoom);
                    const DocPoint s0 = toScreen(p0.x, p0.y);
                    const DocPoint s1 = toScreen(p1.x, p1.y);
                    cairo_set_line_width(cr, wPx);
                    cairo_move_to(cr, s0.x, s0.y);
                    cairo_line_to(cr, s1.x, s1.y);
                    cairo_stroke(cr);
                }
            }
        } else if (obj.kind == ObjectKind::Shape) {
            const double wPx = (std::max)(1.0, obj.strokeWidth * cam.zoom);
            const DocRect sr = rectToScreen(obj.bounds);
            if (obj.shapeKind == ShapeKind::Rectangle || obj.shapeKind == ShapeKind::SignatureStamp) {
                if (obj.fillColor.a > 0) {
                    SetCairoColor(cr, obj.fillColor);
                    cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
                    cairo_fill(cr);
                }
                SetCairoColor(cr, obj.strokeColor);
                cairo_set_line_width(cr, wPx);
                cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
                cairo_stroke(cr);

                if (obj.shapeKind == ShapeKind::SignatureStamp && sr.w > 8.0 && sr.h > 8.0) {
                    const DocRect inner{ sr.x + 3.0, sr.y + 3.0, sr.w - 6.0, sr.h - 6.0 };
                    cairo_set_line_width(cr, 0.8);
                    cairo_rectangle(cr, inner.x, inner.y, inner.w, inner.h);
                    cairo_stroke(cr);
                    if (!obj.textUtf8.empty()) {
                        const double fontPx = (std::max)(8.0, (obj.fontSizePt > 0.0 ? obj.fontSizePt : 10.5) * cam.zoom);
                        DrawPangoTextInRect(
                            cr, obj.textUtf8, obj.fontFamilyUtf8, fontPx,
                            false, true, false, obj.strokeColor,
                            { inner.x + 4.0, inner.y + 4.0, (std::max)(1.0, inner.w - 8.0), (std::max)(1.0, inner.h - 8.0) });
                    }
                }
            } else if (obj.shapeKind == ShapeKind::Ellipse) {
                const double cx = sr.x + sr.w * 0.5;
                const double cy = sr.y + sr.h * 0.5;
                const double rx = (std::max)(0.5, sr.w * 0.5);
                const double ry = (std::max)(0.5, sr.h * 0.5);
                cairo_save(cr);
                cairo_translate(cr, cx, cy);
                cairo_scale(cr, rx, ry);
                cairo_arc(cr, 0.0, 0.0, 1.0, 0.0, 6.28318530717958647692);
                cairo_restore(cr);

                if (obj.fillColor.a > 0) {
                    SetCairoColor(cr, obj.fillColor);
                    cairo_fill_preserve(cr);
                }
                SetCairoColor(cr, obj.strokeColor);
                cairo_set_line_width(cr, wPx);
                cairo_stroke(cr);
            } else {
                const DocPoint p0 = toScreen(obj.shapeStart.x, obj.shapeStart.y);
                const DocPoint p1 = toScreen(obj.shapeEnd.x, obj.shapeEnd.y);
                SetCairoColor(cr, obj.strokeColor);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
                cairo_set_line_width(cr, wPx);
                cairo_move_to(cr, p0.x, p0.y);
                cairo_line_to(cr, p1.x, p1.y);
                cairo_stroke(cr);

                if (obj.shapeKind == ShapeKind::Arrow) {
                    const double dx = p1.x - p0.x;
                    const double dy = p1.y - p0.y;
                    const double len = std::hypot(dx, dy);
                    if (len > 2.0) {
                        const double ux = dx / len;
                        const double uy = dy / len;
                        const double hs = (std::max)(8.0, obj.strokeWidth * 3.2 * cam.zoom);
                        const double px = -uy * (hs * 0.45);
                        const double py = ux * (hs * 0.45);
                        cairo_move_to(cr, p1.x - ux * hs + px, p1.y - uy * hs + py);
                        cairo_line_to(cr, p1.x, p1.y);
                        cairo_line_to(cr, p1.x - ux * hs - px, p1.y - uy * hs - py);
                        cairo_stroke(cr);
                    }
                }
            }
        } else if (obj.kind == ObjectKind::TextBox) {
            const DocRect sr = rectToScreen(obj.bounds);
            if (obj.fillColor.a > 0) {
                SetCairoColor(cr, obj.fillColor);
                cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
                cairo_fill(cr);
            }
            if (!obj.textUtf8.empty()) {
                const double fontPx = (std::max)(6.0, obj.fontSizePt * cam.zoom);
                if (MathTypeset::TextContainsMathMarkup(obj.textUtf8)) {
                    cairo_save(cr);
                    cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
                    cairo_clip(cr);
                    CairoMathBackend mbk(cr);
                    std::istringstream iss(obj.textUtf8);
                    std::string line;
                    double curY = sr.y + 2.0;
                    const double maxW = (std::max)(12.0, sr.w - 4.0);
                    while (std::getline(iss, line) && curY < sr.y + sr.h) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        curY += MathTypeset::DrawRichMathLine(mbk, line, sr.x + 2.0, curY, maxW, fontPx, obj.strokeColor, false);
                    }
                    cairo_restore(cr);
                } else {
                    DrawPangoTextInRect(
                        cr, obj.textUtf8, obj.fontFamilyUtf8, fontPx,
                        obj.bold, false, obj.italic, obj.strokeColor, sr);
                }
            }
        } else if (obj.kind == ObjectKind::Image) {
            const std::vector<std::uint8_t>* bytesPtr = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
            const DocRect sr = rectToScreen(obj.bounds);
            if (bytesPtr && !bytesPtr->empty()) {
                const std::string tag = !obj.assetKey.empty() ? obj.assetKey : ("inline_" + std::to_string(obj.id));
                if (CairoSurfacePtr surf = AcquireAssetSurface(tag, *bytesPtr, cacheSurfaces)) {
                    DrawSurfaceStretched(cr, surf.get(), sr);
                }
            } else {
                SetCairoColor(cr, DocColor::RGBA(200, 210, 224, 255));
                cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
                cairo_fill(cr);
            }
        } else if (obj.kind == ObjectKind::PdfCard) {
            const DocRect sr = rectToScreen(obj.bounds);
            SetCairoColor(cr, DocColor::RGBA(255, 255, 255, 255));
            cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
            cairo_fill(cr);

            const std::vector<std::uint8_t>* bytesPtr = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
            if (bytesPtr && !bytesPtr->empty()) {
                const std::string tag = !obj.assetKey.empty() ? obj.assetKey : ("pdfcard_" + std::to_string(obj.id));
                if (CairoSurfacePtr surf = AcquireAssetSurface(tag, *bytesPtr, cacheSurfaces)) {
                    DrawSurfaceStretched(cr, surf.get(), sr);
                }
            }
            SetCairoColor(cr, DocColor::RGBA(59, 130, 246, 255));
            cairo_set_line_width(cr, 1.5);
            cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
            cairo_stroke(cr);

            if (!bytesPtr) {
                const std::string title = "PDF p." + std::to_string(obj.pdfSourcePageIndex0 + 1) + "\n" + obj.captionUtf8;
                DrawPangoTextInRect(
                    cr, title, "Segoe UI", (std::max)(8.0, 12.0 * cam.zoom),
                    false, true, false, DocColor::RGBA(30, 41, 59, 255),
                    { sr.x + 8.0, sr.y + 8.0, (std::max)(1.0, sr.w - 16.0), (std::max)(1.0, sr.h - 16.0) });
            }
        } else if (obj.kind == ObjectKind::StudyTape ||
                   obj.kind == ObjectKind::MathBlock ||
                   obj.kind == ObjectKind::CircuitBlock ||
                   obj.kind == ObjectKind::CalcSessionCard ||
                   obj.kind == ObjectKind::PlotBlock) {
            const DocRect sr = rectToScreen(obj.bounds);
            const auto card = RenderCommon::BuildStemCardLayout(obj);
            const double cardRad = (std::max)(4.0, 10.0 * cam.zoom);

            // Subtle content-layer elevation shadow (no material translúcido on content cards)
            SetCairoColor(cr, RenderCommon::ApplyLayerOpacity(DocColor::RGBA(15, 23, 42, 22), layerOpacity));
            CairoRoundedRectPath(cr, sr.x + 1.0, sr.y + 2.5, sr.w, sr.h, cardRad);
            cairo_fill(cr);

            SetCairoColor(cr, RenderCommon::ApplyLayerOpacity(card.bg, layerOpacity));
            CairoRoundedRectPath(cr, sr.x, sr.y, sr.w, sr.h, cardRad);
            cairo_fill(cr);

            SetCairoColor(cr, RenderCommon::ApplyLayerOpacity(card.border, layerOpacity));
            cairo_set_line_width(cr, (std::max)(1.0, 1.25 * cam.zoom));
            CairoRoundedRectPath(cr, sr.x, sr.y, sr.w, sr.h, cardRad);
            cairo_stroke(cr);

            const double pad = 6.0 * cam.zoom;
            const double hdrH = 22.0 * cam.zoom;
            cairo_save(cr);
            CairoRoundedRectPath(cr, sr.x, sr.y, sr.w, sr.h, cardRad);
            cairo_clip(cr);
            CairoMathBackend mbk(cr);
            MathTypeset::DrawStemCardContent(
                mbk, obj, sr, cam.zoom,
                RenderCommon::ApplyLayerOpacity(card.titleColor, layerOpacity),
                RenderCommon::ApplyLayerOpacity(card.bodyColor, layerOpacity),
                card.headerUtf8, card.bodyUtf8);
            cairo_restore(cr);

            if (obj.kind == ObjectKind::CalcSessionCard && !obj.pngBytes.empty()) {
                const double imgW = (std::min)(sr.w * 0.42, 130.0 * cam.zoom);
                const double imgH = (std::min)(sr.h * 0.60, imgW * 0.55);
                if (imgW > 18.0 && imgH > 12.0) {
                    const DocRect lcdR{ sr.x + sr.w - imgW - pad, sr.y + hdrH + pad * 0.3, imgW, imgH };
                    if (CairoSurfacePtr surf = AcquireAssetSurface("calc_lcd_" + std::to_string(obj.id), obj.pngBytes, cacheSurfaces)) {
                        DrawSurfaceStretched(cr, surf.get(), lcdR);
                    }
                }
            } else if (obj.kind == ObjectKind::PlotBlock) {
                const auto plot = RenderCommon::BuildPlotRenderData(obj);
                const double px0 = sr.x + pad * 1.2;
                const double py0 = sr.y + hdrH + pad * 0.6;
                const double pw = (std::max)(16.0, sr.w - 2.4 * pad);
                const double ph = (std::max)(16.0, sr.h - hdrH - 3.0 * pad);
                SetCairoColor(cr, DocColor::RGBA(255, 255, 255, 235));
                cairo_rectangle(cr, px0, py0, pw, ph);
                cairo_fill_preserve(cr);
                SetCairoColor(cr, DocColor::RGBA(148, 163, 184, 255));
                cairo_set_line_width(cr, 1.0);
                cairo_stroke(cr);

                auto mapPt = [&](double wx, double wy) -> DocPoint {
                    const double nx = (wx - plot.xMin) / (std::max)(1e-9, plot.xMax - plot.xMin);
                    const double ny = (wy - plot.yMin) / (std::max)(1e-9, plot.yMax - plot.yMin);
                    return {
                        px0 + std::clamp(nx, 0.0, 1.0) * pw,
                        (py0 + ph) - std::clamp(ny, 0.0, 1.0) * ph
                    };
                };
                if (plot.is3D && !plot.surface3DSegments.empty()) {
                    cairo_set_line_width(cr, (std::max)(1.0, 1.35 * cam.zoom));
                    for (const auto& seg : plot.surface3DSegments) {
                        SetCairoColor(cr, seg.color);
                        const auto pa = mapPt(seg.a.x, seg.a.y);
                        const auto pb = mapPt(seg.b.x, seg.b.y);
                        cairo_move_to(cr, pa.x, pa.y);
                        cairo_line_to(cr, pb.x, pb.y);
                        cairo_stroke(cr);
                    }
                } else {
                    if (plot.xMin <= 0.0 && plot.xMax >= 0.0) {
                        const auto a = mapPt(0.0, plot.yMin), b = mapPt(0.0, plot.yMax);
                        cairo_move_to(cr, a.x, a.y); cairo_line_to(cr, b.x, b.y); cairo_stroke(cr);
                    }
                    if (plot.yMin <= 0.0 && plot.yMax >= 0.0) {
                        const auto a = mapPt(plot.xMin, 0.0), b = mapPt(plot.xMax, 0.0);
                        cairo_move_to(cr, a.x, a.y); cairo_line_to(cr, b.x, b.y); cairo_stroke(cr);
                    }
                    if (plot.curvePoints.size() >= 2) {
                        SetCairoColor(cr, DocColor::RGBA(79, 70, 229, 255));
                        cairo_set_line_width(cr, (std::max)(1.3, 1.8 * cam.zoom));
                        const auto first = mapPt(plot.curvePoints[0].x, plot.curvePoints[0].y);
                        cairo_move_to(cr, first.x, first.y);
                        for (std::size_t i = 1; i < plot.curvePoints.size(); ++i) {
                            const auto pt = mapPt(plot.curvePoints[i].x, plot.curvePoints[i].y);
                            cairo_line_to(cr, pt.x, pt.y);
                        }
                        cairo_stroke(cr);
                    }
                    SetCairoColor(cr, DocColor::RGBA(220, 38, 38, 255));
                    for (const auto& sp : plot.scatterPoints) {
                        const auto pt = mapPt(sp.x, sp.y);
                        cairo_arc(cr, pt.x, pt.y, 3.0, 0.0, 6.28318530717958647692);
                        cairo_fill(cr);
                    }
                }
            }
        }

        if (isSelected) {
            const DocRect sr = rectToScreen(obj.bounds);
            const DocRect selBox{ sr.x - 3.0, sr.y - 3.0, sr.w + 6.0, sr.h + 6.0 };
            cairo_save(cr);
            const double dashes[2] = { 5.0, 3.0 };
            cairo_set_dash(cr, dashes, 2, 0.0);
            SetCairoColor(cr, DocColor::RGBA(37, 99, 235, 255));
            cairo_set_line_width(cr, 1.6);
            cairo_rectangle(cr, selBox.x, selBox.y, selBox.w, selBox.h);
            cairo_stroke(cr);

            // Corner selection handles
            cairo_set_dash(cr, nullptr, 0, 0.0);
            constexpr double hSz = 6.0;
            const DocPoint corners[4] = {
                { selBox.x, selBox.y },
                { selBox.Right(), selBox.y },
                { selBox.x, selBox.Bottom() },
                { selBox.Right(), selBox.Bottom() }
            };
            for (const auto& c : corners) {
                SetCairoColor(cr, DocColor::RGBA(255, 255, 255, 255));
                cairo_rectangle(cr, c.x - hSz * 0.5, c.y - hSz * 0.5, hSz, hSz);
                cairo_fill_preserve(cr);
                SetCairoColor(cr, DocColor::RGBA(37, 99, 235, 255));
                cairo_set_line_width(cr, 1.2);
                cairo_stroke(cr);
            }
            cairo_restore(cr);
        }
    }

    void RenderSceneToCairo(
        cairo_t* cr,
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
        if (!cr || widthPx <= 0 || heightPx <= 0) return;

        cairo_save(cr);
        cairo_rectangle(cr, 0.0, 0.0, static_cast<double>(widthPx), static_cast<double>(heightPx));
        cairo_clip(cr);

        if (forceCanvasMode || doc.Mode() == DocumentMode::InfiniteCanvas) {
            SetCairoColor(cr, DocColor::RGBA(252, 253, 255, 255));
            cairo_paint(cr);

            const DocRect viewWorld = cam.ScreenRectToWorld({
                0.0, 0.0, static_cast<double>(widthPx), static_cast<double>(heightPx)
            });
            DrawPaperPattern(cr, viewWorld, doc.CanvasBackground(), cam);

            const std::vector<std::uint64_t> visibleIds = doc.QueryCanvasRect(viewWorld);
            std::vector<const AnnotationObject*> drawList;
            drawList.reserve(visibleIds.size());
            for (std::uint64_t id : visibleIds) {
                if (const auto* o = doc.FindCanvasObject(id)) drawList.push_back(o);
            }
            std::sort(drawList.begin(), drawList.end(), [](const AnnotationObject* a, const AnnotationObject* b) {
                return a->zOrder < b->zOrder;
            });

            for (const auto* o : drawList) {
                const bool sel = RenderCommon::IsObjectSelectedInOverlay(overlay, o->id);
                DrawAnnotationObject(cr, doc, *o, cam, 0.0, 0.0, sel, !synchronousOffscreen, overlay);
            }
            if (overlay && overlay->draftObject) {
                DrawAnnotationObject(cr, doc, *overlay->draftObject, cam, 0.0, 0.0, false, !synchronousOffscreen, overlay);
            }
            if (overlay && overlay->laserPointer.active) {
                const auto& lp = overlay->laserPointer;
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                for (std::size_t i = 1; i < lp.trail.size(); ++i) {
                    const double frac = static_cast<double>(i) / static_cast<double>(lp.trail.size());
                    SetCairoColor(cr, DocColor::RGBA(239, 68, 68, static_cast<std::uint8_t>(40 + 180 * frac)));
                    const DocPoint a = cam.WorldToScreen(lp.trail[i - 1].x, lp.trail[i - 1].y);
                    const DocPoint b = cam.WorldToScreen(lp.trail[i].x, lp.trail[i].y);
                    cairo_set_line_width(cr, 2.0 + 3.0 * frac);
                    cairo_move_to(cr, a.x, a.y);
                    cairo_line_to(cr, b.x, b.y);
                    cairo_stroke(cr);
                }
                const DocPoint tip = cam.WorldToScreen(lp.worldX, lp.worldY);
                SetCairoColor(cr, DocColor::RGBA(239, 68, 68, 95));
                cairo_arc(cr, tip.x, tip.y, 11.0, 0.0, 6.28318530717958647692);
                cairo_fill(cr);
                SetCairoColor(cr, DocColor::RGBA(255, 255, 255, 245));
                cairo_arc(cr, tip.x, tip.y, 4.5, 0.0, 6.28318530717958647692);
                cairo_fill(cr);
            }
            DrawHudOverlaysCairo(cr, doc, cam, widthPx, heightPx, overlay);
            cairo_restore(cr);
            return;
        }

        // Paginated PDF / Notebook Mode
        SetCairoColor(cr, DocColor::RGBA(36, 42, 52, 255));
        cairo_paint(cr);

        const DocRect viewWorld = cam.ScreenRectToWorld({
            0.0, 0.0, static_cast<double>(widthPx), static_cast<double>(heightPx)
        });
        std::size_t firstPg = 0, lastPg = 0;
        if (!doc.Layout().ComputeVisibleRange(viewWorld.y, viewWorld.Bottom(), firstPg, lastPg, 1)) {
            cairo_restore(cr);
            return;
        }

        const auto& pages = doc.Pages();
        for (std::size_t p = firstPg; p <= lastPg && p < pages.size(); ++p) {
            const auto* place = doc.Layout().PlacementAt(p);
            if (!place) continue;
            const DocRect pageWorld = place->WorldRect();
            const DocRect pageScreen = cam.WorldRectToScreen(pageWorld);

            // Drop shadow + crisp white page card
            SetCairoColor(cr, DocColor::RGBA(15, 19, 25, 130));
            cairo_rectangle(cr, pageScreen.x + 4.0, pageScreen.y + 5.0, pageScreen.w, pageScreen.h);
            cairo_fill(cr);

            SetCairoColor(cr, DocColor::RGBA(255, 255, 255, 255));
            cairo_rectangle(cr, pageScreen.x, pageScreen.y, pageScreen.w, pageScreen.h);
            cairo_fill(cr);

            const auto& pg = pages[p];
            if (pg.hasPdfSource && !pg.sourcePdfPathUtf8.empty()) {
                const int targetW = static_cast<int>(std::lround((std::max)(80.0, pageScreen.w)));
                const PageCacheKey key = PageRenderCache::MakeKey(pg, targetW);

                CairoSurfacePtr pageSurf;
                if (!synchronousOffscreen) {
                    pageSurfaceCache.TryGet(key, pageSurf);
                }

                if (!pageSurf) {
                    RasterImageBgra32 img;
                    bool haveRaster = pageCache && pageCache->TryGet(key, img);
                    if (!haveRaster && synchronousOffscreen) {
                        NormalizedCropRect c = pg.crop;
                        c.Clamp();
                        haveRaster = WinRtPdfRenderer::RenderPageToBgra32(
                            Utf8ToWide(pg.sourcePdfPathUtf8),
                            pg.sourcePageIndex0,
                            key.widthBucketPx,
                            key.rotationDeg,
                            c,
                            img);
                        if (haveRaster && pageCache) pageCache->PutDirect(key, img);
                    }
                    if (haveRaster && img.Valid()) {
                        pageSurf = UploadBgra32ToCairoSurface(img);
                        if (pageSurf && !synchronousOffscreen) {
                            pageSurfaceCache.Put(key, pageSurf, img.ByteSize());
                        }
                    } else if (pageCache && !synchronousOffscreen) {
                        pageCache->RequestAsync(key, asyncGen, onAsyncPageReady);
                    }
                }

                if (pageSurf) {
                    DrawSurfaceStretched(cr, pageSurf.get(), pageScreen);
                } else {
                    const std::string msg = "Renderizando p\xC3\xA1gina " + std::to_string(p + 1) + "...";
                    DrawPangoTextInRect(
                        cr, msg, "Segoe UI", 13.0, false, false, false,
                        DocColor::RGBA(120, 132, 148, 255),
                        { pageScreen.x + 16.0, pageScreen.y + 16.0,
                          (std::max)(1.0, pageScreen.w - 32.0), (std::max)(1.0, pageScreen.h - 32.0) });
                }
            }

            cairo_save(cr);
            cairo_rectangle(cr, pageScreen.x, pageScreen.y, pageScreen.w, pageScreen.h);
            cairo_clip(cr);

            DrawPaperPattern(cr, pageWorld, pg.background, cam);

            for (const auto& obj : pg.objects) {
                const bool sel = RenderCommon::IsObjectSelectedInOverlay(overlay, obj.id);
                DrawAnnotationObject(cr, doc, obj, cam, pageWorld.x, pageWorld.y, sel, !synchronousOffscreen, overlay);
            }
            if (overlay && overlay->draftObject && p == overlay->draftPageIndex) {
                DrawAnnotationObject(cr, doc, *overlay->draftObject, cam, pageWorld.x, pageWorld.y, false, !synchronousOffscreen, overlay);
            }

            // Search match highlights
            if (overlay && overlay->searchMatches) {
                for (std::size_t mi = 0; mi < overlay->searchMatches->size(); ++mi) {
                    const auto& m = (*overlay->searchMatches)[mi];
                    if (m.pageIndex != p) continue;
                    const bool isActive = (mi == overlay->activeSearchMatchIndex);
                    SetCairoColor(
                        cr,
                        isActive ? DocColor::RGBA(251, 146, 60, 120) : DocColor::RGBA(250, 204, 21, 90));
                    if (m.hasBox) {
                        const DocRect mr{
                            pageWorld.x + m.boxNormalized.x * pageWorld.w,
                            pageWorld.y + m.boxNormalized.y * pageWorld.h,
                            m.boxNormalized.w * pageWorld.w,
                            m.boxNormalized.h * pageWorld.h
                        };
                        const DocRect sr = cam.WorldRectToScreen(mr);
                        cairo_rectangle(cr, sr.x, sr.y, sr.w, sr.h);
                        cairo_fill(cr);
                    } else {
                        cairo_rectangle(cr, pageScreen.x + 6.0, pageScreen.y + 6.0, 90.0, 20.0);
                        cairo_fill(cr);
                    }
                }
            }

            // Crop preview overlay (dim outer region + blue dashed selection frame)
            if (overlay && overlay->hasCropPreview && overlay->cropPageIndex == p) {
                const DocRect crWorld{
                    pageWorld.x + overlay->cropPreviewPageLocal.x,
                    pageWorld.y + overlay->cropPreviewPageLocal.y,
                    overlay->cropPreviewPageLocal.w,
                    overlay->cropPreviewPageLocal.h
                };
                const DocRect cropScreen = cam.WorldRectToScreen(crWorld);

                cairo_save(cr);
                cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
                cairo_rectangle(cr, pageScreen.x, pageScreen.y, pageScreen.w, pageScreen.h);
                cairo_rectangle(cr, cropScreen.x, cropScreen.y, cropScreen.w, cropScreen.h);
                SetCairoColor(cr, DocColor::RGBA(15, 23, 42, 85));
                cairo_fill(cr);

                const double dashes[2] = { 6.0, 4.0 };
                cairo_set_dash(cr, dashes, 2, 0.0);
                SetCairoColor(cr, DocColor::RGBA(37, 99, 235, 255));
                cairo_set_line_width(cr, 2.0);
                cairo_rectangle(cr, cropScreen.x, cropScreen.y, cropScreen.w, cropScreen.h);
                cairo_stroke(cr);
                cairo_restore(cr);
            }

            cairo_restore(cr);
        }

        DrawHudOverlaysCairo(cr, doc, cam, widthPx, heightPx, overlay);
        cairo_restore(cr);
    }

    void DrawHudOverlaysCairo(
        cairo_t* cr,
        const DocumentModel& doc,
        const Camera2D& cam,
        int widthPx,
        int heightPx,
        const ViewportOverlayState* overlay)
    {
        if (!overlay) return;

        if (overlay->rulerState.visible) {
            const auto rg = RenderCommon::BuildRulerOverlay(overlay->rulerState, cam);
            cairo_new_path(cr);
            cairo_move_to(cr, rg.corners[0].x, rg.corners[0].y);
            for (int k = 1; k < 4; ++k) cairo_line_to(cr, rg.corners[k].x, rg.corners[k].y);
            cairo_close_path(cr);
            SetCairoColor(cr, DocColor::RGBA(224, 242, 254, 175));
            cairo_fill_preserve(cr);
            SetCairoColor(cr, DocColor::RGBA(2, 132, 199, 240));
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);

            SetCairoColor(cr, DocColor::RGBA(3, 105, 161, 255));
            cairo_set_line_width(cr, 2.2);
            cairo_move_to(cr, rg.activeEdge0.x, rg.activeEdge0.y);
            cairo_line_to(cr, rg.activeEdge1.x, rg.activeEdge1.y);
            cairo_stroke(cr);

            for (const auto& tk : rg.ticks) {
                cairo_set_line_width(cr, tk.major ? 1.4 : 0.9);
                cairo_move_to(cr, tk.p0.x, tk.p0.y);
                cairo_line_to(cr, tk.p1.x, tk.p1.y);
                cairo_stroke(cr);
            }
            if (!rg.badgeUtf8.empty()) {
                DrawPangoTextInRect(
                    cr, rg.badgeUtf8, "Segoe UI", 11.0, true, false, false,
                    DocColor::RGBA(12, 74, 110, 255),
                    { rg.badgePos.x, rg.badgePos.y, 110.0, 22.0 });
            }
        }

        if (overlay->minimapVisible) {
            const auto mm = RenderCommon::BuildMinimapOverlay(doc, cam, widthPx, heightPx);
            SetCairoColor(cr, DocColor::RGBA(15, 23, 42, 215));
            cairo_rectangle(cr, mm.panelScreen.x, mm.panelScreen.y, mm.panelScreen.w, mm.panelScreen.h);
            cairo_fill_preserve(cr);
            SetCairoColor(cr, DocColor::RGBA(100, 116, 139, 240));
            cairo_set_line_width(cr, 1.2);
            cairo_stroke(cr);

            for (const auto& it : mm.items) {
                SetCairoColor(cr, it.color);
                cairo_rectangle(cr, it.rectScreen.x, it.rectScreen.y, it.rectScreen.w, it.rectScreen.h);
                cairo_fill(cr);
            }
            SetCairoColor(cr, DocColor::RGBA(250, 204, 21, 250));
            cairo_set_line_width(cr, 1.6);
            cairo_rectangle(cr, mm.viewportScreen.x, mm.viewportScreen.y, mm.viewportScreen.w, mm.viewportScreen.h);
            cairo_stroke(cr);
        }
    }

    bool RenderToBgraSurface(
        const DocumentModel& doc,
        const Camera2D& cam,
        int w,
        int h,
        PageRenderCache* pageCache,
        const ViewportOverlayState* overlay,
        bool forceCanvasMode,
        RasterImageBgra32& outBgra)
    {
        if (w <= 0 || h <= 0) return false;
        cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
        if (!surf || cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
            if (surf) cairo_surface_destroy(surf);
            return false;
        }
        cairo_t* cr = cairo_create(surf);
        if (!cr || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            if (cr) cairo_destroy(cr);
            cairo_surface_destroy(surf);
            return false;
        }

        RenderSceneToCairo(cr, doc, cam, w, h, pageCache, 1, nullptr, overlay, true, forceCanvasMode);
        cairo_destroy(cr);

        cairo_surface_flush(surf);
        const unsigned char* src = cairo_image_surface_get_data(surf);
        const int stride = cairo_image_surface_get_stride(surf);
        const std::size_t rowBytes = static_cast<std::size_t>(w) * 4u;
        if (!src || stride < static_cast<int>(rowBytes)) {
            cairo_surface_destroy(surf);
            return false;
        }

        outBgra.width = w;
        outBgra.height = h;
        outBgra.pixels.resize(rowBytes * static_cast<std::size_t>(h));
        for (int y = 0; y < h; ++y) {
            std::memcpy(
                outBgra.pixels.data() + static_cast<std::size_t>(y) * rowBytes,
                src + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride),
                rowBytes);
        }
        cairo_surface_destroy(surf);
        return outBgra.Valid();
    }
};

D2DRenderer::D2DRenderer() : impl_(new Impl()) {}

D2DRenderer::~D2DRenderer() {
    delete impl_;
    impl_ = nullptr;
}

bool D2DRenderer::InitializeFactories() {
    if (impl_) impl_->initialized = true;
    return true;
}

void D2DRenderer::Resize(int widthPx, int heightPx) {
    if (!impl_) return;
    impl_->widthPx = (std::max)(1, widthPx);
    impl_->heightPx = (std::max)(1, heightPx);
}

void D2DRenderer::DiscardDeviceResources() {
    if (!impl_) return;
    impl_->pageSurfaceCache.Clear();
    impl_->assetSurfaceCache.Clear();
}

void D2DRenderer::RenderToCairo(
    void* cairoContext,
    const DocumentModel& doc,
    const Camera2D& camera,
    int widthPx,
    int heightPx,
    PageRenderCache* pageCache,
    std::uint64_t asyncGen,
    std::function<void()> onAsyncPageReady,
    const ViewportOverlayState* overlay)
{
    if (!impl_ || !cairoContext || widthPx <= 0 || heightPx <= 0) return;
    if (!impl_->initialized) InitializeFactories();
    impl_->RenderSceneToCairo(
        static_cast<cairo_t*>(cairoContext),
        doc,
        camera,
        widthPx,
        heightPx,
        pageCache,
        asyncGen,
        std::move(onAsyncPageReady),
        overlay,
        false,
        false);
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
    // Headless fallback: on Linux GTK4/Cairo, callers use RenderToCairo() or RenderHwndViewportToOffscreen().
    (void)doc;
    (void)camera;
    (void)widthPx;
    (void)heightPx;
    (void)pageCache;
    (void)asyncGen;
    (void)onAsyncPageReady;
    (void)overlay;
}

bool D2DRenderer::RenderPageOffscreen(
    const DocumentModel& doc,
    std::size_t pageIndex0,
    int widthPx,
    PageRenderCache* pageCache,
    RasterImageBgra32& outBgra)
{
    if (!InitializeFactories() || !impl_ || pageIndex0 >= doc.Pages().size()) return false;
    const auto* place = doc.Layout().PlacementAt(pageIndex0);
    if (!place || place->widthWorld <= 0.0) return false;

    const int w = std::clamp(widthPx, 64, 3000);
    const double zoom = static_cast<double>(w) / place->widthWorld;
    const int h = (std::max)(64, static_cast<int>(std::lround(place->heightWorld * zoom)));

    Camera2D cam;
    cam.panX = -place->widthWorld * 0.5;
    cam.panY = place->topWorld;
    cam.zoom = zoom;

    return impl_->RenderToBgraSurface(doc, cam, w, h, pageCache, nullptr, false, outBgra);
}

bool D2DRenderer::RenderInfiniteCanvasOffscreen(
    const DocumentModel& doc,
    const DocRect& worldRegion,
    int widthPx,
    int heightPx,
    RasterImageBgra32& outBgra)
{
    if (worldRegion.w <= 0.0 || worldRegion.h <= 0.0) return false;
    if (!InitializeFactories() || !impl_) return false;
    const int w = (std::max)(1, widthPx);
    const int h = (std::max)(1, heightPx);

    Camera2D cam;
    cam.panX = worldRegion.x;
    cam.panY = worldRegion.y;
    cam.zoom = static_cast<double>(w) / worldRegion.w;

    return impl_->RenderToBgraSurface(doc, cam, w, h, nullptr, nullptr, true, outBgra);
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
    if (!InitializeFactories() || !impl_) return false;
    const int w = (std::max)(1, widthPx);
    const int h = (std::max)(1, heightPx);
    return impl_->RenderToBgraSurface(doc, camera, w, h, pageCache, overlay, false, outBgra);
}

} // namespace DocStudio
