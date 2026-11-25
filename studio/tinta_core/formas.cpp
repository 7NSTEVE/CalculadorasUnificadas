#include "formas.h"

#include <algorithm>
#include <cmath>

namespace tinta {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float Distancia(const Punto& a, const Punto& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

float DistanciaARecta(const Punto& p, const Punto& a, const Punto& b) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float largo = std::sqrt(dx * dx + dy * dy);
  if (largo < 1e-6f) return Distancia(p, a);
  // Producto cruzado dividido entre la longitud: la altura del triángulo.
  return std::fabs((p.x - a.x) * dy - (p.y - a.y) * dx) / largo;
}

void CajaDe(const std::vector<Punto>& puntos, float* x0, float* y0, float* x1,
            float* y1) {
  *x0 = *x1 = puntos[0].x;
  *y0 = *y1 = puntos[0].y;
  for (const auto& p : puntos) {
    *x0 = std::min(*x0, p.x);
    *x1 = std::max(*x1, p.x);
    *y0 = std::min(*y0, p.y);
    *y1 = std::max(*y1, p.y);
  }
}

float Recorrido(const std::vector<Punto>& puntos) {
  float total = 0.0f;
  for (size_t i = 1; i < puntos.size(); ++i) total += Distancia(puntos[i - 1], puntos[i]);
  return total;
}

float ProductoPunto(const Punto& a, const Punto& b) {
  return a.x * b.x + a.y * b.y;
}

float ProductoCruz(const Punto& a, const Punto& b) {
  return a.x * b.y - a.y * b.x;
}

Punto Restar(const Punto& a, const Punto& b) {
  return {a.x - b.x, a.y - b.y};
}

FormaReconocida ValidarFlecha(const Punto& cola, const Punto& punta_a,
                              const Punto& ala_a, const Punto& punta_b,
                              const Punto& ala_b,
                              const ParametrosForma& params) {
  FormaReconocida salida;
  const Punto punta{(punta_a.x + punta_b.x) * 0.5f,
                    (punta_a.y + punta_b.y) * 0.5f};
  const Punto asta = Restar(punta, cola);
  const float largo_asta = Distancia(cola, punta);
  if (largo_asta < 1e-3f) return salida;

  const float diferencia_punta = Distancia(punta_a, punta_b) / largo_asta;
  if (diferencia_punta > params.tolerancia_punta_flecha) return salida;

  const Punto va = Restar(ala_a, punta);
  const Punto vb = Restar(ala_b, punta);
  const float largo_a = std::sqrt(ProductoPunto(va, va));
  const float largo_b = std::sqrt(ProductoPunto(vb, vb));
  const float relacion_a = largo_a / largo_asta;
  const float relacion_b = largo_b / largo_asta;
  if (relacion_a < 0.12f || relacion_a > 0.52f ||
      relacion_b < 0.12f || relacion_b > 0.52f) {
    return salida;
  }

  const Punto direccion{asta.x / largo_asta, asta.y / largo_asta};
  const Punto ua{va.x / largo_a, va.y / largo_a};
  const Punto ub{vb.x / largo_b, vb.y / largo_b};
  const float proyeccion_a = ProductoPunto(ua, direccion);
  const float proyeccion_b = ProductoPunto(ub, direccion);
  // Las alas deben volver hacia la cola, no continuar hacia delante.
  if (proyeccion_a > -0.25f || proyeccion_b > -0.25f) return salida;

  const float cruz_a = ProductoCruz(direccion, ua);
  const float cruz_b = ProductoCruz(direccion, ub);
  if (cruz_a * cruz_b >= -0.08f) return salida;

  const float simetria = std::min(largo_a, largo_b) /
                         std::max(largo_a, largo_b);
  if (simetria < 0.45f) return salida;

  // Se conserva la dirección y el tamaño observado, pero se limpia la cabeza
  // a dos alas simétricas de 35 grados.
  const float largo_cabeza = (largo_a + largo_b) * 0.5f;
  const Punto atras{-direccion.x, -direccion.y};
  const Punto perpendicular{-direccion.y, direccion.x};
  constexpr float kAnguloCabeza = 35.0f * kPi / 180.0f;
  const float longitudinal = largo_cabeza * std::cos(kAnguloCabeza);
  const float lateral = largo_cabeza * std::sin(kAnguloCabeza);
  const float signo_a = cruz_a >= 0.0f ? 1.0f : -1.0f;
  const Punto ala_ideal_a{
      punta.x + atras.x * longitudinal + perpendicular.x * lateral * signo_a,
      punta.y + atras.y * longitudinal + perpendicular.y * lateral * signo_a};
  const Punto ala_ideal_b{
      punta.x + atras.x * longitudinal - perpendicular.x * lateral * signo_a,
      punta.y + atras.y * longitudinal - perpendicular.y * lateral * signo_a};

  const float confianza_punta = std::clamp(
      1.0f - diferencia_punta / params.tolerancia_punta_flecha, 0.0f, 1.0f);
  const float retorno = std::clamp(
      (-proyeccion_a - 0.25f + -proyeccion_b - 0.25f) / 1.5f,
      0.0f, 1.0f);
  salida.tipo = TipoForma::Flecha;
  salida.confianza = std::clamp(
      0.45f + 0.20f * confianza_punta + 0.20f * simetria +
          0.15f * retorno,
      0.0f, 1.0f);
  salida.vertices = {cola, punta, ala_ideal_a, ala_ideal_b};
  return salida.confianza >= params.confianza_minima ? salida
                                                     : FormaReconocida{};
}

FormaReconocida ReconocerFlecha(const std::vector<Punto>& puntos,
                                float diagonal,
                                const ParametrosForma& params) {
  auto vertices = Simplificar(
      puntos, diagonal * std::max(0.025f, params.tolerancia_simplificado * 0.65f));
  if (vertices.size() < 5 || vertices.size() > 7) return {};

  // Forma habitual: cola -> punta -> ala -> punta -> segunda ala.
  FormaReconocida directa = ValidarFlecha(
      vertices.front(), vertices[1], vertices[2], vertices[3],
      vertices.back(), params);
  if (directa.tipo == TipoForma::Flecha) return directa;

  // Variante de un solo trazo que empieza en un ala:
  // ala -> punta -> cola -> punta -> segunda ala.
  FormaReconocida desde_ala = ValidarFlecha(
      vertices[2], vertices[1], vertices.front(), vertices[3],
      vertices.back(), params);
  if (desde_ala.tipo == TipoForma::Flecha) return desde_ala;

  return {};
}

void SimplificarRec(const std::vector<Punto>& puntos, size_t inicio, size_t fin,
                    float tolerancia, std::vector<bool>* conservar) {
  if (fin <= inicio + 1) return;
  float peor = 0.0f;
  size_t indice = inicio;
  for (size_t i = inicio + 1; i < fin; ++i) {
    const float d = DistanciaARecta(puntos[i], puntos[inicio], puntos[fin]);
    if (d > peor) {
      peor = d;
      indice = i;
    }
  }
  if (peor <= tolerancia) return;
  (*conservar)[indice] = true;
  SimplificarRec(puntos, inicio, indice, tolerancia, conservar);
  SimplificarRec(puntos, indice, fin, tolerancia, conservar);
}

}  // namespace

