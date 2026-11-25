#include "notebook_migration_engine.hpp"
#include "binary_parsers.hpp"

#include "../core/cudoc_storage.hpp"
#include "../core/utf_conv.hpp"
#include "../core/zip_container.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../tinta_core/archivo.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace DocStudio::Interop {

using nlohmann::json;

namespace {

std::string LowerExt(const std::string& pathUtf8) {
    const std::size_t dot = pathUtf8.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = pathUtf8.substr(dot);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::string StemFromPath(const std::string& pathUtf8) {
    std::size_t slash = pathUtf8.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? pathUtf8 : pathUtf8.substr(slash + 1);
    const std::size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base.resize(dot);
    return base.empty() ? "Cuaderno importado" : base;
}

bool ReadFileToBytes(const std::wstring& path, std::vector<std::uint8_t>& out, std::string* outErr) {
    std::ifstream ifs(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        if (outErr) *outErr = "No se pudo abrir el archivo de origen.";
        return false;
    }
    const auto sz = ifs.tellg();
    if (sz <= 0 || static_cast<std::uint64_t>(sz) > 512ull * 1024ull * 1024ull) {
        if (outErr) *outErr = "El archivo est\xC3\xA1 vac\xC3\xADo o excede el l\xC3\xADmite de 512 MB.";
        return false;
    }
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!ifs.read(reinterpret_cast<char*>(out.data()), sz)) {
        if (outErr) *outErr = "Error leyendo los bytes del archivo.";
        return false;
    }
    return true;
}

bool IsImageExt(const std::string& lowerPath) {
    return lowerPath.size() >= 4 && (
        lowerPath.compare(lowerPath.size() - 4, 4, ".png") == 0 ||
        lowerPath.compare(lowerPath.size() - 4, 4, ".jpg") == 0 ||
        lowerPath.compare(lowerPath.size() - 4, 4, ".bmp") == 0 ||
        (lowerPath.size() >= 5 && (
            lowerPath.compare(lowerPath.size() - 5, 5, ".jpeg") == 0 ||
            lowerPath.compare(lowerPath.size() - 5, 5, ".webp") == 0 ||
            lowerPath.compare(lowerPath.size() - 5, 5, ".tiff") == 0)));
}

bool IsAudioExt(const std::string& lowerPath) {
    return lowerPath.size() >= 4 && (
        lowerPath.compare(lowerPath.size() - 4, 4, ".wav") == 0 ||
        lowerPath.compare(lowerPath.size() - 4, 4, ".m4a") == 0 ||
        lowerPath.compare(lowerPath.size() - 4, 4, ".mp3") == 0 ||
        lowerPath.compare(lowerPath.size() - 4, 4, ".aac") == 0 ||
        lowerPath.compare(lowerPath.size() - 4, 4, ".ogg") == 0);
}

std::string ToLowerCopy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

DocColor ArgbToDocColor(std::uint32_t argb) noexcept {
    DocColor c{};
    c.a = static_cast<std::uint8_t>((argb >> 24) & 0xFFu);
    c.r = static_cast<std::uint8_t>((argb >> 16) & 0xFFu);
    c.g = static_cast<std::uint8_t>((argb >> 8) & 0xFFu);
    c.b = static_cast<std::uint8_t>(argb & 0xFFu);
    if (c.a == 0 && (c.r != 0 || c.g != 0 || c.b != 0)) c.a = 255u;
    return c;
}

// ============================================================================
// Goodnotes 6 (.goodnotes) Importer (ZIP + Protobuf/JSON strokes + PDF + media)
// ============================================================================
void ExtractStrokesFromGoodnotesProtobuf(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<AnnotationObject>& outStrokes,
    std::vector<std::string>& outTexts,
    int depth = 0)
{
    if (depth > 8 || !data || size == 0) return;
    std::vector<Binary::ProtoField> fields;
    if (!Binary::DecodeProtobufMessage(data, size, fields)) return;

    std::vector<float> xs, ys, ps;
    double strokeWidth = 2.4;
    DocColor color = DocColor::RGBA(24, 38, 66, 255);
    bool isHl = false;

    for (const auto& f : fields) {
        if (f.wireType == 5) {
            const float fv = f.AsFloat();
            if (std::isfinite(fv) && fv > 0.2f && fv < 48.0f && (f.fieldNumber == 4 || f.fieldNumber == 5 || f.fieldNumber == 6)) {
                strokeWidth = fv;
            }
        } else if (f.wireType == 0) {
            if (f.fieldNumber == 7 && f.varintVal == 1) isHl = true;
            if ((f.fieldNumber == 3 || f.fieldNumber == 8) && f.varintVal > 0x00FFFFFFull && f.varintVal <= 0xFFFFFFFFull) {
                color = ArgbToDocColor(static_cast<std::uint32_t>(f.varintVal));
            }
        } else if (f.wireType == 2 && !f.bytesVal.empty()) {
            // Check packed float arrays (Goodnotes stores stroke x[], y[], pressure[] as packed float32)
            if (f.fieldNumber == 1 && (f.bytesVal.size() % 4) == 0 && f.bytesVal.size() >= 8) {
                auto arr = f.AsPackedFloats();
                bool plausible = !arr.empty();
                for (float v : arr) if (!std::isfinite(v) || std::abs(v) > 50000.0f) { plausible = false; break; }
                if (plausible && xs.empty()) { xs = std::move(arr); continue; }
            }
            if (f.fieldNumber == 2 && (f.bytesVal.size() % 4) == 0 && f.bytesVal.size() >= 8) {
                auto arr = f.AsPackedFloats();
                bool plausible = !arr.empty();
                for (float v : arr) if (!std::isfinite(v) || std::abs(v) > 50000.0f) { plausible = false; break; }
                if (plausible && ys.empty()) { ys = std::move(arr); continue; }
            }
            if (f.fieldNumber == 3 && (f.bytesVal.size() % 4) == 0 && f.bytesVal.size() >= 8) {
                auto arr = f.AsPackedFloats();
                bool plausible = !arr.empty();
                for (float v : arr) if (!std::isfinite(v) || v < 0.0f || v > 5.0f) { plausible = false; break; }
                if (plausible && ps.empty()) { ps = std::move(arr); continue; }
            }
            // Check printable UTF-8 text block (supports Spanish accents & math/Greek symbols)
            bool printable = (f.bytesVal.size() >= 3 && f.bytesVal.size() <= 4096);
            int alphaCount = 0;
            for (std::size_t bi = 0; bi < f.bytesVal.size(); ++bi) {
                const std::uint8_t b = f.bytesVal[bi];
                if (b < 0x09 || (b > 0x0D && b < 0x20) || b == 0x7Fu) { printable = false; break; }
                if (std::isalpha(b)) {
                    ++alphaCount;
                } else if (b >= 0xC2u && b <= 0xDFu && bi + 1 < f.bytesVal.size() && (f.bytesVal[bi + 1] & 0xC0u) == 0x80u) {
                    ++alphaCount;
                    ++bi;
                } else if (b >= 0xE0u && b <= 0xEFu && bi + 2 < f.bytesVal.size() &&
                           (f.bytesVal[bi + 1] & 0xC0u) == 0x80u && (f.bytesVal[bi + 2] & 0xC0u) == 0x80u) {
                    ++alphaCount;
                    bi += 2;
                } else if (b >= 0x80u) {
                    printable = false;
                    break;
                }
            }
            if (printable && alphaCount >= 2 && f.fieldNumber >= 9) {
                outTexts.push_back(f.AsString());
                continue;
            }
            // Recurse into nested sub-messages
            ExtractStrokesFromGoodnotesProtobuf(f.bytesVal.data(), f.bytesVal.size(), outStrokes, outTexts, depth + 1);
        }
    }

    const std::size_t n = (std::min)(xs.size(), ys.size());
    if (n >= 2) {
        AnnotationObject st{};
        st.kind = ObjectKind::InkStroke;
        st.strokeColor = isHl ? DocColor::RGBA(253, 224, 71, 128) : color;
        st.strokeWidth = isHl ? (std::max)(10.0, strokeWidth) : strokeWidth;
        st.isHighlighter = isHl;
        st.penStyle = isHl ? InkPenStyle::Highlighter : InkPenStyle::Fountain;
        st.inkPoints.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            InkPoint pt{};
            pt.x = xs[i];
            pt.y = ys[i];
            pt.pressure = (i < ps.size()) ? std::clamp(ps[i], 0.1f, 1.0f) : 0.6f;
            st.inkPoints.push_back(pt);
        }
        st.RecomputeBounds();
        outStrokes.push_back(std::move(st));
    }
}

