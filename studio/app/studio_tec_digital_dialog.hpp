#pragma once
// ============================================================================
// doc_studio/app/studio_tec_digital_dialog.hpp
// Native Win32 + Software Vector Canvas guías de interfaz "Centro Academico TEC"
// (0% GPU, 0% idle CPU, zero background timers, PerMonitorV2 4K,
// Porcelain Light / Graphite Dark / High Contrast synchronized, Read-Only).
// ============================================================================

#ifdef _WIN32

#include "../core/document_model.hpp"
#include "../integrations/tec_digital/tec_academic_store.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <string>

namespace UiGlass { struct GlassSettings; }

namespace DocStudio::TecDigitalUi {

enum class HubSection : int {
    Resumen = 0,
    Cursos = 1,
    Calendario = 2,
    ExpedienteYMalla = 3,
    HorariosVivo = 4,
    BibliotecaOffline = 5,
    TramitesYPerfil = 6
};

struct ActiveDocSearchHit {
    std::string titleUtf8;
    std::string snippetUtf8;
    std::size_t pageIndex = 0;
};

using SendCourseToGeminiFn = std::function<bool(
    const academic::tec_digital::CommunityInfo& course,
    const academic::tec_digital::CourseOverviewSnapshot* overview,
    const std::vector<academic::tec_digital::FileRecord>& downloadedFiles,
    std::string& outStatusUtf8)>;

using AddSm2FlashcardsFn = std::function<std::size_t(
    const std::vector<DocStudio::TecDigital::Sm2FlashcardDraft>& drafts)>;

using SearchActiveDocFn = std::function<std::vector<ActiveDocSearchHit>(
    const std::string& queryUtf8)>;

// Opens the modal guías de interfaz "TEC Digital" workspace window.
// Returns true if the active DocumentModel was modified (e.g. course card,
// schedule, curriculum roadmap, or material inserted onto canvas).
bool RunTecDigitalCenterDialog(
    HWND owner,
    DocStudio::DocumentModel& activeDoc,
    DocStudio::TecDigital::TecAcademicStore& store,
    HubSection initialSection,
    std::string& outStatusBannerUtf8,
    std::function<void()> onDocumentMutated = nullptr,
    int initialSubView = 0,
    SendCourseToGeminiFn onSendToGeminiNotebook = nullptr,
    AddSm2FlashcardsFn onAddSm2Flashcards = nullptr,
    SearchActiveDocFn onSearchActiveDoc = nullptr,
    const UiGlass::GlassSettings* glassSettings = nullptr,
    const wchar_t* autoScreenshotPathW = nullptr,
    int initialInnerTab = 0,
    bool backgroundStealth = false);

}  // namespace DocStudio::TecDigitalUi

#endif  // _WIN32
