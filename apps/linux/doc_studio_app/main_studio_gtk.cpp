#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <pango/pangocairo.h>

#include "table_interop.hpp"
#include "app/studio_actions.hpp"
#include "app/studio_cli_dispatch.hpp"
#include "core/audio_timeline.hpp"
#include "core/camera.hpp"
#include "core/command_stack.hpp"
#include "core/cudoc_storage.hpp"
#include "core/doc_types.hpp"
#include "core/document_model.hpp"
#include "core/notebook_vault.hpp"
#include "core/utf_conv.hpp"
#include "pdf/interop_openxml.hpp"
#include "pdf/pdf_parser.hpp"
#include "pdf/pdf_signer.hpp"
#include "pdf/pdf_writer.hpp"
#include "pdf/winrt_adapters.hpp"
#include "render/d2d_renderer.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using namespace DocStudio;

constexpr int kThumbItemH = 112;
constexpr int kSidebarWidth = 228;
constexpr int kDefaultWinW = 1280;
constexpr int kDefaultWinH = 820;

enum class ToolKind {
    Hand = 0,
    Pen,
    Highlighter,
    Eraser,
    Lasso,
    Laser,
    ShapeRect,
    ShapeArrow,
    TextBox,
    Crop
};

struct StudioAppState;

struct UiWakeupBridge {
    std::mutex mtx;
    StudioAppState* app = nullptr;
    std::atomic<bool> queued{ false };
};

struct ColorSwatchSpec {
    const char* name;
    DocColor color;
};

constexpr ColorSwatchSpec kSwatches[5] = {
    { "Negro",    DocColor::RGBA(17, 24, 39, 255) },
    { "Azul",     DocColor::RGBA(26, 115, 232, 255) },
    { "Rojo",     DocColor::RGBA(220, 38, 38, 255) },
    { "Verde",    DocColor::RGBA(21, 128, 61, 255) },
    { "Amarillo", DocColor::RGBA(250, 204, 21, 255) }
};

struct StudioAppState {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* rootBox = nullptr;
    GtkWidget* ribbonBox = nullptr;
    GtkWidget* bodyPaned = nullptr;
    GtkWidget* sidebarBox = nullptr;
    GtkWidget* sidebarScroll = nullptr;
    GtkWidget* sidebarCanvas = nullptr;
    GtkWidget* searchResultsBox = nullptr;
    GtkWidget* searchResultsLabel = nullptr;
    GtkWidget* searchResultsList = nullptr;
    GtkWidget* viewport = nullptr;
    GtkWidget* statusLabel = nullptr;
    GtkWidget* statusRightLabel = nullptr;

    // Ribbon controls needing state sync
    GtkWidget* btnModePdf = nullptr;
    GtkWidget* btnModeCanvas = nullptr;
    GtkWidget* btnToolHand = nullptr;
    GtkWidget* btnToolPen = nullptr;
    GtkWidget* btnToolHl = nullptr;
    GtkWidget* btnToolEraser = nullptr;
    GtkWidget* btnToolLasso = nullptr;
    GtkWidget* btnToolLaser = nullptr;
    GtkWidget* btnToolRect = nullptr;
    GtkWidget* btnToolArrow = nullptr;
    GtkWidget* btnToolText = nullptr;
    GtkWidget* btnToolCrop = nullptr;
    GtkWidget* btnToggleSnap = nullptr;
    GtkWidget* swatchBtns[5]{};
    GtkWidget* swatchAreas[5]{};
    GtkWidget* paperCombo = nullptr;
    GtkWidget* pageSpin = nullptr;
    GtkWidget* pageTotalLabel = nullptr;
    GtkWidget* btnUndo = nullptr;
    GtkWidget* btnRedo = nullptr;
    GtkWidget* searchEntry = nullptr;

    bool syncingControls = false;
    bool needsInitialFit = true;
    bool viewportDrawnFlag = false;
    bool spacePanHeld = false;

    // Core document studio engines
    DocumentModel doc;
    CommandStack cmdStack;
    Camera2D camera;
    PageRenderCache pageCache{ 256ull * 1024ull * 1024ull };
    D2DRenderer renderer;
    std::uint64_t asyncGen = 1;
    std::shared_ptr<UiWakeupBridge> wakeupBridge = std::make_shared<UiWakeupBridge>();

    std::string currentDocPathUtf8;
    std::string activePdfSourcePathUtf8;
    std::string statusBannerUtf8 = "Document Studio listo";

    ToolKind activeTool = ToolKind::Pen;
    DocColor penColor = DocColor::RGBA(26, 115, 232, 255);
    double penWidth = 2.6;

    // Interactive gesture state
    bool panning = false;
    bool panMoved = false;
    double panStartX = 0.0;
    double panStartY = 0.0;
    double lastPanX = 0.0;
    double lastPanY = 0.0;

    bool erasing = false;
    bool eraseIsCanvas = false;
    std::size_t erasePageIdx = 0;

    bool drawing = false;
    bool draftIsCanvas = false;
    std::size_t draftPageIdx = 0;
    AnnotationObject draftObj{};
    DocPoint draftAnchorWorld{};
    bool hasDraftCrop = false;
    DocRect draftCropPageLocal{};

    std::uint64_t selectedObjectId = 0;
    bool selectedIsCanvas = false;
    std::size_t selectedPageIndex = 0;
    std::vector<std::uint64_t> lassoSelectedIds;

    // P1-P3 Ink Physics, Layers, Study & STEM interactive state
    bool snapShapes = true;
    double penStabilization = 0.65;
    bool presenterMode = false;
    bool replayActive = false;
    std::uint64_t replayCursorMs = 0;
    std::uint64_t replayMaxMs = 1;
    guint replayTimerId = 0;
    LaserPointerState laserPointer{};
    std::uint64_t strokeOriginEpochMs = 0;
    AudioTimelineSession audioSession;
    VaultFullTextIndex ftsIndex;
    StudyDeck activeDeck;
    std::size_t activeDeckCursor = 0;

    // Extended Goodnotes 6 / OneNote / Calcpad state
    bool minimapVisible = false;
    bool focusMode = false;
    bool hasPredictedTip = false;
    RulerState rulerState{};
    EraserFilterMode eraserFilterMode = EraserFilterMode::All;
    bool showcaseStemCli = false;

    std::vector<SearchMatch> searchMatches;
    std::size_t activeSearchIdx = 0;
    std::size_t activePageIndex = 0;

    // CLI options
    std::string cliOpenPathUtf8;
    std::string cliMode;
    std::string cliCalcCardSpec;
    std::string screenshotOutPathUtf8;
    int initialPage1Based = 1;
    bool smokeTestMode = false;
    bool headlessNoDisplay = false;
    bool teardownDone = false;

    std::function<void()> MakeWakeupCallback();
};

// ---------------- Forward Declarations ----------------
void InvalidateViews(StudioAppState* st);
void SyncToolbarControls(StudioAppState* st);
void UpdateWindowTitle(StudioAppState* st);
void UpdateStatusBar(StudioAppState* st);
void CenterCameraOnPage(StudioAppState* st, std::size_t pageIdx);
bool OpenDocumentFile(StudioAppState* st, const std::string& pathUtf8, std::string* outErr = nullptr);
void RebuildSearchResultsUi(StudioAppState* st);
void PerformLiveSearch(StudioAppState* st, const std::string& queryUtf8, bool jumpToMatch);
void StepSearchMatch(StudioAppState* st, int delta);

// ---------------- Small Utilities ----------------

std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        if (static_cast<unsigned char>(c) < 128) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return s;
}

std::string GetFileExtLowerUtf8(const std::string& pathUtf8) {
    try {
        return ToLowerAscii(std::filesystem::u8path(pathUtf8).extension().u8string());
    } catch (...) {
        const auto dot = pathUtf8.rfind('.');
        if (dot == std::string::npos) return {};
        return ToLowerAscii(pathUtf8.substr(dot));
    }
}

std::string EnsureExtension(const std::string& pathUtf8, const std::string& dotExt) {
    if (pathUtf8.empty() || dotExt.empty()) return pathUtf8;
    const std::string cur = GetFileExtLowerUtf8(pathUtf8);
    if (cur == ToLowerAscii(dotExt)) return pathUtf8;
    return pathUtf8 + dotExt;
}

bool ReadAllBytesUtf8(const std::string& pathUtf8, std::vector<std::uint8_t>& out, std::string* outErr = nullptr) {
    std::ifstream ifs(std::filesystem::u8path(pathUtf8), std::ios::binary | std::ios::ate);
    if (!ifs) {
        if (outErr) *outErr = "No se pudo abrir: " + pathUtf8;
        return false;
    }
    const auto sz = ifs.tellg();
    if (sz < 0 || sz > 256ll * 1024ll * 1024ll) {
        if (outErr) *outErr = "Archivo invalido o demasiado grande.";
        return false;
    }
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!out.empty() && !ifs.read(reinterpret_cast<char*>(out.data()), sz)) {
        if (outErr) *outErr = "Error leyendo archivo.";
        return false;
    }
    return true;
}

const char* ToolNameEs(ToolKind t) noexcept {
    switch (t) {
    case ToolKind::Hand:        return "Mano";
    case ToolKind::Pen:         return "Pluma";
    case ToolKind::Highlighter: return "Resaltador";
    case ToolKind::Eraser:      return "Borrador";
    case ToolKind::Lasso:       return "Lazo";
    case ToolKind::Laser:       return "Puntero Láser";
    case ToolKind::ShapeRect:   return "Rectángulo";
    case ToolKind::ShapeArrow:  return "Flecha";
    case ToolKind::TextBox:     return "Texto";
    case ToolKind::Crop:        return "Recorte";
    default:                    return "Pluma";
    }
}

ObjectTarget CurrentTarget(const StudioAppState* st) noexcept {
    if (!st) return { true, 0 };
    const bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
    const std::size_t pg = st->doc.Pages().empty() ? 0 : std::min(st->activePageIndex, st->doc.Pages().size() - 1);
    return { isCanvas, isCanvas ? 0u : pg };
}

void NextCardPlacement(const StudioAppState* st, double& outX, double& outY) noexcept {
    const ObjectTarget t = CurrentTarget(st);
    const std::size_t count = t.isCanvas
        ? st->doc.CanvasObjects().size()
        : (t.pageIndex < st->doc.Pages().size() ? st->doc.Pages()[t.pageIndex].objects.size() : 0);
    const std::size_t col = count / 4;
    const std::size_t row = count % 4;
    outX = 80.0 + static_cast<double>(col) * 390.0;
    outY = 80.0 + static_cast<double>(row) * 145.0;
}

std::uint64_t CurrentStrokeRelMs(StudioAppState* st) noexcept {
    const auto nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    if (st->strokeOriginEpochMs == 0) st->strokeOriginEpochMs = nowMs;
    return (nowMs >= st->strokeOriginEpochMs) ? (nowMs - st->strokeOriginEpochMs) : 0ull;
}

void PopulateViewportOverlay(const StudioAppState* st, ViewportOverlayState& ov) {
    ov.draftObject = st->drawing ? &st->draftObj : nullptr;
    ov.draftPageIndex = st->draftPageIdx;
    ov.selectedObjectId = st->selectedObjectId;
    ov.lassoSelectedIds = st->lassoSelectedIds;
    ov.hasCropPreview = st->hasDraftCrop;
    ov.cropPageIndex = st->draftPageIdx;
    ov.cropPreviewPageLocal = st->draftCropPageLocal;
    ov.searchMatches = st->searchMatches.empty() ? nullptr : &st->searchMatches;
    ov.activeSearchMatchIndex = st->activeSearchIdx;
    ov.replayActive = st->replayActive;
    ov.replayCursorMs = st->replayCursorMs;
    ov.laserPointer = st->laserPointer;
    ov.presenterMode = st->presenterMode;
    ov.minimapVisible = st->minimapVisible;
    ov.rulerState = st->rulerState;
    ov.focusMode = st->focusMode;
}

void ApplyCliCalcCardSpec(StudioAppState* st) {
    if (!st || st->cliCalcCardSpec.empty()) return;
    CalcBridge::CalcStepRecord rec{};
    const auto b1 = st->cliCalcCardSpec.find('|');
    if (b1 == std::string::npos) {
        rec.modelIdUtf8 = "fx-991CW";
        rec.expressionUtf8 = st->cliCalcCardSpec;
    } else {
        rec.modelIdUtf8 = b1 > 0 ? st->cliCalcCardSpec.substr(0, b1) : "fx-991CW";
        const auto b2 = st->cliCalcCardSpec.find('|', b1 + 1);
        if (b2 == std::string::npos) {
            rec.keySeqUtf8 = st->cliCalcCardSpec.substr(b1 + 1);
            rec.expressionUtf8 = rec.keySeqUtf8;
        } else {
            rec.keySeqUtf8 = st->cliCalcCardSpec.substr(b1 + 1, b2 - (b1 + 1));
            rec.expressionUtf8 = st->cliCalcCardSpec.substr(b2 + 1);
        }
    }
    double x = 80.0, y = 80.0;
    NextCardPlacement(st, x, y);
    const ObjectTarget tgt = CurrentTarget(st);
    const std::uint64_t newObjId = AppActions::AddCalcCardAction(st->doc, st->cmdStack, tgt, x, y, rec, st->statusBannerUtf8);
    if (newObjId != 0) {
        st->selectedObjectId = newObjId;
        st->selectedIsCanvas = tgt.isCanvas;
        st->selectedPageIndex = tgt.pageIndex;
    }
}

bool ColorsEqualRgb(DocColor a, DocColor b) noexcept {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

void ShowMessageDialog(GtkWindow* parent, GtkMessageType type, const char* title, const std::string& message) {
    GtkWidget* dlg = gtk_message_dialog_new(
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        type,
        GTK_BUTTONS_OK,
        "%s",
        message.c_str());
    if (title) gtk_window_set_title(GTK_WINDOW(dlg), title);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

std::string RunFileOpenDialog(
    GtkWindow* parent,
    const char* title,
    const std::vector<std::pair<std::string, std::vector<std::string>>>& filters,
    std::vector<std::string>* outMulti = nullptr)
{
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        title,
        parent,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Abrir", GTK_RESPONSE_ACCEPT,
        nullptr);
    if (outMulti) {
        gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dlg), TRUE);
    }
    for (const auto& f : filters) {
        GtkFileFilter* ff = gtk_file_filter_new();
        gtk_file_filter_set_name(ff, f.first.c_str());
        for (const auto& pat : f.second) {
            gtk_file_filter_add_pattern(ff, pat.c_str());
        }
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), ff);
    }

    std::string firstPath;
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        if (outMulti) {
            outMulti->clear();
            GSList* list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dlg));
            for (GSList* it = list; it != nullptr; it = it->next) {
                char* fn = static_cast<char*>(it->data);
                if (fn) {
                    if (firstPath.empty()) firstPath = fn;
                    outMulti->emplace_back(fn);
                    g_free(fn);
                }
            }
            g_slist_free(list);
        } else {
            char* fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
            if (fn) {
                firstPath = fn;
                g_free(fn);
            }
        }
    }
    gtk_widget_destroy(dlg);
    return firstPath;
}

std::string RunFileSaveDialog(
    GtkWindow* parent,
    const char* title,
    const char* defaultName,
    const std::string& filterLabel,
    const std::string& pattern,
    const std::string& dotExt)
{
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
        title,
        parent,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Guardar", GTK_RESPONSE_ACCEPT,
        nullptr);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    if (defaultName) {
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), defaultName);
    }
    if (!pattern.empty()) {
        GtkFileFilter* ff = gtk_file_filter_new();
        gtk_file_filter_set_name(ff, filterLabel.c_str());
        gtk_file_filter_add_pattern(ff, pattern.c_str());
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), ff);
    }

    std::string outPath;
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char* fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (fn) {
            outPath = EnsureExtension(fn, dotExt);
            g_free(fn);
        }
    }
    gtk_widget_destroy(dlg);
    return outPath;
}

bool ShowSingleInputPrompt(
    GtkWindow* parent,
    const char* title,
    const char* labelText,
    std::string& inOutValue,
    bool multiline = false,
    double* inOutFontSize = nullptr)
{
    GtkWidget* dlg = gtk_dialog_new_with_buttons(
        title,
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Aceptar", GTK_RESPONSE_OK,
        nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dlg), multiline ? 440 : 400, multiline ? 220 : 120);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    GtkWidget* lbl = gtk_label_new(labelText);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    GtkWidget* entry = nullptr;
    GtkWidget* textView = nullptr;
    GtkTextBuffer* buffer = nullptr;

    if (multiline) {
        GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(sw, 400, 110);
        textView = gtk_text_view_new();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textView), GTK_WRAP_WORD_CHAR);
        buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView));
        if (!inOutValue.empty()) {
            gtk_text_buffer_set_text(buffer, inOutValue.c_str(), -1);
        }
        gtk_container_add(GTK_CONTAINER(sw), textView);
        gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
    } else {
        entry = gtk_entry_new();
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
        gtk_entry_set_text(GTK_ENTRY(entry), inOutValue.c_str());
        gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
    }

    GtkWidget* fontSpin = nullptr;
    if (inOutFontSize) {
        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget* flbl = gtk_label_new("Tamaño de fuente (pt):");
        fontSpin = gtk_spin_button_new_with_range(8.0, 72.0, 1.0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(fontSpin), *inOutFontSize);
        gtk_box_pack_start(GTK_BOX(hbox), flbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), fontSpin, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    }

    gtk_widget_show_all(dlg);
    if (textView) gtk_widget_grab_focus(textView);
    else if (entry) gtk_widget_grab_focus(entry);

    const gint resp = gtk_dialog_run(GTK_DIALOG(dlg));
    bool ok = (resp == GTK_RESPONSE_OK);
    if (ok) {
        if (multiline && buffer) {
            GtkTextIter start, end;
            gtk_text_buffer_get_bounds(buffer, &start, &end);
            char* txt = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
            inOutValue = txt ? txt : "";
            g_free(txt);
        } else if (entry) {
            inOutValue = gtk_entry_get_text(GTK_ENTRY(entry));
        }
        if (inOutFontSize && fontSpin) {
            *inOutFontSize = gtk_spin_button_get_value(GTK_SPIN_BUTTON(fontSpin));
        }
    }
    gtk_widget_destroy(dlg);
    return ok;
}

bool ShowTwoFieldPrompt(
    GtkWindow* parent,
    const char* title,
    const char* label1,
    std::string& val1,
    const char* label2,
    std::string& val2)
{
    GtkWidget* dlg = gtk_dialog_new_with_buttons(
        title,
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Aceptar", GTK_RESPONSE_OK,
        nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 420, 180);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    GtkWidget* l1 = gtk_label_new(label1);
    gtk_widget_set_halign(l1, GTK_ALIGN_START);
    GtkWidget* e1 = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(e1), val1.c_str());

    GtkWidget* l2 = gtk_label_new(label2);
    gtk_widget_set_halign(l2, GTK_ALIGN_START);
    GtkWidget* e2 = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(e2), TRUE);
    gtk_entry_set_text(GTK_ENTRY(e2), val2.c_str());

    gtk_box_pack_start(GTK_BOX(vbox), l1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), e1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), l2, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), e2, FALSE, FALSE, 0);

    gtk_widget_show_all(dlg);
    const bool ok = (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK);
    if (ok) {
        val1 = gtk_entry_get_text(GTK_ENTRY(e1));
        val2 = gtk_entry_get_text(GTK_ENTRY(e2));
    }
    gtk_widget_destroy(dlg);
    return ok;
}

