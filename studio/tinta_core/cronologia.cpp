#include "cronologia.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "archivo.h"

namespace tinta {
namespace {

constexpr char kMagia[] = {'T', 'I', 'N', 'T', 'A', 'L', 'I', 'V'};
constexpr uint16_t kVersion = 1;
constexpr uint32_t kMaximoEventos = 2'000'000u;
constexpr uint32_t kMaximoElementosEvento = 2'000'000u;
constexpr uint32_t kMaximoTexto = 16u * 1024u * 1024u;

class Escritor {
 public:
  void U8(uint8_t valor) { Crudo(&valor, sizeof(valor)); }
  void U16(uint16_t valor) { Crudo(&valor, sizeof(valor)); }
  void U32(uint32_t valor) { Crudo(&valor, sizeof(valor)); }
  void I64(int64_t valor) { Crudo(&valor, sizeof(valor)); }
  void Bool(bool valor) { U8(valor ? 1u : 0u); }

  void Texto(const std::string& texto) {
    if (texto.size() > kMaximoTexto ||
        texto.size() > std::numeric_limits<uint32_t>::max()) {
      bien_ = false;
      return;
    }
    U32(static_cast<uint32_t>(texto.size()));
    Crudo(texto.data(), texto.size());
  }

  void Bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() > std::numeric_limits<uint32_t>::max()) {
      bien_ = false;
      return;
    }
    U32(static_cast<uint32_t>(bytes.size()));
    Crudo(bytes.data(), bytes.size());
  }

  const std::vector<uint8_t>& datos() const { return datos_; }
  bool bien() const { return bien_; }

 private:
  void Crudo(const void* origen, size_t cantidad) {
    if (!bien_ || cantidad > kMaximoBytesArchivo ||
        datos_.size() > kMaximoBytesArchivo - cantidad) {
      bien_ = false;
      return;
    }
    const auto* p = static_cast<const uint8_t*>(origen);
    datos_.insert(datos_.end(), p, p + cantidad);
  }

  std::vector<uint8_t> datos_;
  bool bien_ = true;
};

class Lector {
 public:
  explicit Lector(const std::vector<uint8_t>& datos)
      : datos_(datos.data()), cantidad_(datos.size()) {}

  uint8_t U8() { uint8_t v = 0; Crudo(&v, sizeof(v)); return v; }
  uint16_t U16() { uint16_t v = 0; Crudo(&v, sizeof(v)); return v; }
  uint32_t U32() { uint32_t v = 0; Crudo(&v, sizeof(v)); return v; }
  int64_t I64() { int64_t v = 0; Crudo(&v, sizeof(v)); return v; }
  bool Bool() {
    const uint8_t v = U8();
    if (v > 1u) bien_ = false;
    return v == 1u;
  }

  std::string Texto() {
    const uint32_t n = U32();
    if (!bien_ || n > kMaximoTexto || n > Restantes()) {
      bien_ = false;
      return {};
    }
    std::string salida(reinterpret_cast<const char*>(datos_ + posicion_), n);
    posicion_ += n;
    return salida;
  }

  std::vector<uint8_t> Bytes() {
    const uint32_t n = U32();
    if (!bien_ || n > kMaximoBytesArchivo || n > Restantes()) {
      bien_ = false;
      return {};
    }
    std::vector<uint8_t> salida(datos_ + posicion_, datos_ + posicion_ + n);
    posicion_ += n;
    return salida;
  }

  bool bien() const { return bien_; }
  bool al_final() const { return bien_ && posicion_ == cantidad_; }

 private:
  size_t Restantes() const {
    return posicion_ <= cantidad_ ? cantidad_ - posicion_ : 0u;
  }

  void Crudo(void* destino, size_t n) {
    if (!bien_ || n > Restantes()) {
      bien_ = false;
      return;
    }
    std::memcpy(destino, datos_ + posicion_, n);
    posicion_ += n;
  }

  const uint8_t* datos_ = nullptr;
  size_t cantidad_ = 0;
  size_t posicion_ = 0;
  bool bien_ = true;
};

Documento DocumentoDePagina(const Pagina& pagina) {
  Documento documento;
  documento.id = "cronologia-nota-viva";
  documento.titulo = "Cronologia Nota Viva";
  Cuaderno cuaderno;
  Seccion seccion;
  seccion.paginas.push_back(pagina);
  cuaderno.secciones.push_back(std::move(seccion));
  documento.cuadernos.push_back(std::move(cuaderno));
  return documento;
}

bool ExtraerPagina(const std::vector<uint8_t>& datos, Pagina* pagina) {
  if (!pagina) return false;
  Documento documento;
  if (!Deserializar(datos, &documento) || documento.cuadernos.size() != 1u ||
      documento.cuadernos[0].secciones.size() != 1u ||
      documento.cuadernos[0].secciones[0].paginas.size() != 1u) {
    return false;
  }
  *pagina = std::move(documento.cuadernos[0].secciones[0].paginas[0]);
  return true;
}

