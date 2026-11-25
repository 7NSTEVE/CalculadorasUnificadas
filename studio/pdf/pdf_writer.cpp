#include "pdf_writer.hpp"
#include "../render/card_render_common.hpp"
#include "../render/math_box_typesetter.hpp"
#include "../core/utf_conv.hpp"

#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#else
#if !defined(__ANDROID__) && !defined(__APPLE__) && defined(__has_include)
#if __has_include(<cairo.h>) && __has_include(<gdk-pixbuf/gdk-pixbuf.h>) && __has_include(<pango/pangocairo.h>)
#define DOCSTUDIO_HAS_CAIRO_PANGO 1
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pango/pangocairo.h>
#endif
#endif
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace DocStudio {

namespace {

#ifdef _WIN32
using Microsoft::WRL::ComPtr;
#endif

std::string FmtNum(double v) {
    if (std::abs(v) < 1e-6) return "0";
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    std::string s(buf);
    while (s.find('.') != std::string::npos && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s.empty() ? "0" : s;
}

std::string EscapePdfLiteralString(const std::string& asciiOrWinAnsi) {
    std::string out = "(";
    for (unsigned char ch : asciiOrWinAnsi) {
        if (ch == '(' || ch == ')' || ch == '\\') {
            out.push_back('\\');
            out.push_back(static_cast<char>(ch));
        } else if (ch >= 0x20 && ch <= 0x7Eu) {
            out.push_back(static_cast<char>(ch));
        } else {
            char oct[8]{};
            std::snprintf(oct, sizeof(oct), "\\%03o", ch);
            out += oct;
        }
    }
    out.push_back(')');
    return out;
}

// Decodes next UTF-8 codepoint advancing pos safely.
std::uint32_t DecodeUtf8Next(const std::string& utf8, std::size_t& pos) {
    if (pos >= utf8.size()) return 0;
    const std::uint8_t b0 = static_cast<std::uint8_t>(utf8[pos]);
    if ((b0 & 0x80u) == 0u) {
        pos += 1;
        return b0;
    }
    if ((b0 & 0xE0u) == 0xC0u && pos + 1 < utf8.size()) {
        const std::uint8_t b1 = static_cast<std::uint8_t>(utf8[pos + 1]);
        if ((b1 & 0xC0u) == 0x80u) {
            pos += 2;
            return ((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
        }
    }
    if ((b0 & 0xF0u) == 0xE0u && pos + 2 < utf8.size()) {
        const std::uint8_t b1 = static_cast<std::uint8_t>(utf8[pos + 1]);
        const std::uint8_t b2 = static_cast<std::uint8_t>(utf8[pos + 2]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            pos += 3;
            return ((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
        }
    }
    if ((b0 & 0xF8u) == 0xF0u && pos + 3 < utf8.size()) {
        const std::uint8_t b1 = static_cast<std::uint8_t>(utf8[pos + 1]);
        const std::uint8_t b2 = static_cast<std::uint8_t>(utf8[pos + 2]);
        const std::uint8_t b3 = static_cast<std::uint8_t>(utf8[pos + 3]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            pos += 4;
            return ((b0 & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) | ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
        }
    }
    pos += 1;
    return '?';
}

bool TryEncodeWinAnsiCodepoint(std::uint32_t cp, std::uint8_t& outByte) noexcept {
    if (cp >= 0x20u && cp <= 0x7Eu) {
        outByte = static_cast<std::uint8_t>(cp);
        return true;
    }
    if (cp >= 0xA0u && cp <= 0xFFu) {
        outByte = static_cast<std::uint8_t>(cp);
        return true;
    }
    switch (cp) {
        case 0x20ACu: outByte = 0x80u; return true;
        case 0x201Au: outByte = 0x82u; return true;
        case 0x0192u: outByte = 0x83u; return true;
        case 0x201Eu: outByte = 0x84u; return true;
        case 0x2026u: outByte = 0x85u; return true;
        case 0x2020u: outByte = 0x86u; return true;
        case 0x2021u: outByte = 0x87u; return true;
        case 0x02C6u: outByte = 0x88u; return true;
        case 0x2030u: outByte = 0x89u; return true;
        case 0x0160u: outByte = 0x8Au; return true;
        case 0x2039u: outByte = 0x8Bu; return true;
        case 0x0152u: outByte = 0x8Cu; return true;
        case 0x017Du: outByte = 0x8Eu; return true;
        case 0x2018u: outByte = 0x91u; return true;
        case 0x2019u: outByte = 0x92u; return true;
        case 0x201Cu: outByte = 0x93u; return true;
        case 0x201Du: outByte = 0x94u; return true;
        case 0x2022u: outByte = 0x95u; return true;
        case 0x2013u: outByte = 0x96u; return true;
        case 0x2014u: outByte = 0x97u; return true;
        case 0x02DCu: outByte = 0x98u; return true;
        case 0x2122u: outByte = 0x99u; return true;
        case 0x0161u: outByte = 0x9Au; return true;
        case 0x203Au: outByte = 0x9Bu; return true;
        case 0x0153u: outByte = 0x9Cu; return true;
        case 0x017Eu: outByte = 0x9Eu; return true;
        case 0x0178u: outByte = 0x9Fu; return true;
        default: return false;
    }
}

bool IsPureWinAnsiUtf8(const std::string& utf8, std::string& outWinAnsi) {
    outWinAnsi.clear();
    outWinAnsi.reserve(utf8.size());
    std::size_t pos = 0;
    while (pos < utf8.size()) {
        const std::uint32_t cp = DecodeUtf8Next(utf8, pos);
        if (cp == '\t' || cp == '\r') {
            outWinAnsi.push_back(' ');
            continue;
        }
        std::uint8_t b = 0;
        if (!TryEncodeWinAnsiCodepoint(cp, b)) return false;
        outWinAnsi.push_back(static_cast<char>(b));
    }
    return true;
}

double HelvWidth1000(std::uint8_t ch, bool bold) noexcept {
    static constexpr int kHelvRegAscii[96] = {
        278,278,355,556,556,889,667,191,333,333,389,584,278,333,278,278,
        556,556,556,556,556,556,556,556,556,556,278,278,584,584,584,556,
        1015,667,667,722,722,667,611,778,722,278,500,667,556,833,722,778,
        667,778,722,667,611,722,667,944,667,667,611,278,278,278,469,556,
        333,556,556,500,556,556,278,556,556,222,222,500,222,833,556,556,
        556,556,333,500,278,556,500,722,500,500,500,334,260,334,584,278
    };
    static constexpr int kHelvBoldAscii[96] = {
        278,333,474,556,556,889,722,238,333,333,389,584,278,333,278,278,
        556,556,556,556,556,556,556,556,556,556,333,333,584,584,584,611,
        975,722,722,722,722,667,611,778,722,278,556,722,611,833,722,778,
        667,778,722,667,611,722,667,944,667,667,611,333,278,333,584,556,
        333,556,611,556,611,556,333,611,611,278,278,556,278,889,611,611,
        611,611,389,556,333,611,556,778,556,556,500,389,280,389,584,278
    };
    if (ch >= 0x20u && ch <= 0x7Fu) {
        return static_cast<double>(bold ? kHelvBoldAscii[ch - 0x20u] : kHelvRegAscii[ch - 0x20u]);
    }
    return 580.0;
}

std::string EncodeCpUtf8(std::uint32_t cp) {
    std::string out;
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
    return out;
}

std::string FormatUtf16BeHex(std::uint32_t cp) {
    char buf[16]{};
    if (cp <= 0xFFFFu) {
        std::snprintf(buf, sizeof(buf), "%04X", static_cast<unsigned>(cp));
    } else {
        const std::uint32_t u = cp - 0x10000u;
        const std::uint16_t hi = static_cast<std::uint16_t>(0xD800u | (u >> 10));
        const std::uint16_t lo = static_cast<std::uint16_t>(0xDC00u | (u & 0x3FFu));
        std::snprintf(buf, sizeof(buf), "%04X%04X", hi, lo);
    }
    return std::string(buf);
}

// Converts UTF-8 to WinAnsi bytes for built-in PDF Helvetica font
std::string Utf8ToWinAnsi(const std::string& utf8) {
    std::string out;
    out.reserve(utf8.size());
    std::size_t pos = 0;
    while (pos < utf8.size()) {
        const std::uint32_t cp = DecodeUtf8Next(utf8, pos);
        std::uint8_t b = 0;
        if (TryEncodeWinAnsiCodepoint(cp, b)) {
            out.push_back(static_cast<char>(b));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

struct ExtractedGlyphOutline {
    double advance1000 = 600.0;
    std::string charProcOps;
};

#ifdef _WIN32
class PdfDWriteGeometrySink final : public ID2D1SimplifiedGeometrySink {
public:
    explicit PdfDWriteGeometrySink(std::ostringstream& ss) : ss_(ss) {}
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(ID2D1SimplifiedGeometrySink)) {
            *ppv = static_cast<ID2D1SimplifiedGeometrySink*>(this);
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return 1; }
    IFACEMETHODIMP_(ULONG) Release() override { return 1; }
    IFACEMETHODIMP_(void) SetFillMode(D2D1_FILL_MODE) override {}
    IFACEMETHODIMP_(void) SetSegmentFlags(D2D1_PATH_SEGMENT) override {}
    IFACEMETHODIMP_(void) BeginFigure(D2D1_POINT_2F startPoint, D2D1_FIGURE_BEGIN) override {
        ss_ << FmtNum(startPoint.x) << ' ' << FmtNum(-startPoint.y) << " m\n";
    }
    IFACEMETHODIMP_(void) AddLines(const D2D1_POINT_2F* points, UINT32 pointsCount) override {
        for (UINT32 i = 0; i < pointsCount; ++i) {
            ss_ << FmtNum(points[i].x) << ' ' << FmtNum(-points[i].y) << " l\n";
        }
    }
    IFACEMETHODIMP_(void) AddBeziers(const D2D1_BEZIER_SEGMENT* beziers, UINT32 beziersCount) override {
        for (UINT32 i = 0; i < beziersCount; ++i) {
            ss_ << FmtNum(beziers[i].point1.x) << ' ' << FmtNum(-beziers[i].point1.y) << ' '
                << FmtNum(beziers[i].point2.x) << ' ' << FmtNum(-beziers[i].point2.y) << ' '
                << FmtNum(beziers[i].point3.x) << ' ' << FmtNum(-beziers[i].point3.y) << " c\n";
        }
    }
    IFACEMETHODIMP_(void) EndFigure(D2D1_FIGURE_END figureEnd) override {
        if (figureEnd == D2D1_FIGURE_END_CLOSED) ss_ << "h\n";
    }
    IFACEMETHODIMP Close() override { return S_OK; }

private:
    std::ostringstream& ss_;
};

bool ExtractGlyphOutlineNative(std::uint32_t cp, bool bold, ExtractedGlyphOutline& out) {
    out = {};
    ComPtr<IDWriteFactory> dwrite;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite.GetAddressOf()))) || !dwrite) {
        return false;
    }
    ComPtr<IDWriteFontCollection> coll;
    if (FAILED(dwrite->GetSystemFontCollection(&coll, FALSE)) || !coll) {
        return false;
    }

    auto tryFamily = [&](ComPtr<IDWriteFontFamily> fam) -> bool {
        if (!fam) return false;
        ComPtr<IDWriteFont> font;
        if (FAILED(fam->GetFirstMatchingFont(
                bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                &font)) || !font) {
            return false;
        }
        BOOL exists = FALSE;
        if (FAILED(font->HasCharacter(cp, &exists)) || !exists) return false;
        ComPtr<IDWriteFontFace> face;
        if (FAILED(font->CreateFontFace(&face)) || !face) return false;
        UINT16 gid = 0;
        if (FAILED(face->GetGlyphIndices(&cp, 1, &gid)) || gid == 0) return false;

        DWRITE_FONT_METRICS fm{};
        face->GetMetrics(&fm);
        DWRITE_GLYPH_METRICS gm{};
        if (FAILED(face->GetDesignGlyphMetrics(&gid, 1, &gm, FALSE)) || fm.designUnitsPerEm == 0) return false;

        out.advance1000 = static_cast<double>(gm.advanceWidth) * 1000.0 / static_cast<double>(fm.designUnitsPerEm);
        std::ostringstream pathOps;
        PdfDWriteGeometrySink sink(pathOps);
        if (FAILED(face->GetGlyphRunOutline(1000.0f, &gid, nullptr, nullptr, 1, FALSE, FALSE, &sink))) {
            return false;
        }
        std::ostringstream cpOps;
        cpOps << FmtNum(out.advance1000) << " 0 d0\n";
        const std::string p = pathOps.str();
        if (!p.empty()) cpOps << p << "f\n";
        out.charProcOps = cpOps.str();
        return true;
    };

    static constexpr const wchar_t* kPreferred[] = {
        L"Cambria Math", L"Segoe UI Symbol", L"Segoe UI", L"Arial", L"Lucida Sans Unicode", L"DejaVu Sans"
    };
    for (const wchar_t* name : kPreferred) {
        UINT32 idx = 0;
        BOOL found = FALSE;
        if (SUCCEEDED(coll->FindFamilyName(name, &idx, &found)) && found) {
            ComPtr<IDWriteFontFamily> fam;
            if (SUCCEEDED(coll->GetFontFamily(idx, &fam)) && tryFamily(fam)) return true;
        }
    }
    const UINT32 count = coll->GetFontFamilyCount();
    for (UINT32 i = 0; i < count; ++i) {
        ComPtr<IDWriteFontFamily> fam;
        if (SUCCEEDED(coll->GetFontFamily(i, &fam)) && tryFamily(fam)) return true;
    }
    return false;
}
#elif defined(DOCSTUDIO_HAS_CAIRO_PANGO)
bool ExtractGlyphOutlineNative(std::uint32_t cp, bool bold, ExtractedGlyphOutline& out) {
    out = {};
    cairo_surface_t* rec = cairo_recording_surface_create(CAIRO_CONTENT_COLOR, nullptr);
    if (!rec) return false;
    cairo_t* cr = cairo_create(rec);
    PangoLayout* layout = pango_cairo_create_layout(cr);
    const std::string utf8 = EncodeCpUtf8(cp);
    PangoFontDescription* desc = pango_font_description_from_string(
        bold ? "DejaVu Sans Bold, Noto Sans Math Bold, FreeSerif Bold, Sans Bold 100"
             : "DejaVu Sans, Noto Sans Math, FreeSerif, Sans 100");
    pango_font_description_set_absolute_size(desc, 1000.0 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, utf8.c_str(), static_cast<int>(utf8.size()));

    PangoRectangle logical{};
    pango_layout_get_extents(layout, nullptr, &logical);
    const double baseline = static_cast<double>(pango_layout_get_baseline(layout)) / static_cast<double>(PANGO_SCALE);
    out.advance1000 = (std::max)(200.0, static_cast<double>(logical.width) / static_cast<double>(PANGO_SCALE));

    cairo_new_path(cr);
    pango_cairo_layout_path(cr, layout);
    cairo_path_t* path = cairo_copy_path(cr);

    std::ostringstream pathOps;
    if (path) {
        for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
            const cairo_path_data_t* d = &path->data[i];
            switch (d->header.type) {
                case CAIRO_PATH_MOVE_TO:
                    pathOps << FmtNum(d[1].point.x) << ' ' << FmtNum(baseline - d[1].point.y) << " m\n";
                    break;
                case CAIRO_PATH_LINE_TO:
                    pathOps << FmtNum(d[1].point.x) << ' ' << FmtNum(baseline - d[1].point.y) << " l\n";
                    break;
                case CAIRO_PATH_CURVE_TO:
                    pathOps << FmtNum(d[1].point.x) << ' ' << FmtNum(baseline - d[1].point.y) << ' '
                            << FmtNum(d[2].point.x) << ' ' << FmtNum(baseline - d[2].point.y) << ' '
                            << FmtNum(d[3].point.x) << ' ' << FmtNum(baseline - d[3].point.y) << " c\n";
                    break;
                case CAIRO_PATH_CLOSE_PATH:
                    pathOps << "h\n";
                    break;
            }
        }
        cairo_path_destroy(path);
    }

    pango_font_description_free(desc);
    g_object_unref(layout);
    cairo_destroy(cr);
    cairo_surface_destroy(rec);

    std::ostringstream cpOps;
    cpOps << FmtNum(out.advance1000) << " 0 d0\n";
    const std::string p = pathOps.str();
    if (!p.empty()) cpOps << p << "f\n";
    out.charProcOps = cpOps.str();
    return true;
}
#else
bool ExtractGlyphOutlineNative(std::uint32_t, bool, ExtractedGlyphOutline& out) {
    out = {};
    return false;
}
#endif

bool DeflateBytes(const std::uint8_t* src, std::size_t len, std::vector<std::uint8_t>& out) {
    uLongf bound = compressBound(static_cast<uLong>(len));
    out.resize(bound);
    if (compress2(out.data(), &bound, src, static_cast<uLong>(len), Z_DEFAULT_COMPRESSION) != Z_OK) {
        out.clear();
        return false;
    }
    out.resize(bound);
    return true;
}

#ifdef _WIN32
bool DecodeImageBytesViaWic(
    const std::vector<std::uint8_t>& encodedBytes,
    int& outW,
    int& outH,
    std::vector<std::uint8_t>& outRgb,
    std::vector<std::uint8_t>& outAlpha)
{
    if (encodedBytes.empty()) return false;
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool didInit = SUCCEEDED(hrCo);

    bool ok = false;
    {
        ComPtr<IWICImagingFactory> factory;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
            ComPtr<IWICStream> stream;
            if (SUCCEEDED(factory->CreateStream(&stream)) &&
                SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(encodedBytes.data()), static_cast<DWORD>(encodedBytes.size())))) {
                ComPtr<IWICBitmapDecoder> decoder;
                if (SUCCEEDED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) {
                    ComPtr<IWICBitmapFrameDecode> frame;
                    if (SUCCEEDED(decoder->GetFrame(0, &frame))) {
                        UINT w = 0, h = 0;
                        if (SUCCEEDED(frame->GetSize(&w, &h)) && w > 0 && h > 0 && w <= 8192 && h <= 8192) {
                            ComPtr<IWICFormatConverter> conv;
                            if (SUCCEEDED(factory->CreateFormatConverter(&conv)) &&
                                SUCCEEDED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                                std::vector<std::uint8_t> rgba(static_cast<std::size_t>(w) * h * 4u);
                                if (SUCCEEDED(conv->CopyPixels(nullptr, w * 4u, static_cast<UINT>(rgba.size()), rgba.data()))) {
                                    outW = static_cast<int>(w);
                                    outH = static_cast<int>(h);
                                    outRgb.resize(static_cast<std::size_t>(w) * h * 3u);
                                    outAlpha.resize(static_cast<std::size_t>(w) * h);
                                    bool hasPartialAlpha = false;
                                    for (std::size_t i = 0, p = static_cast<std::size_t>(w) * h; i < p; ++i) {
                                        outRgb[i * 3 + 0] = rgba[i * 4 + 0];
                                        outRgb[i * 3 + 1] = rgba[i * 4 + 1];
                                        outRgb[i * 3 + 2] = rgba[i * 4 + 2];
                                        const std::uint8_t a = rgba[i * 4 + 3];
                                        outAlpha[i] = a;
                                        if (a != 255) hasPartialAlpha = true;
                                    }
                                    if (!hasPartialAlpha) outAlpha.clear();
                                    ok = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (didInit) CoUninitialize();
    return ok;
}
#elif defined(DOCSTUDIO_HAS_CAIRO_PANGO)
bool DecodeEncodedImageViaGdkPixbuf(
    const std::vector<std::uint8_t>& encodedBytes,
    int& outW,
    int& outH,
    std::vector<std::uint8_t>& outRgb,
    std::vector<std::uint8_t>& outAlpha)
{
    if (encodedBytes.empty()) return false;
    GError* err = nullptr;
    GdkPixbufLoader* loader = gdk_pixbuf_loader_new();
    if (!loader) return false;

    const gboolean writeOk = gdk_pixbuf_loader_write(
        loader,
        reinterpret_cast<const guchar*>(encodedBytes.data()),
        static_cast<gsize>(encodedBytes.size()),
        &err);
    if (!writeOk) {
        if (err) g_error_free(err);
        g_object_unref(loader);
        return false;
    }
    if (!gdk_pixbuf_loader_close(loader, &err)) {
        if (err) g_error_free(err);
        g_object_unref(loader);
        return false;
    }

    GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (!pixbuf) {
        g_object_unref(loader);
        return false;
    }
    g_object_ref(pixbuf);
    g_object_unref(loader);

    const int w = gdk_pixbuf_get_width(pixbuf);
    const int h = gdk_pixbuf_get_height(pixbuf);
    const int nChannels = gdk_pixbuf_get_n_channels(pixbuf);
    const int bits = gdk_pixbuf_get_bits_per_sample(pixbuf);
    const gboolean hasAlpha = gdk_pixbuf_get_has_alpha(pixbuf);
    const int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    const guint8* pixels = gdk_pixbuf_get_pixels(pixbuf);

    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 ||
        gdk_pixbuf_get_colorspace(pixbuf) != GDK_COLORSPACE_RGB ||
        bits != 8 || (nChannels != 3 && nChannels != 4) || !pixels) {
        g_object_unref(pixbuf);
        return false;
    }

    outW = w;
    outH = h;
    outRgb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
    outAlpha.clear();
    if (hasAlpha && nChannels == 4) {
        outAlpha.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    }

    bool hasPartialAlpha = false;
    for (int y = 0; y < h; ++y) {
        const guint8* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(rowstride);
        for (int x = 0; x < w; ++x) {
            const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
            const guint8* p = row + static_cast<std::size_t>(x) * static_cast<std::size_t>(nChannels);
            outRgb[idx * 3 + 0] = p[0];
            outRgb[idx * 3 + 1] = p[1];
            outRgb[idx * 3 + 2] = p[2];
            if (hasAlpha && nChannels == 4) {
                const std::uint8_t a = p[3];
                outAlpha[idx] = a;
                if (a != 255) hasPartialAlpha = true;
            }
        }
    }
    if (!hasPartialAlpha) outAlpha.clear();
    g_object_unref(pixbuf);
    return true;
}
#else
bool DecodeEncodedImagePortablePng(
    const std::vector<std::uint8_t>& encodedBytes,
    int& outW,
    int& outH,
    std::vector<std::uint8_t>& outRgb,
    std::vector<std::uint8_t>& outAlpha)
{
    if (encodedBytes.size() < 33) return false;
    static constexpr std::uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (std::memcmp(encodedBytes.data(), kSig, 8) != 0) return false;
    auto readBe32 = [&](std::size_t off) -> std::uint32_t {
        return (static_cast<std::uint32_t>(encodedBytes[off]) << 24) |
               (static_cast<std::uint32_t>(encodedBytes[off + 1]) << 16) |
               (static_cast<std::uint32_t>(encodedBytes[off + 2]) << 8) |
               static_cast<std::uint32_t>(encodedBytes[off + 3]);
    };
    int w = 0, h = 0, bitDepth = 0, colorType = 0;
    std::vector<std::uint8_t> idat;
    std::size_t pos = 8;
    while (pos + 12 <= encodedBytes.size()) {
        const std::uint32_t len = readBe32(pos);
        const char* type = reinterpret_cast<const char*>(encodedBytes.data() + pos + 4);
        if (pos + 12 + len > encodedBytes.size()) break;
        const std::uint8_t* data = encodedBytes.data() + pos + 8;
        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            w = static_cast<int>(readBe32(pos + 8));
            h = static_cast<int>(readBe32(pos + 12));
            bitDepth = data[8];
            colorType = data[9];
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), data, data + len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + len;
    }
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || bitDepth != 8 || (colorType != 2 && colorType != 6) || idat.empty()) {
        return false;
    }
    const int bpp = (colorType == 6) ? 4 : 3;
    const std::size_t stride = static_cast<std::size_t>(w) * static_cast<std::size_t>(bpp);
    const std::size_t rawExpected = static_cast<std::size_t>(h) * (stride + 1);
    std::vector<std::uint8_t> raw(rawExpected);
    uLongf destLen = static_cast<uLongf>(rawExpected);
    if (uncompress(raw.data(), &destLen, idat.data(), static_cast<uLong>(idat.size())) != Z_OK || destLen < rawExpected) {
        return false;
    }
    std::vector<std::uint8_t> recon(static_cast<std::size_t>(h) * stride);
    auto paeth = [](int a, int b, int c) -> std::uint8_t {
        const int p = a + b - c;
        const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
        if (pa <= pb && pa <= pc) return static_cast<std::uint8_t>(a);
        if (pb <= pc) return static_cast<std::uint8_t>(b);
        return static_cast<std::uint8_t>(c);
    };
    for (int y = 0; y < h; ++y) {
        const std::uint8_t ftype = raw[static_cast<std::size_t>(y) * (stride + 1)];
        const std::uint8_t* src = raw.data() + static_cast<std::size_t>(y) * (stride + 1) + 1;
        std::uint8_t* dst = recon.data() + static_cast<std::size_t>(y) * stride;
        const std::uint8_t* prev = (y > 0) ? (recon.data() + static_cast<std::size_t>(y - 1) * stride) : nullptr;
        for (std::size_t x = 0; x < stride; ++x) {
            const int a = (x >= static_cast<std::size_t>(bpp)) ? dst[x - bpp] : 0;
            const int b = prev ? prev[x] : 0;
            const int c = (prev && x >= static_cast<std::size_t>(bpp)) ? prev[x - bpp] : 0;
            int v = src[x];
            if (ftype == 1) v += a;
            else if (ftype == 2) v += b;
            else if (ftype == 3) v += (a + b) / 2;
            else if (ftype == 4) v += paeth(a, b, c);
            dst[x] = static_cast<std::uint8_t>(v & 0xFF);
        }
    }
    outW = w;
    outH = h;
    outRgb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
    outAlpha.clear();
    if (bpp == 4) outAlpha.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h));
    bool hasPartialAlpha = false;
    for (std::size_t i = 0; i < static_cast<std::size_t>(w) * static_cast<std::size_t>(h); ++i) {
        outRgb[i * 3 + 0] = recon[i * bpp + 0];
        outRgb[i * 3 + 1] = recon[i * bpp + 1];
        outRgb[i * 3 + 2] = recon[i * bpp + 2];
        if (bpp == 4) {
            outAlpha[i] = recon[i * 4 + 3];
            if (outAlpha[i] != 255) hasPartialAlpha = true;
        }
    }
    if (!hasPartialAlpha) outAlpha.clear();
    return true;
}
#endif

struct PdfBuilder {
    struct Entry {
        std::uint32_t id = 0;
        std::string body; // everything between "<id> 0 obj\n" and "\nendobj"
    };

    struct UnicodeFontBank {
        std::uint32_t objId = 0;
        std::string resName;
        bool bold = false;
        std::vector<std::uint32_t> codepoints; // slot (code - 1) -> codepoint
        std::vector<ExtractedGlyphOutline> glyphs;
        std::unordered_map<std::uint32_t, std::uint8_t> cpToCode;
    };

    std::vector<Entry> objects;
    std::vector<UnicodeFontBank> unicodeBanks;

    std::uint32_t AllocId() {
        objects.push_back({ static_cast<std::uint32_t>(objects.size() + 1), {} });
        return objects.back().id;
    }

    void SetObject(std::uint32_t id, std::string body) {
        if (id >= 1 && id <= objects.size()) {
            objects[id - 1].body = std::move(body);
        }
    }

    std::uint32_t AddObject(std::string body) {
        const std::uint32_t id = AllocId();
        SetObject(id, std::move(body));
        return id;
    }

    std::uint32_t AddStream(const std::string& dictEntriesWithoutLength, const std::vector<std::uint8_t>& rawData, bool compress = true) {
        std::vector<std::uint8_t> payload;
        bool isCompressed = false;
        if (compress && !rawData.empty()) {
            isCompressed = DeflateBytes(rawData.data(), rawData.size(), payload);
        }
        const std::vector<std::uint8_t>& finalBytes = isCompressed ? payload : rawData;

        std::ostringstream hdr;
        hdr << "<<" << dictEntriesWithoutLength;
        if (isCompressed) hdr << "/Filter /FlateDecode ";
        hdr << "/Length " << finalBytes.size() << ">>\nstream\n";
        std::string body = hdr.str();
        body.append(reinterpret_cast<const char*>(finalBytes.data()), finalBytes.size());
        body += "\nendstream";
        return AddObject(std::move(body));
    }

    std::uint32_t AddStandardHelveticaFont(bool bold = false) {
        const char* base = bold ? "/Helvetica-Bold" : "/Helvetica";
        return AddObject(
            std::string("<</Type /Font /Subtype /Type1 /BaseFont ") + base +
            " /Encoding /WinAnsiEncoding>>");
    }

    std::uint32_t AddHighlightExtGState(double alpha = 0.36) {
        return AddObject("<</Type /ExtGState /ca " + FmtNum(alpha) + " /CA " + FmtNum(alpha) + ">>");
    }

    struct UnicodeGlyphRef {
        std::string fontResName;
        std::uint32_t fontObjId = 0;
        std::uint8_t code = 1;
        double advance1000 = 600.0;
    };

    UnicodeGlyphRef EnsureUnicodeGlyph(std::uint32_t cp, bool bold) {
        for (const auto& bank : unicodeBanks) {
            if (bank.bold != bold) continue;
            auto it = bank.cpToCode.find(cp);
            if (it != bank.cpToCode.end()) {
                const std::uint8_t c = it->second;
                return { bank.resName, bank.objId, c, bank.glyphs[static_cast<std::size_t>(c - 1)].advance1000 };
            }
        }
        UnicodeFontBank* target = nullptr;
        for (auto& bank : unicodeBanks) {
            if (bank.bold == bold && bank.codepoints.size() < 254u) {
                target = &bank;
                break;
            }
        }
        if (!target) {
            UnicodeFontBank nb;
            nb.objId = AllocId();
            nb.resName = "FU" + std::to_string(unicodeBanks.size() + 1);
            nb.bold = bold;
            unicodeBanks.push_back(std::move(nb));
            target = &unicodeBanks.back();
        }
        ExtractedGlyphOutline outline;
        if (!ExtractGlyphOutlineNative(cp, bold, outline)) {
            outline.advance1000 = 600.0;
            outline.charProcOps = "600 0 d0\n100 100 400 700 re S\n";
        }
        const std::uint8_t code = static_cast<std::uint8_t>(target->codepoints.size() + 1u);
        target->codepoints.push_back(cp);
        target->glyphs.push_back(std::move(outline));
        target->cpToCode[cp] = code;
        return { target->resName, target->objId, code, target->glyphs.back().advance1000 };
    }

    void FinalizeUnicodeFonts() {
        for (const auto& bank : unicodeBanks) {
            if (bank.codepoints.empty()) continue;

            std::vector<std::uint32_t> procIds;
            procIds.reserve(bank.codepoints.size());
            for (const auto& g : bank.glyphs) {
                const std::vector<std::uint8_t> raw(g.charProcOps.begin(), g.charProcOps.end());
                procIds.push_back(AddStream("", raw, true));
            }

            std::ostringstream cmap;
            cmap << "/CIDInit /ProcSet findresource begin\n"
                 << "12 dict begin\n"
                 << "begincmap\n"
                 << "/CIDSystemInfo <</Registry (Adobe) /Ordering (UCS) /Supplement 0>> def\n"
                 << "/CMapName /DocStudio-Uni-" << bank.resName << " def\n"
                 << "/CMapType 2 def\n"
                 << "1 begincodespacerange\n"
                 << "<01> <FF>\n"
                 << "endcodespacerange\n";
            const std::size_t n = bank.codepoints.size();
            for (std::size_t base = 0; base < n; base += 100u) {
                const std::size_t chunk = (std::min<std::size_t>)(100u, n - base);
                cmap << chunk << " beginbfchar\n";
                for (std::size_t k = 0; k < chunk; ++k) {
                    char codeHex[8]{};
                    std::snprintf(codeHex, sizeof(codeHex), "%02X", static_cast<unsigned>(base + k + 1u));
                    cmap << "<" << codeHex << "> <" << FormatUtf16BeHex(bank.codepoints[base + k]) << ">\n";
                }
                cmap << "endbfchar\n";
            }
            cmap << "endcmap\n"
                 << "CMapName currentdict /CMap defineresource pop\n"
                 << "end\nend\n";
            const std::string cmapStr = cmap.str();
            const std::uint32_t cmapId = AddStream("", std::vector<std::uint8_t>(cmapStr.begin(), cmapStr.end()), true);

            std::ostringstream fBody;
            fBody << "<</Type /Font /Subtype /Type3 "
                  << "/FontBBox [-600 -500 1800 1400] "
                  << "/FontMatrix [0.001 0 0 0.001 0 0] "
                  << "/FirstChar 1 /LastChar " << n << " "
                  << "/Widths [";
            for (std::size_t i = 0; i < n; ++i) {
                if (i > 0) fBody << ' ';
                fBody << FmtNum(bank.glyphs[i].advance1000);
            }
            fBody << "] /Encoding <</Type /Encoding /Differences [1";
            for (std::size_t i = 0; i < n; ++i) {
                fBody << " /g" << (i + 1);
            }
            fBody << "]>> /CharProcs <<";
            for (std::size_t i = 0; i < n; ++i) {
                fBody << " /g" << (i + 1) << ' ' << procIds[i] << " 0 R";
            }
            fBody << ">> /ToUnicode " << cmapId << " 0 R>>";
            SetObject(bank.objId, fBody.str());
        }
    }

    std::uint32_t AddImageXObject(const std::vector<std::uint8_t>& encodedBytes) {
        int w = 0, h = 0;
        std::vector<std::uint8_t> rgb, alpha;
#ifdef _WIN32
        if (!DecodeImageBytesViaWic(encodedBytes, w, h, rgb, alpha)) return 0;
#elif defined(DOCSTUDIO_HAS_CAIRO_PANGO)
        if (!DecodeEncodedImageViaGdkPixbuf(encodedBytes, w, h, rgb, alpha)) return 0;
#else
        if (!DecodeEncodedImagePortablePng(encodedBytes, w, h, rgb, alpha)) return 0;
#endif

        std::uint32_t smaskId = 0;
        if (!alpha.empty()) {
            smaskId = AddStream(
                "/Type /XObject /Subtype /Image /Width " + std::to_string(w) +
                " /Height " + std::to_string(h) +
                " /ColorSpace /DeviceGray /BitsPerComponent 8 ",
                alpha, true);
        }

        std::ostringstream dict;
        dict << "/Type /XObject /Subtype /Image /Width " << w << " /Height " << h
             << " /ColorSpace /DeviceRGB /BitsPerComponent 8 ";
        if (smaskId != 0) dict << "/SMask " << smaskId << " 0 R ";
        return AddStream(dict.str(), rgb, true);
    }

    // Deeply clones an object graph from a parsed PdfDocument into this PdfBuilder.
    std::uint32_t CloneObjectFromPdf(
        const PdfDocument& src,
        std::uint32_t srcObjNum,
        std::unordered_map<std::uint32_t, std::uint32_t>& remap,
        int depth = 0)
    {
        auto it = remap.find(srcObjNum);
        if (it != remap.end()) return it->second;
        if (depth > 48) return 0;

        const std::uint32_t newId = AllocId();
        remap[srcObjNum] = newId;

        PdfValue v = src.LookupObject(srcObjNum);
        PdfValue cloned = RewriteValueRefs(src, v, remap, depth + 1);
        SetObject(newId, PdfDocument::SerializeValue(cloned));
        return newId;
    }

    PdfValue RewriteValueRefs(
        const PdfDocument& src,
        const PdfValue& v,
        std::unordered_map<std::uint32_t, std::uint32_t>& remap,
        int depth)
    {
        PdfValue out = v;
        if (v.kind == PdfValue::Kind::Ref) {
            const std::uint32_t mapped = CloneObjectFromPdf(src, v.refVal.obj, remap, depth + 1);
            out.refVal = { mapped, 0 };
        } else if (v.kind == PdfValue::Kind::Array) {
            for (auto& elem : out.arrayVal) {
                elem = RewriteValueRefs(src, elem, remap, depth);
            }
        } else if (v.kind == PdfValue::Kind::Dict) {
            for (auto& kv : out.dictVal) {
                kv.second = RewriteValueRefs(src, kv.second, remap, depth);
            }
        } else if (v.kind == PdfValue::Kind::Stream && v.streamVal) {
            auto sd = std::make_shared<PdfStreamData>();
            sd->rawBytes = v.streamVal->rawBytes;
            for (const auto& kv : v.streamVal->dict) {
                sd->dict[kv.first] = RewriteValueRefs(src, kv.second, remap, depth);
            }
            out.streamVal = std::move(sd);
        }
        return out;
    }

    std::vector<std::uint8_t> Finalize(std::uint32_t catalogId) const {
        std::string out = "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n";
        std::vector<std::size_t> offsets(objects.size() + 1, 0);
        for (const auto& obj : objects) {
            offsets[obj.id] = out.size();
            out += std::to_string(obj.id) + " 0 obj\n";
            out += obj.body;
            out += "\nendobj\n";
        }
        const std::size_t xrefOffset = out.size();
        out += "xref\n0 " + std::to_string(objects.size() + 1) + "\n";
        out += "0000000000 65535 f \n";
        for (std::size_t i = 1; i <= objects.size(); ++i) {
            char line[32]{};
            std::snprintf(line, sizeof(line), "%010zu 00000 n \n", offsets[i]);
            out += line;
        }
        out += "trailer\n<</Size " + std::to_string(objects.size() + 1) +
               " /Root " + std::to_string(catalogId) + " 0 R>>\n";
        out += "startxref\n" + std::to_string(xrefOffset) + "\n%%EOF\n";
        return std::vector<std::uint8_t>(out.begin(), out.end());
    }
};

void EmitPaperPatternOps(std::ostringstream& ss, double w, double h, PaperPattern bg) {
    if (bg == PaperPattern::Plain) return;

    ss << "q\n";
    if (bg == PaperPattern::Ruled) {
        ss << "0.82 0.88 0.96 RG 0.6 w\n";
        for (double y = 36.0; y < h - 18.0; y += 24.0) {
            ss << "0 " << FmtNum(y) << " m " << FmtNum(w) << " " << FmtNum(y) << " l S\n";
        }
        ss << "0.94 0.60 0.62 RG 0.8 w 56 0 m 56 " << FmtNum(h) << " l S\n";
    } else if (bg == PaperPattern::Grid) {
        ss << "0.86 0.90 0.95 RG 0.5 w\n";
        for (double x = 20.0; x < w; x += 20.0) {
            ss << FmtNum(x) << " 0 m " << FmtNum(x) << " " << FmtNum(h) << " l S\n";
        }
        for (double y = 20.0; y < h; y += 20.0) {
            ss << "0 " << FmtNum(y) << " m " << FmtNum(w) << " " << FmtNum(y) << " l S\n";
        }
    } else if (bg == PaperPattern::Dots) {
        ss << "0.70 0.74 0.80 rg\n";
        for (double y = 20.0; y < h; y += 20.0) {
            for (double x = 20.0; x < w; x += 20.0) {
                ss << FmtNum(x - 0.7) << " " << FmtNum(y - 0.7) << " 1.4 1.4 re f\n";
            }
        }
    } else if (bg == PaperPattern::Millimeter) {
        ss << "0.90 0.94 0.91 RG 0.25 w\n";
        for (double x = 8.0; x < w; x += 8.0) {
            ss << FmtNum(x) << " 0 m " << FmtNum(x) << " " << FmtNum(h) << " l S\n";
        }
        for (double y = 8.0; y < h; y += 8.0) {
            ss << "0 " << FmtNum(y) << " m " << FmtNum(w) << " " << FmtNum(y) << " l S\n";
        }
        ss << "0.72 0.85 0.76 RG 0.6 w\n";
        for (double x = 40.0; x < w; x += 40.0) {
            ss << FmtNum(x) << " 0 m " << FmtNum(x) << " " << FmtNum(h) << " l S\n";
        }
        for (double y = 40.0; y < h; y += 40.0) {
            ss << "0 " << FmtNum(y) << " m " << FmtNum(w) << " " << FmtNum(y) << " l S\n";
        }
    }
    ss << "Q\n";
}

void EmitRgbStrokeColor(std::ostringstream& ss, DocColor c) {
    ss << FmtNum(c.r / 255.0) << ' ' << FmtNum(c.g / 255.0) << ' ' << FmtNum(c.b / 255.0) << " RG ";
}

void EmitRgbFillColor(std::ostringstream& ss, DocColor c) {
    ss << FmtNum(c.r / 255.0) << ' ' << FmtNum(c.g / 255.0) << ' ' << FmtNum(c.b / 255.0) << " rg ";
}

void EmitUnicodeAwareTextLine(
    std::ostringstream& ss,
    const std::string& lineUtf8,
    double startX,
    double baselineY,
    double fontSizePt,
    bool bold,
    bool flipTextYInCurrentCtm,
    PdfBuilder& builder,
    std::map<std::string, std::uint32_t>& extraFonts,
    bool italic = false)
{
    const char* helvRes = bold ? "/F2" : "/F1";
    const double yFlipSign = flipTextYInCurrentCtm ? -1.0 : 1.0;
    const double slantC = italic ? 0.21 : 0.0;

    std::string pureWinAnsi;
    if (IsPureWinAnsiUtf8(lineUtf8, pureWinAnsi)) {
        ss << "BT " << helvRes << ' ' << FmtNum(fontSizePt) << " Tf "
           << "1 0 " << FmtNum(slantC) << ' ' << FmtNum(yFlipSign) << ' ' << FmtNum(startX) << ' ' << FmtNum(baselineY) << " Tm "
           << EscapePdfLiteralString(pureWinAnsi) << " Tj ET\n";
        return;
    }

    double curX = startX;
    std::size_t pos = 0;
    std::string waRun;
    auto flushWinAnsiRun = [&]() {
        if (waRun.empty()) return;
        ss << "BT " << helvRes << ' ' << FmtNum(fontSizePt) << " Tf "
           << "1 0 " << FmtNum(slantC) << ' ' << FmtNum(yFlipSign) << ' ' << FmtNum(curX) << ' ' << FmtNum(baselineY) << " Tm "
           << EscapePdfLiteralString(waRun) << " Tj ET\n";
        double adv1000 = 0.0;
        for (unsigned char ch : waRun) {
            adv1000 += HelvWidth1000(ch, bold);
        }
        curX += adv1000 * fontSizePt / 1000.0;
        waRun.clear();
    };

    while (pos < lineUtf8.size()) {
        const std::uint32_t cp = DecodeUtf8Next(lineUtf8, pos);
        if (cp == '\r') continue;
        if (cp == '\t') {
            waRun.push_back(' ');
            continue;
        }
        std::uint8_t waByte = 0;
        if (TryEncodeWinAnsiCodepoint(cp, waByte)) {
            waRun.push_back(static_cast<char>(waByte));
        } else {
            flushWinAnsiRun();
            const auto gr = builder.EnsureUnicodeGlyph(cp, bold);
            extraFonts[gr.fontResName] = gr.fontObjId;
            char hex[8]{};
            std::snprintf(hex, sizeof(hex), "%02X", static_cast<unsigned>(gr.code));
            ss << "BT /" << gr.fontResName << ' ' << FmtNum(fontSizePt) << " Tf "
               << "1 0 " << FmtNum(slantC) << ' ' << FmtNum(yFlipSign) << ' ' << FmtNum(curX) << ' ' << FmtNum(baselineY) << " Tm "
               << '<' << hex << "> Tj ET\n";
            curX += gr.advance1000 * fontSizePt / 1000.0;
        }
    }
    flushWinAnsiRun();
}

void EmitRoundedRectPath(
    std::ostringstream& ss,
    double x, double y, double w, double h, double radius)
{
    const double r = std::clamp(radius, 0.0, (std::min)(w, h) * 0.5);
    if (r <= 0.1) {
        ss << FmtNum(x) << ' ' << FmtNum(y) << ' ' << FmtNum(w) << ' ' << FmtNum(h) << " re ";
        return;
    }
    const double k = r * 0.55228475;
    const double x0 = x, x1 = x + w;
    const double y0 = y, y1 = y + h;
    ss << FmtNum(x0 + r) << ' ' << FmtNum(y0) << " m "
       << FmtNum(x1 - r) << ' ' << FmtNum(y0) << " l "
       << FmtNum(x1 - r + k) << ' ' << FmtNum(y0) << ' ' << FmtNum(x1) << ' ' << FmtNum(y0 + r - k) << ' ' << FmtNum(x1) << ' ' << FmtNum(y0 + r) << " c "
       << FmtNum(x1) << ' ' << FmtNum(y1 - r) << " l "
       << FmtNum(x1) << ' ' << FmtNum(y1 - r + k) << ' ' << FmtNum(x1 - r + k) << ' ' << FmtNum(y1) << ' ' << FmtNum(x1 - r) << ' ' << FmtNum(y1) << " c "
       << FmtNum(x0 + r) << ' ' << FmtNum(y1) << " l "
       << FmtNum(x0 + r - k) << ' ' << FmtNum(y1) << ' ' << FmtNum(x0) << ' ' << FmtNum(y1 - r + k) << ' ' << FmtNum(x0) << ' ' << FmtNum(y1 - r) << " c "
       << FmtNum(x0) << ' ' << FmtNum(y0 + r) << " l "
       << FmtNum(x0) << ' ' << FmtNum(y0 + r - k) << ' ' << FmtNum(x0 + r - k) << ' ' << FmtNum(y0) << ' ' << FmtNum(x0 + r) << ' ' << FmtNum(y0) << " c h ";
}

class PdfMathBackend final : public MathTypeset::IMathDrawBackend {
public:
    PdfMathBackend(
        std::ostringstream& ss,
        PdfBuilder& builder,
        std::map<std::string, std::uint32_t>& extraFonts,
        const DocRect& box,
        bool flipTextYInCurrentCtm)
        : ss_(ss), builder_(builder), extraFonts_(extraFonts),
          box_(box), flip_(flipTextYInCurrentCtm) {}

    MathTypeset::TextMetrics2D MeasureText(
        const std::string& utf8,
        double fontSizePx,
        MathTypeset::FontStyleRole role) override
    {
        const double fs = (std::max)(4.0, fontSizePx);
        const bool bold = (role == MathTypeset::FontStyleRole::UiSemiBold ||
                           role == MathTypeset::FontStyleRole::MathBold ||
                           role == MathTypeset::FontStyleRole::KeycapLabel);
        double adv1000 = 0.0;
        std::size_t pos = 0;
        while (pos < utf8.size()) {
            const std::uint32_t cp = DecodeUtf8Next(utf8, pos);
            if (cp == '\r') continue;
            if (cp == '\t') {
                adv1000 += HelvWidth1000(' ', bold);
                continue;
            }
            std::uint8_t waByte = 0;
            if (TryEncodeWinAnsiCodepoint(cp, waByte)) {
                adv1000 += HelvWidth1000(waByte, bold);
            } else {
                const auto gr = builder_.EnsureUnicodeGlyph(cp, bold);
                extraFonts_[gr.fontResName] = gr.fontObjId;
                adv1000 += gr.advance1000;
            }
        }
        MathTypeset::TextMetrics2D m;
        m.width = adv1000 * fs / 1000.0;
        m.ascent = fs * 0.78;
        m.descent = fs * 0.24;
        return m;
    }

    void DrawTextRun(
        double x,
        double baselineY,
        const std::string& utf8,
        double fontSizePx,
        MathTypeset::FontStyleRole role,
        DocColor color) override
    {
        if (utf8.empty()) return;
        const bool bold = (role == MathTypeset::FontStyleRole::UiSemiBold ||
                           role == MathTypeset::FontStyleRole::MathBold ||
                           role == MathTypeset::FontStyleRole::KeycapLabel);
        const bool italic = (role == MathTypeset::FontStyleRole::MathItalic);
        EmitRgbFillColor(ss_, color);
        EmitUnicodeAwareTextLine(
            ss_, utf8, x, MapY(baselineY), (std::max)(4.0, fontSizePx),
            bold, flip_, builder_, extraFonts_, italic);
    }

    void DrawLine(
        double x0, double y0,
        double x1, double y1,
        double thickness,
        DocColor color) override
    {
        EmitRgbStrokeColor(ss_, color);
        ss_ << FmtNum((std::max)(0.4, thickness)) << " w 1 J "
            << FmtNum(x0) << ' ' << FmtNum(MapY(y0)) << " m "
            << FmtNum(x1) << ' ' << FmtNum(MapY(y1)) << " l S\n";
    }

    void DrawRoundedPill(
        double x, double y, double w, double h,
        double radius,
        DocColor fill,
        DocColor stroke,
        double strokeWidth) override
    {
        const double py = flip_ ? y : (MapY(y + h));
        if (fill.a > 0 && stroke.a > 0 && strokeWidth > 0.0) {
            EmitRgbFillColor(ss_, fill);
            EmitRgbStrokeColor(ss_, stroke);
            ss_ << FmtNum(strokeWidth) << " w ";
            EmitRoundedRectPath(ss_, x, py, w, h, radius);
            ss_ << "B\n";
        } else if (fill.a > 0) {
            EmitRgbFillColor(ss_, fill);
            EmitRoundedRectPath(ss_, x, py, w, h, radius);
            ss_ << "f\n";
        } else if (stroke.a > 0 && strokeWidth > 0.0) {
            EmitRgbStrokeColor(ss_, stroke);
            ss_ << FmtNum(strokeWidth) << " w ";
            EmitRoundedRectPath(ss_, x, py, w, h, radius);
            ss_ << "S\n";
        }
    }

private:
    double MapY(double tdY) const noexcept {
        return flip_ ? tdY : (2.0 * box_.y + box_.h - tdY);
    }

    std::ostringstream& ss_;
    PdfBuilder& builder_;
    std::map<std::string, std::uint32_t>& extraFonts_;
    DocRect box_;
    bool flip_ = true;
};

void EmitAnnotationObjectOps(
    std::ostringstream& ss,
    const AnnotationObject& obj,
    const DocumentModel& doc,
    PdfBuilder& builder,
    std::map<std::string, std::uint32_t>& imageXObjects,
    std::map<std::string, std::uint32_t>& extraFonts,
    double offsetX,
    double offsetY,
    bool flipTextYInCurrentCtm)
{
    if (!doc.IsLayerVisible(obj.layerId) || !doc.IsLayerExportable(obj.layerId)) return;

    auto tx = [&](double x) { return x + offsetX; };
    auto ty = [&](double y) { return y + offsetY; };

    ss << "q\n";
    if (obj.isHighlighter || obj.strokeColor.a < 220) {
        ss << "/GSAlpha gs\n";
    }

    if (obj.kind == ObjectKind::InkStroke) {
        if (obj.inkPoints.size() == 1) {
            EmitRgbFillColor(ss, obj.strokeColor);
            const double r = (std::max)(1.0, obj.strokeWidth * 0.5);
            ss << FmtNum(tx(obj.inkPoints[0].x) - r) << ' ' << FmtNum(ty(obj.inkPoints[0].y) - r)
               << ' ' << FmtNum(r * 2.0) << ' ' << FmtNum(r * 2.0) << " re f\n";
        } else if (obj.inkPoints.size() >= 2) {
            EmitRgbStrokeColor(ss, obj.strokeColor);
            ss << "1 J 1 j\n";
            // Average pressure modulation or segment chunks
            for (std::size_t i = 1; i < obj.inkPoints.size(); ++i) {
                const auto& p0 = obj.inkPoints[i - 1];
                const auto& p1 = obj.inkPoints[i];
                const double avgP = 0.5 * (p0.pressure + p1.pressure);
                const double w = obj.isHighlighter
                    ? (std::max)(4.0, obj.strokeWidth)
                    : (std::max)(0.6, obj.strokeWidth * (0.45 + 1.1 * avgP));
                ss << FmtNum(w) << " w "
                   << FmtNum(tx(p0.x)) << ' ' << FmtNum(ty(p0.y)) << " m "
                   << FmtNum(tx(p1.x)) << ' ' << FmtNum(ty(p1.y)) << " l S\n";
            }
        }
    } else if (obj.kind == ObjectKind::Shape) {
        EmitRgbStrokeColor(ss, obj.strokeColor);
        ss << FmtNum((std::max)(0.5, obj.strokeWidth)) << " w\n";
        const DocRect r = { tx(obj.bounds.x), ty(obj.bounds.y), obj.bounds.w, obj.bounds.h };

        if (obj.shapeKind == ShapeKind::Rectangle || obj.shapeKind == ShapeKind::SignatureStamp) {
            if (obj.isRedaction) {
                // Permanent opaque redaction burn-in (F-087)
                ss << "0 0 0 rg " << FmtNum(r.x) << ' ' << FmtNum(r.y) << ' ' << FmtNum(r.w) << ' ' << FmtNum(r.h) << " re f\n";
            } else if (obj.fillColor.a > 0) {
                EmitRgbFillColor(ss, obj.fillColor);
                ss << FmtNum(r.x) << ' ' << FmtNum(r.y) << ' ' << FmtNum(r.w) << ' ' << FmtNum(r.h) << " re B\n";
            } else {
                ss << FmtNum(r.x) << ' ' << FmtNum(r.y) << ' ' << FmtNum(r.w) << ' ' << FmtNum(r.h) << " re S\n";
            }
            if (obj.shapeKind == ShapeKind::SignatureStamp && !obj.textUtf8.empty()) {
                EmitRgbFillColor(ss, obj.strokeColor);
                const double fs = (std::max)(8.0, obj.fontSizePt > 0.0 ? obj.fontSizePt : 10.0);
                const double baseY = flipTextYInCurrentCtm ? (r.y + fs + 4.0) : (r.y + r.h * 0.5);
                EmitUnicodeAwareTextLine(ss, obj.textUtf8, r.x + 6.0, baseY, fs, true, flipTextYInCurrentCtm, builder, extraFonts);
            }
        } else if (obj.shapeKind == ShapeKind::Ellipse) {
            // 4-segment cubic Bezier approximation for ellipse
            const double cx = r.x + r.w * 0.5;
            const double cy = r.y + r.h * 0.5;
            const double rx = r.w * 0.5;
            const double ry = r.h * 0.5;
            const double kx = rx * 0.55228475;
            const double ky = ry * 0.55228475;
            ss << FmtNum(cx + rx) << ' ' << FmtNum(cy) << " m\n"
               << FmtNum(cx + rx) << ' ' << FmtNum(cy + ky) << ' ' << FmtNum(cx + kx) << ' ' << FmtNum(cy + ry) << ' ' << FmtNum(cx) << ' ' << FmtNum(cy + ry) << " c\n"
               << FmtNum(cx - kx) << ' ' << FmtNum(cy + ry) << ' ' << FmtNum(cx - rx) << ' ' << FmtNum(cy + ky) << ' ' << FmtNum(cx - rx) << ' ' << FmtNum(cy) << " c\n"
               << FmtNum(cx - rx) << ' ' << FmtNum(cy - ky) << ' ' << FmtNum(cx - kx) << ' ' << FmtNum(cy - ry) << ' ' << FmtNum(cx) << ' ' << FmtNum(cy - ry) << " c\n"
               << FmtNum(cx + kx) << ' ' << FmtNum(cy - ry) << ' ' << FmtNum(cx + rx) << ' ' << FmtNum(cy - ky) << ' ' << FmtNum(cx + rx) << ' ' << FmtNum(cy) << " c\n";
            if (obj.fillColor.a > 0) {
                EmitRgbFillColor(ss, obj.fillColor);
                ss << "B\n";
            } else {
                ss << "S\n";
            }
        } else {
            // Line / Arrow
            const double x0 = tx(obj.shapeStart.x), y0 = ty(obj.shapeStart.y);
            const double x1 = tx(obj.shapeEnd.x), y1 = ty(obj.shapeEnd.y);
            ss << "1 J " << FmtNum(x0) << ' ' << FmtNum(y0) << " m " << FmtNum(x1) << ' ' << FmtNum(y1) << " l S\n";
            if (obj.shapeKind == ShapeKind::Arrow || obj.shapeKind == ShapeKind::Connector) {
                const double dx = x1 - x0;
                const double dy = y1 - y0;
                const double len = std::hypot(dx, dy);
                if (len > 1e-3) {
                    const double ux = dx / len, uy = dy / len;
                    const double hs = (std::max)(8.0, obj.strokeWidth * 3.2);
                    const double px = -uy * (hs * 0.45), py = ux * (hs * 0.45);
                    ss << FmtNum(x1 - ux * hs + px) << ' ' << FmtNum(y1 - uy * hs + py) << " m "
                       << FmtNum(x1) << ' ' << FmtNum(y1) << " l "
                       << FmtNum(x1 - ux * hs - px) << ' ' << FmtNum(y1 - uy * hs - py) << " l S\n";
                }
            }
        }
    } else if (obj.kind == ObjectKind::TextBox) {
        const DocRect r = { tx(obj.bounds.x), ty(obj.bounds.y), (std::max)(24.0, obj.bounds.w), (std::max)(18.0, obj.bounds.h) };
        if (obj.fillColor.a > 0) {
            EmitRgbFillColor(ss, obj.fillColor);
            ss << FmtNum(r.x) << ' ' << FmtNum(r.y) << ' ' << FmtNum(r.w) << ' ' << FmtNum(r.h) << " re f\n";
        }
        const double fs = (std::max)(6.0, obj.fontSizePt);
        const bool hasMathMarkup = (obj.textUtf8.find('\\') != std::string::npos ||
                                    obj.textUtf8.find('$') != std::string::npos ||
                                    obj.textUtf8.find('^') != std::string::npos);
        if (hasMathMarkup) {
            PdfMathBackend mathBackend(ss, builder, extraFonts, r, flipTextYInCurrentCtm);
            std::istringstream iss(obj.textUtf8);
            std::string line;
            double curY = r.y + 3.0;
            while (std::getline(iss, line)) {
                if (curY > r.y + r.h) break;
                const double consumed = MathTypeset::DrawRichMathLine(
                    mathBackend, line, r.x + 2.0, curY, (std::max)(12.0, r.w - 4.0),
                    fs, obj.strokeColor, false);
                curY += (std::max)(fs * 1.25, consumed + 2.0);
            }
        } else {
            EmitRgbFillColor(ss, obj.strokeColor);
            std::istringstream iss(obj.textUtf8);
            std::string line;
            int lineIdx = 0;
            while (std::getline(iss, line)) {
                const double baselineOffset = fs * 0.9 + static_cast<double>(lineIdx) * (fs * 1.25);
                const double baseY = flipTextYInCurrentCtm ? (r.y + baselineOffset) : (r.y + r.h - baselineOffset);
                EmitUnicodeAwareTextLine(ss, line, r.x + 2.0, baseY, fs, obj.bold, flipTextYInCurrentCtm, builder, extraFonts);
                ++lineIdx;
            }
        }
    } else if (obj.kind == ObjectKind::Image) {
        const std::vector<std::uint8_t>* bytesPtr = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
        if (bytesPtr && !bytesPtr->empty()) {
            const std::uint32_t xobjId = builder.AddImageXObject(*bytesPtr);
            if (xobjId != 0) {
                const std::string resName = "Im" + std::to_string(imageXObjects.size() + 1);
                imageXObjects[resName] = xobjId;
                const DocRect r = { tx(obj.bounds.x), ty(obj.bounds.y), (std::max)(4.0, obj.bounds.w), (std::max)(4.0, obj.bounds.h) };
                ss << "q " << FmtNum(r.w) << " 0 0 " << FmtNum(-r.h) << ' ' << FmtNum(r.x) << ' ' << FmtNum(r.y + r.h)
                   << " cm /" << resName << " Do Q\n";
            }
        }
    } else if (obj.kind == ObjectKind::StudyTape ||
               obj.kind == ObjectKind::MathBlock ||
               obj.kind == ObjectKind::CircuitBlock ||
               obj.kind == ObjectKind::CalcSessionCard ||
               obj.kind == ObjectKind::PlotBlock) {
        const DocRect r = { tx(obj.bounds.x), ty(obj.bounds.y), (std::max)(48.0, obj.bounds.w), (std::max)(24.0, obj.bounds.h) };
        const auto card = RenderCommon::BuildStemCardLayout(obj);
        EmitRgbFillColor(ss, card.bg);
        EmitRgbStrokeColor(ss, card.border);
        ss << "1.2 w ";
        EmitRoundedRectPath(ss, r.x, r.y, r.w, r.h, 8.0);
        ss << "B\n";

        const bool hasThumb = (obj.kind == ObjectKind::CalcSessionCard && !obj.pngBytes.empty());
        DocRect textRect = r;
        if (hasThumb) {
            textRect.w = (std::max)(48.0, r.w - (std::min)(r.w * 0.40, 120.0) - 10.0);
        }
        PdfMathBackend mathBackend(ss, builder, extraFonts, r, flipTextYInCurrentCtm);
        MathTypeset::DrawStemCardContent(
            mathBackend, obj, textRect, 0.85,
            card.titleColor, card.bodyColor, card.headerUtf8, card.bodyUtf8);

        if (hasThumb) {
            const std::uint32_t xobjId = builder.AddImageXObject(obj.pngBytes);
            if (xobjId != 0) {
                const std::string resName = "Im" + std::to_string(imageXObjects.size() + 1);
                imageXObjects[resName] = xobjId;
                const double iw = (std::min)(r.w * 0.40, 120.0);
                const double ih = (std::min)(r.h * 0.58, iw * 0.55);
                const double ix = r.x + r.w - iw - 6.0;
                const double iy = r.y + 20.0;
                ss << "q " << FmtNum(iw) << " 0 0 " << FmtNum(-ih) << ' ' << FmtNum(ix) << ' ' << FmtNum(iy + ih)
                   << " cm /" << resName << " Do Q\n";
            }
        } else if (obj.kind == ObjectKind::PlotBlock) {
            const auto plot = RenderCommon::BuildPlotRenderData(obj);
            const double px0 = r.x + 8.0;
            const double py0 = r.y + 24.0;
            const double pw = (std::max)(16.0, r.w - 16.0);
            const double ph = (std::max)(16.0, r.h - 42.0);
            ss << "q 1 1 1 rg " << FmtNum(px0) << ' ' << FmtNum(py0) << ' ' << FmtNum(pw) << ' ' << FmtNum(ph) << " re f\n";
            ss << "0.58 0.64 0.72 RG 0.8 w " << FmtNum(px0) << ' ' << FmtNum(py0) << ' ' << FmtNum(pw) << ' ' << FmtNum(ph) << " re S\n";
            auto mapPt = [&](double wx, double wy) -> DocPoint {
                const double nx = std::clamp((wx - plot.xMin) / (std::max)(1e-9, plot.xMax - plot.xMin), 0.0, 1.0);
                const double ny = std::clamp((wy - plot.yMin) / (std::max)(1e-9, plot.yMax - plot.yMin), 0.0, 1.0);
                const double sy = flipTextYInCurrentCtm ? (py0 + ph - ny * ph) : (py0 + ny * ph);
                return { px0 + nx * pw, sy };
            };
            if (plot.is3D && !plot.surface3DSegments.empty()) {
                ss << "0.95 w\n";
                for (const auto& seg : plot.surface3DSegments) {
                    EmitRgbStrokeColor(ss, seg.color);
                    const DocPoint pa = mapPt(seg.a.x, seg.a.y);
                    const DocPoint pb = mapPt(seg.b.x, seg.b.y);
                    ss << FmtNum(pa.x) << ' ' << FmtNum(pa.y) << " m " << FmtNum(pb.x) << ' ' << FmtNum(pb.y) << " l S\n";
                }
            } else if (plot.curvePoints.size() >= 2) {
                const DocPoint p0 = mapPt(plot.curvePoints[0].x, plot.curvePoints[0].y);
                ss << "0.31 0.27 0.90 RG 1.4 w " << FmtNum(p0.x) << ' ' << FmtNum(p0.y) << " m\n";
                for (std::size_t i = 1; i < plot.curvePoints.size(); ++i) {
                    const DocPoint pi = mapPt(plot.curvePoints[i].x, plot.curvePoints[i].y);
                    ss << FmtNum(pi.x) << ' ' << FmtNum(pi.y) << " l\n";
                }
                ss << "S\n";
            }
            ss << "Q\n";
        }
    } else if (obj.kind == ObjectKind::TableBlock ||
               obj.kind == ObjectKind::CodeBlock ||
               obj.kind == ObjectKind::AudioClip) {
        const DocRect r = { tx(obj.bounds.x), ty(obj.bounds.y), (std::max)(60.0, obj.bounds.w), (std::max)(28.0, obj.bounds.h) };
        ss << "0.98 0.99 1 rg 0.78 0.83 0.90 RG 1 w ";
        EmitRoundedRectPath(ss, r.x, r.y, r.w, r.h, 6.0);
        ss << "B\n";
        const std::string header = !obj.captionUtf8.empty() ? obj.captionUtf8
            : (obj.kind == ObjectKind::TableBlock ? "Tabla" : (obj.kind == ObjectKind::CodeBlock ? "C\xC3\xB3" "digo" : "Audio"));
        EmitRgbFillColor(ss, { 24, 34, 52, 255 });
        const double baseY = flipTextYInCurrentCtm ? (r.y + 14.0) : (r.y + r.h - 14.0);
        EmitUnicodeAwareTextLine(ss, header, r.x + 8.0, baseY, 10.5, true, flipTextYInCurrentCtm, builder, extraFonts);
        const std::string body = !obj.textUtf8.empty() ? obj.textUtf8 : obj.evaluatedResultUtf8;
        if (!body.empty()) {
            std::istringstream iss(body);
            std::string ln;
            int li = 0;
            while (std::getline(iss, ln) && li < 12) {
                const double ly = flipTextYInCurrentCtm ? (r.y + 28.0 + li * 12.5) : (r.y + r.h - 28.0 - li * 12.5);
                EmitUnicodeAwareTextLine(ss, ln.substr(0, 96), r.x + 8.0, ly, 9.0, false, flipTextYInCurrentCtm, builder, extraFonts);
                ++li;
            }
        }
    }

    ss << "Q\n";
}

// Builds top-down CTM matrix for a page of cropped source box [L..R, B..T] rotated by rotDeg
std::string BuildPageOverlayCtm(double L, double B, double R, double T, int rotDeg) {
    const int r = ((rotDeg % 360) + 360) % 360;
    std::ostringstream ss;
    if (r == 0) {
        ss << "1 0 0 -1 " << FmtNum(L) << ' ' << FmtNum(T) << " cm\n";
    } else if (r == 90) {
        ss << "0 1 1 0 " << FmtNum(L) << ' ' << FmtNum(B) << " cm\n";
    } else if (r == 180) {
        ss << "-1 0 0 1 " << FmtNum(R) << ' ' << FmtNum(B) << " cm\n";
    } else {
        ss << "0 -1 -1 0 " << FmtNum(R) << ' ' << FmtNum(T) << " cm\n";
    }
    return ss.str();
}

bool WriteAllBytesToFile(const std::wstring& path, const std::vector<std::uint8_t>& bytes, std::string* outErr) {
#ifdef _WIN32
    const std::wstring tmpPath = path + L".tmp";
    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo crear archivo PDF destino (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }
    std::size_t off = 0;
    while (off < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(bytes.size() - off, 4u * 1024u * 1024u));
        DWORD wr = 0;
        if (!WriteFile(hFile, bytes.data() + off, chunk, &wr, nullptr) || wr != chunk) {
            CloseHandle(hFile);
            DeleteFileW(tmpPath.c_str());
            if (outErr) *outErr = "Error escribiendo PDF.";
            return false;
        }
        off += wr;
    }
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    if (!MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmpPath.c_str());
        if (outErr) *outErr = "No se pudo finalizar archivo PDF.";
        return false;
    }
    return true;
#else
    const std::string utf8Path = WideToUtf8(path);
    if (utf8Path.empty()) {
        if (outErr) *outErr = "Ruta de archivo PDF vacia o invalida.";
        return false;
    }
    const std::string tmpPath = utf8Path + ".tmp";
    const int fd = ::open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        if (outErr) *outErr = "No se pudo crear archivo PDF destino (errno " + std::to_string(errno) + ").";
        return false;
    }
    std::size_t off = 0;
    while (off < bytes.size()) {
        const std::size_t chunk = (std::min<std::size_t>)(bytes.size() - off, 4u * 1024u * 1024u);
        const ssize_t wr = ::write(fd, bytes.data() + off, chunk);
        if (wr < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            ::unlink(tmpPath.c_str());
            if (outErr) *outErr = "Error escribiendo PDF (errno " + std::to_string(errno) + ").";
            return false;
        }
        if (wr == 0) {
            ::close(fd);
            ::unlink(tmpPath.c_str());
            if (outErr) *outErr = "Error escribiendo PDF (write=0).";
            return false;
        }
        off += static_cast<std::size_t>(wr);
    }
    (void)::fsync(fd);
    ::close(fd);
    if (std::rename(tmpPath.c_str(), utf8Path.c_str()) != 0) {
        const int errCode = errno;
        ::unlink(tmpPath.c_str());
        if (outErr) *outErr = "No se pudo finalizar archivo PDF (errno " + std::to_string(errCode) + ").";
        return false;
    }
    const std::filesystem::path parentDir = std::filesystem::path(utf8Path).parent_path();
    if (!parentDir.empty()) {
        const int dfd = ::open(parentDir.c_str(), O_RDONLY);
        if (dfd >= 0) {
            (void)::fsync(dfd);
            ::close(dfd);
        }
    }
    return true;
#endif
}

std::shared_ptr<PdfDocument> ResolvePdfSourceCached(
    const DocumentModel& doc,
    const std::string& pathUtf8,
    const std::string& assetKey,
    std::unordered_map<std::string, std::shared_ptr<PdfDocument>>& cache)
{
    const std::string cacheKey = pathUtf8 + "\n" + assetKey;
    auto it = cache.find(cacheKey);
    if (it != cache.end()) return it->second;

    auto loaded = std::make_shared<PdfDocument>();
    std::string dummyErr;
    bool ok = false;
    if (!pathUtf8.empty()) {
        ok = loaded->LoadFromFile(Utf8ToWide(pathUtf8), &dummyErr);
    }
    if (!ok && !assetKey.empty()) {
        if (const auto* bytes = doc.FindAsset(assetKey)) {
            ok = loaded->LoadFromBytes(*bytes, &dummyErr);
        }
    }
    if (!ok) loaded.reset();
    cache[cacheKey] = loaded;
    return loaded;
}

bool TryBuildPdfPageFormXObject(
    const PdfDocument& srcDoc,
    std::size_t pageIdx0,
    PdfBuilder& builder,
    std::unordered_map<std::uint32_t, std::uint32_t>& remap,
    std::uint32_t& outFormId,
    DocRect& outBox,
    int& outRotDeg)
{
    outFormId = 0;
    if (pageIdx0 >= srcDoc.PageCount()) return false;
    const auto& pi = srcDoc.PageAt(pageIdx0);
    outBox = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
    if (outBox.w < 1.0 || outBox.h < 1.0) return false;
    outRotDeg = ((pi.rotationDeg % 360) + 360) % 360;

    const std::vector<std::uint8_t> contentBytes = srcDoc.GetPageContentBytes(pageIdx0);
    const PdfValue pageVal = srcDoc.LookupObject(pi.pageRef.obj);

    PdfValue resRewritten;
    if (const PdfValue* resVal = srcDoc.Resolve(pageVal.DictGet("Resources"))) {
        if (resVal->IsDict()) {
            resRewritten = builder.RewriteValueRefs(srcDoc, *resVal, remap, 1);
        }
    }

    std::ostringstream dict;
    dict << "/Type /XObject /Subtype /Form /FormType 1 "
         << "/BBox [" << FmtNum(outBox.x) << ' ' << FmtNum(outBox.y) << ' '
         << FmtNum(outBox.Right()) << ' ' << FmtNum(outBox.Bottom()) << "] "
         << "/Matrix [1 0 0 1 0 0] ";
    if (resRewritten.IsDict()) {
        dict << "/Resources " << PdfDocument::SerializeValue(resRewritten) << ' ';
    }
    outFormId = builder.AddStream(dict.str(), contentBytes, true);
    return outFormId != 0;
}

} // namespace

bool PdfWriter::ExportDocumentToBytes(
    const DocumentModel& doc,
    std::vector<std::uint8_t>& outPdfBytes,
    std::string* outErr)
{
    if (doc.Pages().empty()) {
        if (outErr) *outErr = "El documento no tiene p\xC3\xA1ginas para exportar.";
        return false;
    }

    PdfBuilder builder;
    const std::uint32_t f1 = builder.AddStandardHelveticaFont(false);
    const std::uint32_t f2 = builder.AddStandardHelveticaFont(true);
    const std::uint32_t gsAlpha = builder.AddHighlightExtGState(0.38);

    std::unordered_map<std::string, std::shared_ptr<PdfDocument>> loadedSources;
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, std::uint32_t>> sourceRemaps;

    const std::uint32_t pagesRootId = builder.AllocId();
    std::vector<std::uint32_t> pageIds;

    for (std::size_t pageOrdinal = 0; pageOrdinal < doc.Pages().size(); ++pageOrdinal) {
        const auto& pg = doc.Pages()[pageOrdinal];
        std::shared_ptr<PdfDocument> srcDoc;
        if (pg.hasPdfSource) {
            srcDoc = ResolvePdfSourceCached(doc, pg.sourcePdfPathUtf8, pg.sourcePdfAssetKey, loadedSources);
            if (!srcDoc || pg.sourcePageIndex0 < 0 ||
                static_cast<std::size_t>(pg.sourcePageIndex0) >= srcDoc->PageCount()) {
                if (outErr) {
                    *outErr = "Falta la fuente PDF original o p\xC3\xA1gina inv\xC3\xA1lida al exportar p\xC3\xA1gina " +
                              std::to_string(pageOrdinal + 1) +
                              (pg.sourcePdfPathUtf8.empty() ? "" : (": " + pg.sourcePdfPathUtf8));
                }
                return false;
            }
        }

        if (pg.hasPdfSource && srcDoc) {
            const auto& srcPageInfo = srcDoc->PageAt(static_cast<std::size_t>(pg.sourcePageIndex0));
            const PdfValue srcPageVal = srcDoc->LookupObject(srcPageInfo.pageRef.obj);
            const std::string remapKey = pg.sourcePdfPathUtf8 + "\n" + pg.sourcePdfAssetKey;
            auto& remap = sourceRemaps[remapKey];

            const DocRect baseBox = srcPageInfo.hasCropBox ? srcPageInfo.cropBox : srcPageInfo.mediaBox;
            const double cropL = baseBox.x + baseBox.w * pg.crop.left;
            const double cropR = baseBox.x + baseBox.w * pg.crop.right;
            const double cropT = baseBox.Bottom() - baseBox.h * pg.crop.top;
            const double cropB = baseBox.Bottom() - baseBox.h * pg.crop.bottom;
            const DocRect finalCrop = DocRect::FromMinMax((std::min)(cropL, cropR), (std::min)(cropB, cropT),
                                                          (std::max)(cropL, cropR), (std::max)(cropB, cropT));

            std::vector<std::uint32_t> clonedContentIds;
            const PdfValue* contents = srcPageVal.DictGet("Contents");
            if (contents) {
                if (contents->IsRef()) {
                    const PdfValue* resolved = srcDoc->Resolve(contents);
                    if (resolved && resolved->IsArray()) {
                        for (const auto& elem : resolved->arrayVal) {
                            if (elem.IsRef()) {
                                clonedContentIds.push_back(builder.CloneObjectFromPdf(*srcDoc, elem.refVal.obj, remap));
                            }
                        }
                    } else {
                        clonedContentIds.push_back(builder.CloneObjectFromPdf(*srcDoc, contents->refVal.obj, remap));
                    }
                } else if (contents->IsArray()) {
                    for (const auto& elem : contents->arrayVal) {
                        if (elem.IsRef()) {
                            clonedContentIds.push_back(builder.CloneObjectFromPdf(*srcDoc, elem.refVal.obj, remap));
                        }
                    }
                }
            }

            PdfDict mergedResDict;
            if (const PdfValue* resVal = srcDoc->Resolve(srcPageVal.DictGet("Resources"))) {
                if (resVal->IsDict()) {
                    PdfValue rewritten = builder.RewriteValueRefs(*srcDoc, *resVal, remap, 1);
                    if (rewritten.IsDict()) mergedResDict = std::move(rewritten.dictVal);
                }
            }

            std::map<std::string, std::uint32_t> overlayImages;
            std::map<std::string, std::uint32_t> overlayExtraFonts;
            if (!pg.objects.empty()) {
                std::ostringstream overlay;
                overlay << "q\n";
                overlay << BuildPageOverlayCtm(finalCrop.x, finalCrop.y, finalCrop.Right(), finalCrop.Bottom(), pg.rotationDeg);
                for (const auto& obj : pg.objects) {
                    EmitAnnotationObjectOps(overlay, obj, doc, builder, overlayImages, overlayExtraFonts, 0.0, 0.0, true);
                }
                overlay << "Q\n";

                const std::string ovStr = overlay.str();
                const std::uint32_t wrapOpen = builder.AddStream("", { 'q', '\n' }, false);
                const std::uint32_t wrapClose = builder.AddStream("", { 'Q', '\n' }, false);
                const std::uint32_t ovId = builder.AddStream("", std::vector<std::uint8_t>(ovStr.begin(), ovStr.end()), true);

                std::vector<std::uint32_t> finalContents;
                if (!clonedContentIds.empty()) {
                    finalContents.push_back(wrapOpen);
                    finalContents.insert(finalContents.end(), clonedContentIds.begin(), clonedContentIds.end());
                    finalContents.push_back(wrapClose);
                }
                finalContents.push_back(ovId);
                clonedContentIds = std::move(finalContents);

                auto ensureSubDict = [&](const std::string& name) -> PdfDict& {
                    auto it = mergedResDict.find(name);
                    if (it == mergedResDict.end() || !it->second.IsDict()) {
                        PdfValue dv;
                        dv.kind = PdfValue::Kind::Dict;
                        mergedResDict[name] = std::move(dv);
                    }
                    return mergedResDict[name].dictVal;
                };
                auto makeRef = [](std::uint32_t id) {
                    PdfValue rv;
                    rv.kind = PdfValue::Kind::Ref;
                    rv.refVal = { id, 0 };
                    return rv;
                };
                PdfDict& fd = ensureSubDict("Font");
                if (fd.find("F1") == fd.end()) fd["F1"] = makeRef(f1);
                if (fd.find("F2") == fd.end()) fd["F2"] = makeRef(f2);
                for (const auto& fkv : overlayExtraFonts) {
                    fd[fkv.first] = makeRef(fkv.second);
                }
                PdfDict& gd = ensureSubDict("ExtGState");
                if (gd.find("GSAlpha") == gd.end()) gd["GSAlpha"] = makeRef(gsAlpha);
                if (!overlayImages.empty()) {
                    PdfDict& xd = ensureSubDict("XObject");
                    for (const auto& ikv : overlayImages) {
                        xd[ikv.first] = makeRef(ikv.second);
                    }
                }
            }

            const std::uint32_t pageId = builder.AllocId();
            std::ostringstream pgBody;
            pgBody << "<</Type /Page /Parent " << pagesRootId << " 0 R "
                   << "/MediaBox [" << FmtNum(srcPageInfo.mediaBox.x) << ' ' << FmtNum(srcPageInfo.mediaBox.y) << ' '
                   << FmtNum(srcPageInfo.mediaBox.Right()) << ' ' << FmtNum(srcPageInfo.mediaBox.Bottom()) << "] "
                   << "/CropBox [" << FmtNum(finalCrop.x) << ' ' << FmtNum(finalCrop.y) << ' '
                   << FmtNum(finalCrop.Right()) << ' ' << FmtNum(finalCrop.Bottom()) << "] "
                   << "/Rotate " << (((pg.rotationDeg % 360) + 360) % 360) << " ";
            if (!mergedResDict.empty()) {
                PdfValue rv;
                rv.kind = PdfValue::Kind::Dict;
                rv.dictVal = std::move(mergedResDict);
                pgBody << "/Resources " << PdfDocument::SerializeValue(rv) << ' ';
            }
            if (!clonedContentIds.empty()) {
                pgBody << "/Contents [";
                for (std::size_t i = 0; i < clonedContentIds.size(); ++i) {
                    if (i > 0) pgBody << ' ';
                    pgBody << clonedContentIds[i] << " 0 R";
                }
                pgBody << "] ";
            }
            pgBody << ">>";
            builder.SetObject(pageId, pgBody.str());
            pageIds.push_back(pageId);
        } else {
            const double w = (std::max)(36.0, pg.widthPt * (pg.crop.right - pg.crop.left));
            const double h = (std::max)(36.0, pg.heightPt * (pg.crop.bottom - pg.crop.top));
            std::map<std::string, std::uint32_t> pageImages;
            std::map<std::string, std::uint32_t> pageExtraFonts;
            std::ostringstream stream;
            EmitPaperPatternOps(stream, w, h, pg.background);
            if (!pg.objects.empty()) {
                stream << "q 1 0 0 -1 0 " << FmtNum(h) << " cm\n";
                for (const auto& obj : pg.objects) {
                    EmitAnnotationObjectOps(stream, obj, doc, builder, pageImages, pageExtraFonts,
                        -pg.widthPt * pg.crop.left, -pg.heightPt * pg.crop.top, true);
                }
                stream << "Q\n";
            }
            const std::string sBytes = stream.str();
            const std::uint32_t cid = builder.AddStream("", std::vector<std::uint8_t>(sBytes.begin(), sBytes.end()), true);

            const std::uint32_t pageId = builder.AllocId();
            std::ostringstream pgBody;
            pgBody << "<</Type /Page /Parent " << pagesRootId << " 0 R "
                   << "/MediaBox [0 0 " << FmtNum(w) << ' ' << FmtNum(h) << "] "
                   << "/Rotate " << (((pg.rotationDeg % 360) + 360) % 360) << " "
                   << "/Resources <</Font <</F1 " << f1 << " 0 R /F2 " << f2 << " 0 R";
            for (const auto& fkv : pageExtraFonts) {
                pgBody << " /" << fkv.first << ' ' << fkv.second << " 0 R";
            }
            pgBody << ">> /ExtGState <</GSAlpha " << gsAlpha << " 0 R>>";
            if (!pageImages.empty()) {
                pgBody << " /XObject <<";
                for (const auto& ikv : pageImages) {
                    pgBody << '/' << ikv.first << ' ' << ikv.second << " 0 R ";
                }
                pgBody << ">>";
            }
            pgBody << ">> /Contents " << cid << " 0 R>>";
            builder.SetObject(pageId, pgBody.str());
            pageIds.push_back(pageId);
        }
    }

    std::ostringstream pagesDict;
    pagesDict << "<</Type /Pages /Count " << pageIds.size() << " /Kids [";
    for (std::size_t i = 0; i < pageIds.size(); ++i) {
        if (i > 0) pagesDict << ' ';
        pagesDict << pageIds[i] << " 0 R";
    }
    pagesDict << "]>>";
    builder.SetObject(pagesRootId, pagesDict.str());

    builder.FinalizeUnicodeFonts();
    const std::uint32_t catalogId = builder.AddObject("<</Type /Catalog /Pages " + std::to_string(pagesRootId) + " 0 R>>");
    outPdfBytes = builder.Finalize(catalogId);
    return true;
}

bool PdfWriter::ExportDocumentToFile(
    const DocumentModel& doc,
    const std::wstring& outPath,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ExportDocumentToBytes(doc, bytes, outErr)) return false;
    return WriteAllBytesToFile(outPath, bytes, outErr);
}

