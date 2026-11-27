#include "ink_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace DocStudio::InkEngine {

namespace {

constexpr double kPi = 3.14159265358979323846;

double Dist2(double x0, double y0, double x1, double y1) noexcept {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    return dx * dx + dy * dy;
}

double Dist(double x0, double y0, double x1, double y1) noexcept {
    return std::sqrt(Dist2(x0, y0, x1, y1));
}

double PolylineLength(const std::vector<InkPoint>& pts) noexcept {
    double total = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        total += Dist(pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y);
    }
    return total;
}

DocRect ComputePointsBounds(const std::vector<InkPoint>& pts) noexcept {
    if (pts.empty()) return {};
    double minX = pts[0].x, maxX = pts[0].x;
    double minY = pts[0].y, maxY = pts[0].y;
    for (const auto& p : pts) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }
    return { minX, minY, maxX - minX, maxY - minY };
}

double DistPointToLineSegment(double px, double py, double x0, double y0, double x1, double y1) noexcept {
    const double vx = x1 - x0;
    const double vy = y1 - y0;
    const double wx = px - x0;
    const double wy = py - y0;
    const double len2 = vx * vx + vy * vy;
    if (len2 <= 1e-12) return Dist(px, py, x0, y0);
    double t = (wx * vx + wy * vy) / len2;
    t = std::clamp(t, 0.0, 1.0);
    return Dist(px, py, x0 + t * vx, y0 + t * vy);
}

bool SegmentsIntersect(double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy) noexcept {
    auto orient = [](double px, double py, double qx, double qy, double rx, double ry) {
        return (qx - px) * (ry - py) - (qy - py) * (rx - px);
    };
    const double o1 = orient(ax, ay, bx, by, cx, cy);
    const double o2 = orient(ax, ay, bx, by, dx, dy);
    const double o3 = orient(cx, cy, dx, dy, ax, ay);
    const double o4 = orient(cx, cy, dx, dy, bx, by);
    if (((o1 > 0.0 && o2 < 0.0) || (o1 < 0.0 && o2 > 0.0)) &&
        ((o3 > 0.0 && o4 < 0.0) || (o3 < 0.0 && o4 > 0.0))) {
        return true;
    }
    auto onSeg = [](double px, double py, double qx, double qy, double rx, double ry) {
        return qx >= (std::min)(px, rx) - 1e-6 && qx <= (std::max)(px, rx) + 1e-6 &&
               qy >= (std::min)(py, ry) - 1e-6 && qy <= (std::max)(py, ry) + 1e-6;
    };
    if (std::abs(o1) < 1e-7 && onSeg(ax, ay, cx, cy, bx, by)) return true;
    if (std::abs(o2) < 1e-7 && onSeg(ax, ay, dx, dy, bx, by)) return true;
    if (std::abs(o3) < 1e-7 && onSeg(cx, cy, ax, ay, dx, dy)) return true;
    if (std::abs(o4) < 1e-7 && onSeg(cx, cy, bx, by, dx, dy)) return true;
    return false;
}

int CountDirectionReversals(const std::vector<InkPoint>& pts) {
    std::vector<DocPoint> filtered;
    filtered.reserve(pts.size());
    for (const auto& p : pts) {
        if (filtered.empty() || Dist(filtered.back().x, filtered.back().y, p.x, p.y) >= 3.5) {
            filtered.push_back({ p.x, p.y });
        }
    }
    if (filtered.size() < 4) return 0;

    int reversals = 0;
    double prevDx = filtered[1].x - filtered[0].x;
    double prevDy = filtered[1].y - filtered[0].y;
    double prevLen = std::hypot(prevDx, prevDy);
    for (std::size_t i = 2; i < filtered.size(); ++i) {
        const double dx = filtered[i].x - filtered[i - 1].x;
        const double dy = filtered[i].y - filtered[i - 1].y;
        const double len = std::hypot(dx, dy);
        if (len < 1e-6 || prevLen < 1e-6) continue;
        const double cosTheta = (prevDx * dx + prevDy * dy) / (prevLen * len);
        if (cosTheta < -0.35) { // turn angle > ~110 degrees
            ++reversals;
            prevDx = dx;
            prevDy = dy;
            prevLen = len;
        } else if (cosTheta > 0.25) {
            // keep progressing reference direction smoothly
            prevDx = dx;
            prevDy = dy;
            prevLen = len;
        }
    }
    return reversals;
}

const std::vector<AnnotationObject>* GetTargetObjects(const DocumentModel& doc, ObjectTarget target) {
    if (target.isCanvas) return &doc.CanvasObjects();
    if (target.pageIndex < doc.Pages().size()) return &doc.Pages()[target.pageIndex].objects;
    return nullptr;
}

} // namespace

