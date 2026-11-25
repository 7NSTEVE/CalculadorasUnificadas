#pragma once

#include "circuit_models.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio::Circuit {

enum class SymbolStandard : std::uint8_t {
    IEC = 0,
    ANSI = 1
};

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct Pin {
    std::string pinId;
    Vec2 localOffset{};
    int resolvedNetId = -1;
    std::string netName;
};

inline Vec2 TransformLocalOffset(
    Vec2 localOffset,
    Vec2 center,
    int rotationDeg,
    bool flipH,
    bool flipV) noexcept
{
    double lx = flipH ? -localOffset.x : localOffset.x;
    double ly = flipV ? -localOffset.y : localOffset.y;
    const int rot = ((rotationDeg % 360) + 360) % 360;
    double rx = lx;
    double ry = ly;
    if (rot == 90) {
        rx = -ly;
        ry = lx;
    } else if (rot == 180) {
        rx = -lx;
        ry = -ly;
    } else if (rot == 270) {
        rx = ly;
        ry = -lx;
    }
    return { center.x + rx, center.y + ry };
}

inline std::vector<Pin> ComponentPinTemplate(ComponentKind kind) {
    switch (kind) {
    case ComponentKind::Ground:
        return { { "gnd", { 0.0, -20.0 }, 0, "GND" } };
    case ComponentKind::Vcc:
        return { { "vcc", { 0.0, 20.0 }, -1, "VCC" } };
    case ComponentKind::OpAmp:
    case ComponentKind::Comparator:
        return {
            { "in+", { -40.0, -20.0 }, -1, "" },
            { "in-", { -40.0, 20.0 }, -1, "" },
            { "out", { 40.0, 0.0 }, -1, "" }
        };
    case ComponentKind::BjtNpn:
    case ComponentKind::BjtPnp:
        return {
            { "C", { 20.0, -20.0 }, -1, "" },
            { "E", { 20.0, 20.0 }, -1, "" },
            { "B", { -40.0, 0.0 }, -1, "" }
        };
    case ComponentKind::MosfetN:
    case ComponentKind::MosfetP:
    case ComponentKind::JfetN:
    case ComponentKind::JfetP:
        return {
            { "D", { 20.0, -20.0 }, -1, "" },
            { "S", { 20.0, 20.0 }, -1, "" },
            { "G", { -40.0, 0.0 }, -1, "" }
        };
    case ComponentKind::Scr:
    case ComponentKind::Triac:
        return {
            { "A", { -40.0, 0.0 }, -1, "" },
            { "K", { 40.0, 0.0 }, -1, "" },
            { "G", { 0.0, 20.0 }, -1, "" }
        };
    case ComponentKind::Potentiometer:
        return {
            { "1", { -40.0, 0.0 }, -1, "" },
            { "2", { 40.0, 0.0 }, -1, "" },
            { "wiper", { 0.0, -20.0 }, -1, "" }
        };
    case ComponentKind::VoltageRegulator:
    case ComponentKind::Ic:
        return {
            { "in", { -40.0, 0.0 }, -1, "" },
            { "gnd", { 0.0, 20.0 }, -1, "" },
            { "out", { 40.0, 0.0 }, -1, "" }
        };
    case ComponentKind::Transformer:
        return {
            { "P+", { -40.0, -20.0 }, -1, "" },
            { "P-", { -40.0, 20.0 }, -1, "" },
            { "S+", { 40.0, -20.0 }, -1, "" },
            { "S-", { 40.0, 20.0 }, -1, "" }
        };
    case ComponentKind::Vcvs:
    case ComponentKind::Vccs:
    case ComponentKind::Relay:
        return {
            { "out+", { 40.0, -20.0 }, -1, "" },
            { "out-", { 40.0, 20.0 }, -1, "" },
            { "in+", { -40.0, -20.0 }, -1, "" },
            { "in-", { -40.0, 20.0 }, -1, "" }
        };
    case ComponentKind::GateNot:
        return {
            { "in", { -40.0, 0.0 }, -1, "" },
            { "out", { 40.0, 0.0 }, -1, "" }
        };
    case ComponentKind::GateAnd:
    case ComponentKind::GateOr:
    case ComponentKind::GateNand:
    case ComponentKind::GateNor:
    case ComponentKind::GateXor:
        return {
            { "A", { -40.0, -20.0 }, -1, "" },
            { "B", { -40.0, 20.0 }, -1, "" },
            { "Y", { 40.0, 0.0 }, -1, "" }
        };
    case ComponentKind::FlipFlopD:
        return {
            { "D", { -40.0, -20.0 }, -1, "" },
            { "CLK", { -40.0, 20.0 }, -1, "" },
            { "Q", { 40.0, -20.0 }, -1, "" },
            { "Qn", { 40.0, 20.0 }, -1, "" }
        };
    case ComponentKind::FlipFlopJK:
        return {
            { "J", { -40.0, -20.0 }, -1, "" },
            { "K", { -40.0, 20.0 }, -1, "" },
            { "CLK", { -40.0, 0.0 }, -1, "" },
            { "Q", { 40.0, 0.0 }, -1, "" }
        };
    case ComponentKind::Multiplexer:
    case ComponentKind::Demultiplexer:
        return {
            { "D0", { -40.0, -20.0 }, -1, "" },
            { "D1", { -40.0, 20.0 }, -1, "" },
            { "Y", { 40.0, 0.0 }, -1, "" },
            { "SEL", { 0.0, 20.0 }, -1, "" }
        };
    default:
        return {
            { "1", { -40.0, 0.0 }, -1, "" },
            { "2", { 40.0, 0.0 }, -1, "" }
        };
    }
}

struct SchematicSymbol {
    std::uint64_t id = 0;
    CircuitComponent component;
    Vec2 center{};
    int rotationDeg = 0;
    bool flipH = false;
    bool flipV = false;
    SymbolStandard standard = SymbolStandard::IEC;
    std::vector<Pin> pins;

    void EnsurePinsFromTemplate() {
        if (pins.empty()) {
            pins = ComponentPinTemplate(component.kind);
        }
    }

    Vec2 PinWorldPos(std::size_t pinIdx) const noexcept {
        if (pinIdx >= pins.size()) return center;
        return TransformLocalOffset(pins[pinIdx].localOffset, center, rotationDeg, flipH, flipV);
    }

    void BoundingBox(double& minX, double& minY, double& maxX, double& maxY) const noexcept {
        const double halfW = (rotationDeg == 90 || rotationDeg == 270) ? 20.0 : 32.0;
        const double halfH = (rotationDeg == 90 || rotationDeg == 270) ? 32.0 : 20.0;
        minX = center.x - halfW;
        maxX = center.x + halfW;
        minY = center.y - halfH;
        maxY = center.y + halfH;
    }
};

struct Wire {
    std::uint64_t id = 0;
    std::vector<Vec2> vertices;
    std::uint64_t startSymbolId = 0;
    std::string startPinId;
    std::uint64_t endSymbolId = 0;
    std::string endPinId;
    int resolvedNetId = -1;
};

struct JunctionDot {
    Vec2 pos{};
    int netId = -1;
    std::uint32_t degree = 0;
};

struct NetLabel {
    std::uint64_t id = 0;
    Vec2 pos{};
    std::string name;
    bool isGlobalPower = false;
};

struct TopologyDiagnostic {
    enum class Severity : std::uint8_t { Info = 0, Warning = 1, Error = 2 };
    enum class Code : std::uint8_t {
        Ok = 0,
        MissingGround,
        FloatingNode,
        ShortedVoltageSource,
        CurrentSourceOpen,
        DanglingWire,
        UnconnectedPin
    };
    Severity severity = Severity::Info;
    Code code = Code::Ok;
    int netId = -1;
    std::uint64_t objectId = 0;
    Vec2 location{};
    std::string messageUtf8;
};

struct SchematicDocument {
    std::string title = "DocStudioSchematic";
    double gridPitch = 20.0;
    SymbolStandard defaultStandard = SymbolStandard::IEC;
    std::vector<SchematicSymbol> symbols;
    std::vector<Wire> wires;
    std::vector<NetLabel> labels;
    std::vector<JunctionDot> computedJunctions;
};

struct SnapResult {
    enum class Kind : std::uint8_t { Grid = 0, Wire = 1, Pin = 2 };
    Kind kind = Kind::Grid;
    Vec2 snapped{};
    std::uint64_t symbolId = 0;
    std::string pinId;
    std::uint64_t wireId = 0;
};

inline double Dist2D(Vec2 a, Vec2 b) noexcept {
    return std::hypot(a.x - b.x, a.y - b.y);
}

