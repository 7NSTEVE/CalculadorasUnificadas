// Reconstrucción temporal de una página de tinta.
//
// Las muestras ya guardan el timestamp monotónico del hardware. Este módulo no
// captura ni rasteriza: calcula qué parte de cada trazo existía en un instante
// para que Metal pueda reproducir la nota paso a paso.
#pragma once

#include <cstdint>

#include "documento.h"
#include "tinta.h"

namespace tinta {

struct RangoTemporalTinta {
  int64_t inicio = 0;
  int64_t fin = 0;

  bool valido() const { return inicio > 0 && fin >= inicio; }
};

// Ignora timestamps cero, reservados para contenido sin cronología (por
// ejemplo, una forma importada). Ese contenido se considera visible desde el
// principio y no debe deformar el rango de la grabación.
RangoTemporalTinta RangoTemporal(const Pagina& pagina);

// Devuelve la parte del trazo que ya existía en `limite_qpc`. Interpola una
// muestra justo en el corte para que el extremo avance de forma continua en vez
// de saltar entre muestras del Pencil.
Trazo TrazoHasta(const Trazo& trazo, int64_t limite_qpc);

}  // namespace tinta
