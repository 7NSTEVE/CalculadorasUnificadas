#include "api.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include <unordered_map>

#include "ajustes.h"
#include "archivo.h"
#include "documento.h"
#include "circuitos.h"
#include "cronologia.h"
#include "edicion.h"
#include "formas.h"
#include "grabacion.h"
#include "importar.h"
#include "ingenieria.h"
#include "lienzo_avanzado.h"
#include "matematicas_avanzadas.h"
#include "reproduccion.h"
#include "tinta.h"
#include "almacen.h"

namespace {
constexpr int64_t kFrecuenciaNs = 1'000'000'000;

bool CopiarEntradaSegura(const uint8_t* datos, size_t cantidad,
                         std::vector<uint8_t>* salida) {
  if (!datos || !salida || cantidad == 0 ||
      cantidad > tinta::kMaximoBytesArchivo) {
    tinta::EstablecerErrorArchivo(
        "la entrada supera el presupuesto seguro de lectura");
    return false;
  }
  try {
    salida->assign(datos, datos + cantidad);
    return true;
  } catch (const std::bad_alloc&) {
    tinta::EstablecerErrorArchivo(
        "no hay memoria suficiente para copiar la entrada");
    return false;
  }
}
}

// La sesión es una página con su historial. Los buffers de salida se reutilizan
// entre llamadas para no asignar memoria en el camino crítico del dibujo.
struct TintaSesion {
  tinta::Pagina pagina;
  tinta::Historial historial;
  tinta::CronologiaDocumento cronologia;
  int64_t tiempo_cronologia_qpc = 0;

  tinta::Trazo en_curso;
  bool hay_trazo = false;
  std::vector<tinta::Muestra> provisionales;
  std::unordered_map<uint64_t, tinta::Muestra> muestras_actualizables;
  uint64_t siguiente_id_muestra = 1;

  tinta::ParametrosPluma params;
  tinta::ParametrosPrediccion params_prediccion;
  bool prediccion_activa = true;
  size_t muestras_predichas = 0;

  std::vector<tinta::Vertice> vertices_en_curso;
  std::vector<tinta::Vertice> vertices_trazo;
  std::vector<float> puntos_trazo;
  std::vector<float> pauta;
  std::vector<float> pauta_avanzada;
  std::vector<float> cortes_impresion;
  std::vector<size_t> indices_salida;
  std::vector<size_t> indices_textos_salida;
  std::vector<size_t> indices_medios_salida;
  std::vector<uint8_t> bytes;
  std::string texto_salida;
  uint64_t contador_copias = 0;

  std::string documento_id;
  std::string documento_titulo;
};

