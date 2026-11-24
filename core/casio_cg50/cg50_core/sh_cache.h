#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cg50 {

// ============================================================================
// SH7305 / SH-4A 4-Way Instruction & Operand Cache + OCRAM (0x7C000000, 16 KB)
//
// Models:
//   - IC (Instruction Cache): 256 sets x 4 ways x 32-byte line = 32 KB
//   - OC (Operand/Data Cache): 256 sets x 4 ways x 32-byte line = 32 KB
//     When CCR.ORA=1, ways 2 & 3 of OC operate as 16 KB On-Chip RAM (OCRAM)
//     mapped at 0x7C000000..0x7C003FFF (used by gint/homebrew add-ins).
//   - CCR bits:
//       bit  0: OCE (Operand Cache Enable)
//       bit  1: WT  (Write-Through for P0/U0/P3)
//       bit  2: CB  (Copy-Back for P1)
//       bit  3: OCI (OC Invalidate all)
//       bit  5: ORA (OC RAM Mode: ways 2 & 3 -> 16 KB OCRAM)
//       bit  7: OIX (OC Index Mode)
//       bit  8: ICE (Instruction Cache Enable)
//       bit 11: ICI (IC Invalidate all)
//       bit 15: IIX (IC Index Mode)
//   - MMIO array windows:
//       0xF0000000..0xF0FFFFFF: IC address array
//       0xF1000000..0xF1FFFFFF: IC data array
//       0xF4000000..0xF4FFFFFF: OC address array
//       0xF5000000..0xF5FFFFFF: OC data array
//   - Real cache-line ops: ICBI, OCBI, OCBP, OCBWB, MOVCA.L, and DMA coherency.
// ============================================================================
class ShCacheUnit final {
public:
    static constexpr std::size_t kSets = 256u;
    static constexpr std::size_t kWays = 4u;
    static constexpr std::size_t kLineBytes = 32u;
    static constexpr std::uint32_t kOcramBase = 0x7C000000u;
    static constexpr std::size_t kOcramSize = 16384u; // 16 KB (256 sets * 2 ways * 32 B)

    static constexpr std::uint32_t kCcrOce = 0x00000001u;
    static constexpr std::uint32_t kCcrWt  = 0x00000002u;
    static constexpr std::uint32_t kCcrCb  = 0x00000004u;
    static constexpr std::uint32_t kCcrOci = 0x00000008u;
    static constexpr std::uint32_t kCcrOra = 0x00000020u;
    static constexpr std::uint32_t kCcrOix = 0x00000080u;
    static constexpr std::uint32_t kCcrIce = 0x00000100u;
    static constexpr std::uint32_t kCcrIci = 0x00000800u;
    static constexpr std::uint32_t kCcrIix = 0x00008000u;

    struct CacheLine {
        std::uint32_t tag = 0; // Physical address bits 31..10
        bool valid = false;
        bool dirty = false;
        std::array<std::uint8_t, kLineBytes> data{};
    };

    struct Stats {
        std::uint64_t icHits = 0;
        std::uint64_t icMisses = 0;
        std::uint64_t ocHits = 0;
        std::uint64_t ocMisses = 0;
        std::uint64_t ocWritebacks = 0;
        std::uint64_t icbiOps = 0;
        std::uint64_t ocbiOps = 0;
        std::uint64_t ocbpOps = 0;
        std::uint64_t ocbwbOps = 0;
    };

    void Reset() noexcept
    {
        ccr_ = 0u;
        InvalidateIcAll();
        InvalidateOcAll();
        ocram_.fill(0u);
        icLru_.fill(0u);
        ocLru_.fill(0u);
        stats_ = Stats{};
    }

    void ApplyCcrWrite(const std::uint32_t value) noexcept
    {
        if ((value & kCcrOci) != 0u) InvalidateOcAll();
        if ((value & kCcrIci) != 0u) InvalidateIcAll();
        ccr_ = value & ~(kCcrOci | kCcrIci);
    }

    std::uint32_t Ccr() const noexcept { return ccr_; }
    bool OcEnabled() const noexcept { return (ccr_ & kCcrOce) != 0u; }
    bool IcEnabled() const noexcept { return (ccr_ & kCcrIce) != 0u; }
    bool OcramEnabled() const noexcept { return (ccr_ & kCcrOra) != 0u; }
    const Stats& GetStats() const noexcept { return stats_; }

    static bool MatchesOcramAddress(const std::uint32_t addr, const std::size_t bytes) noexcept
    {
        return addr >= kOcramBase &&
               static_cast<std::uint64_t>(addr) + bytes <=
                   static_cast<std::uint64_t>(kOcramBase) + kOcramSize;
    }

    std::uint8_t* OcramPointer(const std::uint32_t addr, const std::size_t bytes) noexcept
    {
        if (!OcramEnabled() || !MatchesOcramAddress(addr, bytes)) return nullptr;
        return ocram_.data() + static_cast<std::size_t>(addr - kOcramBase);
    }

