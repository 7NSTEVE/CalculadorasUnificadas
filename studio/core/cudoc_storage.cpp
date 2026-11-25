#include "cudoc_storage.hpp"
#include "utf_conv.hpp"
#include "zip_container.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace DocStudio {

namespace {

using json = nlohmann::json;

const char* ModeToString(DocumentMode m) noexcept {
    return (m == DocumentMode::InfiniteCanvas) ? "infinite_canvas" : "paginated_pdf";
}

bool StringToMode(const std::string& s, DocumentMode& out) noexcept {
    if (s == "paginated_pdf") { out = DocumentMode::PaginatedPdf; return true; }
    if (s == "infinite_canvas") { out = DocumentMode::InfiniteCanvas; return true; }
    return false;
}

const char* PatternToString(PaperPattern p) noexcept {
    switch (p) {
    case PaperPattern::Plain:      return "plain";
    case PaperPattern::Ruled:      return "ruled";
    case PaperPattern::Grid:       return "grid";
    case PaperPattern::Dots:       return "dots";
    case PaperPattern::Millimeter: return "millimeter";
    case PaperPattern::Isometric:  return "isometric";
    case PaperPattern::LogLog:     return "loglog";
    case PaperPattern::Polar:      return "polar";
    case PaperPattern::SmithChart: return "smith_chart";
    case PaperPattern::Cornell:    return "cornell";
    case PaperPattern::MusicStaff: return "music_staff";
    default:                       return "plain";
    }
}

PaperPattern StringToPattern(const std::string& s) noexcept {
    if (s == "ruled") return PaperPattern::Ruled;
    if (s == "grid") return PaperPattern::Grid;
    if (s == "dots") return PaperPattern::Dots;
    if (s == "millimeter") return PaperPattern::Millimeter;
    if (s == "isometric") return PaperPattern::Isometric;
    if (s == "loglog") return PaperPattern::LogLog;
    if (s == "polar") return PaperPattern::Polar;
    if (s == "smith_chart") return PaperPattern::SmithChart;
    if (s == "cornell") return PaperPattern::Cornell;
    if (s == "music_staff") return PaperPattern::MusicStaff;
    return PaperPattern::Plain;
}

const char* ObjectKindToString(ObjectKind k) noexcept {
    switch (k) {
    case ObjectKind::InkStroke:       return "ink";
    case ObjectKind::Shape:           return "shape";
    case ObjectKind::TextBox:         return "textbox";
    case ObjectKind::Image:           return "image";
    case ObjectKind::PdfCard:         return "pdfcard";
    case ObjectKind::StudyTape:       return "study_tape";
    case ObjectKind::MathBlock:       return "math_block";
    case ObjectKind::CircuitBlock:    return "circuit_block";
    case ObjectKind::CalcSessionCard: return "calc_card";
    case ObjectKind::PlotBlock:       return "plot_block";
    case ObjectKind::TableBlock:      return "table_block";
    case ObjectKind::CodeBlock:       return "code_block";
    case ObjectKind::AudioClip:       return "audio_clip";
    case ObjectKind::LinkCard:        return "link_card";
    case ObjectKind::VideoClip:       return "video_clip";
    case ObjectKind::HtmlEmbed:       return "html_embed";
    default:                          return "ink";
    }
}

bool StringToObjectKind(const std::string& s, ObjectKind& out) noexcept {
    if (s == "ink") { out = ObjectKind::InkStroke; return true; }
    if (s == "shape") { out = ObjectKind::Shape; return true; }
    if (s == "textbox") { out = ObjectKind::TextBox; return true; }
    if (s == "image") { out = ObjectKind::Image; return true; }
    if (s == "pdfcard") { out = ObjectKind::PdfCard; return true; }
    if (s == "study_tape") { out = ObjectKind::StudyTape; return true; }
    if (s == "math_block") { out = ObjectKind::MathBlock; return true; }
    if (s == "circuit_block") { out = ObjectKind::CircuitBlock; return true; }
    if (s == "calc_card") { out = ObjectKind::CalcSessionCard; return true; }
    if (s == "plot_block") { out = ObjectKind::PlotBlock; return true; }
    if (s == "table_block") { out = ObjectKind::TableBlock; return true; }
    if (s == "code_block") { out = ObjectKind::CodeBlock; return true; }
    if (s == "audio_clip") { out = ObjectKind::AudioClip; return true; }
    if (s == "link_card") { out = ObjectKind::LinkCard; return true; }
    if (s == "video_clip") { out = ObjectKind::VideoClip; return true; }
    if (s == "html_embed") { out = ObjectKind::HtmlEmbed; return true; }
    return false;
}

const char* ShapeKindToString(ShapeKind s) noexcept {
    switch (s) {
    case ShapeKind::Line:           return "line";
    case ShapeKind::Arrow:          return "arrow";
    case ShapeKind::Rectangle:      return "rectangle";
    case ShapeKind::Ellipse:        return "ellipse";
    case ShapeKind::SignatureStamp: return "signature_stamp";
    case ShapeKind::Connector:      return "connector";
    default:                        return "rectangle";
    }
}

ShapeKind StringToShapeKind(const std::string& s) noexcept {
    if (s == "line") return ShapeKind::Line;
    if (s == "arrow") return ShapeKind::Arrow;
    if (s == "ellipse") return ShapeKind::Ellipse;
    if (s == "signature_stamp") return ShapeKind::SignatureStamp;
    if (s == "connector") return ShapeKind::Connector;
    return ShapeKind::Rectangle;
}

json RectToJson(const DocRect& r) {
    return json{ {"x", r.x}, {"y", r.y}, {"w", r.w}, {"h", r.h} };
}

DocRect JsonToRect(const json& j) {
    DocRect r{};
    if (j.is_object()) {
        r.x = j.value("x", 0.0);
        r.y = j.value("y", 0.0);
        r.w = j.value("w", 0.0);
        r.h = j.value("h", 0.0);
    }
    return r;
}

json ObjectToJson(
    const AnnotationObject& obj,
    const std::string& scopeTag,
    std::size_t idx,
    ZipArchive& zip)
{
    json j;
    j["id"] = obj.id;
    j["zOrder"] = obj.zOrder;
    j["kind"] = ObjectKindToString(obj.kind);
    j["bounds"] = RectToJson(obj.bounds);
    j["strokeColor"] = obj.strokeColor.PackRgba();
    j["fillColor"] = obj.fillColor.PackRgba();
    j["strokeWidth"] = obj.strokeWidth;
    j["isHighlighter"] = obj.isHighlighter;
    if (obj.layerId != 1) j["layerId"] = obj.layerId;
    if (obj.anchorObjectId != 0) {
        j["anchorObjectId"] = obj.anchorObjectId;
        j["anchorOffset"] = { obj.anchorOffset.x, obj.anchorOffset.y };
    }
    if (!obj.audioAssetKey.empty()) j["audioAssetKey"] = obj.audioAssetKey;
    if (obj.inkStartTimeMs != 0) j["inkStartTimeMs"] = obj.inkStartTimeMs;
    if (obj.inkEndTimeMs != 0) j["inkEndTimeMs"] = obj.inkEndTimeMs;
    if (!obj.tags.empty()) j["tags"] = obj.tags;
    if (obj.mathSubMode != MathSubMode::VarSub) j["mathSubMode"] = static_cast<int>(obj.mathSubMode);
    if (obj.hasSlider) {
        j["hasSlider"] = true;
        j["sliderMin"] = obj.sliderMin;
        j["sliderMax"] = obj.sliderMax;
        j["sliderValue"] = obj.sliderValue;
    }
    if (std::abs(obj.rotationDeg) > 1e-6) j["rotationDeg"] = obj.rotationDeg;
    if (obj.opacity < 0.999) j["opacity"] = obj.opacity;
    if (obj.groupId != 0) j["groupId"] = obj.groupId;
    if (obj.underline) j["underline"] = true;
    if (!obj.listStyleUtf8.empty()) j["listStyle"] = obj.listStyleUtf8;
    if (!obj.imageCrop.IsFull()) {
        j["imageCrop"] = { obj.imageCrop.left, obj.imageCrop.top, obj.imageCrop.right, obj.imageCrop.bottom };
    }
    if (!obj.htrRecognizedTextUtf8.empty()) j["htrText"] = obj.htrRecognizedTextUtf8;
    if (!obj.tableJsonUtf8.empty()) j["tableJson"] = obj.tableJsonUtf8;
    if (obj.connectorFromId != 0) j["connectorFromId"] = obj.connectorFromId;
    if (obj.connectorToId != 0) j["connectorToId"] = obj.connectorToId;
    if (!obj.pdfQuadPointsNorm.empty()) {
        json qp = json::array();
        for (const auto& r : obj.pdfQuadPointsNorm) qp.push_back(RectToJson(r));
        j["pdfQuads"] = std::move(qp);
    }
    if (obj.isRedaction) j["isRedaction"] = true;
    if (obj.lockedInPlace) j["lockedInPlace"] = true;
    if (!obj.codeLanguageUtf8.empty()) j["codeLanguage"] = obj.codeLanguageUtf8;
    if (!obj.printoutTextUtf8.empty()) j["printoutText"] = obj.printoutTextUtf8;
    if (obj.printoutTextFromOcr) j["printoutTextFromOcr"] = true;
    if (!obj.printoutSourceKindUtf8.empty()) j["printoutSourceKind"] = obj.printoutSourceKindUtf8;
    if (!obj.printoutSourceNameUtf8.empty()) j["printoutSourceName"] = obj.printoutSourceNameUtf8;
    if (!obj.printoutTextRuns.empty()) {
        json trArr = json::array();
        for (const auto& tr : obj.printoutTextRuns) {
            trArr.push_back({
                {"rect", { tr.rectNorm.x, tr.rectNorm.y, tr.rectNorm.w, tr.rectNorm.h }},
                {"text", tr.textUtf8}
            });
        }
        j["printoutTextRuns"] = std::move(trArr);
    }

    if (obj.kind == ObjectKind::InkStroke) {
        j["penStyle"] = static_cast<int>(obj.penStyle);
        j["stabilization"] = obj.stabilization;
        if (obj.inkSnapped) j["inkSnapped"] = true;
        json pts = json::array();
        for (const auto& pt : obj.inkPoints) {
            if (pt.tiltXDeg != 0.0f || pt.tiltYDeg != 0.0f || pt.timestampMs != 0) {
                pts.push_back({ pt.x, pt.y, pt.pressure, pt.tiltXDeg, pt.tiltYDeg, pt.timestampMs });
            } else {
                pts.push_back({ pt.x, pt.y, pt.pressure });
            }
        }
        j["inkPoints"] = std::move(pts);
    } else if (obj.kind == ObjectKind::Shape) {
        j["shapeKind"] = ShapeKindToString(obj.shapeKind);
        j["shapeStart"] = { obj.shapeStart.x, obj.shapeStart.y };
        j["shapeEnd"] = { obj.shapeEnd.x, obj.shapeEnd.y };
        j["text"] = obj.textUtf8;
    } else if (obj.kind == ObjectKind::TextBox) {
        j["text"] = obj.textUtf8;
        j["textUtf8"] = obj.textUtf8;
        j["fontFamily"] = obj.fontFamilyUtf8;
        j["fontSizePt"] = obj.fontSizePt;
        j["bold"] = obj.bold;
        j["italic"] = obj.italic;
    } else if (obj.kind == ObjectKind::Image) {
        j["assetKey"] = obj.assetKey;
        j["imagePixelW"] = obj.imagePixelW;
        j["imagePixelH"] = obj.imagePixelH;
        j["caption"] = obj.captionUtf8;
        if (!obj.pngBytes.empty()) {
            const std::string inlinePath = "assets/_inline/" + scopeTag + "_" + std::to_string(idx) + ".bin";
            zip.SetEntry(inlinePath, obj.pngBytes);
            j["inlineAssetPath"] = inlinePath;
        }
    } else if (obj.kind == ObjectKind::PdfCard) {
        j["assetKey"] = obj.assetKey;
        j["imagePixelW"] = obj.imagePixelW;
        j["imagePixelH"] = obj.imagePixelH;
        j["pdfSourcePath"] = obj.pdfSourcePathUtf8;
        j["pdfSourceAssetKey"] = obj.pdfSourceAssetKey;
        j["pdfSourcePageIndex0"] = obj.pdfSourcePageIndex0;
        j["caption"] = obj.captionUtf8;
        if (!obj.pngBytes.empty()) {
            const std::string inlinePath = "assets/_inline/" + scopeTag + "_pdfcard_" + std::to_string(idx) + ".bin";
            zip.SetEntry(inlinePath, obj.pngBytes);
            j["inlineAssetPath"] = inlinePath;
        }
    } else if (obj.kind == ObjectKind::StudyTape) {
        j["tapeRevealed"] = obj.tapeRevealed;
        j["text"] = obj.textUtf8;
        j["caption"] = obj.captionUtf8;
    } else if (obj.kind == ObjectKind::MathBlock) {
        j["formulaExpr"] = obj.formulaExprUtf8;
        j["formulaVarName"] = obj.formulaVarNameUtf8;
        j["evaluatedResult"] = obj.evaluatedResultUtf8;
        j["stepCheckInput"] = obj.stepCheckInputUtf8;
        j["stepCheckStatus"] = obj.stepCheckStatusUtf8;
        j["text"] = obj.textUtf8;
        j["fontSizePt"] = obj.fontSizePt;
    } else if (obj.kind == ObjectKind::CircuitBlock) {
        j["circuitNetlist"] = obj.circuitNetlistUtf8;
        j["evaluatedResult"] = obj.evaluatedResultUtf8;
        j["caption"] = obj.captionUtf8;
        j["text"] = obj.textUtf8;
    } else if (obj.kind == ObjectKind::CalcSessionCard) {
        j["calcModelId"] = obj.calcModelIdUtf8;
        j["calcKeySeq"] = obj.calcKeySeqUtf8;
        j["calcLatex"] = obj.calcLatexUtf8;
        j["evaluatedResult"] = obj.evaluatedResultUtf8;
        j["text"] = obj.textUtf8;
        j["caption"] = obj.captionUtf8;
        if (!obj.pngBytes.empty()) {
            const std::string inlinePath = "assets/_inline/" + scopeTag + "_calc_" + std::to_string(idx) + ".bin";
            zip.SetEntry(inlinePath, obj.pngBytes);
            j["inlineAssetPath"] = inlinePath;
        }
    } else if (obj.kind == ObjectKind::PlotBlock) {
        j["plotConfig"] = obj.plotConfigUtf8;
        j["formulaExpr"] = obj.formulaExprUtf8;
        j["evaluatedResult"] = obj.evaluatedResultUtf8;
        j["caption"] = obj.captionUtf8;
    } else if (obj.kind == ObjectKind::TableBlock) {
        j["tableJson"] = obj.tableJsonUtf8;
        j["caption"] = obj.captionUtf8;
        j["text"] = obj.textUtf8;
    } else if (obj.kind == ObjectKind::CodeBlock) {
        j["text"] = obj.textUtf8;
        j["codeLanguage"] = obj.codeLanguageUtf8;
        j["evaluatedResult"] = obj.evaluatedResultUtf8;
        j["caption"] = obj.captionUtf8;
    } else if (obj.kind == ObjectKind::AudioClip) {
        const std::string effKey = !obj.audioAssetKey.empty() ? obj.audioAssetKey : obj.assetKey;
        j["audioAssetKey"] = effKey;
        j["assetKey"] = effKey;
        j["caption"] = obj.captionUtf8;
        j["text"] = obj.textUtf8;
    } else if (obj.kind == ObjectKind::LinkCard) {
        j["linkUrl"] = obj.linkUrlUtf8;
        j["linkTitle"] = obj.linkTitleUtf8;
        j["linkDescription"] = obj.linkDescriptionUtf8;
        j["linkTargetPageIndex"] = obj.linkTargetPageIndex;
        j["caption"] = obj.captionUtf8;
    } else if (obj.kind == ObjectKind::VideoClip) {
        j["videoSourcePath"] = obj.videoSourcePathUtf8;
        j["videoAssetKey"] = obj.videoAssetKey;
        j["videoDurationMs"] = obj.videoDurationMs;
        j["videoPositionMs"] = obj.videoPositionMs;
        j["videoMuted"] = obj.videoMuted;
        j["videoPlaybackRate"] = obj.videoPlaybackRate;
        j["videoLoop"] = obj.videoLoop;
        j["caption"] = obj.captionUtf8;
        if (!obj.videoPosterPngBytes.empty()) {
            const std::string posterPath = "assets/_inline/" + scopeTag + "_vposter_" + std::to_string(idx) + ".png";
            zip.SetEntry(posterPath, obj.videoPosterPngBytes);
            j["videoPosterAssetPath"] = posterPath;
        }
    } else if (obj.kind == ObjectKind::HtmlEmbed) {
        j["htmlSource"] = obj.htmlSourceUtf8;
        j["htmlUrl"] = obj.htmlUrlUtf8;
        j["htmlTitle"] = obj.htmlTitleUtf8;
        j["htmlInteractive"] = obj.htmlInteractive;
        j["htmlAllowNetwork"] = obj.htmlAllowNetwork;
        j["caption"] = obj.captionUtf8;
        if (!obj.htmlSnapshotPngBytes.empty()) {
            const std::string snapPath = "assets/_inline/" + scopeTag + "_htmlsnap_" + std::to_string(idx) + ".png";
            zip.SetEntry(snapPath, obj.htmlSnapshotPngBytes);
            j["htmlSnapshotAssetPath"] = snapPath;
        }
    }
    if (obj.gifPaused) j["gifPaused"] = true;
    if (obj.gifFrameCount > 1) j["gifFrameCount"] = obj.gifFrameCount;
    if (obj.gifAvgDelayMs > 0 && obj.gifAvgDelayMs != 100) j["gifAvgDelayMs"] = obj.gifAvgDelayMs;
    return j;
}

bool JsonToObject(const json& j, const ZipArchive& zip, AnnotationObject& out, std::string* outErr) {
    if (!j.is_object()) {
        if (outErr) *outErr = "Objeto con formato JSON no v\xC3\xA1lido.";
        return false;
    }
    const std::string kindStr = j.value("kind", "");
    if (!StringToObjectKind(kindStr, out.kind)) {
        if (outErr) *outErr = "Tipo de objeto desconocido en .cudoc: " + kindStr;
        return false;
    }
    out.id = j.value("id", std::uint64_t{ 0 });
    out.zOrder = j.value("zOrder", std::uint64_t{ 0 });
    out.bounds = JsonToRect(j.value("bounds", json::object()));
    out.strokeColor = DocColor::UnpackRgba(j.value("strokeColor", std::uint32_t{ 0x1A73E8FFu }));
    out.fillColor = DocColor::UnpackRgba(j.value("fillColor", std::uint32_t{ 0 }));
    out.strokeWidth = j.value("strokeWidth", 2.5);
    out.isHighlighter = j.value("isHighlighter", false);

    if (out.kind == ObjectKind::InkStroke && j.contains("inkPoints") && j["inkPoints"].is_array()) {
        for (const auto& pt : j["inkPoints"]) {
            if (pt.is_array() && pt.size() >= 2) {
                InkPoint ip{};
                ip.x = pt[0].get<double>();
                ip.y = pt[1].get<double>();
                ip.pressure = (pt.size() >= 3) ? pt[2].get<float>() : 0.5f;
                ip.tiltXDeg = (pt.size() >= 4) ? pt[3].get<float>() : 0.0f;
                ip.tiltYDeg = (pt.size() >= 5) ? pt[4].get<float>() : 0.0f;
                ip.timestampMs = (pt.size() >= 6) ? pt[5].get<std::uint64_t>() : 0ull;
                out.inkPoints.push_back(ip);
            }
        }
        out.penStyle = static_cast<InkPenStyle>(j.value("penStyle", 0));
        out.stabilization = j.value("stabilization", 0.20);
        out.inkSnapped = j.value("inkSnapped", false);
    } else if (out.kind == ObjectKind::Shape) {
        out.shapeKind = StringToShapeKind(j.value("shapeKind", "rectangle"));
        if (j.contains("shapeStart") && j["shapeStart"].is_array() && j["shapeStart"].size() >= 2) {
            out.shapeStart.x = j["shapeStart"][0].get<double>();
            out.shapeStart.y = j["shapeStart"][1].get<double>();
        }
        if (j.contains("shapeEnd") && j["shapeEnd"].is_array() && j["shapeEnd"].size() >= 2) {
            out.shapeEnd.x = j["shapeEnd"][0].get<double>();
            out.shapeEnd.y = j["shapeEnd"][1].get<double>();
        }
        out.textUtf8 = j.value("text", "");
    } else if (out.kind == ObjectKind::TextBox) {
        out.textUtf8 = j.value("text", "");
        out.fontFamilyUtf8 = j.value("fontFamily", "Segoe UI");
        out.fontSizePt = j.value("fontSizePt", 14.0);
        out.bold = j.value("bold", false);
        out.italic = j.value("italic", false);
    } else if (out.kind == ObjectKind::Image) {
        out.assetKey = j.value("assetKey", "");
        out.imagePixelW = j.value("imagePixelW", 0);
        out.imagePixelH = j.value("imagePixelH", 0);
        out.captionUtf8 = j.value("caption", "");
        const std::string inlinePath = j.value("inlineAssetPath", "");
        if (!inlinePath.empty()) {
            const auto* entry = zip.FindEntry(inlinePath);
            if (entry) out.pngBytes = *entry;
        }
    }
    out.layerId = j.value("layerId", std::uint64_t{ 1 });
    out.anchorObjectId = j.value("anchorObjectId", std::uint64_t{ 0 });
    if (j.contains("anchorOffset") && j["anchorOffset"].is_array() && j["anchorOffset"].size() >= 2) {
        out.anchorOffset.x = j["anchorOffset"][0].get<double>();
        out.anchorOffset.y = j["anchorOffset"][1].get<double>();
    }
    out.audioAssetKey = j.value("audioAssetKey", "");
    out.inkStartTimeMs = j.value("inkStartTimeMs", std::uint64_t{ 0 });
    out.inkEndTimeMs = j.value("inkEndTimeMs", std::uint64_t{ 0 });
    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& tj : j["tags"]) {
            if (tj.is_string()) out.tags.push_back(tj.get<std::string>());
        }
    }
    out.mathSubMode = static_cast<MathSubMode>(std::clamp(j.value("mathSubMode", 0), 0, 2));
    out.hasSlider = j.value("hasSlider", false);
    if (out.hasSlider) {
        out.sliderMin = j.value("sliderMin", 0.0);
        out.sliderMax = j.value("sliderMax", 100.0);
        out.sliderValue = j.value("sliderValue", 0.0);
    }
    out.rotationDeg = j.value("rotationDeg", 0.0);
    out.opacity = std::clamp(j.value("opacity", 1.0), 0.05, 1.0);
    out.groupId = j.value("groupId", std::uint64_t{ 0 });
    out.underline = j.value("underline", false);
    out.listStyleUtf8 = j.value("listStyle", "");
    if (j.contains("imageCrop") && j["imageCrop"].is_array() && j["imageCrop"].size() >= 4) {
        out.imageCrop.left = j["imageCrop"][0].get<double>();
        out.imageCrop.top = j["imageCrop"][1].get<double>();
        out.imageCrop.right = j["imageCrop"][2].get<double>();
        out.imageCrop.bottom = j["imageCrop"][3].get<double>();
        out.imageCrop.Clamp();
    }
    out.htrRecognizedTextUtf8 = j.value("htrText", "");
    out.tableJsonUtf8 = j.value("tableJson", "");
    out.connectorFromId = j.value("connectorFromId", std::uint64_t{ 0 });
    out.connectorToId = j.value("connectorToId", std::uint64_t{ 0 });
    if (j.contains("pdfQuads") && j["pdfQuads"].is_array()) {
        for (const auto& qj : j["pdfQuads"]) {
            out.pdfQuadPointsNorm.push_back(JsonToRect(qj));
        }
    }
    out.isRedaction = j.value("isRedaction", false);
    out.lockedInPlace = j.value("lockedInPlace", false);
    out.codeLanguageUtf8 = j.value("codeLanguage", "");
    out.printoutTextUtf8 = j.value("printoutText", "");
    out.printoutTextFromOcr = j.value("printoutTextFromOcr", false);
    out.printoutSourceKindUtf8 = j.value("printoutSourceKind", "");
    out.printoutSourceNameUtf8 = j.value("printoutSourceName", "");
    if (j.contains("printoutTextRuns") && j["printoutTextRuns"].is_array()) {
        for (const auto& tj : j["printoutTextRuns"]) {
            if (!tj.is_object()) continue;
            PdfTextRunBox tr{};
            tr.textUtf8 = tj.value("text", "");
            if (tj.contains("rect") && tj["rect"].is_array() && tj["rect"].size() >= 4) {
                tr.rectNorm.x = tj["rect"][0].get<double>();
                tr.rectNorm.y = tj["rect"][1].get<double>();
                tr.rectNorm.w = tj["rect"][2].get<double>();
                tr.rectNorm.h = tj["rect"][3].get<double>();
            }
            if (!tr.textUtf8.empty()) out.printoutTextRuns.push_back(std::move(tr));
        }
    }

    if (out.kind == ObjectKind::PdfCard) {
        out.assetKey = j.value("assetKey", "");
        out.imagePixelW = j.value("imagePixelW", 0);
        out.imagePixelH = j.value("imagePixelH", 0);
        out.pdfSourcePathUtf8 = j.value("pdfSourcePath", "");
        out.pdfSourceAssetKey = j.value("pdfSourceAssetKey", "");
        out.pdfSourcePageIndex0 = j.value("pdfSourcePageIndex0", 0);
        out.captionUtf8 = j.value("caption", "");
        const std::string inlinePath = j.value("inlineAssetPath", "");
        if (!inlinePath.empty()) {
            const auto* entry = zip.FindEntry(inlinePath);
            if (entry) out.pngBytes = *entry;
        }
    } else if (out.kind == ObjectKind::StudyTape) {
        out.tapeRevealed = j.value("tapeRevealed", false);
        out.textUtf8 = j.value("text", "");
        out.captionUtf8 = j.value("caption", "");
    } else if (out.kind == ObjectKind::MathBlock) {
        out.formulaExprUtf8 = j.value("formulaExpr", "");
        out.formulaVarNameUtf8 = j.value("formulaVarName", "");
        out.evaluatedResultUtf8 = j.value("evaluatedResult", "");
        out.stepCheckInputUtf8 = j.value("stepCheckInput", "");
        out.stepCheckStatusUtf8 = j.value("stepCheckStatus", "");
        out.textUtf8 = j.value("text", "");
        out.fontSizePt = j.value("fontSizePt", 14.0);
    } else if (out.kind == ObjectKind::CircuitBlock) {
        out.circuitNetlistUtf8 = j.value("circuitNetlist", "");
        out.evaluatedResultUtf8 = j.value("evaluatedResult", "");
        out.captionUtf8 = j.value("caption", "");
        out.textUtf8 = j.value("text", "");
    } else if (out.kind == ObjectKind::CalcSessionCard) {
        out.calcModelIdUtf8 = j.value("calcModelId", "");
        out.calcKeySeqUtf8 = j.value("calcKeySeq", "");
        out.calcLatexUtf8 = j.value("calcLatex", "");
        out.evaluatedResultUtf8 = j.value("evaluatedResult", "");
        out.captionUtf8 = j.value("caption", "");
        const std::string inlinePath = j.value("inlineAssetPath", "");
        if (!inlinePath.empty()) {
            const auto* entry = zip.FindEntry(inlinePath);
            if (entry) out.pngBytes = *entry;
        }
    } else if (out.kind == ObjectKind::PlotBlock) {
        out.plotConfigUtf8 = j.value("plotConfig", "");
        out.formulaExprUtf8 = j.value("formulaExpr", "");
        out.evaluatedResultUtf8 = j.value("evaluatedResult", "");
        out.captionUtf8 = j.value("caption", "");
    } else if (out.kind == ObjectKind::TableBlock) {
        out.tableJsonUtf8 = j.value("tableJson", "");
        out.captionUtf8 = j.value("caption", "");
        out.textUtf8 = j.value("text", "");
    } else if (out.kind == ObjectKind::CodeBlock) {
        out.textUtf8 = j.value("text", "");
        out.codeLanguageUtf8 = j.value("codeLanguage", "cpp");
        out.evaluatedResultUtf8 = j.value("evaluatedResult", "");
        out.captionUtf8 = j.value("caption", "");
    } else if (out.kind == ObjectKind::AudioClip) {
        out.audioAssetKey = j.value("audioAssetKey", "");
        out.assetKey = j.value("assetKey", "");
        if (out.audioAssetKey.empty()) out.audioAssetKey = out.assetKey;
        if (out.assetKey.empty()) out.assetKey = out.audioAssetKey;
        out.captionUtf8 = j.value("caption", "");
        out.textUtf8 = j.value("text", "");
    } else if (out.kind == ObjectKind::LinkCard) {
        out.linkUrlUtf8 = j.value("linkUrl", "");
        out.linkTitleUtf8 = j.value("linkTitle", "");
        out.linkDescriptionUtf8 = j.value("linkDescription", "");
        out.linkTargetPageIndex = j.value("linkTargetPageIndex", -1);
        out.captionUtf8 = j.value("caption", "");
    } else if (out.kind == ObjectKind::VideoClip) {
        out.videoSourcePathUtf8 = j.value("videoSourcePath", "");
        out.videoAssetKey = j.value("videoAssetKey", "");
        out.videoDurationMs = j.value("videoDurationMs", std::uint64_t{ 0 });
        out.videoPositionMs = j.value("videoPositionMs", std::uint64_t{ 0 });
        out.videoMuted = j.value("videoMuted", false);
        out.videoPlaybackRate = std::clamp(j.value("videoPlaybackRate", 1.0), 0.25, 4.0);
        out.videoLoop = j.value("videoLoop", false);
        out.captionUtf8 = j.value("caption", "");
        const std::string posterPath = j.value("videoPosterAssetPath", "");
        if (!posterPath.empty()) {
            if (const auto* entry = zip.FindEntry(posterPath)) out.videoPosterPngBytes = *entry;
        }
    } else if (out.kind == ObjectKind::HtmlEmbed) {
        out.htmlSourceUtf8 = j.value("htmlSource", "");
        out.htmlUrlUtf8 = j.value("htmlUrl", "");
        out.htmlTitleUtf8 = j.value("htmlTitle", "");
        out.htmlInteractive = j.value("htmlInteractive", false);
        out.htmlAllowNetwork = j.value("htmlAllowNetwork", false);
        out.captionUtf8 = j.value("caption", "");
        const std::string snapPath = j.value("htmlSnapshotAssetPath", "");
        if (!snapPath.empty()) {
            if (const auto* entry = zip.FindEntry(snapPath)) out.htmlSnapshotPngBytes = *entry;
        }
    }
    out.gifPaused = j.value("gifPaused", false);
    out.gifFrameCount = j.value("gifFrameCount", 0);
    out.gifAvgDelayMs = j.value("gifAvgDelayMs", 100);

    out.RecomputeBounds();
    return true;
}

