#include "buscador.h"

#include <algorithm>
#include <cctype>

namespace tinta {

namespace {

// Pliega los caracteres latinos de dos bytes más frecuentes en español,
// inglés académico y lenguas vecinas. Los bytes desconocidos se preservan tal
// cual para no corromper UTF-8 de otros alfabetos que Vision pueda devolver.
bool PlegarLatinC3(unsigned char segundo, char* salida) {
  if (!salida) return false;
  switch (segundo) {
    // A/a: À Á Â Ã Ä Å à á â ã ä å
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
      *salida = 'a'; return true;
    // C/c: Ç ç
    case 0x87: case 0xA7:
      *salida = 'c'; return true;
    // E/e: È É Ê Ë è é ê ë
    case 0x88: case 0x89: case 0x8A: case 0x8B:
    case 0xA8: case 0xA9: case 0xAA: case 0xAB:
      *salida = 'e'; return true;
    // I/i: Ì Í Î Ï ì í î ï
    case 0x8C: case 0x8D: case 0x8E: case 0x8F:
    case 0xAC: case 0xAD: case 0xAE: case 0xAF:
      *salida = 'i'; return true;
    // N/n: Ñ ñ
    case 0x91: case 0xB1:
      *salida = 'n'; return true;
    // O/o: Ò Ó Ô Õ Ö ò ó ô õ ö
    case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
    case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
      *salida = 'o'; return true;
    // U/u: Ù Ú Û Ü ù ú û ü
    case 0x99: case 0x9A: case 0x9B: case 0x9C:
    case 0xB9: case 0xBA: case 0xBB: case 0xBC:
      *salida = 'u'; return true;
    // Y/y: Ý ý ÿ
    case 0x9D: case 0xBD: case 0xBF:
      *salida = 'y'; return true;
    default:
      return false;
  }
}

}  // namespace

// --- Utilidades ---

std::string MotorBusqueda::NormalizarTexto(const std::string& texto) const {
  std::string resultado;
  resultado.reserve(texto.size());
  for (size_t i = 0; i < texto.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(texto[i]);
    if (c < 0x80) {
      resultado.push_back(static_cast<char>(std::tolower(c)));
      continue;
    }
    if (c == 0xC3 && i + 1 < texto.size()) {
      char plegado = 0;
      const unsigned char segundo = static_cast<unsigned char>(texto[i + 1]);
      if (PlegarLatinC3(segundo, &plegado)) {
        resultado.push_back(plegado);
        ++i;
        continue;
      }
    }
    resultado.push_back(static_cast<char>(c));
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

  Ubicacion ubicacion_doc{0, 0, 0};
  indice_.push_back({ubicacion_doc, Rect{0.0f, 0.0f, 0.0f, 0.0f},
                     NormalizarTexto(documento.titulo), documento.titulo, 2.0f});

  for (const auto& nota : documento.notas) {
    indice_.push_back({ubicacion_doc,
                       Rect{nota.posicion.x, nota.posicion.y, nota.ancho, nota.alto},
                       NormalizarTexto(nota.texto), nota.texto, 1.5f});
  }

  for (size_t c = 0; c < documento.cuadernos.size(); ++c) {
    const auto& cuaderno = documento.cuadernos[c];
    Ubicacion ubicacion_cuaderno{c, 0, 0};
    indice_.push_back({ubicacion_cuaderno, Rect{0.0f, 0.0f, 0.0f, 0.0f},
                       NormalizarTexto(cuaderno.titulo), cuaderno.titulo, 1.8f});

    for (const auto& nota : cuaderno.notas) {
      indice_.push_back({ubicacion_cuaderno,
                         Rect{nota.posicion.x, nota.posicion.y, nota.ancho, nota.alto},
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
  indice_.push_back({ubicacion, Rect{0.0f, 0.0f, pagina.ancho, 50.0f},
                     NormalizarTexto(pagina.titulo), pagina.titulo, 1.5f});

  for (const auto& cuadro : pagina.textos) {
    indice_.push_back({ubicacion, cuadro.marco,
                       NormalizarTexto(cuadro.texto), cuadro.texto, 1.0f});
  }

  for (const auto& nota : pagina.notas) {
    indice_.push_back({ubicacion,
                       Rect{nota.posicion.x, nota.posicion.y, nota.ancho, nota.alto},
                       NormalizarTexto(nota.texto), nota.texto, 1.0f});
  }

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
  EntradaIndice entrada;
  entrada.ubicacion = ubicacion;
  entrada.area = area;
  entrada.texto_buscable = normalizado;
  entrada.fragmento_original = fragmento.empty() ? texto : fragmento;
  entrada.peso = 1.15f;
  entrada.origen = OrigenEntrada::OcrSinId;
  indice_.push_back(std::move(entrada));
}

void MotorBusqueda::ReemplazarOcr(const std::string& fuente_id,
                                  const Ubicacion& ubicacion,
                                  const Rect& area,
                                  const std::string& texto,
                                  const std::string& fragmento) {
  if (fuente_id.empty()) {
    IndexarOcr(ubicacion, area, texto, fragmento);
    return;
  }
  EliminarOcr(fuente_id);
  const std::string normalizado = NormalizarTexto(texto);
  if (normalizado.empty()) return;
  EntradaIndice entrada;
  entrada.ubicacion = ubicacion;
  entrada.area = area;
  entrada.texto_buscable = normalizado;
  entrada.fragmento_original = fragmento.empty() ? texto : fragmento;
  entrada.peso = 1.15f;
  entrada.origen = OrigenEntrada::OcrIdentificado;
  entrada.fuente_id = fuente_id;
  indice_.push_back(std::move(entrada));
}

bool MotorBusqueda::EliminarOcr(const std::string& fuente_id) {
  if (fuente_id.empty()) return false;
  const auto anterior = indice_.size();
  indice_.erase(
      std::remove_if(indice_.begin(), indice_.end(),
                     [&](const EntradaIndice& entrada) {
                       return entrada.origen == OrigenEntrada::OcrIdentificado &&
                              entrada.fuente_id == fuente_id;
                     }),
      indice_.end());
  return indice_.size() != anterior;
}

void MotorBusqueda::Limpiar() {
  indice_.clear();
}

std::vector<ResultadoBusqueda> MotorBusqueda::Buscar(
    const std::string& consulta, const Documento& /*documento*/) const {
  std::vector<ResultadoBusqueda> resultados;
  const std::string consulta_norm = NormalizarTexto(consulta);
  if (consulta_norm.empty()) return resultados;

  for (const auto& entrada : indice_) {
    const size_t pos = entrada.texto_buscable.find(consulta_norm);
    if (pos == std::string::npos) continue;
    ResultadoBusqueda res;
    res.ubicacion = entrada.ubicacion;
    res.area = entrada.area;
    res.puntuacion = entrada.peso *
        (entrada.texto_buscable == consulta_norm ? 2.0f : 1.0f);
    res.fragmento = entrada.fragmento_original;
    resultados.push_back(std::move(res));
  }

  std::stable_sort(resultados.begin(), resultados.end(),
                   [](const ResultadoBusqueda& a,
                      const ResultadoBusqueda& b) {
                     return a.puntuacion > b.puntuacion;
                   });
  return resultados;
}

}  // namespace tinta