std::vector<InkPoint> SmoothInkStroke(const std::vector<InkPoint>& raw, double stabilization) {
    if (raw.size() <= 2) return raw;
    const double stab = std::clamp(stabilization, 0.0, 1.0);
    if (stab < 0.02) return raw;

    std::vector<InkPoint> work = raw;
    const int passes = (stab < 0.30) ? 1 : (stab < 0.65 ? 2 : 3);
    const double alpha = (stab < 0.30)
        ? (stab / 0.30) * 0.42
        : (0.45 + 0.40 * ((stab - 0.30) / 0.70));

    for (int pass = 0; pass < passes; ++pass) {
        std::vector<InkPoint> next = work;
        for (std::size_t i = 1; i + 1 < work.size(); ++i) {
            const double avgX = 0.25 * work[i - 1].x + 0.50 * work[i].x + 0.25 * work[i + 1].x;
            const double avgY = 0.25 * work[i - 1].y + 0.50 * work[i].y + 0.25 * work[i + 1].y;
            const float avgP = 0.25f * work[i - 1].pressure + 0.50f * work[i].pressure + 0.25f * work[i + 1].pressure;
            next[i].x = work[i].x * (1.0 - alpha) + avgX * alpha;
            next[i].y = work[i].y * (1.0 - alpha) + avgY * alpha;
            next[i].pressure = static_cast<float>(work[i].pressure * (1.0 - alpha * 0.5) + avgP * (alpha * 0.5));
        }
        work.swap(next);
    }

    if (stab < 0.30 || work.size() < 4) {
        return work;
    }

    // Catmull-Rom spline evaluation for silk-smooth curves at >=0.30 stabilization
    std::vector<InkPoint> res;
    res.reserve((work.size() - 1) * 3 + 1);
    res.push_back(work.front());

    const int subDiv = (stab >= 0.65) ? 4 : 2;
    for (std::size_t i = 0; i + 1 < work.size(); ++i) {
        const InkPoint& p0 = (i == 0) ? work[0] : work[i - 1];
        const InkPoint& p1 = work[i];
        const InkPoint& p2 = work[i + 1];
        const InkPoint& p3 = (i + 2 < work.size()) ? work[i + 2] : work.back();

        for (int s = 1; s <= subDiv; ++s) {
            const double t = static_cast<double>(s) / static_cast<double>(subDiv);
            const double t2 = t * t;
            const double t3 = t2 * t;

            auto catmull = [&](double v0, double v1, double v2, double v3) {
                return 0.5 * ((2.0 * v1) +
                              (-v0 + v2) * t +
                              (2.0 * v0 - 5.0 * v1 + 4.0 * v2 - v3) * t2 +
                              (-v0 + 3.0 * v1 - 3.0 * v2 + v3) * t3);
            };
            InkPoint pt;
            pt.x = catmull(p0.x, p1.x, p2.x, p3.x);
            pt.y = catmull(p0.y, p1.y, p2.y, p3.y);
            pt.pressure = static_cast<float>(p1.pressure + (p2.pressure - p1.pressure) * t);
            pt.tiltXDeg = static_cast<float>(p1.tiltXDeg + (p2.tiltXDeg - p1.tiltXDeg) * t);
            pt.tiltYDeg = static_cast<float>(p1.tiltYDeg + (p2.tiltYDeg - p1.tiltYDeg) * t);
            pt.timestampMs = static_cast<std::uint64_t>(
                static_cast<double>(p1.timestampMs) +
                (static_cast<double>(p2.timestampMs) - static_cast<double>(p1.timestampMs)) * t);
            res.push_back(pt);
        }
    }
    return res;
}

double ComputeEffectiveWidth(const InkPoint& pt, const AnnotationObject& obj, double speedWorldPerSec) {
    if (obj.isHighlighter || obj.penStyle == InkPenStyle::Highlighter) {
        return (std::max)(4.0, obj.strokeWidth);
    }
    const double p = std::clamp(static_cast<double>(pt.pressure), 0.05, 1.0);
    const double tiltMag = std::clamp(std::hypot(static_cast<double>(pt.tiltXDeg), static_cast<double>(pt.tiltYDeg)) / 65.0, 0.0, 1.2);
    const double speedNorm = std::clamp(speedWorldPerSec / 1800.0, 0.0, 1.0);

    switch (obj.penStyle) {
    case InkPenStyle::Fountain:
        return (std::max)(0.6, obj.strokeWidth * (0.35 + 1.15 * p) * (1.0 - 0.22 * speedNorm) * (1.0 + 0.25 * tiltMag));
    case InkPenStyle::Brush:
        return (std::max)(0.7, obj.strokeWidth * (0.22 + 1.85 * p) * (1.0 + 0.30 * tiltMag));
    case InkPenStyle::Pencil:
        return (std::max)(0.7, obj.strokeWidth * (0.55 + 0.65 * p) * (1.0 + 0.85 * tiltMag));
    case InkPenStyle::Ballpoint:
    default:
        return (std::max)(0.6, obj.strokeWidth * (0.50 + 0.90 * p));
    }
}

double ComputeTiltAlphaMultiplier(const InkPoint& pt, InkPenStyle style) {
    if (style != InkPenStyle::Pencil) return 1.0;
    const double tiltMag = std::clamp(std::hypot(static_cast<double>(pt.tiltXDeg), static_cast<double>(pt.tiltYDeg)) / 60.0, 0.0, 1.0);
    const double p = std::clamp(static_cast<double>(pt.pressure), 0.1, 1.0);
    return std::clamp(0.45 + 0.55 * p - 0.28 * tiltMag, 0.22, 0.95);
}