inline bool PointOnOrthogonalSegment(Vec2 p, Vec2 a, Vec2 b, double eps = 1e-3) noexcept {
    const double minX = (std::min)(a.x, b.x) - eps;
    const double maxX = (std::max)(a.x, b.x) + eps;
    const double minY = (std::min)(a.y, b.y) - eps;
    const double maxY = (std::max)(a.y, b.y) + eps;
    if (p.x < minX || p.x > maxX || p.y < minY || p.y > maxY) return false;
    if (std::abs(a.x - b.x) <= eps) return std::abs(p.x - a.x) <= eps;
    if (std::abs(a.y - b.y) <= eps) return std::abs(p.y - a.y) <= eps;
    const double len = Dist2D(a, b);
    if (len <= eps) return Dist2D(p, a) <= eps;
    const double cross = std::abs((p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x)) / len;
    return cross <= eps;
}

inline SnapResult SnapPointToSchematic(const SchematicDocument& doc, Vec2 raw) {
    const double pitch = (doc.gridPitch > 1e-3) ? doc.gridPitch : 20.0;
    const double pinRadius = 0.6 * pitch;
    const double wireRadius = 0.4 * pitch;

    SnapResult best{};
    best.kind = SnapResult::Kind::Grid;
    best.snapped = {
        std::round(raw.x / pitch) * pitch,
        std::round(raw.y / pitch) * pitch
    };

    double bestPinDist = pinRadius;
    bool foundPin = false;
    for (const auto& sym : doc.symbols) {
        for (std::size_t i = 0; i < sym.pins.size(); ++i) {
            const Vec2 wp = sym.PinWorldPos(i);
            const double d = Dist2D(raw, wp);
            if (d <= bestPinDist) {
                bestPinDist = d;
                foundPin = true;
                best.kind = SnapResult::Kind::Pin;
                best.snapped = wp;
                best.symbolId = sym.id;
                best.pinId = sym.pins[i].pinId;
                best.wireId = 0;
            }
        }
    }
    if (foundPin) return best;

    double bestWireDist = wireRadius;
    for (const auto& w : doc.wires) {
        for (std::size_t i = 1; i < w.vertices.size(); ++i) {
            const Vec2 a = w.vertices[i - 1];
            const Vec2 b = w.vertices[i];
            Vec2 proj = a;
            if (std::abs(a.x - b.x) <= 1e-6) {
                proj.x = a.x;
                proj.y = std::clamp(raw.y, (std::min)(a.y, b.y), (std::max)(a.y, b.y));
            } else if (std::abs(a.y - b.y) <= 1e-6) {
                proj.y = a.y;
                proj.x = std::clamp(raw.x, (std::min)(a.x, b.x), (std::max)(a.x, b.x));
            } else {
                const double dx = b.x - a.x;
                const double dy = b.y - a.y;
                const double t = std::clamp(((raw.x - a.x) * dx + (raw.y - a.y) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
                proj = { a.x + t * dx, a.y + t * dy };
            }
            const double d = Dist2D(raw, proj);
            if (d <= bestWireDist) {
                bestWireDist = d;
                best.kind = SnapResult::Kind::Wire;
                best.snapped = proj;
                best.wireId = w.id;
                best.symbolId = 0;
                best.pinId.clear();
            }
        }
    }
    return best;
}

inline std::vector<Vec2> CleanCollinearVertices(const std::vector<Vec2>& pts) {
    std::vector<Vec2> dedup;
    for (const auto& p : pts) {
        if (dedup.empty() || Dist2D(dedup.back(), p) > 1e-6) {
            dedup.push_back(p);
        }
    }
    if (dedup.size() <= 2) return dedup;
    std::vector<Vec2> out;
    out.push_back(dedup.front());
    for (std::size_t i = 1; i + 1 < dedup.size(); ++i) {
        const Vec2& a = out.back();
        const Vec2& b = dedup[i];
        const Vec2& c = dedup[i + 1];
        const bool sameX = std::abs(a.x - b.x) < 1e-6 && std::abs(b.x - c.x) < 1e-6;
        const bool sameY = std::abs(a.y - b.y) < 1e-6 && std::abs(b.y - c.y) < 1e-6;
        if (!sameX && !sameY) {
            out.push_back(b);
        }
    }
    out.push_back(dedup.back());
    return out;
}

struct RectBox {
    double left = 0.0;
    double top = 0.0;
    double right = 0.0;
    double bottom = 0.0;
};

inline bool SegmentIntersectsBox(Vec2 a, Vec2 b, const RectBox& box) noexcept {
    const double eps = 1e-4;
    const double minX = (std::min)(a.x, b.x);
    const double maxX = (std::max)(a.x, b.x);
    const double minY = (std::min)(a.y, b.y);
    const double maxY = (std::max)(a.y, b.y);
    if (maxX <= box.left + eps || minX >= box.right - eps) return false;
    if (maxY <= box.top + eps || minY >= box.bottom - eps) return false;
    return true;
}

inline bool PathIntersectsObstacles(const std::vector<Vec2>& path, const std::vector<RectBox>& obstacles) noexcept {
    for (std::size_t i = 1; i < path.size(); ++i) {
        for (const auto& box : obstacles) {
            if (SegmentIntersectsBox(path[i - 1], path[i], box)) return true;
        }
    }
    return false;
}

inline std::vector<Vec2> RouteManhattanWire(
    Vec2 start,
    Vec2 end,
    const std::vector<SchematicSymbol>& symbols,
    bool preferHorizontalFirst = true,
    double margin = 10.0)
{
    if (Dist2D(start, end) < 1e-6) return { start, end };

    std::vector<RectBox> obstacles;
    obstacles.reserve(symbols.size());
    for (const auto& sym : symbols) {
        double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
        sym.BoundingBox(x0, y0, x1, y1);
        RectBox b{ x0 - margin * 0.5, y0 - margin * 0.5, x1 + margin * 0.5, y1 + margin * 0.5 };
        // Do not block segments that originate or terminate right at the symbol border
        const bool startInside = (start.x > b.left && start.x < b.right && start.y > b.top && start.y < b.bottom);
        const bool endInside = (end.x > b.left && end.x < b.right && end.y > b.top && end.y < b.bottom);
        if (!startInside && !endInside) {
            obstacles.push_back(b);
        }
    }

    if (std::abs(start.x - end.x) < 1e-6 || std::abs(start.y - end.y) < 1e-6) {
        const std::vector<Vec2> straight{ start, end };
        if (!PathIntersectsObstacles(straight, obstacles)) {
            return straight;
        }
    }

    const std::vector<Vec2> lHv = CleanCollinearVertices({ start, { end.x, start.y }, end });
    const std::vector<Vec2> lVh = CleanCollinearVertices({ start, { start.x, end.y }, end });
    if (preferHorizontalFirst) {
        if (!PathIntersectsObstacles(lHv, obstacles)) return lHv;
        if (!PathIntersectsObstacles(lVh, obstacles)) return lVh;
    } else {
        if (!PathIntersectsObstacles(lVh, obstacles)) return lVh;
        if (!PathIntersectsObstacles(lHv, obstacles)) return lHv;
    }

    const double mx = 0.5 * (start.x + end.x);
    const double my = 0.5 * (start.y + end.y);
    const std::vector<Vec2> zHvh = CleanCollinearVertices({ start, { mx, start.y }, { mx, end.y }, end });
    const std::vector<Vec2> zVhv = CleanCollinearVertices({ start, { start.x, my }, { end.x, my }, end });
    if (preferHorizontalFirst) {
        if (!PathIntersectsObstacles(zHvh, obstacles)) return zHvh;
        if (!PathIntersectsObstacles(zVhv, obstacles)) return zVhv;
    } else {
        if (!PathIntersectsObstacles(zVhv, obstacles)) return zVhv;
        if (!PathIntersectsObstacles(zHvh, obstacles)) return zHvh;
    }

    // Compressed Hanan-grid A* orthogonal router
    std::vector<double> xs{ start.x, end.x, mx };
    std::vector<double> ys{ start.y, end.y, my };
    for (const auto& b : obstacles) {
        xs.push_back(b.left - margin);
        xs.push_back(b.right + margin);
        ys.push_back(b.top - margin);
        ys.push_back(b.bottom + margin);
    }
    auto sortUnique = [](std::vector<double>& v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end(), [](double a, double b) { return std::abs(a - b) < 1e-4; }), v.end());
    };
    sortUnique(xs);
    sortUnique(ys);

    const std::size_t nx = xs.size();
    const std::size_t ny = ys.size();
    auto idxOf = [&](const std::vector<double>& arr, double val) -> std::size_t {
        std::size_t bestI = 0;
        double bestD = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            const double d = std::abs(arr[i] - val);
            if (d < bestD) { bestD = d; bestI = i; }
        }
        return bestI;
    };

    const std::size_t sx = idxOf(xs, start.x);
    const std::size_t sy = idxOf(ys, start.y);
    const std::size_t gx = idxOf(xs, end.x);
    const std::size_t gy = idxOf(ys, end.y);

    // State encodes (ix, iy, dir): dir 0 = none, 1 = H, 2 = V
    const std::size_t totalStates = nx * ny * 3;
    std::vector<double> gScore(totalStates, std::numeric_limits<double>::infinity());
    std::vector<int> parent(totalStates, -1);

    auto stateId = [&](std::size_t x, std::size_t y, std::size_t d) {
        return (y * nx + x) * 3 + d;
    };

    struct QItem {
        double f;
        std::size_t state;
        bool operator>(const QItem& o) const noexcept { return f > o.f; }
    };
    std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;
    const std::size_t startState = stateId(sx, sy, 0);
    gScore[startState] = 0.0;
    pq.push({ std::abs(xs[gx] - xs[sx]) + std::abs(ys[gy] - ys[sy]), startState });

    std::size_t foundGoal = totalStates;
    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };
    const std::size_t dKind[4] = { 1, 1, 2, 2 };

    while (!pq.empty()) {
        const QItem top = pq.top();
        pq.pop();
        if (top.f > gScore[top.state] + std::abs(xs[gx] - xs[sx]) + std::abs(ys[gy] - ys[sy]) + 1e6) continue;

        const std::size_t curDir = top.state % 3;
        const std::size_t xy = top.state / 3;
        const std::size_t cx = xy % nx;
        const std::size_t cy = xy / nx;
        if (cx == gx && cy == gy) {
            foundGoal = top.state;
            break;
        }
        const Vec2 pCur{ xs[cx], ys[cy] };
        for (int k = 0; k < 4; ++k) {
            const int nxi = static_cast<int>(cx) + dx[k];
            const int nyi = static_cast<int>(cy) + dy[k];
            if (nxi < 0 || nyi < 0 || nxi >= static_cast<int>(nx) || nyi >= static_cast<int>(ny)) continue;
            const std::size_t ux = static_cast<std::size_t>(nxi);
            const std::size_t uy = static_cast<std::size_t>(nyi);
            const Vec2 pNext{ xs[ux], ys[uy] };
            bool hit = false;
            for (const auto& b : obstacles) {
                if (SegmentIntersectsBox(pCur, pNext, b)) { hit = true; break; }
            }
            if (hit) continue;

            const double segLen = std::abs(pNext.x - pCur.x) + std::abs(pNext.y - pCur.y);
            const double bendCost = (curDir != 0 && curDir != dKind[k]) ? 15.0 : 0.0;
            const std::size_t ns = stateId(ux, uy, dKind[k]);
            const double candG = gScore[top.state] + segLen + bendCost;
            if (candG < gScore[ns]) {
                gScore[ns] = candG;
                parent[ns] = static_cast<int>(top.state);
                const double h = std::abs(xs[gx] - pNext.x) + std::abs(ys[gy] - pNext.y);
                pq.push({ candG + h, ns });
            }
        }
    }

    if (foundGoal < totalStates) {
        std::vector<Vec2> rev;
        int cur = static_cast<int>(foundGoal);
        while (cur >= 0) {
            const std::size_t xy = static_cast<std::size_t>(cur) / 3;
            const std::size_t cx = xy % nx;
            const std::size_t cy = xy / nx;
            rev.push_back({ xs[cx], ys[cy] });
            cur = parent[static_cast<std::size_t>(cur)];
        }
        std::reverse(rev.begin(), rev.end());
        return CleanCollinearVertices(rev);
    }

    return preferHorizontalFirst ? lHv : lVh;
}

