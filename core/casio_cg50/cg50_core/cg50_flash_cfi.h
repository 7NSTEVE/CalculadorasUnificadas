#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cg50 {

// ============================================================================
// Spansion S29GL256P (LY755) NOR Flash CFI State Machine
//
// Recovered directly from Casio fx-CG50 OS 3.60 ILRAM flash driver
// (0xFD800240..0xFD800EB0):
//   - Uniform 128 KB (0x20000) erase sectors (mask 0xFFFE0000)
//   - Sector Erase:    AA@AAA -> 55@554 -> 80@AAA -> AA@AAA -> 55@554 -> 30@SA
//   - Write-to-Buffer: AA@AAA -> 55@554 -> 25@addr -> (N-1)@SA -> N words -> 29@SA
//   - Word Program:    AA@AAA -> 55@554 -> A0@AAA -> data@addr
//   - Abort / Reset:   F0@any or AA@AAA -> 55@554 -> F0@AAA
//   - Autoselect:      AA@AAA -> 55@554 -> 90@AAA (Spansion S29GL256P IDs)
//   - CFI Query:       98@AA  (QRY + 32 MB / 256x128 KB / 64 B write buffer)
//
// NOR programming follows true 1->0 bit semantics (flash &= data), while
// sector erase resets the full 128 KB sector to 0xFF so OS blank-checks
// (0xFD8005B8) and DQ7 polling (0xFD8002A2 / 0xFD800C44) succeed immediately.
// ============================================================================
class SpansionNorFlashCfi {
public:
    static constexpr std::uint32_t kSectorSize = 0x00020000u;     // 128 KB
    static constexpr std::uint32_t kSectorMask = ~(kSectorSize - 1u);
    static constexpr std::size_t   kMaxBufferWords = 64u;         // S29GL256P max = 32 words (64 B); allow 64 safely

    enum class State : std::uint8_t {
        ReadArray = 0,
        Unlock1,
        Unlock2,
        ProgramWord,
        EraseSetup,
        EraseUnlock1,
        EraseUnlock2,
        WriteBufferCount,
        WriteBufferData,
        WriteBufferConfirm,
        Autoselect,
        CfiQuery,
    };

    struct BufferedWord {
        std::uint32_t physical = 0;
        std::uint16_t value = 0xFFFFu;
    };

    struct Stats {
        std::uint64_t sectorErases = 0;
        std::uint64_t bufferPrograms = 0;
        std::uint64_t wordPrograms = 0;
        std::uint64_t resets = 0;
    };

    void Reset() noexcept
    {
        state_ = State::ReadArray;
        bufferSector_ = 0;
        bufferRemaining_ = 0;
        bufferCount_ = 0;
    }

    bool IsReadArray() const noexcept { return state_ == State::ReadArray; }
    State CurrentState() const noexcept { return state_; }
    const Stats& GetStats() const noexcept { return stats_; }

    // Handles 16-bit read when in Autoselect or CFI Query mode.
    // Returns true if this state machine answered the read; false means normal
    // Flash array read (ReadArray).
    bool TryRead16(const std::uint32_t physical, std::uint16_t& outValue) const noexcept
    {
        if (state_ == State::ReadArray) return false;
        const std::uint32_t wordOffset = (physical >> 1) & 0xFFu;
        if (state_ == State::Autoselect) {
            switch (wordOffset) {
            case 0x00u: outValue = 0x0001u; return true; // Spansion / AMD
            case 0x01u: outValue = 0x227Eu; return true; // S29GL-P family
            case 0x02u: outValue = 0x0000u; return true; // Sector unprotected
            case 0x0Eu: outValue = 0x2222u; return true; // S29GL256P ID2
            case 0x0Fu: outValue = 0x2201u; return true; // S29GL256P ID3
            default:    outValue = 0x0000u; return true;
            }
        }
        if (state_ == State::CfiQuery) {
            switch (wordOffset) {
            case 0x10u: outValue = 0x0051u; return true; // 'Q'
            case 0x11u: outValue = 0x0052u; return true; // 'R'
            case 0x12u: outValue = 0x0059u; return true; // 'Y'
            case 0x13u: outValue = 0x0002u; return true; // Primary OEM cmd set (AMD/Spansion)
            case 0x14u: outValue = 0x0000u; return true;
            case 0x15u: outValue = 0x0040u; return true; // Extended table addr
            case 0x16u: outValue = 0x0000u; return true;
            case 0x1Bu: outValue = 0x0027u; return true; // Vcc min 2.7V
            case 0x1Cu: outValue = 0x0036u; return true; // Vcc max 3.6V
            case 0x27u: outValue = 0x0019u; return true; // Device size 2^25 = 32 MB
            case 0x28u: outValue = 0x0002u; return true; // x8/x16 interface
            case 0x29u: outValue = 0x0000u; return true;
            case 0x2Au: outValue = 0x0006u; return true; // Max buffer write 2^6 = 64 B
            case 0x2Bu: outValue = 0x0000u; return true;
            case 0x2Cu: outValue = 0x0001u; return true; // 1 erase block region
            case 0x2Du: outValue = 0x00FFu; return true; // 256 sectors (0xFF + 1)
            case 0x2Eu: outValue = 0x0000u; return true;
            case 0x2Fu: outValue = 0x0000u; return true; // 128 KB = 0x0200 * 256 B
            case 0x30u: outValue = 0x0002u; return true;
            case 0x40u: outValue = 0x0050u; return true; // 'P'
            case 0x41u: outValue = 0x0052u; return true; // 'R'
            case 0x42u: outValue = 0x0049u; return true; // 'I'
            case 0x43u: outValue = 0x0031u; return true; // '1'
            case 0x44u: outValue = 0x0033u; return true; // '3'
            default:    outValue = 0x0000u; return true;
            }
        }
        return false;
    }

