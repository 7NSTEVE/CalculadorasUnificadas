#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

struct DocPoint {
    double x = 0.0;
    double y = 0.0;
};

struct InkPoint {
    double x = 0.0;
    double y = 0.0;
    float pressure = 0.5f; // normalized 0.0 .. 1.0
    float tiltXDeg = 0.0f; // stylus X tilt [-90..90]
    float tiltYDeg = 0.0f; // stylus Y tilt [-90..90]
    std::uint64_t timestampMs = 0; // millisecond capture offset for Ink Replay & Audio sync
};

struct DocRect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    double Right() const noexcept { return x + w; }
    double Bottom() const noexcept { return y + h; }
    bool Empty() const noexcept { return w <= 0.0 || h <= 0.0; }

    bool Intersects(const DocRect& o) const noexcept {
        return !(Right() < o.x || o.Right() < x || Bottom() < o.y || o.Bottom() < y);
    }

    bool Contains(double px, double py, double margin = 0.0) const noexcept {
        return px >= (x - margin) && px <= (Right() + margin) &&
               py >= (y - margin) && py <= (Bottom() + margin);
    }

    static DocRect FromMinMax(double minX, double minY, double maxX, double maxY) noexcept {
        return { minX, minY, (std::max)(0.0, maxX - minX), (std::max)(0.0, maxY - minY) };
    }
};

struct DocColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    static constexpr DocColor RGBA(std::uint8_t rr, std::uint8_t gg, std::uint8_t bb, std::uint8_t aa = 255) noexcept {
        return { rr, gg, bb, aa };
    }

    std::uint32_t PackRgba() const noexcept {
        return (static_cast<std::uint32_t>(r) << 24) |
               (static_cast<std::uint32_t>(g) << 16) |
               (static_cast<std::uint32_t>(b) << 8) |
               static_cast<std::uint32_t>(a);
    }

    static constexpr DocColor UnpackRgba(std::uint32_t v) noexcept {
        return {
            static_cast<std::uint8_t>((v >> 24) & 0xFFu),
            static_cast<std::uint8_t>((v >> 16) & 0xFFu),
            static_cast<std::uint8_t>((v >> 8) & 0xFFu),
            static_cast<std::uint8_t>(v & 0xFFu)
        };
    }
};

enum class DocumentMode : std::uint8_t {
    PaginatedPdf = 0,
    InfiniteCanvas = 1
};

enum class PaperPattern : std::uint8_t {
    Plain = 0,
    Ruled = 1,
    Grid = 2,
    Dots = 3,
    Millimeter = 4,
    Isometric = 5,
    LogLog = 6,
    Polar = 7,
    SmithChart = 8,
    Cornell = 9,
    MusicStaff = 10
};

enum class ObjectKind : std::uint8_t {
    InkStroke = 0,
    Shape = 1,
    TextBox = 2,
    Image = 3,
    PdfCard = 4,
    StudyTape = 5,
    MathBlock = 6,
    CircuitBlock = 7,
    CalcSessionCard = 8,
    PlotBlock = 9,
    TableBlock = 10,
    CodeBlock = 11,
    AudioClip = 12,
    LinkCard = 13,
    VideoClip = 14,
    HtmlEmbed = 15
};

enum class InkPenStyle : std::uint8_t {
    Ballpoint = 0,
    Fountain = 1,
    Brush = 2,
    Pencil = 3,
    Highlighter = 4
};

enum class MathSubMode : std::uint8_t {
    VarSub = 0, // #varsub: symbolic + substituted values/units + result
    NoSub = 1,  // #nosub:  symbolic + result
    NoVar = 2   // #novar:  substituted values/units + result
};

enum class EraserFilterMode : std::uint8_t {
    All = 0,
    HighlighterOnly = 1,
    InkOnly = 2,
    ShapesOnly = 3
};

enum class EraserActionMode : std::uint8_t {
    WholeStroke = 0,
    PartialPixel = 1
};

enum class ShapeKind : std::uint8_t {
    Line = 0,
    Arrow = 1,
    Rectangle = 2,
    Ellipse = 3,
    SignatureStamp = 4,
    Connector = 5
};

