#pragma once

#include "../core/doc_types.hpp"
#include "../math/engineering_cas.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace DocStudio::CalcBridge {

// ---- 1. Canonical 4-Model & SHIFT/ALPHA Metadata (F-207, F-247, F-248) ----

enum class CasioModel : std::uint8_t {
    Fx570LaX    = 0, // fx-570LA X / fx-991LA X (ClassWiz Blanca)
    Fx570VnPlus = 1, // fx-570VN PLUS / fx-570ES PLUS (Gris)
    FxCg50      = 2, // fx-CG50 PRIZM
    Fx991Cw     = 3  // fx-991CW (ClassWiz II Negra)
};

enum class KeyLayerKind : std::uint8_t {
    Normal     = 0,
    Shift      = 1,
    Alpha      = 2,
    ShiftAlpha = 3
};

enum class VerificationStatus : std::uint8_t {
    Unverified       = 0,
    VerifiedCasMatch = 1,
    PrecisionDiff    = 2,
    DimensionError   = 3,
    EvaluationError  = 4
};

struct ShiftAlphaBadgeInfo {
    KeyLayerKind layer = KeyLayerKind::Normal;
    std::string badgeTextUtf8 = "[N] NORMAL"; // Accessible non-color-only badge (F-267)
    std::string colorTokenUtf8 = "neutral";   // "neutral", "amber_shift", "rose_alpha", "violet_shift_alpha"
    std::uint32_t rgbaColor = 0x1A73E8FFu;
};

struct ShiftAlphaState {
    bool shiftActive = false;
    bool alphaActive = false;
    bool stoActive = false;
    bool rclActive = false;
    std::string angleModeUtf8 = "DEG";      // "DEG", "RAD", "GRAD"
    std::string displayFormatUtf8 = "Norm"; // "Norm", "Fix", "Sci", "Eng"
    ShiftAlphaBadgeInfo badge{};
};

struct CasioKeyStep {
    std::size_t stepIndex = 0;
    std::string modelIdUtf8;
    std::string keyNameUtf8;        // Physical key token e.g. "SIN", "FRACCION", "EXE"
    std::string displayCaptionUtf8; // Human/badge caption e.g. "sin^-1", "x10^x", "="
    KeyLayerKind layer = KeyLayerKind::Normal;
    ShiftAlphaBadgeInfo badge{};
    std::string menuContextUtf8;    // e.g. "Calculate", "Complex", "Matrix", "Equation"
    std::string expectedLcdTextUtf8;
    std::string noteUtf8;           // Optional teacher/step note
    std::uint64_t timestampMs = 0;
};

// ---- 2. Display Frames & CalcSessionState / CalcSessionCard (F-134, F-162, F-246, SUP-08) ----

struct CalcDisplayFrame {
    std::uint64_t timestampMs = 0;
    std::size_t stepIndex = 0;
    std::string keyNameUtf8;
    std::string lcdTextUtf8;
    std::string resultUtf8;
    std::string indicatorsSummaryUtf8;
    ShiftAlphaState shiftAlphaState{};
    int pixelWidth = 0;
    int pixelHeight = 0;
    std::vector<std::uint8_t> rgbaPixels;  // Optional top-down RGBA8888 (4 * W * H)
    std::vector<std::uint8_t> lcdPngBytes; // Lossless PNG (auto-encoded from rgbaPixels if empty)
};

struct CalcSessionState {
    std::string calcSessionIdUtf8;
    std::string modelIdUtf8 = "fx-991CW";
    std::string appModeUtf8 = "Calculate";
    std::string inputSequenceUtf8;
    std::vector<CasioKeyStep> steps;
    std::vector<CalcDisplayFrame> displayFrames;
    std::string formulaSourceUtf8;
    std::string unitAssumptionsUtf8 = "DEG; SI";
    std::string resultUtf8;
    std::string latexUtf8;
    VerificationStatus verificationStatus = VerificationStatus::Unverified;
    std::string verificationNoteUtf8;
    ShiftAlphaState shiftAlphaState{};
    std::uint64_t updatedUtcMs = 0;
};