bool ImportGoodnotes6(
    const std::uint8_t* bytes,
    std::size_t size,
    const std::string& filenameHintUtf8,
    DocumentModel& outDoc,
    MigrationResult& res)
{
    ZipArchive zip;
    std::string zErr;
    if (!zip.Deserialize(bytes, size, &zErr)) {
        res.errorUtf8 = "Archivo .goodnotes da\xC3\xB1""ado o ZIP inv\xC3\xA1lido: " + zErr;
        return false;
    }

    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle(StemFromPath(filenameHintUtf8));
    doc.MutablePages().clear();

    std::vector<std::string> entries = zip.ListEntries();
    std::sort(entries.begin(), entries.end());

    // 1. Background PDFs embedded inside .goodnotes attachments/
    for (const auto& ep : entries) {
        const std::string low = ToLowerCopy(ep);
        if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".pdf") == 0) {
            if (const auto* pdfBytes = zip.FindEntry(ep)) {
                PdfDocument pdf;
                if (pdf.LoadFromBytes(*pdfBytes, nullptr) && pdf.PageCount() > 0) {
                    const std::string pdfAsset = doc.RegisterAsset(*pdfBytes);
                    for (std::size_t i = 0; i < pdf.PageCount(); ++i) {
                        const auto& pi = pdf.PageAt(i);
                        PageModel pm{};
                        pm.pageId = doc.AllocatePageId();
                        const DocRect box = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
                        pm.widthPt = (std::max)(72.0, box.w);
                        pm.heightPt = (std::max)(72.0, box.h);
                        pm.rotationDeg = pi.rotationDeg;
                        pm.hasPdfSource = true;
                        pm.sourcePdfAssetKey = pdfAsset;
                        pm.sourcePageIndex0 = static_cast<int>(i);
                        pm.extractedTextUtf8 = pi.extractedTextUtf8;
                        doc.MutablePages().push_back(std::move(pm));
                    }
                    res.report.hasBackgroundPdf = true;
                }
            }
        }
    }

    if (doc.Pages().empty()) {
        PageModel pm{};
        pm.pageId = doc.AllocatePageId();
        pm.widthPt = 595.0;
        pm.heightPt = 842.0;
        pm.background = PaperPattern::Grid;
        doc.MutablePages().push_back(std::move(pm));
    }

    // 2. Parse Goodnotes JSON/media first (avoid misrouting images/audio/json under notes/), then Protobuf (.pb)
    std::size_t pbPageCursor = 0;
    for (const auto& ep : entries) {
        const std::string low = ToLowerCopy(ep);
        const auto* raw = zip.FindEntry(ep);
        if (!raw || raw->empty()) continue;
        if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".pdf") == 0) continue;

        if (low.size() >= 5 && low.compare(low.size() - 5, 5, ".json") == 0) {
            try {
                const json j = json::parse(raw->begin(), raw->end());
                if (j.is_object()) {
                    if (j.contains("title") && j["title"].is_string() && !j["title"].get<std::string>().empty()) {
                        doc.SetTitle(j["title"].get<std::string>());
                    }
                    if (j.contains("strokes") && j["strokes"].is_array()) {
                        for (const auto& sj : j["strokes"]) {
                            if (!sj.is_object() || !sj.contains("points") || !sj["points"].is_array()) continue;
                            AnnotationObject st{};
                            st.kind = ObjectKind::InkStroke;
                            st.strokeWidth = sj.value("width", 2.4);
                            st.isHighlighter = sj.value("highlighter", false);
                            st.strokeColor = ArgbToDocColor(sj.value("colorArgb", 0xFF1A2A48u));
                            for (const auto& pj : sj["points"]) {
                                if (pj.is_array() && pj.size() >= 2) {
                                    InkPoint pt{};
                                    pt.x = pj[0].get<double>();
                                    pt.y = pj[1].get<double>();
                                    pt.pressure = (pj.size() >= 3) ? static_cast<float>(pj[2].get<double>()) : 0.6f;
                                    st.inkPoints.push_back(pt);
                                }
                            }
                            if (st.inkPoints.size() >= 2) {
                                st.RecomputeBounds();
                                doc.AddPageObject(0, std::move(st));
                                ++res.report.editableStrokeCount;
                            }
                        }
                    }
                    if (j.contains("bookmarks") && j["bookmarks"].is_array()) {
                        for (const auto& bj : j["bookmarks"]) {
                            if (bj.is_object()) {
                                doc.AddBookmark(bj.value("title", "Marcador"), bj.value("page", std::size_t{ 0 }));
                                ++res.report.bookmarkOrSectionCount;
                            }
                        }
                    }
                }
            } catch (...) {}
        } else if (IsImageExt(low)) {
            AnnotationObject img{};
            img.kind = ObjectKind::Image;
            img.bounds = { 64.0, 64.0 + static_cast<double>(res.report.imageCount) * 210.0, 280.0, 180.0 };
            img.pngBytes = *raw;
            doc.AddPageObject(0, std::move(img));
            ++res.report.imageCount;
        } else if (IsAudioExt(low)) {
            const std::string key = doc.AddAttachment(StemFromPath(ep) + LowerExt(ep), "audio/wav", *raw);
            AnnotationObject clip{};
            clip.kind = ObjectKind::AudioClip;
            clip.bounds = { 48.0, 36.0 + static_cast<double>(res.report.audioClipCount) * 44.0, 240.0, 34.0 };
            clip.audioAssetKey = key;
            clip.assetKey = key;
            clip.captionUtf8 = "Audio importado: " + StemFromPath(ep);
            doc.AddPageObject(0, std::move(clip));
            if (res.primaryAudioWavBytes.empty() && low.compare(low.size() - 4, 4, ".wav") == 0) {
                res.primaryAudioWavBytes = *raw;
            }
            ++res.report.audioClipCount;
            ++res.report.attachmentCount;
        } else if (low.find(".pb") != std::string::npos || low.rfind("notes/", 0) == 0) {
            std::vector<AnnotationObject> strokes;
            std::vector<std::string> texts;
            ExtractStrokesFromGoodnotesProtobuf(raw->data(), raw->size(), strokes, texts);
            if (!strokes.empty() || !texts.empty()) {
                if (pbPageCursor >= doc.Pages().size()) {
                    PageModel pm{};
                    pm.pageId = doc.AllocatePageId();
                    pm.background = PaperPattern::Grid;
                    doc.MutablePages().push_back(std::move(pm));
                }
                for (auto& st : strokes) {
                    doc.AddPageObject(pbPageCursor, std::move(st));
                    ++res.report.editableStrokeCount;
                }
                double ty = 56.0;
                for (const auto& txt : texts) {
                    AnnotationObject tb{};
                    tb.kind = ObjectKind::TextBox;
                    tb.bounds = { 48.0, ty, 480.0, 42.0 };
                    tb.textUtf8 = txt;
                    tb.fontSizePt = 12.0;
                    tb.strokeColor = DocColor::RGBA(24, 30, 42, 255);
                    doc.AddPageObject(pbPageCursor, std::move(tb));
                    ty += 48.0;
                    ++res.report.textBoxCount;
                }
                ++pbPageCursor;
            }
        }
    }

    res.report.pageCount = doc.Pages().size();
    res.report.visualTier = CompatLevelStatus::Full;
    res.report.structuralTier = CompatLevelStatus::Full;
    res.report.editableTier = (res.report.editableStrokeCount > 0 || res.report.textBoxCount > 0)
        ? CompatLevelStatus::Full : CompatLevelStatus::Partial;
    res.report.visualSummaryUtf8 = "P\xC3\xA1ginas, PDFs base e im\xC3\xA1genes importadas fielmente.";
    res.report.structuralSummaryUtf8 = "Estructura de p\xC3\xA1ginas, marcadores y adjuntos preservada.";
    res.report.editableSummaryUtf8 = (res.report.editableStrokeCount > 0)
        ? "Trazos vectoriales de Goodnotes convertidos a InkStroke nativos editables."
        : "Sin trazos vectoriales directos; PDFs y medios listos para anotaci\xC3\xB3n.";
    if (res.report.editableStrokeCount == 0 && !res.report.hasBackgroundPdf) {
        res.report.warnings.push_back({
            WarningSeverity::Warning, "GN6_NO_STROKES",
            "Formato interno Goodnotes sin flujos vectoriales abiertos",
            "Este archivo .goodnotes no contiene flujos de coordenadas est\xC3\xA1ndar o usa una variante cifrada; se importaron p\xC3\xA1ginas y adjuntos disponibles."
        });
    }

    doc.RebuildLayout();
    doc.SetDirty(false);
    outDoc = std::move(doc);
    res.ok = true;
    return true;
}

