#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Extrae un contenedor ZIP/PPTX a una carpeta nueva y validada. Devuelve 1 si
/// terminó; 0 si el archivo es inválido, cifrado, excede límites o intenta
/// escapar de la carpeta destino.
int tinta_pptx_extraer(const char* archivo_zip,
                       const char* carpeta_destino,
                       char* mensaje_error,
                       size_t capacidad_error);
int tinta_zip_extraer_seguro(const char* archivo_zip,
                             const char* carpeta_destino,
                             char* mensaje_error,
                             size_t capacidad_error);

#ifdef __cplusplus
}
#endif
