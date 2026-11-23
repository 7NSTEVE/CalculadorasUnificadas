#include "BatteryBackedRAM.hpp"

#include "../Data/HardwareId.hpp"
#include "../Chipset/MMU.hpp"
#include "../Emulator.hpp"
#include "../Chipset/Chipset.hpp"

#include <algorithm>
#include <cstring>

namespace casioemux
{
    void BatteryBackedRAM::Initialise()
    {
        bool real_hardware = emulator.GetModelInfo("real_hardware") != 0;
        switch (emulator.hardware_id)
        {
        case HW_ES_PLUS:
            ram_size = 0xE00;
            break;
        case HW_CLASSWIZ:
            ram_size = 0x2000;
            break;
        case HW_CLASSWIZ_II:
            ram_size = 0x6000;
            break;
        case HW_FX_5800P:
            ram_size = 0x8E00;
            break;
        }
        if (!real_hardware)
            ram_size += 0x100;

        ram_buffer = new uint8_t[ram_size];
        std::memset(ram_buffer, 0, ram_size);

        if (!emulator.initial_ram.empty()) {
            const size_t n = std::min(ram_size, emulator.initial_ram.size());
            std::memcpy(ram_buffer, emulator.initial_ram.data(), n);
        }

        const size_t base1 = (emulator.hardware_id == HW_ES_PLUS || emulator.hardware_id == HW_FX_5800P)
            ? 0x8000u
            : (emulator.hardware_id == HW_CLASSWIZ ? 0xD000u : 0x9000u);
        const size_t size1 = (emulator.hardware_id == HW_ES_PLUS || emulator.hardware_id == HW_FX_5800P)
            ? 0x0E00u
            : (emulator.hardware_id == HW_CLASSWIZ ? 0x2000u : 0x6000u);

        region.Setup(base1, size1, "BatteryBackedRAM", ram_buffer,
            [](MMURegion *r, size_t offset) {
                return ((uint8_t *)r->userdata)[offset - r->base];
            },
            [](MMURegion *r, size_t offset, uint8_t data) {
                ((uint8_t *)r->userdata)[offset - r->base] = data;
            }, emulator);

        if (!real_hardware) {
            const size_t base2 = emulator.hardware_id == HW_ES_PLUS
                ? 0x9800u
                : (emulator.hardware_id == HW_CLASSWIZ ? 0x49800u : 0x89800u);
            region_2.Setup(base2, 0x0100u, "BatteryBackedRAM/2", ram_buffer + ram_size - 0x100u,
                [](MMURegion *r, size_t offset) {
                    return ((uint8_t *)r->userdata)[offset - r->base];
                },
                [](MMURegion *r, size_t offset, uint8_t data) {
                    ((uint8_t *)r->userdata)[offset - r->base] = data;
                }, emulator);
        } else if (emulator.hardware_id == HW_FX_5800P) {
            region_2.Setup(0x40000u, 0x8000u, "BatteryBackedRAM/2", ram_buffer + 0xE00u,
                [](MMURegion *r, size_t offset) {
                    return ((uint8_t *)r->userdata)[offset - r->base];
                },
                [](MMURegion *r, size_t offset, uint8_t data) {
                    ((uint8_t *)r->userdata)[offset - r->base] = data;
                }, emulator);
        }
    }

    void BatteryBackedRAM::Uninitialise()
    {
        region.Kill();
        region_2.Kill();
        delete[] ram_buffer;
        ram_buffer = nullptr;
        ram_size = 0;
    }

    bool BatteryBackedRAM::ImportRam(const uint8_t *data, size_t size)
    {
        if (!ram_buffer || !data || size != ram_size)
            return false;
        std::memcpy(ram_buffer, data, ram_size);
        return true;
    }

    size_t BatteryBackedRAM::ExportRam(uint8_t *out, size_t out_size) const
    {
        if (!ram_buffer || ram_size == 0)
            return 0;
        if (!out || out_size < ram_size)
            return ram_size;
        std::memcpy(out, ram_buffer, ram_size);
        return ram_size;
    }
}
