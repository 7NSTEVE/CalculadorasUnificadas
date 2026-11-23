#pragma once
#include "../Config.hpp"

#include "Snapshot.hpp"

namespace casioemu
{
	class Emulator;

	class InterruptSource
	{
		Emulator *emulator;
		bool raise_success, setup_done;
		size_t interrupt_index;

	public:
		InterruptSource();
		void Setup(size_t interrupt_index, Emulator &_emulator);
		bool Enabled();
		bool TryRaise();
		bool Success();

		/**
		 * Solo `raise_success` es estado: el indice y el emulador se fijan al montar
		 * la maquina y son identicos en cualquier instancia del mismo modelo.
		 */
		void SaveState(StateWriter &writer) const
		{
			writer.Bool(raise_success);
		}

		void LoadState(StateReader &reader)
		{
			raise_success = reader.Bool();
		}
	};
}

