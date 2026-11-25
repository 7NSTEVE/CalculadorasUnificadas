#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Lee únicamente la identidad durable embebida en un documento canónico.
// No modifica ninguna sesión ni adopta la identidad. `salida` recibe UTF-8
// terminado en NUL y la función falla si el archivo es inválido o no cabe.
bool tinta_documento_identidad_serializada(const uint8_t* datos,
                                            size_t cantidad,
                                            char* salida,
                                            size_t tam_salida);

#ifdef __cplusplus
}
#endif
