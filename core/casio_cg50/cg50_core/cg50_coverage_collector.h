#pragma once

#ifdef CG50_ENABLE_COVERAGE_COUNTERS

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace cg50 {

struct MmioCovEntry {
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
    std::uint32_t writeBitsSet = 0;
    std::uint32_t writeBitsClr = 0;
    std::uint32_t readBitsSet = 0;
    std::uint32_t readBitsClr = 0;
    std::uint8_t widthsMask = 0; // bit0=8b, bit1=16b, bit2=32b
};

struct CoverageCollector {
    std::array<std::uint64_t, 65536> opcode16Hits{};
    std::array<std::uint64_t, 256> dsp32Op2Hi8Hits{};
    std::unordered_map<std::uint32_t, MmioCovEntry> mmioHits;
    std::unordered_map<std::uint32_t, std::uint64_t> expevtHits;
    std::unordered_map<std::uint32_t, std::uint64_t> intevtHits;
    std::array<std::uint64_t, 4> cpuStateHits{};
    std::array<std::array<std::uint64_t, 4>, 4> cpuStateTransitions{};
    std::uint8_t lastCpuState = 0;
    std::unordered_map<std::uint32_t, std::uint64_t> flashCfiStates;
    std::unordered_map<std::uint32_t, std::uint64_t> flashCfiTransitions;
    std::array<std::uint64_t, 6> dmacChannelsHit{};
    std::unordered_map<std::string, std::uint64_t> boundaryHits;

    static constexpr bool IsMmioRegion(const std::uint32_t addr, const std::uint32_t phys) noexcept
    {
        if (addr >= 0xE0000000u) {
            // Exclude pure on-chip SRAM arrays (XRAM/YRAM/ILRAM, RS-RAM, DSP0/1 2MB SRAM)
            // so mmioHits tracks genuine MMIO control registers without 200K SRAM addresses.
            if (addr >= 0xE5000000u && addr < 0xE5300000u) return false;
            if (addr >= 0xFD800000u && addr < 0xFD804000u) return false;
            if (addr >= 0xFE200000u && addr < 0xFE400000u) return false;
            return true;
        }
        if (phys >= 0x04000000u && phys < 0x05000000u) return true;
        if (phys >= 0x14000000u && phys < 0x15000000u) return true;
        return false;
    }

    static constexpr std::uint32_t NormalizeMmioKey(const std::uint32_t addr,
                                                    const std::uint32_t phys) noexcept
    {
        if (addr >= 0xE0000000u) return addr;
        return phys;
    }

    static constexpr std::uint32_t WidthMask(const std::uint8_t widthBytes) noexcept
    {
        if (widthBytes == 1u) return 0x000000FFu;
        if (widthBytes == 2u) return 0x0000FFFFu;
        return 0xFFFFFFFFu;
    }

    static constexpr std::uint8_t WidthBit(const std::uint8_t widthBytes) noexcept
    {
        if (widthBytes == 1u) return 0x01u;
        if (widthBytes == 2u) return 0x02u;
        return 0x04u;
    }

    void RecordRead(const std::uint32_t addr, const std::uint32_t phys,
                    const std::uint32_t val, const std::uint8_t widthBytes)
    {
        if (!IsMmioRegion(addr, phys)) return;
        const std::uint32_t key = NormalizeMmioKey(addr, phys);
        const std::uint32_t mask = WidthMask(widthBytes);
        auto& e = mmioHits[key];
        ++e.reads;
        e.readBitsSet |= (val & mask);
        e.readBitsClr |= ((~val) & mask);
        e.widthsMask |= WidthBit(widthBytes);
    }

    void RecordWrite(const std::uint32_t addr, const std::uint32_t phys,
                     const std::uint32_t val, const std::uint8_t widthBytes)
    {
        if (!IsMmioRegion(addr, phys)) return;
        const std::uint32_t key = NormalizeMmioKey(addr, phys);
        const std::uint32_t mask = WidthMask(widthBytes);
        auto& e = mmioHits[key];
        ++e.writes;
        e.writeBitsSet |= (val & mask);
        e.writeBitsClr |= ((~val) & mask);
        e.widthsMask |= WidthBit(widthBytes);
    }

    void RecordCpuState(const std::uint8_t nextState) noexcept
    {
        if (nextState < 4u) {
            ++cpuStateHits[nextState];
            if (lastCpuState < 4u && nextState != lastCpuState) {
                ++cpuStateTransitions[lastCpuState][nextState];
            }
            lastCpuState = nextState;
        }
    }

    void RecordFlashTransition(const std::uint8_t prev, const std::uint8_t next)
    {
        ++flashCfiStates[next];
        if (prev != next) {
            const std::uint32_t key = (static_cast<std::uint32_t>(prev) << 8) | next;
            ++flashCfiTransitions[key];
        }
    }

    void RecordBoundary(const char* name)
    {
        ++boundaryHits[name];
    }

    void MergeFrom(const CoverageCollector& other)
    {
        for (std::size_t i = 0; i < opcode16Hits.size(); ++i) {
            opcode16Hits[i] += other.opcode16Hits[i];
        }
        for (std::size_t i = 0; i < dsp32Op2Hi8Hits.size(); ++i) {
            dsp32Op2Hi8Hits[i] += other.dsp32Op2Hi8Hits[i];
        }
        for (const auto& kv : other.mmioHits) {
            auto& dst = mmioHits[kv.first];
            dst.reads += kv.second.reads;
            dst.writes += kv.second.writes;
            dst.writeBitsSet |= kv.second.writeBitsSet;
            dst.writeBitsClr |= kv.second.writeBitsClr;
            dst.readBitsSet |= kv.second.readBitsSet;
            dst.readBitsClr |= kv.second.readBitsClr;
            dst.widthsMask |= kv.second.widthsMask;
        }
        for (const auto& kv : other.expevtHits) expevtHits[kv.first] += kv.second;
        for (const auto& kv : other.intevtHits) intevtHits[kv.first] += kv.second;
        for (std::size_t i = 0; i < 4u; ++i) {
            cpuStateHits[i] += other.cpuStateHits[i];
            for (std::size_t j = 0; j < 4u; ++j) {
                cpuStateTransitions[i][j] += other.cpuStateTransitions[i][j];
            }
        }
        for (const auto& kv : other.flashCfiStates) flashCfiStates[kv.first] += kv.second;
        for (const auto& kv : other.flashCfiTransitions) flashCfiTransitions[kv.first] += kv.second;
        for (std::size_t i = 0; i < 6u; ++i) dmacChannelsHit[i] += other.dmacChannelsHit[i];
        for (const auto& kv : other.boundaryHits) boundaryHits[kv.first] += kv.second;
    }

    bool WriteJson(const std::string& path) const
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        auto hexKey = [](std::uint32_t v, int width) {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::uppercase << std::setw(width) << std::setfill('0') << v;
            return oss.str();
        };
        out << "{\n";
        // opcode16_hits
        out << "  \"opcode16_hits\": {";
        bool first = true;
        for (std::size_t i = 0; i < opcode16Hits.size(); ++i) {
            if (opcode16Hits[i] == 0u) continue;
            if (!first) out << ",";
            first = false;
            out << "\"" << hexKey(static_cast<std::uint32_t>(i), 4) << "\":" << opcode16Hits[i];
        }
        out << "},\n";
        // dsp32_op2_hi8_hits
        out << "  \"dsp32_op2_hi8_hits\": {";
        first = true;
        for (std::size_t i = 0; i < dsp32Op2Hi8Hits.size(); ++i) {
            if (dsp32Op2Hi8Hits[i] == 0u) continue;
            if (!first) out << ",";
            first = false;
            out << "\"" << hexKey(static_cast<std::uint32_t>(i), 2) << "\":" << dsp32Op2Hi8Hits[i];
        }
        out << "},\n";
        // mmio_hits
        out << "  \"mmio_hits\": {\n";
        first = true;
        for (const auto& kv : mmioHits) {
            if (!first) out << ",\n";
            first = false;
            const auto& e = kv.second;
            const std::uint32_t writeToggled = e.writeBitsSet & e.writeBitsClr;
            const std::uint32_t readToggled = e.readBitsSet & e.readBitsClr;
            out << "    \"" << hexKey(kv.first, 8) << "\": {"
                << "\"reads\":" << e.reads
                << ",\"writes\":" << e.writes
                << ",\"write_bits_set\":" << e.writeBitsSet
                << ",\"write_bits_clr\":" << e.writeBitsClr
                << ",\"write_bits_toggled\":" << writeToggled
                << ",\"read_bits_set\":" << e.readBitsSet
                << ",\"read_bits_clr\":" << e.readBitsClr
                << ",\"read_bits_toggled\":" << readToggled
                << ",\"widths_mask\":" << static_cast<unsigned>(e.widthsMask)
                << "}";
        }
        out << "\n  },\n";
        // expevt_hits
        out << "  \"expevt_hits\": {";
        first = true;
        for (const auto& kv : expevtHits) {
            if (!first) out << ",";
            first = false;
            out << "\"" << hexKey(kv.first, 3) << "\":" << kv.second;
        }
        out << "},\n";
        // intevt_hits
        out << "  \"intevt_hits\": {";
        first = true;
        for (const auto& kv : intevtHits) {
            if (!first) out << ",";
            first = false;
            out << "\"" << hexKey(kv.first, 3) << "\":" << kv.second;
        }
        out << "},\n";
        // cpu_states & transitions
        out << "  \"cpu_state_hits\": [" << cpuStateHits[0] << "," << cpuStateHits[1]
            << "," << cpuStateHits[2] << "," << cpuStateHits[3] << "],\n";
        out << "  \"cpu_state_transitions\": [";
        for (std::size_t i = 0; i < 4u; ++i) {
            if (i) out << ",";
            out << "[" << cpuStateTransitions[i][0] << "," << cpuStateTransitions[i][1]
                << "," << cpuStateTransitions[i][2] << "," << cpuStateTransitions[i][3] << "]";
        }
        out << "],\n";
        // flash CFI
        out << "  \"flash_cfi_states\": {";
        first = true;
        for (const auto& kv : flashCfiStates) {
            if (!first) out << ",";
            first = false;
            out << "\"" << kv.first << "\":" << kv.second;
        }
        out << "},\n";
        out << "  \"flash_cfi_transitions\": {";
        first = true;
        for (const auto& kv : flashCfiTransitions) {
            if (!first) out << ",";
            first = false;
            out << "\"" << (kv.first >> 8) << "->" << (kv.first & 0xFFu) << "\":" << kv.second;
        }
        out << "},\n";
        // dmac channels
        out << "  \"dmac_channels_hit\": [";
        for (std::size_t i = 0; i < 6u; ++i) {
            if (i) out << ",";
            out << dmacChannelsHit[i];
        }
        out << "],\n";
        // boundaries
        out << "  \"boundary_hits\": {";
        first = true;
        for (const auto& kv : boundaryHits) {
            if (!first) out << ",";
            first = false;
            out << "\"" << kv.first << "\":" << kv.second;
        }
        out << "}\n";
        out << "}\n";
        return true;
    }
};

class GlobalCoverageSink {
public:
    static void MergeAndScheduleFlush(const CoverageCollector& col)
    {
        const char* outEnv = std::getenv("CG50_COVERAGE_OUT");
        if (!outEnv || outEnv[0] == '\0') return;
        auto& inst = Instance();
        std::lock_guard<std::mutex> lock(inst.mu_);
        inst.outPath_ = outEnv;
        inst.accum_.MergeFrom(col);
        if (!inst.registered_) {
            inst.registered_ = true;
            std::atexit(&GlobalCoverageSink::FlushAtExit);
        }
    }

private:
    static GlobalCoverageSink& Instance()
    {
        static GlobalCoverageSink sink;
        return sink;
    }

    static void FlushAtExit()
    {
        auto& inst = Instance();
        std::lock_guard<std::mutex> lock(inst.mu_);
        if (!inst.outPath_.empty()) {
            inst.accum_.WriteJson(inst.outPath_);
        }
    }

    std::mutex mu_;
    CoverageCollector accum_;
    std::string outPath_;
    bool registered_ = false;
};

} // namespace cg50

#endif // CG50_ENABLE_COVERAGE_COUNTERS