namespace {

int64_t SegundosAQpc(double tiempo_s) {
  if (!std::isfinite(tiempo_s) || tiempo_s <= 0.0 ||
      tiempo_s > static_cast<double>(std::numeric_limits<int64_t>::max()) /
                     static_cast<double>(kFrecuenciaNs)) return 0;
  return static_cast<int64_t>(tiempo_s * static_cast<double>(kFrecuenciaNs));
}

void NormalizarDespues(tinta::Operacion* operacion,
                       const tinta::Pagina& pagina) {
  if (!operacion) return;
  const auto copiar_trazos = [&]() {
    operacion->despues.clear();
    for (const size_t indice : operacion->indices) {
      if (indice < pagina.trazos.size())
        operacion->despues.push_back(pagina.trazos[indice]);
    }
  };
  const auto copiar_textos = [&]() {
    operacion->textos_despues.clear();
    for (const size_t indice : operacion->indices_textos) {
      if (indice < pagina.textos.size())
        operacion->textos_despues.push_back(pagina.textos[indice]);
    }
  };
  const auto copiar_medios = [&]() {
    operacion->medios_despues.clear();
    for (const size_t indice : operacion->indices_medios) {
      if (indice < pagina.medios.size())
        operacion->medios_despues.push_back(pagina.medios[indice]);
    }
  };
  switch (operacion->tipo) {
    case tinta::TipoOperacion::AnadirTrazos:
    case tinta::TipoOperacion::ReemplazarTrazos:
      copiar_trazos();
      break;
    case tinta::TipoOperacion::ReemplazarPaginaTrazos:
      operacion->despues = pagina.trazos;
      break;
    case tinta::TipoOperacion::ReemplazarPaginaCompleta:
      operacion->despues = pagina.trazos;
      operacion->textos_despues = pagina.textos;
      operacion->medios_despues = pagina.medios;
      break;
    case tinta::TipoOperacion::AnadirTextos:
    case tinta::TipoOperacion::ReemplazarTextos:
      copiar_textos();
      break;
    case tinta::TipoOperacion::AnadirMedios:
    case tinta::TipoOperacion::ReemplazarMedios:
      copiar_medios();
      break;
    case tinta::TipoOperacion::QuitarTrazos:
    case tinta::TipoOperacion::QuitarTextos:
    case tinta::TipoOperacion::QuitarMedios:
      break;
  }
}

void RegistrarOperacion(TintaSesion* sesion, tinta::Operacion operacion,
                        int64_t tiempo_qpc = 0) {
  if (!sesion) return;
  if (sesion->cronologia.activa()) {
    if (!sesion->cronologia.PrepararPagina(&sesion->pagina)) return;
    NormalizarDespues(&operacion, sesion->pagina);
  }
  sesion->historial.Registrar(operacion);
  sesion->cronologia.RegistrarOperacion(
      operacion, &sesion->pagina,
      tiempo_qpc > 0 ? tiempo_qpc : sesion->tiempo_cronologia_qpc);
}

tinta::Pagina CopiarMetadatos(const tinta::Pagina& origen) {
  tinta::Pagina copia;
  copia.titulo = origen.titulo;
  copia.papel = origen.papel;
  copia.limites = origen.limites;
  copia.orientacion = origen.orientacion;
  copia.ancho = origen.ancho;
  copia.alto = origen.alto;
  copia.pagina_pdf = origen.pagina_pdf;
  return copia;
}

void RegistrarMetadatos(TintaSesion* sesion,
                        const tinta::Pagina& metadatos_antes) {
  if (!sesion) return;
  sesion->cronologia.RegistrarMetadatos(
      metadatos_antes, sesion->pagina, sesion->tiempo_cronologia_qpc);
}

std::unique_ptr<tinta::Pagina> CopiarPaginaParaCronologia(
    const TintaSesion* sesion) {
  if (!sesion || !sesion->cronologia.activa()) return nullptr;
  try {
    return std::make_unique<tinta::Pagina>(sesion->pagina);
  } catch (const std::bad_alloc&) {
    tinta::EstablecerErrorArchivo(
        "no hay memoria para registrar el cambio en la cronologia");
    return nullptr;
  }
}

void RegistrarCambioDirecto(TintaSesion* sesion,
                            const std::unique_ptr<tinta::Pagina>& antes) {
  if (!sesion || !antes) return;
  sesion->cronologia.RegistrarCambio(
      *antes, &sesion->pagina, sesion->tiempo_cronologia_qpc);
}

tinta::Muestra HacerMuestra(float x, float y, float presion, float altitud,
                            float azimut, double tiempo_s,
                            uint64_t id_actualizacion = 0) {
  tinta::Muestra m;
  m.posicion = {x, y};
  m.presion = (presion < 0.0f) ? tinta::kSinPresion : std::clamp(presion, 0.0f, 1.0f);
  m.inclinacion_x = altitud;
  m.inclinacion_y = azimut;
  m.tiempo_qpc = static_cast<int64_t>(tiempo_s * kFrecuenciaNs);
  m.id_actualizacion = id_actualizacion;
  return m;
}

void OlvidarActualizacionesDelTrazo(TintaSesion* sesion,
                                    const tinta::Trazo& trazo) {
  if (!sesion) return;
  for (const auto& muestra : trazo.muestras) {
    if (muestra.id_actualizacion != 0)
      sesion->muestras_actualizables.erase(muestra.id_actualizacion);
  }
}

bool AplicarActualizacionEstimada(tinta::Muestra& muestra,
                                  uint64_t id_muestra,
                                  float delta_x, float delta_y,
                                  float presion, float altitud, float azimut,
                                  bool posicion, bool fuerza,
                                  bool inclinacion, bool direccion) {
  if (muestra.id_actualizacion != id_muestra) return false;
  if (posicion) {
    muestra.posicion.x += delta_x;
    muestra.posicion.y += delta_y;
  }
  if (fuerza) muestra.presion = presion;
  if (inclinacion) muestra.inclinacion_x = altitud;
  if (direccion) muestra.inclinacion_y = azimut;
  return true;
}

size_t ActualizarEnTrazos(std::vector<tinta::Trazo>& trazos,
                          uint64_t id_muestra,
                          float delta_x, float delta_y,
                          float presion, float altitud, float azimut,
                          bool posicion, bool fuerza,
                          bool inclinacion, bool direccion) {
  size_t actualizadas = 0;
  for (auto& trazo : trazos) {
    for (auto& muestra : trazo.muestras) {
      if (AplicarActualizacionEstimada(
              muestra, id_muestra, delta_x, delta_y, presion, altitud, azimut,
              posicion, fuerza, inclinacion, direccion)) {
        ++actualizadas;
      }
    }
  }
  return actualizadas;
}

// --- Traducción entre los enteros del API en C y los enum del núcleo --------
//
// El API es C porque Swift importa C sin puente. Los enum fuertes se quedan
// dentro: fuera viajan enteros documentados en `api.h`.

template <typename E>
E DesdeEntero(int valor, int maximo, E por_defecto) {
  if (valor < 0 || valor > maximo) return por_defecto;
  return static_cast<E>(valor);
}

tinta::AjustesGrabacion AFuera(const TintaAjustesGrabacion& c) {
  tinta::AjustesGrabacion a;
  a.modo = DesdeEntero(c.modo, 2, tinta::ModoCaptura::NotaViva);
  a.video.resolucion = DesdeEntero(c.resolucion, 5, tinta::Resolucion::Nativa);
  a.video.ancho_personalizado = c.ancho_personalizado;
  a.video.alto_personalizado = c.alto_personalizado;
  a.video.fps = c.fps;
  a.video.calidad = DesdeEntero(c.calidad, 3, tinta::CalidadVideo::Alta);
  a.video.kbps = c.kbps_video;
  a.video.codec = DesdeEntero(c.codec, 1, tinta::CodecVideo::HEVC);
  a.video.hdr = c.hdr;

  a.audio.microfono = c.microfono;
  a.audio.audio_de_la_app = c.audio_de_la_app;
  a.audio.canales = DesdeEntero(c.canales, 2, tinta::ModoCanales::Estereo);
  a.audio.kbps = c.kbps_audio;
  a.audio.frecuencia_hz = c.frecuencia_hz;
  a.audio.posicion =
      DesdeEntero(c.posicion_microfono, 4, tinta::PosicionMicrofono::Automatica);
  a.audio.patron = DesdeEntero(c.patron_polar, 4, tinta::PatronPolar::Automatico);

  a.mostrar_puntero = c.mostrar_puntero;
  return a;
}

TintaAjustesGrabacion ADentro(const tinta::AjustesGrabacion& a) {
  TintaAjustesGrabacion c{};
  c.modo = static_cast<int>(a.modo);
  c.resolucion = static_cast<int>(a.video.resolucion);
  c.ancho_personalizado = a.video.ancho_personalizado;
  c.alto_personalizado = a.video.alto_personalizado;
  c.fps = a.video.fps;
  c.calidad = static_cast<int>(a.video.calidad);
  c.kbps_video = a.video.kbps;
  c.codec = static_cast<int>(a.video.codec);
  c.hdr = a.video.hdr;

  c.microfono = a.audio.microfono;
  c.audio_de_la_app = a.audio.audio_de_la_app;
  c.canales = static_cast<int>(a.audio.canales);
  c.kbps_audio = a.audio.kbps;
  c.frecuencia_hz = a.audio.frecuencia_hz;
  c.posicion_microfono = static_cast<int>(a.audio.posicion);
  c.patron_polar = static_cast<int>(a.audio.patron);

  c.mostrar_puntero = a.mostrar_puntero;
  return c;
}

tinta::CapacidadesGrabacion AFuera(const TintaCapacidades& c) {
  tinta::CapacidadesGrabacion cap;
  cap.pantalla = tinta::TamanoVideo{c.ancho_pantalla, c.alto_pantalla};
  cap.fps_maximo = c.fps_maximo;
  cap.estereo = c.estereo;
  cap.espacial = c.espacial;
  cap.hevc = c.hevc;
  cap.hdr = c.hdr;
  cap.canales_entrada_max = c.canales_entrada_max;
  return cap;
}

tinta::AjustePdf AjusteDesdeEntero(int valor) {
  return DesdeEntero(valor, 2, tinta::AjustePdf::TamanoReal);
}

// Copia con terminador, truncando si no cabe. Nunca escribe fuera del buffer:
// el que llama es Swift y el tamaño lo pone él.
void Copiar(const std::string& texto, char* salida, size_t tam) {
  if (!salida || tam == 0) return;
  const size_t n = std::min(texto.size(), tam - 1);
  if (n > 0) std::memcpy(salida, texto.data(), n);
  salida[n] = '\0';
}

std::vector<tinta::ComponenteElectrico> ComponentesDesdeC(
    const TintaComponenteElectrico* componentes, size_t cantidad) {
  std::vector<tinta::ComponenteElectrico> salida;
  if (!componentes) return salida;
  salida.reserve(cantidad);
  for (size_t i = 0; i < cantidad; ++i) {
    const auto& c = componentes[i];
    tinta::ComponenteElectrico x;
    x.id = c.id ? c.id : "";
    x.tipo = DesdeEntero(
        c.tipo,
        static_cast<int>(tinta::TipoComponenteElectrico::Osciloscopio),
        tinta::TipoComponenteElectrico::Desconocido);
    x.nodo_a = std::max(c.nodo_a, 0);
    x.nodo_b = std::max(c.nodo_b, 0);
    x.nodo_control_a = std::max(c.nodo_control_a, 0);
    x.nodo_control_b = std::max(c.nodo_control_b, 0);
    x.valor = c.valor;
    x.auxiliar = c.auxiliar;
    x.fase_grados = c.fase_grados;
    x.activo = c.activo;
    salida.push_back(std::move(x));
  }
  return salida;
}

std::map<std::string, std::complex<double>> VariablesDesdeC(
    const TintaVariableCompleja* variables, size_t cantidad) {
  std::map<std::string, std::complex<double>> salida;
  if (!variables) return salida;
  for (size_t i = 0; i < cantidad; ++i) {
    if (!variables[i].nombre || variables[i].nombre[0] == '\0') continue;
    std::string nombre = variables[i].nombre;
    std::transform(nombre.begin(), nombre.end(), nombre.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    salida[nombre] = {variables[i].real, variables[i].imaginario};
  }
  return salida;
}

std::vector<std::complex<double>> ComplejosDesdeC(const TintaComplejo* valores,
                                                   size_t cantidad) {
  std::vector<std::complex<double>> salida;
  if (!valores) return salida;
  salida.reserve(cantidad);
  for (size_t i = 0; i < cantidad; ++i)
    salida.emplace_back(valores[i].real, valores[i].imaginario);
  return salida;
}

void CopiarComplejos(const std::vector<std::complex<double>>& valores,
                     TintaComplejo* salida, size_t capacidad) {
  const size_t n = std::min(valores.size(), capacidad);
  for (size_t i = 0; salida && i < n; ++i) {
    salida[i].real = valores[i].real();
    salida[i].imaginario = valores[i].imag();
  }
}

bool CopiarResultadoMatematico(const tinta::ResultadoExpresion& r,
                               TintaComplejo* resultado,
                               char* error, size_t tam_error) {
  if (!r.correcto) {
    Copiar(r.error, error, tam_error);
    return false;
  }
  if (resultado) {
    resultado->real = r.valor.real();
    resultado->imaginario = r.valor.imag();
  }
  if (error && tam_error) error[0] = '\0';
  return true;
}

uint32_t ColorEfectivoTrazo(const tinta::Trazo& trazo) {
  const uint32_t alfa_base = trazo.resaltador ? 0x59u :
      ((trazo.color_argb >> 24) & 0xFFu);
  const float opacidad = trazo.estilo_persistente
      ? std::clamp(trazo.estilo.opacidad, 0.0f, 1.0f) : 1.0f;
  const uint32_t alfa = static_cast<uint32_t>(
      std::round(static_cast<float>(alfa_base) * opacidad));
  return (trazo.color_argb & 0x00FFFFFFu) | (std::min(alfa, 255u) << 24);
}

// Los preajustes se piden por índice y su nombre tiene que sobrevivir a la
// llamada, así que se guardan en una copia estática.
const char* TextoPreajuste(int indice, bool nombre) {
  static const std::vector<tinta::PreajusteGrabacion> lista = tinta::Preajustes();
  if (indice < 0 || indice >= static_cast<int>(lista.size())) return "";
  const tinta::PreajusteGrabacion& p = lista[static_cast<size_t>(indice)];
  return nombre ? p.nombre.c_str() : p.para_que.c_str();
}

bool ExportarReconocimientoCircuito(
    const std::vector<tinta::Trazo>& trazos,
    TintaReconocimientoCircuito* resultado) {
  if (!resultado || trazos.empty()) return false;
  const auto reconocido = tinta::ReconocerComponente(trazos);
  int tipo = 17;  // cable en el enum público ampliado
  switch (reconocido.tipo) {
    case tinta::TipoComponenteCircuito::Resistor: tipo = 0; break;
    case tinta::TipoComponenteCircuito::Capacitor: tipo = 1; break;
    case tinta::TipoComponenteCircuito::Inductor: tipo = 2; break;
    case tinta::TipoComponenteCircuito::FuenteVoltaje: tipo = 3; break;
    case tinta::TipoComponenteCircuito::FuenteCorriente: tipo = 4; break;
    case tinta::TipoComponenteCircuito::Tierra: tipo = 16; break;
    case tinta::TipoComponenteCircuito::Cable: tipo = 17; break;
  }
  resultado->tipo = tipo;
  resultado->confianza = reconocido.confianza;
  if (reconocido.terminales.size() >= 2) {
    resultado->terminal_ax = reconocido.terminales[0].x;
    resultado->terminal_ay = reconocido.terminales[0].y;
    resultado->terminal_bx = reconocido.terminales[1].x;
    resultado->terminal_by = reconocido.terminales[1].y;
  }
  return reconocido.confianza >= 0.55f;
}

struct CajaTrazoApi {
  float min_x = 0.0f;
  float min_y = 0.0f;
  float max_x = 0.0f;
  float max_y = 0.0f;
  bool valida = false;
};

CajaTrazoApi CajaDeTrazoApi(const tinta::Trazo& trazo) {
  CajaTrazoApi caja;
  if (trazo.muestras.empty()) return caja;
  caja.min_x = caja.max_x = trazo.muestras.front().posicion.x;
  caja.min_y = caja.max_y = trazo.muestras.front().posicion.y;
  for (const auto& muestra : trazo.muestras) {
    caja.min_x = std::min(caja.min_x, muestra.posicion.x);
    caja.min_y = std::min(caja.min_y, muestra.posicion.y);
    caja.max_x = std::max(caja.max_x, muestra.posicion.x);
    caja.max_y = std::max(caja.max_y, muestra.posicion.y);
  }
  const float margen = std::max(trazo.ancho_base * 0.75f, 2.0f);
  caja.min_x -= margen;
  caja.min_y -= margen;
  caja.max_x += margen;
  caja.max_y += margen;
  caja.valida = true;
  return caja;
}

bool Intersectan(const CajaTrazoApi& a, const CajaTrazoApi& b) {
  return a.valida && b.valida && a.min_x <= b.max_x && a.max_x >= b.min_x &&
      a.min_y <= b.max_y && a.max_y >= b.min_y;
}

size_t IndiceResaltadorInteligente(const tinta::Pagina& pagina,
                                   const tinta::Trazo& nuevo) {
  const CajaTrazoApi caja = CajaDeTrazoApi(nuevo);
  const float ancho = caja.max_x - caja.min_x;
  const float alto = caja.max_y - caja.min_y;
  // Solo reorganiza un gesto con geometría de línea de texto. Un resaltado
  // libre o vertical conserva el orden explícito elegido por el usuario.
  if (!caja.valida || ancho < 24.0f || ancho < alto * 1.5f)
    return pagina.trazos.size();
  for (size_t i = 0; i < pagina.trazos.size(); ++i) {
    if (!pagina.trazos[i].resaltador &&
        Intersectan(caja, CajaDeTrazoApi(pagina.trazos[i]))) return i;
  }
  return pagina.trazos.size();
}

}  // namespace

namespace {
std::unordered_map<TintaDocumentoId, TintaSesion*> g_documentos;
TintaDocumentoId g_siguiente_id = 1;

struct ViewportState {
  TintaDocumentoId documento = 0;
  float zoom = 1.0f;
  float pan_x = 0.0f;
  float pan_y = 0.0f;
};
ViewportState g_viewports[2];
}  // namespace

extern "C" {

TintaSesion* tinta_sesion_crear(void) { return new TintaSesion(); }

void tinta_sesion_liberar(TintaSesion* sesion) { delete sesion; }

// --- Trazo -----------------------------------------------------------------

void tinta_comenzar_trazo(TintaSesion* sesion, float ancho, uint32_t color_argb,
                          bool resaltador) {
  if (!sesion) return;
  // Un segundo comienzo implica que el consumidor abandonó el trazo anterior.
  // Sus identificadores estimados no deben quedar vivos indefinidamente.
  OlvidarActualizacionesDelTrazo(sesion, sesion->en_curso);
  sesion->en_curso = tinta::Trazo{};
  sesion->en_curso.ancho_base = ancho;
  sesion->en_curso.color_argb = color_argb;
  sesion->en_curso.resaltador = resaltador;
  sesion->en_curso.estilo_persistente = true;
  sesion->en_curso.estilo.opacidad = sesion->params.opacidad;
  sesion->en_curso.estilo.exponente_presion = sesion->params.exponente_presion;
  sesion->en_curso.estilo.factor_ancho_minimo = sesion->params.factor_ancho_minimo;
  sesion->en_curso.estilo.velocidad_ancho_minimo = sesion->params.velocidad_ancho_minimo;
  sesion->en_curso.estilo.suavizado_ancho = sesion->params.suavizado_ancho;
  sesion->en_curso.estilo.estabilizacion = sesion->params.estabilizacion;
  sesion->en_curso.estilo.taper_inicio = sesion->params.taper_inicio;
  sesion->en_curso.estilo.taper_fin = sesion->params.taper_fin;
  sesion->en_curso.estilo.sensibilidad_inclinacion =
      sesion->params.sensibilidad_inclinacion;
  sesion->en_curso.estilo.textura = sesion->params.textura;
  sesion->en_curso.estilo.nivel_refinado = sesion->params.nivel_refinado;
  sesion->hay_trazo = true;
  sesion->muestras_predichas = 0;
}

void tinta_pluma_configurar(TintaSesion* sesion, TintaAjustesPluma a) {
  if (!sesion) return;
  sesion->params.opacidad = std::clamp(a.opacidad, 0.0f, 1.0f);
  sesion->params.exponente_presion = std::clamp(a.exponente_presion, 0.1f, 4.0f);
  sesion->params.factor_ancho_minimo = std::clamp(a.factor_ancho_minimo, 0.0f, 1.0f);
  sesion->params.velocidad_ancho_minimo = std::clamp(a.velocidad_ancho_minimo, 0.1f, 20.0f);
  sesion->params.suavizado_ancho = std::clamp(a.suavizado_ancho, 0, 24);
  sesion->params.estabilizacion = std::clamp(a.estabilizacion, 0.0f, 1.0f);
  sesion->params.taper_inicio = std::clamp(a.taper_inicio, 0.0f, 0.5f);
  sesion->params.taper_fin = std::clamp(a.taper_fin, 0.0f, 0.5f);
  sesion->params.sensibilidad_inclinacion =
      std::clamp(a.sensibilidad_inclinacion, 0.0f, 1.0f);
  sesion->params.textura = std::clamp(a.textura, 0, 3);
  sesion->params.nivel_refinado = std::clamp(a.nivel_refinado, 0, 3);
}

TintaAjustesPluma tinta_pluma_ajustes(const TintaSesion* sesion) {
  TintaAjustesPluma a{};
  if (!sesion) return a;
  a.opacidad = sesion->params.opacidad;
  a.exponente_presion = sesion->params.exponente_presion;
  a.factor_ancho_minimo = sesion->params.factor_ancho_minimo;
  a.velocidad_ancho_minimo = sesion->params.velocidad_ancho_minimo;
  a.suavizado_ancho = sesion->params.suavizado_ancho;
  a.estabilizacion = sesion->params.estabilizacion;
  a.taper_inicio = sesion->params.taper_inicio;
  a.taper_fin = sesion->params.taper_fin;
  a.sensibilidad_inclinacion = sesion->params.sensibilidad_inclinacion;
  a.textura = sesion->params.textura;
  a.nivel_refinado = sesion->params.nivel_refinado;
  return a;
}

bool tinta_trazo_refinar(TintaSesion* sesion, size_t indice, int nivel) {
  if (!sesion || indice >= sesion->pagina.trazos.size() || nivel < 0 || nivel > 3)
    return false;
  auto& trazo = sesion->pagina.trazos[indice];
  if (trazo.estilo_persistente && trazo.estilo.nivel_refinado == nivel)
    return true;
  const tinta::Trazo antes = trazo;
  if (!trazo.estilo_persistente) {
    trazo.estilo_persistente = true;
    trazo.estilo.exponente_presion = sesion->params.exponente_presion;
    trazo.estilo.factor_ancho_minimo = sesion->params.factor_ancho_minimo;
    trazo.estilo.velocidad_ancho_minimo = sesion->params.velocidad_ancho_minimo;
    trazo.estilo.suavizado_ancho = sesion->params.suavizado_ancho;
  }
  trazo.estilo.nivel_refinado = nivel;
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionReemplazar(
                  {indice}, {antes}, {trazo}));
  return true;
}

bool tinta_refinar_todos(TintaSesion* sesion, int nivel) {
  if (!sesion || nivel < 0 || nivel > 3) return false;
  for (size_t i = 0; i < sesion->pagina.trazos.size(); ++i)
    tinta_trazo_refinar(sesion, i, nivel);
  return true;
}

void tinta_anadir_muestra(TintaSesion* sesion, float x, float y, float presion,
                          float altitud, float azimut, double tiempo_s) {
  if (!sesion || !sesion->hay_trazo) return;
  const int64_t tiempo_qpc = SegundosAQpc(tiempo_s);
  if (tiempo_qpc > 0) sesion->tiempo_cronologia_qpc = tiempo_qpc;
  sesion->en_curso.muestras.push_back(
      HacerMuestra(x, y, presion, altitud, azimut, tiempo_s));
}

uint64_t tinta_anadir_muestra_actualizable(
    TintaSesion* sesion, float x, float y, float presion, float altitud,
    float azimut, double tiempo_s) {
  if (!sesion || !sesion->hay_trazo) return 0;
  const int64_t tiempo_qpc = SegundosAQpc(tiempo_s);
  if (tiempo_qpc > 0) sesion->tiempo_cronologia_qpc = tiempo_qpc;
  uint64_t id = sesion->siguiente_id_muestra++;
  if (id == 0) id = sesion->siguiente_id_muestra++;
  const tinta::Muestra muestra =
      HacerMuestra(x, y, presion, altitud, azimut, tiempo_s, id);
  sesion->en_curso.muestras.push_back(muestra);
  sesion->muestras_actualizables[id] = muestra;
  return id;
}

bool tinta_actualizar_muestra(TintaSesion* sesion, uint64_t id_muestra,
                              uint32_t propiedades, float x, float y,
                              float presion, float altitud, float azimut) {
  constexpr uint32_t kPropiedadesConocidas =
      TintaPropiedadMuestraPosicion | TintaPropiedadMuestraFuerza |
      TintaPropiedadMuestraAltitud | TintaPropiedadMuestraAzimut;
  if (!sesion || id_muestra == 0 ||
      (propiedades & kPropiedadesConocidas) == 0 ||
      (propiedades & ~kPropiedadesConocidas) != 0)
    return false;

  const bool posicion = (propiedades & TintaPropiedadMuestraPosicion) != 0;
  const bool fuerza = (propiedades & TintaPropiedadMuestraFuerza) != 0;
  const bool inclinacion = (propiedades & TintaPropiedadMuestraAltitud) != 0;
  const bool direccion = (propiedades & TintaPropiedadMuestraAzimut) != 0;
  if ((posicion && (!std::isfinite(x) || !std::isfinite(y))) ||
      (fuerza && !std::isfinite(presion)) ||
      (inclinacion && !std::isfinite(altitud)) ||
      (direccion && !std::isfinite(azimut)))
    return false;

  auto referencia = sesion->muestras_actualizables.find(id_muestra);
  if (referencia == sesion->muestras_actualizables.end()) return false;

  tinta::Muestra& original = referencia->second;
  const float delta_x = posicion ? x - original.posicion.x : 0.0f;
  const float delta_y = posicion ? y - original.posicion.y : 0.0f;
  const float fuerza_normalizada =
      presion < 0.0f ? tinta::kSinPresion : std::clamp(presion, 0.0f, 1.0f);
  if (posicion) original.posicion = {x, y};
  if (fuerza) original.presion = fuerza_normalizada;
  if (inclinacion) original.inclinacion_x = altitud;
  if (direccion) original.inclinacion_y = azimut;

  size_t actualizadas = 0;
  for (auto& muestra : sesion->en_curso.muestras) {
    if (AplicarActualizacionEstimada(
            muestra, id_muestra, delta_x, delta_y, fuerza_normalizada,
            altitud, azimut, posicion, fuerza, inclinacion, direccion)) {
      ++actualizadas;
    }
  }
  actualizadas += ActualizarEnTrazos(
      sesion->pagina.trazos, id_muestra, delta_x, delta_y,
      fuerza_normalizada, altitud, azimut, posicion, fuerza, inclinacion,
      direccion);
  actualizadas += sesion->historial.ActualizarMuestraEstimada(
      id_muestra, delta_x, delta_y, fuerza_normalizada, altitud, azimut,
      posicion, fuerza, inclinacion, direccion);
  actualizadas += sesion->cronologia.ActualizarMuestraEstimada(
      id_muestra, delta_x, delta_y, fuerza_normalizada, altitud, azimut,
      posicion, fuerza, inclinacion, direccion);
  return actualizadas > 0;
}

bool tinta_finalizar_actualizaciones_muestra(TintaSesion* sesion,
                                             uint64_t id_muestra) {
  return sesion && id_muestra != 0 &&
         sesion->muestras_actualizables.erase(id_muestra) == 1;
}

void tinta_limpiar_provisionales(TintaSesion* sesion) {
  if (sesion) sesion->provisionales.clear();
}

void tinta_anadir_provisional(TintaSesion* sesion, float x, float y,
                              float presion, float altitud, float azimut,
                              double tiempo_s) {
  if (!sesion) return;
  sesion->provisionales.push_back(
      HacerMuestra(x, y, presion, altitud, azimut, tiempo_s));
}

int64_t tinta_terminar_trazo(TintaSesion* sesion) {
  if (!sesion || !sesion->hay_trazo) return -1;
  size_t indice = static_cast<size_t>(-1);
  if (!sesion->en_curso.muestras.empty()) {
    indice = sesion->en_curso.resaltador
        ? IndiceResaltadorInteligente(sesion->pagina, sesion->en_curso)
        : sesion->pagina.trazos.size();
    const int64_t tiempo_evento = sesion->en_curso.muestras.front().tiempo_qpc;
    sesion->pagina.trazos.insert(sesion->pagina.trazos.begin() +
        static_cast<std::ptrdiff_t>(indice), sesion->en_curso);
    RegistrarOperacion(
        sesion, tinta::Historial::OperacionAnadir(indice, sesion->en_curso),
        tiempo_evento);
  }
  sesion->hay_trazo = false;
  sesion->en_curso.muestras.clear();
  sesion->provisionales.clear();
  sesion->muestras_predichas = 0;
  return indice == static_cast<size_t>(-1) ? -1 : static_cast<int64_t>(indice);
}

int64_t tinta_terminar_trazo_con_id(TintaSesion* sesion, const char* id) {
  if (!sesion || !sesion->hay_trazo || !id || id[0] == '\0') return -1;
  const std::string solicitado(id);
  const auto repetido = std::find_if(
      sesion->pagina.trazos.begin(), sesion->pagina.trazos.end(),
      [&](const tinta::Trazo& trazo) {
        return !trazo.id_colaboracion.empty() &&
               trazo.id_colaboracion == solicitado;
      });
  if (repetido != sesion->pagina.trazos.end()) return -1;
  sesion->en_curso.id_colaboracion = solicitado;
  return tinta_terminar_trazo(sesion);
}

void tinta_descartar_trazo(TintaSesion* sesion) {
  if (!sesion) return;
  OlvidarActualizacionesDelTrazo(sesion, sesion->en_curso);
  sesion->hay_trazo = false;
  sesion->en_curso.muestras.clear();
  sesion->provisionales.clear();
  sesion->muestras_predichas = 0;
}

// --- Predicción ------------------------------------------------------------

void tinta_set_prediccion(TintaSesion* sesion, bool activa) {
  if (sesion) sesion->prediccion_activa = activa;
}

void tinta_set_horizonte_ms(TintaSesion* sesion, float ms) {
  if (sesion) sesion->params_prediccion.ms_adelante = ms;
}

size_t tinta_muestras_predichas(const TintaSesion* sesion) {
  return sesion ? sesion->muestras_predichas : 0;
}

// --- Geometría -------------------------------------------------------------

const float* tinta_vertices_en_curso(TintaSesion* sesion, size_t* cantidad) {
  if (cantidad) *cantidad = 0;
  if (!sesion || !sesion->hay_trazo || sesion->en_curso.muestras.empty()) {
    return nullptr;
  }

  const size_t reales = sesion->en_curso.muestras.size();
  sesion->muestras_predichas = 0;

  // Manda el sistema si mandó provisionales; si no, entra el predictor propio.
  // En ambos casos se añaden solo para teselar y se quitan justo después: se
  // dibujan, pero nunca entran al documento ni al archivo.
  if (!sesion->provisionales.empty()) {
    sesion->en_curso.muestras.insert(sesion->en_curso.muestras.end(),
                                     sesion->provisionales.begin(),
                                     sesion->provisionales.end());
    sesion->muestras_predichas = sesion->provisionales.size();
  } else if (sesion->prediccion_activa) {
    const auto predichas = tinta::PredecirContinuacion(
        sesion->en_curso, sesion->params_prediccion, kFrecuenciaNs);
    sesion->en_curso.muestras.insert(sesion->en_curso.muestras.end(),
                                     predichas.begin(), predichas.end());
    sesion->muestras_predichas = predichas.size();
  }

  sesion->vertices_en_curso = tinta::TeselarTrazo(sesion->en_curso, sesion->params);
  sesion->en_curso.muestras.resize(reales);

  if (cantidad) *cantidad = sesion->vertices_en_curso.size();
  return sesion->vertices_en_curso.empty()
             ? nullptr
             : reinterpret_cast<const float*>(sesion->vertices_en_curso.data());
}

const float* tinta_vertices_trazo(TintaSesion* sesion, size_t indice,
                                  size_t* cantidad, uint32_t* color_argb) {
  if (cantidad) *cantidad = 0;
  if (!sesion || indice >= sesion->pagina.trazos.size()) return nullptr;

  const tinta::Trazo& trazo = sesion->pagina.trazos[indice];
  if (color_argb) {
    *color_argb = ColorEfectivoTrazo(trazo);
  }

  sesion->vertices_trazo = tinta::TeselarTrazo(trazo, sesion->params);
  if (cantidad) *cantidad = sesion->vertices_trazo.size();
  return sesion->vertices_trazo.empty()
             ? nullptr
             : reinterpret_cast<const float*>(sesion->vertices_trazo.data());
}

const float* tinta_vertices_trazo_hasta(TintaSesion* sesion, size_t indice,
                                        double tiempo_s, size_t* cantidad,
                                        uint32_t* color_argb) {
  if (cantidad) *cantidad = 0;
  if (!sesion || indice >= sesion->pagina.trazos.size()) return nullptr;

  const tinta::Trazo& original = sesion->pagina.trazos[indice];
  if (color_argb) {
    *color_argb = ColorEfectivoTrazo(original);
  }
  const int64_t limite = tiempo_s <= 0
                              ? 0
                              : static_cast<int64_t>(tiempo_s * kFrecuenciaNs);
  const tinta::Trazo parcial = tinta::TrazoHasta(original, limite);
  sesion->vertices_trazo = tinta::TeselarTrazo(parcial, sesion->params);
  if (cantidad) *cantidad = sesion->vertices_trazo.size();
  return sesion->vertices_trazo.empty()
             ? nullptr
             : reinterpret_cast<const float*>(sesion->vertices_trazo.data());
}

uint32_t tinta_color_en_curso(const TintaSesion* sesion) {
  if (!sesion || !sesion->hay_trazo) return 0xFF000000u;
  const tinta::Trazo& t = sesion->en_curso;
  return ColorEfectivoTrazo(t);
}

// --- Estado ----------------------------------------------------------------

size_t tinta_cantidad_trazos(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.trazos.size() : 0;
}

bool tinta_hay_trazo_en_curso(const TintaSesion* sesion) {
  return sesion && sesion->hay_trazo && !sesion->en_curso.muestras.empty();
}

double tinta_frecuencia_hz(const TintaSesion* sesion) {
  if (!sesion || !sesion->hay_trazo) return 0.0;
  return tinta::FrecuenciaMuestreoHz(sesion->en_curso, kFrecuenciaNs);
}

bool tinta_rango_temporal(const TintaSesion* sesion, double* inicio_s,
                          double* fin_s) {
  if (inicio_s) *inicio_s = 0.0;
  if (fin_s) *fin_s = 0.0;
  if (!sesion) return false;
  const tinta::RangoTemporalTinta rango = tinta::RangoTemporal(sesion->pagina);
  if (!rango.valido()) return false;
  if (inicio_s) *inicio_s = static_cast<double>(rango.inicio) / kFrecuenciaNs;
  if (fin_s) *fin_s = static_cast<double>(rango.fin) / kFrecuenciaNs;
  return true;
}

// --- Cronología vectorial de Nota Viva -----------------------------------

bool tinta_cronologia_comenzar(TintaSesion* sesion, double inicio_s) {
  if (!sesion || sesion->documento_id.empty()) return false;
  const int64_t inicio_qpc = SegundosAQpc(inicio_s);
  if (inicio_qpc <= 0) return false;
  sesion->tiempo_cronologia_qpc = inicio_qpc;
  return sesion->cronologia.Comenzar(
      &sesion->pagina, sesion->documento_id, inicio_qpc);
}

void tinta_cronologia_fijar_tiempo(TintaSesion* sesion, double tiempo_s) {
  if (!sesion) return;
  const int64_t tiempo_qpc = SegundosAQpc(tiempo_s);
  if (tiempo_qpc > 0) sesion->tiempo_cronologia_qpc = tiempo_qpc;
}

void tinta_cronologia_cancelar(TintaSesion* sesion) {
  if (!sesion) return;
  sesion->cronologia.Cancelar();
  sesion->tiempo_cronologia_qpc = 0;
}

size_t tinta_cronologia_cantidad_eventos(const TintaSesion* sesion) {
  return sesion ? sesion->cronologia.cantidad_eventos() : 0u;
}

const uint8_t* tinta_cronologia_finalizar(TintaSesion* sesion,
                                          size_t* cantidad) {
  if (cantidad) *cantidad = 0;
  if (!sesion || !sesion->cronologia.activa()) return nullptr;
  sesion->bytes = sesion->cronologia.Serializar();
  if (sesion->bytes.empty()) return nullptr;
  sesion->cronologia.Cancelar();
  sesion->tiempo_cronologia_qpc = 0;
  if (cantidad) *cantidad = sesion->bytes.size();
  return sesion->bytes.data();
}

bool tinta_cronologia_cargar(TintaSesion* sesion, const uint8_t* datos,
                             size_t cantidad) {
  if (!sesion || !datos || cantidad == 0) return false;
  std::vector<uint8_t> copia;
  if (!CopiarEntradaSegura(datos, cantidad, &copia)) return false;
  tinta::Pagina base;
  if (!sesion->cronologia.Cargar(copia, &base)) return false;
  sesion->pagina = std::move(base);
  sesion->historial.Limpiar();
  tinta_descartar_trazo(sesion);
  return true;
}

bool tinta_cronologia_mostrar_hasta(TintaSesion* sesion, double tiempo_s) {
  if (!sesion) return false;
  const int64_t tiempo_qpc = SegundosAQpc(tiempo_s);
  if (tiempo_qpc <= 0) return false;
  return sesion->cronologia.MostrarHasta(tiempo_qpc, &sesion->pagina);
}

const char* tinta_trazo_id(TintaSesion* sesion, size_t indice) {
  if (!sesion || indice >= sesion->pagina.trazos.size()) return nullptr;
  sesion->texto_salida = sesion->pagina.trazos[indice].id_colaboracion;
  return sesion->texto_salida.c_str();
}

bool tinta_trazo_asignar_id(TintaSesion* sesion, size_t indice, const char* id) {
  if (!sesion || !id || id[0] == '\0' || indice >= sesion->pagina.trazos.size())
    return false;
  const std::string solicitado(id);
  const auto repetido = std::find_if(
      sesion->pagina.trazos.begin(), sesion->pagina.trazos.end(),
      [&](const tinta::Trazo& trazo) {
        return !trazo.id_colaboracion.empty() &&
               trazo.id_colaboracion == solicitado;
      });
  if (repetido != sesion->pagina.trazos.end() &&
      static_cast<size_t>(repetido - sesion->pagina.trazos.begin()) != indice)
    return false;
  auto& trazo = sesion->pagina.trazos[indice];
  if (!trazo.id_colaboracion.empty() && trazo.id_colaboracion != solicitado)
    return false;
  trazo.id_colaboracion = solicitado;
  return true;
}

const uint8_t* tinta_trazo_serializar(TintaSesion* sesion, size_t indice,
                                      size_t* cantidad) {
  if (cantidad) *cantidad = 0;
  if (!sesion || indice >= sesion->pagina.trazos.size()) return nullptr;
  sesion->bytes = tinta::SerializarTrazo(sesion->pagina.trazos[indice]);
  if (cantidad) *cantidad = sesion->bytes.size();
  return sesion->bytes.empty() ? nullptr : sesion->bytes.data();
}

const float* tinta_trazo_puntos(TintaSesion* sesion, size_t indice,
                                size_t* cantidad_puntos) {
  if (cantidad_puntos) *cantidad_puntos = 0;
  if (!sesion || indice >= sesion->pagina.trazos.size()) return nullptr;
  const auto& muestras = sesion->pagina.trazos[indice].muestras;
  sesion->puntos_trazo.clear();
  sesion->puntos_trazo.reserve(muestras.size() * 2);
  for (const auto& muestra : muestras) {
    sesion->puntos_trazo.push_back(muestra.posicion.x);
    sesion->puntos_trazo.push_back(muestra.posicion.y);
  }
  if (cantidad_puntos) *cantidad_puntos = muestras.size();
  return sesion->puntos_trazo.empty() ? nullptr : sesion->puntos_trazo.data();
}

bool tinta_trazo_importar(TintaSesion* sesion, const uint8_t* datos,
                          size_t cantidad) {
  if (!sesion || !datos || cantidad == 0) return false;
  std::vector<uint8_t> copia;
  if (!CopiarEntradaSegura(datos, cantidad, &copia)) return false;
  tinta::Trazo trazo;
  if (!tinta::DeserializarTrazo(copia, &trazo) ||
      trazo.id_colaboracion.empty() || trazo.muestras.empty())
    return false;
  const auto existe = std::find_if(
      sesion->pagina.trazos.begin(), sesion->pagina.trazos.end(),
      [&](const tinta::Trazo& actual) {
        return actual.id_colaboracion == trazo.id_colaboracion;
      });
  if (existe != sesion->pagina.trazos.end()) return true;
  const size_t indice = trazo.resaltador
      ? IndiceResaltadorInteligente(sesion->pagina, trazo)
      : sesion->pagina.trazos.size();
  sesion->pagina.trazos.insert(sesion->pagina.trazos.begin() +
      static_cast<std::ptrdiff_t>(indice), trazo);
  RegistrarOperacion(sesion,
                     tinta::Historial::OperacionAnadir(indice, trazo));
  return true;
}

bool tinta_trazo_importar_copia(TintaSesion* sesion, const uint8_t* datos,
                                size_t cantidad, const char* nuevo_id) {
  if (!sesion || !datos || cantidad == 0 || !nuevo_id || nuevo_id[0] == '\0')
    return false;
  std::vector<uint8_t> copia;
  if (!CopiarEntradaSegura(datos, cantidad, &copia)) return false;
  tinta::Trazo trazo;
  if (!tinta::DeserializarTrazo(copia, &trazo) ||
      trazo.muestras.empty())
    return false;
  const std::string solicitado(nuevo_id);
  const auto existe = std::find_if(
      sesion->pagina.trazos.begin(), sesion->pagina.trazos.end(),
      [&](const tinta::Trazo& actual) {
        return actual.id_colaboracion == solicitado;
      });
  if (existe != sesion->pagina.trazos.end()) return false;
  trazo.id_colaboracion = solicitado;
  const size_t indice = trazo.resaltador
      ? IndiceResaltadorInteligente(sesion->pagina, trazo)
      : sesion->pagina.trazos.size();
  sesion->pagina.trazos.insert(sesion->pagina.trazos.begin() +
      static_cast<std::ptrdiff_t>(indice), trazo);
  RegistrarOperacion(sesion,
                     tinta::Historial::OperacionAnadir(indice, trazo));
  return true;
}

bool tinta_trazo_eliminar_id(TintaSesion* sesion, const char* id) {
  if (!sesion || !id || id[0] == '\0') return false;
  const auto encontrado = std::find_if(
      sesion->pagina.trazos.begin(), sesion->pagina.trazos.end(),
      [&](const tinta::Trazo& trazo) { return trazo.id_colaboracion == id; });
  if (encontrado == sesion->pagina.trazos.end()) return true;
  const size_t indice = static_cast<size_t>(
      encontrado - sesion->pagina.trazos.begin());
  const tinta::Trazo quitado = *encontrado;
  sesion->pagina.trazos.erase(encontrado);
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionQuitar({indice}, {quitado}));
  return true;
}

