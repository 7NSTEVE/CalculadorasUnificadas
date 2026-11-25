// Lógica de IA local: estructurar prompts e historiales sin depender de la
// plataforma.
#pragma once

#include <string>

#include "documento.h"

namespace tinta {

// Genera un prompt con estilo socrático que incluye el contexto de la página
// (textos y notas) pero instruye explícitamente evitar dar respuestas directas.
std::string GenerarPromptSocratico(const Pagina& pagina,
                                   const std::string& pregunta_usuario);

}  // namespace tinta
