#pragma once

#include "tutor_whiteboard_protocol.hpp"
#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"

#include <cstddef>
#include <string>

namespace DocStudio::Tutor {

struct TutorBakeReport {
    bool ok = false;
    std::size_t bakedObjectsCount = 0;
    std::size_t mathBlocksCount = 0;
    std::size_t textBlocksCount = 0;
    std::size_t plotBlocksCount = 0;
    std::size_t calcCardsCount = 0;
    std::string summaryUtf8;
};

// Converts all committed scene entries from a TutorSessionStateMachine into native
// CuadernU DocumentModel objects (MathBlock, TextBox, PlotBlock, CalcSessionCard, Shape::Arrow)
// inside a single atomic CommandStack transaction (1-click Undo/Redo) and persists the
// session snapshot into .cudoc sidecar `extensions/tutor_session.json`.
TutorBakeReport BakeTutorSessionToDocument(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const TutorSessionStateMachine& session,
    double originX = 72.0,
    double originY = 72.0);

} // namespace DocStudio::Tutor
