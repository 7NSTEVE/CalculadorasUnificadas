#pragma once
// ============================================================================
// ui_theme_linux.hpp
// Unified GTK3 + Cairo Design System, Asset Surface Loader & Settings
// Parity port of Win32 ui_theme.h (Porcelana Claro & Graphite Studio).
// ============================================================================

#include "../engines/unified_engines.hpp"

#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace CasioThemeLinux {

struct RgbaColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    constexpr RgbaColor() = default;
    constexpr RgbaColor(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    double rf() const noexcept { return r / 255.0; }
    double gf() const noexcept { return g / 255.0; }
    double bf() const noexcept { return b / 255.0; }
    double af() const noexcept { return a / 255.0; }

    std::string ToCssHex() const {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
        return std::string(buf);
    }
};

inline bool g_studioDarkTheme = false;

namespace Dark {
inline constexpr RgbaColor CanvasBg      {11, 14, 20};
inline constexpr RgbaColor StageBg       {14, 18, 26};
inline constexpr RgbaColor Surface1      {20, 26, 37};
inline constexpr RgbaColor Surface2      {28, 36, 51};
inline constexpr RgbaColor Surface3      {37, 47, 67};
inline constexpr RgbaColor Track         {28, 36, 51};
inline constexpr RgbaColor InputBg       {17, 23, 34};
inline constexpr RgbaColor BorderSubtle  {42, 53, 74};
inline constexpr RgbaColor BorderStrong  {64, 80, 110};
inline constexpr RgbaColor AccentBlue    {59, 130, 246};
inline constexpr RgbaColor AccentHover   {96, 165, 250};
inline constexpr RgbaColor AccentSoft    {24, 52, 92};
inline constexpr RgbaColor AccentText    {147, 197, 253};
inline constexpr RgbaColor SuccessGreen  {16, 185, 129};
inline constexpr RgbaColor SuccessSoft   {16, 58, 46};
inline constexpr RgbaColor SuccessText   {110, 231, 183};
inline constexpr RgbaColor AmberWarm     {245, 158, 11};
inline constexpr RgbaColor AmberSoft     {58, 40, 14};
inline constexpr RgbaColor AmberText     {252, 211, 77};
inline constexpr RgbaColor VioletAccent  {139, 92, 246};
inline constexpr RgbaColor VioletSoft    {46, 33, 84};
inline constexpr RgbaColor VioletText    {196, 181, 253};
inline constexpr RgbaColor RoseRed       {244, 63, 94};
inline constexpr RgbaColor RoseSoft      {64, 22, 34};
inline constexpr RgbaColor RoseText      {253, 164, 175};
inline constexpr RgbaColor TextPrimary   {241, 245, 249};
inline constexpr RgbaColor TextSecondary {148, 163, 184};
inline constexpr RgbaColor TextMuted     {100, 116, 139};
} // namespace Dark

namespace Light {
inline constexpr RgbaColor CanvasBg      {245, 247, 250};
inline constexpr RgbaColor StageBg       {239, 243, 248};
inline constexpr RgbaColor Surface1      {255, 255, 255};
inline constexpr RgbaColor Surface2      {247, 249, 252};
inline constexpr RgbaColor Surface3      {232, 238, 247};
inline constexpr RgbaColor Track         {231, 236, 243};
inline constexpr RgbaColor InputBg       {255, 255, 255};
inline constexpr RgbaColor BorderSubtle  {220, 226, 235};
inline constexpr RgbaColor BorderStrong  {184, 196, 214};
inline constexpr RgbaColor AccentBlue    {0, 122, 255};
inline constexpr RgbaColor AccentHover   {12, 112, 232};
inline constexpr RgbaColor AccentSoft    {232, 242, 255};
inline constexpr RgbaColor AccentText    {0, 96, 208};
inline constexpr RgbaColor SuccessGreen  {16, 163, 108};
inline constexpr RgbaColor SuccessSoft   {234, 248, 242};
inline constexpr RgbaColor SuccessText   {10, 118, 80};
inline constexpr RgbaColor AmberWarm     {210, 118, 12};
inline constexpr RgbaColor AmberSoft     {253, 244, 224};
inline constexpr RgbaColor AmberText     {154, 82, 10};
inline constexpr RgbaColor VioletAccent  {109, 74, 218};
inline constexpr RgbaColor VioletSoft    {240, 236, 255};
inline constexpr RgbaColor VioletText    {91, 56, 194};
inline constexpr RgbaColor RoseRed       {218, 36, 76};
inline constexpr RgbaColor RoseSoft      {254, 236, 240};
inline constexpr RgbaColor RoseText      {180, 24, 62};
inline constexpr RgbaColor TextPrimary   {20, 25, 34};
inline constexpr RgbaColor TextSecondary {90, 102, 120};
inline constexpr RgbaColor TextMuted     {138, 150, 168};
} // namespace Light