void ShowScrollableTextDialog(GtkWindow* parent, const char* title, const std::string& header, const std::string& bodyText) {
    GtkWidget* dlg = gtk_dialog_new_with_buttons(
        title,
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cerrar", GTK_RESPONSE_CLOSE,
        nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 540, 380);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    if (!header.empty()) {
        GtkWidget* hLbl = gtk_label_new(header.c_str());
        gtk_widget_set_halign(hLbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(vbox), hLbl, FALSE, FALSE, 0);
    }

    GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget* tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
    GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(buf, bodyText.c_str(), -1);
    gtk_container_add(GTK_CONTAINER(sw), tv);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

    GtkWidget* copyBtn = gtk_button_new_with_label("Copiar al portapapeles");
    const auto onCopy = +[](GtkTextBuffer* b) {
        GtkTextIter s;
        GtkTextIter e;
        gtk_text_buffer_get_bounds(b, &s, &e);
        char* txt = gtk_text_buffer_get_text(b, &s, &e, FALSE);
        if (txt) {
            GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            gtk_clipboard_set_text(cb, txt, -1);
            g_free(txt);
        }
    };
    g_signal_connect_swapped(copyBtn, "clicked", G_CALLBACK(onCopy), buf);
    gtk_widget_set_halign(copyBtn, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(vbox), copyBtn, FALSE, FALSE, 0);

    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

// ---------------- Wakeup Bridge ----------------

std::function<void()> StudioAppState::MakeWakeupCallback() {
    auto bridge = wakeupBridge;
    return [bridge]() {
        if (bridge->queued.exchange(true)) return;
        auto* heapSp = new std::shared_ptr<UiWakeupBridge>(bridge);
        g_idle_add(+[](gpointer data) -> gboolean {
            auto* sp = static_cast<std::shared_ptr<UiWakeupBridge>*>(data);
            std::shared_ptr<UiWakeupBridge> b = *sp;
            delete sp;

            std::lock_guard<std::mutex> lk(b->mtx);
            b->queued.store(false);
            if (b->app != nullptr && !b->app->teardownDone) {
                InvalidateViews(b->app);
            }
            return G_SOURCE_REMOVE;
        }, heapSp);
    };
}

// ---------------- Hit-testing for Selection & Eraser ----------------

double DistPointToSegment(double px, double py, double x1, double y1, double x2, double y2) noexcept {
    const double dx = x2 - x1;
    const double dy = y2 - y1;
    const double len2 = dx * dx + dy * dy;
    if (len2 <= 1e-9) return std::hypot(px - x1, py - y1);
    const double t = std::clamp(((px - x1) * dx + (py - y1) * dy) / len2, 0.0, 1.0);
    return std::hypot(px - (x1 + t * dx), py - (y1 + t * dy));
}

bool PointHitsAnnotation(const AnnotationObject& obj, double lx, double ly, double tolWorld) {
    if (!obj.bounds.Contains(lx, ly, tolWorld)) return false;
    if (obj.kind == ObjectKind::InkStroke && !obj.inkPoints.empty()) {
        const double hitR = tolWorld + std::max(1.5, obj.strokeWidth * 0.65);
        if (obj.inkPoints.size() == 1) {
            return std::hypot(obj.inkPoints[0].x - lx, obj.inkPoints[0].y - ly) <= hitR;
        }
        for (std::size_t i = 1; i < obj.inkPoints.size(); ++i) {
            const auto& a = obj.inkPoints[i - 1];
            const auto& b = obj.inkPoints[i];
            if (DistPointToSegment(lx, ly, a.x, a.y, b.x, b.y) <= hitR) return true;
        }
        return false;
    }
    if (obj.kind == ObjectKind::Shape && (obj.shapeKind == ShapeKind::Line || obj.shapeKind == ShapeKind::Arrow)) {
        return DistPointToSegment(lx, ly, obj.shapeStart.x, obj.shapeStart.y, obj.shapeEnd.x, obj.shapeEnd.y)
               <= (tolWorld + std::max(2.0, obj.strokeWidth));
    }
    return true;
}

std::vector<std::uint64_t> CollectHits(
    StudioAppState* st,
    bool isCanvas,
    std::size_t pageIdx,
    double lx,
    double ly,
    double tol)
{
    std::vector<std::uint64_t> hits;
    if (isCanvas) {
        const auto candidates = st->doc.QueryCanvasRect({ lx - tol, ly - tol, tol * 2.0, tol * 2.0 });
        std::vector<const AnnotationObject*> matched;
        for (std::uint64_t id : candidates) {
            if (const auto* o = st->doc.FindCanvasObject(id)) {
                if (PointHitsAnnotation(*o, lx, ly, tol)) matched.push_back(o);
            }
        }
        std::sort(matched.begin(), matched.end(), [](const AnnotationObject* a, const AnnotationObject* b) {
            return a->zOrder < b->zOrder;
        });
        for (const auto* o : matched) hits.push_back(o->id);
    } else if (pageIdx < st->doc.Pages().size()) {
        for (const auto& o : st->doc.Pages()[pageIdx].objects) {
            if (PointHitsAnnotation(o, lx, ly, tol)) hits.push_back(o.id);
        }
    }
    return hits;
}

// ---------------- Camera, Layout, Sync & Status ----------------

int GetEffectiveViewportWidth(const StudioAppState* st) {
    if (st && st->viewport) {
        const int aw = gtk_widget_get_allocated_width(st->viewport);
        if (aw >= 120) return aw;
    }
    return (st && st->doc.Mode() == DocumentMode::PaginatedPdf)
        ? (kDefaultWinW - kSidebarWidth)
        : kDefaultWinW;
}

int GetEffectiveViewportHeight(const StudioAppState* st) {
    if (st && st->viewport) {
        const int ah = gtk_widget_get_allocated_height(st->viewport);
        if (ah >= 100) return ah;
    }
    return kDefaultWinH - 110;
}

void RefreshActivePageFromCamera(StudioAppState* st) {
    if (!st || st->doc.Mode() != DocumentMode::PaginatedPdf || st->doc.Pages().empty()) return;
    const double vh = static_cast<double>(GetEffectiveViewportHeight(st));
    const double midY = st->camera.ScreenToWorld(0.0, vh * 0.4).y;
    std::size_t f = 0, l = 0;
    if (st->doc.Layout().ComputeVisibleRange(midY, midY + 10.0, f, l, 0)) {
        if (f != st->activePageIndex) {
            st->activePageIndex = f;
            SyncToolbarControls(st);
        }
    }
}

void CenterCameraOnPage(StudioAppState* st, std::size_t pageIdx) {
    if (!st || st->doc.Pages().empty()) return;
    const std::size_t idx = std::min(pageIdx, st->doc.Pages().size() - 1);
    const auto* pl = st->doc.Layout().PlacementAt(idx);
    if (!pl) return;
    const double vw = std::max(240.0, static_cast<double>(GetEffectiveViewportWidth(st)));
    st->camera.zoom = std::clamp((vw - 64.0) / std::max(120.0, pl->widthWorld), 0.25, 2.2);
    st->camera.panX = -vw / (2.0 * st->camera.zoom);
    st->camera.panY = pl->topWorld - 16.0;
    st->activePageIndex = idx;
}

void SyncSidebarSize(StudioAppState* st) {
    if (!st) return;
    if (st->sidebarBox) {
        if (st->doc.Mode() == DocumentMode::PaginatedPdf && !st->focusMode) {
            gtk_widget_show(st->sidebarBox);
        } else {
            gtk_widget_hide(st->sidebarBox);
        }
    }
    if (st->sidebarCanvas) {
        const int totalH = std::max(120, static_cast<int>(st->doc.Pages().size()) * kThumbItemH);
        gtk_widget_set_size_request(st->sidebarCanvas, kSidebarWidth - 20, totalH);
    }
}

void UpdateWindowTitle(StudioAppState* st) {
    if (!st || !st->window) return;
    std::string name;
    if (!st->currentDocPathUtf8.empty()) {
        name = std::filesystem::u8path(st->currentDocPathUtf8).filename().u8string();
    } else if (!st->activePdfSourcePathUtf8.empty()) {
        name = std::filesystem::u8path(st->activePdfSourcePathUtf8).filename().u8string();
    } else {
        name = st->doc.Title().empty() ? "Calculadora Unificada" : st->doc.Title();
    }
    const std::string dirtyMark = st->doc.IsDirty() ? "*" : "";
    const std::string title = "Document Studio \xE2\x80\x94 " + name + dirtyMark;
    gtk_window_set_title(GTK_WINDOW(st->window), title.c_str());
}

void UpdateStatusBar(StudioAppState* st) {
    if (!st) return;
    const double usedMiB = static_cast<double>(st->pageCache.BytesUsed()) / (1024.0 * 1024.0);
    const double maxMiB = static_cast<double>(st->pageCache.MaxBytes()) / (1024.0 * 1024.0);
    const int zoomPct = static_cast<int>(std::lround(st->camera.zoom * 100.0));
    const std::size_t pgCount = st->doc.Pages().size();
    const std::size_t pgDisplay = pgCount > 0 ? (std::min(st->activePageIndex, pgCount - 1) + 1) : 0;

    std::ostringstream left;
    left << (st->doc.Mode() == DocumentMode::InfiniteCanvas ? "Lienzo Infinito" : "PDF Paginado")
         << "  |  P\xC3\xA1g. " << pgDisplay << "/" << pgCount
         << "  |  Zoom: " << zoomPct << "%"
         << "  |  Herramienta: " << ToolNameEs(st->activeTool);
    if (st->doc.Mode() == DocumentMode::InfiniteCanvas) {
        left << "  |  Quadtree: " << st->doc.CanvasQuadtreeSize() << " objs";
    }
    left << "  |  " << st->statusBannerUtf8;

    std::ostringstream right;
    right << "Cach\xC3\xA9: " << static_cast<int>(std::lround(usedMiB)) << "/"
          << static_cast<int>(std::lround(maxMiB)) << " MiB";

    if (st->statusLabel) gtk_label_set_text(GTK_LABEL(st->statusLabel), left.str().c_str());
    if (st->statusRightLabel) gtk_label_set_text(GTK_LABEL(st->statusRightLabel), right.str().c_str());
}

void SyncToolbarControls(StudioAppState* st) {
    if (!st) return;
    st->syncingControls = true;

    const bool isPdf = (st->doc.Mode() == DocumentMode::PaginatedPdf);
    if (st->btnModePdf) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(st->btnModePdf), isPdf);
    if (st->btnModeCanvas) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(st->btnModeCanvas), !isPdf);

    auto setToolBtn = [](GtkWidget* w, bool active) {
        if (w) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), active);
    };
    setToolBtn(st->btnToolHand,   st->activeTool == ToolKind::Hand);
    setToolBtn(st->btnToolPen,    st->activeTool == ToolKind::Pen);
    setToolBtn(st->btnToolHl,     st->activeTool == ToolKind::Highlighter);
    setToolBtn(st->btnToolEraser, st->activeTool == ToolKind::Eraser);
    setToolBtn(st->btnToolLasso,  st->activeTool == ToolKind::Lasso);
    setToolBtn(st->btnToolLaser,  st->activeTool == ToolKind::Laser);
    setToolBtn(st->btnToolRect,   st->activeTool == ToolKind::ShapeRect);
    setToolBtn(st->btnToolArrow,  st->activeTool == ToolKind::ShapeArrow);
    setToolBtn(st->btnToolText,   st->activeTool == ToolKind::TextBox);
    setToolBtn(st->btnToolCrop,   st->activeTool == ToolKind::Crop);
    setToolBtn(st->btnToggleSnap, st->snapShapes);

    for (int i = 0; i < 5; ++i) {
        if (st->swatchAreas[i]) gtk_widget_queue_draw(st->swatchAreas[i]);
    }

    if (st->paperCombo) {
        const PaperPattern pat = isPdf
            ? (st->doc.Pages().empty() ? PaperPattern::Plain : st->doc.Pages()[std::min(st->activePageIndex, st->doc.Pages().size() - 1)].background)
            : st->doc.CanvasBackground();
        gtk_combo_box_set_active(GTK_COMBO_BOX(st->paperCombo), static_cast<gint>(pat));
    }

    if (st->pageSpin) {
        const double maxPg = std::max(1.0, static_cast<double>(st->doc.Pages().size()));
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(st->pageSpin), 1.0, maxPg);
        const double curPg = std::clamp(static_cast<double>(st->activePageIndex + 1), 1.0, maxPg);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(st->pageSpin), curPg);
        gtk_widget_set_sensitive(st->pageSpin, isPdf);
    }
    if (st->pageTotalLabel) {
        const std::string lbl = "/ " + std::to_string(st->doc.Pages().size());
        gtk_label_set_text(GTK_LABEL(st->pageTotalLabel), lbl.c_str());
    }

    if (st->btnUndo) gtk_widget_set_sensitive(st->btnUndo, st->cmdStack.CanUndo());
    if (st->btnRedo) gtk_widget_set_sensitive(st->btnRedo, st->cmdStack.CanRedo());

    st->syncingControls = false;
}

void InvalidateViews(StudioAppState* st) {
    if (!st || st->teardownDone) return;
    SyncSidebarSize(st);
    SyncToolbarControls(st);
    UpdateWindowTitle(st);
    UpdateStatusBar(st);
    if (st->viewport) gtk_widget_queue_draw(st->viewport);
    if (st->sidebarCanvas && gtk_widget_get_visible(st->sidebarBox)) {
        gtk_widget_queue_draw(st->sidebarCanvas);
    }
}

// ---------------- Search & Sidebar Search Results ----------------

void RebuildSearchResultsUi(StudioAppState* st) {
    if (!st || !st->searchResultsList || !st->searchResultsBox) return;
    GList* children = gtk_container_get_children(GTK_CONTAINER(st->searchResultsList));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    if (st->searchMatches.empty()) {
        gtk_widget_hide(st->searchResultsBox);
        return;
    }

    if (st->searchResultsLabel) {
        const std::string hdr = "Resultados (" + std::to_string(st->searchMatches.size()) + ")";
        gtk_label_set_text(GTK_LABEL(st->searchResultsLabel), hdr.c_str());
    }

    const std::size_t cap = std::min<std::size_t>(st->searchMatches.size(), 250);
    for (std::size_t i = 0; i < cap; ++i) {
        const auto& m = st->searchMatches[i];
        std::string snip = m.snippetUtf8;
        for (char& ch : snip) {
            if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        }
        if (snip.size() > 54) snip = snip.substr(0, 51) + "...";
        const std::string text = "P\xC3\xA1g. " + std::to_string(m.pageIndex + 1) + ": " + snip;

        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* lbl = gtk_label_new(text.c_str());
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_widget_set_margin_start(lbl, 6);
        gtk_widget_set_margin_end(lbl, 6);
        gtk_widget_set_margin_top(lbl, 3);
        gtk_widget_set_margin_bottom(lbl, 3);
        gtk_container_add(GTK_CONTAINER(row), lbl);
        g_object_set_data(G_OBJECT(row), "match-index", GINT_TO_POINTER(static_cast<gint>(i)));
        gtk_container_add(GTK_CONTAINER(st->searchResultsList), row);
    }

    gtk_widget_show_all(st->searchResultsBox);
    if (st->activeSearchIdx < cap) {
        if (GtkListBoxRow* r = gtk_list_box_get_row_at_index(
                GTK_LIST_BOX(st->searchResultsList), static_cast<gint>(st->activeSearchIdx))) {
            gtk_list_box_select_row(GTK_LIST_BOX(st->searchResultsList), r);
        }
    }
}

void PerformLiveSearch(StudioAppState* st, const std::string& queryUtf8, bool jumpToMatch) {
    if (!st) return;
    if (queryUtf8.empty()) {
        st->searchMatches.clear();
        st->activeSearchIdx = 0;
        st->statusBannerUtf8 = "B\xC3\xBAsqueda limpia";
        RebuildSearchResultsUi(st);
        InvalidateViews(st);
        return;
    }

    st->searchMatches = st->doc.SearchText(queryUtf8, false);
    if (st->searchMatches.empty()) {
        st->activeSearchIdx = 0;
        st->statusBannerUtf8 = "Sin resultados para: " + queryUtf8;
    } else {
        if (st->activeSearchIdx >= st->searchMatches.size()) st->activeSearchIdx = 0;
        if (jumpToMatch) {
            CenterCameraOnPage(st, st->searchMatches[st->activeSearchIdx].pageIndex);
        }
        st->statusBannerUtf8 = "Coincidencia " + std::to_string(st->activeSearchIdx + 1) + "/" +
                               std::to_string(st->searchMatches.size());
    }
    RebuildSearchResultsUi(st);
    InvalidateViews(st);
}

void StepSearchMatch(StudioAppState* st, int delta) {
    if (!st) return;
    const std::string q = st->searchEntry ? gtk_entry_get_text(GTK_ENTRY(st->searchEntry)) : "";
    if (q.empty()) {
        PerformLiveSearch(st, "", false);
        return;
    }
    if (st->searchMatches.empty()) {
        st->searchMatches = st->doc.SearchText(q, false);
        st->activeSearchIdx = 0;
    } else {
        const std::size_t n = st->searchMatches.size();
        if (delta >= 0) {
            st->activeSearchIdx = (st->activeSearchIdx + 1) % n;
        } else {
            st->activeSearchIdx = (st->activeSearchIdx + n - 1) % n;
        }
    }

    if (st->searchMatches.empty()) {
        st->statusBannerUtf8 = "Sin resultados para: " + q;
    } else {
        CenterCameraOnPage(st, st->searchMatches[st->activeSearchIdx].pageIndex);
        st->statusBannerUtf8 = "Coincidencia " + std::to_string(st->activeSearchIdx + 1) + "/" +
                               std::to_string(st->searchMatches.size());
    }
    RebuildSearchResultsUi(st);
    InvalidateViews(st);
}

// ---------------- Image Loading Helper (PNG / JPG / BMP via GdkPixbuf) ----------------

bool LoadImageAsPngObject(
    StudioAppState* st,
    const std::string& pathUtf8,
    AnnotationObject& outObj,
    std::string* outErr = nullptr)
{
    GError* gerr = nullptr;
    GdkPixbuf* pb = gdk_pixbuf_new_from_file(pathUtf8.c_str(), &gerr);
    if (!pb) {
        if (outErr) {
            *outErr = gerr && gerr->message ? gerr->message : "No se pudo decodificar la imagen.";
        }
        if (gerr) g_error_free(gerr);
        return false;
    }

    const int iw = gdk_pixbuf_get_width(pb);
    const int ih = gdk_pixbuf_get_height(pb);
    std::vector<std::uint8_t> pngBytes;
    const std::string ext = GetFileExtLowerUtf8(pathUtf8);

    if (ext == ".png") {
        ReadAllBytesUtf8(pathUtf8, pngBytes, outErr);
    }
    if (pngBytes.empty()) {
        gchar* buf = nullptr;
        gsize bufLen = 0;
        if (gdk_pixbuf_save_to_buffer(pb, &buf, &bufLen, "png", &gerr, nullptr) && buf && bufLen > 0) {
            pngBytes.assign(reinterpret_cast<const std::uint8_t*>(buf),
                            reinterpret_cast<const std::uint8_t*>(buf) + bufLen);
            g_free(buf);
        } else {
            if (outErr) {
                *outErr = gerr && gerr->message ? gerr->message : "No se pudo codificar PNG.";
            }
            if (gerr) g_error_free(gerr);
            g_object_unref(pb);
            return false;
        }
    }
    g_object_unref(pb);

    const double aspect = (iw > 0 && ih > 0) ? (static_cast<double>(ih) / static_cast<double>(iw)) : 0.67;
    const double targetW = 420.0;
    const double targetH = std::clamp(targetW * aspect, 48.0, 620.0);

    outObj = AnnotationObject{};
    outObj.kind = ObjectKind::Image;
    outObj.pngBytes = std::move(pngBytes);
    outObj.imagePixelW = iw;
    outObj.imagePixelH = ih;
    if (st->doc.Mode() == DocumentMode::InfiniteCanvas) {
        const DocPoint topLeft = st->camera.ScreenToWorld(80.0, 80.0);
        outObj.bounds = { topLeft.x, topLeft.y, targetW, targetH };
    } else {
        outObj.bounds = { 48.0, 48.0, targetW, targetH };
    }
    return true;
}

// ---------------- Document Open / Save / Export / PDF Actions ----------------

