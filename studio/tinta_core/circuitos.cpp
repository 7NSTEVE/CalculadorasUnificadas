#include "circuitos.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace tinta {
namespace {

float DistanciaCircuito(const Punto& a, const Punto& b) {
  return std::hypot(a.x - b.x, a.y - b.y);
}

float RecorridoCircuito(const Trazo& trazo) {
  float total = 0.0f;
  for (size_t i = 1; i < trazo.muestras.size(); ++i)
    total += DistanciaCircuito(trazo.muestras[i - 1].posicion,
                              trazo.muestras[i].posicion);
  return total;
}

float CambioAngularMedio(const std::vector<Punto>& puntos) {
  if (puntos.size() < 3) return 0.0f;
  float suma = 0.0f;
  int cantidad = 0;
  for (size_t i = 1; i + 1 < puntos.size(); ++i) {
    const float a1 = std::atan2(puntos[i].y - puntos[i - 1].y,
                                puntos[i].x - puntos[i - 1].x);
    const float a2 = std::atan2(puntos[i + 1].y - puntos[i].y,
                                puntos[i + 1].x - puntos[i].x);
    float delta = std::fabs(a2 - a1);
    if (delta > 3.14159265f) delta = 2.0f * 3.14159265f - delta;
    suma += delta;
    ++cantidad;
  }
  return cantidad > 0 ? suma / cantidad : 0.0f;
}

}  // namespace

ReconocimientoCircuito ReconocerComponente(const std::vector<Trazo>& trazos,
                                           const ParametrosForma& params) {
  ReconocimientoCircuito resultado;
  if (trazos.empty()) {
    return resultado;
  }

  int elipses = 0;
  int lineas = 0;
  [[maybe_unused]] int formas_desconocidas = 0;

  for (const auto& trazo : trazos) {
    FormaReconocida forma = ReconocerForma(trazo, params);
    if (forma.tipo == TipoForma::Elipse) {
      elipses++;
    } else if (forma.tipo == TipoForma::Linea) {
      lineas++;
    } else {
      formas_desconocidas++;
    }
  }

  // Heurísticas básicas de reconocimiento de circuitos
  if (elipses == 1) {
    // Un círculo suele ser una fuente
    resultado.tipo = TipoComponenteCircuito::FuenteVoltaje;
    resultado.confianza = 0.75f;
  } else if (lineas == 2 && elipses == 0 && trazos.size() == 2) {
    // Dos líneas paralelas -> Capacitor
    resultado.tipo = TipoComponenteCircuito::Capacitor;
    resultado.confianza = 0.65f;
  } else if (lineas >= 3 && elipses == 0 && trazos.size() >= 3) {
    // Varias líneas -> Posible Tierra
    resultado.tipo = TipoComponenteCircuito::Tierra;
    resultado.confianza = 0.60f;
  } else if (trazos.size() == 1) {
    FormaReconocida forma = ReconocerForma(trazos[0], params);
    if (forma.tipo == TipoForma::Linea) {
      // Una simple línea es un cable
      resultado.tipo = TipoComponenteCircuito::Cable;
      resultado.confianza = 0.90f;
    } else {
      // Si no es una línea limpia, extraemos los puntos y simplificamos 
      // para distinguir esquinas de zigzag de una bobina suave.
      std::vector<Punto> puntos;
      for (const auto& m : trazos[0].muestras) {
        puntos.push_back(m.posicion);
      }
      
      float x0 = puntos.front().x, x1 = x0, y0 = puntos.front().y, y1 = y0;
      for (const auto& p : puntos) {
        x0 = std::min(x0, p.x); x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y); y1 = std::max(y1, p.y);
      }
      const float diagonal = std::max(std::hypot(x1 - x0, y1 - y0), 1e-4f);
      std::vector<Punto> simplificado = Simplificar(puntos, diagonal * 0.045f);
      const float giro = CambioAngularMedio(simplificado);
      if (simplificado.size() >= 6 && giro > 0.72f) {
        resultado.tipo = TipoComponenteCircuito::Resistor;
        resultado.confianza = std::clamp(0.58f + giro * 0.16f, 0.0f, 0.90f);
      } else if (simplificado.size() >= 5 && giro > 0.18f && giro <= 0.72f) {
        resultado.tipo = TipoComponenteCircuito::Inductor;
        resultado.confianza = std::clamp(0.62f + (0.72f - giro) * 0.18f,
                                         0.0f, 0.86f);
      }
    }
  } else {
    // Una combinación que no encaja no se etiqueta. El reconocedor entrenable
    // de la capa iPad puede compararla con ejemplos confirmados por el usuario.
    resultado.confianza = 0.0f;
  }

  // Asignar terminales estimadas en los extremos del primer trazo 
  // (Simplificación para esta primera iteración)
  if (!trazos.empty() && !trazos[0].muestras.empty()) {
    resultado.terminales.push_back(trazos[0].muestras.front().posicion);
    resultado.terminales.push_back(trazos[0].muestras.back().posicion);
  }

  return resultado;
}

