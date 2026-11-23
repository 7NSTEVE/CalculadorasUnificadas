#include "Keyboard.hpp"

#include "../Data/HardwareId.hpp"
#include "../Chipset/MMU.hpp"
#include "../Emulator.hpp"
#include "../Chipset/Chipset.hpp"

namespace casioemux
{
    void Keyboard::Initialise()
    {
        require_frame = false;
        real_hardware = emulator.GetModelInfo("real_hardware") != 0;
        clock_type = CLOCK_UNDEFINED;

        region_ki.Setup(0xF040, 1, "Keyboard/KI", &keyboard_in,
            MMURegion::DefaultRead<uint8_t>, MMURegion::IgnoreWrite, emulator);

        region_input_mode.Setup(0xF041, 1, "Keyboard/InputMode", this,
            [](MMURegion *region, size_t) {
                return ((Keyboard *)region->userdata)->input_mode;
            },
            [](MMURegion *region, size_t, uint8_t data) {
                auto *kb = (Keyboard *)region->userdata;
                kb->input_mode = data;
                kb->RecalculateKI();
            }, emulator);

        region_input_filter.Setup(0xF042, 1, "Keyboard/InputFilter", &input_filter,
            MMURegion::DefaultRead<uint8_t>, MMURegion::DefaultWrite<uint8_t>, emulator);

        region_ko_mask.Setup(0xF044, 2, "Keyboard/KOMask", this,
            [](MMURegion *region, size_t offset) {
                offset -= region->base;
                auto *kb = (Keyboard *)region->userdata;
                return (uint8_t)((kb->keyboard_out_mask & 0x03FFu) >> (offset * 8));
            },
            [](MMURegion *region, size_t offset, uint8_t data) {
                offset -= region->base;
                auto *kb = (Keyboard *)region->userdata;
                kb->keyboard_out_mask &= (uint16_t)(~(((uint16_t)0xFFu) << (offset * 8)));
                kb->keyboard_out_mask |= (uint16_t)(((uint16_t)data) << (offset * 8));
                kb->keyboard_out_mask &= 0x03FFu;
                if (!offset) kb->RecalculateKI();
            }, emulator);

        region_ko.Setup(0xF046, 2, "Keyboard/KO", this,
            [](MMURegion *region, size_t offset) {
                offset -= region->base;
                auto *kb = (Keyboard *)region->userdata;
                return (uint8_t)((kb->keyboard_out & 0x83FFu) >> (offset * 8));
            },
            [](MMURegion *region, size_t offset, uint8_t data) {
                offset -= region->base;
                auto *kb = (Keyboard *)region->userdata;
                kb->keyboard_out &= (uint16_t)(~(((uint16_t)0xFFu) << (offset * 8)));
                kb->keyboard_out |= (uint16_t)(((uint16_t)data) << (offset * 8));
                kb->keyboard_out &= 0x83FFu;
                if (!offset) kb->RecalculateKI();
            }, emulator);

        if (!real_hardware) {
            keyboard_pd_emu = (uint8_t)(int)emulator.GetModelInfo("pd_value");
            keyboard_ready_emu = 1;
            emu_ki_readcount = 0;
            emu_ko_readcount = 0;
            const size_t base = emulator.hardware_id == HW_ES_PLUS
                ? 0u
                : (emulator.hardware_id == HW_CLASSWIZ ? 0x40000u : 0x80000u);

            region_ready_emu.Setup(base + 0x8E00u, 1, "Keyboard/ReadyStatusEmulator", this,
                [](MMURegion *region, size_t) {
                    auto *kb = (Keyboard *)region->userdata;
                    if (kb->keyboard_ready_emu == 8) {
                        uint8_t emu_ki = kb->keyboard_in_emu;
                        uint8_t emu_ko = kb->keyboard_out_emu;
                        if (++kb->emu_ki_readcount > 1) kb->keyboard_in_emu = 0;
                        if (++kb->emu_ko_readcount > 1) kb->keyboard_out_emu = 0;
                        return (uint8_t)((emu_ki == 4 && emu_ko == 16) ? 1 : 0);
                    }
                    return kb->keyboard_ready_emu;
                },
                [](MMURegion *region, size_t, uint8_t data) {
                    ((Keyboard *)region->userdata)->keyboard_ready_emu = data;
                }, emulator);

            region_ki_emu.Setup(base + 0x8E01u, 1, "Keyboard/KIEmulator", this,
                [](MMURegion *region, size_t) {
                    auto *kb = (Keyboard *)region->userdata;
                    uint8_t value = kb->keyboard_in_emu;
                    if (++kb->emu_ki_readcount > 1) kb->keyboard_in_emu = 0;
                    return value;
                }, MMURegion::IgnoreWrite, emulator);

            region_ko_emu.Setup(base + 0x8E02u, 1, "Keyboard/KOEmulator", this,
                [](MMURegion *region, size_t) {
                    auto *kb = (Keyboard *)region->userdata;
                    uint8_t value = kb->keyboard_out_emu;
                    if (++kb->emu_ko_readcount > 1) kb->keyboard_out_emu = 0;
                    return value;
                }, MMURegion::IgnoreWrite, emulator);

            region_pd_emu.Setup(0xF050u, 1, "Keyboard/PdValue", &keyboard_pd_emu,
                MMURegion::DefaultRead<uint8_t>, MMURegion::IgnoreWrite, emulator);
        }

        // Pre-populate full 8x8 matrix without Lua button_map
        for (size_t ix = 0; ix < 63; ++ix) {
            buttons[ix].type = Button::BT_BUTTON;
            buttons[ix].ko_bit = static_cast<uint8_t>(1u << (ix >> 3));
            buttons[ix].ki_bit = static_cast<uint8_t>(1u << (ix & 7));
            buttons[ix].pressed = false;
            buttons[ix].stuck = false;
        }
        buttons[63].type = Button::BT_POWER;
        buttons[63].ko_bit = 0;
        buttons[63].ki_bit = 0;
        buttons[63].pressed = false;
        buttons[63].stuck = false;
    }

