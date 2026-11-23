#include "Screen.hpp"

#include "../Chipset/MMU.hpp"
#include "../Data/HardwareId.hpp"
#include "../Emulator.hpp"
#include "../Logger.hpp"

#include <cstring>

namespace casioemu
{
	Screen::Screen(Emulator &_emulator, int _n_row, int _row_size,
	               int _row_offset, int _row_visible)
		: Peripheral(_emulator), n_row(_n_row), row_size(_row_size),
		  row_offset(_row_offset), row_visible(_row_visible), screen_buffer(nullptr),
		  screen_contrast(0), screen_mode(0), screen_range(0)
	{
		emulator.screen = this;
	}

	void Screen::Initialise()
	{
		require_frame = true;

		// El original dejaba este buffer sin inicializar, y las columnas que no se
		// muestran acababan con basura del monton. Se pone a cero: no cambia la
		// emulacion (esos bytes siempre se leen como 0) y evita ruido al depurar.
		screen_buffer = new std::uint8_t[BufferSize()];
		std::memset(screen_buffer, 0, BufferSize());

		region_buffer.Setup(0xF800, BufferSize(), "Screen/Buffer", this,
			[](MMURegion *region, std::size_t offset) -> std::uint8_t {
				auto self = (Screen *)region->userdata;
				offset -= region->base;
				// Las columnas fuera del cristal existen en el mapa de memoria pero
				// siempre leen cero, igual que en el hardware.
				if ((int)(offset % self->row_size) >= self->row_visible)
					return 0;
				return self->screen_buffer[offset];
			},
			[](MMURegion *region, std::size_t offset, std::uint8_t data) {
				auto self = (Screen *)region->userdata;
				offset -= region->base;
				if ((int)(offset % self->row_size) >= self->row_visible)
					return;
				self->require_frame |= self->screen_buffer[offset] != data;
				self->screen_buffer[offset] = data;
			}, emulator);

		region_range.Setup(0xF030, 1, "Screen/Range", this,
			[](MMURegion *region, std::size_t) -> std::uint8_t {
				return ((Screen *)region->userdata)->screen_range & 0x07;
			},
			[](MMURegion *region, std::size_t, std::uint8_t data) {
				auto self = (Screen *)region->userdata;
				std::uint8_t value = data & 0x07;
				self->require_frame |= self->screen_range != value;
				self->screen_range = value;
			}, emulator);

		region_mode.Setup(0xF031, 1, "Screen/Mode", this,
			[](MMURegion *region, std::size_t) -> std::uint8_t {
				return ((Screen *)region->userdata)->screen_mode & 0x07;
			},
			[](MMURegion *region, std::size_t, std::uint8_t data) {
				auto self = (Screen *)region->userdata;
				std::uint8_t value = data & 0x07;
				self->require_frame |= self->screen_mode != value;
				self->screen_mode = value;
			}, emulator);

		region_contrast.Setup(0xF032, 1, "Screen/Contrast", this,
			[](MMURegion *region, std::size_t) -> std::uint8_t {
				return ((Screen *)region->userdata)->screen_contrast & 0x1F;
			},
			[](MMURegion *region, std::size_t, std::uint8_t data) {
				auto self = (Screen *)region->userdata;
				std::uint8_t value = data & 0x1F;
				self->require_frame |= self->screen_contrast != value;
				self->screen_contrast = value;
			}, emulator);
	}

	void Screen::Uninitialise()
	{
		delete[] screen_buffer;
		screen_buffer = nullptr;
	}

	void Screen::Frame()
	{
		// Sin dibujado propio: solo se acusa recibo de que el anfitrion ya puede
		// leer un buffer nuevo.
		require_frame = false;
	}

	void Screen::SaveState(StateWriter &writer) const
	{
		Peripheral::SaveState(writer);
		writer.Tag(TAG_SCREEN);
		writer.U32((std::uint32_t)BufferSize());
		writer.Raw(screen_buffer, BufferSize());
		writer.U8(screen_contrast);
		writer.U8(screen_mode);
		writer.U8(screen_range);
	}

	void Screen::LoadState(StateReader &reader)
	{
		Peripheral::LoadState(reader);
		reader.Tag(TAG_SCREEN);
		if (reader.U32() != (std::uint32_t)BufferSize())
		{
			reader.Fail();
			return;
		}
		reader.Raw(screen_buffer, BufferSize());
		screen_contrast = reader.U8();
		screen_mode = reader.U8();
		screen_range = reader.U8();
		// El anfitrion tiene que repintar: la pantalla que acaba de aparecer no es la
		// que estaba mostrando.
		require_frame = true;
	}

	Peripheral *CreateScreen(Emulator &emulator)
	{
		// Geometria tomada del original: ES PLUS 96x31 puntos, ClassWiz 192x63.
		if (emulator.hardware_id == HW_ES_PLUS)
			return new Screen(emulator, 31, 16, 16, 12);
		return new Screen(emulator, 63, 32, 32, 24);
	}
}
