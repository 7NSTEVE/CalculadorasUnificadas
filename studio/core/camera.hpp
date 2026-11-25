#pragma once

#include "doc_types.hpp"
#include <algorithm>
#include <cmath>

namespace DocStudio {

struct Camera2D {
    // World coordinates are in double precision (points or infinite canvas units)
    double panX = 0.0;  // world X mapped to screen (0,0)
    double panY = 0.0;  // world Y mapped to screen (0,0)
    double zoom = 1.0;  // screenPx per world unit

    static constexpr double kMinZoom = 0.05;
    static constexpr double kMaxZoom = 16.0;

    void ClampZoom() noexcept {
        zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    }

    DocPoint WorldToScreen(double wx, double wy) const noexcept {
        return { (wx - panX) * zoom, (wy - panY) * zoom };
    }

    DocPoint ScreenToWorld(double sx, double sy) const noexcept {
        const double z = (zoom > 1e-9) ? zoom : 1.0;
        return { panX + (sx / z), panY + (sy / z) };
    }

    DocRect WorldRectToScreen(const DocRect& wr) const noexcept {
        return {
            (wr.x - panX) * zoom,
            (wr.y - panY) * zoom,
            wr.w * zoom,
            wr.h * zoom
        };
    }

    DocRect ScreenRectToWorld(const DocRect& sr) const noexcept {
        const double z = (zoom > 1e-9) ? zoom : 1.0;
        return {
            panX + (sr.x / z),
            panY + (sr.y / z),
            sr.w / z,
            sr.h / z
        };
    }

    // Zooms keeping the world point beneath (anchorScreenX, anchorScreenY) stationary on screen.
    void ZoomAtScreenAnchor(double anchorScreenX, double anchorScreenY, double newZoom) noexcept {
        const DocPoint anchorWorld = ScreenToWorld(anchorScreenX, anchorScreenY);
        zoom = std::clamp(newZoom, kMinZoom, kMaxZoom);
        panX = anchorWorld.x - (anchorScreenX / zoom);
        panY = anchorWorld.y - (anchorScreenY / zoom);
    }

    void ZoomByFactorAtAnchor(double anchorScreenX, double anchorScreenY, double factor) noexcept {
        if (factor <= 0.0) return;
        ZoomAtScreenAnchor(anchorScreenX, anchorScreenY, zoom * factor);
    }
};

} // namespace DocStudio
