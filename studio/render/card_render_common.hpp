#pragma once

#include "../circuit/mna_solver.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/ink_engine.hpp"
#include "../math/engineering_cas.hpp"
#include "d2d_renderer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace DocStudio::RenderCommon {

inline bool IsObjectSelectedInOverlay(const ViewportOverlayState* overlay, std::uint64_t objId) {
    if (!overlay || overlay->presenterMode || objId == 0) return false;
    if (overlay->selectedObjectId == objId) return true;
    return std::find(overlay->lassoSelectedIds.begin(), overlay->lassoSelectedIds.end(), objId) !=
           overlay->lassoSelectedIds.end();
}

inline bool ShouldDrawObjectOnLayer(
    const DocumentModel& doc,
    const AnnotationObject& obj,
    double& outLayerOpacity,
    const ViewportOverlayState* overlay = nullptr)
{
    const auto* layer = doc.FindLayer(obj.layerId);
    if (layer) {
        if (!layer->visible) {
            outLayerOpacity = 0.0;
            return false;
        }
        // Dual-view Teacher Presenter Mode: hide private non-exportable teacher layers on projected view
        if (overlay && overlay->presenterMode && !layer->includeInExport) {
            outLayerOpacity = 0.0;
            return false;
        }
        outLayerOpacity = std::clamp(layer->opacity, 0.05, 1.0);
    } else {
        outLayerOpacity = 1.0;
    }
    // Synchronized Ink & Node Replay: future nodes/strokes appear attenuated at 20% opacity until their timestamp
    if (overlay && overlay->replayActive && obj.inkStartTimeMs > overlay->replayCursorMs) {
        outLayerOpacity *= 0.20;
    }
    return true;
}

inline DocColor ApplyLayerOpacity(DocColor c, double layerOpacity) noexcept {
    if (layerOpacity >= 0.999) return c;
    c.a = static_cast<std::uint8_t>(std::clamp(std::lround(static_cast<double>(c.a) * layerOpacity), 0L, 255L));
    return c;
}

struct CardTextLayout {
    DocColor bg = DocColor::RGBA(248, 250, 252, 245);
    DocColor border = DocColor::RGBA(37, 99, 235, 255);
    DocColor titleColor = DocColor::RGBA(30, 58, 138, 255);
    DocColor bodyColor = DocColor::RGBA(15, 23, 42, 255);
    std::string headerUtf8;
    std::string bodyUtf8;
};

