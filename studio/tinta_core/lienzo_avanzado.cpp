#include "lienzo_avanzado.h"

#include <algorithm>
#include <cmath>

namespace tinta {
namespace {

constexpr float kMilimetrosPorPulgada = 25.4f;

Rect CajaTrazo(const Trazo& trazo) {
  if (trazo.muestras.empty()) return {};
  const float radio = std::max(0.0f, trazo.ancho_base * 0.5f);
  float x0 = trazo.muestras.front().posicion.x - radio;
  float y0 = trazo.muestras.front().posicion.y - radio;
  float x1 = trazo.muestras.front().posicion.x + radio;
  float y1 = trazo.muestras.front().posicion.y + radio;
  for (const Muestra& m : trazo.muestras) {
    x0 = std::min(x0, m.posicion.x - radio);
    y0 = std::min(y0, m.posicion.y - radio);
    x1 = std::max(x1, m.posicion.x + radio);
    y1 = std::max(y1, m.posicion.y + radio);
  }
  return {x0, y0, x1 - x0, y1 - y0};
}

void EscalarTrazo(Trazo* trazo, float sx, float sy) {
  if (!trazo) return;
  for (Muestra& m : trazo->muestras) {
    m.posicion.x *= sx;
    m.posicion.y *= sy;
  }
  // El grosor conserva proporción visual sin quedar elíptico.
  trazo->ancho_base *= std::sqrt(std::max(0.0f, sx * sy));
}

void EscalarRect(Rect* r, float sx, float sy) {
  if (!r) return;
  r->x *= sx;
  r->y *= sy;
  r->ancho *= sx;
  r->alto *= sy;
}

void TrasladarPagina(Pagina* pagina, float dx, float dy) {
  if (!pagina || (dx == 0.0f && dy == 0.0f)) return;
  for (Trazo& trazo : pagina->trazos) {
    for (Muestra& muestra : trazo.muestras) {
      muestra.posicion.x += dx;
      muestra.posicion.y += dy;
    }
  }
  for (CuadroTexto& texto : pagina->textos) {
    texto.marco.x += dx;
    texto.marco.y += dy;
  }
  for (Medio& medio : pagina->medios) {
    medio.marco.x += dx;
    medio.marco.y += dy;
  }
  for (NotaFija& nota : pagina->notas) {
    nota.posicion.x += dx;
    nota.posicion.y += dy;
    for (Trazo& trazo : nota.trazos) {
      for (Muestra& muestra : trazo.muestras) {
        muestra.posicion.x += dx;
        muestra.posicion.y += dy;
      }
    }
  }
}

std::vector<Rect> CajasContenido(const Pagina& pagina) {
  std::vector<Rect> cajas;
  cajas.reserve(pagina.trazos.size() + pagina.textos.size() +
                pagina.medios.size());
  for (const Trazo& trazo : pagina.trazos) {
    Rect caja = CajaTrazo(trazo);
    if (!caja.vacio()) cajas.push_back(caja);
  }
  for (const CuadroTexto& texto : pagina.textos) {
    if (!texto.marco.vacio()) cajas.push_back(texto.marco);
  }
  for (const Medio& medio : pagina.medios) {
    if (!medio.marco.vacio()) cajas.push_back(medio.marco);
  }
  return cajas;
}

}  // namespace

float ALongitudLienzo(float valor, UnidadLienzo unidad, float ppp) {
  if (!std::isfinite(valor) || !std::isfinite(ppp) || ppp <= 0.0f) return 0.0f;
  switch (unidad) {
    case UnidadLienzo::Pixeles: return valor;
    case UnidadLienzo::Milimetros: return valor * ppp / kMilimetrosPorPulgada;
    case UnidadLienzo::Centimetros:
      return valor * 10.0f * ppp / kMilimetrosPorPulgada;
    case UnidadLienzo::Pulgadas: return valor * ppp;
  }
  return valor;
}

float DesdeLongitudLienzo(float pixeles, UnidadLienzo unidad, float ppp) {
  if (!std::isfinite(pixeles) || !std::isfinite(ppp) || ppp <= 0.0f) return 0.0f;
  switch (unidad) {
    case UnidadLienzo::Pixeles: return pixeles;
    case UnidadLienzo::Milimetros: return pixeles * kMilimetrosPorPulgada / ppp;
    case UnidadLienzo::Centimetros:
      return pixeles * kMilimetrosPorPulgada / (ppp * 10.0f);
    case UnidadLienzo::Pulgadas: return pixeles / ppp;
  }
  return pixeles;
}

void AplicarModoPagina(Pagina* pagina, ModoPagina modo) {
  if (!pagina) return;
  switch (modo) {
    case ModoPagina::Libro:
    case ModoPagina::Goodnotes:
      pagina->limites.ancho_infinito = false;
      pagina->limites.alto_infinito = false;
      break;
    case ModoPagina::Hibrido:
    case ModoPagina::Presentacion:
      pagina->limites.ancho_infinito = false;
      pagina->limites.alto_infinito = true;
      break;
    case ModoPagina::OneNote:
    case ModoPagina::Pizarra:
      pagina->limites.ancho_infinito = true;
      pagina->limites.alto_infinito = true;
      break;
    case ModoPagina::HorizontalInfinito:
      pagina->limites.ancho_infinito = true;
      pagina->limites.alto_infinito = false;
      break;
  }
}

bool CambiarTamanoAvanzado(Pagina* pagina, float ancho, float alto,
                           ReacomodoPagina reacomodo) {
  if (!pagina || !std::isfinite(ancho) || !std::isfinite(alto) ||
      ancho <= 0.0f || alto <= 0.0f) {
    return false;
  }
  const float previo_ancho = pagina->ancho;
  const float previo_alto = pagina->alto;
  if (reacomodo == ReacomodoPagina::EscalarProporcional &&
      previo_ancho > 0.0f && previo_alto > 0.0f) {
    const float sx = ancho / previo_ancho;
    const float sy = alto / previo_alto;
    for (Trazo& trazo : pagina->trazos) EscalarTrazo(&trazo, sx, sy);
    for (CuadroTexto& texto : pagina->textos) EscalarRect(&texto.marco, sx, sy);
    for (Medio& medio : pagina->medios) EscalarRect(&medio.marco, sx, sy);
    for (NotaFija& nota : pagina->notas) {
      nota.posicion.x *= sx;
      nota.posicion.y *= sy;
      nota.ancho *= sx;
      nota.alto *= sy;
      for (Trazo& trazo : nota.trazos) EscalarTrazo(&trazo, sx, sy);
    }
  }
  pagina->ancho = ancho;
  pagina->alto = alto;
  if (!pagina->limites.ancho_infinito) pagina->limites.ancho_maximo = ancho;
  if (!pagina->limites.alto_infinito) pagina->limites.alto_maximo = alto;
  return previo_ancho != ancho || previo_alto != alto;
}

bool InsertarEspacio(Pagina* pagina, EjeEspacio eje, float desde,
                     float cantidad) {
  if (!pagina || !std::isfinite(desde) || !std::isfinite(cantidad) ||
      cantidad == 0.0f) {
    return false;
  }
  bool cambio = false;
  const auto mover_punto = [&](Punto* p) {
    if (!p) return;
    float& valor = eje == EjeEspacio::Horizontal ? p->x : p->y;
    if (valor >= desde) {
      valor += cantidad;
      cambio = true;
    }
  };
  for (Trazo& trazo : pagina->trazos) {
    for (Muestra& m : trazo.muestras) mover_punto(&m.posicion);
  }
  for (CuadroTexto& texto : pagina->textos) {
    float& valor = eje == EjeEspacio::Horizontal ? texto.marco.x : texto.marco.y;
    if (valor >= desde) { valor += cantidad; cambio = true; }
  }
  for (Medio& medio : pagina->medios) {
    float& valor = eje == EjeEspacio::Horizontal ? medio.marco.x : medio.marco.y;
    if (valor >= desde) { valor += cantidad; cambio = true; }
  }
  for (NotaFija& nota : pagina->notas) mover_punto(&nota.posicion);

  if (cambio && cantidad > 0.0f) {
    if (eje == EjeEspacio::Horizontal) pagina->ancho += cantidad;
    else pagina->alto += cantidad;
  }
  return cambio;
}

bool MoverBorde(Pagina* pagina, BordePagina borde, float delta,
                bool desplazar_contenido) {
  if (!pagina || !std::isfinite(delta) || delta == 0.0f) return false;
  constexpr float kMinimo = 64.0f;
  float aplicado = delta;
  switch (borde) {
    case BordePagina::Izquierdo:
      aplicado = std::min(aplicado, pagina->ancho - kMinimo);
      pagina->ancho -= aplicado;
      pagina->limites.ancho_infinito = false;
      pagina->limites.ancho_maximo = pagina->ancho;
      if (desplazar_contenido) TrasladarPagina(pagina, -aplicado, 0.0f);
      break;
    case BordePagina::Derecho:
      aplicado = std::max(aplicado, kMinimo - pagina->ancho);
      pagina->ancho += aplicado;
      pagina->limites.ancho_infinito = false;
      pagina->limites.ancho_maximo = pagina->ancho;
      break;
    case BordePagina::Superior:
      aplicado = std::min(aplicado, pagina->alto - kMinimo);
      pagina->alto -= aplicado;
      pagina->limites.alto_infinito = false;
      pagina->limites.alto_maximo = pagina->alto;
      if (desplazar_contenido) TrasladarPagina(pagina, 0.0f, -aplicado);
      break;
    case BordePagina::Inferior:
      aplicado = std::max(aplicado, kMinimo - pagina->alto);
      pagina->alto += aplicado;
      pagina->limites.alto_infinito = false;
      pagina->limites.alto_maximo = pagina->alto;
      break;
  }
  return aplicado != 0.0f;
}

std::vector<float> CortesDeImpresion(const Pagina& pagina, float alto_hoja,
                                     float margen_seguro) {
  std::vector<float> cortes;
  if (alto_hoja <= 0.0f || pagina.alto <= alto_hoja) return cortes;
  const std::vector<Rect> cajas = CajasContenido(pagina);
  float corte = alto_hoja;
  while (corte < pagina.alto) {
    float ajustado = corte;
    for (const Rect& caja : cajas) {
      if (caja.y - margen_seguro < ajustado &&
          caja.abajo() + margen_seguro > ajustado) {
        const float antes = caja.y - margen_seguro;
        const float despues = caja.abajo() + margen_seguro;
        ajustado = antes > (cortes.empty() ? 0.0f : cortes.back())
                       ? antes
                       : despues;
      }
    }
    if (!cortes.empty() && ajustado <= cortes.back()) ajustado = corte;
    cortes.push_back(std::min(ajustado, pagina.alto));
    corte = cortes.back() + alto_hoja;
  }
  if (!cortes.empty() && cortes.back() >= pagina.alto) cortes.pop_back();
  return cortes;
}

}  // namespace tinta