bool OpenDocumentFile(StudioAppState* st, const std::string& pathUtf8, std::string* outErr) {
    if (!st || pathUtf8.empty()) return false;
    const std::string ext = GetFileExtLowerUtf8(pathUtf8);
    const std::wstring wpath = Utf8ToWide(pathUtf8);

    if (ext == ".pdf") {
        PdfDocument pdf;
        std::string err;
        if (!pdf.LoadFromFile(wpath, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error PDF: " + err;
            return false;
        }
        if (!pdf.PopulateDocumentModel(st->doc, pathUtf8, &err)) {
            if (outErr) *outErr = err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPathUtf8.clear();
        st->activePdfSourcePathUtf8 = pathUtf8;
        st->selectedObjectId = 0;
        st->searchMatches.clear();
        st->doc.SetTitle(std::filesystem::u8path(pathUtf8).stem().u8string());
        const std::size_t targetPg = static_cast<std::size_t>(std::max(0, st->initialPage1Based - 1));
        CenterCameraOnPage(st, targetPg);
        st->statusBannerUtf8 = "PDF abierto: " + std::to_string(st->doc.Pages().size()) + " p\xC3\xA1ginas";
    } else if (ext == ".cudoc") {
        bool usedBak = false;
        std::string err;
        if (!CudocStorage::LoadFromFileWithRecovery(wpath, st->doc, &usedBak, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .cudoc: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPathUtf8 = pathUtf8;
        st->activePdfSourcePathUtf8.clear();
        st->selectedObjectId = 0;
        st->searchMatches.clear();
        const std::size_t targetPg = static_cast<std::size_t>(std::max(0, st->initialPage1Based - 1));
        if (st->doc.Mode() == DocumentMode::PaginatedPdf) {
            CenterCameraOnPage(st, targetPg);
        } else {
            st->camera.zoom = 1.0;
            st->camera.panX = -48.0;
            st->camera.panY = -48.0;
        }
        st->statusBannerUtf8 = usedBak ? "Recuperado desde respaldo .bak" : ".cudoc cargado";
    } else if (ext == ".docx") {
        std::string err;
        if (!OpenXmlInterop::ImportFromDocxFile(wpath, st->doc, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .docx: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPathUtf8.clear();
        st->activePdfSourcePathUtf8.clear();
        st->selectedObjectId = 0;
        st->searchMatches.clear();
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = "Word .docx importado";
    } else if (ext == ".pptx") {
        std::string err;
        if (!OpenXmlInterop::ImportFromPptxFile(wpath, st->doc, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .pptx: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPathUtf8.clear();
        st->activePdfSourcePathUtf8.clear();
        st->selectedObjectId = 0;
        st->searchMatches.clear();
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = "PowerPoint .pptx importado";
    } else if (ext == ".xlsx") {
        std::string err;
        if (!DocStudioApp::ImportFromXlsxFile(pathUtf8, st->doc, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .xlsx: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPathUtf8.clear();
        st->activePdfSourcePathUtf8.clear();
        st->selectedObjectId = 0;
        st->searchMatches.clear();
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = "Excel .xlsx importado";
    } else if (ext == ".csv") {
        std::string err;
        if (!DocStudioApp::ImportFromCsvFile(pathUtf8, st->doc, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .csv: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPathUtf8.clear();
        st->activePdfSourcePathUtf8.clear();
        st->selectedObjectId = 0;
        st->searchMatches.clear();
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = "Tabla .csv importada";
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
        AnnotationObject imgObj;
        std::string err;
        if (!LoadImageAsPngObject(st, pathUtf8, imgObj, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error imagen: " + err;
            return false;
        }
        const bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
        const std::size_t pg = st->activePageIndex;
        auto cmd = std::make_unique<AddObjectCommand>(ObjectTarget{ isCanvas, pg }, imgObj);
        const std::uint64_t newId = imgObj.id;
        if (st->cmdStack.Execute(st->doc, std::move(cmd))) {
            st->selectedObjectId = newId;
            st->selectedIsCanvas = isCanvas;
            st->selectedPageIndex = pg;
        }
        st->statusBannerUtf8 = "Imagen insertada en estudio";
    } else {
        if (outErr) *outErr = "Extensi\xC3\xB3n de archivo no soportada: " + ext;
        return false;
    }

    RebuildSearchResultsUi(st);
    InvalidateViews(st);
    return true;
}

void ActionOpenFile(StudioAppState* st) {
    if (!st) return;
    const std::string path = RunFileOpenDialog(
        GTK_WINDOW(st->window),
        "Abrir documento",
        {
            { "Documentos compatibles (*.pdf, *.cudoc, *.png, *.jpg, *.docx, *.pptx, *.xlsx, *.csv)",
              { "*.pdf", "*.cudoc", "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.docx", "*.pptx", "*.xlsx", "*.csv" } },
            { "Documentos PDF (*.pdf)", { "*.pdf" } },
            { "Document Studio (*.cudoc)", { "*.cudoc" } },
            { "Office y Tablas (*.docx, *.pptx, *.xlsx, *.csv)", { "*.docx", "*.pptx", "*.xlsx", "*.csv" } },
            { "Im\xC3\xA1genes (*.png, *.jpg, *.bmp)", { "*.png", "*.jpg", "*.jpeg", "*.bmp" } }
        });
    if (path.empty()) return;
    std::string err;
    if (!OpenDocumentFile(st, path, &err)) {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error al abrir", err);
    }
}

void ActionSaveCudoc(StudioAppState* st, bool forceSaveAs = false) {
    if (!st) return;
    std::string savePath = forceSaveAs ? std::string{} : st->currentDocPathUtf8;
    if (savePath.empty()) {
        savePath = RunFileSaveDialog(
            GTK_WINDOW(st->window),
            "Guardar proyecto .cudoc",
            "documento.cudoc",
            "Document Studio (*.cudoc)",
            "*.cudoc",
            ".cudoc");
        if (savePath.empty()) return;
    }
    std::string err;
    if (CudocStorage::SaveToFile(Utf8ToWide(savePath), st->doc, &err)) {
        st->currentDocPathUtf8 = savePath;
        st->cmdStack.MarkClean(st->doc);
        st->statusBannerUtf8 = "Guardado at\xC3\xB3micamente (.cudoc + .bak)";
        InvalidateViews(st);
    } else {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error al guardar", err);
    }
}

void ActionExportPdf(StudioAppState* st) {
    if (!st) return;
    const std::string outPath = RunFileSaveDialog(
        GTK_WINDOW(st->window),
        "Exportar PDF",
        "exportado.pdf",
        "Archivo PDF (*.pdf)",
        "*.pdf",
        ".pdf");
    if (outPath.empty()) return;

    std::string err;
    bool ok = false;
    if (st->doc.Mode() == DocumentMode::InfiniteCanvas) {
        CanvasPdfExportOptions opts;
        opts.paginated = true;
        ok = PdfWriter::ExportInfiniteCanvasToFile(st->doc, opts, Utf8ToWide(outPath), &err);
    } else {
        ok = PdfWriter::ExportDocumentToFile(st->doc, Utf8ToWide(outPath), &err);
    }
    st->statusBannerUtf8 = ok ? "PDF exportado correctamente" : ("Error PDF: " + err);
    InvalidateViews(st);
    if (!ok) ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error exportando PDF", err);
}

void ActionMergePdfs(StudioAppState* st) {
    if (!st) return;
    std::vector<std::string> inputs;
    RunFileOpenDialog(
        GTK_WINDOW(st->window),
        "Seleccionar PDFs para combinar (m\xC3\xADnimo 2)",
        { { "Archivos PDF (*.pdf)", { "*.pdf" } } },
        &inputs);
    if (inputs.size() < 2) {
        if (!inputs.empty()) {
            ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_INFO, "Combinar PDF",
                "Seleccione al menos 2 archivos PDF para combinar.");
        }
        return;
    }
    const std::string outPath = RunFileSaveDialog(
        GTK_WINDOW(st->window),
        "Guardar PDF combinado",
        "combinado.pdf",
        "Archivo PDF (*.pdf)",
        "*.pdf",
        ".pdf");
    if (outPath.empty()) return;

    std::vector<std::wstring> wInputs;
    wInputs.reserve(inputs.size());
    for (const auto& p : inputs) wInputs.push_back(Utf8ToWide(p));

    std::string err;
    if (PdfWriter::MergePdfFiles(wInputs, Utf8ToWide(outPath), &err)) {
        OpenDocumentFile(st, outPath);
        st->statusBannerUtf8 = "PDFs combinados (" + std::to_string(inputs.size()) + " archivos)";
        InvalidateViews(st);
    } else {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error combinando PDF", err);
    }
}

void ActionSplitPdf(StudioAppState* st) {
    if (!st || st->doc.Pages().empty()) return;
    std::string rangeStr = "1-" + std::to_string(st->doc.Pages().size());
    if (!ShowSingleInputPrompt(GTK_WINDOW(st->window),
            "Dividir / Extraer P\xC3\xA1ginas",
            "P\xC3\xA1ginas (ej. 1-3, 5):",
            rangeStr)) {
        return;
    }

    std::vector<std::size_t> picked;
    std::istringstream iss(rangeStr);
    std::string part;
    const int totalPg = static_cast<int>(st->doc.Pages().size());
    while (std::getline(iss, part, ',')) {
        const auto dash = part.find('-');
        try {
            if (dash != std::string::npos) {
                const int a = std::stoi(part.substr(0, dash));
                const int b = std::stoi(part.substr(dash + 1));
                for (int p = std::max(1, a); p <= b && p <= totalPg; ++p) {
                    picked.push_back(static_cast<std::size_t>(p - 1));
                }
            } else {
                const int p = std::stoi(part);
                if (p >= 1 && p <= totalPg) picked.push_back(static_cast<std::size_t>(p - 1));
            }
        } catch (...) {}
    }

    if (picked.empty()) {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_WARNING, "Rango inv\xC3\xA1lido",
            "No se especificaron n\xC3\xBAmeros de p\xC3\xA1gina v\xC3\xA1lidos.");
        return;
    }

    const std::string outPath = RunFileSaveDialog(
        GTK_WINDOW(st->window),
        "Guardar PDF dividido",
        "extraccion.pdf",
        "Archivo PDF (*.pdf)",
        "*.pdf",
        ".pdf");
    if (outPath.empty()) return;

    DocumentModel subset;
    subset.ResetToBlank(DocumentMode::PaginatedPdf);
    subset.MutablePages().clear();
    for (std::size_t idx : picked) {
        subset.MutablePages().push_back(st->doc.Pages()[idx]);
    }
    subset.RebuildLayout();

    std::string err;
    if (PdfWriter::ExportDocumentToFile(subset, Utf8ToWide(outPath), &err)) {
        st->statusBannerUtf8 = "Extra\xC3\xAD" "das " + std::to_string(picked.size()) + " p\xC3\xA1ginas";
        InvalidateViews(st);
    } else {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error dividiendo PDF", err);
    }
}

void ActionSignPdf(StudioAppState* st) {
    if (!st) return;
    GtkWidget* dlg = gtk_dialog_new_with_buttons(
        "Firma Digital PKCS#7 (SHA-256)",
        GTK_WINDOW(st->window),
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Firmar y Guardar...", GTK_RESPONSE_OK,
        nullptr);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 460, 320);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget* lSigner = gtk_label_new("Firmante:");
    gtk_widget_set_halign(lSigner, GTK_ALIGN_START);
    GtkWidget* eSigner = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(eSigner), "Usuario Calculadora Unificada");
    gtk_widget_set_hexpand(eSigner, TRUE);

    GtkWidget* lReason = gtk_label_new("Motivo:");
    gtk_widget_set_halign(lReason, GTK_ALIGN_START);
    GtkWidget* eReason = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(eReason), "Revisado y aprobado");

    GtkWidget* lLoc = gtk_label_new("Ubicaci\xC3\xB3n:");
    gtk_widget_set_halign(lLoc, GTK_ALIGN_START);
    GtkWidget* eLoc = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(eLoc), "Local");

    GtkWidget* rbSelf = gtk_radio_button_new_with_label(nullptr, "Certificado autofirmado RSA-2048 (SHA-256)");
    GtkWidget* rbPfx = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(rbSelf), "Usar archivo PKCS#12 (.pfx / .p12):");

    GtkWidget* pfxChooser = gtk_file_chooser_button_new("Seleccionar certificado .pfx", GTK_FILE_CHOOSER_ACTION_OPEN);
    GtkFileFilter* ff = gtk_file_filter_new();
    gtk_file_filter_set_name(ff, "Certificados PKCS#12 (*.pfx, *.p12)");
    gtk_file_filter_add_pattern(ff, "*.pfx");
    gtk_file_filter_add_pattern(ff, "*.p12");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(pfxChooser), ff);
    gtk_widget_set_sensitive(pfxChooser, FALSE);

    GtkWidget* lPass = gtk_label_new("Contrase\xC3\xB1" "a .pfx:");
    gtk_widget_set_halign(lPass, GTK_ALIGN_START);
    GtkWidget* ePass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(ePass), FALSE);
    gtk_widget_set_sensitive(ePass, FALSE);

    GtkWidget* chkVisual = gtk_check_button_new_with_label("Estampar sello visual en p\xC3\xA1gina actual");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkVisual), TRUE);

    g_object_set_data(G_OBJECT(rbPfx), "pfx-chooser", pfxChooser);
    g_object_set_data(G_OBJECT(rbPfx), "pfx-pass", ePass);
    const auto onPfxToggle = +[](GtkToggleButton* btn, gpointer) {
        auto* chooser = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(btn), "pfx-chooser"));
        auto* pass = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(btn), "pfx-pass"));
        const gboolean usePfx = gtk_toggle_button_get_active(btn);
        if (chooser) gtk_widget_set_sensitive(chooser, usePfx);
        if (pass) gtk_widget_set_sensitive(pass, usePfx);
    };
    g_signal_connect(rbPfx, "toggled", G_CALLBACK(onPfxToggle), nullptr);

    gtk_grid_attach(GTK_GRID(grid), lSigner, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), eSigner, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lReason, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), eReason, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lLoc,    0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), eLoc,    1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), rbSelf,  0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), rbPfx,   0, 4, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), pfxChooser, 1, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lPass,   0, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ePass,   1, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), chkVisual, 0, 7, 2, 1);

    gtk_widget_show_all(dlg);
    const gint resp = gtk_dialog_run(GTK_DIALOG(dlg));
    if (resp != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dlg);
        return;
    }

    PdfSignatureOptions sopts;
    sopts.signerNameUtf8 = gtk_entry_get_text(GTK_ENTRY(eSigner));
    sopts.reasonUtf8 = gtk_entry_get_text(GTK_ENTRY(eReason));
    sopts.locationUtf8 = gtk_entry_get_text(GTK_ENTRY(eLoc));
    sopts.addVisualStamp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkVisual));
    sopts.visualPageIndex0 = st->activePageIndex;

    const bool usePfx = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rbPfx));
    std::string pfxPathUtf8;
    std::string pfxPassUtf8;
    if (usePfx) {
        char* p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(pfxChooser));
        if (p) {
            pfxPathUtf8 = p;
            g_free(p);
        }
        pfxPassUtf8 = gtk_entry_get_text(GTK_ENTRY(ePass));
    }
    gtk_widget_destroy(dlg);

    if (usePfx && pfxPathUtf8.empty()) {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_WARNING, "Certificado faltante",
            "Seleccione un archivo .pfx/.p12 v\xC3\xA1lido o use el modo autofirmado.");
        return;
    }

    const std::string outPath = RunFileSaveDialog(
        GTK_WINDOW(st->window),
        "Guardar PDF firmado",
        "firmado.pdf",
        "PDF Firmado (*.pdf)",
        "*.pdf",
        ".pdf");
    if (outPath.empty()) return;

    std::string err;
    if (PdfSigner::ExportAndSignDocument(
            st->doc, sopts, Utf8ToWide(outPath),
            Utf8ToWide(pfxPathUtf8), Utf8ToWide(pfxPassUtf8), &err)) {
        st->statusBannerUtf8 = "PDF firmado criptogr\xC3\xA1" "ficamente (PKCS#7 SHA-256)";
        InvalidateViews(st);
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_INFO, "Firma exitosa",
            "Documento firmado con PKCS#7 detached SHA-256.\nPuede verificarlo con el bot\xC3\xB3n 'Verificar'.");
    } else {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error firmando PDF", err);
    }
}

void ActionVerifyPdfSignature(StudioAppState* st) {
    if (!st) return;
    const std::string path = RunFileOpenDialog(
        GTK_WINDOW(st->window),
        "Seleccionar PDF a verificar",
        { { "Archivo PDF (*.pdf)", { "*.pdf" } } });
    if (path.empty()) return;

    std::vector<std::uint8_t> bytes;
    std::string err;
    if (!ReadAllBytesUtf8(path, bytes, &err)) {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error", err);
        return;
    }

    const PdfSignatureVerifyResult vres = PdfSigner::VerifyPdfBytes(bytes);
    const GtkMessageType mtype = (vres.hasSignature && vres.byteRangeValid && vres.cryptographicValid)
        ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING;

    std::ostringstream oss;
    oss << "Firma detectada: " << (vres.hasSignature ? "S\xC3\x8D" : "NO") << "\n"
        << "Firmante (Subject): " << (vres.signerSubjectUtf8.empty() ? "-" : vres.signerSubjectUtf8) << "\n"
        << "Motivo: " << (vres.reasonUtf8.empty() ? "-" : vres.reasonUtf8) << "\n"
        << "Ubicaci\xC3\xB3n: " << (vres.locationUtf8.empty() ? "-" : vres.locationUtf8) << "\n"
        << "ByteRange: ";
    if (vres.byteRange.empty()) {
        oss << "-";
    } else {
        oss << "[";
        for (std::size_t i = 0; i < vres.byteRange.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << vres.byteRange[i];
        }
        oss << "]";
    }
    oss << " (" << (vres.byteRangeValid ? "\xC3\x8DNTEGRO" : "INV\xC3\x81LIDO") << ")\n"
        << "Estado criptogr\xC3\xA1" "fico: " << (vres.cryptographicValid ? "V\xC3\x81LIDA" : "INV\xC3\x81LIDA") << "\n"
        << "Cadena de confianza: " << (vres.trustedBySystemRoot ? "Ra\xC3\xADz de sistema" : "Autofirmada / no ra\xC3\xADz") << "\n\n"
        << vres.diagnosticUtf8;

    ShowMessageDialog(GTK_WINDOW(st->window), mtype, "Verificaci\xC3\xB3n PKCS#7", oss.str());
}

void ActionRunOcr(StudioAppState* st) {
    if (!st || st->doc.Pages().empty()) return;
    const std::size_t idx = std::min(st->activePageIndex, st->doc.Pages().size() - 1);
    RasterImageBgra32 img;
    if (!st->renderer.RenderPageOffscreen(st->doc, idx, 1400, &st->pageCache, img)) {
        st->statusBannerUtf8 = "No se pudo rasterizar p\xC3\xA1gina para OCR";
        InvalidateViews(st);
        return;
    }

    const OcrOutput ocr = WinRtOcrAdapter::RecognizeBgra32(img.width, img.height, img.pixels.data());
    if (ocr.engineAvailable && !ocr.fullTextUtf8.empty()) {
        st->doc.MutablePages()[idx].ocrTextUtf8 = ocr.fullTextUtf8;
        st->doc.BumpRevision();
        st->statusBannerUtf8 = "OCR completado (" + ocr.languageTagUtf8 + "): " +
                               std::to_string(ocr.words.size()) + " palabras";
        InvalidateViews(st);
        ShowScrollableTextDialog(
            GTK_WINDOW(st->window),
            "Texto reconocido (OCR offline)",
            "Idioma: " + ocr.languageTagUtf8 + "  |  Palabras: " + std::to_string(ocr.words.size()),
            ocr.fullTextUtf8);
    } else {
        const std::string msg = ocr.errorUtf8.empty()
            ? "Sin texto detectado en esta p\xC3\xA1gina."
            : ocr.errorUtf8;
        st->statusBannerUtf8 = msg;
        InvalidateViews(st);
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_INFO, "Resultado OCR", msg);
    }
}

void ActionInteropExport(StudioAppState* st, const std::string& fmt) {
    if (!st) return;
    std::string title, defName, label, pattern, dotExt;
    if (fmt == "docx") {
        title = "Exportar Word .docx"; defName = "documento.docx"; label = "Word (*.docx)"; pattern = "*.docx"; dotExt = ".docx";
    } else if (fmt == "pptx") {
        title = "Exportar PowerPoint .pptx"; defName = "presentacion.pptx"; label = "PowerPoint (*.pptx)"; pattern = "*.pptx"; dotExt = ".pptx";
    } else if (fmt == "xlsx") {
        title = "Exportar Excel .xlsx"; defName = "datos.xlsx"; label = "Excel (*.xlsx)"; pattern = "*.xlsx"; dotExt = ".xlsx";
    } else {
        title = "Exportar CSV .csv"; defName = "datos.csv"; label = "CSV (*.csv)"; pattern = "*.csv"; dotExt = ".csv";
    }

    const std::string outPath = RunFileSaveDialog(
        GTK_WINDOW(st->window), title.c_str(), defName.c_str(), label, pattern, dotExt);
    if (outPath.empty()) return;

    std::string err;
    bool ok = false;
    if (fmt == "docx" || fmt == "pptx") {
        std::vector<std::vector<std::uint8_t>> snaps;
        const std::size_t maxSnapPages = std::min<std::size_t>(st->doc.Pages().size(), 12u);
        for (std::size_t p = 0; p < maxSnapPages; ++p) {
            RasterImageBgra32 img;
            std::vector<std::uint8_t> png;
            if (st->renderer.RenderPageOffscreen(st->doc, p, 900, &st->pageCache, img) &&
                WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, png)) {
                snaps.push_back(std::move(png));
            } else {
                snaps.push_back({});
            }
        }
        ok = (fmt == "docx")
            ? OpenXmlInterop::ExportToDocxFile(st->doc, Utf8ToWide(outPath), snaps, &err)
            : OpenXmlInterop::ExportToPptxFile(st->doc, Utf8ToWide(outPath), snaps, &err);
    } else if (fmt == "xlsx") {
        ok = DocStudioApp::ExportToXlsxFile(st->doc, outPath, &err);
    } else {
        ok = DocStudioApp::ExportToCsvFile(st->doc, outPath, &err);
    }

    st->statusBannerUtf8 = ok ? ("Exportaci\xC3\xB3n ." + fmt + " completada") : err;
    InvalidateViews(st);
    if (!ok) ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error de exportaci\xC3\xB3n", err);
}

void ActionInteropImport(StudioAppState* st, const std::string& filterLabel, const std::string& pattern) {
    if (!st) return;
    const std::string path = RunFileOpenDialog(
        GTK_WINDOW(st->window),
        "Importar documento",
        { { filterLabel, { pattern } } });
    if (path.empty()) return;
    std::string err;
    if (!OpenDocumentFile(st, path, &err)) {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error importando", err);
    }
}

void ActionImportPdfToCanvas(StudioAppState* st) {
    if (!st) return;
    const std::string path = RunFileOpenDialog(
        GTK_WINDOW(st->window),
        "Seleccionar PDF para insertar p\xC3\xA1gina en Lienzo \xE2\x88\x9E",
        { { "PDF (*.pdf)", { "*.pdf" } } });
    if (path.empty()) return;

    int pageCount = 0;
    double wPt = 595.0, hPt = 842.0;
    std::string err;
    if (!WinRtPdfRenderer::GetPageCountAndSize(Utf8ToWide(path), 0, pageCount, wPt, hPt, &err) || pageCount <= 0) {
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error PDF",
            err.empty() ? "No se pudo leer el PDF." : err);
        return;
    }

    std::string pgStr = "1";
    if (pageCount > 1) {
        const std::string promptLbl = "N\xC3\xBAmero de p\xC3\xA1gina (1-" + std::to_string(pageCount) + "):";
        if (!ShowSingleInputPrompt(GTK_WINDOW(st->window), "Importar p\xC3\xA1gina a Lienzo", promptLbl.c_str(), pgStr)) {
            return;
        }
    }
    int pg1 = 1;
    try { pg1 = std::clamp(std::stoi(pgStr), 1, pageCount); } catch (...) { pg1 = 1; }
    const int pg0 = pg1 - 1;

    WinRtPdfRenderer::GetPageCountAndSize(Utf8ToWide(path), pg0, pageCount, wPt, hPt, nullptr);
    RasterImageBgra32 img;
    std::vector<std::uint8_t> png;
    if (WinRtPdfRenderer::RenderPageToBgra32(Utf8ToWide(path), pg0, 960, 0, NormalizedCropRect{}, img, nullptr, &err)) {
        WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, png);
    }

    if (st->doc.Mode() != DocumentMode::InfiniteCanvas) {
        st->doc.SetMode(DocumentMode::InfiniteCanvas);
        st->camera.panX = -48.0;
        st->camera.panY = -48.0;
        st->camera.zoom = 1.0;
    }

    const DocPoint anchor = st->camera.ScreenToWorld(80.0, 80.0);
    const double cardW = 360.0;
    const double cardH = cardW * (std::max(100.0, hPt) / std::max(100.0, wPt));

    AnnotationObject card;
    card.kind = ObjectKind::PdfCard;
    card.pdfSourcePathUtf8 = path;
    card.pdfSourcePageIndex0 = pg0;
    card.pngBytes = std::move(png);
    card.bounds = { anchor.x, anchor.y, cardW, cardH };
    card.captionUtf8 = std::filesystem::u8path(path).filename().u8string();
    st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, std::move(card)));
    st->statusBannerUtf8 = "Tarjeta PDF p." + std::to_string(pg1) + " agregada al Lienzo";
    InvalidateViews(st);
}

void ActionReplaceText(StudioAppState* st) {
    if (!st || st->doc.Pages().empty()) return;
    std::string oldTxt = st->searchEntry ? gtk_entry_get_text(GTK_ENTRY(st->searchEntry)) : "";
    std::string newTxt;
    if (!ShowTwoFieldPrompt(GTK_WINDOW(st->window),
            "Reemplazar / Redactar texto",
            "Texto actual:", oldTxt,
            "Nuevo texto:", newTxt)) {
        return;
    }

    std::string report = "Modo cobertura/redactado + TextBox preparado.";
    if (!st->activePdfSourcePathUtf8.empty()) {
        PdfDocument pdf;
        if (pdf.LoadFromFile(Utf8ToWide(st->activePdfSourcePathUtf8))) {
            const auto cap = pdf.InspectTextReplacement(st->activePageIndex, oldTxt);
            report = cap.diagnosticUtf8;
        }
    }

    AnnotationObject redactBox;
    redactBox.kind = ObjectKind::TextBox;
    redactBox.bounds = { 80.0, 120.0, 320.0, 34.0 };
    redactBox.fillColor = DocColor::RGBA(255, 255, 255, 255);
    redactBox.strokeColor = DocColor::RGBA(17, 24, 39, 255);
    redactBox.fontSizePt = 13.0;
    redactBox.textUtf8 = newTxt;
    st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(
        ObjectTarget{ false, st->activePageIndex }, std::move(redactBox)));
    st->statusBannerUtf8 = report;
    InvalidateViews(st);
    ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_INFO, "Reemplazo sem\xC3\xA1ntico PDF", report);
}

void ActionPrintDocument(StudioAppState* st) {
    if (!st || st->doc.Pages().empty()) return;
    GtkPrintOperation* op = gtk_print_operation_new();
    gtk_print_operation_set_n_pages(op, static_cast<gint>(st->doc.Pages().size()));
    gtk_print_operation_set_job_name(op, "Document Studio");

    g_signal_connect(op, "draw-page", G_CALLBACK(+[](GtkPrintOperation*, GtkPrintContext* ctx, gint pageNr, gpointer data) {
        auto* s = static_cast<StudioAppState*>(data);
        if (!s || pageNr < 0 || static_cast<std::size_t>(pageNr) >= s->doc.Pages().size()) return;
        RasterImageBgra32 img;
        if (!s->renderer.RenderPageOffscreen(s->doc, static_cast<std::size_t>(pageNr), 1600, &s->pageCache, img) || !img.Valid()) {
            return;
        }
        cairo_t* cr = gtk_print_context_get_cairo_context(ctx);
        const double cw = gtk_print_context_get_width(ctx);
        const double ch = gtk_print_context_get_height(ctx);
        const double scale = std::min(cw / img.width, ch / img.height);
        const double dstX = (cw - img.width * scale) * 0.5;
        const double dstY = (ch - img.height * scale) * 0.5;

        cairo_surface_t* surf = cairo_image_surface_create_for_data(
            img.pixels.data(), CAIRO_FORMAT_ARGB32, img.width, img.height, img.width * 4);
        cairo_save(cr);
        cairo_translate(cr, dstX, dstY);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, surf, 0.0, 0.0);
        cairo_paint(cr);
        cairo_restore(cr);
        cairo_surface_destroy(surf);
    }), st);

    GError* gerr = nullptr;
    const GtkPrintOperationResult res = gtk_print_operation_run(
        op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(st->window), &gerr);
    if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
        const std::string err = gerr && gerr->message ? gerr->message : "Error imprimiendo documento.";
        if (gerr) g_error_free(gerr);
        ShowMessageDialog(GTK_WINDOW(st->window), GTK_MESSAGE_ERROR, "Error de impresi\xC3\xB3n", err);
    } else if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
        st->statusBannerUtf8 = "Trabajo de impresi\xC3\xB3n enviado";
        InvalidateViews(st);
    }
    g_object_unref(op);
}

// ---------------- Viewport & Sidebar Drawing + Input ----------------

gboolean on_viewport_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st) return FALSE;

    const int allocW = std::max(1, gtk_widget_get_allocated_width(widget));
    const int allocH = std::max(1, gtk_widget_get_allocated_height(widget));

    ViewportOverlayState ov{};
    PopulateViewportOverlay(st, ov);

    st->renderer.RenderToCairo(
        cr, st->doc, st->camera, allocW, allocH,
        &st->pageCache, st->asyncGen, st->MakeWakeupCallback(), &ov);
    st->viewportDrawnFlag = true;
    return FALSE;
}

void on_viewport_size_allocate(GtkWidget* widget, GtkAllocation* alloc, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !alloc) return;
    if (st->needsInitialFit && alloc->width >= 160 && alloc->height >= 120) {
        st->needsInitialFit = false;
        if (st->doc.Mode() == DocumentMode::PaginatedPdf && !st->doc.Pages().empty()) {
            CenterCameraOnPage(st, st->activePageIndex);
        }
        UpdateStatusBar(st);
        gtk_widget_queue_draw(widget);
    }
}

