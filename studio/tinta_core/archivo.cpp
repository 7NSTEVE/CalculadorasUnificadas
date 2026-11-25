#include "archivo.h"

#include <cstring>
#include <utility>

namespace tinta {
namespace {

constexpr char kMagia[6] = {'T', 'I', 'N', 'T', 'A', '\0'};
constexpr char kMagiaTrazo[6] = {'T', 'R', 'A', 'Z', 'O', '\0'};
const char* g_error = "";

// --- Escritura -------------------------------------------------------------

class Escritor {
 public:
  explicit Escritor(std::vector<uint8_t>* destino) : destino_(destino) {}

  void U8(uint8_t v) { destino_->push_back(v); }
  void U16(uint16_t v) { Crudo(&v, sizeof(v)); }
  void U32(uint32_t v) { Crudo(&v, sizeof(v)); }
  void I32(int32_t v) { Crudo(&v, sizeof(v)); }
  void I64(int64_t v) { Crudo(&v, sizeof(v)); }
  void F32(float v) { Crudo(&v, sizeof(v)); }
  void F64(double v) { Crudo(&v, sizeof(v)); }
  void Bool(bool v) { U8(v ? 1u : 0u); }

  void Texto(const std::string& s) {
    U32(static_cast<uint32_t>(s.size()));
    destino_->insert(destino_->end(), s.begin(), s.end());
  }

  void Punto2(const Punto& p) { F32(p.x); F32(p.y); }

  void Rectangulo(const Rect& r) {
    F32(r.x); F32(r.y); F32(r.ancho); F32(r.alto);
  }

 private:
  void Crudo(const void* datos, size_t bytes) {
    const auto* p = static_cast<const uint8_t*>(datos);
    destino_->insert(destino_->end(), p, p + bytes);
  }
  std::vector<uint8_t>* destino_;
};

// --- Lectura ---------------------------------------------------------------

// Todo lector puede quedarse sin datos: cada lectura comprueba que quepa. Un
// archivo truncado tiene que dar un error, no leer memoria de al lado.
class Lector {
 public:
  Lector(const uint8_t* datos, size_t tamano) : datos_(datos), tamano_(tamano) {}

  bool bien() const { return bien_; }
  size_t posicion() const { return pos_; }

  uint8_t U8() { uint8_t v = 0; Crudo(&v, sizeof(v)); return v; }
  uint16_t U16() { uint16_t v = 0; Crudo(&v, sizeof(v)); return v; }
  uint32_t U32() { uint32_t v = 0; Crudo(&v, sizeof(v)); return v; }
  int32_t I32() { int32_t v = 0; Crudo(&v, sizeof(v)); return v; }
  int64_t I64() { int64_t v = 0; Crudo(&v, sizeof(v)); return v; }
  float F32() { float v = 0.0f; Crudo(&v, sizeof(v)); return v; }
  double F64() { double v = 0.0; Crudo(&v, sizeof(v)); return v; }
  bool Bool() { return U8() != 0u; }

  std::string Texto() {
    const uint32_t largo = U32();
    if (!bien_ || pos_ + largo > tamano_) {
      bien_ = false;
      return {};
    }
    std::string s(reinterpret_cast<const char*>(datos_ + pos_), largo);
    pos_ += largo;
    return s;
  }

  Punto Punto2() { Punto p; p.x = F32(); p.y = F32(); return p; }

  Rect Rectangulo() {
    Rect r;
    r.x = F32(); r.y = F32(); r.ancho = F32(); r.alto = F32();
    return r;
  }

  // Lee un contador de elementos, rechazando los absurdos. Sin este tope, un
  // archivo corrupto que diga «4 mil millones de trazos» reserva memoria hasta
  // tumbar el proceso antes de que falle ninguna lectura.
  uint32_t Cuenta(uint32_t tope) {
    const uint32_t n = U32();
    if (n > tope) bien_ = false;
    return bien_ ? n : 0u;
  }

 private:
  void Crudo(void* destino, size_t bytes) {
    if (!bien_ || pos_ + bytes > tamano_) {
      bien_ = false;
      return;
    }
    std::memcpy(destino, datos_ + pos_, bytes);
    pos_ += bytes;
  }

