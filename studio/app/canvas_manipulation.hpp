#pragma once

#include "../core/camera.hpp"
#include "../core/command_stack.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/image_processing.hpp"
#include "../core/ink_engine.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace DocStudio::CanvasManip {

enum class HandleKind {
    None = 0,
    Body,
    NW, N, NE, E, SE, S, SW, W,
    Rotate
};

struct ActiveTransformDrag {
    bool active = false;
    HandleKind handle = HandleKind::None;
    ObjectTarget target{};
    std::vector<std::uint64_t> objectIds;
    std::vector<AnnotationObject> beforeSnapshots;
    DocPoint startWorld{};
    DocRect initialBounds{};
    double initialAngleRad = 0.0;
    double initialRotDeg = 0.0;
    bool is3DRotateDrag = false;
    double startYawDeg = 42.0;
    double startPitchDeg = 28.0;
};

inline const AnnotationObject* FindObjectConst(const DocumentModel& doc, ObjectTarget target, std::uint64_t id) {
    if (id == 0) return nullptr;
    if (target.isCanvas) return doc.FindCanvasObject(id);
    if (target.pageIndex < doc.Pages().size()) {
        for (const auto& o : doc.Pages()[target.pageIndex].objects) {
            if (o.id == id) return &o;
        }
    }
    return nullptr;
}

inline AnnotationObject* FindObjectMut(DocumentModel& doc, ObjectTarget target, std::uint64_t id) {
    if (id == 0) return nullptr;
    if (target.isCanvas) return doc.FindCanvasObjectMutable(id);
    if (target.pageIndex < doc.Pages().size()) {
        return doc.FindPageObjectMutable(target.pageIndex, id);
    }
    return nullptr;
}

inline HandleKind HitTestSelectionHandles(
    const DocRect& objLocalBounds,
    double originWorldX,
    double originWorldY,
    const Camera2D& cam,
    int mouseX,
    int mouseY)
{
    const DocRect scr = cam.WorldRectToScreen({ originWorldX + objLocalBounds.x, originWorldY + objLocalBounds.y, objLocalBounds.w, objLocalBounds.h });
    const double left = scr.x - 3.0;
    const double top = scr.y - 3.0;
    const double right = scr.x + scr.w + 3.0;
    const double bottom = scr.y + scr.h + 3.0;
    const double mx = (left + right) * 0.5;
    const double my = (top + bottom) * 0.5;
    constexpr double kTol = 9.0;

    auto nearPt = [&](double px, double py) {
        return std::hypot(static_cast<double>(mouseX) - px, static_cast<double>(mouseY) - py) <= kTol;
    };

    if (nearPt(mx, top - 19.0)) return HandleKind::Rotate;
    if (nearPt(left, top))      return HandleKind::NW;
    if (nearPt(mx, top))        return HandleKind::N;
    if (nearPt(right, top))     return HandleKind::NE;
    if (nearPt(right, my))      return HandleKind::E;
    if (nearPt(right, bottom))  return HandleKind::SE;
    if (nearPt(mx, bottom))     return HandleKind::S;
    if (nearPt(left, bottom))   return HandleKind::SW;
    if (nearPt(left, my))       return HandleKind::W;
    if (mouseX >= left && mouseX <= right && mouseY >= top && mouseY <= bottom) return HandleKind::Body;
    return HandleKind::None;
}

inline void TranslateAnnotationObject(AnnotationObject& o, double dx, double dy) {
    if (o.kind == ObjectKind::InkStroke) {
        for (auto& pt : o.inkPoints) {
            pt.x += dx;
            pt.y += dy;
        }
    }
    o.shapeStart.x += dx;
    o.shapeStart.y += dy;
    o.shapeEnd.x += dx;
    o.shapeEnd.y += dy;
    o.bounds.x += dx;
    o.bounds.y += dy;
    o.RecomputeBounds();
}