bool ComputeLocalCoordsForPoint(
    StudioAppState* st,
    double screenX,
    double screenY,
    bool requireHitOnPdfPage,
    bool& outIsCanvas,
    std::size_t& outPageIdx,
    double& outLx,
    double& outLy)
{
    const DocPoint wpWorld = st->camera.ScreenToWorld(screenX, screenY);
    outIsCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
    if (outIsCanvas) {
        outPageIdx = 0;
        outLx = wpWorld.x;
        outLy = wpWorld.y;
        return true;
    }

    if (st->doc.Layout().HitTestWorldPoint(wpWorld.x, wpWorld.y, outPageIdx, outLx, outLy)) {
        return true;
    }
    if (requireHitOnPdfPage) return false;

    outPageIdx = st->draftPageIdx;
    if (const auto* pl = st->doc.Layout().PlacementAt(outPageIdx)) {
        const DocRect wr = pl->WorldRect();
        outLx = wpWorld.x - wr.x;
        outLy = wpWorld.y - wr.y;
        return true;
    }
    return false;
}

void EraseHitsAtPoint(StudioAppState* st, bool isCanvas, std::size_t pageIdx, double lx, double ly) {
    const double tol = std::clamp(10.0 / std::max(0.25, st->camera.zoom), 4.0, 24.0);
    const ObjectTarget erTarget{ isCanvas, pageIdx };
    const auto rawHits = CollectHits(st, isCanvas, pageIdx, lx, ly, tol);
    const auto hits = InkEngine::FilterHitsByEraserMode(st->doc, erTarget, rawHits, st->eraserFilterMode, true);
    if (!hits.empty()) {
        for (std::uint64_t id : hits) {
            if (id == st->selectedObjectId) st->selectedObjectId = 0;
        }
        st->cmdStack.Execute(st->doc, std::make_unique<BatchEraseObjectsCommand>(
            erTarget, hits));
        InvalidateViews(st);
    }
}

gboolean on_viewport_button_press(GtkWidget* widget, GdkEventButton* ev, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !ev) return FALSE;
    gtk_widget_grab_focus(widget);

    float pressure = 0.55f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    gdouble rawP = 0.0;
    if (gdk_event_get_axis(reinterpret_cast<const GdkEvent*>(ev), GDK_AXIS_PRESSURE, &rawP)) {
        pressure = std::clamp(static_cast<float>(rawP), 0.1f, 1.0f);
    }
    gdouble rawTx = 0.0, rawTy = 0.0;
    if (gdk_event_get_axis(reinterpret_cast<const GdkEvent*>(ev), GDK_AXIS_XTILT, &rawTx)) {
        tiltX = std::clamp(static_cast<float>(rawTx) * 60.0f, -85.0f, 85.0f);
    }
    if (gdk_event_get_axis(reinterpret_cast<const GdkEvent*>(ev), GDK_AXIS_YTILT, &rawTy)) {
        tiltY = std::clamp(static_cast<float>(rawTy) * 60.0f, -85.0f, 85.0f);
    }

    // Pan with Right/Middle mouse, Hand tool, or Space held
    if (ev->button == 2 || ev->button == 3 || st->activeTool == ToolKind::Hand || st->spacePanHeld) {
        if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1 && st->activeTool == ToolKind::Hand) {
            bool isCanvas = false;
            std::size_t pgIdx = 0;
            double lx = 0.0, ly = 0.0;
            if (ComputeLocalCoordsForPoint(st, ev->x, ev->y, true, isCanvas, pgIdx, lx, ly)) {
                const double tol = std::clamp(8.0 / std::max(0.25, st->camera.zoom), 3.0, 18.0);
                const auto hits = CollectHits(st, isCanvas, pgIdx, lx, ly, tol);
                if (!hits.empty()) {
                    const std::uint64_t hitId = hits.back();
                    AnnotationObject* obj = isCanvas
                        ? st->doc.FindCanvasObjectMutable(hitId)
                        : st->doc.FindPageObjectMutable(pgIdx, hitId);
                    if (obj && obj->kind == ObjectKind::TextBox) {
                        AnnotationObject before = *obj;
                        std::string txt = obj->textUtf8;
                        double fsize = obj->fontSizePt;
                        if (ShowSingleInputPrompt(GTK_WINDOW(st->window), "Editar Cuadro de Texto",
                                "Contenido (UTF-8):", txt, true, &fsize) && !txt.empty()) {
                            AnnotationObject after = before;
                            after.textUtf8 = txt;
                            after.fontSizePt = fsize;
                            st->cmdStack.Execute(st->doc, std::make_unique<ModifyObjectCommand>(
                                ObjectTarget{ isCanvas, pgIdx }, std::move(before), std::move(after)));
                            st->selectedObjectId = hitId;
                            st->selectedIsCanvas = isCanvas;
                            st->selectedPageIndex = pgIdx;
                            InvalidateViews(st);
                        }
                        return TRUE;
                    }
                }
            }
        }

        st->panning = true;
        st->panMoved = false;
        st->panStartX = ev->x;
        st->panStartY = ev->y;
        st->lastPanX = ev->x;
        st->lastPanY = ev->y;
        return TRUE;
    }

    if (ev->button != 1) return FALSE;

    if (st->activeTool == ToolKind::Laser) {
        const DocPoint wpWorld = st->camera.ScreenToWorld(ev->x, ev->y);
        st->drawing = true;
        st->laserPointer.active = true;
        st->laserPointer.worldX = wpWorld.x;
        st->laserPointer.worldY = wpWorld.y;
        st->laserPointer.trail.clear();
        st->laserPointer.trail.push_back(wpWorld);
        gtk_widget_queue_draw(widget);
        return TRUE;
    }

    bool isCanvas = false;
    std::size_t pgIdx = st->activePageIndex;
    double lx = 0.0, ly = 0.0;
    if (!ComputeLocalCoordsForPoint(st, ev->x, ev->y, true, isCanvas, pgIdx, lx, ly)) {
        st->selectedObjectId = 0;
        st->lassoSelectedIds.clear();
        InvalidateViews(st);
        return TRUE;
    }
    if (!isCanvas) st->activePageIndex = pgIdx;

    if (st->activeTool == ToolKind::Eraser) {
        st->erasing = true;
        st->eraseIsCanvas = isCanvas;
        st->erasePageIdx = pgIdx;
        EraseHitsAtPoint(st, isCanvas, pgIdx, lx, ly);
        return TRUE;
    }

    if (st->activeTool == ToolKind::TextBox) {
        std::string txt;
        double fontSize = 14.0;
        if (ShowSingleInputPrompt(GTK_WINDOW(st->window), "Insertar Cuadro de Texto",
                "Contenido (UTF-8):", txt, true, &fontSize) && !txt.empty()) {
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { lx, ly, 260.0, 84.0 };
            tb.textUtf8 = txt;
            tb.strokeColor = st->penColor;
            tb.fontSizePt = fontSize;
            auto cmd = std::make_unique<AddObjectCommand>(ObjectTarget{ isCanvas, pgIdx }, tb);
            if (st->cmdStack.Execute(st->doc, std::move(cmd))) {
                const auto& vec = isCanvas ? st->doc.CanvasObjects() : st->doc.Pages()[pgIdx].objects;
                if (!vec.empty()) {
                    st->selectedObjectId = vec.back().id;
                    st->selectedIsCanvas = isCanvas;
                    st->selectedPageIndex = pgIdx;
                }
            }
            InvalidateViews(st);
        }
        return TRUE;
    }

    if (st->activeTool == ToolKind::Crop && isCanvas) {
        st->statusBannerUtf8 = "El recorte aplica solo en modo PDF Paginado";
        InvalidateViews(st);
        return TRUE;
    }

    st->drawing = true;
    st->draftIsCanvas = isCanvas;
    st->draftPageIdx = pgIdx;
    st->draftAnchorWorld = { lx, ly };
    st->hasDraftCrop = false;
    st->draftObj = AnnotationObject{};
    st->draftObj.layerId = st->doc.ActiveLayerId();
    st->draftObj.strokeColor = st->penColor;
    st->draftObj.strokeWidth = (st->activeTool == ToolKind::Highlighter) ? 14.0 : st->penWidth;
    st->draftObj.isHighlighter = (st->activeTool == ToolKind::Highlighter);

    if (st->activeTool == ToolKind::Pen || st->activeTool == ToolKind::Highlighter || st->activeTool == ToolKind::Lasso) {
        st->draftObj.kind = ObjectKind::InkStroke;
        st->hasPredictedTip = false;
        if (st->activeTool == ToolKind::Lasso) {
            st->draftObj.strokeColor = DocColor::RGBA(37, 99, 235, 220);
            st->draftObj.strokeWidth = 2.0;
        } else if (st->rulerState.visible) {
            const DocPoint snapped = InkEngine::ProjectPointOntoRuler({ lx, ly }, st->rulerState);
            lx = snapped.x;
            ly = snapped.y;
        }
        const std::uint64_t relMs = CurrentStrokeRelMs(st);
        st->draftObj.inkStartTimeMs = relMs;
        st->draftObj.inkEndTimeMs = relMs;
        st->draftObj.inkPoints.push_back({ lx, ly, pressure, tiltX, tiltY, relMs });
    } else if (st->activeTool == ToolKind::ShapeRect) {
        st->draftObj.kind = ObjectKind::Shape;
        st->draftObj.shapeKind = ShapeKind::Rectangle;
        st->draftObj.bounds = { lx, ly, 1.0, 1.0 };
    } else if (st->activeTool == ToolKind::ShapeArrow) {
        st->draftObj.kind = ObjectKind::Shape;
        st->draftObj.shapeKind = ShapeKind::Arrow;
        st->draftObj.shapeStart = { lx, ly };
        st->draftObj.shapeEnd = { lx, ly };
    } else if (st->activeTool == ToolKind::Crop && !isCanvas) {
        st->hasDraftCrop = true;
        st->draftCropPageLocal = { lx, ly, 1.0, 1.0 };
    }

    gtk_widget_queue_draw(widget);
    return TRUE;
}

gboolean on_viewport_motion(GtkWidget* widget, GdkEventMotion* ev, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !ev) return FALSE;

    if (st->panning) {
        if (std::hypot(ev->x - st->panStartX, ev->y - st->panStartY) > 3.5) {
            st->panMoved = true;
        }
        const double dx = ev->x - st->lastPanX;
        const double dy = ev->y - st->lastPanY;
        st->camera.panX -= dx / std::max(0.05, st->camera.zoom);
        st->camera.panY -= dy / std::max(0.05, st->camera.zoom);
        st->lastPanX = ev->x;
        st->lastPanY = ev->y;
        RefreshActivePageFromCamera(st);
        InvalidateViews(st);
        return TRUE;
    }

    if (st->erasing) {
        bool isCanvas = false;
        std::size_t pgIdx = st->erasePageIdx;
        double lx = 0.0, ly = 0.0;
        if (ComputeLocalCoordsForPoint(st, ev->x, ev->y, !st->eraseIsCanvas, isCanvas, pgIdx, lx, ly)) {
            EraseHitsAtPoint(st, isCanvas, pgIdx, lx, ly);
        }
        return TRUE;
    }

    if (!st->drawing) return FALSE;

    if (st->activeTool == ToolKind::Laser) {
        const DocPoint wpWorld = st->camera.ScreenToWorld(ev->x, ev->y);
        st->laserPointer.active = true;
        st->laserPointer.worldX = wpWorld.x;
        st->laserPointer.worldY = wpWorld.y;
        st->laserPointer.trail.push_back(wpWorld);
        if (st->laserPointer.trail.size() > 28) {
            st->laserPointer.trail.erase(st->laserPointer.trail.begin());
        }
        gtk_widget_queue_draw(widget);
        return TRUE;
    }

    float pressure = 0.55f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    gdouble rawP = 0.0;
    if (gdk_event_get_axis(reinterpret_cast<const GdkEvent*>(ev), GDK_AXIS_PRESSURE, &rawP)) {
        pressure = std::clamp(static_cast<float>(rawP), 0.1f, 1.0f);
    }
    gdouble rawTx = 0.0, rawTy = 0.0;
    if (gdk_event_get_axis(reinterpret_cast<const GdkEvent*>(ev), GDK_AXIS_XTILT, &rawTx)) {
        tiltX = std::clamp(static_cast<float>(rawTx) * 60.0f, -85.0f, 85.0f);
    }
    if (gdk_event_get_axis(reinterpret_cast<const GdkEvent*>(ev), GDK_AXIS_YTILT, &rawTy)) {
        tiltY = std::clamp(static_cast<float>(rawTy) * 60.0f, -85.0f, 85.0f);
    }

    bool isCanvas = false;
    std::size_t pgIdx = st->draftPageIdx;
    double lx = 0.0, ly = 0.0;
    if (!ComputeLocalCoordsForPoint(st, ev->x, ev->y, false, isCanvas, pgIdx, lx, ly)) {
        return FALSE;
    }

    if (st->draftObj.kind == ObjectKind::InkStroke) {
        if (st->hasPredictedTip && st->draftObj.inkPoints.size() >= 2) {
            st->draftObj.inkPoints.pop_back();
            st->hasPredictedTip = false;
        }
        if (st->activeTool != ToolKind::Lasso && st->rulerState.visible) {
            const DocPoint snapped = InkEngine::ProjectPointOntoRuler({ lx, ly }, st->rulerState);
            lx = snapped.x;
            ly = snapped.y;
        }
        if (!st->draftObj.inkPoints.empty()) {
            const auto& prev = st->draftObj.inkPoints.back();
            if (std::hypot(lx - prev.x, ly - prev.y) >= 1.1) {
                const std::uint64_t relMs = CurrentStrokeRelMs(st);
                st->draftObj.inkEndTimeMs = relMs;
                st->draftObj.inkPoints.push_back({ lx, ly, pressure, tiltX, tiltY, relMs });
            }
        }
        if (st->activeTool != ToolKind::Lasso && !st->rulerState.visible && st->draftObj.inkPoints.size() >= 3) {
            InkPoint pred{};
            if (InkEngine::PredictNextInkPoint(st->draftObj.inkPoints, pred, 12.0) &&
                std::hypot(pred.x - st->draftObj.inkPoints.back().x, pred.y - st->draftObj.inkPoints.back().y) >= 0.4) {
                st->draftObj.inkPoints.push_back(pred);
                st->hasPredictedTip = true;
            }
        }
    } else if (st->hasDraftCrop) {
        st->draftCropPageLocal = DocRect::FromMinMax(
            std::min(st->draftAnchorWorld.x, lx), std::min(st->draftAnchorWorld.y, ly),
            std::max(st->draftAnchorWorld.x, lx), std::max(st->draftAnchorWorld.y, ly));
    } else if (st->draftObj.shapeKind == ShapeKind::Rectangle) {
        st->draftObj.bounds = DocRect::FromMinMax(
            std::min(st->draftAnchorWorld.x, lx), std::min(st->draftAnchorWorld.y, ly),
            std::max(st->draftAnchorWorld.x, lx), std::max(st->draftAnchorWorld.y, ly));
    } else if (st->draftObj.shapeKind == ShapeKind::Arrow) {
        st->draftObj.shapeEnd = { lx, ly };
    }

    gtk_widget_queue_draw(widget);
    return TRUE;
}

gboolean on_viewport_button_release(GtkWidget* /*widget*/, GdkEventButton* ev, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !ev) return FALSE;

    if (st->panning) {
        st->panning = false;
        if (ev->button == 1 && !st->panMoved && st->activeTool == ToolKind::Hand) {
            bool isCanvas = false;
            std::size_t pgIdx = 0;
            double lx = 0.0, ly = 0.0;
            if (ComputeLocalCoordsForPoint(st, ev->x, ev->y, true, isCanvas, pgIdx, lx, ly)) {
                const double tol = std::clamp(8.0 / std::max(0.25, st->camera.zoom), 3.0, 18.0);
                const auto hits = CollectHits(st, isCanvas, pgIdx, lx, ly, tol);
                if (!hits.empty()) {
                    st->selectedObjectId = hits.back();
                    st->selectedIsCanvas = isCanvas;
                    st->selectedPageIndex = pgIdx;
                    AppActions::ToggleStudyTapeIfHit(
                        st->doc, st->cmdStack, ObjectTarget{ isCanvas, pgIdx },
                        st->selectedObjectId, st->statusBannerUtf8);
                } else {
                    st->selectedObjectId = 0;
                    st->lassoSelectedIds.clear();
                }
            } else {
                st->selectedObjectId = 0;
                st->lassoSelectedIds.clear();
            }
            InvalidateViews(st);
        }
        return TRUE;
    }

    if (st->erasing) {
        st->erasing = false;
        return TRUE;
    }

    if (!st->drawing) return FALSE;
    st->drawing = false;

    if (st->activeTool == ToolKind::Laser) {
        st->laserPointer.active = false;
        InvalidateViews(st);
        return TRUE;
    }

    if (st->hasDraftCrop && !st->draftIsCanvas && st->draftPageIdx < st->doc.Pages().size()) {
        const auto& pg = st->doc.Pages()[st->draftPageIdx];
        const double pw = std::max(1.0, pg.EffectiveWidthPt());
        const double ph = std::max(1.0, pg.EffectiveHeightPt());
        if (st->draftCropPageLocal.w >= 24.0 && st->draftCropPageLocal.h >= 24.0) {
            NormalizedCropRect nc{
                pg.crop.left + (st->draftCropPageLocal.x / pw) * (pg.crop.right - pg.crop.left),
                pg.crop.top + (st->draftCropPageLocal.y / ph) * (pg.crop.bottom - pg.crop.top),
                pg.crop.left + (st->draftCropPageLocal.Right() / pw) * (pg.crop.right - pg.crop.left),
                pg.crop.top + (st->draftCropPageLocal.Bottom() / ph) * (pg.crop.bottom - pg.crop.top)
            };
            nc.Clamp();
            st->cmdStack.Execute(st->doc, std::make_unique<CropPageCommand>(st->draftPageIdx, nc));
            st->pageCache.InvalidateAll(++st->asyncGen);
            st->statusBannerUtf8 = "Recorte aplicado a p\xC3\xA1gina " + std::to_string(st->draftPageIdx + 1);
        }
        st->hasDraftCrop = false;
    } else if (st->draftObj.kind == ObjectKind::InkStroke) {
        if (st->hasPredictedTip && st->draftObj.inkPoints.size() >= 2) {
            st->draftObj.inkPoints.pop_back();
            st->hasPredictedTip = false;
        }
        AppActions::HandleStrokeCompletedAction(
            st->doc,
            st->cmdStack,
            ObjectTarget{ st->draftIsCanvas, st->draftPageIdx },
            st->draftObj,
            (st->activeTool == ToolKind::Lasso),
            st->snapShapes,
            st->penStabilization,
            st->lassoSelectedIds,
            st->selectedObjectId,
            st->statusBannerUtf8);
        st->selectedIsCanvas = st->draftIsCanvas;
        st->selectedPageIndex = st->draftPageIdx;
    } else {
        st->draftObj.RecomputeBounds();
        if (!st->draftObj.bounds.Empty()) {
            const bool isCanvas = st->draftIsCanvas;
            const std::size_t pg = st->draftPageIdx;
            if (st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(
                    ObjectTarget{ isCanvas, pg }, std::move(st->draftObj)))) {
                const auto& vec = isCanvas ? st->doc.CanvasObjects() : st->doc.Pages()[pg].objects;
                if (!vec.empty()) {
                    st->selectedObjectId = vec.back().id;
                    st->selectedIsCanvas = isCanvas;
                    st->selectedPageIndex = pg;
                }
            }
        }
    }

    InvalidateViews(st);
    return TRUE;
}

gboolean on_viewport_scroll(GtkWidget* /*widget*/, GdkEventScroll* ev, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !ev) return FALSE;

    double dx = 0.0, dy = 0.0;
    if (ev->direction == GDK_SCROLL_SMOOTH) {
        gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent*>(ev), &dx, &dy);
    } else if (ev->direction == GDK_SCROLL_UP) {
        dy = -1.0;
    } else if (ev->direction == GDK_SCROLL_DOWN) {
        dy = 1.0;
    } else if (ev->direction == GDK_SCROLL_LEFT) {
        dx = -1.0;
    } else if (ev->direction == GDK_SCROLL_RIGHT) {
        dx = 1.0;
    }

    const bool ctrl = (ev->state & GDK_CONTROL_MASK) != 0;
    const bool shift = (ev->state & GDK_SHIFT_MASK) != 0;
    const bool zoomAction = ctrl || (st->doc.Mode() == DocumentMode::InfiniteCanvas && !shift);

    if (zoomAction) {
        if (std::abs(dy) > 1e-6) {
            const double factor = std::pow(1.12, -dy);
            st->camera.ZoomAtScreenAnchor(ev->x, ev->y, st->camera.zoom * factor);
        }
    } else {
        const double step = 64.0 / std::max(0.2, st->camera.zoom);
        if (shift && std::abs(dx) < 1e-6) {
            st->camera.panX += dy * step;
        } else {
            st->camera.panX += dx * step;
            st->camera.panY += dy * step;
        }
        RefreshActivePageFromCamera(st);
    }

    InvalidateViews(st);
    return TRUE;
}