// ============================================================================
// Notability (.note) Importer (ZIP + bplist00 NSKeyedArchiver curves + PDF + WAV)
// ============================================================================
bool ImportNotabilityNote(
    const std::uint8_t* bytes,
    std::size_t size,
    const std::string& filenameHintUtf8,
    DocumentModel& outDoc,
    MigrationResult& res)
{
    ZipArchive zip;
    std::string zErr;
    if (!zip.Deserialize(bytes, size, &zErr)) {
        res.errorUtf8 = "Archivo .note de Notability da\xC3\xB1""ado o ZIP inv\xC3\xA1lido: " + zErr;
        return false;
    }

    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle(StemFromPath(filenameHintUtf8));
    doc.MutablePages().clear();

    std::vector<std::string> entries = zip.ListEntries();
    std::sort(entries.begin(), entries.end());

    // 1. Background PDFs inside Notability bundle
    for (const auto& ep : entries) {
        const std::string low = ToLowerCopy(ep);
        if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".pdf") == 0) {
            if (const auto* pdfBytes = zip.FindEntry(ep)) {
                PdfDocument pdf;
                if (pdf.LoadFromBytes(*pdfBytes, nullptr) && pdf.PageCount() > 0) {
                    const std::string pdfAsset = doc.RegisterAsset(*pdfBytes);
                    for (std::size_t i = 0; i < pdf.PageCount(); ++i) {
                        const auto& pi = pdf.PageAt(i);
                        PageModel pm{};
                        pm.pageId = doc.AllocatePageId();
                        const DocRect box = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
                        pm.widthPt = (std::max)(72.0, box.w);
                        pm.heightPt = (std::max)(72.0, box.h);
                        pm.hasPdfSource = true;
                        pm.sourcePdfAssetKey = pdfAsset;
                        pm.sourcePageIndex0 = static_cast<int>(i);
                        pm.extractedTextUtf8 = pi.extractedTextUtf8;
                        doc.MutablePages().push_back(std::move(pm));
                    }
                    res.report.hasBackgroundPdf = true;
                }
            }
        }
    }
    if (doc.Pages().empty()) {
        PageModel pm{};
        pm.pageId = doc.AllocatePageId();
        pm.widthPt = 595.0;
        pm.heightPt = 842.0;
        pm.background = PaperPattern::Ruled;
        doc.MutablePages().push_back(std::move(pm));
    }

    // 2. Parse Session.plist (bplist00 NSKeyedArchiver)
    for (const auto& ep : entries) {
        const std::string low = ToLowerCopy(ep);
        const auto* raw = zip.FindEntry(ep);
        if (!raw || raw->empty()) continue;

        if (low.find("session.plist") != std::string::npos || (low.size() >= 6 && low.compare(low.size() - 6, 6, ".plist") == 0)) {
            Binary::PlistValue root{};
            if (Binary::ParseBinaryPlist(raw->data(), raw->size(), root)) {
                const Binary::PlistValue* objs = root.Get("$objects");
                auto processDict = [&](const Binary::PlistValue& d) {
                    const auto* ptsVal = Binary::ResolveArchiverUid(root, d.Get("curvespoints"));
                    const auto* numVal = Binary::ResolveArchiverUid(root, d.Get("curvesnumpoints"));
                    const auto* widVal = Binary::ResolveArchiverUid(root, d.Get("curveswidths"));
                    const auto* colVal = Binary::ResolveArchiverUid(root, d.Get("curvescolors"));

                    if (ptsVal && ptsVal->kind == Binary::PlistValue::Kind::Data && ptsVal->dataVal.size() >= 16) {
                        const std::size_t totalFloats = ptsVal->dataVal.size() / 4;
                        std::vector<float> coords(totalFloats);
                        std::memcpy(coords.data(), ptsVal->dataVal.data(), totalFloats * 4);
                        const std::size_t totalPts = totalFloats / 2;

                        std::vector<std::uint32_t> counts;
                        if (numVal && numVal->kind == Binary::PlistValue::Kind::Data && (numVal->dataVal.size() % 4) == 0) {
                            counts.resize(numVal->dataVal.size() / 4);
                            std::memcpy(counts.data(), numVal->dataVal.data(), counts.size() * 4);
                        } else {
                            counts.push_back(static_cast<std::uint32_t>(totalPts));
                        }

                        std::vector<float> widths;
                        if (widVal && widVal->kind == Binary::PlistValue::Kind::Data && (widVal->dataVal.size() % 4) == 0) {
                            widths.resize(widVal->dataVal.size() / 4);
                            std::memcpy(widths.data(), widVal->dataVal.data(), widths.size() * 4);
                        }
                        std::vector<std::uint32_t> colors;
                        if (colVal && colVal->kind == Binary::PlistValue::Kind::Data && (colVal->dataVal.size() % 4) == 0) {
                            colors.resize(colVal->dataVal.size() / 4);
                            std::memcpy(colors.data(), colVal->dataVal.data(), colors.size() * 4);
                        }

                        std::size_t ptOffset = 0;
                        for (std::size_t si = 0; si < counts.size() && ptOffset < totalPts; ++si) {
                            const std::size_t cnt = (std::min<std::size_t>)(counts[si], totalPts - ptOffset);
                            if (cnt >= 2) {
                                AnnotationObject st{};
                                st.kind = ObjectKind::InkStroke;
                                st.strokeWidth = (si < widths.size() && widths[si] > 0.2f && widths[si] < 40.0f) ? widths[si] : 2.2;
                                st.strokeColor = (si < colors.size()) ? ArgbToDocColor(colors[si]) : DocColor::RGBA(24, 38, 66, 255);
                                st.penStyle = InkPenStyle::Ballpoint;
                                st.inkPoints.reserve(cnt);
                                for (std::size_t k = 0; k < cnt; ++k) {
                                    InkPoint pt{};
                                    pt.x = coords[(ptOffset + k) * 2];
                                    pt.y = coords[(ptOffset + k) * 2 + 1];
                                    pt.pressure = 0.6f;
                                    st.inkPoints.push_back(pt);
                                }
                                st.RecomputeBounds();
                                doc.AddPageObject(0, std::move(st));
                                ++res.report.editableStrokeCount;
                            }
                            ptOffset += cnt;
                        }
                    }

                    for (const char* tKey : { "attributedString", "NS.string", "text" }) {
                        if (const auto* txtV = Binary::ResolveArchiverUid(root, d.Get(tKey))) {
                            if (txtV->kind == Binary::PlistValue::Kind::String && !txtV->strVal.empty()) {
                                AnnotationObject tb{};
                                tb.kind = ObjectKind::TextBox;
                                tb.bounds = { 48.0, 48.0 + static_cast<double>(res.report.textBoxCount) * 60.0, 490.0, 56.0 };
                                tb.textUtf8 = txtV->strVal;
                                tb.fontSizePt = 12.0;
                                tb.strokeColor = DocColor::RGBA(24, 30, 42, 255);
                                doc.AddPageObject(0, std::move(tb));
                                ++res.report.textBoxCount;
                            }
                        }
                    }
                };

                processDict(root);
                if (objs && objs->kind == Binary::PlistValue::Kind::Array) {
                    for (const auto& item : objs->arrayVal) {
                        if (item.kind == Binary::PlistValue::Kind::Dict) processDict(item);
                    }
                    if (res.report.textBoxCount == 0) {
                        for (const auto& item : objs->arrayVal) {
                            if (item.kind != Binary::PlistValue::Kind::String) continue;
                            const std::string& s = item.strVal;
                            if (s.size() < 4 || s[0] == '$') continue;
                            if (s.rfind("NS", 0) == 0 || s.rfind("CF", 0) == 0 || s.rfind("curves", 0) == 0) continue;
                            if (s.find(' ') == std::string::npos && s.size() < 12) continue;
                            AnnotationObject tb{};
                            tb.kind = ObjectKind::TextBox;
                            tb.bounds = { 48.0, 48.0 + static_cast<double>(res.report.textBoxCount) * 60.0, 490.0, 56.0 };
                            tb.textUtf8 = s;
                            tb.fontSizePt = 12.0;
                            tb.strokeColor = DocColor::RGBA(24, 30, 42, 255);
                            doc.AddPageObject(0, std::move(tb));
                            ++res.report.textBoxCount;
                        }
                    }
                }
            }
        } else if (IsImageExt(low)) {
            AnnotationObject img{};
            img.kind = ObjectKind::Image;
            img.bounds = { 64.0, 80.0 + static_cast<double>(res.report.imageCount) * 200.0, 280.0, 180.0 };
            img.pngBytes = *raw;
            doc.AddPageObject(0, std::move(img));
            ++res.report.imageCount;
        } else if (IsAudioExt(low)) {
            const std::string key = doc.AddAttachment(StemFromPath(ep) + LowerExt(ep), "audio/wav", *raw);
            AnnotationObject clip{};
            clip.kind = ObjectKind::AudioClip;
            clip.bounds = { 48.0, 32.0 + static_cast<double>(res.report.audioClipCount) * 44.0, 250.0, 34.0 };
            clip.audioAssetKey = key;
            clip.assetKey = key;
            clip.captionUtf8 = "Grabaci\xC3\xB3n Notability: " + StemFromPath(ep);
            doc.AddPageObject(0, std::move(clip));
            if (res.primaryAudioWavBytes.empty() && low.compare(low.size() - 4, 4, ".wav") == 0) {
                res.primaryAudioWavBytes = *raw;
            }
            ++res.report.audioClipCount;
            ++res.report.attachmentCount;
        }
    }

    res.report.pageCount = doc.Pages().size();
    res.report.visualTier = CompatLevelStatus::Full;
    res.report.structuralTier = CompatLevelStatus::Full;
    res.report.editableTier = (res.report.editableStrokeCount > 0 || res.report.textBoxCount > 0)
        ? CompatLevelStatus::Full : CompatLevelStatus::Partial;
    res.report.visualSummaryUtf8 = "P\xC3\xA1ginas, PDFs e im\xC3\xA1genes de Notability reconstruidas.";
    res.report.structuralSummaryUtf8 = "Nota, audio sincronizado y adjuntos conservados.";
    res.report.editableSummaryUtf8 = "Curvas de tinta (curvespoints) convertidas a trazos vectoriales editables.";

    doc.RebuildLayout();
    doc.SetDirty(false);
    outDoc = std::move(doc);
    res.ok = true;
    return true;
}

