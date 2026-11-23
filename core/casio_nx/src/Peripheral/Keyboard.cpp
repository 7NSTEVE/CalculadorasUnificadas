#include "Keyboard.hpp"

#include "../Chipset/Chipset.hpp"
#include "../Chipset/MMU.hpp"
#include "../Data/HardwareId.hpp"
#include "../Emulator.hpp"
#include "../Logger.hpp"

namespace casioemu
{
	/** Codigo 0xFF (el boton ON) vive en la ultima posicion de la tabla. */
	static const std::size_t POWER_INDEX = 63;

	std::size_t Keyboard::IndexForCode(std::uint8_t code)
	{
		if (code == 0xFF)
			return POWER_INDEX;
		return ((code >> 1) & 0x38) | (code & 0x07);
	}

	Keyboard::Keyboard(Emulator &_emulator) : Peripheral(_emulator)
	{
		emulator.keyboard = this;

		// El original solo marcaba como pulsables los botones que el modelo listaba en
		// Lua. Aqui se habilitan los 64: los que el hardware no tiene cableados no los
		// pulsa nadie, porque la app solo manda codigos reales.
		for (std::size_t ix = 0; ix != 64; ++ix)
		{
			buttons[ix].type = ix == POWER_INDEX ? Button::BT_POWER : Button::BT_BUTTON;
			buttons[ix].pressed = false;
			buttons[ix].stuck = false;
			buttons[ix].ko_bit = 0;
			buttons[ix].ki_bit = 0;
		}
		for (unsigned int code = 0; code != 0x100; ++code)
		{
			if (code == 0xFF)
				continue;
			std::size_t index = IndexForCode((std::uint8_t)code);
			if (index == POWER_INDEX)
				continue;
			buttons[index].ko_bit = (std::uint8_t)(1 << ((code >> 4) & 0x07));
			buttons[index].ki_bit = (std::uint8_t)(1 << (code & 0x07));
		}
	}

	void Keyboard::Initialise()
	{
		require_frame = true;

		real_hardware = emulator.GetModelInfo("real_hardware");

		interrupt_source.Setup(5, emulator);

		region_ki.Setup(0xF040, 1, "Keyboard/KI", &keyboard_in,
			MMURegion::DefaultRead<std::uint8_t>, MMURegion::IgnoreWrite, emulator);

		region_input_filter.Setup(0xF042, 1, "Keyboard/InputFilter", &input_filter,
			MMURegion::DefaultRead<std::uint8_t>, MMURegion::DefaultWrite<std::uint8_t>, emulator);

		region_ko_mask.Setup(0xF044, 2, "Keyboard/KOMask", this,
			[](MMURegion *region, std::size_t offset) -> std::uint8_t {
				offset -= region->base;
				Keyboard *keyboard = ((Keyboard *)region->userdata);
				return (std::uint8_t)((keyboard->keyboard_out_mask & 0x03FF) >> (offset * 8));
			},
			[](MMURegion *region, std::size_t offset, std::uint8_t data) {
				offset -= region->base;
				Keyboard *keyboard = ((Keyboard *)region->userdata);
				keyboard->keyboard_out_mask &= ~(((std::uint16_t)0xFF) << (offset * 8));
				keyboard->keyboard_out_mask |= ((std::uint16_t)data) << (offset * 8);
				keyboard->keyboard_out_mask &= 0x03FF;
				if (!offset)
					keyboard->RecalculateKI();
			}, emulator);

		region_ko.Setup(0xF046, 2, "Keyboard/KO", this,
			[](MMURegion *region, std::size_t offset) -> std::uint8_t {
				offset -= region->base;
				Keyboard *keyboard = ((Keyboard *)region->userdata);
				return (std::uint8_t)((keyboard->keyboard_out & 0x03FF) >> (offset * 8));
			},
			[](MMURegion *region, std::size_t offset, std::uint8_t data) {
				offset -= region->base;
				Keyboard *keyboard = ((Keyboard *)region->userdata);
				keyboard->keyboard_out &= ~(((std::uint16_t)0xFF) << (offset * 8));
				keyboard->keyboard_out |= ((std::uint16_t)data) << (offset * 8);
				keyboard->keyboard_out &= 0x03FF;
				if (!offset)
					keyboard->RecalculateKI();
			}, emulator);

		if (!real_hardware)
		{
			keyboard_pd_emu = emulator.GetModelInfo("pd_value");
			int offset = emulator.hardware_id == HW_ES_PLUS ? 0 : 0x40000;
			region_ready_emu.Setup(offset + 0x8E00, 1, "Keyboard/ReadyStatusEmulator",
				&keyboard_ready_emu, MMURegion::DefaultRead<std::uint8_t>,
				MMURegion::DefaultWrite<std::uint8_t>, emulator);
			region_ki_emu.Setup(offset + 0x8E01, 1, "Keyboard/KIEmulator",
				&keyboard_in_emu, MMURegion::DefaultRead<std::uint8_t>,
				MMURegion::IgnoreWrite, emulator);
			region_ko_emu.Setup(offset + 0x8E02, 1, "Keyboard/KOEmulator",
				&keyboard_out_emu, MMURegion::DefaultRead<std::uint8_t>,
				MMURegion::IgnoreWrite, emulator);
			region_pd_emu.Setup(0xF050, 1, "Keyboard/PdValue",
				&keyboard_pd_emu, MMURegion::DefaultRead<std::uint8_t>,
				MMURegion::IgnoreWrite, emulator);
		}
	}

