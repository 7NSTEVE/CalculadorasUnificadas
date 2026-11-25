#include "exporter_gtk.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace CasioExporterGtk {

namespace {

std::string CurrentTimestampSlug() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

void DrawCompositeLcdOntoCairo(cairo_t* cr,
                               int calcIdx,
                               double dstX, double dstY, double dstW, double dstH,
                               const std::vector<uint8_t>& lcdRgba, int lcdW, int lcdH,
                               uint32_t indicators,
                               const std::string& cw2FallbackExpr,
                               const std::string& cw2FallbackRes,
                               bool crispNearest,
                               bool showGrid) {
    const auto bgCol = CasioThemeLinux::GetLcdBgColor(calcIdx);
    const auto fgCol = CasioThemeLinux::GetLcdFgColor(calcIdx);

    CasioThemeLinux::FillRoundedRect(cr, dstX, dstY, dstW, dstH, 3.0, bgCol);

    if (calcIdx == 2 || calcIdx == 3) {
        // Color/high-res LCD fills entire area
        if (!lcdRgba.empty() && lcdW > 0 && lcdH > 0) {
            CasioThemeLinux::CairoSurfacePtr surf(
                CasioThemeLinux::RgbaToCairoSurface(lcdRgba.data(), lcdW, lcdH));
            if (surf) {
                cairo_save(cr);
                cairo_translate(cr, dstX, dstY);
                cairo_scale(cr, dstW / static_cast<double>(lcdW), dstH / static_cast<double>(lcdH));
                cairo_set_source_surface(cr, surf.get(), 0.0, 0.0);
                cairo_pattern_set_filter(cairo_get_source(cr),
                                         crispNearest ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_BILINEAR);
                cairo_paint(cr);
                cairo_restore(cr);
            }
        } else if (calcIdx == 3 && (!cw2FallbackExpr.empty() || !cw2FallbackRes.empty())) {
            CasioLatexCairo::DrawLatexRichText(
                cr, cw2FallbackExpr, dstX + 10.0, dstY + 12.0, dstW - 20.0, dstH * 0.45,
                16.0, fgCol, false, true);
            CasioLatexCairo::DrawLatexRichText(
                cr, cw2FallbackRes, dstX + 10.0, dstY + dstH * 0.56, dstW - 20.0, dstH * 0.36,
                18.0, fgCol, true, true);
        }
        return;
    }

    // Blanca (0) & Gris (1): indicator strip above dot matrix
    const auto sub = CasioThemeLinux::ComputeLcdIndAndDotRects(calcIdx, dstX, dstY, dstW, dstH);

    std::vector<uint8_t> indRgba;
    int indW = 0, indH = 0;
    CasioLinux::UnifiedCalcEngine::RenderIndicatorStripRgba(
        static_cast<CasioLinux::CalcModelId>(calcIdx),
        indicators,
        fgCol.r, fgCol.g, fgCol.b,
        bgCol.r, bgCol.g, bgCol.b,
        indRgba, indW, indH);

    if (!indRgba.empty() && indW > 0 && indH > 0) {
        CasioThemeLinux::CairoSurfacePtr indSurf(
            CasioThemeLinux::RgbaToCairoSurface(indRgba.data(), indW, indH));
        if (indSurf) {
            cairo_save(cr);
            cairo_translate(cr, sub.indX, sub.indY);
            cairo_scale(cr, sub.indW / static_cast<double>(indW), sub.indH / static_cast<double>(indH));
            cairo_set_source_surface(cr, indSurf.get(), 0.0, 0.0);
            cairo_pattern_set_filter(cairo_get_source(cr),
                                     crispNearest ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_BILINEAR);
            cairo_paint(cr);
            cairo_restore(cr);
        }
    }

    if (!lcdRgba.empty() && lcdW > 0 && lcdH > 0) {
        CasioThemeLinux::CairoSurfacePtr dotSurf(
            CasioThemeLinux::RgbaToCairoSurface(lcdRgba.data(), lcdW, lcdH));
        if (dotSurf) {
            cairo_save(cr);
            cairo_translate(cr, sub.dotX, sub.dotY);
            cairo_scale(cr, sub.dotW / static_cast<double>(lcdW), sub.dotH / static_cast<double>(lcdH));
            cairo_set_source_surface(cr, dotSurf.get(), 0.0, 0.0);
            cairo_pattern_set_filter(cairo_get_source(cr),
                                     crispNearest ? CAIRO_FILTER_NEAREST : CAIRO_FILTER_BILINEAR);
            cairo_paint(cr);
            cairo_restore(cr);
        }

        if (showGrid && sub.dotW >= lcdW * 2.0) {
            cairo_save(cr);
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.08);
            cairo_set_line_width(cr, 0.5);
            const double cellW = sub.dotW / static_cast<double>(lcdW);
            const double cellH = sub.dotH / static_cast<double>(lcdH);
            for (int c = 1; c < lcdW; ++c) {
                double lx = sub.dotX + c * cellW;
                cairo_move_to(cr, lx, sub.dotY);
                cairo_line_to(cr, lx, sub.dotY + sub.dotH);
            }
            for (int r = 1; r < lcdH; ++r) {
                double ly = sub.dotY + r * cellH;
                cairo_move_to(cr, sub.dotX, ly);
                cairo_line_to(cr, sub.dotX + sub.dotW, ly);
            }
            cairo_stroke(cr);
            cairo_restore(cr);
        }
    }
}