struct NormalizedCropRect {
    double left = 0.0;
    double top = 0.0;
    double right = 1.0;
    double bottom = 1.0;

    bool IsFull() const noexcept {
        return left <= 0.0001 && top <= 0.0001 && right >= 0.9999 && bottom >= 0.9999;
    }

    void Clamp() noexcept {
        left = std::clamp(left, 0.0, 0.95);
        top = std::clamp(top, 0.0, 0.95);
        right = std::clamp(right, left + 0.05, 1.0);
        bottom = std::clamp(bottom, top + 0.05, 1.0);
    }
};

struct PdfTextRunBox {
    std::string textUtf8;
    DocRect rectNorm{}; // [0..1] top-left normalized on page
};

struct AnnotationObject {
    std::uint64_t id = 0;
    std::uint64_t zOrder = 0;
    ObjectKind kind = ObjectKind::InkStroke;
    DocRect bounds{};

    // Appearance
    DocColor strokeColor = DocColor::RGBA(26, 115, 232, 255);
    DocColor fillColor = DocColor::RGBA(0, 0, 0, 0);
    double strokeWidth = 2.5;
    bool isHighlighter = false;

    // InkStroke
    std::vector<InkPoint> inkPoints;

    // Shape (also stores endpoints p0->p1 in shapeStart/shapeEnd for line/arrow)
    ShapeKind shapeKind = ShapeKind::Rectangle;
    DocPoint shapeStart{};
    DocPoint shapeEnd{};

    // TextBox
    std::string textUtf8;
    std::string fontFamilyUtf8 = "Segoe UI";
    double fontSizePt = 14.0;
    bool bold = false;
    bool italic = false;

    // Image (PNG bytes stored in DocumentModel::assets[assetKey] or inline pngBytes)
    std::string assetKey;
    std::vector<std::uint8_t> pngBytes;
    int imagePixelW = 0;
    int imagePixelH = 0;
    std::string captionUtf8;

    // PdfCard (used on InfiniteCanvas when importing a PDF page)
    std::string pdfSourcePathUtf8;
    std::string pdfSourceAssetKey;
    int pdfSourcePageIndex0 = 0;

    // Printout (copia impresa): searchable text layer, provenance and background lock
    std::string printoutTextUtf8;                // full page text (embedded PDF text or OCR)
    std::vector<PdfTextRunBox> printoutTextRuns; // text runs with [0..1] boxes relative to bounds
    bool printoutTextFromOcr = false;
    std::string printoutSourceKindUtf8;          // "pdf" | "docx" | "pptx"
    std::string printoutSourceNameUtf8;          // original file name (no directory)
    bool lockedInPlace = false;                  // pinned to background: ignored by move/lasso/eraser hits

    // Layers, Ink physics, Anchor, Study Tape, Audio timeline & STEM card extensions
    std::uint64_t layerId = 1;
    InkPenStyle penStyle = InkPenStyle::Ballpoint;
    double stabilization = 0.20;
    bool inkSnapped = false;
    std::uint64_t anchorObjectId = 0;
    DocPoint anchorOffset{};
    bool tapeRevealed = false;
    std::string formulaExprUtf8;
    std::string formulaVarNameUtf8;
    std::string evaluatedResultUtf8;
    std::string stepCheckInputUtf8;
    std::string stepCheckStatusUtf8;
    std::string circuitNetlistUtf8;
    std::string plotConfigUtf8;
    std::string calcModelIdUtf8;
    std::string calcKeySeqUtf8;
    std::string calcLatexUtf8;
    std::string audioAssetKey;
    std::uint64_t inkStartTimeMs = 0;
    std::uint64_t inkEndTimeMs = 0;

    // Calcpad #varsub/#nosub/#novar, Maple Learn Parameter Sliders & OneNote/Goodnotes Tags
    MathSubMode mathSubMode = MathSubMode::VarSub;
    bool hasSlider = false;
    double sliderMin = 0.0;
    double sliderMax = 100.0;
    double sliderValue = 0.0;
    std::vector<std::string> tags; // e.g. "#examen", "#duda", "#definicion", "#pendiente"