inline RgbaColor CanvasBg()      { return g_studioDarkTheme ? Dark::CanvasBg      : Light::CanvasBg; }
inline RgbaColor StageBg()       { return g_studioDarkTheme ? Dark::StageBg       : Light::StageBg; }
inline RgbaColor Surface1()      { return g_studioDarkTheme ? Dark::Surface1      : Light::Surface1; }
inline RgbaColor Surface2()      { return g_studioDarkTheme ? Dark::Surface2      : Light::Surface2; }
inline RgbaColor Surface3()      { return g_studioDarkTheme ? Dark::Surface3      : Light::Surface3; }
inline RgbaColor Track()         { return g_studioDarkTheme ? Dark::Track         : Light::Track; }
inline RgbaColor InputBg()       { return g_studioDarkTheme ? Dark::InputBg       : Light::InputBg; }
inline RgbaColor BorderSubtle()  { return g_studioDarkTheme ? Dark::BorderSubtle  : Light::BorderSubtle; }
inline RgbaColor BorderStrong()  { return g_studioDarkTheme ? Dark::BorderStrong  : Light::BorderStrong; }
inline RgbaColor Accent()        { return g_studioDarkTheme ? Dark::AccentBlue    : Light::AccentBlue; }
inline RgbaColor AccentHover()   { return g_studioDarkTheme ? Dark::AccentHover   : Light::AccentHover; }
inline RgbaColor AccentSoft()    { return g_studioDarkTheme ? Dark::AccentSoft    : Light::AccentSoft; }
inline RgbaColor AccentText()    { return g_studioDarkTheme ? Dark::AccentText    : Light::AccentText; }
inline RgbaColor Success()       { return g_studioDarkTheme ? Dark::SuccessGreen  : Light::SuccessGreen; }
inline RgbaColor SuccessSoft()   { return g_studioDarkTheme ? Dark::SuccessSoft   : Light::SuccessSoft; }
inline RgbaColor SuccessText()   { return g_studioDarkTheme ? Dark::SuccessText   : Light::SuccessText; }
inline RgbaColor Amber()         { return g_studioDarkTheme ? Dark::AmberWarm     : Light::AmberWarm; }
inline RgbaColor AmberSoft()     { return g_studioDarkTheme ? Dark::AmberSoft     : Light::AmberSoft; }
inline RgbaColor AmberText()     { return g_studioDarkTheme ? Dark::AmberText     : Light::AmberText; }
inline RgbaColor Violet()        { return g_studioDarkTheme ? Dark::VioletAccent  : Light::VioletAccent; }
inline RgbaColor VioletSoft()    { return g_studioDarkTheme ? Dark::VioletSoft    : Light::VioletSoft; }
inline RgbaColor VioletText()    { return g_studioDarkTheme ? Dark::VioletText    : Light::VioletText; }
inline RgbaColor Rose()          { return g_studioDarkTheme ? Dark::RoseRed       : Light::RoseRed; }
inline RgbaColor RoseSoft()      { return g_studioDarkTheme ? Dark::RoseSoft      : Light::RoseSoft; }
inline RgbaColor RoseText()      { return g_studioDarkTheme ? Dark::RoseText      : Light::RoseText; }
inline RgbaColor TextPrimary()   { return g_studioDarkTheme ? Dark::TextPrimary   : Light::TextPrimary; }
inline RgbaColor TextSecondary() { return g_studioDarkTheme ? Dark::TextSecondary : Light::TextSecondary; }
inline RgbaColor TextMuted()     { return g_studioDarkTheme ? Dark::TextMuted     : Light::TextMuted; }

inline RgbaColor GetLcdBgColor(int calcIdx) {
    switch (calcIdx) {
    case 0: return RgbaColor(213, 226, 213);
    case 1: return RgbaColor(214, 227, 214);
    case 2: return RgbaColor(255, 255, 255);
    default: return RgbaColor(207, 230, 219);
    }
}

inline RgbaColor GetLcdFgColor(int calcIdx) {
    switch (calcIdx) {
    case 0: return RgbaColor(28, 38, 56);
    case 1: return RgbaColor(25, 35, 50);
    case 2: return RgbaColor(15, 20, 30);
    default: return RgbaColor(20, 36, 58);
    }
}

inline void CairoSetSource(cairo_t* cr, RgbaColor c, double alphaMul = 1.0) {
    cairo_set_source_rgba(cr, c.rf(), c.gf(), c.bf(), c.af() * alphaMul);
}

inline void CairoRoundedRect(cairo_t* cr, double x, double y, double w, double h, double radius) {
    if (w <= 0.0 || h <= 0.0) return;
    double r = std::max(0.0, std::min(radius, std::min(w, h) * 0.5));
    if (r <= 0.01) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    constexpr double kPi = 3.14159265358979323846;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -0.5 * kPi, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, 0.5 * kPi);
    cairo_arc(cr, x + r, y + h - r, r, 0.5 * kPi, kPi);
    cairo_arc(cr, x + r, y + r, r, kPi, 1.5 * kPi);
    cairo_close_path(cr);
}

inline void FillRoundedRect(cairo_t* cr, double x, double y, double w, double h, double r,
                            RgbaColor fill, double alphaMul = 1.0) {
    CairoRoundedRect(cr, x, y, w, h, r);
    CairoSetSource(cr, fill, alphaMul);
    cairo_fill(cr);
}

inline void StrokeRoundedRect(cairo_t* cr, double x, double y, double w, double h, double r,
                              RgbaColor stroke, double lineWidth = 1.0, double alphaMul = 1.0) {
    CairoRoundedRect(cr, x, y, w, h, r);
    CairoSetSource(cr, stroke, alphaMul);
    cairo_set_line_width(cr, lineWidth);
    cairo_stroke(cr);
}

inline void DrawSoftShadow(cairo_t* cr, double x, double y, double w, double h, double r) {
    for (int step = 3; step >= 1; --step) {
        double pad = step * 3.0;
        double a = g_studioDarkTheme ? (0.14 / step) : (0.06 / step);
        CairoRoundedRect(cr, x - pad * 0.35, y + step * 2.0, w + pad * 0.7, h + pad * 0.4, r + step);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, a);
        cairo_fill(cr);
    }
}