void DrawKeyChipSurfaceOrFallback(cairo_t* cr,
                                  cairo_surface_t* skinSurf,
                                  int calcIdx,
                                  int keyId,
                                  const std::string& keyName,
                                  double x, double y, double w, double h,
                                  bool included,
                                  bool lightBg) {
    cairo_save(cr);
    if (!included) {
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.14);
        CasioThemeLinux::CairoRoundedRect(cr, x, y, w, h, 6.0);
        cairo_fill(cr);
    }

    bool drewSkin = false;
    if (skinSurf && keyId >= 0) {
        const auto& defs = CasioLinux::GetKeysForModel(static_cast<CasioLinux::CalcModelId>(calcIdx));
        for (const auto& kd : defs) {
            if (kd.id == keyId) {
                CasioThemeLinux::CairoSurfacePtr chip(
                    CasioThemeLinux::CropKeySurfaceFromSkin(skinSurf, calcIdx, kd));
                if (chip) {
                    int sw = cairo_image_surface_get_width(chip.get());
                    int sh = cairo_image_surface_get_height(chip.get());
                    if (sw > 0 && sh > 0) {
                        double sc = std::min((w - 4.0) / sw, (h - 4.0) / sh);
                        double dw = sw * sc;
                        double dh = sh * sc;
                        double dx = x + 0.5 * (w - dw);
                        double dy = y + 0.5 * (h - dh);
                        cairo_save(cr);
                        cairo_translate(cr, dx, dy);
                        cairo_scale(cr, sc, sc);
                        cairo_set_source_surface(cr, chip.get(), 0.0, 0.0);
                        cairo_paint_with_alpha(cr, included ? 1.0 : 0.35);
                        cairo_restore(cr);
                        drewSkin = true;
                    }
                }
                break;
            }
        }
    }

    if (!drewSkin) {
        const auto fill = lightBg ? CasioThemeLinux::RgbaColor(238, 242, 248)
                                  : CasioThemeLinux::RgbaColor(37, 47, 67);
        const auto stroke = lightBg ? CasioThemeLinux::RgbaColor(180, 192, 210)
                                    : CasioThemeLinux::RgbaColor(70, 88, 120);
        const auto txtCol = lightBg ? CasioThemeLinux::RgbaColor(20, 25, 34)
                                    : CasioThemeLinux::RgbaColor(241, 245, 249);
        CasioThemeLinux::FillRoundedRect(cr, x + 2.0, y + 2.0, w - 4.0, h - 4.0, 7.0,
                                         fill, included ? 1.0 : 0.40);
        CasioThemeLinux::StrokeRoundedRect(cr, x + 2.0, y + 2.0, w - 4.0, h - 4.0, 7.0,
                                           stroke, 1.2, included ? 1.0 : 0.40);
        std::string lbl = keyName.empty() ? "?" : keyName;
        CasioLatexCairo::DrawLatexRichTextCentered(
            cr, lbl, x + 4.0, y + 4.0, w - 8.0, h - 8.0, 11.5,
            txtCol, true, false);
    }
    cairo_restore(cr);
}

} // namespace

// ============================================================================
// Clipboard Helpers
// ============================================================================

bool CopyCairoSurfaceToClipboard(cairo_surface_t* surf) {
    if (!surf) return false;
    GdkPixbuf* pb = CasioThemeLinux::CairoSurfaceToGdkPixbuf(surf);
    if (!pb) return false;
    GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_image(cb, pb);
    g_object_unref(pb);
    return true;
}

void CopyTextToClipboard(const std::string& text) {
    GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(cb, text.c_str(), static_cast<gint>(text.size()));
}

bool CopyCalculatorLcdToClipboard(int calcIdx,
                                  const std::vector<uint8_t>& lcdRgba, int lcdW, int lcdH,
                                  uint32_t indicators,
                                  const std::string& textSummary,
                                  int clipboardMode) {
    if (clipboardMode == 1) {
        CopyTextToClipboard(textSummary);
        return !textSummary.empty();
    }

    // Scale up cleanly (min ~576px width for Blanca/Gris/CW2, 768px for CG50)
    int outW = (calcIdx == 2) ? 768 : 576;
    int outH = (calcIdx == 1) ? 222 : (calcIdx == 2 ? 432 : 225);
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, outW, outH);
    cairo_t* cr = cairo_create(surf);
    DrawCompositeLcdOntoCairo(cr, calcIdx, 0, 0, outW, outH, lcdRgba, lcdW, lcdH,
                              indicators, textSummary, "", true, false);
    cairo_destroy(cr);

    bool ok = CopyCairoSurfaceToClipboard(surf);
    cairo_surface_destroy(surf);

    if (clipboardMode == 2 && !textSummary.empty()) {
        // Also copy LaTeX/text to PRIMARY selection so middle-click pastes text
        GtkClipboard* primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
        gtk_clipboard_set_text(primary, textSummary.c_str(), static_cast<gint>(textSummary.size()));
    }
    return ok;
}

// ============================================================================
// PopupDisplayWindow
// ============================================================================

PopupDisplayWindow::~PopupDisplayWindow() {
    if (window_) {
        gtk_widget_destroy(window_);
        window_ = nullptr;
    }
}

void PopupDisplayWindow::Initialize(GtkWindow* parent, SelectCalcCb onSelectCalc, SnapshotCb onCopyLcd) {
    parent_ = parent;
    onSelectCalc_ = std::move(onSelectCalc);
    onCopyLcd_ = std::move(onCopyLcd);

    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), "Pop-up Display — LCD Sincronizado CASIO");
    gtk_window_set_default_size(GTK_WINDOW(window_), 680, 320);
    gtk_window_set_keep_above(GTK_WINDOW(window_), TRUE);
    if (parent_) gtk_window_set_transient_for(GTK_WINDOW(window_), parent_);
    g_signal_connect(window_, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), nullptr);

    GtkWidget* rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), rootBox);

    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "studio-header");
    gtk_box_pack_start(GTK_BOX(rootBox), bar, FALSE, FALSE, 0);

    const char* kNames[4] = {"Blanca", "Gris", "CG50", "991CW"};
    for (int i = 0; i < 4; ++i) {
        modelPills_[i] = gtk_button_new_with_label(kNames[i]);
        g_object_set_data(G_OBJECT(modelPills_[i]), "calc-idx", GINT_TO_POINTER(i));
        g_signal_connect(modelPills_[i], "clicked", G_CALLBACK(+[](GtkButton* b, gpointer ud) {
            auto* self = static_cast<PopupDisplayWindow*>(ud);
            int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "calc-idx"));
            self->calcIdx_ = idx;
            self->UpdatePillStyles();
            if (self->onSelectCalc_) self->onSelectCalc_(idx);
            gtk_widget_queue_draw(self->drawArea_);
        }), this);
        gtk_box_pack_start(GTK_BOX(bar), modelPills_[i], FALSE, FALSE, 0);
    }

    GtkWidget* zoomCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "0", "Zoom: Auto");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "1", "x1 Nativo");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "2", "x2 Nitido");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "3", "x3 Aula");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(zoomCombo), "0");
    g_signal_connect(zoomCombo, "changed", G_CALLBACK(+[](GtkComboBox* c, gpointer ud) {
        auto* self = static_cast<PopupDisplayWindow*>(ud);
        const gchar* id = gtk_combo_box_get_active_id(c);
        if (id) self->zoomFactor_ = std::clamp(std::atoi(id), 0, 3);
        gtk_widget_queue_draw(self->drawArea_);
    }), this);
    gtk_box_pack_start(GTK_BOX(bar), zoomCombo, FALSE, FALSE, 4);

    gridBtn_ = gtk_toggle_button_new_with_label("Cuadricula");
    g_signal_connect(gridBtn_, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer ud) {
        auto* self = static_cast<PopupDisplayWindow*>(ud);
        self->showGrid_ = gtk_toggle_button_get_active(b) != FALSE;
        gtk_widget_queue_draw(self->drawArea_);
    }), this);
    gtk_box_pack_start(GTK_BOX(bar), gridBtn_, FALSE, FALSE, 0);

    topBtn_ = gtk_toggle_button_new_with_label("Fijar");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(topBtn_), TRUE);
    g_signal_connect(topBtn_, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer ud) {
        auto* self = static_cast<PopupDisplayWindow*>(ud);
        self->alwaysOnTop_ = gtk_toggle_button_get_active(b) != FALSE;
        gtk_window_set_keep_above(GTK_WINDOW(self->window_), self->alwaysOnTop_ ? TRUE : FALSE);
    }), this);
    gtk_box_pack_start(GTK_BOX(bar), topBtn_, FALSE, FALSE, 0);

    GtkWidget* btnCopy = gtk_button_new_with_label("Copiar LCD");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnCopy), "accent-btn");
    g_signal_connect(btnCopy, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* self = static_cast<PopupDisplayWindow*>(ud);
        if (self->onCopyLcd_) self->onCopyLcd_(self->calcIdx_);
    }), this);
    gtk_box_pack_end(GTK_BOX(bar), btnCopy, FALSE, FALSE, 0);

    drawArea_ = gtk_drawing_area_new();
    g_signal_connect(drawArea_, "draw", G_CALLBACK(OnDraw), this);
    gtk_box_pack_start(GTK_BOX(rootBox), drawArea_, TRUE, TRUE, 0);

    UpdatePillStyles();
}