bool PdfWriter::ExportInfiniteCanvasToBytes(
    const DocumentModel& doc,
    const CanvasPdfExportOptions& opts,
    std::vector<std::uint8_t>& outPdfBytes,
    std::string* outErr)
{
    DocRect region{};
    if (opts.hasCustomRegion && !opts.customRegionWorld.Empty()) {
        region = opts.customRegionWorld;
    } else if (!doc.CanvasObjects().empty()) {
        bool first = true;
        double minX = 0, minY = 0, maxX = 0, maxY = 0;
        for (const auto& obj : doc.CanvasObjects()) {
            if (first) {
                minX = obj.bounds.x;
                minY = obj.bounds.y;
                maxX = obj.bounds.Right();
                maxY = obj.bounds.Bottom();
                first = false;
            } else {
                minX = (std::min)(minX, obj.bounds.x);
                minY = (std::min)(minY, obj.bounds.y);
                maxX = (std::max)(maxX, obj.bounds.Right());
                maxY = (std::max)(maxY, obj.bounds.Bottom());
            }
        }
        region = { minX - 24.0, minY - 24.0, (std::max)(200.0, (maxX - minX) + 48.0), (std::max)(200.0, (maxY - minY) + 48.0) };
    } else {
        region = { 0.0, 0.0, opts.pageWidthPt, opts.pageHeightPt };
    }

    PdfBuilder builder;
    const std::uint32_t f1 = builder.AddStandardHelveticaFont(false);
    const std::uint32_t f2 = builder.AddStandardHelveticaFont(true);
    const std::uint32_t gsAlpha = builder.AddHighlightExtGState(0.38);
    const std::uint32_t pagesRootId = builder.AllocId();
    std::vector<std::uint32_t> pageIds;

    std::unordered_map<std::string, std::shared_ptr<PdfDocument>> cardSources;
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, std::uint32_t>> cardRemaps;
    std::unordered_map<std::string, std::uint32_t> builtCardForms;
    std::unordered_map<std::string, std::pair<DocRect, int>> builtCardGeom;

    auto emitSlicePage = [&](const DocRect& sliceRect) {
        const double pw = (std::max)(72.0, sliceRect.w);
        const double ph = (std::max)(72.0, sliceRect.h);
        std::map<std::string, std::uint32_t> xobjects;
        std::map<std::string, std::uint32_t> pageExtraFonts;
        std::ostringstream ss;
        EmitPaperPatternOps(ss, pw, ph, doc.CanvasBackground());
        ss << "q 1 0 0 -1 0 " << FmtNum(ph) << " cm\n";

        const std::vector<std::uint64_t> ids = doc.QueryCanvasRect(sliceRect);
        std::vector<const AnnotationObject*> visible;
        visible.reserve(ids.size());
        for (std::uint64_t id : ids) {
            if (const auto* o = doc.FindCanvasObject(id)) visible.push_back(o);
        }
        std::sort(visible.begin(), visible.end(), [](const AnnotationObject* a, const AnnotationObject* b) {
            return a->zOrder < b->zOrder;
        });

        for (const AnnotationObject* o : visible) {
            if (!doc.IsLayerVisible(o->layerId) || !doc.IsLayerExportable(o->layerId)) continue;
            if (o->kind == ObjectKind::PdfCard) {
                const double cx = o->bounds.x - sliceRect.x;
                const double cy = o->bounds.y - sliceRect.y;
                const double cw = (std::max)(24.0, o->bounds.w);
                const double ch = (std::max)(24.0, o->bounds.h);

                // Card paper fill
                ss << "q 1 1 1 rg " << FmtNum(cx) << ' ' << FmtNum(cy) << ' ' << FmtNum(cw) << ' ' << FmtNum(ch) << " re f\n";

                bool renderedCardContent = false;
                if (o->pdfSourcePageIndex0 >= 0 && (!o->pdfSourcePathUtf8.empty() || !o->pdfSourceAssetKey.empty())) {
                    const std::string cKey = o->pdfSourcePathUtf8 + "\n" + o->pdfSourceAssetKey + "#" + std::to_string(o->pdfSourcePageIndex0);
                    std::uint32_t formId = 0;
                    DocRect srcBox{};
                    int rotDeg = 0;
                    auto fit = builtCardForms.find(cKey);
                    if (fit != builtCardForms.end()) {
                        formId = fit->second;
                        srcBox = builtCardGeom[cKey].first;
                        rotDeg = builtCardGeom[cKey].second;
                    } else if (auto srcDoc = ResolvePdfSourceCached(doc, o->pdfSourcePathUtf8, o->pdfSourceAssetKey, cardSources)) {
                        const std::string rKey = o->pdfSourcePathUtf8 + "\n" + o->pdfSourceAssetKey;
                        if (TryBuildPdfPageFormXObject(*srcDoc, static_cast<std::size_t>(o->pdfSourcePageIndex0), builder, cardRemaps[rKey], formId, srcBox, rotDeg)) {
                            builtCardForms[cKey] = formId;
                            builtCardGeom[cKey] = { srcBox, rotDeg };
                        }
                    }

                    if (formId != 0 && srcBox.w > 0.5 && srcBox.h > 0.5) {
                        const std::string resName = "Fm" + std::to_string(xobjects.size() + 1);
                        xobjects[resName] = formId;
                        const double bw = srcBox.w;
                        const double bh = srcBox.h;
                        double a = 0.0, b = 0.0, c = 0.0, d = 0.0, e = 0.0, f = 0.0;
                        if (rotDeg == 90) {
                            c = cw / bh; e = cx - c * srcBox.y;
                            b = ch / bw; f = cy - b * srcBox.x;
                        } else if (rotDeg == 180) {
                            a = -cw / bw; e = cx + cw - a * srcBox.x;
                            d = ch / bh;  f = cy - d * srcBox.y;
                        } else if (rotDeg == 270) {
                            c = -cw / bh; e = cx + cw - c * srcBox.y;
                            b = -ch / bw; f = cy + ch - b * srcBox.x;
                        } else {
                            a = cw / bw;  e = cx - a * srcBox.x;
                            d = -ch / bh; f = cy + ch - d * srcBox.y;
                        }
                        ss << "q " << FmtNum(cx) << ' ' << FmtNum(cy) << ' ' << FmtNum(cw) << ' ' << FmtNum(ch) << " re W n "
                           << FmtNum(a) << ' ' << FmtNum(b) << ' ' << FmtNum(c) << ' ' << FmtNum(d) << ' ' << FmtNum(e) << ' ' << FmtNum(f)
                           << " cm /" << resName << " Do Q\n";
                        renderedCardContent = true;
                    }
                }

                if (!renderedCardContent) {
                    const std::vector<std::uint8_t>* imgBytes = !o->pngBytes.empty() ? &o->pngBytes : doc.FindAsset(o->assetKey);
                    if (imgBytes && !imgBytes->empty()) {
                        const std::uint32_t imgId = builder.AddImageXObject(*imgBytes);
                        if (imgId != 0) {
                            const std::string resName = "Im" + std::to_string(xobjects.size() + 1);
                            xobjects[resName] = imgId;
                            ss << "q " << FmtNum(cw) << " 0 0 " << FmtNum(-ch) << ' ' << FmtNum(cx) << ' ' << FmtNum(cy + ch)
                               << " cm /" << resName << " Do Q\n";
                            renderedCardContent = true;
                        }
                    }
                }

                // Crisp border + top corner PDF page pill badge
                ss << "0.22 0.30 0.42 RG 0.9 w " << FmtNum(cx) << ' ' << FmtNum(cy) << ' ' << FmtNum(cw) << ' ' << FmtNum(ch) << " re S\n";
                const std::string badge = renderedCardContent
                    ? ("PDF p." + std::to_string(o->pdfSourcePageIndex0 + 1))
                    : ("PDF p." + std::to_string(o->pdfSourcePageIndex0 + 1) + " (sin fuente)");
                ss << "0.14 0.38 0.72 rg " << FmtNum(cx) << ' ' << FmtNum(cy) << ' ' << FmtNum((std::min)(cw, 96.0)) << " 16 re f\n";
                ss << "1 1 1 rg ";
                EmitUnicodeAwareTextLine(ss, badge, cx + 6.0, cy + 12.0, 9.5, true, true, builder, pageExtraFonts);
                if (!o->captionUtf8.empty()) {
                    ss << "0.1 0.1 0.15 rg ";
                    EmitUnicodeAwareTextLine(ss, o->captionUtf8, cx + 6.0, cy + ch + 13.0, 9.5, false, true, builder, pageExtraFonts);
                }
                ss << "Q\n";
            } else {
                EmitAnnotationObjectOps(ss, *o, doc, builder, xobjects, pageExtraFonts, -sliceRect.x, -sliceRect.y, true);
            }
        }
        ss << "Q\n";

        const std::string sStr = ss.str();
        const std::uint32_t cid = builder.AddStream("", std::vector<std::uint8_t>(sStr.begin(), sStr.end()), true);
        const std::uint32_t pageId = builder.AllocId();
        std::ostringstream pg;
        pg << "<</Type /Page /Parent " << pagesRootId << " 0 R /MediaBox [0 0 " << FmtNum(pw) << ' ' << FmtNum(ph) << "] "
           << "/Resources <</Font <</F1 " << f1 << " 0 R /F2 " << f2 << " 0 R";
        for (const auto& fkv : pageExtraFonts) {
            pg << " /" << fkv.first << ' ' << fkv.second << " 0 R";
        }
        pg << ">> /ExtGState <</GSAlpha " << gsAlpha << " 0 R>>";
        if (!xobjects.empty()) {
            pg << " /XObject <<";
            for (const auto& ikv : xobjects) pg << '/' << ikv.first << ' ' << ikv.second << " 0 R ";
            pg << ">>";
        }
        pg << ">> /Contents " << cid << " 0 R>>";
        builder.SetObject(pageId, pg.str());
        pageIds.push_back(pageId);
    };

    if (!opts.paginated) {
        emitSlicePage(region);
    } else {
        const double pw = (std::max)(144.0, opts.pageWidthPt);
        const double ph = (std::max)(144.0, opts.pageHeightPt);
        const std::size_t safetyCap = (opts.maxPages > 0) ? opts.maxPages : 10000u;

        std::set<std::pair<int, int>> occupiedTiles;
        for (const auto& obj : doc.CanvasObjects()) {
            if (!obj.bounds.Intersects(region)) continue;
            const int c0 = (std::max)(0, static_cast<int>(std::floor((obj.bounds.x - region.x) / pw)));
            const int c1 = (std::max)(c0, static_cast<int>(std::floor(((std::max)(obj.bounds.x, obj.bounds.Right() - 1e-4) - region.x) / pw)));
            const int r0 = (std::max)(0, static_cast<int>(std::floor((obj.bounds.y - region.y) / ph)));
            const int r1 = (std::max)(r0, static_cast<int>(std::floor(((std::max)(obj.bounds.y, obj.bounds.Bottom() - 1e-4) - region.y) / ph)));
            for (int rr = r0; rr <= r1; ++rr) {
                for (int cc = c0; cc <= c1; ++cc) {
                    occupiedTiles.insert({ rr, cc });
                }
            }
        }
        if (occupiedTiles.empty()) {
            occupiedTiles.insert({ 0, 0 });
        }
        for (const auto& rc : occupiedTiles) {
            if (pageIds.size() >= safetyCap) break;
            const DocRect tile{ region.x + rc.second * pw, region.y + rc.first * ph, pw, ph };
            emitSlicePage(tile);
        }
    }

    if (pageIds.empty()) {
        if (outErr) *outErr = "Sin contenido exportable en lienzo infinito.";
        return false;
    }

    std::ostringstream pagesDict;
    pagesDict << "<</Type /Pages /Count " << pageIds.size() << " /Kids [";
    for (std::size_t i = 0; i < pageIds.size(); ++i) {
        if (i > 0) pagesDict << ' ';
        pagesDict << pageIds[i] << " 0 R";
    }
    pagesDict << "]>>";
    builder.SetObject(pagesRootId, pagesDict.str());
    builder.FinalizeUnicodeFonts();
    const std::uint32_t catalogId = builder.AddObject("<</Type /Catalog /Pages " + std::to_string(pagesRootId) + " 0 R>>");
    outPdfBytes = builder.Finalize(catalogId);
    return true;
}

