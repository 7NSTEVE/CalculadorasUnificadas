// Modelo del documento: cuadernos, secciones, páginas, lienzo y papel.
//
// Vive en el núcleo y no en Swift a propósito. Cada pieza que está aquí se
// compila y se prueba en Windows antes de gastar un minuto de runner macOS, que
// es un recurso limitado. La capa nativa debe quedarse en entrada y pintado.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "elementos.h"
#include "tinta.h"

namespace tinta {

// ---------------------------------------------------------------------------
// Lienzo
// ---------------------------------------------------------------------------

// Límites del lienzo, configurables por eje de forma independiente.
//
// Cubre los cuatro casos que se pidieron: infinito en los dos ejes, limitado en
// los dos, e infinito en uno con límite en el otro (una hoja que crece hacia
// abajo pero no se ensancha, que es como funciona un cuaderno de verdad).
struct LimitesLienzo {
  bool ancho_infinito = false;
  bool alto_infinito = true;

  float ancho_minimo = 320.0f;
  float ancho_maximo = 1240.0f;   // ~A4 a 150 ppp
  float alto_minimo = 480.0f;
  float alto_maximo = 1e38f;      // sin techo práctico cuando alto_infinito

  bool valido() const;
};

// Ajusta un tamaño de lienzo a los límites. En un eje infinito solo se aplica el
// mínimo: el máximo se ignora, no se recorta.
void AplicarLimites(const LimitesLienzo& limites, float* ancho, float* alto);

// Encaja un punto dentro del lienzo. Devuelve true si hubo que moverlo, que es
// la señal de que el trazo se está saliendo del papel.
bool ConfinarPunto(const LimitesLienzo& limites, float ancho, float alto,
                   Punto* punto);

// ---------------------------------------------------------------------------
// Papel
// ---------------------------------------------------------------------------

enum class TipoPapel {
  Blanco,
  BlancoSol,  // blanco cálido, menos fatiga con luz artificial
  Crema,
  Gris,
  Oscuro,
  Negro,
};

enum class Pauta {
  Ninguna,
  Rayado,
  Cuadricula,
  Puntos,
  Pentagrama,
  Milimetrico,    // millimeter paper: grid with 1mm base, every 5th line highlighted
  Isometrico,     // isometric grid: 60-degree lines for 3D drawing
  Cornell,        // Cornell note-taking: left margin + bottom summary area
  Logaritmico,    // log paper: vertical lines at log spacing
  SemiLogaritmico, // semi-log: regular horizontal, log vertical
  Polar,          // polar coordinate grid: circles + radial lines
  Ingenieria,     // engineering paper: fine grid (1mm) with bold every 5mm
  Circuitos,      // circuit paper: larger grid (5mm) optimized for drawing circuits
  Cartesiano,     // cuadrícula con ejes X/Y destacados
  Smith,          // carta de Smith aproximada como guía vectorial
  Hexagonal,      // retícula hexagonal
  Tabla,          // celdas configurables
  Personalizada,  // retícula base para una plantilla definida por la persona
};

struct Papel {
  TipoPapel tipo = TipoPapel::Blanco;
  Pauta pauta = Pauta::Ninguna;
  float separacion = 32.0f;  // separacion de la pauta en pixeles logicos
  float margen_izquierdo = 0.0f;
  float opacidad_pauta = 1.0f;  // 0..1, multiplied with color_pauta alpha
  bool snap_cuadricula = false; // optional snap to grid
  float margen_derecho = 0.0f;
  float margen_superior = 0.0f;
  float margen_inferior = 0.0f;
  uint32_t color_personalizado = 0;  // if non-zero, overrides color_fondo
  uint32_t color_pauta_personalizado = 0;
  float grosor_pauta = 1.0f;
  float separacion_secundaria = 0.0f;
  bool mostrar_ejes = false;
  float eje_x = 620.0f;
  float eje_y = 877.0f;
  bool pauta_al_exportar = true;

