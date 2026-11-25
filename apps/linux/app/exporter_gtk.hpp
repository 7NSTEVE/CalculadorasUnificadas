#pragma once
// ============================================================================
// exporter_gtk.hpp
// Shared KeyLog Step Timeline, Clipboard Helpers, Synced Pop-up LCD Window,
// and Interactive Guide Exporter (PNG Sheet + Step Strip PNG + Markdown)
// ============================================================================

#include "knowledge_catalog_linux.hpp"
#include "latex_cairo.hpp"
#include "ui_theme_linux.hpp"

#include <gtk/gtk.h>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace CasioExporterGtk {

struct KeyLogStep {
    int stepNum = 0;
    int calcIdx = 0;
    int keyId = -1;
    std::string keyName;
    std::string friendlyLabel;
    std::string timestampStr;
    std::string note;
    std::vector<uint8_t> ramState;
    std::vector<uint8_t> lcdRgba;
    int lcdW = 0;
    int lcdH = 0;
    uint32_t indicators = 0;
};

struct ExporterStepItem {
    int keyId = -1;
    std::string keyName;
    bool included = true;
};

struct ExporterPresetState {
    int presetId = 0; // 0: Explicada, 1: Rapida, 2: Practica, 3: Presentacion
    int calcIdx = 0;
    int rangeStart = 1;
    int rangeEnd = 1;
    bool showStepNums = true;
    bool showFinalLcd = true;
    bool showLatex = true;
    bool showNotes = true;
    bool showSummary = true;
    bool lightSheet = true;
    bool hideAnswer = false;
    std::string title = "Guia paso a paso CASIO";
    std::string notes = "Secuencia capturada desde Calculadora Unificada.";
    std::string latexExpr;
    std::string linearExpr;
    std::vector<ExporterStepItem> steps;
    std::vector<uint8_t> finalLcdRgba;
    int finalLcdW = 0;
    int finalLcdH = 0;
    uint32_t finalIndicators = 0;
};

// Clipboard helpers
bool CopyCairoSurfaceToClipboard(cairo_surface_t* surf);
void CopyTextToClipboard(const std::string& text);
bool CopyCalculatorLcdToClipboard(int calcIdx,
                                  const std::vector<uint8_t>& lcdRgba, int lcdW, int lcdH,
                                  uint32_t indicators,
                                  const std::string& textSummary,
                                  int clipboardMode);

// Synced high-res Pop-up Display floating window (--popup)
class PopupDisplayWindow {
public:
    using SelectCalcCb = std::function<void(int calcIdx)>;
    using SnapshotCb   = std::function<void(int calcIdx)>;

    ~PopupDisplayWindow();

    void Initialize(GtkWindow* parent, SelectCalcCb onSelectCalc, SnapshotCb onCopyLcd);
    void Show(int activeCalcIdx);
    void Hide();
    bool IsVisible() const;
    GtkWidget* Window() const noexcept { return window_; }

    void UpdateFrame(int activeCalcIdx,
                     const std::vector<uint8_t>& rgba, int w, int h,
                     uint32_t indicators,
                     const std::string& cw2FallbackExpr,
                     const std::string& cw2FallbackRes);
    void RefreshTheme();

private:
    GtkWindow* parent_ = nullptr;
    SelectCalcCb onSelectCalc_;
    SnapshotCb onCopyLcd_;

    GtkWidget* window_ = nullptr;
    GtkWidget* drawArea_ = nullptr;
    std::array<GtkWidget*, 4> modelPills_{nullptr, nullptr, nullptr, nullptr};
    GtkWidget* gridBtn_ = nullptr;
    GtkWidget* topBtn_ = nullptr;

    int calcIdx_ = 0;
    int zoomFactor_ = 0; // 0=Auto, 1=x1, 2=x2, 3=x3
    bool showGrid_ = false;
    bool alwaysOnTop_ = true;

    std::vector<uint8_t> lcdRgba_;
    int lcdW_ = 0;
    int lcdH_ = 0;
    uint32_t indicators_ = 0;
    std::string cw2Expr_;
    std::string cw2Res_;

    void UpdatePillStyles();
    static gboolean OnDraw(GtkWidget* widget, cairo_t* cr, gpointer ud);
};

// Interactive Guide Exporter window
class SequenceExporterWindow {
public:
    using OpenStudioCb = std::function<void(const std::string& args)>;

    ~SequenceExporterWindow();

    void Initialize(GtkWindow* parent,
                    const CasioLinux::AssetPaths& paths,
                    const std::array<cairo_surface_t*, 4>& skinSurfaces,
                    OpenStudioCb onOpenStudio);

    void SyncFromHistory(const std::vector<KeyLogStep>& history,
                         int activeCalcIdx,
                         const std::vector<uint8_t>& activeLcdRgba, int activeW, int activeH,
                         uint32_t activeInd);

    void LoadCustomSequence(int calcIdx,
                            const std::vector<int>& keys,
                            const std::string& title,
                            const std::string& notes);

    void Show();
    void UpdateSkinSurfaces(const std::array<cairo_surface_t*, 4>& skinSurfaces);
    void RefreshTheme();

    // Renders clean 920px high-DPI sheet surface (caller owns returned surface)
    cairo_surface_t* RenderSheetSurface(int widthPx = 920) const;
    // Renders horizontal step strip surface of key chips only (caller owns)
    cairo_surface_t* RenderKeyStripSurface() const;

    bool ExportBundleToDirectory(const std::string& outDir,
                                 std::string* outSheetPng = nullptr,
                                 std::string* outStripPng = nullptr,
                                 std::string* outMarkdown = nullptr) const;

private:
    GtkWindow* parent_ = nullptr;
    CasioLinux::AssetPaths paths_{};
    std::array<cairo_surface_t*, 4> skinSurfaces_{nullptr, nullptr, nullptr, nullptr};
    OpenStudioCb onOpenStudio_;

    GtkWidget* window_ = nullptr;
    GtkWidget* titleEntry_ = nullptr;
    GtkWidget* notesEntry_ = nullptr;
    GtkWidget* latexStatusLbl_ = nullptr;
    GtkWidget* spinStart_ = nullptr;
    GtkWidget* spinEnd_ = nullptr;
    GtkWidget* modelCombo_ = nullptr;
    GtkWidget* statusLbl_ = nullptr;
    GtkWidget* previewArea_ = nullptr;
    GtkWidget* chkNums_ = nullptr;
    GtkWidget* chkLcd_ = nullptr;
    GtkWidget* chkLatex_ = nullptr;
    GtkWidget* chkNotes_ = nullptr;
    GtkWidget* chkSummary_ = nullptr;
    GtkWidget* chkLightSheet_ = nullptr;
    GtkWidget* chkHideAns_ = nullptr;
    std::array<GtkWidget*, 4> presetBtns_{nullptr, nullptr, nullptr, nullptr};

    ExporterPresetState state_{};
    std::vector<KeyLogStep> cachedHistory_;
    bool updatingUi_ = false;
    mutable std::vector<CasioLinux::SkinRect> previewKeyRects_;

    void ApplyPreset(int presetId);
    void RebuildFromRange();
    void RefreshPreview();
    void PullWidgetsToState();

    static gboolean OnPreviewDraw(GtkWidget* w, cairo_t* cr, gpointer ud);
    static gboolean OnPreviewClick(GtkWidget* w, GdkEventButton* ev, gpointer ud);
};

} // namespace CasioExporterGtk
