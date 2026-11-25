// Estado de sesión y bandeja de entrada (Inbox).
//
// Mantiene dónde se quedó el usuario al cerrar la app para poder reanudar
// exactamente en el mismo sitio y con las mismas herramientas.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "documento.h"

namespace tinta {

// Estado exacto de la vista para poder reanudar la sesión.
struct EstadoSesion {
  std::string documento_id;
  int indice_pagina = 0;
  
  // Viewport: posición y escala.
  float vista_x = 0.0f;
  float vista_y = 0.0f;
  float vista_zoom = 1.0f;
  
  std::string herramienta_activa;
  std::vector<uint32_t> colores_seleccionados;
};

// Bandeja de entrada para capturas rápidas sin clasificar.
struct Inbox {
  std::vector<Pagina> paginas_temporales;
  std::vector<Trazo> trazos_temporales;
};

// Serializa el estado a un formato de texto simple.
std::string SerializarEstadoSesion(const EstadoSesion& estado);

// Reconstruye el estado a partir del texto. Devuelve false si el formato es inválido.
bool DeserializarEstadoSesion(const std::string& datos, EstadoSesion* estado);

}  // namespace tinta
