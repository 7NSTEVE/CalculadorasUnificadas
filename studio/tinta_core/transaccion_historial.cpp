#include "transaccion_historial.h"

#include <utility>

namespace tinta {

TransaccionHistorial::TransaccionHistorial(Historial& historial, Pagina& pagina)
    : historial_(&historial), pagina_(&pagina) {
  if (historial.transaccion_activa_) return;

  try {
    pagina_antes_ = pagina;
  } catch (...) {
    historial_ = nullptr;
    pagina_ = nullptr;
    return;
  }

  // Las pilas potencialmente grandes se mueven, no se copian. A partir de aquí
  // las operaciones existentes escriben en pilas temporales vacías.
  hechas_antes_ = std::move(historial.hechas_);
  deshechas_antes_ = std::move(historial.deshechas_);
  historial.transaccion_activa_ = true;
  valida_ = true;
  activa_ = true;
}

TransaccionHistorial::~TransaccionHistorial() {
  if (activa_) Cancelar();
}

void TransaccionHistorial::RestaurarPilas() {
  if (!historial_) return;
  historial_->hechas_.clear();
  historial_->deshechas_.clear();
  historial_->hechas_ = std::move(hechas_antes_);
  historial_->deshechas_ = std::move(deshechas_antes_);
  historial_->transaccion_activa_ = false;
}

bool TransaccionHistorial::Confirmar() {
  if (!valida_ || !activa_ || !historial_ || !pagina_) return false;

  // Si ninguna suboperación registró historial, no se fabrica una entrada
  // vacía y se conserva incluso la rama Redo anterior.
  if (historial_->hechas_.empty()) {
    RestaurarPilas();
    activa_ = false;
    return true;
  }

  // Todo lo que puede asignar memoria se hace ANTES de tocar definitivamente
  // las pilas anteriores. Si falla el snapshot o el append, Cancelar restaura
  // página + Undo/Redo y no deja un comando aplicado sin posibilidad de Undo.
  try {
    Operacion agrupada = Historial::OperacionPaginaCompleta(
        pagina_antes_, *pagina_);
    hechas_antes_.push_back(std::move(agrupada));
  } catch (...) {
    return Cancelar();
  }

  // Una mutación confirmada invalida la rama Redo previa, igual que Registrar.
  deshechas_antes_.clear();
  RestaurarPilas();
  activa_ = false;
  return true;
}

bool TransaccionHistorial::Cancelar() {
  if (!valida_ || !activa_ || !historial_ || !pagina_) return false;

  // Pagina usa strings/vectores con allocators estándar; mover devuelve la
  // fotografía capturada sin volver a reservar la página completa.
  *pagina_ = std::move(pagina_antes_);
  RestaurarPilas();
  activa_ = false;
  return true;
}

}  // namespace tinta