namespace Detail {

class DisjointSet {
public:
    explicit DisjointSet(std::size_t n) : parent_(n), rank_(n, 0) {
        for (std::size_t i = 0; i < n; ++i) parent_[i] = i;
    }
    std::size_t Find(std::size_t x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }
    void Unite(std::size_t a, std::size_t b) {
        a = Find(a);
        b = Find(b);
        if (a == b) return;
        if (rank_[a] < rank_[b]) std::swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
    }

private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> rank_;
};

inline std::string CanonicalNetName(std::string raw) {
    std::string up = ToUpperAscii(std::move(raw));
    if (up == "0" || up == "VSS" || up == "GND" || up == "GROUND") return "GND";
    return up;
}

} // namespace Detail

inline std::vector<CircuitComponent> CompileSchematicToMna(SchematicDocument& doc) {
    doc.computedJunctions.clear();

    struct ElemRef {
        Vec2 pos{};
        bool isPin = false;
        std::size_t symIdx = 0;
        std::size_t pinIdx = 0;
        bool isWireVertex = false;
        std::size_t wireIdx = 0;
        bool isLabel = false;
        std::size_t labelIdx = 0;
    };

    std::vector<ElemRef> elems;
    for (std::size_t si = 0; si < doc.symbols.size(); ++si) {
        doc.symbols[si].EnsurePinsFromTemplate();
        for (std::size_t pi = 0; pi < doc.symbols[si].pins.size(); ++pi) {
            ElemRef e;
            e.pos = doc.symbols[si].PinWorldPos(pi);
            e.isPin = true;
            e.symIdx = si;
            e.pinIdx = pi;
            elems.push_back(e);
        }
    }

    std::vector<std::vector<std::size_t>> wireElemIndices(doc.wires.size());
    for (std::size_t wi = 0; wi < doc.wires.size(); ++wi) {
        for (std::size_t vi = 0; vi < doc.wires[wi].vertices.size(); ++vi) {
            ElemRef e;
            e.pos = doc.wires[wi].vertices[vi];
            e.isWireVertex = true;
            e.wireIdx = wi;
            wireElemIndices[wi].push_back(elems.size());
            elems.push_back(e);
        }
    }

    std::vector<std::size_t> labelElemIndices(doc.labels.size(), 0);
    for (std::size_t li = 0; li < doc.labels.size(); ++li) {
        ElemRef e;
        e.pos = doc.labels[li].pos;
        e.isLabel = true;
        e.labelIdx = li;
        labelElemIndices[li] = elems.size();
        elems.push_back(e);
    }

    if (elems.empty()) return {};

    Detail::DisjointSet dsu(elems.size());

    // 1. All vertices in the same wire belong to one electrical net
    for (const auto& wElems : wireElemIndices) {
        for (std::size_t k = 1; k < wElems.size(); ++k) {
            dsu.Unite(wElems[0], wElems[k]);
        }
    }

    // 2. Coincident positions unite
    constexpr double kEps = 1e-2;
    for (std::size_t i = 0; i < elems.size(); ++i) {
        for (std::size_t j = i + 1; j < elems.size(); ++j) {
            if (Dist2D(elems[i].pos, elems[j].pos) <= kEps) {
                dsu.Unite(i, j);
            }
        }
    }

    // 3. Explicit wire pin bindings unite if present
    for (std::size_t wi = 0; wi < doc.wires.size(); ++wi) {
        if (wireElemIndices[wi].empty()) continue;
        const auto& w = doc.wires[wi];
        const std::size_t wRoot = wireElemIndices[wi][0];
        auto bindEndpoint = [&](std::uint64_t symId, const std::string& pId) {
            if (symId == 0) return;
            for (std::size_t i = 0; i < elems.size(); ++i) {
                if (!elems[i].isPin) continue;
                const auto& sym = doc.symbols[elems[i].symIdx];
                if (sym.id == symId && sym.pins[elems[i].pinIdx].pinId == pId) {
                    dsu.Unite(wRoot, i);
                    break;
                }
            }
        };
        bindEndpoint(w.startSymbolId, w.startPinId);
        bindEndpoint(w.endSymbolId, w.endPinId);
    }

    // 4. T-intersections: point lying on interior of wire segment unites and creates JunctionDot
    std::vector<Vec2> tJunctionPositions;
    for (std::size_t i = 0; i < elems.size(); ++i) {
        for (std::size_t wi = 0; wi < doc.wires.size(); ++wi) {
            if (wireElemIndices[wi].empty()) continue;
            const auto& verts = doc.wires[wi].vertices;
            for (std::size_t vi = 1; vi < verts.size(); ++vi) {
                if (PointOnOrthogonalSegment(elems[i].pos, verts[vi - 1], verts[vi], kEps)) {
                    dsu.Unite(i, wireElemIndices[wi][0]);
                    if (!elems[i].isLabel &&
                        Dist2D(elems[i].pos, verts[vi - 1]) > kEps &&
                        Dist2D(elems[i].pos, verts[vi]) > kEps) {
                        tJunctionPositions.push_back(elems[i].pos);
                    }
                    break;
                }
            }
        }
    }

    // 5. NetLabels with identical canonical names unite; Ground/GND pin unites with GND
    std::map<std::string, std::size_t> nameRoot;
    for (std::size_t li = 0; li < doc.labels.size(); ++li) {
        const std::string cname = Detail::CanonicalNetName(doc.labels[li].name);
        if (cname.empty()) continue;
        auto it = nameRoot.find(cname);
        if (it == nameRoot.end()) {
            nameRoot[cname] = labelElemIndices[li];
        } else {
            dsu.Unite(it->second, labelElemIndices[li]);
        }
    }

    for (std::size_t i = 0; i < elems.size(); ++i) {
        if (!elems[i].isPin) continue;
        const auto& sym = doc.symbols[elems[i].symIdx];
        const auto& pin = sym.pins[elems[i].pinIdx];
        std::string cname = Detail::CanonicalNetName(pin.netName);
        if (sym.component.kind == ComponentKind::Ground) cname = "GND";
        else if (sym.component.kind == ComponentKind::Vcc && cname.empty()) cname = "VCC";
        if (!cname.empty()) {
            auto it = nameRoot.find(cname);
            if (it == nameRoot.end()) nameRoot[cname] = i;
            else dsu.Unite(it->second, i);
        }
    }

    // 6. Wire / Connector symbols bridge their pins
    for (std::size_t si = 0; si < doc.symbols.size(); ++si) {
        const auto kind = doc.symbols[si].component.kind;
        if (kind == ComponentKind::Wire || kind == ComponentKind::Connector) {
            std::vector<std::size_t> pElems;
            for (std::size_t i = 0; i < elems.size(); ++i) {
                if (elems[i].isPin && elems[i].symIdx == si) pElems.push_back(i);
            }
            for (std::size_t k = 1; k < pElems.size(); ++k) dsu.Unite(pElems[0], pElems[k]);
        }
    }

    // 7. Assign deterministic integer net IDs (GND -> 0, others sorted by root index -> 1..N)
    std::size_t gndRoot = std::numeric_limits<std::size_t>::max();
    auto gndIt = nameRoot.find("GND");
    if (gndIt != nameRoot.end()) {
        gndRoot = dsu.Find(gndIt->second);
    }

    std::map<std::size_t, int> rootToNet;
    int nextNet = 1;
    for (std::size_t i = 0; i < elems.size(); ++i) {
        const std::size_t r = dsu.Find(i);
        if (r == gndRoot) {
            rootToNet[r] = 0;
        } else if (rootToNet.find(r) == rootToNet.end()) {
            rootToNet[r] = nextNet++;
        }
    }

    for (std::size_t i = 0; i < elems.size(); ++i) {
        const int netId = rootToNet[dsu.Find(i)];
        if (elems[i].isPin) {
            doc.symbols[elems[i].symIdx].pins[elems[i].pinIdx].resolvedNetId = netId;
        } else if (elems[i].isWireVertex) {
            doc.wires[elems[i].wireIdx].resolvedNetId = netId;
        }
    }

    // 8. Compute JunctionDots (degree >= 3 or T-intersection)
    struct PtCount {
        Vec2 pos{};
        int netId = -1;
        std::uint32_t degree = 0;
    };
    std::vector<PtCount> ptCounts;
    auto addPt = [&](Vec2 p, int netId, std::uint32_t weight) {
        for (auto& pc : ptCounts) {
            if (Dist2D(pc.pos, p) <= kEps) {
                pc.degree += weight;
                return;
            }
        }
        ptCounts.push_back({ p, netId, weight });
    };
    for (std::size_t wi = 0; wi < doc.wires.size(); ++wi) {
        const auto& w = doc.wires[wi];
        if (w.vertices.size() >= 2) {
            addPt(w.vertices.front(), w.resolvedNetId, 1);
            addPt(w.vertices.back(), w.resolvedNetId, 1);
        }
    }
    for (const auto& tp : tJunctionPositions) {
        int netId = -1;
        for (const auto& e : elems) {
            if (Dist2D(e.pos, tp) <= kEps) {
                netId = rootToNet[dsu.Find(static_cast<std::size_t>(&e - elems.data()))];
                break;
            }
        }
        addPt(tp, netId, 3);
    }
    for (const auto& pc : ptCounts) {
        if (pc.degree >= 3) {
            doc.computedJunctions.push_back({ pc.pos, pc.netId, pc.degree });
        }
    }

    // 9. Compile CircuitComponents with updated nodePos/nodeNeg/nodeOut/nodeCtrlPos/nodeCtrlNeg
    std::vector<CircuitComponent> out;
    out.reserve(doc.symbols.size());
    for (auto& sym : doc.symbols) {
        CircuitComponent c = sym.component;
        auto pinNet = [&](std::size_t idx, int fallback) -> int {
            return (idx < sym.pins.size() && sym.pins[idx].resolvedNetId >= 0)
                ? sym.pins[idx].resolvedNetId
                : fallback;
        };
        if (c.kind == ComponentKind::Ground || c.kind == ComponentKind::Wire || c.kind == ComponentKind::Connector) {
            if (!sym.pins.empty()) c.nodePos = pinNet(0, 0);
            c.nodeNeg = 0;
        } else if (c.kind == ComponentKind::Vcc) {
            c.nodePos = pinNet(0, 0);
            c.nodeNeg = 0;
        } else if (c.kind == ComponentKind::OpAmp || c.kind == ComponentKind::Comparator ||
                   c.kind == ComponentKind::BjtNpn || c.kind == ComponentKind::BjtPnp ||
                   c.kind == ComponentKind::MosfetN || c.kind == ComponentKind::MosfetP ||
                   c.kind == ComponentKind::JfetN || c.kind == ComponentKind::JfetP ||
                   c.kind == ComponentKind::Scr || c.kind == ComponentKind::Triac ||
                   c.kind == ComponentKind::Potentiometer || c.kind == ComponentKind::VoltageRegulator ||
                   c.kind == ComponentKind::Ic ||
                   c.kind == ComponentKind::GateAnd || c.kind == ComponentKind::GateOr ||
                   c.kind == ComponentKind::GateNand || c.kind == ComponentKind::GateNor ||
                   c.kind == ComponentKind::GateXor) {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeNeg = pinNet(1, c.nodeNeg);
            c.nodeOut = pinNet(2, c.nodeOut);
        } else if (c.kind == ComponentKind::GateNot) {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeOut = pinNet(1, c.nodeOut);
        } else if (c.kind == ComponentKind::Transformer) {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeNeg = pinNet(1, c.nodeNeg);
            c.nodeOut = pinNet(2, c.nodeOut);
            c.nodeCtrlPos = pinNet(3, c.nodeCtrlPos);
        } else if (c.kind == ComponentKind::Vcvs || c.kind == ComponentKind::Vccs || c.kind == ComponentKind::Relay) {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeNeg = pinNet(1, c.nodeNeg);
            c.nodeCtrlPos = pinNet(2, c.nodeCtrlPos);
            c.nodeCtrlNeg = pinNet(3, c.nodeCtrlNeg);
        } else if (c.kind == ComponentKind::FlipFlopD) {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeCtrlPos = pinNet(1, c.nodeCtrlPos);
            c.nodeOut = pinNet(2, c.nodeOut);
            c.nodeCtrlNeg = pinNet(3, c.nodeCtrlNeg);
        } else if (c.kind == ComponentKind::FlipFlopJK) {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeCtrlNeg = pinNet(1, c.nodeCtrlNeg);
            c.nodeCtrlPos = pinNet(2, c.nodeCtrlPos);
            c.nodeOut = pinNet(3, c.nodeOut);
        } else if (c.kind == ComponentKind::Multiplexer || c.kind == ComponentKind::Demultiplexer) {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeNeg = pinNet(1, c.nodeNeg);
            c.nodeOut = pinNet(2, c.nodeOut);
            c.nodeCtrlPos = pinNet(3, c.nodeCtrlPos);
        } else {
            c.nodePos = pinNet(0, c.nodePos);
            c.nodeNeg = pinNet(1, c.nodeNeg);
        }
        sym.component = c;
        out.push_back(std::move(c));
    }
    return out;
}

