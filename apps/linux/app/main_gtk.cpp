// ============================================================================
// main_gtk.cpp
// Main Native GTK3 + Cairo Application: calculadora-unificada
// Hosts 4 interactive CASIO calculators (fx-570LA X Blanca, fx-570VN PLUS Gris,
// fx-CG50 PRIZM, fx-991CW Negra), Pop-up Display, KeyLog + Step Player Drawer,
// Aprender CASIO (2D PangoCairo LaTeX + 16 Poppler PDFs), Guide Exporter,
// Theme toggle (Porcelana Claro / Graphite Studio), & Document Studio launcher.
// ============================================================================

#include "../engines/unified_engines.hpp"
#include "exporter_gtk.hpp"
#include "knowledge_catalog_linux.hpp"
#include "latex_cairo.hpp"
#include "manual_viewer_gtk.hpp"
#include "ui_theme_linux.hpp"

#include <cairo.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using CasioLinux::AssetPaths;
using CasioLinux::CalcModelId;
using CasioLinux::KeyDef;
using CasioLinux::SkinRect;
using CasioLinux::UnifiedCalcEngine;
using CasioThemeLinux::ViewMode;

struct CliOptions {
    bool hasViewOverride = false;
    ViewMode initialView = ViewMode::Cw2;
    bool compactMode = false;
    bool openPopup = false;
    bool launchStudio = false;
    bool smokeTest = false;
    std::string helpSearchQuery;
    std::string screenshotOutPath;
};

struct CalcUiSlot {
    UnifiedCalcEngine engine;
    CasioThemeLinux::CairoSurfacePtr skinSurface;
    int skinW = 0;
    int skinH = 0;
    bool visible = false;

    std::vector<uint8_t> lcdRgba;
    int lcdW = 0;
    int lcdH = 0;
    uint32_t indicators = 0;

    // Layout viewport on main stage drawing area
    double viewX = 0, viewY = 0, viewW = 0, viewH = 0;
};

struct CaptureThumb {
    int calcIdx = 0;
    std::string timestamp;
    CasioThemeLinux::CairoSurfacePtr surface;
};

struct RamUndoSlot {
    bool valid = false;
    int calcIdx = 0;
    std::vector<uint8_t> data;
    std::string timestamp;
};

struct AppState {
    AssetPaths paths;
    CasioThemeLinux::AppSettings settings;
    CasioKnowledgeLinux::KnowledgeCatalog catalog;

    std::array<CalcUiSlot, 4> calcs;
    std::array<RamUndoSlot, 4> ramSnapshots;
    int activeCalcIdx = 3;

    // Interaction & presentation highlight
    int hoverCalcIdx = -1;
    int hoverKeyId = -1;
    int pressedCalcIdx = -1;
    int pressedKeyId = -1;
    int lastKeyCalcIdx = -1;
    int lastKeyId = -1;

    // History, Captures & Step Player
    std::vector<CasioExporterGtk::KeyLogStep> keyHistory;
    std::vector<CaptureThumb> captures;
    int nextStepNum = 1;
    int playerCursor = -1; // 0-based into keyHistory
    bool playerPlaying = false;
    bool playerSyncRam = true;
    int playerSpeedMs = 500;
    guint playerTimerId = 0;
    guint simTimerId = 0;
    guint screenshotTimerId = 0;
    int screenshotWaitTicks = 0;

    // Companion Windows
    CasioManualGtk::PdfManualWindow pdfWindow;
    CasioManualGtk::SmartHelpWindow smartHelpWindow;
    CasioExporterGtk::PopupDisplayWindow popupWindow;
    CasioExporterGtk::SequenceExporterWindow exporterWindow;

    // GTK Main Widgets
    GtkWidget* window = nullptr;
    GtkWidget* stageArea = nullptr;
    GtkWidget* statusLabel = nullptr;
    GtkWidget* cw2ActionBox = nullptr;
    GtkWidget* cw2WineBtn = nullptr;
    GtkWidget* cw2DiagLabel = nullptr;

    // View buttons & toggle pills
    std::array<GtkWidget*, 6> viewBtns{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    GtkWidget* compactBtn = nullptr;
    GtkWidget* drawerBtn = nullptr;
    GtkWidget* pinBtn = nullptr;
    GtkWidget* themeBtn = nullptr;

    // Sequence Drawer widgets
    GtkWidget* drawerPanel = nullptr;
    GtkWidget* drawerNotebook = nullptr;
    GtkWidget* drawerLatexCanvas = nullptr;
    GtkWidget* drawerKeysListBox = nullptr;
    GtkWidget* drawerCapturesListBox = nullptr;
    GtkWidget* playerPosLabel = nullptr;
    GtkWidget* playerNoteEntry = nullptr;
    GtkWidget* playerPlayBtn = nullptr;
    GtkWidget* playerSyncCheck = nullptr;

    bool updatingUi = false;
    CliOptions cli;
};

std::string NowHms() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

void SetStatus(AppState* app, const std::string& msg) {
    if (app && app->statusLabel) {
        gtk_label_set_text(GTK_LABEL(app->statusLabel), msg.c_str());
    }
}

void LaunchDocumentStudioProcessArgs(AppState* app, const std::vector<std::string>& extraArgs) {
    namespace fs = std::filesystem;
    std::vector<std::string> candidates;
    if (!app->paths.documentStudioBin.empty()) candidates.push_back(app->paths.documentStudioBin);
    {
        std::error_code ec;
        fs::path selfExe = fs::read_symlink("/proc/self/exe", ec);
        if (!ec && !selfExe.empty()) {
            fs::path selfDir = selfExe.parent_path();
            candidates.push_back((selfDir / "document-studio").string());
            candidates.push_back((selfDir.parent_path() / "doc_studio_app" / "document-studio").string());
        }
    }
    if (!app->paths.rootDir.empty()) {
        candidates.push_back((fs::path(app->paths.rootDir) / "linux" / "build" / "studio" / "document-studio").string());
        candidates.push_back((fs::path(app->paths.rootDir) / "linux" / "build" / "doc_studio_app" / "document-studio").string());
        candidates.push_back((fs::path(app->paths.rootDir) / "document-studio").string());
    }
    candidates.push_back("./document-studio");

    std::string exePath;
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(c, ec)) {
            exePath = c;
            break;
        }
    }

    if (exePath.empty()) {
        SetStatus(app, "Document Studio no encontrado en ruta local (./document-studio).");
        return;
    }

    std::vector<gchar*> argv;
    argv.reserve(extraArgs.size() + 2);
    argv.push_back(g_strdup(exePath.c_str()));
    for (const auto& arg : extraArgs) {
        if (!arg.empty()) argv.push_back(g_strdup(arg.c_str()));
    }
    argv.push_back(nullptr);

    GError* err = nullptr;
    gboolean ok = g_spawn_async(nullptr, argv.data(), nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &err);
    for (gchar* p : argv) g_free(p);
    if (!ok) {
        std::string msg = "Error lanzando Document Studio";
        if (err && err->message) msg += std::string(": ") + err->message;
        if (err) g_error_free(err);
        SetStatus(app, msg);
    } else {
        SetStatus(app, "Document Studio iniciado.");
    }
}

void LaunchDocumentStudioProcess(AppState* app, const std::string& extraArg = "") {
    if (extraArg.empty()) {
        LaunchDocumentStudioProcessArgs(app, {});
    } else {
        LaunchDocumentStudioProcessArgs(app, { "--open", extraArg });
    }
}

std::array<cairo_surface_t*, 4> RawSkinPtrs(const AppState* app) {
    return {
        app->calcs[0].skinSurface.get(),
        app->calcs[1].skinSurface.get(),
        app->calcs[2].skinSurface.get(),
        app->calcs[3].skinSurface.get()
    };
}

void PullEngineLcd(AppState* app, int calcIdx, bool force = false) {
    if (calcIdx < 0 || calcIdx > 3) return;
    auto& c = app->calcs[calcIdx];
    if (!c.engine.IsReady()) return;
    int w = c.engine.ScreenWidth();
    int h = c.engine.ScreenHeight();
    if (w <= 0 || h <= 0) return;
    c.lcdW = w;
    c.lcdH = h;
    c.engine.RenderRgba(c.lcdRgba, &c.indicators, force);
}

void SyncPopupWindow(AppState* app) {
    if (!app->popupWindow.IsVisible()) return;
    const int idx = std::clamp(app->activeCalcIdx, 0, 3);
    const auto& c = app->calcs[idx];
    app->popupWindow.UpdateFrame(
        idx, c.lcdRgba, c.lcdW, c.lcdH, c.indicators,
        idx == 3 ? c.engine.GetFallbackExpression() : "",
        idx == 3 ? c.engine.GetFallbackResult() : "");
}

void ApplyViewModeLayout(AppState* app) {
    const auto vm = app->settings.viewMode;
    for (int i = 0; i < 4; ++i) app->calcs[i].visible = false;

    switch (vm) {
    case ViewMode::Blanca:     app->calcs[0].visible = true; app->activeCalcIdx = 0; break;
    case ViewMode::Gris:       app->calcs[1].visible = true; app->activeCalcIdx = 1; break;
    case ViewMode::Cg50:       app->calcs[2].visible = true; app->activeCalcIdx = 2; break;
    case ViewMode::Cw2:        app->calcs[3].visible = true; app->activeCalcIdx = 3; break;
    case ViewMode::BlancaGris:
        app->calcs[0].visible = true;
        app->calcs[1].visible = true;
        if (app->activeCalcIdx != 0 && app->activeCalcIdx != 1) app->activeCalcIdx = 0;
        break;
    case ViewMode::Todas:
        for (int i = 0; i < 4; ++i) app->calcs[i].visible = true;
        break;
    }

    if (app->settings.compactMode && (vm == ViewMode::BlancaGris || vm == ViewMode::Todas)) {
        // Compact mode focuses one vertical calculator
        for (int i = 0; i < 4; ++i) app->calcs[i].visible = (i == app->activeCalcIdx);
    }
    app->settings.activeCalcIdx = app->activeCalcIdx;

    for (int i = 0; i < 6; ++i) {
        if (!app->viewBtns[i]) continue;
        GtkStyleContext* ctx = gtk_widget_get_style_context(app->viewBtns[i]);
        if (i == static_cast<int>(vm)) gtk_style_context_add_class(ctx, "pill-active");
        else gtk_style_context_remove_class(ctx, "pill-active");
    }

    if (app->cw2ActionBox) {
        if (app->calcs[3].visible) gtk_widget_show_all(app->cw2ActionBox);
        else gtk_widget_hide(app->cw2ActionBox);
        if (app->cw2DiagLabel) {
            gtk_label_set_text(GTK_LABEL(app->cw2DiagLabel),
                               app->calcs[3].engine.StatusText().c_str());
        }
    }

    if (app->drawerPanel) {
        gtk_widget_set_visible(app->drawerPanel, app->settings.drawerOpen ? TRUE : FALSE);
    }
    if (app->stageArea) gtk_widget_queue_draw(app->stageArea);
    SyncPopupWindow(app);
}

