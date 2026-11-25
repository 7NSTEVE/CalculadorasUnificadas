// almacen.h
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "documento.h"

namespace tinta {

struct MetadatosCuaderno {
  std::string id;
  std::string titulo;
  int64_t fecha_creacion;
  int64_t fecha_modificacion;
};

void ConfigurarRutaAlmacen(const std::string& ruta_base);

std::vector<MetadatosCuaderno> ListarCuadernos();

bool GuardarCuaderno(Documento& doc);

bool CargarCuaderno(const std::string& id, Documento* doc);

} // namespace tinta
