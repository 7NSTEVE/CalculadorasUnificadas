#pragma once

#include <string>
#include <vector>

#include "documento.h"

namespace tinta {

// Resultado de una búsqueda dentro del documento.
struct ResultadoBusqueda {
  Ubicacion ubicacion;
  Rect area;
  float puntuacion = 0.0f;
  std::string fragmento;
};

// Motor de búsqueda multimodal. Permite indexar documentos y páginas para
// realizar búsquedas rápidas sobre texto escrito, notas y títulos.
class MotorBusqueda {
 public:
  MotorBusqueda() = default;

  // Indexa todo el documento. Solo incorpora texto confirmado y etiquetas;
  // nunca inventa resultados para trazos que todavía no tienen OCR.
  void Indexar(const Documento& documento);

  // Indexa una página suelta. Útil cuando el usuario edita una página y
  // queremos actualizar el índice sin repasar todo el documento.
  void Indexar(const Pagina& pagina, const Ubicacion& ubicacion);

  // Busca en el contenido indexado.
  std::vector<ResultadoBusqueda> Buscar(const std::string& consulta,
                                        const Documento& documento) const;

  // Compatibilidad con callers antiguos: incorpora un fragmento OCR sin ID.
  // Para indexación incremental debe preferirse ReemplazarOcr.
  void IndexarOcr(const Ubicacion& ubicacion, const Rect& area,
                  const std::string& texto, const std::string& fragmento);

  // Fuente estable de OCR (por ejemplo hash de IDs de trazos). Volver a
  // reconocer la misma fuente REEMPLAZA su entrada anterior en vez de crear
  // duplicados. Una cadena vacía elimina el texto reconocido de esa fuente.
  void ReemplazarOcr(const std::string& fuente_id,
                     const Ubicacion& ubicacion, const Rect& area,
                     const std::string& texto, const std::string& fragmento);
  bool EliminarOcr(const std::string& fuente_id);

  // Limpia el índice actual.
  void Limpiar();

 private:
  enum class OrigenEntrada {
    Estructural,
    OcrSinId,
    OcrIdentificado,
  };

  // Representa un fragmento indexado de texto o contenido.
  struct EntradaIndice {
    Ubicacion ubicacion;
    Rect area;
    std::string texto_buscable;
    std::string fragmento_original;
    float peso;
    OrigenEntrada origen = OrigenEntrada::Estructural;
    std::string fuente_id;
  };

  std::vector<EntradaIndice> indice_;

  // Extrae etiquetas de los medios, como el nombre original o el tipo.
  std::string ExtraerEtiquetas(const Medio& medio) const;

  // Normaliza UTF-8 de forma determinista para búsqueda latino/inglesa:
  // minúsculas ASCII + plegado de los diacríticos latinos más usados.
  std::string NormalizarTexto(const std::string& texto) const;
};

}  // namespace tinta