struct CalcSessionCard {
    std::uint64_t objectId = 0;
    DocRect bounds{};
    bool liveBound = false;
    std::size_t activeFrameIndex = 0;
    CalcSessionState session{};

    AnnotationObject ToAnnotationObject(const Cas::SymbolEnv& env = {}) const;
    static CalcSessionCard FromAnnotationObject(const AnnotationObject& obj);
};

// Backward-compatible existing structs (trailing default-initialized fields appended safely)
struct CalcStepRecord {
    std::string modelIdUtf8; // "fx-991LAX", "fx-570VN PLUS", "fx-CG50", "fx-991CW"
    std::string keySeqUtf8;
    std::string expressionUtf8;
    std::string latexUtf8;
    std::string resultUtf8;
    std::string assignVarUtf8;
    std::vector<std::uint8_t> lcdPngBytes;
    // Extended M1_4 metadata
    std::string calcSessionIdUtf8;
    std::string formulaSourceUtf8;
    std::string unitAssumptionsUtf8;
    ShiftAlphaState shiftAlphaState{};
    std::vector<CalcDisplayFrame> displayFrames;
};

// Converts an arbitrary CAS/Casio expression into clean LaTeX string for Document Studio cards & exporters
std::string ExpressionToLatexUtf8(const std::string& exprUtf8, const std::string& resultUtf8 = "");

// Evaluates or replays a Casio step sequence and populates a CalcSessionCard AnnotationObject
AnnotationObject BuildCalcSessionCard(
    const CalcStepRecord& rec,
    const DocRect& bounds,
    const Cas::SymbolEnv& env = {});

// Re-evaluates a CalcSessionCard live when edited or when Canvas variables update
bool RefreshCalcSessionCard(AnnotationObject& obj, const Cas::SymbolEnv& env = {});

struct CasioKeystrokePlan {
    bool ok = false;
    std::string modelIdUtf8 = "fx-991CW";
    std::string appModeUtf8;       // "Calculate", "Complex", "Equation (Simultan)", "Matrix"
    std::string keySequenceUtf8;   // Exact physical Casio button sequence
    std::string evaluatedUtf8;     // Deterministic result
    std::string latexUtf8;
    // Extended M1_4 metadata
    CasioModel model = CasioModel::Fx991Cw;
    std::vector<CasioKeyStep> steps;
    std::vector<std::string> warningsUtf8;
};

// Reverse Bridge (Lienzo -> Casio): translates equations, linear systems, matrices, or AC phasors into exact Casio keystrokes
CasioKeystrokePlan TranslateExpressionToCasioKeys(
    const std::string& exprUtf8,
    const std::string& modelIdUtf8 = "fx-991CW",
    const Cas::SymbolEnv& env = {});

// Formats a complex/phasor quantity simultaneously in Rectangular (a+jb), Polar (r∠θ°), and Exponential (r*e^(j*θ)) forms
std::string FormatPhasorAllFormsUtf8(std::complex<double> z, const std::string& unitUtf8 = "");

// Model normalization & SHIFT/ALPHA badge helpers
CasioModel ParseCasioModel(const std::string& modelIdUtf8) noexcept;
std::string CanonicalModelIdUtf8(CasioModel model);
ShiftAlphaBadgeInfo MakeShiftAlphaBadge(KeyLayerKind layer, const std::string& modelIdUtf8 = "");
ShiftAlphaState NormalizeShiftAlphaState(ShiftAlphaState state, const std::string& modelIdUtf8 = "");
std::string FormatShiftAlphaCalloutUtf8(const ShiftAlphaState& state);

// Pure C++17 lossless PNG encoder (RGBA8888 -> PNG bytes, zero external deps)
std::vector<std::uint8_t> EncodeRgbaToPng(int width, int height, const std::vector<std::uint8_t>& rgbaPixels);