    void Keyboard::Uninitialise()
    {
        region_ki.Kill();
        region_input_mode.Kill();
        region_input_filter.Kill();
        region_ko_mask.Kill();
        region_ko.Kill();
        region_ready_emu.Kill();
        region_ki_emu.Kill();
        region_ko_emu.Kill();
        region_pd_emu.Kill();
    }

    void Keyboard::Reset()
    {
        p0 = p1 = p146 = false;
        input_mode = 0;
        keyboard_out = 0;
        keyboard_out_mask = 0;
        keyboard_in_last = 0xFF;
        input_filter_last = 0;
        if (!real_hardware) {
            keyboard_in_emu = 0;
            emu_ki_readcount = 0;
            keyboard_out_emu = 0;
            emu_ko_readcount = 0;
        }
        RecalculateGhost();
    }

    void Keyboard::Tick()
    {
        if (!real_hardware) {
            if (keyboard_ready_emu & 0x0C)
                emulator.chipset.MaskableInterrupts[EXI0INT].TryRaise();
            return;
        }

        switch (emulator.chipset.data_EXICON & 0x03) {
        case 0:
            input_filter_last &= input_filter;
            if (keyboard_in_last & input_filter_last & ~(keyboard_in & input_filter_last))
                emulator.chipset.MaskableInterrupts[EXI0INT].TryRaise();
            break;
        case 1:
            input_filter_last &= input_filter;
            if (keyboard_in & input_filter_last & ~(keyboard_in_last & input_filter_last))
                emulator.chipset.MaskableInterrupts[EXI0INT].TryRaise();
            break;
        case 2:
            if (input_filter & keyboard_in)
                emulator.chipset.MaskableInterrupts[EXI0INT].TryRaise();
            break;
        case 3:
            if (input_filter & ~keyboard_in)
                emulator.chipset.MaskableInterrupts[EXI0INT].TryRaise();
            break;
        default:
            break;
        }
        input_filter_last = input_filter;
        keyboard_in_last = keyboard_in;
    }

    void Keyboard::Frame()
    {
        require_frame = false;
    }

    void Keyboard::PressButton(Button &button)
    {
        const bool old_pressed = button.pressed;
        button.pressed = true;
        require_frame = true;

        if (button.type == Button::BT_POWER && !old_pressed) {
            emulator.chipset.Reset();
            return;
        }

        if (button.type == Button::BT_BUTTON && !old_pressed) {
            if (real_hardware) {
                RecalculateGhost();
            } else {
                keyboard_in_emu = button.ki_bit;
                keyboard_out_emu = button.ko_bit;
                emu_ki_readcount = 0;
                emu_ko_readcount = 0;
                emulator.chipset.MaskableInterrupts[EXI0INT].TryRaise();
            }
        }
    }

