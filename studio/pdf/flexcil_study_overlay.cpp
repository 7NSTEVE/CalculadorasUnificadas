#include "flexcil_study_overlay.hpp"

#include "../core/cudoc_storage.hpp"
#include "../core/image_processing.hpp"
#include "../core/utf_conv.hpp"
#include "../core/zip_container.hpp"
#include "../render/d2d_renderer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio {

namespace {

std::string CollectTextInsidePageRect(const PageModel& pg, const DocRect& pageRect) {
    const double pw = (std::max)(1.0, pg.EffectiveWidthPt());
    const double ph = (std::max)(1.0, pg.EffectiveHeightPt());
    const DocRect normTarget{
        std::clamp(pageRect.x / pw, 0.0, 1.0),
        std::clamp(pageRect.y / ph, 0.0, 1.0),
        std::clamp(pageRect.w / pw, 0.01, 1.0),
        std::clamp(pageRect.h / ph, 0.01, 1.0)
    };

    std::ostringstream oss;
    for (const auto& tb : pg.textBoxes) {
        if (tb.rectNorm.Intersects(normTarget) && !tb.textUtf8.empty()) {
            if (oss.tellp() > 0) oss << ' ';
            oss << tb.textUtf8;
        }
    }
    std::string collected = oss.str();
    if (collected.empty()) {
        // Fallback to page extracted text or TextBox annotations intersecting pageRect
        for (const auto& obj : pg.objects) {
            if (obj.bounds.Intersects(pageRect) && !obj.textUtf8.empty()) {
                if (!collected.empty()) collected.push_back(' ');
                collected += obj.textUtf8;
            }
        }
        if (collected.empty() && !pg.extractedTextUtf8.empty()) {
            collected = pg.extractedTextUtf8.substr(0, (std::min<std::size_t>)(160, pg.extractedTextUtf8.size()));
        }
    }
    return collected;
}

#ifdef _WIN32
bool EncodeRgbaToJpegWic(const ImageProc::RgbaImage& img, std::vector<std::uint8_t>& outJpegBytes) {
    outJpegBytes.clear();
    if (!img.Valid()) return false;
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;

    Microsoft::WRL::ComPtr<IStream> memStream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream))) return false;

    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder))) return false;
    if (FAILED(encoder->Initialize(memStream.Get(), WICBitmapEncoderNoCache))) return false;

    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> props;
    if (FAILED(encoder->CreateNewFrame(&frame, &props))) return false;
    if (FAILED(frame->Initialize(props.Get()))) return false;
    if (FAILED(frame->SetSize(static_cast<UINT>(img.width), static_cast<UINT>(img.height)))) return false;

    WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
    if (FAILED(frame->SetPixelFormat(&fmt))) return false;

    std::vector<std::uint8_t> bgr24(static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height) * 3u);
    for (int i = 0; i < img.width * img.height; ++i) {
        bgr24[static_cast<std::size_t>(i) * 3u + 0] = img.rgba[static_cast<std::size_t>(i) * 4u + 2];
        bgr24[static_cast<std::size_t>(i) * 3u + 1] = img.rgba[static_cast<std::size_t>(i) * 4u + 1];
        bgr24[static_cast<std::size_t>(i) * 3u + 2] = img.rgba[static_cast<std::size_t>(i) * 4u + 0];
    }

    if (FAILED(frame->WritePixels(
            static_cast<UINT>(img.height),
            static_cast<UINT>(img.width * 3),
            static_cast<UINT>(bgr24.size()),
            bgr24.data()))) {
        return false;
    }
    if (FAILED(frame->Commit()) || FAILED(encoder->Commit())) return false;

    STATSTG stg{};
    if (FAILED(memStream->Stat(&stg, STATFLAG_NONAME))) return false;
    const std::size_t sz = static_cast<std::size_t>(stg.cbSize.QuadPart);
    if (sz == 0) return false;
    outJpegBytes.resize(sz);
    LARGE_INTEGER zero{};
    memStream->Seek(zero, STREAM_SEEK_SET, nullptr);
    ULONG readBytes = 0;
    return SUCCEEDED(memStream->Read(outJpegBytes.data(), static_cast<ULONG>(sz), &readBytes)) && readBytes == sz;
}
#endif

} // namespace

// ---------------- FlexcilGestureRecognizer ----------------