inline CardTextLayout BuildStemCardLayout(const AnnotationObject& obj, bool isDark = false, bool isHc = false) {
    CardTextLayout out{};
    std::string tagSuffix;
    if (!obj.tags.empty()) {
        tagSuffix = "  [";
        for (std::size_t i = 0; i < obj.tags.size(); ++i) {
            if (i > 0) tagSuffix += " ";
            tagSuffix += "#" + obj.tags[i];
        }
        tagSuffix += "]";
    }

    if (obj.kind == ObjectKind::StudyTape) {
        if (!obj.tapeRevealed) {
            out.bg = isDark ? DocColor::RGBA(180, 106, 12, 240) : DocColor::RGBA(245, 158, 11, 240);
            out.border = isHc ? DocColor::RGBA(15, 20, 28, 255) : DocColor::RGBA(180, 83, 9, 255);
            out.titleColor = DocColor::RGBA(255, 255, 255, 255);
            out.bodyColor = DocColor::RGBA(255, 251, 235, 255);
            out.headerUtf8 = "STUDY TAPE (Clic para revelar)" + tagSuffix;
            out.bodyUtf8 = obj.captionUtf8.empty() ? "Contenido oculto para estudio activo" : obj.captionUtf8;
        } else {
            out.bg = isDark ? DocColor::RGBA(52, 42, 24, 235) : (isHc ? DocColor::RGBA(255, 255, 255, 255) : DocColor::RGBA(254, 243, 199, 130));
            out.border = isHc ? DocColor::RGBA(15, 20, 28, 255) : DocColor::RGBA(217, 119, 6, 220);
            out.titleColor = isDark ? DocColor::RGBA(251, 191, 36, 255) : DocColor::RGBA(146, 64, 14, 255);
            out.bodyColor = isDark ? DocColor::RGBA(241, 245, 249, 255) : DocColor::RGBA(15, 23, 42, 255);
            out.headerUtf8 = "REVELADO" + tagSuffix;
            out.bodyUtf8 = obj.textUtf8;
        }
        return out;
    }

    if (obj.kind == ObjectKind::MathBlock) {
        const bool isErr = (obj.stepCheckStatusUtf8.find("ERR") != std::string::npos ||
                            obj.stepCheckStatusUtf8.find("DIFF") != std::string::npos);
        if (isDark) {
            out.bg = DocColor::RGBA(30, 38, 54, 246);
            out.border = isErr ? DocColor::RGBA(248, 113, 113, 255) : DocColor::RGBA(96, 165, 250, 235);
            out.titleColor = DocColor::RGBA(147, 197, 253, 255);
            out.bodyColor = DocColor::RGBA(241, 245, 249, 255);
        } else if (isHc) {
            out.bg = DocColor::RGBA(255, 255, 255, 255);
            out.border = isErr ? DocColor::RGBA(196, 18, 48, 255) : DocColor::RGBA(0, 64, 221, 255);
            out.titleColor = DocColor::RGBA(0, 50, 185, 255);
            out.bodyColor = DocColor::RGBA(0, 0, 0, 255);
        } else {
            out.bg = DocColor::RGBA(239, 246, 255, 245);
            out.border = isErr ? DocColor::RGBA(220, 38, 38, 255) : DocColor::RGBA(37, 99, 235, 255);
            out.titleColor = DocColor::RGBA(29, 78, 216, 255);
            out.bodyColor = DocColor::RGBA(15, 23, 42, 255);
        }
        std::string modeBadge;
        if (obj.mathSubMode == MathSubMode::NoSub) modeBadge = " [#nosub]";
        else if (obj.mathSubMode == MathSubMode::NoVar) modeBadge = " [#novar]";
        if (obj.hasSlider) {
            modeBadge += " [Slider " + std::to_string(obj.sliderMin).substr(0, 4) + ".." +
                         std::to_string(obj.sliderMax).substr(0, 4) + "]";
        }
        out.headerUtf8 = (obj.formulaExprUtf8.empty() ? std::string("MATH / #varsub") : obj.formulaExprUtf8) + modeBadge + tagSuffix;
        std::string body = !obj.textUtf8.empty() ? obj.textUtf8 : obj.evaluatedResultUtf8;
        if (!obj.stepCheckStatusUtf8.empty()) {
            if (!body.empty()) body += "\n";
            body += "[" + obj.stepCheckStatusUtf8 + "]";
        }
        out.bodyUtf8 = std::move(body);
        return out;
    }

    if (obj.kind == ObjectKind::CircuitBlock) {
        if (isDark) {
            out.bg = DocColor::RGBA(24, 42, 46, 246);
            out.border = DocColor::RGBA(45, 212, 191, 235);
            out.titleColor = DocColor::RGBA(94, 234, 212, 255);
            out.bodyColor = DocColor::RGBA(240, 253, 250, 255);
        } else if (isHc) {
            out.bg = DocColor::RGBA(255, 255, 255, 255);
            out.border = DocColor::RGBA(0, 104, 134, 255);
            out.titleColor = DocColor::RGBA(0, 92, 118, 255);
            out.bodyColor = DocColor::RGBA(0, 0, 0, 255);
        } else {
            out.bg = DocColor::RGBA(240, 253, 250, 245);
            out.border = DocColor::RGBA(13, 148, 136, 255);
            out.titleColor = DocColor::RGBA(15, 118, 110, 255);
            out.bodyColor = DocColor::RGBA(15, 23, 42, 255);
        }
        out.headerUtf8 = (obj.captionUtf8.empty() ? std::string("CIRCUITO MNA AC/DC") : obj.captionUtf8) + tagSuffix;
        if (!obj.evaluatedResultUtf8.empty()) {
            out.bodyUtf8 = obj.evaluatedResultUtf8;
        } else if (!obj.circuitNetlistUtf8.empty()) {
            const auto sol = Circuit::SolveMnaNetlist(obj.circuitNetlistUtf8, 0.0);
            out.bodyUtf8 = sol.ok ? sol.summaryUtf8 : sol.errorUtf8;
        }
        return out;
    }

    if (obj.kind == ObjectKind::CalcSessionCard) {
        if (isDark) {
            out.bg = DocColor::RGBA(24, 42, 34, 246);
            out.border = DocColor::RGBA(74, 222, 128, 230);
            out.titleColor = DocColor::RGBA(134, 239, 172, 255);
            out.bodyColor = DocColor::RGBA(240, 253, 244, 255);
        } else if (isHc) {
            out.bg = DocColor::RGBA(255, 255, 255, 255);
            out.border = DocColor::RGBA(16, 112, 56, 255);
            out.titleColor = DocColor::RGBA(14, 96, 48, 255);
            out.bodyColor = DocColor::RGBA(0, 0, 0, 255);
        } else {
            out.bg = DocColor::RGBA(240, 253, 244, 245);
            out.border = DocColor::RGBA(22, 163, 74, 255);
            out.titleColor = DocColor::RGBA(21, 128, 61, 255);
            out.bodyColor = DocColor::RGBA(15, 23, 42, 255);
        }
        out.headerUtf8 = "CASIO " + (obj.calcModelIdUtf8.empty() ? std::string("ClassWiz") : obj.calcModelIdUtf8) + tagSuffix;
        std::string body = !obj.calcKeySeqUtf8.empty() ? ("Teclas: " + obj.calcKeySeqUtf8 + "\n") : "";
        if (!obj.calcLatexUtf8.empty()) body += obj.calcLatexUtf8 + "\n";
        body += !obj.evaluatedResultUtf8.empty() ? ("= " + obj.evaluatedResultUtf8) : obj.textUtf8;
        out.bodyUtf8 = std::move(body);
        return out;
    }

    // PlotBlock
    if (isDark) {
        out.bg = DocColor::RGBA(30, 34, 48, 248);
        out.border = DocColor::RGBA(129, 140, 248, 235);
        out.titleColor = DocColor::RGBA(199, 210, 254, 255);
        out.bodyColor = DocColor::RGBA(226, 232, 240, 255);
    } else if (isHc) {
        out.bg = DocColor::RGBA(255, 255, 255, 255);
        out.border = DocColor::RGBA(54, 52, 163, 255);
        out.titleColor = DocColor::RGBA(40, 38, 138, 255);
        out.bodyColor = DocColor::RGBA(0, 0, 0, 255);
    } else {
        out.bg = DocColor::RGBA(248, 250, 252, 250);
        out.border = DocColor::RGBA(99, 102, 241, 255);
        out.titleColor = DocColor::RGBA(67, 56, 202, 255);
        out.bodyColor = DocColor::RGBA(30, 41, 59, 255);
    }
    out.headerUtf8 = (!obj.captionUtf8.empty() ? obj.captionUtf8
                   : (!obj.formulaExprUtf8.empty() ? obj.formulaExprUtf8 : std::string("GRAFICO 2D / 3D"))) + tagSuffix;
    out.bodyUtf8.clear();
    return out;
}

