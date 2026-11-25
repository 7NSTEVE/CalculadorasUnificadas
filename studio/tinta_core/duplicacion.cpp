#include "duplicacion.h"

#include <cstring>
#include <new>
#include <vector>

#include "archivo.h"
#include "documento.h"

extern "C" bool tinta_documento_identidad_serializada(
    const uint8_t* datos, size_t cantidad, char* salida, size_t tam_salida) {
  if (!datos || cantidad == 0 || cantidad > tinta::kMaximoBytesArchivo ||
      !salida || tam_salida == 0) {
    return false;
  }

  std::vector<uint8_t> copia;
  try {
    copia.assign(datos, datos + cantidad);
  } catch (const std::bad_alloc&) {
    return false;
  }

  tinta::Documento documento;
  if (!tinta::Deserializar(copia, &documento) || documento.id.empty() ||
      documento.id.size() + 1 > tam_salida) {
    return false;
  }

  std::memcpy(salida, documento.id.data(), documento.id.size());
  salida[documento.id.size()] = '\0';
  return true;
}