FlexcilGestureResult FlexcilGestureRecognizer::AnalyzeGestureStroke(
    const PageModel& pdfPage,
    const std::vector<InkPoint>& strokePoints)
{
    FlexcilGestureResult res;
    if (strokePoints.size() < 3) return res;

    double minX = strokePoints[0].x, maxX = strokePoints[0].x;
    double minY = strokePoints[0].y, maxY = strokePoints[0].y;
    for (const auto& pt : strokePoints) {
        minX = (std::min)(minX, pt.x);
        maxX = (std::max)(maxX, pt.x);
        minY = (std::min)(minY, pt.y);
        maxY = (std::max)(maxY, pt.y);
    }
    const double w = (std::max)(8.0, maxX - minX);
    const double h = (std::max)(8.0, maxY - minY);
    res.capturedPageRect = { minX, minY, w, h };

    const double pw = (std::max)(1.0, pdfPage.EffectiveWidthPt());
    const double ph = (std::max)(1.0, pdfPage.EffectiveHeightPt());
    res.capturedNormRect = {
        std::clamp(minX / pw, 0.0, 0.98),
        std::clamp(minY / ph, 0.0, 0.98),
        std::clamp((minX + w) / pw, 0.02, 1.0),
        std::clamp((minY + h) / ph, 0.02, 1.0)
    };

    const InkPoint& pStart = strokePoints.front();
    const InkPoint& pEnd = strokePoints.back();
    const double closeDist = std::hypot(pEnd.x - pStart.x, pEnd.y - pStart.y);

    // 1. Horizontal underline / highlight gesture over text line
    if (w >= 30.0 && h <= 22.0) {
        res.kind = FlexcilGestureKind::UnderlineText;
        res.capturedPageRect = { minX, minY - 8.0, w, h + 16.0 };
        res.extractedTextUtf8 = CollectTextInsidePageRect(pdfPage, res.capturedPageRect);
        return res;
    }

    // 2. Closed 4-corner Square / Rectangle capture gesture
    if (w >= 24.0 && h >= 24.0 && closeDist <= (std::max)(w, h) * 0.35) {
        res.kind = FlexcilGestureKind::SquareCapture;
        res.extractedTextUtf8 = CollectTextInsidePageRect(pdfPage, res.capturedPageRect);
        return res;
    }

    // 3. 'L'-shape capture gesture: starts near (minX, minY), goes down to (minX, maxY), then right to (maxX, maxY)
    bool hasBottomLeftCorner = false;
    for (const auto& pt : strokePoints) {
        if (std::abs(pt.x - minX) <= w * 0.28 && std::abs(pt.y - maxY) <= h * 0.28) {
            hasBottomLeftCorner = true;
            break;
        }
    }
    const bool startTopLeft = (std::abs(pStart.x - minX) <= w * 0.30 && std::abs(pStart.y - minY) <= h * 0.30);
    const bool endBottomRight = (std::abs(pEnd.x - maxX) <= w * 0.30 && std::abs(pEnd.y - maxY) <= h * 0.30);
    if (w >= 24.0 && h >= 24.0 && hasBottomLeftCorner && startTopLeft && endBottomRight) {
        res.kind = FlexcilGestureKind::LCapture;
        res.extractedTextUtf8 = CollectTextInsidePageRect(pdfPage, res.capturedPageRect);
        return res;
    }

    // 4. Bracket selection fallback
    res.kind = FlexcilGestureKind::BracketSelect;
    res.extractedTextUtf8 = CollectTextInsidePageRect(pdfPage, res.capturedPageRect);
    return res;
}

// ---------------- FlexcilStudyOverlay ----------------

FlexcilStudyOverlay::FlexcilStudyOverlay() {
    studyNoteDoc_.ResetToBlank(DocumentMode::PaginatedPdf);
    studyNoteDoc_.SetTitle("Cuaderno Emergente de Estudio");
}

std::string FlexcilStudyOverlay::OpenPdfTabFromBytes(
    const std::string& titleUtf8,
    const std::string& virtualPathUtf8,
    const std::vector<std::uint8_t>& pdfBytes)
{
    PdfDocument parsed;
    if (!parsed.LoadFromBytes(pdfBytes)) return {};

    OpenPdfStudyTab tab;
    tab.tabIdUtf8 = "pdftab_" + std::to_string(nextTabSeq_++);
    tab.titleUtf8 = titleUtf8.empty() ? ("PDF " + tab.tabIdUtf8) : titleUtf8;
    tab.filePathUtf8 = virtualPathUtf8.empty() ? (tab.titleUtf8 + ".pdf") : virtualPathUtf8;

    tab.doc.ResetToBlank(DocumentMode::PaginatedPdf);
    tab.doc.SetTitle(tab.titleUtf8);
    const std::string assetKey = tab.doc.RegisterAsset(pdfBytes);
    tab.doc.MutablePages().clear();

    for (std::size_t i = 0; i < parsed.PageCount(); ++i) {
        const auto& pi = parsed.PageAt(i);
        PageModel pm;
        pm.pageId = tab.doc.AllocatePageId();
        pm.widthPt = pi.mediaBox.w > 0.0 ? pi.mediaBox.w : 595.0;
        pm.heightPt = pi.mediaBox.h > 0.0 ? pi.mediaBox.h : 842.0;
        pm.rotationDeg = pi.rotationDeg;
        pm.hasPdfSource = true;
        pm.sourcePdfPathUtf8 = tab.filePathUtf8;
        pm.sourcePdfAssetKey = assetKey;
        pm.sourcePageIndex0 = static_cast<int>(i);
        pm.extractedTextUtf8 = pi.extractedTextUtf8;
        pm.pdfLinks = pi.pdfLinks;
        pm.formFields = pi.formFields;
        pm.textBoxes = pi.textBoxes;
        tab.doc.MutablePages().push_back(std::move(pm));
    }
    if (tab.doc.Pages().empty()) {
        PageModel blank;
        blank.pageId = tab.doc.AllocatePageId();
        tab.doc.MutablePages().push_back(std::move(blank));
    }
    tab.doc.RebuildLayout();

    activeTabId_ = tab.tabIdUtf8;
    pdfTabs_.push_back(std::move(tab));
    return activeTabId_;
}

bool FlexcilStudyOverlay::ActivatePdfTab(const std::string& tabIdUtf8) {
    for (const auto& t : pdfTabs_) {
        if (t.tabIdUtf8 == tabIdUtf8) {
            activeTabId_ = tabIdUtf8;
            return true;
        }
    }
    return false;
}

