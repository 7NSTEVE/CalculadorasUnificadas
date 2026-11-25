#include "manual_viewer_gtk.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <sstream>

namespace CasioManualGtk {

namespace {

void OpenDirectoryExternally(GtkWindow* parent, const std::string& dirPath) {
    if (dirPath.empty()) return;
    GError* err = nullptr;
    gchar* uri = g_filename_to_uri(dirPath.c_str(), nullptr, &err);
    if (uri) {
        gtk_show_uri_on_window(parent, uri, GDK_CURRENT_TIME, nullptr);
        g_free(uri);
    }
    if (err) g_error_free(err);
}

void OpenFileExternally(GtkWindow* parent, const std::string& filePath) {
    if (filePath.empty()) return;
    GError* err = nullptr;
    gchar* uri = g_filename_to_uri(filePath.c_str(), nullptr, &err);
    if (uri) {
        gtk_show_uri_on_window(parent, uri, GDK_CURRENT_TIME, nullptr);
        g_free(uri);
    }
    if (err) g_error_free(err);
}

std::string ResolveManualFilePath(const CasioLinux::AssetPaths& paths, const std::string& fileName) {
    namespace fs = std::filesystem;
    if (fileName.empty()) return {};
    if (!paths.manualsDir.empty()) {
        fs::path p = fs::path(paths.manualsDir) / fileName;
        if (fs::is_regular_file(p)) return p.string();
    }
    if (!paths.rootDir.empty()) {
        fs::path p = fs::path(paths.rootDir) / "manuales" / fileName;
        if (fs::is_regular_file(p)) return p.string();
    }
    return {};
}

struct HelpCardRenderPayload {
    std::string latexFormula;
    std::string latexExample;
    std::vector<CasioThemeLinux::CairoSurfacePtr> keyChips;
};

struct HelpCardActionPayload {
    SmartHelpWindow::TryStepsCb onTry;
    SmartHelpWindow::ExportGuideCb onExport;
    SmartHelpWindow::OpenPdfCb onPdf;
    int calcIdx = 0;
    std::vector<int> demoSeq;
    std::string title;
    std::string notes;
    std::string pdfFile;
    int pdfPage = 1;
};

void DestroyRenderPayload(gpointer p) {
    delete static_cast<HelpCardRenderPayload*>(p);
}

void DestroyActionPayload(gpointer p) {
    delete static_cast<HelpCardActionPayload*>(p);
}

gboolean OnHelpCardCanvasDraw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* payload = static_cast<HelpCardRenderPayload*>(data);
    if (!payload) return FALSE;

    const int w = gtk_widget_get_allocated_width(widget);
    const int h = gtk_widget_get_allocated_height(widget);

    CasioThemeLinux::FillRoundedRect(cr, 0, 0, w, h, 8.0, CasioThemeLinux::Surface1());
    CasioThemeLinux::StrokeRoundedRect(cr, 0.5, 0.5, w - 1.0, h - 1.0, 8.0,
                                       CasioThemeLinux::BorderSubtle(), 1.0);

    double curY = 8.0;
    if (!payload->latexFormula.empty()) {
        auto ex = CasioLatexCairo::DrawLatexRichText(
            cr, payload->latexFormula, 12.0, curY, std::max(100.0, w - 24.0), 58.0,
            15.0, CasioThemeLinux::AccentText(), false, false);
        curY += std::max(22.0, ex.height + 6.0);
    }

    if (!payload->latexExample.empty()) {
        auto ex = CasioLatexCairo::DrawLatexRichText(
            cr, payload->latexExample, 12.0, curY, std::max(100.0, w - 24.0), 34.0,
            13.0, CasioThemeLinux::TextSecondary(), false, false);
        curY += std::max(18.0, ex.height + 6.0);
    }

    if (!payload->keyChips.empty()) {
        double x = 12.0;
        const double chipH = 34.0;
        for (size_t i = 0; i < payload->keyChips.size(); ++i) {
            cairo_surface_t* ks = payload->keyChips[i].get();
            if (!ks) continue;
            int sw = cairo_image_surface_get_width(ks);
            int sh = cairo_image_surface_get_height(ks);
            if (sw <= 0 || sh <= 0) continue;
            double drawH = std::min(chipH, h - curY - 6.0);
            if (drawH < 16.0) break;
            double scale = drawH / static_cast<double>(sh);
            double drawW = sw * scale;
            if (x + drawW > w - 12.0) break;

            if (i > 0) {
                CasioLatexCairo::DrawLatexRichText(
                    cr, "\xE2\x86\x92", x, curY + drawH * 0.25, 16.0, 20.0,
                    12.0, CasioThemeLinux::TextMuted(), true, false);
                x += 15.0;
            }
            cairo_save(cr);
            cairo_translate(cr, x, curY);
            cairo_scale(cr, scale, scale);
            cairo_set_source_surface(cr, ks, 0.0, 0.0);
            cairo_paint(cr);
            cairo_restore(cr);
            x += drawW + 4.0;
        }
    }
    return TRUE;
}

} // namespace