struct CairoSurfacePtr {
    cairo_surface_t* ptr = nullptr;
    CairoSurfacePtr() = default;
    explicit CairoSurfacePtr(cairo_surface_t* p) : ptr(p) {}
    ~CairoSurfacePtr() { reset(); }
    CairoSurfacePtr(const CairoSurfacePtr&) = delete;
    CairoSurfacePtr& operator=(const CairoSurfacePtr&) = delete;
    CairoSurfacePtr(CairoSurfacePtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    CairoSurfacePtr& operator=(CairoSurfacePtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    void reset(cairo_surface_t* p = nullptr) {
        if (ptr) cairo_surface_destroy(ptr);
        ptr = p;
    }
    cairo_surface_t* get() const noexcept { return ptr; }
    explicit operator bool() const noexcept { return ptr != nullptr; }
};

inline cairo_surface_t* RgbaToCairoSurface(const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return nullptr;
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return nullptr;
    }
    cairo_surface_flush(surf);
    uint8_t* dst = cairo_image_surface_get_data(surf);
    const int stride = cairo_image_surface_get_stride(surf);
    for (int y = 0; y < h; ++y) {
        const uint8_t* srcRow = rgba + static_cast<size_t>(y) * w * 4;
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(dst + static_cast<size_t>(y) * stride);
        for (int x = 0; x < w; ++x) {
            uint8_t r = srcRow[x * 4 + 0];
            uint8_t g = srcRow[x * 4 + 1];
            uint8_t b = srcRow[x * 4 + 2];
            uint8_t a = srcRow[x * 4 + 3];
            if (a != 255) {
                r = static_cast<uint8_t>((static_cast<uint32_t>(r) * a + 127) / 255);
                g = static_cast<uint8_t>((static_cast<uint32_t>(g) * a + 127) / 255);
                b = static_cast<uint8_t>((static_cast<uint32_t>(b) * a + 127) / 255);
            }
            dstRow[x] = (static_cast<uint32_t>(a) << 24) |
                        (static_cast<uint32_t>(r) << 16) |
                        (static_cast<uint32_t>(g) << 8) |
                        static_cast<uint32_t>(b);
        }
    }
    cairo_surface_mark_dirty(surf);
    return surf;
}

inline GdkPixbuf* RgbaToGdkPixbuf(const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return nullptr;
    GdkPixbuf* pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    if (!pb) return nullptr;
    guchar* dst = gdk_pixbuf_get_pixels(pb);
    const int stride = gdk_pixbuf_get_rowstride(pb);
    for (int y = 0; y < h; ++y) {
        std::memcpy(dst + static_cast<size_t>(y) * stride,
                    rgba + static_cast<size_t>(y) * w * 4,
                    static_cast<size_t>(w) * 4);
    }
    return pb;
}

inline GdkPixbuf* CairoSurfaceToGdkPixbuf(cairo_surface_t* surf) {
    if (!surf) return nullptr;
    if (cairo_surface_get_type(surf) != CAIRO_SURFACE_TYPE_IMAGE) return nullptr;
    cairo_surface_flush(surf);
    const int w = cairo_image_surface_get_width(surf);
    const int h = cairo_image_surface_get_height(surf);
    const int srcStride = cairo_image_surface_get_stride(surf);
    const uint8_t* src = cairo_image_surface_get_data(surf);
    if (!src || w <= 0 || h <= 0) return nullptr;

    GdkPixbuf* pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    if (!pb) return nullptr;
    guchar* dst = gdk_pixbuf_get_pixels(pb);
    const int dstStride = gdk_pixbuf_get_rowstride(pb);
    for (int y = 0; y < h; ++y) {
        const uint32_t* sRow = reinterpret_cast<const uint32_t*>(src + static_cast<size_t>(y) * srcStride);
        guchar* dRow = dst + static_cast<size_t>(y) * dstStride;
        for (int x = 0; x < w; ++x) {
            uint32_t px = sRow[x];
            uint8_t a = static_cast<uint8_t>((px >> 24) & 0xFF);
            uint8_t r = static_cast<uint8_t>((px >> 16) & 0xFF);
            uint8_t g = static_cast<uint8_t>((px >> 8) & 0xFF);
            uint8_t b = static_cast<uint8_t>(px & 0xFF);
            if (a > 0 && a < 255) {
                r = static_cast<uint8_t>(std::min<int>(255, (r * 255) / a));
                g = static_cast<uint8_t>(std::min<int>(255, (g * 255) / a));
                b = static_cast<uint8_t>(std::min<int>(255, (b * 255) / a));
            }
            dRow[x * 4 + 0] = r;
            dRow[x * 4 + 1] = g;
            dRow[x * 4 + 2] = b;
            dRow[x * 4 + 3] = a;
        }
    }
    return pb;
}

inline bool SurfaceToPngFile(cairo_surface_t* surf, const std::string& path) {
    if (!surf || path.empty()) return false;
    std::error_code ec;
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    return cairo_surface_write_to_png(surf, path.c_str()) == CAIRO_STATUS_SUCCESS;
}

inline std::string ResolveSkinPath(CasioLinux::CalcModelId model, const CasioLinux::AssetPaths& paths) {
    namespace fs = std::filesystem;
    std::vector<std::string> candidates;
    auto addIf = [&](const fs::path& p) {
        if (!p.empty()) candidates.push_back(p.string());
    };
    fs::path skins(paths.skinsDir);
    fs::path root(paths.rootDir);
    fs::path cw2(paths.fx991cwDir);
    switch (model) {
    case CasioLinux::CalcModelId::BlancaClassWiz:
        addIf(skins / "caratula_classwiz.png");
        addIf(root / "skins" / "caratula_classwiz.png");
        addIf(root / "extracted_white" / "bitmap_103.bmp");
        break;
    case CasioLinux::CalcModelId::GrisEsPlus:
        addIf(skins / "caratula_esplus.png");
        addIf(root / "skins" / "caratula_esplus.png");
        addIf(root / "extracted_gray" / "bitmap_3001.bmp");
        break;
    case CasioLinux::CalcModelId::Cg50Prizm:
        addIf(skins / "caratula_cg50.png");
        addIf(root / "skins" / "caratula_cg50.png");
        addIf(root / "extracted_cg50" / "full_cg50_skin.bmp");
        break;
    case CasioLinux::CalcModelId::NegraCw2:
        addIf(skins / "interface.png");
        addIf(cw2 / "models" / "fx991cw_emu" / "interface.png");
        addIf(root / "runtime" / "fx991cw" / "models" / "fx991cw_emu" / "interface.png");
        addIf(root / "skins" / "interface.png");
        break;
    }
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

// Loads a skin image via GdkPixbuf, optional crops to (cropW, cropH), removes uniform
// exterior corner background via BFS flood-fill with 1px edge smoothing (port of ui_theme.h).
inline cairo_surface_t* LoadSkinSurfaceClean(const std::string& filePath,
                                             int cropW, int cropH,
                                             int& outW, int& outH) {
    outW = 0;
    outH = 0;
    if (filePath.empty()) return nullptr;
    GError* err = nullptr;
    GdkPixbuf* pb = gdk_pixbuf_new_from_file(filePath.c_str(), &err);
    if (err) g_error_free(err);
    if (!pb) return nullptr;

    if (!gdk_pixbuf_get_has_alpha(pb)) {
        GdkPixbuf* withAlpha = gdk_pixbuf_add_alpha(pb, FALSE, 0, 0, 0);
        g_object_unref(pb);
        pb = withAlpha;
    }
    if (!pb) return nullptr;

    const int rawW = gdk_pixbuf_get_width(pb);
    const int rawH = gdk_pixbuf_get_height(pb);
    const int w = (cropW > 0 && cropW <= rawW) ? cropW : rawW;
    const int h = (cropH > 0 && cropH <= rawH) ? cropH : rawH;
    const int srcStride = gdk_pixbuf_get_rowstride(pb);
    const guchar* pixels = gdk_pixbuf_get_pixels(pb);
    const int nChan = gdk_pixbuf_get_n_channels(pb);

    if (w <= 8 || h <= 8 || nChan < 4) {
        g_object_unref(pb);
        return nullptr;
    }

    auto srcAt = [&](int x, int y, int ch) -> uint8_t {
        return pixels[static_cast<size_t>(y) * srcStride + static_cast<size_t>(x) * nChan + ch];
    };

    // Check if image already carries meaningful alpha around corners
    const bool cornerAlreadyTransparent = (srcAt(0, 0, 3) < 200) && (srcAt(w - 1, 0, 3) < 200);

    std::vector<uint8_t> mask(static_cast<size_t>(w) * h, 0);
    if (!cornerAlreadyTransparent) {
        const int sR = srcAt(2, 2, 0);
        const int sG = srcAt(2, 2, 1);
        const int sB = srcAt(2, 2, 2);
        std::deque<int> q;
        const int seeds[4][2] = {{0, 0}, {w - 1, 0}, {0, h - 1}, {w - 1, h - 1}};
        for (const auto& sd : seeds) {
            int idx = sd[1] * w + sd[0];
            if (!mask[idx]) {
                mask[idx] = 1;
                q.push_back(idx);
            }
        }
        constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        while (!q.empty()) {
            int cur = q.front();
            q.pop_front();
            int x = cur % w;
            int y = cur / w;
            for (const auto& d : dirs) {
                int nx = x + d[0], ny = y + d[1];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                size_t nIdx = static_cast<size_t>(ny) * w + nx;
                if (mask[nIdx]) continue;
                if (std::abs(static_cast<int>(srcAt(nx, ny, 0)) - sR) <= 14 &&
                    std::abs(static_cast<int>(srcAt(nx, ny, 1)) - sG) <= 14 &&
                    std::abs(static_cast<int>(srcAt(nx, ny, 2)) - sB) <= 14) {
                    mask[nIdx] = 1;
                    q.push_back(static_cast<int>(nIdx));
                }
            }
        }
    }

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        g_object_unref(pb);
        return nullptr;
    }

    cairo_surface_flush(surf);
    uint8_t* dstData = cairo_image_surface_get_data(surf);
    const int dstStride = cairo_image_surface_get_stride(surf);
    constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int y = 0; y < h; ++y) {
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(dstData + static_cast<size_t>(y) * dstStride);
        for (int x = 0; x < w; ++x) {
            size_t pIdx = static_cast<size_t>(y) * w + x;
            if (mask[pIdx]) {
                dstRow[x] = 0;
                continue;
            }
            uint8_t r = srcAt(x, y, 0);
            uint8_t g = srcAt(x, y, 1);
            uint8_t b = srcAt(x, y, 2);
            uint8_t a = srcAt(x, y, 3);
            if (!cornerAlreadyTransparent) {
                int adjZero = 0;
                for (const auto& d : dirs) {
                    int nx = x + d[0], ny = y + d[1];
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                        mask[static_cast<size_t>(ny) * w + nx]) {
                        adjZero++;
                    }
                }
                if (adjZero > 0) a = (adjZero >= 2) ? 140 : 205;
            }
            if (a != 255) {
                r = static_cast<uint8_t>((static_cast<uint32_t>(r) * a + 127) / 255);
                g = static_cast<uint8_t>((static_cast<uint32_t>(g) * a + 127) / 255);
                b = static_cast<uint8_t>((static_cast<uint32_t>(b) * a + 127) / 255);
            }
            dstRow[x] = (static_cast<uint32_t>(a) << 24) |
                        (static_cast<uint32_t>(r) << 16) |
                        (static_cast<uint32_t>(g) << 8) |
                        static_cast<uint32_t>(b);
        }
    }
    cairo_surface_mark_dirty(surf);
    g_object_unref(pb);
    outW = w;
    outH = h;
    return surf;
}

