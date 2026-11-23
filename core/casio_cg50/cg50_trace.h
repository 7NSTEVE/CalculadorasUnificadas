#pragma once

#include "cg50_native_core.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cg50 {

struct TraceRecord {
    std::uint64_t ordinal = 0;
    bool attempted = false;
    bool opcodeFetched = false;
    bool completed = false;
    Cg50NativeCore::State stateBefore = Cg50NativeCore::State::Empty;
    Cg50NativeCore::State stateAfter = Cg50NativeCore::State::Empty;
    Cg50NativeCore::Registers regsBefore{};
    Cg50NativeCore::Registers regsAfter{};
    Cg50NativeCore::Stats statsBefore{};
    Cg50NativeCore::Stats statsAfter{};

    // Device-side evidence produced by the instruction. The trace does not
    // interpret R61524 command/data semantics; it records the raw accepted cycle.
    std::size_t lcdWritesBefore = 0;
    std::size_t lcdWritesAfter = 0;
    bool lcdWriteObserved = false;
    R61524::BusCycle lastLcdWrite{};
};

// Executes at most one guest instruction and captures architectural state plus
// already-observed device-side evidence. It never mutates the core except through
// StepInstruction().
TraceRecord TraceStep(Cg50NativeCore& core, std::uint64_t ordinal);

// Stable ASCII line intended for boot logs, regression artifacts and diffing.
// Register fields are emitted only when their value changed. A failed instruction
// fetch is printed as OP=???? instead of recycling the previous opcode.
std::string FormatTraceRecord(const TraceRecord& record);

const char* TraceStateName(Cg50NativeCore::State state) noexcept;

} // namespace cg50