inline std::vector<TopologyDiagnostic> ValidateSchematicTopology(const SchematicDocument& doc) {
    std::vector<TopologyDiagnostic> diags;
    if (doc.symbols.empty()) return diags;

    bool hasGround = false;
    std::map<int, int> netPinCount;
    std::map<int, Vec2> netLoc;

    for (const auto& sym : doc.symbols) {
        if (sym.component.kind == ComponentKind::Ground) hasGround = true;
        for (std::size_t i = 0; i < sym.pins.size(); ++i) {
            const int nid = sym.pins[i].resolvedNetId;
            if (nid == 0) hasGround = true;
            if (nid >= 0) {
                netPinCount[nid]++;
                netLoc[nid] = sym.PinWorldPos(i);
            } else {
                TopologyDiagnostic d;
                d.severity = TopologyDiagnostic::Severity::Warning;
                d.code = TopologyDiagnostic::Code::UnconnectedPin;
                d.objectId = sym.id;
                d.location = sym.PinWorldPos(i);
                d.messageUtf8 = "Unconnected pin " + sym.pins[i].pinId + " on " + sym.component.name;
                diags.push_back(std::move(d));
            }
        }
    }
    for (const auto& lbl : doc.labels) {
        if (Detail::CanonicalNetName(lbl.name) == "GND") hasGround = true;
    }

    if (!hasGround) {
        TopologyDiagnostic d;
        d.severity = TopologyDiagnostic::Severity::Error;
        d.code = TopologyDiagnostic::Code::MissingGround;
        d.messageUtf8 = "Circuit has no GND (node 0) reference.";
        diags.push_back(std::move(d));
    }

    for (const auto& kv : netPinCount) {
        if (kv.first > 0 && kv.second < 2) {
            TopologyDiagnostic d;
            d.severity = TopologyDiagnostic::Severity::Warning;
            d.code = TopologyDiagnostic::Code::FloatingNode;
            d.netId = kv.first;
            d.location = netLoc[kv.first];
            d.messageUtf8 = "Floating net N" + std::to_string(kv.first) + " has fewer than 2 connected pins.";
            diags.push_back(std::move(d));
        }
    }

    // Check shorted voltage sources & voltage-source loops
    for (const auto& sym : doc.symbols) {
        const auto& c = sym.component;
        const bool isVsrc = (c.kind == ComponentKind::VoltageSource ||
                             c.kind == ComponentKind::Battery ||
                             c.kind == ComponentKind::AcSource ||
                             c.kind == ComponentKind::PulseSource);
        if (isVsrc && c.nodePos == c.nodeNeg) {
            TopologyDiagnostic d;
            d.severity = TopologyDiagnostic::Severity::Error;
            d.code = TopologyDiagnostic::Code::ShortedVoltageSource;
            d.objectId = sym.id;
            d.netId = c.nodePos;
            d.location = sym.center;
            d.messageUtf8 = "Voltage source " + c.name + " is directly shorted across net N" + std::to_string(c.nodePos);
            diags.push_back(std::move(d));
        }
    }

    // Current source open check: verify both nodes connect to at least one other component
    for (const auto& sym : doc.symbols) {
        const auto& c = sym.component;
        if (c.kind == ComponentKind::CurrentSource) {
            const int degPos = (c.nodePos == 0) ? 2 : netPinCount[c.nodePos];
            const int degNeg = (c.nodeNeg == 0) ? 2 : netPinCount[c.nodeNeg];
            if (degPos < 2 || degNeg < 2) {
                TopologyDiagnostic d;
                d.severity = TopologyDiagnostic::Severity::Error;
                d.code = TopologyDiagnostic::Code::CurrentSourceOpen;
                d.objectId = sym.id;
                d.location = sym.center;
                d.messageUtf8 = "Current source " + c.name + " has an open-circuit terminal.";
                diags.push_back(std::move(d));
            }
        }
    }

    for (const auto& w : doc.wires) {
        if (w.vertices.size() < 2) {
            TopologyDiagnostic d;
            d.severity = TopologyDiagnostic::Severity::Warning;
            d.code = TopologyDiagnostic::Code::DanglingWire;
            d.objectId = w.id;
            d.messageUtf8 = "Dangling or degenerate wire #" + std::to_string(w.id);
            diags.push_back(std::move(d));
        }
    }

    return diags;
}

