// Núcleo de tinta — compartido entre Windows, iOS/iPadOS y Android.
//
// Aquí no entra ninguna API de plataforma. Cada capa nativa aporta la entrada y
// la presentación; la forma del trazo se calcula aquí para que la tinta sea
// idéntica en las cuatro plataformas.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tinta {

struct Punto {
  float x = 0.0f;
  float y = 0.0f;
};

// Marca de que el digitalizador no reporta presión (ratón, o táctil sin sensor
// de fuerza). No es lo mismo que presión cero.
constexpr float kSinPresion = -1.0f;

// Una muestra tal como la entrega el hardware. Se guarda todo lo que reporta
// aunque hoy no se use: es el dato del que depende la fidelidad y no se puede
// reconstruir después.
struct Muestra {
  Punto posicion;
  float presion = kSinPresion;  // 0..1 normalizado
  float inclinacion_x = 0.0f;   // radianes
  float inclinacion_y = 0.0f;
  int64_t tiempo_qpc = 0;  // marca del evento de entrada, no del frame

  // Identidad efímera para reconciliar los valores estimados del digitalizador
  // con la corrección que llega después. No se serializa: solo tiene sentido
  // durante la sesión de entrada que recibió la muestra.
  uint64_t id_actualizacion = 0;

  bool tiene_presion() const { return presion != kSinPresion; }
};

// Aspecto persistente de un trazo. Las muestras originales nunca se cambian:
// estabilización y refinamiento solo alteran la geometría derivada al pintar.
struct EstiloTrazo {
  float opacidad = 1.0f;
  float exponente_presion = 0.7f;
  float factor_ancho_minimo = 0.25f;
  float velocidad_ancho_minimo = 3.0f;
  int suavizado_ancho = 4;
  float estabilizacion = 0.0f;       // 0..1
  float taper_inicio = 0.0f;         // fracción de longitud 0..0.5
  float taper_fin = 0.0f;
  float sensibilidad_inclinacion = 0.0f;
  int textura = 0;                   // 0 sólida, 1 lápiz, 2 tinta seca, 3 marcador
  int nivel_refinado = 0;            // 0 original, 1 suave, 2 legible, 3 limpio
};

struct Trazo {
  // Identidad estable para sincronización idempotente. No describe el orden ni
  // la geometría; permite aplicar dos veces la misma operación sin duplicarla.
  // Los archivos anteriores a v4 la dejan vacía y la capa nativa la asigna al
  // editar o compartir el trazo por primera vez.
  std::string id_colaboracion;
  std::vector<Muestra> muestras;
  float ancho_base = 4.0f;
  uint32_t color_argb = 0xFF1A1A1Au;
  bool resaltador = false;
  bool estilo_persistente = false;
  EstiloTrazo estilo;
};

// Ajustes de la pluma. Los valores por defecto salen del prototipo del
// 2026-08-04 y están puestos para que la respuesta sea viva en el rango bajo,
// que es donde se escribe de verdad.
struct ParametrosPluma {
  // Por debajo de 1 la tinta reacciona antes a la presión suave.
  float exponente_presion = 0.7f;
  // Ancho mínimo como fracción del ancho base.
  float factor_ancho_minimo = 0.25f;
  // Velocidad, en píxeles por milisegundo, a la que el trazo llega a su ancho
  // mínimo cuando el dispositivo no reporta presión.
  float velocidad_ancho_minimo = 3.0f;
  // Ventana del promediado de anchos. Sin esto el trazo tiembla en cada muestra
  // ruidosa del sensor.
  int suavizado_ancho = 4;
  float opacidad = 1.0f;
  float estabilizacion = 0.0f;
  float taper_inicio = 0.0f;
  float taper_fin = 0.0f;
  float sensibilidad_inclinacion = 0.0f;
  int textura = 0;
  int nivel_refinado = 0;
};

// Los dos bordes del trazo y el ancho de cada muestra.
//
// Se devuelven los bordes en crudo, sin convertirlos a ningún tipo de camino
// gráfico: cada capa nativa construye su propia geometría (ID2D1PathGeometry,
// MTLBuffer, etc.) sin que el núcleo dependa de ninguna.
struct Contorno {
  std::vector<Punto> centros;
  std::vector<Punto> izquierda;
  std::vector<Punto> derecha;
  std::vector<float> anchos;

  bool vacio() const { return izquierda.empty(); }
};

// Calcula el contorno de ancho variable de un trazo.
//
// El trazo no se dibuja como una línea gruesa: se calcula su polígono real, que
// es lo que permite que el ancho cambie muestra a muestra. Una línea de ancho
// constante parece un rotulador, no una pluma.
Contorno CalcularContorno(const Trazo& trazo, const ParametrosPluma& params);

// Frecuencia de muestreo efectiva del trazo en Hz, a partir de las marcas de
// tiempo del hardware. Es la medida que dice si se está aprovechando el
// digitalizador o si se están perdiendo muestras por el camino.
double FrecuenciaMuestreoHz(const Trazo& trazo, int64_t frecuencia_qpc);

// Un vértice en píxeles lógicos. El color va como uniforme por trazo, no por
// vértice: ahorra tres cuartas partes del ancho de banda del buffer y un trazo
// nunca cambia de color a mitad.
struct Vertice {
  float x = 0.0f;
  float y = 0.0f;
};

// Convierte el trazo en triángulos sueltos, tres vértices cada uno.
//
// Existe para Metal y para Vulkan, que no saben de caminos: solo de triángulos.
// La capa de Windows usa ID2D1PathGeometry y no necesita esto, pero la
// teselación vive igual en el núcleo para que las tres plataformas rellenen
// exactamente la misma forma.
std::vector<Vertice> TeselarTrazo(const Trazo& trazo,
                                  const ParametrosPluma& params,
                                  int segmentos_casquete = 8);

// Ajustes de la predicción de trazo.
struct ParametrosPrediccion {
  // Cuánto tiempo hacia delante se extrapola. Se debe fijar a la latencia medida
  // de evento a pantalla: predecir más produce adelanto visible, predecir menos
  // deja parte del retraso sin tapar.
  float ms_adelante = 12.0f;
  // Cuántas muestras se usan para estimar velocidad y aceleración. Pocas siguen
  // el ruido del sensor; muchas reaccionan tarde a los cambios de dirección.
  int muestras_ajuste = 5;
  // En cuántos puntos se subdivide el tramo predicho.
  int pasos = 3;
  // Cuánto se le cree a la aceleración. Por debajo de 1 porque la aceleración
  // estimada por diferencias finitas es ruidosa y al elevarla al cuadrado el
  // error se dispara.
  float factor_aceleracion = 0.45f;
  // Tope duro de lo que puede adelantarse la predicción.
  float distancia_maxima_px = 48.0f;
  // Cambio de dirección, en radianes, a partir del cual se recorta la
  // predicción. Es lo que evita el latigazo al invertir el trazo de golpe.
  float umbral_curvatura = 0.30f;
};

// Extrapola la continuación del trazo hacia delante en el tiempo.
//
// Las muestras devueltas **se dibujan pero no se guardan**: en cuanto llegan las
// reales del digitalizador se descartan. Nunca entran al modelo de datos, así
// que no contaminan el archivo ni la sincronización.
//
// El artefacto clásico de cualquier predictor es pasarse en los cambios bruscos
// de dirección. Aquí se controla midiendo el ángulo entre las dos últimas
// velocidades y recortando la predicción cuando el trazo gira.
std::vector<Muestra> PredecirContinuacion(const Trazo& trazo,
                                          const ParametrosPrediccion& params,
                                          int64_t frecuencia_qpc);

}  // namespace tinta