inline void ResizeAnnotationObject(AnnotationObject& o, const AnnotationObject& orig, const DocRect& newBounds) {
    const double oldW = (std::max)(2.0, orig.bounds.w);
    const double oldH = (std::max)(2.0, orig.bounds.h);
    const double sx = (std::max)(12.0, newBounds.w) / oldW;
    const double sy = (std::max)(12.0, newBounds.h) / oldH;

    o = orig;
    o.bounds = { newBounds.x, newBounds.y, (std::max)(12.0, newBounds.w), (std::max)(12.0, newBounds.h) };

    if (o.kind == ObjectKind::InkStroke) {
        for (std::size_t i = 0; i < o.inkPoints.size(); ++i) {
            o.inkPoints[i].x = o.bounds.x + (orig.inkPoints[i].x - orig.bounds.x) * sx;
            o.inkPoints[i].y = o.bounds.y + (orig.inkPoints[i].y - orig.bounds.y) * sy;
        }
        o.strokeWidth = std::clamp(orig.strokeWidth * std::sqrt(sx * sy), 0.6, 48.0);
        o.RecomputeBounds();
    } else if (o.kind == ObjectKind::Shape &&
               (o.shapeKind == ShapeKind::Line || o.shapeKind == ShapeKind::Arrow || o.shapeKind == ShapeKind::Connector)) {
        o.shapeStart.x = o.bounds.x + (orig.shapeStart.x - orig.bounds.x) * sx;
        o.shapeStart.y = o.bounds.y + (orig.shapeStart.y - orig.bounds.y) * sy;
        o.shapeEnd.x = o.bounds.x + (orig.shapeEnd.x - orig.bounds.x) * sx;
        o.shapeEnd.y = o.bounds.y + (orig.shapeEnd.y - orig.bounds.y) * sy;
        o.RecomputeBounds();
    }
}

inline DocRect ComputeResizedBounds(HandleKind h, const DocRect& orig, double dx, double dy) {
    double l = orig.x, t = orig.y, r = orig.Right(), b = orig.Bottom();
    if (h == HandleKind::NW || h == HandleKind::W || h == HandleKind::SW) l += dx;
    if (h == HandleKind::NE || h == HandleKind::E || h == HandleKind::SE) r += dx;
    if (h == HandleKind::NW || h == HandleKind::N || h == HandleKind::NE) t += dy;
    if (h == HandleKind::SW || h == HandleKind::S || h == HandleKind::SE) b += dy;
    if (r - l < 18.0) {
        if (h == HandleKind::NW || h == HandleKind::W || h == HandleKind::SW) l = r - 18.0;
        else r = l + 18.0;
    }
    if (b - t < 16.0) {
        if (h == HandleKind::NW || h == HandleKind::N || h == HandleKind::NE) t = b - 16.0;
        else b = t + 16.0;
    }
    return { l, t, r - l, b - t };
}

inline void ExpandSelectionWithGroupIds(
    const DocumentModel& doc,
    ObjectTarget target,
    std::vector<std::uint64_t>& ids)
{
    std::vector<std::uint64_t> groups;
    for (std::uint64_t id : ids) {
        if (const auto* o = FindObjectConst(doc, target, id)) {
            if (o->groupId != 0 && std::find(groups.begin(), groups.end(), o->groupId) == groups.end()) {
                groups.push_back(o->groupId);
            }
        }
    }
    if (groups.empty()) return;
    const auto* vec = target.isCanvas ? &doc.CanvasObjects()
        : (target.pageIndex < doc.Pages().size() ? &doc.Pages()[target.pageIndex].objects : nullptr);
    if (!vec) return;
    for (const auto& o : *vec) {
        if (o.groupId != 0 && std::find(groups.begin(), groups.end(), o.groupId) != groups.end()) {
            if (std::find(ids.begin(), ids.end(), o.id) == ids.end()) ids.push_back(o.id);
        }
    }
}

inline bool GroupSelectedObjects(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::vector<std::uint64_t>& ids,
    bool ungroup)
{
    if (ids.empty()) return false;
    const std::uint64_t newGroupId = ungroup ? 0ull : doc.AllocateObjectId();
    std::vector<AnnotationObject> before, after;
    for (std::uint64_t id : ids) {
        if (const auto* o = FindObjectConst(doc, target, id)) {
            before.push_back(*o);
            AnnotationObject mod = *o;
            mod.groupId = newGroupId;
            after.push_back(std::move(mod));
        }
    }
    if (before.empty()) return false;
    return cmdStack.Execute(doc, std::make_unique<BatchModifyObjectsCommand>(
        target, std::move(before), std::move(after), ungroup ? "Desagrupar" : "Agrupar"));
}

inline std::vector<std::uint64_t> DuplicateSelectedObjects(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::vector<std::uint64_t>& ids)
{
    std::vector<std::uint64_t> createdIds;
    for (std::uint64_t id : ids) {
        if (const auto* o = FindObjectConst(doc, target, id)) {
            AnnotationObject copy = *o;
            copy.id = 0;
            copy.zOrder = 0;
            TranslateAnnotationObject(copy, 24.0, 24.0);
            if (cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(copy)))) {
                const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
                if (!vec.empty()) createdIds.push_back(vec.back().id);
            }
        }
    }
    return createdIds;
}