bool ReadWholeFileBytes(const std::wstring& path, std::vector<std::uint8_t>& out, std::string* outErr) {
#ifdef _WIN32
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo abrir archivo para lectura (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(hFile, &li) || li.QuadPart < 0 || li.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(hFile);
        if (outErr) *outErr = "Tama\xC3\xB1o de archivo inv\xC3\xA1lido.";
        return false;
    }
    const std::size_t total = static_cast<std::size_t>(li.QuadPart);
    out.resize(total);
    std::size_t offset = 0;
    while (offset < total) {
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(total - offset, 4u * 1024u * 1024u));
        DWORD readNow = 0;
        if (!ReadFile(hFile, out.data() + offset, chunk, &readNow, nullptr) || readNow == 0) {
            CloseHandle(hFile);
            if (outErr) *outErr = "Fallo leyendo archivo (WinError " + std::to_string(GetLastError()) + ").";
            return false;
        }
        offset += readNow;
    }
    CloseHandle(hFile);
    return true;
#else
    const std::string utf8Path = WideToUtf8(path);
    const int fd = ::open(utf8Path.c_str(), O_RDONLY);
    if (fd < 0) {
        if (outErr) *outErr = "No se pudo abrir archivo para lectura (errno " + std::to_string(errno) + ").";
        return false;
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0 || st.st_size < 0 || st.st_size > 512ll * 1024ll * 1024ll) {
        ::close(fd);
        if (outErr) *outErr = "Tama\xC3\xB1o de archivo inv\xC3\xA1lido.";
        return false;
    }
    const std::size_t total = static_cast<std::size_t>(st.st_size);
    out.resize(total);
    std::size_t offset = 0;
    while (offset < total) {
        const std::size_t chunk = (std::min<std::size_t>)(total - offset, 4u * 1024u * 1024u);
        const ssize_t readNow = ::read(fd, out.data() + offset, chunk);
        if (readNow < 0) {
            if (errno == EINTR) continue;
            const int err = errno;
            ::close(fd);
            if (outErr) *outErr = "Fallo leyendo archivo (errno " + std::to_string(err) + ").";
            return false;
        }
        if (readNow == 0) {
            ::close(fd);
            if (outErr) *outErr = "Fallo leyendo archivo (EOF inesperado).";
            return false;
        }
        offset += static_cast<std::size_t>(readNow);
    }
    ::close(fd);
    return true;
