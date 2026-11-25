#include "exportador.h"

#include <fstream>
#include <algorithm>

namespace tinta {

// Heurística sencilla para decidir si algo es una ecuación matemática o texto normal.
static bool EsEcuacion(const std::string& texto) {
  // Las matemáticas de LaTeX suelen llevar barra invertida, o sub/superíndices.
  if (texto.find('\\') != std::string::npos ||
      texto.find('^') != std::string::npos ||
      texto.find('_') != std::string::npos) {
    return true;
  }
  return false;
}

bool ExportarMarkdown(const Pagina& pagina, const std::string& path) {
  std::ofstream out(path);
  if (!out) return false;

  if (!pagina.titulo.empty()) {
    out << "# " << pagina.titulo << "\n\n";
  }

  for (const auto& texto : pagina.textos) {
    out << texto.texto << "\n\n";
  }

  for (const auto& nota : pagina.notas) {
    out << "> " << nota.texto << "\n\n";
  }

  return true;
}

bool ExportarLatex(const Pagina& pagina, const std::string& path) {
  std::ofstream out(path);
  if (!out) return false;

  out << "\\documentclass{article}\n";
  out << "\\usepackage[utf8]{inputenc}\n";
  out << "\\usepackage{amsmath}\n";
  out << "\\begin{document}\n";
  
  if (!pagina.titulo.empty()) {
    out << "\\title{" << pagina.titulo << "}\n";
    out << "\\maketitle\n\n";
  }

  for (const auto& texto : pagina.textos) {
    if (EsEcuacion(texto.texto)) {
      out << "$$\n" << texto.texto << "\n$$\n\n";
    } else {
      out << texto.texto << "\n\n";
    }
  }

  for (const auto& nota : pagina.notas) {
    out << "\\begin{quote}\n";
    if (EsEcuacion(nota.texto)) {
      out << "$$\n" << nota.texto << "\n$$\n";
    } else {
      out << nota.texto << "\n";
    }
    out << "\\end{quote}\n\n";
  }

  out << "\\end{document}\n";
  return true;
}

}  // namespace tinta