// --- Edición ---------------------------------------------------------------

void tinta_deshacer(TintaSesion* sesion) {
  if (!sesion) return;
  tinta::Operacion operacion;
  if (sesion->historial.Deshacer(sesion->pagina, &operacion)) {
    sesion->cronologia.RegistrarOperacion(
        operacion, &sesion->pagina, sesion->tiempo_cronologia_qpc, true);
  }
}

void tinta_rehacer(TintaSesion* sesion) {
  if (!sesion) return;
  tinta::Operacion operacion;
  if (sesion->historial.Rehacer(sesion->pagina, &operacion)) {
    sesion->cronologia.RegistrarOperacion(
        operacion, &sesion->pagina, sesion->tiempo_cronologia_qpc, false);
  }
}

bool tinta_puede_deshacer(const TintaSesion* sesion) {
  return sesion && sesion->historial.puede_deshacer();
}

bool tinta_puede_rehacer(const TintaSesion* sesion) {
  return sesion && sesion->historial.puede_rehacer();
}

void tinta_limpiar(TintaSesion* sesion) {
  if (!sesion) return;
  const std::vector<tinta::Trazo> antes = sesion->pagina.trazos;
  if (antes.empty()) {
    tinta_descartar_trazo(sesion);
    return;
  }
  sesion->pagina.trazos.clear();
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionPagina(antes,
                                                 sesion->pagina.trazos));
  tinta_descartar_trazo(sesion);
}

bool tinta_borrar(TintaSesion* sesion, float x, float y, float radio,
                  bool parcial) {
  return tinta_borrar_filtrado(sesion, x, y, radio, parcial, 0, 0);
}

