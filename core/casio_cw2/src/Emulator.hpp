#pragma once
#include "Config.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>
#include <lua.hpp>

#include "Data/HardwareId.hpp"
#include "Data/ModelInfo.hpp"

namespace casioemux
{
    class Chipset;
    class CPU;
    class MMU;
    class Screen;
    class Keyboard;
    class BatteryBackedRAM;

    struct ModelDefinition
    {
        std::string model_name = "fx-991CW";
        HardwareId hardware_id = HW_CLASSWIZ_II;
        bool real_hardware = false;
        int csr_mask = 0x000F;
        int pd_value = 0x00;
    };

    class Emulator
    {
        unsigned int cycles_per_second = 4 * 1024 * 1024;
        Uint64 cycle_remainder = 0;
        bool running = true;
        bool paused = false;

    public:
        Emulator(const ModelDefinition &definition,
                 const unsigned char *rom_bytes, std::size_t rom_size,
                 const unsigned char *initial_ram_bytes = nullptr, std::size_t initial_ram_size = 0);
        ~Emulator();

        Emulator(const Emulator &) = delete;
        Emulator &operator=(const Emulator &) = delete;

        lua_State *lua_state = nullptr;
        int lua_model_ref = LUA_REFNIL;
        HardwareId hardware_id = HW_CLASSWIZ_II;
        bool realistic_simulation = false;
        bool strict_memory = false;

        std::map<std::string, std::string> model_strings;
        std::map<std::string, int> model_ints;
        std::vector<unsigned char> rom_image;
        std::vector<unsigned char> initial_ram;

        float BatteryVoltage = 1.5f;
        float SolarPanelVoltage = 1.5f;

        Chipset &chipset;
        Screen *screen = nullptr;
        Keyboard *keyboard = nullptr;
        BatteryBackedRAM *ram = nullptr;

        bool Running() const { return running; }
        bool GetPaused() const { return paused; }
        void SetPaused(bool p) { paused = p; }
        unsigned int GetCyclesPerSecond() const { return cycles_per_second; }
        void HandleMemoryError() {}
        void Shutdown() { running = false; }

        void Advance(unsigned int milliseconds);
        void Reset();

        ModelInfo GetModelInfo(std::string key);
        std::string GetModelFilePath(std::string relative_path);
    };
}
