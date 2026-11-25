#pragma once
// ============================================================================
// ui_glass/translucent_math.hpp
// Physically based material translúcido 2.0 optical engine (guías de interfaz + macOS 27 Golden Gate)
// Pure C++17 analytical SDF, C1 quadratic smin, 3D elliptical bevel normals,
// spectral Snell RGB refraction (IOR 1.510 / 1.522 / 1.535), Schlick Fresnel,
// Dual Kawase blur pyramid, +15% vibrancy boost, opposing @1/@2 specular rims,
// adaptive p15/p85 luminance histogram, and UserOpacitySlider (0.0..1.0).
// ============================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace UiGlass {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct SdfSample {
    float dist = 0.0f; // < 0 inside, = 0 boundary, > 0 outside
    float gx = 0.0f;   // analytical unit normal x
    float gy = -1.0f;  // analytical unit normal y
};

inline float Clamp01(float v) noexcept {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

inline float Lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

inline float Smoothstep(float edge0, float edge1, float x) noexcept {
    const float denom = edge1 - edge0;
    if (std::fabs(denom) < 1e-6f) return (x >= edge1) ? 1.0f : 0.0f;
    const float t = Clamp01((x - edge0) / denom);
    return t * t * (3.0f - 2.0f * t);
}

inline Vec3 Normalize3(Vec3 v) noexcept {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f) return { 0.0f, 0.0f, 1.0f };
    const float inv = 1.0f / len;
    return { v.x * inv, v.y * inv, v.z * inv };
}

inline float Dot3(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// guías de interfaz concentric geometry: r_inner = max(minR, r_outer - padding)
inline float ComputeConcentricRadius(float outerRadius, float padding, float minRadius = 4.0f) noexcept {
    const float r = outerRadius - padding;
    if (r <= 0.0f) return 0.0f;
    return (std::max)((std::min)(minRadius, outerRadius), r);
}

// Exact analytical signed distance + unit gradient for rounded box / capsule
// Verified error vs finite difference < 1.71e-8 (Arquitectura material translúcido y HIG §5.3A)
inline SdfSample SdgRoundBox(Vec2 p, Vec2 halfSize, float radius) noexcept {
    const float r = (std::max)(0.5f, (std::min)(radius, (std::min)(halfSize.x, halfSize.y)));
    const float sx = (p.x >= 0.0f) ? 1.0f : -1.0f;
    const float sy = (p.y >= 0.0f) ? 1.0f : -1.0f;
    const float qx = std::fabs(p.x) - halfSize.x + r;
    const float qy = std::fabs(p.y) - halfSize.y + r;
    const float w = (std::max)(qx, qy);
    const float outX = (std::max)(qx, 0.0f);
    const float outY = (std::max)(qy, 0.0f);
    const float lenOut = std::sqrt(outX * outX + outY * outY);

    const float d = ((w > 0.0f) ? lenOut : w) - r;
    float gx = 0.0f, gy = 0.0f;
    if (w > 0.0f) {
        const float denom = (std::max)(lenOut, 1e-6f);
        gx = (outX / denom) * sx;
        gy = (outY / denom) * sy;
    } else if (qx > qy) {
        gx = sx;
        gy = 0.0f;
    } else {
        gx = 0.0f;
        gy = sy;
    }
    return { d, gx, gy };
}

// iPadOS continuous squircle (superellipse n = 4)
inline float SdSquircle(Vec2 p, Vec2 halfSize) noexcept {
    const float bx = (std::max)(1.0f, halfSize.x);
    const float by = (std::max)(1.0f, halfSize.y);
    const float nx = std::fabs(p.x / bx);
    const float ny = std::fabs(p.y / by);
    const float n4 = nx * nx * nx * nx + ny * ny * ny * ny;
    return (std::pow((std::max)(n4, 1e-12f), 0.25f) - 1.0f) * (std::min)(bx, by);
}

// C1 quadratic smooth union with analytical gradient propagation (GlassEffectContainer morphing)
inline SdfSample SminQuadraticGrad(SdfSample a, SdfSample b, float k) noexcept {
    const float kk = (std::max)(k, 1e-4f);
    const float h = Clamp01(0.5f + 0.5f * (b.dist - a.dist) / kk);
    const float d = Lerp(b.dist, a.dist, h) - kk * h * (1.0f - h);
    const float gx = Lerp(b.gx, a.gx, h);
    const float gy = Lerp(b.gy, a.gy, h);
    const float gLen = std::sqrt(gx * gx + gy * gy);
    if (gLen > 1e-5f) {
        return { d, gx / gLen, gy / gLen };
    }
    return { d, 0.0f, 1.0f };
}

// Optical parameters matching HLSL GlassConstants (Arquitectura material translúcido y HIG §5.4)
struct OpticalGlassParams {
    float cornerRadius = 18.0f;
    float bevelWidth = 18.0f;        // W in px
    float bevelHeight = 10.0f;       // H in px
    float baseThickness = 14.0f;     // T_base in px
    float userOpacitySlider = 0.36f; // macOS 27 Golden Gate: 0.0 Ultra-Clear .. 1.0 Frosted Opaque
    float tintR = 1.0f;              // Linear-ish normalized [0..1]
    float tintG = 1.0f;
    float tintB = 1.0f;
    float tintProminence = 0.0f;     // 0.0 regular .. 0.65 prominentGlass
    float iorR = 1.510f;             // Spectral IOR for glass chromatic edge dispersion
    float iorG = 1.522f;
    float iorB = 1.535f;
    bool darkMode = false;
    bool highContrastBorder = false; // Crisp 1px accessibility border
    bool reduceTransparency = false;
    int blurPasses = 2;              // Dual Kawase pyramid depth (1..3)
};

// Apply +15% vibrancy saturation boost to BGRA32 buffer prior to blur
inline void BoostSaturationBgra32(std::uint8_t* bgra, int w, int h, float satFactor = 1.15f) noexcept {
    if (!bgra || w <= 0 || h <= 0) return;
    const std::size_t count = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t o = i * 4u;
        const float b = static_cast<float>(bgra[o + 0]);
        const float g = static_cast<float>(bgra[o + 1]);
        const float r = static_cast<float>(bgra[o + 2]);
        const float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        const float nr = std::clamp(luma + (r - luma) * satFactor, 0.0f, 255.0f);
        const float ng = std::clamp(luma + (g - luma) * satFactor, 0.0f, 255.0f);
        const float nb = std::clamp(luma + (b - luma) * satFactor, 0.0f, 255.0f);
        bgra[o + 0] = static_cast<std::uint8_t>(nb + 0.5f);
        bgra[o + 1] = static_cast<std::uint8_t>(ng + 0.5f);
        bgra[o + 2] = static_cast<std::uint8_t>(nr + 0.5f);
    }
}