OpenPdfStudyTab* FlexcilStudyOverlay::ActivePdfTabMutable() {
    for (auto& t : pdfTabs_) {
        if (t.tabIdUtf8 == activeTabId_) return &t;
    }
    return pdfTabs_.empty() ? nullptr : &pdfTabs_.front();
}

const OpenPdfStudyTab* FlexcilStudyOverlay::ActivePdfTab() const {
    for (const auto& t : pdfTabs_) {
        if (t.tabIdUtf8 == activeTabId_) return &t;
    }
    return pdfTabs_.empty() ? nullptr : &pdfTabs_.front();
}

bool FlexcilStudyOverlay::ApplyGestureStrokeOnActivePdf(
    std::size_t pageIndex0,
    const std::vector<InkPoint>& strokePts,
    FlexcilGestureResult* outGesture,
    std::uint64_t* outCreatedNoteObjId,
    std::uint64_t* outCreatedInkLinkId)
{
    OpenPdfStudyTab* tab = ActivePdfTabMutable();
    if (!tab || pageIndex0 >= tab->doc.Pages().size()) return false;

    PageModel& pg = tab->doc.MutablePages()[pageIndex0];
    const FlexcilGestureResult g = FlexcilGestureRecognizer::AnalyzeGestureStroke(pg, strokePts);
    if (outGesture) *outGesture = g;
    if (g.kind == FlexcilGestureKind::None) return false;

    // Add visual highlight / bracket box on the source PDF page
    AnnotationObject pdfMark;
    pdfMark.id = tab->doc.AllocateObjectId();
    pdfMark.zOrder = tab->doc.AllocateZOrder();
    pdfMark.kind = ObjectKind::Shape;
    pdfMark.shapeKind = ShapeKind::Rectangle;
    pdfMark.layerId = tab->doc.ActiveLayerId();
    pdfMark.bounds = g.capturedPageRect;
    pdfMark.isHighlighter = (g.kind == FlexcilGestureKind::UnderlineText);
    pdfMark.fillColor = (g.kind == FlexcilGestureKind::UnderlineText)
        ? DocColor::RGBA(250, 204, 21, 90)
        : DocColor::RGBA(59, 130, 246, 36);
    pdfMark.strokeColor = DocColor::RGBA(37, 99, 235, 220);
    pdfMark.strokeWidth = 1.4;
    pg.objects.push_back(std::move(pdfMark));

    // Create extracted card inside the Pop-up Study Note with automatic bi-directional InkLink
    const std::size_t noteCount = studyNoteDoc_.Pages()[0].objects.size();
    AnnotationObject noteCard;
    noteCard.kind = ObjectKind::TextBox;
    noteCard.layerId = studyNoteDoc_.ActiveLayerId();
    noteCard.bounds = { 36.0, 48.0 + static_cast<double>(noteCount) * 86.0, 480.0, 72.0 };
    noteCard.fillColor = DocColor::RGBA(239, 246, 255, 245);
    noteCard.strokeColor = DocColor::RGBA(30, 58, 138, 255);
    noteCard.fontSizePt = 11.5;
    const std::string snippet = g.extractedTextUtf8.empty() ? "[Recorte de Figura PDF]" : g.extractedTextUtf8;
    noteCard.textUtf8 = "[" + tab->titleUtf8 + " \xC2\xB7 P\xC3\xA1g. " + std::to_string(pageIndex0 + 1) + "] " + snippet;
    noteCard.captionUtf8 = "Cita PDF P\xC3\xA1g. " + std::to_string(pageIndex0 + 1);

    if (!studyNoteStack_.Execute(studyNoteDoc_, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, std::move(noteCard)))) {
        return false;
    }
    const std::uint64_t createdObjId = studyNoteDoc_.Pages()[0].objects.back().id;
    if (outCreatedNoteObjId) *outCreatedNoteObjId = createdObjId;

    InkLink link;
    link.sourceDocPathUtf8 = tab->filePathUtf8;
    link.sourcePageIndex0 = static_cast<int>(pageIndex0);
    link.sourceRectNorm = g.capturedNormRect;
    link.targetIsCanvas = false;
    link.targetPageIndex0 = 0;
    link.targetObjectId = createdObjId;
    link.labelUtf8 = tab->titleUtf8 + " P." + std::to_string(pageIndex0 + 1);

    const std::uint64_t linkId = studyNoteDoc_.AddInkLink(link);
    tab->doc.AddInkLink(link);
    if (outCreatedInkLinkId) *outCreatedInkLinkId = linkId;
    return true;
}

