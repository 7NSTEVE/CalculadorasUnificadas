#include "transaccion_historial_api.h"

#include <new>

#include "sesion_interna_compat.h"
#include "transaccion_historial.h"

struct TintaTransaccionHistorial {
  explicit TintaTransaccionHistorial(TintaSesion* s)
      : sesion(s), transaccion(s->historial, s->pagina) {}

  TintaSesion* sesion;
  tinta::TransaccionHistorial transaccion;
};

extern "C" TintaTransaccionHistorial* tinta_historial_transaccion_comenzar(
    TintaSesion* sesion) {
  if (!sesion || sesion->historial.transaccion_activa()) return nullptr;
  auto* valor = new (std::nothrow) TintaTransaccionHistorial(sesion);
  if (!valor) return nullptr;
  if (!valor->transaccion.valida()) {
    delete valor;
    return nullptr;
  }
  return valor;
}

extern "C" bool tinta_historial_transaccion_confirmar(
    TintaTransaccionHistorial* transaccion) {
  if (!transaccion) return false;
  const bool resultado = transaccion->transaccion.Confirmar();
  delete transaccion;
  return resultado;
}

extern "C" void tinta_historial_transaccion_cancelar(
    TintaTransaccionHistorial* transaccion) {
  if (!transaccion) return;
  transaccion->transaccion.Cancelar();
  delete transaccion;
}
