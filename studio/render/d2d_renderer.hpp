#pragma once

#include "../core/camera.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/inline_url_parser.hpp"
#include "../core/lru_byte_cache.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace DocStudio {

struct GraphViewData;
class FlexcilStudyOverlay;
struct ZoomWritingBoxState;
class StructuredDatabase;
class AnchoredCommentThreadManager;

namespace OverlayGeometry {
inline DocRect GraphPanelRect(int widthPx, int heightPx) noexcept {
    const double gw = (widthPx > 588) ? 540.0 : static_cast<double>((widthPx > 96) ? (widthPx - 48) : widthPx);
    const double gh = (heightPx > 428) ? 380.0 : static_cast<double>((heightPx > 96) ? (heightPx - 48) : heightPx);
    return { (widthPx - gw) * 0.5, (heightPx - gh) * 0.5, gw, gh };
}
inline DocRect FlexcilPopupPanelRect(int widthPx, int heightPx) noexcept {
    double pw = static_cast<double>(widthPx) * 0.36;
    if (pw < 300.0) pw = 300.0;
    if (pw > 440.0) pw = 440.0;
    if (pw > widthPx - 32.0) pw = (widthPx > 64) ? (widthPx - 32.0) : static_cast<double>(widthPx);
    const double ph = (heightPx > 140) ? (static_cast<double>(heightPx) - 108.0) : static_cast<double>(heightPx);
    return { static_cast<double>(widthPx) - pw - 16.0, 20.0, pw, ph };
}
inline DocRect ZoomBoxDockRect(int widthPx, int heightPx) noexcept {
    const double margin = 16.0;
    const double dh = 168.0;
    const double dw = (widthPx > 64) ? (static_cast<double>(widthPx) - margin * 2.0) : static_cast<double>(widthPx);
    return { margin, static_cast<double>(heightPx) - dh - margin, dw, dh };
}
inline DocRect DatabasePanelRect(int widthPx, int heightPx) noexcept {
    const double dw = (widthPx > 908) ? 860.0 : static_cast<double>((widthPx > 80) ? (widthPx - 48) : widthPx);
    const double dh = (heightPx > 584) ? 520.0 : static_cast<double>((heightPx > 80) ? (heightPx - 64) : heightPx);
    return { (widthPx - dw) * 0.5, (heightPx - dh) * 0.5, dw, dh };
}
inline DocRect CommentsPanelRect(int widthPx, int heightPx) noexcept {
    double pw = 320.0;
    if (pw > widthPx - 32.0) pw = (widthPx > 64) ? (widthPx - 32.0) : static_cast<double>(widthPx);
    const double ph = (heightPx > 140) ? (static_cast<double>(heightPx) - 108.0) : static_cast<double>(heightPx);
    return { static_cast<double>(widthPx) - pw - 16.0, 20.0, pw, ph };
}
inline DocRect AudioTimelineBarRect(int widthPx, int heightPx) noexcept {
    const double bw = (widthPx > 688) ? 640.0 : static_cast<double>((widthPx > 64) ? (widthPx - 48) : widthPx);
    const double bh = 54.0;
    return { (widthPx - bw) * 0.5, static_cast<double>(heightPx) - bh - 16.0, bw, bh };
}
} // namespace OverlayGeometry

struct PageCacheKey {
    std::uint64_t pageId = 0;
    std::string sourcePdfPath;
    int sourcePageIndex0 = 0;
    int widthBucketPx = 800;
    int rotationDeg = 0;
    int cropL = 0;
    int cropT = 0;
    int cropR = 1000;
    int cropB = 1000;

    bool operator==(const PageCacheKey& o) const noexcept {
        return pageId == o.pageId &&
               sourcePageIndex0 == o.sourcePageIndex0 &&
               widthBucketPx == o.widthBucketPx &&
               rotationDeg == o.rotationDeg &&
               cropL == o.cropL && cropT == o.cropT &&
               cropR == o.cropR && cropB == o.cropB &&
               sourcePdfPath == o.sourcePdfPath;
    }
};

struct PageCacheKeyHash {
    std::size_t operator()(const PageCacheKey& k) const noexcept {
        std::size_t h = std::hash<std::uint64_t>{}(k.pageId);
        auto mix = [&](std::size_t v) {
            h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        };
        mix(std::hash<std::string>{}(k.sourcePdfPath));
        mix(static_cast<std::size_t>(k.sourcePageIndex0));
        mix(static_cast<std::size_t>(k.widthBucketPx));
        mix(static_cast<std::size_t>(k.rotationDeg));
        mix(static_cast<std::size_t>((k.cropL * 131) ^ (k.cropT * 997) ^ (k.cropR * 31) ^ k.cropB));
        return h;
    }
};