bool FlexcilStudyOverlay::ApplyGestureStrokeOnDocument(
    DocumentModel& sourceDoc,
    const std::string& sourcePathUtf8,
    std::size_t pageIndex0,
    const std::vector<InkPoint>& strokePts,
    FlexcilGestureResult* outGesture,
    std::uint64_t* outCreatedNoteObjId,
    std::uint64_t* outCreatedInkLinkId)
{
    if (pageIndex0 >= sourceDoc.Pages().size()) return false;
    PageModel& pg = sourceDoc.MutablePages()[pageIndex0];
    const FlexcilGestureResult g = FlexcilGestureRecognizer::AnalyzeGestureStroke(pg, strokePts);
    if (outGesture) *outGesture = g;
    if (g.kind == FlexcilGestureKind::None) return false;

    AnnotationObject pdfMark;
    pdfMark.id = sourceDoc.AllocateObjectId();
    pdfMark.zOrder = sourceDoc.AllocateZOrder();
    pdfMark.kind = ObjectKind::Shape;
    pdfMark.shapeKind = ShapeKind::Rectangle;
    pdfMark.layerId = sourceDoc.ActiveLayerId();
    pdfMark.bounds = g.capturedPageRect;
    pdfMark.isHighlighter = (g.kind == FlexcilGestureKind::UnderlineText);
    pdfMark.fillColor = (g.kind == FlexcilGestureKind::UnderlineText)
        ? DocColor::RGBA(250, 204, 21, 90)
        : DocColor::RGBA(59, 130, 246, 36);
    pdfMark.strokeColor = DocColor::RGBA(37, 99, 235, 220);
    pdfMark.strokeWidth = 1.4;
    pg.objects.push_back(std::move(pdfMark));
    sourceDoc.SetDirty(true);
    sourceDoc.BumpRevision();

    const std::string docTitle = sourceDoc.Title().empty() ? "Documento" : sourceDoc.Title();
    const std::size_t noteCount = studyNoteDoc_.Pages().empty() ? 0u : studyNoteDoc_.Pages()[0].objects.size();
    AnnotationObject noteCard;
    noteCard.kind = ObjectKind::TextBox;
    noteCard.layerId = studyNoteDoc_.ActiveLayerId();
    noteCard.bounds = { 24.0, 28.0 + static_cast<double>(noteCount) * 78.0, 320.0, 66.0 };
    noteCard.fillColor = DocColor::RGBA(239, 246, 255, 245);
    noteCard.strokeColor = DocColor::RGBA(30, 58, 138, 255);
    noteCard.fontSizePt = 11.5;
    const std::string snippet = g.extractedTextUtf8.empty() ? "[Recorte de Figura]" : g.extractedTextUtf8;
    noteCard.textUtf8 = "[" + docTitle + " \xC2\xB7 P\xC3\xA1g. " + std::to_string(pageIndex0 + 1) + "] " + snippet;
    noteCard.captionUtf8 = "Cita P\xC3\xA1g. " + std::to_string(pageIndex0 + 1);

    if (!studyNoteStack_.Execute(studyNoteDoc_, std::make_unique<AddObjectCommand>(ObjectTarget{ false, 0 }, std::move(noteCard)))) {
        return false;
    }
    const std::uint64_t createdObjId = studyNoteDoc_.Pages()[0].objects.back().id;
    if (outCreatedNoteObjId) *outCreatedNoteObjId = createdObjId;

    InkLink link;
    link.sourceDocPathUtf8 = sourcePathUtf8.empty() ? docTitle : sourcePathUtf8;
    link.sourcePageIndex0 = static_cast<int>(pageIndex0);
    link.sourceRectNorm = g.capturedNormRect;
    link.targetIsCanvas = false;
    link.targetPageIndex0 = 0;
    link.targetObjectId = createdObjId;
    link.labelUtf8 = docTitle + " P." + std::to_string(pageIndex0 + 1);

    const std::uint64_t linkId = studyNoteDoc_.AddInkLink(link);
    sourceDoc.AddInkLink(link);
    if (outCreatedInkLinkId) *outCreatedInkLinkId = linkId;
    popupVisible_ = true;
    return true;
}

std::string FlexcilStudyOverlay::SerializeStateToJson() const {
    json root;
    root["popupVisible"] = popupVisible_;
    root["gestureMode"] = gestureMode_;
    root["scrollDir"] = static_cast<int>(scrollDir_);
    root["popupScreenRect"] = { popupScreenRect_.x, popupScreenRect_.y, popupScreenRect_.w, popupScreenRect_.h };
    root["activeTabId"] = activeTabId_;

    json cards = json::array();
    if (!studyNoteDoc_.Pages().empty()) {
        for (const auto& o : studyNoteDoc_.Pages()[0].objects) {
            cards.push_back({
                {"id", o.id},
                {"text", o.textUtf8},
                {"caption", o.captionUtf8},
                {"bounds", {o.bounds.x, o.bounds.y, o.bounds.w, o.bounds.h}}
            });
        }
    }
    root["studyCards"] = std::move(cards);

    json links = json::array();
    for (const auto& lk : studyNoteDoc_.InkLinks()) {
        links.push_back({
            {"sourceDocPath", lk.sourceDocPathUtf8},
            {"sourcePageIndex0", lk.sourcePageIndex0},
            {"sourceRect", {lk.sourceRectNorm.left, lk.sourceRectNorm.top, lk.sourceRectNorm.right, lk.sourceRectNorm.bottom}},
            {"targetObjectId", lk.targetObjectId},
            {"label", lk.labelUtf8}
        });
    }
    root["inkLinks"] = std::move(links);

    json metas = json::array();
    for (const auto& m : metadataIndex_) {
        metas.push_back({
            {"docId", m.docIdUtf8},
            {"title", m.titleUtf8},
            {"author", m.authorUtf8},
            {"subject", m.subjectUtf8},
            {"keywords", m.keywordsUtf8},
            {"pageCount", m.pageCount},
            {"localPath", m.localPathUtf8}
        });
    }
    root["metadataIndex"] = std::move(metas);
    return root.dump(2);
}