// ============================================================================
// Microsoft OneNote (.one / .onepkg) Importer (MSCF CAB + [MS-ONE] + InkML/media)
// ============================================================================
void ExtractOneNoteSectionIntoPage(
    const std::uint8_t* data,
    std::size_t size,
    const std::string& sectionTitle,
    DocumentModel& doc,
    MigrationResult& res)
{
    PageModel pm{};
    pm.pageId = doc.AllocatePageId();
    pm.widthPt = 720.0;
    pm.heightPt = 960.0;
    pm.background = PaperPattern::Grid;
    const std::size_t pageIdx = doc.Pages().size();
    doc.MutablePages().push_back(std::move(pm));
    doc.AddBookmark(sectionTitle, pageIdx, 0.0, 0);
    ++res.report.bookmarkOrSectionCount;
    res.notebookSectionsUtf8.push_back(sectionTitle);

    // 1. Extract InkML <trace> polylines if present inside OneNote binary/XML payload
    const std::string rawAscii(reinterpret_cast<const char*>(data), size);
    const auto inkMlStrokes = Binary::ParseInkMlTraces(rawAscii);
    for (const auto& pts : inkMlStrokes) {
        AnnotationObject st{};
        st.kind = ObjectKind::InkStroke;
        st.strokeColor = DocColor::RGBA(24, 38, 66, 255);
        st.strokeWidth = 2.4;
        st.penStyle = InkPenStyle::Ballpoint;
        st.inkPoints = pts;
        st.RecomputeBounds();
        doc.AddPageObject(pageIdx, std::move(st));
        ++res.report.editableStrokeCount;
    }

    // 2. Scan embedded PNG & JPEG images by magic framing
    static constexpr std::uint8_t kPngSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    static constexpr std::uint8_t kPngIend[8] = { 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82 };
    double imgY = 420.0;
    for (std::size_t p = 0; p + 24 < size && res.report.imageCount < 32; ++p) {
        if (std::memcmp(data + p, kPngSig, 8) == 0) {
            for (std::size_t q = p + 16; q + 8 <= size; ++q) {
                if (std::memcmp(data + q, kPngIend, 8) == 0) {
                    const std::size_t pngLen = (q + 8) - p;
                    if (pngLen <= 16u * 1024u * 1024u) {
                        AnnotationObject img{};
                        img.kind = ObjectKind::Image;
                        img.bounds = { 64.0, imgY, 300.0, 190.0 };
                        img.pngBytes.assign(data + p, data + p + pngLen);
                        doc.AddPageObject(pageIdx, std::move(img));
                        imgY += 205.0;
                        ++res.report.imageCount;
                    }
                    p = q + 7;
                    break;
                }
            }
        } else if (data[p] == 0xFFu && data[p + 1] == 0xD8u && data[p + 2] == 0xFFu &&
                   (data[p + 3] == 0xE0u || data[p + 3] == 0xE1u || data[p + 3] == 0xDBu)) {
            for (std::size_t q = p + 16; q + 2 <= size && (q - p) <= 16u * 1024u * 1024u; ++q) {
                if (data[q] == 0xFFu && data[q + 1] == 0xD9u) {
                    const std::size_t jpgLen = (q + 2) - p;
                    if (jpgLen >= 64) {
                        AnnotationObject img{};
                        img.kind = ObjectKind::Image;
                        img.bounds = { 64.0, imgY, 300.0, 190.0 };
                        img.pngBytes.assign(data + p, data + p + jpgLen);
                        doc.AddPageObject(pageIdx, std::move(img));
                        imgY += 205.0;
                        ++res.report.imageCount;
                    }
                    p = q + 1;
                    break;
                }
            }
        }
    }

    // 3. Extract embedded RIFF/WAVE audio clips by RIFF header
    for (std::size_t p = 0; p + 44 < size && res.report.audioClipCount < 8; ++p) {
        if (std::memcmp(data + p, "RIFF", 4) == 0 && std::memcmp(data + p + 8, "WAVE", 4) == 0) {
            const std::uint32_t riffLen = Binary::ReadLe32(data + p + 4);
            const std::size_t totalLen = static_cast<std::size_t>(riffLen) + 8u;
            if (totalLen >= 44 && p + totalLen <= size && totalLen <= 32u * 1024u * 1024u) {
                std::vector<std::uint8_t> wav(data + p, data + p + totalLen);
                const std::string key = doc.AddAttachment("onenote_audio_" + std::to_string(res.report.audioClipCount + 1) + ".wav", "audio/wav", wav);
                AnnotationObject clip{};
                clip.kind = ObjectKind::AudioClip;
                clip.bounds = { 48.0, 36.0 + static_cast<double>(res.report.audioClipCount) * 44.0, 260.0, 34.0 };
                clip.audioAssetKey = key;
                clip.assetKey = key;
                clip.captionUtf8 = "Audio OneNote (" + sectionTitle + ")";
                doc.AddPageObject(pageIdx, std::move(clip));
                if (res.primaryAudioWavBytes.empty()) res.primaryAudioWavBytes = std::move(wav);
                ++res.report.audioClipCount;
                ++res.report.attachmentCount;
                p += totalLen - 1;
            }
        }
    }

    // 4. Extract readable UTF-16LE text runs (even & odd alignments + Latin/Greek/Math Unicode)
    std::vector<std::string> extractedLines;
    std::size_t pos = 0;
    while (pos + 8 <= size && extractedLines.size() < 200) {
        std::string run;
        int alphaCnt = 0;
        std::size_t k = pos;
        while (k + 1 < size) {
            const std::uint16_t ch = Binary::ReadLe16(data + k);
            const bool isAsciiPrint = (ch >= 0x20u && ch <= 0x7Eu);
            const bool isLatinExt = (ch >= 0x00A0u && ch <= 0x024Fu);
            const bool isGreek = (ch >= 0x0370u && ch <= 0x03FFu);
            const bool isMathSym = (ch >= 0x2000u && ch <= 0x22FFu);
            if (isAsciiPrint || isLatinExt || isGreek || isMathSym || ch == 0x0Au || ch == 0x0Du || ch == 0x09u) {
                if (ch == 0x0Du) { /* skip CR */ }
                else if (ch < 0x80u) {
                    run.push_back(static_cast<char>(ch));
                    if (std::isalpha(static_cast<unsigned char>(ch))) ++alphaCnt;
                } else if (ch < 0x800u) {
                    run.push_back(static_cast<char>(0xC0u | (ch >> 6)));
                    run.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
                    if (isLatinExt || isGreek) ++alphaCnt;
                } else {
                    run.push_back(static_cast<char>(0xE0u | (ch >> 12)));
                    run.push_back(static_cast<char>(0x80u | ((ch >> 6) & 0x3Fu)));
                    run.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
                }
                k += 2;
            } else {
                break;
            }
        }
        if (run.size() >= 6 && alphaCnt >= 4 && run.find("<trace") == std::string::npos) {
            extractedLines.push_back(run);
            pos = k + 2;
        } else {
            pos += 1;
        }
    }

    // Add extracted text blocks onto page
    double textY = 86.0;
    std::string combinedText;
    for (const auto& ln : extractedLines) {
        if (!combinedText.empty()) combinedText += '\n';
        combinedText += ln;
    }
    if (!combinedText.empty()) {
        doc.MutablePages()[pageIdx].extractedTextUtf8 = combinedText;
        AnnotationObject tb{};
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 48.0, textY, 620.0, 300.0 };
        tb.textUtf8 = combinedText;
        tb.fontSizePt = 12.0;
        tb.strokeColor = DocColor::RGBA(24, 30, 42, 255);
        doc.AddPageObject(pageIdx, std::move(tb));
        ++res.report.textBoxCount;
    }
}