// ============================================================================
// PdfManualWindow (Poppler-glib 16-Manual Reader)
// ============================================================================

PdfManualWindow::~PdfManualWindow() {
    if (doc_) {
        g_object_unref(doc_);
        doc_ = nullptr;
    }
    if (window_) {
        gtk_widget_destroy(window_);
        window_ = nullptr;
    }
}

void PdfManualWindow::Initialize(GtkWindow* parent,
                                 const CasioLinux::AssetPaths& paths,
                                 const CasioKnowledgeLinux::KnowledgeCatalog* catalog) {
    parent_ = parent;
    paths_ = paths;
    catalog_ = catalog;

    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), "Visor de Manuales Oficiales CASIO (Poppler PDF)");
    gtk_window_set_default_size(GTK_WINDOW(window_), 980, 780);
    if (parent_) gtk_window_set_transient_for(GTK_WINDOW(window_), parent_);
    g_signal_connect(window_, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), nullptr);

    GtkWidget* rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), rootBox);

    GtkWidget* topBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(topBar), "studio-header");
    gtk_box_pack_start(GTK_BOX(rootBox), topBar, FALSE, FALSE, 0);

    manualCombo_ = gtk_combo_box_text_new();
    if (catalog_) {
        for (const auto& m : catalog_->OfficialManuals()) {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(manualCombo_), m.fileName.c_str(),
                                      m.shortLabel.c_str());
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(manualCombo_), 0);
    g_signal_connect(manualCombo_, "changed", G_CALLBACK(OnManualChanged), this);
    gtk_box_pack_start(GTK_BOX(topBar), manualCombo_, FALSE, FALSE, 0);

    GtkWidget* btnFirst = gtk_button_new_with_label("<<");
    GtkWidget* btnPrev  = gtk_button_new_with_label("< Ant");
    pageSpin_ = gtk_spin_button_new_with_range(1, 999, 1);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(pageSpin_), TRUE);
    pageCountLabel_ = gtk_label_new("/ 0");
    GtkWidget* btnNext  = gtk_button_new_with_label("Sig >");
    GtkWidget* btnLast  = gtk_button_new_with_label(">>");

    g_signal_connect(btnFirst, "clicked", G_CALLBACK(OnFirstClicked), this);
    g_signal_connect(btnPrev,  "clicked", G_CALLBACK(OnPrevClicked),  this);
    g_signal_connect(pageSpin_, "value-changed", G_CALLBACK(OnPageSpinChanged), this);
    g_signal_connect(btnNext,  "clicked", G_CALLBACK(OnNextClicked),  this);
    g_signal_connect(btnLast,  "clicked", G_CALLBACK(OnLastClicked),  this);

    gtk_box_pack_start(GTK_BOX(topBar), btnFirst, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topBar), btnPrev,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topBar), pageSpin_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topBar), pageCountLabel_, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(topBar), btnNext,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(topBar), btnLast,  FALSE, FALSE, 0);

    GtkWidget* zoomCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "0.75", "75%");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "1.00", "100%");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "1.25", "125%");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "1.50", "150%");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(zoomCombo), "2.00", "200%");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(zoomCombo), "1.25");
    g_signal_connect(zoomCombo, "changed", G_CALLBACK(OnZoomChanged), this);
    gtk_box_pack_start(GTK_BOX(topBar), zoomCombo, FALSE, FALSE, 4);

    GtkWidget* btnExt = gtk_button_new_with_label("Visor Externo");
    GtkWidget* btnDir = gtk_button_new_with_label("Carpeta PDF");
    g_signal_connect(btnExt, "clicked", G_CALLBACK(OnOpenExternalClicked), this);
    g_signal_connect(btnDir, "clicked", G_CALLBACK(OnOpenFolderClicked), this);
    gtk_box_pack_end(GTK_BOX(topBar), btnDir, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(topBar), btnExt, FALSE, FALSE, 0);

    statusLabel_ = gtk_label_new("Selecciona un manual CASIO oficial");
    gtk_widget_set_halign(statusLabel_, GTK_ALIGN_START);
    gtk_widget_set_margin_start(statusLabel_, 10);
    gtk_widget_set_margin_top(statusLabel_, 4);
    gtk_widget_set_margin_bottom(statusLabel_, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(statusLabel_), "muted-label");
    gtk_box_pack_start(GTK_BOX(rootBox), statusLabel_, FALSE, FALSE, 0);

    scrolled_ = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(rootBox), scrolled_, TRUE, TRUE, 0);

    drawArea_ = gtk_drawing_area_new();
    g_signal_connect(drawArea_, "draw", G_CALLBACK(OnDraw), this);
    gtk_container_add(GTK_CONTAINER(scrolled_), drawArea_);
}

