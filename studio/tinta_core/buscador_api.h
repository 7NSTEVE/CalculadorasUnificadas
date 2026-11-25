#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TintaBuscador TintaBuscador;

typedef struct {
  size_t cuaderno;
  size_t seccion;
  size_t pagina;
  float x;
  float y;
  float ancho;
  float alto;
  float puntuacion;
  const char* fragmento;
} TintaResultadoBusquedaC;

TintaBuscador* tinta_buscador_crear(void);
void tinta_buscador_liberar(TintaBuscador* buscador);
void tinta_buscador_limpiar(TintaBuscador* buscador);

bool tinta_buscador_reemplazar_ocr(
    TintaBuscador* buscador, const char* fuente_id,
    size_t cuaderno, size_t seccion, size_t pagina,
    float x, float y, float ancho, float alto,
    const char* texto, const char* fragmento);

bool tinta_buscador_eliminar_ocr(TintaBuscador* buscador,
                                 const char* fuente_id);

// Ejecuta la búsqueda y conserva los resultados dentro del handle hasta la
// siguiente búsqueda/limpieza/liberación. Los punteros `fragmento` no deben
// almacenarse fuera de esa ventana.
size_t tinta_buscador_buscar(TintaBuscador* buscador, const char* consulta);
bool tinta_buscador_resultado(const TintaBuscador* buscador, size_t indice,
                              TintaResultadoBusquedaC* salida);

#ifdef __cplusplus
}
#endif