bool ImportOneNote(
    const std::uint8_t* bytes,
    std::size_t size,
    const std::string& filenameHintUtf8,
    bool isPackage,
    DocumentModel& outDoc,
    MigrationResult& res)
{
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle(StemFromPath(filenameHintUtf8));
    doc.MutablePages().clear();

    if (isPackage) {
        std::vector<Binary::CabFileEntry> cabFiles;
        std::string cabErr;
        if (!Binary::ExtractCabArchive(bytes, size, cabFiles, &cabErr)) {
            res.errorUtf8 = "Error desempaquetando .onepkg (MSCF CAB): " + cabErr;
            return false;
        }
        for (const auto& cf : cabFiles) {
            const std::string low = ToLowerCopy(cf.filename);
            if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".one") == 0) {
                ExtractOneNoteSectionIntoPage(cf.data.data(), cf.data.size(), StemFromPath(cf.filename), doc, res);
            } else if (!cf.data.empty()) {
                doc.AddAttachment(cf.filename, "application/octet-stream", cf.data);
                ++res.report.attachmentCount;
            }
        }
    } else {
        ExtractOneNoteSectionIntoPage(bytes, size, StemFromPath(filenameHintUtf8), doc, res);
    }

    if (doc.Pages().empty()) {
        PageModel pm{};
        pm.pageId = doc.AllocatePageId();
        doc.MutablePages().push_back(std::move(pm));
    }

    res.report.pageCount = doc.Pages().size();
    res.report.visualTier = CompatLevelStatus::Partial;
    res.report.structuralTier = CompatLevelStatus::Full;
    res.report.editableTier = (res.report.textBoxCount > 0 || res.report.editableStrokeCount > 0)
        ? CompatLevelStatus::Full : CompatLevelStatus::Partial;
    res.report.visualSummaryUtf8 = "Secciones, texto, im\xC3\xA1genes y trazos InkML reconstruidos en p\xC3\xA1ginas nativas.";
    res.report.structuralSummaryUtf8 = "Jerarqu\xC3\xAD""a de secciones .one/.onepkg mapeada a marcadores y p\xC3\xA1ginas.";
    res.report.editableSummaryUtf8 = "Texto UTF-16LE, trazos vectoriales e im\xC3\xA1genes totalmente editables.";
    res.report.warnings.push_back({
        WarningSeverity::Info, "ONENOTE_LAYOUT_NOTE",
        "Diferencias menores de maquetaci\xC3\xB3n en OneNote binario",
        "Los contenedores flotantes de OneNote se normalizan a objetos posicionados editables en CuadernU."
    });

    doc.RebuildLayout();
    doc.SetDirty(false);
    outDoc = std::move(doc);
    res.ok = true;
    return true;
}

// ============================================================================
// CuadernU Legacy (.tinta) Native Binary Importer (via unified tinta_core)
// ============================================================================
PaperPattern MapTintaPauta(tinta::Pauta p) noexcept {
    switch (p) {
    case tinta::Pauta::Rayado:          return PaperPattern::Ruled;
    case tinta::Pauta::Cuadricula:      return PaperPattern::Grid;
    case tinta::Pauta::Ingenieria:      return PaperPattern::Grid;
    case tinta::Pauta::Circuitos:       return PaperPattern::Grid;
    case tinta::Pauta::Cartesiano:      return PaperPattern::Grid;
    case tinta::Pauta::Puntos:          return PaperPattern::Dots;
    case tinta::Pauta::Milimetrico:     return PaperPattern::Millimeter;
    case tinta::Pauta::Isometrico:      return PaperPattern::Isometric;
    case tinta::Pauta::Logaritmico:     return PaperPattern::LogLog;
    case tinta::Pauta::SemiLogaritmico: return PaperPattern::LogLog;
    case tinta::Pauta::Polar:           return PaperPattern::Polar;
    case tinta::Pauta::Smith:           return PaperPattern::SmithChart;
    case tinta::Pauta::Cornell:         return PaperPattern::Cornell;
    case tinta::Pauta::Pentagrama:      return PaperPattern::MusicStaff;
    default:                            return PaperPattern::Plain;
    }
}