void PopupDisplayWindow::Show(int activeCalcIdx) {
    if (!window_) return;
    calcIdx_ = std::clamp(activeCalcIdx, 0, 3);
    UpdatePillStyles();
    gtk_widget_show_all(window_);
    gtk_window_present(GTK_WINDOW(window_));
}

void PopupDisplayWindow::Hide() {
    if (window_) gtk_widget_hide(window_);
}

bool PopupDisplayWindow::IsVisible() const {
    return window_ && gtk_widget_get_visible(window_);
}

void PopupDisplayWindow::UpdateFrame(int activeCalcIdx,
                                       const std::vector<uint8_t>& rgba, int w, int h,
                                       uint32_t indicators,
                                       const std::string& cw2FallbackExpr,
                                       const std::string& cw2FallbackRes) {
    calcIdx_ = std::clamp(activeCalcIdx, 0, 3);
    lcdRgba_ = rgba;
    lcdW_ = w;
    lcdH_ = h;
    indicators_ = indicators;
    cw2Expr_ = cw2FallbackExpr;
    cw2Res_ = cw2FallbackRes;
    UpdatePillStyles();
    if (drawArea_ && IsVisible()) gtk_widget_queue_draw(drawArea_);
}

void PopupDisplayWindow::RefreshTheme() {
    if (drawArea_) gtk_widget_queue_draw(drawArea_);
}

void PopupDisplayWindow::UpdatePillStyles() {
    for (int i = 0; i < 4; ++i) {
        if (!modelPills_[i]) continue;
        GtkStyleContext* ctx = gtk_widget_get_style_context(modelPills_[i]);
        if (i == calcIdx_) gtk_style_context_add_class(ctx, "pill-active");
        else gtk_style_context_remove_class(ctx, "pill-active");
    }
}

gboolean PopupDisplayWindow::OnDraw(GtkWidget* widget, cairo_t* cr, gpointer ud) {
    auto* self = static_cast<PopupDisplayWindow*>(ud);
    const int w = gtk_widget_get_allocated_width(widget);
    const int h = gtk_widget_get_allocated_height(widget);

    CasioThemeLinux::CairoSetSource(cr, CasioThemeLinux::StageBg());
    cairo_paint(cr);

    const double pad = 16.0;
    const double availW = std::max(80.0, w - pad * 2.0);
    const double availH = std::max(40.0, h - pad * 2.0);

    double refW = 384.0, refH = 150.0;
    if (self->calcIdx_ == 1) { refW = 288.0; refH = 111.0; }
    else if (self->calcIdx_ == 2) { refW = 384.0; refH = 216.0; }
    else if (self->calcIdx_ == 3) { refW = 205.0; refH = 87.0; }

    double dstW = availW, dstH = availH;
    if (self->zoomFactor_ > 0) {
        double mul = static_cast<double>(self->zoomFactor_);
        double natW = (self->calcIdx_ == 1 ? 288.0 : 192.0) * mul;
        if (self->calcIdx_ == 2) natW = 384.0 * std::min(mul, 2.0);
        dstW = std::min(availW, natW);
        dstH = dstW * (refH / refW);
        if (dstH > availH) { dstH = availH; dstW = dstH * (refW / refH); }
    } else {
        if (availW / availH > refW / refH) {
            dstH = availH;
            dstW = dstH * (refW / refH);
        } else {
            dstW = availW;
            dstH = dstW * (refH / refW);
        }
    }

    const double x = 0.5 * (w - dstW);
    const double y = 0.5 * (h - dstH);

    CasioThemeLinux::DrawSoftShadow(cr, x - 8.0, y - 8.0, dstW + 16.0, dstH + 16.0, 10.0);
    CasioThemeLinux::FillRoundedRect(cr, x - 8.0, y - 8.0, dstW + 16.0, dstH + 16.0, 10.0,
                                     CasioThemeLinux::RgbaColor(24, 30, 42));
    CasioThemeLinux::StrokeRoundedRect(cr, x - 8.0, y - 8.0, dstW + 16.0, dstH + 16.0, 10.0,
                                       CasioThemeLinux::BorderStrong(), 1.2);

    DrawCompositeLcdOntoCairo(cr, self->calcIdx_, x, y, dstW, dstH,
                              self->lcdRgba_, self->lcdW_, self->lcdH_, self->indicators_,
                              self->cw2Expr_, self->cw2Res_, true, self->showGrid_);
    return TRUE;
}

// ============================================================================
// SequenceExporterWindow
// ============================================================================

SequenceExporterWindow::~SequenceExporterWindow() {
    if (window_) {
        gtk_widget_destroy(window_);
        window_ = nullptr;
    }
}