void PdfManualWindow::Show() {
    if (!window_) return;
    gtk_widget_show_all(window_);
    gtk_window_present(GTK_WINDOW(window_));
    if (!doc_ && catalog_ && !catalog_->OfficialManuals().empty()) {
        LoadSelectedFile(catalog_->OfficialManuals().front().fileName, 1);
    }
}

void PdfManualWindow::RefreshTheme() {
    if (drawArea_) gtk_widget_queue_draw(drawArea_);
}

void PdfManualWindow::OpenManual(const std::string& fileName, int pageNo) {
    if (!window_) return;
    updatingUi_ = true;
    if (!fileName.empty()) {
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(manualCombo_), fileName.c_str());
    }
    updatingUi_ = false;
    LoadSelectedFile(fileName, pageNo);
    gtk_widget_show_all(window_);
    gtk_window_present(GTK_WINDOW(window_));
}

void PdfManualWindow::LoadSelectedFile(const std::string& fileName, int pageNo) {
    if (doc_) {
        g_object_unref(doc_);
        doc_ = nullptr;
    }
    currentFile_ = fileName;
    pageCount_ = 0;
    currentPage_ = 1;

    std::string fullPath = ResolveManualFilePath(paths_, fileName);
    if (fullPath.empty()) {
        gtk_label_set_text(GTK_LABEL(statusLabel_), ("Manual no encontrado en manuales/: " + fileName).c_str());
        gtk_label_set_text(GTK_LABEL(pageCountLabel_), "/ 0");
        gtk_widget_queue_draw(drawArea_);
        return;
    }

    GError* err = nullptr;
    gchar* uri = g_filename_to_uri(fullPath.c_str(), nullptr, &err);
    if (uri) {
        doc_ = poppler_document_new_from_file(uri, nullptr, &err);
        g_free(uri);
    }
    if (!doc_) {
        std::string msg = "No se pudo abrir PDF: " + fileName;
        if (err && err->message) msg += std::string(" (") + err->message + ")";
        if (err) g_error_free(err);
        gtk_label_set_text(GTK_LABEL(statusLabel_), msg.c_str());
        gtk_widget_queue_draw(drawArea_);
        return;
    }
    if (err) g_error_free(err);

    pageCount_ = poppler_document_get_n_pages(doc_);
    updatingUi_ = true;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(pageSpin_), 1, std::max(1, pageCount_));
    updatingUi_ = false;
    gtk_label_set_text(GTK_LABEL(pageCountLabel_), ("/ " + std::to_string(pageCount_)).c_str());
    gtk_label_set_text(GTK_LABEL(statusLabel_), (fileName + " — " + fullPath).c_str());

    SetPage(pageNo);
}

void PdfManualWindow::SetPage(int page1Based) {
    if (pageCount_ <= 0) return;
    currentPage_ = std::clamp(page1Based, 1, pageCount_);
    updatingUi_ = true;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(pageSpin_), currentPage_);
    updatingUi_ = false;
    UpdateDrawingAreaSize();
    gtk_widget_queue_draw(drawArea_);
}

void PdfManualWindow::UpdateDrawingAreaSize() {
    if (!doc_ || pageCount_ <= 0) {
        gtk_widget_set_size_request(drawArea_, 760, 900);
        return;
    }
    PopplerPage* page = poppler_document_get_page(doc_, currentPage_ - 1);
    if (!page) return;
    double pw = 595.0, ph = 842.0;
    poppler_page_get_size(page, &pw, &ph);
    g_object_unref(page);
    gtk_widget_set_size_request(drawArea_,
                                static_cast<int>(pw * zoom_) + 48,
                                static_cast<int>(ph * zoom_) + 48);
}