bool tinta_borrar_filtrado(TintaSesion* sesion, float x, float y, float radio,
                           bool parcial, int filtro, uint32_t color_argb) {
  if (!sesion) return false;
  const tinta::Punto centro{x, y};

  const auto coincide = [filtro, color_argb](const tinta::Trazo& trazo) {
    switch (filtro) {
      case 1: return trazo.resaltador;
      case 2:
        return (trazo.color_argb & 0x00FFFFFFu) ==
               (color_argb & 0x00FFFFFFu);
      case 3: return !trazo.resaltador;
      default: return true;
    }
  };

  if (parcial) {
    // El borrador parcial reconstruye la lista entera, así que para deshacerlo
    // se guarda el antes y el después de toda la página. Es caro, pero es lo
    // único correcto: un trazo puede convertirse en tres.
    const std::vector<tinta::Trazo> antes = sesion->pagina.trazos;
    const auto tocados = tinta::TrazosTocados(sesion->pagina, centro, radio);
    if (tocados.empty()) return false;
    std::vector<bool> toca(sesion->pagina.trazos.size(), false);
    for (const size_t indice : tocados) {
      if (indice < toca.size()) toca[indice] = true;
    }
    bool cambio = false;
    std::vector<tinta::Trazo> resultado;
    resultado.reserve(sesion->pagina.trazos.size());
    for (size_t i = 0; i < sesion->pagina.trazos.size(); ++i) {
      const auto& trazo = sesion->pagina.trazos[i];
      if (!toca[i] || !coincide(trazo)) {
        resultado.push_back(trazo);
        continue;
      }
      const auto pedazos = tinta::PartirTrazo(trazo, centro, radio);
      if (pedazos.size() == 1 &&
          pedazos[0].muestras.size() == trazo.muestras.size()) {
        resultado.push_back(trazo);
        continue;
      }
      cambio = true;
      resultado.insert(resultado.end(), pedazos.begin(), pedazos.end());
    }
    if (!cambio) return false;
    sesion->pagina.trazos = std::move(resultado);
    RegistrarOperacion(sesion, tinta::Historial::OperacionPagina(
        antes, sesion->pagina.trazos));
    return true;
  }

  auto tocados = tinta::TrazosTocados(sesion->pagina, centro, radio);
  tocados.erase(std::remove_if(tocados.begin(), tocados.end(),
                               [&](const size_t indice) {
    return indice >= sesion->pagina.trazos.size() ||
           !coincide(sesion->pagina.trazos[indice]);
  }), tocados.end());
  if (tocados.empty()) return false;
  const auto quitados = tinta::BorrarTrazosCompletos(sesion->pagina, tocados);
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionQuitar(tocados, quitados));
  return true;
}

} // extern "C"

namespace {

std::vector<size_t> IndicesValidosLimite(const size_t* indices,
                                         size_t cantidad, size_t limite) {
  std::vector<size_t> salida;
  if (!indices) return salida;
  salida.reserve(cantidad);
  for (size_t i = 0; i < cantidad; ++i) {
    if (indices[i] < limite) salida.push_back(indices[i]);
  }
  std::sort(salida.begin(), salida.end());
  salida.erase(std::unique(salida.begin(), salida.end()), salida.end());
  return salida;
}

std::vector<size_t> IndicesValidos(const TintaSesion* sesion,
                                   const size_t* indices, size_t cantidad) {
  if (!sesion) return {};
  return IndicesValidosLimite(indices, cantidad,
                              sesion->pagina.trazos.size());
}

std::vector<tinta::Trazo> CopiarTrazos(const tinta::Pagina& pagina,
                                       const std::vector<size_t>& indices) {
  std::vector<tinta::Trazo> salida;
  salida.reserve(indices.size());
  for (const size_t indice : indices) {
    if (indice < pagina.trazos.size()) salida.push_back(pagina.trazos[indice]);
  }
  return salida;
}

tinta::FiltroLazo CrearFiltroLazo(int tipo, uint32_t color_argb) {
  tinta::FiltroLazo filtro;
  if (tipo >= static_cast<int>(tinta::TipoFiltroLazo::Todos) &&
      tipo <= static_cast<int>(tinta::TipoFiltroLazo::SoloPdf)) {
    filtro.tipo = static_cast<tinta::TipoFiltroLazo>(tipo);
  }
  filtro.color_especifico = color_argb;
  return filtro;
}

TintaSeleccionInfo ExportarSeleccion(TintaSesion* sesion,
                                     const tinta::Seleccion& seleccion) {
  TintaSeleccionInfo salida{};
  if (!sesion) return salida;
  sesion->indices_salida = seleccion.trazos;
  sesion->indices_textos_salida = seleccion.textos;
  sesion->indices_medios_salida = seleccion.medios;
  salida.trazos = sesion->indices_salida.empty()
                      ? nullptr : sesion->indices_salida.data();
  salida.cantidad_trazos = sesion->indices_salida.size();
  salida.textos = sesion->indices_textos_salida.empty()
                      ? nullptr : sesion->indices_textos_salida.data();
  salida.cantidad_textos = sesion->indices_textos_salida.size();
  salida.medios = sesion->indices_medios_salida.empty()
                      ? nullptr : sesion->indices_medios_salida.data();
  salida.cantidad_medios = sesion->indices_medios_salida.size();
  return salida;
}

tinta::Seleccion CrearSeleccionValida(
    const TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios) {
  tinta::Seleccion seleccion;
  if (!sesion) return seleccion;
  seleccion.trazos = IndicesValidosLimite(
      trazos, cantidad_trazos, sesion->pagina.trazos.size());
  seleccion.textos = IndicesValidosLimite(
      textos, cantidad_textos, sesion->pagina.textos.size());
  seleccion.medios = IndicesValidosLimite(
      medios, cantidad_medios, sesion->pagina.medios.size());
  return seleccion;
}

template <typename T, typename F>
std::string IdCopiaUnico(TintaSesion* sesion, const std::string& base,
                         const std::vector<T>& elementos, F&& obtener_id) {
  std::string candidato;
  do {
    candidato = (base.empty() ? "elemento" : base) + "@copia." +
                std::to_string(++sesion->contador_copias);
  } while (std::any_of(elementos.begin(), elementos.end(),
                       [&](const T& elemento) {
    return obtener_id(elemento) == candidato;
  }));
  return candidato;
}

void BorrarIndicesTexto(std::vector<tinta::CuadroTexto>* elementos,
                        const std::vector<size_t>& indices) {
  if (!elementos) return;
  for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
    if (*it < elementos->size()) elementos->erase(elementos->begin() + *it);
  }
}

void BorrarIndicesMedio(std::vector<tinta::Medio>* elementos,
                        const std::vector<size_t>& indices) {
  if (!elementos) return;
  for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
    if (*it < elementos->size()) elementos->erase(elementos->begin() + *it);
  }
}

}  // namespace

extern "C" {

const size_t* tinta_lazo_seleccionar_trazos(TintaSesion* sesion,
                                            const float* puntos_xy,
                                            size_t cantidad_puntos,
                                            bool requerir_completo,
                                            size_t* cantidad_indices) {
  if (cantidad_indices) *cantidad_indices = 0;
  if (!sesion || !puntos_xy || cantidad_puntos < 3) return nullptr;
  std::vector<tinta::Punto> poligono;
  poligono.reserve(cantidad_puntos);
  for (size_t i = 0; i < cantidad_puntos; ++i) {
    poligono.push_back({puntos_xy[i * 2], puntos_xy[i * 2 + 1]});
  }
  sesion->indices_salida = tinta::SeleccionarConLazo(
      sesion->pagina, poligono, requerir_completo);
  if (cantidad_indices) *cantidad_indices = sesion->indices_salida.size();
  return sesion->indices_salida.empty() ? nullptr : sesion->indices_salida.data();
}

TintaSeleccionInfo tinta_seleccion_poligono(
    TintaSesion* sesion, const float* puntos_xy, size_t cantidad_puntos,
    int filtro, uint32_t color_argb, bool requerir_completo) {
  if (!sesion || !puntos_xy || cantidad_puntos < 3) return {};
  std::vector<tinta::Punto> poligono;
  poligono.reserve(cantidad_puntos);
  for (size_t i = 0; i < cantidad_puntos; ++i) {
    poligono.push_back({puntos_xy[i * 2], puntos_xy[i * 2 + 1]});
  }
  return ExportarSeleccion(sesion, tinta::SeleccionarPoligono(
      sesion->pagina, poligono, CrearFiltroLazo(filtro, color_argb),
      requerir_completo));
}

TintaSeleccionInfo tinta_seleccion_rectangulo(
    TintaSesion* sesion, float x, float y, float ancho, float alto,
    int filtro, uint32_t color_argb, bool requerir_completo) {
  if (!sesion || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(ancho) || !std::isfinite(alto) || ancho <= 0.0f ||
      alto <= 0.0f) return {};
  return ExportarSeleccion(sesion, tinta::SeleccionarRectangulo(
      sesion->pagina, x, y, ancho, alto,
      CrearFiltroLazo(filtro, color_argb), requerir_completo));
}

TintaSeleccionInfo tinta_seleccion_elipse(
    TintaSesion* sesion, float centro_x, float centro_y, float radio_x,
    float radio_y, int filtro, uint32_t color_argb,
    bool requerir_completo) {
  if (!sesion || !std::isfinite(centro_x) || !std::isfinite(centro_y) ||
      !std::isfinite(radio_x) || !std::isfinite(radio_y) || radio_x <= 0.0f ||
      radio_y <= 0.0f) return {};
  return ExportarSeleccion(sesion, tinta::SeleccionarElipse(
      sesion->pagina, {centro_x, centro_y}, radio_x, radio_y,
      CrearFiltroLazo(filtro, color_argb), requerir_completo));
}

TintaSeleccionInfo tinta_seleccion_pincel(
    TintaSesion* sesion, const float* puntos_xy, size_t cantidad_puntos,
    float radio, int filtro, uint32_t color_argb,
    bool requerir_completo) {
  if (!sesion || !puntos_xy || cantidad_puntos == 0 ||
      !std::isfinite(radio) || radio <= 0.0f) return {};
  std::vector<tinta::Punto> recorrido;
  recorrido.reserve(cantidad_puntos);
  for (size_t i = 0; i < cantidad_puntos; ++i) {
    recorrido.push_back({puntos_xy[i * 2], puntos_xy[i * 2 + 1]});
  }
  return ExportarSeleccion(sesion, tinta::SeleccionarPincel(
      sesion->pagina, recorrido, radio, CrearFiltroLazo(filtro, color_argb),
      requerir_completo));
}

TintaSeleccionInfo tinta_seleccion_todo(TintaSesion* sesion, int filtro,
                                        uint32_t color_argb) {
  if (!sesion) return {};
  return ExportarSeleccion(sesion, tinta::InvertirSeleccion(
      sesion->pagina, {}, CrearFiltroLazo(filtro, color_argb)));
}

bool tinta_trazos_transformar(TintaSesion* sesion, const size_t* indices,
                              size_t cantidad, float desplazar_x,
                              float desplazar_y, float escala,
                              float rotacion, float pivote_x, float pivote_y,
                              bool reflejar_horizontal,
                              bool reflejar_vertical) {
  const auto validos = IndicesValidos(sesion, indices, cantidad);
  if (validos.empty() || !std::isfinite(escala) || escala <= 0.0f ||
      !std::isfinite(desplazar_x) || !std::isfinite(desplazar_y) ||
      !std::isfinite(rotacion)) return false;
  const auto antes = CopiarTrazos(sesion->pagina, validos);
  tinta::Transformacion t;
  t.desplazar_x = desplazar_x;
  t.desplazar_y = desplazar_y;
  t.escala = escala;
  t.rotacion = rotacion;
  t.pivote = {pivote_x, pivote_y};
  t.reflejar_horizontal = reflejar_horizontal;
  t.reflejar_vertical = reflejar_vertical;
  tinta::TransformarTrazos(sesion->pagina, validos, t);
  const auto despues = CopiarTrazos(sesion->pagina, validos);
  RegistrarOperacion(sesion, tinta::Historial::OperacionReemplazar(
      validos, antes, despues));
  return true;
}

bool tinta_trazos_eliminar(TintaSesion* sesion, const size_t* indices,
                           size_t cantidad) {
  const auto validos = IndicesValidos(sesion, indices, cantidad);
  if (validos.empty()) return false;
  const auto quitados = tinta::BorrarTrazosCompletos(sesion->pagina, validos);
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionQuitar(validos, quitados));
  return true;
}

bool tinta_trazos_recolorear(TintaSesion* sesion, const size_t* indices,
                             size_t cantidad, uint32_t color_argb) {
  const auto validos = IndicesValidos(sesion, indices, cantidad);
  if (validos.empty()) return false;
  const auto antes = CopiarTrazos(sesion->pagina, validos);
  tinta::RecolorearTrazos(sesion->pagina, validos, color_argb);
  const auto despues = CopiarTrazos(sesion->pagina, validos);
  RegistrarOperacion(sesion, tinta::Historial::OperacionReemplazar(
      validos, antes, despues));
  return true;
}

bool tinta_seleccion_transformar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios, float desplazar_x, float desplazar_y,
    float escala_x, float escala_y, float rotacion, float pivote_x,
    float pivote_y, bool reflejar_horizontal, bool reflejar_vertical) {
  const tinta::Seleccion seleccion = CrearSeleccionValida(
      sesion, trazos, cantidad_trazos, textos, cantidad_textos, medios,
      cantidad_medios);
  if (!sesion || seleccion.vacia() || !std::isfinite(desplazar_x) ||
      !std::isfinite(desplazar_y) || !std::isfinite(escala_x) ||
      !std::isfinite(escala_y) || escala_x <= 0.0f || escala_y <= 0.0f ||
      !std::isfinite(rotacion) || !std::isfinite(pivote_x) ||
      !std::isfinite(pivote_y)) return false;
  const tinta::Pagina antes = sesion->pagina;
  tinta::Transformacion t;
  t.desplazar_x = desplazar_x;
  t.desplazar_y = desplazar_y;
  t.escala_x = escala_x;
  t.escala_y = escala_y;
  t.rotacion = rotacion;
  t.pivote = {pivote_x, pivote_y};
  t.reflejar_horizontal = reflejar_horizontal;
  t.reflejar_vertical = reflejar_vertical;
  tinta::TransformarSeleccion(sesion->pagina, seleccion, t);
  RegistrarOperacion(sesion, tinta::Historial::OperacionPaginaCompleta(
      antes, sesion->pagina));
  return true;
}

bool tinta_seleccion_eliminar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios) {
  const tinta::Seleccion seleccion = CrearSeleccionValida(
      sesion, trazos, cantidad_trazos, textos, cantidad_textos, medios,
      cantidad_medios);
  if (!sesion || seleccion.vacia()) return false;
  const tinta::Pagina antes = sesion->pagina;
  (void)tinta::BorrarTrazosCompletos(sesion->pagina, seleccion.trazos);
  BorrarIndicesTexto(&sesion->pagina.textos, seleccion.textos);
  BorrarIndicesMedio(&sesion->pagina.medios, seleccion.medios);
  RegistrarOperacion(sesion, tinta::Historial::OperacionPaginaCompleta(
      antes, sesion->pagina));
  return true;
}

TintaSeleccionInfo tinta_seleccion_duplicar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios, float desplazar_x, float desplazar_y) {
  const tinta::Seleccion original = CrearSeleccionValida(
      sesion, trazos, cantidad_trazos, textos, cantidad_textos, medios,
      cantidad_medios);
  if (!sesion || original.vacia() || !std::isfinite(desplazar_x) ||
      !std::isfinite(desplazar_y)) return {};
  const tinta::Pagina antes = sesion->pagina;
  tinta::Seleccion copias;
  for (const size_t indice : original.trazos) {
    tinta::Trazo copia = sesion->pagina.trazos[indice];
    copia.id_colaboracion = IdCopiaUnico(
        sesion, copia.id_colaboracion, sesion->pagina.trazos,
        [](const tinta::Trazo& t) { return t.id_colaboracion; });
    copias.trazos.push_back(sesion->pagina.trazos.size());
    sesion->pagina.trazos.push_back(std::move(copia));
  }
  for (const size_t indice : original.textos) {
    tinta::CuadroTexto copia = sesion->pagina.textos[indice];
    copia.id = IdCopiaUnico(sesion, copia.id, sesion->pagina.textos,
                            [](const tinta::CuadroTexto& t) { return t.id; });
    copias.textos.push_back(sesion->pagina.textos.size());
    sesion->pagina.textos.push_back(std::move(copia));
  }
  for (const size_t indice : original.medios) {
    tinta::Medio copia = sesion->pagina.medios[indice];
    copia.id = IdCopiaUnico(sesion, copia.id, sesion->pagina.medios,
                            [](const tinta::Medio& m) { return m.id; });
    copias.medios.push_back(sesion->pagina.medios.size());
    sesion->pagina.medios.push_back(std::move(copia));
  }
  tinta::Transformacion t;
  t.desplazar_x = desplazar_x;
  t.desplazar_y = desplazar_y;
  tinta::TransformarSeleccion(sesion->pagina, copias, t);
  RegistrarOperacion(sesion, tinta::Historial::OperacionPaginaCompleta(
      antes, sesion->pagina));
  return ExportarSeleccion(sesion, copias);
}

