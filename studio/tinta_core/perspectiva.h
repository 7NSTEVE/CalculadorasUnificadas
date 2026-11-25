#pragma once

#include "tinta.h"
#include <array>

namespace tinta {

// Representa las cuatro esquinas de una pizarra o papel detectado en una foto,
// junto con las dimensiones de la imagen original.
struct ModeloFotoPizarra {
  std::array<Punto, 4> esquinas_origen; // Arriba-izquierda, arriba-derecha, abajo-derecha, abajo-izquierda
  float ancho_imagen_original = 0.0f;
  float alto_imagen_original = 0.0f;
};

// Matriz de transformación de perspectiva 3x3 en orden de fila principal.
using MatrizPerspectiva = std::array<float, 9>;

// Calcula la matriz de transformación de perspectiva (homografía) 3x3
// dada una correspondencia de 4 puntos origen a 4 puntos destino.
MatrizPerspectiva CalcularMatrizPerspectiva(const std::array<Punto, 4>& origen,
                                            const std::array<Punto, 4>& destino);

// Aplica la matriz de transformación de perspectiva a un Punto bidimensional.
Punto AplicarPerspectiva(const MatrizPerspectiva& matriz, const Punto& p);

// Parámetros para corregir el contraste, ideal para resaltar rotuladores de pizarra.
struct CorreccionContraste {
  float nivel_negro = 0.0f; // Los valores menores o iguales se vuelven 0.0 (negro)
  float nivel_blanco = 1.0f; // Los valores mayores o iguales se vuelven 1.0 (blanco)
};

// Aplica la corrección de contraste a un valor normalizado (0.0 a 1.0).
float AplicarCorreccionContraste(float valor, const CorreccionContraste& correccion);

}  // namespace tinta
