#pragma once
#include <string>
#include <vector>
#include "elementos.h"
#include "documento.h"

namespace tinta {

// Modelo de un documento PDF importado en la app (solo metadatos, no render).
struct DocumentoPdf {
  std::string id;
  std::string hash; // Identidad del contenido
  int total_paginas = 0;
  
  // true si el PDF tiene texto seleccionable/OCR
  bool tiene_texto = false;
  
  // Extrae un "Printout": convierte una página del PDF en un Medio que se puede insertar 
  // en el lienzo, con espacio infinito alrededor.
  Medio CrearPrintout(int pagina, float x, float y) const;
};

// Convierte el lienzo de una página existente para darle espacio infinito alrededor del PDF.
// Esto cumple el requisito "PDF dentro de lienzo infinito" y "márgenes adicionales al PDF".
void HacerLienzoInfinitoAlrededorDePdf(Pagina* pagina, float margen_inicial_px = 300.0f);

}
