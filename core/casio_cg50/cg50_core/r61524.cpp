#include "r61524.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cg50 {

void R61524::Reset()
{
    pins_ = Pins{};
    writes_.clear();
    gram_.assign(kVisibleWidth * kVisibleHeight, 0x0000);
    indexRegister_ = 0;
    entryMode_ = 0x1030;
    displayControl_ = 0;
    driverOutputControl_ = 0;
    curX_ = 0;
    curY_ = 0;
    winStartX_ = 0;
    winEndX_ = static_cast<std::uint16_t>(kVisibleWidth - 1);
    winStartY_ = 0;
    winEndY_ = static_cast<std::uint16_t>(kVisibleHeight - 1);
    hasValidFrame_ = false;
}

void R61524::AdvanceCursorAfterData() noexcept
{
    const bool incX = (entryMode_ & 0x0010) != 0;
    const bool incY = (entryMode_ & 0x0020) != 0;
    const bool verticalFirst = (entryMode_ & 0x0008) != 0;

    if (!verticalFirst) {
        if (incX) {
            if (curX_ < winEndX_) {
                curX_++;
            } else {
                curX_ = winStartX_;
                if (incY) {
                    curY_ = (curY_ < winEndY_) ? (curY_ + 1) : winStartY_;
                } else {
                    curY_ = (curY_ > winStartY_) ? (curY_ - 1) : winEndY_;
                }
            }
        } else {
            if (curX_ > winStartX_) {
                curX_--;
            } else {
                curX_ = winEndX_;
                if (incY) {
                    curY_ = (curY_ < winEndY_) ? (curY_ + 1) : winStartY_;
                } else {
                    curY_ = (curY_ > winStartY_) ? (curY_ - 1) : winEndY_;
                }
            }
        }
    } else {
        if (incY) {
            if (curY_ < winEndY_) {
                curY_++;
            } else {
                curY_ = winStartY_;
                if (incX) {
                    curX_ = (curX_ < winEndX_) ? (curX_ + 1) : winStartX_;
                } else {
                    curX_ = (curX_ > winStartX_) ? (curX_ - 1) : winEndX_;
                }
            }
        } else {
            if (curY_ > winStartY_) {
                curY_--;
            } else {
                curY_ = winEndY_;
                if (incX) {
                    curX_ = (curX_ < winEndX_) ? (curX_ + 1) : winStartX_;
                } else {
                    curX_ = (curX_ > winStartX_) ? (curX_ - 1) : winEndX_;
                }
            }
        }
    }
}

bool R61524::ObserveWrite16(const std::uint16_t value)
{
    if (!pins_.v28sys || !pins_.resetX) return true;
    writes_.push_back(BusCycle{pins_, value});

    if (gram_.empty()) {
        gram_.assign(kVisibleWidth * kVisibleHeight, 0x0000);
    }

    // RS = 0 -> Index Register (Command)
    // RS = 1 -> Parameter / Data Write
    if (!pins_.registerSelect) {
        indexRegister_ = value;
    } else {
        switch (indexRegister_) {
        case 0x0000: // NOP / Start Oscillation
            break;
        case 0x0001: // Driver Output Control (SS bit 8 flips X scan)
            driverOutputControl_ = value;
            break;
        case 0x0002: // LCD Driving Wave Control
            break;
        case 0x0003: // Entry Mode (Address increment direction + DFM)
            entryMode_ = value;
            break;
        case 0x0007: // Display Control
            displayControl_ = value;
            hasValidFrame_ = ((value & 0x0001) != 0);
            break;
        case 0x0200: // RAM Address Set (Horizontal / X)
            curX_ = std::min<std::uint16_t>(value, static_cast<std::uint16_t>(kVisibleWidth - 1));
            break;
        case 0x0201: // RAM Address Set (Vertical / Y)
            curY_ = std::min<std::uint16_t>(value, static_cast<std::uint16_t>(kVisibleHeight - 1));
            break;
        case 0x0022:
        case 0x0202: // RAM Data Write (GRAM)
        {
            // SS flip (driver output control bit 8) is stored but NOT
            // applied: the validated boot renders through this exact path
            // and mirroring writes without official evidence would change
            // the menu output. The store keeps the OS write visible for
            // save-state and diagnostics.
            if (curX_ < kVisibleWidth && curY_ < kVisibleHeight) {
                gram_[static_cast<std::size_t>(curY_) * kVisibleWidth + curX_] = value;
                hasValidFrame_ = true;
            }
            AdvanceCursorAfterData();
            break;
        }
        case 0x0210: // Horizontal RAM Address Start Position
            winStartX_ = std::min<std::uint16_t>(value, static_cast<std::uint16_t>(kVisibleWidth - 1));
            break;
        case 0x0211: // Horizontal RAM Address End Position
            winEndX_ = std::min<std::uint16_t>(value, static_cast<std::uint16_t>(kVisibleWidth - 1));
            break;
        case 0x0212: // Vertical RAM Address Start Position
            winStartY_ = std::min<std::uint16_t>(value, static_cast<std::uint16_t>(kVisibleHeight - 1));
            break;
        case 0x0213: // Vertical RAM Address End Position
            winEndY_ = std::min<std::uint16_t>(value, static_cast<std::uint16_t>(kVisibleHeight - 1));
            break;
        default:
            // CG50_LCD_CMD_LOG=1: registra comandos fuera del conjunto
            // implementado (evidencia de cobertura de comandos, N-7).
            {
                static const bool logUnknown =
                    std::getenv("CG50_LCD_CMD_LOG") != nullptr;
                if (logUnknown) {
                    std::fprintf(stderr,
                                 "[LCDCMD] unknown index=0x%04X value=0x%04X\n",
                                 indexRegister_, value);
                }
            }
            break;
        }
    }
    return true;
}