bool FlexcilStudyOverlay::DeserializeStateFromJson(const std::string& jsonUtf8) {
    try {
        const json root = json::parse(jsonUtf8);
        if (!root.is_object()) return false;
        popupVisible_ = root.value("popupVisible", false);
        gestureMode_ = root.value("gestureMode", false);
        scrollDir_ = static_cast<PdfScrollDirection>(std::clamp(root.value("scrollDir", 0), 0, 1));
        if (root.contains("popupScreenRect") && root["popupScreenRect"].is_array() && root["popupScreenRect"].size() >= 4) {
            popupScreenRect_ = {
                root["popupScreenRect"][0].get<double>(),
                root["popupScreenRect"][1].get<double>(),
                root["popupScreenRect"][2].get<double>(),
                root["popupScreenRect"][3].get<double>()
            };
        }
        activeTabId_ = root.value("activeTabId", "");
        studyNoteDoc_.ResetToBlank(DocumentMode::PaginatedPdf);
        studyNoteDoc_.SetTitle("Cuaderno Emergente de Estudio");
        if (root.contains("studyCards") && root["studyCards"].is_array()) {
            for (const auto& cj : root["studyCards"]) {
                AnnotationObject card;
                card.id = cj.value("id", std::uint64_t{ 0 });
                card.kind = ObjectKind::TextBox;
                card.textUtf8 = cj.value("text", "");
                card.captionUtf8 = cj.value("caption", "");
                if (cj.contains("bounds") && cj["bounds"].is_array() && cj["bounds"].size() >= 4) {
                    card.bounds = {
                        cj["bounds"][0].get<double>(),
                        cj["bounds"][1].get<double>(),
                        cj["bounds"][2].get<double>(),
                        cj["bounds"][3].get<double>()
                    };
                }
                studyNoteDoc_.AddPageObject(0, std::move(card));
            }
        }
        if (root.contains("inkLinks") && root["inkLinks"].is_array()) {
            for (const auto& lj : root["inkLinks"]) {
                InkLink lk;
                lk.sourceDocPathUtf8 = lj.value("sourceDocPath", "");
                lk.sourcePageIndex0 = lj.value("sourcePageIndex0", 0);
                if (lj.contains("sourceRect") && lj["sourceRect"].is_array() && lj["sourceRect"].size() >= 4) {
                    lk.sourceRectNorm = {
                        lj["sourceRect"][0].get<double>(),
                        lj["sourceRect"][1].get<double>(),
                        lj["sourceRect"][2].get<double>(),
                        lj["sourceRect"][3].get<double>()
                    };
                }
                lk.targetObjectId = lj.value("targetObjectId", std::uint64_t{ 0 });
                lk.labelUtf8 = lj.value("label", "");
                studyNoteDoc_.AddInkLink(lk);
            }
        }
        metadataIndex_.clear();
        if (root.contains("metadataIndex") && root["metadataIndex"].is_array()) {
            for (const auto& mj : root["metadataIndex"]) {
                PdfSourceMetadata m;
                m.docIdUtf8 = mj.value("docId", "");
                m.titleUtf8 = mj.value("title", "");
                m.authorUtf8 = mj.value("author", "");
                m.subjectUtf8 = mj.value("subject", "");
                if (mj.contains("keywords") && mj["keywords"].is_array()) {
                    for (const auto& k : mj["keywords"]) if (k.is_string()) m.keywordsUtf8.push_back(k.get<std::string>());
                }
                m.pageCount = mj.value("pageCount", std::size_t{ 0 });
                m.localPathUtf8 = mj.value("localPath", "");
                metadataIndex_.push_back(std::move(m));
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

CitationJumpTarget FlexcilStudyOverlay::JumpFromStudyNoteObjectToSourcePdf(std::uint64_t studyNoteObjectId) {
    CitationJumpTarget target;
    for (const auto& lk : studyNoteDoc_.InkLinks()) {
        if (lk.targetObjectId == studyNoteObjectId) {
            target.valid = true;
            target.sourceDocPathUtf8 = lk.sourceDocPathUtf8;
            target.sourcePage0 = static_cast<std::size_t>((std::max)(0, lk.sourcePageIndex0));
            target.sourceRectNorm = lk.sourceRectNorm;
            target.labelUtf8 = lk.labelUtf8;

            for (auto& tab : pdfTabs_) {
                if (tab.filePathUtf8 == lk.sourceDocPathUtf8) {
                    activeTabId_ = tab.tabIdUtf8;
                    tab.currentPage0 = target.sourcePage0;
                    break;
                }
            }
            return target;
        }
    }
    return target;
}

// ---------------- Static Page, Bookmark & Export Managers ----------------

bool FlexcilStudyOverlay::DuplicatePage(DocumentModel& doc, std::size_t pageIndex0) {
    auto& pages = doc.MutablePages();
    if (pageIndex0 >= pages.size()) return false;
    PageModel copy = pages[pageIndex0];
    copy.pageId = doc.AllocatePageId();
    for (auto& o : copy.objects) {
        o.id = doc.AllocateObjectId();
        o.zOrder = doc.AllocateZOrder();
    }
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(pageIndex0 + 1), std::move(copy));
    doc.RebuildLayout();
    doc.SetDirty(true);
    doc.BumpRevision();
    return true;
}

bool FlexcilStudyOverlay::ReorderPage(DocumentModel& doc, std::size_t fromIndex0, std::size_t toIndex0) {
    auto& pages = doc.MutablePages();
    if (fromIndex0 >= pages.size() || toIndex0 >= pages.size() || fromIndex0 == toIndex0) return false;
    PageModel moving = std::move(pages[fromIndex0]);
    pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(fromIndex0));
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(toIndex0), std::move(moving));
    doc.RebuildLayout();
    doc.SetDirty(true);
    doc.BumpRevision();
    return true;
}

bool FlexcilStudyOverlay::InsertPageAt(DocumentModel& doc, std::size_t insertIndex0, PaperPattern pattern) {
    auto& pages = doc.MutablePages();
    const std::size_t idx = (std::min)(insertIndex0, pages.size());
    PageModel pg;
    pg.pageId = doc.AllocatePageId();
    pg.background = pattern;
    if (!pages.empty()) {
        pg.widthPt = pages.front().widthPt;
        pg.heightPt = pages.front().heightPt;
    }
    pages.insert(pages.begin() + static_cast<std::ptrdiff_t>(idx), std::move(pg));
    doc.RebuildLayout();
    doc.SetDirty(true);
    doc.BumpRevision();
    return true;
}

bool FlexcilStudyOverlay::DeletePageAt(DocumentModel& doc, std::size_t pageIndex0) {
    auto& pages = doc.MutablePages();
    if (pages.size() <= 1 || pageIndex0 >= pages.size()) return false;
    pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(pageIndex0));
    doc.RebuildLayout();
    doc.SetDirty(true);
    doc.BumpRevision();
    return true;
}