bool Igual(const Muestra& a, const Muestra& b) {
  return a.posicion.x == b.posicion.x && a.posicion.y == b.posicion.y &&
         a.presion == b.presion && a.inclinacion_x == b.inclinacion_x &&
         a.inclinacion_y == b.inclinacion_y &&
         a.tiempo_qpc == b.tiempo_qpc;
}

bool Igual(const EstiloTrazo& a, const EstiloTrazo& b) {
  return a.opacidad == b.opacidad &&
         a.exponente_presion == b.exponente_presion &&
         a.factor_ancho_minimo == b.factor_ancho_minimo &&
         a.velocidad_ancho_minimo == b.velocidad_ancho_minimo &&
         a.suavizado_ancho == b.suavizado_ancho &&
         a.estabilizacion == b.estabilizacion &&
         a.taper_inicio == b.taper_inicio && a.taper_fin == b.taper_fin &&
         a.sensibilidad_inclinacion == b.sensibilidad_inclinacion &&
         a.textura == b.textura && a.nivel_refinado == b.nivel_refinado;
}

bool Igual(const Trazo& a, const Trazo& b) {
  if (a.id_colaboracion != b.id_colaboracion ||
      a.ancho_base != b.ancho_base || a.color_argb != b.color_argb ||
      a.resaltador != b.resaltador ||
      a.estilo_persistente != b.estilo_persistente ||
      (a.estilo_persistente && !Igual(a.estilo, b.estilo)) ||
      a.muestras.size() != b.muestras.size()) return false;
  for (size_t i = 0; i < a.muestras.size(); ++i) {
    if (!Igual(a.muestras[i], b.muestras[i])) return false;
  }
  return true;
}

bool Igual(const Rect& a, const Rect& b) {
  return a.x == b.x && a.y == b.y && a.ancho == b.ancho &&
         a.alto == b.alto;
}

bool Igual(const EstiloTexto& a, const EstiloTexto& b) {
  return a.familia == b.familia && a.tamano == b.tamano &&
         a.negrita == b.negrita && a.cursiva == b.cursiva &&
         a.subrayado == b.subrayado && a.tachado == b.tachado &&
         a.color_argb == b.color_argb &&
         a.color_resaltado == b.color_resaltado &&
         a.alineacion == b.alineacion && a.interlineado == b.interlineado;
}

bool Igual(const CuadroTexto& a, const CuadroTexto& b) {
  return a.id == b.id && a.texto == b.texto && Igual(a.marco, b.marco) &&
         Igual(a.estilo, b.estilo) && a.rotacion == b.rotacion &&
         a.alto_automatico == b.alto_automatico &&
         a.desde_dictado == b.desde_dictado;
}

bool Igual(const Medio& a, const Medio& b) {
  return a.id == b.id && a.hash == b.hash &&
         a.nombre_original == b.nombre_original && a.tipo == b.tipo &&
         Igual(a.marco, b.marco) && a.rotacion == b.rotacion &&
         a.opacidad == b.opacidad && a.recorte_x == b.recorte_x &&
         a.recorte_y == b.recorte_y && a.recorte_ancho == b.recorte_ancho &&
         a.recorte_alto == b.recorte_alto &&
         a.pagina_fuente == b.pagina_fuente &&
         a.segundo_inicio == b.segundo_inicio;
}

bool Igual(const NotaFija& a, const NotaFija& b) {
  if (a.id != b.id || a.texto != b.texto || !Igual(a.estilo, b.estilo) ||
      a.posicion.x != b.posicion.x || a.posicion.y != b.posicion.y ||
      a.ancho != b.ancho || a.alto != b.alto ||
      a.color_argb != b.color_argb || a.alcance != b.alcance ||
      a.contraida != b.contraida || a.anclada != b.anclada ||
      a.trazos.size() != b.trazos.size()) return false;
  for (size_t i = 0; i < a.trazos.size(); ++i) {
    if (!Igual(a.trazos[i], b.trazos[i])) return false;
  }
  return true;
}

bool IgualNotas(const std::vector<NotaFija>& a,
                const std::vector<NotaFija>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (!Igual(a[i], b[i])) return false;
  }
  return true;
}

bool Igual(const Papel& a, const Papel& b) {
  return a.tipo == b.tipo && a.pauta == b.pauta &&
         a.separacion == b.separacion &&
         a.margen_izquierdo == b.margen_izquierdo &&
         a.opacidad_pauta == b.opacidad_pauta &&
         a.snap_cuadricula == b.snap_cuadricula &&
         a.margen_derecho == b.margen_derecho &&
         a.margen_superior == b.margen_superior &&
         a.margen_inferior == b.margen_inferior &&
         a.color_personalizado == b.color_personalizado &&
         a.color_pauta_personalizado == b.color_pauta_personalizado &&
         a.grosor_pauta == b.grosor_pauta &&
         a.separacion_secundaria == b.separacion_secundaria &&
         a.mostrar_ejes == b.mostrar_ejes && a.eje_x == b.eje_x &&
         a.eje_y == b.eje_y && a.pauta_al_exportar == b.pauta_al_exportar;
}

