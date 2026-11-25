#pragma once

#include "command_stack.hpp"
#include "doc_types.hpp"
#include "document_model.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace DocStudio::InkEngine {

struct SnappedShapeResult {
    bool recognized = false;
    ShapeKind kind = ShapeKind::Line;
    DocPoint start{};
    DocPoint end{};
    DocRect bounds{};
    std::string label;
};

// Velocity/stabilization-aware smoothing (<0.30 preserves snappy response, >=0.30 applies Catmull-Rom + bilateral damping)
std::vector<InkPoint> SmoothInkStroke(const std::vector<InkPoint>& raw, double stabilization);

// Pen style + pressure + stylus tilt + speed width multiplier (in world pt)
double ComputeEffectiveWidth(const InkPoint& pt, const AnnotationObject& obj, double speedWorldPerSec);

// Pen style tilt-aware alpha factor [0.0..1.0] (pencil tilt lowers density smoothly across broad stroke)
double ComputeTiltAlphaMultiplier(const InkPoint& pt, InkPenStyle style);

// Hold-to-Snap recognizer for lines, directional arrows, rectangles, and ellipses/circles
SnappedShapeResult RecognizeHeldShape(const std::vector<InkPoint>& raw);

// Goodnotes Scribble-to-Erase zig-zag scratch gesture detector
bool IsScribbleEraseGesture(const std::vector<InkPoint>& raw);

// Circle-to-Lasso closed loop detector
bool IsClosedLassoLoop(const std::vector<InkPoint>& raw);

// Ray-casting point-in-polygon helper
bool PointInPolygon(double x, double y, const std::vector<InkPoint>& poly);

// Hit test any annotation object against a scribble polyline or lasso closed polygon
bool ObjectIntersectsTrace(const AnnotationObject& obj, const std::vector<InkPoint>& trace, bool closedPolygon);

// Returns object IDs intersected by scribble gesture (excluding locked/invisible layers when requested)
std::vector<std::uint64_t> FindObjectsForScribbleErase(
    const DocumentModel& doc,
    ObjectTarget target,
    const std::vector<InkPoint>& scribble,
    bool skipLockedOrHiddenLayers = true);

// Returns object IDs enclosed by or touching lasso loop (excluding locked/invisible layers when requested)
std::vector<std::uint64_t> SelectObjectsInsideLasso(
    const DocumentModel& doc,
    ObjectTarget target,
    const std::vector<InkPoint>& lassoLoop,
    bool skipLockedOrHiddenLayers = true);

// Updates anchored child strokes/objects whenever parent object moves by (dx, dy)
void UpdateAnchoredChildrenAfterMove(
    DocumentModel& doc,
    ObjectTarget target,
    std::uint64_t parentId,
    double dx,
    double dy);

// Computes [minTimestampMs, maxTimestampMs] across all ink/timestamped objects on target
std::pair<std::uint64_t, std::uint64_t> ComputeReplayTimelineRange(
    const DocumentModel& doc,
    ObjectTarget target);

// Filters an InkStroke's points up to replayCursorMs for synchronized Ink Replay + Audio playback
std::vector<InkPoint> FilterInkPointsAtReplayTime(
    const AnnotationObject& obj,
    std::uint64_t replayCursorMs);

// Sub-frame kinematic velocity-damped lookahead prediction (<16 ms latency mitigation)
bool PredictNextInkPoint(
    const std::vector<InkPoint>& pts,
    InkPoint& outPredicted,
    double lookaheadMs = 12.0);

// Goodnotes 2026 Selective Eraser & Delete Specific Items filter helpers
std::vector<std::uint64_t> FilterHitsByEraserMode(
    const DocumentModel& doc,
    ObjectTarget target,
    const std::vector<std::uint64_t>& rawHitIds,
    EraserFilterMode mode,
    bool skipLockedOrHiddenLayers = true);

std::vector<std::uint64_t> CollectObjectsMatchingFilter(
    const DocumentModel& doc,
    ObjectTarget target,
    EraserFilterMode mode,
    bool skipLockedOrHiddenLayers = true);

// Graduated Ruler / Protractor / Compass (0..360 deg) edge & arc projection + magnetic snap
DocPoint ProjectPointOntoRuler(
    DocPoint pt,
    const RulerState& ruler,
    double snapDistWorld = 24.0,
    bool* outSnapped = nullptr);

// Goodnotes-style Partial/Pixel Eraser: geometrically splits an InkStroke around an eraser circle
// (returns true if stroke was modified/split; outPieces holds 0, 1, or N remaining sub-strokes)
bool EraseStrokePartial(
    const AnnotationObject& stroke,
    DocPoint eraserCenter,
    double eraserRadiusWorld,
    std::vector<AnnotationObject>& outPieces);

// Handwriting Neatening / Restyle (F-044): normalizes baseline slant, vertical jitter, and smooths strokes
bool NeatenAndRestyleStrokes(
    DocumentModel& doc,
    ObjectTarget target,
    const std::vector<std::uint64_t>& ids);

// Magnetic snap-to-grid (F-040) supporting Grid, DotGrid, Isometric, Lined, Polar
DocPoint SnapPointToGrid(
    DocPoint pt,
    PaperPattern pattern,
    double gridSpacingPt = 22.0);

// Updates all ShapeKind::Connector arrows/lines anchored to movedObjectId (F-110)
void UpdateMagneticConnectors(
    DocumentModel& doc,
    ObjectTarget target,
    std::uint64_t movedObjectId);

} // namespace DocStudio::InkEngine