void SequenceExporterWindow::Initialize(GtkWindow* parent,
                                        const CasioLinux::AssetPaths& paths,
                                        const std::array<cairo_surface_t*, 4>& skinSurfaces,
                                        OpenStudioCb onOpenStudio) {
    parent_ = parent;
    paths_ = paths;
    skinSurfaces_ = skinSurfaces;
    onOpenStudio_ = std::move(onOpenStudio);

    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), "Exportador de Guias Paso a Paso (PNG + Strip + Markdown)");
    gtk_window_set_default_size(GTK_WINDOW(window_), 1080, 780);
    if (parent_) gtk_window_set_transient_for(GTK_WINDOW(window_), parent_);
    g_signal_connect(window_, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), nullptr);

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(root), 10);
    gtk_container_add(GTK_CONTAINER(window_), root);

    // Left controls column (340px)
    GtkWidget* leftCol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(leftCol, 340, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(leftCol), "studio-panel");
    gtk_container_set_border_width(GTK_CONTAINER(leftCol), 10);
    gtk_box_pack_start(GTK_BOX(root), leftCol, FALSE, FALSE, 0);

    // Preset bar
    GtkWidget* pLbl = gtk_label_new("Preset de exportacion");
    gtk_widget_set_halign(pLbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(leftCol), pLbl, FALSE, FALSE, 0);

    GtkWidget* presetRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(leftCol), presetRow, FALSE, FALSE, 0);
    const char* kPresetNames[4] = {"Explicada", "Rapida", "Practica", "Presentac."};
    for (int i = 0; i < 4; ++i) {
        presetBtns_[i] = gtk_button_new_with_label(kPresetNames[i]);
        g_object_set_data(G_OBJECT(presetBtns_[i]), "preset-id", GINT_TO_POINTER(i));
        g_signal_connect(presetBtns_[i], "clicked", G_CALLBACK(+[](GtkButton* b, gpointer ud) {
            auto* self = static_cast<SequenceExporterWindow*>(ud);
            self->ApplyPreset(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "preset-id")));
        }), this);
        gtk_box_pack_start(GTK_BOX(presetRow), presetBtns_[i], TRUE, TRUE, 0);
    }

    // Range #A .. #B
    GtkWidget* rangeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(leftCol), rangeRow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rangeRow), gtk_label_new("Pasos #"), FALSE, FALSE, 0);
    spinStart_ = gtk_spin_button_new_with_range(1, 9999, 1);
    spinEnd_ = gtk_spin_button_new_with_range(1, 9999, 1);
    gtk_box_pack_start(GTK_BOX(rangeRow), spinStart_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(rangeRow), gtk_label_new("a"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rangeRow), spinEnd_, TRUE, TRUE, 0);
    auto onRangeChg = G_CALLBACK(+[](GtkSpinButton*, gpointer ud) {
        auto* self = static_cast<SequenceExporterWindow*>(ud);
        if (!self->updatingUi_) self->RebuildFromRange();
    });
    g_signal_connect(spinStart_, "value-changed", onRangeChg, this);
    g_signal_connect(spinEnd_, "value-changed", onRangeChg, this);

    // Equivalent model switcher
    GtkWidget* mRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(leftCol), mRow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mRow), gtk_label_new("Modelo:"), FALSE, FALSE, 0);
    modelCombo_ = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "0", "fx-570LA X (Blanca)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "1", "fx-570VN PLUS (Gris)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "2", "fx-CG50 PRIZM");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "3", "fx-991CW (Negra)");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(modelCombo_), "0");
    g_signal_connect(modelCombo_, "changed", G_CALLBACK(+[](GtkComboBox* c, gpointer ud) {
        auto* self = static_cast<SequenceExporterWindow*>(ud);
        if (self->updatingUi_) return;
        const gchar* id = gtk_combo_box_get_active_id(c);
        if (!id) return;
        int target = std::clamp(std::atoi(id), 0, 3);
        if (target != self->state_.calcIdx) {
            std::vector<int> srcIds;
            for (const auto& st : self->state_.steps) srcIds.push_back(st.keyId);
            std::vector<int> mappedIds;
            if (CasioKnowledgeLinux::TryConvertKeySequenceBetweenModels(
                    self->state_.calcIdx, srcIds, target, mappedIds)) {
                const auto& dstDefs = CasioLinux::GetKeysForModel(static_cast<CasioLinux::CalcModelId>(target));
                for (size_t i = 0; i < mappedIds.size() && i < self->state_.steps.size(); ++i) {
                    self->state_.steps[i].keyId = mappedIds[i];
                    for (const auto& dk : dstDefs) {
                        if (dk.id == mappedIds[i]) {
                            self->state_.steps[i].keyName = dk.name;
                            break;
                        }
                    }
                }
                self->state_.calcIdx = target;
                gtk_label_set_text(GTK_LABEL(self->statusLbl_),
                                   ("Secuencia convertida a " + CasioKnowledgeLinux::CalcModelDisplayName(target)).c_str());
            } else {
                self->state_.calcIdx = target;
                gtk_label_set_text(GTK_LABEL(self->statusLbl_),
                                   "Algunas teclas son especificas; mostrando modelo elegido.");
            }
            self->RefreshPreview();
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(mRow), modelCombo_, TRUE, TRUE, 0);

    // Editable Title & Notes with live LaTeX validation
    gtk_box_pack_start(GTK_BOX(leftCol), gtk_label_new("Titulo de la guia:"), FALSE, FALSE, 0);
    titleEntry_ = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(titleEntry_), state_.title.c_str());
    gtk_box_pack_start(GTK_BOX(leftCol), titleEntry_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(leftCol), gtk_label_new("Notas / Enunciado ($...$ LaTeX):"), FALSE, FALSE, 0);
    notesEntry_ = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(notesEntry_), state_.notes.c_str());
    gtk_box_pack_start(GTK_BOX(leftCol), notesEntry_, FALSE, FALSE, 0);

    latexStatusLbl_ = gtk_label_new("LaTeX: OK");
    gtk_widget_set_halign(latexStatusLbl_, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(latexStatusLbl_), "muted-label");
    gtk_box_pack_start(GTK_BOX(leftCol), latexStatusLbl_, FALSE, FALSE, 0);

    auto onTextChg = G_CALLBACK(+[](GtkEditable*, gpointer ud) {
        auto* self = static_cast<SequenceExporterWindow*>(ud);
        if (!self->updatingUi_) {
            self->PullWidgetsToState();
            self->RefreshPreview();
        }
    });
    g_signal_connect(titleEntry_, "changed", onTextChg, this);
    g_signal_connect(notesEntry_, "changed", onTextChg, this);

    // Options checkboxes
    chkNums_ = gtk_check_button_new_with_label("Numerar teclas (#1, #2...)");
    chkLcd_ = gtk_check_button_new_with_label("Mostrar resultado LCD");
    chkLatex_ = gtk_check_button_new_with_label("Mostrar formula LaTeX 2D");
    chkNotes_ = gtk_check_button_new_with_label("Incluir notas / observaciones");
    chkSummary_ = gtk_check_button_new_with_label("Mostrar lista lineal de pasos");
    chkLightSheet_ = gtk_check_button_new_with_label("Hoja blanca para impresion");
    chkHideAns_ = gtk_check_button_new_with_label("Modo Practica (ocultar LCD)");

    GtkWidget* chks[] = {chkNums_, chkLcd_, chkLatex_, chkNotes_, chkSummary_, chkLightSheet_, chkHideAns_};
    bool defs[] = {true, true, true, true, true, true, false};
    for (int i = 0; i < 7; ++i) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chks[i]), defs[i]);
        g_signal_connect(chks[i], "toggled", G_CALLBACK(+[](GtkToggleButton*, gpointer ud) {
            auto* self = static_cast<SequenceExporterWindow*>(ud);
            if (!self->updatingUi_) {
                self->PullWidgetsToState();
                self->RefreshPreview();
            }
        }), this);
        gtk_box_pack_start(GTK_BOX(leftCol), chks[i], FALSE, FALSE, 0);
    }

    // Export Action Buttons
    GtkWidget* btnCopyImg = gtk_button_new_with_label("Copiar Hoja PNG al Portapapeles");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnCopyImg), "accent-btn");
    g_signal_connect(btnCopyImg, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* self = static_cast<SequenceExporterWindow*>(ud);
        CasioThemeLinux::CairoSurfacePtr surf(self->RenderSheetSurface(920));
        if (CopyCairoSurfaceToClipboard(surf.get())) {
            gtk_label_set_text(GTK_LABEL(self->statusLbl_), "Hoja PNG copiada al portapapeles.");
        }
    }), this);
    gtk_box_pack_start(GTK_BOX(leftCol), btnCopyImg, FALSE, FALSE, 2);

    GtkWidget* btnCopyTex = gtk_button_new_with_label("Copiar LaTeX + Pasos (Texto)");
    g_signal_connect(btnCopyTex, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* self = static_cast<SequenceExporterWindow*>(ud);
        std::ostringstream oss;
        oss << self->state_.title << "\n"
            << self->state_.latexExpr << "\nPasos: ";
        for (size_t i = 0; i < self->state_.steps.size(); ++i) {
            if (!self->state_.steps[i].included) continue;
            oss << "[" << self->state_.steps[i].keyName << "] ";
        }
        CopyTextToClipboard(oss.str());
        gtk_label_set_text(GTK_LABEL(self->statusLbl_), "Expresion LaTeX y pasos copiados como texto.");
    }), this);
    gtk_box_pack_start(GTK_BOX(leftCol), btnCopyTex, FALSE, FALSE, 0);

    GtkWidget* btnSave = gtk_button_new_with_label("Exportar PNG + Strip + Markdown");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnSave), "success-btn");
    g_signal_connect(btnSave, "clicked", G_CALLBACK((+[](GtkButton*, gpointer ud) {
        auto* self = static_cast<SequenceExporterWindow*>(ud);
        std::string defaultDir = self->paths_.rootDir.empty()
            ? "."
            : (std::filesystem::path(self->paths_.rootDir) / "guias").string();
        std::error_code ec;
        std::filesystem::create_directories(defaultDir, ec);

        GtkWidget* dlg = gtk_file_chooser_dialog_new(
            "Seleccionar carpeta para guardar Guia PNG + Strip + Markdown",
            GTK_WINDOW(self->window_),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "Cancelar", GTK_RESPONSE_CANCEL,
            "Guardar aqui", GTK_RESPONSE_ACCEPT,
            nullptr);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), defaultDir.c_str());
        if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
            gchar* chosen = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
            if (chosen) {
                std::string pngPath;
                std::string stripPath;
                std::string mdPath;
                if (self->ExportBundleToDirectory(chosen, &pngPath, &stripPath, &mdPath)) {
                    gtk_label_set_text(GTK_LABEL(self->statusLbl_),
                                       ("Exportado en: " + std::filesystem::path(mdPath).filename().string()).c_str());
                } else {
                    gtk_label_set_text(GTK_LABEL(self->statusLbl_), "Error al guardar archivos.");
                }
                g_free(chosen);
            }
        }
        gtk_widget_destroy(dlg);
    })), this);
    gtk_box_pack_start(GTK_BOX(leftCol), btnSave, FALSE, FALSE, 0);

    GtkWidget* btnStudio = gtk_button_new_with_label("Abrir en Document Studio");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnStudio), "violet-btn");
    g_signal_connect(btnStudio, "clicked", G_CALLBACK((+[](GtkButton*, gpointer ud) {
        auto* self = static_cast<SequenceExporterWindow*>(ud);
        std::string outDir = self->paths_.rootDir.empty()
            ? "/tmp"
            : (std::filesystem::path(self->paths_.rootDir) / "guias").string();
        std::string sheetPng;
        std::string stripPng;
        std::string mdPath;
        self->ExportBundleToDirectory(outDir, &sheetPng, &stripPng, &mdPath);
        if (self->onOpenStudio_) self->onOpenStudio_(mdPath);
    })), this);
    gtk_box_pack_start(GTK_BOX(leftCol), btnStudio, FALSE, FALSE, 0);

    statusLbl_ = gtk_label_new("Haz clic sobre cualquier tecla en la vista previa para incluir/ocultar.");
    gtk_label_set_line_wrap(GTK_LABEL(statusLbl_), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(statusLbl_), "muted-label");
    gtk_box_pack_end(GTK_BOX(leftCol), statusLbl_, FALSE, FALSE, 0);

    // Right live sheet preview
    GtkWidget* sc = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(root), sc, TRUE, TRUE, 0);

    previewArea_ = gtk_drawing_area_new();
    gtk_widget_set_size_request(previewArea_, 920, 620);
    gtk_widget_add_events(previewArea_, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(previewArea_, "draw", G_CALLBACK(OnPreviewDraw), this);
    g_signal_connect(previewArea_, "button-press-event", G_CALLBACK(OnPreviewClick), this);
    gtk_container_add(GTK_CONTAINER(sc), previewArea_);

    ApplyPreset(0);
}

