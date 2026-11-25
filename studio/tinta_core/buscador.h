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

  // Incorpora texto reconocido por el motor local de la plataforma (Vision en
  // iPadOS), conservando su caja y el fragmento original. Una cadena vacía se
  // ignora para que una falla de OCR no produzca coincidencias fantasma.
  void IndexarOcr(const Ubicacion& ubicacion, const Rect& area,
                  const std::string& texto, const std::string& fragmento);

  // Limpia el índice actual.
  void Limpiar();

 private:
  // Representa un fragmento indexado de texto o contenido.
  struct EntradaIndice {
    Ubicacion ubicacion;
    Rect area;
    std::string texto_buscable;
    std::string fragmento_original;
    float peso;
  };

  std::vector<EntradaIndice> indice_;

  // Extrae etiquetas de los medios, como el nombre original o el tipo.
  std::string ExtraerEtiquetas(const Medio& medio) const;

  // Utilidad para pasar a minúsculas y quitar acentos de forma muy básica.
  std::string NormalizarTexto(const std::string& texto) const;
};

}  // namespace tinta