gboolean PdfManualWindow::OnDraw(GtkWidget* widget, cairo_t* cr, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    const int w = gtk_widget_get_allocated_width(widget);
    const int h = gtk_widget_get_allocated_height(widget);

    CasioThemeLinux::CairoSetSource(cr, CasioThemeLinux::StageBg());
    cairo_paint(cr);

    if (!self->doc_ || self->pageCount_ <= 0) {
        CasioLatexCairo::DrawLatexRichTextCentered(
            cr, "No se encontro el documento PDF seleccionado en manuales/",
            20.0, 20.0, std::max(100.0, w - 40.0), std::max(100.0, h - 40.0),
            16.0, CasioThemeLinux::TextSecondary(), true, false);
        return TRUE;
    }

    PopplerPage* page = poppler_document_get_page(self->doc_, self->currentPage_ - 1);
    if (!page) return TRUE;
    double pw = 595.0, ph = 842.0;
    poppler_page_get_size(page, &pw, &ph);

    const double renderW = pw * self->zoom_;
    const double renderH = ph * self->zoom_;
    const double px = std::max(24.0, 0.5 * (w - renderW));
    const double py = 20.0;

    CasioThemeLinux::DrawSoftShadow(cr, px, py, renderW, renderH, 4.0);
    cairo_save(cr);
    cairo_rectangle(cr, px, py, renderW, renderH);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_fill_preserve(cr);
    cairo_clip(cr);
    cairo_translate(cr, px, py);
    cairo_scale(cr, self->zoom_, self->zoom_);
    poppler_page_render(page, cr);
    cairo_restore(cr);

    CasioThemeLinux::StrokeRoundedRect(cr, px, py, renderW, renderH, 2.0,
                                       CasioThemeLinux::BorderStrong(), 1.0);
    g_object_unref(page);
    return TRUE;
}

void PdfManualWindow::OnManualChanged(GtkComboBox* combo, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    if (self->updatingUi_) return;
    const gchar* id = gtk_combo_box_get_active_id(combo);
    if (id && id[0]) self->LoadSelectedFile(id, 1);
}

void PdfManualWindow::OnPageSpinChanged(GtkSpinButton* spin, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    if (self->updatingUi_) return;
    self->SetPage(gtk_spin_button_get_value_as_int(spin));
}

void PdfManualWindow::OnPrevClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    self->SetPage(self->currentPage_ - 1);
}

void PdfManualWindow::OnNextClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    self->SetPage(self->currentPage_ + 1);
}

void PdfManualWindow::OnFirstClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    self->SetPage(1);
}

void PdfManualWindow::OnLastClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    self->SetPage(self->pageCount_);
}

void PdfManualWindow::OnZoomChanged(GtkComboBoxText* combo, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    const gchar* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
    if (!id) return;
    try { self->zoom_ = std::clamp(std::stod(id), 0.5, 3.0); } catch (...) {}
    self->UpdateDrawingAreaSize();
    gtk_widget_queue_draw(self->drawArea_);
}

void PdfManualWindow::OnOpenFolderClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    OpenDirectoryExternally(GTK_WINDOW(self->window_), self->paths_.manualsDir);
}

void PdfManualWindow::OnOpenExternalClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<PdfManualWindow*>(userData);
    std::string p = ResolveManualFilePath(self->paths_, self->currentFile_);
    if (!p.empty()) OpenFileExternally(GTK_WINDOW(self->window_), p);
}

// ============================================================================
// SmartHelpWindow ("Aprender CASIO — Ayuda Inteligente")
// ============================================================================

SmartHelpWindow::~SmartHelpWindow() {
    if (window_) {
        gtk_widget_destroy(window_);
        window_ = nullptr;
    }
}