bool Igual(const LimitesLienzo& a, const LimitesLienzo& b) {
  return a.ancho_infinito == b.ancho_infinito &&
         a.alto_infinito == b.alto_infinito &&
         a.ancho_minimo == b.ancho_minimo &&
         a.ancho_maximo == b.ancho_maximo &&
         a.alto_minimo == b.alto_minimo && a.alto_maximo == b.alto_maximo;
}

bool MetadatosIguales(const Pagina& a, const Pagina& b) {
  return a.titulo == b.titulo && Igual(a.papel, b.papel) &&
         Igual(a.limites, b.limites) && a.orientacion == b.orientacion &&
         a.ancho == b.ancho && a.alto == b.alto &&
         a.pagina_pdf == b.pagina_pdf;
}

template <typename T, typename ObtenerId, typename SonIguales>
void Diferenciar(const std::vector<T>& antes, const std::vector<T>& despues,
                 std::vector<T>* reemplazos,
                 std::vector<uint32_t>* indices,
                 std::vector<std::string>* eliminados,
                 std::vector<std::string>* orden,
                 ObtenerId obtener_id, SonIguales son_iguales) {
  if (!reemplazos || !indices || !eliminados || !orden) return;
  std::unordered_map<std::string, const T*> mapa_antes;
  std::unordered_set<std::string> ids_despues;
  mapa_antes.reserve(antes.size());
  ids_despues.reserve(despues.size());
  for (const auto& item : antes) mapa_antes[obtener_id(item)] = &item;

  for (size_t i = 0; i < despues.size(); ++i) {
    const std::string& id = obtener_id(despues[i]);
    ids_despues.insert(id);
    const auto previo = mapa_antes.find(id);
    if (previo == mapa_antes.end() || !son_iguales(*previo->second, despues[i])) {
      reemplazos->push_back(despues[i]);
      indices->push_back(static_cast<uint32_t>(i));
    }
  }
  for (const auto& item : antes) {
    const std::string& id = obtener_id(item);
    if (!ids_despues.contains(id)) eliminados->push_back(id);
  }

  // Solo un cambio en el orden relativo de IDs comunes necesita una lista de
  // orden. Las posiciones de altas nuevas ya viajan en `indices`.
  std::vector<std::string> comunes_antes;
  std::vector<std::string> comunes_despues;
  comunes_antes.reserve(antes.size());
  comunes_despues.reserve(despues.size());
  for (const auto& item : antes) {
    const auto& id = obtener_id(item);
    if (ids_despues.contains(id)) comunes_antes.push_back(id);
  }
  for (const auto& item : despues) {
    const auto& id = obtener_id(item);
    if (mapa_antes.contains(id)) comunes_despues.push_back(id);
  }
  if (comunes_antes != comunes_despues) {
    orden->reserve(despues.size());
    for (const auto& item : despues) orden->push_back(obtener_id(item));
  }
}

void DiferenciarTrazos(const std::vector<Trazo>& antes,
                       const std::vector<Trazo>& despues,
                       EventoCronologia* evento) {
  Diferenciar(antes, despues, &evento->delta.trazos,
              &evento->indices_trazos, &evento->trazos_eliminados,
              &evento->orden_trazos,
              [](const Trazo& t) -> const std::string& {
                return t.id_colaboracion;
              }, [](const Trazo& a, const Trazo& b) { return Igual(a, b); });
}

void DiferenciarTextos(const std::vector<CuadroTexto>& antes,
                       const std::vector<CuadroTexto>& despues,
                       EventoCronologia* evento) {
  Diferenciar(antes, despues, &evento->delta.textos,
              &evento->indices_textos, &evento->textos_eliminados,
              &evento->orden_textos,
              [](const CuadroTexto& t) -> const std::string& { return t.id; },
              [](const CuadroTexto& a, const CuadroTexto& b) {
                return Igual(a, b);
              });
}

void DiferenciarMedios(const std::vector<Medio>& antes,
                       const std::vector<Medio>& despues,
                       EventoCronologia* evento) {
  Diferenciar(antes, despues, &evento->delta.medios,
              &evento->indices_medios, &evento->medios_eliminados,
              &evento->orden_medios,
              [](const Medio& m) -> const std::string& { return m.id; },
              [](const Medio& a, const Medio& b) { return Igual(a, b); });
}

template <typename T, typename ObtenerId>
bool AjustarIndices(const std::vector<T>& reemplazos,
                    const std::vector<T>& estado_actual,
                    std::vector<uint32_t>* indices,
                    ObtenerId obtener_id) {
  if (!indices || reemplazos.size() != indices->size()) return false;
  for (size_t i = 0; i < reemplazos.size(); ++i) {
    const auto encontrado = std::find_if(
        estado_actual.begin(), estado_actual.end(), [&](const T& item) {
          return obtener_id(item) == obtener_id(reemplazos[i]);
        });
    if (encontrado == estado_actual.end()) return false;
    (*indices)[i] = static_cast<uint32_t>(
        encontrado - estado_actual.begin());
  }
  return true;
}