struct KeyCaptureSpecLinux {
    CasioLinux::SkinRect bounds{};
    CasioLinux::SkinRect body{};
    CasioLinux::SkinRect legend{};
    int mask = 0; // 0: rect, 1: body+legend, 2: neutral D-pad ellipse
};

inline KeyCaptureSpecLinux GetKeyCaptureSpec(int model, const CasioLinux::KeyDef& k, int bw, int bh) {
    KeyCaptureSpecLinux s{};
    const int id = k.id;
    const int cx = k.cx;
    if (model == 2) {
        if (id <= 5) s.bounds = {cx - 44, 503, cx + 44, 590};
        else if (id <= 9) s.bounds = {cx - 43, 600, cx + 43, 668};
        else if (id <= 13) s.bounds = {430, 593, 595, 753};
        else if (id <= 17) s.bounds = {cx - 43, 670, cx + 43, 737};
        else if (id <= 23) s.bounds = {cx - 42, 750, cx + 42, 821};
        else if (id <= 29) s.bounds = {cx - 42, 821, cx + 42, 893};
        else if (id <= 34) s.bounds = {cx - 50, 907, cx + 50, 996};
        else if (id <= 39) s.bounds = {cx - 50, 999, cx + 50, 1082};
        else if (id <= 44) s.bounds = {cx - 50, 1085, cx + 50, 1169};
        else s.bounds = {cx - 50, 1172, cx + 50, 1256};
    } else if (model == 0) {
        if (id == 0) s.bounds = {90, 450, 157, 537};
        else if (id == 1) s.bounds = {158, 450, 226, 537};
        else if (id >= 2 && id <= 5) { s.bounds = {221, 463, 424, 607}; s.mask = 2; }
        else if (id == 6) { s.bounds = {405, 449, 498, 537}; s.body = {418, 473, 483, 537}; s.legend = {405, 449, 498, 473}; s.mask = 1; }
        else if (id == 7) { s.bounds = {484, 449, 556, 537}; s.body = {484, 473, 556, 537}; s.legend = {505, 449, 541, 473}; s.mask = 1; }
        else if (id == 8) s.bounds = {89, 550, 165, 618};
        else if (id == 9) { s.bounds = {167, 550, 243, 618}; s.body = {169, 574, 242, 618}; s.legend = {169, 550, 241, 574}; s.mask = 1; }
        else if (id == 10) { s.bounds = {400, 545, 478, 618}; s.body = {404, 573, 477, 618}; s.legend = {417, 545, 475, 574}; s.mask = 1; }
        else if (id == 11) s.bounds = {479, 545, 555, 618};
        else if (id <= 17) s.bounds = {cx - 38, 619, cx + 38, 684};
        else if (id <= 23) s.bounds = {cx - 38, 684, cx + 38, 747};
        else if (id <= 29) s.bounds = {cx - 38, 748, cx + 38, 815};
        else if (id <= 34) s.bounds = {cx - 45, 819, cx + 45, 901};
        else if (id <= 39) s.bounds = {cx - 45, 903, cx + 45, 981};
        else if (id <= 44) s.bounds = {cx - 45, 983, cx + 45, 1062};
        else s.bounds = {cx - 45, 1063, cx + 45, 1142};
    } else if (model == 3) {
        if (id == 0) s.bounds = {72, 714, 192, 850};
        else if (id == 1) s.bounds = {218, 714, 338, 852};
        else if (id == 2 || id == 6 || id == 7 || id == 8 || id == 10) s.bounds = {366, 738, 776, 1118};
        else if (id == 3 || id == 9) s.bounds = {800, 740, 924, 998};
        else if (id == 4) s.bounds = {64, 856, 200, 996};
        else if (id == 5) s.bounds = {218, 882, 338, 996};
        else if (id <= 15) s.bounds = {cx - 71, 1002, cx + 71, 1142};
        else if (id <= 21) s.bounds = {cx - 74, 1143, cx + 68, 1272};
        else if (id <= 27) s.bounds = {(id == 22 ? 50 : cx - 74), 1274, cx + 66, 1400};
        else if (id <= 32) s.bounds = {cx - 84, 1404, cx + 78, 1558};
        else if (id <= 37) s.bounds = {cx - 84, 1559, cx + 78, 1707};
        else if (id <= 42) s.bounds = {cx - 84, 1708, cx + 78, 1857};
        else s.bounds = {cx - 84, 1858, cx + 78, 2008};
    } else {
        if (id == 0) { s.bounds = {36, 274, 96, 338}; s.body = {39, 292, 94, 338}; s.legend = {47, 274, 83, 292}; s.mask = 1; }
        else if (id == 1) { s.bounds = {310, 274, 371, 338}; s.body = {311, 291, 370, 338}; s.legend = {330, 274, 353, 291}; s.mask = 1; }
        else if (id == 2 || id == 5 || id == 6 || id == 7) s.bounds = {148, 289, 256, 387};
        else if (id == 3) { s.bounds = {90, 282, 149, 347}; s.body = {93, 300, 148, 347}; s.legend = {92, 282, 146, 300}; s.mask = 1; }
        else if (id == 4) { s.bounds = {251, 283, 320, 346}; s.body = {258, 300, 319, 346}; s.legend = {251, 283, 317, 300}; s.mask = 1; }
        else if (id <= 11) s.bounds = {cx - 27, 347, cx + 27, 398};
        else if (id <= 17) s.bounds = {cx - 27, 398, cx + 27, 444};
        else if (id <= 23) s.bounds = {cx - 27, 444, cx + 27, 487};
        else if (id <= 29) s.bounds = {cx - 27, 487, cx + 27, 536};
        else if (id <= 34) s.bounds = {cx - 32, 536, cx + 32, 594};
        else if (id <= 39) s.bounds = {cx - 32, 594, cx + 32, 650};
        else if (id <= 44) s.bounds = {cx - 32, 650, cx + 32, 707};
        else s.bounds = {cx - 32, 707, cx + 32, 767};
    }
    s.bounds.left = std::max(0, s.bounds.left);
    s.bounds.top = std::max(0, s.bounds.top);
    s.bounds.right = std::min(bw, s.bounds.right);
    s.bounds.bottom = std::min(bh, s.bounds.bottom);
    return s;
}