SnappedShapeResult RecognizeHeldShape(const std::vector<InkPoint>& raw) {
    SnappedShapeResult res{};
    if (raw.size() < 4) return res;

    const DocRect bb = ComputePointsBounds(raw);
    const double diag = std::hypot(bb.w, bb.h);
    const double len = PolylineLength(raw);
    if (diag < 14.0 || len < 14.0) return res;

    const double gap = Dist(raw.front().x, raw.front().y, raw.back().x, raw.back().y);

    // 1. Line vs Arrow check (unclosed stroke that closely tracks a main chord or ends with an arrow hook)
    if (gap > diag * 0.35) {
        // Test straight line on whole stroke or shaft (first 72% for potential arrow head)
        auto maxDevToChord = [&](std::size_t endIdx, DocPoint& outA, DocPoint& outB) -> double {
            outA = { raw.front().x, raw.front().y };
            outB = { raw[endIdx].x, raw[endIdx].y };
            double maxD = 0.0;
            for (std::size_t i = 0; i <= endIdx; ++i) {
                const double d = DistPointToLineSegment(raw[i].x, raw[i].y, outA.x, outA.y, outB.x, outB.y);
                if (d > maxD) maxD = d;
            }
            return maxD;
        };

        DocPoint a{}, b{};
        const double fullDev = maxDevToChord(raw.size() - 1, a, b);
        const double chordLen = Dist(a.x, a.y, b.x, b.y);
        if (chordLen >= 16.0 && fullDev / chordLen < 0.095 && len < chordLen * 1.35) {
            res.recognized = true;
            res.kind = ShapeKind::Line;
            res.start = a;
            res.end = b;
            res.bounds = bb;
            res.label = "Linea";
            return res;
        }

        // Check for Arrow (straight shaft over first ~68%..82% + barb hook at end)
        for (double splitFrac : { 0.72, 0.78, 0.65 }) {
            const std::size_t shaftEnd = (std::min)(raw.size() - 2,
                (std::max<std::size_t>)(2u, static_cast<std::size_t>(std::llround(raw.size() * splitFrac))));
            DocPoint s0{}, s1{};
            const double shaftDev = maxDevToChord(shaftEnd, s0, s1);
            const double shaftLen = Dist(s0.x, s0.y, s1.x, s1.y);
            if (shaftLen >= 20.0 && shaftDev / shaftLen < 0.11) {
                // Find tip point furthest along shaft direction in tail
                const double ux = (s1.x - s0.x) / shaftLen;
                const double uy = (s1.y - s0.y) / shaftLen;
                double maxProj = -1e18;
                DocPoint tip = s1;
                double maxTailPerp = 0.0;
                for (std::size_t i = shaftEnd; i < raw.size(); ++i) {
                    const double dx = raw[i].x - s0.x;
                    const double dy = raw[i].y - s0.y;
                    const double proj = dx * ux + dy * uy;
                    if (proj > maxProj) {
                        maxProj = proj;
                        tip = { raw[i].x, raw[i].y };
                    }
                    const double perp = std::abs(-dx * uy + dy * ux);
                    if (perp > maxTailPerp) maxTailPerp = perp;
                }
                if (maxTailPerp >= 4.5 && maxTailPerp <= shaftLen * 0.55) {
                    res.recognized = true;
                    res.kind = ShapeKind::Arrow;
                    res.start = s0;
                    res.end = tip;
                    res.bounds = bb;
                    res.label = "Flecha";
                    return res;
                }
            }
        }
        return res;
    }

    // 2. Closed contour: Ellipse / Circle vs Rectangle
    if (bb.w < 12.0 || bb.h < 12.0) return res;
    const double cx = bb.x + bb.w * 0.5;
    const double cy = bb.y + bb.h * 0.5;
    const double rx = bb.w * 0.5;
    const double ry = bb.h * 0.5;

    double ellipseErrSum = 0.0;
    double rectErrSum = 0.0;
    for (const auto& pt : raw) {
        const double nx = (pt.x - cx) / rx;
        const double ny = (pt.y - cy) / ry;
        const double ellR = std::hypot(nx, ny);
        ellipseErrSum += std::abs(ellR - 1.0);

        const double edgeDistX = (std::min)(std::abs(pt.x - bb.x), std::abs(pt.x - bb.Right())) / (std::max)(1.0, rx);
        const double edgeDistY = (std::abs(pt.y - bb.y) < std::abs(pt.y - bb.Bottom())
            ? std::abs(pt.y - bb.y)
            : std::abs(pt.y - bb.Bottom())) / (std::max)(1.0, ry);
        rectErrSum += (std::min)(edgeDistX, edgeDistY);
    }
    const double meanEllipseErr = ellipseErrSum / static_cast<double>(raw.size());
    const double meanRectErr = rectErrSum / static_cast<double>(raw.size());

    if (meanEllipseErr < 0.22 && meanEllipseErr <= meanRectErr) {
        res.recognized = true;
        res.kind = ShapeKind::Ellipse;
        res.bounds = bb;
        res.label = (std::abs(bb.w - bb.h) < (std::max)(bb.w, bb.h) * 0.14) ? "C\xC3\xADrculo" : "Elipse";
        return res;
    }
    if (meanRectErr < 0.24) {
        res.recognized = true;
        res.kind = ShapeKind::Rectangle;
        res.bounds = bb;
        res.label = "Rect\xC3\xA1ngulo";
        return res;
    }
    return res;
}