    void Keyboard::PressCode(uint8_t code)
    {
        if (code == 0xFF) {
            PressButton(buttons[63]);
            return;
        }
        const size_t idx = static_cast<size_t>(((code >> 1) & 0x38u) | (code & 0x07u));
        if (idx < 63) {
            PressButton(buttons[idx]);
        }
    }

    void Keyboard::ReleaseAll()
    {
        bool had_effect = false;
        for (auto &button : buttons) {
            if (button.pressed) {
                button.pressed = false;
                if (button.type == Button::BT_BUTTON)
                    had_effect = true;
            }
        }
        require_frame = true;
        if (!real_hardware) {
            keyboard_in_emu = 0;
            keyboard_out_emu = 0;
        } else if (had_effect) {
            RecalculateGhost();
        }
    }

    void Keyboard::RecalculateGhost()
    {
        struct KOColumn { uint8_t connections; uint8_t KIRows; bool seen; } columns[8];
        for (size_t cx = 0; cx != 8; ++cx) {
            columns[cx] = {0, 0, false};
            for (size_t rx = 0; rx != 8; ++rx) {
                Button &b = buttons[cx * 8 + rx];
                if (b.type == Button::BT_BUTTON && b.pressed) {
                    columns[cx].KIRows |= (uint8_t)(1u << rx);
                    for (size_t ax = 0; ax != 8; ++ax) {
                        Button &s = buttons[ax * 8 + rx];
                        if (s.type == Button::BT_BUTTON && s.pressed)
                            columns[cx].connections |= (uint8_t)(1u << ax);
                    }
                }
            }
        }
        for (size_t gx = 0; gx != 8; ++gx) ki_ghost[gx] = 0;
        for (size_t cx = 0; cx != 8; ++cx) {
            if (!columns[cx].seen) {
                uint8_t to_visit = (uint8_t)(1u << cx);
                uint8_t ghost_mask = (uint8_t)(1u << cx);
                columns[cx].seen = true;
                while (to_visit) {
                    uint8_t next = 0;
                    for (size_t vx = 0; vx != 8; ++vx) {
                        if (to_visit & (1u << vx)) {
                            for (size_t sx = 0; sx != 8; ++sx) {
                                if ((columns[vx].connections & (1u << sx)) && !columns[sx].seen) {
                                    next |= (uint8_t)(1u << sx);
                                    ghost_mask |= (uint8_t)(1u << sx);
                                    columns[cx].KIRows |= columns[sx].KIRows;
                                    columns[sx].seen = true;
                                }
                            }
                        }
                    }
                    to_visit = next;
                }
                for (size_t gx = 0; gx != 8; ++gx) {
                    if (ghost_mask & (1u << gx)) keyboard_ghost[gx] = ghost_mask;
                    if (columns[cx].KIRows & (1u << gx)) ki_ghost[gx] = columns[cx].KIRows;
                }
            }
        }
        RecalculateKI();
    }

    void Keyboard::RecalculateKI()
    {
        uint8_t ko_ghosted = 0;
        uint8_t ki_pulled_up = 0;
        for (size_t ix = 0; ix != 8; ++ix)
            if (keyboard_out & ~keyboard_out_mask & (1u << ix))
                ko_ghosted |= keyboard_ghost[ix];

        keyboard_in = (uint8_t)(~input_mode);
        for (auto &b : buttons)
            if (b.type == Button::BT_BUTTON && b.pressed && (b.ko_bit & ko_ghosted))
                keyboard_in &= (uint8_t)(~b.ki_bit);

        for (size_t ix = 0; ix != 8; ++ix)
            if (keyboard_in & (1u << ix))
                ki_pulled_up |= ki_ghost[ix];

        for (auto &b : buttons)
            if (b.type == Button::BT_BUTTON && b.pressed && (b.ki_bit & input_mode & ki_pulled_up))
                keyboard_in |= b.ki_bit;

        if ((keyboard_out & ~keyboard_out_mask & (1u << 7)) && p0) keyboard_in &= 0x7Fu;
        if ((keyboard_out & ~keyboard_out_mask & (1u << 8)) && p1) keyboard_in &= 0x7Fu;
        if ((keyboard_out & ~keyboard_out_mask & (1u << 9)) && p146) keyboard_in &= 0x7Fu;
    }
}
