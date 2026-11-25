// Reconocimiento de formas dibujadas a mano.
//
// Cuando alguien dibuja un círculo torcido y levanta el lápiz, la forma se
// endereza. Es geometría pura, así que vive en el núcleo y se prueba entera sin
// gastar compilaciones de plataforma.
//
// También reconoce la flecha de un solo trazo cuando existe un asta dominante
// y dos alas cortas confirmables en su punta. Los garabatos ambiguos se dejan
// intactos: el reconocimiento nunca debe destruir la escritura original.
#pragma once

#include <vector>

#include "tinta.h"

namespace tinta {

enum class TipoForma {
  Ninguna,
  Linea,
  Rectangulo,
  Elipse,
  Triangulo,
  // Agregado al final para conservar los enteros persistidos de las formas
  // anteriores y el contrato del API C.
  Flecha,
};

struct FormaReconocida {
  TipoForma tipo = TipoForma::Ninguna;
  // 0..1. Por debajo del umbral de `ReconocerForma` se devuelve Ninguna: es
  // preferible dejar el trazo a mano que enderezar algo que no era una forma.
  float confianza = 0.0f;
  // Vértices ideales. En la elipse son las cuatro esquinas de su caja.
  std::vector<Punto> vertices;
};

struct ParametrosForma {
  // Cuánto puede desviarse un trazo de la recta para seguir siendo una línea,
  // como fracción de su longitud.
  float tolerancia_linea = 0.045f;
  // Distancia entre el principio y el final, como fracción del recorrido, por
  // debajo de la cual el trazo se considera cerrado.
  float tolerancia_cierre = 0.22f;
  // Dispersión máxima del radio para aceptar una elipse.
  float tolerancia_elipse = 0.18f;
  // Tolerancia del simplificador, como fracción de la diagonal de la caja.
  float tolerancia_simplificado = 0.06f;
  // Distancia máxima entre las dos visitas a la punta de una flecha, como
  // fracción de la longitud de su asta.
  float tolerancia_punta_flecha = 0.18f;
  // Confianza mínima para devolver algo distinto de Ninguna.
  float confianza_minima = 0.55f;
};

// Simplifica una polilínea quitando los puntos que no cambian su forma.
// Es Ramer–Douglas–Peucker; se expone porque sirve además para aligerar trazos
// al guardar.
std::vector<Punto> Simplificar(const std::vector<Punto>& puntos, float tolerancia);

FormaReconocida ReconocerForma(const Trazo& trazo,
                               const ParametrosForma& params = {});

// Construye el trazo ideal de una forma, conservando color y grosor del
// original. Devuelve un trazo vacío si la forma es Ninguna.
Trazo TrazoDesdeForma(const FormaReconocida& forma, const Trazo& original,
                      int muestras_por_curva = 48);

}  // namespace tinta
