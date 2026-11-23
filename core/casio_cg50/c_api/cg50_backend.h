// Alias de inclusión (2026-10-30) — ABI C fase F1.
//
// rama-cg50_backend.cpp (y el test de contrato de rama) incluyen el nombre
// histórico "cg50_backend.h". Para que el blanco de la ABI C sea la variante
// nativa pura —la que NO conoce GCal: rama-cg50_backend.{h,cpp}— este alias
// redirige la inclusión al header de rama. Por resolución de includes con
// comillas, un TU del mismo directorio encuentra SIEMPRE este alias antes de
// las rutas -I; así el backend de la raíz (que sí enlaza GCal con DLLs
// 32-bit CASIO) queda estructuralmente fuera del destino de la ABI.
#pragma once
#include "rama-cg50_backend.h"