void RebuildDrawerLists(AppState* app) {
    if (!app->drawerKeysListBox) return;

    // Clear existing rows in key history list
    GList* kids = gtk_container_get_children(GTK_CONTAINER(app->drawerKeysListBox));
    for (GList* l = kids; l != nullptr; l = l->next) {
        gtk_container_remove(GTK_CONTAINER(app->drawerKeysListBox), GTK_WIDGET(l->data));
    }
    g_list_free(kids);

    int startIdx = std::max(0, static_cast<int>(app->keyHistory.size()) - 60);
    for (int i = static_cast<int>(app->keyHistory.size()) - 1; i >= startIdx; --i) {
        const auto& st = app->keyHistory[i];
        std::ostringstream oss;
        oss << "#" << st.stepNum << " [" << st.keyName << "] · "
            << (st.calcIdx == 0 ? "Blanca" : st.calcIdx == 1 ? "Gris" : st.calcIdx == 2 ? "CG50" : "991CW")
            << " (" << st.timestampStr << ")";
        if (!st.note.empty()) oss << " — " << st.note;

        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* lbl = gtk_label_new(oss.str().c_str());
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_margin_start(lbl, 6);
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        gtk_container_add(GTK_CONTAINER(row), lbl);
        g_object_set_data(G_OBJECT(row), "step-idx", GINT_TO_POINTER(i));
        gtk_container_add(GTK_CONTAINER(app->drawerKeysListBox), row);
    }
    gtk_widget_show_all(app->drawerKeysListBox);

    if (app->playerPosLabel) {
        std::ostringstream ps;
        if (app->keyHistory.empty()) ps << "Paso 0 / 0";
        else ps << "Paso " << (app->playerCursor + 1) << " / " << app->keyHistory.size();
        gtk_label_set_text(GTK_LABEL(app->playerPosLabel), ps.str().c_str());
    }
    if (app->drawerLatexCanvas) gtk_widget_queue_draw(app->drawerLatexCanvas);
}

void RebuildCapturesList(AppState* app) {
    if (!app->drawerCapturesListBox) return;
    GList* kids = gtk_container_get_children(GTK_CONTAINER(app->drawerCapturesListBox));
    for (GList* l = kids; l != nullptr; l = l->next) {
        gtk_container_remove(GTK_CONTAINER(app->drawerCapturesListBox), GTK_WIDGET(l->data));
    }
    g_list_free(kids);

    for (int i = static_cast<int>(app->captures.size()) - 1; i >= 0; --i) {
        const auto& cap = app->captures[i];
        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_margin_start(hbox, 6);
        gtk_widget_set_margin_end(hbox, 6);
        gtk_widget_set_margin_top(hbox, 4);
        gtk_widget_set_margin_bottom(hbox, 4);

        std::string txt = CasioKnowledgeLinux::CalcModelDisplayName(cap.calcIdx) + " (" + cap.timestamp + ")";
        GtkWidget* lbl = gtk_label_new(txt.c_str());
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);

        GtkWidget* btnCopy = gtk_button_new_with_label("Copiar");
        g_object_set_data(G_OBJECT(btnCopy), "cap-idx", GINT_TO_POINTER(i));
        g_signal_connect(btnCopy, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            int ci = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "cap-idx"));
            if (ci >= 0 && ci < static_cast<int>(a->captures.size())) {
                if (CasioExporterGtk::CopyCairoSurfaceToClipboard(a->captures[ci].surface.get())) {
                    SetStatus(a, "Captura LCD copiada al portapapeles.");
                }
            }
        }), app);
        gtk_box_pack_end(GTK_BOX(hbox), btnCopy, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(app->drawerCapturesListBox), hbox);
    }
    gtk_widget_show_all(app->drawerCapturesListBox);
}

void CaptureCurrentLcdThumbnail(AppState* app, int calcIdx) {
    if (calcIdx < 0 || calcIdx > 3) return;
    PullEngineLcd(app, calcIdx, true);
    const auto& c = app->calcs[calcIdx];
    if (c.lcdRgba.empty() || c.lcdW <= 0 || c.lcdH <= 0) return;

    CaptureThumb thumb{};
    thumb.calcIdx = calcIdx;
    thumb.timestamp = NowHms();
    thumb.surface.reset(CasioThemeLinux::RgbaToCairoSurface(c.lcdRgba.data(), c.lcdW, c.lcdH));
    app->captures.push_back(std::move(thumb));
    if (app->captures.size() > 24) app->captures.erase(app->captures.begin());
    RebuildCapturesList(app);
    SetStatus(app, "Captura LCD agregada a pestana Capturas.");
}

void RecordAndExecuteKeyTap(AppState* app, int calcIdx, int keyId, bool fastSettle = false) {
    if (calcIdx < 0 || calcIdx > 3) return;
    auto& c = app->calcs[calcIdx];
    app->activeCalcIdx = calcIdx;
    app->lastKeyCalcIdx = calcIdx;
    app->lastKeyId = keyId;

    const auto& defs = CasioLinux::GetKeysForModel(static_cast<CalcModelId>(calcIdx));
    std::string kName = "K" + std::to_string(keyId);
    uint16_t matrixCode = 0;
    for (const auto& kd : defs) {
        if (kd.id == keyId) {
            kName = kd.name;
            matrixCode = kd.matrixCode;
            break;
        }
    }

    if (calcIdx == 3) {
        std::string note;
        c.engine.SendKeyToCw2OrFallback(keyId, &note);
        if (!note.empty()) SetStatus(app, note);
    } else if (c.engine.IsReady()) {
        const int holdMs = 150;
        const int settleMs = (keyId == 49 || keyId == 47 || keyId == 32) ? 260 : (fastSettle ? 200 : 180);
        if (matrixCode != 0) c.engine.TapKey(matrixCode, holdMs, settleMs);
        else c.engine.TapKeyById(keyId, holdMs, settleMs);
    }
    PullEngineLcd(app, calcIdx, true);

    CasioExporterGtk::KeyLogStep step{};
    step.stepNum = app->nextStepNum++;
    step.calcIdx = calcIdx;
    step.keyId = keyId;
    step.keyName = kName;
    step.friendlyLabel = kName;
    step.timestampStr = NowHms();
    if (c.engine.IsReady() && calcIdx < 2) {
        step.ramState = c.engine.ExportState();
    }
    step.lcdRgba = c.lcdRgba;
    step.lcdW = c.lcdW;
    step.lcdH = c.lcdH;
    step.indicators = c.indicators;

    app->keyHistory.push_back(std::move(step));
    if (app->keyHistory.size() > 300) {
        app->keyHistory.erase(app->keyHistory.begin());
    }
    app->playerCursor = static_cast<int>(app->keyHistory.size()) - 1;

    RebuildDrawerLists(app);
    SyncPopupWindow(app);
    if (app->stageArea) gtk_widget_queue_draw(app->stageArea);
}

void ReplaySequenceOnEngine(AppState* app, int calcIdx, const std::vector<int>& keys, const std::string& label) {
    if (calcIdx < 0 || calcIdx > 3 || keys.empty()) return;
    // Switch view to reveal target calculator
    if (calcIdx == 0) app->settings.viewMode = ViewMode::Blanca;
    else if (calcIdx == 1) app->settings.viewMode = ViewMode::Gris;
    else if (calcIdx == 2) app->settings.viewMode = ViewMode::Cg50;
    else app->settings.viewMode = ViewMode::Cw2;
    ApplyViewModeLayout(app);

    for (int kid : keys) {
        RecordAndExecuteKeyTap(app, calcIdx, kid, true);
    }
    SetStatus(app, "Secuencia ejecutada (" + std::to_string(keys.size()) + " pasos): " + label);
}

void StepPlayerJumpTo(AppState* app, int targetIdx) {
    if (app->keyHistory.empty()) return;
    app->playerCursor = std::clamp(targetIdx, 0, static_cast<int>(app->keyHistory.size()) - 1);
    const auto& st = app->keyHistory[app->playerCursor];
    app->activeCalcIdx = st.calcIdx;
    app->lastKeyCalcIdx = st.calcIdx;
    app->lastKeyId = st.keyId;

    if (app->playerSyncRam && !st.ramState.empty() && st.calcIdx < 2 && app->calcs[st.calcIdx].engine.IsReady()) {
        app->calcs[st.calcIdx].engine.ImportState(st.ramState);
        PullEngineLcd(app, st.calcIdx, true);
    }
    if (app->playerNoteEntry) {
        gtk_entry_set_text(GTK_ENTRY(app->playerNoteEntry), st.note.c_str());
    }
    RebuildDrawerLists(app);
    SyncPopupWindow(app);
    if (app->stageArea) gtk_widget_queue_draw(app->stageArea);
}

gboolean OnPlayerTick(gpointer ud) {
    auto* app = static_cast<AppState*>(ud);
    if (!app->playerPlaying || app->keyHistory.empty()) {
        app->playerPlaying = false;
        app->playerTimerId = 0;
        if (app->playerPlayBtn) gtk_button_set_label(GTK_BUTTON(app->playerPlayBtn), "Play");
        return G_SOURCE_REMOVE;
    }
    if (app->playerCursor + 1 >= static_cast<int>(app->keyHistory.size())) {
        app->playerPlaying = false;
        app->playerTimerId = 0;
        if (app->playerPlayBtn) gtk_button_set_label(GTK_BUTTON(app->playerPlayBtn), "Play");
        return G_SOURCE_REMOVE;
    }
    StepPlayerJumpTo(app, app->playerCursor + 1);
    return G_SOURCE_CONTINUE;
}