// Compute p15 and p85 luminance percentiles (0..255) for adaptive contrast
inline void ComputeLumaPercentilesBgra32(
    const std::uint8_t* bgra, int w, int h, float& outP15, float& outP85) noexcept
{
    outP15 = 128.0f;
    outP85 = 128.0f;
    if (!bgra || w <= 0 || h <= 0) return;
    std::array<std::uint32_t, 256> hist{};
    const int stepX = (std::max)(1, w / 64);
    const int stepY = (std::max)(1, h / 32);
    std::uint32_t total = 0;
    for (int y = 0; y < h; y += stepY) {
        const std::uint8_t* row = bgra + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 4u;
        for (int x = 0; x < w; x += stepX) {
            const std::size_t o = static_cast<std::size_t>(x) * 4u;
            const int luma = (77 * row[o + 2] + 150 * row[o + 1] + 29 * row[o + 0]) >> 8;
            hist[static_cast<std::size_t>(std::clamp(luma, 0, 255))]++;
            total++;
        }
    }
    if (total == 0) return;
    const std::uint32_t t15 = (total * 15u) / 100u;
    const std::uint32_t t85 = (total * 85u) / 100u;
    std::uint32_t acc = 0;
    bool got15 = false;
    for (int i = 0; i < 256; ++i) {
        acc += hist[static_cast<std::size_t>(i)];
        if (!got15 && acc >= t15) { outP15 = static_cast<float>(i); got15 = true; }
        if (acc >= t85) { outP85 = static_cast<float>(i); break; }
    }
}

// Fast bilinear BGRA32 sampler with edge clamp
inline void SampleBilinearBgra32(
    const std::uint8_t* src, int w, int h, float fx, float fy,
    float& outR, float& outG, float& outB) noexcept
{
    const float cx = std::clamp(fx, 0.0f, static_cast<float>(w - 1));
    const float cy = std::clamp(fy, 0.0f, static_cast<float>(h - 1));
    const int x0 = static_cast<int>(cx);
    const int y0 = static_cast<int>(cy);
    const int x1 = (std::min)(x0 + 1, w - 1);
    const int y1 = (std::min)(y0 + 1, h - 1);
    const float tx = cx - static_cast<float>(x0);
    const float ty = cy - static_cast<float>(y0);

    const std::uint8_t* p00 = src + (static_cast<std::size_t>(y0) * w + x0) * 4u;
    const std::uint8_t* p10 = src + (static_cast<std::size_t>(y0) * w + x1) * 4u;
    const std::uint8_t* p01 = src + (static_cast<std::size_t>(y1) * w + x0) * 4u;
    const std::uint8_t* p11 = src + (static_cast<std::size_t>(y1) * w + x1) * 4u;

    const float w00 = (1.0f - tx) * (1.0f - ty);
    const float w10 = tx * (1.0f - ty);
    const float w01 = (1.0f - tx) * ty;
    const float w11 = tx * ty;

    outB = (p00[0] * w00 + p10[0] * w10 + p01[0] * w01 + p11[0] * w11) * (1.0f / 255.0f);
    outG = (p00[1] * w00 + p10[1] * w10 + p01[1] * w01 + p11[1] * w11) * (1.0f / 255.0f);
    outR = (p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11) * (1.0f / 255.0f);
}

