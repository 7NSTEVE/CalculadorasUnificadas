#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace DocStudio::Cas {

struct CvdRgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

enum class CvdPaletteKind : std::uint8_t {
    Cividis = 0,
    Viridis = 1,
    OkabeItoDiscrete = 2
};

inline CvdRgba SampleCvdPalette(CvdPaletteKind kind, double tNorm) noexcept {
    if (kind == CvdPaletteKind::OkabeItoDiscrete) {
        static constexpr CvdRgba kOkabeIto[8] = {
            {   0, 114, 178, 255 }, // Blue
            { 213,  94,   0, 255 }, // Vermilion
            {   0, 158, 115, 255 }, // Bluish green
            { 204, 121, 167, 255 }, // Reddish purple
            { 230, 159,   0, 255 }, // Orange
            {  86, 180, 233, 255 }, // Sky blue
            { 240, 228,  66, 255 }, // Yellow
            {  15,  23,  42, 255 }  // Dark slate
        };
        const std::size_t idx = static_cast<std::size_t>(std::clamp(std::llround(tNorm * 7.0), 0LL, 7LL));
        return kOkabeIto[idx];
    }

    const double t = std::clamp(tNorm, 0.0, 1.0);
    struct Stop { double pos; double r; double g; double b; };
    if (kind == CvdPaletteKind::Viridis) {
        static constexpr Stop kViridis[5] = {
            { 0.00,  68.0,   1.0,  84.0 },
            { 0.25,  59.0,  82.0, 139.0 },
            { 0.50,  33.0, 145.0, 140.0 },
            { 0.75,  94.0, 201.0,  98.0 },
            { 1.00, 253.0, 231.0,  37.0 }
        };
        for (int i = 0; i < 4; ++i) {
            if (t <= kViridis[i + 1].pos || i == 3) {
                const double u = std::clamp((t - kViridis[i].pos) / (kViridis[i + 1].pos - kViridis[i].pos), 0.0, 1.0);
                auto lc = [&](double a, double b) -> std::uint8_t {
                    return static_cast<std::uint8_t>(std::clamp(std::lround(a + (b - a) * u), 0L, 255L));
                };
                return { lc(kViridis[i].r, kViridis[i + 1].r), lc(kViridis[i].g, kViridis[i + 1].g), lc(kViridis[i].b, kViridis[i + 1].b), 245 };
            }
        }
    }

    static constexpr Stop kCividis[4] = {
        { 0.00,  18.0,  44.0, 110.0 },
        { 0.35,  31.0, 118.0, 140.0 },
        { 0.70, 122.0, 168.0,  92.0 },
        { 1.00, 246.0, 214.0,  69.0 }
    };
    for (int i = 0; i < 3; ++i) {
        if (t <= kCividis[i + 1].pos || i == 2) {
            const double u = std::clamp((t - kCividis[i].pos) / (kCividis[i + 1].pos - kCividis[i].pos), 0.0, 1.0);
            auto lc = [&](double a, double b) -> std::uint8_t {
                return static_cast<std::uint8_t>(std::clamp(std::lround(a + (b - a) * u), 0L, 255L));
            };
            return { lc(kCividis[i].r, kCividis[i + 1].r), lc(kCividis[i].g, kCividis[i + 1].g), lc(kCividis[i].b, kCividis[i + 1].b), 245 };
        }
    }
    return { 31, 118, 140, 245 };
}

struct ColorbarMetadata {
    CvdPaletteKind palette = CvdPaletteKind::Cividis;
    double minVal = 0.0;
    double maxVal = 1.0;
    std::string labelUtf8;
    std::vector<double> tickValues;
    std::vector<std::pair<double, CvdRgba>> gradientStops;
};

inline ColorbarMetadata BuildColorbarMetadata(
    CvdPaletteKind palette,
    double minVal,
    double maxVal,
    const std::string& labelUtf8 = "z",
    int numTicks = 5)
{
    ColorbarMetadata cb;
    cb.palette = palette;
    cb.minVal = minVal;
    cb.maxVal = maxVal;
    cb.labelUtf8 = labelUtf8;
    const int nTicks = (std::max)(2, numTicks);
    for (int i = 0; i < nTicks; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(nTicks - 1);
        cb.tickValues.push_back(minVal + u * (maxVal - minVal));
    }
    for (int i = 0; i <= 8; ++i) {
        const double u = static_cast<double>(i) / 8.0;
        cb.gradientStops.push_back({ u, SampleCvdPalette(palette, u) });
    }
    return cb;
}

struct PlotSample2D {
    double x = 0.0;
    double y = 0.0;
    double t = 0.0;
    bool valid = true;
};

struct PlotCurveSeries {
    std::string id;
    std::string labelUtf8;
    CvdRgba color{};
    std::vector<PlotSample2D> points;
};

enum class CriticalPointKind : std::uint8_t {
    Root = 0,
    LocalMin = 1,
    LocalMax = 2,
    Intersection = 3
};

struct PlotCriticalPoint {
    CriticalPointKind kind = CriticalPointKind::Root;
    double x = 0.0;
    double y = 0.0;
    std::string labelUtf8;
};

struct ContourSegment2D {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
};

struct ContourIsoLevel {
    double level = 0.0;
    CvdRgba color{};
    std::vector<ContourSegment2D> segments;
};

struct ContourPlotResult {
    bool ok = false;
    double zMin = 0.0;
    double zMax = 0.0;
    ColorbarMetadata colorbar{};
    std::vector<ContourIsoLevel> levels;
    std::string errorUtf8;
};

struct Surface3DViewState {
    double yawDeg = 42.0;
    double pitchDeg = 28.0;
    double zoom = 1.0;
    double xMin = -3.0, xMax = 3.0;
    double yMin = -3.0, yMax = 3.0;
    double zMin = -1.0, zMax = 1.0;
    bool autoZ = true;
    bool showAxes = true;
    bool showColorbar = true;
    bool showBaseContours = false;
    int gridSteps = 20;
    CvdPaletteKind palette = CvdPaletteKind::Cividis;
};

struct Surface3DSegment {
    double ax = 0.0, ay = 0.0;
    double bx = 0.0, by = 0.0;
    double depth = 0.0;
    CvdRgba color{};
    bool isAxis = false;
    std::string labelUtf8;
};

struct Surface3DProjection {
    bool ok = false;
    double zMin = 0.0;
    double zMax = 0.0;
    ColorbarMetadata colorbar{};
    std::vector<Surface3DSegment> segments;
    std::vector<ContourIsoLevel> baseContours;
    std::string errorUtf8;
};

enum class ParametricSheetKind : std::uint8_t {
    Millimetric = 0,
    Engineering = 1,
    SemiLogX = 2,
    SemiLogY = 3,
    LogLog = 4,
    Polar = 5,
    SmithChart = 6,
    Cartesian = 7,
    Isometric = 8
};

