#pragma once
#include "../Config.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace casioemu
{
	/**
	 * Serializacion del estado completo de la maquina emulada.
	 *
	 * Existe para que "volver atras" sea instantaneo. Sin esto, el unico modo de
	 * recuperar un estado anterior es reproducir la secuencia de teclas desde el
	 * encendido, y eso cuesta ~20 ms por tecla: 300 teclas son 6 segundos.
	 *
	 * REGLA DE ESTE FICHERO: un volcado casi correcto produce una calculadora casi
	 * correcta, que es el peor resultado posible del proyecto entero. Por eso:
	 *
	 *   - cada bloque va precedido de una MARCA; si al leer no cuadra, se aborta.
	 *     Asi, si alguien anade un campo al guardar y olvida leerlo, el desajuste
	 *     revienta en la marca siguiente en vez de colarse como corrupcion silenciosa.
	 *   - la cabecera lleva una huella del modelo y de la ROM, para que no se pueda
	 *     restaurar una VN PLUS dentro de una ClassWiz.
	 *   - el payload lleva su propia suma de comprobacion, verificada ANTES de tocar
	 *     nada de la maquina.
	 *
	 * El formato es little-endian explicito, no un volcado de structs, para que un
	 * volcado hecho en el iPad se pueda leer en el banco de pruebas de Windows.
	 */

	/** Marcas de bloque. Son ASCII para que se reconozcan en un visor hexadecimal. */
	enum StateTag : std::uint32_t
	{
		TAG_EMULATOR = 0x314C4D45, // "EML1"
		TAG_CHIPSET  = 0x31504843, // "CHP1"
		TAG_CPU      = 0x31555043, // "CPU1"
		TAG_STACK    = 0x314B5453, // "STK1"
		TAG_PERIPH   = 0x31524550, // "PER1"
		TAG_RAM      = 0x314D4152, // "RAM1"
		TAG_SCREEN   = 0x314E4353, // "SCN1"
		TAG_TIMER    = 0x314D4954, // "TIM1"
		TAG_STANDBY  = 0x31594253, // "SBY1"
		TAG_KEYBOARD = 0x3159454B, // "KEY1"
		TAG_MISC     = 0x4331534D, // "MSC1"
		TAG_END      = 0x21444E45  // "END!"
	};

	class StateWriter
	{
		std::vector<unsigned char> &out;

	public:
		explicit StateWriter(std::vector<unsigned char> &destination) : out(destination)
		{
		}

		void Raw(const void *data, std::size_t size)
		{
			const unsigned char *bytes = static_cast<const unsigned char *>(data);
			out.insert(out.end(), bytes, bytes + size);
		}

		void U8(std::uint8_t value)
		{
			out.push_back(value);
		}

		void U16(std::uint16_t value)
		{
			U8((std::uint8_t)(value & 0xFF));
			U8((std::uint8_t)((value >> 8) & 0xFF));
		}

		void U32(std::uint32_t value)
		{
			U16((std::uint16_t)(value & 0xFFFF));
			U16((std::uint16_t)((value >> 16) & 0xFFFF));
		}

		void U64(std::uint64_t value)
		{
			U32((std::uint32_t)(value & 0xFFFFFFFFu));
			U32((std::uint32_t)(value >> 32));
		}

		void Bool(bool value)
		{
			U8(value ? 1 : 0);
		}

		void Tag(std::uint32_t tag)
		{
			U32(tag);
		}
	};

	class StateReader
	{
		const unsigned char *data;
		std::size_t size, position;
		bool failed;

	public:
		StateReader(const unsigned char *bytes, std::size_t length)
			: data(bytes), size(length), position(0), failed(false)
		{
		}

		/** Cierto si en algun momento se leyo de mas o una marca no cuadro. */
		bool Failed() const
		{
			return failed;
		}

		std::size_t Position() const
		{
			return position;
		}

		void Fail()
		{
			failed = true;
		}

		void Raw(void *destination, std::size_t length)
		{
			// Al fallar se rellena de ceros en vez de dejar basura: si el llamante
			// ignora Failed(), al menos el estado es determinista y no memoria vieja.
			if (failed || position + length > size)
			{
				failed = true;
				std::memset(destination, 0, length);
				return;
			}
			std::memcpy(destination, data + position, length);
			position += length;
		}

		std::uint8_t U8()
		{
			std::uint8_t value = 0;
			Raw(&value, 1);
			return value;
		}

		// El orden de evaluacion de los operandos de | no esta especificado en C++,
		// asi que cada byte se toma en su propia sentencia. Con `U8() | (U8() << 8)`
		// el compilador podria leerlos al reves.
		std::uint16_t U16()
		{
			std::uint16_t low = U8();
			std::uint16_t high = U8();
			return (std::uint16_t)(low | (high << 8));
		}

		std::uint32_t U32()
		{
			std::uint32_t low = U16();
			std::uint32_t high = U16();
			return low | (high << 16);
		}

		std::uint64_t U64()
		{
			std::uint64_t low = U32();
			std::uint64_t high = U32();
			return low | (high << 32);
		}

		bool Bool()
		{
			return U8() != 0;
		}

		/** Comprueba una marca de bloque. Si no cuadra, todo lo demas fallara ya. */
		void Tag(std::uint32_t expected)
		{
			if (U32() != expected)
				failed = true;
		}
	};

	/** FNV-1a de 64 bits. Sirve de huella de la ROM y de suma del payload. */
	std::uint64_t StateHash(const void *data, std::size_t size);
}
