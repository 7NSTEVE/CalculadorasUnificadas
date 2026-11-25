#pragma once

#include "command_stack.hpp"
#include "doc_types.hpp"
#include "document_model.hpp"
#include "ink_engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

// ---------------- Handwritten Word Clustering & Continuous Document Reflow ----------------

struct HandwrittenWordCluster {
    std::vector<std::uint64_t> strokeIds;
    DocRect bounds{};
    std::string recognizedWordUtf8;
    DocColor dominantColor = DocColor::RGBA(24, 30, 42, 255);
    double averageStrokeWidth = 2.4;
};

struct SpellingSuggestion {
    std::size_t clusterIndex = 0;
    std::string misspelledWordUtf8;
    std::string correctedWordUtf8;
    DocRect wordBounds{};
};

// ---------------- Zoom Writing Box with Auto-Advance (GN-INK-024 / NT-INK-019 / NS-INK-031) ----------------

struct ZoomWritingBoxState {
    bool active = false;
    double zoomFactor = 2.5;
    DocRect pageTargetRect{ 48.0, 72.0, 180.0, 52.0 };
    double leftMarginPt = 48.0;
    double rightMarginPt = 547.0;
    double lineHeightPt = 36.0;
    double autoAdvanceTriggerRatio = 0.78; // entering rightmost 22% triggers horizontal advance
    std::size_t autoAdvanceCount = 0;
    std::size_t lineWrapCount = 0;
};

// ---------------- Customizable Ink Palettes & Stylus Gesture State ----------------

struct InkPalettePreset {
    std::string nameUtf8;
    std::vector<DocColor> colors;
    std::vector<double> strokeWidthsPt;
};

struct FloatingInkToolbarState {
    bool visible = true;
    bool horizontal = true;
    bool focusModeHidden = false;
    DocPoint screenPos{ 240.0, 56.0 };
    InkPenStyle activePen = InkPenStyle::Ballpoint;
     InkPenStyle previousPen = InkPenStyle::Ballpoint;
    bool eraserActive = false;
    double activeWidthPt = 2.4;
    DocColor activeColor = DocColor::RGBA(24, 30, 42, 255);
};

// ---------------- SmartScriptReflowEngine ----------------

class SmartScriptReflowEngine {
public:
    // Groups InkStroke objects into word clusters sorted in reading order (top-to-bottom lines, left-to-right words)
    static std::vector<HandwrittenWordCluster> ClusterHandwrittenWords(
        const DocumentModel& doc,
        ObjectTarget target,
        double wordGapThresholdPt = 18.0,
        double lineGapThresholdPt = 26.0);

    // Responsive handwriting word reflow: wraps word stroke clusters cleanly to fit within [leftMarginPt, leftMarginPt + maxLineWidthPt]
    static std::size_t ReflowHandwrittenWordsToWidth(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        double leftMarginPt,
        double topMarginPt,
        double maxLineWidthPt,
        double lineHeightPt = 44.0,
        double wordSpacingPt = 14.0);

    // Scales and translates a selection of InkStroke / Shape objects uniformly around their collective bounding box
    static bool ScaleAndTransformInkSelection(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        const std::vector<std::uint64_t>& objectIds,
        double scaleX,
        double scaleY,
        double translateDx = 0.0,
        double translateDy = 0.0);

    // Synthesizes vector handwriting strokes (InkStroke) from typed/pasted UTF-8 text (AP-INK-023)
    static std::vector<AnnotationObject> SynthesizeHandwritingStrokesFromText(
        const std::string& textUtf8,
        double startX,
        double startY,
        double charHeightPt = 22.0,
        DocColor inkColor = DocColor::RGBA(24, 30, 42, 255),
        double strokeWidth = 2.2,
        double slantRatio = 0.14);

    // Pastes typed text directly into DocumentModel as realistic vector handwriting strokes
    static std::vector<std::uint64_t> PasteTextAsHandwriting(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        const std::string& textUtf8,
        double startX,
        double startY);

    // Checks spelling of recognized handwritten word clusters and optionally replaces misspelled strokes in-place (GN-INK-021 / MS-INK-021)
    static std::vector<SpellingSuggestion> CheckHandwritingSpelling(
        const DocumentModel& doc,
        ObjectTarget target);

    static bool ApplyHandwritingSpellingCorrection(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        const SpellingSuggestion& suggestion);

    // Double-tap document conversion: converts handwritten word clusters -> TextBoxes and sketched loops -> geometric Shapes (MS-INK-020)
    static std::size_t ConvertHandwritingAndDiagramsOnDoubleTap(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target);

    // Vertical stylus gesture between words/lines: downward stroke inserts space, upward stroke joins gap (MS-INK-018)
    static bool ApplyWordJoinOrSplitSpaceGesture(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        const std::vector<InkPoint>& gestureStroke);

    // Zoom Writing Box auto-advance controller (GN-INK-024 / NT-INK-019 / NS-INK-031)
    static bool UpdateZoomWritingBoxOnStroke(
        ZoomWritingBoxState& boxState,
        const std::vector<InkPoint>& latestPageStrokePoints);

    // Stylus barrel double-tap toggle between active pen and eraser (NF-INK-024 / AP-INK-026)
    static void HandleStylusDoubleTapToggle(FloatingInkToolbarState& toolbar) noexcept;
};

} // namespace DocStudio