    // Direct manipulation, grouping, rich text, image crop/opacity, HTR, tables, connectors, code & PDF semantics
    double rotationDeg = 0.0;
    double opacity = 1.0;
    std::uint64_t groupId = 0;
    bool underline = false;
    std::string listStyleUtf8; // "", "bullet", "number", "checkbox"
    NormalizedCropRect imageCrop{};
    std::string htrRecognizedTextUtf8;
    std::string tableJsonUtf8;
    std::uint64_t connectorFromId = 0;
    std::uint64_t connectorToId = 0;
    std::vector<DocRect> pdfQuadPointsNorm;
    bool isRedaction = false;
    std::string codeLanguageUtf8; // "cpp", "c", "python", "ino"

    // Rich Multimedia: Interactive LinkCard, Native Media Foundation VideoClip, Lazy WebView2 HtmlEmbed, GIF89a control
    std::string linkUrlUtf8;
    std::string linkTitleUtf8;
    std::string linkDescriptionUtf8;
    int linkTargetPageIndex = -1; // >= 0 for internal page jump, -1 for external URL

    std::string videoSourcePathUtf8;
    std::string videoAssetKey;
    std::uint64_t videoDurationMs = 0;
    std::uint64_t videoPositionMs = 0;
    std::vector<std::uint8_t> videoPosterPngBytes;
    bool videoMuted = false;
    double videoPlaybackRate = 1.0;
    bool videoLoop = false;

    std::string htmlSourceUtf8;
    std::string htmlUrlUtf8;
    std::string htmlTitleUtf8;
    bool htmlInteractive = false;
    bool htmlAllowNetwork = false;
    std::vector<std::uint8_t> htmlSnapshotPngBytes;

    bool gifPaused = false;
    int gifFrameCount = 0;
    int gifAvgDelayMs = 100;

    void RecomputeBounds() noexcept {
        if (kind == ObjectKind::InkStroke) {
            if (inkPoints.empty()) {
                bounds = {};
                return;
            }
            double minX = inkPoints[0].x, maxX = inkPoints[0].x;
            double minY = inkPoints[0].y, maxY = inkPoints[0].y;
            for (const auto& pt : inkPoints) {
                if (pt.x < minX) minX = pt.x;
                if (pt.x > maxX) maxX = pt.x;
                if (pt.y < minY) minY = pt.y;
                if (pt.y > maxY) maxY = pt.y;
            }
            const double pad = (std::max)(2.0, strokeWidth);
            bounds = { minX - pad, minY - pad, (maxX - minX) + pad * 2.0, (maxY - minY) + pad * 2.0 };
        } else if (kind == ObjectKind::Shape &&
                   (shapeKind == ShapeKind::Line || shapeKind == ShapeKind::Arrow || shapeKind == ShapeKind::Connector)) {
            const double minX = (std::min)(shapeStart.x, shapeEnd.x);
            const double minY = (std::min)(shapeStart.y, shapeEnd.y);
            const double maxX = (std::max)(shapeStart.x, shapeEnd.x);
            const double maxY = (std::max)(shapeStart.y, shapeEnd.y);
            const double pad = (std::max)(4.0, strokeWidth * 2.0);
            bounds = { minX - pad, minY - pad, (maxX - minX) + pad * 2.0, (maxY - minY) + pad * 2.0 };
        }
    }

    void Translate(double dx, double dy) noexcept {
        bounds.x += dx;
        bounds.y += dy;
        for (auto& pt : inkPoints) {
            pt.x += dx;
            pt.y += dy;
        }
        shapeStart.x += dx;
        shapeStart.y += dy;
        shapeEnd.x += dx;
        shapeEnd.y += dy;
        RecomputeBounds();
    }
};

struct PdfLinkRect {
    DocRect rectNorm{};     // [0..1] top-left normalized on page
    int targetPage0 = -1;   // >= 0 for internal page jump
    std::string uriUtf8;    // non-empty for external URI
    std::string titleUtf8;
};