// True Dual Kawase Blur pyramid (downsample 5-tap + upsample 8-tap tent filter)
inline std::vector<std::uint8_t> DualKawaseBlurBgra32(
    const std::uint8_t* src, int w, int h, int passes)
{
    std::vector<std::uint8_t> cur(src, src + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
    if (w < 4 || h < 4 || passes <= 0) return cur;

    struct Level {
        int w = 0;
        int h = 0;
        std::vector<std::uint8_t> px;
    };
    const int depth = std::clamp(passes, 1, 3);
    std::vector<Level> pyr;
    pyr.push_back({ w, h, std::move(cur) });

    // Downsample passes (center * 4 + 4 diagonal corners) / 8
    for (int d = 0; d < depth; ++d) {
        const auto& prev = pyr.back();
        const int nw = (std::max)(2, prev.w / 2);
        const int nh = (std::max)(2, prev.h / 2);
        Level next{ nw, nh, std::vector<std::uint8_t>(static_cast<std::size_t>(nw) * nh * 4u, 255) };
        for (int y = 0; y < nh; ++y) {
            const float sy = (static_cast<float>(y) + 0.5f) * (static_cast<float>(prev.h) / static_cast<float>(nh)) - 0.5f;
            for (int x = 0; x < nw; ++x) {
                const float sx = (static_cast<float>(x) + 0.5f) * (static_cast<float>(prev.w) / static_cast<float>(nw)) - 0.5f;
                float r0, g0, b0, r1, g1, b1, r2, g2, b2, r3, g3, b3, r4, g4, b4;
                SampleBilinearBgra32(prev.px.data(), prev.w, prev.h, sx, sy, r0, g0, b0);
                SampleBilinearBgra32(prev.px.data(), prev.w, prev.h, sx - 1.0f, sy - 1.0f, r1, g1, b1);
                SampleBilinearBgra32(prev.px.data(), prev.w, prev.h, sx + 1.0f, sy - 1.0f, r2, g2, b2);
                SampleBilinearBgra32(prev.px.data(), prev.w, prev.h, sx - 1.0f, sy + 1.0f, r3, g3, b3);
                SampleBilinearBgra32(prev.px.data(), prev.w, prev.h, sx + 1.0f, sy + 1.0f, r4, g4, b4);
                const float r = (r0 * 4.0f + r1 + r2 + r3 + r4) * (255.0f / 8.0f);
                const float g = (g0 * 4.0f + g1 + g2 + g3 + g4) * (255.0f / 8.0f);
                const float b = (b0 * 4.0f + b1 + b2 + b3 + b4) * (255.0f / 8.0f);
                const std::size_t o = (static_cast<std::size_t>(y) * nw + x) * 4u;
                next.px[o + 0] = static_cast<std::uint8_t>(std::clamp(b, 0.0f, 255.0f));
                next.px[o + 1] = static_cast<std::uint8_t>(std::clamp(g, 0.0f, 255.0f));
                next.px[o + 2] = static_cast<std::uint8_t>(std::clamp(r, 0.0f, 255.0f));
                next.px[o + 3] = 255;
            }
        }
        pyr.push_back(std::move(next));
    }

    // Upsample passes (4 cardinal * 1 + 4 diagonal * 2) / 12
    for (int d = depth; d >= 1; --d) {
        const auto& srcLv = pyr[static_cast<std::size_t>(d)];
        auto& dstLv = pyr[static_cast<std::size_t>(d - 1)];
        for (int y = 0; y < dstLv.h; ++y) {
            const float sy = (static_cast<float>(y) + 0.5f) * (static_cast<float>(srcLv.h) / static_cast<float>(dstLv.h)) - 0.5f;
            for (int x = 0; x < dstLv.w; ++x) {
                const float sx = (static_cast<float>(x) + 0.5f) * (static_cast<float>(srcLv.w) / static_cast<float>(dstLv.w)) - 0.5f;
                float r = 0.0f, g = 0.0f, b = 0.0f;
                auto add = [&](float ox, float oy, float wt) {
                    float tr, tg, tb;
                    SampleBilinearBgra32(srcLv.px.data(), srcLv.w, srcLv.h, sx + ox, sy + oy, tr, tg, tb);
                    r += tr * wt; g += tg * wt; b += tb * wt;
                };
                add(-1.5f, 0.0f, 1.0f); add(1.5f, 0.0f, 1.0f);
                add(0.0f, -1.5f, 1.0f); add(0.0f, 1.5f, 1.0f);
                add(-0.75f, -0.75f, 2.0f); add(0.75f, -0.75f, 2.0f);
                add(-0.75f, 0.75f, 2.0f);  add(0.75f, 0.75f, 2.0f);
                const std::size_t o = (static_cast<std::size_t>(y) * dstLv.w + x) * 4u;
                dstLv.px[o + 0] = static_cast<std::uint8_t>(std::clamp(b * (255.0f / 12.0f), 0.0f, 255.0f));
                dstLv.px[o + 1] = static_cast<std::uint8_t>(std::clamp(g * (255.0f / 12.0f), 0.0f, 255.0f));
                dstLv.px[o + 2] = static_cast<std::uint8_t>(std::clamp(r * (255.0f / 12.0f), 0.0f, 255.0f));
                dstLv.px[o + 3] = 255;
            }
        }
    }
    return pyr[0].px;
}

// Full 5-stage material translúcido 2.0 optical compositor over a captured backdrop region.
// Modifies dstBgra in-place, rendering soft ambient drop shadow outside the SDF
// and true Snell RGB refracted, Dual-Kawase blurred, Fresnel + dual-specular-rim glass inside.
inline void CompositeTranslucentCapsuleBgra32(
    std::uint8_t* dstBgra,
    int bufW, int bufH,
    int padX, int padY,
    const OpticalGlassParams& params,
    float* outMeanLuma = nullptr)
{
    if (!dstBgra || bufW <= 4 || bufH <= 4) return;
    const int boxW = bufW - padX * 2;
    const int boxH = bufH - padY * 2;
    if (boxW <= 4 || boxH <= 4) return;

    const float effOpacity = params.reduceTransparency ? 0.92f : Clamp01(params.userOpacitySlider);

    // Stage 1: Copy backdrop + apply +15% vibrancy saturation boost + adaptive luminance stats
    std::vector<std::uint8_t> saturated(dstBgra, dstBgra + static_cast<std::size_t>(bufW) * bufH * 4u);
    float p15 = 128.0f, p85 = 128.0f;
    ComputeLumaPercentilesBgra32(saturated.data(), bufW, bufH, p15, p85);
    if (outMeanLuma) *outMeanLuma = 0.5f * (p15 + p85);
    BoostSaturationBgra32(saturated.data(), bufW, bufH, 1.15f);

    // Stage 2: Dual Kawase Blur pyramid
    const int blurDepth = params.reduceTransparency ? 1 : std::clamp(params.blurPasses + (effOpacity > 0.55f ? 1 : 0), 1, 3);
    const std::vector<std::uint8_t> blurred = DualKawaseBlurBgra32(saturated.data(), bufW, bufH, blurDepth);

    const float cx = static_cast<float>(bufW) * 0.5f;
    const float cy = static_cast<float>(bufH) * 0.5f;
    const Vec2 halfSize{ static_cast<float>(boxW) * 0.5f, static_cast<float>(boxH) * 0.5f };
    const float radius = (std::min)(params.cornerRadius, (std::min)(halfSize.x, halfSize.y));
    const float bevelW = (std::max)(4.0f, (std::min)(params.bevelWidth, (std::min)(halfSize.x, halfSize.y)));
    const float opticalScale = 1.0f - 0.85f * effOpacity;
    const float specSuppress = 1.0f - 0.75f * effOpacity;

    // Opposing cenital (@1) & diagonal (@2) specular lights from HLSL spec §5.4
    const Vec3 halfVec1 = Normalize3({ -0.35f, -0.85f, 1.40f });
    const Vec3 halfVec2 = Normalize3({  0.35f,  0.85f, 1.35f });
    const float etaR = 1.0f / params.iorR;
    const float etaG = 1.0f / params.iorG;
    const float etaB = 1.0f / params.iorB;

    // Frosted body target color (Light vs Dark Golden Gate)
    const float frostBaseR = params.darkMode ? 0.145f : 0.955f;
    const float frostBaseG = params.darkMode ? 0.165f : 0.965f;
    const float frostBaseB = params.darkMode ? 0.205f : 0.985f;
    const float frostedR = Lerp(frostBaseR, params.tintR, params.tintProminence);
    const float frostedG = Lerp(frostBaseG, params.tintG, params.tintProminence);
    const float frostedB = Lerp(frostBaseB, params.tintB, params.tintProminence);

    // Subtle shadow intensity adapts to background complexity / mode
    const float shadowStrength = params.darkMode ? 0.34f : 0.14f;

    for (int y = 0; y < bufH; ++y) {
        const float py = (static_cast<float>(y) + 0.5f) - cy;
        for (int x = 0; x < bufW; ++x) {
            const float px = (static_cast<float>(x) + 0.5f) - cx;
            const SdfSample s = SdgRoundBox({ px, py }, halfSize, radius);
            const std::size_t o = (static_cast<std::size_t>(y) * bufW + x) * 4u;

            // Outside capsule: render soft grounded shadow (with 2px vertical offset)
            const float cov = Clamp01(0.5f - s.dist);
            if (cov <= 0.001f) {
                const SdfSample sSh = SdgRoundBox({ px, py - 2.5f }, halfSize, radius);
                if (sSh.dist > 0.0f && sSh.dist < static_cast<float>(padY + 2)) {
                    const float shFalloff = std::exp(-sSh.dist * 0.42f) * shadowStrength;
                    const float keep = 1.0f - shFalloff;
                    dstBgra[o + 0] = static_cast<std::uint8_t>(dstBgra[o + 0] * keep);
                    dstBgra[o + 1] = static_cast<std::uint8_t>(dstBgra[o + 1] * keep);
                    dstBgra[o + 2] = static_cast<std::uint8_t>(dstBgra[o + 2] * keep);
                }
                continue;
            }

            // Stage 3: 3D elliptical bevel height z(d) & surface normal N
            const float u = Clamp01(1.0f + s.dist / bevelW);
            const float oneMinusU2 = (std::max)(1.0f - u * u, 1e-4f);
            const float sqrtTerm = std::sqrt(oneMinusU2);
            const float z = params.bevelHeight * sqrtTerm;
            const float dzdd = -(params.bevelHeight / bevelW) * (u / sqrtTerm);
            const Vec3 N = Normalize3({ -dzdd * s.gx * opticalScale, -dzdd * s.gy * opticalScale, 1.0f });

            // Stage 4: Spectral Snell refraction per R/G/B channel
            const float cosThetaI = Clamp01(N.z);
            const float sin2I = 1.0f - cosThetaI * cosThetaI;
            const float totalDepth = params.baseThickness + z;

            auto refrOffset = [&](float eta) -> Vec2 {
                const float kRefr = (std::max)(1.0f - eta * eta * sin2I, 1e-4f);
                const float term = eta * cosThetaI - std::sqrt(kRefr);
                const float tzNeg = (std::max)(-(-eta + term * N.z), 1e-3f);
                return {
                    std::clamp(totalDepth * ((term * N.x) / tzNeg), -12.0f, 12.0f),
                    std::clamp(totalDepth * ((term * N.y) / tzNeg), -12.0f, 12.0f)
                };
            };

            const Vec2 offR = refrOffset(etaR);
            const Vec2 offG = refrOffset(etaG);
            const Vec2 offB = refrOffset(etaB);

            float colR, colG, colB, dummy1, dummy2;
            SampleBilinearBgra32(blurred.data(), bufW, bufH, static_cast<float>(x) + offR.x, static_cast<float>(y) + offR.y, colR, dummy1, dummy2);
            SampleBilinearBgra32(blurred.data(), bufW, bufH, static_cast<float>(x) + offG.x, static_cast<float>(y) + offG.y, dummy1, colG, dummy2);
            SampleBilinearBgra32(blurred.data(), bufW, bufH, static_cast<float>(x) + offB.x, static_cast<float>(y) + offB.y, dummy1, dummy2, colB);

            // Physical glass Beer-Lambert absorption (green absorbed least)
            const float dScale = totalDepth * 0.06f;
            float bodyR = colR * std::exp(-0.020f * dScale);
            float bodyG = colG * std::exp(-0.005f * dScale);
            float bodyB = colB * std::exp(-0.015f * dScale);

            // Apply optional prominent tint (reserved strictly for primary actions!)
            if (params.tintProminence > 0.001f) {
                bodyR = Lerp(bodyR, params.tintR, params.tintProminence);
                bodyG = Lerp(bodyG, params.tintG, params.tintProminence);
                bodyB = Lerp(bodyB, params.tintB, params.tintProminence);
            }

            // Blend with frosted surface according to macOS 27 UserOpacitySlider
            const float frostMix = params.reduceTransparency ? 0.90f : (0.28f + effOpacity * 0.62f);
            bodyR = Lerp(bodyR, frostedR, frostMix);
            bodyG = Lerp(bodyG, frostedG, frostMix);
            bodyB = Lerp(bodyB, frostedB, frostMix);

            // Stage 5: Schlick Fresnel (F0=0.043) + opposing @1/@2 specular rim highlights
            const float f0 = 0.043f;
            const float omc = 1.0f - cosThetaI;
            const float fresnel = f0 + (1.0f - f0) * (omc * omc * omc * omc * omc);

            const float ndh1 = Clamp01(Dot3(N, halfVec1));
            const float ndh2 = Clamp01(Dot3(N, halfVec2));
            const float spec1 = std::pow(ndh1, 24.0f) * (params.darkMode ? 0.32f : 0.45f);
            const float spec2 = std::pow(ndh2, 32.0f) * (params.darkMode ? 0.16f : 0.22f);

            const float rimMask = Smoothstep(2.4f, 0.0f, std::fabs(s.dist + 0.9f));
            const float topRimDot = Clamp01(s.gx * -0.35f + s.gy * -0.92f);
            const float botRimDot = Clamp01(s.gx *  0.35f + s.gy *  0.92f);
            const float rimGlow = rimMask * ((params.darkMode ? 0.26f : 0.48f) * topRimDot +
                                             (params.darkMode ? 0.10f : 0.18f) * botRimDot +
                                             (params.darkMode ? 0.06f : 0.08f));

            const float specAdd = (fresnel * (params.darkMode ? 0.35f : 0.42f) + spec1 + spec2 + rimGlow) * specSuppress;
            float finalR = bodyR + specAdd;
            float finalG = bodyG + specAdd;
            float finalB = bodyB + specAdd;

            // Subtle hairline border for optical legibility, or crisp 1px HighContrastBorder
            const float borderLine = Smoothstep(1.15f, 0.15f, std::fabs(s.dist + 0.5f));
            if (params.highContrastBorder) {
                const float hcVal = params.darkMode ? 0.92f : 0.12f;
                finalR = Lerp(finalR, hcVal, borderLine * 0.90f);
                finalG = Lerp(finalG, hcVal, borderLine * 0.90f);
                finalB = Lerp(finalB, hcVal, borderLine * 0.90f);
            } else {
                const float bCol = params.darkMode ? 0.85f : 0.18f;
                const float bAlpha = params.darkMode ? 0.14f : 0.10f;
                finalR = Lerp(finalR, bCol, borderLine * bAlpha);
                finalG = Lerp(finalG, bCol, borderLine * bAlpha);
                finalB = Lerp(finalB, bCol, borderLine * bAlpha);
            }

            finalR = Clamp01(finalR);
            finalG = Clamp01(finalG);
            finalB = Clamp01(finalB);

            // Analytical anti-aliased blend over existing backdrop pixel
            const float bgB = dstBgra[o + 0] * (1.0f / 255.0f);
            const float bgG = dstBgra[o + 1] * (1.0f / 255.0f);
            const float bgR = dstBgra[o + 2] * (1.0f / 255.0f);

            dstBgra[o + 0] = static_cast<std::uint8_t>(std::clamp(Lerp(bgB, finalB, cov) * 255.0f, 0.0f, 255.0f));
            dstBgra[o + 1] = static_cast<std::uint8_t>(std::clamp(Lerp(bgG, finalG, cov) * 255.0f, 0.0f, 255.0f));
            dstBgra[o + 2] = static_cast<std::uint8_t>(std::clamp(Lerp(bgR, finalR, cov) * 255.0f, 0.0f, 255.0f));
            dstBgra[o + 3] = 255;
        }
    }
}

// WCAG 2.1 sRGB linearization + Relative Luminance + Contrast Ratio (HIG Accessibility §4.2)
inline float LinearizeSrgbChannel(float c) noexcept {
    const float v = Clamp01(c);
    return (v <= 0.04045f) ? (v / 12.92f) : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

inline float RelativeLuminanceRgb8(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return 0.2126f * LinearizeSrgbChannel(static_cast<float>(r) / 255.0f) +
           0.7152f * LinearizeSrgbChannel(static_cast<float>(g) / 255.0f) +
           0.0722f * LinearizeSrgbChannel(static_cast<float>(b) / 255.0f);
}

inline float ContrastRatioRgb8(
    std::uint8_t r1, std::uint8_t g1, std::uint8_t b1,
    std::uint8_t r2, std::uint8_t g2, std::uint8_t b2) noexcept
{
    const float l1 = RelativeLuminanceRgb8(r1, g1, b1);
    const float l2 = RelativeLuminanceRgb8(r2, g2, b2);
    const float hi = (std::max)(l1, l2);
    const float lo = (std::min)(l1, l2);
    return (hi + 0.05f) / (lo + 0.05f);
}

// Autonomous foreground polarity decision from backdrop p15/p85 percentiles + opacity
inline bool ShouldUseDarkForegroundOnGlass(float p15, float p85, bool darkMode, float userOpacity) noexcept {
    const float effOp = Clamp01(userOpacity);
    const float meanBg = 0.5f * (p15 + p85);
    const float frostLuma = darkMode ? 38.0f : 244.0f;
    const float frostMix = 0.28f + effOp * 0.62f;
    const float perceived = Lerp(meanBg, frostLuma, frostMix);
    return perceived >= 132.0f;
}

// Analytical SDF + normalized gradient for continuous iPadOS squircle (superellipse n = 4)
inline SdfSample SdgSquircle(Vec2 p, Vec2 halfSize) noexcept {
    const float bx = (std::max)(1.0f, halfSize.x);
    const float by = (std::max)(1.0f, halfSize.y);
    const float ax = std::fabs(p.x) / bx;
    const float ay = std::fabs(p.y) / by;
    const float n4 = ax * ax * ax * ax + ay * ay * ay * ay;
    const float minB = (std::min)(bx, by);
    if (n4 < 1e-10f) {
        return { -minB, 0.0f, -1.0f };
    }
    const float r = std::pow(n4, 0.25f);
    const float d = (r - 1.0f) * minB;
    const float sx = (p.x >= 0.0f) ? 1.0f : -1.0f;
    const float sy = (p.y >= 0.0f) ? 1.0f : -1.0f;
    const float gxRaw = sx * (ax * ax * ax) / bx;
    const float gyRaw = sy * (ay * ay * ay) / by;
    const float gLen = std::sqrt(gxRaw * gxRaw + gyRaw * gyRaw);
    if (gLen > 1e-6f) {
        return { d, gxRaw / gLen, gyRaw / gLen };
    }
    return { d, 0.0f, -1.0f };
}

// Batched GlassEffectContainer primitive for C1 smin fusion over a full viewport/strip
struct GlassCapsulePrimitive {
    float cx = 0.0f;
    float cy = 0.0f;
    float halfW = 20.0f;
    float halfH = 16.0f;
    float cornerRadius = 16.0f; // <= 0 => full capsule r = min(halfW, halfH)
    bool useSquircle = false;
    bool prominent = false;
    float tintR = 1.0f;
    float tintG = 1.0f;
    float tintB = 1.0f;
    float tintProminence = 0.0f;
};

// Batched GlassEffectContainer compositor (Arquitectura material translúcido y HIG §5.3B & §5.5 #4)
// Evaluates all primitives in one SDF pass using SminQuadraticGrad (smooth union k),
// refract-sampling the real underlying scene pixels once per region.
inline void CompositeTranslucentContainerBgra32(
    std::uint8_t* dstBgra,
    int bufW, int bufH,
    const std::vector<GlassCapsulePrimitive>& capsules,
    const OpticalGlassParams& baseParams,
    float smoothUnionK = 6.0f,
    float* outMeanLuma = nullptr)
{
    if (!dstBgra || bufW <= 4 || bufH <= 4 || capsules.empty()) return;

    // Bounding region covering all capsules + shadow/bevel halo for minimal CPU work
    int minX = bufW, minY = bufH, maxX = 0, maxY = 0;
    const int halo = 14;
    for (const auto& c : capsules) {
        minX = (std::min)(minX, (std::max)(0, static_cast<int>(std::floor(c.cx - c.halfW)) - halo));
        minY = (std::min)(minY, (std::max)(0, static_cast<int>(std::floor(c.cy - c.halfH)) - halo));
        maxX = (std::max)(maxX, (std::min)(bufW, static_cast<int>(std::ceil(c.cx + c.halfW)) + halo));
        maxY = (std::max)(maxY, (std::min)(bufH, static_cast<int>(std::ceil(c.cy + c.halfH)) + halo));
    }
    const int regW = maxX - minX;
    const int regH = maxY - minY;
    if (regW <= 4 || regH <= 4) return;

    std::vector<std::uint8_t> sub(static_cast<std::size_t>(regW) * regH * 4u);
    for (int y = 0; y < regH; ++y) {
        std::memcpy(
            sub.data() + static_cast<std::size_t>(y) * regW * 4u,
            dstBgra + (static_cast<std::size_t>(minY + y) * bufW + minX) * 4u,
            static_cast<std::size_t>(regW) * 4u);
    }

    const float effOpacity = baseParams.reduceTransparency ? 0.92f : Clamp01(baseParams.userOpacitySlider);
    float p15 = 128.0f, p85 = 128.0f;
    ComputeLumaPercentilesBgra32(sub.data(), regW, regH, p15, p85);
    if (outMeanLuma) *outMeanLuma = 0.5f * (p15 + p85);
    BoostSaturationBgra32(sub.data(), regW, regH, 1.15f);

    const int blurDepth = baseParams.reduceTransparency ? 1 : std::clamp(baseParams.blurPasses + (effOpacity > 0.55f ? 1 : 0), 1, 3);
    const std::vector<std::uint8_t> blurred = DualKawaseBlurBgra32(sub.data(), regW, regH, blurDepth);

    const float opticalScale = 1.0f - 0.85f * effOpacity;
    const float specSuppress = 1.0f - 0.75f * effOpacity;
    const Vec3 halfVec1 = Normalize3({ -0.35f, -0.85f, 1.40f });
    const Vec3 halfVec2 = Normalize3({  0.35f,  0.85f, 1.35f });
    const float etaR = 1.0f / baseParams.iorR;
    const float etaG = 1.0f / baseParams.iorG;
    const float etaB = 1.0f / baseParams.iorB;
    const float frostBaseR = baseParams.darkMode ? 0.145f : 0.955f;
    const float frostBaseG = baseParams.darkMode ? 0.165f : 0.965f;
    const float frostBaseB = baseParams.darkMode ? 0.205f : 0.985f;
    const float shadowStrength = baseParams.darkMode ? 0.34f : 0.14f;

    auto evalPrim = [&](const GlassCapsulePrimitive& c, float px, float py) noexcept -> SdfSample {
        const Vec2 rel{ px - c.cx, py - c.cy };
        if (c.useSquircle) return SdgSquircle(rel, { c.halfW, c.halfH });
        const float r = (c.cornerRadius <= 0.0f) ? (std::min)(c.halfW, c.halfH) : (std::min)(c.cornerRadius, (std::min)(c.halfW, c.halfH));
        return SdgRoundBox(rel, { c.halfW, c.halfH }, r);
    };

    for (int y = 0; y < regH; ++y) {
        const float fy = static_cast<float>(minY + y) + 0.5f;
        for (int x = 0; x < regW; ++x) {
            const float fx = static_cast<float>(minX + x) + 0.5f;

            SdfSample s{};
            bool hasS = false;
            float bestDist = 1e9f;
            const GlassCapsulePrimitive* owner = &capsules[0];
            for (const auto& c : capsules) {
                const SdfSample sc = evalPrim(c, fx, fy);
                if (sc.dist < bestDist) {
                    bestDist = sc.dist;
                    owner = &c;
                }
                if (!hasS) {
                    s = sc;
                    hasS = true;
                } else if (smoothUnionK > 0.05f && !c.prominent && !owner->prominent) {
                    s = SminQuadraticGrad(s, sc, smoothUnionK);
                } else if (sc.dist < s.dist) {
                    s = sc;
                }
            }

            const std::size_t dstOff = (static_cast<std::size_t>(minY + y) * bufW + (minX + x)) * 4u;
            const float cov = Clamp01(0.5f - s.dist);
            if (cov <= 0.001f) {
                // Evaluate shifted shadow SDF
                float shDist = 1e9f;
                for (const auto& c : capsules) {
                    const float sd = evalPrim(c, fx, fy - 2.5f).dist;
                    if (sd < shDist) shDist = sd;
                }
                if (shDist > 0.0f && shDist < static_cast<float>(halo)) {
                    const float shFalloff = std::exp(-shDist * 0.44f) * shadowStrength;
                    const float keep = 1.0f - shFalloff;
                    dstBgra[dstOff + 0] = static_cast<std::uint8_t>(dstBgra[dstOff + 0] * keep);
                    dstBgra[dstOff + 1] = static_cast<std::uint8_t>(dstBgra[dstOff + 1] * keep);
                    dstBgra[dstOff + 2] = static_cast<std::uint8_t>(dstBgra[dstOff + 2] * keep);
                }
                continue;
            }

            const float bevelW = (std::max)(4.0f, (std::min)(baseParams.bevelWidth, (std::min)(owner->halfW, owner->halfH) * 0.85f));
            const float bevelH = (std::min)(baseParams.bevelHeight, bevelW * 0.55f);
            const float u = Clamp01(1.0f + s.dist / bevelW);
            const float sqrtTerm = std::sqrt((std::max)(1.0f - u * u, 1e-4f));
            const float z = bevelH * sqrtTerm;
            const float dzdd = -(bevelH / bevelW) * (u / sqrtTerm);
            const Vec3 N = Normalize3({ -dzdd * s.gx * opticalScale, -dzdd * s.gy * opticalScale, 1.0f });

            const float cosThetaI = Clamp01(N.z);
            const float sin2I = 1.0f - cosThetaI * cosThetaI;
            const float totalDepth = baseParams.baseThickness + z;
            auto refrOffset = [&](float eta) -> Vec2 {
                const float kRefr = (std::max)(1.0f - eta * eta * sin2I, 1e-4f);
                const float term = eta * cosThetaI - std::sqrt(kRefr);
                const float tzNeg = (std::max)(-(-eta + term * N.z), 1e-3f);
                return {
                    std::clamp(totalDepth * ((term * N.x) / tzNeg), -12.0f, 12.0f),
                    std::clamp(totalDepth * ((term * N.y) / tzNeg), -12.0f, 12.0f)
                };
            };

            const Vec2 offR = refrOffset(etaR);
            const Vec2 offG = refrOffset(etaG);
            const Vec2 offB = refrOffset(etaB);
            float colR, colG, colB, d1, d2;
            SampleBilinearBgra32(blurred.data(), regW, regH, static_cast<float>(x) + offR.x, static_cast<float>(y) + offR.y, colR, d1, d2);
            SampleBilinearBgra32(blurred.data(), regW, regH, static_cast<float>(x) + offG.x, static_cast<float>(y) + offG.y, d1, colG, d2);
            SampleBilinearBgra32(blurred.data(), regW, regH, static_cast<float>(x) + offB.x, static_cast<float>(y) + offB.y, d1, d2, colB);

            const float dScale = totalDepth * 0.06f;
            float bodyR = colR * std::exp(-0.020f * dScale);
            float bodyG = colG * std::exp(-0.005f * dScale);
            float bodyB = colB * std::exp(-0.015f * dScale);

            const float prom = owner->prominent ? Clamp01(owner->tintProminence) : 0.0f;
            if (prom > 0.001f) {
                bodyR = Lerp(bodyR, owner->tintR, prom);
                bodyG = Lerp(bodyG, owner->tintG, prom);
                bodyB = Lerp(bodyB, owner->tintB, prom);
            }
            const float frostedR = Lerp(frostBaseR, owner->tintR, prom);
            const float frostedG = Lerp(frostBaseG, owner->tintG, prom);
            const float frostedB = Lerp(frostBaseB, owner->tintB, prom);
            const float frostMix = baseParams.reduceTransparency ? 0.90f : (0.26f + effOpacity * 0.62f);
            bodyR = Lerp(bodyR, frostedR, frostMix);
            bodyG = Lerp(bodyG, frostedG, frostMix);
            bodyB = Lerp(bodyB, frostedB, frostMix);

            const float f0 = 0.043f;
            const float omc = 1.0f - cosThetaI;
            const float fresnel = f0 + (1.0f - f0) * (omc * omc * omc * omc * omc);
            const float spec1 = std::pow(Clamp01(Dot3(N, halfVec1)), 24.0f) * (baseParams.darkMode ? 0.32f : 0.45f);
            const float spec2 = std::pow(Clamp01(Dot3(N, halfVec2)), 32.0f) * (baseParams.darkMode ? 0.16f : 0.22f);
            const float rimMask = Smoothstep(2.4f, 0.0f, std::fabs(s.dist + 0.9f));
            const float topRimDot = Clamp01(s.gx * -0.35f + s.gy * -0.92f);
            const float botRimDot = Clamp01(s.gx *  0.35f + s.gy *  0.92f);
            const float rimGlow = rimMask * ((baseParams.darkMode ? 0.26f : 0.48f) * topRimDot +
                                             (baseParams.darkMode ? 0.10f : 0.18f) * botRimDot +
                                             (baseParams.darkMode ? 0.06f : 0.08f));
            const float specAdd = (fresnel * (baseParams.darkMode ? 0.35f : 0.42f) + spec1 + spec2 + rimGlow) * specSuppress;

            float finalR = bodyR + specAdd;
            float finalG = bodyG + specAdd;
            float finalB = bodyB + specAdd;
            const float borderLine = Smoothstep(1.15f, 0.15f, std::fabs(s.dist + 0.5f));
            if (baseParams.highContrastBorder) {
                const float hcVal = baseParams.darkMode ? 0.92f : 0.12f;
                finalR = Lerp(finalR, hcVal, borderLine * 0.90f);
                finalG = Lerp(finalG, hcVal, borderLine * 0.90f);
                finalB = Lerp(finalB, hcVal, borderLine * 0.90f);
            } else {
                const float bCol = baseParams.darkMode ? 0.85f : 0.18f;
                const float bAlpha = baseParams.darkMode ? 0.14f : 0.10f;
                finalR = Lerp(finalR, bCol, borderLine * bAlpha);
                finalG = Lerp(finalG, bCol, borderLine * bAlpha);
                finalB = Lerp(finalB, bCol, borderLine * bAlpha);
            }

            const float bgB = dstBgra[dstOff + 0] * (1.0f / 255.0f);
            const float bgG = dstBgra[dstOff + 1] * (1.0f / 255.0f);
            const float bgR = dstBgra[dstOff + 2] * (1.0f / 255.0f);
            dstBgra[dstOff + 0] = static_cast<std::uint8_t>(std::clamp(Lerp(bgB, Clamp01(finalB), cov) * 255.0f, 0.0f, 255.0f));
            dstBgra[dstOff + 1] = static_cast<std::uint8_t>(std::clamp(Lerp(bgG, Clamp01(finalG), cov) * 255.0f, 0.0f, 255.0f));
            dstBgra[dstOff + 2] = static_cast<std::uint8_t>(std::clamp(Lerp(bgR, Clamp01(finalR), cov) * 255.0f, 0.0f, 255.0f));
            dstBgra[dstOff + 3] = 255;
        }
    }
}

} // namespace UiGlass