struct SheetSegment {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    bool major = false;
    bool medium = false;
    std::string labelUtf8;
};

struct SheetSpec {
    ParametricSheetKind kind = ParametricSheetKind::Millimetric;
    double width = 1000.0;
    double height = 1000.0;
    double pxPerMm = 150.0 / 25.4;
    double stepPx = 25.0;
    int xDecades = 3;
    int yDecades = 3;
    int xStartExp = 0;
    int yStartExp = 0;
    int polarRings = 5;
    double polarAngleStepDeg = 30.0;
    std::vector<double> smithRValues = { 0.2, 0.5, 1.0, 2.0, 5.0 };
    std::vector<double> smithXValues = { 0.2, 0.5, 1.0, 2.0, 5.0 };
};

namespace detail_plot {

inline std::unordered_set<std::string> CollectPlotBoundVars(
    const SymbolEnv& env,
    const std::string& v1 = "",
    const std::string& v2 = "")
{
    std::unordered_set<std::string> vars;
    for (const auto& kv : env) vars.insert(kv.first);
    if (!v1.empty()) vars.insert(v1);
    if (!v2.empty()) vars.insert(v2);
    return vars;
}

inline bool EvalScalarExpr(
    const std::string& expr,
    const MathJsonNode* compiledAst,
    const std::string& varName,
    double xVal,
    const SymbolEnv& baseEnv,
    double& outY)
{
    SymbolEnv work = baseEnv;
    if (!varName.empty()) {
        work[varName] = QuantityValue{ { xVal, 0.0 }, {}, "", 1.0, 0.0, false };
    }
    CasEvalResult ev{};
    if (compiledAst) {
        if (EvaluateAst(*compiledAst, work, ev) && ev.ok && ev.result.IsReal() && std::isfinite(ev.result.value.real())) {
            outY = ev.result.value.real();
            return true;
        }
    }
    ev = EvaluateExpression(expr, work);
    if (ev.ok && ev.result.IsReal() && std::isfinite(ev.result.value.real())) {
        outY = ev.result.value.real();
        return true;
    }
    return false;
}

inline bool EvalScalarXY(
    const std::string& expr,
    const MathJsonNode* compiledAst,
    double xVal,
    double yVal,
    const SymbolEnv& baseEnv,
    double& outZ)
{
    SymbolEnv work = baseEnv;
    work["x"] = QuantityValue{ { xVal, 0.0 }, {}, "", 1.0, 0.0, false };
    work["y"] = QuantityValue{ { yVal, 0.0 }, {}, "", 1.0, 0.0, false };
    CasEvalResult ev{};
    if (compiledAst) {
        if (EvaluateAst(*compiledAst, work, ev) && ev.ok && ev.result.IsReal() && std::isfinite(ev.result.value.real())) {
            outZ = ev.result.value.real();
            return true;
        }
    }
    ev = EvaluateExpression(expr, work);
    if (ev.ok && ev.result.IsReal() && std::isfinite(ev.result.value.real())) {
        outZ = ev.result.value.real();
        return true;
    }
    return false;
}

} // namespace detail_plot

inline std::vector<PlotSample2D> SampleParametric2D(
    const std::string& xExpr,
    const std::string& yExpr,
    const std::string& paramVar,
    double tMin,
    double tMax,
    std::size_t numSamples,
    const SymbolEnv& env = {})
{
    std::vector<PlotSample2D> pts;
    if (numSamples < 2 || tMax <= tMin) return pts;
    const std::string pName = paramVar.empty() ? "t" : paramVar;
    const auto boundVars = detail_plot::CollectPlotBoundVars(env, pName);
    MathJsonNode xAst{}, yAst{};
    const bool hasXAst = ParseExpressionToMathJson(xExpr, xAst, boundVars);
    const bool hasYAst = ParseExpressionToMathJson(yExpr, yAst, boundVars);

    pts.reserve(numSamples);
    for (std::size_t i = 0; i < numSamples; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(numSamples - 1);
        const double t = tMin + u * (tMax - tMin);
        double xv = 0.0, yv = 0.0;
        const bool okX = detail_plot::EvalScalarExpr(xExpr, hasXAst ? &xAst : nullptr, pName, t, env, xv);
        const bool okY = detail_plot::EvalScalarExpr(yExpr, hasYAst ? &yAst : nullptr, pName, t, env, yv);
        pts.push_back({ xv, yv, t, okX && okY });
    }
    return pts;
}

inline std::vector<PlotSample2D> SamplePolar2D(
    const std::string& rExpr,
    const std::string& thetaVar,
    double thetaMin,
    double thetaMax,
    std::size_t numSamples,
    const SymbolEnv& env = {})
{
    std::vector<PlotSample2D> pts;
    if (numSamples < 2 || thetaMax <= thetaMin) return pts;
    const std::string thName = thetaVar.empty() ? "theta" : thetaVar;
    const auto boundVars = detail_plot::CollectPlotBoundVars(env, thName);
    MathJsonNode rAst{};
    const bool hasAst = ParseExpressionToMathJson(rExpr, rAst, boundVars);

    pts.reserve(numSamples);
    double minR = 1e18, maxR = -1e18;
    std::vector<double> rVals(numSamples, 0.0);
    std::vector<bool> rOk(numSamples, false);
    for (std::size_t i = 0; i < numSamples; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(numSamples - 1);
        const double th = thetaMin + u * (thetaMax - thetaMin);
        double rv = 0.0;
        if (detail_plot::EvalScalarExpr(rExpr, hasAst ? &rAst : nullptr, thName, th, env, rv)) {
            rVals[i] = rv;
            rOk[i] = true;
            minR = (std::min)(minR, rv);
            maxR = (std::max)(maxR, rv);
        }
    }
    const double span = (std::max)(1.0, maxR - minR);
    for (std::size_t i = 0; i < numSamples; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(numSamples - 1);
        const double th = thetaMin + u * (thetaMax - thetaMin);
        bool ok = rOk[i];
        if (ok && i > 0 && rOk[i - 1] && std::abs(rVals[i] - rVals[i - 1]) > 0.5 * span && std::abs(rVals[i] - rVals[i - 1]) > 10.0) {
            ok = false; // asymptote discontinuity marker
        }
        pts.push_back({ ok ? (rVals[i] * std::cos(th)) : 0.0, ok ? (rVals[i] * std::sin(th)) : 0.0, th, ok });
    }
    return pts;
}

