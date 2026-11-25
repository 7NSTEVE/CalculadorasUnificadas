#include "reproduccion.h"

#include <algorithm>
#include <limits>

namespace tinta {
namespace {

float Mezclar(float a, float b, double t) {
  return static_cast<float>(a + (b - a) * t);
}

Muestra Interpolar(const Muestra& a, const Muestra& b, int64_t tiempo) {
  const int64_t distancia = b.tiempo_qpc - a.tiempo_qpc;
  const double t = distancia > 0
                       ? std::clamp(static_cast<double>(tiempo - a.tiempo_qpc) /
                                        static_cast<double>(distancia),
                                    0.0, 1.0)
                       : 0.0;
  Muestra m;
  m.posicion.x = Mezclar(a.posicion.x, b.posicion.x, t);
  m.posicion.y = Mezclar(a.posicion.y, b.posicion.y, t);
  if (a.tiene_presion() && b.tiene_presion()) {
    m.presion = Mezclar(a.presion, b.presion, t);
  } else {
    m.presion = a.tiene_presion() ? a.presion : b.presion;
  }
  m.inclinacion_x = Mezclar(a.inclinacion_x, b.inclinacion_x, t);
  m.inclinacion_y = Mezclar(a.inclinacion_y, b.inclinacion_y, t);
  m.tiempo_qpc = tiempo;
  return m;
}

}  // namespace

RangoTemporalTinta RangoTemporal(const Pagina& pagina) {
  int64_t minimo = std::numeric_limits<int64_t>::max();
  int64_t maximo = 0;
  for (const Trazo& trazo : pagina.trazos) {
    for (const Muestra& muestra : trazo.muestras) {
      if (muestra.tiempo_qpc <= 0) continue;
      minimo = std::min(minimo, muestra.tiempo_qpc);
      maximo = std::max(maximo, muestra.tiempo_qpc);
    }
  }
  if (maximo <= 0 || minimo == std::numeric_limits<int64_t>::max()) return {};
  return {minimo, maximo};
}

Trazo TrazoHasta(const Trazo& trazo, int64_t limite_qpc) {
  Trazo salida;
  salida.ancho_base = trazo.ancho_base;
  salida.color_argb = trazo.color_argb;
  salida.resaltador = trazo.resaltador;
  salida.estilo_persistente = trazo.estilo_persistente;
  salida.estilo = trazo.estilo;

  salida.muestras.reserve(trazo.muestras.size());
  for (size_t i = 0; i < trazo.muestras.size(); ++i) {
    const Muestra& actual = trazo.muestras[i];

    // El contenido sin timestamp es base estática y existe desde el inicio.
    if (actual.tiempo_qpc <= 0 || actual.tiempo_qpc <= limite_qpc) {
      salida.muestras.push_back(actual);
      continue;
    }

    if (!salida.muestras.empty()) {
      const Muestra& anterior = trazo.muestras[i - 1];
      if (anterior.tiempo_qpc > 0 && anterior.tiempo_qpc < limite_qpc &&
          actual.tiempo_qpc > limite_qpc) {
        salida.muestras.push_back(Interpolar(anterior, actual, limite_qpc));
      }
    }
    break;
  }
  return salida;
}

}  // namespace tinta
