#pragma once
#include "../Config.hpp"

#include "../Chipset/InterruptSource.hpp"
#include "../Chipset/MMURegion.hpp"
#include "Peripheral.hpp"

#include <cstddef>
#include <cstdint>

namespace casioemu
{
	/**
	 * Matriz de teclado.
	 *
	 * Toda la logica de hardware se conserva del original: registros KI/KO, filtro de
	 * entrada, interrupcion 5, calculo de teclas fantasma y la interfaz alternativa que
	 * usan las ROM sacadas del emulador de Casio (0x8E00-0x8E02 y 0xF050).
	 *
	 * Lo unico que cambia es de donde viene la pulsacion: antes de un evento de SDL
	 * contra rectangulos definidos en Lua, y ahora directamente del codigo de matriz,
	 * que es lo que la app de iOS puede dar sin arrastrar la caratula hasta aqui.
	 */
	class Keyboard : public Peripheral
	{
	public:
		Keyboard(Emulator &emulator);

		void Initialise() override;
		void Reset() override;
		void Tick() override;
		void Frame() override;

		/**
		 * Pulsa la tecla del codigo dado: nibble alto KO, nibble bajo KI.
		 * 0xFF es el boton ON, que reinicia el chipset.
		 */
		void PressCode(std::uint8_t code);
		void ReleaseAll();

		/** Posicion en la tabla de 64 botones que corresponde a un codigo de matriz. */
		static std::size_t IndexForCode(std::uint8_t code);

		void SaveState(StateWriter &writer) const override;
		void LoadState(StateReader &reader) override;

	private:
		void RecalculateKI();
		void RecalculateGhost();

		MMURegion region_ko_mask, region_ko, region_ki, region_input_filter;
		std::uint16_t keyboard_out, keyboard_out_mask;
		std::uint8_t keyboard_in, input_filter, keyboard_ghost[8];

		bool real_hardware;
		MMURegion region_ready_emu, region_ko_emu, region_ki_emu, region_pd_emu;
		std::uint8_t keyboard_ready_emu, keyboard_out_emu, keyboard_in_emu, keyboard_pd_emu;

		std::uint8_t has_input;
		InterruptSource interrupt_source;

		struct Button
		{
			enum ButtonType
			{
				BT_NONE,
				BT_BUTTON,
				BT_POWER
			} type;
			std::uint8_t ko_bit, ki_bit;
			bool pressed, stuck;
		} buttons[64];

		bool p0, p1, p146;
	};
}