void RenderStageCalculators(AppState* app, cairo_t* cr, int width, int height) {
    CasioThemeLinux::CairoSetSource(cr, CasioThemeLinux::StageBg());
    cairo_paint(cr);

    std::vector<int> visIdx;
    for (int i = 0; i < 4; ++i) {
        if (app->calcs[i].visible) visIdx.push_back(i);
    }
    if (visIdx.empty()) return;

    const int n = static_cast<int>(visIdx.size());
    const int cols = (n <= 1) ? 1 : ((n == 2) ? 2 : (width > 1220 ? 4 : 2));
    const int rows = (n + cols - 1) / cols;
    const double outerPad = 14.0;
    const double gap = 14.0;
    const double cellW = std::max(120.0, (width - outerPad * 2.0 - gap * (cols - 1)) / cols);
    const double cellH = std::max(220.0, (height - outerPad * 2.0 - gap * (rows - 1)) / rows);

    for (int k = 0; k < n; ++k) {
        const int idx = visIdx[k];
        auto& c = app->calcs[idx];
        const int col = k % cols;
        const int row = k / cols;
        const double cx = outerPad + col * (cellW + gap);
        const double cy = outerPad + row * (cellH + gap);

        // Card container backing
        const bool isActive = (idx == app->activeCalcIdx && n > 1);
        CasioThemeLinux::DrawSoftShadow(cr, cx, cy, cellW, cellH, 12.0);
        CasioThemeLinux::FillRoundedRect(cr, cx, cy, cellW, cellH, 12.0, CasioThemeLinux::Surface1());
        CasioThemeLinux::StrokeRoundedRect(cr, cx, cy, cellW, cellH, 12.0,
                                           isActive ? CasioThemeLinux::Accent() : CasioThemeLinux::BorderSubtle(),
                                           isActive ? 2.0 : 1.0);

        // Card title header
        const double titleH = 26.0;
        std::string title = CasioKnowledgeLinux::CalcModelDisplayName(idx);
        CasioLatexCairo::DrawLatexRichText(
            cr, title, cx + 12.0, cy + 6.0, cellW - 24.0, 20.0, 12.5,
            isActive ? CasioThemeLinux::AccentText() : CasioThemeLinux::TextSecondary(), true, false);

        const double bodyPad = 8.0;
        const double availW = std::max(60.0, cellW - bodyPad * 2.0);
        const double availH = std::max(100.0, cellH - titleH - bodyPad * 2.0);
        const int natW = c.skinW > 0 ? c.skinW : CasioLinux::GetSkinWidth(static_cast<CalcModelId>(idx));
        const int natH = c.skinH > 0 ? c.skinH : CasioLinux::GetSkinHeight(static_cast<CalcModelId>(idx));

        double dstW = availW, dstH = availH;
        const double natAspect = static_cast<double>(natW) / std::max(1, natH);
        if (availW / availH > natAspect) {
            dstH = availH;
            dstW = dstH * natAspect;
        } else {
            dstW = availW;
            dstH = dstW / natAspect;
        }
        const double vx = cx + bodyPad + 0.5 * (availW - dstW);
        const double vy = cy + titleH + bodyPad + 0.5 * (availH - dstH);
        c.viewX = vx;
        c.viewY = vy;
        c.viewW = dstW;
        c.viewH = dstH;

        const double scaleX = dstW / natW;
        const double scaleY = dstH / natH;

        // Draw authentic chassis skin or clean procedural fallback chassis
        if (c.skinSurface) {
            cairo_save(cr);
            cairo_translate(cr, vx, vy);
            cairo_scale(cr, scaleX, scaleY);
            cairo_set_source_surface(cr, c.skinSurface.get(), 0.0, 0.0);
            cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
            cairo_paint(cr);
            cairo_restore(cr);
        } else {
            CasioThemeLinux::FillRoundedRect(cr, vx, vy, dstW, dstH, 16.0,
                                             idx == 0 ? CasioThemeLinux::RgbaColor(240, 242, 245)
                                                      : CasioThemeLinux::RgbaColor(42, 48, 60));
            CasioThemeLinux::StrokeRoundedRect(cr, vx, vy, dstW, dstH, 16.0,
                                               CasioThemeLinux::BorderStrong(), 1.5);
        }

        // Composite Live LCD inside exact GetLcdSkinRect(model)
        const SkinRect lcdNat = CasioLinux::GetLcdSkinRect(static_cast<CalcModelId>(idx));
        const double lx = vx + lcdNat.left * scaleX;
        const double ly = vy + lcdNat.top * scaleY;
        const double lw = lcdNat.Width() * scaleX;
        const double lh = lcdNat.Height() * scaleY;

        const auto bgCol = CasioThemeLinux::GetLcdBgColor(idx);
        const auto fgCol = CasioThemeLinux::GetLcdFgColor(idx);

        CasioThemeLinux::FillRoundedRect(cr, lx, ly, lw, lh, 2.0, bgCol);

        if (idx == 0 || idx == 1) {
            const auto sub = CasioThemeLinux::ComputeLcdIndAndDotRects(idx, lx, ly, lw, lh);
            std::vector<uint8_t> indRgba;
            int iw = 0, ih = 0;
            UnifiedCalcEngine::RenderIndicatorStripRgba(
                static_cast<CalcModelId>(idx), c.indicators,
                fgCol.r, fgCol.g, fgCol.b, bgCol.r, bgCol.g, bgCol.b,
                indRgba, iw, ih);
            if (!indRgba.empty() && iw > 0 && ih > 0) {
                CasioThemeLinux::CairoSurfacePtr isurf(CasioThemeLinux::RgbaToCairoSurface(indRgba.data(), iw, ih));
                if (isurf) {
                    cairo_save(cr);
                    cairo_translate(cr, sub.indX, sub.indY);
                    cairo_scale(cr, sub.indW / iw, sub.indH / ih);
                    cairo_set_source_surface(cr, isurf.get(), 0.0, 0.0);
                    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
                    cairo_paint(cr);
                    cairo_restore(cr);
                }
            }
            if (!c.lcdRgba.empty() && c.lcdW > 0 && c.lcdH > 0) {
                CasioThemeLinux::CairoSurfacePtr dsurf(CasioThemeLinux::RgbaToCairoSurface(c.lcdRgba.data(), c.lcdW, c.lcdH));
                if (dsurf) {
                    cairo_save(cr);
                    cairo_translate(cr, sub.dotX, sub.dotY);
                    cairo_scale(cr, sub.dotW / c.lcdW, sub.dotH / c.lcdH);
                    cairo_set_source_surface(cr, dsurf.get(), 0.0, 0.0);
                    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
                    cairo_paint(cr);
                    cairo_restore(cr);
                }
            }
        } else {
            // CG50 & CW2
            if (!c.lcdRgba.empty() && c.lcdW > 0 && c.lcdH > 0) {
                CasioThemeLinux::CairoSurfacePtr dsurf(CasioThemeLinux::RgbaToCairoSurface(c.lcdRgba.data(), c.lcdW, c.lcdH));
                if (dsurf) {
                    cairo_save(cr);
                    cairo_translate(cr, lx, ly);
                    cairo_scale(cr, lw / c.lcdW, lh / c.lcdH);
                    cairo_set_source_surface(cr, dsurf.get(), 0.0, 0.0);
                    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
                    cairo_paint(cr);
                    cairo_restore(cr);
                }
            }
            if (idx == 3 && !c.engine.UsingExternalProcess()) {
                std::string expr = c.engine.GetFallbackExpression();
                std::string res  = c.engine.GetFallbackResult();
                if (!expr.empty() || !res.empty()) {
                    CasioLatexCairo::DrawLatexRichText(
                        cr, expr, lx + 8.0, ly + 8.0, lw - 16.0, lh * 0.44,
                        std::max(10.0, lh * 0.16), fgCol, false, true);
                    CasioLatexCairo::DrawLatexRichText(
                        cr, res, lx + 8.0, ly + lh * 0.56, lw - 16.0, lh * 0.36,
                        std::max(11.0, lh * 0.19), fgCol, true, true);
                }
            }
        }

        // Hover / Press / Teacher Presentation key highlights
        const auto& keys = CasioLinux::GetKeysForModel(static_cast<CalcModelId>(idx));
        auto drawKeyHighlight = [&](int targetKeyId, CasioThemeLinux::RgbaColor strokeCol, double fillAlpha, double lwPx) {
            if (targetKeyId < 0) return;
            for (const auto& kd : keys) {
                if (kd.id == targetKeyId) {
                    const double kw = std::max(12.0, kd.w * scaleX);
                    const double kh = std::max(10.0, kd.h * scaleY);
                    const double kx = vx + kd.cx * scaleX - kw * 0.5;
                    const double ky = vy + kd.cy * scaleY - kh * 0.5;
                    CasioThemeLinux::FillRoundedRect(cr, kx, ky, kw, kh, 5.0, strokeCol, fillAlpha);
                    CasioThemeLinux::StrokeRoundedRect(cr, kx, ky, kw, kh, 5.0, strokeCol, lwPx);
                    break;
                }
            }
        };

        if (app->settings.teacherHighlight && app->lastKeyCalcIdx == idx && app->lastKeyId >= 0) {
            drawKeyHighlight(app->lastKeyId, CasioThemeLinux::Amber(), 0.16, 2.2);
        }
        if (app->hoverCalcIdx == idx && app->hoverKeyId >= 0) {
            drawKeyHighlight(app->hoverKeyId, CasioThemeLinux::Accent(), 0.18, 1.8);
        }
        if (app->pressedCalcIdx == idx && app->pressedKeyId >= 0) {
            drawKeyHighlight(app->pressedKeyId, CasioThemeLinux::Success(), 0.32, 2.4);
        }
    }
}

bool HitTestStageKey(const AppState* app, double mx, double my, int& outCalcIdx, int& outKeyId, std::string& outKeyName) {
    outCalcIdx = -1;
    outKeyId = -1;
    outKeyName.clear();
    for (int i = 0; i < 4; ++i) {
        const auto& c = app->calcs[i];
        if (!c.visible || c.viewW <= 0.0 || c.viewH <= 0.0) continue;
        if (mx < c.viewX || mx > c.viewX + c.viewW || my < c.viewY || my > c.viewY + c.viewH) continue;

        const int natW = c.skinW > 0 ? c.skinW : CasioLinux::GetSkinWidth(static_cast<CalcModelId>(i));
        const int natH = c.skinH > 0 ? c.skinH : CasioLinux::GetSkinHeight(static_cast<CalcModelId>(i));
        const double sx = (mx - c.viewX) * (static_cast<double>(natW) / c.viewW);
        const double sy = (my - c.viewY) * (static_cast<double>(natH) / c.viewH);

        const auto& keys = CasioLinux::GetKeysForModel(static_cast<CalcModelId>(i));
        for (const auto& kd : keys) {
            const double hw = std::max(14.0, kd.w * 0.52);
            const double hh = std::max(12.0, kd.h * 0.52);
            if (std::abs(sx - kd.cx) <= hw && std::abs(sy - kd.cy) <= hh) {
                outCalcIdx = i;
                outKeyId = kd.id;
                outKeyName = kd.name;
                return true;
            }
        }
        outCalcIdx = i;
        return false;
    }
    return false;
}