	void Keyboard::Reset()
	{
		p0 = false;
		p1 = false;
		p146 = false;
		keyboard_out = 0;
		keyboard_out_mask = 0;

		if (!real_hardware)
		{
			keyboard_in_emu = 0;
			keyboard_out_emu = 0;
		}

		RecalculateGhost();
	}

	void Keyboard::Tick()
	{
		if (has_input && interrupt_source.Enabled())
			interrupt_source.TryRaise();
	}

	void Keyboard::Frame()
	{
		require_frame = false;
	}

	void Keyboard::PressCode(std::uint8_t code)
	{
		Button &button = buttons[IndexForCode(code)];
		bool old_pressed = button.pressed;
		button.pressed = true;

		require_frame = true;

		if (button.type == Button::BT_POWER && !old_pressed)
		{
			emulator.chipset.Reset();
			return;
		}

		if (button.type == Button::BT_BUTTON && button.pressed != old_pressed)
		{
			if (real_hardware)
			{
				RecalculateGhost();
			}
			else
			{
				// El emulador de Casio no admite varias teclas a la vez: se informa
				// de una sola.
				has_input = keyboard_in_emu = button.ki_bit;
				keyboard_out_emu = button.ko_bit;
			}
		}
	}

	void Keyboard::ReleaseAll()
	{
		bool had_effect = false;
		for (auto &button : buttons)
		{
			if (!button.stuck && button.pressed)
			{
				button.pressed = false;
				if (button.type == Button::BT_BUTTON)
					had_effect = true;
			}
		}
		if (had_effect)
		{
			require_frame = true;
			if (real_hardware)
				RecalculateGhost();
			else
				has_input = keyboard_in_emu = keyboard_out_emu = 0;
		}
	}

	void Keyboard::RecalculateGhost()
	{
		struct KOColumn
		{
			std::uint8_t connections;
			bool seen;
		} columns[8];

		has_input = 0;
		for (auto &button : buttons)
			if (button.type == Button::BT_BUTTON && button.pressed && button.ki_bit & input_filter)
				has_input |= button.ki_bit;

		for (std::size_t cx = 0; cx != 8; ++cx)
		{
			columns[cx].seen = false;
			columns[cx].connections = 0;
			for (std::size_t rx = 0; rx != 8; ++rx)
			{
				Button &button = buttons[cx * 8 + rx];
				if (button.type == Button::BT_BUTTON && button.pressed)
				{
					for (std::size_t ax = 0; ax != 8; ++ax)
					{
						Button &sibling = buttons[ax * 8 + rx];
						if (sibling.type == Button::BT_BUTTON && sibling.pressed)
							columns[cx].connections |= 1 << ax;
					}
				}
			}
		}

		for (std::size_t cx = 0; cx != 8; ++cx)
		{
			if (!columns[cx].seen)
			{
				std::uint8_t to_visit = 1 << cx;
				std::uint8_t ghost_mask = 1 << cx;
				columns[cx].seen = true;

				while (to_visit)
				{
					std::uint8_t new_to_visit = 0;
					for (std::size_t vx = 0; vx != 8; ++vx)
					{
						if (to_visit & (1 << vx))
						{
							for (std::size_t sx = 0; sx != 8; ++sx)
							{
								if (columns[vx].connections & (1 << sx) && !columns[sx].seen)
								{
									new_to_visit |= 1 << sx;
									ghost_mask |= 1 << sx;
									columns[sx].seen = true;
								}
							}
						}
					}
					to_visit = new_to_visit;
				}

				for (std::size_t gx = 0; gx != 8; ++gx)
					if (ghost_mask & (1 << gx))
						keyboard_ghost[gx] = ghost_mask;
			}
		}

		RecalculateKI();
	}

