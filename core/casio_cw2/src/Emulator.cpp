#include "Emulator.hpp"

#include "Chipset/Chipset.hpp"
#include "Logger.hpp"

namespace casioemux
{
    Emulator::Emulator(const ModelDefinition &definition,
                       const unsigned char *rom_bytes, std::size_t rom_size,
                       const unsigned char *initial_ram_bytes, std::size_t initial_ram_size)
        : chipset(*new Chipset(*this))
    {
        running = true;
        paused = false;
        cycle_remainder = 0;

        model_strings["model_name"] = definition.model_name;
        model_ints["hardware_id"] = static_cast<int>(definition.hardware_id);
        model_ints["real_hardware"] = definition.real_hardware ? 1 : 0;
        model_ints["csr_mask"] = definition.csr_mask;
        model_ints["pd_value"] = definition.pd_value;

        hardware_id = definition.hardware_id;
        cycles_per_second = hardware_id == HW_ES_PLUS
            ? 512u * 1024u
            : (hardware_id == HW_CLASSWIZ ? 2u * 1024u * 1024u : 4u * 1024u * 1024u);

        if (rom_bytes && rom_size > 0)
            rom_image.assign(rom_bytes, rom_bytes + rom_size);
        if (initial_ram_bytes && initial_ram_size > 0)
            initial_ram.assign(initial_ram_bytes, initial_ram_bytes + initial_ram_size);

        chipset.Setup();
        chipset.SetupInternals();
        chipset.Reset();
    }

    Emulator::~Emulator()
    {
        running = false;
        delete &chipset;
    }

    ModelInfo Emulator::GetModelInfo(std::string key)
    {
        return ModelInfo(*this, std::move(key));
    }

    std::string Emulator::GetModelFilePath(std::string relative_path)
    {
        return relative_path;
    }

    void Emulator::Advance(unsigned int milliseconds)
    {
        if (paused || !running || milliseconds == 0)
            return;

        cycle_remainder += static_cast<Uint64>(cycles_per_second) * milliseconds;
        Uint64 cycles_to_emulate = cycle_remainder / 1000u;
        cycle_remainder -= cycles_to_emulate * 1000u;

        for (Uint64 ix = 0; ix != cycles_to_emulate; ++ix)
            chipset.Tick();
    }

    void Emulator::Reset()
    {
        chipset.Reset();
    }
}