gboolean on_sidebar_draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st) return FALSE;

    const int w = std::max(1, gtk_widget_get_allocated_width(widget));
    const int h = std::max(1, gtk_widget_get_allocated_height(widget));

    cairo_set_source_rgb(cr, 0.945, 0.961, 0.976); // #f1f5f9
    cairo_rectangle(cr, 0.0, 0.0, w, h);
    cairo_fill(cr);

    GtkAdjustment* vadj = st->sidebarScroll
        ? gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(st->sidebarScroll))
        : nullptr;
    const double scrollY = vadj ? gtk_adjustment_get_value(vadj) : 0.0;
    const double pageH = vadj ? std::max(120.0, gtk_adjustment_get_page_size(vadj)) : static_cast<double>(h);

    const std::size_t count = st->doc.Pages().size();
    const std::size_t first = static_cast<std::size_t>(std::max(0, static_cast<int>(scrollY / kThumbItemH)));
    const std::size_t last = std::min(count, first + static_cast<std::size_t>(pageH / kThumbItemH + 2));

    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* fontDesc = pango_font_description_from_string("Sans 9");
    pango_layout_set_font_description(layout, fontDesc);

    for (std::size_t i = first; i < last; ++i) {
        const double top = static_cast<double>(i) * kThumbItemH + 6.0;
        const double x0 = 8.0;
        const double cardW = std::max(60.0, static_cast<double>(w) - 16.0);
        const double cardH = kThumbItemH - 12.0;
        const bool active = (i == st->activePageIndex);

        // Rounded card
        const double r = 7.0;
        cairo_new_sub_path(cr);
        cairo_arc(cr, x0 + cardW - r, top + r, r, -G_PI / 2.0, 0.0);
        cairo_arc(cr, x0 + cardW - r, top + cardH - r, r, 0.0, G_PI / 2.0);
        cairo_arc(cr, x0 + r, top + cardH - r, r, G_PI / 2.0, G_PI);
        cairo_arc(cr, x0 + r, top + r, r, G_PI, 3.0 * G_PI / 2.0);
        cairo_close_path(cr);

        if (active) cairo_set_source_rgb(cr, 0.859, 0.918, 0.996); // #dbeafe
        else cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_fill_preserve(cr);

        if (active) cairo_set_source_rgb(cr, 0.145, 0.388, 0.922); // #2563eb
        else cairo_set_source_rgb(cr, 0.796, 0.835, 0.882);
        cairo_set_line_width(cr, active ? 2.0 : 1.0);
        cairo_stroke(cr);

        // Thumbnail frame
        const auto& pg = st->doc.Pages()[i];
        const double tx = x0 + 9.0;
        const double ty = top + 8.0;
        const double tw = 56.0;
        const double th = cardH - 16.0;

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_rectangle(cr, tx, ty, tw, th);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.58, 0.64, 0.72);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        if (pg.hasPdfSource && !pg.sourcePdfPathUtf8.empty()) {
            const PageCacheKey tkey = PageRenderCache::MakeKey(pg, 160);
            RasterImageBgra32 img;
            if (st->pageCache.TryGet(tkey, img) && img.Valid()) {
                cairo_surface_t* surf = cairo_image_surface_create_for_data(
                    img.pixels.data(), CAIRO_FORMAT_ARGB32, img.width, img.height, img.width * 4);
                const double sx = (tw - 2.0) / img.width;
                const double sy = (th - 2.0) / img.height;
                const double sc = std::min(sx, sy);
                const double ox = tx + 1.0 + ((tw - 2.0) - img.width * sc) * 0.5;
                const double oy = ty + 1.0 + ((th - 2.0) - img.height * sc) * 0.5;
                cairo_save(cr);
                cairo_translate(cr, ox, oy);
                cairo_scale(cr, sc, sc);
                cairo_set_source_surface(cr, surf, 0.0, 0.0);
                cairo_paint(cr);
                cairo_restore(cr);
                cairo_surface_destroy(surf);
            } else {
                st->pageCache.RequestAsync(tkey, st->asyncGen, st->MakeWakeupCallback());
                cairo_set_source_rgb(cr, 0.85, 0.89, 0.94);
                for (int l = 0; l < 4; ++l) {
                    cairo_rectangle(cr, tx + 7.0, ty + 12.0 + l * 14.0, tw - 14.0, 4.0);
                    cairo_fill(cr);
                }
            }
        } else {
            // Miniature paper pattern preview
            cairo_save(cr);
            cairo_rectangle(cr, tx + 1.0, ty + 1.0, tw - 2.0, th - 2.0);
            cairo_clip(cr);
            if (pg.background == PaperPattern::Grid || pg.background == PaperPattern::Millimeter) {
                cairo_set_source_rgb(cr, 0.84, 0.89, 0.95);
                cairo_set_line_width(cr, 0.6);
                for (double gx = tx + 8.0; gx < tx + tw; gx += 8.0) {
                    cairo_move_to(cr, gx, ty);
                    cairo_line_to(cr, gx, ty + th);
                }
                for (double gy = ty + 8.0; gy < ty + th; gy += 8.0) {
                    cairo_move_to(cr, tx, gy);
                    cairo_line_to(cr, tx + tw, gy);
                }
                cairo_stroke(cr);
            } else if (pg.background == PaperPattern::Ruled) {
                cairo_set_source_rgb(cr, 0.80, 0.87, 0.96);
                cairo_set_line_width(cr, 0.7);
                for (double gy = ty + 10.0; gy < ty + th; gy += 9.0) {
                    cairo_move_to(cr, tx, gy);
                    cairo_line_to(cr, tx + tw, gy);
                }
                cairo_stroke(cr);
            } else if (pg.background == PaperPattern::Dots) {
                cairo_set_source_rgb(cr, 0.72, 0.77, 0.84);
                for (double gy = ty + 8.0; gy < ty + th; gy += 8.0) {
                    for (double gx = tx + 8.0; gx < tx + tw; gx += 8.0) {
                        cairo_arc(cr, gx, gy, 0.9, 0.0, 2.0 * G_PI);
                        cairo_fill(cr);
                    }
                }
            }
            if (!pg.objects.empty()) {
                cairo_set_source_rgb(cr, 0.15, 0.39, 0.92);
                cairo_arc(cr, tx + tw - 8.0, ty + 8.0, 3.0, 0.0, 2.0 * G_PI);
                cairo_fill(cr);
            }
            cairo_restore(cr);
        }

        // Labels
        std::ostringstream lbl;
        lbl << "P\xC3\xA1g. " << (i + 1) << "\n"
            << static_cast<int>(pg.EffectiveWidthPt()) << "\xC3\x97"
            << static_cast<int>(pg.EffectiveHeightPt()) << " pt";
        if (pg.rotationDeg != 0) lbl << " (" << pg.rotationDeg << "\xC2\xB0)";

        cairo_set_source_rgb(cr, 0.06, 0.09, 0.16);
        pango_layout_set_text(layout, lbl.str().c_str(), -1);
        cairo_move_to(cr, x0 + 73.0, top + 12.0);
        pango_cairo_show_layout(cr, layout);

        // Mini ▲ ▼ reorder hints
        cairo_set_source_rgb(cr, 0.28, 0.34, 0.42);
        pango_layout_set_text(layout, "\xE2\x96\xB2  \xE2\x96\xBC", -1);
        cairo_move_to(cr, x0 + cardW - 44.0, top + cardH - 22.0);
        pango_cairo_show_layout(cr, layout);
    }

    pango_font_description_free(fontDesc);
    g_object_unref(layout);
    return FALSE;
}

gboolean on_sidebar_button_press(GtkWidget* widget, GdkEventButton* ev, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !ev || ev->button != 1) return FALSE;

    const std::size_t idx = static_cast<std::size_t>(std::max(0, static_cast<int>(ev->y)) / kThumbItemH);
    if (idx >= st->doc.Pages().size()) return FALSE;

    const int w = std::max(1, gtk_widget_get_allocated_width(widget));
    const double localY = ev->y - static_cast<double>(idx) * kThumbItemH;
    st->activePageIndex = idx;

    // Check ▲ / ▼ mini buttons at bottom-right of card
    if (ev->x >= w - 58.0 && localY >= kThumbItemH - 30.0 && localY <= kThumbItemH - 4.0) {
        if (ev->x < w - 30.0) {
            if (st->activePageIndex > 0 &&
                st->cmdStack.Execute(st->doc, std::make_unique<ReorderPageCommand>(
                    st->activePageIndex, st->activePageIndex - 1))) {
                --st->activePageIndex;
            }
        } else {
            if (st->activePageIndex + 1 < st->doc.Pages().size() &&
                st->cmdStack.Execute(st->doc, std::make_unique<ReorderPageCommand>(
                    st->activePageIndex, st->activePageIndex + 1))) {
                ++st->activePageIndex;
            }
        }
        InvalidateViews(st);
        return TRUE;
    }

    CenterCameraOnPage(st, idx);
    InvalidateViews(st);
    return TRUE;
}

// ---------------- Keyboard Shortcuts ----------------

gboolean on_window_key_press(GtkWidget* /*widget*/, GdkEventKey* ev, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !ev) return FALSE;

    const bool ctrl = (ev->state & GDK_CONTROL_MASK) != 0;
    const bool shift = (ev->state & GDK_SHIFT_MASK) != 0;
    GtkWidget* focused = gtk_window_get_focus(GTK_WINDOW(st->window));
    const bool inTextInput = focused && (GTK_IS_ENTRY(focused) || GTK_IS_TEXT_VIEW(focused));

    if (ctrl && (ev->keyval == GDK_KEY_o || ev->keyval == GDK_KEY_O)) {
        ActionOpenFile(st);
        return TRUE;
    }
    if (ctrl && (ev->keyval == GDK_KEY_s || ev->keyval == GDK_KEY_S)) {
        ActionSaveCudoc(st, shift);
        return TRUE;
    }
    if (ctrl && (ev->keyval == GDK_KEY_p || ev->keyval == GDK_KEY_P)) {
        ActionPrintDocument(st);
        return TRUE;
    }
    if (ctrl && (ev->keyval == GDK_KEY_f || ev->keyval == GDK_KEY_F)) {
        if (st->searchEntry) {
            gtk_widget_grab_focus(st->searchEntry);
            gtk_editable_select_region(GTK_EDITABLE(st->searchEntry), 0, -1);
        }
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_F3) {
        StepSearchMatch(st, shift ? -1 : 1);
        return TRUE;
    }
    if (ctrl && !inTextInput && (ev->keyval == GDK_KEY_z || ev->keyval == GDK_KEY_Z)) {
        if (shift) st->cmdStack.Redo(st->doc);
        else st->cmdStack.Undo(st->doc);
        InvalidateViews(st);
        return TRUE;
    }
    if (ctrl && !inTextInput && (ev->keyval == GDK_KEY_y || ev->keyval == GDK_KEY_Y)) {
        st->cmdStack.Redo(st->doc);
        InvalidateViews(st);
        return TRUE;
    }
    if (ctrl && (ev->keyval == GDK_KEY_plus || ev->keyval == GDK_KEY_equal || ev->keyval == GDK_KEY_KP_Add)) {
        const double vw = GetEffectiveViewportWidth(st);
        const double vh = GetEffectiveViewportHeight(st);
        st->camera.ZoomAtScreenAnchor(vw * 0.5, vh * 0.5, st->camera.zoom * 1.18);
        InvalidateViews(st);
        return TRUE;
    }
    if (ctrl && (ev->keyval == GDK_KEY_minus || ev->keyval == GDK_KEY_KP_Subtract)) {
        const double vw = GetEffectiveViewportWidth(st);
        const double vh = GetEffectiveViewportHeight(st);
        st->camera.ZoomAtScreenAnchor(vw * 0.5, vh * 0.5, st->camera.zoom * 0.85);
        InvalidateViews(st);
        return TRUE;
    }
    if (ctrl && (ev->keyval == GDK_KEY_0 || ev->keyval == GDK_KEY_KP_0)) {
        if (st->doc.Mode() == DocumentMode::PaginatedPdf) {
            CenterCameraOnPage(st, st->activePageIndex);
        } else {
            st->camera.zoom = 1.0;
            st->camera.panX = -48.0;
            st->camera.panY = -48.0;
        }
        InvalidateViews(st);
        return TRUE;
    }

    if (inTextInput) return FALSE;

    if (ev->keyval == GDK_KEY_space) {
        st->spacePanHeld = true;
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_Escape) {
        if (st->drawing || st->hasDraftCrop) {
            st->drawing = false;
            st->hasDraftCrop = false;
        } else {
            st->selectedObjectId = 0;
            st->lassoSelectedIds.clear();
        }
        InvalidateViews(st);
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_Delete || ev->keyval == GDK_KEY_BackSpace) {
        if (!st->lassoSelectedIds.empty()) {
            if (st->cmdStack.Execute(st->doc, std::make_unique<BatchEraseObjectsCommand>(
                    ObjectTarget{ st->selectedIsCanvas, st->selectedPageIndex }, st->lassoSelectedIds))) {
                st->lassoSelectedIds.clear();
                st->selectedObjectId = 0;
                st->statusBannerUtf8 = "Selección Lazo eliminada (Ctrl+Z para deshacer)";
                InvalidateViews(st);
            }
            return TRUE;
        }
        if (st->selectedObjectId != 0) {
            if (st->cmdStack.Execute(st->doc, std::make_unique<RemoveObjectCommand>(
                    ObjectTarget{ st->selectedIsCanvas, st->selectedPageIndex }, st->selectedObjectId))) {
                st->selectedObjectId = 0;
                st->statusBannerUtf8 = "Objeto eliminado (Ctrl+Z para deshacer)";
                InvalidateViews(st);
            }
            return TRUE;
        }
    }
    if (ev->keyval == GDK_KEY_Page_Up && st->doc.Mode() == DocumentMode::PaginatedPdf) {
        if (st->activePageIndex > 0) {
            CenterCameraOnPage(st, st->activePageIndex - 1);
            InvalidateViews(st);
        }
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_Page_Down && st->doc.Mode() == DocumentMode::PaginatedPdf) {
        if (st->activePageIndex + 1 < st->doc.Pages().size()) {
            CenterCameraOnPage(st, st->activePageIndex + 1);
            InvalidateViews(st);
        }
        return TRUE;
    }
    return FALSE;
}

gboolean on_window_key_release(GtkWidget* /*widget*/, GdkEventKey* ev, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st || !ev) return FALSE;
    if (ev->keyval == GDK_KEY_space) {
        st->spacePanHeld = false;
    }
    return FALSE;
}

// ---------------- Drag & Drop ----------------

void on_drag_data_received(
    GtkWidget* /*widget*/,
    GdkDragContext* context,
    gint /*x*/,
    gint /*y*/,
    GtkSelectionData* selData,
    guint /*info*/,
    guint time,
    gpointer data)
{
    auto* st = static_cast<StudioAppState*>(data);
    bool handled = false;
    if (st && selData) {
        gchar** uris = gtk_selection_data_get_uris(selData);
        if (uris) {
            if (uris[0]) {
                char* localPath = g_filename_from_uri(uris[0], nullptr, nullptr);
                if (localPath) {
                    handled = OpenDocumentFile(st, localPath);
                    g_free(localPath);
                }
            }
            g_strfreev(uris);
        }
    }
    gtk_drag_finish(context, handled ? TRUE : FALSE, FALSE, time);
}

// ---------------- Screenshot & Teardown ----------------

void WarmPageCacheForViewportAndSidebar(StudioAppState* st) {
    if (!st || st->doc.Mode() != DocumentMode::PaginatedPdf) return;
    const int vw = GetEffectiveViewportWidth(st);
    const int vh = GetEffectiveViewportHeight(st);
    const DocRect viewWorld = st->camera.ScreenRectToWorld({ 0.0, 0.0, static_cast<double>(vw), static_cast<double>(vh) });

    std::size_t firstPg = 0, lastPg = 0;
    if (st->doc.Layout().ComputeVisibleRange(viewWorld.y, viewWorld.Bottom(), firstPg, lastPg, 1)) {
        for (std::size_t p = firstPg; p <= lastPg && p < st->doc.Pages().size(); ++p) {
            const auto& pg = st->doc.Pages()[p];
            if (!pg.hasPdfSource || pg.sourcePdfPathUtf8.empty()) continue;
            const auto* pl = st->doc.Layout().PlacementAt(p);
            const double screenW = pl ? (pl->widthWorld * st->camera.zoom) : 600.0;
            const int targetW = static_cast<int>(std::lround(std::max(80.0, screenW)));
            const PageCacheKey key = PageRenderCache::MakeKey(pg, targetW);
            RasterImageBgra32 img;
            if (!st->pageCache.TryGet(key, img)) {
                NormalizedCropRect c = pg.crop;
                c.Clamp();
                if (WinRtPdfRenderer::RenderPageToBgra32(
                        Utf8ToWide(pg.sourcePdfPathUtf8), pg.sourcePageIndex0,
                        key.widthBucketPx, key.rotationDeg, c, img, nullptr, nullptr)) {
                    st->pageCache.PutDirect(key, std::move(img));
                }
            }
        }
    }

    auto ensureThumb = [&](std::size_t p) {
        if (p >= st->doc.Pages().size()) return;
        const auto& pg = st->doc.Pages()[p];
        if (!pg.hasPdfSource || pg.sourcePdfPathUtf8.empty()) return;
        const PageCacheKey tkey = PageRenderCache::MakeKey(pg, 160);
        RasterImageBgra32 img;
        if (st->pageCache.TryGet(tkey, img)) return;
        NormalizedCropRect c = pg.crop;
        c.Clamp();
        if (WinRtPdfRenderer::RenderPageToBgra32(
                Utf8ToWide(pg.sourcePdfPathUtf8), pg.sourcePageIndex0,
                tkey.widthBucketPx, tkey.rotationDeg, c, img, nullptr, nullptr)) {
            st->pageCache.PutDirect(tkey, std::move(img));
        }
    };
    const std::size_t topCount = std::min<std::size_t>(st->doc.Pages().size(), 7u);
    for (std::size_t p = 0; p < topCount; ++p) ensureThumb(p);
    const std::size_t firstThumb = st->activePageIndex > 2 ? st->activePageIndex - 2 : 0;
    const std::size_t endThumb = std::min(st->doc.Pages().size(), firstThumb + 6);
    for (std::size_t p = firstThumb; p < endThumb; ++p) ensureThumb(p);
}

bool CaptureStudioScreenshotToPng(StudioAppState* st, const std::string& outPngUtf8) {
    if (!st || outPngUtf8.empty()) return false;

    try {
        const auto parent = std::filesystem::u8path(outPngUtf8).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
    } catch (...) {}

    WarmPageCacheForViewportAndSidebar(st);

    if (!st->window || st->headlessNoDisplay) {
        // Pure headless fallback (no X11/Wayland display available)
        cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kDefaultWinW, kDefaultWinH);
        cairo_t* cr = cairo_create(surf);

        cairo_set_source_rgb(cr, 0.973, 0.980, 0.988);
        cairo_paint(cr);

        const int sideW = (st->doc.Mode() == DocumentMode::PaginatedPdf) ? kSidebarWidth : 0;
        const int topH = 74;
        const int botH = 26;
        const int vpW = std::max(1, kDefaultWinW - sideW);
        const int vpH = std::max(1, kDefaultWinH - topH - botH);

        // Header bar
        cairo_set_source_rgb(cr, 0.945, 0.961, 0.976);
        cairo_rectangle(cr, 0.0, 0.0, kDefaultWinW, topH);
        cairo_fill(cr);

        // Sidebar
        if (sideW > 0) {
            cairo_set_source_rgb(cr, 0.945, 0.961, 0.976);
            cairo_rectangle(cr, 0.0, topH, sideW, vpH);
            cairo_fill(cr);
        }

        // Viewport via D2DRenderer::RenderToCairo
        cairo_save(cr);
        cairo_translate(cr, sideW, topH);
        cairo_rectangle(cr, 0.0, 0.0, vpW, vpH);
        cairo_clip(cr);
        ViewportOverlayState ov{};
        PopulateViewportOverlay(st, ov);
        st->renderer.RenderToCairo(cr, st->doc, st->camera, vpW, vpH,
            &st->pageCache, st->asyncGen, nullptr, &ov);
        cairo_restore(cr);

        const cairo_status_t wst = cairo_surface_write_to_png(surf, outPngUtf8.c_str());
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
        return wst == CAIRO_STATUS_SUCCESS;
    }

    InvalidateViews(st);
    for (int i = 0; i < 40 && gtk_events_pending(); ++i) {
        gtk_main_iteration_do(FALSE);
    }

    gtk_container_check_resize(GTK_CONTAINER(st->window));
    for (int i = 0; i < 20 && gtk_events_pending(); ++i) {
        gtk_main_iteration_do(FALSE);
    }

    int mw = gtk_widget_get_allocated_width(st->window);
    int mh = gtk_widget_get_allocated_height(st->window);
    if (mw < 100 || mh < 100) {
        int minW = 0, natW = 0, minH = 0, natH = 0;
        gtk_widget_get_preferred_width(st->window, &minW, &natW);
        mw = std::max(kDefaultWinW, std::max(minW, natW));
        gtk_widget_get_preferred_height_for_width(st->window, mw, &minH, &natH);
        mh = std::max(kDefaultWinH, std::max(minH, natH));
        GtkAllocation alloc{ 0, 0, mw, mh };
        gtk_widget_size_allocate(st->window, &alloc);
        gtk_container_check_resize(GTK_CONTAINER(st->window));
    }

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, mw, mh);
    cairo_t* cr = cairo_create(surf);
    cairo_set_source_rgb(cr, 0.973, 0.980, 0.988);
    cairo_rectangle(cr, 0.0, 0.0, mw, mh);
    cairo_fill(cr);

    st->viewportDrawnFlag = false;
    gtk_widget_draw(st->window, cr);

    if (!st->viewportDrawnFlag && st->viewport && gtk_widget_get_visible(st->viewport)) {
        gint vx = 0, vy = 0;
        if (!gtk_widget_translate_coordinates(st->viewport, st->window, 0, 0, &vx, &vy)) {
            vx = (st->doc.Mode() == DocumentMode::PaginatedPdf) ? kSidebarWidth : 0;
            vy = 74;
        }
        const int aw = gtk_widget_get_allocated_width(st->viewport);
        const int ah = gtk_widget_get_allocated_height(st->viewport);
        const int vw = (aw > 10) ? aw : std::max(1, mw - vx);
        const int vh = (ah > 10) ? ah : std::max(1, mh - vy - 26);
        cairo_save(cr);
        cairo_translate(cr, vx, vy);
        cairo_rectangle(cr, 0.0, 0.0, vw, vh);
        cairo_clip(cr);
        ViewportOverlayState ov{};
        PopulateViewportOverlay(st, ov);
        st->renderer.RenderToCairo(cr, st->doc, st->camera, vw, vh,
            &st->pageCache, st->asyncGen, nullptr, &ov);
        cairo_restore(cr);
    }

    const cairo_status_t wst = cairo_surface_write_to_png(surf, outPngUtf8.c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return wst == CAIRO_STATUS_SUCCESS;
}

void PerformTeardown(StudioAppState* st) {
    if (!st || st->teardownDone) return;
    st->teardownDone = true;
    if (st->replayTimerId != 0) {
        g_source_remove(st->replayTimerId);
        st->replayTimerId = 0;
    }
    {
        std::lock_guard<std::mutex> lk(st->wakeupBridge->mtx);
        st->wakeupBridge->app = nullptr;
    }
    st->pageCache.Shutdown();
    WinRtPdfRenderer::ClearOpenDocumentCache();
    st->renderer.DiscardDeviceResources();
    st->window = nullptr;
}

void on_window_destroy(GtkWidget* /*widget*/, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    PerformTeardown(st);
}

// ---------------- UI Construction ----------------

void ApplyStudioCss() {
    static constexpr const char* kCss = R"CSS(
window.studio-root {
    background-color: #f8fafc;
}
.studio-ribbon {
    background-color: #f8fafc;
    border-bottom: 1px solid #cbd5e1;
    padding: 4px 8px;
}
.ribbon-row {
    margin: 1px 0px;
}
.ribbon-btn {
    min-height: 25px;
    padding: 2px 5px;
    border-radius: 5px;
    border: 1px solid #cbd5e1;
    background-image: none;
    background-color: #ffffff;
    color: #0f172a;
    font-size: 12px;
}
.ribbon-btn:hover {
    background-color: #f1f5f9;
    border-color: #94a3b8;
}
.ribbon-btn:checked {
    background-color: #dbeafe;
    border-color: #2563eb;
    color: #1e40af;
    font-weight: 600;
}
.accent-btn {
    background-image: none;
    background-color: #2563eb;
    border-color: #1d4ed8;
    color: #ffffff;
    font-weight: 600;
}
.accent-btn:hover {
    background-color: #1d4ed8;
}
.swatch-btn {
    min-width: 22px;
    min-height: 22px;
    padding: 1px;
    border: none;
    background: transparent;
}
.sidebar-panel {
    background-color: #f1f5f9;
    border-right: 1px solid #cbd5e1;
}
.sidebar-header {
    background-color: #e2e8f0;
    border-bottom: 1px solid #cbd5e1;
    padding: 4px 8px;
    font-weight: 600;
    color: #1e293b;
}
.search-results-pane {
    background-color: #ffffff;
    border-top: 1px solid #cbd5e1;
}
.studio-statusbar {
    background-color: #f1f5f9;
    border-top: 1px solid #cbd5e1;
    padding: 3px 10px;
    color: #334155;
    font-size: 12px;
}
)CSS";

    GtkCssProvider* prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(prov, kCss, -1, nullptr);
    if (GdkScreen* scr = gdk_screen_get_default()) {
        gtk_style_context_add_provider_for_screen(
            scr, GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(prov);
}

GtkWidget* MakeRibbonButton(const char* label, const char* tooltip, bool accent = false) {
    GtkWidget* btn = gtk_button_new_with_label(label);
    GtkStyleContext* ctx = gtk_widget_get_style_context(btn);
    gtk_style_context_add_class(ctx, "ribbon-btn");
    if (accent) gtk_style_context_add_class(ctx, "accent-btn");
    if (tooltip) gtk_widget_set_tooltip_text(btn, tooltip);
    gtk_widget_set_can_focus(btn, FALSE);
    return btn;
}

GtkWidget* MakeRibbonToggle(const char* label, const char* tooltip) {
    GtkWidget* btn = gtk_toggle_button_new_with_label(label);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "ribbon-btn");
    if (tooltip) gtk_widget_set_tooltip_text(btn, tooltip);
    gtk_widget_set_can_focus(btn, FALSE);
    return btn;
}