int MapGdkKeyToCalcKeyId(int calcIdx, guint keyval) {
    if (calcIdx == 3) {
        switch (keyval) {
        case GDK_KEY_0: case GDK_KEY_KP_0: return 43;
        case GDK_KEY_1: case GDK_KEY_KP_1: return 38;
        case GDK_KEY_2: case GDK_KEY_KP_2: return 39;
        case GDK_KEY_3: case GDK_KEY_KP_3: return 40;
        case GDK_KEY_4: case GDK_KEY_KP_4: return 33;
        case GDK_KEY_5: case GDK_KEY_KP_5: return 34;
        case GDK_KEY_6: case GDK_KEY_KP_6: return 35;
        case GDK_KEY_7: case GDK_KEY_KP_7: return 28;
        case GDK_KEY_8: case GDK_KEY_KP_8: return 29;
        case GDK_KEY_9: case GDK_KEY_KP_9: return 30;
        case GDK_KEY_plus: case GDK_KEY_KP_Add: return 41;
        case GDK_KEY_minus: case GDK_KEY_KP_Subtract: return 42;
        case GDK_KEY_asterisk: case GDK_KEY_KP_Multiply: return 36;
        case GDK_KEY_slash: case GDK_KEY_KP_Divide: return 37;
        case GDK_KEY_Return: case GDK_KEY_KP_Enter: case GDK_KEY_equal: return 47;
        case GDK_KEY_period: case GDK_KEY_KP_Decimal: return 44;
        case GDK_KEY_BackSpace: return 31;
        case GDK_KEY_Escape: case GDK_KEY_Delete: return 32;
        case GDK_KEY_Up: return 2;
        case GDK_KEY_Down: return 10;
        case GDK_KEY_Left: return 6;
        case GDK_KEY_Right: return 8;
        case GDK_KEY_s: case GDK_KEY_S: return 23;
        case GDK_KEY_c: case GDK_KEY_C: return 24;
        case GDK_KEY_t: case GDK_KEY_T: return 25;
        case GDK_KEY_l: case GDK_KEY_L: return 21;
        case GDK_KEY_parenleft: return 26;
        case GDK_KEY_parenright: return 27;
        default: return -1;
        }
    }

    switch (keyval) {
    case GDK_KEY_0: case GDK_KEY_KP_0: return 45;
    case GDK_KEY_1: case GDK_KEY_KP_1: return 40;
    case GDK_KEY_2: case GDK_KEY_KP_2: return 41;
    case GDK_KEY_3: case GDK_KEY_KP_3: return 42;
    case GDK_KEY_4: case GDK_KEY_KP_4: return 35;
    case GDK_KEY_5: case GDK_KEY_KP_5: return 36;
    case GDK_KEY_6: case GDK_KEY_KP_6: return 37;
    case GDK_KEY_7: case GDK_KEY_KP_7: return 30;
    case GDK_KEY_8: case GDK_KEY_KP_8: return 31;
    case GDK_KEY_9: case GDK_KEY_KP_9: return 32;
    case GDK_KEY_plus: case GDK_KEY_KP_Add: return 43;
    case GDK_KEY_minus: case GDK_KEY_KP_Subtract: return 44;
    case GDK_KEY_asterisk: case GDK_KEY_KP_Multiply: return 38;
    case GDK_KEY_slash: case GDK_KEY_KP_Divide: return 39;
    case GDK_KEY_Return: case GDK_KEY_KP_Enter: case GDK_KEY_equal: return 49;
    case GDK_KEY_period: case GDK_KEY_KP_Decimal: return 46;
    case GDK_KEY_BackSpace: return 33;
    case GDK_KEY_Escape: case GDK_KEY_Delete: return 34;
    case GDK_KEY_Up:    return calcIdx == 2 ? 10 : 2;
    case GDK_KEY_Left:  return calcIdx == 2 ? 11 : (calcIdx == 1 ? 5 : 3);
    case GDK_KEY_Right: return calcIdx == 2 ? 12 : (calcIdx == 1 ? 6 : 4);
    case GDK_KEY_Down:  return calcIdx == 2 ? 13 : (calcIdx == 1 ? 7 : 5);
    case GDK_KEY_s: case GDK_KEY_S: return calcIdx == 2 ? 21 : 21;
    case GDK_KEY_c: case GDK_KEY_C: return 22;
    case GDK_KEY_t: case GDK_KEY_T: return 23;
    case GDK_KEY_l: case GDK_KEY_L: return 20;
    case GDK_KEY_parenleft: return 26;
    case GDK_KEY_parenright: return 27;
    default: return -1;
    }
}

void CopyActiveLcdAction(AppState* app, int calcIdx) {
    if (calcIdx < 0 || calcIdx > 3) calcIdx = app->activeCalcIdx;
    PullEngineLcd(app, calcIdx, true);
    const auto& c = app->calcs[calcIdx];
    std::vector<std::string> recentNames;
    for (const auto& s : app->keyHistory) {
        if (s.calcIdx == calcIdx) recentNames.push_back(s.keyName);
    }
    auto conv = CasioKnowledgeLinux::ConvertKeyNamesToLinearAndLatex(recentNames);
    std::string txt = (calcIdx == 3 && !c.engine.GetFallbackResult().empty())
        ? (c.engine.GetFallbackExpression() + " = " + c.engine.GetFallbackResult())
        : conv.latexDisplay;
    if (CasioExporterGtk::CopyCalculatorLcdToClipboard(
            calcIdx, c.lcdRgba, c.lcdW, c.lcdH, c.indicators, txt, app->settings.clipboardMode)) {
        SetStatus(app, "LCD y expresion copiados al portapapeles.");
    }
}

void SaveRamSnapshotAction(AppState* app) {
    const int idx = app->activeCalcIdx;
    if (idx >= 2 || !app->calcs[idx].engine.IsReady()) {
        SetStatus(app, "Instantanea RAM disponible en fx-570LA X y fx-570VN PLUS.");
        return;
    }
    app->ramSnapshots[idx].data = app->calcs[idx].engine.ExportState();
    app->ramSnapshots[idx].calcIdx = idx;
    app->ramSnapshots[idx].timestamp = NowHms();
    app->ramSnapshots[idx].valid = !app->ramSnapshots[idx].data.empty();
    SetStatus(app, app->ramSnapshots[idx].valid
              ? ("RAM guardada (" + std::to_string(app->ramSnapshots[idx].data.size()) + " bytes) a las " + app->ramSnapshots[idx].timestamp)
              : "No se pudo guardar RAM.");
}

void RestoreRamSnapshotAction(AppState* app) {
    const int idx = app->activeCalcIdx;
    if (idx >= 2 || !app->ramSnapshots[idx].valid || !app->calcs[idx].engine.IsReady()) {
        SetStatus(app, "No hay instantanea RAM guardada para este modelo.");
        return;
    }
    if (app->calcs[idx].engine.ImportState(app->ramSnapshots[idx].data)) {
        PullEngineLcd(app, idx, true);
        SyncPopupWindow(app);
        if (app->stageArea) gtk_widget_queue_draw(app->stageArea);
        SetStatus(app, "RAM restaurada (" + app->ramSnapshots[idx].timestamp + ").");
    }
}

bool CaptureStageScreenshotToPng(AppState* app, const std::string& pngPath) {
    if (!app || pngPath.empty()) return false;
    GtkWidget* targetWin = app->window;
    if (app->cli.openPopup && app->popupWindow.Window() && gtk_widget_get_visible(app->popupWindow.Window())) {
        targetWin = app->popupWindow.Window();
    } else if (!app->cli.helpSearchQuery.empty() && app->smartHelpWindow.Window() &&
               gtk_widget_get_visible(app->smartHelpWindow.Window())) {
        targetWin = app->smartHelpWindow.Window();
    }
    if (targetWin && gtk_widget_get_realized(targetWin) && gtk_widget_get_mapped(targetWin)) {
        gtk_container_check_resize(GTK_CONTAINER(targetWin));
        while (gtk_events_pending()) gtk_main_iteration();
        int w = gtk_widget_get_allocated_width(targetWin);
        int h = gtk_widget_get_allocated_height(targetWin);
        int sw = (targetWin == app->window && app->stageArea) ? gtk_widget_get_allocated_width(app->stageArea) : w;
        if (w >= 200 && h >= 160 && sw >= 100) {
            cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            cairo_t* cr = cairo_create(surf);
            CasioThemeLinux::CairoSetSource(cr, CasioThemeLinux::CanvasBg());
            cairo_paint(cr);
            gtk_widget_draw(targetWin, cr);
            cairo_destroy(cr);
            bool ok = CasioThemeLinux::SurfaceToPngFile(surf, pngPath);
            cairo_surface_destroy(surf);
            return ok;
        }
    }
    int w = app->stageArea ? gtk_widget_get_allocated_width(app->stageArea) : 1120;
    int h = app->stageArea ? gtk_widget_get_allocated_height(app->stageArea) : 760;
    if (w < 400 || h < 480) {
        w = app->settings.compactMode ? 520 : 1120;
        h = 760;
    }
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t* cr = cairo_create(surf);
    RenderStageCalculators(app, cr, w, h);
    cairo_destroy(cr);
    bool ok = CasioThemeLinux::SurfaceToPngFile(surf, pngPath);
    cairo_surface_destroy(surf);
    return ok;
}

void InitializeEnginesAndSkins(AppState* app) {
    for (int i = 0; i < 4; ++i) {
        const auto m = static_cast<CalcModelId>(i);
        std::string err;
        app->calcs[i].engine.Initialize(m, app->paths, &err);

        const std::string skinPath = CasioThemeLinux::ResolveSkinPath(m, app->paths);
        if (!skinPath.empty()) {
            const int cropW = (i == 3) ? 992 : 0;
            const int cropH = (i == 3) ? 2082 : 0;
            app->calcs[i].skinSurface.reset(
                CasioThemeLinux::LoadSkinSurfaceClean(
                    skinPath, cropW, cropH, app->calcs[i].skinW, app->calcs[i].skinH));
        }
        if (app->calcs[i].skinW <= 0) app->calcs[i].skinW = CasioLinux::GetSkinWidth(m);
        if (app->calcs[i].skinH <= 0) app->calcs[i].skinH = CasioLinux::GetSkinHeight(m);

        // Complete initial boot ticks on nX-U8 / fx-CG50 PRIZM / CW2 engines
        if (app->calcs[i].engine.IsReady()) {
            const int bootMs = (i == 2) ? 3500 : (i < 2 ? 480 : 600);
            for (int t = 0; t < bootMs; t += 100) {
                app->calcs[i].engine.StepMs(100);
            }
            PullEngineLcd(app, i, true);
        }
    }
}

