// Edición: borrador, selección por lazo, transformaciones e historial.
//
// Todo esto es geometría y estructuras de datos, así que vive en el núcleo y se
// prueba en Windows. La capa nativa solo decide cuándo llamarlo.
#pragma once

#include <cstddef>
#include <vector>

#include "documento.h"
#include "tinta.h"

namespace tinta {

// ---------------------------------------------------------------------------
// Borrador
// ---------------------------------------------------------------------------

// Índices de los trazos que toca un borrador circular.
//
// Se compara contra los segmentos y no solo contra las muestras: con un trazo
// rápido, dos muestras consecutivas pueden quedar a decenas de píxeles y el
// borrador se colaría entre ellas sin borrar nada.
std::vector<size_t> TrazosTocados(const Pagina& pagina, const Punto& centro,
                                  float radio);

// Borra trazos completos. Devuelve los que se quitaron, en orden, para poder
// deshacer.
std::vector<Trazo> BorrarTrazosCompletos(Pagina& pagina,
                                         const std::vector<size_t>& indices);

// Parte un trazo por donde pasa el borrador y devuelve los pedazos que quedan.
//
// Un trazo partido por el medio da dos trazos; borrado entero, ninguno. Es la
// diferencia entre el borrador «por objeto» y el «por píxel» de GoodNotes.
std::vector<Trazo> PartirTrazo(const Trazo& trazo, const Punto& centro,
                               float radio);

// Aplica el borrador parcial a toda la página. Devuelve true si algo cambió.
bool BorrarParcial(Pagina& pagina, const Punto& centro, float radio);

// ---------------------------------------------------------------------------
// Selección por lazo
// ---------------------------------------------------------------------------

enum class TipoFiltroLazo {
  Todos,
  SoloTinta,
  SoloTexto,
  SoloImagen,
  SoloPdf
};

struct FiltroLazo {
  TipoFiltroLazo tipo = TipoFiltroLazo::Todos;
  uint32_t color_especifico = 0; // Si es distinto de 0, solo selecciona trazos de este color exacto
};

struct Seleccion {
  std::vector<size_t> trazos;
  std::vector<size_t> textos;
  std::vector<size_t> medios;

