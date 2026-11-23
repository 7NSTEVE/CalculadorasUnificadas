#include "Screen.hpp"

#include "../Chipset/MMURegion.hpp"
#include "../Data/HardwareId.hpp"
#include "../Chipset/MMU.hpp"
#include "../Emulator.hpp"
#include "../Chipset/Chipset.hpp"

#include <cstring>

namespace casioemux
{
    namespace
    {
        struct Cw2IndicatorSpec
        {
            uint8_t mask;
            uint8_t offset;
        };

        // Exact mapping from CasioEmuX Screen<HW_CLASSWIZ_II>::sprite_bitmap[SPR_S..SPR_SUN]
        constexpr Cw2IndicatorSpec kCw2Indicators[20] = {
            {0x01, 0x01}, // 0: S
            {0x01, 0x03}, // 1: Math
            {0x01, 0x04}, // 2: D
            {0x01, 0x05}, // 3: R
            {0x01, 0x06}, // 4: G
            {0x01, 0x07}, // 5: FIX
            {0x01, 0x08}, // 6: SCI
            {0x01, 0x09}, // 7: f(x)
            {0x01, 0x0A}, // 8: E
            {0x01, 0x0B}, // 9: i (CMPLX)
            {0x01, 0x0C}, // 10: angle
            {0x01, 0x0D}, // 11: WDOWN
            {0x01, 0x0E}, // 12: VERIFY
            {0x01, 0x0F}, // 13: g(x)
            {0x01, 0x10}, // 14: LEFT
            {0x01, 0x11}, // 15: DOWN
            {0x01, 0x12}, // 16: UP
            {0x01, 0x13}, // 17: RIGHT
            {0x01, 0x15}, // 18: PAUSE
            {0x01, 0x16}  // 19: SUN
        };
    }

    void Screen::Initialise()
    {
        require_frame = true;
        screen_contrast = 0x10;
        screen_mode = 0;
        screen_range = 0;
        screen_select = 0;

        constexpr size_t buf_bytes = static_cast<size_t>(N_ROW + 1) * ROW_SIZE;
        screen_buffer = new uint8_t[buf_bytes];
        screen_buffer1 = new uint8_t[buf_bytes];
        std::memset(screen_buffer, 0, buf_bytes);
        std::memset(screen_buffer1, 0, buf_bytes);

        region_select.Setup(0xF037, 1, "Screen/Select", this,
            DefaultRead<uint8_t, 0x04, &Screen::screen_select>,
            SetRequireFrameWrite<uint8_t, 0x04, &Screen::screen_select>, emulator);

        if (!emulator.GetModelInfo("real_hardware")) {
            region_buffer.Setup(0xF800, buf_bytes, "Screen/Buffer", this,
                [](MMURegion *region, size_t offset) {
                    offset -= region->base;
                    if (offset % ROW_SIZE >= ROW_SIZE_DISP) return (uint8_t)0;
                    return ((Screen *)region->userdata)->screen_buffer[offset];
                },
                [](MMURegion *region, size_t offset, uint8_t data) {
                    offset -= region->base;
                    if (offset % ROW_SIZE >= ROW_SIZE_DISP) return;
                    auto *self = (Screen *)region->userdata;
                    self->require_frame |= (self->screen_buffer[offset] != data);
                    self->screen_buffer[offset] = data;
                }, emulator);

            region_buffer1.Setup(0x89000, buf_bytes, "Screen/Buffer1", this,
                [](MMURegion *region, size_t offset) {
                    offset -= region->base;
                    if (offset % ROW_SIZE >= ROW_SIZE_DISP) return (uint8_t)0;
                    return ((Screen *)region->userdata)->screen_buffer1[offset];
                },
                [](MMURegion *region, size_t offset, uint8_t data) {
                    offset -= region->base;
                    if (offset % ROW_SIZE >= ROW_SIZE_DISP) return;
                    auto *self = (Screen *)region->userdata;
                    self->require_frame |= (self->screen_buffer1[offset] != data);
                    self->screen_buffer1[offset] = data;
                }, emulator);
        } else {
            region_buffer.Setup(0xF800, buf_bytes, "Screen/Buffer", this,
                [](MMURegion *region, size_t offset) {
                    offset -= region->base;
                    if (offset % ROW_SIZE >= ROW_SIZE_DISP) return (uint8_t)0;
                    auto *self = (Screen *)region->userdata;
                    return (self->screen_select & 0x04)
                        ? self->screen_buffer1[offset]
                        : self->screen_buffer[offset];
                },
                [](MMURegion *region, size_t offset, uint8_t data) {
                    offset -= region->base;
                    if (offset % ROW_SIZE >= ROW_SIZE_DISP) return;
                    auto *self = (Screen *)region->userdata;
                    if (self->screen_select & 0x04) {
                        self->require_frame |= (self->screen_buffer1[offset] != data);
                        self->screen_buffer1[offset] = data;
                    } else {
                        self->require_frame |= (self->screen_buffer[offset] != data);
                        self->screen_buffer[offset] = data;
                    }
                }, emulator);
        }

        region_range.Setup(0xF030, 1, "Screen/Range", this,
            DefaultRead<uint8_t, 0x07, &Screen::screen_range>,
            SetRequireFrameWrite<uint8_t, 0x07, &Screen::screen_range>, emulator);

        region_mode.Setup(0xF031, 1, "Screen/Mode", this,
            DefaultRead<uint8_t, 0x07, &Screen::screen_mode>,
            SetRequireFrameWrite<uint8_t, 0x07, &Screen::screen_mode>, emulator);

        region_contrast.Setup(0xF032, 1, "Screen/Contrast", this,
            DefaultRead<uint8_t, 0x3F, &Screen::screen_contrast>,
            SetRequireFrameWrite<uint8_t, 0x3F, &Screen::screen_contrast>, emulator);
    }

