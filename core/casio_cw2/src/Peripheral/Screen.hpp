#pragma once
#include "../Config.hpp"

#include "Peripheral.hpp"
#include "../Chipset/MMURegion.hpp"

#include <cstddef>
#include <cstdint>

namespace casioemux
{
    class Screen : public Peripheral
    {
        static constexpr int N_ROW = 63;
        static constexpr int ROW_SIZE = 32;
        static constexpr int OFFSET = 32;
        static constexpr int ROW_SIZE_DISP = 24;

        MMURegion region_buffer, region_buffer1, region_contrast, region_mode, region_range, region_select;
        uint8_t *screen_buffer = nullptr;
        uint8_t *screen_buffer1 = nullptr;
        uint8_t screen_contrast = 0x10;
        uint8_t screen_mode = 0;
        uint8_t screen_range = 0;
        uint8_t screen_select = 0;

        template<typename value_type, value_type mask = (value_type)-1,
                 value_type Screen:: *member_ptr>
        static uint8_t DefaultRead(MMURegion *region, size_t offset)
        {
            auto this_obj = (Screen *)(region->userdata);
            value_type value = this_obj->*member_ptr;
            return (uint8_t)((value & mask) >> ((offset - region->base) * 8));
        }

        template<typename value_type, value_type mask = (value_type)-1,
                 value_type Screen:: *member_ptr, bool only_on_change = true>
        static void SetRequireFrameWrite(MMURegion *region, size_t offset, uint8_t data)
        {
            auto this_obj = (Screen *)(region->userdata);
            value_type &value = this_obj->*member_ptr;
            value_type old_value = value;
            value &= (value_type)(~(((value_type)0xFF) << ((offset - region->base) * 8)));
            value |= (value_type)(((value_type)data) << ((offset - region->base) * 8));
            value &= mask;
            if (only_on_change && old_value == value)
                return;
            this_obj->require_frame = true;
        }

    public:
        using Peripheral::Peripheral;

        void Initialise() override;
        void Uninitialise() override;
        void Frame() override;

        int Width() const { return ROW_SIZE_DISP * 8; }
        int Height() const { return N_ROW; }
        int Mode() const { return screen_mode; }
        int Contrast() const { return screen_contrast; }

        // Outputs 192*63 bytes in {0, 1, 2, 3} (0=bg, 1=light gray 1/3, 2=mid gray 2/3, 3=dark ink).
        size_t CopyGrayLevels(uint8_t *out, size_t out_size) const;
        // 20-bit indicator mask matching SPR_S..SPR_SUN (row 0 status strip).
        uint32_t IndicatorMask() const;
    };
}