// Async byte-budgeted LRU page bitmap cache (64 MiB ultra-low-RAM default)
class PageRenderCache {
public:
    explicit PageRenderCache(std::size_t maxBytes = 64ull * 1024ull * 1024ull);
    ~PageRenderCache();

    PageRenderCache(const PageRenderCache&) = delete;
    PageRenderCache& operator=(const PageRenderCache&) = delete;

    static PageCacheKey MakeKey(const PageModel& pg, int targetWidthPx);

    void SetMaxBytes(std::size_t maxBytes);
    std::size_t MaxBytes() const;
    std::size_t BytesUsed() const;
    std::size_t EntryCount() const;
    std::uint64_t EvictionCount() const;

    bool TryGet(const PageCacheKey& key, RasterImageBgra32& outImg);
    void PutDirect(const PageCacheKey& key, RasterImageBgra32 img);
    void RequestAsync(const PageCacheKey& key, std::uint64_t generation, std::function<void()> onReady);
    void InvalidateAll(std::uint64_t newGeneration);
    void Shutdown();

private:
    struct PendingJob {
        PageCacheKey key;
        std::uint64_t generation = 0;
        std::function<void()> onReady;
    };

    LruByteCache<PageCacheKey, RasterImageBgra32, PageCacheKeyHash> cache_;
    std::mutex queueMtx_;
    std::condition_variable cv_;
    std::deque<PendingJob> queue_;
    std::unordered_map<PageCacheKey, std::uint64_t, PageCacheKeyHash> inFlight_;
    std::atomic<std::uint64_t> activeGen_{ 1 };
    std::atomic<bool> stop_{ false };
    std::thread worker_;

    void WorkerLoop();
};

struct ViewportOverlayState {
    const AnnotationObject* draftObject = nullptr;
    std::size_t draftPageIndex = 0;
    std::uint64_t selectedObjectId = 0;
    std::vector<std::uint64_t> lassoSelectedIds;
    bool hasCropPreview = false;
    std::size_t cropPageIndex = 0;
    DocRect cropPreviewPageLocal{};
    const std::vector<SearchMatch>* searchMatches = nullptr;
    std::size_t activeSearchMatchIndex = 0;
    bool replayActive = false;
    std::uint64_t replayCursorMs = 0;
    LaserPointerState laserPointer{};
    bool presenterMode = false;
    bool minimapVisible = false;
    RulerState rulerState{};
    bool focusMode = false;
    bool darkTheme = false;
    bool highContrast = false;
    const RasterImageBgra32* floatingOverlayBgra = nullptr;
    DocRect floatingOverlayScreenRect{};
    bool hasPdfTextSel = false;
    std::size_t pdfTextSelPage = 0;
    DocRect pdfTextSelRectLocal{};
    bool showSelectionHandles = true;
    bool graphOverlayVisible = false;
    const GraphViewData* graphOverlay = nullptr;

    // Flexcil Dual Study Pop-up & Gesture Capture indicator
    bool flexcilPopupVisible = false;
    bool flexcilGestureMode = false;
    const FlexcilStudyOverlay* flexcilOverlay = nullptr;

    // Zoom Writing Box magnified bottom dock + page target highlight
    const ZoomWritingBoxState* zoomWritingBox = nullptr;
    std::size_t zoomWritingPageIndex = 0;

    // Structured Database visual panel (9 views: Table..Map)
    bool dbPanelVisible = false;
    int dbViewKind = 0;
    const StructuredDatabase* activeDatabase = nullptr;

    // Anchored Comment Pins & Side Panel
    const AnchoredCommentThreadManager* commentThreads = nullptr;
    bool commentsPanelVisible = false;
    std::uint64_t selectedCommentThreadId = 0;

    // Distraction-free Reading View
    bool readingViewMode = false;
    std::string readingModeTitleUtf8;
    std::string readingModeTextUtf8;

    // Synchronized Audio Timeline waveform bar
    bool audioTimelineVisible = false;
    bool audioRecording = false;
    bool audioPlaying = false;
    std::uint64_t audioCursorMs = 0;
    std::uint64_t audioDurationMs = 0;
    double audioSpeed = 1.0;
    std::vector<float> audioEnvelopeBins;
    std::vector<std::uint64_t> audioBookmarkMs;

