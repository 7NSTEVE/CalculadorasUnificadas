#pragma once
#include "../Config.hpp"

#include "../Chipset/MMURegion.hpp"
#include "Peripheral.hpp"

#include <cstddef>
#include <cstdint>

namespace casioemu
{
	/**
	 * Controlador del LCD.
	 *
	 * Conserva intactos los registros que el firmware toca (buffer en 0xF800, y
	 * rango/modo/contraste en 0xF030-0xF032), porque eso si es emulacion. Lo que
	 * desaparece respecto al original es el dibujado con SDL: aqui simplemente se
	 * expone el buffer crudo y que lo pinte quien quiera.
	 *
	 * Formato del buffer: (filas+1) x bytes_por_fila. La fila 0 es la linea de
	 * indicadores (S, A, M, STO, DEG...), y a partir de RowOffset() vienen las filas
	 * de la matriz de puntos, de las que solo las primeras VisibleBytesPerRow()
	 * llegan al cristal. Cada byte son 8 pixeles, el bit mas alto a la izquierda.
	 */
	class Screen : public Peripheral
	{
	public:
		Screen(Emulator &emulator, int n_row, int row_size, int row_offset, int row_visible);

		void Initialise() override;
		void Uninitialise() override;
		void Frame() override;

		void SaveState(StateWriter &writer) const override;
		void LoadState(StateReader &reader) override;

		int RowCount() const { return n_row; }
		int RowSize() const { return row_size; }
		int RowOffset() const { return row_offset; }
		int VisibleBytesPerRow() const { return row_visible; }
		int PixelWidth() const { return row_visible * 8; }
		std::size_t BufferSize() const { return (std::size_t)(n_row + 1) * row_size; }
		const std::uint8_t *Buffer() const { return screen_buffer; }

		std::uint8_t Contrast() const { return screen_contrast; }
		std::uint8_t Mode() const { return screen_mode; }
		std::uint8_t Range() const { return screen_range; }

	private:
		const int n_row, row_size, row_offset, row_visible;

		MMURegion region_buffer, region_contrast, region_mode, region_range;
		std::uint8_t *screen_buffer;
		std::uint8_t screen_contrast, screen_mode, screen_range;
	};

	Peripheral *CreateScreen(Emulator &emulator);
}