TintaSeleccionInfo tinta_seleccion_ordenar(
    TintaSesion* sesion, const size_t* trazos, size_t cantidad_trazos,
    const size_t* textos, size_t cantidad_textos, const size_t* medios,
    size_t cantidad_medios, bool traer_al_frente) {
  const tinta::Seleccion original = CrearSeleccionValida(
      sesion, trazos, cantidad_trazos, textos, cantidad_textos, medios,
      cantidad_medios);
  if (!sesion || original.vacia()) return {};
  const tinta::Pagina antes = sesion->pagina;
  if (traer_al_frente) tinta::TraerAlFrente(sesion->pagina, original);
  else tinta::EnviarAtras(sesion->pagina, original);
  tinta::Seleccion nuevos;
  const auto rango = [traer_al_frente](size_t total, size_t cantidad) {
    std::vector<size_t> indices;
    indices.reserve(cantidad);
    const size_t inicio = traer_al_frente ? total - cantidad : 0;
    for (size_t i = 0; i < cantidad; ++i) indices.push_back(inicio + i);
    return indices;
  };
  nuevos.trazos = rango(sesion->pagina.trazos.size(), original.trazos.size());
  nuevos.textos = rango(sesion->pagina.textos.size(), original.textos.size());
  nuevos.medios = rango(sesion->pagina.medios.size(), original.medios.size());
  RegistrarOperacion(sesion, tinta::Historial::OperacionPaginaCompleta(
      antes, sesion->pagina));
  return ExportarSeleccion(sesion, nuevos);
}

int tinta_enderezar_ultimo_trazo(TintaSesion* sesion) {
  if (!sesion || sesion->pagina.trazos.empty()) return 0;

  const size_t indice = sesion->pagina.trazos.size() - 1;
  const tinta::Trazo& original = sesion->pagina.trazos[indice];
  const auto forma = tinta::ReconocerForma(original);
  if (forma.tipo == tinta::TipoForma::Ninguna) return 0;

  const tinta::Trazo ideal = tinta::TrazoDesdeForma(forma, original);
  if (ideal.muestras.empty()) return 0;

  sesion->pagina.trazos[indice] = ideal;
  RegistrarOperacion(
      sesion,
      tinta::Historial::OperacionReemplazar({indice}, {original}, {ideal}));
  return static_cast<int>(forma.tipo);
}

// --- Papel y lienzo --------------------------------------------------------

void tinta_set_papel(TintaSesion* sesion, int tipo, int pauta, float separacion,
                     float margen_izquierdo) {
  if (!sesion) return;
  const tinta::Pagina antes = CopiarMetadatos(sesion->pagina);
  // Los valores fuera de rango se ignoran en vez de convertirse en un enum
  // inválido, que sería comportamiento indefinido al leerlo.
  if (tipo >= 0 && tipo <= 5) {
    sesion->pagina.papel.tipo = static_cast<tinta::TipoPapel>(tipo);
  }
  if (pauta >= 0 && pauta <= 17) {
    sesion->pagina.papel.pauta = static_cast<tinta::Pauta>(pauta);
  }
  if (separacion > 0.0f) sesion->pagina.papel.separacion = separacion;
  sesion->pagina.papel.margen_izquierdo = margen_izquierdo;
  RegistrarMetadatos(sesion, antes);
}

uint32_t tinta_color_papel(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.papel.color_fondo() : 0xFFFDFDFBu;
}

uint32_t tinta_color_pauta(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.papel.color_pauta() : 0x22203040u;
}

uint32_t tinta_color_tinta_sugerido(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.papel.color_tinta_por_defecto() : 0xFF1A1A1Au;
}

const float* tinta_pauta_visible(TintaSesion* sesion, float x, float y,
                                 float ancho, float alto, size_t* cantidad) {
  if (cantidad) *cantidad = 0;
  if (!sesion) return nullptr;

  const auto lineas = tinta::GenerarPauta(sesion->pagina.papel, x, y, ancho, alto);
  sesion->pauta.clear();
  sesion->pauta.reserve(lineas.size() * 4);
  for (const auto& l : lineas) {
    sesion->pauta.push_back(l.desde.x);
    sesion->pauta.push_back(l.desde.y);
    sesion->pauta.push_back(l.hasta.x);
    sesion->pauta.push_back(l.hasta.y);
  }

  if (cantidad) *cantidad = lineas.size();
  return sesion->pauta.empty() ? nullptr : sesion->pauta.data();
}

const float* tinta_pauta_visible_avanzada(TintaSesion* sesion, float x, float y,
                                          float ancho, float alto,
                                          size_t* cantidad) {
  if (cantidad) *cantidad = 0;
  if (!sesion) return nullptr;
  const auto lineas = tinta::GenerarPauta(sesion->pagina.papel, x, y, ancho, alto);
  sesion->pauta_avanzada.clear();
  sesion->pauta_avanzada.reserve(lineas.size() * 5);
  for (const auto& linea : lineas) {
    sesion->pauta_avanzada.push_back(linea.desde.x);
    sesion->pauta_avanzada.push_back(linea.desde.y);
    sesion->pauta_avanzada.push_back(linea.hasta.x);
    sesion->pauta_avanzada.push_back(linea.hasta.y);
    sesion->pauta_avanzada.push_back(linea.destacada ? 1.0f : 0.0f);
  }
  if (cantidad) *cantidad = lineas.size();
  return sesion->pauta_avanzada.empty() ? nullptr : sesion->pauta_avanzada.data();
}

void tinta_set_papel_avanzado(TintaSesion* sesion, int pauta,
                              float separacion, float opacidad,
                              float grosor, uint32_t color_argb,
                              float margen_izquierdo, float margen_derecho,
                              float margen_superior, float margen_inferior,
                              bool mostrar_ejes, float eje_x, float eje_y,
                              bool pauta_al_exportar) {
  if (!sesion) return;
  const tinta::Pagina antes = CopiarMetadatos(sesion->pagina);
  tinta::Papel& papel = sesion->pagina.papel;
  if (pauta >= 0 && pauta <= 17) papel.pauta = static_cast<tinta::Pauta>(pauta);
  papel.separacion = std::max(1.0f, separacion);
  papel.opacidad_pauta = std::clamp(opacidad, 0.0f, 1.0f);
  papel.grosor_pauta = std::clamp(grosor, 0.25f, 12.0f);
  papel.color_pauta_personalizado = color_argb;
  papel.margen_izquierdo = std::max(0.0f, margen_izquierdo);
  papel.margen_derecho = std::max(0.0f, margen_derecho);
  papel.margen_superior = std::max(0.0f, margen_superior);
  papel.margen_inferior = std::max(0.0f, margen_inferior);
  papel.mostrar_ejes = mostrar_ejes;
  papel.eje_x = eje_x;
  papel.eje_y = eje_y;
  papel.pauta_al_exportar = pauta_al_exportar;
  RegistrarMetadatos(sesion, antes);
}

void tinta_set_limites(TintaSesion* sesion, bool ancho_infinito,
                       bool alto_infinito, float ancho_min, float ancho_max,
                       float alto_min, float alto_max) {
  if (!sesion) return;
  const tinta::Pagina antes = CopiarMetadatos(sesion->pagina);
  tinta::LimitesLienzo limites;
  limites.ancho_infinito = ancho_infinito;
  limites.alto_infinito = alto_infinito;
  limites.ancho_minimo = ancho_min;
  limites.ancho_maximo = ancho_max;
  limites.alto_minimo = alto_min;
  limites.alto_maximo = alto_max;
  // Unos límites incoherentes se rechazan enteros: aplicarlos a medias dejaría
  // la página en un estado del que no se puede salir.
  if (!limites.valido()) return;

  sesion->pagina.limites = limites;
  tinta::AplicarLimites(limites, &sesion->pagina.ancho, &sesion->pagina.alto);
  RegistrarMetadatos(sesion, antes);
}

float tinta_ancho_pagina(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.ancho : 0.0f;
}

float tinta_alto_pagina(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.alto : 0.0f;
}

bool tinta_pagina_ancho_infinito(const TintaSesion* sesion) {
  return sesion && sesion->pagina.limites.ancho_infinito;
}

bool tinta_pagina_alto_infinito(const TintaSesion* sesion) {
  return sesion && sesion->pagina.limites.alto_infinito;
}

bool tinta_crecer_si_hace_falta(TintaSesion* sesion, float margen) {
  if (!sesion) return false;
  const tinta::Pagina antes = CopiarMetadatos(sesion->pagina);
  const bool cambio = sesion->pagina.CrecerSiHaceFalta(margen);
  if (cambio) RegistrarMetadatos(sesion, antes);
  return cambio;
}

void tinta_pagina_aplicar_modo(TintaSesion* sesion, int modo) {
  if (!sesion) return;
  const tinta::Pagina antes = CopiarMetadatos(sesion->pagina);
  tinta::AplicarModoPagina(
      &sesion->pagina,
      DesdeEntero(modo, 6, tinta::ModoPagina::Hibrido));
  RegistrarMetadatos(sesion, antes);
}

bool tinta_pagina_cambiar_tamano(TintaSesion* sesion, float ancho, float alto,
                                 int unidad, float ppp, int reacomodo) {
  if (!sesion) return false;
  auto antes = CopiarPaginaParaCronologia(sesion);
  if (sesion->cronologia.activa() && !antes) return false;
  const auto u = DesdeEntero(unidad, 3, tinta::UnidadLienzo::Pixeles);
  const auto r = DesdeEntero(reacomodo, 1, tinta::ReacomodoPagina::Conservar);
  const bool cambio = tinta::CambiarTamanoAvanzado(
      &sesion->pagina, tinta::ALongitudLienzo(ancho, u, ppp),
      tinta::ALongitudLienzo(alto, u, ppp), r);
  if (cambio) RegistrarCambioDirecto(sesion, antes);
  return cambio;
}

bool tinta_pagina_ajustar_al_contenido(TintaSesion* sesion, float margen) {
  if (!sesion) return false;
  const tinta::Pagina antes = CopiarMetadatos(sesion->pagina);
  const bool cambio = tinta::AjustarAlContenido(&sesion->pagina, margen);
  if (cambio) RegistrarMetadatos(sesion, antes);
  return cambio;
}

bool tinta_pagina_insertar_espacio(TintaSesion* sesion, int eje, float desde,
                                   float cantidad) {
  if (!sesion) return false;
  auto antes = CopiarPaginaParaCronologia(sesion);
  if (sesion->cronologia.activa() && !antes) return false;
  const bool cambio = tinta::InsertarEspacio(
      &sesion->pagina,
      DesdeEntero(eje, 1, tinta::EjeEspacio::Vertical), desde, cantidad);
  if (cambio) RegistrarCambioDirecto(sesion, antes);
  return cambio;
}

bool tinta_pagina_mover_borde(TintaSesion* sesion, int borde, float delta,
                              bool desplazar_contenido) {
  if (!sesion) return false;
  auto antes = CopiarPaginaParaCronologia(sesion);
  if (sesion->cronologia.activa() && !antes) return false;
  const bool cambio = tinta::MoverBorde(
      &sesion->pagina,
      DesdeEntero(borde, 3, tinta::BordePagina::Derecho), delta,
      desplazar_contenido);
  if (cambio) RegistrarCambioDirecto(sesion, antes);
  return cambio;
}

size_t tinta_reflujo_planificar(TintaSesion* sesion,
                                float x, float y, float ancho, float alto,
                                float factor_espaciado, int modo,
                                TintaTransformacionTrazo* resultado,
                                size_t capacidad) {
  if (!sesion || ancho <= 0 || alto <= 0 || factor_espaciado <= 0 ||
      modo < 0 || modo > 2) return 0;
  struct Caja { size_t indice; float min_x, min_y, max_x, max_y; };
  std::vector<Caja> cajas;
  const float max_x_region = x + ancho, max_y_region = y + alto;
  for (size_t i = 0; i < sesion->pagina.trazos.size(); ++i) {
    const auto& trazo = sesion->pagina.trazos[i];
    if (trazo.muestras.empty()) continue;
    Caja c{i, trazo.muestras[0].posicion.x, trazo.muestras[0].posicion.y,
           trazo.muestras[0].posicion.x, trazo.muestras[0].posicion.y};
    for (const auto& m : trazo.muestras) {
      c.min_x = std::min(c.min_x, m.posicion.x); c.max_x = std::max(c.max_x, m.posicion.x);
      c.min_y = std::min(c.min_y, m.posicion.y); c.max_y = std::max(c.max_y, m.posicion.y);
    }
    if (c.max_x >= x && c.min_x <= max_x_region &&
        c.max_y >= y && c.min_y <= max_y_region) cajas.push_back(c);
  }
  std::sort(cajas.begin(), cajas.end(), [](const Caja& a, const Caja& b) {
    const float ay = (a.min_y + a.max_y) * 0.5f, by = (b.min_y + b.max_y) * 0.5f;
    return std::abs(ay - by) < 8.0f ? a.min_x < b.min_x : ay < by;
  });
  if (cajas.empty()) return 0;
  float altura_media = 0;
  for (const auto& c : cajas) altura_media += std::max(c.max_y - c.min_y, 4.0f);
  altura_media /= cajas.size();
  const float paso = std::max(altura_media * factor_espaciado, 6.0f);
  std::vector<TintaTransformacionTrazo> plan;
  plan.reserve(cajas.size());
  float centro_linea_previo = -1e9f, objetivo_y = cajas.front().min_y;
  size_t linea = 0; float izquierda_linea = cajas.front().min_x;
  for (const auto& c : cajas) {
    const float centro = (c.min_y + c.max_y) * 0.5f;
    if (centro - centro_linea_previo > std::max(altura_media * 0.8f, 10.0f)) {
      if (centro_linea_previo > -1e8f) ++linea;
      centro_linea_previo = centro; izquierda_linea = c.min_x;
    }
    TintaTransformacionTrazo t{c.indice, 0.0f,
      objetivo_y + static_cast<float>(linea) * paso - c.min_y};
    if (modo == 1) t.delta_x = x + 12.0f - izquierda_linea;
    else if (modo == 2) {
      const size_t columna = &c - cajas.data();
      const float hueco = std::max((ancho - 24.0f) / std::max<size_t>(cajas.size(), 1), 20.0f);
      t.delta_x = x + 12.0f + columna * hueco - c.min_x;
    }
    plan.push_back(t);
  }
  const size_t n = std::min(capacidad, plan.size());
  if (resultado && n) std::copy_n(plan.begin(), n, resultado);
  return plan.size();
}

bool tinta_reflujo_aplicar(TintaSesion* sesion,
                           const TintaTransformacionTrazo* plan,
                           size_t cantidad) {
  if (!sesion || (!plan && cantidad)) return false;
  std::vector<size_t> indices;
  indices.reserve(cantidad);
  for (size_t i = 0; i < cantidad; ++i) {
    if (plan[i].indice_trazo >= sesion->pagina.trazos.size()) return false;
    indices.push_back(plan[i].indice_trazo);
  }
  std::sort(indices.begin(), indices.end());
  if (std::adjacent_find(indices.begin(), indices.end()) != indices.end())
    return false;
  const auto antes = CopiarTrazos(sesion->pagina, indices);
  for (size_t i = 0; i < cantidad; ++i) {
    auto& trazo = sesion->pagina.trazos[plan[i].indice_trazo];
    for (auto& muestra : trazo.muestras) {
      muestra.posicion.x += plan[i].delta_x;
      muestra.posicion.y += plan[i].delta_y;
    }
  }
  const auto despues = CopiarTrazos(sesion->pagina, indices);
  if (!indices.empty()) {
    RegistrarOperacion(sesion, tinta::Historial::OperacionReemplazar(
        indices, antes, despues));
  }
  return true;
}

const float* tinta_pagina_cortes_impresion(TintaSesion* sesion,
                                           float alto_hoja,
                                           float margen_seguro,
                                           size_t* cantidad) {
  if (cantidad) *cantidad = 0;
  if (!sesion) return nullptr;
  sesion->cortes_impresion =
      tinta::CortesDeImpresion(sesion->pagina, alto_hoja, margen_seguro);
  if (cantidad) *cantidad = sesion->cortes_impresion.size();
  return sesion->cortes_impresion.empty() ? nullptr
                                          : sesion->cortes_impresion.data();
}

