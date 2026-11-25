#pragma once

#include "../core/notebook_vault.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio::Cas {

// ---------------- F-318: Quality Control & Provenance Audit Metadata ----------------

enum class VerificationStatus : std::uint8_t {
    Unverified = 0,
    SymbolicallyVerified = 1,
    NumericallyVerified = 2,
    DimensionallyVerified = 3,
    UserConfirmed = 4,
    Failed = 5
};

struct ProvenanceRef {
    std::string kindUtf8;      // "cas_step", "fts_hit", "math_block", "calc_card", "ocr_raw"
    std::string docKeyUtf8;
    std::size_t pageIndex = 0;
    std::uint64_t objectId = 0;
    std::string excerptUtf8;
};

struct CasAuditMetadata {
    std::string casVersion = "UnifiedCas-2026.1";
    VerificationStatus verificationStatus = VerificationStatus::Unverified;
    std::string methodUtf8;
    bool usedAi = false;
    std::vector<ProvenanceRef> provenanceRefs;
};

// ---------------- F-133, F-310, SUP-07: ConfirmInterpretationCard State Machine ----------------

enum class ConfirmationStatus : std::uint8_t {
    PendingReview = 0,
    Editing = 1,
    Confirmed = 2,
    Rejected = 3
};

struct InterpretationCard {
    std::uint64_t cardId = 0;
    ConfirmationStatus status = ConfirmationStatus::PendingReview;
    std::string rawSourceTextUtf8;   // Preserved verbatim (F-319)
    std::string rawSourceLatexUtf8;  // Preserved verbatim (F-319)
    double confidence = 0.0;
    MathJsonNode proposedAst{};
    MathJsonNode editedAst{};
    std::string editableExprUtf8;
    std::string editableLatexUtf8;
    std::string rejectionReasonUtf8;
    bool astValid = false;
    std::vector<std::string> validationErrors;
    CasAuditMetadata audit{};
};

InterpretationCard CreateInterpretationCard(
    std::uint64_t cardId,
    const std::string& rawCandidateUtf8,
    const std::string& rawLatexUtf8 = "",
    double confidence = 1.0);
bool BeginEditInterpretationCard(InterpretationCard& card);
bool UpdateInterpretationCardFromMathJson(InterpretationCard& card, const std::string& mathJsonUtf8);
bool UpdateInterpretationCardFromExpr(InterpretationCard& card, const std::string& exprOrLatexUtf8);
bool ConfirmInterpretationCard(InterpretationCard& card);
bool RejectInterpretationCard(InterpretationCard& card, const std::string& reasonUtf8 = "");

// ---------------- F-313, F-314, SUP-07: Multi-Line CheckMyWork & Error Classifier ----------------

enum class StepErrorCategory : std::uint8_t {
    None = 0,
    SignError = 1,
    OrderOfOperations = 2,
    DimensionalMismatch = 3,
    EquationBalanceError = 4,
    AlgebraicMismatch = 5,
    SyntaxError = 6
};

struct StepCheckResultV2 {
    bool equivalent = false;
    bool dimensionMismatch = false;
    double maxResidual = 0.0;
    std::string statusCode; // "OK", "DIFF", "DIM_ERR", "SIGN_ERR", "ORDER_ERR", "BALANCE_ERR", "SYNTAX_ERR"
    StepErrorCategory errorCategory = StepErrorCategory::None;
    std::string messageUtf8;
    std::string hintUtf8;
    std::string prevDimSiUtf8;
    std::string nextDimSiUtf8;
    CasAuditMetadata audit{};
};

StepCheckResultV2 CheckAlgebraicStepV2(
    const std::string& prevLineUtf8,
    const std::string& nextLineUtf8,
    const SymbolEnv& baseEnv = {});

struct StepLineDiagnostic {
    int lineIndex = 0;
    std::string rawLineUtf8;
    std::string canonicalExprUtf8;
    std::string unitDimSiUtf8;
    bool valid = true;
    StepCheckResultV2 transitionCheck{};
};

struct CheckMyWorkReport {
    bool allValid = false;
    int firstErrorLineIndex = -1;
    std::string summaryUtf8;
    std::vector<StepLineDiagnostic> lines;
    CasAuditMetadata audit{};
};

CheckMyWorkReport CheckMyWork(
    const std::vector<std::string>& derivationLinesUtf8,
    const SymbolEnv& baseEnv = {});

// ---------------- F-312, SUP-07: Deterministic Solve & Socratic Teach Me ----------------

struct SolveStep {
    int stepNumber = 1;
    std::string titleUtf8;
    std::string expressionUtf8;
    std::string latexUtf8;
    std::string rationaleUtf8;
};

