#include "perspectiva.h"
#include <cmath>
#include <algorithm>
#include <utility>

namespace tinta {

namespace {

// Resuelve un sistema de ecuaciones lineales 8x8 mediante eliminación gaussiana simple.
// Se usa para calcular la homografía.
bool ResolverSistema8(float A[8][9], float x[8]) {
  for (int i = 0; i < 8; ++i) {
    int max_row = i;
    float max_val = std::abs(A[i][i]);
    for (int k = i + 1; k < 8; ++k) {
      float val = std::abs(A[k][i]);
      if (val > max_val) {
        max_row = k;
        max_val = val;
      }
    }
    
    if (max_val < 1e-6f) {
      return false; // Sistema singular o cercano a ser singular
    }
    
    if (max_row != i) {
      for (int j = i; j < 9; ++j) {
        std::swap(A[i][j], A[max_row][j]);
      }
    }
    
    for (int k = i + 1; k < 8; ++k) {
      float factor = A[k][i] / A[i][i];
      for (int j = i; j < 9; ++j) {
        A[k][j] -= factor * A[i][j];
      }
    }
  }
  
  for (int i = 7; i >= 0; --i) {
    float suma = 0.0f;
    for (int j = i + 1; j < 8; ++j) {
      suma += A[i][j] * x[j];
    }
    x[i] = (A[i][8] - suma) / A[i][i];
  }
  
  return true;
}

} // namespace anónimo

MatrizPerspectiva CalcularMatrizPerspectiva(const std::array<Punto, 4>& origen,
                                            const std::array<Punto, 4>& destino) {
  float A[8][9] = {};
  
  for (int i = 0; i < 4; ++i) {
    float x = origen[i].x;
    float y = origen[i].y;
    float u = destino[i].x;
    float v = destino[i].y;
    
    // Ecuación para u
    A[2*i][0] = x;
    A[2*i][1] = y;
    A[2*i][2] = 1.0f;
    A[2*i][3] = 0.0f;
    A[2*i][4] = 0.0f;
    A[2*i][5] = 0.0f;
    A[2*i][6] = -u * x;
    A[2*i][7] = -u * y;
    A[2*i][8] = u;
    
    // Ecuación para v
    A[2*i+1][0] = 0.0f;
    A[2*i+1][1] = 0.0f;
    A[2*i+1][2] = 0.0f;
    A[2*i+1][3] = x;
    A[2*i+1][4] = y;
    A[2*i+1][5] = 1.0f;
    A[2*i+1][6] = -v * x;
    A[2*i+1][7] = -v * y;
    A[2*i+1][8] = v;
  }
  
  float h[8];
  if (ResolverSistema8(A, h)) {
    return {h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], 1.0f};
  }
  
  // En caso de fallo (puntos colineales, etc.), devuelve matriz identidad
  return {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
}

Punto AplicarPerspectiva(const MatrizPerspectiva& matriz, const Punto& p) {
  float x = p.x;
  float y = p.y;
  
  float u = matriz[0] * x + matriz[1] * y + matriz[2];
  float v = matriz[3] * x + matriz[4] * y + matriz[5];
  float w = matriz[6] * x + matriz[7] * y + matriz[8];
  
  if (std::abs(w) > 1e-6f) {
    return {u / w, v / w};
  }
  return {u, v};
}

float AplicarCorreccionContraste(float valor, const CorreccionContraste& correccion) {
  float rango = correccion.nivel_blanco - correccion.nivel_negro;
  if (std::abs(rango) < 1e-6f) {
    return valor; // Evita división por cero si configurado incorrectamente
  }
  float ajustado = (valor - correccion.nivel_negro) / rango;
  return std::clamp(ajustado, 0.0f, 1.0f);
}

}  // namespace tinta