bool FlexcilStudyOverlay::RenameBookmark(
    DocumentModel& doc,
    std::uint64_t bookmarkId,
    const std::string& newTitleUtf8,
    int newLevel)
{
    if (newTitleUtf8.empty()) return false;
    auto bms = doc.Bookmarks();
    bool found = false;
    for (auto& bm : bms) {
        if (bm.id == bookmarkId) {
            bm.titleUtf8 = newTitleUtf8;
            if (newLevel >= 0) bm.level = newLevel;
            found = true;
            break;
        }
    }
    if (found) doc.SetBookmarks(std::move(bms));
    return found;
}

std::vector<PdfAnnotationInventoryItem> FlexcilStudyOverlay::ListAllAnnotations(const DocumentModel& doc) {
    std::vector<PdfAnnotationInventoryItem> list;
    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        for (const auto& obj : doc.Pages()[p].objects) {
            PdfAnnotationInventoryItem item;
            item.pageIndex0 = p;
            item.objectId = obj.id;
            item.kind = obj.kind;
            item.layerId = obj.layerId;
            item.bounds = obj.bounds;
            if (!obj.textUtf8.empty()) item.summaryUtf8 = obj.textUtf8;
            else if (!obj.htrRecognizedTextUtf8.empty()) item.summaryUtf8 = "[HTR] " + obj.htrRecognizedTextUtf8;
            else if (!obj.captionUtf8.empty()) item.summaryUtf8 = obj.captionUtf8;
            else if (obj.isHighlighter) item.summaryUtf8 = "Resaltado";
            else item.summaryUtf8 = "Trazo / Objeto #" + std::to_string(obj.id);
            list.push_back(std::move(item));
        }
    }
    return list;
}

std::string FlexcilStudyOverlay::ExportAnnotationsMarkdownSummary(const DocumentModel& doc) {
    std::ostringstream oss;
    oss << "# Resumen de Anotaciones PDF — " << doc.Title() << "\n\n";
    for (const auto& item : ListAllAnnotations(doc)) {
        oss << "- [P\xC3\xA1gina " << (item.pageIndex0 + 1) << "] " << item.summaryUtf8 << "\n";
    }
    return oss.str();
}

std::uint64_t FlexcilStudyOverlay::SignPdfFormWithInk(
    DocumentModel& doc,
    CommandStack& cmdStack,
    std::size_t pageIndex0,
    const std::string& formFieldNameUtf8,
    const std::string& signerNameUtf8,
    const std::vector<InkPoint>& signatureTrajectory)
{
    if (pageIndex0 >= doc.Pages().size()) return 0;
    PageModel& pg = doc.MutablePages()[pageIndex0];
    DocRect stampBounds{ 320.0, 680.0, 210.0, 56.0 };

    for (auto& ff : pg.formFields) {
        if (ff.nameUtf8 == formFieldNameUtf8 || formFieldNameUtf8.empty()) {
            ff.valueUtf8 = "Firmado por: " + signerNameUtf8;
            ff.checked = true;
            stampBounds = {
                ff.rectNorm.x * pg.EffectiveWidthPt(),
                ff.rectNorm.y * pg.EffectiveHeightPt(),
                (std::max)(140.0, ff.rectNorm.w * pg.EffectiveWidthPt()),
                (std::max)(42.0, ff.rectNorm.h * pg.EffectiveHeightPt())
            };
            break;
        }
    }

    AnnotationObject stamp;
    stamp.kind = ObjectKind::Shape;
    stamp.shapeKind = ShapeKind::SignatureStamp;
    stamp.layerId = doc.ActiveLayerId();
    stamp.bounds = stampBounds;
    stamp.strokeColor = DocColor::RGBA(23, 37, 84, 255);
    stamp.fillColor = DocColor::RGBA(239, 246, 255, 220);
    stamp.textUtf8 = "FIRMADO: " + signerNameUtf8;
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, pageIndex0 }, std::move(stamp)))) {
        return 0;
    }
    const std::uint64_t stampId = doc.Pages()[pageIndex0].objects.back().id;

    if (!signatureTrajectory.empty()) {
        AnnotationObject sigInk;
        sigInk.kind = ObjectKind::InkStroke;
        sigInk.layerId = doc.ActiveLayerId();
        sigInk.anchorObjectId = stampId;
        sigInk.strokeColor = DocColor::RGBA(15, 23, 42, 255);
        sigInk.strokeWidth = 2.2;
        sigInk.inkPoints = signatureTrajectory;
        sigInk.htrRecognizedTextUtf8 = signerNameUtf8;
        sigInk.RecomputeBounds();
        cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, pageIndex0 }, std::move(sigInk)));
    }
    return stampId;
}

