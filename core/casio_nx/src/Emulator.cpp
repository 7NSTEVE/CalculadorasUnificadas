#include "Emulator.hpp"

#include "Chipset/Chipset.hpp"
#include "Logger.hpp"

namespace casioemu
{
	Emulator::Emulator(const ModelDefinition &definition,
	                   const unsigned char *rom_bytes, std::size_t rom_size)
		: chipset(*new Chipset(*this)), lua_state(nullptr)
	{
		running = true;
		paused = false;
		cycle_remainder = 0;

		model_strings["model_name"] = definition.model_name;
		model_ints["hardware_id"] = definition.hardware_id;
		model_ints["real_hardware"] = definition.real_hardware ? 1 : 0;
		model_ints["csr_mask"] = definition.csr_mask;
		model_ints["pd_value"] = definition.pd_value;

		if (definition.hardware_id != HW_ES_PLUS && definition.hardware_id != HW_CLASSWIZ)
			PANIC("hardware id desconocido: %d\n", (int)definition.hardware_id);
		hardware_id = definition.hardware_id;

		// Mismas frecuencias que el emulador de escritorio: de aqui depende que los
		// temporizadores y el parpadeo del cursor vayan a la velocidad correcta.
		cycles_per_second = hardware_id == HW_ES_PLUS ? 128 * 1024 : 1024 * 1024;

		rom_image.assign(rom_bytes, rom_bytes + rom_size);

		// El orden importa y es el mismo que en el original: Setup construye los
		// perifericos, SetupInternals carga la ROM y los inicializa, y Reset arranca.
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
		return ModelInfo(*this, key);
	}

	std::string Emulator::GetModelFilePath(std::string relative_path)
	{
		// No hay ficheros de modelo: la ROM llega en memoria y el resto son datos
		// nativos. Si algo llama aqui es que quedo una ruta sin migrar.
		PANIC("GetModelFilePath('%s') en una compilacion sin ficheros de modelo\n",
		      relative_path.c_str());
		return relative_path;
	}

	void Emulator::HandleMemoryError()
	{
		// El emulador de escritorio podia pausarse aqui para depurar. En la app no
		// tiene sentido parar la calculadora: el nucleo ya avisa por el log.
	}

	unsigned int Emulator::GetCyclesPerSecond()
	{
		return cycles_per_second;
	}

	bool Emulator::Running()
	{
		return running;
	}

	bool Emulator::GetPaused()
	{
		return paused;
	}

	void Emulator::SetPaused(bool _paused)
	{
		paused = _paused;
	}

	void Emulator::Advance(unsigned int milliseconds)
	{
		if (paused || !running)
			return;

		cycle_remainder += (Uint64)cycles_per_second * milliseconds;
		Uint64 cycles_to_emulate = cycle_remainder / 1000;
		cycle_remainder -= cycles_to_emulate * 1000;

		for (Uint64 ix = 0; ix != cycles_to_emulate; ++ix)
			chipset.Tick();
	}

	void Emulator::Reset()
	{
		chipset.Reset();
	}

	// ------------------------------------------------------------------ instantaneas

	namespace
	{
		const char state_magic[8] = {'C', 'A', 'S', 'I', 'O', 'S', 'N', 'P'};
		// Cabecera: magica(8) + version(4) + hardware(4) + real_hardware(1) +
		// csr_mask(4) + pd_value(4) + tam_rom(8) + huella_rom(8) + tam_payload(8) +
		// huella_payload(8).
		const std::size_t state_header_size = 8 + 4 + 4 + 1 + 4 + 4 + 8 + 8 + 8 + 8;
	}

	void Emulator::SaveState(std::vector<unsigned char> &destination) const
	{
		std::vector<unsigned char> payload;
		StateWriter writer(payload);

		writer.Tag(TAG_EMULATOR);
		writer.Bool(running);
		writer.Bool(paused);
		// Sin esto, restaurar perderia hasta un ciclo de reloj por vez. Parece
		// despreciable y no lo es: el rebobinado repetido lo iria acumulando.
		writer.U64(cycle_remainder);

		chipset.SaveState(writer);

		destination.clear();
		destination.reserve(state_header_size + payload.size());

		StateWriter header(destination);
		header.Raw(state_magic, sizeof(state_magic));
		header.U32(state_format_version);
		header.U32((std::uint32_t)hardware_id);
		header.U8((std::uint8_t)(model_ints.at("real_hardware") ? 1 : 0));
		header.U32((std::uint32_t)model_ints.at("csr_mask"));
		header.U32((std::uint32_t)model_ints.at("pd_value"));
		header.U64((std::uint64_t)rom_image.size());
		header.U64(StateHash(rom_image.data(), rom_image.size()));
		header.U64((std::uint64_t)payload.size());
		header.U64(StateHash(payload.data(), payload.size()));

		destination.insert(destination.end(), payload.begin(), payload.end());
	}

	bool Emulator::LoadState(const unsigned char *data, std::size_t size)
	{
		if (!data || size < state_header_size)
			return false;

		StateReader header(data, size);
		char magic[sizeof(state_magic)];
		header.Raw(magic, sizeof(magic));
		if (header.Failed() || std::memcmp(magic, state_magic, sizeof(magic)) != 0)
			return false;

		if (header.U32() != state_format_version)
			return false;
		if (header.U32() != (std::uint32_t)hardware_id)
			return false;
		if (header.U8() != (std::uint8_t)(model_ints.at("real_hardware") ? 1 : 0))
			return false;
		if (header.U32() != (std::uint32_t)model_ints.at("csr_mask"))
			return false;
		if (header.U32() != (std::uint32_t)model_ints.at("pd_value"))
			return false;
		// La ROM se compara por tamano y huella. Restaurar una VN PLUS dentro de una
		// ClassWiz daria un monton de bytes que el firmware equivocado interpretaria
		// como suyos, y la calculadora resultante calcularia mal en silencio.
		if (header.U64() != (std::uint64_t)rom_image.size())
			return false;
		if (header.U64() != StateHash(rom_image.data(), rom_image.size()))
			return false;

		std::uint64_t payload_size = header.U64();
		std::uint64_t payload_hash = header.U64();
		if (header.Failed())
			return false;
		if (payload_size != (std::uint64_t)(size - state_header_size))
			return false;

		const unsigned char *payload = data + state_header_size;
		if (StateHash(payload, (std::size_t)payload_size) != payload_hash)
			return false;

		// A partir de aqui el volcado esta validado y se aplica.
		StateReader reader(payload, (std::size_t)payload_size);
		reader.Tag(TAG_EMULATOR);
		running = reader.Bool();
		paused = reader.Bool();
		cycle_remainder = reader.U64();

		chipset.LoadState(reader);

		if (reader.Failed() || reader.Position() != (std::size_t)payload_size)
		{
			// No deberia ocurrir nunca: la suma ya cuadraba. Si ocurre, la maquina
			// esta a medio escribir y hay que dejarla en un estado definido.
			Reset();
			return false;
		}
		return true;
	}
}
