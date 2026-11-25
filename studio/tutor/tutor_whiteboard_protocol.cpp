#include "tutor_whiteboard_protocol.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace DocStudio::Tutor {

using nlohmann::json;

namespace {

bool IsValidBeatId(const std::string& id) {
    if (id.size() != 9 || id.rfind("beat_", 0) != 0) return false;
    for (std::size_t i = 5; i < 9; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(id[i]))) return false;
    }
    return true;
}

} // namespace

bool IsAllowedActionType(const std::string& t) noexcept {
    static const std::unordered_set<std::string> kAllowed = {
        "write_math",
        "write_text",
        "write_structured_notes",
        "annotate_step",
        "highlight_term",
        "underline_term",
        "circle_term",
        "colorize_term",
        "point_at_term",
        "write_margin_note",
        "cancel_terms",
        "trace_provenance",
        "draw_arrow",
        "trace_origin",
        "expand_intermediate_steps",
        "draw_diagram",
        "draw_sketch",
        "plot_function",
        "plot_riemann_sum",
        "plot_pv_diagram",
        "draw_thermal_shell",
        "focus_camera",
        "dim_steps",
        "create_checkpoint",
        "rewind_to_checkpoint",
        "open_clarification_branch",
        "show_student_prompt",
        "verify_student_ink",
        "paste_image_clip",
        "paste_problem_clip",
        "render_pdf_audit",
        "draw_inline_graph",
        "embed_pdf_page",
        "student_note",
        "sync_calculator_state"
    };
    return kAllowed.find(t) != kAllowed.end();
}

bool IsAllowedZone(const std::string& z) noexcept {
    static const std::unordered_set<std::string> kZones = {
        "header",
        "derivation_flow",
        "visual_lab",
        "formula_dock",
        "clarification_sidecar",
        "student_workspace",
        "media_layer"
    };
    return kZones.find(z) != kZones.end();
}

bool IsAllowedRelation(const std::string& r) noexcept {
    static const std::unordered_set<std::string> kRels = {
        "auto_next",
        "below",
        "right_of",
        "left_of",
        "above",
        "between",
        "callout_to",
        "overlay_on"
    };
    return kRels.find(r) != kRels.end();
}

