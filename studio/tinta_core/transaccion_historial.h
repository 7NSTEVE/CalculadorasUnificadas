#pragma once

#include <vector>

#include "edicion.h"

namespace tinta {

/// Agrupa varias mutaciones ya existentes como una sola entrada de Undo sin
/// pedir a cada operación que conozca el concepto de transacción.
///
/// Al comenzar, las pilas anteriores se mueven fuera de Historial (O(1)) y las
/// operaciones internas escriben en pilas temporales normales. Confirmar
/// descarta esas entradas temporales, restaura las pilas originales y añade un
/// único snapshot de página. Cancelar restaura también la página.
class TransaccionHistorial {
 public:
  TransaccionHistorial(Historial& historial, Pagina& pagina);
  ~TransaccionHistorial();

  TransaccionHistorial(const TransaccionHistorial&) = delete;
  TransaccionHistorial& operator=(const TransaccionHistorial&) = delete;
  TransaccionHistorial(TransaccionHistorial&&) = delete;
  TransaccionHistorial& operator=(TransaccionHistorial&&) = delete;

  bool valida() const { return valida_; }
  bool activa() const { return activa_; }
  bool Confirmar();
  bool Cancelar();

 private:
  void RestaurarPilas();

  Historial* historial_ = nullptr;
  Pagina* pagina_ = nullptr;
  Pagina pagina_antes_;
  std::vector<Operacion> hechas_antes_;
  std::vector<Operacion> deshechas_antes_;
  bool valida_ = false;
  bool activa_ = false;
};

}  // namespace tinta