  const uint8_t* datos_;
  size_t tamano_;
  size_t pos_ = 0;
  bool bien_ = true;
};

constexpr uint32_t kTopeElementos = 20'000'000u;

// --- Estructuras -----------------------------------------------------------

void EscribirEstilo(Escritor& e, const EstiloTexto& s) {
  e.Texto(s.familia);
  e.F32(s.tamano);
  e.Bool(s.negrita);
  e.Bool(s.cursiva);
  e.Bool(s.subrayado);
  e.Bool(s.tachado);
  e.U32(s.color_argb);
  e.U32(s.color_resaltado);
  e.U8(static_cast<uint8_t>(s.alineacion));
  e.F32(s.interlineado);
}

EstiloTexto LeerEstilo(Lector& l) {
  EstiloTexto s;
  s.familia = l.Texto();
  s.tamano = l.F32();
  s.negrita = l.Bool();
  s.cursiva = l.Bool();
  s.subrayado = l.Bool();
  s.tachado = l.Bool();
  s.color_argb = l.U32();
  s.color_resaltado = l.U32();
  s.alineacion = static_cast<Alineacion>(l.U8());
  s.interlineado = l.F32();
  return s;
}

void EscribirTrazo(Escritor& e, const Trazo& t) {
  e.Texto(t.id_colaboracion);
  e.F32(t.ancho_base);
  e.U32(t.color_argb);
  e.Bool(t.resaltador);
  e.Bool(t.estilo_persistente);
  if (t.estilo_persistente) {
    e.F32(t.estilo.opacidad);
    e.F32(t.estilo.exponente_presion);
    e.F32(t.estilo.factor_ancho_minimo);
    e.F32(t.estilo.velocidad_ancho_minimo);
    e.I32(t.estilo.suavizado_ancho);
    e.F32(t.estilo.estabilizacion);
    e.F32(t.estilo.taper_inicio);
    e.F32(t.estilo.taper_fin);
    e.F32(t.estilo.sensibilidad_inclinacion);
    e.I32(t.estilo.textura);
    e.I32(t.estilo.nivel_refinado);
  }
  e.U32(static_cast<uint32_t>(t.muestras.size()));
  for (const auto& m : t.muestras) {
    e.Punto2(m.posicion);
    e.F32(m.presion);
    e.F32(m.inclinacion_x);
    e.F32(m.inclinacion_y);
    e.I64(m.tiempo_qpc);
  }
}

Trazo LeerTrazo(Lector& l, uint16_t version) {
  Trazo t;
  if (version >= 4) t.id_colaboracion = l.Texto();
  t.ancho_base = l.F32();
  t.color_argb = l.U32();
  t.resaltador = l.Bool();
  if (version >= 3) {
    t.estilo_persistente = l.Bool();
    if (t.estilo_persistente) {
      t.estilo.opacidad = l.F32();
      t.estilo.exponente_presion = l.F32();
      t.estilo.factor_ancho_minimo = l.F32();
      t.estilo.velocidad_ancho_minimo = l.F32();
      t.estilo.suavizado_ancho = l.I32();
      t.estilo.estabilizacion = l.F32();
      t.estilo.taper_inicio = l.F32();
      t.estilo.taper_fin = l.F32();
      t.estilo.sensibilidad_inclinacion = l.F32();
      t.estilo.textura = l.I32();
      t.estilo.nivel_refinado = l.I32();
    }
  }
  const uint32_t n = l.Cuenta(kTopeElementos);
  if (!l.bien()) return t;
  t.muestras.reserve(n);
  for (uint32_t i = 0; i < n && l.bien(); ++i) {
    Muestra m;
    m.posicion = l.Punto2();
    m.presion = l.F32();
    m.inclinacion_x = l.F32();
    m.inclinacion_y = l.F32();
    m.tiempo_qpc = l.I64();
    t.muestras.push_back(m);
  }
  return t;
}

void EscribirNota(Escritor& e, const NotaFija& n) {
  e.Texto(n.id);
  e.Texto(n.texto);
  EscribirEstilo(e, n.estilo);
  e.Punto2(n.posicion);
  e.F32(n.ancho);
  e.F32(n.alto);
  e.U32(n.color_argb);
  e.U8(static_cast<uint8_t>(n.alcance));
  e.Bool(n.contraida);
  e.Bool(n.anclada);
  e.U32(static_cast<uint32_t>(n.trazos.size()));
  for (const auto& t : n.trazos) EscribirTrazo(e, t);
}

NotaFija LeerNota(Lector& l, uint16_t version) {
  NotaFija n;
  n.id = l.Texto();
  n.texto = l.Texto();
  n.estilo = LeerEstilo(l);
  n.posicion = l.Punto2();
  n.ancho = l.F32();
  n.alto = l.F32();
  n.color_argb = l.U32();
  n.alcance = static_cast<AlcanceNota>(l.U8());
  n.contraida = l.Bool();
  n.anclada = l.Bool();
  const uint32_t n_trazos = l.Cuenta(kTopeElementos);
  for (uint32_t i = 0; i < n_trazos && l.bien(); ++i)
    n.trazos.push_back(LeerTrazo(l, version));
  return n;
}

void EscribirMedio(Escritor& e, const Medio& m) {
  e.Texto(m.id);
  e.Texto(m.hash);
  e.Texto(m.nombre_original);
  e.U8(static_cast<uint8_t>(m.tipo));
  e.Rectangulo(m.marco);
  e.F32(m.rotacion);
  e.F32(m.opacidad);
  e.F32(m.recorte_x);
  e.F32(m.recorte_y);
  e.F32(m.recorte_ancho);
  e.F32(m.recorte_alto);
  e.I32(m.pagina_fuente);
  e.F64(m.segundo_inicio);
}

Medio LeerMedio(Lector& l) {
  Medio m;
  m.id = l.Texto();
  m.hash = l.Texto();
  m.nombre_original = l.Texto();
  m.tipo = static_cast<TipoMedio>(l.U8());
  m.marco = l.Rectangulo();
  m.rotacion = l.F32();
  m.opacidad = l.F32();
  m.recorte_x = l.F32();
  m.recorte_y = l.F32();
  m.recorte_ancho = l.F32();
  m.recorte_alto = l.F32();
  m.pagina_fuente = l.I32();
  m.segundo_inicio = l.F64();
  return m;
}

void EscribirPagina(Escritor& e, const Pagina& p) {
  e.Texto(p.titulo);
  e.U8(static_cast<uint8_t>(p.papel.tipo));
  e.U8(static_cast<uint8_t>(p.papel.pauta));
  e.F32(p.papel.separacion);
  e.F32(p.papel.margen_izquierdo);
  // Desde v2 el papel avanzado también forma parte del documento canónico.
  e.F32(p.papel.margen_derecho);
  e.F32(p.papel.margen_superior);
  e.F32(p.papel.margen_inferior);
  e.U32(p.papel.color_personalizado);
  e.U32(p.papel.color_pauta_personalizado);
  e.F32(p.papel.opacidad_pauta);
  e.F32(p.papel.grosor_pauta);
  e.F32(p.papel.separacion_secundaria);
  e.Bool(p.papel.snap_cuadricula);
  e.Bool(p.papel.mostrar_ejes);
  e.F32(p.papel.eje_x);
  e.F32(p.papel.eje_y);
  e.Bool(p.papel.pauta_al_exportar);

  e.Bool(p.limites.ancho_infinito);
  e.Bool(p.limites.alto_infinito);
  e.F32(p.limites.ancho_minimo);
  e.F32(p.limites.ancho_maximo);
  e.F32(p.limites.alto_minimo);
  e.F32(p.limites.alto_maximo);

  e.U8(static_cast<uint8_t>(p.orientacion));
  e.F32(p.ancho);
  e.F32(p.alto);
  e.I32(p.pagina_pdf);

  e.U32(static_cast<uint32_t>(p.trazos.size()));
  for (const auto& t : p.trazos) EscribirTrazo(e, t);

  e.U32(static_cast<uint32_t>(p.textos.size()));
  for (const auto& t : p.textos) {
    e.Texto(t.id);
    e.Texto(t.texto);
    e.Rectangulo(t.marco);
    EscribirEstilo(e, t.estilo);
    e.F32(t.rotacion);
    e.Bool(t.alto_automatico);
    e.Bool(t.desde_dictado);
  }

  e.U32(static_cast<uint32_t>(p.medios.size()));
  for (const auto& m : p.medios) EscribirMedio(e, m);

  e.U32(static_cast<uint32_t>(p.notas.size()));
  for (const auto& n : p.notas) EscribirNota(e, n);
}

Pagina LeerPagina(Lector& l, uint16_t version) {
  Pagina p;
  p.titulo = l.Texto();
  p.papel.tipo = static_cast<TipoPapel>(l.U8());
  p.papel.pauta = static_cast<Pauta>(l.U8());
  p.papel.separacion = l.F32();
  p.papel.margen_izquierdo = l.F32();
  if (version >= 2) {
    p.papel.margen_derecho = l.F32();
    p.papel.margen_superior = l.F32();
    p.papel.margen_inferior = l.F32();
    p.papel.color_personalizado = l.U32();
    p.papel.color_pauta_personalizado = l.U32();
    p.papel.opacidad_pauta = l.F32();
    p.papel.grosor_pauta = l.F32();
    p.papel.separacion_secundaria = l.F32();
    p.papel.snap_cuadricula = l.Bool();
    p.papel.mostrar_ejes = l.Bool();
    p.papel.eje_x = l.F32();
    p.papel.eje_y = l.F32();
    p.papel.pauta_al_exportar = l.Bool();
  }

  p.limites.ancho_infinito = l.Bool();
  p.limites.alto_infinito = l.Bool();
  p.limites.ancho_minimo = l.F32();
  p.limites.ancho_maximo = l.F32();
  p.limites.alto_minimo = l.F32();
  p.limites.alto_maximo = l.F32();

  p.orientacion = static_cast<OrientacionPagina>(l.U8());
  p.ancho = l.F32();
  p.alto = l.F32();
  p.pagina_pdf = l.I32();

  uint32_t n = l.Cuenta(kTopeElementos);
  for (uint32_t i = 0; i < n && l.bien(); ++i)
    p.trazos.push_back(LeerTrazo(l, version));

  n = l.Cuenta(kTopeElementos);
  for (uint32_t i = 0; i < n && l.bien(); ++i) {
    CuadroTexto t;
    t.id = l.Texto();
    t.texto = l.Texto();
    t.marco = l.Rectangulo();
    t.estilo = LeerEstilo(l);
    t.rotacion = l.F32();
    t.alto_automatico = l.Bool();
    t.desde_dictado = l.Bool();
    p.textos.push_back(t);
  }

  n = l.Cuenta(kTopeElementos);
  for (uint32_t i = 0; i < n && l.bien(); ++i) p.medios.push_back(LeerMedio(l));

  n = l.Cuenta(kTopeElementos);
  for (uint32_t i = 0; i < n && l.bien(); ++i)
    p.notas.push_back(LeerNota(l, version));

  return p;
}

}  // namespace

std::vector<uint8_t> SerializarTrazo(const Trazo& trazo) {
  std::vector<uint8_t> datos;
  Escritor e(&datos);
  for (char c : kMagiaTrazo) e.U8(static_cast<uint8_t>(c));
  e.U16(kVersionArchivo);
  EscribirTrazo(e, trazo);
  return datos;
}

bool DeserializarTrazo(const std::vector<uint8_t>& datos, Trazo* trazo) {
  g_error = "";
  if (!trazo || datos.size() < sizeof(kMagiaTrazo) + 2) {
    g_error = "trazo demasiado corto o destino nulo";
    return false;
  }
  if (std::memcmp(datos.data(), kMagiaTrazo, sizeof(kMagiaTrazo)) != 0) {
    g_error = "la cabecera no es de un trazo de tinta";
    return false;
  }
  Lector l(datos.data(), datos.size());
  for (size_t i = 0; i < sizeof(kMagiaTrazo); ++i) l.U8();
  const uint16_t version = l.U16();
  if (version == 0 || version > kVersionArchivo) {
    g_error = "version de trazo desconocida";
    return false;
  }
  Trazo leido = LeerTrazo(l, version);
  if (!l.bien() || l.posicion() != datos.size()) {
    g_error = "trazo truncado o con datos sobrantes";
    return false;
  }
  *trazo = std::move(leido);
  return true;
}

std::vector<uint8_t> Serializar(const Documento& documento) {
  g_error = "";
  // Desde v5 un agregado canónico sin identidad no es un archivo válido. No
  // se emite un v5 ambiguo que después pueda hacerse pasar por un legado v4.
  if (documento.id.empty()) {
    g_error = "el documento no tiene identidad durable";
    return {};
  }
  std::vector<uint8_t> datos;
  Escritor e(&datos);

  for (char c : kMagia) e.U8(static_cast<uint8_t>(c));
  e.U16(kVersionArchivo);

  // Desde v5 la identidad durable forma parte del agregado canónico. Antes
  // existía en `Documento`, pero se perdía al serializar y los sidecars del
  // iPad terminaban dependiendo del orden de apertura del proceso.
  e.Texto(documento.id);
  e.Texto(documento.titulo);

  e.U32(static_cast<uint32_t>(documento.notas.size()));
  for (const auto& n : documento.notas) EscribirNota(e, n);

  e.U32(static_cast<uint32_t>(documento.cuadernos.size()));
  for (const auto& cuaderno : documento.cuadernos) {
    e.Texto(cuaderno.titulo);

    e.U32(static_cast<uint32_t>(cuaderno.notas.size()));
    for (const auto& n : cuaderno.notas) EscribirNota(e, n);

    e.U32(static_cast<uint32_t>(cuaderno.secciones.size()));
    for (const auto& seccion : cuaderno.secciones) {
      e.Texto(seccion.titulo);
      e.U32(static_cast<uint32_t>(seccion.paginas.size()));
      for (const auto& pagina : seccion.paginas) EscribirPagina(e, pagina);
    }
  }

  return datos;
}

bool Deserializar(const std::vector<uint8_t>& datos, Documento* documento) {
  g_error = "";
  if (!documento) {
    g_error = "destino nulo";
    return false;
  }
  if (datos.size() < sizeof(kMagia) + 2) {
    g_error = "archivo demasiado corto para tener cabecera";
    return false;
  }
  if (std::memcmp(datos.data(), kMagia, sizeof(kMagia)) != 0) {
    g_error = "la cabecera no es de un archivo de tinta";
    return false;
  }

  Lector l(datos.data(), datos.size());
  for (size_t i = 0; i < sizeof(kMagia); ++i) l.U8();

  const uint16_t version = l.U16();
  if (version == 0 || version > kVersionArchivo) {
    g_error = "version de formato desconocida";
    return false;
  }

  // Se llena un documento aparte: si el archivo está truncado, el que ya tenía
  // el usuario abierto no se toca.
  Documento leido;
  if (version >= 5) {
    leido.id = l.Texto();
    if (!l.bien() || leido.id.empty()) {
      g_error = "el archivo v5 no contiene identidad durable";
      return false;
    }
  }
  leido.titulo = l.Texto();

  uint32_t n = l.Cuenta(kTopeElementos);
  for (uint32_t i = 0; i < n && l.bien(); ++i)
    leido.notas.push_back(LeerNota(l, version));

  const uint32_t cuadernos = l.Cuenta(kTopeElementos);
  for (uint32_t c = 0; c < cuadernos && l.bien(); ++c) {
    Cuaderno cuaderno;
    cuaderno.titulo = l.Texto();

    n = l.Cuenta(kTopeElementos);
    for (uint32_t i = 0; i < n && l.bien(); ++i)
      cuaderno.notas.push_back(LeerNota(l, version));

    const uint32_t secciones = l.Cuenta(kTopeElementos);
    for (uint32_t s = 0; s < secciones && l.bien(); ++s) {
      Seccion seccion;
      seccion.titulo = l.Texto();
      const uint32_t paginas = l.Cuenta(kTopeElementos);
      for (uint32_t p = 0; p < paginas && l.bien(); ++p) {
        seccion.paginas.push_back(LeerPagina(l, version));
      }
      cuaderno.secciones.push_back(std::move(seccion));
    }
    leido.cuadernos.push_back(std::move(cuaderno));
  }

  if (!l.bien()) {
    g_error = "los datos se acaban antes de tiempo o traen cuentas imposibles";
    return false;
  }

  *documento = std::move(leido);
  return true;
}

const char* UltimoErrorArchivo() { return g_error; }

void EstablecerErrorArchivo(const char* mensaje) {
  g_error = mensaje ? mensaje : "";
}

}  // namespace tinta
