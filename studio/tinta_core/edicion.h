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

std::vector<size_t> TrazosTocados(const Pagina& pagina, const Punto& centro,
                                  float radio);
std::vector<Trazo> BorrarTrazosCompletos(Pagina& pagina,
                                         const std::vector<size_t>& indices);
std::vector<Trazo> PartirTrazo(const Trazo& trazo, const Punto& centro,
                               float radio);
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
  uint32_t color_especifico = 0;
};

struct Seleccion {
  std::vector<size_t> trazos;
  std::vector<size_t> textos;
  std::vector<size_t> medios;

  bool vacia() const { return trazos.empty() && textos.empty() && medios.empty(); }
};

bool PuntoEnPoligono(const std::vector<Punto>& poligono, const Punto& punto);
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
  float escala_x = 1.0f;
  float escala_y = 1.0f;
  float rotacion = 0.0f;
  Punto pivote{0.0f, 0.0f};
  bool reflejar_horizontal = false;
  bool reflejar_vertical = false;
};

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
  AnadirTextos,
  QuitarTextos,
  ReemplazarTextos,
  AnadirMedios,
  QuitarMedios,
  ReemplazarMedios,
};

struct Operacion {
  TipoOperacion tipo = TipoOperacion::AnadirTrazos;
  std::vector<size_t> indices;
  std::vector<size_t> indices_textos;
  std::vector<size_t> indices_medios;
  std::vector<Trazo> antes;
  std::vector<Trazo> despues;
  std::vector<CuadroTexto> textos_antes;
  std::vector<CuadroTexto> textos_despues;
  std::vector<Medio> medios_antes;
  std::vector<Medio> medios_despues;
};

class TransaccionHistorial;

class Historial {
 public:
  void Registrar(const Operacion& operacion);
  bool Deshacer(Pagina& pagina, Operacion* aplicada = nullptr);
  bool Rehacer(Pagina& pagina, Operacion* aplicada = nullptr);
  void Limpiar();

  size_t ActualizarMuestraEstimada(uint64_t id_muestra,
                                   float delta_x, float delta_y,
                                   float presion, float altitud, float azimut,
                                   bool posicion, bool fuerza,
                                   bool inclinacion, bool direccion);

  bool puede_deshacer() const { return !hechas_.empty(); }
  bool puede_rehacer() const { return !deshechas_.empty(); }
  size_t profundidad() const { return hechas_.size(); }
  bool transaccion_activa() const { return transaccion_activa_; }

  static Operacion OperacionAnadir(size_t indice, const Trazo& trazo);
  static Operacion OperacionQuitar(const std::vector<size_t>& indices,
                                   const std::vector<Trazo>& trazos);
  static Operacion OperacionReemplazar(const std::vector<size_t>& indices,
                                       const std::vector<Trazo>& antes,
                                       const std::vector<Trazo>& despues);
  static Operacion OperacionPagina(const std::vector<Trazo>& antes,
                                   const std::vector<Trazo>& despues);
  static Operacion OperacionPaginaCompleta(const Pagina& antes,
                                           const Pagina& despues);
  static Operacion OperacionAnadirTexto(size_t indice,
                                        const CuadroTexto& texto);
  static Operacion OperacionQuitarTexto(size_t indice,
                                        const CuadroTexto& texto);
  static Operacion OperacionReemplazarTexto(size_t indice,
                                             const CuadroTexto& antes,
                                             const CuadroTexto& despues);
  static Operacion OperacionAnadirMedio(size_t indice, const Medio& medio);
  static Operacion OperacionQuitarMedio(size_t indice,
                                        const Medio& medio);
  static Operacion OperacionReemplazarMedio(size_t indice,
                                             const Medio& antes,
                                             const Medio& despues);

 private:
  friend class TransaccionHistorial;
  static void Aplicar(Pagina& pagina, const Operacion& operacion, bool invertida);

  std::vector<Operacion> hechas_;
  std::vector<Operacion> deshechas_;
  bool transaccion_activa_ = false;
};

}  // namespace tinta