inline std::vector<PlotCurveSeries> SampleMultiCurve2D(
    const std::vector<std::string>& exprs,
    const std::string& var,
    double xMin,
    double xMax,
    std::size_t numSamples,
    CvdPaletteKind discretePalette = CvdPaletteKind::OkabeItoDiscrete,
    const SymbolEnv& env = {})
{
    std::vector<PlotCurveSeries> out;
    if (numSamples < 2 || xMax <= xMin) return out;
    const std::string vName = var.empty() ? "x" : var;

    for (std::size_t cIdx = 0; cIdx < exprs.size(); ++cIdx) {
        PlotCurveSeries series;
        series.id = "curve_" + std::to_string(cIdx);
        series.labelUtf8 = exprs[cIdx];
        const double tCol = exprs.size() <= 1 ? 0.0 : static_cast<double>(cIdx % 8) / 7.0;
        series.color = SampleCvdPalette(discretePalette, tCol);

        const auto boundVars = detail_plot::CollectPlotBoundVars(env, vName);
        MathJsonNode ast{};
        const bool hasAst = ParseExpressionToMathJson(exprs[cIdx], ast, boundVars);
        std::vector<double> ys(numSamples, 0.0);
        std::vector<bool> oks(numSamples, false);
        double yMin = 1e18, yMax = -1e18;
        for (std::size_t i = 0; i < numSamples; ++i) {
            const double u = static_cast<double>(i) / static_cast<double>(numSamples - 1);
            const double x = xMin + u * (xMax - xMin);
            if (detail_plot::EvalScalarExpr(exprs[cIdx], hasAst ? &ast : nullptr, vName, x, env, ys[i])) {
                oks[i] = true;
                yMin = (std::min)(yMin, ys[i]);
                yMax = (std::max)(yMax, ys[i]);
            }
        }
        const double span = (std::max)(1.0, yMax - yMin);
        for (std::size_t i = 0; i < numSamples; ++i) {
            const double u = static_cast<double>(i) / static_cast<double>(numSamples - 1);
            const double x = xMin + u * (xMax - xMin);
            bool ok = oks[i];
            if (ok && i > 0 && oks[i - 1] && std::abs(ys[i] - ys[i - 1]) > 0.65 * span && std::abs(ys[i] - ys[i - 1]) > 20.0) {
                ok = false;
            }
            series.points.push_back({ x, ys[i], x, ok });
        }
        out.push_back(std::move(series));
    }
    return out;
}