struct SolveMethodResult {
    std::string methodNameUtf8;
    std::vector<SolveStep> steps;
    std::vector<std::string> solutionsUtf8;
    std::string finalSummaryUtf8;
};

struct DeterministicSolveResult {
    bool ok = false;
    std::string problemUtf8;
    std::string targetVarUtf8;
    SolveMethodResult primaryMethod{};
    bool hasAlternativeMethod = false;
    SolveMethodResult alternativeMethod{};
    std::string errorUtf8;
    CasAuditMetadata audit{};
};

DeterministicSolveResult SolveDeterministic(
    const std::string& equationOrSystemUtf8,
    const std::string& solveForVar = "x",
    const SymbolEnv& env = {});

struct SocraticHintLevel {
    int level = 1;
    std::string promptUtf8;
    std::string questionForStudentUtf8;
    bool revealsDirectAnswer = false;
    std::vector<ProvenanceRef> provenanceRefs;
};

struct TeachMeResponse {
    bool ok = false;
    std::string problemUtf8;
    std::string contextSummaryUtf8;
    std::vector<SocraticHintLevel> hints;
    CheckMyWorkReport studentWorkAudit{};
    CasAuditMetadata audit{};
};

TeachMeResponse GenerateTeachMeSocratic(
    const std::string& problemUtf8,
    const std::vector<std::string>& studentStepsUtf8 = {},
    const std::string& solveForVar = "x",
    const SymbolEnv& env = {},
    const std::vector<FtsHit>& notebookContext = {});

// ---------------- F-306, F-307: Opt-In AI Settings & Local-First Dispatch ----------------

struct AiSettings {
    bool enabled = false;
    bool hideAiUi = false;
    bool allowCloudFallback = false;
    std::string preferredLocalProviderUtf8 = "local_deterministic_cas";
};

enum class AiCapabilityKind : std::uint8_t {
    ConfirmOcrAst = 0,
    CheckMyWork = 1,
    Solve = 2,
    TeachMe = 3,
    SearchSynthesis = 4,
    SummarizeNotes = 5,
    DraftFlashcards = 6,
    TranslateSideBySide = 7,
    GenerateReport = 8
};

enum class DispatchRoute : std::uint8_t {
    LocalDeterministic = 0,
    LocalOnlyCloudBlocked = 1,
    OptInCloudPermitted = 2
};

struct DispatchDecision {
    AiCapabilityKind capability = AiCapabilityKind::Solve;
    DispatchRoute route = DispatchRoute::LocalDeterministic;
    bool requiresCloud = false;
    bool executedLocally = true;
    std::string policyNoteUtf8;
};

DispatchDecision ResolveLocalFirstDispatch(
    AiCapabilityKind capability,
    const AiSettings& settings,
    bool callerRequestedCloudEnhancement = false);

// ---------------- F-308: Grounded Search Synthesis ----------------

struct SearchCitation {
    int citationIndex = 1;
    std::string docKeyUtf8;
    std::size_t pageIndex = 0;
    std::uint64_t objectId = 0;
    std::string sourceKindUtf8;
    std::string quotedSnippetUtf8;
    int score = 0;
};

struct GroundedSearchSynthesis {
    bool hasEvidence = false;
    std::string queryUtf8;
    std::string synthesizedAnswerUtf8;
    std::vector<SearchCitation> citations;
    CasAuditMetadata audit{};
};

GroundedSearchSynthesis SynthesizeGroundedSearchAnswer(
    const std::string& queryUtf8,
    const std::vector<FtsHit>& hits,
    std::size_t maxCitations = 6);

// ---------------- F-309: Non-Destructive Note Summarizer ----------------

struct SummaryArtifact {
    bool ok = false;
    std::string targetLayerNameUtf8 = "Resumen Asistido (No Destructivo)";
    AnnotationObject summaryBlock{};
    std::vector<std::uint64_t> sourceObjectIds;
    CasAuditMetadata audit{};
};

SummaryArtifact SummarizeDocumentNonDestructive(
    const DocumentModel& doc,
    std::size_t maxBullets = 8);

// ---------------- F-311: Reviewable Flashcard Preview/Edit Workflow ----------------

enum class FlashcardDraftStatus : std::uint8_t {
    PendingReview = 0,
    Edited = 1,
    Approved = 2,
    Rejected = 3
};

struct FlashcardDraft {
    FlashcardItem item{};
    FlashcardDraftStatus status = FlashcardDraftStatus::PendingReview;
    std::string sourceTagUtf8;
};