gboolean OnSimulationTimer(gpointer ud) {
    auto* app = static_cast<AppState*>(ud);
    bool anyDirty = false;
    for (int i = 0; i < 4; ++i) {
        auto& c = app->calcs[i];
        if (!c.visible || !c.engine.IsReady()) continue;
        c.engine.StepMs(16);
        PullEngineLcd(app, i, false);
        anyDirty = true;
    }
    if (anyDirty && app->stageArea) {
        gtk_widget_queue_draw(app->stageArea);
        SyncPopupWindow(app);
    }
    return G_SOURCE_CONTINUE;
}

GtkWidget* BuildMainUi(AppState* app) {
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), "Calculadora Unificada CASIO — Core Oficial (Linux GTK3)");
    gtk_window_set_default_size(GTK_WINDOW(app->window),
                                app->settings.compactMode ? 520 : 1240, 820);
    if (app->settings.alwaysOnTop) gtk_window_set_keep_above(GTK_WINDOW(app->window), TRUE);

    g_signal_connect(app->window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        if (a->simTimerId) { g_source_remove(a->simTimerId); a->simTimerId = 0; }
        if (a->playerTimerId) { g_source_remove(a->playerTimerId); a->playerTimerId = 0; }
        if (a->screenshotTimerId) { g_source_remove(a->screenshotTimerId); a->screenshotTimerId = 0; }
        CasioThemeLinux::SaveUnifiedSettings(a->settings);
        if (gtk_main_level() > 0) gtk_main_quit();
    }), app);

    GtkWidget* rootVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->window), rootVBox);

    // Top Navigation / Header Bar
    GtkWidget* headerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(headerBox), "studio-header");
    gtk_box_pack_start(GTK_BOX(rootVBox), headerBox, FALSE, FALSE, 0);

    auto makeWrappingToolbar = []() -> GtkWidget* {
        GtkWidget* fb = gtk_flow_box_new();
        gtk_orientable_set_orientation(GTK_ORIENTABLE(fb), GTK_ORIENTATION_HORIZONTAL);
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb), GTK_SELECTION_NONE);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb), FALSE);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb), 4);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb), 4);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(fb), 1);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb), 12);
        return fb;
    };

    GtkWidget* row1 = makeWrappingToolbar();
    gtk_box_pack_start(GTK_BOX(headerBox), row1, FALSE, FALSE, 0);

    struct ViewBtnSpec { const char* label; ViewMode mode; };
    const ViewBtnSpec kViews[6] = {
        {"fx-991CW (Negra)", ViewMode::Cw2},
        {"fx-570LA X (Blanca)", ViewMode::Blanca},
        {"fx-570VN PLUS (Gris)", ViewMode::Gris},
        {"fx-CG50 PRIZM", ViewMode::Cg50},
        {"Blanca + Gris (2)", ViewMode::BlancaGris},
        {"Todas (4)", ViewMode::Todas}
    };
    for (int i = 0; i < 6; ++i) {
        int vmInt = static_cast<int>(kViews[i].mode);
        GtkWidget* b = gtk_button_new_with_label(kViews[i].label);
        app->viewBtns[vmInt] = b;
        g_object_set_data(G_OBJECT(b), "view-mode", GINT_TO_POINTER(vmInt));
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            a->settings.viewMode = static_cast<ViewMode>(
                GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "view-mode")));
            ApplyViewModeLayout(a);
            CasioThemeLinux::SaveUnifiedSettings(a->settings);
        }), app);
        gtk_container_add(GTK_CONTAINER(row1), b);
    }

    app->compactBtn = gtk_toggle_button_new_with_label("Modo Compacto");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->compactBtn), app->settings.compactMode ? TRUE : FALSE);
    g_signal_connect(app->compactBtn, "toggled", G_CALLBACK(+[](GtkToggleButton* tb, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        a->settings.compactMode = gtk_toggle_button_get_active(tb) != FALSE;
        if (a->settings.compactMode) {
            gtk_window_resize(GTK_WINDOW(a->window), a->settings.drawerOpen ? 860 : 520, 820);
        } else {
            gtk_window_resize(GTK_WINDOW(a->window), 1240, 820);
        }
        ApplyViewModeLayout(a);
        CasioThemeLinux::SaveUnifiedSettings(a->settings);
    }), app);
    gtk_container_add(GTK_CONTAINER(row1), app->compactBtn);

    app->pinBtn = gtk_toggle_button_new_with_label("Siempre visible");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->pinBtn), app->settings.alwaysOnTop ? TRUE : FALSE);
    g_signal_connect(app->pinBtn, "toggled", G_CALLBACK(+[](GtkToggleButton* tb, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        a->settings.alwaysOnTop = gtk_toggle_button_get_active(tb) != FALSE;
        gtk_window_set_keep_above(GTK_WINDOW(a->window), a->settings.alwaysOnTop ? TRUE : FALSE);
        CasioThemeLinux::SaveUnifiedSettings(a->settings);
    }), app);
    gtk_container_add(GTK_CONTAINER(row1), app->pinBtn);

    app->themeBtn = gtk_button_new_with_label(app->settings.darkTheme ? "Tema: Oscuro" : "Tema: Claro");
    g_signal_connect(app->themeBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        a->settings.darkTheme = !a->settings.darkTheme;
        CasioThemeLinux::ApplyGtkThemeCss(a->settings.darkTheme);
        gtk_button_set_label(btn, a->settings.darkTheme ? "Tema: Oscuro" : "Tema: Claro");
        a->popupWindow.RefreshTheme();
        a->pdfWindow.RefreshTheme();
        a->smartHelpWindow.RefreshTheme();
        a->exporterWindow.RefreshTheme();
        if (a->stageArea) gtk_widget_queue_draw(a->stageArea);
        if (a->drawerLatexCanvas) gtk_widget_queue_draw(a->drawerLatexCanvas);
        CasioThemeLinux::SaveUnifiedSettings(a->settings);
    }), app);
    gtk_container_add(GTK_CONTAINER(row1), app->themeBtn);

    // Row 2: Action Toolbar
    GtkWidget* row2 = makeWrappingToolbar();
    gtk_box_pack_start(GTK_BOX(headerBox), row2, FALSE, FALSE, 0);

    app->drawerBtn = gtk_toggle_button_new_with_label("Capturar pasos");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->drawerBtn), app->settings.drawerOpen ? TRUE : FALSE);
    g_signal_connect(app->drawerBtn, "toggled", G_CALLBACK(+[](GtkToggleButton* tb, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        a->settings.drawerOpen = gtk_toggle_button_get_active(tb) != FALSE;
        if (a->settings.compactMode && a->window) {
            gtk_window_resize(GTK_WINDOW(a->window), a->settings.drawerOpen ? 860 : 520, 820);
        }
        ApplyViewModeLayout(a);
        CasioThemeLinux::SaveUnifiedSettings(a->settings);
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), app->drawerBtn);

    GtkWidget* btnPopup = gtk_button_new_with_label("Pop-up Display");
    g_signal_connect(btnPopup, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        a->popupWindow.Show(a->activeCalcIdx);
        SyncPopupWindow(a);
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnPopup);

    GtkWidget* btnHelp = gtk_button_new_with_label("Aprender CASIO");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnHelp), "accent-btn");
    g_signal_connect(btnHelp, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        a->smartHelpWindow.ShowWithQuery("", -1, -1);
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnHelp);

    GtkWidget* btnManuals = gtk_button_new_with_label("Manuales (16)");
    g_signal_connect(btnManuals, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        a->pdfWindow.Show();
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnManuals);

    GtkWidget* btnExport = gtk_button_new_with_label("Exportador de Guias");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnExport), "success-btn");
    g_signal_connect(btnExport, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        const auto& c = a->calcs[a->activeCalcIdx];
        a->exporterWindow.SyncFromHistory(
            a->keyHistory, a->activeCalcIdx, c.lcdRgba, c.lcdW, c.lcdH, c.indicators);
        a->exporterWindow.Show();
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnExport);

    GtkWidget* btnStudio = gtk_button_new_with_label("Document Studio");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnStudio), "violet-btn");
    g_signal_connect(btnStudio, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        LaunchDocumentStudioProcess(static_cast<AppState*>(ud));
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnStudio);

    GtkWidget* btnSendStudio = gtk_button_new_with_label("Enviar -> Lienzo");
    gtk_widget_set_tooltip_text(btnSendStudio, "Enviar modelo y secuencia actual como Tarjeta CASIO a Document Studio");
    g_signal_connect(btnSendStudio, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        const std::string model = CasioKnowledgeLinux::CalcModelDisplayName(a->activeCalcIdx);
        std::string keys;
        std::string expr;
        int count = 0;
        for (const auto& step : a->keyHistory) {
            if (step.calcIdx != a->activeCalcIdx) continue;
            if (!keys.empty()) keys += " ";
            keys += "[" + step.keyName + "]";
            if (step.keyName.size() == 1 && (std::isdigit(static_cast<unsigned char>(step.keyName[0])) ||
                    step.keyName[0] == '+' || step.keyName[0] == '-' || step.keyName[0] == '*' ||
                    step.keyName[0] == '/' || step.keyName[0] == '.' || step.keyName[0] == '(' || step.keyName[0] == ')')) {
                expr += step.keyName;
            } else if (step.keyName == "×" || step.keyName == "x") {
                expr += "*";
            } else if (step.keyName == "÷") {
                expr += "/";
            }
            if (++count >= 16) break;
        }
        if (keys.empty()) keys = "[2] [+] [3] [=]";
        if (expr.empty()) expr = "2 + 3";
        const std::string spec = model + "|" + keys + "|" + expr;
        LaunchDocumentStudioProcessArgs(a, { "--mode", "canvas", "--calc-card", spec });
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnSendStudio);

    GtkWidget* btnCopyLcd = gtk_button_new_with_label("Copiar LCD");
    g_signal_connect(btnCopyLcd, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        CopyActiveLcdAction(a, a->activeCalcIdx);
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnCopyLcd);

    GtkWidget* btnResetRom = gtk_button_new_with_label("Reiniciar ROM");
    g_signal_connect(btnResetRom, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        auto& c = a->calcs[a->activeCalcIdx];
        if (c.engine.IsReady()) {
            c.engine.Reset();
            c.engine.StepMs(480);
            PullEngineLcd(a, a->activeCalcIdx, true);
            SyncPopupWindow(a);
            if (a->stageArea) gtk_widget_queue_draw(a->stageArea);
            SetStatus(a, "ROM reiniciada en " + CasioKnowledgeLinux::CalcModelDisplayName(a->activeCalcIdx));
        }
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnResetRom);

    GtkWidget* btnSaveRam = gtk_button_new_with_label("Guardar RAM");
    g_signal_connect(btnSaveRam, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        SaveRamSnapshotAction(static_cast<AppState*>(ud));
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnSaveRam);

    GtkWidget* btnRestoreRam = gtk_button_new_with_label("Restaurar RAM");
    g_signal_connect(btnRestoreRam, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        RestoreRamSnapshotAction(static_cast<AppState*>(ud));
    }), app);
    gtk_container_add(GTK_CONTAINER(row2), btnRestoreRam);

    // Optional CW2 Wine64 contextual bar
    app->cw2ActionBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_no_show_all(app->cw2ActionBox, TRUE);
    gtk_widget_set_margin_start(app->cw2ActionBox, 10);
    gtk_widget_set_margin_end(app->cw2ActionBox, 10);
    gtk_widget_set_margin_top(app->cw2ActionBox, 4);
    gtk_widget_set_margin_bottom(app->cw2ActionBox, 4);
    gtk_box_pack_start(GTK_BOX(rootVBox), app->cw2ActionBox, FALSE, FALSE, 0);

    app->cw2WineBtn = gtk_button_new_with_label("Abrir CasioEmuX (Wine64)");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->cw2WineBtn), "accent-btn");
    g_signal_connect(app->cw2WineBtn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
        auto* a = static_cast<AppState*>(ud);
        std::string diag;
        bool ok = a->calcs[3].engine.LaunchExternalCw2Window(&diag);
        SetStatus(a, diag.empty() ? (ok ? "CasioEmuX (Wine64) lanzado." : "Wine64 / CasioEmuX no disponible; usando fallback integrado.") : diag);
        if (a->cw2DiagLabel) {
            gtk_label_set_text(GTK_LABEL(a->cw2DiagLabel), a->calcs[3].engine.StatusText().c_str());
        }
    }), app);
    gtk_box_pack_start(GTK_BOX(app->cw2ActionBox), app->cw2WineBtn, FALSE, FALSE, 0);

    app->cw2DiagLabel = gtk_label_new("fx-991CW ClassWiz II");
    gtk_label_set_ellipsize(GTK_LABEL(app->cw2DiagLabel), PANGO_ELLIPSIZE_END);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->cw2DiagLabel), "muted-label");
    gtk_box_pack_start(GTK_BOX(app->cw2ActionBox), app->cw2DiagLabel, TRUE, TRUE, 0);

    // Center Split: Interactive Calculators Stage + Toggleable Sequence Capture Drawer
    GtkWidget* centerHBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(rootVBox), centerHBox, TRUE, TRUE, 0);

    app->stageArea = gtk_drawing_area_new();
    gtk_widget_set_hexpand(app->stageArea, TRUE);
    gtk_widget_set_vexpand(app->stageArea, TRUE);
    gtk_widget_add_events(app->stageArea,
                          GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_LEAVE_NOTIFY_MASK);

    g_signal_connect(app->stageArea, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer ud) -> gboolean {
        auto* a = static_cast<AppState*>(ud);
        RenderStageCalculators(a, cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w));
        return TRUE;
    }), app);

    g_signal_connect(app->stageArea, "motion-notify-event", G_CALLBACK((+[](GtkWidget* w, GdkEventMotion* ev, gpointer ud) -> gboolean {
        auto* a = static_cast<AppState*>(ud);
        int ci = -1;
        int ki = -1;
        std::string kn;
        HitTestStageKey(a, ev->x, ev->y, ci, ki, kn);
        if (ci != a->hoverCalcIdx || ki != a->hoverKeyId) {
            a->hoverCalcIdx = ci;
            a->hoverKeyId = ki;
            if (ki >= 0) {
                const auto* ke = a->catalog.FindKey(ci, ki);
                if (ke && ke->normal.available) {
                    SetStatus(a, "[" + kn + "] " + ke->normal.label + " — " + ke->normal.summary);
                } else {
                    SetStatus(a, "Tecla [" + kn + "] (" + CasioKnowledgeLinux::CalcModelDisplayName(ci) + ")");
                }
            }
            gtk_widget_queue_draw(w);
        }
        return TRUE;
    })), app);

    g_signal_connect(app->stageArea, "leave-notify-event", G_CALLBACK((+[](GtkWidget* w, GdkEventCrossing*, gpointer ud) -> gboolean {
        auto* a = static_cast<AppState*>(ud);
        if (a->hoverKeyId >= 0) {
            a->hoverCalcIdx = -1;
            a->hoverKeyId = -1;
            gtk_widget_queue_draw(w);
        }
        return TRUE;
    })), app);

    g_signal_connect(app->stageArea, "button-press-event", G_CALLBACK((+[](GtkWidget*, GdkEventButton* ev, gpointer ud) -> gboolean {
        auto* a = static_cast<AppState*>(ud);
        if (ev->button != 1) return FALSE;
        int ci = -1;
        int ki = -1;
        std::string kn;
        if (HitTestStageKey(a, ev->x, ev->y, ci, ki, kn) && ki >= 0) {
            a->pressedCalcIdx = ci;
            a->pressedKeyId = ki;
            RecordAndExecuteKeyTap(a, ci, ki, false);
            return TRUE;
        } else if (ci >= 0) {
            a->activeCalcIdx = ci;
            SyncPopupWindow(a);
            gtk_widget_queue_draw(a->stageArea);
        }
        return TRUE;
    })), app);

    g_signal_connect(app->stageArea, "button-release-event", G_CALLBACK(+[](GtkWidget* w, GdkEventButton*, gpointer ud) -> gboolean {
        auto* a = static_cast<AppState*>(ud);
        if (a->pressedKeyId >= 0) {
            a->pressedCalcIdx = -1;
            a->pressedKeyId = -1;
            gtk_widget_queue_draw(w);
        }
        return TRUE;
    }), app);

    gtk_box_pack_start(GTK_BOX(centerHBox), app->stageArea, TRUE, TRUE, 0);

    // Side Expandable Drawer: Live LaTeX Bar + Tabs (Teclas | Capturas | Pasos)
    app->drawerPanel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_size_request(app->drawerPanel, 336, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->drawerPanel), "studio-panel");
    gtk_container_set_border_width(GTK_CONTAINER(app->drawerPanel), 8);
    gtk_box_pack_end(GTK_BOX(centerHBox), app->drawerPanel, FALSE, FALSE, 0);

    app->drawerLatexCanvas = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->drawerLatexCanvas, -1, 56);
    g_signal_connect(app->drawerLatexCanvas, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer ud) -> gboolean {
        auto* a = static_cast<AppState*>(ud);
        const int width = gtk_widget_get_allocated_width(w);
        const int height = gtk_widget_get_allocated_height(w);
        CasioThemeLinux::FillRoundedRect(cr, 0, 0, width, height, 8.0, CasioThemeLinux::Surface2());
        CasioThemeLinux::StrokeRoundedRect(cr, 0.5, 0.5, width - 1, height - 1, 8.0, CasioThemeLinux::BorderSubtle(), 1.0);
        std::vector<std::string> names;
        int start = std::max(0, static_cast<int>(a->keyHistory.size()) - 20);
        for (int i = start; i < static_cast<int>(a->keyHistory.size()); ++i) {
            if (a->keyHistory[i].calcIdx == a->activeCalcIdx) names.push_back(a->keyHistory[i].keyName);
        }
        auto conv = CasioKnowledgeLinux::ConvertKeyNamesToLinearAndLatex(names);
        CasioLatexCairo::DrawLatexRichTextCentered(
            cr, conv.latexDisplay, 8.0, 4.0, width - 16.0, height - 8.0, 15.0,
            CasioThemeLinux::AccentText(), false, false);
        return TRUE;
    }), app);
    gtk_box_pack_start(GTK_BOX(app->drawerPanel), app->drawerLatexCanvas, FALSE, FALSE, 0);

    app->drawerNotebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(app->drawerPanel), app->drawerNotebook, TRUE, TRUE, 0);

    // Tab 1: Teclas
    {
        GtkWidget* tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget* sc = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        app->drawerKeysListBox = gtk_list_box_new();
        gtk_container_add(GTK_CONTAINER(sc), app->drawerKeysListBox);
        gtk_box_pack_start(GTK_BOX(tab), sc, TRUE, TRUE, 0);

        GtkWidget* rowBtns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget* btnClear = gtk_button_new_with_label("Limpiar Historial");
        g_signal_connect(btnClear, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            a->keyHistory.clear();
            a->playerCursor = -1;
            a->playerPlaying = false;
            RebuildDrawerLists(a);
            SetStatus(a, "Historial de teclas limpiado.");
        }), app);
        GtkWidget* btnExpNow = gtk_button_new_with_label("Exportar Pasos");
        gtk_style_context_add_class(gtk_widget_get_style_context(btnExpNow), "accent-btn");
        g_signal_connect(btnExpNow, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            const auto& c = a->calcs[a->activeCalcIdx];
            a->exporterWindow.SyncFromHistory(
                a->keyHistory, a->activeCalcIdx, c.lcdRgba, c.lcdW, c.lcdH, c.indicators);
            a->exporterWindow.Show();
        }), app);
        gtk_box_pack_start(GTK_BOX(rowBtns), btnClear, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(rowBtns), btnExpNow, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(tab), rowBtns, FALSE, FALSE, 2);

        gtk_notebook_append_page(GTK_NOTEBOOK(app->drawerNotebook), tab, gtk_label_new("Teclas"));
    }

    // Tab 2: Capturas
    {
        GtkWidget* tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        GtkWidget* btnSnap = gtk_button_new_with_label("Capturar LCD Activo");
        gtk_style_context_add_class(gtk_widget_get_style_context(btnSnap), "accent-btn");
        g_signal_connect(btnSnap, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            CaptureCurrentLcdThumbnail(a, a->activeCalcIdx);
        }), app);
        gtk_box_pack_start(GTK_BOX(tab), btnSnap, FALSE, FALSE, 2);

        GtkWidget* sc = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        app->drawerCapturesListBox = gtk_list_box_new();
        gtk_container_add(GTK_CONTAINER(sc), app->drawerCapturesListBox);
        gtk_box_pack_start(GTK_BOX(tab), sc, TRUE, TRUE, 0);
        gtk_notebook_append_page(GTK_NOTEBOOK(app->drawerNotebook), tab, gtk_label_new("Capturas"));
    }

    // Tab 3: Pasos (Step Player)
    {
        GtkWidget* tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width(GTK_CONTAINER(tab), 6);

        app->playerPosLabel = gtk_label_new("Paso 0 / 0");
        gtk_box_pack_start(GTK_BOX(tab), app->playerPosLabel, FALSE, FALSE, 0);

        GtkWidget* trRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GtkWidget* bFirst = gtk_button_new_with_label("|<");
        GtkWidget* bPrev  = gtk_button_new_with_label("<");
        app->playerPlayBtn = gtk_button_new_with_label("Play");
        GtkWidget* bNext  = gtk_button_new_with_label(">");
        GtkWidget* bLast  = gtk_button_new_with_label(">|");
        gtk_box_pack_start(GTK_BOX(trRow), bFirst, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(trRow), bPrev, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(trRow), app->playerPlayBtn, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(trRow), bNext, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(trRow), bLast, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(tab), trRow, FALSE, FALSE, 0);

        g_signal_connect(bFirst, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            StepPlayerJumpTo(static_cast<AppState*>(ud), 0);
        }), app);
        g_signal_connect(bPrev, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            StepPlayerJumpTo(a, a->playerCursor - 1);
        }), app);
        g_signal_connect(bNext, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            StepPlayerJumpTo(a, a->playerCursor + 1);
        }), app);
        g_signal_connect(bLast, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            StepPlayerJumpTo(a, static_cast<int>(a->keyHistory.size()) - 1);
        }), app);
        g_signal_connect(app->playerPlayBtn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            a->playerPlaying = !a->playerPlaying;
            if (a->playerTimerId) { g_source_remove(a->playerTimerId); a->playerTimerId = 0; }
            if (a->playerPlaying && !a->keyHistory.empty()) {
                if (a->playerCursor + 1 >= static_cast<int>(a->keyHistory.size())) a->playerCursor = -1;
                gtk_button_set_label(btn, "Pausa");
                a->playerTimerId = g_timeout_add(a->playerSpeedMs, OnPlayerTick, a);
            } else {
                a->playerPlaying = false;
                gtk_button_set_label(btn, "Play");
            }
        }), app);

        GtkWidget* spdCombo = gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(spdCombo), "900", "Velocidad: Lenta (0.9s)");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(spdCombo), "500", "Velocidad: Normal (0.5s)");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(spdCombo), "250", "Velocidad: Rapida (0.25s)");
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(spdCombo), "500");
        g_signal_connect(spdCombo, "changed", G_CALLBACK(+[](GtkComboBox* c, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            const gchar* id = gtk_combo_box_get_active_id(c);
            if (id) a->playerSpeedMs = std::clamp(std::atoi(id), 150, 2000);
            if (a->playerPlaying) {
                if (a->playerTimerId) g_source_remove(a->playerTimerId);
                a->playerTimerId = g_timeout_add(a->playerSpeedMs, OnPlayerTick, a);
            }
        }), app);
        gtk_box_pack_start(GTK_BOX(tab), spdCombo, FALSE, FALSE, 0);

        app->playerSyncCheck = gtk_check_button_new_with_label("Sincronizar RAM al recorrer");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->playerSyncCheck), TRUE);
        g_signal_connect(app->playerSyncCheck, "toggled", G_CALLBACK(+[](GtkToggleButton* tb, gpointer ud) {
            static_cast<AppState*>(ud)->playerSyncRam = gtk_toggle_button_get_active(tb) != FALSE;
        }), app);
        gtk_box_pack_start(GTK_BOX(tab), app->playerSyncCheck, FALSE, FALSE, 0);

        GtkWidget* btnRestoreStep = gtk_button_new_with_label("Restaurar Calculadora a Este Paso");
        g_signal_connect(btnRestoreStep, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            if (a->playerCursor >= 0 && a->playerCursor < static_cast<int>(a->keyHistory.size())) {
                const auto& st = a->keyHistory[a->playerCursor];
                if (!st.ramState.empty() && st.calcIdx < 2 && a->calcs[st.calcIdx].engine.IsReady()) {
                    a->calcs[st.calcIdx].engine.ImportState(st.ramState);
                    PullEngineLcd(a, st.calcIdx, true);
                    SyncPopupWindow(a);
                    gtk_widget_queue_draw(a->stageArea);
                    SetStatus(a, "Estado RAM restaurado al paso #" + std::to_string(st.stepNum));
                }
            }
        }), app);
        gtk_box_pack_start(GTK_BOX(tab), btnRestoreStep, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(tab), gtk_label_new("Nota del paso actual:"), FALSE, FALSE, 0);
        app->playerNoteEntry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(tab), app->playerNoteEntry, FALSE, FALSE, 0);

        GtkWidget* btnSaveNote = gtk_button_new_with_label("Guardar Nota en Paso");
        g_signal_connect(btnSaveNote, "clicked", G_CALLBACK(+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            if (a->playerCursor >= 0 && a->playerCursor < static_cast<int>(a->keyHistory.size())) {
                a->keyHistory[a->playerCursor].note = gtk_entry_get_text(GTK_ENTRY(a->playerNoteEntry));
                RebuildDrawerLists(a);
            }
        }), app);
        gtk_box_pack_start(GTK_BOX(tab), btnSaveNote, FALSE, FALSE, 0);

        GtkWidget* btnExportStepsPng = gtk_button_new_with_label("Exportar Strip PNG de Pasos");
        gtk_style_context_add_class(gtk_widget_get_style_context(btnExportStepsPng), "success-btn");
        g_signal_connect(btnExportStepsPng, "clicked", G_CALLBACK((+[](GtkButton*, gpointer ud) {
            auto* a = static_cast<AppState*>(ud);
            const auto& c = a->calcs[a->activeCalcIdx];
            a->exporterWindow.SyncFromHistory(a->keyHistory, a->activeCalcIdx, c.lcdRgba, c.lcdW, c.lcdH, c.indicators);
            std::string outDir = a->paths.rootDir.empty()
                ? "/tmp"
                : (std::filesystem::path(a->paths.rootDir) / "guias").string();
            std::string sheetPng;
            std::string stripPng;
            std::string mdPath;
            if (a->exporterWindow.ExportBundleToDirectory(outDir, &sheetPng, &stripPng, &mdPath)) {
                SetStatus(a, "Exportado Strip + Hoja en " + std::filesystem::path(stripPng).filename().string());
            }
        })), app);
        gtk_box_pack_start(GTK_BOX(tab), btnExportStepsPng, FALSE, FALSE, 0);

        gtk_notebook_append_page(GTK_NOTEBOOK(app->drawerNotebook), tab, gtk_label_new("Pasos"));
    }

    // Bottom status bar
    app->statusLabel = gtk_label_new("Listo · Core CASIO Linux activo.");
    gtk_label_set_ellipsize(GTK_LABEL(app->statusLabel), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(app->statusLabel), 72);
    gtk_widget_set_halign(app->statusLabel, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->statusLabel, 10);
    gtk_widget_set_margin_top(app->statusLabel, 4);
    gtk_widget_set_margin_bottom(app->statusLabel, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->statusLabel), "muted-label");
    gtk_box_pack_end(GTK_BOX(rootVBox), app->statusLabel, FALSE, FALSE, 0);

    // Keyboard shortcuts on main window
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(+[](GtkWidget* win, GdkEventKey* ev, gpointer ud) -> gboolean {
        auto* a = static_cast<AppState*>(ud);
        GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(win));
        if (focus && (GTK_IS_ENTRY(focus) || GTK_IS_TEXT_VIEW(focus) || GTK_IS_SPIN_BUTTON(focus))) {
            return FALSE;
        }
        const bool ctrl = (ev->state & GDK_CONTROL_MASK) != 0;
        if (ctrl && (ev->keyval == GDK_KEY_c || ev->keyval == GDK_KEY_C)) {
            CopyActiveLcdAction(a, a->activeCalcIdx);
            return TRUE;
        }
        if (ctrl && (ev->keyval == GDK_KEY_s || ev->keyval == GDK_KEY_S)) {
            SaveRamSnapshotAction(a);
            return TRUE;
        }
        if (ctrl && (ev->keyval == GDK_KEY_z || ev->keyval == GDK_KEY_Z)) {
            RestoreRamSnapshotAction(a);
            return TRUE;
        }
        if (ctrl && (ev->keyval == GDK_KEY_e || ev->keyval == GDK_KEY_E)) {
            const auto& c = a->calcs[a->activeCalcIdx];
            a->exporterWindow.SyncFromHistory(a->keyHistory, a->activeCalcIdx, c.lcdRgba, c.lcdW, c.lcdH, c.indicators);
            a->exporterWindow.Show();
            return TRUE;
        }
        if (ev->keyval == GDK_KEY_F1) {
            a->smartHelpWindow.ShowWithQuery("", -1, -1);
            return TRUE;
        }
        int kid = MapGdkKeyToCalcKeyId(a->activeCalcIdx, ev->keyval);
        if (kid >= 0) {
            RecordAndExecuteKeyTap(a, a->activeCalcIdx, kid, false);
            return TRUE;
        }
        return FALSE;
    }), app);

    // Initialize Companion Windows
    app->pdfWindow.Initialize(GTK_WINDOW(app->window), app->paths, &app->catalog);
    app->smartHelpWindow.Initialize(
        GTK_WINDOW(app->window), app->paths, &app->catalog, RawSkinPtrs(app),
        [app](int calcIdx, const std::vector<int>& keys, const std::string& title) {
            ReplaySequenceOnEngine(app, calcIdx, keys, title);
        },
        [app](int calcIdx, const std::vector<int>& keys, const std::string& title, const std::string& notes) {
            app->exporterWindow.LoadCustomSequence(calcIdx, keys, title, notes);
            app->exporterWindow.Show();
        },
        [app](const std::string& pdfFile, int pageNo) {
            app->pdfWindow.OpenManual(pdfFile, pageNo);
        });

    app->popupWindow.Initialize(
        GTK_WINDOW(app->window),
        [app](int ci) {
            app->activeCalcIdx = ci;
            ApplyViewModeLayout(app);
        },
        [app](int ci) {
            CopyActiveLcdAction(app, ci);
        });

    app->exporterWindow.Initialize(
        GTK_WINDOW(app->window), app->paths, RawSkinPtrs(app),
        [app](const std::string& mdPath) {
            LaunchDocumentStudioProcess(app, mdPath);
        });

    ApplyViewModeLayout(app);
    return app->window;
}