bool IsScribbleEraseGesture(const std::vector<InkPoint>& raw) {
    if (raw.size() < 8) return false;
    const DocRect bb = ComputePointsBounds(raw);
    const double diag = std::hypot(bb.w, bb.h);
    const double len = PolylineLength(raw);
    if (diag < 10.0 || len < diag * 2.5) return false;
    const int reversals = CountDirectionReversals(raw);
    return reversals >= 4;
}

bool IsClosedLassoLoop(const std::vector<InkPoint>& raw) {
    if (raw.size() < 8) return false;
    if (IsScribbleEraseGesture(raw)) return false;
    const DocRect bb = ComputePointsBounds(raw);
    const double diag = std::hypot(bb.w, bb.h);
    const double len = PolylineLength(raw);
    if (diag < 16.0 || len < diag * 2.0) return false;
    const double gap = Dist(raw.front().x, raw.front().y, raw.back().x, raw.back().y);
    return gap <= diag * 0.45;
}

bool PointInPolygon(double x, double y, const std::vector<InkPoint>& poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    std::size_t j = poly.size() - 1;
    for (std::size_t i = 0; i < poly.size(); j = i++) {
        const double xi = poly[i].x, yi = poly[i].y;
        const double xj = poly[j].x, yj = poly[j].y;
        const bool crossY = (yi > y) != (yj > y);
        if (crossY) {
            const double xIntersect = (xj - xi) * (y - yi) / ((yj - yi) + 1e-18) + xi;
            if (x < xIntersect) inside = !inside;
        }
    }
    return inside;
}

bool ObjectIntersectsTrace(const AnnotationObject& obj, const std::vector<InkPoint>& trace, bool closedPolygon) {
    if (trace.empty()) return false;
    const DocRect traceBb = ComputePointsBounds(trace);
    if (!obj.bounds.Intersects(traceBb) && !closedPolygon) return false;

    if (closedPolygon) {
        // Enclosed center OR any vertex/point inside lasso OR bounding box border intersects trace
        const DocPoint center{ obj.bounds.x + obj.bounds.w * 0.5, obj.bounds.y + obj.bounds.h * 0.5 };
        if (PointInPolygon(center.x, center.y, trace)) return true;
        if (obj.kind == ObjectKind::InkStroke) {
            for (const auto& pt : obj.inkPoints) {
                if (PointInPolygon(pt.x, pt.y, trace)) return true;
            }
        } else {
            const DocPoint corners[4] = {
                { obj.bounds.x, obj.bounds.y },
                { obj.bounds.Right(), obj.bounds.y },
                { obj.bounds.x, obj.bounds.Bottom() },
                { obj.bounds.Right(), obj.bounds.Bottom() }
            };
            for (const auto& c : corners) {
                if (PointInPolygon(c.x, c.y, trace)) return true;
            }
        }
        return false;
    }

    // Scribble polyline intersection test
    if (obj.kind == ObjectKind::InkStroke && obj.inkPoints.size() >= 2) {
        for (std::size_t i = 1; i < obj.inkPoints.size(); ++i) {
            const auto& a = obj.inkPoints[i - 1];
            const auto& b = obj.inkPoints[i];
            for (std::size_t j = 1; j < trace.size(); ++j) {
                if (SegmentsIntersect(a.x, a.y, b.x, b.y, trace[j - 1].x, trace[j - 1].y, trace[j].x, trace[j].y)) {
                    return true;
                }
            }
        }
        for (const auto& tp : trace) {
            for (const auto& sp : obj.inkPoints) {
                if (Dist(tp.x, tp.y, sp.x, sp.y) <= (std::max)(8.0, obj.strokeWidth + 4.0)) {
                    return true;
                }
            }
        }
        return false;
    }

    for (const auto& tp : trace) {
        if (obj.bounds.Contains(tp.x, tp.y)) return true;
    }
    return false;
}

std::vector<std::uint64_t> FindObjectsForScribbleErase(
    const DocumentModel& doc,
    ObjectTarget target,
    const std::vector<InkPoint>& scribble,
    bool skipLockedOrHiddenLayers,
    bool inkStrokesOnly)
{
    std::vector<std::uint64_t> ids;
    const auto* objs = GetTargetObjects(doc, target);
    if (!objs) return ids;
    for (const auto& obj : *objs) {
        if (inkStrokesOnly) {
            const bool inkLike = (obj.kind == ObjectKind::InkStroke) ||
                                 (obj.kind == ObjectKind::Shape && !obj.isRedaction);
            if (!inkLike) continue;
        }
        if (skipLockedOrHiddenLayers) {
            if (obj.lockedInPlace || !doc.IsLayerVisible(obj.layerId) || doc.IsLayerLocked(obj.layerId)) continue;
        }
        if (ObjectIntersectsTrace(obj, scribble, false)) {
            ids.push_back(obj.id);
        }
    }
    return ids;
}

