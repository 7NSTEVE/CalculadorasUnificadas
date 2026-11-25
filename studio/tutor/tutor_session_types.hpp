#pragma once

#include "../core/doc_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio::Tutor {

inline constexpr const char* kTutorProtocolVersion = "cuadernu.tutor.v1.1";

enum class TutorState : std::uint8_t {
    Idle = 0,
    CalibratingAec = 1,
    Listening = 2,
    UserSpeaking = 3,
    EvaluatingTurn = 4,
    ThinkingVerifying = 5,
    SpeakingAndDrawing = 6,
    Interrupted = 7,
    WaitingForStudent = 8,
    Rewinding = 9
};

struct TutorPlacement {
    std::string zone = "derivation_flow"; // header|derivation_flow|visual_lab|formula_dock|clarification_sidecar|student_workspace|media_layer
    std::string relation = "auto_next";   // auto_next|below|right_of|left_of|above|between|callout_to|overlay_on
    std::string targetId;
    std::string beforeTargetId;           // used when relation == "between" (in-place reflow)
    double gapPx = 24.0;
};

struct TutorAnimationSpec {
    std::string style = "handwriting_svg"; // handwriting_svg|fade_in|draw_stroke|pulse|camera_glide|spring_reflow|instant
    int durationMs = 900;
    std::string cueId;
    int offsetMs = 0;
};

struct TutorCanvasAction {
    std::string actionId;
    std::string type;
    std::string objectId;
    TutorPlacement placement{};
    TutorAnimationSpec animation{};
    std::string colorToken = "ink_primary";
    std::string payloadJsonUtf8 = "{}";
};

struct TutorProvenanceEdge {
    std::string targetRef;
    std::string sourceRef;
    std::string ruleEs;
};

struct TutorVerificationInfo {
    std::string status = "conceptual"; // sympy_verified|cas_verified|conceptual|unverified_warning
    std::string sympyRule;
    std::string dimensionalCheck;
    std::string sourceDocName;
    int sourcePage = 0;
    std::string sourceExcerpt;
};

struct TutorOriginGlossaryItem {
    std::string symbol;
    std::string meaning;
    std::string origin;
};

struct TutorPedagogyMeta {
    std::string microGoal;
    std::string conceptTag;
    bool waitForStudent = false;
    std::string checkQuestion;
    std::string expectedAnswerExpr;
    std::string cognitiveLoad = "low"; // low|medium
    std::vector<std::string> suggestedFollowups;
    std::vector<TutorOriginGlossaryItem> originGlossary;
};

struct TutorBeat {
    std::string protocol = kTutorProtocolVersion;
    std::string sessionId;
    std::string turnId;
    std::string beatId;
    int stepIndex = 0;
    std::string checkpointId;
    std::string branchId;
    std::string speechMarkupUtf8;
    double speechRate = 0.96;
    std::vector<TutorCanvasAction> actions;
    std::vector<TutorProvenanceEdge> provenance;
    TutorVerificationInfo verification{};
    TutorPedagogyMeta pedagogy{};
};

struct TutorInterruptSnapshot {
    std::string beatId;
    std::uint64_t playbackMs = 0;
    std::size_t lastWordIndex = 0;
    std::vector<std::string> completedActionIds;
    std::vector<std::string> cancelledActionIds;
    std::string activeCheckpointId;
};

struct TutorSceneEntry {
    std::string objectId;
    std::string actionType;
    int stepIndex = 0;
    std::string checkpointId;
    std::string branchId;
    std::string zone;
    std::string latexOrTextUtf8;
    std::string stepLabelUtf8;
    std::string payloadJsonUtf8;
    bool dimmed = false;
};

struct TutorValidationResult {
    bool ok = false;
    std::vector<std::string> errors;
};

} // namespace DocStudio::Tutor