inline cairo_surface_t* CropKeySurfaceFromSkin(cairo_surface_t* skinSurf, int model, const CasioLinux::KeyDef& kd) {
    if (!skinSurf) return nullptr;
    cairo_surface_flush(skinSurf);
    const int bw = cairo_image_surface_get_width(skinSurf);
    const int bh = cairo_image_surface_get_height(skinSurf);
    const int srcStride = cairo_image_surface_get_stride(skinSurf);
    const uint8_t* srcData = cairo_image_surface_get_data(skinSurf);
    if (!srcData || bw <= 0 || bh <= 0) return nullptr;

    const auto spec = GetKeyCaptureSpec(model, kd, bw, bh);
    const int w = spec.bounds.Width();
    const int h = spec.bounds.Height();
    if (w <= 0 || h <= 0) return nullptr;

    cairo_surface_t* out = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(out) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(out);
        return nullptr;
    }
    cairo_surface_flush(out);
    uint8_t* dstData = cairo_image_surface_get_data(out);
    const int dstStride = cairo_image_surface_get_stride(out);

    const uint32_t bgPx = *reinterpret_cast<const uint32_t*>(
        srcData + static_cast<size_t>(spec.bounds.top) * srcStride + static_cast<size_t>(spec.bounds.left) * 4);
    const unsigned bgR = (bgPx >> 16) & 0xFFu;
    const unsigned bgG = (bgPx >> 8) & 0xFFu;
    const unsigned bgB = bgPx & 0xFFu;

    std::vector<uint8_t> bodyMask;
    if (model == 1 && spec.mask == 1) {
        bodyMask.resize(static_cast<size_t>(w) * h, 0);
        const int seedX = (spec.body.left + spec.body.right) / 2 - spec.bounds.left;
        const int seedY = (spec.body.top + spec.body.bottom) / 2 - spec.bounds.top;
        if (seedX >= 0 && seedX < w && seedY >= 0 && seedY < h) {
            std::vector<int> frontier;
            frontier.push_back(seedY * w + seedX);
            bodyMask[frontier[0]] = 1;
            for (size_t idx = 0; idx < frontier.size(); ++idx) {
                const int px = frontier[idx] % w;
                const int py = frontier[idx] / w;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int xx = px + dx, yy = py + dy;
                        const int gx = spec.bounds.left + xx, gy = spec.bounds.top + yy;
                        if (xx < 0 || xx >= w || yy < 0 || yy >= h ||
                            gx < spec.body.left || gx >= spec.body.right ||
                            gy < spec.body.top || gy >= spec.body.bottom) continue;
                        const int ni = yy * w + xx;
                        if (bodyMask[ni]) continue;
                        const uint32_t c = *reinterpret_cast<const uint32_t*>(
                            srcData + static_cast<size_t>(gy) * srcStride + static_cast<size_t>(gx) * 4);
                        if (((c >> 16) & 0xFFu) < 185 || ((c >> 8) & 0xFFu) < 185 || (c & 0xFFu) < 185) {
                            bodyMask[ni] = 1;
                            frontier.push_back(ni);
                        }
                    }
                }
            }
            auto expanded = bodyMask;
            for (int f : frontier) {
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        int xx = (f % w) + dx, yy = (f / w) + dy;
                        int gx = spec.bounds.left + xx, gy = spec.bounds.top + yy;
                        if (xx >= 0 && xx < w && yy >= 0 && yy < h &&
                            gx >= spec.body.left && gx < spec.body.right &&
                            gy >= spec.body.top && gy < spec.body.bottom) {
                            expanded[static_cast<size_t>(yy) * w + xx] = 1;
                        }
                    }
                }
            }
            bodyMask.swap(expanded);
        }
    }

    for (int y = 0; y < h; ++y) {
        const int gy = spec.bounds.top + y;
        const uint32_t* sRow = reinterpret_cast<const uint32_t*>(srcData + static_cast<size_t>(gy) * srcStride);
        uint32_t* dRow = reinterpret_cast<uint32_t*>(dstData + static_cast<size_t>(y) * dstStride);
        for (int x = 0; x < w; ++x) {
            const int gx = spec.bounds.left + x;
            uint32_t p = sRow[gx];
            bool keep = true;
            if (spec.mask == 1) {
                const bool body = bodyMask.empty()
                    ? (gx >= spec.body.left && gx < spec.body.right && gy >= spec.body.top && gy < spec.body.bottom)
                    : (bodyMask[static_cast<size_t>(y) * w + x] != 0);
                bool legend = (gx >= spec.legend.left && gx < spec.legend.right &&
                               gy >= spec.legend.top && gy < spec.legend.bottom);
                if (model == 0 && legend) {
                    legend = (((p >> 16) & 0xFFu) > bgR + 2u) ||
                             (((p >> 8) & 0xFFu) > bgG + 2u) ||
                             ((p & 0xFFu) > bgB + 2u);
                }
                keep = body || legend;
            } else if (spec.mask == 2) {
                const double dx = (gx - 322.0) / 100.5;
                const double dy = (gy - 535.0) / 72.0;
                const unsigned r = (p >> 16) & 0xFFu, g = (p >> 8) & 0xFFu, b = p & 0xFFu;
                const unsigned hi = std::max(r, std::max(g, b));
                const unsigned lo = std::min(r, std::min(g, b));
                keep = (dx * dx + dy * dy <= 1.0) && (hi - lo < 12);
            }
            dRow[x] = keep ? p : 0u;
        }
    }
    cairo_surface_mark_dirty(out);
    return out;
}