    // Document Lock indicator
    bool noteLocked = false;

    // Graph Local/Global + Filter badge
    bool graphLocalMode = false;
    std::string graphFilterLabelUtf8;

    // Hover Link Preview Card (OB-LINK-045)
    bool hasHoverPreview = false;
    DocPoint hoverPreviewScreenPt{};
    std::string hoverPreviewTitleUtf8;
    std::string hoverPreviewExcerptUtf8;

    // Multi-frame GIF / animation tick counter (NT-EDIT-031)
    std::uint32_t animationTick = 0;

    // Universal Accessibility Suite (Reading Ruler, Dyslexia, Anti-Fatigue Tints, Color-Blind Okabe-Ito)
    bool readingRulerEnabled = false;
    double readingRulerCursorY = 260.0;
    int readingRulerHeightPx = 76;
    double readingRulerMaskOpacity = 0.42;
    bool dyslexiaFriendlyMode = false;
    int paperTintPreset = 0;
    int colorBlindPaletteMode = 0;
    bool enhancedFocusRing = false;

    // Smart Pomodoro Study HUD Pill
    bool pomodoroVisible = false;
    bool pomodoroBreakPhase = false;
    std::string pomodoroBadgeUtf8;

    // Zero-GPU On-Demand Rich Media State (Real GIF timing, Active Video frame, Live HtmlEmbed)
    std::uint64_t animationClockMs = 0; // Monotonic ms clock for real GIF89a /grctlext/Delay timing (0 = fallback animationTick)
    std::uint64_t activeVideoObjectId = 0;
    const RasterImageBgra32* activeVideoFrameBgra = nullptr;
    std::uint64_t activeVideoPosMs = 0;
    bool activeVideoPlaying = false;
    std::uint64_t activeHtmlObjectId = 0;
    int topChromeClearancePx = 0;
};

class D2DRenderer {
public:
    D2DRenderer();
    ~D2DRenderer();

    D2DRenderer(const D2DRenderer&) = delete;
    D2DRenderer& operator=(const D2DRenderer&) = delete;

    bool InitializeFactories();
#ifdef _WIN32
    void AttachHwnd(HWND hwnd, int widthPx, int heightPx);
    // Strict 0% GPU mode by default (D2D1_RENDER_TARGET_TYPE_SOFTWARE). Optional HW toggle.
    void SetHardwareAcceleration(bool enable);
    bool IsHardwareAccelerationEnabled() const noexcept;
    void TrimCaches(bool aggressive = true);
    bool LastFrameHadVisibleAnimatedGif() const noexcept;
    UINT LastFrameMinGifDelayMs() const noexcept;
    std::size_t CachedBitmapCount() const noexcept;
    static bool InspectGifMetadataWic(
        const std::vector<std::uint8_t>& gifBytes,
        int& outFrameCount,
        std::vector<int>& outDelaysMs);
    bool HitTestTextBoxInlineUrl(
        const AnnotationObject& obj,
        double localX,
        double localY,
        double zoom,
        bool dyslexiaMode,
        InlineUrlSpan& outSpan) const;
#else
    void RenderToCairo(
        void* cairoContext,
        const DocumentModel& doc,
        const Camera2D& camera,
        int widthPx,
        int heightPx,
        PageRenderCache* pageCache,
        std::uint64_t asyncGen,
        std::function<void()> onAsyncPageReady,
        const ViewportOverlayState* overlay = nullptr);
#endif
    void Resize(int widthPx, int heightPx);
    void DiscardDeviceResources();

    void RenderViewport(
        const DocumentModel& doc,
        const Camera2D& camera,
        int widthPx,
        int heightPx,
        PageRenderCache* pageCache,
        std::uint64_t asyncGen,
        std::function<void()> onAsyncPageReady,
        const ViewportOverlayState* overlay = nullptr);

    bool RenderPageOffscreen(
        const DocumentModel& doc,
        std::size_t pageIndex0,
        int widthPx,
        PageRenderCache* pageCache,
        RasterImageBgra32& outBgra);

    bool RenderInfiniteCanvasOffscreen(
        const DocumentModel& doc,
        const DocRect& worldRegion,
        int widthPx,
        int heightPx,
        RasterImageBgra32& outBgra);

    bool RenderHwndViewportToOffscreen(
        const DocumentModel& doc,
        const Camera2D& camera,
        int widthPx,
        int heightPx,
        PageRenderCache* pageCache,
        RasterImageBgra32& outBgra,
        const ViewportOverlayState* overlay = nullptr);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace DocStudio
