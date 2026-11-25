#pragma once

#include "../core/ai_provider_vault.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/structured_database.hpp"
#include "../core/zip_container.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

// ---------------- Universal Formats Interop (.canvas, SVG, Anki/CSV/TSV, OneNote Graph HTML, .nebo/.nsa/.ntb/.goodnotes/.note) ----------------

class UniversalFormatsInterop {
public:
    // JSON Canvas 1.0 (.canvas) read/write (OB-EXT-090, OB-UI-107)
    static std::string ExportToJsonCanvas(const DocumentModel& doc);
    static bool ImportFromJsonCanvas(
        const std::string& jsonCanvasUtf8,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    // Vector SVG page export (MS-EXT-080)
    static std::string ExportPageToSvg(const DocumentModel& doc, std::size_t pageIndex0);

    // Study Deck / Flashcards CSV, TSV & Anki export/import (GN-EXT-087, GN-EXT-088, NO-EXT-101, NO-EXT-107)
    static std::string ExportStudyDeckToDelimited(const StudyDeck& deck, char delimiter = ',');
    static StudyDeck ImportStudyDeckFromDelimited(
        const std::string& contentUtf8,
        char delimiter = ',',
        const std::string& deckTitleUtf8 = "Mazo Importado");

    // Microsoft OneNote / Graph compatible HTML page representation (ON-EXT-092..097, NO-EXT-099, NO-EXT-105)
    static std::string ExportPageToOneNoteGraphHtml(const DocumentModel& doc, std::size_t pageIndex0);
    static bool ImportPageFromOneNoteGraphHtml(
        const std::string& htmlUtf8,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    // Universal Note Package (.nebo, .nsa, .ntb, .goodnotes, .note) container export/import
    static bool ExportUniversalNotePackage(
        const DocumentModel& doc,
        const std::wstring& outFilePath,
        const std::string& formatExtensionUtf8,
        std::string* outErr = nullptr);

    static bool ImportUniversalNotePackage(
        const std::wstring& inFilePath,
        DocumentModel& outDoc,
        std::string* outDetectedFormatUtf8 = nullptr,
        std::string* outErr = nullptr);
};

// ---------------- Web Clipper Engine (Templates, Variables, Pipe Filters & Domain Rules) ----------------

struct WebClipperTemplate {
    std::string idUtf8;
    std::string nameUtf8;
    std::string domainPatternUtf8;   // e.g. "arxiv.org", "wikipedia.org"
    std::string requiredSchemaTypeUtf8; // e.g. "ScholarlyArticle", "Article"
    std::string markdownTemplateUtf8;
};

struct WebClipCaptureResult {
    bool ok = false;
    std::string matchedTemplateNameUtf8;
    std::string extractedTitleUtf8;
    std::string extractedAuthorUtf8;
    std::string extractedSchemaTypeUtf8;
    std::vector<std::string> highlightsUtf8;
    std::string markdownOutputUtf8;
};

class WebClipperEngine {
public:
    // Real WinHTTP GET download of live HTTP/HTTPS web pages (zero browser dependency)
    static bool FetchUrlHtmlWinHttp(
        const std::string& urlUtf8,
        std::string& outHtmlUtf8,
        std::string* outErr = nullptr,
        int timeoutMs = 12000);

    static const WebClipperTemplate* MatchTemplateForPage(
        const std::string& urlUtf8,
        const std::string& rawHtmlUtf8,
        const std::vector<WebClipperTemplate>& templates);

    static WebClipCaptureResult ClipHtmlPage(
        const std::string& urlUtf8,
        const std::string& rawHtmlUtf8,
        const std::string& userSelectionUtf8,
        const WebClipperTemplate& tpl,
        const std::string& captureDateIsoUtf8 = "2026-11-20");
};

// ---------------- Event-Triggered Automations & Action Buttons (NO-EXT-112, NO-EXT-113) ----------------

enum class AutomationTriggerKind : std::uint8_t {
    OnRecordCreated = 0,
    OnPropertyChanged = 1,
    OnActionButtonClicked = 2
};

struct AutomationActionStep {
    std::string actionKindUtf8; // "set_property", "append_text", "add_tag"
    std::string targetKeyUtf8;
    std::string valueUtf8;
};

struct AutomationRule {
    std::string ruleIdUtf8;
    std::string nameUtf8;
    AutomationTriggerKind trigger = AutomationTriggerKind::OnActionButtonClicked;
    std::string watchColumnIdUtf8;
    std::string watchValueUtf8;
    std::vector<AutomationActionStep> steps;
};

class AutomationRuleEngine {
public:
    void RegisterRule(AutomationRule rule);
    const std::vector<AutomationRule>& Rules() const noexcept { return rules_; }
    void ClearRules() noexcept { rules_.clear(); }
    std::size_t TriggerEvent(
        AutomationTriggerKind trigger,
        StructuredDatabase& db,
        const std::string& recordIdUtf8,
        const std::string& changedColumnIdUtf8 = "",
        const std::string& newValueUtf8 = "") const;

    std::string SerializeToJson() const;
    bool DeserializeFromJson(const std::string& jsonUtf8);

private:
    std::vector<AutomationRule> rules_;
};

// ---------------- Local/Configurable LLM Endpoint (Ollama / LM Studio / OpenAI-compatible) ----------------

struct LocalLlmConfig {
    bool enabled = false;
    std::string endpointUrlUtf8 = "http://127.0.0.1:11434/api/chat";
    std::string modelNameUtf8 = "llama3.2";
    std::string apiKeyUtf8; // Optional Bearer token (only user-provided, never hardcoded)
    int timeoutMs = 15000;
};

// ---------------- Grounded Local Study AI Assistant (Citations, Quizzes, Summaries, Translation) ----------------

struct GroundedCitation {
    std::string docTitleUtf8;
    std::size_t pageIndex0 = 0;
    std::uint64_t objectId = 0;
    std::uint64_t audioTimestampMs = 0;
    std::string sourceKindUtf8; // "page_text", "handwriting", "math", "audio_transcript"
    std::string quoteSnippetUtf8;
};

struct GroundedAiAnswer {
    bool answered = false;
    bool usedLiveLlm = false;
    std::string providerUsedUtf8;
    std::uint64_t latencyMs = 0;
    std::string answerTextUtf8;
    std::vector<GroundedCitation> citations;
};

struct QuizQuestionItem {
    std::string questionUtf8;
    std::vector<std::string> optionsUtf8;
    int correctOptionIndex = 0;
    GroundedCitation sourceCitation;
};

struct QuizGradeReport {
    std::size_t totalQuestions = 0;
    std::size_t correctCount = 0;
    double scorePercent = 0.0;
};

struct StudyProgressMetrics {
    std::uint64_t totalStudyMinutes = 0;
    std::size_t masteredCards = 0;
    std::size_t totalCards = 0;
    double completionRatio = 0.0;
};

class GroundedStudyAiAssistant {
public:
    static bool QueryLocalLlmViaWinHttp(
        const LocalLlmConfig& cfg,
        const std::string& systemPromptUtf8,
        const std::string& userPromptUtf8,
        std::string& outReplyUtf8,
        std::string* outErr = nullptr);

    static GroundedAiAnswer AnswerQuestionWithCitations(
        const DocumentModel& doc,
        const std::vector<SpeakerTranscriptSegment>& audioSegments,
        const std::string& questionUtf8,
        const LocalLlmConfig* optionalLlm = nullptr,
        const AiProviderVault* optionalVault = nullptr);

    static std::string GenerateExecutiveSummary(
        const DocumentModel& doc,
        const std::vector<SpeakerTranscriptSegment>& audioSegments,
        const LocalLlmConfig* optionalLlm = nullptr,
        const AiProviderVault* optionalVault = nullptr);

    static std::vector<QuizQuestionItem> GenerateMultipleChoiceQuiz(
        const DocumentModel& doc,
        std::size_t maxQuestions = 5);

    static QuizGradeReport GradeQuizResponses(
        const std::vector<QuizQuestionItem>& quiz,
        const std::vector<int>& selectedOptionIndices);

    static std::string TranslateSelectionOffline(
        const std::string& textUtf8,
        const std::string& targetLangCodeUtf8 = "en",
        const LocalLlmConfig* optionalLlm = nullptr,
        const AiProviderVault* optionalVault = nullptr);

    static StudyProgressMetrics ComputeStudyProgress(
        std::uint64_t accumulatedMinutes,
        const StudyDeck& deck);
};

} // namespace DocStudio