inline std::vector<PlotCriticalPoint> FindRoots2D(
    const std::string& expr,
    const std::string& var,
    double xMin,
    double xMax,
    std::size_t scanSteps = 256,
    const SymbolEnv& env = {})
{
    std::vector<PlotCriticalPoint> roots;
    if (scanSteps < 4 || xMax <= xMin) return roots;
    const std::string vName = var.empty() ? "x" : var;
    const auto boundVars = detail_plot::CollectPlotBoundVars(env, vName);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast, boundVars);
    auto eval = [&](double x, double& y) {
        return detail_plot::EvalScalarExpr(expr, hasAst ? &ast : nullptr, vName, x, env, y);
    };

    const double dx = (xMax - xMin) / static_cast<double>(scanSteps);
    std::vector<double> xs(scanSteps + 1), ys(scanSteps + 1);
    std::vector<bool> ok(scanSteps + 1, false);
    for (std::size_t i = 0; i <= scanSteps; ++i) {
        xs[i] = xMin + static_cast<double>(i) * dx;
        ok[i] = eval(xs[i], ys[i]);
    }

    std::vector<bool> rootIsTangent;
    auto addRoot = [&](double rx, bool isTangent) {
        double ry = 0.0;
        if (!eval(rx, ry) || std::abs(ry) > 1e-4) return;
        const double tangentTol = (std::max)(1e-6, 2.0 * dx);
        for (std::size_t k = 0; k < roots.size(); ++k) {
            const double tol = (isTangent && rootIsTangent[k]) ? tangentTol : 1e-6;
            if (std::abs(roots[k].x - rx) < tol) return;
        }
        std::ostringstream lbl;
        lbl.precision(5);
        lbl << "Root (" << rx << ", 0)";
        roots.push_back({ CriticalPointKind::Root, rx, 0.0, lbl.str() });
        rootIsTangent.push_back(isTangent);
    };

    // Pass 1: Sign-change brackets refined by safeguarded bisection to 1e-11
    constexpr double kPhi = 0.6180339887498948482;
    std::vector<bool> inSignChange(scanSteps + 1, false);
    auto bisectBracket = [&](double a, double fa, double b) {
        for (int it = 0; it < 64; ++it) {
            const double mid = 0.5 * (a + b);
            double fmid = 0.0;
            if (!eval(mid, fmid)) break;
            if (std::abs(fmid) < 1e-12 || std::abs(b - a) < 1e-11) {
                a = b = mid;
                break;
            }
            if ((fa < 0.0 && fmid < 0.0) || (fa > 0.0 && fmid > 0.0)) {
                a = mid; fa = fmid;
            } else {
                b = mid;
            }
        }
        addRoot(0.5 * (a + b), false);
    };
    for (std::size_t i = 1; i <= scanSteps; ++i) {
        if (ok[i - 1] && ok[i] &&
            ((ys[i - 1] < 0.0 && ys[i] > 0.0) || (ys[i - 1] > 0.0 && ys[i] < 0.0))) {
            inSignChange[i - 1] = true;
            inSignChange[i] = true;
            bisectBracket(xs[i - 1], ys[i - 1], xs[i]);
        } else if (ok[i - 1] && ok[i] &&
                   (std::abs(ys[i - 1]) < 1e-4 || std::abs(ys[i]) < 1e-4) &&
                   !(ys[i - 1] == 0.0 && ys[i] == 0.0)) {
            // R3-06: Sub-probe cells touching a near-zero grid point so close crossing/tangent roots inside one cell are never missed
            constexpr std::size_t kSub = 16;
            std::vector<double> subXs;
            subXs.reserve(kSub + 3);
            subXs.push_back(xs[i - 1]);
            subXs.push_back(xs[i - 1] + 1e-4 * dx);
            for (std::size_t s = 1; s < kSub; ++s) {
                subXs.push_back(xs[i - 1] + (static_cast<double>(s) / static_cast<double>(kSub)) * dx);
            }
            subXs.push_back(xs[i] - 1e-4 * dx);
            subXs.push_back(xs[i]);
            std::vector<double> subYs(subXs.size(), 0.0);
            std::vector<bool> subOk(subXs.size(), false);
            subOk[0] = true; subYs[0] = ys[i - 1];
            subOk.back() = true; subYs.back() = ys[i];
            for (std::size_t m = 1; m + 1 < subXs.size(); ++m) {
                subOk[m] = eval(subXs[m], subYs[m]);
            }
            for (std::size_t m = 1; m < subXs.size(); ++m) {
                if (subOk[m - 1] && subOk[m] &&
                    ((subYs[m - 1] < 0.0 && subYs[m] > 0.0) || (subYs[m - 1] > 0.0 && subYs[m] < 0.0))) {
                    bisectBracket(subXs[m - 1], subYs[m - 1], subXs[m]);
                }
            }
            for (std::size_t m = 1; m + 1 < subXs.size(); ++m) {
                if (!subOk[m - 1] || !subOk[m] || !subOk[m + 1]) continue;
                if ((subYs[m - 1] < 0.0 && subYs[m] > 0.0) || (subYs[m - 1] > 0.0 && subYs[m] < 0.0) ||
                    (subYs[m] < 0.0 && subYs[m + 1] > 0.0) || (subYs[m] > 0.0 && subYs[m + 1] < 0.0)) {
                    continue;
                }
                const double a0 = std::abs(subYs[m - 1]), a1 = std::abs(subYs[m]), a2 = std::abs(subYs[m + 1]);
                if (a1 <= a0 && a1 <= a2 && (a1 < a0 || a1 < a2)) {
                    double a = subXs[m - 1], b = subXs[m + 1];
                    double c = b - kPhi * (b - a);
                    double d = a + kPhi * (b - a);
                    double fc = 0.0, fd = 0.0;
                    if (eval(c, fc) && eval(d, fd)) {
                        for (int it = 0; it < 64; ++it) {
                            if (std::abs(b - a) < 1e-12) break;
                            if (std::abs(fc) < std::abs(fd)) {
                                b = d; d = c; fd = fc;
                                c = b - kPhi * (b - a);
                                if (!eval(c, fc)) break;
                            } else {
                                a = c; c = d; fc = fd;
                                d = a + kPhi * (b - a);
                                if (!eval(d, fd)) break;
                            }
                        }
                        const double rx = 0.5 * (a + b);
                        double ry = 0.0;
                        if (eval(rx, ry) && std::abs(ry) < 1e-8) {
                            addRoot(rx, true);
                        }
                    }
                }
            }
        }
    }

    // Pass 2: Collapse contiguous |ys[i]| < 1e-10 grid runs (flat/quartic zero troughs) to a single minimum-|y| root
    std::size_t idx = 0;
    while (idx <= scanSteps) {
        if (!ok[idx] || std::abs(ys[idx]) >= 1e-10) {
            ++idx;
            continue;
        }
        const std::size_t runL = idx;
        while (idx + 1 <= scanSteps && ok[idx + 1] && std::abs(ys[idx + 1]) < 1e-10) {
            ++idx;
        }
        const std::size_t runR = idx;
        ++idx;

        bool hasSignChangeInRun = false;
        for (std::size_t k = runL; k <= runR; ++k) {
            if (inSignChange[k]) {
                hasSignChangeInRun = true;
                break;
            }
        }
        if (hasSignChangeInRun) continue;

        std::size_t bestIdx = runL;
        double bestAbs = std::abs(ys[runL]);
        for (std::size_t k = runL + 1; k <= runR; ++k) {
            const double ay = std::abs(ys[k]);
            if (ay < bestAbs) {
                bestAbs = ay;
                bestIdx = k;
            }
        }
        std::size_t tieR = bestIdx;
        while (tieR + 1 <= runR && std::abs(ys[tieR + 1]) == bestAbs) {
            ++tieR;
        }
        const std::size_t midBest = bestIdx + (tieR - bestIdx) / 2;
        double candX = xs[midBest];

        if (bestAbs > 0.0 && bestIdx > 0 && ok[bestIdx - 1] && tieR < scanSteps && ok[tieR + 1]) {
            double a = xs[bestIdx - 1], b = xs[tieR + 1];
            double c = b - kPhi * (b - a);
            double d = a + kPhi * (b - a);
            double fc = 0.0, fd = 0.0;
            if (eval(c, fc) && eval(d, fd)) {
                for (int it = 0; it < 64; ++it) {
                    if (std::abs(b - a) < 1e-12) break;
                    if (std::abs(fc) < std::abs(fd)) {
                        b = d;
                        d = c; fd = fc;
                        c = b - kPhi * (b - a);
                        if (!eval(c, fc)) break;
                    } else {
                        a = c;
                        c = d; fc = fd;
                        d = a + kPhi * (b - a);
                        if (!eval(d, fd)) break;
                    }
                }
                const double rx = 0.5 * (a + b);
                double ry = 0.0;
                if (eval(rx, ry) && std::abs(ry) < bestAbs + 1e-12) {
                    candX = rx;
                }
            }
        }
        double ry = 0.0;
        if (eval(candX, ry) && std::abs(ry) < 1e-8) {
            bool isTangent = true;
            if (runL > 0 && ok[runL - 1] && runR < scanSteps && ok[runR + 1] &&
                ((ys[runL - 1] < 0.0 && ys[runR + 1] > 0.0) || (ys[runL - 1] > 0.0 && ys[runR + 1] < 0.0))) {
                isTangent = false;
            } else {
                const double eps = (std::min)(0.25 * dx, (std::max)(1e-7, 1e-3 * dx));
                if (candX - eps >= xMin && candX + eps <= xMax) {
                    double yL = 0.0, yR = 0.0;
                    if (eval(candX - eps, yL) && eval(candX + eps, yR) &&
                        ((yL < -1e-15 && yR > 1e-15) || (yL > 1e-15 && yR < -1e-15))) {
                        isTangent = false;
                    }
                }
            }
            addRoot(candX, isTangent);
        }
    }

    // Pass 3: Even-multiplicity tangent roots outside the |y| < 1e-10 trough
    for (std::size_t i = 1; i + 1 <= scanSteps; ++i) {
        if (!ok[i - 1] || !ok[i] || !ok[i + 1]) continue;
        if (std::abs(ys[i]) < 1e-10 || inSignChange[i]) continue;
        const double a0 = std::abs(ys[i - 1]), a1 = std::abs(ys[i]), a2 = std::abs(ys[i + 1]);
        if (a1 <= a0 && a1 <= a2 && (a1 < a0 || a1 < a2)) {
            double a = xs[i - 1], b = xs[i + 1];
            double c = b - kPhi * (b - a);
            double d = a + kPhi * (b - a);
            double fc = 0.0, fd = 0.0;
            if (!eval(c, fc) || !eval(d, fd)) continue;
            for (int it = 0; it < 64; ++it) {
                if (std::abs(b - a) < 1e-12) break;
                if (std::abs(fc) < std::abs(fd)) {
                    b = d;
                    d = c; fd = fc;
                    c = b - kPhi * (b - a);
                    if (!eval(c, fc)) break;
                } else {
                    a = c;
                    c = d; fc = fd;
                    d = a + kPhi * (b - a);
                    if (!eval(d, fd)) break;
                }
            }
            const double rx = 0.5 * (a + b);
            double ry = 0.0;
            if (eval(rx, ry) && std::abs(ry) < 1e-8) {
                addRoot(rx, true);
            }
        }
    }
    std::sort(roots.begin(), roots.end(), [](const PlotCriticalPoint& lhs, const PlotCriticalPoint& rhs) {
        return lhs.x < rhs.x;
    });
    return roots;
}

