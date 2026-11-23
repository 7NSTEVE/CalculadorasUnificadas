#pragma once
#include "../Config.hpp"

#include "../Chipset/Snapshot.hpp"

#include <SDL.h>

namespace casioemu
{
	class Emulator;

	class Peripheral
	{
	protected:
		Emulator &emulator;

		/**
		 * This should be true if the state of this peripheral changed
		 * so that it requires a call to Frame().
		 * It should not directly call Frame() because otherwise it may
		 * call it more than required (once per timer_interval)
		 */
		bool require_frame;

	public:
		Peripheral(Emulator &emulator);
		virtual void Initialise();
		virtual void Uninitialise();
		virtual void Tick();
		virtual void TickAfterInterrupts();
		virtual void Frame();
		virtual void UIEvent(SDL_Event &event);
		virtual void Reset();
		virtual bool GetRequireFrame();

		/**
		 * Vuelca el estado mutable del periferico, y lo restaura.
		 *
		 * La base solo guarda `require_frame`; cada periferico anade lo suyo DESPUES
		 * de llamar a la base. Lo que NO se guarda es la estructura: regiones de MMU,
		 * tamanos y punteros se reconstruyen igual al crear la maquina, y meterlos en
		 * el volcado solo abriria la puerta a restaurar punteros de otro proceso.
		 */
		virtual void SaveState(StateWriter &writer) const;
		virtual void LoadState(StateReader &reader);

		virtual ~Peripheral();
	};
}