    void Screen::Uninitialise()
    {
        region_buffer.Kill();
        region_buffer1.Kill();
        region_select.Kill();
        region_range.Kill();
        region_mode.Kill();
        region_contrast.Kill();
        delete[] screen_buffer;
        delete[] screen_buffer1;
        screen_buffer = nullptr;
        screen_buffer1 = nullptr;
    }

    void Screen::Frame()
    {
        require_frame = false;
    }

    uint32_t Screen::IndicatorMask() const
    {
        if (!screen_buffer || !screen_buffer1)
            return 0;
        if (screen_mode != 5 && screen_mode != 6)
            return 0;

        uint32_t mask = 0;
        for (uint32_t i = 0; i < 20u; ++i) {
            const uint8_t off = kCw2Indicators[i].offset;
            const uint8_t bit = kCw2Indicators[i].mask;
            if (((screen_buffer[off] | screen_buffer1[off]) & bit) != 0)
                mask |= (1u << i);
        }
        return mask;
    }

    size_t Screen::CopyGrayLevels(uint8_t *out, size_t out_size) const
    {
        constexpr size_t kWidth = static_cast<size_t>(ROW_SIZE_DISP) * 8u;
        constexpr size_t kTotal = static_cast<size_t>(N_ROW) * kWidth;
        if (!out || out_size < kTotal || !screen_buffer || !screen_buffer1)
            return 0;

        if (screen_mode != 5) {
            std::memset(out, 0, kTotal);
            return kTotal;
        }

        for (int iy = 0; iy < N_ROW; ++iy) {
            const size_t row_off = static_cast<size_t>(iy) * ROW_SIZE + OFFSET;
            uint8_t *dst_row = out + static_cast<size_t>(iy) * kWidth;
            for (int ix = 0; ix < ROW_SIZE_DISP; ++ix) {
                const uint8_t b0 = screen_buffer[row_off + ix];
                const uint8_t b1 = screen_buffer1[row_off + ix];
                for (int bit = 0; bit < 8; ++bit) {
                    const uint8_t m = static_cast<uint8_t>(0x80u >> bit);
                    uint8_t level = 0;
                    if (b0 & m) level += 1u; // weight 1/3
                    if (b1 & m) level += 2u; // weight 2/3
                    dst_row[static_cast<size_t>(ix) * 8u + static_cast<size_t>(bit)] = level;
                }
            }
        }
        return kTotal;
    }
}