GtkWidget* BuildInteropMenu(StudioAppState* st) {
    GtkWidget* menu = gtk_menu_new();
    auto addItem = [&](const char* label, GCallback cb, gpointer udata) {
        GtkWidget* mi = gtk_menu_item_new_with_label(label);
        g_signal_connect(mi, "activate", cb, udata);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    };
    auto addSep = [&]() {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    };

    addItem("Exportar a Word (.docx)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropExport(static_cast<StudioAppState*>(d), "docx");
    }), st);
    addItem("Exportar a PowerPoint (.pptx)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropExport(static_cast<StudioAppState*>(d), "pptx");
    }), st);
    addItem("Exportar a Excel (.xlsx)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropExport(static_cast<StudioAppState*>(d), "xlsx");
    }), st);
    addItem("Exportar a CSV (.csv)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropExport(static_cast<StudioAppState*>(d), "csv");
    }), st);

    addSep();

    addItem("Importar Word (.docx)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropImport(static_cast<StudioAppState*>(d), "Word (*.docx)", "*.docx");
    }), st);
    addItem("Importar PowerPoint (.pptx)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropImport(static_cast<StudioAppState*>(d), "PowerPoint (*.pptx)", "*.pptx");
    }), st);
    addItem("Importar Excel (.xlsx)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropImport(static_cast<StudioAppState*>(d), "Excel (*.xlsx)", "*.xlsx");
    }), st);
    addItem("Importar CSV (.csv)...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionInteropImport(static_cast<StudioAppState*>(d), "CSV (*.csv)", "*.csv");
    }), st);

    addSep();

    addItem("Importar p\xC3\xA1gina PDF a Lienzo \xE2\x88\x9E...", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
        ActionImportPdfToCanvas(static_cast<StudioAppState*>(d));
    }), st);

    gtk_widget_show_all(menu);
    return menu;
}

GtkWidget* BuildStemMenu(StudioAppState* st) {
    GtkWidget* menu = gtk_menu_new();
    using MenuItemCb = void (*)(GtkMenuItem*, gpointer);
    auto addItem = [&](const char* label, MenuItemCb cb, gpointer udata) {
        GtkWidget* mi = gtk_menu_item_new_with_label(label);
        g_signal_connect(mi, "activate", reinterpret_cast<GCallback>(cb), udata);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    };
    auto addSep = [&]() {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    };

    addItem("Insertar Fórmula #varsub / Unidades...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string expr = "V_out = 4.7 kOhm * 2 mA | V";
        if (!ShowSingleInputPrompt(GTK_WINDOW(s->window), "Fórmula #varsub / Unidades",
                "Expresión (soporta unidades SI/imp, '|', fasores r∠θ°):", expr) || expr.empty()) return;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddFormulaNodeAction(s->doc, s->cmdStack, tgt, x, y, expr, "", s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Verificar Paso Algebraico...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string prev = "(x + 2)^2";
        std::string next = "x^2 + 4*x + 4";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Verificar Paso Algebraico",
                "Paso anterior:", prev, "Paso actual:", next)) return;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddFormulaNodeAction(s->doc, s->cmdStack, tgt, x, y, next, prev, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addSep();

    addItem("Resolver Circuito MNA (DC / AC)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string net = "V1 1 0 12; R1 1 2 1000; R2 2 0 2000";
        std::string freq = "0";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Resolver Circuito MNA",
                "Netlist (separar líneas con ';', R C L V I OPAMP):", net, "Frecuencia Hz (0 = DC):", freq)) return;
        for (char& ch : net) { if (ch == ';') ch = '\n'; }
        double freqHz = 0.0;
        try { freqHz = std::stod(freq); } catch (...) {}
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddCircuitNodeAction(s->doc, s->cmdStack, tgt, x, y, net, freqHz, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Simplificar Mapa de Karnaugh...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string vars = "4";
        std::string mins = "0,1,4,5,12,13";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Mapa de Karnaugh (Quine-McCluskey)",
                "Número de variables (2..4):", vars, "Minitérminos (ej. 0,1,4,5):", mins)) return;
        int nv = 4;
        try { nv = std::clamp(std::stoi(vars), 2, 4); } catch (...) {}
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddKMapNodeAction(s->doc, s->cmdStack, tgt, x, y, nv, mins, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Gráfico 2D / Regresión CSV...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string expr = "sin(x) + 0.25*x";
        std::string rng = "-6.28, 6.28";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Gráfico 2D / Regresión CSV",
                "Expresión f(x) o puntos x,y separados por ';':", expr, "Rango xMin, xMax:", rng)) return;
        for (char& ch : expr) { if (ch == ';') ch = '\n'; }
        double xMin = -6.28, xMax = 6.28;
        if (const auto comma = rng.find(','); comma != std::string::npos) {
            try { xMin = std::stod(rng.substr(0, comma)); xMax = std::stod(rng.substr(comma + 1)); } catch (...) {}
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddPlotNodeAction(s->doc, s->cmdStack, tgt, x, y, expr, xMin, xMax, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Insertar Tarjeta CASIO Interactiva...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string model = "fx-991CW";
        std::string raw = "4.7 kOhm * 2 mA | V";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Tarjeta CASIO Interactiva",
                "Modelo CASIO:", model, "Teclas|Expresión o Cálculo:", raw)) return;
        CalcBridge::CalcStepRecord rec{};
        rec.modelIdUtf8 = model;
        if (const auto bar = raw.find('|'); bar != std::string::npos && raw.find('=') == std::string::npos && raw.find(" | ") == std::string::npos) {
            rec.keySeqUtf8 = raw.substr(0, bar);
            rec.expressionUtf8 = raw.substr(bar + 1);
        } else {
            rec.keySeqUtf8 = "[CALC]";
            rec.expressionUtf8 = raw;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddCalcCardAction(s->doc, s->cmdStack, tgt, x, y, rec, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addSep();

    addItem("Exportar Circuito (SVG / DXF / KiCad)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const ObjectTarget tgt = CurrentTarget(s);
        const AnnotationObject* cObj = nullptr;
        if (s->selectedObjectId != 0) {
            cObj = tgt.isCanvas ? s->doc.FindCanvasObject(s->selectedObjectId)
                : (tgt.pageIndex < s->doc.Pages().size() ? s->doc.FindPageObjectMutable(tgt.pageIndex, s->selectedObjectId) : nullptr);
            if (cObj && cObj->kind != ObjectKind::CircuitBlock) cObj = nullptr;
        }
        if (!cObj && (tgt.isCanvas || tgt.pageIndex < s->doc.Pages().size())) {
            const auto& vec = tgt.isCanvas ? s->doc.CanvasObjects() : s->doc.Pages()[tgt.pageIndex].objects;
            for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                if (it->kind == ObjectKind::CircuitBlock) { cObj = &(*it); break; }
            }
        }
        if (!cObj) {
            s->statusBannerUtf8 = "Inserte o seleccione primero un Circuito MNA.";
            InvalidateViews(s);
            return;
        }
        std::vector<Circuit::CircuitComponent> comps;
        std::string parseErr;
        if (!Circuit::ParseNetlistText(cObj->circuitNetlistUtf8, comps, &parseErr)) {
            s->statusBannerUtf8 = parseErr;
            InvalidateViews(s);
            return;
        }
        const auto sol = Circuit::SolveMnaCircuit(comps, 0.0);
        const std::string outPath = RunFileSaveDialog(
            GTK_WINDOW(s->window), "Exportar Circuito CAD", "circuito.svg",
            "Esquema SVG / DXF / KiCad (*.svg, *.dxf, *.net)", "*", "");
        if (outPath.empty()) return;
        const std::string ext = GetFileExtLowerUtf8(outPath);
        std::string payload;
        if (ext == ".dxf") payload = Circuit::ExportCircuitToDxf(comps, sol.ok ? &sol : nullptr);
        else if (ext == ".net" || ext == ".cir") payload = Circuit::ExportCircuitToKiCadNetlist(comps, s->doc.Title());
        else payload = Circuit::ExportCircuitToSvg(comps, sol.ok ? &sol : nullptr);
        std::ofstream ofs(std::filesystem::u8path(outPath), std::ios::binary | std::ios::trunc);
        if (ofs) {
            ofs.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            s->statusBannerUtf8 = "Circuito exportado (" + std::to_string(payload.size()) + " bytes)";
        }
        InvalidateViews(s);
    }, st);

    addSep();

    addItem("Alternar Modo Calcpad (#varsub / #nosub / #novar)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        AppActions::CycleSelectedMathSubModeAction(
            s->doc, s->cmdStack, CurrentTarget(s), s->selectedObjectId, s->statusBannerUtf8);
        InvalidateViews(s);
    }, st);

    addItem("Slider de Parámetro Reactivo...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string varN = "k";
        std::string vals = "2.0, 0.2, 5.0";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Slider de Parámetro Reactivo",
                "Variable (ej. k, R_load, F):", varN, "ValorActual, Mín, Máx [Unidad]:", vals) || varN.empty()) return;
        double curV = 2.0, minV = 0.2, maxV = 5.0;
        std::string unitSuf;
        for (char& ch : vals) { if (ch == ',') ch = ' '; }
        std::istringstream iss(vals);
        iss >> curV >> minV >> maxV >> unitSuf;
        AppActions::UpdateParameterSliderAction(
            s->doc, s->cmdStack, CurrentTarget(s), varN, curV, minV, maxV, unitSuf, s->statusBannerUtf8);
        InvalidateViews(s);
    }, st);

    addItem("Matriz / Sistema de Ecuaciones Lineales (SEL)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string inExpr = "2*x + y = 7; x - y = 2";
        if (!ShowSingleInputPrompt(GTK_WINDOW(s->window), "Matriz o Sistema Lineal (SEL)",
                "Ej: 2*x + y = 7; x - y = 2  ó  det([[2,1],[1,3]]):", inExpr) || inExpr.empty()) return;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddMatrixOrLinearSystemAction(
            s->doc, s->cmdStack, tgt, x, y, inExpr, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Superficie 3D z = f(x,y) [Paleta CVD Accesible]...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string expr = "sin(x^2 + y^2)";
        std::string dom = "-2.5, 2.5";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Superficie 3D z = f(x, y) [CVD]",
                "Expresión z = f(x, y):", expr, "Dominio min, max (ej. -2.5, 2.5):", dom) || expr.empty()) return;
        double dMin = -2.5, dMax = 2.5;
        if (const auto comma = dom.find(','); comma != std::string::npos) {
            try { dMin = std::stod(dom.substr(0, comma)); dMax = std::stod(dom.substr(comma + 1)); } catch (...) {}
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddSurface3DPlotNodeAction(
            s->doc, s->cmdStack, tgt, x, y, expr, dMin, dMax, dMin, dMax, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Simulación Transitoria Osciloscopio + Laplace (s)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string net = "V1 1 0 10; D1 1 2; R1 2 0 1k; C1 2 0 2.2u";
        std::string cfg = "35, 60, 2";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Osciloscopio MNA Transitorio + Laplace (s)",
                "Netlist (R, C, L, V, I, OPAMP, D):", net, "Duración_ms, Frecuencia_Hz, Nodo_sonda:", cfg) || net.empty()) return;
        for (char& ch : net) { if (ch == ';') ch = '\n'; }
        double durMs = 35.0, fHz = 60.0;
        int probeN = 2;
        for (char& ch : cfg) { if (ch == ',') ch = ' '; }
        std::istringstream iss(cfg);
        iss >> durMs >> fHz >> probeN;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddTransientLaplaceCircuitAction(
            s->doc, s->cmdStack, tgt, x, y, net, durMs, fHz, probeN, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("OCR -> Fórmula con Tarjeta de Confirmación...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string cand = "P_tot = 3 bar + 100 psi";
        if (s->doc.Mode() == DocumentMode::PaginatedPdf && s->activePageIndex < s->doc.Pages().size()) {
            const auto& ocr = s->doc.Pages()[s->activePageIndex].ocrTextUtf8;
            if (!ocr.empty()) {
                const auto nl = ocr.find('\n');
                cand = ocr.substr(0, nl == std::string::npos ? 80 : std::min<std::size_t>(80, nl));
            }
        }
        if (!ShowSingleInputPrompt(GTK_WINDOW(s->window), "Tarjeta de Confirmación OCR -> Fórmula",
                "Verifique o edite la expresión reconocida antes de ejecutar:", cand) || cand.empty()) {
            s->statusBannerUtf8 = "Conversión OCR cancelada (trazo original intacto)";
            InvalidateViews(s);
            return;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddFormulaNodeAction(
            s->doc, s->cmdStack, tgt, x, y, cand, "", s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Exportar Memoria de Cálculo / Informe Lab (.html)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const std::string outPath = RunFileSaveDialog(
            GTK_WINDOW(s->window), "Exportar Memoria de Cálculo / Informe Lab",
            "memoria_calculo_stem.html", "Informe HTML5 / Markdown (*.html, *.md)", "*", ".html");
        if (outPath.empty()) return;
        const std::string ext = GetFileExtLowerUtf8(outPath);
        const std::string payload = (ext == ".md")
            ? Bridge::ExportCalculationReportMarkdown(s->doc)
            : Bridge::ExportCalculationReportHtml(s->doc);
        std::ofstream ofs(std::filesystem::u8path(outPath), std::ios::binary | std::ios::trunc);
        if (ofs) {
            ofs.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            s->statusBannerUtf8 = "Memoria de cálculo exportada (" + std::to_string(payload.size()) + " bytes)";
        }
        InvalidateViews(s);
    }, st);

    addSep();

    addItem("Traducir Ecuación/Matriz/Fasor -> Teclas CASIO...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string expr = "2*x + y = 7; x - y = 2";
        std::string mod = "fx-991CW";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Puente Lienzo -> Teclas CASIO",
                "Ecuación, Matriz [[...]], SEL o Fasor:", expr, "Modelo (fx-991CW / fx-991LAX):", mod) || expr.empty()) return;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddCasioReverseBridgeAction(
            s->doc, s->cmdStack, tgt, x, y, expr, mod, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Diagrama Fasorial Vectorial (Rect / Polar / Exp)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string ph = "V_s = 120 @ 0 V; I_s = 40 @ -36.87 A; V_L = 96 @ 53.13 V";
        if (!ShowSingleInputPrompt(GTK_WINDOW(s->window), "Generador de Diagrama Fasorial",
                "Fasores separados por ';' (ej. V = 120 @ 0 V; I = 40 @ -36.87 A):", ph) || ph.empty()) return;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddPhasorDiagramNodeAction(
            s->doc, s->cmdStack, tgt, x, y, ph, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Diagrama de Bode Semilogarítmico |H(jw)| dB...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string net = "V1 1 0 1; R1 1 2 1k; C1 2 0 1u";
        std::string rng = "2, 10, 100000";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Diagrama de Bode Semilogarítmico",
                "Netlist AC (separado por ';'):", net, "NodoSalida, fMin_Hz, fMax_Hz:", rng) || net.empty()) return;
        for (char& ch : net) { if (ch == ';') ch = '\n'; }
        int outN = 2;
        double f0 = 10.0, f1 = 100000.0;
        for (char& ch : rng) { if (ch == ',') ch = ' '; }
        std::istringstream iss(rng);
        iss >> outN >> f0 >> f1;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddBodePlotNodeAction(
            s->doc, s->cmdStack, tgt, x, y, net, outN, f0, f1, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Inspector de Variables y Dimensiones Físicas...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        ShowScrollableTextDialog(
            GTK_WINDOW(s->window),
            "Inspector Físico de Variables y Unidades SI",
            "Magnitudes físicas, dimensiones fundamentales SI y nodo de origen:",
            AppActions::FormatVariableInspectorUtf8(s->doc));
    }, st);

    gtk_widget_show_all(menu);
    return menu;
}

