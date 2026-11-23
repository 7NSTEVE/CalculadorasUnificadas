#pragma once

#include "cg50_native_core.h"
#include "cg50_core/cg50_board_wiring.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace cg50 {

enum class StopKind : std::uint8_t {
    Empty,
    RunningBudget,
    Sleeping,
    MemoryFault,
    UnsupportedOpcode,
    OtherHalt
};

struct StopInfo {
    StopKind kind = StopKind::Empty;
    Cg50NativeCore::State state = Cg50NativeCore::State::Empty;
    std::uint32_t pc = 0;
    std::uint32_t address = 0;
    bool memoryWrite = false;
    int descriptorLogWindow = -1;
    std::uint32_t unsupportedPc = 0;
    std::uint16_t unsupportedOpcode = 0;
    std::uint64_t instructions = 0;
    std::uint64_t cycles = 0;
};

inline const char* StopKindName(const StopKind kind) noexcept
{
    switch (kind) {
    case StopKind::Empty: return "empty";
    case StopKind::RunningBudget: return "budget";
    case StopKind::Sleeping: return "sleep";
    case StopKind::MemoryFault: return "memory";
    case StopKind::UnsupportedOpcode: return "opcode";
    case StopKind::OtherHalt: return "other";
    }
    return "other";
}

inline StopInfo InspectStop(const Cg50NativeCore& core) noexcept
{
    StopInfo info{};
    const auto& regs = core.GetRegisters();
    const auto& stats = core.GetStats();

    info.state = core.GetState();
    info.pc = regs.pc;
    info.instructions = stats.instructions;
    info.cycles = stats.cycles;

    if (stats.lastMemoryFaultAddress.updates != 0) {
        info.kind = StopKind::MemoryFault;
        info.address = static_cast<std::uint32_t>(stats.lastMemoryFaultAddress);
        info.memoryWrite = stats.lastMemoryFaultWasWrite;
        info.descriptorLogWindow = DescriptorLogErrorWindowIndex(info.address);
        return info;
    }

    if (stats.unsupportedOpcode.updates != 0) {
        info.kind = StopKind::UnsupportedOpcode;
        info.unsupportedPc = stats.unsupportedPc;
        info.unsupportedOpcode = static_cast<std::uint16_t>(stats.unsupportedOpcode);
        return info;
    }

    switch (core.GetState()) {
    case Cg50NativeCore::State::Empty:
        info.kind = StopKind::Empty;
        break;
    case Cg50NativeCore::State::Running:
        info.kind = StopKind::RunningBudget;
        break;
    case Cg50NativeCore::State::Sleeping:
        info.kind = StopKind::Sleeping;
        break;
    case Cg50NativeCore::State::Halted:
        info.kind = StopKind::OtherHalt;
        break;
    }
    return info;
}

inline std::string FormatStopSummary(const StopInfo& info, const char* prefix)
{
    std::ostringstream out;
    out << prefix << " kind=" << StopKindName(info.kind);

    switch (info.kind) {
    case StopKind::MemoryFault:
        out << " access=" << (info.memoryWrite ? "WRITE" : "READ")
            << " address=0x" << std::hex << std::uppercase << std::setw(8)
            << std::setfill('0') << info.address;
        if (info.descriptorLogWindow >= 0)
            out << " logwin=" << std::dec << info.descriptorLogWindow;
        break;
    case StopKind::UnsupportedOpcode:
        out << " pc=0x" << std::hex << std::uppercase << std::setw(8)
            << std::setfill('0') << info.unsupportedPc
            << " op=0x" << std::setw(4) << info.unsupportedOpcode;
        break;
    case StopKind::Empty:
    case StopKind::RunningBudget:
    case StopKind::Sleeping:
    case StopKind::OtherHalt:
        out << " pc=0x" << std::hex << std::uppercase << std::setw(8)
            << std::setfill('0') << info.pc;
        break;
    }

    out << " instructions=" << std::dec << info.instructions
        << " cycles=" << info.cycles;
    return out.str();
}

inline std::string FormatStopSummary(const Cg50NativeCore& core, const char* prefix)
{
    return FormatStopSummary(InspectStop(core), prefix);
}

} // namespace cg50