  bool vacia() const { return trazos.empty() && textos.empty() && medios.empty(); }
};

bool PuntoEnPoligono(const std::vector<Punto>& poligono, const Punto& punto);

// Trazos dentro del lazo.
//
// Con `requerir_completo` solo entran los que caen enteros dentro, que es el
// comportamiento que espera quien viene de GoodNotes. Sin él basta con que una
// muestra esté dentro.
std::vector<size_t> SeleccionarConLazo(const Pagina& pagina,
                                       const std::vector<Punto>& poligono,
                                       bool requerir_completo = true);

Seleccion SeleccionarPoligono(const Pagina& pagina, const std::vector<Punto>& poligono, const FiltroLazo& filtro = {}, bool requerir_completo = true);
Seleccion SeleccionarRectangulo(const Pagina& pagina, float x, float y, float ancho, float alto, const FiltroLazo& filtro = {}, bool requerir_completo = true);
Seleccion SeleccionarElipse(const Pagina& pagina, const Punto& centro, float radio_x, float radio_y, const FiltroLazo& filtro = {}, bool requerir_completo = true);
Seleccion SeleccionarPincel(const Pagina& pagina,
                            const std::vector<Punto>& recorrido,
                            float radio, const FiltroLazo& filtro = {},
                            bool requerir_completo = false);

Seleccion UnirSelecciones(const Seleccion& a, const Seleccion& b);
Seleccion RestarSelecciones(const Seleccion& original, const Seleccion& a_quitar);
Seleccion InvertirSeleccion(const Pagina& pagina, const Seleccion& actual, const FiltroLazo& filtro = {});

void TraerAlFrente(Pagina& pagina, const Seleccion& seleccion);
void EnviarAtras(Pagina& pagina, const Seleccion& seleccion);

// ---------------------------------------------------------------------------
// Transformaciones
// ---------------------------------------------------------------------------

struct Transformacion {
  float desplazar_x = 0.0f;
  float desplazar_y = 0.0f;
  float escala = 1.0f;
  // Escala adicional por eje. Mantener ambas en 1 conserva exactamente el
  // comportamiento de la API histórica de escala proporcional.
  float escala_x = 1.0f;
  float escala_y = 1.0f;
  float rotacion = 0.0f;  // radianes
  Punto pivote{0.0f, 0.0f};
  bool reflejar_horizontal = false;
  bool reflejar_vertical = false;
};

// Aplica una transformación a los trazos indicados.
//
// El grosor se escala con la figura: si no, al agrandar una anotación la letra
// crece pero la pluma se queda fina y se ve como otra cosa.
void TransformarTrazos(Pagina& pagina, const std::vector<size_t>& indices,
                       const Transformacion& t);
void TransformarSeleccion(Pagina& pagina, const Seleccion& seleccion,
                          const Transformacion& t);

void RecolorearTrazos(Pagina& pagina, const std::vector<size_t>& indices,
                      uint32_t color_argb);

// ---------------------------------------------------------------------------
// Historial
// ---------------------------------------------------------------------------

enum class TipoOperacion {
  AnadirTrazos,
  QuitarTrazos,
  ReemplazarTrazos,
  ReemplazarPaginaTrazos,
  ReemplazarPaginaCompleta,
};

// Una operación deshacible.
//
// Guarda el estado anterior de lo que tocó, no la página entera: una página con
// miles de trazos no se puede copiar en cada pincelada.
struct Operacion {
  TipoOperacion tipo = TipoOperacion::AnadirTrazos;
  std::vector<size_t> indices;
  std::vector<Trazo> antes;
  std::vector<Trazo> despues;
  std::vector<CuadroTexto> textos_antes;
  std::vector<CuadroTexto> textos_despues;
  std::vector<Medio> medios_antes;
  std::vector<Medio> medios_despues;
};

class Historial {
 public:
  void Registrar(const Operacion& operacion);
  bool Deshacer(Pagina& pagina);
  bool Rehacer(Pagina& pagina);
  void Limpiar();

  // Propaga una corrección tardía del digitalizador por todas las copias de
  // una muestra guardadas en el historial. La posición se aplica como delta
  // para conservar transformaciones hechas después de levantar el lápiz.
  size_t ActualizarMuestraEstimada(uint64_t id_muestra,
                                   float delta_x, float delta_y,
                                   float presion, float altitud, float azimut,
                                   bool posicion, bool fuerza,
                                   bool inclinacion, bool direccion);

  bool puede_deshacer() const { return !hechas_.empty(); }
  bool puede_rehacer() const { return !deshechas_.empty(); }
  size_t profundidad() const { return hechas_.size(); }

  // Ayudas para registrar sin construir la operación a mano.
  static Operacion OperacionAnadir(size_t indice, const Trazo& trazo);
  static Operacion OperacionQuitar(const std::vector<size_t>& indices,
                                   const std::vector<Trazo>& trazos);
  static Operacion OperacionReemplazar(const std::vector<size_t>& indices,
                                       const std::vector<Trazo>& antes,
                                       const std::vector<Trazo>& despues);
  // Para operaciones cuyo número de trazos puede cambiar, por ejemplo borrar
  // por píxel (un trazo puede convertirse en varios) o limpiar la página.
  static Operacion OperacionPagina(const std::vector<Trazo>& antes,
                                   const std::vector<Trazo>& despues);
  static Operacion OperacionPaginaCompleta(const Pagina& antes,
                                           const Pagina& despues);

 private:
  static void Aplicar(Pagina& pagina, const Operacion& operacion, bool invertida);

  std::vector<Operacion> hechas_;
  std::vector<Operacion> deshechas_;
};

}  // namespace tinta