std::vector<std::uint64_t> SelectObjectsInsideLasso(
    const DocumentModel& doc,
    ObjectTarget target,
    const std::vector<InkPoint>& lassoLoop,
    bool skipLockedOrHiddenLayers)
{
    std::vector<std::uint64_t> ids;
    const auto* objs = GetTargetObjects(doc, target);
    if (!objs) return ids;
    for (const auto& obj : *objs) {
        if (skipLockedOrHiddenLayers) {
            if (obj.lockedInPlace || !doc.IsLayerVisible(obj.layerId) || doc.IsLayerLocked(obj.layerId)) continue;
        }
        if (ObjectIntersectsTrace(obj, lassoLoop, true)) {
            ids.push_back(obj.id);
        }
    }
    return ids;
}

void UpdateAnchoredChildrenAfterMove(
    DocumentModel& doc,
    ObjectTarget target,
    std::uint64_t parentId,
    double dx,
    double dy)
{
    if (parentId == 0 || (std::abs(dx) < 1e-9 && std::abs(dy) < 1e-9)) return;

    auto translateObj = [&](AnnotationObject& child) {
        if (child.anchorObjectId != parentId) return;
        for (auto& pt : child.inkPoints) {
            pt.x += dx;
            pt.y += dy;
        }
        child.shapeStart.x += dx;
        child.shapeStart.y += dy;
        child.shapeEnd.x += dx;
        child.shapeEnd.y += dy;
        child.bounds.x += dx;
        child.bounds.y += dy;
        child.RecomputeBounds();
    };

    if (target.isCanvas) {
        std::vector<std::uint64_t> childIds;
        for (const auto& o : doc.CanvasObjects()) {
            if (o.anchorObjectId == parentId) childIds.push_back(o.id);
        }
        for (std::uint64_t cid : childIds) {
            if (AnnotationObject* c = doc.FindCanvasObjectMutable(cid)) {
                translateObj(*c);
            }
        }
        doc.RebuildCanvasIndex();
    } else if (target.pageIndex < doc.Pages().size()) {
        for (auto& o : doc.MutablePages()[target.pageIndex].objects) {
            translateObj(o);
        }
    }
    doc.SetDirty(true);
    doc.BumpRevision();
}

std::pair<std::uint64_t, std::uint64_t> ComputeReplayTimelineRange(
    const DocumentModel& doc,
    ObjectTarget target)
{
    const auto* objs = GetTargetObjects(doc, target);
    if (!objs) return { 0ull, 0ull };

    bool any = false;
    std::uint64_t minT = (std::numeric_limits<std::uint64_t>::max)();
    std::uint64_t maxT = 0;

    for (const auto& obj : *objs) {
        if (obj.inkStartTimeMs > 0 || obj.inkEndTimeMs > 0) {
            any = true;
            minT = (std::min)(minT, obj.inkStartTimeMs);
            maxT = (std::max)(maxT, obj.inkEndTimeMs);
        }
        for (const auto& pt : obj.inkPoints) {
            if (pt.timestampMs > 0) {
                any = true;
                minT = (std::min)(minT, pt.timestampMs);
                maxT = (std::max)(maxT, pt.timestampMs);
            }
        }
    }
    if (!any) return { 0ull, 0ull };
    return { minT, maxT };
}

std::vector<InkPoint> FilterInkPointsAtReplayTime(
    const AnnotationObject& obj,
    std::uint64_t replayCursorMs)
{
    if (obj.inkPoints.empty()) return {};
    bool hasTimestamps = false;
    for (const auto& pt : obj.inkPoints) {
        if (pt.timestampMs > 0) {
            hasTimestamps = true;
            break;
        }
    }
    if (!hasTimestamps) {
        if (obj.inkStartTimeMs > replayCursorMs) return {};
        return obj.inkPoints;
    }

    std::vector<InkPoint> out;
    out.reserve(obj.inkPoints.size());
    for (const auto& pt : obj.inkPoints) {
        if (pt.timestampMs <= replayCursorMs) {
            out.push_back(pt);
        }
    }
    return out;
}

bool PredictNextInkPoint(
    const std::vector<InkPoint>& pts,
    InkPoint& outPredicted,
    double lookaheadMs)
{
    if (pts.size() < 2 || lookaheadMs <= 0.0) return false;
    const InkPoint& p1 = pts.back();
    const InkPoint& p0 = pts[pts.size() - 2];
    double dtMs = 8.0;
    if (p1.timestampMs > p0.timestampMs) {
        dtMs = std::clamp(static_cast<double>(p1.timestampMs - p0.timestampMs), 1.0, 50.0);
    }
    const double vx = (p1.x - p0.x) / dtMs;
    const double vy = (p1.y - p0.y) / dtMs;
    const double clampedLook = std::clamp(lookaheadMs, 1.0, 20.0);
    // Damping factor 0.65 prevents overshoot on sharp corners
    const double dx = std::clamp(vx * clampedLook * 0.65, -24.0, 24.0);
    const double dy = std::clamp(vy * clampedLook * 0.65, -24.0, 24.0);
    if (std::hypot(dx, dy) < 0.35) return false;

    outPredicted = p1;
    outPredicted.x = p1.x + dx;
    outPredicted.y = p1.y + dy;
    outPredicted.timestampMs = p1.timestampMs + static_cast<std::uint64_t>(std::lround(clampedLook));
    return true;
}