inline std::vector<PlotCriticalPoint> FindExtrema2D(
    const std::string& expr,
    const std::string& var,
    double xMin,
    double xMax,
    std::size_t scanSteps = 256,
    const SymbolEnv& env = {})
{
    std::vector<PlotCriticalPoint> extrema;
    if (scanSteps < 6 || xMax <= xMin) return extrema;
    const std::string vName = var.empty() ? "x" : var;
    const auto boundVars = detail_plot::CollectPlotBoundVars(env, vName);
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast, boundVars);
    auto eval = [&](double x, double& y) {
        return detail_plot::EvalScalarExpr(expr, hasAst ? &ast : nullptr, vName, x, env, y);
    };

    const double dx = (xMax - xMin) / static_cast<double>(scanSteps);
    std::vector<double> xs(scanSteps + 1), ys(scanSteps + 1);
    std::vector<bool> ok(scanSteps + 1, false);
    for (std::size_t i = 0; i <= scanSteps; ++i) {
        xs[i] = xMin + static_cast<double>(i) * dx;
        ok[i] = eval(xs[i], ys[i]);
    }

    constexpr double kPhi = 0.6180339887498948482;
    for (std::size_t i = 1; i + 1 <= scanSteps; ++i) {
        if (!ok[i - 1] || !ok[i] || !ok[i + 1]) continue;
        const bool isMinBracket = (ys[i] <= ys[i - 1] && ys[i] <= ys[i + 1] && (ys[i] < ys[i - 1] || ys[i] < ys[i + 1]));
        const bool isMaxBracket = (ys[i] >= ys[i - 1] && ys[i] >= ys[i + 1] && (ys[i] > ys[i - 1] || ys[i] > ys[i + 1]));
        if (!isMinBracket && !isMaxBracket) continue;

        double a = xs[i - 1], b = xs[i + 1];
        double c = b - kPhi * (b - a);
        double d = a + kPhi * (b - a);
        double fc = 0.0, fd = 0.0;
        if (!eval(c, fc) || !eval(d, fd)) continue;
        for (int it = 0; it < 55; ++it) {
            if (std::abs(b - a) < 1e-11) break;
            const bool pickLeft = isMinBracket ? (fc < fd) : (fc > fd);
            if (pickLeft) {
                b = d; d = c; fd = fc;
                c = b - kPhi * (b - a);
                if (!eval(c, fc)) break;
            } else {
                a = c; c = d; fc = fd;
                d = a + kPhi * (b - a);
                if (!eval(d, fd)) break;
            }
        }
        const double rx = 0.5 * (a + b);
        double ry = 0.0;
        if (!eval(rx, ry)) continue;
        bool dup = false;
        for (const auto& ex : extrema) {
            if (std::abs(ex.x - rx) < 1e-6) { dup = true; break; }
        }
        if (dup) continue;
        const CriticalPointKind kind = isMinBracket ? CriticalPointKind::LocalMin : CriticalPointKind::LocalMax;
        std::ostringstream lbl;
        lbl.precision(5);
        lbl << (kind == CriticalPointKind::LocalMin ? "Min (" : "Max (") << rx << ", " << ry << ")";
        extrema.push_back({ kind, rx, ry, lbl.str() });
    }
    return extrema;
}

inline std::vector<PlotCriticalPoint> FindIntersections2D(
    const std::string& exprA,
    const std::string& exprB,
    const std::string& var,
    double xMin,
    double xMax,
    std::size_t scanSteps = 256,
    const SymbolEnv& env = {})
{
    const std::string diffExpr = "(" + exprA + ") - (" + exprB + ")";
    auto roots = FindRoots2D(diffExpr, var, xMin, xMax, scanSteps, env);
    const std::string vName = var.empty() ? "x" : var;
    const auto boundVars = detail_plot::CollectPlotBoundVars(env, vName);
    MathJsonNode astA{};
    const bool hasA = ParseExpressionToMathJson(exprA, astA, boundVars);
    for (auto& pt : roots) {
        pt.kind = CriticalPointKind::Intersection;
        double yVal = 0.0;
        if (detail_plot::EvalScalarExpr(exprA, hasA ? &astA : nullptr, vName, pt.x, env, yVal)) {
            pt.y = yVal;
        }
        std::ostringstream lbl;
        lbl.precision(5);
        lbl << "Intersect (" << pt.x << ", " << pt.y << ")";
        pt.labelUtf8 = lbl.str();
    }
    return roots;
}