inline bool IsPlot3DObject(const AnnotationObject& obj) {
    if (obj.kind != ObjectKind::PlotBlock || obj.plotConfigUtf8.empty()) return false;
    return obj.plotConfigUtf8.find("\"is3D\":true") != std::string::npos ||
           obj.plotConfigUtf8.find("\"mode\":\"3d\"") != std::string::npos;
}

inline bool UpdatePlot3DRotation(AnnotationObject& obj, double yawDeg, double pitchDeg) {
    if (obj.kind != ObjectKind::PlotBlock || obj.plotConfigUtf8.empty()) return false;
    try {
        auto j = nlohmann::json::parse(obj.plotConfigUtf8);
        if (!j.is_object()) return false;
        j["yawDeg"] = std::fmod(yawDeg + 3600.0, 360.0);
        j["pitchDeg"] = std::clamp(pitchDeg, -85.0, 85.0);
        obj.plotConfigUtf8 = j.dump();
        return true;
    } catch (...) {
        return false;
    }
}

inline const InkLink* HitTestInkLink(
    const DocumentModel& doc,
    std::size_t pageIdx,
    DocPoint pageLocal,
    double pageW,
    double pageH)
{
    for (const auto& lk : doc.InkLinks()) {
        if (lk.sourcePageIndex0 != static_cast<int>(pageIdx)) continue;
        const DocRect r{
            lk.sourceRectNorm.left * pageW,
            lk.sourceRectNorm.top * pageH,
            (std::max)(80.0, (lk.sourceRectNorm.right - lk.sourceRectNorm.left) * pageW),
            (std::max)(20.0, (lk.sourceRectNorm.bottom - lk.sourceRectNorm.top) * pageH)
        };
        if (r.Contains(pageLocal.x, pageLocal.y)) return &lk;
    }
    return nullptr;
}

inline const PdfLinkRect* HitTestPdfLink(
    const PageModel& pg,
    DocPoint pageLocal,
    double pageW,
    double pageH)
{
    for (const auto& pl : pg.pdfLinks) {
        const DocRect r{ pl.rectNorm.x * pageW, pl.rectNorm.y * pageH, pl.rectNorm.w * pageW, pl.rectNorm.h * pageH };
        if (r.Contains(pageLocal.x, pageLocal.y)) return &pl;
    }
    return nullptr;
}

inline int HitTestFormField(
    const PageModel& pg,
    DocPoint pageLocal,
    double pageW,
    double pageH)
{
    for (std::size_t i = 0; i < pg.formFields.size(); ++i) {
        const auto& ff = pg.formFields[i];
        const DocRect r{ ff.rectNorm.x * pageW, ff.rectNorm.y * pageH, ff.rectNorm.w * pageW, ff.rectNorm.h * pageH };
        if (r.Contains(pageLocal.x, pageLocal.y)) return static_cast<int>(i);
    }
    return -1;
}

// Extract selected PDF text within page-local rectangle (F-075, F-076)
inline std::string ExtractPdfTextInRect(
    const PageModel& pg,
    const DocRect& selLocal,
    double pageW,
    double pageH,
    std::vector<DocRect>* outQuadRectsNorm = nullptr)
{
    if (outQuadRectsNorm) outQuadRectsNorm->clear();
    const double nx0 = std::clamp(selLocal.x / (std::max)(1.0, pageW), 0.0, 1.0);
    const double ny0 = std::clamp(selLocal.y / (std::max)(1.0, pageH), 0.0, 1.0);
    const double nx1 = std::clamp(selLocal.Right() / (std::max)(1.0, pageW), 0.0, 1.0);
    const double ny1 = std::clamp(selLocal.Bottom() / (std::max)(1.0, pageH), 0.0, 1.0);
    const DocRect normSel{ nx0, ny0, nx1 - nx0, ny1 - ny0 };

    std::string out;
    for (const auto& tb : pg.textBoxes) {
        if (tb.rectNorm.Intersects(normSel)) {
            if (!out.empty()) out.push_back(' ');
            out += tb.textUtf8;
            if (outQuadRectsNorm) outQuadRectsNorm->push_back(tb.rectNorm);
        }
    }
    if (out.empty() && !pg.extractedTextUtf8.empty()) {
        // Graceful fallback if PDF content stream didn't expose positioned Td/Tm boxes
        out = pg.extractedTextUtf8.substr(0, (std::min<std::size_t>)(pg.extractedTextUtf8.size(), 512u));
        if (outQuadRectsNorm) outQuadRectsNorm->push_back(normSel);
    }
    return out;
}