namespace {
bool MatchesEraserFilter(const AnnotationObject& obj, EraserFilterMode mode) noexcept {
    switch (mode) {
    case EraserFilterMode::All:
        return true;
    case EraserFilterMode::HighlighterOnly:
        return obj.kind == ObjectKind::InkStroke && obj.isHighlighter;
    case EraserFilterMode::InkOnly:
        return obj.kind == ObjectKind::InkStroke && !obj.isHighlighter;
    case EraserFilterMode::ShapesOnly:
        return obj.kind == ObjectKind::Shape;
    }
    return true;
}
} // namespace

std::vector<std::uint64_t> FilterHitsByEraserMode(
    const DocumentModel& doc,
    ObjectTarget target,
    const std::vector<std::uint64_t>& rawHitIds,
    EraserFilterMode mode,
    bool skipLockedOrHiddenLayers)
{
    std::vector<std::uint64_t> out;
    for (std::uint64_t id : rawHitIds) {
        const AnnotationObject* obj = nullptr;
        if (target.isCanvas) {
            obj = doc.FindCanvasObject(id);
        } else if (target.pageIndex < doc.Pages().size()) {
            for (const auto& o : doc.Pages()[target.pageIndex].objects) {
                if (o.id == id) { obj = &o; break; }
            }
        }
        if (!obj) continue;
        if (skipLockedOrHiddenLayers && (!doc.IsLayerVisible(obj->layerId) || doc.IsLayerLocked(obj->layerId))) {
            continue;
        }
        if (MatchesEraserFilter(*obj, mode)) {
            out.push_back(id);
        }
    }
    return out;
}

std::vector<std::uint64_t> CollectObjectsMatchingFilter(
    const DocumentModel& doc,
    ObjectTarget target,
    EraserFilterMode mode,
    bool skipLockedOrHiddenLayers)
{
    std::vector<std::uint64_t> out;
    const auto* objs = GetTargetObjects(doc, target);
    if (!objs) return out;
    for (const auto& obj : *objs) {
        if (skipLockedOrHiddenLayers && (!doc.IsLayerVisible(obj.layerId) || doc.IsLayerLocked(obj.layerId))) {
            continue;
        }
        if (MatchesEraserFilter(obj, mode)) {
            out.push_back(obj.id);
        }
    }
    return out;
}

DocPoint ProjectPointOntoRuler(
    DocPoint pt,
    const RulerState& ruler,
    double snapDistWorld,
    bool* outSnapped)
{
    if (outSnapped) *outSnapped = false;
    if (!ruler.visible) return pt;

    const double rad = ruler.angleDeg * (kPi / 180.0);
    const double ux = std::cos(rad);
    const double uy = std::sin(rad);

    if (ruler.mode == RulerMode::Protractor) {
        const double radius = (std::max)(40.0, ruler.lengthPt * 0.42);
        const double dx = pt.x - ruler.centerX;
        const double dy = pt.y - ruler.centerY;
        const double dist = std::hypot(dx, dy);
        // Check base straight edge of protractor
        const double along = dx * ux + dy * uy;
        const double perp = -dx * uy + dy * ux;
        if (std::abs(along) <= radius + snapDistWorld && std::abs(perp) <= snapDistWorld * 0.85) {
            if (outSnapped) *outSnapped = true;
            const double ca = std::clamp(along, -radius, radius);
            return { ruler.centerX + ca * ux, ruler.centerY + ca * uy };
        }
        // Check semicircular arc (upper half relative to ruler angle)
        if (std::abs(dist - radius) <= snapDistWorld && perp <= snapDistWorld) {
            const double ang = std::atan2(dy, dx);
            if (outSnapped) *outSnapped = true;
            return { ruler.centerX + radius * std::cos(ang), ruler.centerY + radius * std::sin(ang) };
        }
        return pt;
    }

    if (ruler.mode == RulerMode::Compass) {
        const double radius = (std::max)(18.0, ruler.compassRadiusPt);
        const double dx = pt.x - ruler.centerX;
        const double dy = pt.y - ruler.centerY;
        const double dist = std::hypot(dx, dy);
        if (dist > 1e-4 && std::abs(dist - radius) <= snapDistWorld * 1.25) {
            if (outSnapped) *outSnapped = true;
            return { ruler.centerX + (dx / dist) * radius, ruler.centerY + (dy / dist) * radius };
        }
        return pt;
    }

    const double halfL = (std::max)(20.0, ruler.lengthPt * 0.5);
    const double rx = pt.x - ruler.centerX;
    const double ry = pt.y - ruler.centerY;
    const double along = rx * ux + ry * uy;
    const double perp = -rx * uy + ry * ux;

    if (std::abs(along) <= halfL + snapDistWorld && std::abs(perp) <= snapDistWorld) {
        if (outSnapped) *outSnapped = true;
        const double clampedAlong = std::clamp(along, -halfL, halfL);
        return { ruler.centerX + clampedAlong * ux, ruler.centerY + clampedAlong * uy };
    }
    return pt;
}

