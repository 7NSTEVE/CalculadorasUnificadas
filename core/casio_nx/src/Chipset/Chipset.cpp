#include "Chipset.hpp"

#include "../Data/HardwareId.hpp"
#include "../Emulator.hpp"
#include "../Logger.hpp"
#include "CPU.hpp"
#include "MMU.hpp"

#include "../Peripheral/ROMWindow.hpp"
#include "../Peripheral/BatteryBackedRAM.hpp"
#include "../Peripheral/Screen.hpp"
#include "../Peripheral/Keyboard.hpp"
#include "../Peripheral/StandbyControl.hpp"
#include "../Peripheral/Miscellaneous.hpp"
#include "../Peripheral/Timer.hpp"

#include <fstream>
#include <algorithm>
#include <cstring>

namespace casioemu
{
	Chipset::Chipset(Emulator &_emulator) : emulator(_emulator), cpu(*new CPU(emulator)), mmu(*new MMU(emulator))
	{
	}

	void Chipset::Setup()
	{
		for (size_t ix = 0; ix != INT_COUNT; ++ix)
			interrupts_active[ix] = false;
		pending_interrupt_count = 0;

		cpu.SetMemoryModel(CPU::MM_LARGE);

		std::initializer_list<int> segments_es_plus {0, 1, 8}, segments_classwiz {0, 1, 2, 3, 4, 5};
		for (auto segment_index : emulator.hardware_id == HW_ES_PLUS ? segments_es_plus : segments_classwiz)
			mmu.GenerateSegmentDispatch(segment_index);

		ConstructPeripherals();
	}

	Chipset::~Chipset()
	{
		DestructPeripherals();
		DestructInterruptSFR();

		delete &mmu;
		delete &cpu;
	}

	void Chipset::ConstructInterruptSFR()
	{
		region_int_mask.Setup(0xF010, 2, "Chipset/InterruptMask", &data_int_mask, MMURegion::DefaultRead<uint16_t, interrupt_bitfield_mask>, MMURegion::DefaultWrite<uint16_t, interrupt_bitfield_mask>, emulator);

		region_int_pending.Setup(0xF014, 2, "Chipset/InterruptPending", &data_int_pending, MMURegion::DefaultRead<uint16_t, interrupt_bitfield_mask>, MMURegion::DefaultWrite<uint16_t, interrupt_bitfield_mask>, emulator);
	}

	void Chipset::DestructInterruptSFR()
	{
		region_int_pending.Kill();
		region_int_mask.Kill();
	}

	void Chipset::ConstructPeripherals()
	{
		peripherals.push_front(new ROMWindow(emulator));
		peripherals.push_front(new BatteryBackedRAM(emulator));
		peripherals.push_front(CreateScreen(emulator));
		peripherals.push_front(new Keyboard(emulator));
		peripherals.push_front(new StandbyControl(emulator));
		peripherals.push_front(new Miscellaneous(emulator));
		peripherals.push_front(new Timer(emulator));
	}

	void Chipset::DestructPeripherals()
	{
		for (auto &peripheral : peripherals)
		{
			peripheral->Uninitialise();
			delete peripheral;
		}
	}

	void Chipset::SetupInternals()
	{
		// UNICO CAMBIO respecto al original en todo Chipset/: la ROM ya no se lee de
		// disco, llega en memoria desde el anfitrion. En iOS la imagen viene incrustada
		// en el binario de la app y no hay ruta de fichero que valga.
		rom_data = emulator.GetRomImage();
		if (rom_data.empty())
			PANIC("no se suministro imagen de ROM\n");

		for (auto &peripheral : peripherals)
			peripheral->Initialise();

		ConstructInterruptSFR();

		cpu.SetupInternals();
		mmu.SetupInternals();
	}

	void Chipset::Reset()
	{
		data_int_mask = 0;
		data_int_pending = 0;

		for (auto &peripheral : peripherals)
			peripheral->Reset();

		cpu.Reset();

		interrupts_active[INT_RESET] = true;
		pending_interrupt_count = 1;

		run_mode = RM_RUN;
	}

	void Chipset::Break()
	{
		if (cpu.GetExceptionLevel() > 1)
		{
			Reset();
			return;
		}

		if (interrupts_active[INT_BREAK])
			return;
		interrupts_active[INT_BREAK] = true;
		pending_interrupt_count++;
	}

	void Chipset::Halt()
	{
		run_mode = RM_HALT;
	}

	void Chipset::Stop()
	{
		run_mode = RM_STOP;
	}

	void Chipset::RaiseEmulator()
	{
		if (interrupts_active[INT_EMULATOR])
			return;
		interrupts_active[INT_EMULATOR] = true;
		pending_interrupt_count++;
	}

	void Chipset::RaiseNonmaskable()
	{
		if (interrupts_active[INT_NONMASKABLE])
			return;
		interrupts_active[INT_NONMASKABLE] = true;
		pending_interrupt_count++;
	}

	void Chipset::RaiseMaskable(size_t index)
	{
		if (index < INT_MASKABLE || index >= INT_SOFTWARE)
			PANIC("%zu is not a valid maskable interrupt index\n", index);
		if (interrupts_active[index])
			return;
		interrupts_active[index] = true;
		pending_interrupt_count++;
	}