inline ContourPlotResult ComputeContourIsoLevels(
    const std::string& expr,
    double xMin, double xMax, std::size_t nx,
    double yMin, double yMax, std::size_t ny,
    int numLevels = 10,
    CvdPaletteKind palette = CvdPaletteKind::Cividis,
    const SymbolEnv& env = {})
{
    ContourPlotResult res;
    if (nx < 2 || ny < 2 || xMax <= xMin || yMax <= yMin || numLevels < 1) {
        res.errorUtf8 = "Parametros de contorno invalidos";
        return res;
    }

    const auto boundVars = detail_plot::CollectPlotBoundVars(env, "x", "y");
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast, boundVars);
    std::vector<double> z(nx * ny, 0.0);
    std::vector<bool> ok(nx * ny, false);
    double zMin = 1e18, zMax = -1e18;
    std::size_t validCount = 0;

    for (std::size_t j = 0; j < ny; ++j) {
        const double y = yMin + (static_cast<double>(j) / static_cast<double>(ny - 1)) * (yMax - yMin);
        for (std::size_t i = 0; i < nx; ++i) {
            const double x = xMin + (static_cast<double>(i) / static_cast<double>(nx - 1)) * (xMax - xMin);
            double zv = 0.0;
            if (detail_plot::EvalScalarXY(expr, hasAst ? &ast : nullptr, x, y, env, zv)) {
                z[j * nx + i] = zv;
                ok[j * nx + i] = true;
                zMin = (std::min)(zMin, zv);
                zMax = (std::max)(zMax, zv);
                ++validCount;
            }
        }
    }
    if (validCount < 4) {
        res.errorUtf8 = "No se pudieron evaluar suficientes puntos para contorno";
        return res;
    }

    res.ok = true;
    res.zMin = zMin;
    res.zMax = zMax;
    res.colorbar = BuildColorbarMetadata(palette, zMin, zMax, expr);

    const double span = zMax - zMin;
    if (span < 1e-14) return res;

    const double dx = (xMax - xMin) / static_cast<double>(nx - 1);
    const double dy = (yMax - yMin) / static_cast<double>(ny - 1);

    for (int lvlIdx = 0; lvlIdx < numLevels; ++lvlIdx) {
        const double u = static_cast<double>(lvlIdx + 1) / static_cast<double>(numLevels + 1);
        const double level = zMin + u * span;
        ContourIsoLevel iso;
        iso.level = level;
        iso.color = SampleCvdPalette(palette, u);

        for (std::size_t j = 0; j + 1 < ny; ++j) {
            const double y0 = yMin + static_cast<double>(j) * dy;
            const double y1 = y0 + dy;
            for (std::size_t i = 0; i + 1 < nx; ++i) {
                const std::size_t i00 = j * nx + i;
                const std::size_t i10 = j * nx + (i + 1);
                const std::size_t i11 = (j + 1) * nx + (i + 1);
                const std::size_t i01 = (j + 1) * nx + i;
                if (!ok[i00] || !ok[i10] || !ok[i11] || !ok[i01]) continue;

                const double z00 = z[i00], z10 = z[i10], z11 = z[i11], z01 = z[i01];
                int mask = 0;
                if (z00 >= level) mask |= 1;
                if (z10 >= level) mask |= 2;
                if (z11 >= level) mask |= 4;
                if (z01 >= level) mask |= 8;
                if (mask == 0 || mask == 15) continue;

                const double x0 = xMin + static_cast<double>(i) * dx;
                const double x1 = x0 + dx;

                auto interp = [&](int edge, double& px, double& py) {
                    auto frac = [&](double a, double b) {
                        const double d = b - a;
                        return (std::abs(d) < 1e-15) ? 0.5 : std::clamp((level - a) / d, 0.0, 1.0);
                    };
                    if (edge == 0) { px = x0 + frac(z00, z10) * dx; py = y0; }
                    else if (edge == 1) { px = x1; py = y0 + frac(z10, z11) * dy; }
                    else if (edge == 2) { px = x0 + frac(z01, z11) * dx; py = y1; }
                    else { px = x0; py = y0 + frac(z00, z01) * dy; }
                };
                auto emitEdgePair = [&](int eA, int eB) {
                    ContourSegment2D seg;
                    interp(eA, seg.x0, seg.y0);
                    interp(eB, seg.x1, seg.y1);
                    iso.segments.push_back(seg);
                };

                switch (mask) {
                case 1: case 14: emitEdgePair(3, 0); break;
                case 2: case 13: emitEdgePair(0, 1); break;
                case 3: case 12: emitEdgePair(3, 1); break;
                case 4: case 11: emitEdgePair(1, 2); break;
                case 6: case 9:  emitEdgePair(0, 2); break;
                case 7: case 8:  emitEdgePair(3, 2); break;
                case 5: {
                    const double zCenter = 0.25 * (z00 + z10 + z11 + z01);
                    if (zCenter >= level) { emitEdgePair(3, 2); emitEdgePair(0, 1); }
                    else { emitEdgePair(3, 0); emitEdgePair(1, 2); }
                    break;
                }
                case 10: {
                    const double zCenter = 0.25 * (z00 + z10 + z11 + z01);
                    if (zCenter >= level) { emitEdgePair(3, 0); emitEdgePair(1, 2); }
                    else { emitEdgePair(3, 2); emitEdgePair(0, 1); }
                    break;
                }
                default: break;
                }
            }
        }
        res.levels.push_back(std::move(iso));
    }
    return res;
}

inline Surface3DProjection BuildProjectedSurface3D(
    const std::string& expr,
    const Surface3DViewState& view,
    const SymbolEnv& env = {})
{
    Surface3DProjection proj;
    const int n = std::clamp(view.gridSteps, 4, 60);
    if (view.xMax <= view.xMin || view.yMax <= view.yMin) {
        proj.errorUtf8 = "Dominio 3D invalido";
        return proj;
    }

    const auto boundVars = detail_plot::CollectPlotBoundVars(env, "x", "y");
    MathJsonNode ast{};
    const bool hasAst = ParseExpressionToMathJson(expr, ast, boundVars);
    const std::size_t nu = static_cast<std::size_t>(n);
    std::vector<double> zGrid(nu * nu, 0.0);
    std::vector<bool> okGrid(nu * nu, false);
    double zMin = 1e18, zMax = -1e18;
    std::size_t validCount = 0;

    for (std::size_t j = 0; j < nu; ++j) {
        const double y = view.yMin + (static_cast<double>(j) / static_cast<double>(nu - 1)) * (view.yMax - view.yMin);
        for (std::size_t i = 0; i < nu; ++i) {
            const double x = view.xMin + (static_cast<double>(i) / static_cast<double>(nu - 1)) * (view.xMax - view.xMin);
            double zv = 0.0;
            if (detail_plot::EvalScalarXY(expr, hasAst ? &ast : nullptr, x, y, env, zv)) {
                zGrid[j * nu + i] = zv;
                okGrid[j * nu + i] = true;
                zMin = (std::min)(zMin, zv);
                zMax = (std::max)(zMax, zv);
                ++validCount;
            }
        }
    }
    if (validCount < 4) {
        proj.errorUtf8 = "No se pudo evaluar la superficie 3D";
        return proj;
    }

    if (!view.autoZ && view.zMax > view.zMin) {
        zMin = view.zMin;
        zMax = view.zMax;
    }
    const double zSpan = (std::max)(1e-9, zMax - zMin);

    constexpr double kPi = 3.14159265358979323846;
    const double yaw = view.yawDeg * kPi / 180.0;
    const double pitch = view.pitchDeg * kPi / 180.0;
    const double cy = std::cos(yaw), sy = std::sin(yaw);
    const double cp = std::cos(pitch), sp = std::sin(pitch);
    const double zm = std::clamp(view.zoom, 0.25, 4.0);

    auto projectPt = [&](double nx, double ny, double nz, double& outX, double& outY, double& outDepth) {
        const double rx = nx * cy - ny * sy;
        const double ry = nx * sy + ny * cy;
        const double py = ry * cp - nz * sp;
        const double pz = ry * sp + nz * cp;
        outX = 0.5 + 0.30 * zm * rx;
        outY = 0.54 - 0.30 * zm * pz;
        outDepth = py;
    };

    for (std::size_t j = 0; j < nu; ++j) {
        const double ny = -1.0 + 2.0 * (static_cast<double>(j) / static_cast<double>(nu - 1));
        for (std::size_t i = 0; i < nu; ++i) {
            if (!okGrid[j * nu + i]) continue;
            const double nx = -1.0 + 2.0 * (static_cast<double>(i) / static_cast<double>(nu - 1));
            const double t0 = std::clamp((zGrid[j * nu + i] - zMin) / zSpan, 0.0, 1.0);
            const double nz0 = -0.8 + 1.6 * t0;
            double ax = 0.0, ay = 0.0, ad = 0.0;
            projectPt(nx, ny, nz0, ax, ay, ad);

            if (i + 1 < nu && okGrid[j * nu + (i + 1)]) {
                const double nx1 = -1.0 + 2.0 * (static_cast<double>(i + 1) / static_cast<double>(nu - 1));
                const double t1 = std::clamp((zGrid[j * nu + (i + 1)] - zMin) / zSpan, 0.0, 1.0);
                double bx = 0.0, by = 0.0, bd = 0.0;
                projectPt(nx1, ny, -0.8 + 1.6 * t1, bx, by, bd);
                proj.segments.push_back({ ax, ay, bx, by, 0.5 * (ad + bd), SampleCvdPalette(view.palette, 0.5 * (t0 + t1)), false, "" });
            }
            if (j + 1 < nu && okGrid[(j + 1) * nu + i]) {
                const double ny1 = -1.0 + 2.0 * (static_cast<double>(j + 1) / static_cast<double>(nu - 1));
                const double t1 = std::clamp((zGrid[(j + 1) * nu + i] - zMin) / zSpan, 0.0, 1.0);
                double bx = 0.0, by = 0.0, bd = 0.0;
                projectPt(nx, ny1, -0.8 + 1.6 * t1, bx, by, bd);
                proj.segments.push_back({ ax, ay, bx, by, 0.5 * (ad + bd), SampleCvdPalette(view.palette, 0.5 * (t0 + t1)), false, "" });
            }
        }
    }

    if (view.showAxes) {
        auto addAxis = [&](double x0, double y0, double z0, double x1, double y1, double z1, const std::string& lbl) {
            double ax, ay, ad, bx, by, bd;
            projectPt(x0, y0, z0, ax, ay, ad);
            projectPt(x1, y1, z1, bx, by, bd);
            proj.segments.push_back({ ax, ay, bx, by, 0.5 * (ad + bd) - 10.0, { 100, 116, 139, 220 }, true, lbl });
        };
        addAxis(-1.0, -1.0, -0.8, 1.15, -1.0, -0.8, "X");
        addAxis(-1.0, -1.0, -0.8, -1.0, 1.15, -0.8, "Y");
        addAxis(-1.0, -1.0, -0.8, -1.0, -1.0, 0.95, "Z");
    }

    std::sort(proj.segments.begin(), proj.segments.end(), [](const Surface3DSegment& a, const Surface3DSegment& b) {
        return a.depth < b.depth;
    });

    if (view.showBaseContours) {
        const auto cnt = ComputeContourIsoLevels(expr, view.xMin, view.xMax, nu, view.yMin, view.yMax, nu, 6, view.palette, env);
        if (cnt.ok) proj.baseContours = cnt.levels;
    }

    proj.ok = true;
    proj.zMin = zMin;
    proj.zMax = zMax;
    proj.colorbar = BuildColorbarMetadata(view.palette, zMin, zMax, expr);
    return proj;
}