    const std::uint8_t* OcramPointer(const std::uint32_t addr, const std::size_t bytes) const noexcept
    {
        if (!OcramEnabled() || !MatchesOcramAddress(addr, bytes)) return nullptr;
        return ocram_.data() + static_cast<std::size_t>(addr - kOcramBase);
    }

    void InvalidateIcAll() noexcept
    {
        for (auto& set : ic_) {
            for (auto& line : set) {
                line.valid = false;
                line.dirty = false;
            }
        }
    }

    void InvalidateOcAll() noexcept
    {
        for (auto& set : oc_) {
            for (auto& line : set) {
                line.valid = false;
                line.dirty = false;
            }
        }
    }

    // Track instruction fetch hit/miss against IC tag array
    void ObserveIcFetch(const std::uint32_t physAddr, const std::uint8_t* lineSource) noexcept
    {
        if (!IcEnabled()) return;
        const std::size_t setIdx = (physAddr >> 5) & (kSets - 1u);
        const std::uint32_t tag = physAddr & ~0x3FFu;
        for (std::size_t w = 0; w < kWays; ++w) {
            if (ic_[setIdx][w].valid && ic_[setIdx][w].tag == tag) {
                ++stats_.icHits;
                return;
            }
        }
        ++stats_.icMisses;
        const std::size_t victim = icLru_[setIdx] & 0x3u;
        icLru_[setIdx] = static_cast<std::uint8_t>((victim + 1u) & 0x3u);
        auto& line = ic_[setIdx][victim];
        line.tag = tag;
        line.valid = true;
        line.dirty = false;
        if (lineSource != nullptr) {
            std::memcpy(line.data.data(), lineSource, kLineBytes);
        }
    }

    // Track operand read/write against OC tag array (copy-back marks dirty)
    void ObserveOcAccess(const std::uint32_t physAddr,
                         const bool isWrite,
                         const bool copyBack,
                         const std::uint8_t* lineSource) noexcept
    {
        if (!OcEnabled()) return;
        const std::size_t maxWays = OcramEnabled() ? 2u : kWays;
        const std::size_t setIdx = (physAddr >> 5) & (kSets - 1u);
        const std::uint32_t tag = physAddr & ~0x3FFu;
        for (std::size_t w = 0; w < maxWays; ++w) {
            auto& line = oc_[setIdx][w];
            if (line.valid && line.tag == tag) {
                ++stats_.ocHits;
                if (isWrite && copyBack) line.dirty = true;
                if (lineSource != nullptr) std::memcpy(line.data.data(), lineSource, kLineBytes);
                return;
            }
        }
        ++stats_.ocMisses;
        const std::size_t victim = ocLru_[setIdx] % maxWays;
        ocLru_[setIdx] = static_cast<std::uint8_t>((victim + 1u) % maxWays);
        auto& line = oc_[setIdx][victim];
        if (line.valid && line.dirty) {
            ++stats_.ocWritebacks;
        }
        line.tag = tag;
        line.valid = true;
        line.dirty = (isWrite && copyBack);
        if (lineSource != nullptr) {
            std::memcpy(line.data.data(), lineSource, kLineBytes);
        }
    }

    // ICBI @Rn: invalidate matching IC line
    void Icbi(const std::uint32_t physAddr) noexcept
    {
        ++stats_.icbiOps;
        const std::size_t setIdx = (physAddr >> 5) & (kSets - 1u);
        const std::uint32_t tag = physAddr & ~0x3FFu;
        for (std::size_t w = 0; w < kWays; ++w) {
            if (ic_[setIdx][w].valid && ic_[setIdx][w].tag == tag) {
                ic_[setIdx][w].valid = false;
            }
        }
    }

    // OCBI @Rn: invalidate matching OC line WITHOUT writeback
    void Ocbi(const std::uint32_t physAddr) noexcept
    {
        ++stats_.ocbiOps;
        const std::size_t setIdx = (physAddr >> 5) & (kSets - 1u);
        const std::uint32_t tag = physAddr & ~0x3FFu;
        for (std::size_t w = 0; w < kWays; ++w) {
            if (oc_[setIdx][w].valid && oc_[setIdx][w].tag == tag) {
                oc_[setIdx][w].valid = false;
                oc_[setIdx][w].dirty = false;
            }
        }
    }

    // OCBP @Rn: writeback if dirty + invalidate
    void Ocbp(const std::uint32_t physAddr) noexcept
    {
        ++stats_.ocbpOps;
        const std::size_t setIdx = (physAddr >> 5) & (kSets - 1u);
        const std::uint32_t tag = physAddr & ~0x3FFu;
        for (std::size_t w = 0; w < kWays; ++w) {
            if (oc_[setIdx][w].valid && oc_[setIdx][w].tag == tag) {
                if (oc_[setIdx][w].dirty) ++stats_.ocWritebacks;
                oc_[setIdx][w].valid = false;
                oc_[setIdx][w].dirty = false;
            }
        }
    }