	void Keyboard::RecalculateKI()
	{
		std::uint8_t keyboard_out_ghosted = 0;
		for (std::size_t ix = 0; ix != 7; ++ix)
			if (keyboard_out & ~keyboard_out_mask & (1 << ix))
				keyboard_out_ghosted |= keyboard_ghost[ix];

		keyboard_in = 0xFF;
		for (auto &button : buttons)
			if (button.type == Button::BT_BUTTON && button.pressed && button.ko_bit & keyboard_out_ghosted)
				keyboard_in &= ~button.ki_bit;

		if (keyboard_out & ~keyboard_out_mask & (1 << 7) && p0)
			keyboard_in &= 0x7F;
		if (keyboard_out & ~keyboard_out_mask & (1 << 8) && p1)
			keyboard_in &= 0x7F;
		if (keyboard_out & ~keyboard_out_mask & (1 << 9) && p146)
			keyboard_in &= 0x7F;
	}

	void Keyboard::SaveState(StateWriter &writer) const
	{
		Peripheral::SaveState(writer);
		writer.Tag(TAG_KEYBOARD);
		writer.U16(keyboard_out);
		writer.U16(keyboard_out_mask);
		writer.U8(keyboard_in);
		writer.U8(input_filter);
		for (int ix = 0; ix != 8; ++ix)
			writer.U8(keyboard_ghost[ix]);

		// Interfaz alternativa de las ROM sacadas del emulador de Casio. Se guarda
		// siempre, tenga o no `real_hardware`: cuesta cuatro bytes y evita que el
		// formato dependa del modelo.
		writer.U8(keyboard_ready_emu);
		writer.U8(keyboard_out_emu);
		writer.U8(keyboard_in_emu);
		writer.U8(keyboard_pd_emu);

		writer.U8(has_input);
		// De cada boton solo es estado si esta pulsado o encallado. El tipo y los bits
		// KO/KI los fija Initialise a partir de la tabla de teclas del modelo.
		for (int ix = 0; ix != 64; ++ix)
		{
			writer.Bool(buttons[ix].pressed);
			writer.Bool(buttons[ix].stuck);
		}
		writer.Bool(p0);
		writer.Bool(p1);
		writer.Bool(p146);
		interrupt_source.SaveState(writer);
	}

	void Keyboard::LoadState(StateReader &reader)
	{
		Peripheral::LoadState(reader);
		reader.Tag(TAG_KEYBOARD);
		keyboard_out = reader.U16();
		keyboard_out_mask = reader.U16();
		keyboard_in = reader.U8();
		input_filter = reader.U8();
		for (int ix = 0; ix != 8; ++ix)
			keyboard_ghost[ix] = reader.U8();

		keyboard_ready_emu = reader.U8();
		keyboard_out_emu = reader.U8();
		keyboard_in_emu = reader.U8();
		keyboard_pd_emu = reader.U8();

		has_input = reader.U8();
		for (int ix = 0; ix != 64; ++ix)
		{
			buttons[ix].pressed = reader.Bool();
			buttons[ix].stuck = reader.Bool();
		}
		p0 = reader.Bool();
		p1 = reader.Bool();
		p146 = reader.Bool();
		interrupt_source.LoadState(reader);
	}
}