	void Chipset::RaiseSoftware(size_t index)
	{
		index += 0x40;
		if (interrupts_active[index])
			return;
		interrupts_active[index] = true;
		pending_interrupt_count++;
	}

	void Chipset::AcceptInterrupt()
	{
		size_t old_exception_level = cpu.GetExceptionLevel();

		size_t index = 0;
		// * Reset has priority over everything.
		if (interrupts_active[INT_RESET])
			index = INT_RESET;
		// * Software interrupts are immediately accepted.
		if (!index)
			for (size_t ix = INT_SOFTWARE; ix != INT_COUNT; ++ix)
				if (interrupts_active[ix])
				{
					if (old_exception_level > 1)
						PANIC("software interrupt while exception level was greater than 1\n");
					index = ix;
					break;
				}
		// * No need to check the old exception level as NMICI has an exception level of 3.
		if (!index && interrupts_active[INT_EMULATOR])
			index = INT_EMULATOR;
		// * No need to check the old exception level as BRK initiates a reset if
		//   the currect exception level is greater than 1.
		if (!index && interrupts_active[INT_BREAK])
			index = INT_BREAK;
		if (!index && interrupts_active[INT_NONMASKABLE] && old_exception_level <= 2)
			index = INT_NONMASKABLE;
		if (!index && old_exception_level <= 1)
			for (size_t ix = INT_MASKABLE; ix != INT_SOFTWARE; ++ix)
				if (interrupts_active[ix])
				{
					index = ix;
					break;
				}

		size_t exception_level;
		switch (index)
		{
		case INT_RESET:
			exception_level = 0;
			break;

		case INT_BREAK:
		case INT_NONMASKABLE:
			exception_level = 2;
			break;

		case INT_EMULATOR:
			exception_level = 3;
			break;

		default:
			exception_level = 1;
			break;
		}

		if (index >= INT_MASKABLE && index < INT_SOFTWARE)
		{
			if (InterruptEnabledBySFR(index))
			{
				SetInterruptPendingSFR(index);
				if (cpu.GetMasterInterruptEnable())
					cpu.Raise(exception_level, index);
			}
		}
		else
		{
			cpu.Raise(exception_level, index);
		}

		run_mode = RM_RUN;

		// * TODO: introduce delay

		interrupts_active[index] = false;
		pending_interrupt_count--;
	}

	bool Chipset::InterruptEnabledBySFR(size_t index)
	{
		return data_int_mask & (1 << (index - managed_interrupt_base));
	}

	bool Chipset::GetInterruptPendingSFR(size_t index)
	{
		return data_int_pending & (1 << (index - managed_interrupt_base));
	}

	void Chipset::SetInterruptPendingSFR(size_t index)
	{
		data_int_pending |= (1 << (index - managed_interrupt_base));
	}

	bool Chipset::GetRequireFrame()
	{
		return std::any_of(peripherals.begin(), peripherals.end(), [](Peripheral *peripheral){
			return peripheral->GetRequireFrame();
		});
	}

	void Chipset::Frame()
	{
		for (auto peripheral : peripherals)
			peripheral->Frame();
	}

	void Chipset::Tick()
	{
		// * TODO: decrement delay counter, return if it's not 0

		for (auto peripheral : peripherals)
			peripheral->Tick();

		if (pending_interrupt_count)
			AcceptInterrupt();

		for (auto peripheral : peripherals)
			peripheral->TickAfterInterrupts();

		if (run_mode == RM_RUN)
			cpu.Next();
	}

	void Chipset::UIEvent(SDL_Event &event)
	{
		for (auto peripheral : peripherals)
			peripheral->UIEvent(event);
	}

	void Chipset::SaveState(StateWriter &writer) const
	{
		writer.Tag(TAG_CHIPSET);
		writer.U8((std::uint8_t)run_mode);
		writer.U32((std::uint32_t)pending_interrupt_count);
		for (int ix = 0; ix != INT_COUNT; ++ix)
			writer.Bool(interrupts_active[ix]);
		writer.U16(data_int_mask);
		writer.U16(data_int_pending);

		cpu.SaveState(writer);

		// El numero de perifericos va delante para que restaurar en una maquina de
		// otro modelo se detecte aqui y no dentro de un periferico equivocado.
		std::uint32_t count = 0;
		for (Peripheral *peripheral : peripherals)
		{
			(void)peripheral;
			++count;
		}
		writer.U32(count);
		for (Peripheral *peripheral : peripherals)
			peripheral->SaveState(writer);

		writer.Tag(TAG_END);
	}

	void Chipset::LoadState(StateReader &reader)
	{
		reader.Tag(TAG_CHIPSET);
		run_mode = (RunMode)reader.U8();
		pending_interrupt_count = reader.U32();
		for (int ix = 0; ix != INT_COUNT; ++ix)
			interrupts_active[ix] = reader.Bool();
		data_int_mask = reader.U16();
		data_int_pending = reader.U16();

		cpu.LoadState(reader);

		std::uint32_t count = 0;
		for (Peripheral *peripheral : peripherals)
		{
			(void)peripheral;
			++count;
		}
		if (reader.U32() != count)
		{
			reader.Fail();
			return;
		}
		for (Peripheral *peripheral : peripherals)
			peripheral->LoadState(reader);

		reader.Tag(TAG_END);
	}
}