#ifdef _WIN32
// Masks a BGRA32 raster image against a freehand or circular polygon in world coordinates,
// making pixels outside the loop transparent and drawing a subtle 1.5px anti-aliased border.
inline void ApplyPolygonMaskToBgra32(
    RasterImageBgra32& img,
    const DocRect& worldRect,
    const std::vector<DocPoint>& polyWorld)
{
    if (!img.Valid() || polyWorld.size() < 3 || worldRect.w <= 1.0 || worldRect.h <= 1.0) return;
    const int w = img.width;
    const int h = img.height;
    const double sx = static_cast<double>(w) / worldRect.w;
    const double sy = static_cast<double>(h) / worldRect.h;

    std::vector<DocPoint> polyPx;
    polyPx.reserve(polyWorld.size());
    for (const auto& p : polyWorld) {
        polyPx.push_back({ (p.x - worldRect.x) * sx, (p.y - worldRect.y) * sy });
    }
    const std::size_t n = polyPx.size();

    for (int y = 0; y < h; ++y) {
        const double py = static_cast<double>(y) + 0.5;
        for (int x = 0; x < w; ++x) {
            const double px = static_cast<double>(x) + 0.5;
            bool inside = false;
            double minEdgeDistSq = 1e18;
            for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
                const double xi = polyPx[i].x, yi = polyPx[i].y;
                const double xj = polyPx[j].x, yj = polyPx[j].y;
                if (((yi > py) != (yj > py)) &&
                    (px < (xj - xi) * (py - yi) / (yj - yi + 1e-12) + xi)) {
                    inside = !inside;
                }
                const double vx = xj - xi, vy = yj - yi;
                const double lenSq = vx * vx + vy * vy;
                double t = (lenSq > 1e-9) ? (((px - xi) * vx + (py - yi) * vy) / lenSq) : 0.0;
                t = std::clamp(t, 0.0, 1.0);
                const double dx = px - (xi + t * vx);
                const double dy = py - (yi + t * vy);
                const double dSq = dx * dx + dy * dy;
                if (dSq < minEdgeDistSq) minEdgeDistSq = dSq;
            }
            std::uint8_t* p = &img.pixels[(static_cast<std::size_t>(y) * w + x) * 4u];
            if (!inside) {
                p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
            } else if (minEdgeDistSq <= 3.2) {
                // Subtle accent border along the circular/freehand cutout rim
                p[0] = 235; p[1] = 115; p[2] = 26; p[3] = 255;
            }
        }
    }
}