struct PdfFormField {
    std::string nameUtf8;
    std::string fieldTypeUtf8; // "Tx", "Btn", "Ch"
    std::string valueUtf8;
    DocRect rectNorm{};
    bool checked = false;
    bool readOnly = false;
};

struct EmbeddedAttachment {
    std::string filenameUtf8;
    std::string mimeTypeUtf8;
    std::string assetKey;
    std::size_t sizeBytes = 0;
};

struct PageModel {
    std::uint64_t pageId = 0;
    double widthPt = 595.0;  // A4 width (pt)
    double heightPt = 842.0; // A4 height (pt)
    int rotationDeg = 0;     // 0, 90, 180, 270
    NormalizedCropRect crop{};
    PaperPattern background = PaperPattern::Plain;

    // If sourced from an existing PDF file (plus portable asset key inside .cudoc)
    bool hasPdfSource = false;
    std::string sourcePdfPathUtf8;
    std::string sourcePdfAssetKey;
    int sourcePageIndex0 = 0;

    // Optional cached OCR or extracted plain text
    std::string extractedTextUtf8;
    std::string ocrTextUtf8;

    // Semantic PDF metadata (clickable links, AcroForm fields, selectable text runs)
    std::vector<PdfLinkRect> pdfLinks;
    std::vector<PdfFormField> formFields;
    std::vector<PdfTextRunBox> textBoxes;

    std::vector<AnnotationObject> objects;
    std::uint64_t activeLayerId = 1;

    double EffectiveWidthPt() const noexcept {
        const int r = ((rotationDeg % 360) + 360) % 360;
        const double baseW = (r == 90 || r == 270) ? heightPt : widthPt;
        return baseW * (std::max)(0.05, crop.right - crop.left);
    }

    double EffectiveHeightPt() const noexcept {
        const int r = ((rotationDeg % 360) + 360) % 360;
        const double baseH = (r == 90 || r == 270) ? widthPt : heightPt;
        return baseH * (std::max)(0.05, crop.bottom - crop.top);
    }
};

struct LayerModel {
    std::uint64_t id = 1;
    std::string nameUtf8 = "Capa Principal";
    bool visible = true;
    bool locked = false;
    bool includeInExport = true;
    double opacity = 1.0;
};

struct InkLink {
    std::uint64_t id = 0;
    std::string sourceDocPathUtf8;
    int sourcePageIndex0 = 0;
    NormalizedCropRect sourceRectNorm{};
    bool targetIsCanvas = true;
    int targetPageIndex0 = 0;
    std::uint64_t targetObjectId = 0;
    std::string labelUtf8;
};

struct CollapsedPageRange {
    int startPage0 = 0;
    int endPage0 = 0;
    bool collapsed = true;
};

struct LaserPointerState {
    bool active = false;
    double worldX = 0.0;
    double worldY = 0.0;
    std::vector<DocPoint> trail;
};

enum class RulerMode : std::uint8_t {
    Straightedge = 0,
    Protractor = 1,
    Compass = 2
};

struct RulerState {
    bool visible = false;
    RulerMode mode = RulerMode::Straightedge;
    double centerX = 320.0;
    double centerY = 260.0;
    double angleDeg = 0.0; // 0..360 graduated protractor/ruler
    double lengthPt = 360.0;
    double compassRadiusPt = 110.0;
};

struct DocumentBookmark {
    std::uint64_t id = 0;
    std::string titleUtf8;
    std::size_t pageIndex = 0;
    double worldY = 0.0;
    int level = 0; // hierarchical outline depth (0, 1, 2)
};

struct SearchMatch {
    std::size_t pageIndex = 0;
    std::size_t charOffset = 0;
    std::size_t charLength = 0;
    std::string snippetUtf8;
    bool hasBox = false;
    DocRect boxNormalized{}; // [0..1] in page coordinates if layout box matched
    bool onCanvas = false;      // match located on the Infinite Canvas
    std::uint64_t objectId = 0; // object containing the match (0 = page text layer)
    DocRect worldBox{};         // match box in canvas world coordinates when onCanvas
};

} // namespace DocStudio