    // OCBWB @Rn: writeback if dirty (keep valid)
    void Ocbwb(const std::uint32_t physAddr) noexcept
    {
        ++stats_.ocbwbOps;
        const std::size_t setIdx = (physAddr >> 5) & (kSets - 1u);
        const std::uint32_t tag = physAddr & ~0x3FFu;
        for (std::size_t w = 0; w < kWays; ++w) {
            if (oc_[setIdx][w].valid && oc_[setIdx][w].tag == tag) {
                if (oc_[setIdx][w].dirty) {
                    ++stats_.ocWritebacks;
                    oc_[setIdx][w].dirty = false;
                }
            }
        }
    }

    // DMA coherency helper: invalidate OC/IC lines overlapping a DMA destination range
    void InvalidateRangeForDma(const std::uint32_t physStart, const std::size_t length) noexcept
    {
        if (length == 0u || (!OcEnabled() && !IcEnabled())) return;
        const std::uint32_t startLine = physStart & ~(static_cast<std::uint32_t>(kLineBytes - 1u));
        const std::uint32_t endLine = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(physStart) + length + kLineBytes - 1u) &
            ~static_cast<std::uint64_t>(kLineBytes - 1u));
        if (endLine - startLine >= kSets * kLineBytes) {
            InvalidateOcAll();
            InvalidateIcAll();
            return;
        }
        for (std::uint32_t a = startLine; a < endLine; a += static_cast<std::uint32_t>(kLineBytes)) {
            const std::size_t setIdx = (a >> 5) & (kSets - 1u);
            const std::uint32_t tag = a & ~0x3FFu;
            for (std::size_t w = 0; w < kWays; ++w) {
                if (oc_[setIdx][w].valid && oc_[setIdx][w].tag == tag) {
                    oc_[setIdx][w].valid = false;
                    oc_[setIdx][w].dirty = false;
                }
                if (ic_[setIdx][w].valid && ic_[setIdx][w].tag == tag) {
                    ic_[setIdx][w].valid = false;
                }
            }
        }
    }

    // MMIO Cache Array access ([0xF0000000..0xF1FFFFFF] IC, [0xF4000000..0xF5FFFFFF] OC)
    static bool MatchesMmioArray(const std::uint32_t addr) noexcept
    {
        return (addr >= 0xF0000000u && addr < 0xF2000000u) ||
               (addr >= 0xF4000000u && addr < 0xF6000000u);
    }

    std::uint32_t MmioRead32(const std::uint32_t addr) const noexcept
    {
        const bool isOc = (addr >= 0xF4000000u);
        const bool isData = ((addr & 0x01000000u) != 0u);
        const std::size_t way = (addr >> 13) & 0x3u;
        const std::size_t setIdx = (addr >> 5) & (kSets - 1u);
        const auto& line = isOc ? oc_[setIdx][way] : ic_[setIdx][way];
        if (!isData) {
            return (line.tag & ~0x3FFu) | (line.valid ? 0x1u : 0u) | (line.dirty ? 0x2u : 0u);
        }
        const std::size_t off = addr & 0x1Cu;
        return (static_cast<std::uint32_t>(line.data[off]) << 24) |
               (static_cast<std::uint32_t>(line.data[off + 1u]) << 16) |
               (static_cast<std::uint32_t>(line.data[off + 2u]) << 8) |
               static_cast<std::uint32_t>(line.data[off + 3u]);
    }

    void MmioWrite32(const std::uint32_t addr, const std::uint32_t val) noexcept
    {
        const bool isOc = (addr >= 0xF4000000u);
        const bool isData = ((addr & 0x01000000u) != 0u);
        const std::size_t way = (addr >> 13) & 0x3u;
        const std::size_t setIdx = (addr >> 5) & (kSets - 1u);
        auto& line = isOc ? oc_[setIdx][way] : ic_[setIdx][way];
        if (!isData) {
            line.tag = val & ~0x3FFu;
            line.valid = (val & 0x1u) != 0u;
            line.dirty = isOc && ((val & 0x2u) != 0u);
        } else {
            const std::size_t off = addr & 0x1Cu;
            line.data[off] = static_cast<std::uint8_t>((val >> 24) & 0xFFu);
            line.data[off + 1u] = static_cast<std::uint8_t>((val >> 16) & 0xFFu);
            line.data[off + 2u] = static_cast<std::uint8_t>((val >> 8) & 0xFFu);
            line.data[off + 3u] = static_cast<std::uint8_t>(val & 0xFFu);
        }
    }

    const std::array<std::uint8_t, kOcramSize>& OcramBytes() const noexcept { return ocram_; }
    std::array<std::uint8_t, kOcramSize>& OcramBytes() noexcept { return ocram_; }

private:
    std::uint32_t ccr_ = 0;
    std::array<std::array<CacheLine, kWays>, kSets> ic_{};
    std::array<std::array<CacheLine, kWays>, kSets> oc_{};
    std::array<std::uint8_t, kSets> icLru_{};
    std::array<std::uint8_t, kSets> ocLru_{};
    std::array<std::uint8_t, kOcramSize> ocram_{};
    Stats stats_{};
};

} // namespace cg50
