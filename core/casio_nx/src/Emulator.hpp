#pragma once
#include "Config.hpp"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>
#include <lua.hpp>

#include "Chipset/Snapshot.hpp"
#include "Data/ColourInfo.hpp"
#include "Data/HardwareId.hpp"
#include "Data/ModelInfo.hpp"
#include "Data/SpriteInfo.hpp"

namespace casioemu
{
	class Chipset;
	class CPU;
	class MMU;
	class Screen;
	class Keyboard;
	class BatteryBackedRAM;

	/**
	 * Definicion de un modelo de calculadora.
	 *
	 * En el emulador de escritorio esto salia de un script Lua. Aqui son datos nativos:
	 * la app de iOS no lleva interprete y ademas asi el modelo se puede validar en
	 * tiempo de compilacion.
	 */
	struct ModelDefinition
	{
		std::string model_name;
		HardwareId hardware_id = HW_ES_PLUS;
		/**
		 * false = la ROM salio del emulador oficial de Casio y usa su interfaz de
		 * teclado (registros 0x8E00-0x8E02 y 0xF050) en vez de la matriz fisica.
		 */
		bool real_hardware = false;
		/** Segmentos de memoria de codigo menos uno: 0x0001 = 128 KiB, 0x0003 = 256 KiB. */
		int csr_mask = 0x0001;
		int pd_value = 0;
	};

	/**
	 * Emulador sin interfaz: ni ventana, ni hilos, ni interprete.
	 *
	 * Quien manda es el anfitrion (la app de iOS o el banco de pruebas): llama a
	 * Advance() para dejar correr el tiempo emulado y luego lee el buffer del LCD.
	 * Es un cambio importante respecto al original, que corria en su propio hilo
	 * con temporizador; aqui no hay concurrencia que sincronizar.
	 */
	class Emulator
	{
	public:
		Emulator(const ModelDefinition &definition,
		         const unsigned char *rom_bytes, std::size_t rom_size);
		~Emulator();

		Chipset &chipset;
		HardwareId hardware_id;

		/**
		 * Atajos a los dos perifericos con los que habla el anfitrion. Se registran
		 * ellos mismos al construirse, porque la lista de perifericos vive dentro del
		 * Chipset y es privada.
		 */
		Screen *screen = nullptr;
		Keyboard *keyboard = nullptr;
		/// Se registra solo al inicializarse, como los otros dos.
		BatteryBackedRAM *ram = nullptr;

		/** Siempre nulo. Existe porque el nucleo lo nombra; no hay interprete detras. */
		lua_State *lua_state;
		/** Opciones sueltas del emulador de escritorio. Aqui siempre vacio. */
		std::map<std::string, std::string> argv_map;

		ModelInfo GetModelInfo(std::string key);
		std::string GetModelFilePath(std::string relative_path);
		void HandleMemoryError();
		unsigned int GetCyclesPerSecond();
		bool Running();
		bool GetPaused();
		void SetPaused(bool paused);

		/**
		 * Deja correr el equivalente a `milliseconds` de tiempo real.
		 *
		 * El resto de ciclos se guarda entre llamadas, para que llamar muchas veces
		 * con valores pequenos no vaya perdiendo tiempo por redondeo.
		 */
		void Advance(unsigned int milliseconds);

		/** Equivale a pulsar ON: reinicia el chipset. */
		void Reset();

		/**
		 * Vuelca el estado completo de la maquina en `destination`.
		 *
		 * Lo que sale de aqui restaurado en otra instancia del MISMO modelo y con la
		 * MISMA ROM da una calculadora indistinguible: mismos registros, misma RAM,
		 * misma pantalla y el mismo futuro ante las mismas teclas.
		 */
		void SaveState(std::vector<unsigned char> &destination) const;

		/**
		 * Restaura un volcado. Devuelve false y NO toca la maquina si la cabecera no
		 * cuadra (otro modelo, otra ROM, otra version del formato) o si la suma de
		 * comprobacion falla.
		 *
		 * Si falla DESPUES de haber empezado a aplicar —solo posible con un volcado
		 * cuya suma cuadre pero cuyo contenido no, es decir, nunca en la practica— la
		 * maquina queda reiniciada, no a medias. Un estado a medias es peor que
		 * ninguno: seria una calculadora que parece funcionar y calcula mal.
		 */
		bool LoadState(const unsigned char *data, std::size_t size);

		/** Version del formato de volcado. Sube cuando cambia el contenido. */
		static const std::uint32_t state_format_version = 1;

		const std::vector<unsigned char> &GetRomImage() const { return rom_image; }

		/** Valores del modelo, que es lo que consulta GetModelInfo. */
		std::map<std::string, int> model_ints;
		std::map<std::string, std::string> model_strings;
		std::map<std::string, ColourInfo> model_colours;
		std::map<std::string, SpriteInfo> model_sprites;

		friend class ModelInfo;
		friend class CPU;
		friend class MMU;

	private:
		std::vector<unsigned char> rom_image;
		unsigned int cycles_per_second;
		bool running;
		bool paused;
		/** Milicilcos pendientes, para no acumular error de redondeo en Advance(). */
		Uint64 cycle_remainder;
	};
}