// Extended Card & Session API
AnnotationObject BuildCalcSessionCard(
    const CalcSessionState& state,
    const DocRect& bounds,
    const Cas::SymbolEnv& env = {});
CalcSessionCard CreateSessionCard(
    const CalcSessionState& state,
    const DocRect& bounds,
    const Cas::SymbolEnv& env = {});
bool RefreshCalcSessionCard(CalcSessionCard& card, const Cas::SymbolEnv& env = {});
bool BindCardToSession(AnnotationObject& obj, const std::string& calcSessionIdUtf8);
std::string ExtractSessionIdFromCard(const AnnotationObject& obj);

// Deterministic keystroke -> expression reconstruction & replay
std::string KeySequenceToExpressionUtf8(const std::string& keySeqUtf8);
bool ReplaySessionKeySequence(CalcSessionState& session, const Cas::SymbolEnv& env = {});

// In-process session registry (thread-safe, used by RefreshCalcSessionCard when live-bound)
void RegisterLiveSessionState(const CalcSessionState& state);
bool LookupLiveSessionState(const std::string& calcSessionIdUtf8, CalcSessionState& outState);
void UnregisterLiveSessionState(const std::string& calcSessionIdUtf8);

// ---- 3. Local Zero-Network Bidirectional IPC (CalcIpcChannel) ----

enum class CalcIpcRole : std::uint8_t {
    CalculatorHost = 0, // CalculadoraUnificada
    DocumentStudio = 1  // DocumentStudio
};

enum class CalcIpcMessageType : std::uint8_t {
    SessionBind      = 0,
    SessionStateSync = 1,
    KeystrokeReplay  = 2,
    LcdFrameCapture  = 3,
    ExpressionPush   = 4,
    Ack              = 5,
    Error            = 6
};

struct CalcIpcConfig {
    std::string channelNameUtf8 = "default";
    std::string directoryOverrideUtf8; // Isolated temp dir for tests; empty = OS local runtime dir
    CalcIpcRole role = CalcIpcRole::DocumentStudio;
    bool enableNativePipe = false;     // Optional Win32 Named Pipe / POSIX AF_UNIX stream signal
};

struct CalcIpcMessage {
    std::uint64_t seq = 0;
    std::uint64_t timestampUtcMs = 0;
    CalcIpcRole sender = CalcIpcRole::CalculatorHost;
    CalcIpcMessageType type = CalcIpcMessageType::SessionStateSync;
    std::string sessionIdUtf8;
    std::string modelIdUtf8;
    std::string keySeqUtf8;
    std::string expressionUtf8;
    std::string resultUtf8;
    std::string payloadJsonUtf8;
};

class CalcIpcChannel {
public:
    CalcIpcChannel();
    ~CalcIpcChannel();
    CalcIpcChannel(const CalcIpcChannel&) = delete;
    CalcIpcChannel& operator=(const CalcIpcChannel&) = delete;

    bool Open(const CalcIpcConfig& cfg, std::string* outErr = nullptr);
    void Close();
    bool IsOpen() const noexcept;
    std::string ChannelDirectoryUtf8() const;
    std::string NativePipeNameUtf8() const;

    bool Send(const CalcIpcMessage& msg, std::string* outErr = nullptr);
    std::vector<CalcIpcMessage> Poll(std::size_t maxMessages = 64, std::string* outErr = nullptr);

    // High-level bidirectional helpers
    bool PublishSession(const CalcSessionState& state, std::string* outErr = nullptr);
    bool ReadSession(const std::string& sessionIdUtf8, CalcSessionState& outState, std::string* outErr = nullptr) const;
    std::vector<std::string> ListActiveSessions() const;
    bool PushLcdFrame(const std::string& sessionIdUtf8, const CalcDisplayFrame& frame, std::string* outErr = nullptr);
    bool RequestKeystrokeReplay(
        const std::string& sessionIdUtf8,
        const std::string& modelIdUtf8,
        const std::string& keySeqUtf8,
        const std::string& expressionUtf8 = "",
        std::string* outErr = nullptr);
    bool PullIntoCard(AnnotationObject& cardObj, const Cas::SymbolEnv& env = {}, std::string* outErr = nullptr) const;

