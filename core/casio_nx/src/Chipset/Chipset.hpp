#pragma once
#include "../Config.hpp"

#include "MMURegion.hpp"
#include "Snapshot.hpp"

#include <string>
#include <vector>
#include <forward_list>
#include <SDL.h>

namespace casioemu
{
	class Emulator;
	class CPU;
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
		RunMode run_mode;

		std::forward_list<Peripheral *> peripherals;

		/**
		 * A bunch of internally used methods for encapsulation purposes.
		 */
		size_t pending_interrupt_count;
		bool interrupts_active[INT_COUNT];
		void AcceptInterrupt();
		void RaiseSoftware(size_t index);

		void ConstructPeripherals();
		void DestructPeripherals();

		void ConstructInterruptSFR();
		void DestructInterruptSFR();
		MMURegion region_int_mask, region_int_pending;
		uint16_t data_int_mask, data_int_pending;
		static const size_t managed_interrupt_base = 4, managed_interrupt_amount = 13;
		static const uint16_t interrupt_bitfield_mask = (1 << managed_interrupt_amount) - 1;

	public:
		Chipset(Emulator &emulator);
		void Setup(); // must be called after emulator.hardware_id is initialized
		~Chipset();

		Emulator &emulator;
		CPU &cpu;
		MMU &mmu;
		std::vector<unsigned char> rom_data;

		/**
		 * This exists because the Emulator that owns this Chipset is not ready
		 * to supply a ROM path upon construction. It has to call `LoadROM` later
		 * in its constructor.
		 */
		void SetupInternals();

		/**
		 * See 1.3.7 in the nX-U8 manual.
		 */
		void Reset();
		void Break();
		void Halt();
		void Stop();
		void RaiseEmulator();
		void RaiseNonmaskable();
		void RaiseMaskable(size_t index);
		bool InterruptEnabledBySFR(size_t index);
		void SetInterruptPendingSFR(size_t index);
		bool GetInterruptPendingSFR(size_t index);

		void Tick();
		bool GetRequireFrame();
		void Frame();
		void UIEvent(SDL_Event &event);

		/**
		 * Estado del chipset, del procesador y de todos los perifericos, en ese orden.
		 *
		 * Los perifericos se recorren en el orden de la lista, que es siempre el mismo
		 * para un modelo dado porque lo fija ConstructPeripherals. Cada uno escribe su
		 * propia marca, asi que un desajuste de orden se detecta al leer.
		 */
		void SaveState(StateWriter &writer) const;
		void LoadState(StateReader &reader);

		friend class CPU;
	};
}