GtkWidget* BuildStudyMenu(StudioAppState* st) {
    GtkWidget* menu = gtk_menu_new();
    using MenuItemCb = void (*)(GtkMenuItem*, gpointer);
    auto addItem = [&](const char* label, MenuItemCb cb, gpointer udata) {
        GtkWidget* mi = gtk_menu_item_new_with_label(label);
        g_signal_connect(mi, "activate", reinterpret_cast<GCallback>(cb), udata);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    };
    auto addSep = [&]() {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    };

    addItem("Capas: Nueva capa...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string name = "Anotaciones Docente";
        if (!ShowSingleInputPrompt(GTK_WINDOW(s->window), "Nueva Capa", "Nombre de la capa:", name) || name.empty()) return;
        const std::uint64_t lid = s->doc.AddLayer(name);
        s->doc.SetActiveLayerId(lid);
        s->statusBannerUtf8 = "Capa activa: " + name;
        InvalidateViews(s);
    }, st);

    addItem("Capas: Cambiar capa activa", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const auto& layers = s->doc.Layers();
        if (layers.empty()) return;
        std::size_t idx = 0;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].id == s->doc.ActiveLayerId()) { idx = (i + 1) % layers.size(); break; }
        }
        s->doc.SetActiveLayerId(layers[idx].id);
        s->statusBannerUtf8 = "Capa activa: " + layers[idx].nameUtf8 +
            (layers[idx].visible ? " [Vis]" : " [Oculta]") + (layers[idx].locked ? " [Bloq]" : "");
        InvalidateViews(s);
    }, st);

    addItem("Capas: Mostrar / Ocultar capa activa", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const LayerModel* cur = s->doc.FindLayer(s->doc.ActiveLayerId());
        if (!cur) return;
        const bool nextVis = !cur->visible;
        s->doc.SetLayerProperties(cur->id, nextVis, cur->locked, cur->includeInExport, cur->opacity);
        s->statusBannerUtf8 = "Capa '" + cur->nameUtf8 + "' ahora " + (nextVis ? "visible" : "oculta");
        InvalidateViews(s);
    }, st);

    addItem("Capas: Bloquear / Desbloquear capa activa", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const LayerModel* cur = s->doc.FindLayer(s->doc.ActiveLayerId());
        if (!cur) return;
        const bool nextLock = !cur->locked;
        s->doc.SetLayerProperties(cur->id, cur->visible, nextLock, cur->includeInExport, cur->opacity);
        s->statusBannerUtf8 = "Capa '" + cur->nameUtf8 + "' ahora " + (nextLock ? "bloqueada" : "desbloqueada");
        InvalidateViews(s);
    }, st);

    addSep();

    addItem("Insertar Study Tape (Ocultar/Revelar)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        std::string cap = "Fórmula Impedancia RLC";
        std::string ans = "Z = R + j*(w*L - 1/(w*C))";
        if (!ShowTwoFieldPrompt(GTK_WINDOW(s->window), "Insertar Study Tape",
                "Etiqueta visible:", cap, "Contenido oculto:", ans)) return;
        double x = 80.0, y = 80.0;
        NextCardPlacement(s, x, y);
        const ObjectTarget tgt = CurrentTarget(s);
        const std::uint64_t newObjId = AppActions::AddStudyTapeAction(s->doc, s->cmdStack, tgt, x, y, cap, ans, s->statusBannerUtf8);
        if (newObjId != 0) { s->selectedObjectId = newObjId; s->selectedIsCanvas = tgt.isCanvas; s->selectedPageIndex = tgt.pageIndex; }
        InvalidateViews(s);
    }, st);

    addItem("Anclar tinta seleccionada a objeto", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const ObjectTarget tgt = CurrentTarget(s);
        if (!tgt.isCanvas && tgt.pageIndex >= s->doc.Pages().size()) return;
        const auto& vec = tgt.isCanvas ? s->doc.CanvasObjects() : s->doc.Pages()[tgt.pageIndex].objects;
        std::uint64_t parentId = 0;
        for (const auto& o : vec) {
            if (o.kind != ObjectKind::InkStroke && o.id != s->selectedObjectId) parentId = o.id;
        }
        if (parentId == 0) {
            s->statusBannerUtf8 = "Se requiere un objeto destino para anclar tinta";
            InvalidateViews(s);
            return;
        }
        int anchoredCount = 0;
        std::vector<std::uint64_t> ids = s->lassoSelectedIds;
        if (ids.empty() && s->selectedObjectId != 0) ids.push_back(s->selectedObjectId);
        for (std::uint64_t oid : ids) {
            if (oid == parentId) continue;
            AnnotationObject* obj = tgt.isCanvas ? s->doc.FindCanvasObjectMutable(oid) : s->doc.FindPageObjectMutable(tgt.pageIndex, oid);
            if (!obj) continue;
            AnnotationObject before = *obj;
            AnnotationObject after = before;
            after.anchorObjectId = parentId;
            if (s->cmdStack.Execute(s->doc, std::make_unique<ModifyObjectCommand>(tgt, std::move(before), std::move(after)))) {
                ++anchoredCount;
            }
        }
        s->statusBannerUtf8 = anchoredCount > 0
            ? (std::to_string(anchoredCount) + " trazo(s) anclado(s) al objeto #" + std::to_string(parentId))
            : "Seleccione o encierre con Lazo un trazo para anclar";
        InvalidateViews(s);
    }, st);

    addItem("Vincular página PDF ↔ Lienzo (InkLink)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        InkLink link{};
        link.sourceDocPathUtf8 = s->activePdfSourcePathUtf8;
        link.sourcePageIndex0 = static_cast<int>(s->activePageIndex);
        link.sourceRectNorm = { 0.1, 0.1, 0.9, 0.4 };
        link.targetIsCanvas = true;
        link.targetPageIndex0 = 0;
        link.targetObjectId = s->selectedObjectId;
        link.labelUtf8 = "PDF Pág " + std::to_string(s->activePageIndex + 1) + " <-> Lienzo";
        const std::uint64_t lid = s->doc.AddInkLink(std::move(link));
        s->statusBannerUtf8 = "InkLink #" + std::to_string(lid) + " creado (" + std::to_string(s->doc.InkLinks().size()) + " enlaces)";
        InvalidateViews(s);
    }, st);

    addItem("Colapsar / Expandir rango PDF actual", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->doc.Pages().size() < 2) {
            s->statusBannerUtf8 = "Colapso de rango requiere al menos 2 páginas";
            InvalidateViews(s);
            return;
        }
        const int s0 = static_cast<int>(s->activePageIndex);
        const int e0 = std::min(static_cast<int>(s->doc.Pages().size()) - 1, s0 + 1);
        s->doc.ToggleCollapsedRange(s0, e0);
        s->statusBannerUtf8 = s->doc.IsPageCollapsed(s0)
            ? ("Rango Pág " + std::to_string(s0 + 1) + "-" + std::to_string(e0 + 1) + " colapsado")
            : ("Rango Pág " + std::to_string(s0 + 1) + "-" + std::to_string(e0 + 1) + " expandido");
        InvalidateViews(s);
    }, st);

    addSep();

    addItem("Repasar Flashcards (SM-2)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->activeDeck = StudySetManager::BuildDeckFromDocument(s->doc, s->doc.Title());
        if (s->activeDeck.cards.empty()) {
            s->statusBannerUtf8 = "No hay tarjetas StudyTape/Math/CASIO en este documento";
            ShowMessageDialog(GTK_WINDOW(s->window), GTK_MESSAGE_INFO, "Study Set SM-2",
                "Agregue una Study Tape, Fórmula #varsub o Tarjeta CASIO para generar Flashcards SM-2.");
            InvalidateViews(s);
            return;
        }
        if (s->activeDeckCursor >= s->activeDeck.cards.size()) s->activeDeckCursor = 0;
        auto& card = s->activeDeck.cards[s->activeDeckCursor];
        std::string qScore = "5";
        const std::string promptTxt =
            "Pregunta: " + card.questionUtf8 + "\nRespuesta: " + card.answerUtf8 + "\nCalificación SM-2 (0..5):";
        if (ShowSingleInputPrompt(GTK_WINDOW(s->window), "Flashcards SM-2", promptTxt.c_str(), qScore)) {
            int q = 5;
            try { q = std::clamp(std::stoi(qScore), 0, 5); } catch (...) {}
            StudySetManager::ReviewCardSm2(card, q, 1700000000ull);
            s->statusBannerUtf8 = "SM-2 tarjeta " + std::to_string(s->activeDeckCursor + 1) + "/" +
                std::to_string(s->activeDeck.cards.size()) + ": próximo repaso en " +
                std::to_string(card.intervalDays) + " día(s)";
            s->activeDeckCursor = (s->activeDeckCursor + 1) % s->activeDeck.cards.size();
        }
        InvalidateViews(s);
    }, st);

    addItem("Buscar en Índice Unicode (FTS Vault)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->ftsIndex.IndexDocument("active", s->doc);
        std::string q = "ohm";
        if (!ShowSingleInputPrompt(GTK_WINDOW(s->window), "Búsqueda Unicode Vault FTS", "Término a buscar:", q) || q.empty()) return;
        const auto hits = s->ftsIndex.Query(q, 24);
        if (hits.empty()) {
            s->statusBannerUtf8 = "FTS (" + std::to_string(s->ftsIndex.VocabularySize()) + " términos): 0 coincidencias";
        } else {
            if (hits.front().objectId != 0) s->selectedObjectId = hits.front().objectId;
            if (s->doc.Mode() == DocumentMode::PaginatedPdf && hits.front().pageIndex < s->doc.Pages().size()) {
                CenterCameraOnPage(s, hits.front().pageIndex);
            }
            s->statusBannerUtf8 = "FTS " + std::to_string(hits.size()) + " resultado(s): " + hits.front().snippetUtf8;
        }
        InvalidateViews(s);
    }, st);

    addItem("Ink Replay + Línea Temporal (Play/Stop)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->replayActive) {
            s->replayActive = false;
            if (s->replayTimerId != 0) { g_source_remove(s->replayTimerId); s->replayTimerId = 0; }
            s->audioSession.SetPlaying(false);
            s->statusBannerUtf8 = "Ink Replay detenido";
        } else {
            const auto range = InkEngine::ComputeReplayTimelineRange(s->doc, CurrentTarget(s));
            const std::uint64_t audioDur = s->audioSession.DurationMs();
            s->replayMaxMs = std::max(range.second, audioDur);
            if (s->replayMaxMs < 100) s->replayMaxMs = 1500;
            s->replayCursorMs = range.first;
            s->replayActive = true;
            s->audioSession.SeekMs(s->replayCursorMs);
            s->audioSession.SetPlaying(true);
            if (s->replayTimerId != 0) g_source_remove(s->replayTimerId);
            s->replayTimerId = g_timeout_add(45, +[](gpointer ud) -> gboolean {
                auto* stPtr = static_cast<StudioAppState*>(ud);
                if (!stPtr || stPtr->teardownDone || !stPtr->replayActive) {
                    if (stPtr) stPtr->replayTimerId = 0;
                    return G_SOURCE_REMOVE;
                }
                stPtr->replayCursorMs += 45;
                stPtr->audioSession.AdvancePlaybackMs(45);
                if (stPtr->replayCursorMs >= stPtr->replayMaxMs) {
                    stPtr->replayActive = false;
                    stPtr->replayTimerId = 0;
                    stPtr->audioSession.SetPlaying(false);
                    stPtr->statusBannerUtf8 = "Ink Replay completado";
                    InvalidateViews(stPtr);
                    return G_SOURCE_REMOVE;
                }
                stPtr->statusBannerUtf8 = "Ink Replay: " + std::to_string(stPtr->replayCursorMs) + " / " + std::to_string(stPtr->replayMaxMs) + " ms";
                InvalidateViews(stPtr);
                return G_SOURCE_CONTINUE;
            }, s);
            s->statusBannerUtf8 = "Ink Replay reproduciendo (0 / " + std::to_string(s->replayMaxMs) + " ms)";
        }
        InvalidateViews(s);
    }, st);

    addItem("Grabar / Exportar Audio WAV (16-bit PCM)...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const auto micRep = s->audioSession.CaptureFromMicrophone(1200, 16000, false);
        std::vector<std::uint8_t> wav;
        if (s->audioSession.ExportWavBytes(wav)) {
            const std::string outWav = RunFileSaveDialog(
                GTK_WINDOW(s->window), "Guardar Nota de Audio WAV (16-bit PCM)", "nota_estudio.wav",
                "Audio PCM WAV (*.wav)", "*.wav", ".wav");
            if (!outWav.empty()) {
                std::ofstream ofs(std::filesystem::u8path(outWav), std::ios::binary | std::ios::trunc);
                if (ofs) ofs.write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));
            }
            s->statusBannerUtf8 = micRep.statusNoteUtf8 + " (" + std::to_string(s->audioSession.DurationMs()) + " ms)";
        }
        InvalidateViews(s);
    }, st);

    addItem("Estabilización de Tinta (Bajo/Medio/Alto)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->penStabilization < 0.25) s->penStabilization = 0.45;
        else if (s->penStabilization < 0.70) s->penStabilization = 0.85;
        else s->penStabilization = 0.15;
        s->statusBannerUtf8 = "Estabilización de tinta: " + std::to_string(static_cast<int>(std::lround(s->penStabilization * 100.0))) + "%";
        InvalidateViews(s);
    }, st);

    addItem("Modo Presentador + Puntero Láser", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->presenterMode = !s->presenterMode;
        if (s->presenterMode) s->activeTool = ToolKind::Laser;
        s->statusBannerUtf8 = s->presenterMode
            ? "Modo Presentador + Puntero Láser ACTIVADO"
            : "Modo Presentador desactivado";
        InvalidateViews(s);
    }, st);

    addSep();

    addItem("Minimapa de Lienzo Infinito (Mostrar / Ocultar)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->minimapVisible = !s->minimapVisible;
        s->statusBannerUtf8 = s->minimapVisible ? "Minimapa activado" : "Minimapa oculto";
        InvalidateViews(s);
    }, st);

    addItem("Regla Graduada 0°-360° (Mostrar / Rotar +15° / Ocultar)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (!s->rulerState.visible) {
            const double vw = static_cast<double>(GetEffectiveViewportWidth(s));
            const double vh = static_cast<double>(GetEffectiveViewportHeight(s));
            const DocPoint cWorld = s->camera.ScreenToWorld(vw * 0.5, vh * 0.55);
            s->rulerState.centerX = cWorld.x;
            s->rulerState.centerY = cWorld.y;
            s->rulerState.angleDeg = 0.0;
            s->rulerState.visible = true;
        } else if (s->rulerState.angleDeg < 165.0) {
            s->rulerState.angleDeg += 15.0;
        } else {
            s->rulerState.visible = false;
            s->rulerState.angleDeg = 0.0;
        }
        s->statusBannerUtf8 = s->rulerState.visible
            ? ("Regla graduada activa (" + std::to_string(static_cast<int>(s->rulerState.angleDeg)) + "°)")
            : "Regla graduada oculta";
        InvalidateViews(s);
    }, st);

    addItem("Borrador Selectivo: Todo / Solo Resaltador / Solo Tinta", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->eraserFilterMode == EraserFilterMode::All) {
            s->eraserFilterMode = EraserFilterMode::HighlighterOnly;
            s->statusBannerUtf8 = "Borrador Selectivo: SOLO RESALTADOR (protege tinta y fórmulas)";
        } else if (s->eraserFilterMode == EraserFilterMode::HighlighterOnly) {
            s->eraserFilterMode = EraserFilterMode::InkOnly;
            s->statusBannerUtf8 = "Borrador Selectivo: SOLO TINTA";
        } else {
            s->eraserFilterMode = EraserFilterMode::All;
            s->statusBannerUtf8 = "Borrador Selectivo: TODO";
        }
        s->activeTool = ToolKind::Eraser;
        InvalidateViews(s);
    }, st);

    addItem("Limpiar Todos los Resaltados de la Vista", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        AppActions::ClearItemsByFilterAction(
            s->doc, s->cmdStack, CurrentTarget(s), EraserFilterMode::HighlighterOnly, s->statusBannerUtf8);
        InvalidateViews(s);
    }, st);

    addItem("Etiquetar Objeto (#examen / #duda) y Ver Tablero...", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->selectedObjectId != 0) {
            std::string tg = "examen";
            if (ShowSingleInputPrompt(GTK_WINDOW(s->window), "Etiquetar Objeto (#tag)",
                    "Etiqueta (ej. examen, importante, duda):", tg) && !tg.empty()) {
                AppActions::AssignTagToObjectAction(
                    s->doc, s->cmdStack, CurrentTarget(s), s->selectedObjectId, tg, s->statusBannerUtf8);
            }
        }
        ShowScrollableTextDialog(
            GTK_WINDOW(s->window),
            "Tablero Resumen de Etiquetas (#tags)",
            "Índice cruzado de etiquetas en el documento:",
            AppActions::FormatTagSummaryBoardUtf8(s->doc));
        InvalidateViews(s);
    }, st);

    addItem("Modo Concentración (Foco Sin Distracciones)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->focusMode = !s->focusMode;
        s->statusBannerUtf8 = s->focusMode ? "Modo Concentración activado" : "Modo Concentración desactivado";
        InvalidateViews(s);
    }, st);

    addItem("Capas: Crear Capa Privada Docente (Oculta en Proyección/PDF)", +[](GtkMenuItem*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        AppActions::AddPrivateTeacherLayerAction(s->doc, s->statusBannerUtf8);
        InvalidateViews(s);
    }, st);

    gtk_widget_show_all(menu);
    return menu;
}