void SequenceExporterWindow::SyncFromHistory(const std::vector<KeyLogStep>& history,
                                             int activeCalcIdx,
                                             const std::vector<uint8_t>& activeLcdRgba, int activeW, int activeH,
                                             uint32_t activeInd) {
    cachedHistory_ = history;
    state_.calcIdx = std::clamp(activeCalcIdx, 0, 3);
    state_.finalLcdRgba = activeLcdRgba;
    state_.finalLcdW = activeW;
    state_.finalLcdH = activeH;
    state_.finalIndicators = activeInd;

    int minStep = 1, maxStep = 1;
    if (!cachedHistory_.empty()) {
        minStep = cachedHistory_.front().stepNum;
        maxStep = cachedHistory_.back().stepNum;
    }
    int selEnd = maxStep;
    int selStart = std::max(minStep, maxStep - 11);
    for (int i = static_cast<int>(cachedHistory_.size()) - 1; i >= 0; --i) {
        if (cachedHistory_[i].calcIdx == state_.calcIdx) {
            selEnd = cachedHistory_[i].stepNum;
            selStart = selEnd;
            int count = 1;
            for (int j = i - 1; j >= 0 && count < 12; --j) {
                if (cachedHistory_[j].calcIdx != state_.calcIdx) break;
                selStart = cachedHistory_[j].stepNum;
                ++count;
            }
            break;
        }
    }
    updatingUi_ = true;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(spinStart_), minStep, std::max(minStep, maxStep));
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(spinEnd_), minStep, std::max(minStep, maxStep));
    state_.rangeStart = selStart;
    state_.rangeEnd = selEnd;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinStart_), state_.rangeStart);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinEnd_), state_.rangeEnd);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(modelCombo_), std::to_string(state_.calcIdx).c_str());
    updatingUi_ = false;

    RebuildFromRange();
}

