#pragma once
// ============================================================================
// latex_cairo.hpp
// Native Linux PangoCairo 2D LaTeX Mathematical Renderer
// Three-phase AST pipeline (Parse -> Measure -> Draw) porting latex_math_renderer.h
// Supports inline $...$ / $$...$$, auto-math tokens, \frac, \dfrac, \tfrac,
// \binom, \sqrt[n]{x}, \int, \oint, \sum, \prod, \lim, ^ / _, \vec, \bar, \hat,
// \left...\right, matrices/cases, Greek letters, operators & multi-line wrap.
// ============================================================================

#include "ui_theme_linux.hpp"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <string>

namespace CasioLatexCairo {

struct ValidationResult {
    bool ok = true;
    std::string message;
};

struct MeasureExtent {
    double width = 0.0;
    double height = 0.0;
};

ValidationResult ValidateRichLatexInput(const std::string& raw);

MeasureExtent MeasureLatexRichText(cairo_t* cr,
                                   const std::string& text,
                                   double fontPx,
                                   double maxWidth = 0.0,
                                   bool bold = false,
                                   bool mathDefault = false);

MeasureExtent DrawLatexRichText(cairo_t* cr,
                                const std::string& text,
                                double x, double y,
                                double maxWidth, double maxHeight,
                                double fontPx,
                                CasioThemeLinux::RgbaColor color,
                                bool bold = false,
                                bool mathDefault = false);

void DrawLatexRichTextCentered(cairo_t* cr,
                               const std::string& text,
                               double x, double y,
                               double w, double h,
                               double fontPx,
                               CasioThemeLinux::RgbaColor color,
                               bool bold = false,
                               bool mathDefault = true);

} // namespace CasioLatexCairo