bool EraseStrokePartial(
    const AnnotationObject& stroke,
    DocPoint eraserCenter,
    double eraserRadiusWorld,
    std::vector<AnnotationObject>& outPieces)
{
    outPieces.clear();
    if (stroke.kind != ObjectKind::InkStroke || stroke.inkPoints.empty()) return false;

    const double reach = (std::max)(2.0, eraserRadiusWorld) + stroke.strokeWidth * 0.5;
    const double reach2 = reach * reach;

    // Fast bbox reject
    if (eraserCenter.x + reach < stroke.bounds.x || eraserCenter.x - reach > stroke.bounds.Right() ||
        eraserCenter.y + reach < stroke.bounds.y || eraserCenter.y - reach > stroke.bounds.Bottom()) {
        return false;
    }

    // Densify long segments slightly before splitting so fast strokes split cleanly along circle intersections
    std::vector<InkPoint> dense;
    dense.reserve(stroke.inkPoints.size() * 2);
    dense.push_back(stroke.inkPoints[0]);
    const double maxSegLen = (std::max)(2.5, reach * 0.45);
    for (std::size_t i = 1; i < stroke.inkPoints.size(); ++i) {
        const auto& a = stroke.inkPoints[i - 1];
        const auto& b = stroke.inkPoints[i];
        const double segLen = Dist(a.x, a.y, b.x, b.y);
        if (segLen > maxSegLen) {
            const int steps = (std::min)(32, static_cast<int>(std::ceil(segLen / maxSegLen)));
            for (int s = 1; s < steps; ++s) {
                const double t = static_cast<double>(s) / static_cast<double>(steps);
                InkPoint mid{};
                mid.x = a.x + (b.x - a.x) * t;
                mid.y = a.y + (b.y - a.y) * t;
                mid.pressure = static_cast<float>(a.pressure + (b.pressure - a.pressure) * t);
                mid.tiltXDeg = static_cast<float>(a.tiltXDeg + (b.tiltXDeg - a.tiltXDeg) * t);
                mid.tiltYDeg = static_cast<float>(a.tiltYDeg + (b.tiltYDeg - a.tiltYDeg) * t);
                mid.timestampMs = static_cast<std::uint64_t>(
                    static_cast<double>(a.timestampMs) +
                    (static_cast<double>(b.timestampMs) - static_cast<double>(a.timestampMs)) * t);
                dense.push_back(mid);
            }
        }
        dense.push_back(b);
    }

    bool anyErased = false;
    std::vector<InkPoint> current;
    auto flushPiece = [&]() {
        if (current.size() >= 2) {
            AnnotationObject piece = stroke;
            piece.id = 0;
            piece.zOrder = 0;
            piece.inkPoints = std::move(current);
            piece.inkStartTimeMs = piece.inkPoints.front().timestampMs;
            piece.inkEndTimeMs = piece.inkPoints.back().timestampMs;
            piece.RecomputeBounds();
            outPieces.push_back(std::move(piece));
            current.clear();
        } else {
            current.clear();
        }
    };

    for (const auto& pt : dense) {
        if (Dist2(pt.x, pt.y, eraserCenter.x, eraserCenter.y) <= reach2) {
            anyErased = true;
            flushPiece();
        } else {
            current.push_back(pt);
        }
    }
    flushPiece();
    return anyErased;
}

bool NeatenAndRestyleStrokes(
    DocumentModel& doc,
    ObjectTarget target,
    const std::vector<std::uint64_t>& ids)
{
    if (ids.empty()) return false;
    std::vector<AnnotationObject*> strokes;
    double sumX = 0.0, sumY = 0.0;
    std::size_t totalPts = 0;

    for (std::uint64_t id : ids) {
        AnnotationObject* obj = target.isCanvas
            ? doc.FindCanvasObjectMutable(id)
            : doc.FindPageObjectMutable(target.pageIndex, id);
        if (!obj || obj->kind != ObjectKind::InkStroke || obj->inkPoints.size() < 2) continue;
        if (!doc.IsLayerVisible(obj->layerId) || doc.IsLayerLocked(obj->layerId)) continue;
        strokes.push_back(obj);
        for (const auto& p : obj->inkPoints) {
            sumX += p.x;
            sumY += p.y;
            ++totalPts;
        }
    }
    if (strokes.empty() || totalPts < 2) return false;

    const double meanX = sumX / static_cast<double>(totalPts);
    const double meanY = sumY / static_cast<double>(totalPts);

    // Estimate line baseline slope via linear regression over the stroke group and deskew gently (<= 12 deg)
    double covXY = 0.0, varX = 0.0;
    for (const auto* s : strokes) {
        for (const auto& p : s->inkPoints) {
            const double dx = p.x - meanX;
            const double dy = p.y - meanY;
            covXY += dx * dy;
            varX += dx * dx;
        }
    }
    double skewAngle = (varX > 1e-3) ? std::atan2(covXY, varX) : 0.0;
    const double maxDeskewRad = 12.0 * (kPi / 180.0);
    if (std::abs(skewAngle) > maxDeskewRad) skewAngle = 0.0; // don't rotate vertical drawings
    const double cosA = std::cos(-skewAngle);
    const double sinA = std::sin(-skewAngle);

    for (auto* s : strokes) {
        for (auto& p : s->inkPoints) {
            const double dx = p.x - meanX;
            const double dy = p.y - meanY;
            p.x = meanX + dx * cosA - dy * sinA;
            p.y = meanY + dx * sinA + dy * cosA;
            // Gently normalize pressure spikes for consistent calligraphy look
            p.pressure = static_cast<float>(0.35 + 0.65 * std::clamp(static_cast<double>(p.pressure), 0.15, 0.95));
        }
        s->inkPoints = SmoothInkStroke(s->inkPoints, 0.55);
        s->RecomputeBounds();
    }
    if (target.isCanvas) doc.RebuildCanvasIndex();
    doc.SetDirty(true);
    doc.BumpRevision();
    return true;
}

