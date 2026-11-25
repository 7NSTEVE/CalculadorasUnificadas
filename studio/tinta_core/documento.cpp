#include "documento.h"

#include <algorithm>
#include <cmath>

namespace tinta {

// ---------------------------------------------------------------------------
// Lienzo
// ---------------------------------------------------------------------------

bool LimitesLienzo::valido() const {
  if (ancho_minimo <= 0.0f || alto_minimo <= 0.0f) return false;
  if (!ancho_infinito && ancho_maximo < ancho_minimo) return false;
  if (!alto_infinito && alto_maximo < alto_minimo) return false;
  return true;
}

void AplicarLimites(const LimitesLienzo& limites, float* ancho, float* alto) {
  if (ancho) {
    *ancho = std::max(*ancho, limites.ancho_minimo);
    if (!limites.ancho_infinito) {
      *ancho = std::min(*ancho, limites.ancho_maximo);
    }
  }
  if (alto) {
    *alto = std::max(*alto, limites.alto_minimo);
    if (!limites.alto_infinito) {
      *alto = std::min(*alto, limites.alto_maximo);
    }
  }
}

bool ConfinarPunto(const LimitesLienzo& limites, float ancho, float alto,
                   Punto* punto) {
  if (!punto) return false;
  const Punto original = *punto;

  punto->x = std::max(0.0f, punto->x);
  punto->y = std::max(0.0f, punto->y);
  // En un eje infinito no hay borde derecho o inferior contra el que chocar.
  if (!limites.ancho_infinito) punto->x = std::min(punto->x, ancho);
  if (!limites.alto_infinito) punto->y = std::min(punto->y, alto);

  return punto->x != original.x || punto->y != original.y;
}

// ---------------------------------------------------------------------------
// Papel
// ---------------------------------------------------------------------------

uint32_t Papel::color_fondo() const {
  if (color_personalizado != 0) return color_personalizado;
  switch (tipo) {
    case TipoPapel::Blanco: return 0xFFFDFDFBu;
    case TipoPapel::BlancoSol: return 0xFFFBF6EAu;
    case TipoPapel::Crema: return 0xFFF5EBD8u;
    case TipoPapel::Gris: return 0xFF3A3A3Cu;
    case TipoPapel::Oscuro: return 0xFF1C1C1Eu;
    case TipoPapel::Negro: return 0xFF000000u;
  }
  return 0xFFFDFDFBu;
}

bool Papel::es_oscuro() const {
  return tipo == TipoPapel::Gris || tipo == TipoPapel::Oscuro ||
         tipo == TipoPapel::Negro;
}

uint32_t Papel::color_pauta() const {
  if (color_pauta_personalizado != 0) return color_pauta_personalizado;
  // Sobre papel oscuro la pauta va más clara que el fondo, no más oscura.
  uint32_t c = es_oscuro() ? 0x33FFFFFFu : 0x22203040u;
  uint8_t a = static_cast<uint8_t>(((c >> 24) & 0xFF) * opacidad_pauta);
  return (c & 0x00FFFFFFu) | (static_cast<uint32_t>(a) << 24);
}

uint32_t Papel::color_tinta_por_defecto() const {
  // Escribir en negro sobre papel negro es el error más fácil de cometer aquí.
  return es_oscuro() ? 0xFFF2F2F7u : 0xFF1A1A1Au;
}

std::vector<LineaGuia> GenerarPauta(const Papel& papel, float x, float y,
                                    float ancho, float alto) {
  std::vector<LineaGuia> lineas;
  if (papel.pauta == Pauta::Ninguna || papel.separacion <= 0.0f) return lineas;
  if (ancho <= 0.0f || alto <= 0.0f) return lineas;

  const float paso = papel.separacion;
  const float derecha = x + ancho;
  const float abajo = y + alto;

  // Se empieza en el primer múltiplo del paso anterior al rectángulo visible,
  // no en el borde: así la pauta no se desplaza al desplazarse la vista.
  const float primer_x = std::floor(x / paso) * paso;
  const float primer_y = std::floor(y / paso) * paso;

  const auto horizontales = [&]() {
    for (float py = primer_y; py <= abajo; py += paso) {
      if (py < y) continue;
      lineas.push_back({{x, py}, {derecha, py}, false});
    }
  };
  const auto verticales = [&]() {
    for (float px = primer_x; px <= derecha; px += paso) {
      if (px < x) continue;
      lineas.push_back({{px, y}, {px, abajo}, false});
    }
  };

  switch (papel.pauta) {
    case Pauta::Rayado:
      horizontales();
      break;
    case Pauta::Cuadricula:
      horizontales();
      verticales();
      break;
    case Pauta::Puntos:
      // Un punto es un segmento de longitud cero; la capa de dibujo lo pinta
      // como un disco del grosor de la pauta.
      for (float py = primer_y; py <= abajo; py += paso) {
        if (py < y) continue;
        for (float px = primer_x; px <= derecha; px += paso) {
          if (px < x) continue;
          lineas.push_back({{px, py}, {px, py}, false});
        }
      }
      break;
    case Pauta::Pentagrama: {
      // Grupos de cinco líneas con el doble de separación entre grupos.
      const float alto_grupo = paso * 4.0f;
      const float salto = alto_grupo + paso * 3.0f;
      float base = std::floor(y / salto) * salto;
      for (; base <= abajo; base += salto) {
        for (int i = 0; i < 5; ++i) {
          const float py = base + i * paso;
          if (py < y || py > abajo) continue;
          lineas.push_back({{x, py}, {derecha, py}, false});
        }
      }
      break;
    }
    case Pauta::Milimetrico:
    case Pauta::Ingenieria: {
      const float espaciado = (papel.pauta == Pauta::Milimetrico) ? (1.0f * 150.0f / 25.4f) : (1.0f * 150.0f / 25.4f);
      const float p_x = std::floor(x / espaciado) * espaciado;
      const float p_y = std::floor(y / espaciado) * espaciado;
      int i_y = static_cast<int>(std::floor(y / espaciado));
      for (float py = p_y; py <= abajo; py += espaciado, i_y++) {
        if (py < y) continue;
        lineas.push_back({{x, py}, {derecha, py}, (i_y % 5 == 0)});
      }
      int i_x = static_cast<int>(std::floor(x / espaciado));
      for (float px = p_x; px <= derecha; px += espaciado, i_x++) {
        if (px < x) continue;
        lineas.push_back({{px, y}, {px, abajo}, (i_x % 5 == 0)});
      }
      break;
    }
    case Pauta::Isometrico: {
      // Lineas horizontales y a 60 y -60 grados.
      const float h_paso = paso / std::sin(3.14159265f / 3.0f);
      const float v_paso = paso;
      // 0 degrees
      for (float py = primer_y; py <= abajo; py += v_paso) {
        if (py < y) continue;
        lineas.push_back({{x, py}, {derecha, py}, false});
      }
      // 60 and -60 degrees
      // We need to cover the bounding box
      const float max_r = std::max(derecha, abajo) * 2.0f;
      const float min_r = -max_r;
      for (float r = min_r; r <= max_r; r += h_paso) {
        // x * cos(60) + y * sin(60) = r
        // x * 0.5 + y * sqrt(3)/2 = r => y = (r - x * 0.5) / (sqrt(3)/2)
        // x * cos(-60) + y * sin(-60) = r
        // x * 0.5 - y * sqrt(3)/2 = r => y = (x * 0.5 - r) / (sqrt(3)/2)
      }
      // A simpler way is to draw lines y = sqrt(3)*x + c and y = -sqrt(3)*x + c
      const float m = std::sqrt(3.0f);
      const float intercept_paso = paso / std::cos(3.14159265f / 6.0f);
      const float min_c = y - m * derecha - 100.0f;
      const float max_c = abajo + m * x + 100.0f;
      float first_c1 = std::floor(min_c / intercept_paso) * intercept_paso;
      for (float c = first_c1; c <= max_c; c += intercept_paso) {
        float x1 = x, y1 = m * x1 + c;
        float x2 = derecha, y2 = m * x2 + c;
        lineas.push_back({{x1, y1}, {x2, y2}, false});
      }
      const float min_c2 = y + m * x - 100.0f;
      const float max_c2 = abajo + m * derecha + 100.0f;
      float first_c2 = std::floor(min_c2 / intercept_paso) * intercept_paso;
      for (float c = first_c2; c <= max_c2; c += intercept_paso) {
        float x1 = x, y1 = -m * x1 + c;
        float x2 = derecha, y2 = -m * x2 + c;
        lineas.push_back({{x1, y1}, {x2, y2}, false});
      }
      break;
    }
    case Pauta::Cornell: {
      horizontales();
      const float cornell_x = 2.5f * 150.0f;
      if (cornell_x >= x && cornell_x <= derecha) {
        lineas.push_back({{cornell_x, y}, {cornell_x, abajo}, true});
      }
      float c_y = abajo - 300.0f;
      if (c_y >= y && c_y <= abajo) {
        lineas.push_back({{x, c_y}, {derecha, c_y}, true});
      }
      break;
    }
    case Pauta::Logaritmico:
    case Pauta::SemiLogaritmico: {
      if (papel.pauta == Pauta::Logaritmico) horizontales(); // Wait, log vertical and log horizontal? Prompt says: Logaritmico: Vertical lines at log spacing across page width. Regular horizontal lines. So both have regular horizontal.
      else horizontales();
      
      const float decada = paso * 10.0f;
      const float log_primer_x = std::floor(x / decada) * decada;
      for (float px_base = log_primer_x; px_base <= derecha; px_base += decada) {
        for (int i = 1; i <= 9; ++i) {
          float px = px_base + decada * std::log10(static_cast<float>(i));
          if (px >= x && px <= derecha) {
            lineas.push_back({{px, y}, {px, abajo}, (i == 1)});
          }
        }
      }
      break;
    }
    case Pauta::Polar: {
      const float cx = (derecha + x) * 0.5f;
      const float cy = (abajo + y) * 0.5f;
      const float max_rad = std::sqrt(ancho * ancho + alto * alto);
      for (float r = paso; r <= max_rad; r += paso) {
        // We can approximate circles with line segments if needed, or just let the drawer do it. 
        // The type LineaGuia represents straight lines. A circle can be drawn as a polygon.
        const int segmentos = 64;
        for (int i = 0; i < segmentos; ++i) {
          float a1 = i * 2.0f * 3.14159265f / segmentos;
          float a2 = (i + 1) * 2.0f * 3.14159265f / segmentos;
          lineas.push_back({{cx + r * std::cos(a1), cy + r * std::sin(a1)},
                            {cx + r * std::cos(a2), cy + r * std::sin(a2)}, false});
        }
      }
      for (int i = 0; i < 12; ++i) {
        float a = i * 30.0f * 3.14159265f / 180.0f;
        lineas.push_back({{cx, cy}, {cx + max_rad * std::cos(a), cy + max_rad * std::sin(a)}, false});
      }
      break;
    }
    case Pauta::Circuitos: {
      const float c_paso = 5.0f * 150.0f / 25.4f; // 5mm
      const float c_px = std::floor(x / c_paso) * c_paso;
      const float c_py = std::floor(y / c_paso) * c_paso;
      for (float py = c_py; py <= abajo; py += c_paso) {
        if (py < y) continue;
        lineas.push_back({{x, py}, {derecha, py}, false});
      }
      for (float px = c_px; px <= derecha; px += c_paso) {
        if (px < x) continue;
        lineas.push_back({{px, y}, {px, abajo}, false});
      }
      break;
    }
    case Pauta::Cartesiano: {
      horizontales();
      verticales();
      if (papel.mostrar_ejes || papel.eje_x != 0.0f || papel.eje_y != 0.0f) {
        if (papel.eje_x >= x && papel.eje_x <= derecha) {
          lineas.push_back({{papel.eje_x, y}, {papel.eje_x, abajo}, true});
        }
        if (papel.eje_y >= y && papel.eje_y <= abajo) {
          lineas.push_back({{x, papel.eje_y}, {derecha, papel.eje_y}, true});
        }
      }
      break;
    }
    case Pauta::Smith: {
      const float cx = x + ancho * 0.5f;
      const float cy = y + alto * 0.5f;
      const float radio = std::max(1.0f, std::min(ancho, alto) * 0.46f);
      const auto circulo = [&](float ox, float oy, float r, bool destacada) {
        constexpr int segmentos = 96;
        for (int i = 0; i < segmentos; ++i) {
          const float a = i * 2.0f * 3.14159265f / segmentos;
          const float b = (i + 1) * 2.0f * 3.14159265f / segmentos;
          lineas.push_back({{ox + r * std::cos(a), oy + r * std::sin(a)},
                            {ox + r * std::cos(b), oy + r * std::sin(b)},
                            destacada});
        }
      };
      circulo(cx, cy, radio, true);
      lineas.push_back({{cx - radio, cy}, {cx + radio, cy}, true});
      for (float normalizada : {0.2f, 0.5f, 1.0f, 2.0f, 5.0f}) {
        const float r = radio / (1.0f + normalizada);
        const float ox = cx + radio - r;
        circulo(ox, cy, r, false);
        const float rr = radio / std::max(0.2f, normalizada);
        circulo(cx + radio, cy + rr, rr, false);
        circulo(cx + radio, cy - rr, rr, false);
      }
      break;
    }
    case Pauta::Hexagonal: {
      const float m = std::sqrt(3.0f);
      verticales();
      const float salto = std::max(1.0f, paso * m);
      const float min_c = y - m * derecha - paso;
      const float max_c = abajo + m * derecha + paso;
      for (float c = std::floor(min_c / salto) * salto; c <= max_c; c += salto) {
        lineas.push_back({{x, m * x + c}, {derecha, m * derecha + c}, false});
        lineas.push_back({{x, -m * x + c}, {derecha, -m * derecha + c}, false});
      }
      break;
    }
    case Pauta::Tabla:
    case Pauta::Personalizada:
      horizontales();
      verticales();
      break;
    case Pauta::Ninguna:
      break;
  }

  if (papel.margen_izquierdo > 0.0f) {
    const float px = papel.margen_izquierdo;
    if (px >= x && px <= derecha) {
      lineas.push_back({{px, y}, {px, abajo}, true});
    }
  }

  return lineas;
}

// ---------------------------------------------------------------------------
// Página
// ---------------------------------------------------------------------------

DimensionesPagina DimensionesDeTamano(TamanoPagina tamano) {
  const auto px = [](float mm) { return std::round(mm * 150.0f / 25.4f); };
  switch (tamano) {
    case TamanoPagina::Personalizado: return {0.0f, 0.0f};
    case TamanoPagina::A4: return {1240.0f, 1754.0f};
    case TamanoPagina::Carta: return {px(216.0f), px(279.0f)};
    case TamanoPagina::Oficio: return {px(216.0f), px(356.0f)};
    case TamanoPagina::A3: return {px(297.0f), px(420.0f)};
    case TamanoPagina::A5: return {px(148.0f), px(210.0f)};
    case TamanoPagina::Dieciseis9: return {1240.0f, std::round(1240.0f * 9.0f / 16.0f)};
    case TamanoPagina::Dieciseis10: return {1240.0f, std::round(1240.0f * 10.0f / 16.0f)};
    case TamanoPagina::Cuatro3: return {1240.0f, std::round(1240.0f * 3.0f / 4.0f)};
    case TamanoPagina::CuadernoIngenieria: return {1400.0f, 1754.0f};
  }
  return {0.0f, 0.0f};
}

Pagina CrearPagina(TamanoPagina tamano, OrientacionPagina orientacion) {
  Pagina p;
  p.orientacion = orientacion;
  if (tamano != TamanoPagina::Personalizado) {
    DimensionesPagina dim = DimensionesDeTamano(tamano);
    if (orientacion == OrientacionPagina::Horizontal) {
      std::swap(dim.ancho, dim.alto);
    }
    p.ancho = dim.ancho;
    p.alto = dim.alto;
    p.limites.ancho_maximo = dim.ancho;
    p.limites.alto_maximo = dim.alto;
    p.limites.ancho_infinito = false;
    p.limites.alto_infinito = false;
  }
  return p;
}

void CambiarTamano(Pagina* pagina, TamanoPagina tamano) {
  if (!pagina || tamano == TamanoPagina::Personalizado) return;
  DimensionesPagina dim = DimensionesDeTamano(tamano);
  if (pagina->orientacion == OrientacionPagina::Horizontal) {
    std::swap(dim.ancho, dim.alto);
  }
  pagina->ancho = dim.ancho;
  pagina->alto = dim.alto;
  pagina->limites.ancho_maximo = dim.ancho;
  pagina->limites.alto_maximo = dim.alto;
  AplicarLimites(pagina->limites, &pagina->ancho, &pagina->alto);
}

void FijarTamano(Pagina* pagina, float ancho, float alto) {
  if (!pagina) return;
  pagina->ancho = ancho;
  pagina->alto = alto;
  pagina->limites.ancho_infinito = false;
  pagina->limites.alto_infinito = false;
  pagina->limites.ancho_maximo = ancho;
  pagina->limites.alto_maximo = alto;
}

void HacerInfinito(Pagina* pagina) {
  if (!pagina) return;
  pagina->limites.ancho_infinito = true;
  pagina->limites.alto_infinito = true;
  pagina->limites.ancho_maximo = 1e38f;
  pagina->limites.alto_maximo = 1e38f;
}

void HacerVerticalInfinito(Pagina* pagina) {
  if (!pagina) return;
  pagina->limites.ancho_infinito = false;
  pagina->limites.alto_infinito = true;
  pagina->limites.alto_maximo = 1e38f;
}

void HacerHorizontalInfinito(Pagina* pagina) {
  if (!pagina) return;
  pagina->limites.ancho_infinito = true;
  pagina->limites.alto_infinito = false;
  pagina->limites.ancho_maximo = 1e38f;
}

bool AjustarAlContenido(Pagina* pagina, float margen) {
  if (!pagina) return false;
  float cx, cy, cancho, calto;
  if (!pagina->CajaDeTinta(&cx, &cy, &cancho, &calto)) return false;
  
  float nuevo_ancho = cx + cancho + margen;
  float nuevo_alto = cy + calto + margen;
  
  if (pagina->ancho != nuevo_ancho || pagina->alto != nuevo_alto) {
    pagina->ancho = nuevo_ancho;
    pagina->alto = nuevo_alto;
    if (!pagina->limites.ancho_infinito) pagina->limites.ancho_maximo = std::max(pagina->limites.ancho_maximo, nuevo_ancho);
    if (!pagina->limites.alto_infinito) pagina->limites.alto_maximo = std::max(pagina->limites.alto_maximo, nuevo_alto);
    return true;
  }
  return false;
}

bool Pagina::CajaDeTinta(float* x, float* y, float* ancho, float* alto) const {
  bool hay = false;
  float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;

  for (const auto& trazo : trazos) {
    // El grosor cuenta: la caja tiene que abarcar el borde del trazo, no la
    // línea central, o al exportar se recorta media pluma.
    const float radio = trazo.ancho_base * 0.5f;
    for (const auto& muestra : trazo.muestras) {
      const float mx0 = muestra.posicion.x - radio;
      const float mx1 = muestra.posicion.x + radio;
      const float my0 = muestra.posicion.y - radio;
      const float my1 = muestra.posicion.y + radio;
      if (!hay) {
        x0 = mx0; x1 = mx1; y0 = my0; y1 = my1;
        hay = true;
      } else {
        x0 = std::min(x0, mx0);
        x1 = std::max(x1, mx1);
        y0 = std::min(y0, my0);
        y1 = std::max(y1, my1);
      }
    }
  }

  if (!hay) return false;
  if (x) *x = x0;
  if (y) *y = y0;
  if (ancho) *ancho = x1 - x0;
  if (alto) *alto = y1 - y0;
  return true;
}

bool Pagina::CrecerSiHaceFalta(float margen) {
  float cx = 0.0f, cy = 0.0f, cancho = 0.0f, calto = 0.0f;
  if (!CajaDeTinta(&cx, &cy, &cancho, &calto)) return false;

  const float ancho_previo = ancho;
  const float alto_previo = alto;

  if (limites.ancho_infinito && cx + cancho + margen > ancho) {
    ancho = cx + cancho + margen;
  }
  if (limites.alto_infinito && cy + calto + margen > alto) {
    alto = cy + calto + margen;
  }

  AplicarLimites(limites, &ancho, &alto);
  return ancho != ancho_previo || alto != alto_previo;
}

bool MoverPagina(Cuaderno& cuaderno, size_t seccion_origen, size_t pagina_origen,
                 size_t seccion_destino, size_t posicion_destino) {
  if (seccion_origen >= cuaderno.secciones.size()) return false;
  if (seccion_destino >= cuaderno.secciones.size()) return false;

  auto& origen = cuaderno.secciones[seccion_origen];
  if (pagina_origen >= origen.paginas.size()) return false;

  Pagina pagina = std::move(origen.paginas[pagina_origen]);
  origen.paginas.erase(origen.paginas.begin() + pagina_origen);

  auto& destino = cuaderno.secciones[seccion_destino];
  // Al mover dentro de la misma sección, quitar primero corre los índices
  // posteriores: sin este ajuste la página cae una posición más adelante de
  // donde se soltó.
  if (seccion_origen == seccion_destino && posicion_destino > pagina_origen) {
    --posicion_destino;
  }
  posicion_destino = std::min(posicion_destino, destino.paginas.size());
  destino.paginas.insert(destino.paginas.begin() + posicion_destino,
                         std::move(pagina));
  return true;
}

// ---------------------------------------------------------------------------
// Notas fijas
// ---------------------------------------------------------------------------

namespace {

// Saca la nota de donde esté y la devuelve. Deja `encontrada` en false si no
// existe en ninguna parte del documento.
NotaFija ExtraerNota(Documento& documento, const std::string& id,
                     bool* encontrada) {
  *encontrada = false;
  NotaFija copia;

  const auto sacar = [&](std::vector<NotaFija>& lista) {
    for (size_t i = 0; i < lista.size(); ++i) {
      if (lista[i].id != id) continue;
      copia = lista[i];
      lista.erase(lista.begin() + i);
      *encontrada = true;
      return true;
    }
    return false;
  };

  if (sacar(documento.notas)) return copia;
  for (auto& cuaderno : documento.cuadernos) {
    if (sacar(cuaderno.notas)) return copia;
    for (auto& seccion : cuaderno.secciones) {
      for (auto& pagina : seccion.paginas) {
        if (sacar(pagina.notas)) return copia;
      }
    }
  }
  return copia;
}

}  // namespace

std::vector<NotaFija*> NotasVisibles(Documento& documento,
                                     const Ubicacion& donde) {
  std::vector<NotaFija*> visibles;

  // Las globales primero: son las que están siempre, pase lo que pase.
  for (auto& nota : documento.notas) visibles.push_back(&nota);

  if (donde.cuaderno >= documento.cuadernos.size()) return visibles;
  Cuaderno& cuaderno = documento.cuadernos[donde.cuaderno];
  for (auto& nota : cuaderno.notas) visibles.push_back(&nota);

  if (donde.seccion >= cuaderno.secciones.size()) return visibles;
  Seccion& seccion = cuaderno.secciones[donde.seccion];
  if (donde.pagina >= seccion.paginas.size()) return visibles;

  for (auto& nota : seccion.paginas[donde.pagina].notas) visibles.push_back(&nota);
  return visibles;
}

bool CambiarAlcanceNota(Documento& documento, const std::string& id_nota,
                        AlcanceNota nuevo, const Ubicacion& donde) {
  // Se comprueba que el destino existe **antes** de sacar la nota: si no,
  // un índice malo la haría desaparecer del documento.
  if (nuevo == AlcanceNota::Cuaderno || nuevo == AlcanceNota::Pagina) {
    if (donde.cuaderno >= documento.cuadernos.size()) return false;
  }
  if (nuevo == AlcanceNota::Pagina) {
    const Cuaderno& cuaderno = documento.cuadernos[donde.cuaderno];
    if (donde.seccion >= cuaderno.secciones.size()) return false;
    if (donde.pagina >= cuaderno.secciones[donde.seccion].paginas.size()) {
      return false;
    }
  }

  bool encontrada = false;
  NotaFija nota = ExtraerNota(documento, id_nota, &encontrada);
  if (!encontrada) return false;

  nota.alcance = nuevo;
  switch (nuevo) {
    case AlcanceNota::Global:
      documento.notas.push_back(nota);
      break;
    case AlcanceNota::Cuaderno:
      documento.cuadernos[donde.cuaderno].notas.push_back(nota);
      break;
    case AlcanceNota::Pagina:
      documento.cuadernos[donde.cuaderno]
          .secciones[donde.seccion]
          .paginas[donde.pagina]
          .notas.push_back(nota);
      break;
  }
  return true;
}

NotaFija* BuscarNota(Documento& documento, const std::string& id_nota) {
  for (auto& nota : documento.notas) {
    if (nota.id == id_nota) return &nota;
  }
  for (auto& cuaderno : documento.cuadernos) {
    for (auto& nota : cuaderno.notas) {
      if (nota.id == id_nota) return &nota;
    }
    for (auto& seccion : cuaderno.secciones) {
      for (auto& pagina : seccion.paginas) {
        for (auto& nota : pagina.notas) {
          if (nota.id == id_nota) return &nota;
        }
      }
    }
  }
  return nullptr;
}

}  // namespace tinta