std::string StripSpeechCueMarkup(const std::string& speechMarkupUtf8) {
    std::string out;
    out.reserve(speechMarkupUtf8.size());
    std::size_t i = 0;
    while (i < speechMarkupUtf8.size()) {
        if (speechMarkupUtf8[i] == '<' && speechMarkupUtf8.compare(i, 4, "<cue") == 0) {
            const auto close = speechMarkupUtf8.find('>', i);
            if (close != std::string::npos) {
                i = close + 1;
                continue;
            }
        }
        out.push_back(speechMarkupUtf8[i]);
        ++i;
    }
    // Collapse double spaces left around stripped cue markers
    std::string clean;
    clean.reserve(out.size());
    bool prevSpace = false;
    for (char ch : out) {
        const bool isSp = (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
        if (isSp) {
            if (!prevSpace && !clean.empty()) clean.push_back(' ');
            prevSpace = true;
        } else {
            clean.push_back(ch);
            prevSpace = false;
        }
    }
    if (!clean.empty() && clean.back() == ' ') clean.pop_back();
    return clean;
}

std::string TruncateSpokenTextByWordIndex(const std::string& plainSpeechUtf8, std::size_t lastWordIndex) {
    if (lastWordIndex == 0 || plainSpeechUtf8.empty()) return {};
    std::istringstream iss(plainSpeechUtf8);
    std::string word;
    std::ostringstream oss;
    std::size_t count = 0;
    while (iss >> word && count < lastWordIndex) {
        if (count > 0) oss << ' ';
        oss << word;
        ++count;
    }
    return oss.str();
}

bool ParseAndValidateTutorBeat(
    const std::string& rawJsonUtf8,
    TutorBeat& outBeat,
    TutorValidationResult& outValidation)
{
    outValidation = {};
    outBeat = {};

    json j;
    try {
        j = json::parse(rawJsonUtf8);
    } catch (const std::exception& ex) {
        outValidation.errors.push_back(std::string("JSON parse error: ") + ex.what());
        return false;
    }
    if (!j.is_object()) {
        outValidation.errors.push_back("Root payload must be a JSON object");
        return false;
    }

    const std::string proto = j.value("protocol", "");
    if (proto != "cuadernu.tutor.v1.1" && proto != "cuadernu.tutor.v1") {
        outValidation.errors.push_back("Unsupported protocol: '" + proto + "'");
    }
    outBeat.protocol = proto;

    outBeat.sessionId = j.value("session_id", "");
    if (outBeat.sessionId.size() < 4) {
        outValidation.errors.push_back("session_id must be at least 4 characters");
    }
    outBeat.turnId = j.value("turn_id", "");
    if (outBeat.turnId.empty()) {
        outValidation.errors.push_back("turn_id is required");
    }
    outBeat.beatId = j.value("beat_id", "");
    if (!IsValidBeatId(outBeat.beatId)) {
        outValidation.errors.push_back("beat_id must match pattern beat_0000");
    }
    if (!j.contains("step_index") || !j["step_index"].is_number_integer() || j["step_index"].get<int>() < 0) {
        outValidation.errors.push_back("step_index must be a non-negative integer");
    } else {
        outBeat.stepIndex = j["step_index"].get<int>();
    }

    outBeat.checkpointId = j.value("checkpoint_id", "");
    if (j.contains("branch_id") && j["branch_id"].is_string()) {
        outBeat.branchId = j["branch_id"].get<std::string>();
    }
    if (!j.contains("speech_markup") || !j["speech_markup"].is_string()) {
        outValidation.errors.push_back("speech_markup string is required");
    } else {
        outBeat.speechMarkupUtf8 = j["speech_markup"].get<std::string>();
    }
    outBeat.speechRate = std::clamp(j.value("speech_rate", 0.96), 0.75, 1.35);

    if (!j.contains("actions") || !j["actions"].is_array()) {
        outValidation.errors.push_back("actions array is required");
    } else {
        const auto& arr = j["actions"];
        if (arr.size() > 8) {
            outValidation.errors.push_back("actions cannot exceed 8 items per beat");
        }
        std::unordered_set<std::string> seenActionIds;
        for (std::size_t idx = 0; idx < arr.size(); ++idx) {
            const auto& item = arr[idx];
            if (!item.is_object()) {
                outValidation.errors.push_back("action[" + std::to_string(idx) + "] must be an object");
                continue;
            }
            TutorCanvasAction act;
            act.actionId = item.value("action_id", "");
            act.type = item.value("type", "");
            act.objectId = item.value("object_id", "");
            act.colorToken = item.value("color_token", "ink_primary");

            if (act.actionId.empty()) {
                outValidation.errors.push_back("action[" + std::to_string(idx) + "] missing action_id");
            } else if (!seenActionIds.insert(act.actionId).second) {
                outValidation.errors.push_back("duplicate action_id: " + act.actionId);
            }
            if (!IsAllowedActionType(act.type)) {
                outValidation.errors.push_back("unknown action type: '" + act.type + "'");
            }
            if (item.contains("placement") && item["placement"].is_object()) {
                const auto& pl = item["placement"];
                act.placement.zone = pl.value("zone", "derivation_flow");
                act.placement.relation = pl.value("relation", "auto_next");
                if (pl.contains("target_id") && pl["target_id"].is_string()) {
                    act.placement.targetId = pl["target_id"].get<std::string>();
                }
                if (pl.contains("before_target_id") && pl["before_target_id"].is_string()) {
                    act.placement.beforeTargetId = pl["before_target_id"].get<std::string>();
                }
                act.placement.gapPx = std::clamp(pl.value("gap_px", 24.0), 4.0, 120.0);

                if (!IsAllowedZone(act.placement.zone)) {
                    outValidation.errors.push_back("invalid placement.zone: " + act.placement.zone);
                }
                if (!IsAllowedRelation(act.placement.relation)) {
                    outValidation.errors.push_back("invalid placement.relation: " + act.placement.relation);
                }
                if (!act.objectId.empty() && act.placement.targetId == act.objectId) {
                    outValidation.errors.push_back("cyclic self-anchor on object_id: " + act.objectId);
                }
            }
            if (item.contains("animation") && item["animation"].is_object()) {
                const auto& an = item["animation"];
                act.animation.style = an.value("style", "handwriting_svg");
                act.animation.durationMs = std::clamp(an.value("duration_ms", 900), 0, 6000);
                if (an.contains("cue_id") && an["cue_id"].is_string()) {
                    act.animation.cueId = an["cue_id"].get<std::string>();
                }
                act.animation.offsetMs = an.value("offset_ms", 0);
            }
            if (item.contains("payload") && item["payload"].is_object()) {
                act.payloadJsonUtf8 = item["payload"].dump();
            }
            outBeat.actions.push_back(std::move(act));
        }
    }

    if (j.contains("provenance") && j["provenance"].is_array()) {
        for (const auto& pe : j["provenance"]) {
            if (!pe.is_object()) continue;
            TutorProvenanceEdge edge;
            edge.targetRef = pe.value("target_ref", "");
            edge.sourceRef = pe.value("source_ref", "");
            edge.ruleEs = pe.value("rule_es", "");
            if (!edge.targetRef.empty() && !edge.sourceRef.empty()) {
                outBeat.provenance.push_back(std::move(edge));
            }
        }
    }

    if (j.contains("verification") && j["verification"].is_object()) {
        const auto& v = j["verification"];
        outBeat.verification.status = v.value("status", "conceptual");
        if (v.contains("sympy_rule") && v["sympy_rule"].is_string()) {
            outBeat.verification.sympyRule = v["sympy_rule"].get<std::string>();
        }
        if (v.contains("dimensional_check") && v["dimensional_check"].is_string()) {
            outBeat.verification.dimensionalCheck = v["dimensional_check"].get<std::string>();
        }
        if (v.contains("source_citation") && v["source_citation"].is_object()) {
            const auto& sc = v["source_citation"];
            outBeat.verification.sourceDocName = sc.value("doc_name", "");
            outBeat.verification.sourcePage = sc.value("page", 0);
            outBeat.verification.sourceExcerpt = sc.value("excerpt", "");
        }
    }

    if (!j.contains("pedagogy") || !j["pedagogy"].is_object()) {
        outValidation.errors.push_back("pedagogy object is required");
    } else {
        const auto& ped = j["pedagogy"];
        outBeat.pedagogy.microGoal = ped.value("micro_goal", "");
        outBeat.pedagogy.conceptTag = ped.value("concept_tag", "");
        outBeat.pedagogy.waitForStudent = ped.value("wait_for_student", false);
        if (ped.contains("check_question") && ped["check_question"].is_string()) {
            outBeat.pedagogy.checkQuestion = ped["check_question"].get<std::string>();
        }
        if (ped.contains("expected_answer_expr") && ped["expected_answer_expr"].is_string()) {
            outBeat.pedagogy.expectedAnswerExpr = ped["expected_answer_expr"].get<std::string>();
        }
        outBeat.pedagogy.cognitiveLoad = ped.value("cognitive_load", "low");
        if (ped.contains("suggested_followups") && ped["suggested_followups"].is_array()) {
            for (const auto& f : ped["suggested_followups"]) {
                if (f.is_string()) outBeat.pedagogy.suggestedFollowups.push_back(f.get<std::string>());
            }
        }
        if (ped.contains("origin_glossary") && ped["origin_glossary"].is_array()) {
            for (const auto& g : ped["origin_glossary"]) {
                if (!g.is_object()) continue;
                TutorOriginGlossaryItem item;
                item.symbol = g.value("symbol", "");
                item.meaning = g.value("meaning", "");
                item.origin = g.value("origin", "");
                if (!item.symbol.empty()) outBeat.pedagogy.originGlossary.push_back(std::move(item));
            }
        }
    }

    outValidation.ok = outValidation.errors.empty();
    return outValidation.ok;
}

std::string SerializeTutorBeatToJson(const TutorBeat& beat) {
    json j;
    j["protocol"] = beat.protocol.empty() ? kTutorProtocolVersion : beat.protocol;
    j["session_id"] = beat.sessionId;
    j["turn_id"] = beat.turnId;
    j["beat_id"] = beat.beatId;
    j["step_index"] = beat.stepIndex;
    j["checkpoint_id"] = beat.checkpointId;
    if (beat.branchId.empty()) j["branch_id"] = nullptr;
    else j["branch_id"] = beat.branchId;
    j["speech_markup"] = beat.speechMarkupUtf8;
    j["speech_rate"] = beat.speechRate;

    json acts = json::array();
    for (const auto& a : beat.actions) {
        json aj;
        aj["action_id"] = a.actionId;
        aj["type"] = a.type;
        if (!a.objectId.empty()) aj["object_id"] = a.objectId;
        aj["color_token"] = a.colorToken;
        aj["placement"] = {
            {"zone", a.placement.zone},
            {"relation", a.placement.relation},
            {"target_id", a.placement.targetId.empty() ? json(nullptr) : json(a.placement.targetId)},
            {"before_target_id", a.placement.beforeTargetId.empty() ? json(nullptr) : json(a.placement.beforeTargetId)},
            {"gap_px", a.placement.gapPx}
        };
        aj["animation"] = {
            {"style", a.animation.style},
            {"duration_ms", a.animation.durationMs},
            {"cue_id", a.animation.cueId.empty() ? json(nullptr) : json(a.animation.cueId)},
            {"offset_ms", a.animation.offsetMs}
        };
        try {
            aj["payload"] = json::parse(a.payloadJsonUtf8.empty() ? "{}" : a.payloadJsonUtf8);
        } catch (...) {
            aj["payload"] = json::object();
        }
        acts.push_back(std::move(aj));
    }
    j["actions"] = std::move(acts);

    json prov = json::array();
    for (const auto& pe : beat.provenance) {
        prov.push_back({{"target_ref", pe.targetRef}, {"source_ref", pe.sourceRef}, {"rule_es", pe.ruleEs}});
    }
    j["provenance"] = std::move(prov);

    j["verification"] = {
        {"status", beat.verification.status},
        {"sympy_rule", beat.verification.sympyRule.empty() ? json(nullptr) : json(beat.verification.sympyRule)},
        {"dimensional_check", beat.verification.dimensionalCheck.empty() ? json(nullptr) : json(beat.verification.dimensionalCheck)}
    };
    if (!beat.verification.sourceDocName.empty()) {
        j["verification"]["source_citation"] = {
            {"doc_name", beat.verification.sourceDocName},
            {"page", beat.verification.sourcePage},
            {"excerpt", beat.verification.sourceExcerpt}
        };
    } else {
        j["verification"]["source_citation"] = nullptr;
    }

    json gloss = json::array();
    for (const auto& g : beat.pedagogy.originGlossary) {
        gloss.push_back({{"symbol", g.symbol}, {"meaning", g.meaning}, {"origin", g.origin}});
    }
    j["pedagogy"] = {
        {"micro_goal", beat.pedagogy.microGoal},
        {"concept_tag", beat.pedagogy.conceptTag},
        {"wait_for_student", beat.pedagogy.waitForStudent},
        {"check_question", beat.pedagogy.checkQuestion.empty() ? json(nullptr) : json(beat.pedagogy.checkQuestion)},
        {"expected_answer_expr", beat.pedagogy.expectedAnswerExpr.empty() ? json(nullptr) : json(beat.pedagogy.expectedAnswerExpr)},
        {"cognitive_load", beat.pedagogy.cognitiveLoad},
        {"suggested_followups", beat.pedagogy.suggestedFollowups},
        {"origin_glossary", std::move(gloss)}
    };
    return j.dump();
}

void TutorSessionStateMachine::Reset(std::string sessionIdUtf8) {
    sessionId_ = std::move(sessionIdUtf8);
    state_ = TutorState::Idle;
    activeCheckpointId_.clear();
    scene_.clear();
    objectIdToIndex_.clear();
    provenanceEdges_.clear();
    beats_.clear();
    transcriptHistory_.clear();
}

bool TutorSessionStateMachine::ApplyBeat(const TutorBeat& beat, std::string* outErr) {
    if (beat.beatId.empty()) {
        if (outErr) *outErr = "Beat missing beatId";
        return false;
    }
    if (sessionId_.empty()) sessionId_ = beat.sessionId;
    if (!beat.checkpointId.empty()) activeCheckpointId_ = beat.checkpointId;

    for (const auto& act : beat.actions) {
        if (act.type == "rewind_to_checkpoint") {
            try {
                const auto p = json::parse(act.payloadJsonUtf8);
                const std::string cp = p.value("checkpoint_id", "");
                if (!cp.empty()) RewindToCheckpoint(cp);
            } catch (...) {}
            continue;
        }
        if (act.objectId.empty()) continue;

        TutorSceneEntry entry;
        entry.objectId = act.objectId;
        entry.actionType = act.type;
        entry.stepIndex = beat.stepIndex;
        entry.checkpointId = beat.checkpointId;
        entry.branchId = beat.branchId;
        entry.zone = act.placement.zone;
        entry.payloadJsonUtf8 = act.payloadJsonUtf8;
        entry.dimmed = false;

        try {
            const auto p = json::parse(act.payloadJsonUtf8);
            if (p.contains("latex") && p["latex"].is_string()) {
                entry.latexOrTextUtf8 = p["latex"].get<std::string>();
            } else if (p.contains("text") && p["text"].is_string()) {
                entry.latexOrTextUtf8 = p["text"].get<std::string>();
            }
            if (p.contains("step_label") && p["step_label"].is_string()) {
                entry.stepLabelUtf8 = p["step_label"].get<std::string>();
            }
        } catch (...) {}

        auto it = objectIdToIndex_.find(entry.objectId);
        if (it != objectIdToIndex_.end()) {
            scene_[it->second] = std::move(entry);
        } else {
            objectIdToIndex_[entry.objectId] = scene_.size();
            scene_.push_back(std::move(entry));
        }
    }

    for (const auto& pe : beat.provenance) {
        provenanceEdges_.push_back(pe);
    }

    const std::string cleanSpeech = StripSpeechCueMarkup(beat.speechMarkupUtf8);
    if (!cleanSpeech.empty()) {
        transcriptHistory_.push_back(cleanSpeech);
    }
    beats_.push_back(beat);
    state_ = beat.pedagogy.waitForStudent ? TutorState::WaitingForStudent : TutorState::SpeakingAndDrawing;
    return true;
}

std::string TutorSessionStateMachine::CommitInterruption(const TutorInterruptSnapshot& snap) {
    state_ = TutorState::Interrupted;
    if (!snap.activeCheckpointId.empty()) {
        activeCheckpointId_ = snap.activeCheckpointId;
    }

    std::string truncated;
    for (auto it = beats_.rbegin(); it != beats_.rend(); ++it) {
        if (it->beatId == snap.beatId) {
            const std::string plain = StripSpeechCueMarkup(it->speechMarkupUtf8);
            truncated = TruncateSpokenTextByWordIndex(plain, snap.lastWordIndex);
            if (!transcriptHistory_.empty()) {
                transcriptHistory_.back() = truncated.empty()
                    ? "[interrumpido antes de hablar]"
                    : (truncated + " [interrumpido]");
            }
            break;
        }
    }

    // Remove only actions explicitly reported as cancelled before completion; never erase prior steps.
    if (!snap.cancelledActionIds.empty() && !beats_.empty()) {
        std::unordered_set<std::string> cancelled(snap.cancelledActionIds.begin(), snap.cancelledActionIds.end());
        std::unordered_set<std::string> objsToDrop;
        for (const auto& b : beats_) {
            if (b.beatId != snap.beatId) continue;
            for (const auto& a : b.actions) {
                if (cancelled.count(a.actionId) && !a.objectId.empty()) {
                    objsToDrop.insert(a.objectId);
                }
            }
        }
        if (!objsToDrop.empty()) {
            std::vector<TutorSceneEntry> kept;
            kept.reserve(scene_.size());
            objectIdToIndex_.clear();
            for (auto& e : scene_) {
                if (objsToDrop.count(e.objectId)) continue;
                objectIdToIndex_[e.objectId] = kept.size();
                kept.push_back(std::move(e));
            }
            scene_ = std::move(kept);
        }
    }
    return truncated;
}

bool TutorSessionStateMachine::RewindToCheckpoint(const std::string& checkpointId) {
    int targetStep = -1;
    for (const auto& e : scene_) {
        if (e.checkpointId == checkpointId) {
            targetStep = (std::max)(targetStep, e.stepIndex);
        }
    }
    if (targetStep < 0) return false;
    activeCheckpointId_ = checkpointId;
    state_ = TutorState::Rewinding;
    for (auto& e : scene_) {
        e.dimmed = (e.stepIndex > targetStep);
    }
    return true;
}

std::vector<TutorProvenanceEdge> TutorSessionStateMachine::FindOriginChain(
    const std::string& targetRef,
    std::size_t maxDepth) const
{
    std::vector<TutorProvenanceEdge> chain;
    std::unordered_set<std::string> visited;
    std::string cur = targetRef;
    for (std::size_t depth = 0; depth < maxDepth && !cur.empty(); ++depth) {
        if (!visited.insert(cur).second) break;
        bool found = false;
        for (auto it = provenanceEdges_.rbegin(); it != provenanceEdges_.rend(); ++it) {
            if (it->targetRef == cur) {
                chain.push_back(*it);
                cur = it->sourceRef;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    return chain;
}

std::string TutorSessionStateMachine::ExportStateJson() const {
    json j;
    j["protocol"] = kTutorProtocolVersion;
    j["session_id"] = sessionId_;
    j["state"] = static_cast<int>(state_);
    j["active_checkpoint_id"] = activeCheckpointId_;
    j["beat_count"] = beats_.size();
    json objs = json::array();
    for (const auto& e : scene_) {
        objs.push_back({
            {"object_id", e.objectId},
            {"type", e.actionType},
            {"step_index", e.stepIndex},
            {"checkpoint_id", e.checkpointId},
            {"branch_id", e.branchId},
            {"zone", e.zone},
            {"text", e.latexOrTextUtf8},
            {"step_label", e.stepLabelUtf8},
            {"dimmed", e.dimmed}
        });
    }
    j["scene"] = std::move(objs);
    j["transcript_history"] = transcriptHistory_;
    return j.dump();
}

} // namespace DocStudio::Tutor