bool PdfWriter::ExportInfiniteCanvasToFile(
    const DocumentModel& doc,
    const CanvasPdfExportOptions& opts,
    const std::wstring& outPath,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ExportInfiniteCanvasToBytes(doc, opts, bytes, outErr)) return false;
    return WriteAllBytesToFile(outPath, bytes, outErr);
}

bool PdfWriter::MergePdfFiles(
    const std::vector<std::wstring>& inputPaths,
    const std::wstring& outPath,
    std::string* outErr)
{
    if (inputPaths.empty()) {
        if (outErr) *outErr = "No se especificaron archivos PDF para combinar.";
        return false;
    }
    DocumentModel merged;
    merged.ResetToBlank(DocumentMode::PaginatedPdf);
    merged.MutablePages().clear();

    for (const auto& p : inputPaths) {
        PdfDocument pdf;
        if (!pdf.LoadFromFile(p, outErr)) return false;
        const std::string pUtf8 = WideToUtf8(p);
        for (const auto& pi : pdf.Pages()) {
            PageModel pm;
            pm.pageId = merged.AllocatePageId();
            const DocRect box = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
            pm.widthPt = (std::max)(36.0, box.w);
            pm.heightPt = (std::max)(36.0, box.h);
            pm.rotationDeg = pi.rotationDeg;
            pm.hasPdfSource = true;
            pm.sourcePdfPathUtf8 = pUtf8;
            pm.sourcePageIndex0 = static_cast<int>(pi.pageIndex0);
            pm.extractedTextUtf8 = pi.extractedTextUtf8;
            merged.MutablePages().push_back(std::move(pm));
        }
    }
    merged.RebuildLayout();
    return ExportDocumentToFile(merged, outPath, outErr);
}

