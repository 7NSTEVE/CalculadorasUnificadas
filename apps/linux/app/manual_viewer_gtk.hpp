#pragma once
// ============================================================================
// manual_viewer_gtk.hpp
// Embedded Poppler-glib 16-Manual PDF Reader + "Aprender CASIO" Smart Help UI
// ============================================================================

#include "knowledge_catalog_linux.hpp"
#include "latex_cairo.hpp"
#include "ui_theme_linux.hpp"

#include <gtk/gtk.h>
#include <poppler.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace CasioManualGtk {

class PdfManualWindow {
public:
    ~PdfManualWindow();

    void Initialize(GtkWindow* parent,
                    const CasioLinux::AssetPaths& paths,
                    const CasioKnowledgeLinux::KnowledgeCatalog* catalog);

    void OpenManual(const std::string& fileName, int pageNo = 1);
    void Show();
    void RefreshTheme();

private:
    GtkWindow* parent_ = nullptr;
    CasioLinux::AssetPaths paths_{};
    const CasioKnowledgeLinux::KnowledgeCatalog* catalog_ = nullptr;

    GtkWidget* window_ = nullptr;
    GtkWidget* manualCombo_ = nullptr;
    GtkWidget* pageSpin_ = nullptr;
    GtkWidget* pageCountLabel_ = nullptr;
    GtkWidget* statusLabel_ = nullptr;
    GtkWidget* drawArea_ = nullptr;
    GtkWidget* scrolled_ = nullptr;

    PopplerDocument* doc_ = nullptr;
    std::string currentFile_;
    int pageCount_ = 0;
    int currentPage_ = 1; // 1-based
    double zoom_ = 1.25;
    bool updatingUi_ = false;

    void LoadSelectedFile(const std::string& fileName, int pageNo);
    void SetPage(int page1Based);
    void UpdateDrawingAreaSize();

    static gboolean OnDraw(GtkWidget* widget, cairo_t* cr, gpointer userData);
    static void OnManualChanged(GtkComboBox* combo, gpointer userData);
    static void OnPageSpinChanged(GtkSpinButton* spin, gpointer userData);
    static void OnPrevClicked(GtkButton*, gpointer userData);
    static void OnNextClicked(GtkButton*, gpointer userData);
    static void OnFirstClicked(GtkButton*, gpointer userData);
    static void OnLastClicked(GtkButton*, gpointer userData);
    static void OnZoomChanged(GtkComboBoxText* combo, gpointer userData);
    static void OnOpenFolderClicked(GtkButton*, gpointer userData);
    static void OnOpenExternalClicked(GtkButton*, gpointer userData);
};

class SmartHelpWindow {
public:
    using TryStepsCb = std::function<void(int calcIdx, const std::vector<int>& keys, const std::string& title)>;
    using ExportGuideCb = std::function<void(int calcIdx, const std::vector<int>& keys,
                                             const std::string& title, const std::string& notes)>;
    using OpenPdfCb = std::function<void(const std::string& fileName, int pageNo)>;

    ~SmartHelpWindow();

    void Initialize(GtkWindow* parent,
                    const CasioLinux::AssetPaths& paths,
                    const CasioKnowledgeLinux::KnowledgeCatalog* catalog,
                    const std::array<cairo_surface_t*, 4>& skinSurfaces,
                    TryStepsCb onTrySteps,
                    ExportGuideCb onExportGuide,
                    OpenPdfCb onOpenPdf);

    void ShowWithQuery(const std::string& query = "", int modelFilter = -1, int catFilter = -1);
    void UpdateSkinSurfaces(const std::array<cairo_surface_t*, 4>& skinSurfaces);
    void RefreshTheme();
    GtkWidget* Window() const noexcept { return window_; }

private:
    GtkWindow* parent_ = nullptr;
    CasioLinux::AssetPaths paths_{};
    const CasioKnowledgeLinux::KnowledgeCatalog* catalog_ = nullptr;
    std::array<cairo_surface_t*, 4> skinSurfaces_{nullptr, nullptr, nullptr, nullptr};

    TryStepsCb onTrySteps_;
    ExportGuideCb onExportGuide_;
    OpenPdfCb onOpenPdf_;

    GtkWidget* window_ = nullptr;
    GtkWidget* searchEntry_ = nullptr;
    GtkWidget* modelCombo_ = nullptr;
    GtkWidget* countLabel_ = nullptr;
    GtkWidget* resultsBox_ = nullptr;
    std::array<GtkWidget*, 5> catButtons_{nullptr, nullptr, nullptr, nullptr, nullptr};

    int activeCategory_ = -1; // -1=All, 0=Basicas, 1=Alg/Calc, 2=Teclas/LCD, 3=Diag/Err, 4=Manuales(16)
    int activeModel_ = -1;
    bool updatingUi_ = false;
    std::vector<CasioKnowledgeLinux::SmartHelpItem> currentItems_;

    void RebuildResults();
    void UpdateCategoryButtons();
    void PopulateManualsView();
    void AppendHelpCard(const CasioKnowledgeLinux::SmartHelpItem& item);

    static void OnSearchChanged(GtkEditable*, gpointer userData);
    static void OnClearClicked(GtkButton*, gpointer userData);
    static void OnModelChanged(GtkComboBox* combo, gpointer userData);
    static void OnOpenFolderClicked(GtkButton*, gpointer userData);
};

} // namespace CasioManualGtk