// Perceptually uniform Colorblind-Safe (CVD Cividis/Viridis-inspired: deep navy -> teal -> warm gold)
inline DocColor ColorblindSafeColormap(double tNorm) noexcept {
    const double t = std::clamp(tNorm, 0.0, 1.0);
    // 4 control stops designed for deuteranopia/protanopia/tritanopia discriminability
    struct Stop { double pos; double r; double g; double b; };
    static constexpr Stop kStops[4] = {
        { 0.00,  18.0,  44.0, 110.0 }, // deep indigo-blue
        { 0.35,  31.0, 118.0, 140.0 }, // ocean teal
        { 0.70, 122.0, 168.0,  92.0 }, // sage olive
        { 1.00, 246.0, 214.0,  69.0 }  // bright solar gold
    };
    for (int i = 0; i < 3; ++i) {
        if (t <= kStops[i + 1].pos || i == 2) {
            const double u = std::clamp((t - kStops[i].pos) / (kStops[i + 1].pos - kStops[i].pos), 0.0, 1.0);
            const auto lerpC = [&](double a, double b) -> std::uint8_t {
                return static_cast<std::uint8_t>(std::clamp(std::lround(a + (b - a) * u), 0L, 255L));
            };
            return DocColor::RGBA(
                lerpC(kStops[i].r, kStops[i + 1].r),
                lerpC(kStops[i].g, kStops[i + 1].g),
                lerpC(kStops[i].b, kStops[i + 1].b),
                245);
        }
    }
    return DocColor::RGBA(31, 118, 140, 245);
}

