#pragma once

#include "ai_provider_vault.hpp"
#include "audio_timeline.hpp"
#include "command_stack.hpp"
#include "document_model.hpp"
#include "knowledge_graph.hpp"
#include "notebook_vault.hpp"
#include "structured_database.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../pdf/flexcil_study_overlay.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

// ---------------- 1. Guided Step-by-Step Tutor (3-Level Socratic + AI/Offline CAS) ----------------

enum class TutorHintLevel : std::uint8_t {
    ConceptualHint = 1,
    StepByStepPlan = 2,
    FullVerification = 3
};

struct GuidedTutorResult {
    bool ok = false;
    bool usedLiveAi = false;
    std::string providerUsedUtf8;
    std::uint64_t latencyMs = 0;
    TutorHintLevel level = TutorHintLevel::ConceptualHint;
    std::string responseMarkdownUtf8;
};

// ---------------- 2. Concept Map Generator ----------------

struct ConceptMapBranch {
    std::string titleUtf8;
    std::vector<std::string> subItemsUtf8;
};

struct ConceptMapBuildResult {
    bool ok = false;
    bool usedLiveAi = false;
    std::size_t nodeCount = 0;
    std::size_t connectorCount = 0;
    std::string summaryUtf8;
};

// ---------------- 3. Formula Sheet & Glossary Extractor ----------------

struct FormulaSheetGlossaryResult {
    std::size_t formulaCount = 0;
    std::size_t glossaryCount = 0;
    std::size_t codeSnippetCount = 0;
    std::string markdownUtf8;
};

// ---------------- 4. Smart Pomodoro Timer ----------------

struct PomodoroTimerState {
    bool running = false;
    bool paused = false;
    bool inBreakPhase = false;
    bool autoEnableFocusMode = true;
    int focusMinutes = 25;
    int breakMinutes = 5;
    int remainingSeconds = 25 * 60;
    int completedCycles = 0;
    std::uint64_t lastTickMs = 0;

    void Configure(int focusMin, int breakMin, bool autoFocus = true) noexcept;
    void Start(std::uint64_t nowTickMs) noexcept;
    void TogglePause(std::uint64_t nowTickMs) noexcept;
    void Reset() noexcept;
    // Returns true if visual countdown/phase changed; outputs exact focus seconds to add to totalStudySeconds
    bool Tick(std::uint64_t nowTickMs, std::uint64_t& outAddedFocusSec, bool* outPhaseTransitioned = nullptr) noexcept;
    std::string FormatHudBadgeUtf8() const;
};

// ---------------- 5. Automatic Task & Deadline Extractor + RFC 5545 .ics ----------------

struct ExtractedStudyTask {
    std::string titleUtf8;
    std::string dueDateIsoUtf8; // "YYYY-MM-DD" or empty
    std::string categoryUtf8;   // "Entrega", "Examen", "Tarea", "Pendiente"
    std::size_t sourcePageIndex0 = 0;
    bool sourceIsCanvas = false;
};

struct TaskExtractionReport {
    std::vector<ExtractedStudyTask> extracted;
    std::size_t insertedIntoDatabaseCount = 0;
    std::string icsCalendarUtf8;
};

// ---------------- 6. Post-Audio 1-Click Lecture Automation ----------------

struct PostLectureAutomationReport {
    bool ok = false;
    std::size_t transcriptSegments = 0;
    std::size_t tasksInserted = 0;
    std::uint64_t insertedCardObjId = 0;
    std::string summaryMarkdownUtf8;
    std::string statusNoteUtf8;
};

// ---------------- 7. Academic Citation Formatter (APA 7 / IEEE / BibTeX) ----------------

enum class CitationStyle : std::uint8_t {
    Apa7 = 0,
    Ieee = 1,
    BibTeX = 2
};

struct AcademicSourceInfo {
    std::string authorsUtf8;     // e.g. "Stewart, J.; Clegg, D."
    std::string titleUtf8;       // e.g. "Calculo de varias variables"
    std::string yearUtf8;        // e.g. "2024"
    std::string venueOrPublisherUtf8; // Journal / Publisher
    std::string urlUtf8;
    std::string doiUtf8;
    std::string pagesUtf8;
    std::string sourceKindUtf8;  // "book", "article", "web"
};

class StudyAssistantTools {
public:
    // 1. Guided Step-by-Step Tutor (Socratic progressive hints + AI Multi-Provider / offline fallback)
    static GuidedTutorResult RunGuidedTutor(
        const DocumentModel& doc,
        const std::string& problemOrTopicUtf8,
        TutorHintLevel level,
        bool socraticNoSpoiler,
        const AiProviderVault* optionalVault = nullptr);

    // 2. Generate SM-2 Flashcards + optional StudyTape Practice Exercises on canvas/page (atomic undo)
    static std::vector<FlashcardItem> GenerateSmartFlashcards(
        const DocumentModel& doc,
        std::size_t maxCards = 8,
        const AiProviderVault* optionalVault = nullptr,
        bool* outUsedLiveAi = nullptr);

    static std::size_t InsertPracticeStudyTapes(
        DocumentModel& doc,
        CommandStack& stack,
        ObjectTarget target,
        const std::vector<FlashcardItem>& cards,
        double startX = 80.0,
        double startY = 80.0);

    // 3. Concept Map Generator (nodes + magnetic ShapeKind::Connector edges, atomic 1-step Undo/Redo)
    static ConceptMapBuildResult GenerateConceptMapInDocument(
        DocumentModel& doc,
        CommandStack& stack,
        ObjectTarget target,
        const std::string& centralTopicUtf8,
        const AiProviderVault* optionalVault = nullptr,
        double originX = 120.0,
        double originY = 100.0);

    // 4. Formula Sheet & Glossary Builder (scans document + optional vault)
    static FormulaSheetGlossaryResult BuildFormulaSheetAndGlossary(
        const DocumentModel& doc,
        const KnowledgeVault* optionalVault = nullptr);

    static bool AppendFormulaSheetPage(
        DocumentModel& doc,
        CommandStack& stack,
        const FormulaSheetGlossaryResult& sheet);

    // 5. Automatic Task & Deadline Extractor -> StructuredDatabase + RFC 5545 .ics
    static TaskExtractionReport ExtractTasksAndDeadlines(
        const DocumentModel& doc,
        const KnowledgeVault* optionalVault,
        StructuredDatabase* optionalDbToPopulate = nullptr);

    static std::string ExportTasksToIcsCalendar(
        const std::vector<ExtractedStudyTask>& tasks,
        const std::string& calendarTitleUtf8 = "CuadernU Entregas y Estudio");

    // 6. Post-Audio 1-Click Pipeline: Transcribe -> Summarize -> Extract Tasks -> Insert Card
    static PostLectureAutomationReport RunPostLectureAutomation(
        DocumentModel& doc,
        CommandStack& stack,
        ObjectTarget target,
        AudioTimelineSession& audioSession,
        StructuredDatabase& db,
        const AiProviderVault* optionalVault = nullptr,
        double cardX = 80.0,
        double cardY = 80.0);

    // 7. Academic Citation Formatter (APA 7, IEEE, BibTeX) + source harvester
    static std::string FormatAcademicCitation(
        const AcademicSourceInfo& src,
        CitationStyle style,
        int ieeeIndex = 1);

    static std::vector<AcademicSourceInfo> HarvestCitationCandidates(
        const DocumentModel& doc,
        const FlexcilStudyOverlay* optionalFlexcil = nullptr);
};

} // namespace DocStudio
