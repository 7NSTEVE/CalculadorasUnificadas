#include "BatteryBackedRAM.hpp"

#include "../Data/HardwareId.hpp"
#include "../Chipset/MMU.hpp"
#include "../Emulator.hpp"
#include "../Chipset/Chipset.hpp"
#include "../Logger.hpp"

#include <fstream>
#include <cerrno>
#include <system_error>

namespace casioemu
{
	void BatteryBackedRAM::Initialise()
	{
		bool real_hardware = emulator.GetModelInfo("real_hardware");
		switch (emulator.hardware_id)
		{
		case HW_ES_PLUS:
			ram_size = 0xE00;
			break;
		case HW_CLASSWIZ:
			ram_size = 0x2000;
			break;
		}
		if (!real_hardware)
			ram_size += 0x100;

		ram_buffer = new uint8_t[ram_size];
		for (size_t ix = 0; ix != ram_size; ++ix)
			ram_buffer[ix] = 0;

		// El anfitrion necesita poder mirar aqui para ensenar el contenido real de
		// las memorias A-F, X, Y y M, en vez de lo que la app crea recordar.
		emulator.ram = this;

		ram_file_requested = false;
		if (emulator.argv_map.find("ram") != emulator.argv_map.end())
		{
			ram_file_requested = true;

			if (emulator.argv_map.find("clean_ram") == emulator.argv_map.end())
				LoadRAMImage();
		}

		region.Setup(emulator.hardware_id == HW_ES_PLUS ? 0x8000 : 0xD000,
			emulator.hardware_id == HW_ES_PLUS ? 0x0E00 : 0x2000,
			"BatteryBackedRAM", ram_buffer, [](MMURegion *region, size_t offset) {
			return ((uint8_t *)region->userdata)[offset - region->base];
		}, [](MMURegion *region, size_t offset, uint8_t data) {
			((uint8_t *)region->userdata)[offset - region->base] = data;
		}, emulator);
		if (!real_hardware)
			region_2.Setup(emulator.hardware_id == HW_ES_PLUS ? 0x9800 : 0x49800, 0x0100,
				"BatteryBackedRAM/2", ram_buffer + ram_size - 0x100, [](MMURegion *region, size_t offset) {
				return ((uint8_t *)region->userdata)[offset - region->base];
			}, [](MMURegion *region, size_t offset, uint8_t data) {
				((uint8_t *)region->userdata)[offset - region->base] = data;
			}, emulator);
	}

	void BatteryBackedRAM::Uninitialise()
	{
		if (ram_file_requested && emulator.argv_map.find("preserve_ram") == emulator.argv_map.end())
			SaveRAMImage();

		delete[] ram_buffer;
	}

	void BatteryBackedRAM::SaveState(StateWriter &writer) const
	{
		Peripheral::SaveState(writer);
		writer.Tag(TAG_RAM);
		// El tamano va dentro para que restaurar en una maquina de otro modelo
		// (0xE00 vs 0x2000, mas 0x100 si la ROM viene del emulador) no escriba fuera.
		writer.U32((std::uint32_t)ram_size);
		writer.Raw(ram_buffer, ram_size);
	}

	void BatteryBackedRAM::LoadState(StateReader &reader)
	{
		Peripheral::LoadState(reader);
		reader.Tag(TAG_RAM);
		if (reader.U32() != (std::uint32_t)ram_size)
		{
			reader.Fail();
			return;
		}
		reader.Raw(ram_buffer, ram_size);
	}

	void BatteryBackedRAM::SaveRAMImage()
	{
		std::ofstream ram_handle(emulator.argv_map["ram"], std::ofstream::binary);
		if (ram_handle.fail())
		{
			const auto mensaje = std::system_category().message(errno);
			logger::Info("[BatteryBackedRAM] std::ofstream failed: %s\n", mensaje.c_str());
			return;
		}
		ram_handle.write((char *)ram_buffer, ram_size);
		if (ram_handle.fail())
		{
			const auto mensaje = std::system_category().message(errno);
			logger::Info("[BatteryBackedRAM] std::ofstream failed: %s\n", mensaje.c_str());
			return;
		}
	}

	void BatteryBackedRAM::LoadRAMImage()
	{
		std::ifstream ram_handle(emulator.argv_map["ram"], std::ifstream::binary);
		if (ram_handle.fail())
		{
			const auto mensaje = std::system_category().message(errno);
			logger::Info("[BatteryBackedRAM] std::ifstream failed: %s\n", mensaje.c_str());
			return;
		}
		ram_handle.read((char *)ram_buffer, ram_size);
		if (ram_handle.fail())
		{
			const auto mensaje = std::system_category().message(errno);
			logger::Info("[BatteryBackedRAM] std::ifstream failed: %s\n", mensaje.c_str());
			return;
		}
	}
}