bool PdfWriter::SplitPdfFile(
    const std::wstring& inputPath,
    const std::vector<std::size_t>& zeroBasedPages,
    const std::wstring& outPath,
    std::string* outErr)
{
    PdfDocument pdf;
    if (!pdf.LoadFromFile(inputPath, outErr)) return false;

    DocumentModel subset;
    subset.ResetToBlank(DocumentMode::PaginatedPdf);
    subset.MutablePages().clear();
    const std::string pUtf8 = WideToUtf8(inputPath);

    for (std::size_t idx : zeroBasedPages) {
        if (idx >= pdf.PageCount()) continue;
        const auto& pi = pdf.PageAt(idx);
        PageModel pm;
        pm.pageId = subset.AllocatePageId();
        const DocRect box = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
        pm.widthPt = (std::max)(36.0, box.w);
        pm.heightPt = (std::max)(36.0, box.h);
        pm.rotationDeg = pi.rotationDeg;
        pm.hasPdfSource = true;
        pm.sourcePdfPathUtf8 = pUtf8;
        pm.sourcePageIndex0 = static_cast<int>(pi.pageIndex0);
        pm.extractedTextUtf8 = pi.extractedTextUtf8;
        subset.MutablePages().push_back(std::move(pm));
    }
    if (subset.Pages().empty()) {
        if (outErr) *outErr = "Ninguna p\xC3\xA1gina v\xC3\xA1lida seleccionada para dividir.";
        return false;
    }
    subset.RebuildLayout();
    return ExportDocumentToFile(subset, outPath, outErr);
}