void SequenceExporterWindow::LoadCustomSequence(int calcIdx,
                                                const std::vector<int>& keys,
                                                const std::string& title,
                                                const std::string& notes) {
    cachedHistory_.clear();
    state_.calcIdx = std::clamp(calcIdx, 0, 3);
    state_.title = title;
    state_.notes = notes;
    state_.steps.clear();

    const auto& defs = CasioLinux::GetKeysForModel(static_cast<CasioLinux::CalcModelId>(state_.calcIdx));
    for (int kid : keys) {
        ExporterStepItem item{};
        item.keyId = kid;
        item.keyName = "K" + std::to_string(kid);
        for (const auto& kd : defs) {
            if (kd.id == kid) { item.keyName = kd.name; break; }
        }
        state_.steps.push_back(std::move(item));
    }

    updatingUi_ = true;
    gtk_entry_set_text(GTK_ENTRY(titleEntry_), state_.title.c_str());
    gtk_entry_set_text(GTK_ENTRY(notesEntry_), state_.notes.c_str());
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(modelCombo_), std::to_string(state_.calcIdx).c_str());
    updatingUi_ = false;
    RefreshPreview();
}

void SequenceExporterWindow::Show() {
    if (!window_) return;
    gtk_widget_show_all(window_);
    gtk_window_present(GTK_WINDOW(window_));
}

void SequenceExporterWindow::UpdateSkinSurfaces(const std::array<cairo_surface_t*, 4>& skinSurfaces) {
    skinSurfaces_ = skinSurfaces;
    if (previewArea_) gtk_widget_queue_draw(previewArea_);
}

void SequenceExporterWindow::RefreshTheme() {
    if (previewArea_) gtk_widget_queue_draw(previewArea_);
}

void SequenceExporterWindow::ApplyPreset(int presetId) {
    state_.presetId = presetId;
    updatingUi_ = true;
    for (int i = 0; i < 4; ++i) {
        if (!presetBtns_[i]) continue;
        auto* ctx = gtk_widget_get_style_context(presetBtns_[i]);
        if (i == presetId) gtk_style_context_add_class(ctx, "pill-active");
        else gtk_style_context_remove_class(ctx, "pill-active");
    }
    if (presetId == 0) { // Explicada
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNums_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLcd_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLatex_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNotes_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkSummary_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLightSheet_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkHideAns_), FALSE);
    } else if (presetId == 1) { // Rapida
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNums_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLcd_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLatex_), FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNotes_), FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkSummary_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLightSheet_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkHideAns_), FALSE);
    } else if (presetId == 2) { // Practica
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNums_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLcd_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLatex_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNotes_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkSummary_), FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLightSheet_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkHideAns_), TRUE);
    } else { // Presentacion
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNums_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLcd_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLatex_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkNotes_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkSummary_), TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkLightSheet_), FALSE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkHideAns_), FALSE);
    }
    updatingUi_ = false;
    PullWidgetsToState();
    RefreshPreview();
}

void SequenceExporterWindow::RebuildFromRange() {
    if (cachedHistory_.empty()) {
        RefreshPreview();
        return;
    }
    int s = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinStart_));
    int e = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinEnd_));
    if (s > e) std::swap(s, e);
    state_.rangeStart = s;
    state_.rangeEnd = e;
    state_.steps.clear();

    int lastCalc = state_.calcIdx;
    for (const auto& st : cachedHistory_) {
        if (st.stepNum < s || st.stepNum > e) continue;
        ExporterStepItem item{};
        item.keyId = st.keyId;
        item.keyName = st.keyName;
        item.included = true;
        state_.steps.push_back(std::move(item));
        lastCalc = st.calcIdx;
        if (!st.lcdRgba.empty()) {
            state_.finalLcdRgba = st.lcdRgba;
            state_.finalLcdW = st.lcdW;
            state_.finalLcdH = st.lcdH;
            state_.finalIndicators = st.indicators;
        }
    }
    state_.calcIdx = lastCalc;
    RefreshPreview();
}

void SequenceExporterWindow::PullWidgetsToState() {
    state_.title = gtk_entry_get_text(GTK_ENTRY(titleEntry_));
    state_.notes = gtk_entry_get_text(GTK_ENTRY(notesEntry_));
    state_.showStepNums = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkNums_)) != FALSE;
    state_.showFinalLcd = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkLcd_)) != FALSE;
    state_.showLatex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkLatex_)) != FALSE;
    state_.showNotes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkNotes_)) != FALSE;
    state_.showSummary = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkSummary_)) != FALSE;
    state_.lightSheet = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkLightSheet_)) != FALSE;
    state_.hideAnswer = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkHideAns_)) != FALSE;

    auto valTitle = CasioLatexCairo::ValidateRichLatexInput(state_.title);
    auto valNotes = CasioLatexCairo::ValidateRichLatexInput(state_.notes);
    if (!valTitle.ok) gtk_label_set_text(GTK_LABEL(latexStatusLbl_), ("Titulo: " + valTitle.message).c_str());
    else if (!valNotes.ok) gtk_label_set_text(GTK_LABEL(latexStatusLbl_), ("Notas: " + valNotes.message).c_str());
    else gtk_label_set_text(GTK_LABEL(latexStatusLbl_), "LaTeX: Sintaxis valida");
}