DocPoint SnapPointToGrid(
    DocPoint pt,
    PaperPattern pattern,
    double gridSpacingPt)
{
    const double step = (std::max)(4.0, gridSpacingPt);
    switch (pattern) {
    case PaperPattern::Ruled:
    case PaperPattern::Cornell:
    case PaperPattern::MusicStaff:
        return { pt.x, std::round(pt.y / step) * step };
    case PaperPattern::Isometric: {
        const double dy = step * 0.8660254037844386;
        const double row = std::round(pt.y / dy);
        const double offset = (static_cast<long long>(row) % 2 != 0) ? (step * 0.5) : 0.0;
        const double col = std::round((pt.x - offset) / step);
        return { col * step + offset, row * dy };
    }
    case PaperPattern::Grid:
    case PaperPattern::Dots:
    case PaperPattern::LogLog:
    case PaperPattern::Polar:
    case PaperPattern::SmithChart:
    case PaperPattern::Plain:
    default:
        return { std::round(pt.x / step) * step, std::round(pt.y / step) * step };
    }
}

void UpdateMagneticConnectors(
    DocumentModel& doc,
    ObjectTarget target,
    std::uint64_t movedObjectId)
{
    if (movedObjectId == 0) return;

    auto getCenter = [&](std::uint64_t id, DocPoint& outCenter) -> bool {
        const AnnotationObject* o = target.isCanvas
            ? doc.FindCanvasObject(id)
            : (target.pageIndex < doc.Pages().size() ? [&]() -> const AnnotationObject* {
                for (const auto& item : doc.Pages()[target.pageIndex].objects) {
                    if (item.id == id) return &item;
                }
                return nullptr;
            }() : nullptr);
        if (!o) return false;
        outCenter = { o->bounds.x + o->bounds.w * 0.5, o->bounds.y + o->bounds.h * 0.5 };
        return true;
    };

    auto updateVec = [&](std::vector<AnnotationObject>& objs) {
        bool changed = false;
        for (auto& o : objs) {
            if (o.kind != ObjectKind::Shape || o.shapeKind != ShapeKind::Connector) continue;
            if (o.connectorFromId != movedObjectId && o.connectorToId != movedObjectId) continue;
            DocPoint p{};
            if (o.connectorFromId != 0 && getCenter(o.connectorFromId, p)) {
                o.shapeStart = p;
                changed = true;
            }
            if (o.connectorToId != 0 && getCenter(o.connectorToId, p)) {
                o.shapeEnd = p;
                changed = true;
            }
            o.RecomputeBounds();
        }
        return changed;
    };

    if (target.isCanvas) {
        std::vector<std::uint64_t> connIds;
        for (const auto& o : doc.CanvasObjects()) {
            if (o.kind == ObjectKind::Shape && o.shapeKind == ShapeKind::Connector &&
                (o.connectorFromId == movedObjectId || o.connectorToId == movedObjectId)) {
                connIds.push_back(o.id);
            }
        }
        for (std::uint64_t cid : connIds) {
            if (AnnotationObject* c = doc.FindCanvasObjectMutable(cid)) {
                DocPoint p{};
                if (c->connectorFromId != 0 && getCenter(c->connectorFromId, p)) c->shapeStart = p;
                if (c->connectorToId != 0 && getCenter(c->connectorToId, p)) c->shapeEnd = p;
                c->RecomputeBounds();
            }
        }
        if (!connIds.empty()) {
            doc.RebuildCanvasIndex();
            doc.SetDirty(true);
            doc.BumpRevision();
        }
    } else if (target.pageIndex < doc.Pages().size()) {
        if (updateVec(doc.MutablePages()[target.pageIndex].objects)) {
            doc.SetDirty(true);
            doc.BumpRevision();
        }
    }
}

} // namespace DocStudio::InkEngine