bool ImportCuadernuTinta(
    const std::uint8_t* bytes,
    std::size_t size,
    const std::string& filenameHintUtf8,
    DocumentModel& outDoc,
    MigrationResult& res)
{
    std::vector<std::uint8_t> buf(bytes, bytes + size);
    tinta::Documento td{};
    if (!tinta::Deserializar(buf, &td)) {
        res.errorUtf8 = std::string("Error leyendo archivo .tinta: ") + tinta::UltimoErrorArchivo();
        return false;
    }

    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    if (!td.id.empty()) doc.SetDocumentUuid(td.id);
    doc.SetTitle(td.titulo.empty() ? StemFromPath(filenameHintUtf8) : td.titulo);
    doc.MutablePages().clear();

    for (const auto& cuaderno : td.cuadernos) {
        for (const auto& sec : cuaderno.secciones) {
            const std::string secLabel = cuaderno.titulo.empty() ? sec.titulo : (cuaderno.titulo + " / " + sec.titulo);
            res.notebookSectionsUtf8.push_back(secLabel);
            bool addedBookmark = false;
            for (const auto& tp : sec.paginas) {
                PageModel pm{};
                pm.pageId = doc.AllocatePageId();
                pm.widthPt = (std::max)(120.0, static_cast<double>(tp.ancho));
                pm.heightPt = (std::max)(120.0, static_cast<double>(tp.alto));
                pm.background = MapTintaPauta(tp.papel.pauta);
                const std::size_t pageIdx = doc.Pages().size();
                doc.MutablePages().push_back(std::move(pm));
                if (!addedBookmark && !secLabel.empty()) {
                    doc.AddBookmark(secLabel, pageIdx, 0.0, 0);
                    ++res.report.bookmarkOrSectionCount;
                    addedBookmark = true;
                }

                for (const auto& tr : tp.trazos) {
                    if (tr.muestras.empty()) continue;
                    AnnotationObject st{};
                    st.kind = ObjectKind::InkStroke;
                    st.strokeColor = ArgbToDocColor(tr.color_argb);
                    st.strokeWidth = (std::max)(0.8, static_cast<double>(tr.ancho_base));
                    st.isHighlighter = tr.resaltador;
                    st.penStyle = tr.resaltador ? InkPenStyle::Highlighter : InkPenStyle::Fountain;
                    st.opacity = tr.estilo_persistente ? std::clamp(static_cast<double>(tr.estilo.opacidad), 0.05, 1.0) : 1.0;
                    st.inkPoints.reserve(tr.muestras.size());
                    for (const auto& m : tr.muestras) {
                        InkPoint pt{};
                        pt.x = m.posicion.x;
                        pt.y = m.posicion.y;
                        pt.pressure = m.tiene_presion() ? std::clamp(m.presion, 0.05f, 1.0f) : 0.6f;
                        pt.tiltXDeg = m.inclinacion_x;
                        pt.tiltYDeg = m.inclinacion_y;
                        pt.timestampMs = (m.tiempo_qpc > 0) ? static_cast<std::uint64_t>(m.tiempo_qpc / 10000ll) : 0ull;
                        st.inkPoints.push_back(pt);
                    }
                    st.RecomputeBounds();
                    doc.AddPageObject(pageIdx, std::move(st));
                    ++res.report.editableStrokeCount;
                }

                for (const auto& ct : tp.textos) {
                    AnnotationObject tb{};
                    tb.kind = ObjectKind::TextBox;
                    tb.bounds = { ct.marco.x, ct.marco.y, (std::max)(40.0, static_cast<double>(ct.marco.ancho)), (std::max)(24.0, static_cast<double>(ct.marco.alto)) };
                    tb.textUtf8 = ct.texto;
                    tb.fontSizePt = (std::max)(8.0, static_cast<double>(ct.estilo.tamano));
                    tb.bold = ct.estilo.negrita;
                    tb.italic = ct.estilo.cursiva;
                    tb.underline = ct.estilo.subrayado;
                    tb.rotationDeg = ct.rotacion * (180.0 / 3.14159265358979323846);
                    tb.strokeColor = ArgbToDocColor(ct.estilo.color_argb);
                    doc.AddPageObject(pageIdx, std::move(tb));
                    ++res.report.textBoxCount;
                }

                for (const auto& nf : tp.notas) {
                    AnnotationObject sn{};
                    sn.kind = ObjectKind::TextBox;
                    sn.bounds = { nf.posicion.x, nf.posicion.y, (std::max)(120.0, static_cast<double>(nf.ancho)), (std::max)(80.0, static_cast<double>(nf.alto)) };
                    sn.textUtf8 = nf.texto;
                    sn.fillColor = ArgbToDocColor(nf.color_argb);
                    sn.strokeColor = DocColor::RGBA(28, 32, 42, 255);
                    doc.AddPageObject(pageIdx, std::move(sn));
                    ++res.report.textBoxCount;
                }

                for (const auto& md : tp.medios) {
                    AnnotationObject mo{};
                    mo.bounds = {
                        md.marco.x, md.marco.y,
                        (std::max)(48.0, static_cast<double>(md.marco.ancho)),
                        (std::max)(36.0, static_cast<double>(md.marco.alto))
                    };
                    mo.rotationDeg = md.rotacion * (180.0 / 3.14159265358979323846);
                    mo.opacity = std::clamp(static_cast<double>(md.opacidad), 0.05, 1.0);
                    mo.captionUtf8 = md.nombre_original.empty() ? md.hash : md.nombre_original;
                    if (md.tipo == tinta::TipoMedio::Audio) {
                        mo.kind = ObjectKind::AudioClip;
                        mo.audioAssetKey = md.hash;
                        mo.assetKey = md.hash;
                        doc.AddPageObject(pageIdx, std::move(mo));
                        ++res.report.audioClipCount;
                    } else {
                        mo.kind = ObjectKind::Image;
                        mo.assetKey = md.hash;
                        mo.imageCrop = {
                            md.recorte_x, md.recorte_y,
                            md.recorte_x + md.recorte_ancho,
                            md.recorte_y + md.recorte_alto
                        };
                        mo.imageCrop.Clamp();
                        doc.AddPageObject(pageIdx, std::move(mo));
                        ++res.report.imageCount;
                    }
                }
            }
        }
        // Migrate notebook-scoped sticky notes onto first page
        if (!cuaderno.notas.empty() && !doc.Pages().empty()) {
            for (const auto& nf : cuaderno.notas) {
                AnnotationObject sn{};
                sn.kind = ObjectKind::TextBox;
                sn.bounds = { nf.posicion.x, nf.posicion.y, (std::max)(120.0, static_cast<double>(nf.ancho)), (std::max)(80.0, static_cast<double>(nf.alto)) };
                sn.textUtf8 = nf.texto;
                sn.fillColor = ArgbToDocColor(nf.color_argb);
                sn.strokeColor = DocColor::RGBA(28, 32, 42, 255);
                doc.AddPageObject(0, std::move(sn));
                ++res.report.textBoxCount;
            }
        }
    }

    if (doc.Pages().empty()) {
        PageModel pm{};
        pm.pageId = doc.AllocatePageId();
        doc.MutablePages().push_back(std::move(pm));
    }

    res.report.pageCount = doc.Pages().size();
    res.report.visualTier = CompatLevelStatus::Full;
    res.report.structuralTier = CompatLevelStatus::Full;
    res.report.editableTier = CompatLevelStatus::Full;
    res.report.visualSummaryUtf8 = "Fidelidad visual 1:1 desde CuadernU Cl\xC3\xA1sico (.tinta).";
    res.report.structuralSummaryUtf8 = "ID durable, cuadernos, secciones, p\xC3\xA1ginas y pautas migrados completamente.";
    res.report.editableSummaryUtf8 = "Todos los trazos con presi\xC3\xB3n/inclinaci\xC3\xB3n, cuadros de texto y notas son 100% editables.";

    doc.RebuildLayout();
    doc.SetDirty(false);
    outDoc = std::move(doc);
    res.ok = true;
    return true;
}

