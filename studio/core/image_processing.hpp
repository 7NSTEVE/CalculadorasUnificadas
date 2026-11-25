#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../tinta_core/perspectiva.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace DocStudio::ImageProc {

struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // 4 bytes/pixel (R, G, B, A)
    bool Valid() const noexcept {
        return width > 0 && height > 0 && rgba.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    }
};

// 4-point 3x3 perspective rectification using CuadernU's unified tinta::CalcularMatrizPerspectiva (F-104)
inline RgbaImage RectifyPerspectiveQuad(
    const RgbaImage& src,
    const std::array<tinta::Punto, 4>& srcQuadPx,
    int outW,
    int outH)
{
    RgbaImage dst{};
    if (!src.Valid() || outW <= 0 || outH <= 0) return dst;
    dst.width = outW;
    dst.height = outH;
    dst.rgba.assign(static_cast<std::size_t>(outW) * static_cast<std::size_t>(outH) * 4u, 255u);

    const std::array<tinta::Punto, 4> dstRect{
        tinta::Punto{ 0.0f, 0.0f },
        tinta::Punto{ static_cast<float>(outW - 1), 0.0f },
        tinta::Punto{ static_cast<float>(outW - 1), static_cast<float>(outH - 1) },
        tinta::Punto{ 0.0f, static_cast<float>(outH - 1) }
    };
    // Map destination pixel -> source pixel via inverse homography
    const tinta::MatrizPerspectiva invH = tinta::CalcularMatrizPerspectiva(dstRect, srcQuadPx);

    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            const tinta::Punto sp = tinta::AplicarPerspectiva(invH, { static_cast<float>(x), static_cast<float>(y) });
            const int x0 = static_cast<int>(std::floor(sp.x));
            const int y0 = static_cast<int>(std::floor(sp.y));
            const float fx = sp.x - static_cast<float>(x0);
            const float fy = sp.y - static_cast<float>(y0);

            auto sample = [&](int sx, int sy, int c) -> float {
                sx = std::clamp(sx, 0, src.width - 1);
                sy = std::clamp(sy, 0, src.height - 1);
                return static_cast<float>(src.rgba[(static_cast<std::size_t>(sy) * src.width + sx) * 4u + c]);
            };

            const std::size_t dIdx = (static_cast<std::size_t>(y) * outW + x) * 4u;
            for (int c = 0; c < 4; ++c) {
                const float v00 = sample(x0, y0, c);
                const float v10 = sample(x0 + 1, y0, c);
                const float v01 = sample(x0, y0 + 1, c);
                const float v11 = sample(x0 + 1, y0 + 1, c);
                const float v0 = v00 + (v10 - v00) * fx;
                const float v1 = v01 + (v11 - v01) * fx;
                const float v = v0 + (v1 - v0) * fy;
                dst.rgba[dIdx + c] = static_cast<std::uint8_t>(std::clamp(std::lround(v), 0l, 255l));
            }
        }
    }
    return dst;
}

// Whiteboard lighting normalization & contrast stretch (F-104)
inline void EnhanceWhiteboardLighting(RgbaImage& img, float blackPoint = 0.14f, float whitePoint = 0.86f) {
    if (!img.Valid()) return;
    tinta::CorreccionContraste cc{};
    cc.nivel_negro = blackPoint;
    cc.nivel_blanco = whitePoint;
    for (std::size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        for (int c = 0; c < 3; ++c) {
            const float norm = static_cast<float>(img.rgba[i + c]) / 255.0f;
            const float adj = tinta::AplicarCorreccionContraste(norm, cc);
            img.rgba[i + c] = static_cast<std::uint8_t>(std::clamp(std::lround(adj * 255.0f), 0l, 255l));
        }
    }
}