bool EventoVacio(const EventoCronologia& e) {
  return e.delta.trazos.empty() && e.delta.textos.empty() &&
         e.delta.medios.empty() && e.trazos_eliminados.empty() &&
         e.textos_eliminados.empty() && e.medios_eliminados.empty() &&
         e.orden_trazos.empty() && e.orden_textos.empty() &&
         e.orden_medios.empty() && !e.cambia_metadatos && !e.cambia_notas;
}

template <typename T, typename ObtenerId>
void EliminarIds(std::vector<T>* items,
                 const std::vector<std::string>& eliminados,
                 ObtenerId obtener_id) {
  if (!items || eliminados.empty()) return;
  const std::unordered_set<std::string> ids(eliminados.begin(),
                                             eliminados.end());
  items->erase(std::remove_if(items->begin(), items->end(),
                              [&](const T& item) {
                                return ids.contains(obtener_id(item));
                              }), items->end());
}

template <typename T, typename ObtenerId>
bool AplicarReemplazos(std::vector<T>* destino,
                       const std::vector<T>& reemplazos,
                       const std::vector<uint32_t>& indices,
                       ObtenerId obtener_id) {
  if (!destino || reemplazos.size() != indices.size()) return false;
  for (size_t i = 0; i < reemplazos.size(); ++i) {
    const auto& id = obtener_id(reemplazos[i]);
    const auto existente = std::find_if(
        destino->begin(), destino->end(), [&](const T& actual) {
          return obtener_id(actual) == id;
        });
    if (existente != destino->end()) {
      *existente = reemplazos[i];
    } else {
      const size_t indice = std::min<size_t>(indices[i], destino->size());
      destino->insert(destino->begin() + static_cast<std::ptrdiff_t>(indice),
                      reemplazos[i]);
    }
  }
  return true;
}

template <typename T, typename ObtenerId>
bool AplicarOrden(std::vector<T>* items,
                  const std::vector<std::string>& orden,
                  ObtenerId obtener_id) {
  if (!items || orden.empty()) return true;
  if (items->size() != orden.size()) return false;
  std::unordered_map<std::string, T> por_id;
  por_id.reserve(items->size());
  for (auto& item : *items) {
    if (!por_id.emplace(obtener_id(item), std::move(item)).second) return false;
  }
  std::vector<T> ordenados;
  ordenados.reserve(orden.size());
  for (const auto& id : orden) {
    auto encontrado = por_id.find(id);
    if (encontrado == por_id.end()) return false;
    ordenados.push_back(std::move(encontrado->second));
    por_id.erase(encontrado);
  }
  if (!por_id.empty()) return false;
  *items = std::move(ordenados);
  return true;
}

bool AplicarEvento(const EventoCronologia& evento, Pagina* pagina) {
  if (!pagina) return false;
  EliminarIds(&pagina->trazos, evento.trazos_eliminados,
              [](const Trazo& t) -> const std::string& {
                return t.id_colaboracion;
              });
  EliminarIds(&pagina->textos, evento.textos_eliminados,
              [](const CuadroTexto& t) -> const std::string& { return t.id; });
  EliminarIds(&pagina->medios, evento.medios_eliminados,
              [](const Medio& m) -> const std::string& { return m.id; });

  if (!AplicarReemplazos(&pagina->trazos, evento.delta.trazos,
                         evento.indices_trazos,
                         [](const Trazo& t) -> const std::string& {
                           return t.id_colaboracion;
                         }) ||
      !AplicarReemplazos(&pagina->textos, evento.delta.textos,
                         evento.indices_textos,
                         [](const CuadroTexto& t) -> const std::string& {
                           return t.id;
                         }) ||
      !AplicarReemplazos(&pagina->medios, evento.delta.medios,
                         evento.indices_medios,
                         [](const Medio& m) -> const std::string& {
                           return m.id;
                         })) return false;

  if (!AplicarOrden(&pagina->trazos, evento.orden_trazos,
                    [](const Trazo& t) -> const std::string& {
                      return t.id_colaboracion;
                    }) ||
      !AplicarOrden(&pagina->textos, evento.orden_textos,
                    [](const CuadroTexto& t) -> const std::string& {
                      return t.id;
                    }) ||
      !AplicarOrden(&pagina->medios, evento.orden_medios,
                    [](const Medio& m) -> const std::string& {
                      return m.id;
                    })) return false;

  if (evento.cambia_metadatos) {
    pagina->titulo = evento.delta.titulo;
    pagina->papel = evento.delta.papel;
    pagina->limites = evento.delta.limites;
    pagina->orientacion = evento.delta.orientacion;
    pagina->ancho = evento.delta.ancho;
    pagina->alto = evento.delta.alto;
    pagina->pagina_pdf = evento.delta.pagina_pdf;
  }
  if (evento.cambia_notas) pagina->notas = evento.delta.notas;
  return true;
}