struct FlashcardReviewSession {
    std::string deckTitleUtf8;
    std::vector<FlashcardDraft> drafts;
};

FlashcardReviewSession BuildFlashcardReviewSession(const DocumentModel& doc, const std::string& deckTitle = "");
bool EditFlashcardDraft(FlashcardReviewSession& session, std::size_t index, const std::string& newQ, const std::string& newA);
bool ApproveFlashcardDraft(FlashcardReviewSession& session, std::size_t index);
bool RejectFlashcardDraft(FlashcardReviewSession& session, std::size_t index);
std::size_t CommitApprovedFlashcardsToDeck(const FlashcardReviewSession& session, StudyDeck& outDeck);

// ---------------- F-315: Deterministic IEEE / TEC / Lab Report Templates ----------------

enum class ReportTemplateKind : std::uint8_t {
    IeeePaper = 0,
    TecEngineering = 1,
    LabReport = 2
};

struct DeterministicReportOptions {
    ReportTemplateKind templateKind = ReportTemplateKind::TecEngineering;
    std::string authorUtf8 = "Equipo STEM";
    std::string courseOrProjectUtf8 = "Memoria de C\xC3\xA1lculo";
    std::string institutionUtf8 = "TEC / Lab";
    bool includeAuditFooter = true;
};

struct DeterministicReportOutput {
    bool ok = false;
    ReportTemplateKind templateKind = ReportTemplateKind::TecEngineering;
    std::string markdownUtf8;
    std::string htmlUtf8;
    CasAuditMetadata audit{};
};

DeterministicReportOutput RenderDeterministicReportTemplate(
    const DocumentModel& doc,
    const DeterministicReportOptions& opts = {});

// ---------------- F-316: Non-Destructive Side-by-Side Translation ----------------

struct SideBySideTranslation {
    bool ok = false;
    std::string sourceLangUtf8;
    std::string targetLangUtf8;
    std::string sourceTextUtf8;
    std::string translatedTextUtf8;
    std::vector<std::string> preservedMathSegments;
    std::vector<std::string> preservedUnitTokens;
    bool allMathAndUnitsIntact = false;
    CasAuditMetadata audit{};
};

SideBySideTranslation TranslatePreservingMathAndUnits(
    const std::string& sourceTextUtf8,
    const std::string& sourceLangUtf8 = "es",
    const std::string& targetLangUtf8 = "en");

// ---------------- F-317: Deterministic Canvas + Calculator Macro Recorder ----------------

enum class MacroActionKind : std::uint8_t {
    CanvasAddMathBlock = 0,
    CanvasUpdateMathExpr = 1,
    CanvasEvaluateDag = 2,
    CanvasCheckStep = 3,
    CanvasConfirmOcrCard = 4,
    CalcPressKey = 5,
    CalcSetMode = 6,
    CalcBridgeToCanvas = 7
};

struct MacroActionStep {
    std::uint64_t seqIndex = 0;
    MacroActionKind kind = MacroActionKind::CanvasAddMathBlock;
    std::string targetIdOrModelUtf8;
    std::string payloadUtf8;
    std::string expectedResultUtf8;
};

class DeterministicMacroRecorder {
public:
    void Start(std::string macroNameUtf8 = "Macro_STEM");
    void Stop();
    bool IsRecording() const noexcept { return recording_; }
    void Record(MacroActionKind kind, std::string target, std::string payload, std::string expected = "");
    const std::string& Name() const noexcept { return name_; }
    const std::vector<MacroActionStep>& Steps() const noexcept { return steps_; }
    void Clear();
    std::string ToJsonString() const;
    bool FromJsonString(const std::string& jsonUtf8, std::string* outErr = nullptr);

private:
    bool recording_ = false;
    std::string name_ = "Macro_STEM";
    std::vector<MacroActionStep> steps_;
};

// ---------------- F-319: Anti-Hiding Invariant ----------------

struct RawTruthProjection {
    std::string rawExpressionUtf8;
    MathSubMode subMode = MathSubMode::VarSub;
    std::vector<std::pair<std::string, std::string>> sourceVariables;
    std::vector<std::string> deterministicErrors;
    bool visibleWithoutAi = true;
};

RawTruthProjection ExtractRawTruthProjection(
    const AnnotationObject& obj,
    const SymbolEnv& env = {});

bool ValidateAntiHidingInvariant(
    const RawTruthProjection& truth,
    const std::string& renderedDisplayUtf8,
    const AiSettings& settings,
    std::vector<std::string>* outViolations = nullptr);

} // namespace DocStudio::Cas