// Transparent background removal for white paper / whiteboard photos (F-105)
inline void RemovePaperBackground(RgbaImage& img, std::uint8_t lumaThreshold = 220) {
    if (!img.Valid()) return;
    for (std::size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const float r = static_cast<float>(img.rgba[i]);
        const float g = static_cast<float>(img.rgba[i + 1]);
        const float b = static_cast<float>(img.rgba[i + 2]);
        const float luma = 0.299f * r + 0.587f * g + 0.114f * b;
        const float chroma = (std::max)({ r, g, b }) - (std::min)({ r, g, b });
        if (luma >= static_cast<float>(lumaThreshold) && chroma < 28.0f) {
            img.rgba[i + 3] = 0u;
        } else if (luma >= static_cast<float>(lumaThreshold - 28) && chroma < 34.0f) {
            const float t = (static_cast<float>(lumaThreshold) - luma) / 28.0f;
            img.rgba[i + 3] = static_cast<std::uint8_t>(std::clamp(std::lround(t * img.rgba[i + 3]), 0l, 255l));
        }
    }
}

// Lightweight native SVG rasterizer for simple <rect>, <circle>, <ellipse>, <line> SVGs (F-100)
inline bool RasterizeSimpleSvgToRgba(const std::string& svgXml, RgbaImage& outImg, int targetW = 512, int targetH = 384) {
    if (svgXml.find("<svg") == std::string::npos) return false;
    auto findAttr = [](const std::string& tag, const std::string& name, double defVal) -> double {
        const std::string key = name + "=\"";
        const std::size_t p = tag.find(key);
        if (p == std::string::npos) return defVal;
        const std::size_t s = p + key.size();
        const std::size_t e = tag.find('"', s);
        if (e == std::string::npos) return defVal;
        try { return std::stod(tag.substr(s, e - s)); } catch (...) { return defVal; }
    };
    auto parseHexColor = [](const std::string& tag, const std::string& name, std::uint32_t defArgb) -> std::uint32_t {
        const std::string key = name + "=\"#";
        const std::size_t p = tag.find(key);
        if (p == std::string::npos) return defArgb;
        const std::size_t s = p + key.size();
        if (s + 6 > tag.size()) return defArgb;
        try {
            const unsigned long rgb = std::stoul(tag.substr(s, 6), nullptr, 16);
            return 0xFF000000u | static_cast<std::uint32_t>(rgb & 0xFFFFFFu);
        } catch (...) { return defArgb; }
    };

    double vbW = 300.0, vbH = 200.0;
    {
        const std::size_t svgEnd = svgXml.find('>', svgXml.find("<svg"));
        if (svgEnd != std::string::npos) {
            const std::string hdr = svgXml.substr(0, svgEnd + 1);
            vbW = (std::max)(16.0, findAttr(hdr, "width", 300.0));
            vbH = (std::max)(16.0, findAttr(hdr, "height", 200.0));
        }
    }
    outImg.width = std::clamp(targetW, 64, 1024);
    outImg.height = std::clamp(targetH, 64, 1024);
    outImg.rgba.assign(static_cast<std::size_t>(outImg.width) * outImg.height * 4u, 0u);
    const double sx = static_cast<double>(outImg.width) / vbW;
    const double sy = static_cast<double>(outImg.height) / vbH;

    auto setPx = [&](int x, int y, std::uint32_t argb) {
        if (x < 0 || y < 0 || x >= outImg.width || y >= outImg.height) return;
        const std::size_t idx = (static_cast<std::size_t>(y) * outImg.width + x) * 4u;
        outImg.rgba[idx + 0] = static_cast<std::uint8_t>((argb >> 16) & 0xFFu);
        outImg.rgba[idx + 1] = static_cast<std::uint8_t>((argb >> 8) & 0xFFu);
        outImg.rgba[idx + 2] = static_cast<std::uint8_t>(argb & 0xFFu);
        outImg.rgba[idx + 3] = static_cast<std::uint8_t>((argb >> 24) & 0xFFu);
    };

    std::size_t pos = 0;
    while (pos < svgXml.size()) {
        const std::size_t lt = svgXml.find('<', pos);
        if (lt == std::string::npos) break;
        const std::size_t gt = svgXml.find('>', lt);
        if (gt == std::string::npos) break;
        const std::string tag = svgXml.substr(lt, gt - lt + 1);
        pos = gt + 1;

        if (tag.rfind("<rect", 0) == 0) {
            const int x0 = static_cast<int>(findAttr(tag, "x", 0.0) * sx);
            const int y0 = static_cast<int>(findAttr(tag, "y", 0.0) * sy);
            const int rw = static_cast<int>(findAttr(tag, "width", 40.0) * sx);
            const int rh = static_cast<int>(findAttr(tag, "height", 40.0) * sy);
            const std::uint32_t col = parseHexColor(tag, "fill", 0xFF2563EBu);
            for (int y = y0; y < y0 + rh; ++y)
                for (int x = x0; x < x0 + rw; ++x) setPx(x, y, col);
        } else if (tag.rfind("<circle", 0) == 0) {
            const double cx = findAttr(tag, "cx", vbW * 0.5) * sx;
            const double cy = findAttr(tag, "cy", vbH * 0.5) * sy;
            const double r = findAttr(tag, "r", 24.0) * ((sx + sy) * 0.5);
            const std::uint32_t col = parseHexColor(tag, "fill", 0xFF0EA5E9u);
            for (int y = static_cast<int>(cy - r); y <= static_cast<int>(cy + r); ++y)
                for (int x = static_cast<int>(cx - r); x <= static_cast<int>(cx + r); ++x)
                    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) setPx(x, y, col);
        } else if (tag.rfind("<line", 0) == 0) {
            const double x1 = findAttr(tag, "x1", 0.0) * sx;
            const double y1 = findAttr(tag, "y1", 0.0) * sy;
            const double x2 = findAttr(tag, "x2", vbW) * sx;
            const double y2 = findAttr(tag, "y2", vbH) * sy;
            const std::uint32_t col = parseHexColor(tag, "stroke", 0xFF1E293Bu);
            const int steps = (std::max)(1, static_cast<int>(std::hypot(x2 - x1, y2 - y1) * 1.5));
            for (int s = 0; s <= steps; ++s) {
                const double t = static_cast<double>(s) / static_cast<double>(steps);
                const int px = static_cast<int>(std::lround(x1 + (x2 - x1) * t));
                const int py = static_cast<int>(std::lround(y1 + (y2 - y1) * t));
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) setPx(px + dx, py + dy, col);
            }
        }
    }
    return true;
}

