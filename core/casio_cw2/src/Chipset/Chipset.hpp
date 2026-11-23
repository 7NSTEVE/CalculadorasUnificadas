#pragma once
#include "../Config.hpp"

#include "MMURegion.hpp"
#include "InterruptSource.hpp"

#include "../Peripheral/IOPorts.hpp"
#include "../Peripheral/ExternalInterrupts.hpp"
#include "../Peripheral/Flash.hpp"

#include <string>
#include <vector>
#include <forward_list>
#include <SDL.h>

namespace casioemux
{
    class Emulator;
    class CPU;
    class Coprocessor;
    class MMU;
    class Peripheral;

    class Chipset
    {
        enum InterruptIndex
        {
            INT_CHECKFLAG,
            INT_RESET,
            INT_BREAK,
            INT_EMULATOR,
            INT_NONMASKABLE,
            INT_MASKABLE,
            INT_SOFTWARE = 64,
            INT_COUNT = 128
        };

        enum RunMode
        {
            RM_STOP,
            RM_HALT,
            RM_RUN
        };
        RunMode run_mode = RM_RUN;

        std::forward_list<Peripheral *> peripherals;

        size_t pending_interrupt_count = 0;
        bool interrupts_active[INT_COUNT]{};
        void AcceptInterrupt();
        void RaiseSoftware(size_t index);

        void ConstructPeripherals();
        void DestructPeripherals();

        void ConstructClockGenerator();
        void GenerateTickForClock();
        void ResetClockGenerator();
        void DestructClockGenerator();

        void ConstructInterruptSFR();
        void ResetInterruptSFR();
        void DestructInterruptSFR();
        MMURegion region_int_mask, region_int_pending;
        uint32_t data_int_mask = 0, data_int_pending = 0;
        static const size_t managed_interrupt_base = 4;

        MMURegion region_BLKCON;

        MMURegion region_FCON, region_LTBR, region_HTBR, region_LTBADJ;
        int LSCLKFreq = 16384;

        long long LSCLKTickCounter = 0, HSCLKTickCounter = 0, HSCLKTimeCounter = 0, LSCLKTimeCounter = 0, LSCLKThresh = 0;
        int LSCLKFreqAddition = 0;

        bool real_hardware = false;

    public:
        Chipset(Emulator &emulator);
        void Setup();
        ~Chipset();

        Emulator &emulator;
        CPU &cpu;
        Coprocessor &coprocessor;
        MMU &mmu;
        std::vector<unsigned char> rom_data;

        Flash *flash = nullptr;
        InterruptSource *MaskableInterrupts = nullptr;
        size_t EffectiveMICount = 0;

        IOPorts *ioport = nullptr;
        ExternalInterrupts *EXIhandle = nullptr;

        bool HWRemap = false;
        bool WDT_enabled = false;

        uint8_t data_BLKCON = 0, BLKCON_mask = 0;
        uint8_t data_EXICON = 0;

        uint8_t data_FCON = 0, data_LTBR = 0, data_HTBR = 0;
        uint16_t data_LTBADJ = 0;

        uint8_t LSCLK_output = 0;
        uint8_t LTBC_H = 0;
        uint8_t LSCLK_output_H = 0;
        uint8_t HSCLK_output = 0;

        int ClockDiv = 1;
        bool LSCLKMode = false;

        bool LSCLKTick = false, HSCLKTick = false;
        bool LTBCReset = false, HTBCReset = false;

        const int HTBROutputCount = 128;

        bool Port0Inputlevel[3]{}, Port1Inputlevel[7]{};
        bool Port0Outputlevel[3]{}, Port1Outputlevel[7]{};
        bool UserInput_level_Port0[3]{}, UserInput_level_Port1[7]{};
        bool UserInput_state_Port0[3]{}, UserInput_state_Port1[7]{};

        bool SegmentAccess = false;

        void SetupInternals();
        void Reset();
        void Break();
        void Halt();
        void Stop();
        bool GetRunningState();
        void RaiseEmulator();
        void RequestNonmaskable();
        void RaiseNonmaskable();
        void ResetNonmaskable();
        void RaiseMaskable(size_t index);
        void ResetMaskable(size_t index);
        void SetInterruptPendingSFR(size_t index, bool val);
        bool GetInterruptPendingSFR(size_t index);
        void HandleInterrupts();
        void InstCallBack();
        void InputToPort(int, int, bool);
        void RemovePortInput(int, int);

        void Tick();
        bool GetRequireFrame();
        void Frame();
        void UIEvent(SDL_Event &event);

        friend class CPU;
    };
}