std::vector<Punto> Simplificar(const std::vector<Punto>& puntos,
                               float tolerancia) {
  if (puntos.size() < 3 || tolerancia <= 0.0f) return puntos;

  std::vector<bool> conservar(puntos.size(), false);
  conservar.front() = true;
  conservar.back() = true;
  SimplificarRec(puntos, 0, puntos.size() - 1, tolerancia, &conservar);

  std::vector<Punto> salida;
  for (size_t i = 0; i < puntos.size(); ++i) {
    if (conservar[i]) salida.push_back(puntos[i]);
  }
  return salida;
}

FormaReconocida ReconocerForma(const Trazo& trazo, const ParametrosForma& params) {
  FormaReconocida forma;
  if (trazo.muestras.size() < 4) return forma;

  std::vector<Punto> puntos;
  puntos.reserve(trazo.muestras.size());
  for (const auto& m : trazo.muestras) puntos.push_back(m.posicion);

  const float recorrido = Recorrido(puntos);
  if (recorrido < 1e-3f) return forma;

  float x0, y0, x1, y1;
  CajaDe(puntos, &x0, &y0, &x1, &y1);
  const float ancho = x1 - x0;
  const float alto = y1 - y0;
  const float diagonal = std::sqrt(ancho * ancho + alto * alto);
  if (diagonal < 1e-3f) return forma;

  const float cierre = Distancia(puntos.front(), puntos.back()) / recorrido;
  const bool cerrado = cierre < params.tolerancia_cierre;

  // --- Línea -------------------------------------------------------------
  if (!cerrado) {
    const FormaReconocida flecha = ReconocerFlecha(puntos, diagonal, params);
    if (flecha.tipo == TipoForma::Flecha) return flecha;

    float desviacion = 0.0f;
    for (const auto& p : puntos) {
      desviacion = std::max(desviacion, DistanciaARecta(p, puntos.front(), puntos.back()));
    }
    const float relativa = desviacion / recorrido;
    if (relativa < params.tolerancia_linea) {
      forma.tipo = TipoForma::Linea;
      forma.confianza = std::clamp(1.0f - relativa / params.tolerancia_linea, 0.0f, 1.0f);
      forma.vertices = {puntos.front(), puntos.back()};
      return forma.confianza >= params.confianza_minima ? forma : FormaReconocida{};
    }
    return forma;
  }

  // --- Elipse ------------------------------------------------------------
  // Se mide contra una elipse y no contra un círculo: el radio esperado en cada
  // ángulo depende de la caja, así que un óvalo ancho también se reconoce.
  const Punto centro{(x0 + x1) * 0.5f, (y0 + y1) * 0.5f};
  const float semi_x = std::max(ancho * 0.5f, 1e-3f);
  const float semi_y = std::max(alto * 0.5f, 1e-3f);

  float suma = 0.0f;
  for (const auto& p : puntos) {
    const float nx = (p.x - centro.x) / semi_x;
    const float ny = (p.y - centro.y) / semi_y;
    // En una elipse perfecta esto vale 1 en todos los puntos.
    suma += std::sqrt(nx * nx + ny * ny);
  }
  const float media = suma / puntos.size();

  float varianza = 0.0f;
  for (const auto& p : puntos) {
    const float nx = (p.x - centro.x) / semi_x;
    const float ny = (p.y - centro.y) / semi_y;
    const float r = std::sqrt(nx * nx + ny * ny);
    varianza += (r - media) * (r - media);
  }
  const float dispersion = std::sqrt(varianza / puntos.size());

  // --- Polígono ----------------------------------------------------------
  auto simplificado = Simplificar(puntos, diagonal * params.tolerancia_simplificado);
  // En un trazo cerrado el primer y el último punto son el mismo vértice.
  if (simplificado.size() > 1 &&
      Distancia(simplificado.front(), simplificado.back()) < diagonal * 0.15f) {
    simplificado.pop_back();
  }
  const size_t lados = simplificado.size();

  const bool parece_elipse = dispersion < params.tolerancia_elipse;
  const bool parece_poligono = (lados == 3 || lados == 4);

  // Una elipse simplificada da muchos vértices; un polígono da pocos. Cuando
  // las dos pruebas se disparan gana la que tenga menos vértices, porque un
  // cuadrado también pasa la prueba de dispersión de forma mediocre.
  if (parece_poligono && (!parece_elipse || lados <= 4)) {
    if (lados == 3) {
      forma.tipo = TipoForma::Triangulo;
      forma.vertices = simplificado;
    } else {
      // El rectángulo se endereza a la caja: es lo que espera quien lo dibuja.
      forma.tipo = TipoForma::Rectangulo;
      forma.vertices = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    }
    forma.confianza = std::clamp(1.0f - cierre / params.tolerancia_cierre * 0.5f, 0.0f, 1.0f);
    return forma.confianza >= params.confianza_minima ? forma : FormaReconocida{};
  }

  if (parece_elipse) {
    forma.tipo = TipoForma::Elipse;
    forma.confianza =
        std::clamp(1.0f - dispersion / params.tolerancia_elipse, 0.0f, 1.0f);
    forma.vertices = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    return forma.confianza >= params.confianza_minima ? forma : FormaReconocida{};
  }

  return forma;
}

