#include "elementos.h"

#include <algorithm>

namespace tinta {

bool Rect::Contiene(const Punto& p) const {
  return p.x >= x && p.x <= derecha() && p.y >= y && p.y <= abajo();
}

bool Rect::Intersecta(const Rect& otro) const {
  return !(otro.x > derecha() || otro.derecha() < x || otro.y > abajo() ||
           otro.abajo() < y);
}

bool Medio::recorte_valido() const {
  if (recorte_ancho <= 0.0f || recorte_alto <= 0.0f) return false;
  if (recorte_x < 0.0f || recorte_y < 0.0f) return false;
  if (recorte_x + recorte_ancho > 1.0001f) return false;
  if (recorte_y + recorte_alto > 1.0001f) return false;
  return true;
}

Rect Medio::RegionVisible(float ancho_original, float alto_original) const {
  Rect region;
  region.x = recorte_x * ancho_original;
  region.y = recorte_y * alto_original;
  region.ancho = recorte_ancho * ancho_original;
  region.alto = recorte_alto * alto_original;
  return region;
}

void RecortarRelativo(Medio* medio, float x, float y, float ancho, float alto) {
  if (!medio) return;
  if (ancho <= 0.0f || alto <= 0.0f) return;

  // El recorte nuevo viene en fracciones de lo que hoy se ve, no del original.
  // Componerlo así es lo que permite recortar dos veces seguidas sin que la
  // segunda se calcule contra la imagen entera.
  const float x0 = std::clamp(x, 0.0f, 1.0f);
  const float y0 = std::clamp(y, 0.0f, 1.0f);
  const float w = std::clamp(ancho, 0.0f, 1.0f - x0);
  const float h = std::clamp(alto, 0.0f, 1.0f - y0);

  medio->recorte_x += x0 * medio->recorte_ancho;
  medio->recorte_y += y0 * medio->recorte_alto;
  medio->recorte_ancho *= w;
  medio->recorte_alto *= h;
}

}  // namespace tinta
