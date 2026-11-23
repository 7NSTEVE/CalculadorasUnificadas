#include "cg50_trace.h"

#include <iomanip>
#include <sstream>

namespace cg50 {
namespace {

void AppendHex32(std::ostringstream& out, const char* name,
                 const std::uint32_t before, const std::uint32_t after)
{
    if (before == after) return;
    out << ' ' << name << '='
        << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << before
        << '>' << std::setw(8) << after;
}

void AppendCount(std::ostringstream& out, const char* name,
                 const std::uint64_t before, const std::uint64_t after)
{
    if (before == after) return;
    out << ' ' << name << '=' << std::dec << before << '>' << after;
}

void AppendPin(std::ostringstream& out, const char* name, const bool value)
{
    out << name << (value ? '1' : '0');
}

} // namespace

const char* TraceStateName(const Cg50NativeCore::State state) noexcept
{
    switch (state) {
    case Cg50NativeCore::State::Empty: return "EMPTY";
    case Cg50NativeCore::State::Running: return "RUN";
    case Cg50NativeCore::State::Sleeping: return "SLEEP";
    case Cg50NativeCore::State::Halted: return "HALT";
    }
    return "?";
}

TraceRecord TraceStep(Cg50NativeCore& core, const std::uint64_t ordinal)
{
    TraceRecord record{};
    record.ordinal = ordinal;
    record.stateBefore = core.GetState();
    record.regsBefore = core.GetRegisters();
    record.statsBefore = core.GetStats();
    record.lcdWritesBefore = core.Peripherals().Lcd().ObservedWrites().size();
    record.attempted = record.stateBefore == Cg50NativeCore::State::Running;

    if (record.attempted) record.completed = core.StepInstruction();

    record.stateAfter = core.GetState();
    record.regsAfter = core.GetRegisters();
    record.statsAfter = core.GetStats();
    const auto& lcdWrites = core.Peripherals().Lcd().ObservedWrites();
    record.lcdWritesAfter = lcdWrites.size();
    record.lcdWriteObserved = record.lcdWritesAfter > record.lcdWritesBefore;
    if (record.lcdWriteObserved)
        record.lastLcdWrite = lcdWrites.back();
    record.opcodeFetched = record.attempted &&
                           record.statsAfter.lastPc == record.regsBefore.pc;
    return record;
}

std::string FormatTraceRecord(const TraceRecord& record)
{
    std::ostringstream out;
    out << std::dec << std::setw(10) << std::setfill('0') << record.ordinal
        << ' ' << TraceStateName(record.stateBefore)
        << " PC=" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
        << record.regsBefore.pc;

    if (!record.attempted) {
        out << " WAIT -> " << TraceStateName(record.stateAfter)
            << " PC=" << std::setw(8) << record.regsAfter.pc;
        return out.str();
    }

    if (record.opcodeFetched)
        out << " OP=" << std::setw(4) << record.statsAfter.lastOpcode;
    else
        out << " OP=????";

    out << " -> " << TraceStateName(record.stateAfter)
        << " PC=" << std::setw(8) << record.regsAfter.pc;

    for (std::size_t i = 0; i < record.regsBefore.r.size(); ++i) {
        if (record.regsBefore.r[i] == record.regsAfter.r[i]) continue;
        out << " R" << std::dec << i << '='
            << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
            << record.regsBefore.r[i] << '>' << std::setw(8) << record.regsAfter.r[i];
    }

    AppendHex32(out, "PR", record.regsBefore.pr, record.regsAfter.pr);
    AppendHex32(out, "SR", record.regsBefore.sr, record.regsAfter.sr);
    AppendHex32(out, "GBR", record.regsBefore.gbr, record.regsAfter.gbr);
    AppendHex32(out, "VBR", record.regsBefore.vbr, record.regsAfter.vbr);
    AppendHex32(out, "MACH", record.regsBefore.mach, record.regsAfter.mach);
    AppendHex32(out, "MACL", record.regsBefore.macl, record.regsAfter.macl);
    AppendHex32(out, "SSR", record.regsBefore.ssr, record.regsAfter.ssr);
    AppendHex32(out, "SPC", record.regsBefore.spc, record.regsAfter.spc);
    AppendHex32(out, "SGR", record.regsBefore.sgr, record.regsAfter.sgr);
    AppendHex32(out, "DBR", record.regsBefore.dbr, record.regsAfter.dbr);
    AppendHex32(out, "TRA", record.regsBefore.tra, record.regsAfter.tra);
    AppendHex32(out, "EXPEVT", record.regsBefore.expevt, record.regsAfter.expevt);
    AppendHex32(out, "INTEVT", record.regsBefore.intevt, record.regsAfter.intevt);

    AppendCount(out, "INS", record.statsBefore.instructions, record.statsAfter.instructions);
    AppendCount(out, "CYC", record.statsBefore.cycles, record.statsAfter.cycles);

    if (record.lcdWriteObserved) {
        const auto& cycle = record.lastLcdWrite;
        out << " LCD16=" << std::hex << std::uppercase << std::setw(4)
            << std::setfill('0') << cycle.value << " PINS=";
        AppendPin(out, "CS", cycle.pins.chipSelect);
        out << ',';
        AppendPin(out, "RS", cycle.pins.registerSelect);
        out << ',';
        AppendPin(out, "RST", cycle.pins.resetX);
        out << ',';
        AppendPin(out, "IM1", cycle.pins.interfaceMode1);
        out << ',';
        AppendPin(out, "FM", cycle.pins.frameMark);
        out << ',';
        AppendPin(out, "PWR", cycle.pins.v28sys);
        out << ',';
        AppendPin(out, "BL", cycle.pins.backlightOn);
        out << ',';
        AppendPin(out, "BR", cycle.pins.backlightBrightness);
        if (record.lcdWritesAfter > record.lcdWritesBefore + 1)
            out << " LCDDELTA=" << std::dec
                << (record.lcdWritesAfter - record.lcdWritesBefore);
    }

    if (record.statsAfter.lastMemoryFaultAddress.updates !=
        record.statsBefore.lastMemoryFaultAddress.updates) {
        const auto faultAddress =
            static_cast<std::uint32_t>(record.statsAfter.lastMemoryFaultAddress);
        out << " MEM="
            << (record.statsAfter.lastMemoryFaultWasWrite ? 'W' : 'R')
            << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
            << faultAddress;
        const int logWindow = DescriptorLogErrorWindowIndex(faultAddress);
        if (logWindow >= 0)
            out << " LOGWIN=" << std::dec << logWindow;
    }
    if (record.statsAfter.unsupportedOpcode.updates !=
        record.statsBefore.unsupportedOpcode.updates) {
        out << " UNSUP=" << std::hex << std::uppercase << std::setw(8)
            << record.statsAfter.unsupportedPc << ':' << std::setw(4)
            << static_cast<std::uint16_t>(record.statsAfter.unsupportedOpcode);
    }
    if (record.statsAfter.exceptionsTaken != record.statsBefore.exceptionsTaken) {
        out << " EXC=" << std::hex << std::uppercase << std::setw(8)
            << record.statsAfter.lastExceptionEvent;
    }
    if (record.statsAfter.interruptsAccepted != record.statsBefore.interruptsAccepted) {
        out << " IRQ=" << std::hex << std::uppercase << std::setw(8)
            << record.statsAfter.lastInterruptEvent
            << "/P" << std::dec << static_cast<unsigned>(record.statsAfter.lastInterruptPriority);
    }

    if (!record.completed && record.stateAfter == Cg50NativeCore::State::Running)
        out << " STEP_FALSE";

    return out.str();
}

} // namespace cg50