void SmartHelpWindow::Initialize(GtkWindow* parent,
                                 const CasioLinux::AssetPaths& paths,
                                 const CasioKnowledgeLinux::KnowledgeCatalog* catalog,
                                 const std::array<cairo_surface_t*, 4>& skinSurfaces,
                                 TryStepsCb onTrySteps,
                                 ExportGuideCb onExportGuide,
                                 OpenPdfCb onOpenPdf) {
    parent_ = parent;
    paths_ = paths;
    catalog_ = catalog;
    skinSurfaces_ = skinSurfaces;
    onTrySteps_ = std::move(onTrySteps);
    onExportGuide_ = std::move(onExportGuide);
    onOpenPdf_ = std::move(onOpenPdf);

    window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_), "Aprender CASIO — Ayuda Inteligente, Teclas, LaTeX y 16 Manuales");
    gtk_window_set_default_size(GTK_WINDOW(window_), 960, 740);
    if (parent_) gtk_window_set_transient_for(GTK_WINDOW(window_), parent_);
    g_signal_connect(window_, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), nullptr);

    GtkWidget* rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), rootBox);

    // Search + Model bar
    GtkWidget* topBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(topBar), "studio-header");
    gtk_box_pack_start(GTK_BOX(rootBox), topBar, FALSE, FALSE, 0);

    searchEntry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(searchEntry_),
                                   "Buscar tecla, formula, error, procedimiento o tema (ej: integral, sin, matriz, Math ERROR)...");
    g_signal_connect(searchEntry_, "changed", G_CALLBACK(OnSearchChanged), this);
    gtk_box_pack_start(GTK_BOX(topBar), searchEntry_, TRUE, TRUE, 0);

    GtkWidget* btnClear = gtk_button_new_with_label("Limpiar");
    g_signal_connect(btnClear, "clicked", G_CALLBACK(OnClearClicked), this);
    gtk_box_pack_start(GTK_BOX(topBar), btnClear, FALSE, FALSE, 0);

    modelCombo_ = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "-1", "Todos los modelos");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "0", "fx-570LA X (Blanca)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "1", "fx-570VN PLUS (Gris)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "2", "fx-CG50 PRIZM");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(modelCombo_), "3", "fx-991CW (Negra)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(modelCombo_), 0);
    g_signal_connect(modelCombo_, "changed", G_CALLBACK(OnModelChanged), this);
    gtk_box_pack_start(GTK_BOX(topBar), modelCombo_, FALSE, FALSE, 0);

    GtkWidget* btnFolder = gtk_button_new_with_label("Carpeta PDF");
    g_signal_connect(btnFolder, "clicked", G_CALLBACK(OnOpenFolderClicked), this);
    gtk_box_pack_start(GTK_BOX(topBar), btnFolder, FALSE, FALSE, 0);

    // Category pill strip
    GtkWidget* catBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(catBar, 10);
    gtk_widget_set_margin_end(catBar, 10);
    gtk_widget_set_margin_top(catBar, 8);
    gtk_widget_set_margin_bottom(catBar, 4);
    gtk_box_pack_start(GTK_BOX(rootBox), catBar, FALSE, FALSE, 0);

    struct CatSpec { const char* label; int val; };
    const CatSpec kCats[5] = {
        {"Todo", -1},
        {"Guias y Algebra/Calculo", 1},
        {"Teclas e Indicadores", 2},
        {"Diagnostico y Errores", 3},
        {"Manuales Oficiales (16)", 4}
    };
    for (int i = 0; i < 5; ++i) {
        GtkWidget* b = gtk_button_new_with_label(kCats[i].label);
        catButtons_[i] = b;
        g_object_set_data(G_OBJECT(b), "cat-val", GINT_TO_POINTER(kCats[i].val));
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer ud) {
            auto* self = static_cast<SmartHelpWindow*>(ud);
            self->activeCategory_ = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "cat-val"));
            self->UpdateCategoryButtons();
            self->RebuildResults();
        }), this);
        gtk_box_pack_start(GTK_BOX(catBar), b, FALSE, FALSE, 0);
    }

    countLabel_ = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(countLabel_), "muted-label");
    gtk_box_pack_end(GTK_BOX(catBar), countLabel_, FALSE, FALSE, 4);

    // Scrollable results box
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(rootBox), scrolled, TRUE, TRUE, 0);

    resultsBox_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(resultsBox_, 12);
    gtk_widget_set_margin_end(resultsBox_, 12);
    gtk_widget_set_margin_top(resultsBox_, 6);
    gtk_widget_set_margin_bottom(resultsBox_, 12);
    gtk_container_add(GTK_CONTAINER(scrolled), resultsBox_);

    UpdateCategoryButtons();
}

void SmartHelpWindow::ShowWithQuery(const std::string& query, int modelFilter, int catFilter) {
    if (!window_) return;
    updatingUi_ = true;
    if (!query.empty()) gtk_entry_set_text(GTK_ENTRY(searchEntry_), query.c_str());
    activeModel_ = modelFilter;
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(modelCombo_), std::to_string(modelFilter).c_str());
    if (catFilter >= -1 && catFilter <= 4) activeCategory_ = catFilter;
    updatingUi_ = false;
    UpdateCategoryButtons();
    RebuildResults();
    gtk_widget_show_all(window_);
    gtk_window_present(GTK_WINDOW(window_));
}