void EscribirTextos(Escritor* e, const std::vector<std::string>& textos) {
  if (!e || textos.size() > kMaximoElementosEvento) return;
  e->U32(static_cast<uint32_t>(textos.size()));
  for (const auto& texto : textos) e->Texto(texto);
}

bool LeerTextos(Lector* l, std::vector<std::string>* textos) {
  if (!l || !textos) return false;
  const uint32_t n = l->U32();
  if (!l->bien() || n > kMaximoElementosEvento) return false;
  textos->reserve(n);
  std::unordered_set<std::string> unicos;
  unicos.reserve(n);
  for (uint32_t i = 0; i < n; ++i) {
    std::string valor = l->Texto();
    if (!l->bien() || valor.empty() || !unicos.insert(valor).second) return false;
    textos->push_back(std::move(valor));
  }
  return true;
}

void EscribirIndices(Escritor* e, const std::vector<uint32_t>& indices) {
  if (!e || indices.size() > kMaximoElementosEvento) return;
  e->U32(static_cast<uint32_t>(indices.size()));
  for (const uint32_t indice : indices) e->U32(indice);
}

bool LeerIndices(Lector* l, std::vector<uint32_t>* indices) {
  if (!l || !indices) return false;
  const uint32_t n = l->U32();
  if (!l->bien() || n > kMaximoElementosEvento) return false;
  indices->reserve(n);
  for (uint32_t i = 0; i < n; ++i) indices->push_back(l->U32());
  return l->bien();
}

template <typename T, typename ObtenerId>
bool IdsValidos(const std::vector<T>& elementos, ObtenerId obtener_id) {
  std::unordered_set<std::string> vistos;
  vistos.reserve(elementos.size());
  for (const auto& item : elementos) {
    const auto& id = obtener_id(item);
    if (id.empty() || !vistos.insert(id).second) return false;
  }
  return true;
}

bool PaginaConIdsValidos(const Pagina& pagina) {
  return IdsValidos(pagina.trazos,
                    [](const Trazo& t) -> const std::string& {
                      return t.id_colaboracion;
                    }) &&
         IdsValidos(pagina.textos,
                    [](const CuadroTexto& t) -> const std::string& {
                      return t.id;
                    }) &&
         IdsValidos(pagina.medios,
                    [](const Medio& m) -> const std::string& { return m.id; });
}

size_t ActualizarEnTrazo(Trazo* trazo, uint64_t id_muestra,
                         float delta_x, float delta_y, float presion,
                         float altitud, float azimut, bool posicion,
                         bool fuerza, bool inclinacion, bool direccion) {
  if (!trazo || id_muestra == 0) return 0;
  size_t cambios = 0;
  for (auto& muestra : trazo->muestras) {
    if (muestra.id_actualizacion != id_muestra) continue;
    if (posicion) {
      muestra.posicion.x += delta_x;
      muestra.posicion.y += delta_y;
    }
    if (fuerza) muestra.presion = presion;
    if (inclinacion) muestra.inclinacion_x = altitud;
    if (direccion) muestra.inclinacion_y = azimut;
    ++cambios;
  }
  return cambios;
}

}  // namespace

bool CronologiaDocumento::Comenzar(Pagina* pagina,
                                    const std::string& documento_id,
                                    int64_t inicio_qpc) {
  Cancelar();
  if (!pagina || documento_id.empty() || inicio_qpc <= 0) {
    EstablecerErrorArchivo("no se puede iniciar una cronologia sin pagina, identidad y reloj");
    return false;
  }
  try {
    documento_id_ = documento_id;
    inicio_qpc_ = inicio_qpc;
    siguiente_id_ = 1;
    if (!AsegurarIdentidades(pagina)) return false;
    base_ = *pagina;
    estado_reproduccion_ = base_;
    ultimo_tiempo_mostrado_ = inicio_qpc_;
    activa_ = true;
    cargada_ = false;
    return true;
  } catch (const std::bad_alloc&) {
    Cancelar();
    EstablecerErrorArchivo("no hay memoria suficiente para iniciar la cronologia");
    return false;
  }
}

void CronologiaDocumento::Cancelar() {
  base_ = Pagina{};
  eventos_.clear();
  documento_id_.clear();
  siguiente_id_ = 1;
  inicio_qpc_ = 0;
  activa_ = false;
  cargada_ = false;
  estado_reproduccion_ = Pagina{};
  indice_reproduccion_ = 0;
  ultimo_tiempo_mostrado_ = 0;
}