#endif
}

bool WriteBytesAndFlush(const std::wstring& path, const std::vector<std::uint8_t>& data, std::string* outErr) {
#ifdef _WIN32
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo crear archivo temporal (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }
    std::size_t offset = 0;
    while (offset < data.size()) {
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(data.size() - offset, 4u * 1024u * 1024u));
        DWORD written = 0;
        if (!WriteFile(hFile, data.data() + offset, chunk, &written, nullptr) || written != chunk) {
            CloseHandle(hFile);
            DeleteFileW(path.c_str());
            if (outErr) *outErr = "Error escribiendo bytes en disco (WinError " + std::to_string(GetLastError()) + ").";
            return false;
        }
        offset += written;
    }
    if (!FlushFileBuffers(hFile)) {
        CloseHandle(hFile);
        DeleteFileW(path.c_str());
        if (outErr) *outErr = "Error en FlushFileBuffers (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }
    CloseHandle(hFile);
    return true;
#else
    const std::string utf8Path = WideToUtf8(path);
    const int fd = ::open(utf8Path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        if (outErr) *outErr = "No se pudo crear archivo temporal (errno " + std::to_string(errno) + ").";
        return false;
    }
    std::size_t offset = 0;
    while (offset < data.size()) {
        const std::size_t chunk = (std::min<std::size_t>)(data.size() - offset, 4u * 1024u * 1024u);
        const ssize_t written = ::write(fd, data.data() + offset, chunk);
        if (written < 0) {
            if (errno == EINTR) continue;
            const int err = errno;
            ::close(fd);
            ::unlink(utf8Path.c_str());
            if (outErr) *outErr = "Error escribiendo bytes en disco (errno " + std::to_string(err) + ").";
            return false;
        }
        if (written == 0) {
            ::close(fd);
            ::unlink(utf8Path.c_str());
            if (outErr) *outErr = "Error escribiendo bytes en disco.";
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    if (::fsync(fd) != 0 && errno != EINVAL && errno != EROFS && errno != ENOTSUP) {
        const int err = errno;
        ::close(fd);
        ::unlink(utf8Path.c_str());
        if (outErr) *outErr = "Error en fsync (errno " + std::to_string(err) + ").";
        return false;
    }
    ::close(fd);
    return true;
#endif
}

} // namespace

bool CudocStorage::SerializeToBytes(
    const DocumentModel& doc,
    std::vector<std::uint8_t>& outZipBytes,
    std::string* outErr)
{
    try {
        ZipArchive zip;

        for (const auto& kv : doc.Assets()) {
            zip.SetEntry("assets/" + kv.first, kv.second);
        }

        auto ensurePdfAssetEmbedded = [&](const std::string& pathUtf8, std::string& assetKey) {
            if (assetKey.empty() && !pathUtf8.empty()) {
                std::vector<std::uint8_t> pdfBytes;
                if (ReadWholeFileBytes(Utf8ToWide(pathUtf8), pdfBytes, nullptr) && !pdfBytes.empty()) {
                    std::uint64_t h = 14695981039346656037ull;
                    for (std::uint8_t b : pdfBytes) {
                        h ^= b;
                        h *= 1099511628211ull;
                    }
                    char hex[32]{};
                    std::snprintf(hex, sizeof(hex), "src_pdf_%016llx", static_cast<unsigned long long>(h));
                    assetKey = hex;
                    if (!zip.HasEntry("assets/" + assetKey)) {
                        zip.SetEntry("assets/" + assetKey, std::move(pdfBytes));
                    }
                }
            } else if (!assetKey.empty()) {
                const auto* existing = doc.FindAsset(assetKey);
                if (existing && !zip.HasEntry("assets/" + assetKey)) {
                    zip.SetEntry("assets/" + assetKey, *existing);
                }
            }
        };

        json pagesJson = json::array();
        for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
            const auto& pg = doc.Pages()[p];
            std::string pagePdfAsset = pg.sourcePdfAssetKey;
            if (pg.hasPdfSource) {
                ensurePdfAssetEmbedded(pg.sourcePdfPathUtf8, pagePdfAsset);
            }
            json pj;
            pj["pageId"] = pg.pageId;
            pj["widthPt"] = pg.widthPt;
            pj["heightPt"] = pg.heightPt;
            pj["rotationDeg"] = pg.rotationDeg;
            pj["crop"] = {
                {"left", pg.crop.left},
                {"top", pg.crop.top},
                {"right", pg.crop.right},
                {"bottom", pg.crop.bottom}
            };
            pj["background"] = PatternToString(pg.background);
            pj["hasPdfSource"] = pg.hasPdfSource;
            pj["sourcePdfPath"] = pg.sourcePdfPathUtf8;
            pj["sourcePdfAssetKey"] = pagePdfAsset;
            pj["sourcePageIndex0"] = pg.sourcePageIndex0;
            pj["extractedText"] = pg.extractedTextUtf8;
            pj["ocrText"] = pg.ocrTextUtf8;
            if (!pg.pdfLinks.empty()) {
                json linksArr = json::array();
                for (const auto& lnk : pg.pdfLinks) {
                    linksArr.push_back({
                        {"rect", {lnk.rectNorm.x, lnk.rectNorm.y, lnk.rectNorm.w, lnk.rectNorm.h}},
                        {"uri", lnk.uriUtf8},
                        {"targetPage0", lnk.targetPage0},
                        {"title", lnk.titleUtf8}
                    });
                }
                pj["pdfLinks"] = std::move(linksArr);
            }
            if (!pg.formFields.empty()) {
                json ffArr = json::array();
                for (const auto& ff : pg.formFields) {
                    ffArr.push_back({
                        {"name", ff.nameUtf8},
                        {"value", ff.valueUtf8},
                        {"fieldType", ff.fieldTypeUtf8},
                        {"checked", ff.checked},
                        {"readOnly", ff.readOnly},
                        {"rect", {ff.rectNorm.x, ff.rectNorm.y, ff.rectNorm.w, ff.rectNorm.h}}
                    });
                }
                pj["formFields"] = std::move(ffArr);
            }
            if (!pg.textBoxes.empty()) {
                json tbArr = json::array();
                for (const auto& tb : pg.textBoxes) {
                    tbArr.push_back({
                        {"rect", {tb.rectNorm.x, tb.rectNorm.y, tb.rectNorm.w, tb.rectNorm.h}},
                        {"text", tb.textUtf8}
                    });
                }
                pj["textBoxes"] = std::move(tbArr);
            }

            json objs = json::array();
            for (std::size_t i = 0; i < pg.objects.size(); ++i) {
                objs.push_back(ObjectToJson(pg.objects[i], "p" + std::to_string(p), i, zip));
            }
            pj["objects"] = std::move(objs);
            pagesJson.push_back(std::move(pj));
        }

        json canvasObjsJson = json::array();
        for (std::size_t i = 0; i < doc.CanvasObjects().size(); ++i) {
            AnnotationObject copyObj = doc.CanvasObjects()[i];
            if (copyObj.kind == ObjectKind::PdfCard && copyObj.assetKey.empty() && copyObj.pngBytes.empty()) {
                ensurePdfAssetEmbedded(copyObj.pdfSourcePathUtf8, copyObj.pdfSourceAssetKey);
            }
            canvasObjsJson.push_back(ObjectToJson(copyObj, "canvas", i, zip));
        }
        json objectsRoot;
        objectsRoot["canvasObjects"] = std::move(canvasObjsJson);

        json manifest;
        manifest["format"] = kFormatMagic;
        manifest["formatVersion"] = kCurrentFormatVersion;
        manifest["documentUuid"] = doc.DocumentUuid();
        manifest["title"] = doc.Title();
        manifest["mode"] = ModeToString(doc.Mode());
        manifest["doublePageSpread"] = doc.DoublePageSpread();
        manifest["canvasBackground"] = PatternToString(doc.CanvasBackground());
        manifest["pageCount"] = doc.Pages().size();
        manifest["canvasObjectCount"] = doc.CanvasObjects().size();
        manifest["nextObjectId"] = doc.NextObjectId();
        manifest["nextPageId"] = doc.NextPageId();
        manifest["nextZOrder"] = doc.NextZOrder();
        manifest["nextAssetSeq"] = doc.NextAssetSeq();
        manifest["activeLayerId"] = doc.ActiveLayerId();
        {
            json layersJson = json::array();
            for (const auto& l : doc.Layers()) {
                layersJson.push_back({
                    {"id", l.id},
                    {"name", l.nameUtf8},
                    {"visible", l.visible},
                    {"locked", l.locked},
                    {"includeInExport", l.includeInExport},
                    {"opacity", l.opacity}
                });
            }
            manifest["layers"] = std::move(layersJson);
        }
        {
            json linksJson = json::array();
            for (const auto& lk : doc.InkLinks()) {
                linksJson.push_back({
                    {"id", lk.id},
                    {"sourceDocPath", lk.sourceDocPathUtf8},
                    {"sourcePageIndex0", lk.sourcePageIndex0},
                    {"sourceRect", {lk.sourceRectNorm.left, lk.sourceRectNorm.top, lk.sourceRectNorm.right, lk.sourceRectNorm.bottom}},
                    {"targetIsCanvas", lk.targetIsCanvas},
                    {"targetPageIndex0", lk.targetPageIndex0},
                    {"targetObjectId", lk.targetObjectId},
                    {"label", lk.labelUtf8}
                });
            }
            manifest["inkLinks"] = std::move(linksJson);
        }
        {
            json colJson = json::array();
            for (const auto& cr : doc.CollapsedRanges()) {
                if (cr.collapsed) {
                    colJson.push_back({ {"start", cr.startPage0}, {"end", cr.endPage0}, {"collapsed", cr.collapsed} });
                }
            }
            manifest["collapsedRanges"] = std::move(colJson);
        }
        {
            json bmJson = json::array();
            for (const auto& bm : doc.Bookmarks()) {
                bmJson.push_back({
                    {"id", bm.id},
                    {"title", bm.titleUtf8},
                    {"pageIndex", bm.pageIndex},
                    {"worldY", bm.worldY},
                    {"level", bm.level}
                });
            }
            manifest["bookmarks"] = std::move(bmJson);
        }
        {
            json attJson = json::array();
            for (const auto& att : doc.Attachments()) {
                attJson.push_back({
                    {"filename", att.filenameUtf8},
                    {"mimeType", att.mimeTypeUtf8},
                    {"assetKey", att.assetKey},
                    {"sizeBytes", att.sizeBytes}
                });
            }
            manifest["attachments"] = std::move(attJson);
        }

        zip.SetTextEntry("manifest.json", manifest.dump(2));
        zip.SetTextEntry("pages.json", pagesJson.dump(2));
        zip.SetTextEntry("objects.json", objectsRoot.dump(2));
        for (const auto& [extName, extPayload] : doc.ExtensionPayloads()) {
            if (!extName.empty()) {
                zip.SetTextEntry("extensions/" + extName, extPayload);
            }
        }

        return zip.Serialize(outZipBytes, true, outErr);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool CudocStorage::DeserializeFromBytes(
    const std::uint8_t* bytes,
    std::size_t size,
    DocumentModel& outDoc,
    std::string* outErr)
{
    ZipArchive zip;
    if (!zip.Deserialize(bytes, size, outErr)) {
        return false;
    }

    const auto* manifestEntry = zip.FindEntry("manifest.json");
    const auto* pagesEntry = zip.FindEntry("pages.json");
    const auto* objectsEntry = zip.FindEntry("objects.json");
    if (!manifestEntry || !pagesEntry || !objectsEntry) {
        if (outErr) *outErr = "Archivo .cudoc incompleto: falta manifest.json, pages.json u objects.json.";
        return false;
    }

    try {
        const json manifest = json::parse(manifestEntry->begin(), manifestEntry->end());
        if (!manifest.is_object()) {
            if (outErr) *outErr = "manifest.json no es un objeto JSON.";
            return false;
        }
        if (manifest.value("format", "") != kFormatMagic) {
            if (outErr) *outErr = "Firma de formato .cudoc inv\xC3\xA1lida.";
            return false;
        }
        const int version = manifest.value("formatVersion", 0);
        if (version < 1 || version > kCurrentFormatVersion) {
            if (outErr) *outErr = "Versi\xC3\xB3n de formato .cudoc no soportada: " + std::to_string(version);
            return false;
        }

        DocumentMode mode = DocumentMode::PaginatedPdf;
        if (!StringToMode(manifest.value("mode", "paginated_pdf"), mode)) {
            if (outErr) *outErr = "Modo de documento desconocido en manifest.json.";
            return false;
        }

        const json pagesJson = json::parse(pagesEntry->begin(), pagesEntry->end());
        const json objectsRoot = json::parse(objectsEntry->begin(), objectsEntry->end());
        if (!pagesJson.is_array() || !objectsRoot.is_object()) {
            if (outErr) *outErr = "Estructura JSON inv\xC3\xA1lida en pages.json u objects.json.";
            return false;
        }

        DocumentModel tempDoc;
        tempDoc.ResetToBlank(mode);
        const std::string savedUuid = manifest.value("documentUuid", "");
        if (!savedUuid.empty()) tempDoc.SetDocumentUuid(savedUuid);
        tempDoc.SetTitle(manifest.value("title", "Sin t\xC3\xADtulo"));
        tempDoc.SetDoublePageSpread(manifest.value("doublePageSpread", false));
        tempDoc.SetCanvasBackground(StringToPattern(manifest.value("canvasBackground", "grid")));
        tempDoc.MutablePages().clear();

        // Restore named assets from assets/* (excluding assets/_inline/*) and extensions/* sidecars
        constexpr const char* kAssetPrefix = "assets/";
        constexpr const char* kInlinePrefix = "assets/_inline/";
        constexpr const char* kExtPrefix = "extensions/";
        for (const std::string& path : zip.ListEntries()) {
            if (path.rfind(kAssetPrefix, 0) == 0 && path.rfind(kInlinePrefix, 0) != 0) {
                const std::string key = path.substr(7);
                if (!key.empty()) {
                    const auto* data = zip.FindEntry(path);
                    if (data) tempDoc.SetNamedAsset(key, *data);
                }
            } else if (path.rfind(kExtPrefix, 0) == 0) {
                const std::string extName = path.substr(11);
                if (!extName.empty()) {
                    const auto* data = zip.FindEntry(path);
                    if (data) {
                        tempDoc.SetExtensionPayload(extName, std::string(data->begin(), data->end()));
                    }
                }
            }
        }

        std::uint64_t maxObjId = manifest.value("nextObjectId", std::uint64_t{ 0 });
        std::uint64_t maxPageId = manifest.value("nextPageId", std::uint64_t{ 0 });
        std::uint64_t maxZ = manifest.value("nextZOrder", std::uint64_t{ 0 });
        const std::uint64_t maxAsset = manifest.value("nextAssetSeq", std::uint64_t{ 0 });

        for (const auto& pj : pagesJson) {
            if (!pj.is_object()) {
                if (outErr) *outErr = "Elemento de p\xC3\xA1gina inv\xC3\xA1lido.";
                return false;
            }
            PageModel pg;
            pg.pageId = pj.value("pageId", std::uint64_t{ 0 });
            pg.widthPt = (std::max)(36.0, pj.value("widthPt", 595.0));
            pg.heightPt = (std::max)(36.0, pj.value("heightPt", 842.0));
            pg.rotationDeg = pj.value("rotationDeg", 0);
            if (pj.contains("crop") && pj["crop"].is_object()) {
                const auto& cj = pj["crop"];
                pg.crop.left = cj.value("left", 0.0);
                pg.crop.top = cj.value("top", 0.0);
                pg.crop.right = cj.value("right", 1.0);
                pg.crop.bottom = cj.value("bottom", 1.0);
                pg.crop.Clamp();
            }
            pg.background = StringToPattern(pj.value("background", "plain"));
            pg.hasPdfSource = pj.value("hasPdfSource", false);
            pg.sourcePdfPathUtf8 = pj.value("sourcePdfPath", "");
            pg.sourcePdfAssetKey = pj.value("sourcePdfAssetKey", "");
            pg.sourcePageIndex0 = pj.value("sourcePageIndex0", 0);
            pg.extractedTextUtf8 = pj.value("extractedText", "");
            pg.ocrTextUtf8 = pj.value("ocrText", "");
            if (pj.contains("pdfLinks") && pj["pdfLinks"].is_array()) {
                for (const auto& lj : pj["pdfLinks"]) {
                    if (!lj.is_object()) continue;
                    PdfLinkRect lr{};
                    if (lj.contains("rect") && lj["rect"].is_array() && lj["rect"].size() >= 4) {
                        lr.rectNorm = {
                            lj["rect"][0].get<double>(), lj["rect"][1].get<double>(),
                            lj["rect"][2].get<double>(), lj["rect"][3].get<double>()
                        };
                    }
                    lr.uriUtf8 = lj.value("uri", "");
                    lr.targetPage0 = lj.value("targetPage0", -1);
                    lr.titleUtf8 = lj.value("title", "");
                    pg.pdfLinks.push_back(std::move(lr));
                }
            }
            if (pj.contains("formFields") && pj["formFields"].is_array()) {
                for (const auto& fj : pj["formFields"]) {
                    if (!fj.is_object()) continue;
                    PdfFormField ff{};
                    ff.nameUtf8 = fj.value("name", "");
                    ff.valueUtf8 = fj.value("value", "");
                    ff.fieldTypeUtf8 = fj.value("fieldType", "Tx");
                    ff.checked = fj.value("checked", false);
                    ff.readOnly = fj.value("readOnly", false);
                    if (fj.contains("rect") && fj["rect"].is_array() && fj["rect"].size() >= 4) {
                        ff.rectNorm = {
                            fj["rect"][0].get<double>(), fj["rect"][1].get<double>(),
                            fj["rect"][2].get<double>(), fj["rect"][3].get<double>()
                        };
                    }
                    pg.formFields.push_back(std::move(ff));
                }
            }
            if (pj.contains("textBoxes") && pj["textBoxes"].is_array()) {
                for (const auto& tj : pj["textBoxes"]) {
                    if (!tj.is_object()) continue;
                    PdfTextRunBox tb{};
                    if (tj.contains("rect") && tj["rect"].is_array() && tj["rect"].size() >= 4) {
                        tb.rectNorm = {
                            tj["rect"][0].get<double>(), tj["rect"][1].get<double>(),
                            tj["rect"][2].get<double>(), tj["rect"][3].get<double>()
                        };
                    }
                    tb.textUtf8 = tj.value("text", "");
                    pg.textBoxes.push_back(std::move(tb));
                }
            }
            if (pg.pageId > maxPageId) maxPageId = pg.pageId;

            if (pj.contains("objects") && pj["objects"].is_array()) {
                for (const auto& oj : pj["objects"]) {
                    AnnotationObject obj;
                    if (!JsonToObject(oj, zip, obj, outErr)) return false;
                    if (obj.id > maxObjId) maxObjId = obj.id;
                    if (obj.zOrder > maxZ) maxZ = obj.zOrder;
                    pg.objects.push_back(std::move(obj));
                }
            }
            tempDoc.MutablePages().push_back(std::move(pg));
        }

        if (tempDoc.Pages().empty()) {
            PageModel blank;
            blank.pageId = ++maxPageId;
            tempDoc.MutablePages().push_back(std::move(blank));
        }

        if (objectsRoot.contains("canvasObjects") && objectsRoot["canvasObjects"].is_array()) {
            for (const auto& oj : objectsRoot["canvasObjects"]) {
                AnnotationObject obj;
                if (!JsonToObject(oj, zip, obj, outErr)) return false;
                if (obj.id > maxObjId) maxObjId = obj.id;
                if (obj.zOrder > maxZ) maxZ = obj.zOrder;
                tempDoc.AddCanvasObject(std::move(obj));
            }
        }

        if (manifest.contains("layers") && manifest["layers"].is_array()) {
            std::vector<LayerModel> restoredLayers;
            for (const auto& lj : manifest["layers"]) {
                if (!lj.is_object()) continue;
                LayerModel l;
                l.id = lj.value("id", std::uint64_t{ 1 });
                l.nameUtf8 = lj.value("name", "Capa");
                l.visible = lj.value("visible", true);
                l.locked = lj.value("locked", false);
                l.includeInExport = lj.value("includeInExport", true);
                l.opacity = lj.value("opacity", 1.0);
                restoredLayers.push_back(std::move(l));
            }
            if (!restoredLayers.empty()) {
                tempDoc.SetLayers(std::move(restoredLayers));
            }
        }
        tempDoc.SetActiveLayerId(manifest.value("activeLayerId", std::uint64_t{ 1 }));

        if (manifest.contains("inkLinks") && manifest["inkLinks"].is_array()) {
            std::vector<InkLink> links;
            for (const auto& lkj : manifest["inkLinks"]) {
                if (!lkj.is_object()) continue;
                InkLink lk;
                lk.id = lkj.value("id", std::uint64_t{ 0 });
                lk.sourceDocPathUtf8 = lkj.value("sourceDocPath", "");
                lk.sourcePageIndex0 = lkj.value("sourcePageIndex0", 0);
                if (lkj.contains("sourceRect") && lkj["sourceRect"].is_array() && lkj["sourceRect"].size() >= 4) {
                    lk.sourceRectNorm.left = lkj["sourceRect"][0].get<double>();
                    lk.sourceRectNorm.top = lkj["sourceRect"][1].get<double>();
                    lk.sourceRectNorm.right = lkj["sourceRect"][2].get<double>();
                    lk.sourceRectNorm.bottom = lkj["sourceRect"][3].get<double>();
                    lk.sourceRectNorm.Clamp();
                }
                lk.targetIsCanvas = lkj.value("targetIsCanvas", true);
                lk.targetPageIndex0 = lkj.value("targetPageIndex0", 0);
                lk.targetObjectId = lkj.value("targetObjectId", std::uint64_t{ 0 });
                lk.labelUtf8 = lkj.value("label", "");
                links.push_back(std::move(lk));
            }
            tempDoc.SetInkLinks(std::move(links));
        }

        if (manifest.contains("collapsedRanges") && manifest["collapsedRanges"].is_array()) {
            std::vector<CollapsedPageRange> cols;
            for (const auto& cj : manifest["collapsedRanges"]) {
                if (!cj.is_object()) continue;
                cols.push_back(CollapsedPageRange{
                    cj.value("start", 0),
                    cj.value("end", 0),
                    cj.value("collapsed", true)
                });
            }
            tempDoc.SetCollapsedRanges(std::move(cols));
        }

        if (manifest.contains("bookmarks") && manifest["bookmarks"].is_array()) {
            std::vector<DocumentBookmark> bms;
            for (const auto& bmj : manifest["bookmarks"]) {
                if (!bmj.is_object()) continue;
                DocumentBookmark bm{};
                bm.id = bmj.value("id", std::uint64_t{ 0 });
                bm.titleUtf8 = bmj.value("title", "");
                bm.pageIndex = bmj.value("pageIndex", std::size_t{ 0 });
                bm.worldY = bmj.value("worldY", 0.0);
                bm.level = bmj.value("level", 0);
                bms.push_back(std::move(bm));
            }
            tempDoc.SetBookmarks(std::move(bms));
        }

        if (manifest.contains("attachments") && manifest["attachments"].is_array()) {
            std::vector<EmbeddedAttachment> atts;
            for (const auto& aj : manifest["attachments"]) {
                if (!aj.is_object()) continue;
                EmbeddedAttachment att{};
                att.filenameUtf8 = aj.value("filename", "adjunto.bin");
                att.mimeTypeUtf8 = aj.value("mimeType", "application/octet-stream");
                att.assetKey = aj.value("assetKey", "");
                att.sizeBytes = aj.value("sizeBytes", std::size_t{ 0 });
                if (!att.assetKey.empty()) atts.push_back(std::move(att));
            }
            tempDoc.SetAttachments(std::move(atts));
        }

        tempDoc.RestoreCounters(maxObjId, maxPageId, maxZ, maxAsset);
        tempDoc.RebuildLayout();
        tempDoc.SetDirty(false);
        outDoc = std::move(tempDoc);
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool CudocStorage::SaveToFile(
    const std::wstring& path,
    DocumentModel& doc,
    std::string* outErr,
    bool simulateFailBeforeRename)
{
    std::vector<std::uint8_t> bytes;
    if (!SerializeToBytes(doc, bytes, outErr)) return false;

    const std::wstring tmpPath = path + L".tmp";
    const std::wstring bakPath = path + L".bak";

    if (!WriteBytesAndFlush(tmpPath, bytes, outErr)) {
        return false;
    }

    if (simulateFailBeforeRename) {
#ifdef _WIN32
        DeleteFileW(tmpPath.c_str());
#else
        ::unlink(WideToUtf8(tmpPath).c_str());
#endif
        if (outErr) *outErr = "Simulaci\xC3\xB3n de fallo antes del reemplazo at\xC3\xB3mico.";
        return false;
    }

#ifdef _WIN32
    const DWORD attrs = GetFileAttributesW(path.c_str());
    const bool destExists = (attrs != INVALID_FILE_ATTRIBUTES) && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
    if (destExists) {
        if (!ReplaceFileW(path.c_str(), tmpPath.c_str(), bakPath.c_str(), REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
            if (!MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                DeleteFileW(tmpPath.c_str());
                if (outErr) *outErr = "Fallo en ReplaceFileW/MoveFileExW (WinError " + std::to_string(GetLastError()) + ").";
                return false;
            }
        }
    } else {
        if (!MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(tmpPath.c_str());
            if (outErr) *outErr = "Fallo en MoveFileExW (WinError " + std::to_string(GetLastError()) + ").";
            return false;
        }
    }
#else
    const std::string dstUtf8 = WideToUtf8(path);
    const std::string tmpUtf8 = WideToUtf8(tmpPath);
    const std::string bakUtf8 = WideToUtf8(bakPath);
    struct stat st{};
    const bool destExists = (::stat(dstUtf8.c_str(), &st) == 0 && S_ISREG(st.st_mode));
    if (destExists) {
        const std::string bakTmpUtf8 = bakUtf8 + ".new";
        ::unlink(bakTmpUtf8.c_str());
        bool bakReady = false;
        if (::link(dstUtf8.c_str(), bakTmpUtf8.c_str()) == 0) {
            if (::rename(bakTmpUtf8.c_str(), bakUtf8.c_str()) == 0) {
                bakReady = true;
            } else {
                ::unlink(bakTmpUtf8.c_str());
            }
        }
        if (!bakReady) {
            std::vector<std::uint8_t> prevBytes;
            if (ReadWholeFileBytes(path, prevBytes, nullptr) &&
                WriteBytesAndFlush(bakPath + L".new", prevBytes, nullptr)) {
                if (::rename(bakTmpUtf8.c_str(), bakUtf8.c_str()) == 0) {
                    bakReady = true;
                } else {
                    ::unlink(bakTmpUtf8.c_str());
                }
            }
        }
        if (!bakReady) {
            std::rename(dstUtf8.c_str(), bakUtf8.c_str());
        }
    }
    if (std::rename(tmpUtf8.c_str(), dstUtf8.c_str()) != 0) {
        const int err = errno;
        ::unlink(tmpUtf8.c_str());
        if (outErr) *outErr = "Fallo en rename() at\xC3\xB3mico (errno " + std::to_string(err) + ").";
        return false;
    }
    const std::size_t slash = dstUtf8.find_last_of('/');
    const std::string parentDir = (slash == std::string::npos) ? "." : (slash == 0 ? "/" : dstUtf8.substr(0, slash));
    const int dfd = ::open(parentDir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) {
        ::fsync(dfd);
        ::close(dfd);
    }
#endif

    doc.SetDirty(false);
    return true;
}

bool CudocStorage::LoadFromFile(
    const std::wstring& path,
    DocumentModel& outDoc,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ReadWholeFileBytes(path, bytes, outErr)) return false;
    return DeserializeFromBytes(bytes.data(), bytes.size(), outDoc, outErr);
}

bool CudocStorage::LoadFromFileWithRecovery(
    const std::wstring& path,
    DocumentModel& outDoc,
    bool* outUsedBackup,
    std::string* outErr)
{
    if (outUsedBackup) *outUsedBackup = false;
    std::string primaryErr;
    if (LoadFromFile(path, outDoc, &primaryErr)) {
        return true;
    }
    const std::wstring bakPath = path + L".bak";
#ifdef _WIN32
    const DWORD bakAttrs = GetFileAttributesW(bakPath.c_str());
    const bool bakExists = (bakAttrs != INVALID_FILE_ATTRIBUTES && (bakAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    struct stat bakSt{};
    const bool bakExists = (::stat(WideToUtf8(bakPath).c_str(), &bakSt) == 0 && S_ISREG(bakSt.st_mode));
#endif
    if (bakExists) {
        std::string bakErr;
        if (LoadFromFile(bakPath, outDoc, &bakErr)) {
            if (outUsedBackup) *outUsedBackup = true;
            return true;
        }
        if (outErr) *outErr = "Principal: " + primaryErr + " | Respaldo .bak: " + bakErr;
        return false;
    }
    if (outErr) *outErr = primaryErr;
    return false;
}

} // namespace DocStudio
