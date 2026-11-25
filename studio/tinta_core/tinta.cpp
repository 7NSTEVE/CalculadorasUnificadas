#include "tinta.h"

#include <algorithm>
#include <cmath>

namespace tinta {
namespace {

Punto Resta(const Punto& a, const Punto& b) { return {a.x - b.x, a.y - b.y}; }

float Longitud(const Punto& p) { return std::sqrt(p.x * p.x + p.y * p.y); }

// Velocidad normalizada a 0..1 entre una muestra y la anterior.
float VelocidadNormalizada(const std::vector<Muestra>& muestras, size_t i,
                           int64_t frecuencia_qpc, float velocidad_tope) {
  if (i == 0 || frecuencia_qpc <= 0) return 0.0f;
  const int64_t ticks = muestras[i].tiempo_qpc - muestras[i - 1].tiempo_qpc;
  if (ticks <= 0) return 0.0f;
  const double ms = 1000.0 * static_cast<double>(ticks) / frecuencia_qpc;
  const float distancia =
      Longitud(Resta(muestras[i].posicion, muestras[i - 1].posicion));
  const double v = distancia / ms / velocidad_tope;
  return static_cast<float>(std::clamp(v, 0.0, 1.0));
}

std::vector<float> MediaMovil(const std::vector<float>& valores, int ventana) {
  if (ventana <= 1 || valores.size() <= 2) return valores;
  std::vector<float> salida(valores.size());
  const int n = static_cast<int>(valores.size());
  for (int i = 0; i < n; ++i) {
    const int desde = std::max(0, i - ventana);
    const int hasta = std::min(n - 1, i + ventana);
    float suma = 0.0f;
    for (int j = desde; j <= hasta; ++j) suma += valores[j];
    salida[i] = suma / static_cast<float>(hasta - desde + 1);
  }
  return salida;
}

std::vector<Muestra> SuavizarPosiciones(const std::vector<Muestra>& entrada,
                                        int ventana, float intensidad) {
  if (ventana <= 0 || intensidad <= 0.0f || entrada.size() < 3) return entrada;
  std::vector<Muestra> salida = entrada;
  const int n = static_cast<int>(entrada.size());
  for (int i = 1; i + 1 < n; ++i) {
    const int desde = std::max(0, i - ventana);
    const int hasta = std::min(n - 1, i + ventana);
    Punto promedio{};
    for (int j = desde; j <= hasta; ++j) {
      promedio.x += entrada[j].posicion.x;
      promedio.y += entrada[j].posicion.y;
    }
    const float cantidad = static_cast<float>(hasta - desde + 1);
    promedio.x /= cantidad; promedio.y /= cantidad;
    salida[i].posicion.x = entrada[i].posicion.x * (1.0f - intensidad) +
                           promedio.x * intensidad;
    salida[i].posicion.y = entrada[i].posicion.y * (1.0f - intensidad) +
                           promedio.y * intensidad;
  }
  return salida;
}

// Normal unitaria a la dirección del trazo. Usa los vecinos en vez del punto
// anterior para que la orientación no salte en las curvas cerradas.
Punto NormalEn(const std::vector<Muestra>& muestras, size_t i) {
  const Punto& antes = muestras[i == 0 ? 0 : i - 1].posicion;
  const Punto& despues =
      muestras[std::min(muestras.size() - 1, i + 1)].posicion;
  Punto direccion = Resta(despues, antes);
  float largo = Longitud(direccion);
  if (largo < 1e-6f) {
    direccion = {1.0f, 0.0f};
    largo = 1.0f;
  }
  return {-direccion.y / largo, direccion.x / largo};
}

}  // namespace

Contorno CalcularContorno(const Trazo& trazo, const ParametrosPluma& params) {
  Contorno contorno;
  ParametrosPluma efectivos = params;
  if (trazo.estilo_persistente) {
    efectivos.opacidad = trazo.estilo.opacidad;
    efectivos.exponente_presion = trazo.estilo.exponente_presion;
    efectivos.factor_ancho_minimo = trazo.estilo.factor_ancho_minimo;
    efectivos.velocidad_ancho_minimo = trazo.estilo.velocidad_ancho_minimo;
    efectivos.suavizado_ancho = trazo.estilo.suavizado_ancho;
    efectivos.estabilizacion = trazo.estilo.estabilizacion;
    efectivos.taper_inicio = trazo.estilo.taper_inicio;
    efectivos.taper_fin = trazo.estilo.taper_fin;
    efectivos.sensibilidad_inclinacion = trazo.estilo.sensibilidad_inclinacion;
    efectivos.textura = trazo.estilo.textura;
    efectivos.nivel_refinado = trazo.estilo.nivel_refinado;
  }
  const int nivel = std::clamp(efectivos.nivel_refinado, 0, 3);
  const int ventana_posicion = static_cast<int>(std::round(
      std::clamp(efectivos.estabilizacion, 0.0f, 1.0f) * 6.0f)) + nivel * 2;
  const float intensidad = std::clamp(efectivos.estabilizacion * 0.65f +
      static_cast<float>(nivel) * 0.16f, 0.0f, 0.88f);
  const std::vector<Muestra> muestras_procesadas =
      SuavizarPosiciones(trazo.muestras, ventana_posicion, intensidad);
  const auto& muestras = muestras_procesadas;
  if (muestras.empty()) return contorno;

  // La frecuencia del contador solo hace falta para la modulación por
  // velocidad; se deduce de las propias marcas si no se pasa aparte.
  constexpr int64_t kFrecuenciaQpcSupuesta = 10'000'000;  // 100 ns por tick

  std::vector<float> anchos(muestras.size());
  for (size_t i = 0; i < muestras.size(); ++i) {
    float factor;
    if (muestras[i].tiene_presion()) {
      factor = std::pow(muestras[i].presion, efectivos.exponente_presion);
    } else {
      // Sin sensor de fuerza, el ancho lo da la velocidad: al escribir rápido
      // la línea adelgaza, igual que una pluma real.
      factor = 1.0f - VelocidadNormalizada(muestras, i, kFrecuenciaQpcSupuesta,
                                           efectivos.velocidad_ancho_minimo);
    }
    factor = std::clamp(factor, 0.0f, 1.0f);
    if (nivel >= 2) factor = factor * (nivel == 2 ? 0.72f : 0.45f) +
                              (nivel == 2 ? 0.28f : 0.55f);
    const float altitud = std::clamp(muestras[i].inclinacion_x, 0.0f,
                                     1.57079632679f);
    factor *= 1.0f + std::clamp(efectivos.sensibilidad_inclinacion, 0.0f, 1.0f) *
                       (1.0f - std::sin(altitud)) * 0.75f;
    const float progreso = muestras.size() <= 1 ? 1.0f
        : static_cast<float>(i) / static_cast<float>(muestras.size() - 1);
    if (efectivos.taper_inicio > 0.0f && progreso < efectivos.taper_inicio)
      factor *= std::max(0.05f, progreso / efectivos.taper_inicio);
    if (efectivos.taper_fin > 0.0f && 1.0f - progreso < efectivos.taper_fin)
      factor *= std::max(0.05f, (1.0f - progreso) / efectivos.taper_fin);
    if (efectivos.textura == 1)
      factor *= 0.92f + 0.08f * std::sin(static_cast<float>(i) * 2.37f);
    else if (efectivos.textura == 2 && i % 7 == 0)
      factor *= 0.58f;
    else if (efectivos.textura == 3)
      factor = std::max(factor, 0.82f);
    anchos[i] = trazo.ancho_base *
                (efectivos.factor_ancho_minimo +
                 (1.0f - efectivos.factor_ancho_minimo) * factor);
  }

  contorno.anchos = MediaMovil(anchos, efectivos.suavizado_ancho + nivel * 2);

  contorno.centros.reserve(muestras.size());
  contorno.izquierda.reserve(muestras.size());
  contorno.derecha.reserve(muestras.size());
  for (size_t i = 0; i < muestras.size(); ++i) {
    const Punto normal = NormalEn(muestras, i);
    const float mitad = contorno.anchos[i] * 0.5f;
    const Punto& p = muestras[i].posicion;
    contorno.centros.push_back(p);
    contorno.izquierda.push_back({p.x + normal.x * mitad, p.y + normal.y * mitad});
    contorno.derecha.push_back({p.x - normal.x * mitad, p.y - normal.y * mitad});
  }

  return contorno;
}

namespace {

void EmitirTriangulo(std::vector<Vertice>& salida, const Punto& a, const Punto& b,
                     const Punto& c) {
  salida.push_back({a.x, a.y});
  salida.push_back({b.x, b.y});
  salida.push_back({c.x, c.y});
}

// Casquete redondeado como abanico de triángulos alrededor del centro.
void EmitirCasquete(std::vector<Vertice>& salida, const Punto& centro,
                    float radio, int segmentos) {
  if (radio <= 0.0f || segmentos < 3) return;
  constexpr float kDosPi = 6.28318530717958647692f;
  Punto anterior{centro.x + radio, centro.y};
  for (int i = 1; i <= segmentos; ++i) {
    const float angulo = kDosPi * static_cast<float>(i) / segmentos;
    const Punto actual{centro.x + radio * std::cos(angulo),
                       centro.y + radio * std::sin(angulo)};
    EmitirTriangulo(salida, centro, anterior, actual);
    anterior = actual;
  }
}

}  // namespace

std::vector<Vertice> TeselarTrazo(const Trazo& trazo,
                                  const ParametrosPluma& params,
                                  int segmentos_casquete) {
  std::vector<Vertice> salida;
  const Contorno contorno = CalcularContorno(trazo, params);
  if (contorno.vacio()) return salida;

  const auto& izq = contorno.izquierda;
  const auto& der = contorno.derecha;
  const auto& centros = contorno.centros;

  // Un solo punto: es un toque, se dibuja el punto redondo y nada más.
  if (izq.size() == 1) {
    EmitirCasquete(salida, centros[0],
                   std::max(contorno.anchos[0] * 0.5f, 0.25f), segmentos_casquete);
    return salida;
  }

  // Dos triángulos por tramo, como una tira: (izq[i], der[i], izq[i+1]) y
  // (der[i], der[i+1], izq[i+1]).
  salida.reserve((izq.size() - 1) * 6 + segmentos_casquete * 6);
  for (size_t i = 0; i + 1 < izq.size(); ++i) {
    EmitirTriangulo(salida, izq[i], der[i], izq[i + 1]);
    EmitirTriangulo(salida, der[i], der[i + 1], izq[i + 1]);
  }

  // Casquetes en los extremos. Además de redondear las puntas, tapan la grieta
  // que dejan los tramos en las curvas cerradas del interior del giro.
  EmitirCasquete(salida, centros.front(),
                 std::max(contorno.anchos.front() * 0.5f, 0.25f),
                 segmentos_casquete);
  EmitirCasquete(salida, centros.back(),
                 std::max(contorno.anchos.back() * 0.5f, 0.25f),
                 segmentos_casquete);

  // Y una junta redonda en cada muestra interior, por el mismo motivo: sin ella
  // un giro brusco deja un pico visible.
  for (size_t i = 1; i + 1 < centros.size(); ++i) {
    EmitirCasquete(salida, centros[i],
                   std::max(contorno.anchos[i] * 0.5f, 0.25f),
                   std::max(4, segmentos_casquete / 2));
  }

  return salida;
}

std::vector<Muestra> PredecirContinuacion(const Trazo& trazo,
                                          const ParametrosPrediccion& params,
                                          int64_t frecuencia_qpc) {
  std::vector<Muestra> prediccion;
  const auto& m = trazo.muestras;
  if (m.size() < 3 || frecuencia_qpc <= 0 || params.ms_adelante <= 0.0f) {
    return prediccion;
  }

  const auto ms_entre = [&](size_t a, size_t b) {
    return 1000.0 * static_cast<double>(m[b].tiempo_qpc - m[a].tiempo_qpc) /
           frecuencia_qpc;
  };

  // Velocidad media de los últimos intervalos. Promediar aquí en vez de tomar
  // solo el último salto es lo que impide que una muestra ruidosa mande la
  // predicción a otro lado.
  const size_t disponibles = m.size();
  const size_t usar = std::min<size_t>(
      std::max(3, params.muestras_ajuste), disponibles);
  const size_t primero = disponibles - usar;

  Punto velocidad{0.0f, 0.0f};  // px/ms
  double peso_total = 0.0;
  for (size_t i = primero + 1; i < disponibles; ++i) {
    const double dt = ms_entre(i - 1, i);
    if (dt <= 0.0) continue;
    // Las muestras más recientes pesan más: son las que dicen hacia dónde va.
    const double peso = static_cast<double>(i - primero);
    velocidad.x += static_cast<float>((m[i].posicion.x - m[i - 1].posicion.x) / dt * peso);
    velocidad.y += static_cast<float>((m[i].posicion.y - m[i - 1].posicion.y) / dt * peso);
    peso_total += peso;
  }
  if (peso_total <= 0.0) return prediccion;
  velocidad.x /= static_cast<float>(peso_total);
  velocidad.y /= static_cast<float>(peso_total);

  // Aceleración por diferencia entre la velocidad de los dos últimos tramos.
  Punto aceleracion{0.0f, 0.0f};  // px/ms²
  const size_t n = disponibles;
  const double dt1 = ms_entre(n - 2, n - 1);
  const double dt0 = ms_entre(n - 3, n - 2);
  Punto v1{0.0f, 0.0f}, v0{0.0f, 0.0f};
  if (dt1 > 0.0 && dt0 > 0.0) {
    v1 = {static_cast<float>((m[n - 1].posicion.x - m[n - 2].posicion.x) / dt1),
          static_cast<float>((m[n - 1].posicion.y - m[n - 2].posicion.y) / dt1)};
    v0 = {static_cast<float>((m[n - 2].posicion.x - m[n - 3].posicion.x) / dt0),
          static_cast<float>((m[n - 2].posicion.y - m[n - 3].posicion.y) / dt0)};
    const double dt_medio = (dt1 + dt0) * 0.5;
    if (dt_medio > 0.0) {
      aceleracion.x = static_cast<float>((v1.x - v0.x) / dt_medio);
      aceleracion.y = static_cast<float>((v1.y - v0.y) / dt_medio);
    }
  }

  // Cuánto giró el trazo entre los dos últimos tramos. Si viene girando fuerte,
  // extrapolar en línea recta se pasa de largo — el latigazo clásico —, así que
  // se recorta la predicción de forma proporcional al giro.
  float confianza = 1.0f;
  const float largo1 = Longitud(v1);
  const float largo0 = Longitud(v0);
  if (largo1 > 1e-6f && largo0 > 1e-6f) {
    const float coseno =
        std::clamp((v1.x * v0.x + v1.y * v0.y) / (largo1 * largo0), -1.0f, 1.0f);
    const float angulo = std::acos(coseno);
    if (angulo > params.umbral_curvatura) {
      const float exceso = (angulo - params.umbral_curvatura) /
                           (3.14159265f - params.umbral_curvatura);
      confianza = std::clamp(1.0f - exceso * 2.0f, 0.0f, 1.0f);
    }
  }
  if (confianza <= 0.0f) return prediccion;

  // Presión: continuación lineal de la tendencia reciente, para que el tramo
  // predicho tenga un grosor plausible y no aparezca un escalón.
  const Muestra& ultima = m[n - 1];
  float deriva_presion = 0.0f;
  if (ultima.tiene_presion() && m[n - 2].tiene_presion() && dt1 > 0.0) {
    deriva_presion =
        static_cast<float>((ultima.presion - m[n - 2].presion) / dt1);
  }

  const int pasos = std::max(1, params.pasos);
  const float horizonte = params.ms_adelante * confianza;
  float recorrido = 0.0f;
  Punto anterior = ultima.posicion;

  for (int k = 1; k <= pasos; ++k) {
    const float t = horizonte * static_cast<float>(k) / static_cast<float>(pasos);

    Muestra futura;
    futura.posicion.x = ultima.posicion.x + velocidad.x * t +
                        0.5f * aceleracion.x * t * t * params.factor_aceleracion;
    futura.posicion.y = ultima.posicion.y + velocidad.y * t +
                        0.5f * aceleracion.y * t * t * params.factor_aceleracion;

    recorrido += Longitud(Resta(futura.posicion, anterior));
    if (recorrido > params.distancia_maxima_px) break;
    anterior = futura.posicion;

    futura.presion = ultima.tiene_presion()
                         ? std::clamp(ultima.presion + deriva_presion * t, 0.0f, 1.0f)
                         : kSinPresion;
    futura.inclinacion_x = ultima.inclinacion_x;
    futura.inclinacion_y = ultima.inclinacion_y;
    futura.tiempo_qpc =
        ultima.tiempo_qpc +
        static_cast<int64_t>(static_cast<double>(t) * frecuencia_qpc / 1000.0);

    prediccion.push_back(futura);
  }

  return prediccion;
}

double FrecuenciaMuestreoHz(const Trazo& trazo, int64_t frecuencia_qpc) {
  const auto& muestras = trazo.muestras;
  if (muestras.size() < 2 || frecuencia_qpc <= 0) return 0.0;
  const int64_t ticks = muestras.back().tiempo_qpc - muestras.front().tiempo_qpc;
  if (ticks <= 0) return 0.0;
  const double segundos = static_cast<double>(ticks) / frecuencia_qpc;
  return static_cast<double>(muestras.size() - 1) / segundos;
}

}  // namespace tinta