bool CronologiaDocumento::AsegurarIdentidades(Pagina* pagina) {
  if (!pagina) return false;
  std::unordered_set<std::string> usados;
  usados.reserve(pagina->trazos.size() + pagina->textos.size() +
                 pagina->medios.size());
  auto nueva = [&](const char* tipo) {
    std::string id;
    do {
      id = "nv:" + documento_id_ + ":" + tipo + ":" +
           std::to_string(siguiente_id_++);
    } while (usados.contains(id));
    usados.insert(id);
    return id;
  };
  for (auto& trazo : pagina->trazos) {
    if (trazo.id_colaboracion.empty() ||
        !usados.insert(trazo.id_colaboracion).second) {
      trazo.id_colaboracion = nueva("t");
    }
  }
  for (auto& texto : pagina->textos) {
    if (texto.id.empty() || !usados.insert(texto.id).second) {
      texto.id = nueva("x");
    }
  }
  for (auto& medio : pagina->medios) {
    if (medio.id.empty() || !usados.insert(medio.id).second) {
      medio.id = nueva("m");
    }
  }
  return true;
}

void CronologiaDocumento::RegistrarOperacion(const Operacion& operacion,
                                              Pagina* pagina_despues,
                                              int64_t tiempo_qpc,
                                              bool invertida) {
  if (!activa_ || !pagina_despues) return;
  if (!AsegurarIdentidades(pagina_despues)) return;
  EventoCronologia evento;
  evento.tiempo_qpc = std::max(tiempo_qpc, inicio_qpc_);

  switch (operacion.tipo) {
    case TipoOperacion::AnadirTrazos:
      DiferenciarTrazos(invertida ? operacion.despues : std::vector<Trazo>{},
                        invertida ? std::vector<Trazo>{} : operacion.despues,
                        &evento);
      break;
    case TipoOperacion::QuitarTrazos:
      DiferenciarTrazos(invertida ? std::vector<Trazo>{} : operacion.antes,
                        invertida ? operacion.antes : std::vector<Trazo>{},
                        &evento);
      break;
    case TipoOperacion::ReemplazarTrazos:
      DiferenciarTrazos(invertida ? operacion.despues : operacion.antes,
                        invertida ? operacion.antes : operacion.despues,
                        &evento);
      break;
    case TipoOperacion::ReemplazarPaginaTrazos:
      DiferenciarTrazos(invertida ? operacion.despues : operacion.antes,
                        invertida ? operacion.antes : operacion.despues,
                        &evento);
      break;
    case TipoOperacion::ReemplazarPaginaCompleta:
      DiferenciarTrazos(invertida ? operacion.despues : operacion.antes,
                        invertida ? operacion.antes : operacion.despues,
                        &evento);
      DiferenciarTextos(
          invertida ? operacion.textos_despues : operacion.textos_antes,
          invertida ? operacion.textos_antes : operacion.textos_despues,
          &evento);
      DiferenciarMedios(
          invertida ? operacion.medios_despues : operacion.medios_antes,
          invertida ? operacion.medios_antes : operacion.medios_despues,
          &evento);
      break;
    case TipoOperacion::AnadirTextos:
      DiferenciarTextos(
          invertida ? operacion.textos_despues : std::vector<CuadroTexto>{},
          invertida ? std::vector<CuadroTexto>{} : operacion.textos_despues,
          &evento);
      break;
    case TipoOperacion::QuitarTextos:
      DiferenciarTextos(
          invertida ? std::vector<CuadroTexto>{} : operacion.textos_antes,
          invertida ? operacion.textos_antes : std::vector<CuadroTexto>{},
          &evento);
      break;
    case TipoOperacion::ReemplazarTextos:
      DiferenciarTextos(
          invertida ? operacion.textos_despues : operacion.textos_antes,
          invertida ? operacion.textos_antes : operacion.textos_despues,
          &evento);
      break;
    case TipoOperacion::AnadirMedios:
      DiferenciarMedios(invertida ? operacion.medios_despues
                                  : std::vector<Medio>{},
                        invertida ? std::vector<Medio>{}
                                  : operacion.medios_despues,
                        &evento);
      break;
    case TipoOperacion::QuitarMedios:
      DiferenciarMedios(invertida ? std::vector<Medio>{}
                                  : operacion.medios_antes,
                        invertida ? operacion.medios_antes
                                  : std::vector<Medio>{},
                        &evento);
      break;
    case TipoOperacion::ReemplazarMedios:
      DiferenciarMedios(
          invertida ? operacion.medios_despues : operacion.medios_antes,
          invertida ? operacion.medios_antes : operacion.medios_despues,
          &evento);
      break;
  }

  if (!AjustarIndices(
          evento.delta.trazos, pagina_despues->trazos,
          &evento.indices_trazos,
          [](const Trazo& t) -> const std::string& {
            return t.id_colaboracion;
          }) ||
      !AjustarIndices(
          evento.delta.textos, pagina_despues->textos,
          &evento.indices_textos,
          [](const CuadroTexto& t) -> const std::string& { return t.id; }) ||
      !AjustarIndices(
          evento.delta.medios, pagina_despues->medios,
          &evento.indices_medios,
          [](const Medio& m) -> const std::string& { return m.id; })) {
    activa_ = false;
    EstablecerErrorArchivo(
        "la operacion no coincide con el estado actual de la cronologia");
    return;
  }
  RegistrarEvento(std::move(evento));
}