void SmartHelpWindow::UpdateSkinSurfaces(const std::array<cairo_surface_t*, 4>& skinSurfaces) {
    skinSurfaces_ = skinSurfaces;
    if (window_ && gtk_widget_get_visible(window_)) RebuildResults();
}

void SmartHelpWindow::RefreshTheme() {
    if (window_ && gtk_widget_get_visible(window_)) RebuildResults();
}

void SmartHelpWindow::UpdateCategoryButtons() {
    for (int i = 0; i < 5; ++i) {
        if (!catButtons_[i]) continue;
        int val = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(catButtons_[i]), "cat-val"));
        GtkStyleContext* ctx = gtk_widget_get_style_context(catButtons_[i]);
        if (val == activeCategory_) gtk_style_context_add_class(ctx, "pill-active");
        else gtk_style_context_remove_class(ctx, "pill-active");
    }
}

void SmartHelpWindow::RebuildResults() {
    GList* children = gtk_container_get_children(GTK_CONTAINER(resultsBox_));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_container_remove(GTK_CONTAINER(resultsBox_), GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (!catalog_) return;
    const std::string query = gtk_entry_get_text(GTK_ENTRY(searchEntry_));

    if (activeCategory_ == 4 && query.empty()) {
        PopulateManualsView();
        gtk_widget_show_all(resultsBox_);
        return;
    }

    currentItems_ = catalog_->Search(query, activeCategory_, activeModel_, 45);
    gtk_label_set_text(GTK_LABEL(countLabel_),
                       (std::to_string(currentItems_.size()) + " resultados").c_str());

    if (currentItems_.empty()) {
        GtkWidget* emptyLbl = gtk_label_new("Sin coincidencias para esta busqueda. Prueba con otro termino o quita el filtro.");
        gtk_widget_set_margin_top(emptyLbl, 30);
        gtk_box_pack_start(GTK_BOX(resultsBox_), emptyLbl, FALSE, FALSE, 0);
    } else {
        for (const auto& item : currentItems_) {
            AppendHelpCard(item);
        }
    }
    gtk_widget_show_all(resultsBox_);
}

void SmartHelpWindow::PopulateManualsView() {
    const auto& manuals = catalog_->OfficialManuals();
    int shown = 0;
    const int filterBit = (activeModel_ >= 0) ? (1 << activeModel_) : 0;

    for (const auto& m : manuals) {
        if (filterBit != 0 && (m.calcMask & filterBit) == 0) continue;
        shown++;

        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_style_context_add_class(gtk_widget_get_style_context(card), "studio-card");
        gtk_box_pack_start(GTK_BOX(resultsBox_), card, FALSE, FALSE, 0);

        GtkWidget* infoCol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_box_pack_start(GTK_BOX(card), infoCol, TRUE, TRUE, 0);

        GtkWidget* titleLbl = gtk_label_new(m.shortLabel.c_str());
        PangoAttrList* attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(titleLbl), attrs);
        pango_attr_list_unref(attrs);
        gtk_widget_set_halign(titleLbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(infoCol), titleLbl, FALSE, FALSE, 0);

        GtkWidget* subLbl = gtk_label_new((m.fullTitle + " — " + m.modelCategory + " · " + m.manualKind).c_str());
        gtk_label_set_line_wrap(GTK_LABEL(subLbl), TRUE);
        gtk_widget_set_halign(subLbl, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(subLbl), "muted-label");
        gtk_box_pack_start(GTK_BOX(infoCol), subLbl, FALSE, FALSE, 0);

        std::string resolved = ResolveManualFilePath(paths_, m.fileName);
        GtkWidget* btnOpen = gtk_button_new_with_label(resolved.empty() ? "No disponible" : "Abrir Manual PDF");
        gtk_style_context_add_class(gtk_widget_get_style_context(btnOpen), "accent-btn");
        gtk_widget_set_sensitive(btnOpen, !resolved.empty());

        auto* act = new HelpCardActionPayload{};
        act->onPdf = onOpenPdf_;
        act->pdfFile = m.fileName;
        act->pdfPage = 1;
        g_object_set_data_full(G_OBJECT(btnOpen), "help-act", act, DestroyActionPayload);
        g_signal_connect(btnOpen, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
            auto* a = static_cast<HelpCardActionPayload*>(g_object_get_data(G_OBJECT(b), "help-act"));
            if (a && a->onPdf) a->onPdf(a->pdfFile, a->pdfPage);
        }), nullptr);
        gtk_box_pack_end(GTK_BOX(card), btnOpen, FALSE, FALSE, 0);
    }
    gtk_label_set_text(GTK_LABEL(countLabel_), (std::to_string(shown) + " manuales PDF oficiales").c_str());
}