// --- Guardar y cargar ------------------------------------------------------

bool tinta_documento_asignar_identidad(TintaSesion* sesion,
                                        const char* identidad) {
  if (!sesion || !identidad) return false;
  const std::string valor(identidad);
  if (valor.empty() || valor.size() > 128) return false;
  const bool segura = std::all_of(valor.begin(), valor.end(), [](char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '-' || c == '_' || c == '.';
  });
  if (!segura) return false;
  // La identidad nace una vez y después es inmutable. Permitir sustituirla en
  // una sesión ya asociada haría posible abrir A y rebautizarlo como B antes de
  // guardar, exactamente el cruce documental que este campo debe impedir.
  if (!sesion->documento_id.empty() && sesion->documento_id != valor) {
    tinta::EstablecerErrorArchivo(
        "la identidad durable de la sesion es inmutable");
    return false;
  }
  sesion->documento_id = valor;
  return true;
}

const char* tinta_documento_identidad(const TintaSesion* sesion) {
  return sesion ? sesion->documento_id.c_str() : "";
}

const uint8_t* tinta_guardar(TintaSesion* sesion, size_t* cantidad) {
  if (cantidad) *cantidad = 0;
  if (!sesion) return nullptr;
  if (sesion->documento_id.empty()) {
    tinta::EstablecerErrorArchivo(
        "no se puede guardar sin identidad durable");
    return nullptr;
  }

  tinta::Documento documento;
  documento.id = sesion->documento_id;
  documento.titulo = sesion->documento_titulo;
  tinta::Cuaderno cuaderno;
  tinta::Seccion seccion;
  seccion.paginas.push_back(sesion->pagina);
  cuaderno.secciones.push_back(std::move(seccion));
  documento.cuadernos.push_back(std::move(cuaderno));

  sesion->bytes = tinta::Serializar(documento);
  if (sesion->bytes.empty()) return nullptr;
  if (cantidad) *cantidad = sesion->bytes.size();
  return sesion->bytes.empty() ? nullptr : sesion->bytes.data();
}

bool tinta_cargar(TintaSesion* sesion, const uint8_t* datos, size_t cantidad) {
  if (!sesion || !datos || cantidad == 0) return false;

  std::vector<uint8_t> copia;
  if (!CopiarEntradaSegura(datos, cantidad, &copia)) return false;
  tinta::Documento documento;
  if (!tinta::Deserializar(copia, &documento)) return false;
  if (!documento.id.empty() && !sesion->documento_id.empty() &&
      documento.id != sesion->documento_id) {
    tinta::EstablecerErrorArchivo(
        "la identidad del archivo no corresponde a esta sesion");
    return false;
  }
  if (documento.cuadernos.empty()) return false;
  if (documento.cuadernos[0].secciones.empty()) return false;
  if (documento.cuadernos[0].secciones[0].paginas.empty()) return false;

  sesion->pagina = documento.cuadernos[0].secciones[0].paginas[0];
  if (!documento.id.empty()) sesion->documento_id = documento.id;
  sesion->documento_titulo = documento.titulo;
  // El historial no sobrevive a una carga: deshacer hasta antes de abrir el
  // archivo no significa nada.
  sesion->historial.Limpiar();
  tinta_descartar_trazo(sesion);
  return true;
}

bool tinta_cargar_como_copia(TintaSesion* sesion, const uint8_t* datos,
                             size_t cantidad, const char* identidad_nueva) {
  if (!sesion || !datos || cantidad == 0 || !identidad_nueva) return false;
  std::vector<uint8_t> copia;
  if (!CopiarEntradaSegura(datos, cantidad, &copia)) return false;
  tinta::Documento documento;
  if (!tinta::Deserializar(copia, &documento)) return false;
  if (documento.cuadernos.empty() ||
      documento.cuadernos[0].secciones.empty() ||
      documento.cuadernos[0].secciones[0].paginas.empty()) return false;
  if (!tinta_documento_asignar_identidad(sesion, identidad_nueva)) {
    tinta::EstablecerErrorArchivo("la identidad nueva de la copia no es valida");
    return false;
  }
  sesion->pagina = documento.cuadernos[0].secciones[0].paginas[0];
  sesion->documento_titulo = documento.titulo;
  sesion->historial.Limpiar();
  tinta_descartar_trazo(sesion);
  return true;
}

const char* tinta_ultimo_error(void) { return tinta::UltimoErrorArchivo(); }

// --- Grabación de pantalla -------------------------------------------------

TintaAjustesGrabacion tinta_grabacion_por_defecto(void) {
  return ADentro(tinta::AjustesGrabacion{});
}

int tinta_grabacion_cantidad_preajustes(void) {
  return static_cast<int>(tinta::Preajustes().size());
}

TintaAjustesGrabacion tinta_grabacion_preajuste(int indice) {
  const std::vector<tinta::PreajusteGrabacion> lista = tinta::Preajustes();
  if (indice < 0 || indice >= static_cast<int>(lista.size())) {
    return ADentro(tinta::AjustesGrabacion{});
  }
  return ADentro(lista[static_cast<size_t>(indice)].ajustes);
}

const char* tinta_grabacion_nombre_preajuste(int indice) {
  return TextoPreajuste(indice, true);
}

const char* tinta_grabacion_para_que_preajuste(int indice) {
  return TextoPreajuste(indice, false);
}

TintaAjustesGrabacion tinta_grabacion_ajustar(TintaAjustesGrabacion ajustes,
                                              TintaCapacidades cap) {
  return ADentro(tinta::AjustarACapacidades(AFuera(ajustes), AFuera(cap)));
}

bool tinta_grabacion_validar(TintaAjustesGrabacion ajustes,
                             TintaCapacidades cap, char* motivo, size_t tam) {
  std::string texto;
  const bool bien = tinta::Validar(AFuera(ajustes), AFuera(cap), &texto);
  Copiar(texto, motivo, tam);
  return bien;
}

void tinta_grabacion_tamano(TintaAjustesGrabacion ajustes, TintaCapacidades cap,
                            int* ancho, int* alto) {
  const tinta::TamanoVideo t =
      tinta::ResolverTamano(AFuera(ajustes).video, AFuera(cap));
  if (ancho) *ancho = t.ancho;
  if (alto) *alto = t.alto;
}

int tinta_grabacion_kbps_video(TintaAjustesGrabacion ajustes,
                               TintaCapacidades cap) {
  return tinta::KbpsVideoEfectivo(AFuera(ajustes).video, AFuera(cap));
}

int tinta_grabacion_kbps_audio(TintaAjustesGrabacion ajustes,
                               TintaCapacidades cap) {
  return tinta::KbpsAudioEfectivo(AFuera(ajustes).audio, AFuera(cap));
}

int tinta_grabacion_canales(TintaAjustesGrabacion ajustes) {
  return tinta::CanalesDe(AFuera(ajustes).audio.canales);
}

double tinta_grabacion_mb_por_minuto(TintaAjustesGrabacion ajustes,
                                     TintaCapacidades cap) {
  return tinta::MegabytesPorMinuto(AFuera(ajustes), AFuera(cap));
}

double tinta_grabacion_minutos_que_caben(TintaAjustesGrabacion ajustes,
                                         TintaCapacidades cap,
                                         double megabytes_libres) {
  return tinta::MinutosQueCaben(AFuera(ajustes), AFuera(cap), megabytes_libres);
}

// --- Puntero del lápiz del iPad ----------------------------------------------

float tinta_puntero_radio_efectivo(float radio) {
  tinta::AjustesPuntero p;
  p.radio = radio;
  return p.RadioEfectivo();
}

float tinta_puntero_grosor_anillo(float radio, float grosor) {
  tinta::AjustesPuntero p;
  p.radio = radio;
  p.grosor_anillo = grosor;
  return p.GrosorAnilloEfectivo();
}

uint32_t tinta_puntero_color_efectivo(uint32_t color_puntero,
                                      uint32_t color_pluma,
                                      bool seguir_color_pluma) {
  tinta::AjustesPuntero p;
  p.color_argb = color_puntero;
  p.seguir_color_pluma = seguir_color_pluma;
  return p.ColorEfectivo(color_pluma);
}

bool tinta_puntero_debe_verse(bool visible, bool hay_hover, bool dibujando,
                              bool mostrar_al_dibujar) {
  tinta::AjustesPuntero p;
  p.visible = visible;
  p.mostrar_al_dibujar = mostrar_al_dibujar;
  return p.DebeVerse(hay_hover, dibujando);
}

// --- Pantalla siempre activa -----------------------------------------------

bool tinta_pantalla_siempre_activa(bool siempre, bool al_grabar,
                                   bool al_escribir, bool grabando,
                                   bool escribiendo) {
  tinta::AjustesPantalla p;
  p.siempre_activa = siempre;
  p.siempre_activa_al_grabar = al_grabar;
  p.siempre_activa_al_escribir = al_escribir;
  return p.Debe(grabando, escribiendo);
}

// --- Dictado ---------------------------------------------------------------

size_t tinta_limpiar_dictado(const char* crudo, bool puntuacion_hablada,
                             bool mayuscula_inicial, char* salida, size_t tam) {
  if (!crudo) {
    if (salida && tam > 0) salida[0] = '\0';
    return 0;
  }
  tinta::AjustesDictado ajustes;
  ajustes.puntuacion_hablada = puntuacion_hablada;
  ajustes.mayuscula_inicial = mayuscula_inicial;

  const std::string limpio = tinta::LimpiarDictado(crudo, ajustes);
  if (salida) Copiar(limpio, salida, tam);
  return limpio.size();
}

// --- Importar PDF ----------------------------------------------------------

int tinta_pdf_orientacion(float ancho_pt, float alto_pt, int rotacion) {
  tinta::PaginaPdfFuente fuente;
  fuente.ancho_pt = ancho_pt;
  fuente.alto_pt = alto_pt;
  fuente.rotacion = rotacion;
  return tinta::OrientacionDe(fuente) == tinta::OrientacionPagina::Horizontal
             ? 1
             : 0;
}

void tinta_pdf_tamano_pagina(float ancho_pt, float alto_pt, int rotacion,
                             float ppp, int ajuste, float ancho_objetivo,
                             float alto_objetivo, bool margen_para_notas,
                             float fraccion_margen, float* ancho, float* alto) {
  tinta::PaginaPdfFuente fuente;
  fuente.ancho_pt = ancho_pt;
  fuente.alto_pt = alto_pt;
  fuente.rotacion = rotacion;

  tinta::AjustesImportarPdf ajustes;
  ajustes.ppp = ppp;
  ajustes.ajuste = AjusteDesdeEntero(ajuste);
  ajustes.ancho_objetivo = ancho_objetivo;
  ajustes.alto_objetivo = alto_objetivo;
  ajustes.margen_para_notas = margen_para_notas;
  ajustes.fraccion_margen = fraccion_margen;

  tinta::TamanoDePagina(fuente, ajustes, ancho, alto);
}

void tinta_pdf_marco(float ancho_pt, float alto_pt, int rotacion, float ppp,
                     int ajuste, float ancho_objetivo, float alto_objetivo,
                     float* x, float* y, float* ancho, float* alto) {
  tinta::PaginaPdfFuente fuente;
  fuente.ancho_pt = ancho_pt;
  fuente.alto_pt = alto_pt;
  fuente.rotacion = rotacion;

  tinta::AjustesImportarPdf ajustes;
  ajustes.ppp = ppp;
  ajustes.ajuste = AjusteDesdeEntero(ajuste);
  ajustes.ancho_objetivo = ancho_objetivo;
  ajustes.alto_objetivo = alto_objetivo;

  const tinta::Rect marco = tinta::MarcoDelPdf(fuente, ajustes);
  if (x) *x = marco.x;
  if (y) *y = marco.y;
  if (ancho) *ancho = marco.ancho;
  if (alto) *alto = marco.alto;
}

// --- Recortar imágenes -----------------------------------------------------

void tinta_recorte_componer(float* recorte_x, float* recorte_y,
                            float* recorte_ancho, float* recorte_alto, float x,
                            float y, float ancho, float alto) {
  if (!recorte_x || !recorte_y || !recorte_ancho || !recorte_alto) return;

  tinta::Medio medio;
  medio.recorte_x = *recorte_x;
  medio.recorte_y = *recorte_y;
  medio.recorte_ancho = *recorte_ancho;
  medio.recorte_alto = *recorte_alto;

  tinta::RecortarRelativo(&medio, x, y, ancho, alto);

  *recorte_x = medio.recorte_x;
  *recorte_y = medio.recorte_y;
  *recorte_ancho = medio.recorte_ancho;
  *recorte_alto = medio.recorte_alto;
}

// --- Elementos colocables -------------------------------------------------

bool tinta_texto_anadir(TintaSesion* sesion, const char* id,
                        const char* texto, float x, float y,
                        float ancho, float alto, bool desde_dictado) {
  if (!sesion || !id || !*id || !texto || ancho <= 0.0f || alto <= 0.0f)
    return false;
  const auto repetido = std::find_if(
      sesion->pagina.textos.begin(), sesion->pagina.textos.end(),
      [id](const tinta::CuadroTexto& item) { return item.id == id; });
  if (repetido != sesion->pagina.textos.end()) return false;
  tinta::CuadroTexto item;
  item.id = id;
  item.texto = texto;
  item.marco = {x, y, ancho, alto};
  item.desde_dictado = desde_dictado;
  sesion->pagina.textos.push_back(std::move(item));
  const size_t indice = sesion->pagina.textos.size() - 1u;
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionAnadirTexto(
                  indice, sesion->pagina.textos[indice]));
  return true;
}

size_t tinta_texto_cantidad(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.textos.size() : 0;
}

TintaTextoInfo tinta_texto_info(const TintaSesion* sesion, size_t indice) {
  TintaTextoInfo salida{};
  if (!sesion || indice >= sesion->pagina.textos.size()) return salida;
  const tinta::CuadroTexto& item = sesion->pagina.textos[indice];
  salida.id = item.id.c_str();
  salida.texto = item.texto.c_str();
  salida.x = item.marco.x;
  salida.y = item.marco.y;
  salida.ancho = item.marco.ancho;
  salida.alto = item.marco.alto;
  salida.rotacion = item.rotacion;
  salida.alto_automatico = item.alto_automatico;
  salida.desde_dictado = item.desde_dictado;
  return salida;
}

bool tinta_texto_actualizar(TintaSesion* sesion, const char* id,
                            const char* texto, float x, float y,
                            float ancho, float alto, float rotacion) {
  if (!sesion || !id || !texto || ancho <= 0.0f || alto <= 0.0f) return false;
  auto item = std::find_if(sesion->pagina.textos.begin(), sesion->pagina.textos.end(),
      [id](const tinta::CuadroTexto& candidato) { return candidato.id == id; });
  if (item == sesion->pagina.textos.end()) return false;
  const size_t indice = static_cast<size_t>(item - sesion->pagina.textos.begin());
  const tinta::CuadroTexto antes = *item;
  item->texto = texto;
  item->marco = {x, y, ancho, alto};
  item->rotacion = rotacion;
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionReemplazarTexto(
                  indice, antes, *item));
  return true;
}

bool tinta_texto_eliminar(TintaSesion* sesion, const char* id) {
  if (!sesion || !id) return false;
  auto& elementos = sesion->pagina.textos;
  const auto item = std::find_if(elementos.begin(), elementos.end(),
      [id](const tinta::CuadroTexto& actual) { return actual.id == id; });
  if (item == elementos.end()) return false;
  const size_t indice = static_cast<size_t>(item - elementos.begin());
  const tinta::CuadroTexto quitado = *item;
  elementos.erase(item);
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionQuitarTexto(indice, quitado));
  return true;
}

bool tinta_medio_anadir(TintaSesion* sesion, const char* id,
                        const char* hash, const char* nombre_original,
                        int tipo, float x, float y, float ancho, float alto) {
  if (!sesion || !id || !*id || !hash || !nombre_original ||
      tipo < 0 || tipo > 4 || ancho <= 0.0f || alto <= 0.0f) return false;
  const auto repetido = std::find_if(
      sesion->pagina.medios.begin(), sesion->pagina.medios.end(),
      [id](const tinta::Medio& item) { return item.id == id; });
  if (repetido != sesion->pagina.medios.end()) return false;
  tinta::Medio item;
  item.id = id;
  item.hash = hash;
  item.nombre_original = nombre_original;
  item.tipo = static_cast<tinta::TipoMedio>(tipo);
  item.marco = {x, y, ancho, alto};
  sesion->pagina.medios.push_back(std::move(item));
  const size_t indice = sesion->pagina.medios.size() - 1u;
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionAnadirMedio(
                  indice, sesion->pagina.medios[indice]));
  return true;
}