void SequenceExporterWindow::RefreshPreview() {
    std::vector<std::string> activeNames;
    for (const auto& s : state_.steps) {
        if (s.included) activeNames.push_back(s.keyName);
    }
    auto conv = CasioKnowledgeLinux::ConvertKeyNamesToLinearAndLatex(activeNames);
    state_.linearExpr = conv.linearText;
    state_.latexExpr = conv.latexDisplay;
    if (previewArea_) gtk_widget_queue_draw(previewArea_);
}

cairo_surface_t* SequenceExporterWindow::RenderKeyStripSurface() const {
    int count = 0;
    for (const auto& s : state_.steps) if (s.included) count++;
    if (count <= 0) count = 1;
    const int chipW = 68;
    const int chipH = 74;
    const int gap = 12;
    const int pad = 16;
    const int w = pad * 2 + count * chipW + std::max(0, count - 1) * gap;
    const int h = pad * 2 + chipH;

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t* cr = cairo_create(surf);
    CasioThemeLinux::RgbaColor bg = state_.lightSheet ? CasioThemeLinux::RgbaColor(255, 255, 255)
                                                      : CasioThemeLinux::Dark::Surface1;
    CasioThemeLinux::CairoSetSource(cr, bg);
    cairo_paint(cr);

    cairo_surface_t* skin = (state_.calcIdx >= 0 && state_.calcIdx < 4) ? skinSurfaces_[state_.calcIdx] : nullptr;
    int x = pad;
    int idxNum = 1;
    for (const auto& st : state_.steps) {
        if (!st.included) continue;
        DrawKeyChipSurfaceOrFallback(cr, skin, state_.calcIdx, st.keyId, st.keyName,
                                     x, pad, chipW, chipH, true, state_.lightSheet);
        if (state_.showStepNums) {
            std::string badge = "#" + std::to_string(idxNum++);
            CasioThemeLinux::FillRoundedRect(cr, x + 2, pad - 4, 24, 15, 5.0, CasioThemeLinux::Accent());
            CasioLatexCairo::DrawLatexRichTextCentered(cr, badge, x + 2, pad - 4, 24, 15, 9.5,
                                                       CasioThemeLinux::RgbaColor(255, 255, 255), true, false);
        }
        x += chipW + gap;
    }
    cairo_destroy(cr);
    return surf;
}

cairo_surface_t* SequenceExporterWindow::RenderSheetSurface(int widthPx) const {
    const int W = std::max(720, widthPx);
    // Estimate layout height cleanly
    const int chipSize = 58;
    const int chipGap = 12;
    const int pad = 28;
    const int perRow = std::max(4, (W - pad * 2) / (chipSize + chipGap));
    const int numSteps = static_cast<int>(state_.steps.size());
    const int rows = std::max(1, (numSteps + perRow - 1) / perRow);
    int H = 130 + rows * (chipSize + 24);
    if (state_.showLatex) H += 74;
    if (state_.showFinalLcd) H += 150;
    if (state_.showNotes && !state_.notes.empty()) H += 68;
    if (state_.showSummary) H += 52;
    H = std::max(420, H);

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t* cr = cairo_create(surf);

    const bool light = state_.lightSheet;
    const auto bgCol = light ? CasioThemeLinux::RgbaColor(252, 253, 255) : CasioThemeLinux::Dark::CanvasBg;
    const auto cardBg = light ? CasioThemeLinux::RgbaColor(244, 247, 252) : CasioThemeLinux::Dark::Surface1;
    const auto borderCol = light ? CasioThemeLinux::RgbaColor(214, 222, 235) : CasioThemeLinux::Dark::BorderSubtle;
    const auto txtMain = light ? CasioThemeLinux::RgbaColor(18, 24, 34) : CasioThemeLinux::Dark::TextPrimary;
    const auto txtSec = light ? CasioThemeLinux::RgbaColor(88, 100, 118) : CasioThemeLinux::Dark::TextSecondary;
    const auto accCol = light ? CasioThemeLinux::Light::AccentBlue : CasioThemeLinux::Dark::AccentBlue;

    CasioThemeLinux::CairoSetSource(cr, bgCol);
    cairo_paint(cr);

    // Header
    CasioThemeLinux::FillRoundedRect(cr, pad, 20, W - pad * 2, 68, 12.0, cardBg);
    CasioThemeLinux::StrokeRoundedRect(cr, pad, 20, W - pad * 2, 68, 12.0, borderCol, 1.2);
    CasioThemeLinux::FillRoundedRect(cr, pad + 14, 30, 178, 22, 6.0, accCol);
    CasioLatexCairo::DrawLatexRichTextCentered(
        cr, CasioKnowledgeLinux::CalcModelDisplayName(state_.calcIdx),
        pad + 14, 30, 178, 22, 11.0, CasioThemeLinux::RgbaColor(255, 255, 255), true, false);
    CasioLatexCairo::DrawLatexRichText(
        cr, state_.title.empty() ? "Guia CASIO" : state_.title,
        pad + 14, 58, W - pad * 2 - 28, 30, 18.0, txtMain, true, false);

    double curY = 104.0;

    // Key chips grid
    previewKeyRects_.clear();
    cairo_surface_t* skin = (state_.calcIdx >= 0 && state_.calcIdx < 4) ? skinSurfaces_[state_.calcIdx] : nullptr;
    int col = 0;
    double rowY = curY;
    int activeNum = 1;
    for (int i = 0; i < numSteps; ++i) {
        double kx = pad + col * (chipSize + chipGap);
        DrawKeyChipSurfaceOrFallback(cr, skin, state_.calcIdx,
                                     state_.steps[i].keyId, state_.steps[i].keyName,
                                     kx, rowY + 8, chipSize, chipSize,
                                     state_.steps[i].included, light);
        previewKeyRects_.push_back({static_cast<int>(kx), static_cast<int>(rowY),
                                    static_cast<int>(kx + chipSize), static_cast<int>(rowY + chipSize + 10)});
        if (state_.showStepNums && state_.steps[i].included) {
            std::string b = "#" + std::to_string(activeNum++);
            CasioThemeLinux::FillRoundedRect(cr, kx, rowY, 24, 15, 5.0, accCol);
            CasioLatexCairo::DrawLatexRichTextCentered(
                cr, b, kx, rowY, 24, 15, 9.5, CasioThemeLinux::RgbaColor(255, 255, 255), true, false);
        }
        col++;
        if (col >= perRow && i + 1 < numSteps) {
            col = 0;
            rowY += chipSize + 22;
        }
    }
    curY = rowY + chipSize + 24;

    if (state_.showLatex) {
        CasioThemeLinux::FillRoundedRect(cr, pad, curY, W - pad * 2, 58, 10.0, cardBg);
        CasioThemeLinux::StrokeRoundedRect(cr, pad, curY, W - pad * 2, 58, 10.0, borderCol, 1.0);
        CasioLatexCairo::DrawLatexRichTextCentered(
            cr, state_.latexExpr.empty() ? "$0$" : state_.latexExpr,
            pad + 14, curY + 6, W - pad * 2 - 28, 46, 19.0, accCol, false, false);
        curY += 72.0;
    }

    if (state_.showFinalLcd) {
        const double lcdBoxW = 340.0;
        const double lcdBoxH = (state_.calcIdx == 2) ? 140.0 : 124.0;
        const double lx = pad;
        CasioThemeLinux::FillRoundedRect(cr, lx, curY, W - pad * 2, lcdBoxH + 16, 10.0, cardBg);
        CasioThemeLinux::StrokeRoundedRect(cr, lx, curY, W - pad * 2, lcdBoxH + 16, 10.0, borderCol, 1.0);

        if (state_.hideAnswer) {
            CasioThemeLinux::FillRoundedRect(cr, lx + 12, curY + 8, lcdBoxW, lcdBoxH, 6.0,
                                             CasioThemeLinux::RgbaColor(180, 192, 206));
            CasioLatexCairo::DrawLatexRichTextCentered(
                cr, "Modo Practica: respuesta oculta",
                lx + 12, curY + 8, lcdBoxW, lcdBoxH, 14.0, txtMain, true, false);
        } else {
            DrawCompositeLcdOntoCairo(cr, state_.calcIdx, lx + 12, curY + 8, lcdBoxW, lcdBoxH,
                                      state_.finalLcdRgba, state_.finalLcdW, state_.finalLcdH,
                                      state_.finalIndicators, state_.linearExpr, "", true, false);
        }

        if (state_.showSummary) {
            std::ostringstream sum;
            sum << "Secuencia: ";
            for (const auto& s : state_.steps) {
                if (s.included) sum << "[" << s.keyName << "] ";
            }
            CasioLatexCairo::DrawLatexRichText(
                cr, sum.str(), lx + lcdBoxW + 28, curY + 14,
                W - pad * 2 - lcdBoxW - 44, lcdBoxH, 13.0, txtMain, false, false);
        }
        curY += lcdBoxH + 30.0;
    }

    if (state_.showNotes && !state_.notes.empty()) {
        CasioLatexCairo::DrawLatexRichText(
            cr, "Nota: " + state_.notes, pad + 4, curY, W - pad * 2 - 8, 60,
            13.5, txtSec, false, false);
    }

    cairo_destroy(cr);
    return surf;
}

