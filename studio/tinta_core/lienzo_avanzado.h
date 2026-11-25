// Operaciones de página híbrida, impresión y reacomodo.
#pragma once

#include <vector>

#include "documento.h"

namespace tinta {

enum class UnidadLienzo { Pixeles, Milimetros, Centimetros, Pulgadas };
enum class ModoPagina {
  Libro,
  OneNote,
  Goodnotes,
  Hibrido,
  Presentacion,
  Pizarra,
  HorizontalInfinito,
};
enum class ReacomodoPagina { Conservar, EscalarProporcional };
enum class EjeEspacio { Horizontal, Vertical };
enum class BordePagina { Izquierdo, Derecho, Superior, Inferior };

float ALongitudLienzo(float valor, UnidadLienzo unidad, float ppp = 150.0f);
float DesdeLongitudLienzo(float pixeles, UnidadLienzo unidad,
                          float ppp = 150.0f);

void AplicarModoPagina(Pagina* pagina, ModoPagina modo);

// Cambia el tamaño y opcionalmente escala el contenido desde el origen. El
// modo Conservar nunca mueve ni deforma lo escrito.
bool CambiarTamanoAvanzado(Pagina* pagina, float ancho, float alto,
                           ReacomodoPagina reacomodo);

// Inserta espacio desplazando solamente lo que está después de la línea. Sirve
// tanto para «insertar 3 cm» como para el reflow manual por región.
bool InsertarEspacio(Pagina* pagina, EjeEspacio eje, float desde,
                     float cantidad);

// Desplaza físicamente un borde sin destruir contenido. En los bordes
// izquierdo/superior, `desplazar_contenido` conserva la posición visual de lo
// escrito trasladando sus coordenadas al nuevo origen.
bool MoverBorde(Pagina* pagina, BordePagina borde, float delta,
                bool desplazar_contenido);

// Posiciones Y donde terminarían las páginas impresas. Si un corte atraviesa
// tinta, texto o un medio, se mueve antes del objeto cuando cabe; si no, después.
std::vector<float> CortesDeImpresion(const Pagina& pagina, float alto_hoja,
                                     float margen_seguro);

}  // namespace tinta