struct ColoredSeg2D {
    DocPoint a{};
    DocPoint b{};
    DocColor color = DocColor::RGBA(31, 118, 140, 255);
};

struct PlotRenderData {
    double xMin = -5.0;
    double xMax = 5.0;
    double yMin = -5.0;
    double yMax = 5.0;
    bool is3D = false;
    std::vector<DocPoint> curvePoints;
    std::vector<DocPoint> scatterPoints;
    std::vector<ColoredSeg2D> surface3DSegments;
    std::string footerUtf8;
};

inline PlotRenderData BuildPlotRenderData(const AnnotationObject& obj) {
    PlotRenderData pr{};
    std::string expr = obj.formulaExprUtf8;
    std::string varName = "x";
    std::string yVarName = "y";
    double yDomainMin = -3.0;
    double yDomainMax = 3.0;
    double paramVal = 1.0;
    std::string paramName = "a";
    std::string csvData;
    std::string modeStr = "2d";
    int degree = 1;
    int gridSteps = 18;
    double yawDeg = 42.0;
    double pitchDeg = 28.0;
    Cas::SymbolEnv env;
    std::vector<double> waveX;
    std::vector<double> waveY;

    if (!obj.plotConfigUtf8.empty()) {
        try {
            const auto j = nlohmann::json::parse(obj.plotConfigUtf8);
            if (j.is_object()) {
                expr = j.value("expr", expr);
                varName = j.value("var", varName);
                yVarName = j.value("yVar", yVarName);
                modeStr = j.value("mode", modeStr);
                if (j.value("is3D", false)) modeStr = "3d";
                pr.xMin = j.value("xMin", -5.0);
                pr.xMax = j.value("xMax", 5.0);
                yDomainMin = j.value("yMin", -3.0);
                yDomainMax = j.value("yMax", 3.0);
                paramName = j.value("paramName", paramName);
                paramVal = j.value("paramVal", 1.0);
                csvData = j.value("csv", std::string{});
                degree = j.value("degree", 1);
                gridSteps = std::clamp(j.value("gridSteps", 18), 6, 40);
                yawDeg = j.value("yawDeg", 42.0);
                pitchDeg = j.value("pitchDeg", 28.0);
                if (j.contains("env") && j["env"].is_object()) {
                    for (auto it = j["env"].begin(); it != j["env"].end(); ++it) {
                        if (it.value().is_number()) {
                            const double v = it.value().get<double>();
                            env[it.key()] = Cas::QuantityValue{ { v, 0.0 }, {}, "", 1.0, 0.0, false };
                        }
                    }
                }
                if (j.contains("waveX") && j["waveX"].is_array() &&
                    j.contains("waveY") && j["waveY"].is_array()) {
                    const std::size_t n = (std::min)(j["waveX"].size(), j["waveY"].size());
                    waveX.reserve(n);
                    waveY.reserve(n);
                    for (std::size_t i = 0; i < n; ++i) {
                        waveX.push_back(j["waveX"][i].get<double>());
                        waveY.push_back(j["waveY"][i].get<double>());
                    }
                }
            }
        } catch (...) {
            // Fall back cleanly if plotConfig is raw CSV or formula text
        }
    }

    if (env.find(paramName) == env.end()) {
        env[paramName] = Cas::QuantityValue{ { paramVal, 0.0 }, {}, "", 1.0, 0.0, false };
    }

    if (modeStr == "phasor") {
        pr.is3D = true; // uses colored vector segment pipeline across Direct2D, Cairo, PDF & SVG
        pr.xMin = 0.0; pr.xMax = 1.0;
        pr.yMin = 0.0; pr.yMax = 1.0;
        constexpr double kPi = 3.14159265358979323846;
        const DocColor axisCol = DocColor::RGBA(148, 163, 184, 210);
        const DocColor ringCol = DocColor::RGBA(203, 213, 225, 190);
        // Axes Re & jIm
        pr.surface3DSegments.push_back({ { 0.08, 0.50 }, { 0.92, 0.50 }, axisCol });
        pr.surface3DSegments.push_back({ { 0.50, 0.08 }, { 0.50, 0.92 }, axisCol });
        // Concentric polar reference circles (50% and 100%)
        for (double radR : { 0.20, 0.40 }) {
            for (int k = 0; k < 36; ++k) {
                const double a0 = (2.0 * kPi * k) / 36.0;
                const double a1 = (2.0 * kPi * (k + 1)) / 36.0;
                pr.surface3DSegments.push_back({
                    { 0.50 + radR * std::cos(a0), 0.50 + radR * std::sin(a0) },
                    { 0.50 + radR * std::cos(a1), 0.50 + radR * std::sin(a1) },
                    ringCol
                });
            }
        }

        // Parse phasor list from JSON "phasors": [{"mag":120,"deg":0},{"mag":4,"deg":-36.87}]
        struct PhasorItem { double mag = 0.0; double deg = 0.0; };
        std::vector<PhasorItem> items;
        if (!obj.plotConfigUtf8.empty()) {
            try {
                const auto j = nlohmann::json::parse(obj.plotConfigUtf8);
                if (j.contains("phasors") && j["phasors"].is_array()) {
                    for (const auto& el : j["phasors"]) {
                        items.push_back({ std::abs(el.value("mag", 0.0)), el.value("deg", 0.0) });
                    }
                }
            } catch (...) {}
        }
        double maxMag = 1e-9;
        for (const auto& it : items) maxMag = (std::max)(maxMag, it.mag);

        static constexpr DocColor kPalette[4] = {
            DocColor::RGBA(37, 99, 235, 255),
            DocColor::RGBA(220, 38, 38, 255),
            DocColor::RGBA(5, 150, 105, 255),
            DocColor::RGBA(217, 119, 6, 255)
        };
        for (std::size_t idx = 0; idx < items.size(); ++idx) {
            const double len = 0.38 * (0.28 + 0.72 * (items[idx].mag / maxMag));
            const double th = items[idx].deg * (kPi / 180.0);
            const double tx = 0.50 + len * std::cos(th);
            const double ty = 0.50 + len * std::sin(th);
            const DocColor col = kPalette[idx % 4];
            pr.surface3DSegments.push_back({ { 0.50, 0.50 }, { tx, ty }, col });
            // Arrowhead wings
            const double backA1 = th + kPi - 0.42;
            const double backA2 = th + kPi + 0.42;
            pr.surface3DSegments.push_back({ { tx, ty }, { tx + 0.045 * std::cos(backA1), ty + 0.045 * std::sin(backA1) }, col });
            pr.surface3DSegments.push_back({ { tx, ty }, { tx + 0.045 * std::cos(backA2), ty + 0.045 * std::sin(backA2) }, col });
        }
        pr.footerUtf8 = "Diagrama Fasorial Complejo (Re / jIm)";
        return pr;
    }

    if (modeStr == "3d" && !expr.empty()) {
        pr.is3D = true;
        const std::size_t gs = static_cast<std::size_t>(gridSteps);
        const auto pts3d = Cas::SampleSurface3D(expr, pr.xMin, pr.xMax, gs + 1, yDomainMin, yDomainMax, gs + 1, env);
        if (pts3d.size() == (gs + 1) * (gs + 1)) {
            double zMin = 1e18, zMax = -1e18;
            for (const auto& p : pts3d) {
                if (p.valid && std::isfinite(p.z)) {
                    zMin = (std::min)(zMin, p.z);
                    zMax = (std::max)(zMax, p.z);
                }
            }
            if (zMin > zMax) { zMin = -1.0; zMax = 1.0; }
            const double zSpan = (std::max)(1e-9, zMax - zMin);
            const double xMid = 0.5 * (pr.xMin + pr.xMax);
            const double xHalf = (std::max)(1e-9, 0.5 * (pr.xMax - pr.xMin));
            const double yMid = 0.5 * (yDomainMin + yDomainMax);
            const double yHalf = (std::max)(1e-9, 0.5 * (yDomainMax - yDomainMin));
            const double zMid = 0.5 * (zMin + zMax);

            constexpr double kPi = 3.14159265358979323846;
            const double yaw = yawDeg * (kPi / 180.0);
            const double pitch = pitchDeg * (kPi / 180.0);
            const double cy = std::cos(yaw), sy = std::sin(yaw);
            const double cp = std::cos(pitch), sp = std::sin(pitch);

            struct ProjVertex {
                double sx = 0.5;
                double sy = 0.5;
                double depth = 0.0;
                double zNorm = 0.5;
                bool valid = false;
            };
            std::vector<ProjVertex> proj(pts3d.size());
            double uMin = 1e18, uMax = -1e18, vMin = 1e18, vMax = -1e18;
            for (std::size_t i = 0; i < pts3d.size(); ++i) {
                if (!pts3d[i].valid || !std::isfinite(pts3d[i].z)) continue;
                const double nx = (pts3d[i].x - xMid) / xHalf;
                const double ny = (pts3d[i].y - yMid) / yHalf;
                const double nz = ((pts3d[i].z - zMid) / (0.5 * zSpan)) * 0.72;
                const double rx = nx * cy - ny * sy;
                const double ry = nx * sy + ny * cy;
                const double u = rx;
                const double v = nz * cp - ry * sp;
                const double depth = ry * cp + nz * sp;
                proj[i] = { u, v, depth, (pts3d[i].z - zMin) / zSpan, true };
                uMin = (std::min)(uMin, u); uMax = (std::max)(uMax, u);
                vMin = (std::min)(vMin, v); vMax = (std::max)(vMax, v);
            }

            const double uSpan = (std::max)(1e-9, uMax - uMin);
            const double vSpan = (std::max)(1e-9, vMax - vMin);
            for (auto& pv : proj) {
                if (!pv.valid) continue;
                pv.sx = 0.06 + 0.88 * ((pv.sx - uMin) / uSpan);
                pv.sy = 0.06 + 0.88 * ((pv.sy - vMin) / vSpan);
            }

            struct DepthSeg {
                double depth = 0.0;
                ColoredSeg2D seg;
            };
            std::vector<DepthSeg> dsegs;
            dsegs.reserve(gs * gs * 2);
            auto addEdge = [&](std::size_t i0, std::size_t i1) {
                if (!proj[i0].valid || !proj[i1].valid) return;
                const double zAvg = 0.5 * (proj[i0].zNorm + proj[i1].zNorm);
                const double dAvg = 0.5 * (proj[i0].depth + proj[i1].depth);
                dsegs.push_back({
                    dAvg,
                    { { proj[i0].sx, proj[i0].sy }, { proj[i1].sx, proj[i1].sy }, ColorblindSafeColormap(zAvg) }
                });
            };
            for (std::size_t iy = 0; iy <= gs; ++iy) {
                for (std::size_t ix = 0; ix <= gs; ++ix) {
                    const std::size_t idx = iy * (gs + 1) + ix;
                    if (ix < gs) addEdge(idx, idx + 1);
                    if (iy < gs) addEdge(idx, idx + gs + 1);
                }
            }
            // Painter's order: farthest to nearest
            std::sort(dsegs.begin(), dsegs.end(), [](const DepthSeg& a, const DepthSeg& b) {
                return a.depth > b.depth;
            });
            pr.surface3DSegments.reserve(dsegs.size());
            for (const auto& ds : dsegs) pr.surface3DSegments.push_back(ds.seg);
        }
        pr.xMin = 0.0; pr.xMax = 1.0;
        pr.yMin = 0.0; pr.yMax = 1.0;
        pr.footerUtf8 = "z = " + expr + " [Paleta CVD accesible]";
        return pr;
    }

    if (!waveX.empty() && waveX.size() == waveY.size()) {
        pr.xMin = waveX.front();
        pr.xMax = (std::max)(waveX.front() + 1e-9, waveX.back());
        pr.curvePoints.reserve(waveX.size());
        for (std::size_t i = 0; i < waveX.size(); ++i) {
            pr.curvePoints.push_back({ waveX[i], waveY[i] });
        }
        pr.footerUtf8 = "Osciloscopio MNA Transitorio";
    } else if (!csvData.empty()) {
        const auto reg = Cas::FitCsvRegression(csvData, degree);
        if (reg.ok) {
            pr.scatterPoints = reg.dataPoints;
            pr.curvePoints = reg.fittedCurve;
            pr.footerUtf8 = reg.equationUtf8;
        }
    } else if (!expr.empty()) {
        pr.curvePoints = Cas::SampleFunction2D(expr, varName, pr.xMin, pr.xMax, 80, env);
        pr.footerUtf8 = "y = " + expr;
    }

    bool anyY = false;
    double minY = 1e18, maxY = -1e18;
    auto scanY = [&](const std::vector<DocPoint>& pts) {
        for (const auto& pt : pts) {
            if (!std::isfinite(pt.y)) continue;
            if (!anyY) {
                minY = maxY = pt.y;
                anyY = true;
            } else {
                minY = (std::min)(minY, pt.y);
                maxY = (std::max)(maxY, pt.y);
            }
        }
    };
    scanY(pr.curvePoints);
    scanY(pr.scatterPoints);
    if (anyY) {
        const double pad = (std::max)(0.25, (maxY - minY) * 0.12);
        pr.yMin = minY - pad;
        pr.yMax = maxY + pad;
    }
    return pr;
}