struct LcdSubRects {
    double indX = 0, indY = 0, indW = 0, indH = 0;
    double dotX = 0, dotY = 0, dotW = 0, dotH = 0;
};

inline LcdSubRects ComputeLcdIndAndDotRects(int calcIdx, double x, double y, double dstLcdW, double dstLcdH) {
    LcdSubRects out{};
    const int nativeIndH   = (calcIdx == 1) ? 12 : 8;
    const int nativePadTop = (calcIdx == 1) ? 2  : 1;
    const int nativeGapY   = (calcIdx == 1) ? 3  : 2;
    const int nativeTotalH = (calcIdx == 1) ? 111 : 75;
    const int rows         = (calcIdx == 1) ? 31 : 63;

    double padTop = dstLcdH * (static_cast<double>(nativePadTop) / nativeTotalH);
    double indH   = std::max(static_cast<double>(nativeIndH), dstLcdH * (static_cast<double>(nativeIndH) / nativeTotalH));
    double gapY   = std::max(1.0, dstLcdH * (static_cast<double>(nativeGapY) / nativeTotalH));
    double padBot = dstLcdH * (1.0 / nativeTotalH);
    if (padTop + indH + gapY + rows > dstLcdH) {
        padTop = std::max(0.0, dstLcdH / nativeTotalH);
        indH   = std::max(6.0, dstLcdH * (static_cast<double>(nativeIndH) / nativeTotalH));
        gapY   = std::max(1.0, dstLcdH * (static_cast<double>(nativeGapY) / nativeTotalH));
        padBot = 0.0;
    }
    out.indX = x;
    out.indY = y + padTop;
    out.indW = dstLcdW;
    out.indH = indH;
    out.dotX = x;
    out.dotY = out.indY + out.indH + gapY;
    out.dotW = dstLcdW;
    out.dotH = std::max(1.0, dstLcdH - (padTop + indH + gapY + padBot));
    return out;
}

