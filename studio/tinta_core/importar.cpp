#include "importar.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tinta {
namespace {

constexpr float kPuntosPorPulgada = 72.0f;

std::string Identificador(const std::string& hash, int indice,
                          const char* sufijo) {
  return hash + "#" + std::to_string(indice) + sufijo;
}

}  // namespace

void TamanoRotado(const PaginaPdfFuente& fuente, float* ancho, float* alto) {
  float w = fuente.ancho_pt > 0.0f ? fuente.ancho_pt : 612.0f;
  float h = fuente.alto_pt > 0.0f ? fuente.alto_pt : 792.0f;
  // La rotación del PDF viene en grados y puede llegar negativa o mayor de 360.
  int giro = fuente.rotacion % 360;
  if (giro < 0) giro += 360;
  if (giro == 90 || giro == 270) std::swap(w, h);
  if (ancho) *ancho = w;
  if (alto) *alto = h;
}

OrientacionPagina OrientacionDe(const PaginaPdfFuente& fuente) {
  float w = 0.0f;
  float h = 0.0f;
  TamanoRotado(fuente, &w, &h);
  return w > h ? OrientacionPagina::Horizontal : OrientacionPagina::Vertical;
}

namespace {

// Tamaño del PDF en píxeles lógicos, sin margen de notas todavía.
void TamanoDelPdfEnPixeles(const PaginaPdfFuente& fuente,
                           const AjustesImportarPdf& ajustes, float* ancho,
                           float* alto) {
  float w_pt = 0.0f;
  float h_pt = 0.0f;
  TamanoRotado(fuente, &w_pt, &h_pt);

  const float ppp = ajustes.ppp > 0.0f ? ajustes.ppp : 150.0f;
  float w = w_pt * ppp / kPuntosPorPulgada;
  float h = h_pt * ppp / kPuntosPorPulgada;

  const float objetivo_ancho =
      ajustes.ancho_objetivo > 0.0f ? ajustes.ancho_objetivo : w;
  const float objetivo_alto =
      ajustes.alto_objetivo > 0.0f ? ajustes.alto_objetivo : h;

  switch (ajustes.ajuste) {
    case AjustePdf::TamanoReal:
      break;
    case AjustePdf::AnchoDelLienzo: {
      // El ancho manda y el alto lo sigue: deformar una diapositiva para que
      // quepa es peor que dejarla más larga.
      const float k = w > 0.0f ? objetivo_ancho / w : 1.0f;
      w *= k;
      h *= k;
      break;
    }
    case AjustePdf::Encajar: {
      const float kx = w > 0.0f ? objetivo_ancho / w : 1.0f;
      const float ky = h > 0.0f ? objetivo_alto / h : 1.0f;
      const float k = std::min(kx, ky);
      w *= k;
      h *= k;
      break;
    }
  }

  if (ancho) *ancho = std::max(w, 1.0f);
  if (alto) *alto = std::max(h, 1.0f);
}

float AnchoDelMargen(const AjustesImportarPdf& ajustes, float ancho_pdf) {
  if (!ajustes.margen_para_notas) return 0.0f;
  const float fraccion = std::clamp(ajustes.fraccion_margen, 0.0f, 3.0f);
  return ancho_pdf * fraccion;
}

}  // namespace

void TamanoDePagina(const PaginaPdfFuente& fuente,
                    const AjustesImportarPdf& ajustes, float* ancho,
                    float* alto) {
  float w = 0.0f;
  float h = 0.0f;
  TamanoDelPdfEnPixeles(fuente, ajustes, &w, &h);
  if (ancho) *ancho = w + AnchoDelMargen(ajustes, w);
  if (alto) *alto = h;
}

Rect MarcoDelPdf(const PaginaPdfFuente& fuente,
                 const AjustesImportarPdf& ajustes) {
  float w = 0.0f;
  float h = 0.0f;
  TamanoDelPdfEnPixeles(fuente, ajustes, &w, &h);

  Rect marco;
  marco.x = 0.0f;
  marco.y = 0.0f;
  marco.ancho = w;
  marco.alto = h;
  return marco;
}

