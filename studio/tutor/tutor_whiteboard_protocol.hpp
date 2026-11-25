#pragma once

#include "tutor_session_types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio::Tutor {

// Deterministic C++17 validator and parser for cuadernu.tutor.v1.1 beats.
bool ParseAndValidateTutorBeat(
    const std::string& rawJsonUtf8,
    TutorBeat& outBeat,
    TutorValidationResult& outValidation);

std::string SerializeTutorBeatToJson(const TutorBeat& beat);

// Removes <cue id='...'/> tags and returns clean speech text for TTS / transcript truncation.
std::string StripSpeechCueMarkup(const std::string& speechMarkupUtf8);

// Truncates UTF-8 spoken text strictly to the first wordCount words (for exact barge-in history commit).
std::string TruncateSpokenTextByWordIndex(const std::string& plainSpeechUtf8, std::size_t lastWordIndex);

bool IsAllowedActionType(const std::string& actionType) noexcept;
bool IsAllowedZone(const std::string& zone) noexcept;
bool IsAllowedRelation(const std::string& relation) noexcept;

// Maintains addressable scene graph, checkpoints, provenance links, and interruption state.
class TutorSessionStateMachine {
public:
    void Reset(std::string sessionIdUtf8 = "");

    const std::string& SessionId() const noexcept { return sessionId_; }
    TutorState State() const noexcept { return state_; }
    void TransitionTo(TutorState next) noexcept { state_ = next; }

    // Commits a validated TutorBeat into the scene graph (never erases prior steps).
    bool ApplyBeat(const TutorBeat& beat, std::string* outErr = nullptr);

    // Commits a barge-in interruption: freezes board, preserves all completed objects,
    // records truncated spoken transcript, and transitions state cleanly to Interrupted.
    std::string CommitInterruption(const TutorInterruptSnapshot& snap);

    // Rewinds visual focus to checkpointId: dims subsequent steps without deleting them.
    bool RewindToCheckpoint(const std::string& checkpointId);

    // Provenance trace lookup for "Where did this come from?" queries.
    std::vector<TutorProvenanceEdge> FindOriginChain(const std::string& targetRef, std::size_t maxDepth = 6) const;

    const std::vector<TutorSceneEntry>& SceneEntries() const noexcept { return scene_; }
    const std::vector<TutorBeat>& CommittedBeats() const noexcept { return beats_; }
    const std::vector<std::string>& TranscriptHistory() const noexcept { return transcriptHistory_; }
    const std::string& ActiveCheckpointId() const noexcept { return activeCheckpointId_; }

    std::string ExportStateJson() const;

private:
    std::string sessionId_;
    TutorState state_ = TutorState::Idle;
    std::string activeCheckpointId_;
    std::vector<TutorSceneEntry> scene_;
    std::unordered_map<std::string, std::size_t> objectIdToIndex_;
    std::vector<TutorProvenanceEdge> provenanceEdges_;
    std::vector<TutorBeat> beats_;
    std::vector<std::string> transcriptHistory_;
};

} // namespace DocStudio::Tutor
