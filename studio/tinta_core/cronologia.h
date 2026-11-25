// Cronología vectorial de una Nota Viva.
//
// Una grabación no guarda una captura completa de la página en cada toque. Se
// conserva una página base y, después, únicamente los elementos que cambiaron.
// Eso permite reconstruir inserciones, ediciones, transformaciones, borrados,
// deshacer y rehacer sin convertir el cuaderno en un vídeo de píxeles ni
// multiplicar por miles el tamaño del archivo.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "documento.h"
#include "edicion.h"

namespace tinta {

struct EventoCronologia {
  int64_t tiempo_qpc = 0;

  // `delta` contiene solamente los elementos que se crean o sustituyen. Sus
  // metadatos de página se aplican únicamente cuando `cambia_metadatos` vale
  // true y sus notas cuando `cambia_notas` vale true.
  Pagina delta;
  std::vector<uint32_t> indices_trazos;
  std::vector<uint32_t> indices_textos;
  std::vector<uint32_t> indices_medios;

  std::vector<std::string> trazos_eliminados;
  std::vector<std::string> textos_eliminados;
  std::vector<std::string> medios_eliminados;

  // Solo se escribe una lista de orden cuando cambió el orden relativo de los
  // elementos que ya existían. Insertar o borrar no obliga a repetir todos los
  // identificadores de una página grande.
  std::vector<std::string> orden_trazos;
  std::vector<std::string> orden_textos;
  std::vector<std::string> orden_medios;

  bool cambia_metadatos = false;
  bool cambia_notas = false;
};

class CronologiaDocumento {
 public:
  // Puede modificar identificadores vacíos o duplicados de la página. Una
  // cronología no puede referirse de forma inequívoca a un elemento sin ID.
  bool Comenzar(Pagina* pagina, const std::string& documento_id,
                int64_t inicio_qpc);
  void Cancelar();

  bool activa() const { return activa_; }
  bool cargada() const { return cargada_; }
  size_t cantidad_eventos() const { return eventos_.size(); }
  int64_t inicio_qpc() const { return inicio_qpc_; }

  // Normaliza IDs después de una operación que pudo crear fragmentos (por
  // ejemplo el borrador parcial) y antes de copiarla al historial.
  bool PrepararPagina(Pagina* pagina) { return AsegurarIdentidades(pagina); }

  // Registra la consecuencia visible de una operación del historial. En
  // deshacer se invierte el antes/después; en rehacer se usa la dirección
  // normal. `pagina_despues` es también la fuente de los IDs ya normalizados.
  void RegistrarOperacion(const Operacion& operacion,
                          Pagina* pagina_despues,
                          int64_t tiempo_qpc,
                          bool invertida = false);

  // Cambios de papel, tamaño o cualquier operación que todavía no pertenezca
  // al historial pueden registrarse comparando dos páginas. Es una ruta menos
  // frecuente y deliberadamente más general.
  void RegistrarCambio(const Pagina& antes, Pagina* despues,
                       int64_t tiempo_qpc);
  void RegistrarMetadatos(const Pagina& antes, const Pagina& despues,
                          int64_t tiempo_qpc);

  // Las correcciones tardías de UIKit deben modificar el trazo ya guardado en
  // la cronología, no aparecer como un salto artificial posterior.
  size_t ActualizarMuestraEstimada(uint64_t id_muestra,
                                   float delta_x, float delta_y,
                                   float presion, float altitud, float azimut,
                                   bool posicion, bool fuerza,
                                   bool inclinacion, bool direccion);

  std::vector<uint8_t> Serializar() const;
  bool Cargar(const std::vector<uint8_t>& datos, Pagina* pagina_inicial);

  // Devuelve true si la composición de la página cambió. Aunque devuelva
  // false, el render de tinta puede necesitar avanzar el corte temporal dentro
  // de un trazo cuyas muestras ya estaban cargadas.
  bool MostrarHasta(int64_t tiempo_qpc, Pagina* pagina);

 private:
  bool AsegurarIdentidades(Pagina* pagina);
  void RegistrarEvento(EventoCronologia evento);
  void ReiniciarReproduccion();

  Pagina base_;
  std::vector<EventoCronologia> eventos_;
  std::string documento_id_;
  uint64_t siguiente_id_ = 1;
  int64_t inicio_qpc_ = 0;
  bool activa_ = false;
  bool cargada_ = false;

  Pagina estado_reproduccion_;
  size_t indice_reproduccion_ = 0;
  int64_t ultimo_tiempo_mostrado_ = 0;
};

}  // namespace tinta