CliOptions ParseCli(int argc, char** argv) {
    CliOptions o{};
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--compact") {
            o.hasViewOverride = true; o.initialView = ViewMode::Blanca; o.compactMode = true;
        } else if (a == "--compact-gray") {
            o.hasViewOverride = true; o.initialView = ViewMode::Gris; o.compactMode = true;
        } else if (a == "--compact-cg50") {
            o.hasViewOverride = true; o.initialView = ViewMode::Cg50; o.compactMode = true;
        } else if (a == "--compact-cw2") {
            o.hasViewOverride = true; o.initialView = ViewMode::Cw2; o.compactMode = true;
        } else if (a == "--popup") {
            o.openPopup = true;
        } else if (a == "--studio") {
            o.launchStudio = true;
        } else if (a == "--smoke-test") {
            o.smokeTest = true;
        } else if (a == "--help-search" && i + 1 < argc) {
            o.helpSearchQuery = argv[++i];
        } else if (a == "--screenshot-out" && i + 1 < argc) {
            o.screenshotOutPath = argv[++i];
        }
    }
    return o;
}

int RunSmokeTest(AppState* app, bool hasDisplay) {
    std::cout << "[SMOKE] DiscoverAssetPaths rootDir=" << app->paths.rootDir << "\n";
    std::string catDiag;
    bool catOk = app->catalog.Load(app->paths, &catDiag);
    std::cout << "[SMOKE] KnowledgeCatalog loaded=" << (catOk ? "true" : "false")
              << " (" << catDiag << ")\n";

    InitializeEnginesAndSkins(app);

    if (hasDisplay) {
        CasioThemeLinux::ApplyGtkThemeCss(app->settings.darkTheme);
        BuildMainUi(app);
        app->settings.viewMode = app->cli.hasViewOverride ? app->cli.initialView : ViewMode::BlancaGris;
        gtk_widget_show_all(app->window);
        ApplyViewModeLayout(app);
        while (gtk_events_pending()) gtk_main_iteration();
    }

    // Verify 2+3= on Blanca (0) & Gris (1)
    const int kSeq[4] = {41, 43, 42, 49}; // '2', '+', '3', '='
    for (int calcIdx : {0, 1}) {
        auto& c = app->calcs[calcIdx];
        if (!c.engine.IsReady()) {
            std::cerr << "[SMOKE] ERROR: Engine " << calcIdx << " not ready: "
                      << c.engine.StatusText() << "\n";
            if (app->window) gtk_widget_destroy(app->window);
            return 2;
        }
        for (int f = 0; f < 30; ++f) c.engine.StepMs(20);
        for (int kid : kSeq) {
            RecordAndExecuteKeyTap(app, calcIdx, kid, false);
        }
        for (int f = 0; f < 25; ++f) c.engine.StepMs(20);
        PullEngineLcd(app, calcIdx, true);
        if (c.lcdRgba.empty()) {
            std::cerr << "[SMOKE] ERROR: Empty LCD RGBA for calc " << calcIdx << "\n";
            if (app->window) gtk_widget_destroy(app->window);
            return 3;
        }
        std::cout << "[SMOKE] Calc " << calcIdx << " (" << c.lcdW << "x" << c.lcdH
                  << ") status=" << c.engine.StatusText() << " RGBA=" << c.lcdRgba.size() << "B\n";
    }

    auto eval = CasioLinux::EvaluateMathUtf8("2+3");
    std::cout << "[SMOKE] EvaluateMathUtf8(\"2+3\") ok=" << (eval.ok ? "true" : "false")
              << " formatted=" << eval.formatted << "\n";

    // Smoke-test PangoCairo 2D LaTeX measurement + rendering
    {
        cairo_surface_t* texSurf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 420, 120);
        cairo_t* cr = cairo_create(texSurf);
        auto ext = CasioLatexCairo::DrawLatexRichText(
            cr, "Resultado: $\\int_{0}^{3} x^{2}\\,dx = \\frac{27}{3} = 9$",
            10.0, 10.0, 400.0, 100.0, 16.0, CasioThemeLinux::TextPrimary(), false, false);
        cairo_destroy(cr);
        cairo_surface_destroy(texSurf);
        std::cout << "[SMOKE] LaTeX 2D extent=" << ext.width << "x" << ext.height << "\n";
    }

    // Smoke-test Poppler opening all PDF manuals from catalog/manuales/
    if (!app->catalog.OfficialManuals().empty() && !app->paths.manualsDir.empty()) {
        int pdfOkCount = 0;
        int pdfTotalPages = 0;
        for (const auto& m : app->catalog.OfficialManuals()) {
            std::filesystem::path pdfPath = std::filesystem::path(app->paths.manualsDir) / m.fileName;
            std::error_code ec;
            if (!std::filesystem::is_regular_file(pdfPath, ec)) continue;
            gchar* uri = g_filename_to_uri(pdfPath.string().c_str(), nullptr, nullptr);
            if (!uri) continue;
            GError* perr = nullptr;
            PopplerDocument* doc = poppler_document_new_from_file(uri, nullptr, &perr);
            g_free(uri);
            if (doc) {
                pdfOkCount++;
                pdfTotalPages += poppler_document_get_n_pages(doc);
                g_object_unref(doc);
            } else {
                std::cerr << "[SMOKE] WARNING: Poppler failed on " << m.fileName
                          << ": " << (perr ? perr->message : "unknown") << "\n";
                if (perr) g_error_free(perr);
            }
        }
        std::cout << "[SMOKE] Poppler manuals opened=" << pdfOkCount << "/"
                  << app->catalog.OfficialManuals().size()
                  << " total_pages=" << pdfTotalPages << "\n";
    }

    if (hasDisplay) {
        // Smoke-test Guide Exporter sheet + strip PNG + Markdown output
        const auto& c0 = app->calcs[0];
        app->exporterWindow.SyncFromHistory(app->keyHistory, 0, c0.lcdRgba, c0.lcdW, c0.lcdH, c0.indicators);
        std::string outSheet, outStrip, outMd;
        bool expOk = app->exporterWindow.ExportBundleToDirectory("/tmp/cu_smoke_export", &outSheet, &outStrip, &outMd);
        std::cout << "[SMOKE] Exporter bundle ok=" << (expOk ? "true" : "false")
                  << " md=" << outMd << "\n";

        while (gtk_events_pending()) gtk_main_iteration();
    }

    if (!app->cli.screenshotOutPath.empty()) {
        app->settings.viewMode = app->cli.hasViewOverride ? app->cli.initialView : ViewMode::BlancaGris;
        ApplyViewModeLayout(app);
        if (app->window) {
            gtk_container_check_resize(GTK_CONTAINER(app->window));
            while (gtk_events_pending()) gtk_main_iteration();
        }
        bool saved = CaptureStageScreenshotToPng(app, app->cli.screenshotOutPath);
        std::cout << "[SMOKE] Screenshot -> " << app->cli.screenshotOutPath
                  << " saved=" << (saved ? "true" : "false") << "\n";
        if (!saved) {
            if (app->window) gtk_widget_destroy(app->window);
            return 4;
        }
    }

    if (app->window) {
        gtk_widget_destroy(app->window);
        app->window = nullptr;
    }
    for (int i = 0; i < 4; ++i) app->calcs[i].engine.Shutdown();
    std::cout << "[SMOKE] PASSED\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    AppState app{};
    app.cli = ParseCli(argc, argv);
    app.paths = CasioLinux::DiscoverAssetPaths(argc > 0 ? argv[0] : nullptr);
    CasioThemeLinux::LoadUnifiedSettings(app.settings);
    if (app.cli.hasViewOverride) {
        app.settings.viewMode = app.cli.initialView;
        app.settings.compactMode = app.cli.compactMode;
    }

    if (app.cli.smokeTest) {
        bool hasDisplay = (gtk_init_check(&argc, &argv) != FALSE);
        return RunSmokeTest(&app, hasDisplay);
    }

    gtk_init(&argc, &argv);
    CasioThemeLinux::ApplyGtkThemeCss(app.settings.darkTheme);

    std::string catDiag;
    app.catalog.Load(app.paths, &catDiag);
    InitializeEnginesAndSkins(&app);
    BuildMainUi(&app);

    gtk_widget_show_all(app.window);
    ApplyViewModeLayout(&app);
    SetStatus(&app, "Listo · Catalogo: " + (catDiag.empty() ? "cargado" : catDiag));

    app.simTimerId = g_timeout_add(16, OnSimulationTimer, &app);

    if (app.cli.openPopup) {
        app.popupWindow.Show(app.activeCalcIdx);
        SyncPopupWindow(&app);
    }
    if (!app.cli.helpSearchQuery.empty()) {
        app.smartHelpWindow.ShowWithQuery(app.cli.helpSearchQuery, -1, -1);
    }
    if (app.cli.launchStudio) {
        LaunchDocumentStudioProcess(&app);
    }
    if (!app.cli.screenshotOutPath.empty()) {
        app.screenshotWaitTicks = 0;
        app.screenshotTimerId = g_timeout_add(80, +[](gpointer ud) -> gboolean {
            auto* a = static_cast<AppState*>(ud);
            if (!a->window) {
                a->screenshotTimerId = 0;
                return G_SOURCE_REMOVE;
            }
            GtkWidget* targetWin = a->window;
            if (a->cli.openPopup && a->popupWindow.Window() && gtk_widget_get_visible(a->popupWindow.Window())) {
                targetWin = a->popupWindow.Window();
            } else if (!a->cli.helpSearchQuery.empty() && a->smartHelpWindow.Window() &&
                       gtk_widget_get_visible(a->smartHelpWindow.Window())) {
                targetWin = a->smartHelpWindow.Window();
            }
            gtk_container_check_resize(GTK_CONTAINER(targetWin));
            while (gtk_events_pending()) gtk_main_iteration();
            a->screenshotWaitTicks++;
            const bool mapped = (gtk_widget_get_mapped(targetWin) != FALSE);
            const int targetW = gtk_widget_get_allocated_width(targetWin);
            const int stageW = (targetWin == a->window && a->stageArea)
                ? gtk_widget_get_allocated_width(a->stageArea) : targetW;
            if ((mapped && stageW >= 160 && a->screenshotWaitTicks >= 5) || a->screenshotWaitTicks >= 25) {
                gtk_widget_queue_draw(targetWin);
                gtk_container_check_resize(GTK_CONTAINER(targetWin));
                while (gtk_events_pending()) gtk_main_iteration();
                CaptureStageScreenshotToPng(a, a->cli.screenshotOutPath);
                a->screenshotTimerId = 0;
                gtk_main_quit();
                return G_SOURCE_REMOVE;
            }
            return G_SOURCE_CONTINUE;
        }, &app);
    }

    gtk_main();

    for (int i = 0; i < 4; ++i) app.calcs[i].engine.Shutdown();
    return 0;
}
