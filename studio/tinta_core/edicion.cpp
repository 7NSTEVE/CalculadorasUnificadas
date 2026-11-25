#include "edicion.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace tinta {
namespace {

// Distancia al cuadrado de un punto al segmento AB. Al cuadrado para no sacar
// raíces en un bucle que corre por cada muestra de cada trazo.
float DistanciaCuadradaASegmento(const Punto& p, const Punto& a, const Punto& b) {
  const float abx = b.x - a.x;
  const float aby = b.y - a.y;
  const float largo2 = abx * abx + aby * aby;

  float t = 0.0f;
  if (largo2 > 1e-12f) {
    t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / largo2;
    t = std::clamp(t, 0.0f, 1.0f);
  }
  const float dx = p.x - (a.x + abx * t);
  const float dy = p.y - (a.y + aby * t);
  return dx * dx + dy * dy;
}

bool TrazoTocaCirculo(const Trazo& trazo, const Punto& centro, float radio) {
  if (trazo.muestras.empty()) return false;
  // El radio efectivo incluye medio grosor del trazo: el borrador debe morder
  // en cuanto toca el borde visible, no el eje.
  const float alcance = radio + trazo.ancho_base * 0.5f;
  const float alcance2 = alcance * alcance;

  if (trazo.muestras.size() == 1) {
    const float dx = trazo.muestras[0].posicion.x - centro.x;
    const float dy = trazo.muestras[0].posicion.y - centro.y;
    return dx * dx + dy * dy <= alcance2;
  }

  for (size_t i = 0; i + 1 < trazo.muestras.size(); ++i) {
    if (DistanciaCuadradaASegmento(centro, trazo.muestras[i].posicion,
                                   trazo.muestras[i + 1].posicion) <= alcance2) {
      return true;
    }
  }
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Borrador
// ---------------------------------------------------------------------------

std::vector<size_t> TrazosTocados(const Pagina& pagina, const Punto& centro,
                                  float radio) {
  std::vector<size_t> tocados;
  for (size_t i = 0; i < pagina.trazos.size(); ++i) {
    if (TrazoTocaCirculo(pagina.trazos[i], centro, radio)) tocados.push_back(i);
  }
  return tocados;
}

std::vector<Trazo> BorrarTrazosCompletos(Pagina& pagina,
                                         const std::vector<size_t>& indices) {
  std::vector<Trazo> quitados;
  // De mayor a menor, para que borrar uno no desplace los índices que faltan.
  std::vector<size_t> ordenados = indices;
  std::sort(ordenados.begin(), ordenados.end(), std::greater<size_t>());
  ordenados.erase(std::unique(ordenados.begin(), ordenados.end()), ordenados.end());

  for (size_t indice : ordenados) {
    if (indice >= pagina.trazos.size()) continue;
    quitados.push_back(pagina.trazos[indice]);
    pagina.trazos.erase(pagina.trazos.begin() + indice);
  }
  std::reverse(quitados.begin(), quitados.end());
  return quitados;
}

std::vector<Trazo> PartirTrazo(const Trazo& trazo, const Punto& centro,
                               float radio) {
  std::vector<Trazo> pedazos;
  if (trazo.muestras.empty()) return pedazos;

  const float alcance = radio + trazo.ancho_base * 0.5f;
  const float alcance2 = alcance * alcance;

  Trazo actual;
  actual.ancho_base = trazo.ancho_base;
  actual.color_argb = trazo.color_argb;
  actual.resaltador = trazo.resaltador;
  actual.estilo_persistente = trazo.estilo_persistente;
  actual.estilo = trazo.estilo;

  const auto cerrar = [&]() {
    // Un pedazo de una sola muestra es un punto suelto: se conserva, porque al
    // borrar el centro de una raya el usuario espera que queden las dos puntas.
    if (!actual.muestras.empty()) {
      pedazos.push_back(actual);
      actual.muestras.clear();
    }
  };

  for (const auto& muestra : trazo.muestras) {
    const float dx = muestra.posicion.x - centro.x;
    const float dy = muestra.posicion.y - centro.y;
    if (dx * dx + dy * dy <= alcance2) {
      cerrar();
    } else {
      actual.muestras.push_back(muestra);
    }
  }
  cerrar();

  return pedazos;
}

bool BorrarParcial(Pagina& pagina, const Punto& centro, float radio) {
  bool cambio = false;
  std::vector<Trazo> resultado;
  resultado.reserve(pagina.trazos.size());

  for (const auto& trazo : pagina.trazos) {
    if (!TrazoTocaCirculo(trazo, centro, radio)) {
      resultado.push_back(trazo);
      continue;
    }
    const auto pedazos = PartirTrazo(trazo, centro, radio);
    // Si sale el mismo trazo entero, el borrador rozó el grosor pero no llegó a
    // ninguna muestra: no hay cambio que registrar.
    if (pedazos.size() == 1 &&
        pedazos[0].muestras.size() == trazo.muestras.size()) {
      resultado.push_back(trazo);
      continue;
    }
    cambio = true;
    for (const auto& pedazo : pedazos) resultado.push_back(pedazo);
  }

  if (cambio) pagina.trazos = std::move(resultado);
  return cambio;
}

// ---------------------------------------------------------------------------
// Selección por lazo
// ---------------------------------------------------------------------------

bool PuntoEnPoligono(const std::vector<Punto>& poligono, const Punto& punto) {
  if (poligono.size() < 3) return false;
  bool dentro = false;
  // Lanzamiento de rayo: se cuentan los cruces con los lados hacia un lado.
  for (size_t i = 0, j = poligono.size() - 1; i < poligono.size(); j = i++) {
    const Punto& a = poligono[i];
    const Punto& b = poligono[j];
    if ((a.y > punto.y) != (b.y > punto.y)) {
      const float x_corte = (b.x - a.x) * (punto.y - a.y) / (b.y - a.y) + a.x;
      if (punto.x < x_corte) dentro = !dentro;
    }
  }
  return dentro;
}

std::vector<size_t> SeleccionarConLazo(const Pagina& pagina,
                                       const std::vector<Punto>& poligono,
                                       bool requerir_completo) {
  std::vector<size_t> seleccion;
  if (poligono.size() < 3) return seleccion;

  for (size_t i = 0; i < pagina.trazos.size(); ++i) {
    const auto& muestras = pagina.trazos[i].muestras;
    if (muestras.empty()) continue;

    bool alguna = false;
    bool todas = true;
    for (const auto& muestra : muestras) {
      if (PuntoEnPoligono(poligono, muestra.posicion)) {
        alguna = true;
      } else {
        todas = false;
      }
      if (alguna && !todas && !requerir_completo) break;
    }

    if (requerir_completo ? todas : alguna) seleccion.push_back(i);
  }
  return seleccion;
}

template <typename F>
Seleccion SeleccionarForma(const Pagina& pagina, const FiltroLazo& filtro, bool requerir_completo, F&& en_forma) {
  Seleccion s;

  if (filtro.tipo == TipoFiltroLazo::Todos || filtro.tipo == TipoFiltroLazo::SoloTinta) {
    for (size_t i = 0; i < pagina.trazos.size(); ++i) {
      if (filtro.color_especifico != 0 && pagina.trazos[i].color_argb != filtro.color_especifico) continue;
      const auto& muestras = pagina.trazos[i].muestras;
      if (muestras.empty()) continue;
      bool alguna = false, todas = true;
      for (const auto& m : muestras) {
        if (en_forma(m.posicion)) alguna = true; else todas = false;
        if (alguna && !todas && !requerir_completo) break;
      }
      if (requerir_completo ? todas : alguna) s.trazos.push_back(i);
    }
  }

  if (filtro.tipo == TipoFiltroLazo::Todos || filtro.tipo == TipoFiltroLazo::SoloTexto) {
    for (size_t i = 0; i < pagina.textos.size(); ++i) {
      const auto& r = pagina.textos[i].marco;
      Punto p1{r.x, r.y}, p2{r.x + r.ancho, r.y}, p3{r.x + r.ancho, r.y + r.alto}, p4{r.x, r.y + r.alto};
      Punto esquinas[] = {p1, p2, p3, p4};
      bool alguna = false, todas = true;
      for (const auto& p : esquinas) {
        if (en_forma(p)) alguna = true; else todas = false;
      }
      if (requerir_completo ? todas : alguna) s.textos.push_back(i);
    }
  }

  if (filtro.tipo == TipoFiltroLazo::Todos || filtro.tipo == TipoFiltroLazo::SoloImagen || filtro.tipo == TipoFiltroLazo::SoloPdf) {
    for (size_t i = 0; i < pagina.medios.size(); ++i) {
      const auto& m = pagina.medios[i];
      if (filtro.tipo == TipoFiltroLazo::SoloImagen && m.tipo != TipoMedio::Imagen) continue;
      if (filtro.tipo == TipoFiltroLazo::SoloPdf && m.tipo != TipoMedio::Pdf) continue;
      const auto& r = m.marco;
      Punto p1{r.x, r.y}, p2{r.x + r.ancho, r.y}, p3{r.x + r.ancho, r.y + r.alto}, p4{r.x, r.y + r.alto};
      Punto esquinas[] = {p1, p2, p3, p4};
      bool alguna = false, todas = true;
      for (const auto& p : esquinas) {
        if (en_forma(p)) alguna = true; else todas = false;
      }
      if (requerir_completo ? todas : alguna) s.medios.push_back(i);
    }
  }

  return s;
}

Seleccion SeleccionarPoligono(const Pagina& pagina, const std::vector<Punto>& poligono, const FiltroLazo& filtro, bool requerir_completo) {
  if (poligono.size() < 3) return {};
  return SeleccionarForma(pagina, filtro, requerir_completo, [&](const Punto& p) {
    return PuntoEnPoligono(poligono, p);
  });
}

Seleccion SeleccionarRectangulo(const Pagina& pagina, float x, float y, float ancho, float alto, const FiltroLazo& filtro, bool requerir_completo) {
  return SeleccionarForma(pagina, filtro, requerir_completo, [=](const Punto& p) {
    return p.x >= x && p.x <= x + ancho && p.y >= y && p.y <= y + alto;
  });
}

Seleccion SeleccionarElipse(const Pagina& pagina, const Punto& centro, float radio_x, float radio_y, const FiltroLazo& filtro, bool requerir_completo) {
  return SeleccionarForma(pagina, filtro, requerir_completo, [=](const Punto& p) {
    if (radio_x <= 0.0f || radio_y <= 0.0f) return false;
    float dx = (p.x - centro.x) / radio_x;
    float dy = (p.y - centro.y) / radio_y;
    return (dx * dx + dy * dy) <= 1.0f;
  });
}

namespace {

float DistanciaCuadradaSegmento(const Punto& p, const Punto& a,
                                const Punto& b) {
  const float vx = b.x - a.x;
  const float vy = b.y - a.y;
  const float longitud2 = vx * vx + vy * vy;
  if (longitud2 <= 0.000001f) {
    const float dx = p.x - a.x;
    const float dy = p.y - a.y;
    return dx * dx + dy * dy;
  }
  const float t = std::clamp(((p.x - a.x) * vx + (p.y - a.y) * vy) /
                                 longitud2,
                             0.0f, 1.0f);
  const float dx = p.x - (a.x + t * vx);
  const float dy = p.y - (a.y + t * vy);
  return dx * dx + dy * dy;
}

bool PuntoCercaRecorrido(const Punto& punto,
                         const std::vector<Punto>& recorrido,
                         float radio2) {
  if (recorrido.empty()) return false;
  if (recorrido.size() == 1) {
    return DistanciaCuadradaSegmento(punto, recorrido.front(),
                                     recorrido.front()) <= radio2;
  }
  for (size_t i = 1; i < recorrido.size(); ++i) {
    if (DistanciaCuadradaSegmento(punto, recorrido[i - 1], recorrido[i]) <=
        radio2) return true;
  }
  return false;
}

}  // namespace

Seleccion SeleccionarPincel(const Pagina& pagina,
                            const std::vector<Punto>& recorrido, float radio,
                            const FiltroLazo& filtro,
                            bool requerir_completo) {
  if (recorrido.empty() || !std::isfinite(radio) || radio <= 0.0f) return {};
  const float radio2 = radio * radio;
  return SeleccionarForma(pagina, filtro, requerir_completo,
                          [&](const Punto& punto) {
    return PuntoCercaRecorrido(punto, recorrido, radio2);
  });
}

Seleccion UnirSelecciones(const Seleccion& a, const Seleccion& b) {
  Seleccion r;
  auto unir = [](const std::vector<size_t>& v1, const std::vector<size_t>& v2) {
    std::vector<size_t> res;
    std::set_union(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(res));
    return res;
  };
  r.trazos = unir(a.trazos, b.trazos);
  r.textos = unir(a.textos, b.textos);
  r.medios = unir(a.medios, b.medios);
  return r;
}

Seleccion RestarSelecciones(const Seleccion& original, const Seleccion& a_quitar) {
  Seleccion r;
  auto restar = [](const std::vector<size_t>& v1, const std::vector<size_t>& v2) {
    std::vector<size_t> res;
    std::set_difference(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(res));
    return res;
  };
  r.trazos = restar(original.trazos, a_quitar.trazos);
  r.textos = restar(original.textos, a_quitar.textos);
  r.medios = restar(original.medios, a_quitar.medios);
  return r;
}

Seleccion InvertirSeleccion(const Pagina& pagina, const Seleccion& actual, const FiltroLazo& filtro) {
  Seleccion todo;
  if (filtro.tipo == TipoFiltroLazo::Todos || filtro.tipo == TipoFiltroLazo::SoloTinta) {
    for (size_t i = 0; i < pagina.trazos.size(); ++i) {
      if (filtro.color_especifico != 0 && pagina.trazos[i].color_argb != filtro.color_especifico) continue;
      todo.trazos.push_back(i);
    }
  }
  if (filtro.tipo == TipoFiltroLazo::Todos || filtro.tipo == TipoFiltroLazo::SoloTexto) {
    for (size_t i = 0; i < pagina.textos.size(); ++i) todo.textos.push_back(i);
  }
  if (filtro.tipo == TipoFiltroLazo::Todos || filtro.tipo == TipoFiltroLazo::SoloImagen || filtro.tipo == TipoFiltroLazo::SoloPdf) {
    for (size_t i = 0; i < pagina.medios.size(); ++i) {
      if (filtro.tipo == TipoFiltroLazo::SoloImagen && pagina.medios[i].tipo != TipoMedio::Imagen) continue;
      if (filtro.tipo == TipoFiltroLazo::SoloPdf && pagina.medios[i].tipo != TipoMedio::Pdf) continue;
      todo.medios.push_back(i);
    }
  }
  return RestarSelecciones(todo, actual);
}

// ---------------------------------------------------------------------------
// Transformaciones
// ---------------------------------------------------------------------------

void TransformarTrazos(Pagina& pagina, const std::vector<size_t>& indices,
                       const Transformacion& t) {
  const float cos_r = std::cos(t.rotacion);
  const float sin_r = std::sin(t.rotacion);
  const float sx = t.escala * t.escala_x *
                   (t.reflejar_horizontal ? -1.0f : 1.0f);
  const float sy = t.escala * t.escala_y *
                   (t.reflejar_vertical ? -1.0f : 1.0f);

  for (size_t indice : indices) {
    if (indice >= pagina.trazos.size()) continue;
    Trazo& trazo = pagina.trazos[indice];

    for (auto& muestra : trazo.muestras) {
      // Al pivote, escalar, rotar, volver, y desplazar.
      float x = (muestra.posicion.x - t.pivote.x) * sx;
      float y = (muestra.posicion.y - t.pivote.y) * sy;
      const float rx = x * cos_r - y * sin_r;
      const float ry = x * sin_r + y * cos_r;
      muestra.posicion.x = rx + t.pivote.x + t.desplazar_x;
      muestra.posicion.y = ry + t.pivote.y + t.desplazar_y;
    }

    // El grosor escala con la figura: si no, al agrandar una anotación la letra
    // crece pero la pluma se queda fina y deja de parecer la misma escritura.
    // Con escala no proporcional no existe un único grosor exacto. La media
    // geométrica conserva el área visual sin favorecer un eje.
    trazo.ancho_base *= std::sqrt(std::abs(sx * sy));
  }
}

namespace {

Punto TransformarPunto(const Punto& punto, const Transformacion& t) {
  const float sx = t.escala * t.escala_x *
                   (t.reflejar_horizontal ? -1.0f : 1.0f);
  const float sy = t.escala * t.escala_y *
                   (t.reflejar_vertical ? -1.0f : 1.0f);
  const float x = (punto.x - t.pivote.x) * sx;
  const float y = (punto.y - t.pivote.y) * sy;
  const float cos_r = std::cos(t.rotacion);
  const float sin_r = std::sin(t.rotacion);
  return {x * cos_r - y * sin_r + t.pivote.x + t.desplazar_x,
          x * sin_r + y * cos_r + t.pivote.y + t.desplazar_y};
}

void TransformarRectangulo(Rect* marco, float* rotacion,
                           const Transformacion& t) {
  if (!marco || !rotacion) return;
  const Punto centro{marco->x + marco->ancho * 0.5f,
                     marco->y + marco->alto * 0.5f};
  const Punto nuevo = TransformarPunto(centro, t);
  const float sx = std::abs(t.escala * t.escala_x);
  const float sy = std::abs(t.escala * t.escala_y);
  marco->ancho = std::max(1.0f, marco->ancho * sx);
  marco->alto = std::max(1.0f, marco->alto * sy);
  marco->x = nuevo.x - marco->ancho * 0.5f;
  marco->y = nuevo.y - marco->alto * 0.5f;
  if (t.reflejar_horizontal != t.reflejar_vertical) *rotacion = -*rotacion;
  *rotacion += t.rotacion;
}

}  // namespace

void TransformarSeleccion(Pagina& pagina, const Seleccion& seleccion,
                          const Transformacion& t) {
  TransformarTrazos(pagina, seleccion.trazos, t);
  for (const size_t indice : seleccion.textos) {
    if (indice < pagina.textos.size()) {
      TransformarRectangulo(&pagina.textos[indice].marco,
                            &pagina.textos[indice].rotacion, t);
    }
  }
  for (const size_t indice : seleccion.medios) {
    if (indice < pagina.medios.size()) {
      TransformarRectangulo(&pagina.medios[indice].marco,
                            &pagina.medios[indice].rotacion, t);
    }
  }
}

template <typename T>
void TraerAlFrenteVector(std::vector<T>& vec, const std::vector<size_t>& indices) {
  if (indices.empty() || vec.empty()) return;
  std::vector<T> seleccionados;
  std::vector<T> resto;
  
  size_t idx = 0;
  for (size_t i = 0; i < vec.size(); ++i) {
    if (idx < indices.size() && indices[idx] == i) {
      seleccionados.push_back(std::move(vec[i]));
      idx++;
    } else {
      resto.push_back(std::move(vec[i]));
    }
  }
  
  vec.clear();
  vec.insert(vec.end(), std::make_move_iterator(resto.begin()), std::make_move_iterator(resto.end()));
  vec.insert(vec.end(), std::make_move_iterator(seleccionados.begin()), std::make_move_iterator(seleccionados.end()));
}

template <typename T>
void EnviarAtrasVector(std::vector<T>& vec, const std::vector<size_t>& indices) {
  if (indices.empty() || vec.empty()) return;
  std::vector<T> seleccionados;
  std::vector<T> resto;
  
  size_t idx = 0;
  for (size_t i = 0; i < vec.size(); ++i) {
    if (idx < indices.size() && indices[idx] == i) {
      seleccionados.push_back(std::move(vec[i]));
      idx++;
    } else {
      resto.push_back(std::move(vec[i]));
    }
  }
  
  vec.clear();
  vec.insert(vec.end(), std::make_move_iterator(seleccionados.begin()), std::make_move_iterator(seleccionados.end()));
  vec.insert(vec.end(), std::make_move_iterator(resto.begin()), std::make_move_iterator(resto.end()));
}

void TraerAlFrente(Pagina& pagina, const Seleccion& seleccion) {
  TraerAlFrenteVector(pagina.trazos, seleccion.trazos);
  TraerAlFrenteVector(pagina.textos, seleccion.textos);
  TraerAlFrenteVector(pagina.medios, seleccion.medios);
}

void EnviarAtras(Pagina& pagina, const Seleccion& seleccion) {
  EnviarAtrasVector(pagina.trazos, seleccion.trazos);
  EnviarAtrasVector(pagina.textos, seleccion.textos);
  EnviarAtrasVector(pagina.medios, seleccion.medios);
}

void RecolorearTrazos(Pagina& pagina, const std::vector<size_t>& indices,
                      uint32_t color_argb) {
  for (size_t indice : indices) {
    if (indice >= pagina.trazos.size()) continue;
    pagina.trazos[indice].color_argb = color_argb;
  }
}

// ---------------------------------------------------------------------------
// Historial
// ---------------------------------------------------------------------------

namespace {

bool AplicarActualizacionEstimada(Muestra& muestra, uint64_t id_muestra,
                                  float delta_x, float delta_y,
                                  float presion, float altitud, float azimut,
                                  bool posicion, bool fuerza,
                                  bool inclinacion, bool direccion) {
  if (id_muestra == 0 || muestra.id_actualizacion != id_muestra) return false;
  if (posicion) {
    muestra.posicion.x += delta_x;
    muestra.posicion.y += delta_y;
  }
  if (fuerza) muestra.presion = presion;
  if (inclinacion) muestra.inclinacion_x = altitud;
  if (direccion) muestra.inclinacion_y = azimut;
  return true;
}

size_t ActualizarEnTrazos(std::vector<Trazo>& trazos, uint64_t id_muestra,
                          float delta_x, float delta_y, float presion,
                          float altitud, float azimut, bool posicion,
                          bool fuerza, bool inclinacion, bool direccion) {
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

size_t ActualizarEnOperaciones(std::vector<Operacion>& operaciones,
                               uint64_t id_muestra,
                               float delta_x, float delta_y, float presion,
                               float altitud, float azimut, bool posicion,
                               bool fuerza, bool inclinacion, bool direccion) {
  size_t actualizadas = 0;
  for (auto& operacion : operaciones) {
    actualizadas += ActualizarEnTrazos(
        operacion.antes, id_muestra, delta_x, delta_y, presion, altitud,
        azimut, posicion, fuerza, inclinacion, direccion);
    actualizadas += ActualizarEnTrazos(
        operacion.despues, id_muestra, delta_x, delta_y, presion, altitud,
        azimut, posicion, fuerza, inclinacion, direccion);
  }
  return actualizadas;
}

}  // namespace

Operacion Historial::OperacionAnadir(size_t indice, const Trazo& trazo) {
  Operacion op;
  op.tipo = TipoOperacion::AnadirTrazos;
  op.indices = {indice};
  op.despues = {trazo};
  return op;
}

Operacion Historial::OperacionQuitar(const std::vector<size_t>& indices,
                                     const std::vector<Trazo>& trazos) {
  Operacion op;
  op.tipo = TipoOperacion::QuitarTrazos;
  op.indices = indices;
  op.antes = trazos;
  return op;
}

Operacion Historial::OperacionReemplazar(const std::vector<size_t>& indices,
                                         const std::vector<Trazo>& antes,
                                         const std::vector<Trazo>& despues) {
  Operacion op;
  op.tipo = TipoOperacion::ReemplazarTrazos;
  op.indices = indices;
  op.antes = antes;
  op.despues = despues;
  return op;
}

Operacion Historial::OperacionPagina(const std::vector<Trazo>& antes,
                                     const std::vector<Trazo>& despues) {
  Operacion op;
  op.tipo = TipoOperacion::ReemplazarPaginaTrazos;
  op.antes = antes;
  op.despues = despues;
  return op;
}

Operacion Historial::OperacionPaginaCompleta(const Pagina& antes,
                                             const Pagina& despues) {
  Operacion op;
  op.tipo = TipoOperacion::ReemplazarPaginaCompleta;
  op.antes = antes.trazos;
  op.despues = despues.trazos;
  op.textos_antes = antes.textos;
  op.textos_despues = despues.textos;
  op.medios_antes = antes.medios;
  op.medios_despues = despues.medios;
  return op;
}

void Historial::Registrar(const Operacion& operacion) {
  hechas_.push_back(operacion);
  // Cualquier acción nueva invalida la rama rehecha, como en cualquier editor.
  deshechas_.clear();
}

void Historial::Aplicar(Pagina& pagina, const Operacion& operacion,
                        bool invertida) {
  const bool anadir = (operacion.tipo == TipoOperacion::AnadirTrazos) != invertida;

  switch (operacion.tipo) {
    case TipoOperacion::AnadirTrazos:
    case TipoOperacion::QuitarTrazos: {
      const auto& trazos = (operacion.tipo == TipoOperacion::AnadirTrazos)
                               ? operacion.despues
                               : operacion.antes;
      if (anadir) {
        // De menor a mayor, para que cada inserción deje al siguiente su sitio.
        for (size_t k = 0; k < operacion.indices.size() && k < trazos.size(); ++k) {
          const size_t indice = std::min(operacion.indices[k], pagina.trazos.size());
          pagina.trazos.insert(pagina.trazos.begin() + indice, trazos[k]);
        }
      } else {
        std::vector<size_t> ordenados = operacion.indices;
        std::sort(ordenados.begin(), ordenados.end(), std::greater<size_t>());
        for (size_t indice : ordenados) {
          if (indice < pagina.trazos.size()) {
            pagina.trazos.erase(pagina.trazos.begin() + indice);
          }
        }
      }
      break;
    }
    case TipoOperacion::ReemplazarTrazos: {
      const auto& fuente = invertida ? operacion.antes : operacion.despues;
      for (size_t k = 0; k < operacion.indices.size() && k < fuente.size(); ++k) {
        const size_t indice = operacion.indices[k];
        if (indice < pagina.trazos.size()) pagina.trazos[indice] = fuente[k];
      }
      break;
    }
    case TipoOperacion::ReemplazarPaginaTrazos:
      pagina.trazos = invertida ? operacion.antes : operacion.despues;
      break;
    case TipoOperacion::ReemplazarPaginaCompleta:
      pagina.trazos = invertida ? operacion.antes : operacion.despues;
      pagina.textos = invertida ? operacion.textos_antes
                                : operacion.textos_despues;
      pagina.medios = invertida ? operacion.medios_antes
                                : operacion.medios_despues;
      break;
  }
}

bool Historial::Deshacer(Pagina& pagina) {
  if (hechas_.empty()) return false;
  const Operacion op = hechas_.back();
  hechas_.pop_back();
  Aplicar(pagina, op, true);
  deshechas_.push_back(op);
  return true;
}

bool Historial::Rehacer(Pagina& pagina) {
  if (deshechas_.empty()) return false;
  const Operacion op = deshechas_.back();
  deshechas_.pop_back();
  Aplicar(pagina, op, false);
  hechas_.push_back(op);
  return true;
}

void Historial::Limpiar() {
  hechas_.clear();
  deshechas_.clear();
}

size_t Historial::ActualizarMuestraEstimada(
    uint64_t id_muestra, float delta_x, float delta_y, float presion,
    float altitud, float azimut, bool posicion, bool fuerza,
    bool inclinacion, bool direccion) {
  return ActualizarEnOperaciones(
             hechas_, id_muestra, delta_x, delta_y, presion, altitud, azimut,
             posicion, fuerza, inclinacion, direccion) +
         ActualizarEnOperaciones(
             deshechas_, id_muestra, delta_x, delta_y, presion, altitud,
             azimut, posicion, fuerza, inclinacion, direccion);
}

}  // namespace tinta
