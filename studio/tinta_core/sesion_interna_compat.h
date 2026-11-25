// Definición interna compartida exclusivamente por extensiones del API que
// necesitan operar sobre TintaSesion sin agrandar api.cpp.
//
// IMPORTANTE: el cuerpo de `struct TintaSesion` debe permanecer token-a-token
// equivalente al de api.cpp. C++ permite la misma definición de clase en varias
// unidades de traducción bajo ODR; no se debe añadir estado sólo aquí.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "cronologia.h"
#include "documento.h"
#include "edicion.h"
#include "tinta.h"

struct TintaSesion {
  tinta::Pagina pagina;
  tinta::Historial historial;
  tinta::CronologiaDocumento cronologia;
  int64_t tiempo_cronologia_qpc = 0;

  tinta::Trazo en_curso;
  bool hay_trazo = false;
  std::vector<tinta::Muestra> provisionales;
  std::unordered_map<uint64_t, tinta::Muestra> muestras_actualizables;
  uint64_t siguiente_id_muestra = 1;

  tinta::ParametrosPluma params;
  tinta::ParametrosPrediccion params_prediccion;
  bool prediccion_activa = true;
  size_t muestras_predichas = 0;

  std::vector<tinta::Vertice> vertices_en_curso;
  std::vector<tinta::Vertice> vertices_trazo;
  std::vector<float> puntos_trazo;
  std::vector<float> pauta;
  std::vector<float> pauta_avanzada;
  std::vector<float> cortes_impresion;
  std::vector<size_t> indices_salida;
  std::vector<size_t> indices_textos_salida;
  std::vector<size_t> indices_medios_salida;
  std::vector<uint8_t> bytes;
  std::string texto_salida;
  uint64_t contador_copias = 0;

  std::string documento_id;
  std::string documento_titulo;
};