void FlexcilStudyOverlay::IndexPdfSourceMetadata(const PdfSourceMetadata& meta) {
    for (auto& m : metadataIndex_) {
        if (m.docIdUtf8 == meta.docIdUtf8) {
            m = meta;
            return;
        }
    }
    metadataIndex_.push_back(meta);
}

std::vector<PdfSourceMetadata> FlexcilStudyOverlay::SearchIndexedPdfMetadata(const std::string& queryUtf8) const {
    const std::string q = FoldUtf8ForSearch(queryUtf8, false, true);
    std::vector<PdfSourceMetadata> hits;
    for (const auto& m : metadataIndex_) {
        bool match = q.empty() ||
            FoldUtf8ForSearch(m.titleUtf8, false, true).find(q) != std::string::npos ||
            FoldUtf8ForSearch(m.authorUtf8, false, true).find(q) != std::string::npos ||
            FoldUtf8ForSearch(m.subjectUtf8, false, true).find(q) != std::string::npos;
        if (!match) {
            for (const auto& kw : m.keywordsUtf8) {
                if (FoldUtf8ForSearch(kw, false, true).find(q) != std::string::npos) {
                    match = true;
                    break;
                }
            }
        }
        if (match) hits.push_back(m);
    }
    return hits;
}

bool FlexcilStudyOverlay::ExportSelectivePdfToBytes(
    const DocumentModel& doc,
    const PdfSelectiveExportOptions& opts,
    std::vector<std::uint8_t>& outPdfBytes,
    std::string* outErr)
{
    // Build filtered DocumentModel clone preserving shared assets
    std::vector<std::uint8_t> rawCudoc;
    DocumentModel filtered;
    if (!CudocStorage::SerializeToBytes(doc, rawCudoc, outErr) ||
        !CudocStorage::DeserializeFromBytes(rawCudoc.data(), rawCudoc.size(), filtered, outErr)) {
        return false;
    }

    std::vector<PageModel> keptPages;
    const auto& srcPages = filtered.Pages();
    auto shouldKeepPage = [&](std::size_t idx0) -> bool {
        if (!opts.selectedPages0.empty()) {
            if (std::find(opts.selectedPages0.begin(), opts.selectedPages0.end(), idx0) == opts.selectedPages0.end()) {
                return false;
            }
        }
        if (opts.onlyAnnotatedPages && srcPages[idx0].objects.empty()) {
            return false;
        }
        return true;
    };

    for (std::size_t i = 0; i < srcPages.size(); ++i) {
        if (!shouldKeepPage(i)) continue;
        PageModel pg = srcPages[i];
        if (!opts.includeAnnotations) {
            pg.objects.clear();
        }
        keptPages.push_back(std::move(pg));
    }
    if (keptPages.empty() && !srcPages.empty()) {
        PageModel pg = srcPages.front();
        if (!opts.includeAnnotations) pg.objects.clear();
        keptPages.push_back(std::move(pg));
    }
    filtered.MutablePages() = std::move(keptPages);
    filtered.RebuildLayout();

    if (!PdfWriter::ExportDocumentToBytes(filtered, outPdfBytes, outErr)) {
        return false;
    }

    // If editable mode (!opts.flattened && opts.includeAnnotations), append an embedded CuadernU editable trailer comment block
    // so ImportEditablePdfAnnotationsFromBytes can reconstruct 100% of editable AnnotationObjects losslessly
    if (!opts.flattened && opts.includeAnnotations) {
        std::vector<std::uint8_t> cudocPayload;
        if (CudocStorage::SerializeToBytes(filtered, cudocPayload, nullptr)) {
            static constexpr char kHex[] = "0123456789abcdef";
            std::string trailer = "\n%CUADERNU_EDITABLE_CUDOC_HEX_BEGIN:";
            trailer.reserve(trailer.size() + cudocPayload.size() * 2u + 40u);
            for (std::uint8_t b : cudocPayload) {
                trailer.push_back(kHex[(b >> 4) & 0x0Fu]);
                trailer.push_back(kHex[b & 0x0Fu]);
            }
            trailer += ":CUADERNU_EDITABLE_CUDOC_HEX_END\n";
            outPdfBytes.insert(outPdfBytes.end(), trailer.begin(), trailer.end());
        }
    }
    return true;
}

