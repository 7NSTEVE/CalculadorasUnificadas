#include "foco.h"

namespace tinta {

ConfiguracionVisibilidad ElementosVisiblesEnFoco(NivelFoco nivel) {
  switch (nivel) {
    case NivelFoco::Pagina:
      return {true, true, true, true, false};
    case NivelFoco::Problema:
      // Problema: oculta minimapa para concentrarse en una pregunta.
      return {true, false, true, true, false};
    case NivelFoco::Region:
      // Región: similar a Problema, puede ocultar reloj.
      return {true, false, true, false, false};
    case NivelFoco::Clase:
      // Clase: bloquea notificaciones pero permite herramientas.
      return {true, false, true, true, true};
    case NivelFoco::DeepStudy:
      // DeepStudy: bloquea distracciones, maximiza el área.
      return {false, false, false, false, true};
    case NivelFoco::Examen:
      // Examen: Bloqueo total. Solo herramientas básicas, sin cambio de app.
      return {false, false, false, true, true};
  }
  return {true, true, true, true, false};
}

GestorFoco::GestorFoco() {
  estado_.nivel = NivelFoco::Pagina;
  estado_.region_objetivo = std::nullopt;
}

ConfiguracionVisibilidad GestorFoco::ObtenerVisibilidadActual() const {
  return ElementosVisiblesEnFoco(estado_.nivel);
}

bool GestorFoco::CambiarNivel(NivelFoco nuevo_nivel, std::optional<Rect> region) {
  // Regla 1: El nivel Region requiere una región específica válida (no nula y no vacía).
  if (nuevo_nivel == NivelFoco::Region) {
    if (!region.has_value() || region->vacio()) {
      return false; // Transición inválida
    }
  }

  // Regla 2: Examen no permite salir fácilmente, pero a nivel de núcleo 
  // permitimos la transición de estado (la interfaz nativa debería bloquear la UI).

  estado_.nivel = nuevo_nivel;
  estado_.region_objetivo = region;
  return true;
}

}  // namespace tinta