bool PdfWriter::TryDirectReplaceTextInPdfBytes(
    const std::vector<std::uint8_t>& srcPdfBytes,
    std::size_t pageIndex0,
    const std::string& oldTextUtf8,
    const std::string& newTextUtf8,
    std::vector<std::uint8_t>& outPdfBytes,
    std::string* outErr)
{
    PdfDocument doc;
    if (!doc.LoadFromBytes(srcPdfBytes, outErr)) return false;
    const auto cap = doc.InspectTextReplacement(pageIndex0, oldTextUtf8);
    if (!cap.supportedDirectStream) {
        if (outErr) *outErr = cap.diagnosticUtf8;
        return false;
    }

    const std::string oldLit = EscapePdfLiteralString(Utf8ToWinAnsi(oldTextUtf8));
    const std::string newLit = EscapePdfLiteralString(Utf8ToWinAnsi(newTextUtf8));

    // Rewrite PDF cleanly replacing matching page's decompressed content stream while preserving Catalog metadata & Outlines
    PdfBuilder builder;
    std::unordered_map<std::uint32_t, std::uint32_t> remap;
    const std::uint32_t pagesRootId = builder.AllocId();
    std::vector<std::uint32_t> pageIds;
    pageIds.reserve(doc.PageCount());

    for (std::size_t i = 0; i < doc.PageCount(); ++i) {
        const std::uint32_t newPageId = builder.AllocId();
        pageIds.push_back(newPageId);
        remap[doc.PageAt(i).pageRef.obj] = newPageId;
    }

    for (std::size_t i = 0; i < doc.PageCount(); ++i) {
        const auto& pi = doc.PageAt(i);
        const PdfValue pageVal = doc.LookupObject(pi.pageRef.obj);
        PdfValue rewrittenPage = builder.RewriteValueRefs(doc, pageVal, remap, 1);
        PdfDict d = rewrittenPage.dictVal;

        if (i == pageIndex0) {
            std::vector<std::uint8_t> raw = doc.GetPageContentBytes(i);
            std::string txt(reinterpret_cast<const char*>(raw.data()), raw.size());
            std::size_t pos = 0;
            std::size_t replaced = 0;
            while ((pos = txt.find(oldLit, pos)) != std::string::npos) {
                txt.replace(pos, oldLit.size(), newLit);
                pos += newLit.size();
                ++replaced;
            }
            if (replaced == 0) {
                if (outErr) *outErr = "Cadena literal no reemplazada en stream.";
                return false;
            }
            const std::uint32_t newStreamId = builder.AddStream("", std::vector<std::uint8_t>(txt.begin(), txt.end()), true);
            PdfValue cref;
            cref.kind = PdfValue::Kind::Ref;
            cref.refVal = { newStreamId, 0 };
            d["Contents"] = cref;
        }

        PdfValue parentRef;
        parentRef.kind = PdfValue::Kind::Ref;
        parentRef.refVal = { pagesRootId, 0 };
        d["Parent"] = parentRef;

        PdfValue dictV;
        dictV.kind = PdfValue::Kind::Dict;
        dictV.dictVal = std::move(d);
        builder.SetObject(pageIds[i], PdfDocument::SerializeValue(dictV));
    }

    std::ostringstream pagesDict;
    pagesDict << "<</Type /Pages /Count " << pageIds.size() << " /Kids [";
    for (std::size_t i = 0; i < pageIds.size(); ++i) {
        if (i > 0) pagesDict << ' ';
        pagesDict << pageIds[i] << " 0 R";
    }
    pagesDict << "]>>";
    builder.SetObject(pagesRootId, pagesDict.str());

    std::uint32_t catId = 0;
    if (doc.CatalogRef().obj != 0) {
        const PdfValue origCat = doc.LookupObject(doc.CatalogRef().obj);
        if (origCat.IsDict()) {
            catId = builder.AllocId();
            remap[doc.CatalogRef().obj] = catId;
            PdfValue rewrittenCat = builder.RewriteValueRefs(doc, origCat, remap, 1);
            PdfDict cd = std::move(rewrittenCat.dictVal);
            PdfValue typeName;
            typeName.kind = PdfValue::Kind::Name;
            typeName.strVal = "Catalog";
            cd["Type"] = typeName;
            PdfValue pagesRef;
            pagesRef.kind = PdfValue::Kind::Ref;
            pagesRef.refVal = { pagesRootId, 0 };
            cd["Pages"] = pagesRef;
            PdfValue catV;
            catV.kind = PdfValue::Kind::Dict;
            catV.dictVal = std::move(cd);
            builder.SetObject(catId, PdfDocument::SerializeValue(catV));
        }
    }
    if (catId == 0) {
        catId = builder.AddObject("<</Type /Catalog /Pages " + std::to_string(pagesRootId) + " 0 R>>");
    }
    outPdfBytes = builder.Finalize(catId);
    return true;
}

} // namespace DocStudio