inline SchematicDocument AutoLayoutFromNetlist(
    const std::vector<CircuitComponent>& comps,
    SymbolStandard standard = SymbolStandard::IEC)
{
    SchematicDocument doc;
    doc.defaultStandard = standard;
    doc.gridPitch = 20.0;

    std::uint64_t nextId = 1;
    std::map<int, std::vector<Vec2>> netAnchors;

    const std::size_t cols = 3;
    for (std::size_t i = 0; i < comps.size(); ++i) {
        SchematicSymbol sym;
        sym.id = nextId++;
        sym.component = comps[i];
        sym.standard = standard;
        const std::size_t col = i % cols;
        const std::size_t row = i / cols;
        sym.center = { 120.0 + static_cast<double>(col) * 180.0, 100.0 + static_cast<double>(row) * 120.0 };
        sym.pins = ComponentPinTemplate(comps[i].kind);

        auto assignPin = [&](std::size_t idx, int netId) {
            if (idx < sym.pins.size()) {
                sym.pins[idx].resolvedNetId = netId;
                sym.pins[idx].netName = (netId == 0) ? "GND" : ("N" + std::to_string(netId));
                netAnchors[netId].push_back(sym.PinWorldPos(idx));
            }
        };
        const auto k = comps[i].kind;
        if (k == ComponentKind::OpAmp || k == ComponentKind::Comparator ||
            k == ComponentKind::BjtNpn || k == ComponentKind::BjtPnp ||
            k == ComponentKind::MosfetN || k == ComponentKind::MosfetP ||
            k == ComponentKind::JfetN || k == ComponentKind::JfetP ||
            k == ComponentKind::Scr || k == ComponentKind::Triac ||
            k == ComponentKind::Potentiometer || k == ComponentKind::VoltageRegulator ||
            k == ComponentKind::Ic ||
            k == ComponentKind::GateAnd || k == ComponentKind::GateOr ||
            k == ComponentKind::GateNand || k == ComponentKind::GateNor ||
            k == ComponentKind::GateXor) {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeNeg);
            assignPin(2, comps[i].nodeOut);
        } else if (k == ComponentKind::GateNot) {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeOut);
        } else if (k == ComponentKind::Transformer) {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeNeg);
            assignPin(2, comps[i].nodeOut);
            assignPin(3, comps[i].nodeCtrlPos);
        } else if (k == ComponentKind::Vcvs || k == ComponentKind::Vccs || k == ComponentKind::Relay) {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeNeg);
            assignPin(2, comps[i].nodeCtrlPos);
            assignPin(3, comps[i].nodeCtrlNeg);
        } else if (k == ComponentKind::FlipFlopD) {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeCtrlPos);
            assignPin(2, comps[i].nodeOut);
            assignPin(3, comps[i].nodeCtrlNeg);
        } else if (k == ComponentKind::FlipFlopJK) {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeCtrlNeg);
            assignPin(2, comps[i].nodeCtrlPos);
            assignPin(3, comps[i].nodeOut);
        } else if (k == ComponentKind::Multiplexer || k == ComponentKind::Demultiplexer) {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeNeg);
            assignPin(2, comps[i].nodeOut);
            assignPin(3, comps[i].nodeCtrlPos);
        } else if (k == ComponentKind::Ground || k == ComponentKind::Vcc) {
            assignPin(0, comps[i].nodePos);
        } else {
            assignPin(0, comps[i].nodePos);
            assignPin(1, comps[i].nodeNeg);
        }
        doc.symbols.push_back(std::move(sym));
    }

    // Create net labels & orthogonal Manhattan wires connecting pins on the same net (>0)
    for (const auto& kv : netAnchors) {
        if (kv.second.empty()) continue;
        NetLabel lbl;
        lbl.id = nextId++;
        lbl.pos = kv.second.front();
        lbl.name = (kv.first == 0) ? "GND" : ("N" + std::to_string(kv.first));
        lbl.isGlobalPower = (kv.first == 0);
        doc.labels.push_back(std::move(lbl));

        if (kv.first > 0 && kv.second.size() >= 2) {
            for (std::size_t idx = 1; idx < kv.second.size(); ++idx) {
                Wire w;
                w.id = nextId++;
                w.vertices = RouteManhattanWire(kv.second[idx - 1], kv.second[idx], doc.symbols, true);
                w.resolvedNetId = kv.first;
                doc.wires.push_back(std::move(w));
            }
        }
    }

    // Add junction dots where >=3 pin/wire endpoints meet on a net
    for (const auto& kv : netAnchors) {
        if (kv.first > 0 && kv.second.size() >= 3) {
            doc.computedJunctions.push_back({ kv.second[1], kv.first, static_cast<std::uint32_t>(kv.second.size()) });
        }
    }
    return doc;
}