Pagina PaginaDesdePdf(const PaginaPdfFuente& fuente, int indice,
                      const AjustesImportarPdf& ajustes,
                      const std::string& hash, const std::string& nombre) {
  Pagina pagina;
  float ancho = 0.0f;
  float alto = 0.0f;
  TamanoDePagina(fuente, ajustes, &ancho, &alto);

  pagina.titulo = nombre.empty()
                      ? ("Pagina " + std::to_string(indice + 1))
                      : (nombre + " - " + std::to_string(indice + 1));
  pagina.ancho = ancho;
  pagina.alto = alto;
  pagina.orientacion = OrientacionDe(fuente);
  pagina.pagina_pdf = indice;
  pagina.papel.tipo = ajustes.papel;
  pagina.papel.pauta = ajustes.pauta;

  // El PDF fija el tamaño de la hoja: no se puede ensanchar sin dejar la
  // diapositiva flotando en un lado. El alto sí se deja abierto si se pidió.
  pagina.limites.ancho_infinito = false;
  pagina.limites.ancho_minimo = ancho;
  pagina.limites.ancho_maximo = ancho;
  pagina.limites.alto_infinito = ajustes.alto_infinito;
  pagina.limites.alto_minimo = alto;
  pagina.limites.alto_maximo = ajustes.alto_infinito
      ? std::numeric_limits<float>::max() : alto;

  Medio medio;
  medio.id = Identificador(hash, indice, "");
  medio.hash = hash;
  medio.nombre_original = nombre;
  medio.tipo = TipoMedio::Pdf;
  medio.pagina_fuente = indice;
  medio.marco = MarcoDelPdf(fuente, ajustes);
  pagina.medios.push_back(medio);

  return pagina;
}

std::vector<Pagina> ImportarPdf(const std::vector<PaginaPdfFuente>& paginas,
                                const AjustesImportarPdf& ajustes,
                                const std::string& hash,
                                const std::string& nombre) {
  std::vector<Pagina> salida;
  salida.reserve(paginas.size());
  for (size_t i = 0; i < paginas.size(); ++i) {
    salida.push_back(
        PaginaDesdePdf(paginas[i], static_cast<int>(i), ajustes, hash, nombre));
  }
  return salida;
}

bool SoltarEnLienzoLibre(const Pagina& origen, Pagina* destino, float x, float y,
                         float escala) {
  if (!destino) return false;
  if (origen.pagina_pdf < 0) return false;

  const float k = escala > 0.0f ? escala : 1.0f;

  const Medio* fuente = nullptr;
  for (const Medio& m : origen.medios) {
    if (m.tipo == TipoMedio::Pdf) {
      fuente = &m;
      break;
    }
  }
  if (!fuente) return false;

  Medio copia = *fuente;
  copia.id = fuente->id + "@libre";
  copia.marco.x = x + fuente->marco.x * k;
  copia.marco.y = y + fuente->marco.y * k;
  copia.marco.ancho = fuente->marco.ancho * k;
  copia.marco.alto = fuente->marco.alto * k;
  destino->medios.push_back(copia);

  // La tinta escrita encima viaja con la diapositiva. Separarlas es perder la
  // anotación: las flechas dejarían de señalar nada.
  for (const Trazo& trazo : origen.trazos) {
    Trazo movido = trazo;
    movido.ancho_base = trazo.ancho_base * k;
    for (Muestra& m : movido.muestras) {
      m.posicion.x = x + m.posicion.x * k;
      m.posicion.y = y + m.posicion.y * k;
    }
    destino->trazos.push_back(movido);
  }

  for (const CuadroTexto& texto : origen.textos) {
    CuadroTexto movido = texto;
    movido.id = texto.id + "@libre";
    movido.marco.x = x + texto.marco.x * k;
    movido.marco.y = y + texto.marco.y * k;
    movido.marco.ancho = texto.marco.ancho * k;
    movido.marco.alto = texto.marco.alto * k;
    movido.estilo.tamano = texto.estilo.tamano * k;
    destino->textos.push_back(movido);
  }

  // Si el destino tiene un eje infinito, que crezca para que la diapositiva
  // quepa entera en vez de quedar cortada por el borde.
  destino->CrecerSiHaceFalta(48.0f);
  return true;
}

}  // namespace tinta