void SmartHelpWindow::AppendHelpCard(const CasioKnowledgeLinux::SmartHelpItem& item) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "studio-card");
    gtk_box_pack_start(GTK_BOX(resultsBox_), card, FALSE, FALSE, 0);

    // Header row: badge + title
    GtkWidget* hdrRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(card), hdrRow, FALSE, FALSE, 0);

    GtkWidget* badgeLbl = gtk_label_new(item.badge.c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(badgeLbl), "studio-badge");
    gtk_box_pack_start(GTK_BOX(hdrRow), badgeLbl, FALSE, FALSE, 0);

    GtkWidget* titleLbl = gtk_label_new(item.title.c_str());
    PangoAttrList* attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(titleLbl), attrs);
    pango_attr_list_unref(attrs);
    gtk_label_set_line_wrap(GTK_LABEL(titleLbl), TRUE);
    gtk_widget_set_halign(titleLbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(hdrRow), titleLbl, TRUE, TRUE, 0);

    if (!item.summary.empty()) {
        GtkWidget* sumLbl = gtk_label_new(item.summary.c_str());
        gtk_label_set_line_wrap(GTK_LABEL(sumLbl), TRUE);
        gtk_widget_set_halign(sumLbl, GTK_ALIGN_START);
        gtk_label_set_xalign(GTK_LABEL(sumLbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(card), sumLbl, FALSE, FALSE, 0);
    }

    if (!item.stepsText.empty()) {
        GtkWidget* stLbl = gtk_label_new(("Secuencia: " + item.stepsText).c_str());
        gtk_label_set_line_wrap(GTK_LABEL(stLbl), TRUE);
        gtk_widget_set_halign(stLbl, GTK_ALIGN_START);
        gtk_label_set_xalign(GTK_LABEL(stLbl), 0.0f);
        gtk_style_context_add_class(gtk_widget_get_style_context(stLbl), "muted-label");
        gtk_box_pack_start(GTK_BOX(card), stLbl, FALSE, FALSE, 0);
    }

    // Native Cairo 2D LaTeX + authentic skin key chip preview canvas
    if (!item.latexFormula.empty() || !item.latexExample.empty() || !item.demoSeq.empty()) {
        auto* rp = new HelpCardRenderPayload{};
        rp->latexFormula = item.latexFormula;
        rp->latexExample = item.latexExample;

        if (!item.demoSeq.empty() && item.calcIdx >= 0 && item.calcIdx < 4 && skinSurfaces_[item.calcIdx]) {
            const auto& keyDefs = CasioLinux::GetKeysForModel(static_cast<CasioLinux::CalcModelId>(item.calcIdx));
            size_t count = std::min<size_t>(item.demoSeq.size(), 12);
            for (size_t i = 0; i < count; ++i) {
                int kid = item.demoSeq[i];
                for (const auto& kd : keyDefs) {
                    if (kd.id == kid) {
                        cairo_surface_t* chip = CasioThemeLinux::CropKeySurfaceFromSkin(
                            skinSurfaces_[item.calcIdx], item.calcIdx, kd);
                        if (chip) rp->keyChips.emplace_back(chip);
                        break;
                    }
                }
            }
        }

        int canvasH = 16;
        if (!rp->latexFormula.empty()) canvasH += 40;
        if (!rp->latexExample.empty()) canvasH += 26;
        if (!rp->keyChips.empty()) canvasH += 42;

        GtkWidget* canvas = gtk_drawing_area_new();
        gtk_widget_set_size_request(canvas, -1, canvasH);
        g_object_set_data_full(G_OBJECT(canvas), "render-payload", rp, DestroyRenderPayload);
        g_signal_connect(canvas, "draw", G_CALLBACK(OnHelpCardCanvasDraw), rp);
        gtk_box_pack_start(GTK_BOX(card), canvas, FALSE, FALSE, 2);
    }

    if (!item.tipOrWarning.empty()) {
        GtkWidget* tipLbl = gtk_label_new(("Nota: " + item.tipOrWarning).c_str());
        gtk_label_set_line_wrap(GTK_LABEL(tipLbl), TRUE);
        gtk_widget_set_halign(tipLbl, GTK_ALIGN_START);
        gtk_label_set_xalign(GTK_LABEL(tipLbl), 0.0f);
        gtk_style_context_add_class(gtk_widget_get_style_context(tipLbl), "muted-label");
        gtk_box_pack_start(GTK_BOX(card), tipLbl, FALSE, FALSE, 0);
    }

    // Action strip: [Probar Pasos] [Exportar Guia] [Ver PDF p.N]
    const bool hasSeq = !item.demoSeq.empty();
    const bool hasPdf = !item.pdfFile.empty() && !ResolveManualFilePath(paths_, item.pdfFile).empty();
    if (hasSeq || hasPdf) {
        GtkWidget* btnRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_pack_start(GTK_BOX(card), btnRow, FALSE, FALSE, 2);

        if (hasSeq) {
            GtkWidget* btnTry = gtk_button_new_with_label("Probar Pasos");
            gtk_style_context_add_class(gtk_widget_get_style_context(btnTry), "success-btn");
            auto* act = new HelpCardActionPayload{};
            act->onTry = onTrySteps_;
            act->calcIdx = item.calcIdx;
            act->demoSeq = item.demoSeq;
            act->title = item.title;
            g_object_set_data_full(G_OBJECT(btnTry), "help-act", act, DestroyActionPayload);
            g_signal_connect(btnTry, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                auto* a = static_cast<HelpCardActionPayload*>(g_object_get_data(G_OBJECT(b), "help-act"));
                if (a && a->onTry) a->onTry(a->calcIdx, a->demoSeq, a->title);
            }), nullptr);
            gtk_box_pack_start(GTK_BOX(btnRow), btnTry, FALSE, FALSE, 0);

            GtkWidget* btnExp = gtk_button_new_with_label("Exportar Guia");
            gtk_style_context_add_class(gtk_widget_get_style_context(btnExp), "accent-btn");
            auto* actE = new HelpCardActionPayload{};
            actE->onExport = onExportGuide_;
            actE->calcIdx = item.calcIdx;
            actE->demoSeq = item.demoSeq;
            actE->title = item.title;
            actE->notes = item.latexFormula.empty() ? item.summary : item.latexFormula;
            g_object_set_data_full(G_OBJECT(btnExp), "help-act", actE, DestroyActionPayload);
            g_signal_connect(btnExp, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                auto* a = static_cast<HelpCardActionPayload*>(g_object_get_data(G_OBJECT(b), "help-act"));
                if (a && a->onExport) a->onExport(a->calcIdx, a->demoSeq, a->title, a->notes);
            }), nullptr);
            gtk_box_pack_start(GTK_BOX(btnRow), btnExp, FALSE, FALSE, 0);
        }

        if (hasPdf) {
            std::string lbl = "Ver PDF (" + item.pdfFile + " p." + std::to_string(std::max(1, item.pdfPage)) + ")";
            GtkWidget* btnPdf = gtk_button_new_with_label(lbl.c_str());
            auto* actP = new HelpCardActionPayload{};
            actP->onPdf = onOpenPdf_;
            actP->pdfFile = item.pdfFile;
            actP->pdfPage = std::max(1, item.pdfPage);
            g_object_set_data_full(G_OBJECT(btnPdf), "help-act", actP, DestroyActionPayload);
            g_signal_connect(btnPdf, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                auto* a = static_cast<HelpCardActionPayload*>(g_object_get_data(G_OBJECT(b), "help-act"));
                if (a && a->onPdf) a->onPdf(a->pdfFile, a->pdfPage);
            }), nullptr);
            gtk_box_pack_start(GTK_BOX(btnRow), btnPdf, FALSE, FALSE, 0);
        }
    }
}

void SmartHelpWindow::OnSearchChanged(GtkEditable*, gpointer userData) {
    auto* self = static_cast<SmartHelpWindow*>(userData);
    if (self->updatingUi_) return;
    self->RebuildResults();
}

void SmartHelpWindow::OnClearClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<SmartHelpWindow*>(userData);
    gtk_entry_set_text(GTK_ENTRY(self->searchEntry_), "");
}

void SmartHelpWindow::OnModelChanged(GtkComboBox* combo, gpointer userData) {
    auto* self = static_cast<SmartHelpWindow*>(userData);
    if (self->updatingUi_) return;
    const gchar* id = gtk_combo_box_get_active_id(combo);
    if (id) {
        try { self->activeModel_ = std::stoi(id); } catch (...) { self->activeModel_ = -1; }
    }
    self->RebuildResults();
}

void SmartHelpWindow::OnOpenFolderClicked(GtkButton*, gpointer userData) {
    auto* self = static_cast<SmartHelpWindow*>(userData);
    OpenDirectoryExternally(GTK_WINDOW(self->window_), self->paths_.manualsDir);
}

} // namespace CasioManualGtk