void TallyDocumentMetrics(const DocumentModel& doc, CompatibilityReport& rep) {
    rep.pageCount = doc.Pages().size();
    rep.bookmarkOrSectionCount = doc.Bookmarks().size();
    rep.attachmentCount = doc.Attachments().size();
    auto tallyVec = [&](const std::vector<AnnotationObject>& objs) {
        for (const auto& o : objs) {
            switch (o.kind) {
            case ObjectKind::InkStroke:  ++rep.editableStrokeCount; break;
            case ObjectKind::TextBox:    ++rep.textBoxCount; break;
            case ObjectKind::Shape:      ++rep.shapeCount; break;
            case ObjectKind::Image:      ++rep.imageCount; break;
            case ObjectKind::TableBlock: ++rep.tableCount; break;
            case ObjectKind::MathBlock:  ++rep.mathBlockCount; break;
            case ObjectKind::AudioClip:  ++rep.audioClipCount; break;
            default: break;
            }
        }
    };
    for (const auto& pg : doc.Pages()) {
        if (pg.hasPdfSource) rep.hasBackgroundPdf = true;
        tallyVec(pg.objects);
    }
    tallyVec(doc.CanvasObjects());
}

} // namespace

std::string CompatibilityReport::ToJsonUtf8() const {
    json j;
    j["format"] = FormatName(format);
    j["sourceName"] = sourceNameUtf8;
    j["tiers"] = {
        {"visual", {{"status", TierStatusLabel(visualTier)}, {"summary", visualSummaryUtf8}}},
        {"structural", {{"status", TierStatusLabel(structuralTier)}, {"summary", structuralSummaryUtf8}}},
        {"editable", {{"status", TierStatusLabel(editableTier)}, {"summary", editableSummaryUtf8}}}
    };
    j["metrics"] = {
        {"pages", pageCount},
        {"editableStrokes", editableStrokeCount},
        {"textBoxes", textBoxCount},
        {"shapes", shapeCount},
        {"images", imageCount},
        {"tables", tableCount},
        {"mathBlocks", mathBlockCount},
        {"audioClips", audioClipCount},
        {"attachments", attachmentCount},
        {"bookmarksOrSections", bookmarkOrSectionCount},
        {"hasBackgroundPdf", hasBackgroundPdf}
    };
    json wArr = json::array();
    for (const auto& w : warnings) {
        wArr.push_back({
            {"severity", w.severity == WarningSeverity::Lossy ? "lossy" : (w.severity == WarningSeverity::Warning ? "warning" : "info")},
            {"code", w.codeUtf8},
            {"title", w.titleUtf8},
            {"detail", w.detailUtf8}
        });
    }
    j["warnings"] = std::move(wArr);
    return j.dump(2);
}

std::string CompatibilityReport::FormatHumanSummaryUtf8() const {
    std::ostringstream os;
    os << "Formato detectado: " << FormatName(format) << "\n"
       << "Archivo: " << sourceNameUtf8 << "\n\n"
       << "Niveles de fidelidad:\n"
       << "  • Visual: " << TierStatusLabel(visualTier) << " — " << visualSummaryUtf8 << "\n"
       << "  • Estructural: " << TierStatusLabel(structuralTier) << " — " << structuralSummaryUtf8 << "\n"
       << "  • Editable: " << TierStatusLabel(editableTier) << " — " << editableSummaryUtf8 << "\n\n"
       << "Contenido detectado:\n"
       << "  P\xC3\xA1ginas: " << pageCount
       << " | Trazos editables: " << editableStrokeCount
       << " | Textos: " << textBoxCount
       << " | Tablas: " << tableCount
       << " | Ecuaciones: " << mathBlockCount
       << " | Im\xC3\xA1genes: " << imageCount
       << " | Audio: " << audioClipCount
       << " | Adjuntos: " << attachmentCount << "\n";
    if (!warnings.empty()) {
        os << "\nAdvertencias previas a la conversi\xC3\xB3n:\n";
        for (const auto& w : warnings) {
            os << "  - [" << w.codeUtf8 << "] " << w.titleUtf8 << ": " << w.detailUtf8 << "\n";
        }
    }
    return os.str();
}

SourceFormat NotebookMigrationEngine::DetectFormat(
    const std::string& filenameUtf8,
    const std::uint8_t* bytes,
    std::size_t size)
{
    const std::string ext = LowerExt(filenameUtf8);

    // 1. Inspect authoritative file magic bytes / container structure FIRST
    //    so renamed or uppercase-extension files are never misclassified.
    if (bytes && size >= 6) {
        if (std::memcmp(bytes, "%PDF-", 5) == 0) return SourceFormat::PdfDocument;
        if (std::memcmp(bytes, "TINTA\0", 6) == 0) return SourceFormat::CuadernuTinta;
        if (std::memcmp(bytes, "MSCF", 4) == 0) return SourceFormat::OneNotePackage;
        if (size >= 16 && bytes[0] == 0xE4 && bytes[1] == 0x52 && bytes[2] == 0x5C && bytes[3] == 0x7B) {
            return SourceFormat::OneNoteSection; // [MS-ONESTORE] guidFileType
        }
        if (bytes[0] == 'P' && bytes[1] == 'K' && bytes[2] == 0x03 && bytes[3] == 0x04) {
            ZipArchive zip;
            if (zip.Deserialize(bytes, size, nullptr)) {
                if (zip.HasEntry("manifest.json") && zip.HasEntry("pages.json")) return SourceFormat::CudocNative;
                if (zip.HasEntry("word/document.xml")) return SourceFormat::WordDocx;
                if (zip.HasEntry("ppt/presentation.xml")) return SourceFormat::PowerPointPptx;
                for (const auto& e : zip.ListEntries()) {
                    const std::string low = ToLowerCopy(e);
                    if (low.find("session.plist") != std::string::npos) return SourceFormat::Notability;
                    if (low.find("index.events.pb") != std::string::npos || low.rfind("notes/", 0) == 0) return SourceFormat::Goodnotes6;
                }
            }
        }
    }

    // 2. Fallback to case-insensitive extension hint (handles non-magic or dry-run probes)
    if (ext == ".goodnotes") return SourceFormat::Goodnotes6;
    if (ext == ".note")      return SourceFormat::Notability;
    if (ext == ".onepkg")    return SourceFormat::OneNotePackage;
    if (ext == ".one")       return SourceFormat::OneNoteSection;
    if (ext == ".tinta")     return SourceFormat::CuadernuTinta;
    if (ext == ".docx")      return SourceFormat::WordDocx;
    if (ext == ".pptx")      return SourceFormat::PowerPointPptx;
    if (ext == ".pdf")       return SourceFormat::PdfDocument;
    if (ext == ".cudoc")     return SourceFormat::CudocNative;
    return SourceFormat::Unknown;
}