  uint32_t color_fondo() const;
  uint32_t color_pauta() const;
  // Si el papel es oscuro, la tinta negra por defecto se vuelve clara: escribir
  // en negro sobre negro es el error más fácil de cometer aquí.
  bool es_oscuro() const;
  uint32_t color_tinta_por_defecto() const;
};

// Un segmento de la pauta, en píxeles lógicos.
struct LineaGuia {
  Punto desde;
  Punto hasta;
  bool destacada = false;  // el margen, o la línea gruesa de la cuadrícula
};

// Genera la pauta visible dentro de un rectángulo.
//
// Se genera solo lo visible y no la página entera: en un lienzo de altura
// infinita, dibujar toda la pauta sería trabajo sin límite.
std::vector<LineaGuia> GenerarPauta(const Papel& papel, float x, float y,
                                    float ancho, float alto);

// Tamaños estándar de página en píxeles lógicos a 150 ppp.
// 150 ppp es la densidad del prototipo actual (ancho_maximo = 1240 ≈ 210mm × 150/25.4).
enum class TamanoPagina {
  Personalizado,
  A4,          // 210 × 297 mm
  Carta,       // 216 × 279 mm (US Letter)
  Oficio,      // 216 × 356 mm (US Legal)
  A3,          // 297 × 420 mm
  A5,          // 148 × 210 mm
  Dieciseis9,  // 16:9 al ancho A4
  Dieciseis10, // 16:10 al ancho A4
  Cuatro3,     // 4:3 al ancho A4
  CuadernoIngenieria, // Engineering notebook: wider with margin
};

// Devuelve ancho y alto en píxeles lógicos a 150 ppp. Para Personalizado devuelve {0,0}.
struct DimensionesPagina {
  float ancho;
  float alto;
};

// ---------------------------------------------------------------------------
// Página, sección, cuaderno
// ---------------------------------------------------------------------------

enum class OrientacionPagina { Vertical, Horizontal };

struct Pagina {
  std::string titulo;
  std::vector<Trazo> trazos;
  std::vector<CuadroTexto> textos;
  std::vector<Medio> medios;
  // Solo las notas de alcance Página. Las de cuaderno y globales viven arriba.
  std::vector<NotaFija> notas;
  Papel papel;
  LimitesLienzo limites;
  OrientacionPagina orientacion = OrientacionPagina::Vertical;
  float ancho = 1240.0f;
  float alto = 1754.0f;

  // Índice del PDF importado al que corresponde esta página, o -1 si no viene de
  // un PDF. Permite que una página del PDF se mueva libremente entre secciones
  // sin perder de dónde salió.
  int pagina_pdf = -1;

  // Rectángulo que abarca toda la tinta. Sirve para exportar recortado y para
  // hacer crecer el lienzo cuando la altura es infinita.
  bool CajaDeTinta(float* x, float* y, float* ancho, float* alto) const;

  // Crece el lienzo si la tinta se acerca al borde por un eje infinito.
  // Devuelve true si el tamaño cambió.
  bool CrecerSiHaceFalta(float margen);
};

struct Seccion {
  std::string titulo;
  std::vector<Pagina> paginas;
};

struct Cuaderno {
  std::string titulo;
  std::vector<Seccion> secciones;
  // Notas que siguen visibles en todas las páginas de este cuaderno.
  std::vector<NotaFija> notas;
};

struct Documento {
  std::string id;
  std::string titulo;
  std::vector<Cuaderno> cuadernos;
  // Notas que siguen visibles aunque se cambie de cuaderno.
  std::vector<NotaFija> notas;
};

DimensionesPagina DimensionesDeTamano(TamanoPagina tamano);

// Crea una Pagina con el tamaño y la orientación dados, aplicando los límites.
Pagina CrearPagina(TamanoPagina tamano, OrientacionPagina orientacion = OrientacionPagina::Vertical);

// Ajusta una página existente a un nuevo tamaño sin mover la tinta.
void CambiarTamano(Pagina* pagina, TamanoPagina tamano);

// Ajusta los límites para que la página sea del tamaño fijo dado (ambos ejes no infinitos).
void FijarTamano(Pagina* pagina, float ancho, float alto);

// Convierte una página fija a lienzo infinito (ambos ejes).
void HacerInfinito(Pagina* pagina);

// Convierte el eje vertical a infinito dejando el ancho fijo.
void HacerVerticalInfinito(Pagina* pagina);

// Convierte el eje horizontal a infinito dejando el alto fijo.
void HacerHorizontalInfinito(Pagina* pagina);

// Ajusta el tamaño de la página para abarcar exactamente la tinta más un margen.
bool AjustarAlContenido(Pagina* pagina, float margen = 20.0f);

// Mueve una página entre secciones, o dentro de la misma. Devuelve false si los
// índices no existen. Es la operación de reordenar páginas de un PDF.
bool MoverPagina(Cuaderno& cuaderno, size_t seccion_origen, size_t pagina_origen,
                 size_t seccion_destino, size_t posicion_destino);

// ---------------------------------------------------------------------------
// Notas fijas
// ---------------------------------------------------------------------------

// Dónde está el lector ahora mismo.
struct Ubicacion {
  size_t cuaderno = 0;
  size_t seccion = 0;
  size_t pagina = 0;
};

// Punteros a las notas visibles desde una ubicación: las de la página, más las
// del cuaderno, más las globales.
//
// Son punteros al documento, no copias, para poder moverlas y editarlas. Valen
// hasta que se modifique la estructura del documento.
std::vector<NotaFija*> NotasVisibles(Documento& documento, const Ubicacion& donde);

// Cambia el alcance de una nota, moviéndola al contenedor que le toca.
//
// Es la operación de arrastrar un papelito de la hoja al escritorio: la nota
// deja de pertenecer a la página y pasa a seguir al lector.
bool CambiarAlcanceNota(Documento& documento, const std::string& id_nota,
                        AlcanceNota nuevo, const Ubicacion& donde);

// Busca una nota por identificador en todo el documento.
NotaFija* BuscarNota(Documento& documento, const std::string& id_nota);

}  // namespace tinta
