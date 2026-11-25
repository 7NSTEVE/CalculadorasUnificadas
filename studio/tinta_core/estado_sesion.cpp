#include "estado_sesion.h"

#include <sstream>
#include <string>
#include <cstdlib>

namespace tinta {

std::string SerializarEstadoSesion(const EstadoSesion& estado) {
  std::ostringstream out;
  out << "DOC:" << estado.documento_id << "\n";
  out << "PAG:" << estado.indice_pagina << "\n";
  out << "X:" << estado.vista_x << "\n";
  out << "Y:" << estado.vista_y << "\n";
  out << "ZOOM:" << estado.vista_zoom << "\n";
  out << "HERRAMIENTA:" << estado.herramienta_activa << "\n";
  for (uint32_t color : estado.colores_seleccionados) {
    out << "COLOR:" << color << "\n";
  }
  return out.str();
}

bool DeserializarEstadoSesion(const std::string& datos, EstadoSesion* estado) {
  if (!estado) return false;
  
  estado->documento_id.clear();
  estado->indice_pagina = 0;
  estado->vista_x = 0.0f;
  estado->vista_y = 0.0f;
  estado->vista_zoom = 1.0f;
  estado->herramienta_activa.clear();
  estado->colores_seleccionados.clear();

  std::istringstream in(datos);
  std::string linea;
  while (std::getline(in, linea)) {
    if (linea.empty()) continue;
    
    // Quitar posible '\r' al final si el archivo viene de Windows
    if (linea.back() == '\r') {
      linea.pop_back();
    }

    size_t pos = linea.find(':');
    if (pos == std::string::npos) continue;

    std::string clave = linea.substr(0, pos);
    std::string valor = linea.substr(pos + 1);

    if (clave == "DOC") {
      estado->documento_id = valor;
    } else if (clave == "PAG") {
      estado->indice_pagina = static_cast<int>(std::strtol(valor.c_str(), nullptr, 10));
    } else if (clave == "X") {
      estado->vista_x = std::strtof(valor.c_str(), nullptr);
    } else if (clave == "Y") {
      estado->vista_y = std::strtof(valor.c_str(), nullptr);
    } else if (clave == "ZOOM") {
      estado->vista_zoom = std::strtof(valor.c_str(), nullptr);
    } else if (clave == "HERRAMIENTA") {
      estado->herramienta_activa = valor;
    } else if (clave == "COLOR") {
      estado->colores_seleccionados.push_back(
          static_cast<uint32_t>(std::strtoul(valor.c_str(), nullptr, 10)));
    }
  }

  return true;
}

}  // namespace tinta