void CronologiaDocumento::RegistrarCambio(const Pagina& antes,
                                           Pagina* despues,
                                           int64_t tiempo_qpc) {
  if (!activa_ || !despues) return;
  if (!AsegurarIdentidades(despues)) return;
  EventoCronologia evento;
  evento.tiempo_qpc = std::max(tiempo_qpc, inicio_qpc_);
  DiferenciarTrazos(antes.trazos, despues->trazos, &evento);
  DiferenciarTextos(antes.textos, despues->textos, &evento);
  DiferenciarMedios(antes.medios, despues->medios, &evento);
  evento.cambia_metadatos = !MetadatosIguales(antes, *despues);
  evento.cambia_notas = !IgualNotas(antes.notas, despues->notas);
  if (evento.cambia_metadatos) {
    evento.delta.titulo = despues->titulo;
    evento.delta.papel = despues->papel;
    evento.delta.limites = despues->limites;
    evento.delta.orientacion = despues->orientacion;
    evento.delta.ancho = despues->ancho;
    evento.delta.alto = despues->alto;
    evento.delta.pagina_pdf = despues->pagina_pdf;
  }
  if (evento.cambia_notas) evento.delta.notas = despues->notas;
  RegistrarEvento(std::move(evento));
}

void CronologiaDocumento::RegistrarMetadatos(const Pagina& antes,
                                             const Pagina& despues,
                                             int64_t tiempo_qpc) {
  if (!activa_ || MetadatosIguales(antes, despues)) return;
  EventoCronologia evento;
  evento.tiempo_qpc = std::max(tiempo_qpc, inicio_qpc_);
  evento.cambia_metadatos = true;
  evento.delta.titulo = despues.titulo;
  evento.delta.papel = despues.papel;
  evento.delta.limites = despues.limites;
  evento.delta.orientacion = despues.orientacion;
  evento.delta.ancho = despues.ancho;
  evento.delta.alto = despues.alto;
  evento.delta.pagina_pdf = despues.pagina_pdf;
  RegistrarEvento(std::move(evento));
}

void CronologiaDocumento::RegistrarEvento(EventoCronologia evento) {
  if (EventoVacio(evento)) return;
  try {
    eventos_.push_back(std::move(evento));
  } catch (const std::bad_alloc&) {
    activa_ = false;
    EstablecerErrorArchivo("la cronologia excedio la memoria disponible");
  }
}

size_t CronologiaDocumento::ActualizarMuestraEstimada(
    uint64_t id_muestra, float delta_x, float delta_y, float presion,
    float altitud, float azimut, bool posicion, bool fuerza,
    bool inclinacion, bool direccion) {
  if (!activa_ || id_muestra == 0) return 0;
  size_t cambios = 0;
  for (auto& evento : eventos_) {
    for (auto& trazo : evento.delta.trazos) {
      cambios += ActualizarEnTrazo(&trazo, id_muestra, delta_x, delta_y,
                                   presion, altitud, azimut, posicion, fuerza,
                                   inclinacion, direccion);
    }
  }
  return cambios;
}

std::vector<uint8_t> CronologiaDocumento::Serializar() const {
  if ((!activa_ && !cargada_) || !PaginaConIdsValidos(base_) ||
      eventos_.size() > kMaximoEventos) {
    EstablecerErrorArchivo("la cronologia no esta lista para serializarse");
    return {};
  }
  try {
    Escritor e;
    for (const char c : kMagia) e.U8(static_cast<uint8_t>(c));
    e.U16(kVersion);
    e.I64(inicio_qpc_);
    e.Texto(documento_id_);
    e.Bytes(tinta::Serializar(DocumentoDePagina(base_)));
    e.U32(static_cast<uint32_t>(eventos_.size()));
    for (const auto& evento : eventos_) {
      e.I64(evento.tiempo_qpc);
      e.Bool(evento.cambia_metadatos);
      e.Bool(evento.cambia_notas);
      e.Bytes(tinta::Serializar(DocumentoDePagina(evento.delta)));
      EscribirIndices(&e, evento.indices_trazos);
      EscribirIndices(&e, evento.indices_textos);
      EscribirIndices(&e, evento.indices_medios);
      EscribirTextos(&e, evento.trazos_eliminados);
      EscribirTextos(&e, evento.textos_eliminados);
      EscribirTextos(&e, evento.medios_eliminados);
      EscribirTextos(&e, evento.orden_trazos);
      EscribirTextos(&e, evento.orden_textos);
      EscribirTextos(&e, evento.orden_medios);
      if (!e.bien()) break;
    }
    if (!e.bien() || e.datos().empty() ||
        e.datos().size() > kMaximoBytesArchivo) {
      EstablecerErrorArchivo("la cronologia excede el presupuesto seguro de archivo");
      return {};
    }
    return e.datos();
  } catch (const std::bad_alloc&) {
    EstablecerErrorArchivo("no hay memoria suficiente para serializar la cronologia");
    return {};
  }
}