// Application settings persisted in ~/.config/CalculadoraUnificada/config_unificada.ini
enum class ViewMode : int {
    Blanca     = 0,
    Gris       = 1,
    Cg50       = 2,
    Cw2        = 3,
    BlancaGris = 4,
    Todas      = 5
};

struct AppSettings {
    bool darkTheme = false; // Porcelana Claro by default (matches ui_theme.h)
    bool compactMode = false;
    bool drawerOpen = false;
    bool alwaysOnTop = false;
    bool teacherHighlight = true;
    int clipboardMode = 2; // 0: image, 1: text, 2: both
    ViewMode viewMode = ViewMode::Cw2;
    int activeCalcIdx = 3;
};

inline std::string GetSettingsFilePath() {
    namespace fs = std::filesystem;
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    fs::path dir = (xdg && xdg[0]) ? fs::path(xdg) : (fs::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config");
    dir /= "CalculadoraUnificada";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "config_unificada.ini").string();
}

inline void LoadUnifiedSettings(AppSettings& s) {
    std::ifstream in(GetSettingsFilePath());
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        auto toInt = [&](int def) {
            try { return std::stoi(v); } catch (...) { return def; }
        };
        if (k == "darkTheme") s.darkTheme = (toInt(0) != 0);
        else if (k == "compactMode") s.compactMode = (toInt(0) != 0);
        else if (k == "drawerOpen") s.drawerOpen = (toInt(0) != 0);
        else if (k == "alwaysOnTop") s.alwaysOnTop = (toInt(0) != 0);
        else if (k == "teacherHighlight") s.teacherHighlight = (toInt(1) != 0);
        else if (k == "clipboardMode") s.clipboardMode = std::clamp(toInt(2), 0, 2);
        else if (k == "viewMode") s.viewMode = static_cast<ViewMode>(std::clamp(toInt(3), 0, 5));
        else if (k == "activeCalcIdx") s.activeCalcIdx = std::clamp(toInt(3), 0, 3);
    }
    g_studioDarkTheme = s.darkTheme;
}

