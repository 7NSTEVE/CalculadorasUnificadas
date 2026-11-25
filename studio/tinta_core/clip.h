#pragma once
#include <string>
#include <vector>
#include "tinta.h"
#include "elementos.h"

namespace tinta {

enum class FormaClip {
  Rectangulo,
  Poligono,
  Elipse,
  Libre // Basado en un trazo
};

// "Source-Aware Clip" - Un recorte que sabe de dónde vino.
struct OrigenClip {
  std::string id_documento_original;
  std::string id_pagina_original;
  float recorte_x = 0.0f;
  float recorte_y = 0.0f;
  float recorte_ancho = 0.0f;
  float recorte_alto = 0.0f;
};

struct RecorteClip {
  std::string id;
  FormaClip forma = FormaClip::Rectangulo;
  
  // Los vértices si la forma es Poligono o Libre. Para Rectángulo o Elipse
  // se usa la caja envolvente (marco).
  std::vector<Punto> vertices;
  Rect marco;
  
  OrigenClip origen;
};

// Construye un Medio a partir de un RecorteClip
Medio MedioDesdeClip(const RecorteClip& clip, const std::string& hash_imagen);

}