void BuildStudioWindow(GtkApplication* app, StudioAppState* st) {
    st->app = app;
    ApplyStudioCss();

    st->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(st->window), "Document Studio \xE2\x80\x94 Calculadora Unificada");
    gtk_window_set_default_size(GTK_WINDOW(st->window), kDefaultWinW, kDefaultWinH);
    gtk_style_context_add_class(gtk_widget_get_style_context(st->window), "studio-root");
    if (st->smokeTestMode) {
        gtk_window_set_focus_on_map(GTK_WINDOW(st->window), FALSE);
    }

    st->rootBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(st->window), st->rootBox);

    // ---- Top Ribbon (2 crisp rows) ----
    st->ribbonBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_style_context_add_class(gtk_widget_get_style_context(st->ribbonBox), "studio-ribbon");
    gtk_box_pack_start(GTK_BOX(st->rootBox), st->ribbonBox, FALSE, FALSE, 0);

    GtkWidget* row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(row1), "ribbon-row");
    gtk_box_pack_start(GTK_BOX(st->ribbonBox), row1, FALSE, FALSE, 0);

    GtkWidget* btnOpen = MakeRibbonButton("Abrir", "Abrir archivo (Ctrl+O)");
    g_signal_connect_swapped(btnOpen, "clicked", G_CALLBACK(ActionOpenFile), st);

    GtkWidget* btnSave = MakeRibbonButton("Guardar", "Guardar proyecto .cudoc at\xC3\xB3micamente (Ctrl+S)", true);
    g_signal_connect(btnSave, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        ActionSaveCudoc(static_cast<StudioAppState*>(d), false);
    }), st);

    GtkWidget* btnExportPdf = MakeRibbonButton("Export PDF", "Exportar documento/anotaciones a PDF");
    g_signal_connect_swapped(btnExportPdf, "clicked", G_CALLBACK(ActionExportPdf), st);

    GtkWidget* btnMerge = MakeRibbonButton("Combinar", "Combinar m\xC3\xBAltiples PDFs");
    g_signal_connect_swapped(btnMerge, "clicked", G_CALLBACK(ActionMergePdfs), st);

    GtkWidget* btnSplit = MakeRibbonButton("Dividir", "Extraer rango de p\xC3\xA1ginas a PDF");
    g_signal_connect_swapped(btnSplit, "clicked", G_CALLBACK(ActionSplitPdf), st);

    GtkWidget* btnSign = MakeRibbonButton("Firmar", "Firmar PDF con PKCS#7 SHA-256 (.pfx o autofirmado)");
    g_signal_connect_swapped(btnSign, "clicked", G_CALLBACK(ActionSignPdf), st);

    GtkWidget* btnVerify = MakeRibbonButton("Verificar", "Verificar firma digital PDF");
    g_signal_connect_swapped(btnVerify, "clicked", G_CALLBACK(ActionVerifyPdfSignature), st);

    GtkWidget* btnOcr = MakeRibbonButton("OCR", "Reconocimiento \xC3\xB3ptico de texto en p\xC3\xA1gina actual");
    g_signal_connect_swapped(btnOcr, "clicked", G_CALLBACK(ActionRunOcr), st);

    GtkWidget* btnInterop = gtk_menu_button_new();
    gtk_button_set_label(GTK_BUTTON(btnInterop), "Office \xE2\x96\xBE");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnInterop), "ribbon-btn");
    gtk_widget_set_tooltip_text(btnInterop, "Importar / Exportar DOCX, PPTX, XLSX, CSV");
    gtk_widget_set_can_focus(btnInterop, FALSE);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(btnInterop), BuildInteropMenu(st));

    GtkWidget* btnStem = gtk_menu_button_new();
    gtk_button_set_label(GTK_BUTTON(btnStem), "+STEM \xE2\x96\xBE");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnStem), "ribbon-btn");
    gtk_widget_set_tooltip_text(btnStem, "Fórmulas #varsub, Unidades SI, Circuitos MNA, Karnaugh, Gráficos y Tarjetas CASIO");
    gtk_widget_set_can_focus(btnStem, FALSE);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(btnStem), BuildStemMenu(st));

    GtkWidget* btnStudy = gtk_menu_button_new();
    gtk_button_set_label(GTK_BUTTON(btnStudy), "Estudio \xE2\x96\xBE");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnStudy), "ribbon-btn");
    gtk_widget_set_tooltip_text(btnStudy, "Capas, Study Tape, Flashcards SM-2, FTS Vault, Ink Replay, Audio WAV y Presentador");
    gtk_widget_set_can_focus(btnStudy, FALSE);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(btnStudy), BuildStudyMenu(st));

    GtkWidget* btnPrint = MakeRibbonButton("Imprimir", "Imprimir documento (Ctrl+P)");
    g_signal_connect_swapped(btnPrint, "clicked", G_CALLBACK(ActionPrintDocument), st);

    for (GtkWidget* w : { btnOpen, btnSave, btnExportPdf, btnMerge, btnSplit, btnSign, btnVerify, btnOcr, btnInterop, btnStem, btnStudy, btnPrint }) {
        gtk_box_pack_start(GTK_BOX(row1), w, FALSE, FALSE, 0);
    }

    // Search widgets at right side of Row 1
    GtkWidget* searchBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_end(GTK_BOX(row1), searchBox, FALSE, FALSE, 0);

    st->searchEntry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(st->searchEntry), "Buscar... (Ctrl+F)");
    gtk_widget_set_size_request(st->searchEntry, 140, -1);
    g_signal_connect(st->searchEntry, "search-changed", G_CALLBACK(+[](GtkSearchEntry* e, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        PerformLiveSearch(s, gtk_entry_get_text(GTK_ENTRY(e)), false);
    }), st);
    g_signal_connect(st->searchEntry, "activate", G_CALLBACK(+[](GtkEntry*, gpointer d) {
        StepSearchMatch(static_cast<StudioAppState*>(d), 1);
    }), st);

    GtkWidget* btnFindPrev = MakeRibbonButton("\xE2\x97\x80", "Coincidencia anterior (Shift+F3)");
    g_signal_connect(btnFindPrev, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        StepSearchMatch(static_cast<StudioAppState*>(d), -1);
    }), st);

    GtkWidget* btnFindNext = MakeRibbonButton("Ir", "Siguiente coincidencia (F3)");
    g_signal_connect(btnFindNext, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        StepSearchMatch(static_cast<StudioAppState*>(d), 1);
    }), st);

    GtkWidget* btnReplace = MakeRibbonButton("Reem.", "Reemplazar / redactar texto");
    g_signal_connect_swapped(btnReplace, "clicked", G_CALLBACK(ActionReplaceText), st);

    gtk_box_pack_start(GTK_BOX(searchBox), st->searchEntry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(searchBox), btnFindPrev, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(searchBox), btnFindNext, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(searchBox), btnReplace, FALSE, FALSE, 0);

    // ---- Row 2: Modes, Tools, Swatches, Paper, Page Ops, Undo/Redo, Zoom ----
    GtkWidget* row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_style_context_add_class(gtk_widget_get_style_context(row2), "ribbon-row");
    gtk_box_pack_start(GTK_BOX(st->ribbonBox), row2, FALSE, FALSE, 0);

    st->btnModePdf = MakeRibbonToggle("PDF P\xC3\xA1g.", "Modo Documento PDF Paginado");
    st->btnModeCanvas = MakeRibbonToggle("Lienzo \xE2\x88\x9E", "Modo Lienzo Infinito");
    g_signal_connect(st->btnModePdf, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->syncingControls || !gtk_toggle_button_get_active(b)) { SyncToolbarControls(s); return; }
        s->doc.SetMode(DocumentMode::PaginatedPdf);
        s->doc.RebuildLayout();
        CenterCameraOnPage(s, s->activePageIndex);
        InvalidateViews(s);
    }), st);
    g_signal_connect(st->btnModeCanvas, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->syncingControls || !gtk_toggle_button_get_active(b)) { SyncToolbarControls(s); return; }
        s->doc.SetMode(DocumentMode::InfiniteCanvas);
        s->camera.panX = -50.0;
        s->camera.panY = -50.0;
        s->camera.zoom = 1.0;
        InvalidateViews(s);
    }), st);

    auto makeToolToggle = [&](const char* lbl, const char* tip, ToolKind tk) -> GtkWidget* {
        GtkWidget* b = MakeRibbonToggle(lbl, tip);
        g_object_set_data(G_OBJECT(b), "tool-kind", GINT_TO_POINTER(static_cast<int>(tk)));
        g_signal_connect(b, "toggled", G_CALLBACK(+[](GtkToggleButton* tb, gpointer d) {
            auto* s = static_cast<StudioAppState*>(d);
            if (s->syncingControls) return;
            if (!gtk_toggle_button_get_active(tb)) { SyncToolbarControls(s); return; }
            s->activeTool = static_cast<ToolKind>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(tb), "tool-kind")));
            InvalidateViews(s);
        }), st);
        return b;
    };

    st->btnToolHand   = makeToolToggle("Mano",    "Desplazar / Clic para seleccionar o revelar Study Tape", ToolKind::Hand);
    st->btnToolPen    = makeToolToggle("Pluma",   "Pluma con presión, inclinación y Scribble-to-Erase", ToolKind::Pen);
    st->btnToolHl     = makeToolToggle("Resalt.", "Resaltador translúcido", ToolKind::Highlighter);
    st->btnToolEraser = makeToolToggle("Borrar",  "Borrador de objetos (con Deshacer)", ToolKind::Eraser);
    st->btnToolLasso  = makeToolToggle("Lazo",    "Selección multi-objeto con lazo cerrado", ToolKind::Lasso);
    st->btnToolLaser  = makeToolToggle("Láser",   "Puntero láser para presentaciones", ToolKind::Laser);
    st->btnToolRect   = makeToolToggle("Rect",    "Rectángulo", ToolKind::ShapeRect);
    st->btnToolArrow  = makeToolToggle("Flecha",  "Flecha direccional", ToolKind::ShapeArrow);
    st->btnToolText   = makeToolToggle("Texto",   "Insertar cuadro de texto UTF-8", ToolKind::TextBox);
    st->btnToolCrop   = makeToolToggle("Recorte", "Recortar página PDF", ToolKind::Crop);

    st->btnToggleSnap = MakeRibbonToggle("Snap", "Hold-to-Snap: ajusta líneas, flechas, rectángulos y elipses");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(st->btnToggleSnap), st->snapShapes);
    g_signal_connect(st->btnToggleSnap, "toggled", G_CALLBACK(+[](GtkToggleButton* tb, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->syncingControls) return;
        s->snapShapes = gtk_toggle_button_get_active(tb);
        s->statusBannerUtf8 = s->snapShapes ? "Hold-to-Snap activado" : "Hold-to-Snap desactivado";
        InvalidateViews(s);
    }), st);

    GtkWidget* btnImg = MakeRibbonButton("+Img", "Insertar imagen PNG/JPG");
    g_signal_connect(btnImg, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        const std::string p = RunFileOpenDialog(
            GTK_WINDOW(s->window),
            "Insertar imagen",
            { { "Im\xC3\xA1genes (*.png, *.jpg, *.bmp)", { "*.png", "*.jpg", "*.jpeg", "*.bmp" } } });
        if (!p.empty()) {
            std::string err;
            if (!OpenDocumentFile(s, p, &err)) {
                ShowMessageDialog(GTK_WINDOW(s->window), GTK_MESSAGE_ERROR, "Error imagen", err);
            }
        }
    }), st);

    for (GtkWidget* w : {
            st->btnModePdf, st->btnModeCanvas,
            gtk_separator_new(GTK_ORIENTATION_VERTICAL),
            st->btnToolHand, st->btnToolPen, st->btnToolHl, st->btnToolEraser,
            st->btnToolLasso, st->btnToolLaser,
            st->btnToolRect, st->btnToolArrow, st->btnToolText, st->btnToolCrop, btnImg, st->btnToggleSnap,
            gtk_separator_new(GTK_ORIENTATION_VERTICAL) }) {
        gtk_box_pack_start(GTK_BOX(row2), w, FALSE, FALSE, 0);
    }

    // 5 Color Swatches
    for (int i = 0; i < 5; ++i) {
        GtkWidget* btn = gtk_button_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "swatch-btn");
        gtk_widget_set_tooltip_text(btn, kSwatches[i].name);
        gtk_widget_set_can_focus(btn, FALSE);

        GtkWidget* area = gtk_drawing_area_new();
        gtk_widget_set_size_request(area, 20, 20);
        g_object_set_data(G_OBJECT(area), "swatch-idx", GINT_TO_POINTER(i));
        g_signal_connect(area, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer d) -> gboolean {
            auto* s = static_cast<StudioAppState*>(d);
            const int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "swatch-idx"));
            const int aw = gtk_widget_get_allocated_width(w);
            const int ah = gtk_widget_get_allocated_height(w);
            const double cx = aw * 0.5;
            const double cy = ah * 0.5;
            const double rad = std::min(aw, ah) * 0.38;
            const DocColor col = kSwatches[idx].color;
            const bool active = ColorsEqualRgb(s->penColor, col);

            if (active) {
                cairo_set_source_rgb(cr, 0.145, 0.388, 0.922);
                cairo_arc(cr, cx, cy, rad + 2.5, 0.0, 2.0 * G_PI);
                cairo_set_line_width(cr, 2.0);
                cairo_stroke(cr);
            }
            cairo_set_source_rgb(cr, col.r / 255.0, col.g / 255.0, col.b / 255.0);
            cairo_arc(cr, cx, cy, rad, 0.0, 2.0 * G_PI);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0.39, 0.45, 0.55);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
            return FALSE;
        }), st);

        gtk_container_add(GTK_CONTAINER(btn), area);
        g_object_set_data(G_OBJECT(btn), "swatch-idx", GINT_TO_POINTER(i));
        g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
            auto* s = static_cast<StudioAppState*>(d);
            const int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "swatch-idx"));
            s->penColor = kSwatches[idx].color;
            if (idx == 4) s->activeTool = ToolKind::Highlighter;
            InvalidateViews(s);
        }), st);

        st->swatchBtns[i] = btn;
        st->swatchAreas[i] = area;
        gtk_box_pack_start(GTK_BOX(row2), btn, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(row2), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 1);

    // Paper Pattern selector
    st->paperCombo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->paperCombo), "Papel: Blanco");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->paperCombo), "Papel: Rayado");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->paperCombo), "Papel: Cuadr\xC3\xAD" "cula");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->paperCombo), "Papel: Puntos");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->paperCombo), "Papel: Milimetrado");
    gtk_widget_set_tooltip_text(st->paperCombo, "Patr\xC3\xB3n de papel");
    gtk_widget_set_can_focus(st->paperCombo, FALSE);
    g_signal_connect(st->paperCombo, "changed", G_CALLBACK(+[](GtkComboBox* cb, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->syncingControls) return;
        const gint idx = gtk_combo_box_get_active(cb);
        if (idx < 0 || idx > 4) return;
        const PaperPattern pat = static_cast<PaperPattern>(idx);
        const bool isCanvas = (s->doc.Mode() == DocumentMode::InfiniteCanvas);
        s->cmdStack.Execute(s->doc, std::make_unique<SetBackgroundCommand>(
            ObjectTarget{ isCanvas, s->activePageIndex }, pat));
        InvalidateViews(s);
    }), st);
    gtk_box_pack_start(GTK_BOX(row2), st->paperCombo, FALSE, FALSE, 0);

    // Page ops
    GtkWidget* btnAddPg = MakeRibbonButton("+P\xC3\xA1g", "Insertar p\xC3\xA1gina nueva");
    g_signal_connect(btnAddPg, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        PageModel blank;
        blank.background = PaperPattern::Grid;
        s->cmdStack.Execute(s->doc, std::make_unique<InsertPageCommand>(s->activePageIndex + 1, std::move(blank)));
        CenterCameraOnPage(s, s->activePageIndex + 1);
        InvalidateViews(s);
    }), st);

    GtkWidget* btnRotPg = MakeRibbonButton("Rot 90\xC2\xB0", "Rotar p\xC3\xA1gina 90\xC2\xB0");
    g_signal_connect(btnRotPg, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->cmdStack.Execute(s->doc, std::make_unique<RotatePageCommand>(s->activePageIndex, 90));
        s->pageCache.InvalidateAll(++s->asyncGen);
        InvalidateViews(s);
    }), st);

    GtkWidget* btnDelPg = MakeRibbonButton("-P\xC3\xA1g", "Eliminar p\xC3\xA1gina actual");
    g_signal_connect(btnDelPg, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (!s->cmdStack.Execute(s->doc, std::make_unique<DeletePageCommand>(s->activePageIndex))) {
            s->statusBannerUtf8 = "Debe permanecer al menos 1 p\xC3\xA1gina";
        } else if (s->activePageIndex >= s->doc.Pages().size()) {
            s->activePageIndex = s->doc.Pages().size() - 1;
        }
        InvalidateViews(s);
    }), st);

    GtkWidget* btnPrevPg = MakeRibbonButton("\xE2\x97\x80", "P\xC3\xA1gina anterior (PageUp)");
    g_signal_connect(btnPrevPg, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->activePageIndex > 0) {
            CenterCameraOnPage(s, s->activePageIndex - 1);
            InvalidateViews(s);
        }
    }), st);

    st->pageSpin = gtk_spin_button_new_with_range(1.0, 1.0, 1.0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(st->pageSpin), TRUE);
    gtk_widget_set_size_request(st->pageSpin, 52, -1);
    g_signal_connect(st->pageSpin, "value-changed", G_CALLBACK(+[](GtkSpinButton* sp, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->syncingControls || s->doc.Pages().empty()) return;
        const std::size_t idx = static_cast<std::size_t>(std::clamp(gtk_spin_button_get_value_as_int(sp) - 1, 0,
            static_cast<int>(s->doc.Pages().size()) - 1));
        CenterCameraOnPage(s, idx);
        InvalidateViews(s);
    }), st);

    st->pageTotalLabel = gtk_label_new("/ 1");

    GtkWidget* btnNextPg = MakeRibbonButton("\xE2\x96\xB6", "P\xC3\xA1gina siguiente (PageDown)");
    g_signal_connect(btnNextPg, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->activePageIndex + 1 < s->doc.Pages().size()) {
            CenterCameraOnPage(s, s->activePageIndex + 1);
            InvalidateViews(s);
        }
    }), st);

    st->btnUndo = MakeRibbonButton("Desh.", "Deshacer (Ctrl+Z)");
    g_signal_connect(st->btnUndo, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->cmdStack.Undo(s->doc);
        InvalidateViews(s);
    }), st);

    st->btnRedo = MakeRibbonButton("Reh.", "Rehacer (Ctrl+Y)");
    g_signal_connect(st->btnRedo, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->cmdStack.Redo(s->doc);
        InvalidateViews(s);
    }), st);

    GtkWidget* btnZoomOut = MakeRibbonButton("-", "Alejar (Ctrl+-)");
    g_signal_connect(btnZoomOut, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->camera.ZoomAtScreenAnchor(GetEffectiveViewportWidth(s) * 0.5,
            GetEffectiveViewportHeight(s) * 0.5, s->camera.zoom * 0.85);
        InvalidateViews(s);
    }), st);

    GtkWidget* btnZoomReset = MakeRibbonButton("100%", "Ajustar p\xC3\xA1gina (Ctrl+0)");
    g_signal_connect(btnZoomReset, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->doc.Mode() == DocumentMode::PaginatedPdf) CenterCameraOnPage(s, s->activePageIndex);
        else { s->camera.zoom = 1.0; s->camera.panX = -48.0; s->camera.panY = -48.0; }
        InvalidateViews(s);
    }), st);

    GtkWidget* btnZoomIn = MakeRibbonButton("+", "Acercar (Ctrl++)");
    g_signal_connect(btnZoomIn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        s->camera.ZoomAtScreenAnchor(GetEffectiveViewportWidth(s) * 0.5,
            GetEffectiveViewportHeight(s) * 0.5, s->camera.zoom * 1.18);
        InvalidateViews(s);
    }), st);

    for (GtkWidget* w : {
            btnAddPg, btnRotPg, btnDelPg, btnPrevPg, st->pageSpin, st->pageTotalLabel, btnNextPg,
            gtk_separator_new(GTK_ORIENTATION_VERTICAL),
            st->btnUndo, st->btnRedo,
            gtk_separator_new(GTK_ORIENTATION_VERTICAL),
            btnZoomOut, btnZoomReset, btnZoomIn }) {
        gtk_box_pack_start(GTK_BOX(row2), w, FALSE, FALSE, 0);
    }

    // ---- Main Body: Left Sidebar + Central Viewport ----
    GtkWidget* bodyBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(st->rootBox), bodyBox, TRUE, TRUE, 0);

    st->sidebarBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(st->sidebarBox, kSidebarWidth, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(st->sidebarBox), "sidebar-panel");
    gtk_box_pack_start(GTK_BOX(bodyBox), st->sidebarBox, FALSE, FALSE, 0);

    GtkWidget* sideHdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(sideHdr), "sidebar-header");
    GtkWidget* sideTitle = gtk_label_new("P\xC3\xA1ginas");
    gtk_widget_set_halign(sideTitle, GTK_ALIGN_START);
    GtkWidget* btnMoveUp = MakeRibbonButton("\xE2\x96\xB2", "Mover p\xC3\xA1gina arriba");
    GtkWidget* btnMoveDown = MakeRibbonButton("\xE2\x96\xBC", "Mover p\xC3\xA1gina abajo");
    g_signal_connect(btnMoveUp, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->activePageIndex > 0 &&
            s->cmdStack.Execute(s->doc, std::make_unique<ReorderPageCommand>(
                s->activePageIndex, s->activePageIndex - 1))) {
            --s->activePageIndex;
            InvalidateViews(s);
        }
    }), st);
    g_signal_connect(btnMoveDown, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->activePageIndex + 1 < s->doc.Pages().size() &&
            s->cmdStack.Execute(s->doc, std::make_unique<ReorderPageCommand>(
                s->activePageIndex, s->activePageIndex + 1))) {
            ++s->activePageIndex;
            InvalidateViews(s);
        }
    }), st);
    gtk_box_pack_start(GTK_BOX(sideHdr), sideTitle, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(sideHdr), btnMoveUp, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sideHdr), btnMoveDown, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(st->sidebarBox), sideHdr, FALSE, FALSE, 0);

    st->sidebarScroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(st->sidebarScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    st->sidebarCanvas = gtk_drawing_area_new();
    gtk_widget_add_events(st->sidebarCanvas, GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK);
    g_signal_connect(st->sidebarCanvas, "draw", G_CALLBACK(on_sidebar_draw), st);
    g_signal_connect(st->sidebarCanvas, "button-press-event", G_CALLBACK(on_sidebar_button_press), st);
    gtk_container_add(GTK_CONTAINER(st->sidebarScroll), st->sidebarCanvas);
    gtk_box_pack_start(GTK_BOX(st->sidebarBox), st->sidebarScroll, TRUE, TRUE, 0);

    // Search results list pane (visible when search has matches)
    st->searchResultsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(st->searchResultsBox), "search-results-pane");
    gtk_widget_set_size_request(st->searchResultsBox, -1, 190);
    GtkWidget* srHdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(srHdr), "sidebar-header");
    st->searchResultsLabel = gtk_label_new("Resultados (0)");
    gtk_widget_set_halign(st->searchResultsLabel, GTK_ALIGN_START);
    GtkWidget* srClose = MakeRibbonButton("\xC3\x97", "Limpiar b\xC3\xBAsqueda");
    g_signal_connect(srClose, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (s->searchEntry) gtk_entry_set_text(GTK_ENTRY(s->searchEntry), "");
        PerformLiveSearch(s, "", false);
    }), st);
    gtk_box_pack_start(GTK_BOX(srHdr), st->searchResultsLabel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(srHdr), srClose, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(st->searchResultsBox), srHdr, FALSE, FALSE, 0);

    GtkWidget* srScroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(srScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    st->searchResultsList = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(st->searchResultsList), GTK_SELECTION_SINGLE);
    g_signal_connect(st->searchResultsList, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* row, gpointer d) {
        auto* s = static_cast<StudioAppState*>(d);
        if (!row) return;
        const std::size_t idx = static_cast<std::size_t>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "match-index")));
        if (idx < s->searchMatches.size()) {
            s->activeSearchIdx = idx;
            CenterCameraOnPage(s, s->searchMatches[idx].pageIndex);
            s->statusBannerUtf8 = "Coincidencia " + std::to_string(idx + 1) + "/" + std::to_string(s->searchMatches.size());
            InvalidateViews(s);
        }
    }), st);
    gtk_container_add(GTK_CONTAINER(srScroll), st->searchResultsList);
    gtk_box_pack_start(GTK_BOX(st->searchResultsBox), srScroll, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(st->sidebarBox), st->searchResultsBox, FALSE, FALSE, 0);

    // Central Cairo Viewport
    st->viewport = gtk_drawing_area_new();
    gtk_widget_set_hexpand(st->viewport, TRUE);
    gtk_widget_set_vexpand(st->viewport, TRUE);
    gtk_widget_set_can_focus(st->viewport, TRUE);
    gtk_widget_add_events(st->viewport,
        GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK);
    g_signal_connect(st->viewport, "draw", G_CALLBACK(on_viewport_draw), st);
    g_signal_connect(st->viewport, "size-allocate", G_CALLBACK(on_viewport_size_allocate), st);
    g_signal_connect(st->viewport, "button-press-event", G_CALLBACK(on_viewport_button_press), st);
    g_signal_connect(st->viewport, "motion-notify-event", G_CALLBACK(on_viewport_motion), st);
    g_signal_connect(st->viewport, "button-release-event", G_CALLBACK(on_viewport_button_release), st);
    g_signal_connect(st->viewport, "scroll-event", G_CALLBACK(on_viewport_scroll), st);
    gtk_box_pack_start(GTK_BOX(bodyBox), st->viewport, TRUE, TRUE, 0);

    // Bottom Status Bar
    GtkWidget* statusBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(statusBar), "studio-statusbar");
    st->statusLabel = gtk_label_new("Document Studio listo");
    gtk_label_set_xalign(GTK_LABEL(st->statusLabel), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(st->statusLabel), PANGO_ELLIPSIZE_END);
    st->statusRightLabel = gtk_label_new("Cach\xC3\xA9: 0/256 MiB");
    gtk_box_pack_start(GTK_BOX(statusBar), st->statusLabel, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(statusBar), st->statusRightLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(st->rootBox), statusBar, FALSE, FALSE, 0);

    // Drag & Drop support
    static GtkTargetEntry dropTargets[] = {
        { const_cast<gchar*>("text/uri-list"), 0, 0 }
    };
    gtk_drag_dest_set(st->window, GTK_DEST_DEFAULT_ALL, dropTargets, 1, GDK_ACTION_COPY);
    g_signal_connect(st->window, "drag-data-received", G_CALLBACK(on_drag_data_received), st);

    // Keyboard & Lifecycle signals
    g_signal_connect(st->window, "key-press-event", G_CALLBACK(on_window_key_press), st);
    g_signal_connect(st->window, "key-release-event", G_CALLBACK(on_window_key_release), st);
    g_signal_connect(st->window, "destroy", G_CALLBACK(on_window_destroy), st);

    gtk_widget_show_all(st->window);
    gtk_widget_hide(st->searchResultsBox);
}

void ApplyInitialDocumentAndMode(StudioAppState* st) {
    if (!st) return;
    if (st->cliMode == "canvas" && st->doc.CanvasObjects().empty()) {
        st->doc.ResetToBlank(DocumentMode::InfiniteCanvas);
        st->doc.SetCanvasBackground(PaperPattern::Grid);
        st->doc.SetTitle("Lienzo Infinito");
    }

    if (!st->cliOpenPathUtf8.empty()) {
        std::string err;
        if (!OpenDocumentFile(st, st->cliOpenPathUtf8, &err)) {
            st->statusBannerUtf8 = "Error abriendo argumento: " + err;
        }
    }

    if (st->cliMode == "canvas") {
        if (st->doc.Mode() != DocumentMode::InfiniteCanvas) {
            st->doc.SetMode(DocumentMode::InfiniteCanvas);
        }
        st->camera.zoom = 1.0;
        st->camera.panX = -48.0;
        st->camera.panY = -48.0;
    } else if (st->cliMode == "pdf") {
        if (st->doc.Mode() != DocumentMode::PaginatedPdf) {
            st->doc.SetMode(DocumentMode::PaginatedPdf);
            st->doc.RebuildLayout();
        }
        const std::size_t pgIdx = static_cast<std::size_t>(std::max(0, st->initialPage1Based - 1));
        CenterCameraOnPage(st, pgIdx);
    } else if (st->doc.Mode() == DocumentMode::PaginatedPdf) {
        const std::size_t pgIdx = static_cast<std::size_t>(std::max(0, st->initialPage1Based - 1));
        CenterCameraOnPage(st, pgIdx);
    }
    st->initialPage1Based = 1;

    if (st->showcaseStemCli) {
        AppActions::PopulateStemShowcaseScene(st->doc, st->cmdStack);
        st->minimapVisible = true;
        st->rulerState.visible = true;
        st->rulerState.centerX = 610.0;
        st->rulerState.centerY = 294.0;
        st->rulerState.angleDeg = -8.0;
        st->rulerState.lengthPt = 250.0;
        st->camera.zoom = 1.0;
        st->camera.panX = -18.0;
        st->camera.panY = -12.0;
    } else if (!st->cliCalcCardSpec.empty()) {
        ApplyCliCalcCardSpec(st);
    } else if (st->doc.Mode() == DocumentMode::InfiniteCanvas &&
        st->doc.CanvasObjects().empty() &&
        st->cliOpenPathUtf8.empty()) {
        AnnotationObject box;
        box.kind = ObjectKind::Shape;
        box.shapeKind = ShapeKind::Rectangle;
        box.bounds = { 80.0, 80.0, 260.0, 120.0 };
        box.strokeColor = DocColor::RGBA(37, 99, 235, 255);
        box.fillColor = DocColor::RGBA(239, 246, 255, 255);
        st->doc.AddCanvasObject(std::move(box));

        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 96.0, 102.0, 228.0, 80.0 };
        tb.textUtf8 = "Lienzo Infinito listo\nZoom y pan ilimitados";
        tb.strokeColor = DocColor::RGBA(15, 23, 42, 255);
        tb.fontSizePt = 15.0;
        st->doc.AddCanvasObject(std::move(tb));
    }

    InvalidateViews(st);
}

void ParseCliArgs(int argc, char** argv, StudioAppState* st, std::vector<char*>& outGtkArgv) {
    outGtkArgv.clear();
    if (argc > 0) outGtkArgv.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--open" && i + 1 < argc) {
            st->cliOpenPathUtf8 = argv[++i];
        } else if (arg == "--page" && i + 1 < argc) {
            try {
                st->initialPage1Based = std::max(1, std::stoi(argv[++i]));
            } catch (...) {
                st->initialPage1Based = 1;
            }
        } else if (arg == "--mode" && i + 1 < argc) {
            const std::string m = ToLowerAscii(argv[++i]);
            if (m == "canvas" || m == "pdf") st->cliMode = m;
        } else if (arg == "--calc-card" && i + 1 < argc) {
            st->cliCalcCardSpec = argv[++i];
        } else if (arg == "--showcase-stem") {
            st->showcaseStemCli = true;
            st->cliMode = "canvas";
        } else if (arg == "--screenshot-out" && i + 1 < argc) {
            st->screenshotOutPathUtf8 = argv[++i];
        } else if (arg == "--smoke-test") {
            st->smokeTestMode = true;
        } else {
            outGtkArgv.push_back(argv[i]);
        }
    }
}

void on_app_activate(GtkApplication* app, gpointer data) {
    auto* st = static_cast<StudioAppState*>(data);
    if (!st) return;

    BuildStudioWindow(app, st);
    ApplyInitialDocumentAndMode(st);

    if (!st->screenshotOutPathUtf8.empty() || st->smokeTestMode) {
        struct SmokePollState {
            StudioAppState* st = nullptr;
            int tries = 0;
        };
        auto* ps = new SmokePollState{ st, 0 };
        g_timeout_add(50, +[](gpointer d) -> gboolean {
            auto* p = static_cast<SmokePollState*>(d);
            StudioAppState* s = p ? p->st : nullptr;
            if (!s || s->teardownDone) {
                delete p;
                return G_SOURCE_REMOVE;
            }
            p->tries++;
            const bool mapped = s->window && gtk_widget_get_mapped(s->window) && gtk_widget_get_realized(s->window);
            const int vpW = s->viewport ? gtk_widget_get_allocated_width(s->viewport) : 0;
            if ((!mapped || vpW < 200 || p->tries < 4) && p->tries < 35) {
                return G_SOURCE_CONTINUE;
            }
            if (!s->screenshotOutPathUtf8.empty()) {
                CaptureStudioScreenshotToPng(s, s->screenshotOutPathUtf8);
            }
            if (s->smokeTestMode) {
                if (s->window) gtk_widget_destroy(s->window);
                if (s->app) g_application_quit(G_APPLICATION(s->app));
            }
            delete p;
            return G_SOURCE_REMOVE;
        }, ps);
    }
}

} // namespace

int main(int argc, char** argv) {
    auto st = std::make_unique<StudioAppState>();

    std::vector<std::string> cliArgsUtf8;
    for (int i = 1; i < argc; ++i) cliArgsUtf8.emplace_back(argv[i]);
    const CliDispatch::StudioCliOptions cliOpts = CliDispatch::ParseArgsUtf8(cliArgsUtf8);

    std::vector<char*> gtkArgs;
    ParseCliArgs(argc, argv, st.get(), gtkArgs);

    // Execute extended batch/workflow/academic/audio/crypto/LAN CLI commands first
    bool ranExtendedCli = false;
    if (cliOpts.HasAnyExtendedWorkflow() ||
        (!cliOpts.openPathUtf8.empty() && !cliOpts.decryptPassUtf8.empty())) {
        ranExtendedCli = true;
        const auto disp = CliDispatch::ExecuteWorkflow(cliOpts, st->doc, st->audioSession);
        if (!disp.statusBannerUtf8.empty()) {
            st->statusBannerUtf8 = disp.statusBannerUtf8;
        }
        if (disp.exitImmediately) {
            PerformTeardown(st.get());
            return disp.exitCode;
        }
        if (cliOpts.calcBridgeSync) {
            st->cliCalcCardSpec.clear();
        }
        const std::string openExt = GetFileExtLowerUtf8(st->cliOpenPathUtf8);
        if (openExt == ".png" || openExt == ".jpg" || openExt == ".jpeg" || !cliOpts.decryptPassUtf8.empty()) {
            st->cliOpenPathUtf8.clear();
        }
    }

    int gtkArgc = 1;
    char* singleArgv[2] = { argc > 0 ? argv[0] : const_cast<char*>("document-studio"), nullptr };
    char** gtkArgv = singleArgv;

    if (!gtk_init_check(&gtkArgc, &gtkArgv)) {
        if (st->smokeTestMode || !st->screenshotOutPathUtf8.empty() || ranExtendedCli) {
            st->headlessNoDisplay = true;
            if (!ranExtendedCli) {
                ApplyInitialDocumentAndMode(st.get());
            } else if (cliOpts.simulateGpuReset) {
                st->renderer.DiscardDeviceResources();
            }
            bool ok = true;
            if (!st->screenshotOutPathUtf8.empty()) {
                ok = CaptureStudioScreenshotToPng(st.get(), st->screenshotOutPathUtf8);
            }
            PerformTeardown(st.get());
            return ok ? 0 : 1;
        }
        std::fprintf(stderr, "Document Studio: no se pudo inicializar el entorno gr\xC3\xA1" "fico GTK3.\n");
        PerformTeardown(st.get());
        return 1;
    }

    GtkApplication* app = gtk_application_new(
        "org.calculadoraunificada.documentstudio",
        G_APPLICATION_NON_UNIQUE);
    if (!st->screenshotOutPathUtf8.empty()) {
        st->smokeTestMode = true;
    }
    g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), st.get());

    // Pass filtered argc=1 so GApplication doesn't reject custom CLI flags
    const int status = g_application_run(G_APPLICATION(app), 1, singleArgv);
    PerformTeardown(st.get());
    g_object_unref(app);
    return status;
}