std::vector<float> ExtraerCaracteristicasCircuito(
    const std::vector<Trazo>& trazos, const ParametrosForma& params) {
  constexpr size_t kCantidad = 14;
  std::vector<float> f(kCantidad, 0.0f);
  if (trazos.empty()) return f;

  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  bool primera = true;
  float recorrido_total = 0.0f;
  float cierres = 0.0f;
  float vertices = 0.0f;
  float giros = 0.0f;
  int lineas = 0, elipses = 0, rectangulos = 0, triangulos = 0;
  std::vector<Punto> extremos;

  for (const auto& trazo : trazos) {
    if (trazo.muestras.empty()) continue;
    for (const auto& muestra : trazo.muestras) {
      const Punto p = muestra.posicion;
      if (primera) { x0 = x1 = p.x; y0 = y1 = p.y; primera = false; }
      x0 = std::min(x0, p.x); x1 = std::max(x1, p.x);
      y0 = std::min(y0, p.y); y1 = std::max(y1, p.y);
    }
    const float recorrido = RecorridoCircuito(trazo);
    recorrido_total += recorrido;
    const Punto inicio = trazo.muestras.front().posicion;
    const Punto fin = trazo.muestras.back().posicion;
    extremos.push_back(inicio); extremos.push_back(fin);
    if (recorrido > 1e-5f)
      cierres += std::clamp(1.0f - DistanciaCircuito(inicio, fin) / recorrido,
                            0.0f, 1.0f);

    std::vector<Punto> puntos;
    puntos.reserve(trazo.muestras.size());
    for (const auto& m : trazo.muestras) puntos.push_back(m.posicion);
    const auto simplificado = Simplificar(puntos, std::max(recorrido * 0.015f, 0.5f));
    vertices += std::min<float>(simplificado.size(), 24.0f) / 24.0f;
    giros += CambioAngularMedio(simplificado) / 3.14159265f;

    switch (ReconocerForma(trazo, params).tipo) {
      case TipoForma::Linea: ++lineas; break;
      case TipoForma::Elipse: ++elipses; break;
      case TipoForma::Rectangulo: ++rectangulos; break;
      case TipoForma::Triangulo: ++triangulos; break;
      case TipoForma::Flecha: break;
      case TipoForma::Ninguna: break;
    }
  }

  const float n = std::max<float>(trazos.size(), 1.0f);
  const float ancho = std::max(x1 - x0, 0.0f);
  const float alto = std::max(y1 - y0, 0.0f);
  const float diagonal = std::max(std::hypot(ancho, alto), 1e-4f);
  int pares_cercanos = 0;
  int pares = 0;
  for (size_t i = 0; i < extremos.size(); ++i) {
    for (size_t j = i + 1; j < extremos.size(); ++j) {
      ++pares;
      if (DistanciaCircuito(extremos[i], extremos[j]) < diagonal * 0.10f)
        ++pares_cercanos;
    }
  }

  f[0] = std::min<float>(trazos.size(), 16.0f) / 16.0f;
  f[1] = lineas / n;
  f[2] = elipses / n;
  f[3] = rectangulos / n;
  f[4] = triangulos / n;
  f[5] = ancho / std::max(ancho + alto, 1e-4f);
  f[6] = cierres / n;
  f[7] = std::tanh(recorrido_total / (diagonal * n * 4.0f));
  f[8] = vertices / n;
  f[9] = giros / n;
  f[10] = pares > 0 ? static_cast<float>(pares_cercanos) / pares : 0.0f;
  f[11] = std::min(recorrido_total / (diagonal * 12.0f), 1.0f);
  f[12] = std::min(ancho / std::max(alto, diagonal * 0.02f), 10.0f) / 10.0f;
  f[13] = std::min(alto / std::max(ancho, diagonal * 0.02f), 10.0f) / 10.0f;
  return f;
}

std::string GenerarSpice(const Circuito& circuito) {
  std::string spice = "* Netlist de Tinta\n";
  for (const auto& comp : circuito.componentes) {
    if (comp.tipo == TipoComponenteCircuito::Tierra ||
        comp.tipo == TipoComponenteCircuito::Cable) {
      spice += "* " + comp.id_texto +
               ": topología conservada en los nodos; no es un elemento SPICE\n";
      continue;
    }
    if (comp.id_texto.empty() || comp.nodos.size() < 2) {
      spice += "* OMITIDO: componente sin referencia o sin dos nodos confirmados\n";
      continue;
    }
    if (!(comp.valor > 0.0f)) {
      spice += "* OMITIDO " + comp.id_texto +
               ": falta un valor confirmado; Tinta no asigna valores ficticios\n";
      continue;
    }
    spice += comp.id_texto + " ";
    for (int nodo : comp.nodos) {
      spice += std::to_string(nodo) + " ";
    }
    std::ostringstream valor;
    valor << std::setprecision(9) << comp.valor;
    if (comp.tipo == TipoComponenteCircuito::FuenteVoltaje ||
        comp.tipo == TipoComponenteCircuito::FuenteCorriente) {
      spice += "DC ";
    }
    spice += valor.str();
    spice += "\n";
  }
  return spice;
}

}  // namespace tinta