    // Cross-platform Win32 Named Pipe / POSIX AF_UNIX local stream self-test / transport
    bool StartNativePipeServer(std::string* outErr = nullptr);
    bool SendNativePipeBytes(const std::string& payload, std::string* outErr = nullptr) const;
    bool RecvNativePipeBytes(std::string& outPayload, std::uint32_t timeoutMs = 500, std::string* outErr = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::string SerializeSessionStateToJson(const CalcSessionState& state);
bool DeserializeSessionStateFromJson(const std::string& jsonUtf8, CalcSessionState& outState, std::string* outErr = nullptr);

// ---- 4. Differential Model Comparison (F-206, F-248) ----

struct ModelComparisonEntry {
    CasioModel model = CasioModel::Fx991Cw;
    std::string modelIdUtf8;
    bool supported = true;
    std::string appModeUtf8;
    std::string keySequenceUtf8;
    std::string displayResultUtf8;
    double numericValue = 0.0;
    bool hasNumericValue = false;
    double deltaFromCas = 0.0;
    int displayDigits = 10;
    int internalDigits = 15;
    std::string roundingNoteUtf8;
    std::string diffNoteUtf8;
    std::vector<CasioKeyStep> steps;
};

struct ModelComparisonReport {
    bool ok = false;
    std::string expressionUtf8;
    std::string casReferenceResultUtf8;
    double casReferenceNumeric = 0.0;
    bool hasCasReferenceNumeric = false;
    bool allModelsSupported = true;
    bool allResultsAgreeWithinTol = true;
    double maxDeltaAcrossModels = 0.0;
    bool hasKeySequenceDifferences = false;
    std::string diffSummaryUtf8;
    std::vector<ModelComparisonEntry> models; // Ordered: fx-570LA X, fx-570VN PLUS, fx-CG50, fx-991CW
};

ModelComparisonReport CompareCalculatorModels(
    const std::string& exprUtf8,
    const Cas::SymbolEnv& env = {},
    const std::vector<std::string>& modelIds = {});

// ---- 5. KeyLog Presenter Controller Model (F-205, F-207) ----

struct KeyLogPresenterState {
    std::string calcSessionIdUtf8;
    std::string modelIdUtf8;
    std::size_t cursorIndex = 0;
    bool playing = false;
    double speedMultiplier = 1.0; // 0.5, 1.0, 2.0
    bool presenterCleanMode = false;
    std::vector<CasioKeyStep> steps;
    std::vector<CalcDisplayFrame> frames;
};

KeyLogPresenterState BuildKeyLogPresenterFromSession(const CalcSessionState& session);
KeyLogPresenterState BuildKeyLogPresenterFromPlan(const CasioKeystrokePlan& plan);
bool StepPresenterFirst(KeyLogPresenterState& st) noexcept;
bool StepPresenterPrev(KeyLogPresenterState& st) noexcept;
bool StepPresenterNext(KeyLogPresenterState& st) noexcept;
bool StepPresenterLast(KeyLogPresenterState& st) noexcept;
bool JumpPresenterToStep(KeyLogPresenterState& st, std::size_t stepIdx) noexcept;
const CasioKeyStep* CurrentPresenterStep(const KeyLogPresenterState& st) noexcept;
const CalcDisplayFrame* CurrentPresenterFrame(const KeyLogPresenterState& st) noexcept;
std::string FormatPresenterStepCalloutUtf8(const KeyLogPresenterState& st);

} // namespace DocStudio::CalcBridge