bool SequenceExporterWindow::ExportBundleToDirectory(const std::string& outDir,
                                                     std::string* outSheetPng,
                                                     std::string* outStripPng,
                                                     std::string* outMarkdown) const {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(outDir, ec);
    const std::string stamp = CurrentTimestampSlug();

    const fs::path sheetFile = fs::path(outDir) / ("guia_" + stamp + ".png");
    const fs::path stripFile = fs::path(outDir) / ("tira_pasos_" + stamp + ".png");
    const fs::path mdFile    = fs::path(outDir) / ("guia_" + stamp + ".md");

    CasioThemeLinux::CairoSurfacePtr sheet(RenderSheetSurface(920));
    CasioThemeLinux::CairoSurfacePtr strip(RenderKeyStripSurface());
    bool okSheet = CasioThemeLinux::SurfaceToPngFile(sheet.get(), sheetFile.string());
    bool okStrip = CasioThemeLinux::SurfaceToPngFile(strip.get(), stripFile.string());

    std::ofstream md(mdFile);
    if (!md.is_open()) return false;
    md << "# " << (state_.title.empty() ? "Guia Paso a Paso CASIO" : state_.title) << "\n\n";
    md << "**Modelo:** " << CasioKnowledgeLinux::CalcModelDisplayName(state_.calcIdx) << "\n\n";
    if (!state_.latexExpr.empty()) {
        md << "## Expresion Matematica\n\n" << state_.latexExpr << "\n\n";
    }
    md << "## Secuencia de Teclas\n\n";
    int n = 1;
    for (const auto& s : state_.steps) {
        if (!s.included) continue;
        md << n++ << ". `[" << s.keyName << "]`\n";
    }
    md << "\n";
    if (okStrip) md << "### Tira de Teclas\n\n![" << stripFile.filename().string() << "](" << stripFile.filename().string() << ")\n\n";
    if (okSheet) md << "### Hoja Completa\n\n![" << sheetFile.filename().string() << "](" << sheetFile.filename().string() << ")\n\n";
    if (!state_.notes.empty()) md << "## Notas\n\n" << state_.notes << "\n";

    if (outSheetPng) *outSheetPng = sheetFile.string();
    if (outStripPng) *outStripPng = stripFile.string();
    if (outMarkdown) *outMarkdown = mdFile.string();
    return okSheet;
}

gboolean SequenceExporterWindow::OnPreviewDraw(GtkWidget* w, cairo_t* cr, gpointer ud) {
    auto* self = static_cast<SequenceExporterWindow*>(ud);
    const int allocW = gtk_widget_get_allocated_width(w);
    CasioThemeLinux::CairoSetSource(cr, CasioThemeLinux::StageBg());
    cairo_paint(cr);

    CasioThemeLinux::CairoSurfacePtr surf(self->RenderSheetSurface(std::max(840, allocW - 24)));
    if (surf) {
        int sw = cairo_image_surface_get_width(surf.get());
        double ox = std::max(8.0, 0.5 * (allocW - sw));
        cairo_set_source_surface(cr, surf.get(), ox, 8.0);
        cairo_paint(cr);
    }
    return TRUE;
}

gboolean SequenceExporterWindow::OnPreviewClick(GtkWidget* w, GdkEventButton* ev, gpointer ud) {
    auto* self = static_cast<SequenceExporterWindow*>(ud);
    const int allocW = gtk_widget_get_allocated_width(w);
    const double ox = std::max(8.0, 0.5 * (allocW - std::max(840, allocW - 24)));
    const int px = static_cast<int>(ev->x - ox);
    const int py = static_cast<int>(ev->y - 8.0);

    for (size_t i = 0; i < self->previewKeyRects_.size() && i < self->state_.steps.size(); ++i) {
        const auto& r = self->previewKeyRects_[i];
        if (px >= r.left && px <= r.right && py >= r.top && py <= r.bottom) {
            self->state_.steps[i].included = !self->state_.steps[i].included;
            self->RefreshPreview();
            return TRUE;
        }
    }
    return FALSE;
}

} // namespace CasioExporterGtk