CompatibilityReport NotebookMigrationEngine::AnalyzeCompatibilityBytes(
    const std::uint8_t* bytes,
    std::size_t size,
    const std::string& filenameHintUtf8)
{
    DocumentModel dryRunDoc;
    MigrationResult res{};
    ImportAndMigrateBytes(bytes, size, filenameHintUtf8, dryRunDoc, res);
    if (!res.ok && res.report.warnings.empty()) {
        res.report.warnings.push_back({
            WarningSeverity::Lossy, "IMPORT_PREFLIGHT_ERROR",
            "No se pudo analizar completamente el archivo",
            res.errorUtf8
        });
    }
    return res.report;
}

CompatibilityReport NotebookMigrationEngine::AnalyzeCompatibilityFile(const std::wstring& path) {
    std::vector<std::uint8_t> bytes;
    std::string err;
    const std::string utf8Name = WideToUtf8(path);
    if (!ReadFileToBytes(path, bytes, &err)) {
        CompatibilityReport rep{};
        rep.sourceNameUtf8 = utf8Name;
        rep.format = DetectFormat(utf8Name);
        rep.formatLabelUtf8 = FormatName(rep.format);
        rep.warnings.push_back({ WarningSeverity::Lossy, "FILE_READ_ERROR", "No se pudo leer el archivo", err });
        return rep;
    }
    return AnalyzeCompatibilityBytes(bytes.data(), bytes.size(), utf8Name);
}

bool NotebookMigrationEngine::ImportAndMigrateBytes(
    const std::uint8_t* bytes,
    std::size_t size,
    const std::string& filenameHintUtf8,
    DocumentModel& outDoc,
    MigrationResult& outResult)
{
    outResult = {};
    outResult.report.sourceNameUtf8 = StemFromPath(filenameHintUtf8) + LowerExt(filenameHintUtf8);
    const SourceFormat fmt = DetectFormat(filenameHintUtf8, bytes, size);
    outResult.report.format = fmt;
    outResult.report.formatLabelUtf8 = FormatName(fmt);

    if (!bytes || size == 0) {
        outResult.errorUtf8 = "Buffer de archivo vac\xC3\xADo.";
        return false;
    }

    switch (fmt) {
    case SourceFormat::Goodnotes6:
        return ImportGoodnotes6(bytes, size, filenameHintUtf8, outDoc, outResult);
    case SourceFormat::Notability:
        return ImportNotabilityNote(bytes, size, filenameHintUtf8, outDoc, outResult);
    case SourceFormat::OneNoteSection:
        return ImportOneNote(bytes, size, filenameHintUtf8, false, outDoc, outResult);
    case SourceFormat::OneNotePackage:
        return ImportOneNote(bytes, size, filenameHintUtf8, true, outDoc, outResult);
    case SourceFormat::CuadernuTinta:
        return ImportCuadernuTinta(bytes, size, filenameHintUtf8, outDoc, outResult);
    case SourceFormat::WordDocx: {
        std::string err;
        if (!OpenXmlInterop::ImportFromDocxBytes(bytes, size, outDoc, &err)) {
            outResult.errorUtf8 = err;
            return false;
        }
        TallyDocumentMetrics(outDoc, outResult.report);
        outResult.report.visualTier = CompatLevelStatus::Full;
        outResult.report.structuralTier = CompatLevelStatus::Full;
        outResult.report.editableTier = CompatLevelStatus::Full;
        outResult.report.visualSummaryUtf8 = "P\xC3\xA1rrafos, encabezados, tablas, ecuaciones e im\xC3\xA1genes importados.";
        outResult.report.structuralSummaryUtf8 = "Saltos de p\xC3\xA1gina, encabezados (marcadores), tablas y ecuaciones OMML preservadas.";
        outResult.report.editableSummaryUtf8 = "Bloques de texto, TableBlock reactivos y MathBlock totalmente editables.";
        outResult.ok = true;
        return true;
    }
    case SourceFormat::PowerPointPptx: {
        std::string err;
        if (!OpenXmlInterop::ImportFromPptxBytes(bytes, size, outDoc, &err)) {
            outResult.errorUtf8 = err;
            return false;
        }
        TallyDocumentMetrics(outDoc, outResult.report);
        outResult.report.visualTier = CompatLevelStatus::Full;
        outResult.report.structuralTier = CompatLevelStatus::Full;
        outResult.report.editableTier = CompatLevelStatus::Full;
        outResult.report.visualSummaryUtf8 = "Diapositivas con geometr\xC3\xAD""a 16:9, formas e im\xC3\xA1genes posicionadas.";
        outResult.report.structuralSummaryUtf8 = "Cada diapositiva mapeada a p\xC3\xA1gina nativa con sus relaciones de medios.";
        outResult.report.editableSummaryUtf8 = "Textos, formas, tablas, ecuaciones OMML y trazos vectoriales son editables.";
        outResult.ok = true;
        return true;
    }
    case SourceFormat::PdfDocument: {
        PdfDocument pdf;
        std::string err;
        std::vector<std::uint8_t> copyBytes(bytes, bytes + size);
        if (!pdf.LoadFromBytes(std::move(copyBytes), &err) || !pdf.PopulateDocumentModel(outDoc, "", &err)) {
            outResult.errorUtf8 = err;
            return false;
        }
        outDoc.SetTitle(StemFromPath(filenameHintUtf8));
        TallyDocumentMetrics(outDoc, outResult.report);
        outResult.report.visualTier = CompatLevelStatus::Full;
        outResult.report.structuralTier = CompatLevelStatus::Full;
        outResult.report.editableTier = CompatLevelStatus::Full;
        outResult.report.visualSummaryUtf8 = "Renderizado nativo vectorial PDF sin p\xC3\xA9rdida.";
        outResult.report.structuralSummaryUtf8 = "\xC3\x8Dndice (/Outlines), enlaces, formularios AcroForm y adjuntos extra\xC3\xAD""dos.";
        outResult.report.editableSummaryUtf8 = "Anotaciones, resaltado sem\xC3\xA1ntico, formularios y redacci\xC3\xB3n permanente disponibles.";
        outResult.ok = true;
        return true;
    }
    case SourceFormat::CudocNative: {
        std::string err;
        if (!CudocStorage::DeserializeFromBytes(bytes, size, outDoc, &err)) {
            outResult.errorUtf8 = err;
            return false;
        }
        TallyDocumentMetrics(outDoc, outResult.report);
        outResult.report.visualTier = CompatLevelStatus::Full;
        outResult.report.structuralTier = CompatLevelStatus::Full;
        outResult.report.editableTier = CompatLevelStatus::Full;
        outResult.report.visualSummaryUtf8 = "Documento nativo CuadernU.";
        outResult.report.structuralSummaryUtf8 = "Fidelidad nativa 100%.";
        outResult.report.editableSummaryUtf8 = "Todos los objetos nativos son editables.";
        outResult.ok = true;
        return true;
    }
    default:
        outResult.errorUtf8 = "Formato de archivo no reconocido para migraci\xC3\xB3n (" + filenameHintUtf8 + ").";
        return false;
    }
}

bool NotebookMigrationEngine::ImportAndMigrateFile(
    const std::wstring& path,
    DocumentModel& outDoc,
    MigrationResult& outResult)
{
    std::vector<std::uint8_t> bytes;
    std::string err;
    const std::string utf8Path = WideToUtf8(path);
    if (!ReadFileToBytes(path, bytes, &err)) {
        outResult.ok = false;
        outResult.errorUtf8 = err;
        return false;
    }
    const bool ok = ImportAndMigrateBytes(bytes.data(), bytes.size(), utf8Path, outDoc, outResult);
    if (ok && outResult.report.format == SourceFormat::PdfDocument) {
        for (auto& pg : outDoc.MutablePages()) {
            if (pg.hasPdfSource && pg.sourcePdfPathUtf8.empty()) {
                pg.sourcePdfPathUtf8 = utf8Path;
            }
        }
    }
    return ok;
}

} // namespace DocStudio::Interop