// -------- Live Current & Polarity Visualization (F-242) --------

struct CurrentArrowOverlay {
    std::string componentName;
    Vec2 pStart{};
    Vec2 pEnd{};
    double currentMagA = 0.0;
    double currentSignedA = 0.0;
    double normIntensity = 0.0; // [0..1]
    std::string labelUtf8;
};

struct NodePolarityBadge {
    int netId = 0;
    Vec2 anchor{};
    double voltageRealV = 0.0;
    double voltageMagV = 0.0;
    double voltagePhaseDeg = 0.0;
    int polaritySign = 0; // +1, -1, 0
    std::string badgeUtf8;
};

struct SchematicOverlayData {
    std::vector<CurrentArrowOverlay> currentArrows;
    std::vector<NodePolarityBadge> polarityBadges;
    std::vector<TopologyDiagnostic> diagnostics;
};

inline SchematicOverlayData BuildSchematicOverlay(
    const SchematicDocument& sch,
    const CircuitSolveResult& sol)
{
    SchematicOverlayData ov;
    ov.diagnostics = ValidateSchematicTopology(sch);
    if (!sol.ok) return ov;

    std::unordered_map<std::string, const BranchResult*> branchByName;
    for (const auto& br : sol.branches) branchByName[br.name] = &br;

    for (const auto& sym : sch.symbols) {
        if (!sym.component.active || sym.pins.size() < 2) continue;
        auto itBr = branchByName.find(sym.component.name);
        if (itBr == branchByName.end()) continue;
        const auto& br = *itBr->second;
        Vec2 pA = sym.PinWorldPos(0);
        Vec2 pB = sym.PinWorldPos(1);
        if (sym.component.kind == ComponentKind::OpAmp && sym.pins.size() >= 3) {
            pA = sym.center;
            pB = sym.PinWorldPos(2);
        }
        const double signedI = br.currentA.real();
        const double magI = std::abs(br.currentA);
        if (signedI < 0.0) std::swap(pA, pB);

        CurrentArrowOverlay arr;
        arr.componentName = br.name;
        arr.pStart = pA;
        arr.pEnd = pB;
        arr.currentMagA = magI;
        arr.currentSignedA = signedI;
        arr.normIntensity = std::clamp(std::log10(1.0 + magI / 1e-4) / 4.0, 0.0, 1.0);

        std::ostringstream lss;
        lss.precision(3);
        if (magI >= 1.0) lss << magI << " A";
        else if (magI >= 1e-3) lss << (magI * 1e3) << " mA";
        else if (magI >= 1e-6) lss << (magI * 1e6) << " uA";
        else lss << magI << " A";
        arr.labelUtf8 = lss.str();
        ov.currentArrows.push_back(std::move(arr));
    }

    std::set<int> emittedNets;
    for (const auto& sym : sch.symbols) {
        for (std::size_t pi = 0; pi < sym.pins.size(); ++pi) {
            const int nid = sym.pins[pi].resolvedNetId;
            if (nid < 0 || emittedNets.count(nid)) continue;
            emittedNets.insert(nid);

            NodePolarityBadge b;
            b.netId = nid;
            b.anchor = sym.PinWorldPos(pi);
            if (nid == 0) {
                b.polaritySign = 0;
                b.badgeUtf8 = "0 V (GND)";
            } else {
                auto it = sol.nodeVoltages.find(nid);
                if (it != sol.nodeVoltages.end()) {
                    b.voltageRealV = it->second.real();
                    b.voltageMagV = std::abs(it->second);
                    b.voltagePhaseDeg = std::arg(it->second) * (180.0 / kPi);
                    if (std::abs(it->second.imag()) > 1e-6) {
                        b.polaritySign = (b.voltageRealV >= 0.0) ? 1 : -1;
                        std::ostringstream oss;
                        oss.precision(3);
                        oss << b.voltageMagV << " V@" << b.voltagePhaseDeg << "deg";
                        b.badgeUtf8 = oss.str();
                    } else {
                        b.polaritySign = (std::abs(b.voltageRealV) < 1e-6) ? 0 : (b.voltageRealV > 0.0 ? 1 : -1);
                        std::ostringstream oss;
                        oss.precision(3);
                        oss << (b.voltageRealV >= 0.0 ? "+" : "") << b.voltageRealV << " V";
                        b.badgeUtf8 = oss.str();
                    }
                }
            }
            ov.polarityBadges.push_back(std::move(b));
        }
    }
    return ov;
}

// -------- Lightweight STL-Only JSON Serialization for SchematicDocument --------

namespace Detail {

inline std::string EscapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::map<std::string, JsonValue> objVal;

    const JsonValue* Get(const std::string& k) const {
        if (type != Type::Object) return nullptr;
        auto it = objVal.find(k);
        return (it != objVal.end()) ? &it->second : nullptr;
    }
    double AsDouble(double def = 0.0) const noexcept {
        return (type == Type::Number) ? numVal : def;
    }
    int AsInt(int def = 0) const noexcept {
        return (type == Type::Number) ? static_cast<int>(std::llround(numVal)) : def;
    }
    bool AsBool(bool def = false) const noexcept {
        return (type == Type::Bool) ? boolVal : def;
    }
    std::string AsString(const std::string& def = "") const {
        return (type == Type::String) ? strVal : def;
    }
};

class MiniJsonParser {
public:
    explicit MiniJsonParser(const std::string& src) : s_(src) {}

    bool Parse(JsonValue& out) {
        SkipWs();
        if (!ParseValue(out)) return false;
        SkipWs();
        return pos_ == s_.size();
    }

private:
    const std::string& s_;
    std::size_t pos_ = 0;

    void SkipWs() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    bool ParseValue(JsonValue& out) {
        SkipWs();
        if (pos_ >= s_.size()) return false;
        const char ch = s_[pos_];
        if (ch == '{') return ParseObject(out);
        if (ch == '[') return ParseArray(out);
        if (ch == '"') {
            out = JsonValue{};
            out.type = JsonValue::Type::String;
            return ParseString(out.strVal);
        }
        if (ch == 't' && s_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            out = JsonValue{};
            out.type = JsonValue::Type::Bool;
            out.boolVal = true;
            return true;
        }
        if (ch == 'f' && s_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            out = JsonValue{};
            out.type = JsonValue::Type::Bool;
            out.boolVal = false;
            return true;
        }
        if (ch == 'n' && s_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            out = JsonValue{};
            out.type = JsonValue::Type::Null;
            return true;
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            const std::size_t start = pos_;
            if (s_[pos_] == '-') ++pos_;
            while (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_])) ||
                   s_[pos_] == '.' || s_[pos_] == 'e' || s_[pos_] == 'E' ||
                   s_[pos_] == '+' || s_[pos_] == '-')) {
                if ((s_[pos_] == '+' || s_[pos_] == '-') && pos_ > start &&
                    s_[pos_ - 1] != 'e' && s_[pos_ - 1] != 'E') {
                    break;
                }
                ++pos_;
            }
            const std::string tok = s_.substr(start, pos_ - start);
            if (tok.empty()) return false;
            try {
                std::size_t idx = 0;
                out = JsonValue{};
                out.type = JsonValue::Type::Number;
                out.numVal = std::stod(tok, &idx);
                return idx == tok.size() && std::isfinite(out.numVal);
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    bool ParseString(std::string& out) {
        if (pos_ >= s_.size() || s_[pos_] != '"') return false;
        ++pos_;
        out.clear();
        while (pos_ < s_.size()) {
            const char ch = s_[pos_++];
            if (ch == '"') return true;
            if (ch == '\\') {
                if (pos_ >= s_.size()) return false;
                const char esc = s_[pos_++];
                if (esc == '"' || esc == '\\' || esc == '/') out += esc;
                else if (esc == 'n') out += '\n';
                else if (esc == 'r') out += '\r';
                else if (esc == 't') out += '\t';
                else out += esc;
            } else {
                out += ch;
            }
        }
        return false;
    }

    bool ParseArray(JsonValue& out) {
        if (pos_ >= s_.size() || s_[pos_] != '[') return false;
        ++pos_;
        out = JsonValue{};
        out.type = JsonValue::Type::Array;
        SkipWs();
        if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
        while (pos_ < s_.size()) {
            JsonValue elem;
            if (!ParseValue(elem)) return false;
            out.arrVal.push_back(std::move(elem));
            SkipWs();
            if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
            if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
            return false;
        }
        return false;
    }

    bool ParseObject(JsonValue& out) {
        if (pos_ >= s_.size() || s_[pos_] != '{') return false;
        ++pos_;
        out = JsonValue{};
        out.type = JsonValue::Type::Object;
        SkipWs();
        if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
        while (pos_ < s_.size()) {
            SkipWs();
            std::string key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (pos_ >= s_.size() || s_[pos_] != ':') return false;
            ++pos_;
            JsonValue val;
            if (!ParseValue(val)) return false;
            out.objVal[std::move(key)] = std::move(val);
            SkipWs();
            if (pos_ < s_.size() && s_[pos_] == ',') { ++pos_; continue; }
            if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
            return false;
        }
        return false;
    }
};

} // namespace Detail

