#pragma once
#include "../Config.hpp"

#include "Peripheral.hpp"
#include "../Chipset/MMURegion.hpp"

namespace casioemu
{
	class BatteryBackedRAM : public Peripheral
	{
		MMURegion region, region_2;
		uint8_t *ram_buffer;
		size_t ram_size;
		bool ram_file_requested;

	public:
		using Peripheral::Peripheral;

		void Initialise() override;
		void Uninitialise() override;
		void SaveRAMImage();
		void LoadRAMImage();

		void SaveState(StateWriter &writer) const override;
		void LoadState(StateReader &reader) override;

		/// Solo lectura: el anfitrion mira, no toca.
		const uint8_t *Buffer() const { return ram_buffer; }
		size_t Size() const { return ram_size; }
	};
}