// -------- HUD Overlays: Minimap (F-013) & Graduated Ruler 0..360 deg (F-039) --------

struct MinimapItemBox {
    DocRect rectScreen{};
    DocColor color = DocColor::RGBA(59, 130, 246, 200);
};

struct MinimapOverlayData {
    DocRect panelScreen{};
    DocRect viewportScreen{};
    std::vector<MinimapItemBox> items;
};

inline MinimapOverlayData BuildMinimapOverlay(
    const DocumentModel& doc,
    const Camera2D& cam,
    int viewWidthPx,
    int viewHeightPx)
{
    MinimapOverlayData mm{};
    const double pw = (viewWidthPx < 780) ? 124.0 : 148.0;
    const double ph = (viewWidthPx < 780) ? 80.0 : 96.0;
    const double margin = 12.0;
    mm.panelScreen = {
        (std::max)(8.0, static_cast<double>(viewWidthPx) - pw - margin),
        (std::max)(8.0, static_cast<double>(viewHeightPx) - ph - margin),
        pw,
        ph
    };

    const DocRect curViewWorld = cam.ScreenRectToWorld({
        0.0, 0.0, static_cast<double>(viewWidthPx), static_cast<double>(viewHeightPx)
    });
    DocRect worldAll = curViewWorld;
    auto expand = [&](const DocRect& r) {
        const double x0 = (std::min)(worldAll.x, r.x);
        const double y0 = (std::min)(worldAll.y, r.y);
        const double x1 = (std::max)(worldAll.Right(), r.Right());
        const double y1 = (std::max)(worldAll.Bottom(), r.Bottom());
        worldAll = { x0, y0, x1 - x0, y1 - y0 };
    };

    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        for (const auto& o : doc.CanvasObjects()) expand(o.bounds);
    } else {
        for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
            if (const auto* pl = doc.Layout().PlacementAt(p)) expand(pl->WorldRect());
        }
    }

    const double padW = (std::max)(40.0, worldAll.w * 0.08);
    const double padH = (std::max)(40.0, worldAll.h * 0.08);
    worldAll = { worldAll.x - padW, worldAll.y - padH, worldAll.w + 2.0 * padW, worldAll.h + 2.0 * padH };

    const double innerPad = 6.0;
    const double availW = pw - 2.0 * innerPad;
    const double availH = ph - 2.0 * innerPad;
    const double scale = (std::min)(availW / (std::max)(1.0, worldAll.w), availH / (std::max)(1.0, worldAll.h));
    const double offX = mm.panelScreen.x + innerPad + 0.5 * (availW - worldAll.w * scale);
    const double offY = mm.panelScreen.y + innerPad + 0.5 * (availH - worldAll.h * scale);

    auto mapRect = [&](const DocRect& wr) -> DocRect {
        return {
            offX + (wr.x - worldAll.x) * scale,
            offY + (wr.y - worldAll.y) * scale,
            (std::max)(2.0, wr.w * scale),
            (std::max)(2.0, wr.h * scale)
        };
    };

    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        for (const auto& o : doc.CanvasObjects()) {
            DocColor col = DocColor::RGBA(71, 85, 105, 185);
            if (o.kind == ObjectKind::MathBlock) col = DocColor::RGBA(37, 99, 235, 220);
            else if (o.kind == ObjectKind::CircuitBlock) col = DocColor::RGBA(13, 148, 136, 220);
            else if (o.kind == ObjectKind::CalcSessionCard) col = DocColor::RGBA(22, 163, 74, 220);
            else if (o.kind == ObjectKind::PlotBlock) col = DocColor::RGBA(99, 102, 241, 220);
            mm.items.push_back({ mapRect(o.bounds), col });
        }
    } else {
        for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
            if (const auto* pl = doc.Layout().PlacementAt(p)) {
                mm.items.push_back({ mapRect(pl->WorldRect()), DocColor::RGBA(226, 232, 240, 220) });
            }
        }
    }

    mm.viewportScreen = mapRect(curViewWorld);
    return mm;
}