bool CronologiaDocumento::Cargar(const std::vector<uint8_t>& datos,
                                 Pagina* pagina_inicial) {
  if (!pagina_inicial || datos.empty() || datos.size() > kMaximoBytesArchivo) {
    EstablecerErrorArchivo("cronologia vacia o demasiado grande");
    return false;
  }
  try {
    Lector l(datos);
    for (const char esperado : kMagia) {
      if (l.U8() != static_cast<uint8_t>(esperado)) {
        EstablecerErrorArchivo("cabecera de cronologia desconocida");
        return false;
      }
    }
    const uint16_t version = l.U16();
    if (!l.bien() || version != kVersion) {
      EstablecerErrorArchivo("version de cronologia desconocida");
      return false;
    }
    const int64_t inicio = l.I64();
    std::string documento_id = l.Texto();
    Pagina base;
    if (inicio <= 0 || documento_id.empty() ||
        !ExtraerPagina(l.Bytes(), &base) || !PaginaConIdsValidos(base)) {
      EstablecerErrorArchivo("estado base de cronologia invalido");
      return false;
    }
    const uint32_t cantidad = l.U32();
    if (!l.bien() || cantidad > kMaximoEventos) {
      EstablecerErrorArchivo("cantidad de eventos de cronologia invalida");
      return false;
    }
    std::vector<EventoCronologia> eventos;
    eventos.reserve(cantidad);
    int64_t tiempo_anterior = inicio;
    for (uint32_t i = 0; i < cantidad; ++i) {
      EventoCronologia evento;
      evento.tiempo_qpc = l.I64();
      evento.cambia_metadatos = l.Bool();
      evento.cambia_notas = l.Bool();
      if (evento.tiempo_qpc < inicio || evento.tiempo_qpc < tiempo_anterior ||
          !ExtraerPagina(l.Bytes(), &evento.delta) ||
          !LeerIndices(&l, &evento.indices_trazos) ||
          !LeerIndices(&l, &evento.indices_textos) ||
          !LeerIndices(&l, &evento.indices_medios) ||
          !LeerTextos(&l, &evento.trazos_eliminados) ||
          !LeerTextos(&l, &evento.textos_eliminados) ||
          !LeerTextos(&l, &evento.medios_eliminados) ||
          !LeerTextos(&l, &evento.orden_trazos) ||
          !LeerTextos(&l, &evento.orden_textos) ||
          !LeerTextos(&l, &evento.orden_medios) ||
          evento.indices_trazos.size() != evento.delta.trazos.size() ||
          evento.indices_textos.size() != evento.delta.textos.size() ||
          evento.indices_medios.size() != evento.delta.medios.size() ||
          !PaginaConIdsValidos(evento.delta)) {
        EstablecerErrorArchivo("evento de cronologia invalido o fuera de orden");
        return false;
      }
      tiempo_anterior = evento.tiempo_qpc;
      eventos.push_back(std::move(evento));
    }
    if (!l.al_final()) {
      EstablecerErrorArchivo("la cronologia contiene bytes sobrantes");
      return false;
    }

    // Se valida la secuencia completa antes de sustituir la cronología abierta.
    // Así un orden imposible o un ID ausente no falla a mitad de reproducción.
    Pagina comprobacion = base;
    for (const auto& evento : eventos) {
      if (!AplicarEvento(evento, &comprobacion) ||
          !PaginaConIdsValidos(comprobacion)) {
        EstablecerErrorArchivo("la secuencia de cronologia no es aplicable");
        return false;
      }
    }

    Cancelar();
    base_ = std::move(base);
    eventos_ = std::move(eventos);
    documento_id_ = std::move(documento_id);
    inicio_qpc_ = inicio;
    activa_ = false;
    cargada_ = true;
    ReiniciarReproduccion();
    *pagina_inicial = estado_reproduccion_;
    return true;
  } catch (const std::bad_alloc&) {
    EstablecerErrorArchivo("no hay memoria suficiente para cargar la cronologia");
    return false;
  }
}

void CronologiaDocumento::ReiniciarReproduccion() {
  estado_reproduccion_ = base_;
  indice_reproduccion_ = 0;
  ultimo_tiempo_mostrado_ = inicio_qpc_;
}

bool CronologiaDocumento::MostrarHasta(int64_t tiempo_qpc, Pagina* pagina) {
  if (!cargada_ || !pagina) return false;
  const int64_t destino = std::max(tiempo_qpc, inicio_qpc_);
  bool cambio = false;
  if (destino < ultimo_tiempo_mostrado_) {
    ReiniciarReproduccion();
    cambio = true;
  }
  while (indice_reproduccion_ < eventos_.size() &&
         eventos_[indice_reproduccion_].tiempo_qpc <= destino) {
    if (!AplicarEvento(eventos_[indice_reproduccion_],
                       &estado_reproduccion_)) {
      EstablecerErrorArchivo("la cronologia fallo durante la reproduccion");
      return false;
    }
    ++indice_reproduccion_;
    cambio = true;
  }
  ultimo_tiempo_mostrado_ = destino;
  if (cambio) *pagina = estado_reproduccion_;
  return cambio;
}

}  // namespace tinta
