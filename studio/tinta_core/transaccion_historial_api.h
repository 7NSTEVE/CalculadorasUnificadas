#pragma once

#include <stdbool.h>

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TintaTransaccionHistorial TintaTransaccionHistorial;

// Agrupa las mutaciones hechas hasta confirmar/cancelar como una sola entrada
// de Undo. Devuelve NULL si ya existe otra transacción sobre la sesión o si no
// hay memoria suficiente para copiar el estado anterior de la página.
TintaTransaccionHistorial* tinta_historial_transaccion_comenzar(
    TintaSesion* sesion);

// Confirma y libera el handle. `true` también es válido si no hubo mutaciones:
// en ese caso no se fabrica una entrada vacía de historial.
bool tinta_historial_transaccion_confirmar(
    TintaTransaccionHistorial* transaccion);

// Revierte página + pilas Undo/Redo y libera el handle. Es seguro pasar NULL.
void tinta_historial_transaccion_cancelar(
    TintaTransaccionHistorial* transaccion);

#ifdef __cplusplus
}
#endif
