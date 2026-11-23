#pragma once
#include "../Config.hpp"

#include "Peripheral.hpp"
#include "../Chipset/MMURegion.hpp"

#include <cstddef>
#include <cstdint>

namespace casioemux
{
    class BatteryBackedRAM : public Peripheral
    {
        MMURegion region, region_2;
        size_t ram_size = 0;

    public:
        using Peripheral::Peripheral;
        uint8_t *ram_buffer = nullptr;

        void Initialise() override;
        void Uninitialise() override;

        size_t Size() const { return ram_size; }
        const uint8_t *Buffer() const { return ram_buffer; }
        bool ImportRam(const uint8_t *data, size_t size);
        size_t ExportRam(uint8_t *out, size_t out_size) const;
    };
}