struct RulerTickSeg {
    DocPoint p0{};
    DocPoint p1{};
    bool major = false;
};

struct RulerOverlayData {
    DocPoint corners[4]{};
    DocPoint activeEdge0{};
    DocPoint activeEdge1{};
    std::vector<RulerTickSeg> ticks;
    std::string badgeUtf8;
    DocPoint badgePos{};
};

inline RulerOverlayData BuildRulerOverlay(const RulerState& r, const Camera2D& cam) {
    RulerOverlayData out{};
    constexpr double kPi = 3.14159265358979323846;
    const double rad = r.angleDeg * (kPi / 180.0);
    const double ux = std::cos(rad);
    const double uy = std::sin(rad);
    const double nx = -uy;
    const double ny = ux;
    const double halfL = 0.5 * (std::max)(80.0, r.lengthPt);
    const double widthW = 36.0;

    const DocPoint w0{ r.centerX - ux * halfL, r.centerY - uy * halfL };
    const DocPoint w1{ r.centerX + ux * halfL, r.centerY + uy * halfL };
    const DocPoint w2{ w1.x + nx * widthW, w1.y + ny * widthW };
    const DocPoint w3{ w0.x + nx * widthW, w0.y + ny * widthW };

    out.corners[0] = cam.WorldToScreen(w0.x, w0.y);
    out.corners[1] = cam.WorldToScreen(w1.x, w1.y);
    out.corners[2] = cam.WorldToScreen(w2.x, w2.y);
    out.corners[3] = cam.WorldToScreen(w3.x, w3.y);
    out.activeEdge0 = out.corners[0];
    out.activeEdge1 = out.corners[1];

    const int numSteps = static_cast<int>((2.0 * halfL) / 10.0);
    for (int k = 0; k <= numSteps; ++k) {
        const double d = -halfL + static_cast<double>(k) * 10.0;
        const bool major = (k % 5 == 0);
        const double tickLen = major ? 12.0 : 6.0;
        const DocPoint tw0{ r.centerX + ux * d, r.centerY + uy * d };
        const DocPoint tw1{ tw0.x + nx * tickLen, tw0.y + ny * tickLen };
        out.ticks.push_back({ cam.WorldToScreen(tw0.x, tw0.y), cam.WorldToScreen(tw1.x, tw1.y), major });
    }

    const int degNorm = ((static_cast<int>(std::lround(r.angleDeg)) % 360) + 360) % 360;
    out.badgeUtf8 = "Regla " + std::to_string(degNorm) + "\xC2\xB0";
    // Geometric center of the ruler band so a pill badge never clips across the bottom edge
    out.badgePos = cam.WorldToScreen(
        r.centerX + nx * (widthW * 0.52),
        r.centerY + ny * (widthW * 0.52));
    return out;
}

} // namespace DocStudio::RenderCommon
