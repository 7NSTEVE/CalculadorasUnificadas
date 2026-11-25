// Exportador a Markdown y LaTeX.
// Vive en el núcleo.
#pragma once

#include <string>
#include "documento.h"

namespace tinta {

// Extrae todo el texto de una página y lo guarda en formato Markdown.
bool ExportarMarkdown(const Pagina& pagina, const std::string& path);

// Exporta a LaTeX, envolviendo ecuaciones en $$ y textos en formato estándar.
bool ExportarLatex(const Pagina& pagina, const std::string& path);

}  // namespace tinta