#ifdef _WIN32
// Decode PNG/JPEG/BMP/TIFF/WEBP/GIF bytes to RGBA via Windows Imaging Component (WIC)
inline bool DecodeImageBytesWic(const std::uint8_t* bytes, std::size_t size, RgbaImage& outImg) {
    outImg = {};
    if (!bytes || size == 0) return false;
    // Detect SVG XML directly
    if (size >= 4 && size < 4u * 1024u * 1024u) {
        const std::string probe(reinterpret_cast<const char*>(bytes), (std::min<std::size_t>)(size, 256u));
        if (probe.find("<svg") != std::string::npos || probe.find("<?xml") != std::string::npos) {
            std::string full(reinterpret_cast<const char*>(bytes), size);
            if (RasterizeSimpleSvgToRgba(full, outImg)) return true;
        }
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        return false;
    }
    Microsoft::WRL::ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream))) return false;
    if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes), static_cast<DWORD>(size)))) return false;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) return false;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;

    UINT w = 0, h = 0;
    if (FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0 || w > 16384 || h > 16384) return false;
    Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
    if (FAILED(factory->CreateFormatConverter(&conv))) return false;
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        return false;
    }

    outImg.width = static_cast<int>(w);
    outImg.height = static_cast<int>(h);
    outImg.rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
    if (FAILED(conv->CopyPixels(nullptr, w * 4u, static_cast<UINT>(outImg.rgba.size()), outImg.rgba.data()))) {
        outImg = {};
        return false;
    }
    return true;
}