size_t tinta_medio_cantidad(const TintaSesion* sesion) {
  return sesion ? sesion->pagina.medios.size() : 0;
}

TintaMedioInfo tinta_medio_info(const TintaSesion* sesion, size_t indice) {
  TintaMedioInfo salida{};
  if (!sesion || indice >= sesion->pagina.medios.size()) return salida;
  const tinta::Medio& item = sesion->pagina.medios[indice];
  salida.id = item.id.c_str();
  salida.hash = item.hash.c_str();
  salida.nombre_original = item.nombre_original.c_str();
  salida.tipo = static_cast<int>(item.tipo);
  salida.x = item.marco.x;
  salida.y = item.marco.y;
  salida.ancho = item.marco.ancho;
  salida.alto = item.marco.alto;
  salida.rotacion = item.rotacion;
  salida.opacidad = item.opacidad;
  salida.recorte_x = item.recorte_x;
  salida.recorte_y = item.recorte_y;
  salida.recorte_ancho = item.recorte_ancho;
  salida.recorte_alto = item.recorte_alto;
  salida.pagina_fuente = item.pagina_fuente;
  salida.segundo_inicio = item.segundo_inicio;
  return salida;
}

bool tinta_medio_actualizar_marco(TintaSesion* sesion, const char* id,
                                  float x, float y, float ancho, float alto,
                                  float rotacion, float opacidad) {
  if (!sesion || !id || ancho <= 0.0f || alto <= 0.0f) return false;
  auto item = std::find_if(sesion->pagina.medios.begin(), sesion->pagina.medios.end(),
      [id](const tinta::Medio& candidato) { return candidato.id == id; });
  if (item == sesion->pagina.medios.end()) return false;
  const size_t indice = static_cast<size_t>(item - sesion->pagina.medios.begin());
  const tinta::Medio antes = *item;
  item->marco = {x, y, ancho, alto};
  item->rotacion = rotacion;
  item->opacidad = std::clamp(opacidad, 0.0f, 1.0f);
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionReemplazarMedio(
                  indice, antes, *item));
  return true;
}

bool tinta_medio_set_fuente(TintaSesion* sesion, const char* id,
                            int pagina_fuente, double segundo_inicio) {
  if (!sesion || !id || pagina_fuente < 0 || segundo_inicio < 0.0) return false;
  auto item = std::find_if(sesion->pagina.medios.begin(), sesion->pagina.medios.end(),
      [id](const tinta::Medio& candidato) { return candidato.id == id; });
  if (item == sesion->pagina.medios.end()) return false;
  const size_t indice = static_cast<size_t>(item - sesion->pagina.medios.begin());
  const tinta::Medio antes = *item;
  item->pagina_fuente = pagina_fuente;
  item->segundo_inicio = segundo_inicio;
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionReemplazarMedio(
                  indice, antes, *item));
  return true;
}

bool tinta_medio_recortar(TintaSesion* sesion, const char* id,
                          float x, float y, float ancho, float alto) {
  if (!sesion || !id) return false;
  auto item = std::find_if(sesion->pagina.medios.begin(), sesion->pagina.medios.end(),
      [id](const tinta::Medio& candidato) { return candidato.id == id; });
  if (item == sesion->pagina.medios.end()) return false;
  const size_t indice = static_cast<size_t>(item - sesion->pagina.medios.begin());
  const tinta::Medio antes = *item;
  tinta::RecortarRelativo(&*item, x, y, ancho, alto);
  if (!item->recorte_valido()) return false;
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionReemplazarMedio(
                  indice, antes, *item));
  return true;
}

bool tinta_medio_quitar_recorte(TintaSesion* sesion, const char* id) {
  if (!sesion || !id) return false;
  auto item = std::find_if(sesion->pagina.medios.begin(), sesion->pagina.medios.end(),
      [id](const tinta::Medio& candidato) { return candidato.id == id; });
  if (item == sesion->pagina.medios.end()) return false;
  const size_t indice = static_cast<size_t>(item - sesion->pagina.medios.begin());
  const tinta::Medio antes = *item;
  item->recorte_x = 0.0f; item->recorte_y = 0.0f;
  item->recorte_ancho = 1.0f; item->recorte_alto = 1.0f;
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionReemplazarMedio(
                  indice, antes, *item));
  return true;
}

bool tinta_medio_eliminar(TintaSesion* sesion, const char* id) {
  if (!sesion || !id) return false;
  auto& elementos = sesion->pagina.medios;
  const auto item = std::find_if(elementos.begin(), elementos.end(),
      [id](const tinta::Medio& actual) { return actual.id == id; });
  if (item == elementos.end()) return false;
  const size_t indice = static_cast<size_t>(item - elementos.begin());
  const tinta::Medio quitado = *item;
  elementos.erase(item);
  RegistrarOperacion(
      sesion, tinta::Historial::OperacionQuitarMedio(indice, quitado));
  return true;
}

// --- Matemáticas avanzadas locales ---------------------------------------

