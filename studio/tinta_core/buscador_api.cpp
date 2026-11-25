#include "buscador_api.h"

#include <new>
#include <string>
#include <vector>

#include "buscador.h"

struct TintaBuscador {
  tinta::MotorBusqueda motor;
  tinta::Documento documento_dummy;
  std::vector<tinta::ResultadoBusqueda> resultados;
};

extern "C" TintaBuscador* tinta_buscador_crear(void) {
  return new (std::nothrow) TintaBuscador();
}

extern "C" void tinta_buscador_liberar(TintaBuscador* buscador) {
  delete buscador;
}

extern "C" void tinta_buscador_limpiar(TintaBuscador* buscador) {
  if (!buscador) return;
  buscador->resultados.clear();
  buscador->motor.Limpiar();
}

extern "C" bool tinta_buscador_reemplazar_ocr(
    TintaBuscador* buscador, const char* fuente_id,
    size_t cuaderno, size_t seccion, size_t pagina,
    float x, float y, float ancho, float alto,
    const char* texto, const char* fragmento) {
  if (!buscador || !fuente_id || !*fuente_id || !texto ||
      ancho < 0.0f || alto < 0.0f) {
    return false;
  }
  try {
    tinta::Ubicacion ubicacion{cuaderno, seccion, pagina};
    tinta::Rect area{x, y, ancho, alto};
    buscador->motor.ReemplazarOcr(
        fuente_id, ubicacion, area, texto,
        fragmento ? fragmento : "");
    buscador->resultados.clear();
    return true;
  } catch (...) {
    return false;
  }
}

extern "C" bool tinta_buscador_eliminar_ocr(TintaBuscador* buscador,
                                               const char* fuente_id) {
  if (!buscador || !fuente_id || !*fuente_id) return false;
  try {
    buscador->resultados.clear();
    return buscador->motor.EliminarOcr(fuente_id);
  } catch (...) {
    return false;
  }
}

extern "C" size_t tinta_buscador_buscar(TintaBuscador* buscador,
                                          const char* consulta) {
  if (!buscador || !consulta) return 0;
  try {
    buscador->resultados = buscador->motor.Buscar(
        consulta, buscador->documento_dummy);
    return buscador->resultados.size();
  } catch (...) {
    buscador->resultados.clear();
    return 0;
  }
}

extern "C" bool tinta_buscador_resultado(
    const TintaBuscador* buscador, size_t indice,
    TintaResultadoBusquedaC* salida) {
  if (!buscador || !salida || indice >= buscador->resultados.size()) {
    return false;
  }
  const auto& valor = buscador->resultados[indice];
  salida->cuaderno = valor.ubicacion.cuaderno;
  salida->seccion = valor.ubicacion.seccion;
  salida->pagina = valor.ubicacion.pagina;
  salida->x = valor.area.x;
  salida->y = valor.area.y;
  salida->ancho = valor.area.ancho;
  salida->alto = valor.area.alto;
  salida->puntuacion = valor.puntuacion;
  salida->fragmento = valor.fragmento.c_str();
  return true;
}