inline std::string SchematicToJson(const SchematicDocument& doc) {
    std::ostringstream oss;
    oss << std::setprecision(9);
    oss << "{\"format\":\"docstudio_schematic_v1\","
        << "\"title\":\"" << Detail::EscapeJson(doc.title) << "\","
        << "\"gridPitch\":" << doc.gridPitch << ","
        << "\"standard\":" << static_cast<int>(doc.defaultStandard) << ","
        << "\"symbols\":[";
    for (std::size_t i = 0; i < doc.symbols.size(); ++i) {
        const auto& s = doc.symbols[i];
        if (i > 0) oss << ",";
        oss << "{\"id\":" << s.id
            << ",\"kind\":" << static_cast<int>(s.component.kind)
            << ",\"name\":\"" << Detail::EscapeJson(s.component.name) << "\""
            << ",\"model\":\"" << Detail::EscapeJson(s.component.modelName) << "\""
            << ",\"value\":" << s.component.value
            << ",\"auxValue\":" << s.component.auxValue
            << ",\"phaseDeg\":" << s.component.phaseDeg
            << ",\"nodePos\":" << s.component.nodePos
            << ",\"nodeNeg\":" << s.component.nodeNeg
            << ",\"nodeOut\":" << s.component.nodeOut
            << ",\"nodeCtrlPos\":" << s.component.nodeCtrlPos
            << ",\"nodeCtrlNeg\":" << s.component.nodeCtrlNeg
            << ",\"active\":" << (s.component.active ? "true" : "false")
            << ",\"waveform\":" << static_cast<int>(s.component.waveform)
            << ",\"freqHz\":" << s.component.freqHz
            << ",\"dcOffset\":" << s.component.dcOffset
            << ",\"dutyCycle\":" << s.component.dutyCycle
            << ",\"modFreqHz\":" << s.component.modFreqHz
            << ",\"modIndex\":" << s.component.modIndex
            << ",\"riseSec\":" << s.component.riseSec
            << ",\"fallSec\":" << s.component.fallSec
            << ",\"extraNodes\":[";
        for (std::size_t ei = 0; ei < s.component.extraNodes.size(); ++ei) {
            if (ei > 0) oss << ",";
            oss << s.component.extraNodes[ei];
        }
        oss << "],\"params\":{";
        bool firstParam = true;
        for (const auto& kv : s.component.params) {
            if (!firstParam) oss << ",";
            firstParam = false;
            oss << "\"" << Detail::EscapeJson(kv.first) << "\":" << kv.second;
        }
        oss << "}"
            << ",\"x\":" << s.center.x
            << ",\"y\":" << s.center.y
            << ",\"rot\":" << s.rotationDeg
            << ",\"flipH\":" << (s.flipH ? "true" : "false")
            << ",\"flipV\":" << (s.flipV ? "true" : "false")
            << ",\"std\":" << static_cast<int>(s.standard)
            << ",\"pins\":[";
        for (std::size_t pi = 0; pi < s.pins.size(); ++pi) {
            if (pi > 0) oss << ",";
            oss << "{\"id\":\"" << Detail::EscapeJson(s.pins[pi].pinId) << "\""
                << ",\"ox\":" << s.pins[pi].localOffset.x
                << ",\"oy\":" << s.pins[pi].localOffset.y
                << ",\"net\":" << s.pins[pi].resolvedNetId
                << ",\"netName\":\"" << Detail::EscapeJson(s.pins[pi].netName) << "\"}";
        }
        oss << "]}";
    }
    oss << "],\"wires\":[";
    for (std::size_t i = 0; i < doc.wires.size(); ++i) {
        const auto& w = doc.wires[i];
        if (i > 0) oss << ",";
        oss << "{\"id\":" << w.id
            << ",\"startSym\":" << w.startSymbolId
            << ",\"startPin\":\"" << Detail::EscapeJson(w.startPinId) << "\""
            << ",\"endSym\":" << w.endSymbolId
            << ",\"endPin\":\"" << Detail::EscapeJson(w.endPinId) << "\""
            << ",\"net\":" << w.resolvedNetId
            << ",\"pts\":[";
        for (std::size_t vi = 0; vi < w.vertices.size(); ++vi) {
            if (vi > 0) oss << ",";
            oss << "[" << w.vertices[vi].x << "," << w.vertices[vi].y << "]";
        }
        oss << "]}";
    }
    oss << "],\"labels\":[";
    for (std::size_t i = 0; i < doc.labels.size(); ++i) {
        const auto& l = doc.labels[i];
        if (i > 0) oss << ",";
        oss << "{\"id\":" << l.id
            << ",\"x\":" << l.pos.x
            << ",\"y\":" << l.pos.y
            << ",\"name\":\"" << Detail::EscapeJson(l.name) << "\""
            << ",\"power\":" << (l.isGlobalPower ? "true" : "false") << "}";
    }
    oss << "]}";
    return oss.str();
}

