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
    regs_.fill(0x0000);
    regs_[0x0000u] = kConfiguredId;
    regs_[0x0003u] = 0x1030u;
    regs_[0x05A1u] = 0x00D2u;
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
    gramReadDummyPending_ = false;
    vsyncCycleAccum_ = 0;
    frameMarkPulse_ = false;
}

// TickFrameMark is inlined in r61524.h for hot-path performance (zero 64-bit modulo per instruction).

void R61524::RenderDisplayRgb565(std::uint16_t* outPixels, const std::size_t count) const noexcept
{
    if (outPixels == nullptr || count < kVisibleWidth * kVisibleHeight) return;
    if (gram_.size() < kVisibleWidth * kVisibleHeight || DisplayBlanked()) {
        std::memset(outPixels, 0, kVisibleWidth * kVisibleHeight * sizeof(std::uint16_t));
        return;
    }
    const bool mirrorX = HorizontalMirrorEnabled();
    const std::size_t scrollY = VerticalScrollEnabled()
        ? (static_cast<std::size_t>(regs_[0x0404u]) % kVisibleHeight)
        : 0u;
    const std::uint16_t cabcDuty = regs_[0x05A1u] & 0x00FFu;
    const bool dimBacklight = (regs_[0x05A2u] != 0u) && (cabcDuty < 0xD2u);

    for (std::size_t y = 0; y < kVisibleHeight; ++y) {
        const std::size_t srcY = (y + scrollY) % kVisibleHeight;
        for (std::size_t x = 0; x < kVisibleWidth; ++x) {
            const std::size_t srcX = mirrorX ? (kVisibleWidth - 1u - x) : x;
            std::uint16_t px = gram_[srcY * kVisibleWidth + srcX];
            if (dimBacklight) {
                const std::uint32_t r = ((px >> 11) & 0x1Fu) * cabcDuty / 0xD2u;
                const std::uint32_t g = ((px >> 5) & 0x3Fu) * cabcDuty / 0xD2u;
                const std::uint32_t b = (px & 0x1Fu) * cabcDuty / 0xD2u;
                px = static_cast<std::uint16_t>((r << 11) | (g << 5) | b);
            }
            outPixels[y * kVisibleWidth + x] = px;
        }
    }
}

void R61524::AdvanceCursorAfterData() const noexcept
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
        if (indexRegister_ == 0x0202u || indexRegister_ == 0x0022u) {
            gramReadDummyPending_ = true;
        }
    } else {
        if (indexRegister_ < kRegisterSpace &&
            indexRegister_ != 0x0202u && indexRegister_ != 0x0022u) {
            regs_[indexRegister_] = value;
        }
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
            gramReadDummyPending_ = true;
            break;
        case 0x0201: // RAM Address Set (Vertical / Y)
            curY_ = std::min<std::uint16_t>(value, static_cast<std::uint16_t>(kVisibleHeight - 1));
            gramReadDummyPending_ = true;
            break;
        case 0x0022:
        case 0x0202: // RAM Data Write (GRAM)
        {
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
            // All R61524 registers in [0x0000..0x06FF] (Power 0x0100..0x0111,
            // Gamma 0x0300..0x0329, Base Image & Scroll 0x0400..0x0404,
            // Backlight CABC 0x05A0..0x05A3, NVM 0x0606) are backed in regs_.
            if (indexRegister_ >= kRegisterSpace) {
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
    if (indexRegister_ == 0x0000u) {
        value = kConfiguredId;
        return true;
    }
    // Authentic R61524 GRAM Read (0x0202 / 0x0022):
    // Casio OS LCD read routine (PC=0x8004DA72) performs 1 dummy read followed
    // by the RGB565 pixel read(s) with cursor auto-increment. Keeping 0x0003
    // returning 0 preserves the validated OS boot contract at PC=0x8004E0A4.
    if (indexRegister_ == 0x0202u || indexRegister_ == 0x0022u) {
        if (gramReadDummyPending_) {
            gramReadDummyPending_ = false;
            value = 0x0000u;
            return true;
        }
        if (!gram_.empty() && curX_ < kVisibleWidth && curY_ < kVisibleHeight) {
            value = gram_[static_cast<std::size_t>(curY_) * kVisibleWidth + curX_];
        } else {
            value = 0x0000u;
        }
        AdvanceCursorAfterData();
        return true;
    }
    // Preserve validated OS boot contract on 0x0003 (PC=0x8004E0A4 expects 0);
    // return authentic register values for scroll (0x0400..0x0404), CABC
    // backlight (0x05A0..0x05A3), gamma (0x0300..0x0329) and NVM (0x0606).
    if ((indexRegister_ >= 0x0300u && indexRegister_ <= 0x0329u) ||
        (indexRegister_ >= 0x0400u && indexRegister_ <= 0x0404u) ||
        (indexRegister_ >= 0x05A0u && indexRegister_ <= 0x05A3u) ||
        indexRegister_ == 0x0606u) {
        value = regs_[indexRegister_];
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
    // Optional trailing extension: key R61524 registers (scroll + backlight) + dummy latch
    append16(regs_[0x0400u]);
    append16(regs_[0x0401u]);
    append16(regs_[0x0404u]);
    append16(regs_[0x05A1u]);
    appendBool(gramReadDummyPending_);
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
    regs_.fill(0u);
    regs_[0x0000u] = kConfiguredId;
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
    regs_[0x0001u] = driverOutputControl_;
    regs_[0x0003u] = entryMode_;
    regs_[0x0007u] = displayControl_;
    if (cursor + 9u <= size) {
        regs_[0x0400u] = read16();
        regs_[0x0401u] = read16();
        regs_[0x0404u] = read16();
        regs_[0x05A1u] = read16();
        gramReadDummyPending_ = readBool();
    } else {
        regs_[0x05A1u] = 0x00D2u;
        gramReadDummyPending_ = false;
    }
    writes_.clear();
    return true;
}

} // namespace cg50