inline bool CopyBgra32ToClipboardWin32(HWND owner, const RasterImageBgra32& img) {
    if (!img.Valid()) return false;
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    const std::size_t pixelBytes = static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 4u;
    const std::size_t totalBytes = sizeof(BITMAPINFOHEADER) + pixelBytes;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, totalBytes);
    if (hMem) {
        auto* dst = static_cast<std::uint8_t*>(GlobalLock(hMem));
        if (dst) {
            BITMAPINFOHEADER bmi{};
            bmi.biSize = sizeof(BITMAPINFOHEADER);
            bmi.biWidth = img.width;
            bmi.biHeight = img.height; // bottom-up standard CF_DIB for maximum app compatibility
            bmi.biPlanes = 1;
            bmi.biBitCount = 32;
            bmi.biCompression = BI_RGB;
            bmi.biSizeImage = static_cast<DWORD>(pixelBytes);
            std::memcpy(dst, &bmi, sizeof(bmi));
            std::uint8_t* outPixels = dst + sizeof(bmi);
            const std::size_t rowBytes = static_cast<std::size_t>(img.width) * 4u;
            for (int y = 0; y < img.height; ++y) {
                const std::uint8_t* srcRow = img.pixels.data() + static_cast<std::size_t>(img.height - 1 - y) * rowBytes;
                std::memcpy(outPixels + static_cast<std::size_t>(y) * rowBytes, srcRow, rowBytes);
            }
            GlobalUnlock(hMem);
            if (!SetClipboardData(CF_DIB, hMem)) {
                GlobalFree(hMem);
            }
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
    return true;
}

inline bool CopyTextToClipboardWin32(HWND owner, const std::string& utf8) {
    if (utf8.empty()) return false;
    const std::wstring w = Utf8ToWide(utf8);
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (hMem) {
        void* p = GlobalLock(hMem);
        if (p) {
            std::memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        } else {
            GlobalFree(hMem);
        }
    }
    CloseClipboard();
    return true;
}

// Paste image (CF_HDROP / CF_DIBV5 / CF_DIB) or text (CF_UNICODETEXT) from Windows clipboard (F-099)
inline bool PasteClipboardIntoDocument(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double worldX,
    double worldY,
    std::string& outStatus)
{
    if (!OpenClipboard(owner)) return false;

    // 0. Check copied image file path(s) from Windows Explorer (CF_HDROP)
    if (IsClipboardFormatAvailable(CF_HDROP)) {
        HDROP hDrop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
        if (hDrop) {
            const UINT cnt = DragQueryFileW(hDrop, 0xFFFFFFFFu, nullptr, 0);
            if (cnt > 0) {
                wchar_t filePath[MAX_PATH * 2]{};
                if (DragQueryFileW(hDrop, 0, filePath, static_cast<UINT>(std::size(filePath))) > 0) {
                    const std::wstring wpath(filePath);
                    CloseClipboard();
                    std::ifstream ifs(std::filesystem::path(wpath), std::ios::binary | std::ios::ate);
                    if (ifs.is_open()) {
                        const auto fsz = ifs.tellg();
                        if (fsz > 0 && static_cast<std::uint64_t>(fsz) <= 64ull * 1024ull * 1024ull) {
                            std::vector<std::uint8_t> rawBytes(static_cast<std::size_t>(fsz));
                            ifs.seekg(0, std::ios::beg);
                            if (ifs.read(reinterpret_cast<char*>(rawBytes.data()), fsz)) {
                                ImageProc::RgbaImage decoded{};
                                bool okDec = false;
                                if (wpath.size() >= 4 && _wcsicmp(wpath.c_str() + wpath.size() - 4, L".svg") == 0) {
                                    okDec = ImageProc::RasterizeSimpleSvgToRgba(std::string(rawBytes.begin(), rawBytes.end()), decoded, 512, 512);
                                } else {
                                    okDec = ImageProc::DecodeImageBytesWic(rawBytes.data(), rawBytes.size(), decoded);
                                }
                                std::vector<std::uint8_t> png;
                                if (okDec && ImageProc::EncodeRgbaToPngWic(decoded, png)) {
                                    AnnotationObject obj{};
                                    obj.kind = ObjectKind::Image;
                                    obj.layerId = doc.ActiveLayerId();
                                    const double dispW = std::clamp(static_cast<double>(decoded.width) * 0.65, 120.0, 420.0);
                                    const double dispH = dispW * (static_cast<double>(decoded.height) / (std::max)(1, decoded.width));
                                    obj.bounds = { worldX, worldY, dispW, dispH };
                                    obj.pngBytes = std::move(png);
                                    cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(obj)));
                                    outStatus = "Archivo de imagen pegado (" + std::to_string(decoded.width) + "x" + std::to_string(decoded.height) + ").";
                                    return true;
                                }
                            }
                        }
                    }
                    return false;
                }
            }
        }
    }

    // 1. Check bitmap / DIBV5 / DIB on clipboard with safe header offset & real alpha preservation
    const UINT dibFmt = IsClipboardFormatAvailable(CF_DIBV5) ? CF_DIBV5
                      : (IsClipboardFormatAvailable(CF_DIB) ? CF_DIB : 0u);
    if (dibFmt != 0u) {
        HANDLE hDib = GetClipboardData(dibFmt);
        if (hDib) {
            const SIZE_T totalBytes = GlobalSize(hDib);
            const auto* rawBase = static_cast<const std::uint8_t*>(GlobalLock(hDib));
            if (rawBase && totalBytes >= sizeof(BITMAPINFOHEADER)) {
                const auto* bmi = reinterpret_cast<const BITMAPINFOHEADER*>(rawBase);
                if (bmi->biSize >= sizeof(BITMAPINFOHEADER) &&
                    bmi->biSize <= totalBytes &&
                    bmi->biWidth > 0 && bmi->biWidth <= 16384 &&
                    bmi->biHeight != 0 && std::abs(bmi->biHeight) <= 16384 &&
                    (bmi->biBitCount == 24 || bmi->biBitCount == 32)) {
                    const int w = bmi->biWidth;
                    const int h = std::abs(bmi->biHeight);
                    const bool topDown = (bmi->biHeight < 0);
                    // Only BITMAPINFOHEADER (40 bytes) places 12-byte BI_BITFIELDS masks after biSize;
                    // BITMAPV4HEADER (108) and BITMAPV5HEADER (124) include masks inside biSize.
                    const std::size_t bitfieldExtra =
                        (bmi->biSize == sizeof(BITMAPINFOHEADER) && bmi->biCompression == BI_BITFIELDS) ? 12u : 0u;
                    const std::size_t paletteBytes = static_cast<std::size_t>(bmi->biClrUsed) * 4u;
                    const std::size_t pixelOffset = static_cast<std::size_t>(bmi->biSize) + bitfieldExtra + paletteBytes;
                    const std::size_t stride = ((static_cast<std::size_t>(w) * bmi->biBitCount + 31u) / 32u) * 4u;
                    const std::size_t neededBytes = pixelOffset + stride * static_cast<std::size_t>(h);
                    if (neededBytes <= totalBytes) {
                        const std::uint8_t* bits = rawBase + pixelOffset;
                        bool hasRealAlpha = false;
                        if (bmi->biBitCount == 32) {
                            for (int y = 0; y < h && !hasRealAlpha; ++y) {
                                const std::uint8_t* row = bits + static_cast<std::size_t>(y) * stride;
                                for (int x = 0; x < w; ++x) {
                                    if (row[static_cast<std::size_t>(x) * 4u + 3u] != 0u) {
                                        hasRealAlpha = true;
                                        break;
                                    }
                                }
                            }
                        }

                        ImageProc::RgbaImage img{};
                        img.width = w;
                        img.height = h;
                        img.rgba.resize(static_cast<std::size_t>(w) * h * 4u, 255u);
                        for (int y = 0; y < h; ++y) {
                            const int srcY = topDown ? y : (h - 1 - y);
                            const std::uint8_t* row = bits + static_cast<std::size_t>(srcY) * stride;
                            for (int x = 0; x < w; ++x) {
                                const std::size_t dstIdx = (static_cast<std::size_t>(y) * w + x) * 4u;
                                const std::size_t srcIdx = static_cast<std::size_t>(x) * (bmi->biBitCount / 8);
                                img.rgba[dstIdx + 0] = row[srcIdx + 2];
                                img.rgba[dstIdx + 1] = row[srcIdx + 1];
                                img.rgba[dstIdx + 2] = row[srcIdx + 0];
                                img.rgba[dstIdx + 3] = (bmi->biBitCount == 32 && hasRealAlpha) ? row[srcIdx + 3] : 255u;
                            }
                        }
                        GlobalUnlock(hDib);
                        CloseClipboard();

                        std::vector<std::uint8_t> png;
                        if (ImageProc::EncodeRgbaToPngWic(img, png)) {
                            AnnotationObject obj{};
                            obj.kind = ObjectKind::Image;
                            obj.layerId = doc.ActiveLayerId();
                            const double dispW = std::clamp(static_cast<double>(w) * 0.65, 120.0, 420.0);
                            const double dispH = dispW * (static_cast<double>(h) / (std::max)(1, w));
                            obj.bounds = { worldX, worldY, dispW, dispH };
                            obj.pngBytes = std::move(png);
                            cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(obj)));
                            outStatus = "Imagen pegada desde el portapapeles (" + std::to_string(w) + "x" + std::to_string(h) + ").";
                            return true;
                        }
                        return false;
                    }
                }
            }
            GlobalUnlock(hDib);
        }
    }

    // 2. Check text on clipboard
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hTxt = GetClipboardData(CF_UNICODETEXT);
        if (hTxt) {
            const auto* wptr = static_cast<const wchar_t*>(GlobalLock(hTxt));
            if (wptr && wptr[0] != L'\0') {
                const std::string utf8 = WideToUtf8(wptr);
                GlobalUnlock(hTxt);
                CloseClipboard();

                AnnotationObject tb{};
                tb.kind = ObjectKind::TextBox;
                tb.layerId = doc.ActiveLayerId();
                tb.bounds = { worldX, worldY, 320.0, 88.0 };
                tb.textUtf8 = utf8;
                tb.fontSizePt = 13.0;
                tb.strokeColor = DocColor::RGBA(24, 30, 42, 255);
                cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(tb)));
                outStatus = "Texto pegado desde el portapapeles.";
                return true;
            }
            GlobalUnlock(hTxt);
        }
    }

    CloseClipboard();
    return false;
}
#endif

} // namespace DocStudio::CanvasManip