inline void SaveUnifiedSettings(const AppSettings& s) {
    std::ofstream out(GetSettingsFilePath());
    if (!out.is_open()) return;
    out << "darkTheme=" << (s.darkTheme ? 1 : 0) << "\n";
    out << "compactMode=" << (s.compactMode ? 1 : 0) << "\n";
    out << "drawerOpen=" << (s.drawerOpen ? 1 : 0) << "\n";
    out << "alwaysOnTop=" << (s.alwaysOnTop ? 1 : 0) << "\n";
    out << "teacherHighlight=" << (s.teacherHighlight ? 1 : 0) << "\n";
    out << "clipboardMode=" << s.clipboardMode << "\n";
    out << "viewMode=" << static_cast<int>(s.viewMode) << "\n";
    out << "activeCalcIdx=" << s.activeCalcIdx << "\n";
}

inline GtkCssProvider* g_themeCssProvider = nullptr;

inline void ApplyGtkThemeCss(bool dark) {
    g_studioDarkTheme = dark;
    std::ostringstream css;
    const std::string bg     = CanvasBg().ToCssHex();
    const std::string stage  = StageBg().ToCssHex();
    const std::string surf1  = Surface1().ToCssHex();
    const std::string surf2  = Surface2().ToCssHex();
    const std::string surf3  = Surface3().ToCssHex();
    const std::string input  = InputBg().ToCssHex();
    const std::string border = BorderSubtle().ToCssHex();
    const std::string bStr   = BorderStrong().ToCssHex();
    const std::string txt    = TextPrimary().ToCssHex();
    const std::string txtSec = TextSecondary().ToCssHex();
    const std::string acc    = Accent().ToCssHex();
    const std::string accSof = AccentSoft().ToCssHex();
    const std::string accTxt = AccentText().ToCssHex();
    const std::string suc    = Success().ToCssHex();
    const std::string vio    = Violet().ToCssHex();

    css << "window, dialog { background-color: " << bg << "; color: " << txt << "; }\n"
        << "flowbox, flowboxchild { padding: 0; margin: 0; background: transparent; border: none; outline: none; }\n"
        << ".studio-header { background-color: " << surf1 << "; border-bottom: 1px solid " << border << "; padding: 6px 10px; }\n"
        << ".studio-panel { background-color: " << surf1 << "; border: 1px solid " << border << "; border-radius: 10px; }\n"
        << ".studio-card { background-color: " << surf2 << "; border: 1px solid " << border << "; border-radius: 8px; padding: 8px; }\n"
        << ".studio-stage { background-color: " << stage << "; }\n"
        << ".studio-badge { background-color: " << accSof << "; color: " << accTxt << "; border-radius: 6px; padding: 2px 8px; font-weight: 600; }\n"
        << "entry, spinbutton, textview, combobox button { background-color: " << input << "; color: " << txt << "; border: 1px solid " << bStr << "; border-radius: 6px; }\n"
        << "button { background-image: none; background-color: " << surf2 << "; color: " << txt << "; border: 1px solid " << border << "; border-radius: 7px; padding: 4px 10px; }\n"
        << "button:hover { background-color: " << surf3 << "; border-color: " << bStr << "; }\n"
        << "button.pill-active, togglebutton:checked { background-color: " << acc << "; color: #ffffff; border-color: " << acc << "; font-weight: 600; }\n"
        << "button.accent-btn { background-color: " << acc << "; color: #ffffff; border-color: " << acc << "; font-weight: 600; }\n"
        << "button.success-btn { background-color: " << suc << "; color: #ffffff; border-color: " << suc << "; font-weight: 600; }\n"
        << "button.violet-btn { background-color: " << vio << "; color: #ffffff; border-color: " << vio << "; font-weight: 600; }\n"
        << ".muted-label { color: " << txtSec << "; }\n"
        << "notebook > header { background-color: " << surf1 << "; border-bottom: 1px solid " << border << "; }\n"
        << "notebook > header tab { color: " << txtSec << "; padding: 6px 12px; }\n"
        << "notebook > header tab:checked { color: " << accTxt << "; border-bottom: 2px solid " << acc << "; font-weight: 600; }\n"
        << "treeview.view { background-color: " << surf1 << "; color: " << txt << "; }\n"
        << "treeview.view:selected { background-color: " << acc << "; color: #ffffff; }\n";

    if (!g_themeCssProvider) {
        g_themeCssProvider = gtk_css_provider_new();
        if (GdkScreen* screen = gdk_screen_get_default()) {
            gtk_style_context_add_provider_for_screen(
                screen,
                GTK_STYLE_PROVIDER(g_themeCssProvider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }
    }
    const std::string str = css.str();
    gtk_css_provider_load_from_data(g_themeCssProvider, str.c_str(), static_cast<gssize>(str.size()), nullptr);
}

} // namespace CasioThemeLinux