Trazo TrazoDesdeForma(const FormaReconocida& forma, const Trazo& original,
                      int muestras_por_curva) {
  Trazo salida;
  if (forma.tipo == TipoForma::Ninguna || forma.vertices.empty()) return salida;

  salida.ancho_base = original.ancho_base;
  salida.color_argb = original.color_argb;
  salida.resaltador = original.resaltador;

  // La presión media del trazo original, para que la forma enderezada tenga el
  // mismo peso visual que lo que se dibujó.
  float presion = kSinPresion;
  int con_presion = 0;
  float suma = 0.0f;
  for (const auto& m : original.muestras) {
    if (m.tiene_presion()) {
      suma += m.presion;
      ++con_presion;
    }
  }
  if (con_presion > 0) presion = suma / con_presion;

  const int64_t t0 = original.muestras.empty() ? 0 : original.muestras.front().tiempo_qpc;
  int64_t paso_tiempo = 1;
  if (original.muestras.size() > 1) {
    const int64_t span = original.muestras.back().tiempo_qpc - t0;
    paso_tiempo = std::max<int64_t>(1, span / static_cast<int64_t>(original.muestras.size()));
  }

  int contador = 0;
  const auto emitir = [&](const Punto& p) {
    Muestra m;
    m.posicion = p;
    m.presion = presion;
    m.tiempo_qpc = t0 + paso_tiempo * contador;
    ++contador;
    salida.muestras.push_back(m);
  };

  const auto emitir_segmento = [&](const Punto& a, const Punto& b, int pasos) {
    for (int i = 0; i <= pasos; ++i) {
      const float t = static_cast<float>(i) / pasos;
      emitir({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
    }
  };

  switch (forma.tipo) {
    case TipoForma::Linea:
      emitir_segmento(forma.vertices[0], forma.vertices[1], 16);
      break;

    case TipoForma::Triangulo:
    case TipoForma::Rectangulo: {
      const auto& v = forma.vertices;
      for (size_t i = 0; i < v.size(); ++i) {
        emitir_segmento(v[i], v[(i + 1) % v.size()], 16);
      }
      break;
    }

    case TipoForma::Elipse: {
      const auto& v = forma.vertices;
      const float cx = (v[0].x + v[2].x) * 0.5f;
      const float cy = (v[0].y + v[2].y) * 0.5f;
      const float rx = (v[2].x - v[0].x) * 0.5f;
      const float ry = (v[2].y - v[0].y) * 0.5f;
      const int pasos = std::max(8, muestras_por_curva);
      for (int i = 0; i <= pasos; ++i) {
        const float a = 2.0f * kPi * static_cast<float>(i) / pasos;
        emitir({cx + rx * std::cos(a), cy + ry * std::sin(a)});
      }
      break;
    }

    case TipoForma::Flecha: {
      if (forma.vertices.size() < 4) break;
      const Punto& cola = forma.vertices[0];
      const Punto& punta = forma.vertices[1];
      const Punto& ala_a = forma.vertices[2];
      const Punto& ala_b = forma.vertices[3];
      emitir_segmento(cola, punta, 20);
      emitir_segmento(punta, ala_a, 8);
      emitir_segmento(ala_a, punta, 8);
      emitir_segmento(punta, ala_b, 8);
      break;
    }

    case TipoForma::Ninguna:
      break;
  }

  return salida;
}

}  // namespace tinta
