#include "buscador.h"

#include <algorithm>
#include <cctype>

namespace tinta {

// --- Utilidades ---

std::string MotorBusqueda::NormalizarTexto(const std::string& texto) const {
  std::string resultado = texto;
  for (char& c : resultado) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return resultado;
}

std::string MotorBusqueda::ExtraerEtiquetas(const Medio& medio) const {
  std::string etiquetas = medio.nombre_original;
  if (medio.tipo == TipoMedio::Imagen) {
    etiquetas += " imagen foto";
  } else if (medio.tipo == TipoMedio::Video) {
    etiquetas += " video";
  } else if (medio.tipo == TipoMedio::Pdf) {
    etiquetas += " pdf documento";
  } else if (medio.tipo == TipoMedio::Audio) {
    etiquetas += " audio grabacion";
  }
  return etiquetas;
}

// --- MotorBusqueda ---

void MotorBusqueda::Indexar(const Documento& documento) {
  Limpiar();

  // Indexar título del documento (a nivel global, o en la ubicación inicial)
  Ubicacion ubicacion_doc{0, 0, 0};
  indice_.push_back({ubicacion_doc, Rect{0.0f, 0.0f, 0.0f, 0.0f},
                     NormalizarTexto(documento.titulo), documento.titulo, 2.0f});

  // Indexar notas globales del documento
  for (const auto& nota : documento.notas) {
    indice_.push_back({ubicacion_doc, Rect{nota.posicion.x, nota.posicion.y, nota.ancho, nota.alto},
                       NormalizarTexto(nota.texto), nota.texto, 1.5f});
  }

  // Indexar cuadernos, secciones y páginas
  for (size_t c = 0; c < documento.cuadernos.size(); ++c) {
    const auto& cuaderno = documento.cuadernos[c];
    Ubicacion ubicacion_cuaderno{c, 0, 0};
    indice_.push_back({ubicacion_cuaderno, Rect{0.0f, 0.0f, 0.0f, 0.0f},
                       NormalizarTexto(cuaderno.titulo), cuaderno.titulo, 1.8f});
    
    for (const auto& nota : cuaderno.notas) {
      indice_.push_back({ubicacion_cuaderno, Rect{nota.posicion.x, nota.posicion.y, nota.ancho, nota.alto},
                         NormalizarTexto(nota.texto), nota.texto, 1.5f});
    }

    for (size_t s = 0; s < cuaderno.secciones.size(); ++s) {
      const auto& seccion = cuaderno.secciones[s];
      Ubicacion ubicacion_seccion{c, s, 0};
      indice_.push_back({ubicacion_seccion, Rect{0.0f, 0.0f, 0.0f, 0.0f},
                         NormalizarTexto(seccion.titulo), seccion.titulo, 1.6f});

      for (size_t p = 0; p < seccion.paginas.size(); ++p) {
        const auto& pagina = seccion.paginas[p];
        Ubicacion ubicacion_pagina{c, s, p};
        Indexar(pagina, ubicacion_pagina);
      }
    }
  }
}

void MotorBusqueda::Indexar(const Pagina& pagina, const Ubicacion& ubicacion) {
  // Indexar el título de la página
  indice_.push_back({ubicacion, Rect{0.0f, 0.0f, pagina.ancho, 50.0f},
                     NormalizarTexto(pagina.titulo), pagina.titulo, 1.5f});

  // Indexar cuadros de texto
  for (const auto& cuadro : pagina.textos) {
    indice_.push_back({ubicacion, cuadro.marco,
                       NormalizarTexto(cuadro.texto), cuadro.texto, 1.0f});
  }

  // Indexar notas de la página
  for (const auto& nota : pagina.notas) {
    indice_.push_back({ubicacion, Rect{nota.posicion.x, nota.posicion.y, nota.ancho, nota.alto},
                       NormalizarTexto(nota.texto), nota.texto, 1.0f});
  }

  // Indexar medios (etiquetas)
  for (const auto& medio : pagina.medios) {
    std::string etiquetas = ExtraerEtiquetas(medio);
    indice_.push_back({ubicacion, medio.marco,
                       NormalizarTexto(etiquetas), medio.nombre_original, 0.8f});
  }

}

void MotorBusqueda::IndexarOcr(const Ubicacion& ubicacion, const Rect& area,
                               const std::string& texto,
                               const std::string& fragmento) {
  const std::string normalizado = NormalizarTexto(texto);
  if (normalizado.empty()) return;
  indice_.push_back({ubicacion, area, normalizado,
                     fragmento.empty() ? texto : fragmento, 1.15f});
}

void MotorBusqueda::Limpiar() {
  indice_.clear();
}

std::vector<ResultadoBusqueda> MotorBusqueda::Buscar(const std::string& consulta,
                                                     const Documento& /*documento*/) const {
  std::vector<ResultadoBusqueda> resultados;
  std::string consulta_norm = NormalizarTexto(consulta);

  if (consulta_norm.empty()) {
    return resultados;
  }

  for (const auto& entrada : indice_) {
    // Búsqueda simple de subcadena
    size_t pos = entrada.texto_buscable.find(consulta_norm);
    if (pos != std::string::npos) {
      ResultadoBusqueda res;
      res.ubicacion = entrada.ubicacion;
      res.area = entrada.area;
      // Incrementa la puntuación si es una coincidencia exacta
      res.puntuacion = entrada.peso * (entrada.texto_buscable == consulta_norm ? 2.0f : 1.0f);
      res.fragmento = entrada.fragmento_original;
      resultados.push_back(res);
    }
  }

  // Ordenar de mayor a menor puntuación
  std::stable_sort(resultados.begin(), resultados.end(),
            [](const ResultadoBusqueda& a, const ResultadoBusqueda& b) {
              return a.puntuacion > b.puntuacion;
            });

  return resultados;
}

}  // namespace tinta