bool R61524::ReadBus16(std::uint16_t& value) const noexcept
{
    // Read-back contract of the VALIDATED baseline (official-correlated):
    // index 0 returns the panel ID and every other register reads 0. The
    // richer read-back (entry mode, cursor, window, GRAM) regressed the OS
    // boot at instruction ~7.03M (LCD driver poll at PC=0x8004E0A4 saw the
    // stored entry mode instead of 0 and derailed the timeline); it stays
    // disabled until CPU73050 evidence shows the real panel answers.
    if (indexRegister_ == 0x0000) {
        value = kConfiguredId;
        return true;
    }
    value = 0;
    return true;
}

bool R61524::ExportState(std::vector<std::uint8_t>& out) const
{
    out.clear();
    const auto append16 = [&out](const std::uint16_t v) {
        out.push_back(static_cast<std::uint8_t>(v >> 8));
        out.push_back(static_cast<std::uint8_t>(v));
    };
    const auto appendBool = [&out](const bool v) {
        out.push_back(v ? 1u : 0u);
    };
    appendBool(pins_.chipSelect);
    appendBool(pins_.registerSelect);
    appendBool(pins_.resetX);
    appendBool(pins_.interfaceMode1);
    appendBool(pins_.frameMark);
    appendBool(pins_.v28sys);
    appendBool(pins_.backlightOn);
    appendBool(pins_.backlightBrightness);
    const std::uint32_t gramPixels =
        static_cast<std::uint32_t>(gram_.size());
    out.push_back(static_cast<std::uint8_t>(gramPixels >> 24));
    out.push_back(static_cast<std::uint8_t>(gramPixels >> 16));
    out.push_back(static_cast<std::uint8_t>(gramPixels >> 8));
    out.push_back(static_cast<std::uint8_t>(gramPixels));
    for (const std::uint16_t pixel : gram_) append16(pixel);
    append16(indexRegister_);
    append16(entryMode_);
    append16(displayControl_);
    append16(driverOutputControl_);
    append16(curX_);
    append16(curY_);
    append16(winStartX_);
    append16(winEndX_);
    append16(winStartY_);
    append16(winEndY_);
    appendBool(hasValidFrame_);
    return true;
}

bool R61524::ImportState(const std::uint8_t* data, const std::size_t size)
{
    if (data == nullptr) return false;
    std::size_t cursor = 0;
    const auto need = [&cursor, size](const std::size_t bytes) {
        if (cursor + bytes > size) return false;
        return true;
    };
    const auto readBool = [&data, &cursor]() {
        return data[cursor++] != 0u;
    };
    if (!need(8u)) return false;
    pins_.chipSelect = readBool();
    pins_.registerSelect = readBool();
    pins_.resetX = readBool();
    pins_.interfaceMode1 = readBool();
    pins_.frameMark = readBool();
    pins_.v28sys = readBool();
    pins_.backlightOn = readBool();
    pins_.backlightBrightness = readBool();
    if (!need(4u)) return false;
    std::uint32_t gramPixels = 0;
    gramPixels |= static_cast<std::uint32_t>(data[cursor++]) << 24;
    gramPixels |= static_cast<std::uint32_t>(data[cursor++]) << 16;
    gramPixels |= static_cast<std::uint32_t>(data[cursor++]) << 8;
    gramPixels |= static_cast<std::uint32_t>(data[cursor++]);
    if (gramPixels != kVisibleWidth * kVisibleHeight) return false;
    if (!need(static_cast<std::size_t>(gramPixels) * 2u)) return false;
    gram_.resize(gramPixels);
    for (std::size_t i = 0; i < gramPixels; ++i) {
        const std::uint16_t hi = data[cursor++];
        gram_[i] = static_cast<std::uint16_t>((hi << 8) | data[cursor++]);
    }
    const auto read16 = [&data, &cursor]() {
        const std::uint16_t hi = data[cursor++];
        return static_cast<std::uint16_t>((hi << 8) | data[cursor++]);
    };
    if (!need(10u * 2u + 1u)) return false;
    indexRegister_ = read16();
    entryMode_ = read16();
    displayControl_ = read16();
    driverOutputControl_ = read16();
    curX_ = read16();
    curY_ = read16();
    winStartX_ = read16();
    winEndX_ = read16();
    winStartY_ = read16();
    winEndY_ = read16();
    hasValidFrame_ = readBool();
    writes_.clear();
    return true;
}

} // namespace cg50