    // Processes a 16-bit write anywhere inside the NOR flash physical window.
    // Callbacks:
    //   programWord(std::uint32_t phys, std::uint16_t val) -> NOR AND-program
    //   eraseSector(std::uint32_t sectorBasePhys)          -> fill 128 KB with 0xFF
    //
    // Returns true if the write was consumed as part of the CFI protocol.
    template <typename ProgramWordFn, typename EraseSectorFn>
    bool HandleWrite16(const std::uint32_t physical,
                       const std::uint16_t value,
                       ProgramWordFn&& programWord,
                       EraseSectorFn&& eraseSector) noexcept
    {
        const std::uint32_t cmdOff = physical & 0x0000FFFFu;
        const std::uint8_t cmd = static_cast<std::uint8_t>(value & 0xFFu);

        switch (state_) {
        case State::ReadArray:
            if (cmdOff == 0x0AAAu && cmd == 0xAAu) {
                state_ = State::Unlock1;
                return true;
            }
            if (cmdOff == 0x00AAu && cmd == 0x98u) {
                state_ = State::CfiQuery;
                return true;
            }
            if (cmd == 0xF0u) {
                ++stats_.resets;
                return true;
            }
            return false;

        case State::Autoselect:
        case State::CfiQuery:
            if (cmd == 0xF0u) {
                state_ = State::ReadArray;
                ++stats_.resets;
                return true;
            }
            if (cmdOff == 0x0AAAu && cmd == 0xAAu) {
                state_ = State::Unlock1;
                return true;
            }
            return true;

        case State::Unlock1:
            if (cmdOff == 0x0554u && cmd == 0x55u) {
                state_ = State::Unlock2;
            } else if (cmdOff == 0x0AAAu && cmd == 0xAAu) {
                state_ = State::Unlock1;
            } else {
                state_ = State::ReadArray;
            }
            return true;

        case State::Unlock2:
            if (cmd == 0x25u) {
                // Spansion Write-to-Buffer command written to target sector/address.
                bufferSector_ = physical & kSectorMask;
                bufferCount_ = 0;
                bufferRemaining_ = 0;
                state_ = State::WriteBufferCount;
                return true;
            }
            if (cmdOff == 0x0AAAu) {
                if (cmd == 0xA0u) {
                    state_ = State::ProgramWord;
                    return true;
                }
                if (cmd == 0x80u) {
                    state_ = State::EraseSetup;
                    return true;
                }
                if (cmd == 0x90u) {
                    state_ = State::Autoselect;
                    return true;
                }
                if (cmd == 0xF0u) {
                    // Write-to-Buffer Abort Reset (0xFD800E80: AA@AAA, 55@554, F0@AAA)
                    Reset();
                    ++stats_.resets;
                    return true;
                }
            }
            if (cmd == 0xF0u) {
                Reset();
                ++stats_.resets;
                return true;
            }
            state_ = State::ReadArray;
            return true;

        case State::ProgramWord:
            programWord(physical, value);
            ++stats_.wordPrograms;
            state_ = State::ReadArray;
            return true;

        case State::EraseSetup:
            if (cmdOff == 0x0AAAu && cmd == 0xAAu) {
                state_ = State::EraseUnlock1;
            } else {
                state_ = State::ReadArray;
            }
            return true;

        case State::EraseUnlock1:
            if (cmdOff == 0x0554u && cmd == 0x55u) {
                state_ = State::EraseUnlock2;
            } else {
                state_ = State::ReadArray;
            }
            return true;

        case State::EraseUnlock2:
            if (cmd == 0x30u) {
                eraseSector(physical & kSectorMask);
                ++stats_.sectorErases;
            }
            state_ = State::ReadArray;
            return true;

        case State::WriteBufferCount: {
            const std::size_t count = static_cast<std::size_t>(value & 0xFFu) + 1u;
            if (count == 0u || count > kMaxBufferWords) {
                Reset();
                return true;
            }
            bufferCount_ = 0;
            bufferRemaining_ = count;
            state_ = State::WriteBufferData;
            return true;
        }

        case State::WriteBufferData:
            if (bufferCount_ < kMaxBufferWords) {
                buffer_[bufferCount_++] = BufferedWord{physical, value};
            }
            if (bufferRemaining_ > 0u) {
                --bufferRemaining_;
            }
            if (bufferRemaining_ == 0u) {
                state_ = State::WriteBufferConfirm;
            }
            return true;

        case State::WriteBufferConfirm:
            if (cmd == 0x29u) {
                for (std::size_t i = 0; i < bufferCount_; ++i) {
                    programWord(buffer_[i].physical, buffer_[i].value);
                }
                ++stats_.bufferPrograms;
            }
            Reset();
            return true;
        }
        Reset();
        return true;
    }

private:
    State state_ = State::ReadArray;
    std::uint32_t bufferSector_ = 0;
    std::size_t bufferRemaining_ = 0;
    std::size_t bufferCount_ = 0;
    std::array<BufferedWord, kMaxBufferWords> buffer_{};
    Stats stats_{};
};

} // namespace cg50
