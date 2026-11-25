// Gestor de Foco (Modo Foco)
#pragma once

#include <optional>
#include "elementos.h"

namespace tinta {

enum class NivelFoco { 
  Pagina, 
  Problema, 
  Region, 
  Clase, 
  DeepStudy, 
  Examen 
};

struct ConfiguracionVisibilidad {
  bool mostrar_barra_herramientas;
  bool mostrar_minimapa;
  bool permitir_cambio_app;
  bool mostrar_reloj;
  bool bloquear_notificaciones;
};

struct EstadoFoco {
  NivelFoco nivel = NivelFoco::Pagina;
  std::optional<Rect> region_objetivo = std::nullopt;
};

ConfiguracionVisibilidad ElementosVisiblesEnFoco(NivelFoco nivel);

class GestorFoco {
 public:
  GestorFoco();

  const EstadoFoco& ObtenerEstado() const { return estado_; }
  ConfiguracionVisibilidad ObtenerVisibilidadActual() const;

  // Cambia el nivel de foco. Retorna false si la transición no es válida 
  // (por ejemplo, Region requiere un Rect válido).
  bool CambiarNivel(NivelFoco nuevo_nivel, std::optional<Rect> region = std::nullopt);
  
 private:
  EstadoFoco estado_;
};

}  // namespace tinta