bool tinta_matematica_evaluar(const char* expresion,
                              const TintaVariableCompleja* variables,
                              size_t cantidad_variables,
                              TintaComplejo* resultado,
                              char* error, size_t tam_error) {
  if (!expresion || expresion[0] == '\0') {
    Copiar("la expresión está vacía", error, tam_error);
    return false;
  }
  const auto r = tinta::EvaluarExpresion(
      expresion, VariablesDesdeC(variables, cantidad_variables));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

size_t tinta_matematica_muestrear(const char* expresion,
                                  const char* variable,
                                  double inicio, double fin,
                                  size_t cantidad,
                                  const TintaVariableCompleja* variables,
                                  size_t cantidad_variables,
                                  TintaMuestraFuncion* muestras,
                                  size_t capacidad,
                                  char* error, size_t tam_error) {
  if (!expresion || expresion[0] == '\0' || !variable || variable[0] == '\0' ||
      cantidad < 2) {
    Copiar("expresión, variable o intervalo inválido", error, tam_error);
    return 0;
  }
  const auto resultado = tinta::MuestrearFuncion(
      expresion, variable, inicio, fin, cantidad,
      VariablesDesdeC(variables, cantidad_variables));
  const size_t n = std::min(capacidad, resultado.size());
  bool hay_validas = false;
  for (size_t i = 0; muestras && i < n; ++i) {
    muestras[i].x = resultado[i].x;
    muestras[i].y.real = resultado[i].y.real();
    muestras[i].y.imaginario = resultado[i].y.imag();
    muestras[i].valida = resultado[i].valida;
    hay_validas = hay_validas || resultado[i].valida;
  }
  if (!resultado.empty() && !hay_validas && n > 0) {
    Copiar("la función no produjo muestras válidas en el intervalo", error,
           tam_error);
  } else if (error && tam_error) {
    error[0] = '\0';
  }
  return resultado.size();
}

bool tinta_matematica_derivar(const char* expresion, const char* variable,
                              double x,
                              const TintaVariableCompleja* variables,
                              size_t cantidad_variables,
                              TintaComplejo* resultado,
                              char* error, size_t tam_error) {
  if (!expresion || !variable || expresion[0] == '\0' || variable[0] == '\0') {
    Copiar("expresión o variable vacía", error, tam_error);
    return false;
  }
  const auto r = tinta::DerivadaNumerica(
      expresion, variable, x, VariablesDesdeC(variables, cantidad_variables));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

bool tinta_matematica_integrar(const char* expresion, const char* variable,
                               double inicio, double fin, int subintervalos,
                               const TintaVariableCompleja* variables,
                               size_t cantidad_variables,
                               TintaComplejo* resultado,
                               char* error, size_t tam_error) {
  if (!expresion || !variable || expresion[0] == '\0' || variable[0] == '\0') {
    Copiar("expresión o variable vacía", error, tam_error);
    return false;
  }
  const auto r = tinta::IntegralNumerica(
      expresion, variable, inicio, fin, subintervalos,
      VariablesDesdeC(variables, cantidad_variables));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

bool tinta_matematica_convertir_unidad(double valor, const char* origen,
                                       const char* destino, double* resultado,
                                       char* error, size_t tam_error) {
  if (!origen || !destino || origen[0] == '\0' || destino[0] == '\0') {
    Copiar("unidad de origen o destino vacía", error, tam_error);
    return false;
  }
  const auto r = tinta::ConvertirUnidad(valor, origen, destino);
  if (!r.correcto) {
    Copiar(r.error, error, tam_error);
    return false;
  }
  if (resultado) *resultado = r.valor;
  if (error && tam_error) error[0] = '\0';
  return true;
}

bool tinta_matriz_determinante(const TintaComplejo* valores, size_t orden,
                               TintaComplejo* resultado,
                               char* error, size_t tam_error) {
  const auto r = tinta::DeterminanteMatriz(
      ComplejosDesdeC(valores, orden * orden), orden);
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

size_t tinta_matriz_invertir(const TintaComplejo* valores, size_t orden,
                             TintaComplejo* resultado, size_t capacidad,
                             char* error, size_t tam_error) {
  const auto r = tinta::InvertirMatriz(
      ComplejosDesdeC(valores, orden * orden), orden);
  if (!r.correcto) { Copiar(r.error, error, tam_error); return 0; }
  CopiarComplejos(r.valores, resultado, capacidad);
  if (error && tam_error) error[0] = '\0';
  return r.valores.size();
}

size_t tinta_sistema_lineal_resolver(const TintaComplejo* coeficientes,
                                     const TintaComplejo* terminos,
                                     size_t orden, TintaComplejo* resultado,
                                     size_t capacidad,
                                     char* error, size_t tam_error) {
  const auto r = tinta::ResolverSistemaLineal(
      ComplejosDesdeC(coeficientes, orden * orden),
      ComplejosDesdeC(terminos, orden), orden);
  if (!r.correcto) { Copiar(r.error, error, tam_error); return 0; }
  CopiarComplejos(r.valores, resultado, capacidad);
  if (error && tam_error) error[0] = '\0';
  return r.valores.size();
}

bool tinta_vector_producto_punto(const TintaComplejo* a,
                                 const TintaComplejo* b, size_t cantidad,
                                 TintaComplejo* resultado,
                                 char* error, size_t tam_error) {
  const auto r = tinta::ProductoPunto(ComplejosDesdeC(a, cantidad),
                                      ComplejosDesdeC(b, cantidad));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

size_t tinta_vector_producto_cruz(const TintaComplejo* a,
                                  const TintaComplejo* b,
                                  TintaComplejo* resultado, size_t capacidad,
                                  char* error, size_t tam_error) {
  const auto r = tinta::ProductoCruz(ComplejosDesdeC(a, 3),
                                     ComplejosDesdeC(b, 3));
  if (!r.correcto) { Copiar(r.error, error, tam_error); return 0; }
  CopiarComplejos(r.valores, resultado, capacidad);
  if (error && tam_error) error[0] = '\0';
  return r.valores.size();
}

bool tinta_matematica_buscar_raiz(const char* expresion, const char* variable,
                                  double estimacion,
                                  const TintaVariableCompleja* variables,
                                  size_t cantidad_variables,
                                  TintaComplejo* resultado,
                                  char* error, size_t tam_error) {
  if (!expresion || !variable) {
    Copiar("expresión o variable vacía", error, tam_error); return false;
  }
  const auto r = tinta::BuscarRaiz(
      expresion, variable, estimacion,
      VariablesDesdeC(variables, cantidad_variables));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

bool tinta_matematica_limite(const char* expresion, const char* variable,
                             double punto, int lado,
                             const TintaVariableCompleja* variables,
                             size_t cantidad_variables,
                             TintaComplejo* resultado,
                             char* error, size_t tam_error) {
  if (!expresion || !variable || lado < -1 || lado > 1) {
    Copiar("parámetros de límite inválidos", error, tam_error); return false;
  }
  const auto r = tinta::CalcularLimite(
      expresion, variable, punto, lado,
      VariablesDesdeC(variables, cantidad_variables));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

bool tinta_matematica_sumatoria(const char* expresion, const char* variable,
                                int64_t inicio, int64_t fin,
                                const TintaVariableCompleja* variables,
                                size_t cantidad_variables,
                                TintaComplejo* resultado,
                                char* error, size_t tam_error) {
  if (!expresion || !variable) {
    Copiar("expresión o variable vacía", error, tam_error); return false;
  }
  const auto r = tinta::CalcularSumatoria(
      expresion, variable, inicio, fin,
      VariablesDesdeC(variables, cantidad_variables));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

size_t tinta_matematica_fourier(const TintaComplejo* muestras, size_t cantidad,
                                bool inversa, TintaComplejo* resultado,
                                size_t capacidad) {
  const auto r = tinta::TransformadaFourierDiscreta(
      ComplejosDesdeC(muestras, cantidad), inversa);
  CopiarComplejos(r, resultado, capacidad);
  return r.size();
}

bool tinta_matematica_laplace(const char* expresion, const char* variable,
                              TintaComplejo s, double tiempo_maximo, int pasos,
                              const TintaVariableCompleja* variables,
                              size_t cantidad_variables,
                              TintaComplejo* resultado,
                              char* error, size_t tam_error) {
  if (!expresion || !variable) {
    Copiar("expresión o variable vacía", error, tam_error); return false;
  }
  const auto r = tinta::TransformadaLaplaceNumerica(
      expresion, variable, {s.real, s.imaginario}, tiempo_maximo, pasos,
      VariablesDesdeC(variables, cantidad_variables));
  return CopiarResultadoMatematico(r, resultado, error, tam_error);
}

size_t tinta_matematica_edo_primer_orden(
    const char* derivada, double t0, TintaComplejo y0, double t1,
    size_t pasos, const TintaVariableCompleja* variables,
    size_t cantidad_variables, TintaMuestraEDO* resultado, size_t capacidad,
    char* error, size_t tam_error) {
  if (!derivada) { Copiar("derivada vacía", error, tam_error); return 0; }
  const auto r = tinta::IntegrarEDOPrimerOrden(
      derivada, t0, {y0.real, y0.imaginario}, t1, pasos,
      VariablesDesdeC(variables, cantidad_variables));
  if (!r.correcto) { Copiar(r.error, error, tam_error); return 0; }
  const size_t n = std::min(capacidad, r.muestras.size());
  for (size_t i = 0; resultado && i < n; ++i) {
    resultado[i].t = r.muestras[i].t;
    resultado[i].y.real = r.muestras[i].y.real();
    resultado[i].y.imaginario = r.muestras[i].y.imag();
  }
  if (error && tam_error) error[0] = '\0';
  return r.muestras.size();
}

// --- Ingeniería electrónica ----------------------------------------------

int tinta_circuito_resolver_dc(const TintaComponenteElectrico* componentes,
                               size_t cantidad, double* voltajes,
                               size_t capacidad, char* error, size_t tam_error) {
  const auto r = tinta::ResolverDC(ComponentesDesdeC(componentes, cantidad));
  if (!r.correcto) { Copiar(r.error, error, tam_error); return -1; }
  const size_t n = std::min(capacidad, r.voltajes.size());
  if (voltajes) std::copy_n(r.voltajes.begin(), n, voltajes);
  if (error && tam_error) error[0] = '\0';
  return static_cast<int>(r.voltajes.size());
}

int tinta_circuito_resolver_ac(const TintaComponenteElectrico* componentes,
                               size_t cantidad, double frecuencia_hz,
                               TintaComplejo* voltajes, size_t capacidad,
                               char* error, size_t tam_error) {
  const auto r = tinta::ResolverAC(ComponentesDesdeC(componentes, cantidad),
                                   frecuencia_hz);
  if (!r.correcto) { Copiar(r.error, error, tam_error); return -1; }
  const size_t n = std::min(capacidad, r.voltajes.size());
  for (size_t i = 0; voltajes && i < n; ++i) {
    voltajes[i].real = r.voltajes[i].real();
    voltajes[i].imaginario = r.voltajes[i].imag();
  }
  if (error && tam_error) error[0] = '\0';
  return static_cast<int>(r.voltajes.size());
}

int tinta_circuito_barrido_dc(
    const TintaComponenteElectrico* componentes, size_t cantidad,
    int indice_fuente, double inicio, double fin, int puntos, int nodo,
    TintaMuestraCircuito* muestras, size_t capacidad,
    char* error, size_t tam_error) {
  auto circuito = ComponentesDesdeC(componentes, cantidad);
  if (indice_fuente < 0 ||
      indice_fuente >= static_cast<int>(circuito.size())) {
    Copiar("Índice de fuente fuera de rango", error, tam_error);
    return -1;
  }
  const auto tipo = circuito[indice_fuente].tipo;
  if (tipo != tinta::TipoComponenteElectrico::FuenteVoltaje &&
      tipo != tinta::TipoComponenteElectrico::FuenteCorriente) {
    Copiar("El barrido DC requiere una fuente independiente de voltaje o corriente",
           error, tam_error);
    return -1;
  }
  if (puntos < 2 || puntos > 100000 || nodo < 0 ||
      !std::isfinite(inicio) || !std::isfinite(fin)) {
    Copiar("Parámetros de barrido DC inválidos", error, tam_error);
    return -1;
  }
  for (int i = 0; i < puntos; ++i) {
    const double fraccion = static_cast<double>(i) / (puntos - 1);
    const double entrada = inicio + (fin - inicio) * fraccion;
    circuito[indice_fuente].valor = entrada;
    const auto r = tinta::ResolverDC(circuito);
    if (!r.correcto) {
      Copiar("Barrido DC en " + std::to_string(entrada) + ": " + r.error,
             error, tam_error);
      return -1;
    }
    if (nodo >= static_cast<int>(r.voltajes.size())) {
      Copiar("Nodo de salida fuera de rango", error, tam_error);
      return -1;
    }
    if (muestras && static_cast<size_t>(i) < capacidad) {
      muestras[i] = {entrada, r.voltajes[nodo], 0.0};
    }
  }
  if (error && tam_error) error[0] = '\0';
  return puntos;
}

int tinta_circuito_barrido_ac(
    const TintaComponenteElectrico* componentes, size_t cantidad,
    double inicio_hz, double fin_hz, int puntos_por_decada, int nodo,
    TintaMuestraCircuito* muestras, size_t capacidad,
    char* error, size_t tam_error) {
  if (!(inicio_hz > 0.0) || !(fin_hz > inicio_hz) ||
      puntos_por_decada < 1 || puntos_por_decada > 1000 || nodo < 0 ||
      !std::isfinite(inicio_hz) || !std::isfinite(fin_hz)) {
    Copiar("Parámetros de barrido AC inválidos", error, tam_error);
    return -1;
  }
  const double decadas = std::log10(fin_hz / inicio_hz);
  const double estimadas = std::ceil(decadas * puntos_por_decada) + 1.0;
  if (!std::isfinite(estimadas) || estimadas > 100000.0) {
    Copiar("El barrido AC excede 100000 muestras", error, tam_error);
    return -1;
  }
  const auto barrido = tinta::BarridoAC(
      ComponentesDesdeC(componentes, cantidad), inicio_hz, fin_hz,
      puntos_por_decada);
  for (size_t i = 0; i < barrido.size(); ++i) {
    const auto& punto = barrido[i];
    if (!punto.resultado.correcto) {
      Copiar("Barrido AC en " + std::to_string(punto.frecuencia_hz) +
                 " Hz: " + punto.resultado.error,
             error, tam_error);
      return -1;
    }
    if (nodo >= static_cast<int>(punto.resultado.voltajes.size())) {
      Copiar("Nodo de salida fuera de rango", error, tam_error);
      return -1;
    }
    if (muestras && i < capacidad) {
      const auto valor = punto.resultado.voltajes[nodo];
      muestras[i] = {punto.frecuencia_hz, valor.real(), valor.imag()};
    }
  }
  if (error && tam_error) error[0] = '\0';
  return static_cast<int>(barrido.size());
}

int tinta_circuito_transitorio(
    const TintaComponenteElectrico* componentes, size_t cantidad,
    double duracion_s, double paso_s, int nodo,
    TintaMuestraCircuito* muestras, size_t capacidad,
    char* error, size_t tam_error) {
  if (nodo < 0) {
    Copiar("Nodo de salida inválido", error, tam_error);
    return -1;
  }
  std::string motivo;
  const auto puntos = tinta::ResolverTransitorio(
      ComponentesDesdeC(componentes, cantidad), duracion_s, paso_s, &motivo);
  if (!motivo.empty()) {
    Copiar(motivo, error, tam_error);
    return -1;
  }
  if (puntos.size() > 2000000) {
    Copiar("El transitorio excede 2000000 muestras", error, tam_error);
    return -1;
  }
  for (size_t i = 0; i < puntos.size(); ++i) {
    if (nodo >= static_cast<int>(puntos[i].voltajes.size())) {
      Copiar("Nodo de salida fuera de rango", error, tam_error);
      return -1;
    }
    if (muestras && i < capacidad) {
      muestras[i] = {puntos[i].segundo, puntos[i].voltajes[nodo], 0.0};
    }
  }
  if (error && tam_error) error[0] = '\0';
  return static_cast<int>(puntos.size());
}

bool tinta_circuito_equivalente(const TintaComponenteElectrico* componentes,
                                size_t cantidad, int nodo_a, int nodo_b,
                                double* voltaje_thevenin,
                                double* resistencia_thevenin,
                                double* corriente_norton,
                                char* error, size_t tam_error) {
  const auto r = tinta::CalcularEquivalente(
      ComponentesDesdeC(componentes, cantidad), nodo_a, nodo_b);
  if (!r.correcto) { Copiar(r.error, error, tam_error); return false; }
  if (voltaje_thevenin) *voltaje_thevenin = r.voltaje_thevenin;
  if (resistencia_thevenin) *resistencia_thevenin = r.resistencia_thevenin;
  if (corriente_norton) *corriente_norton = r.corriente_norton;
  if (error && tam_error) error[0] = '\0';
  return true;
}

double tinta_toolbox_divisor(double entrada_v, double r_superior,
                             double r_inferior) {
  return tinta::DivisorVoltaje(entrada_v, r_superior, r_inferior);
}
double tinta_toolbox_opamp_no_inversor(double rf, double rg) {
  return tinta::GananciaOpAmpNoInversor(rf, rg);
}
double tinta_toolbox_opamp_inversor(double ri, double rf) {
  return tinta::GananciaOpAmpInversor(ri, rf);
}
double tinta_toolbox_corte_rc(double r, double c) { return tinta::FrecuenciaCorteRC(r, c); }
double tinta_toolbox_resonancia_lc(double l, double c) { return tinta::FrecuenciaResonanciaLC(l, c); }
double tinta_toolbox_constante_rc(double r, double c) { return tinta::ConstanteTiempoRC(r, c); }
double tinta_toolbox_a_db(double relacion, bool potencia) { return tinta::ADecibelios(relacion, potencia); }
double tinta_toolbox_desde_db(double db, bool potencia) { return tinta::DesdeDecibelios(db, potencia); }

bool tinta_circuito_reconocer_ultimos(const TintaSesion* sesion,
                                      size_t cantidad_trazos,
                                      TintaReconocimientoCircuito* resultado) {
  if (!sesion || !resultado || cantidad_trazos == 0 ||
      sesion->pagina.trazos.empty()) return false;
  cantidad_trazos = std::min(cantidad_trazos, sesion->pagina.trazos.size());
  const auto inicio = sesion->pagina.trazos.end()
      - static_cast<std::ptrdiff_t>(cantidad_trazos);
  return ExportarReconocimientoCircuito(
      std::vector<tinta::Trazo>(inicio, sesion->pagina.trazos.end()), resultado);
}

bool tinta_circuito_reconocer_seleccion(const TintaSesion* sesion,
                                        const size_t* indices,
                                        size_t cantidad_trazos,
                                        TintaReconocimientoCircuito* resultado) {
  if (!sesion || !indices || cantidad_trazos == 0 || !resultado) return false;
  const auto validos = IndicesValidosLimite(indices, cantidad_trazos,
                                             sesion->pagina.trazos.size());
  return ExportarReconocimientoCircuito(
      CopiarTrazos(sesion->pagina, validos), resultado);
}

size_t tinta_circuito_caracteristicas_ultimos(
    const TintaSesion* sesion, size_t cantidad_trazos,
    float* salida, size_t capacidad) {
  if (!sesion || cantidad_trazos == 0 || sesion->pagina.trazos.empty()) return 0;
  cantidad_trazos = std::min(cantidad_trazos, sesion->pagina.trazos.size());
  const auto inicio = sesion->pagina.trazos.end()
      - static_cast<std::ptrdiff_t>(cantidad_trazos);
  const auto f = tinta::ExtraerCaracteristicasCircuito(
      std::vector<tinta::Trazo>(inicio, sesion->pagina.trazos.end()));
  if (salida) std::copy_n(f.begin(), std::min(capacidad, f.size()), salida);
  return f.size();
}

size_t tinta_circuito_caracteristicas_seleccion(
    const TintaSesion* sesion, const size_t* indices, size_t cantidad_trazos,
    float* salida, size_t capacidad) {
  if (!sesion || !indices || cantidad_trazos == 0) return 0;
  const auto validos = IndicesValidosLimite(indices, cantidad_trazos,
                                             sesion->pagina.trazos.size());
  if (validos.empty()) return 0;
  const auto f = tinta::ExtraerCaracteristicasCircuito(
      CopiarTrazos(sesion->pagina, validos));
  if (salida) std::copy_n(f.begin(), std::min(capacidad, f.size()), salida);
  return f.size();
}

// --- Panel lateral ---------------------------------------------------------

namespace {

// Rellena por puntero y no devuelve la estructura: esto vive dentro de un
// bloque `extern "C"`, y devolver un tipo de C++ desde ahí es exactamente lo
// que avisa -Wreturn-type-c-linkage.
void ArmarPanel(tinta::AjustesPanel* a, bool visible, int lado, float fraccion,
                float min_pt, float max_fraccion,
                float ancho_minimo_para_dividir) {
  a->visible = visible;
  a->lado = lado == 1 ? tinta::LadoPanel::Izquierda : tinta::LadoPanel::Derecha;
  a->fraccion = fraccion;
  a->min_pt = min_pt;
  a->max_fraccion = max_fraccion;
  a->ancho_minimo_para_dividir = ancho_minimo_para_dividir;
}

}  // namespace

void tinta_panel_reparto(float ancho_total, bool visible, int lado,
                         float fraccion, float min_pt, float max_fraccion,
                         float ancho_minimo_para_dividir, float* origen_lienzo,
                         float* ancho_lienzo, float* origen_panel,
                         float* ancho_panel) {
  tinta::AjustesPanel a;
  ArmarPanel(&a, visible, lado, fraccion, min_pt, max_fraccion,
             ancho_minimo_para_dividir);
  if (origen_lienzo) *origen_lienzo = tinta::OrigenLienzo(ancho_total, a);
  if (ancho_lienzo) *ancho_lienzo = tinta::AnchoLienzo(ancho_total, a);
  if (origen_panel) *origen_panel = tinta::OrigenPanel(ancho_total, a);
  if (ancho_panel) *ancho_panel = tinta::AnchoPanel(ancho_total, a);
}

bool tinta_panel_cubre_todo(float ancho_total, float min_pt, float max_fraccion,
                            float ancho_minimo_para_dividir) {
  tinta::AjustesPanel a;
  ArmarPanel(&a, true, 0, 0.5f, min_pt, max_fraccion,
             ancho_minimo_para_dividir);
  return tinta::PanelCubreTodo(ancho_total, a);
}

float tinta_panel_fraccion_al_soltar(float fraccion_cruda, float max_fraccion) {
  tinta::AjustesPanel a;
  a.max_fraccion = max_fraccion;
  return tinta::FraccionAlSoltar(fraccion_cruda, a);
}

// --- Documentos y Split View -----------------------------------------------

TintaDocumentoId tinta_documento_abrir(void) {
  TintaDocumentoId id = g_siguiente_id++;
  g_documentos[id] = tinta_sesion_crear();
  return id;
}

void tinta_documento_cerrar(TintaDocumentoId id) {
  auto it = g_documentos.find(id);
  if (it != g_documentos.end()) {
    tinta_sesion_liberar(it->second);
    g_documentos.erase(it);
    
    // Limpiar viewports que apuntaban a este documento
    for (int i = 0; i < 2; ++i) {
      if (g_viewports[i].documento == id) {
        g_viewports[i].documento = 0;
      }
    }
  }
}

void tinta_almacen_configurar_ruta(const char* ruta_base) {
  if (ruta_base) {
    tinta::ConfigurarRutaAlmacen(ruta_base);
  }
}

int tinta_almacen_listar(struct TintaMetadatos* buffer, int capacidad) {
  std::vector<tinta::MetadatosCuaderno> lista = tinta::ListarCuadernos();
  int retornados = 0;
  for (const auto& meta : lista) {
    if (buffer && retornados < capacidad) {
      Copiar(meta.id, buffer[retornados].id, sizeof(buffer[retornados].id));
      Copiar(meta.titulo, buffer[retornados].titulo, sizeof(buffer[retornados].titulo));
      buffer[retornados].fecha_creacion = meta.fecha_creacion;
      buffer[retornados].fecha_modificacion = meta.fecha_modificacion;
    }
    retornados++;
  }
  return retornados;
}

bool tinta_almacen_guardar_sesion(TintaDocumentoId id) {
  TintaSesion* sesion = tinta_documento_sesion(id);
  if (!sesion) return false;

  tinta::Documento doc;
  doc.id = sesion->documento_id;
  doc.titulo = sesion->documento_titulo;
  
  tinta::Cuaderno cuaderno;
  tinta::Seccion seccion;
  seccion.paginas.push_back(sesion->pagina);
  cuaderno.secciones.push_back(std::move(seccion));
  doc.cuadernos.push_back(std::move(cuaderno));

  if (tinta::GuardarCuaderno(doc)) {
    sesion->documento_id = doc.id;
    sesion->documento_titulo = doc.titulo;
    return true;
  }
  return false;
}

TintaDocumentoId tinta_almacen_abrir_cuaderno(const char* id) {
  if (!id) return 0;
  tinta::Documento doc;
  if (!tinta::CargarCuaderno(id, &doc)) return 0;
  
  if (doc.cuadernos.empty() || doc.cuadernos[0].secciones.empty() || doc.cuadernos[0].secciones[0].paginas.empty()) {
    return 0;
  }

  TintaDocumentoId doc_id = tinta_documento_abrir();
  TintaSesion* sesion = tinta_documento_sesion(doc_id);
  if (sesion) {
    sesion->pagina = doc.cuadernos[0].secciones[0].paginas[0];
    sesion->documento_id = doc.id;
    sesion->documento_titulo = doc.titulo;
    sesion->historial.Limpiar();
    tinta_descartar_trazo(sesion);
  }
  return doc_id;
}

size_t tinta_documentos_abiertos(TintaDocumentoId* ids, size_t max_cantidad) {
  size_t i = 0;
  for (const auto& par : g_documentos) {
    if (ids && i < max_cantidad) {
      ids[i] = par.first;
    }
    ++i;
  }
  return g_documentos.size();
}

TintaSesion* tinta_documento_sesion(TintaDocumentoId id) {
  auto it = g_documentos.find(id);
  return it != g_documentos.end() ? it->second : nullptr;
}

void tinta_viewport_set_documento(TintaViewport viewport, TintaDocumentoId id) {
  if (static_cast<int>(viewport) >= 0 && static_cast<int>(viewport) < 2) {
    g_viewports[viewport].documento = id;
  }
}

TintaDocumentoId tinta_viewport_get_documento(TintaViewport viewport) {
  if (static_cast<int>(viewport) >= 0 && static_cast<int>(viewport) < 2) {
    return g_viewports[viewport].documento;
  }
  return 0;
}

void tinta_viewport_set_vista(TintaViewport viewport, float zoom, float pan_x, float pan_y) {
  if (static_cast<int>(viewport) >= 0 && static_cast<int>(viewport) < 2) {
    g_viewports[viewport].zoom = zoom;
    g_viewports[viewport].pan_x = pan_x;
    g_viewports[viewport].pan_y = pan_y;
  }
}

void tinta_viewport_get_vista(TintaViewport viewport, float* zoom, float* pan_x, float* pan_y) {
  if (static_cast<int>(viewport) >= 0 && static_cast<int>(viewport) < 2) {
    if (zoom) *zoom = g_viewports[viewport].zoom;
    if (pan_x) *pan_x = g_viewports[viewport].pan_x;
    if (pan_y) *pan_y = g_viewports[viewport].pan_y;
  }
}

}  // extern "C"
