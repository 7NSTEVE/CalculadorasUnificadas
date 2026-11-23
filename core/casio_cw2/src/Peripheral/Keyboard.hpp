#pragma once
#include "../Config.hpp"

#include "Peripheral.hpp"
#include "../Chipset/MMURegion.hpp"

#include <cstdint>

namespace casioemux
{
    class Keyboard : public Peripheral
    {
        MMURegion region_ko_mask, region_ko, region_ki, region_input_mode, region_input_filter;
        uint16_t keyboard_out = 0, keyboard_out_mask = 0;
        uint8_t keyboard_in = 0xFF, input_mode = 0, input_filter = 0;
        uint8_t keyboard_ghost[8]{}, ki_ghost[8]{};
        uint8_t keyboard_in_last = 0xFF, input_filter_last = 0;

        bool real_hardware = false;
        MMURegion region_ready_emu, region_ko_emu, region_ki_emu, region_pd_emu;
        uint8_t keyboard_ready_emu = 1, keyboard_out_emu = 0, keyboard_in_emu = 0, keyboard_pd_emu = 0;
        int emu_ki_readcount = 0, emu_ko_readcount = 0;

        size_t EXI0INT = 0;

        struct Button
        {
            enum ButtonType
            {
                BT_NONE,
                BT_BUTTON,
                BT_POWER
            } type = BT_NONE;
            uint8_t ko_bit = 0;
            uint8_t ki_bit = 0;
            bool pressed = false;
            bool stuck = false;
        } buttons[64];

        bool p0 = false, p1 = false, p146 = false;

    public:
        using Peripheral::Peripheral;

        void Initialise() override;
        void Uninitialise() override;
        void Reset() override;
        void Tick() override;
        void Frame() override;

        void PressCode(uint8_t code);
        void ReleaseAll();
        void RecalculateKI();
        void RecalculateGhost();

    private:
        void PressButton(Button &button);
    };
}