bool FlexcilStudyOverlay::ImportEditablePdfAnnotationsFromBytes(
    const std::vector<std::uint8_t>& pdfBytes,
    DocumentModel& outDoc,
    std::string* outErr)
{
    const std::string raw(reinterpret_cast<const char*>(pdfBytes.data()), pdfBytes.size());
    const std::string kBegin = "%CUADERNU_EDITABLE_CUDOC_HEX_BEGIN:";
    const std::string kEnd = ":CUADERNU_EDITABLE_CUDOC_HEX_END";
    const auto p0 = raw.find(kBegin);
    const auto p1 = (p0 != std::string::npos) ? raw.find(kEnd, p0 + kBegin.size()) : std::string::npos;
    if (p0 != std::string::npos && p1 != std::string::npos) {
        const std::string hex = raw.substr(p0 + kBegin.size(), p1 - (p0 + kBegin.size()));
        if (hex.size() >= 2 && (hex.size() % 2 == 0)) {
            std::vector<std::uint8_t> cudocBytes(hex.size() / 2u);
            auto hv = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return 0;
            };
            for (std::size_t i = 0; i < cudocBytes.size(); ++i) {
                cudocBytes[i] = static_cast<std::uint8_t>((hv(hex[2 * i]) << 4) | hv(hex[2 * i + 1]));
            }
            return CudocStorage::DeserializeFromBytes(cudocBytes.data(), cudocBytes.size(), outDoc, outErr);
        }
    }

    // Fallback: load as standard PDF
    PdfDocument parsed;
    if (!parsed.LoadFromBytes(pdfBytes, outErr)) return false;
    outDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    outDoc.MutablePages().clear();
    for (std::size_t i = 0; i < parsed.PageCount(); ++i) {
        PageModel pm;
        pm.pageId = outDoc.AllocatePageId();
        pm.widthPt = parsed.PageAt(i).mediaBox.w > 0.0 ? parsed.PageAt(i).mediaBox.w : 595.0;
        pm.heightPt = parsed.PageAt(i).mediaBox.h > 0.0 ? parsed.PageAt(i).mediaBox.h : 842.0;
        pm.extractedTextUtf8 = parsed.PageAt(i).extractedTextUtf8;
        outDoc.MutablePages().push_back(std::move(pm));
    }
    outDoc.RebuildLayout();
    return true;
}

bool FlexcilStudyOverlay::ExportPageToPngOrJpegFile(
    const DocumentModel& doc,
    std::size_t pageIndex0,
    const std::wstring& outPath,
    bool asJpeg,
    int targetWidthPx,
    std::string* outErr)
{
    if (pageIndex0 >= doc.Pages().size()) {
        if (outErr) *outErr = "Indice de pagina fuera de rango.";
        return false;
    }

    D2DRenderer renderer;
    if (!renderer.InitializeFactories()) {
        if (outErr) *outErr = "No se pudieron inicializar las factorias Direct2D/DirectWrite.";
        return false;
    }
    RasterImageBgra32 bgra;
    if (!renderer.RenderPageOffscreen(doc, pageIndex0, (std::max)(120, targetWidthPx), nullptr, bgra) || !bgra.Valid()) {
        if (outErr) *outErr = "Fallo el renderizado offscreen Direct2D de la pagina.";
        return false;
    }

    ImageProc::RgbaImage rgba;
    rgba.width = bgra.width;
    rgba.height = bgra.height;
    rgba.rgba.resize(bgra.pixels.size());
    for (std::size_t i = 0; i + 3 < bgra.pixels.size(); i += 4) {
        rgba.rgba[i + 0] = bgra.pixels[i + 2];
        rgba.rgba[i + 1] = bgra.pixels[i + 1];
        rgba.rgba[i + 2] = bgra.pixels[i + 0];
        rgba.rgba[i + 3] = 255u;
    }

    std::vector<std::uint8_t> encoded;
#ifdef _WIN32
    const bool okEnc = asJpeg
        ? EncodeRgbaToJpegWic(rgba, encoded)
        : ImageProc::EncodeRgbaToPngWic(rgba, encoded);
    if (!okEnc || encoded.empty()) {
        if (outErr) *outErr = "No se pudo codificar imagen con WIC.";
        return false;
    }
#else
    (void)asJpeg;
    if (outErr) *outErr = "WIC solo disponible en Windows.";
    return false;
#endif

    try {
        const fs::path p(WideToUtf8(outPath));
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs) return false;
        ofs.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
        ofs.flush();
        return static_cast<bool>(ofs);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool FlexcilStudyOverlay::ExportLocalNotabilityNtbPackage(
    const DocumentModel& doc,
    const std::wstring& outNtbPath,
    std::string* outErr)
{
    if (outNtbPath.empty()) {
        if (outErr) *outErr = "Ruta de salida .ntb vacia.";
        return false;
    }
    std::vector<std::uint8_t> cudocBytes;
    if (!CudocStorage::SerializeToBytes(doc, cudocBytes, outErr)) return false;
    std::vector<std::uint8_t> pdfBytes;
    PdfWriter::ExportDocumentToBytes(doc, pdfBytes, nullptr);

    ZipArchive zip;
    json meta = {
        {"format", "cuadernu.ntb.compat.v1"},
        {"title", doc.Title()},
        {"pages", doc.Pages().size()}
    };
    zip.SetTextEntry("Session.plist.json", meta.dump(2));
    zip.SetEntry("nb_payload.cudoc", std::move(cudocBytes));
    if (!pdfBytes.empty()) zip.SetEntry("Document.pdf", std::move(pdfBytes));

    std::vector<std::uint8_t> zipBytes;
    if (!zip.Serialize(zipBytes, true, outErr)) return false;

    try {
        const fs::path p(WideToUtf8(outNtbPath));
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs) return false;
        ofs.write(reinterpret_cast<const char*>(zipBytes.data()), static_cast<std::streamsize>(zipBytes.size()));
        ofs.flush();
        return static_cast<bool>(ofs);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

} // namespace DocStudio