inline bool EncodeRgbaToPngWic(const RgbaImage& img, std::vector<std::uint8_t>& outPngBytes) {
    outPngBytes.clear();
    if (!img.Valid()) return false;

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;

    Microsoft::WRL::ComPtr<IStream> memStream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream))) return false;

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))) return false;
    if (FAILED(encoder->Initialize(memStream.Get(), WICBitmapEncoderNoCache))) return false;

    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    if (FAILED(encoder->CreateNewFrame(&frame, nullptr))) return false;
    if (FAILED(frame->Initialize(nullptr))) return false;
    if (FAILED(frame->SetSize(static_cast<UINT>(img.width), static_cast<UINT>(img.height)))) return false;

    WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&pf))) return false;

    std::vector<std::uint8_t> bgra = img.rgba;
    for (std::size_t i = 0; i + 3 < bgra.size(); i += 4) {
        std::swap(bgra[i], bgra[i + 2]);
    }
    if (FAILED(frame->WritePixels(static_cast<UINT>(img.height), static_cast<UINT>(img.width) * 4u,
        static_cast<UINT>(bgra.size()), bgra.data()))) {
        return false;
    }
    if (FAILED(frame->Commit()) || FAILED(encoder->Commit())) return false;

    HGLOBAL hMem = nullptr;
    if (FAILED(GetHGlobalFromStream(memStream.Get(), &hMem)) || !hMem) return false;
    const SIZE_T len = GlobalSize(hMem);
    const void* ptr = GlobalLock(hMem);
    if (!ptr || len == 0) return false;
    outPngBytes.resize(len);
    std::memcpy(outPngBytes.data(), ptr, len);
    GlobalUnlock(hMem);
    return true;
}

// Capture any Win32 HDC rectangle (srcX, srcY, w, h) into PNG bytes via BitBlt + WIC (ON-EDIT-041)
inline bool CaptureDcRegionToPngBytes(
    HDC hSrc,
    int srcX,
    int srcY,
    int w,
    int h,
    std::vector<std::uint8_t>& outPngBytes,
    int& outW,
    int& outH)
{
    outPngBytes.clear();
    outW = 0;
    outH = 0;
    if (!hSrc || w <= 0 || h <= 0) return false;
    HDC hMem = CreateCompatibleDC(hSrc);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hSrc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (hMem && hBmp && bits) {
        HGDIOBJ old = SelectObject(hMem, hBmp);
        const BOOL bltOk = BitBlt(hMem, 0, 0, w, h, hSrc, srcX, srcY, SRCCOPY | CAPTUREBLT)
                        || BitBlt(hMem, 0, 0, w, h, hSrc, srcX, srcY, SRCCOPY);
        if (bltOk) {
            RgbaImage img{};
            img.width = w;
            img.height = h;
            img.rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
            const auto* src = static_cast<const std::uint8_t*>(bits);
            for (std::size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
                img.rgba[i + 0] = src[i + 2]; // R
                img.rgba[i + 1] = src[i + 1]; // G
                img.rgba[i + 2] = src[i + 0]; // B
                img.rgba[i + 3] = 255u;       // A
            }
            const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            ok = EncodeRgbaToPngWic(img, outPngBytes);
            if (SUCCEEDED(hrCo)) CoUninitialize();
            if (ok) {
                outW = w;
                outH = h;
            }
        }
        SelectObject(hMem, old);
    }
    if (hBmp) DeleteObject(hBmp);
    if (hMem) DeleteDC(hMem);
    return ok;
}

// Multi-monitor safe desktop virtual screen capture -> PNG (ON-EDIT-041)
inline bool CaptureVirtualScreenPngBytes(
    std::vector<std::uint8_t>& outPngBytes,
    int& outW,
    int& outH)
{
    outPngBytes.clear();
    outW = 0;
    outH = 0;
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = (std::max)(1, GetSystemMetrics(SM_CXVIRTUALSCREEN));
    const int vh = (std::max)(1, GetSystemMetrics(SM_CYVIRTUALSCREEN));
    HDC hScr = GetDC(nullptr);
    if (!hScr) return false;
    const bool ok = CaptureDcRegionToPngBytes(hScr, vx, vy, vw, vh, outPngBytes, outW, outH);
    ReleaseDC(nullptr, hScr);
    return ok;
}
#endif

} // namespace DocStudio::ImageProc