inline bool SchematicFromJson(const std::string& jsonUtf8, SchematicDocument& outDoc) {
    Detail::MiniJsonParser parser(jsonUtf8);
    Detail::JsonValue root;
    if (!parser.Parse(root) || root.type != Detail::JsonValue::Type::Object) return false;
    const auto* fmt = root.Get("format");
    if (!fmt || fmt->AsString() != "docstudio_schematic_v1") return false;

    outDoc = SchematicDocument{};
    if (const auto* t = root.Get("title")) outDoc.title = t->AsString("DocStudioSchematic");
    if (const auto* gp = root.Get("gridPitch")) outDoc.gridPitch = gp->AsDouble(20.0);
    if (const auto* st = root.Get("standard")) outDoc.defaultStandard = static_cast<SymbolStandard>(st->AsInt(0));

    if (const auto* syms = root.Get("symbols"); syms && syms->type == Detail::JsonValue::Type::Array) {
        for (const auto& sv : syms->arrVal) {
            SchematicSymbol s;
            if (const auto* v = sv.Get("id")) s.id = static_cast<std::uint64_t>(v->AsDouble(0.0));
            if (const auto* v = sv.Get("kind")) s.component.kind = static_cast<ComponentKind>(v->AsInt(0));
            if (const auto* v = sv.Get("name")) s.component.name = v->AsString();
            if (const auto* v = sv.Get("model")) s.component.modelName = v->AsString();
            if (const auto* v = sv.Get("value")) s.component.value = v->AsDouble(1000.0);
            if (const auto* v = sv.Get("auxValue")) s.component.auxValue = v->AsDouble(0.0);
            if (const auto* v = sv.Get("phaseDeg")) s.component.phaseDeg = v->AsDouble(0.0);
            if (const auto* v = sv.Get("nodePos")) s.component.nodePos = v->AsInt(0);
            if (const auto* v = sv.Get("nodeNeg")) s.component.nodeNeg = v->AsInt(0);
            if (const auto* v = sv.Get("nodeOut")) s.component.nodeOut = v->AsInt(0);
            if (const auto* v = sv.Get("nodeCtrlPos")) s.component.nodeCtrlPos = v->AsInt(0);
            if (const auto* v = sv.Get("nodeCtrlNeg")) s.component.nodeCtrlNeg = v->AsInt(0);
            if (const auto* v = sv.Get("active")) s.component.active = v->AsBool(true);
            if (const auto* v = sv.Get("waveform")) s.component.waveform = static_cast<WaveformKind>(v->AsInt(0));
            if (const auto* v = sv.Get("freqHz")) s.component.freqHz = v->AsDouble(0.0);
            if (const auto* v = sv.Get("dcOffset")) s.component.dcOffset = v->AsDouble(0.0);
            if (const auto* v = sv.Get("dutyCycle")) s.component.dutyCycle = v->AsDouble(0.5);
            if (const auto* v = sv.Get("modFreqHz")) s.component.modFreqHz = v->AsDouble(0.0);
            if (const auto* v = sv.Get("modIndex")) s.component.modIndex = v->AsDouble(0.0);
            if (const auto* v = sv.Get("riseSec")) s.component.riseSec = v->AsDouble(1e-6);
            if (const auto* v = sv.Get("fallSec")) s.component.fallSec = v->AsDouble(1e-6);
            if (const auto* en = sv.Get("extraNodes"); en && en->type == Detail::JsonValue::Type::Array) {
                for (const auto& item : en->arrVal) s.component.extraNodes.push_back(item.AsInt(0));
            }
            if (const auto* pm = sv.Get("params"); pm && pm->type == Detail::JsonValue::Type::Object) {
                for (const auto& kv : pm->objVal) {
                    if (kv.second.type == Detail::JsonValue::Type::Number) {
                        s.component.params[kv.first] = kv.second.numVal;
                    }
                }
            }
            if (const auto* v = sv.Get("x")) s.center.x = v->AsDouble(0.0);
            if (const auto* v = sv.Get("y")) s.center.y = v->AsDouble(0.0);
            if (const auto* v = sv.Get("rot")) s.rotationDeg = v->AsInt(0);
            if (const auto* v = sv.Get("flipH")) s.flipH = v->AsBool(false);
            if (const auto* v = sv.Get("flipV")) s.flipV = v->AsBool(false);
            if (const auto* v = sv.Get("std")) s.standard = static_cast<SymbolStandard>(v->AsInt(0));
            if (const auto* ps = sv.Get("pins"); ps && ps->type == Detail::JsonValue::Type::Array) {
                for (const auto& pv : ps->arrVal) {
                    Pin p;
                    if (const auto* v = pv.Get("id")) p.pinId = v->AsString();
                    if (const auto* v = pv.Get("ox")) p.localOffset.x = v->AsDouble(0.0);
                    if (const auto* v = pv.Get("oy")) p.localOffset.y = v->AsDouble(0.0);
                    if (const auto* v = pv.Get("net")) p.resolvedNetId = v->AsInt(-1);
                    if (const auto* v = pv.Get("netName")) p.netName = v->AsString();
                    s.pins.push_back(std::move(p));
                }
            }
            s.EnsurePinsFromTemplate();
            outDoc.symbols.push_back(std::move(s));
        }
    }

    if (const auto* ws = root.Get("wires"); ws && ws->type == Detail::JsonValue::Type::Array) {
        for (const auto& wv : ws->arrVal) {
            Wire w;
            if (const auto* v = wv.Get("id")) w.id = static_cast<std::uint64_t>(v->AsDouble(0.0));
            if (const auto* v = wv.Get("startSym")) w.startSymbolId = static_cast<std::uint64_t>(v->AsDouble(0.0));
            if (const auto* v = wv.Get("startPin")) w.startPinId = v->AsString();
            if (const auto* v = wv.Get("endSym")) w.endSymbolId = static_cast<std::uint64_t>(v->AsDouble(0.0));
            if (const auto* v = wv.Get("endPin")) w.endPinId = v->AsString();
            if (const auto* v = wv.Get("net")) w.resolvedNetId = v->AsInt(-1);
            if (const auto* pts = wv.Get("pts"); pts && pts->type == Detail::JsonValue::Type::Array) {
                for (const auto& pt : pts->arrVal) {
                    if (pt.type == Detail::JsonValue::Type::Array && pt.arrVal.size() >= 2) {
                        w.vertices.push_back({ pt.arrVal[0].AsDouble(0.0), pt.arrVal[1].AsDouble(0.0) });
                    }
                }
            }
            outDoc.wires.push_back(std::move(w));
        }
    }

    if (const auto* ls = root.Get("labels"); ls && ls->type == Detail::JsonValue::Type::Array) {
        for (const auto& lv : ls->arrVal) {
            NetLabel l;
            if (const auto* v = lv.Get("id")) l.id = static_cast<std::uint64_t>(v->AsDouble(0.0));
            if (const auto* v = lv.Get("x")) l.pos.x = v->AsDouble(0.0);
            if (const auto* v = lv.Get("y")) l.pos.y = v->AsDouble(0.0);
            if (const auto* v = lv.Get("name")) l.name = v->AsString();
            if (const auto* v = lv.Get("power")) l.isGlobalPower = v->AsBool(false);
            outDoc.labels.push_back(std::move(l));
        }
    }
    return true;
}

// -------- Circuit Component Search Metadata Extractor (F-173) --------

struct CircuitIndexMetadata {
    bool ok = false;
    std::vector<std::string> componentNames;
    std::vector<std::string> componentKinds;
    std::vector<std::string> modelTokens;
    std::vector<std::string> valueTokens;
    std::vector<std::string> netTokens;
    std::string searchableTextUtf8;
};

using CircuitSearchMetadata = CircuitIndexMetadata;

inline CircuitIndexMetadata ExtractCircuitIndexMetadata(const std::vector<CircuitComponent>& components) {
    CircuitIndexMetadata meta;
    if (components.empty()) return meta;

    std::set<std::string> seenNames, seenKinds, seenModels, seenValues, seenNets;
    auto pushUnique = [](std::vector<std::string>& dst, std::set<std::string>& seen, const std::string& val) {
        if (!val.empty() && seen.insert(val).second) dst.push_back(val);
    };

    for (const auto& c : components) {
        pushUnique(meta.componentNames, seenNames, c.name);
        pushUnique(meta.componentKinds, seenKinds, ComponentKindLabel(c.kind, false));
        pushUnique(meta.componentKinds, seenKinds, ComponentKindLabel(c.kind, true));
        if (!c.modelName.empty()) pushUnique(meta.modelTokens, seenModels, c.modelName);

        std::ostringstream vRaw;
        vRaw << c.value;
        pushUnique(meta.valueTokens, seenValues, vRaw.str());

        auto addNet = [&](int n) {
            if (n == 0) pushUnique(meta.netTokens, seenNets, "GND");
            else if (n > 0) pushUnique(meta.netTokens, seenNets, "N" + std::to_string(n));
        };
        addNet(c.nodePos);
        addNet(c.nodeNeg);
        if (c.nodeOut > 0) addNet(c.nodeOut);
        if (c.nodeCtrlPos > 0) addNet(c.nodeCtrlPos);
        if (c.nodeCtrlNeg > 0) addNet(c.nodeCtrlNeg);
    }

    std::ostringstream joined;
    auto appendList = [&](const std::vector<std::string>& items) {
        for (const auto& item : items) {
            if (joined.tellp() > 0) joined << ' ';
            joined << item;
        }
    };
    appendList(meta.componentNames);
    appendList(meta.componentKinds);
    appendList(meta.modelTokens);
    appendList(meta.valueTokens);
    appendList(meta.netTokens);
    meta.searchableTextUtf8 = joined.str();
    meta.ok = true;
    return meta;
}

inline CircuitIndexMetadata ExtractCircuitIndexMetadata(const std::string& netlistOrSchematicUtf8) {
    SchematicDocument doc;
    if (SchematicFromJson(netlistOrSchematicUtf8, doc)) {
        std::vector<CircuitComponent> comps = CompileSchematicToMna(doc);
        CircuitIndexMetadata meta = ExtractCircuitIndexMetadata(comps);
        std::set<std::string> seenNets(meta.netTokens.begin(), meta.netTokens.end());
        for (const auto& lbl : doc.labels) {
            if (!lbl.name.empty() && seenNets.insert(lbl.name).second) {
                meta.netTokens.push_back(lbl.name);
                if (!meta.searchableTextUtf8.empty()) meta.searchableTextUtf8 += ' ';
                meta.searchableTextUtf8 += lbl.name;
            }
        }
        return meta;
    }

    std::vector<CircuitComponent> comps;
    if (!ParseNetlistText(netlistOrSchematicUtf8, comps, nullptr)) {
        return CircuitIndexMetadata{};
    }
    CircuitIndexMetadata meta = ExtractCircuitIndexMetadata(comps);
    // Also capture raw value/model tokens from netlist text (e.g. "10k", "1u", "LM358")
    std::istringstream iss(netlistOrSchematicUtf8);
    std::string tok;
    std::set<std::string> existing(meta.valueTokens.begin(), meta.valueTokens.end());
    while (iss >> tok) {
        if (tok.size() >= 2 && (std::isdigit(static_cast<unsigned char>(tok[0])) || tok[0] == '.')) {
            if (existing.insert(tok).second) {
                meta.valueTokens.push_back(tok);
                meta.searchableTextUtf8 += ' ' + tok;
            }
        }
    }
    return meta;
}

} // namespace DocStudio::Circuit
