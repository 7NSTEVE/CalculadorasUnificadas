#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"
#include "../tutor/tutor_document_baker.hpp"
#include "../tutor/tutor_whiteboard_protocol.hpp"

#include <iostream>
#include <string>

using namespace DocStudio;
using namespace DocStudio::Tutor;

namespace {

int gFailures = 0;

void Check(bool cond, const char* label) {
    if (!cond) {
        std::cerr << "[FAIL] " << label << "\n";
        ++gFailures;
    } else {
        std::cout << "[PASS] " << label << "\n";
    }
}

} // namespace

int main() {
    // 1. Positive parse & validate for cuadernu.tutor.v1.1 beat with provenance + widgets + Casio card
    const std::string validBeatJson = R"JSON({
      "protocol": "cuadernu.tutor.v1.1",
      "session_id": "ses_test_001",
      "turn_id": "turn_01",
      "beat_id": "beat_0001",
      "step_index": 1,
      "checkpoint_id": "cp_step1",
      "branch_id": null,
      "speech_markup": "Primero <cue id='c1'/> sacamos la constante tres y luego <cue id='c2'/> aplicamos potencia.",
      "speech_rate": 0.95,
      "actions": [
        {
          "action_id": "act_1",
          "type": "write_math",
          "object_id": "eq_1",
          "placement": { "zone": "derivation_flow", "relation": "auto_next", "gap_px": 24 },
          "animation": { "style": "handwriting_svg", "duration_ms": 1100, "cue_id": "c1" },
          "color_token": "ink_primary",
          "payload": { "latex": "W = nRT \\int_{V_i}^{V_f} \\frac{1}{V}\\,dV", "step_label": "Paso 1" }
        },
        {
          "action_id": "act_2",
          "type": "plot_riemann_sum",
          "object_id": "widget_1",
          "placement": { "zone": "visual_lab", "relation": "auto_next", "gap_px": 20 },
          "animation": { "style": "fade_in", "duration_ms": 600, "cue_id": "c2" },
          "payload": { "expr": "2 + 4*x", "a": 1, "b": 5, "exact_value": 56 }
        },
        {
          "action_id": "act_3",
          "type": "sync_calculator_state",
          "object_id": "casio_1",
          "placement": { "zone": "formula_dock", "relation": "auto_next", "gap_px": 16 },
          "payload": { "model": "fx-CG50", "keys": ["OPTN", "F4", "EXE"], "expected_result": "56" }
        }
      ],
      "provenance": [
        { "target_ref": "eq_1:term-const", "source_ref": "pdf:Formulario:p2", "rule_es": "Gas ideal PV = nRT" }
      ],
      "verification": {
        "status": "sympy_verified",
        "sympy_rule": "ConstantTimesRule -> ReciprocalRule",
        "dimensional_check": "J [OK]"
      },
      "pedagogy": {
        "micro_goal": "Sacar nRT fuera de la integral",
        "concept_tag": "phys3.thermo.isothermal",
        "wait_for_student": true,
        "check_question": "¿Por qué sale nRT?",
        "cognitive_load": "low",
        "suggested_followups": ["¿De dónde salió nRT?", "Pasos intermedios"]
      }
    })JSON";

    TutorBeat beat1;
    TutorValidationResult val1;
    Check(ParseAndValidateTutorBeat(validBeatJson, beat1, val1) && val1.ok, "Valid v1.1 TutorBeat parses cleanly");
    Check(beat1.actions.size() == 3, "Parsed 3 canvas actions");
    Check(beat1.provenance.size() == 1, "Parsed provenance edge");

    // Round-trip serialization must remain valid
    TutorBeat roundTrip;
    TutorValidationResult valRt;
    Check(ParseAndValidateTutorBeat(SerializeTutorBeatToJson(beat1), roundTrip, valRt) && valRt.ok, "Round-trip JSON serialization validates");

    // 2. Cue stripping & exact barge-in word truncation
    const std::string plain = StripSpeechCueMarkup(beat1.speechMarkupUtf8);
    Check(plain == "Primero sacamos la constante tres y luego aplicamos potencia.", "StripSpeechCueMarkup cleans <cue> markers");
    Check(TruncateSpokenTextByWordIndex(plain, 5) == "Primero sacamos la constante tres", "TruncateSpokenTextByWordIndex matches exact heard prefix");

    // 3. Negative-path tests: invalid JSON, unknown action, cyclic self-anchor, bad beat_id
    TutorBeat badBeat;
    TutorValidationResult badVal;
    Check(!ParseAndValidateTutorBeat("{invalid json", badBeat, badVal) && !badVal.ok, "Rejects malformed JSON");

    const std::string unknownActionJson = R"JSON({
      "protocol": "cuadernu.tutor.v1.1",
      "session_id": "ses_bad_01",
      "turn_id": "t1",
      "beat_id": "beat_0002",
      "step_index": 2,
      "speech_markup": "hola",
      "actions": [{ "action_id": "a1", "type": "exec_shell_hack", "object_id": "x1", "placement": { "zone": "derivation_flow", "relation": "below", "target_id": "x1" } }],
      "pedagogy": { "micro_goal": "x", "wait_for_student": false, "cognitive_load": "low" }
    })JSON";
    Check(!ParseAndValidateTutorBeat(unknownActionJson, badBeat, badVal) && badVal.errors.size() >= 2,
          "Rejects unknown action type AND cyclic self-anchor");

    // 4. State machine: ApplyBeat, Provenance lookup, Interruption rollback without erasing prior steps, Rewind dim
    TutorSessionStateMachine sm;
    sm.Reset("ses_test_001");
    Check(sm.ApplyBeat(beat1), "StateMachine applies beat 1");
    Check(sm.State() == TutorState::WaitingForStudent, "State transitions to WaitingForStudent");

    const auto chain = sm.FindOriginChain("eq_1:term-const");
    Check(chain.size() == 1 && chain[0].sourceRef == "pdf:Formulario:p2", "FindOriginChain resolves provenance");

    // Apply beat 2 then interrupt mid-speech cancelling only its unrendered second action
    TutorBeat beat2 = beat1;
    beat2.beatId = "beat_0002";
    beat2.stepIndex = 2;
    beat2.checkpointId = "cp_step2";
    beat2.actions.resize(2);
    beat2.actions[0].actionId = "act_201";
    beat2.actions[0].objectId = "eq_2";
    beat2.actions[1].actionId = "act_202";
    beat2.actions[1].objectId = "eq_3_cancelled";
    beat2.pedagogy.waitForStudent = false;
    Check(sm.ApplyBeat(beat2), "StateMachine applies beat 2");

    TutorInterruptSnapshot snap;
    snap.beatId = "beat_0002";
    snap.playbackMs = 1950;
    snap.lastWordIndex = 4;
    snap.completedActionIds = {"act_201"};
    snap.cancelledActionIds = {"act_202"};
    snap.activeCheckpointId = "cp_step2";
    const std::string heard = sm.CommitInterruption(snap);
    Check(heard == "Primero sacamos la constante", "CommitInterruption returns exact heard prefix");
    Check(sm.State() == TutorState::Interrupted, "State transitions to Interrupted");
    Check(sm.SceneEntries().size() == 4, "Interruption drops cancelled object while preserving eq_1, widget_1, casio_1, eq_2");

    Check(sm.RewindToCheckpoint("cp_step1"), "RewindToCheckpoint succeeds for cp_step1");
    Check(sm.SceneEntries().back().dimmed && !sm.SceneEntries().front().dimmed, "Rewind dims later steps without erasing board");

    // 5. Atomic Document Baking + Undo/Redo + .cudoc sidecar extension
    // Reset rewind dim before baking all active steps
    Check(sm.RewindToCheckpoint("cp_step2"), "Restore cp_step2 active view");
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack stack;
    const auto bake = BakeTutorSessionToDocument(doc, stack, ObjectTarget{true, 0}, sm);
    Check(bake.ok && bake.bakedObjectsCount == 4 && bake.mathBlocksCount == 2 && bake.plotBlocksCount == 1 && bake.calcCardsCount == 1,
          "BakeTutorSessionToDocument inserts MathBlock, PlotBlock, CalcSessionCard atomically");
    Check(doc.FindExtensionPayload("tutor_session.json") != nullptr, "Persists tutor_session.json sidecar extension");
    Check(doc.CanvasObjects().size() == 4, "DocumentModel contains 4 baked objects");
    Check(stack.Undo(doc) && doc.CanvasObjects().empty(), "1-step Undo cleanly reverts entire baked session");
    Check(stack.Redo(doc) && doc.CanvasObjects().size() == 4, "1-step Redo cleanly restores baked session");

    std::cout << "TutorWhiteboardTest finished with " << gFailures << " failure(s).\n";
    return gFailures == 0 ? 0 : 1;
}