inline std::vector<SheetSegment> GenerateParametricSheet(const SheetSpec& spec) {
    std::vector<SheetSegment> segs;
    if (spec.width <= 0.0 || spec.height <= 0.0) return segs;
    const double w = spec.width;
    const double h = spec.height;
    constexpr double kPi = 3.14159265358979323846;

    auto clipLineToBox = [&](double x0, double y0, double x1, double y1, SheetSegment& out) -> bool {
        double t0 = 0.0, t1 = 1.0;
        const double dx = x1 - x0, dy = y1 - y0;
        auto clipTest = [&](double p, double q) -> bool {
            if (std::abs(p) < 1e-12) return q >= 0.0;
            const double r = q / p;
            if (p < 0.0) { if (r > t1) return false; if (r > t0) t0 = r; }
            else { if (r < t0) return false; if (r < t1) t1 = r; }
            return true;
        };
        if (clipTest(-dx, x0) && clipTest(dx, w - x0) && clipTest(-dy, y0) && clipTest(dy, h - y0)) {
            out.x0 = x0 + t0 * dx;
            out.y0 = y0 + t0 * dy;
            out.x1 = x0 + t1 * dx;
            out.y1 = y0 + t1 * dy;
            return true;
        }
        return false;
    };

    switch (spec.kind) {
    case ParametricSheetKind::Millimetric:
    case ParametricSheetKind::Engineering: {
        const double step = (spec.kind == ParametricSheetKind::Millimetric)
            ? (std::max)(1.0, spec.pxPerMm)
            : (std::max)(2.0, spec.pxPerMm * 2.5);
        int ix = 0;
        for (double x = 0.0; x <= w + 0.01; x += step, ++ix) {
            segs.push_back({ x, 0.0, x, h, (ix % 10 == 0), (ix % 5 == 0 && ix % 10 != 0), "" });
        }
        int iy = 0;
        for (double y = 0.0; y <= h + 0.01; y += step, ++iy) {
            segs.push_back({ 0.0, y, w, y, (iy % 10 == 0), (iy % 5 == 0 && iy % 10 != 0), "" });
        }
        break;
    }
    case ParametricSheetKind::SemiLogX:
    case ParametricSheetKind::SemiLogY:
    case ParametricSheetKind::LogLog: {
        const bool logX = (spec.kind == ParametricSheetKind::SemiLogX || spec.kind == ParametricSheetKind::LogLog);
        const bool logY = (spec.kind == ParametricSheetKind::SemiLogY || spec.kind == ParametricSheetKind::LogLog);
        if (logX) {
            const int xDec = (std::max)(1, spec.xDecades);
            const double decW = w / static_cast<double>(xDec);
            for (int d = 0; d < xDec; ++d) {
                for (int m = 1; m <= 9; ++m) {
                    const double x = static_cast<double>(d) * decW + decW * std::log10(static_cast<double>(m));
                    std::string lbl = (m == 1) ? ("1e" + std::to_string(spec.xStartExp + d)) : "";
                    segs.push_back({ x, 0.0, x, h, (m == 1), (m == 5), lbl });
                }
            }
            segs.push_back({ w, 0.0, w, h, true, false, "1e" + std::to_string(spec.xStartExp + xDec) });
        } else {
            const double step = (std::max)(4.0, spec.stepPx);
            int ix = 0;
            for (double x = 0.0; x <= w + 0.01; x += step, ++ix) {
                segs.push_back({ x, 0.0, x, h, (ix % 5 == 0), false, "" });
            }
        }
        if (logY) {
            const int yDec = (std::max)(1, spec.yDecades);
            const double decH = h / static_cast<double>(yDec);
            for (int d = 0; d < yDec; ++d) {
                for (int m = 1; m <= 9; ++m) {
                    const double y = h - (static_cast<double>(d) * decH + decH * std::log10(static_cast<double>(m)));
                    std::string lbl = (m == 1) ? ("1e" + std::to_string(spec.yStartExp + d)) : "";
                    segs.push_back({ 0.0, y, w, y, (m == 1), (m == 5), lbl });
                }
            }
            segs.push_back({ 0.0, 0.0, w, 0.0, true, false, "1e" + std::to_string(spec.yStartExp + yDec) });
        } else {
            const double step = (std::max)(4.0, spec.stepPx);
            int iy = 0;
            for (double y = 0.0; y <= h + 0.01; y += step, ++iy) {
                segs.push_back({ 0.0, y, w, y, (iy % 5 == 0), false, "" });
            }
        }
        break;
    }
    case ParametricSheetKind::Polar: {
        const double cx = w * 0.5, cy = h * 0.5;
        const double maxR = 0.46 * (std::min)(w, h);
        const int rings = (std::max)(1, spec.polarRings);
        const int segCount = 72;
        for (int rIdx = 1; rIdx <= rings; ++rIdx) {
            const double r = maxR * (static_cast<double>(rIdx) / static_cast<double>(rings));
            const bool isOuter = (rIdx == rings);
            for (int s = 0; s < segCount; ++s) {
                const double a0 = (2.0 * kPi * s) / segCount;
                const double a1 = (2.0 * kPi * (s + 1)) / segCount;
                segs.push_back({ cx + r * std::cos(a0), cy + r * std::sin(a0),
                                 cx + r * std::cos(a1), cy + r * std::sin(a1), isOuter, false, "" });
            }
        }
        const double stepDeg = std::clamp(spec.polarAngleStepDeg, 5.0, 90.0);
        for (double deg = 0.0; deg < 360.0 - 1e-6; deg += stepDeg) {
            const double rad = deg * kPi / 180.0;
            const bool cardinal = (std::abs(std::fmod(deg, 90.0)) < 1e-6);
            segs.push_back({ cx, cy, cx + maxR * std::cos(rad), cy - maxR * std::sin(rad), cardinal, false, std::to_string(static_cast<int>(std::llround(deg))) });
        }
        break;
    }
    case ParametricSheetKind::SmithChart: {
        const double cx = w * 0.5, cy = h * 0.5;
        const double R = (std::max)(10.0, 0.46 * (std::min)(w, h));
        // Outer unit circle |Gamma| = 1 (r = 0)
        constexpr int kCircleSegs = 96;
        for (int i = 0; i < kCircleSegs; ++i) {
            const double a0 = (2.0 * kPi * i) / kCircleSegs;
            const double a1 = (2.0 * kPi * (i + 1)) / kCircleSegs;
            segs.push_back({ cx + R * std::cos(a0), cy + R * std::sin(a0),
                             cx + R * std::cos(a1), cy + R * std::sin(a1), true, false, "" });
        }
        // Horizontal real axis
        segs.push_back({ cx - R, cy, cx + R, cy, true, false, "Im=0" });

        // Constant-resistance circles: center (cx + R*r/(1+r), cy), radius R/(1+r)
        for (double rNorm : spec.smithRValues) {
            if (rNorm <= 0.0) continue;
            const double rr = R / (1.0 + rNorm);
            const double ox = cx + R - rr;
            const bool isUnity = (std::abs(rNorm - 1.0) < 1e-6);
            for (int i = 0; i < kCircleSegs; ++i) {
                const double a0 = (2.0 * kPi * i) / kCircleSegs;
                const double a1 = (2.0 * kPi * (i + 1)) / kCircleSegs;
                segs.push_back({ ox + rr * std::cos(a0), cy + rr * std::sin(a0),
                                 ox + rr * std::cos(a1), cy + rr * std::sin(a1), isUnity, false, "" });
            }
        }

        // Constant-reactance arcs (+x and -x), STRICTLY clipped to unit disk (x-cx)^2 + (y-cy)^2 <= R^2
        for (double xNorm : spec.smithXValues) {
            if (xNorm <= 0.0) continue;
            const double rx = R / xNorm;
            for (int sign : { 1, -1 }) {
                const double ox = cx + R;
                const double oy = cy - sign * rx; // +x in upper half (y < cy in screen coords)
                // Circle-circle intersection of (cx, cy, R) and (ox, oy, rx)
                // Parametrize along reactance circle in angles that fall inside unit disk
                constexpr int kArcSteps = 128;
                bool havePrev = false;
                double px = 0.0, py = 0.0;
                for (int k = 0; k <= kArcSteps; ++k) {
                    const double ang = (2.0 * kPi * k) / kArcSteps;
                    const double xPt = ox + rx * std::cos(ang);
                    const double yPt = oy + rx * std::sin(ang);
                    const double dist2 = (xPt - cx) * (xPt - cx) + (yPt - cy) * (yPt - cy);
                    const bool inside = (dist2 <= R * R + 1e-3);
                    if (inside) {
                        if (havePrev) {
                            segs.push_back({ px, py, xPt, yPt, (std::abs(xNorm - 1.0) < 1e-6), false, "" });
                        }
                        px = xPt;
                        py = yPt;
                        havePrev = true;
                    } else {
                        havePrev = false;
                    }
                }
            }
        }
        break;
    }
    case ParametricSheetKind::Cartesian: {
        const double step = (std::max)(4.0, spec.stepPx);
        int ix = 0;
        for (double x = 0.0; x <= w + 0.01; x += step, ++ix) {
            segs.push_back({ x, 0.0, x, h, false, (ix % 5 == 0), "" });
        }
        int iy = 0;
        for (double y = 0.0; y <= h + 0.01; y += step, ++iy) {
            segs.push_back({ 0.0, y, w, y, false, (iy % 5 == 0), "" });
        }
        segs.push_back({ 0.0, h * 0.5, w, h * 0.5, true, false, "X" });
        segs.push_back({ w * 0.5, 0.0, w * 0.5, h, true, false, "Y" });
        break;
    }
    case ParametricSheetKind::Isometric: {
        const double step = (std::max)(4.0, spec.stepPx);
        for (double y = 0.0; y <= h + 0.01; y += step) {
            segs.push_back({ 0.0, y, w, y, false, false, "" });
        }
        const double m = std::sqrt(3.0);
        const double cStep = step / std::cos(kPi / 6.0);
        const double cMin1 = -m * w;
        const double cMax1 = h;
        for (double c = std::floor(cMin1 / cStep) * cStep; c <= cMax1; c += cStep) {
            SheetSegment s{};
            if (clipLineToBox(0.0, c, w, m * w + c, s)) segs.push_back(s);
        }
        const double cMin2 = 0.0;
        const double cMax2 = h + m * w;
        for (double c = std::floor(cMin2 / cStep) * cStep; c <= cMax2; c += cStep) {
            SheetSegment s{};
            if (clipLineToBox(0.0, c, w, -m * w + c, s)) segs.push_back(s);
        }
        break;
    }
    }
    return segs;
}

} // namespace DocStudio::Cas
